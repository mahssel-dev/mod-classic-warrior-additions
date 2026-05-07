#include "ScriptMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "Unit.h"
#include "SpellAuras.h"
#include "Config.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include <algorithm>
#include <cmath>
#include <list>

static constexpr uint32 REND_IDS[] = { 772, 2974, 2975, 11574, 11575, 11576, 25208, 47465, 47466 };

static bool UnitHasRend(Unit* unit)
{
    for (uint32 id : REND_IDS)
        if (unit->HasAura(id))
            return true;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Thunderclap Rend Spread
//
// Collects nearby unfriendly units via Cell::VisitObjects in AfterCast, then spreads Rend.
// Behaviour controlled by worldserver.conf:
//   WarriorAdditions.RendSpread.SpreadSameRank   0/1
//   WarriorAdditions.RendSpread.RefreshOnSpread  0/1
//   WarriorAdditions.RendSpread.RequiresTalentId <spell id, 0 = none>
// ─────────────────────────────────────────────────────────────────────────────
// TC nominal range 8 yd + caster combat reach (~1.5) + target combat reach (~1.5 max)
static constexpr float TC_RANGE = 11.0f;

class spell_warrior_thunderclap_rend_spread : public SpellScript
{
    PrepareSpellScript(spell_warrior_thunderclap_rend_spread);

    bool Load() override
    {
        return GetCaster() && GetCaster()->getClass() == CLASS_WARRIOR;
    }

    void HandleAfterCast()
    {
        bool   spreadSameRank   = sConfigMgr->GetOption<int32>("WarriorAdditions.RendSpread.SpreadSameRank",    0) != 0;
        bool   refreshOnSpread  = sConfigMgr->GetOption<int32>("WarriorAdditions.RendSpread.RefreshOnSpread",   1) != 0;
        uint32 requiresTalentId = sConfigMgr->GetOption<uint32>("WarriorAdditions.RendSpread.RequiresTalentId", 0);

        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (requiresTalentId > 0 && !caster->HasAura(requiresTalentId))
            return;

        // Collect unfriendly units in TC range directly (VisitObjects only — avoids C2665)
        std::list<Unit*> targets;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(caster, caster, TC_RANGE);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(caster, targets, check);
        Cell::VisitObjects(caster, searcher, TC_RANGE);

        if (targets.empty())
            return;

        uint32 rendSpellId = 0;
        Aura*  sourceAura  = nullptr;

        ObjectGuid casterGuid = caster->GetGUID();

        if (spreadSameRank)
        {
            // Use the warrior's current target as the sole Rend source
            Unit* victim = caster->GetVictim();
            if (!victim)
                return;
            for (int i = 8; i >= 0; --i)
            {
                if (Aura* a = victim->GetAura(REND_IDS[i], casterGuid))
                {
                    rendSpellId = REND_IDS[i];
                    sourceAura  = a;
                    break;
                }
            }
        }
        else
        {
            // Find the highest Rend rank cast by this warrior on any unit in range
            for (int i = 8; i >= 0 && !rendSpellId; --i)
            {
                for (Unit* unit : targets)
                {
                    if (Aura* a = unit->GetAura(REND_IDS[i], casterGuid))
                    {
                        rendSpellId = REND_IDS[i];
                        sourceAura  = a;
                        break;
                    }
                }
            }
        }

        if (!rendSpellId || !sourceAura)
            return;

        int32 sourceDuration    = sourceAura->GetDuration();
        int32 sourceMaxDuration = sourceAura->GetMaxDuration();

        for (Unit* unit : targets)
        {
            bool hadRend = UnitHasRend(unit);
            if (!hadRend || refreshOnSpread)
            {
                // AddAura bypasses the spell cast system (no facing/range checks).
                // CastSpell(triggered=true) still runs CheckRange which enforces
                // SPELL_FACING_FLAG_INFRONT for melee spells, silently failing for
                // units behind the caster.
                caster->AddAura(rendSpellId, unit);
                if (!hadRend)
                {
                    // Spread: match source's remaining duration so it doesn't start at full
                    if (Aura* newAura = unit->GetAura(rendSpellId, casterGuid))
                    {
                        newAura->SetMaxDuration(sourceMaxDuration);
                        newAura->SetDuration(sourceDuration);
                    }
                }
                // Refresh (hadRend): AddAura resets to full duration via TryRefreshStackOrCreate — don't override
            }
        }
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_warrior_thunderclap_rend_spread::HandleAfterCast);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Thunderclap AP Scaling
//
// Adds flat bonus damage per hit: floor(Strength * StrMultiplier).
// Controlled by worldserver.conf:
//   WarriorAdditions.ThunderclapAP.StrMultiplier <float 0.0–5.0, default 0.20>
// ─────────────────────────────────────────────────────────────────────────────
class spell_warrior_thunderclap_ap_scaling : public SpellScript
{
    PrepareSpellScript(spell_warrior_thunderclap_ap_scaling);

    bool Load() override
    {
        return GetCaster() && GetCaster()->getClass() == CLASS_WARRIOR;
    }

    void HandleOnHit()
    {
        float strMult = sConfigMgr->GetOption<float>("WarriorAdditions.ThunderclapAP.StrMultiplier", 0.20f);
        strMult = std::min(5.0f, std::max(0.0f, strMult));

        if (strMult <= 0.0f)
            return;

        Unit* caster = GetCaster();
        if (!caster)
            return;

        int32 bonus = static_cast<int32>(std::floor(caster->GetStat(STAT_STRENGTH) * strMult));
        if (bonus <= 0)
            return;

        SetHitDamage(GetHitDamage() + bonus);
    }

    void Register() override
    {
        OnHit += SpellHitFn(spell_warrior_thunderclap_ap_scaling::HandleOnHit);
    }
};

void AddSC_warrior_additions()
{
    RegisterSpellScript(spell_warrior_thunderclap_rend_spread);
    RegisterSpellScript(spell_warrior_thunderclap_ap_scaling);
}
