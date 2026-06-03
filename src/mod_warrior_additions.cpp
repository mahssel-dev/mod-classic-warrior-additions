#include "ScriptMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "Unit.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "Config.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Random.h"
#include "Tokenize.h"
#include <algorithm>
#include <cmath>
#include <list>
#include <string>

static constexpr uint32 REND_IDS[] = { 772, 2974, 2975, 11574, 11575, 11576, 25208, 47465, 47466 };


// Returns true if the unit has any Rend aura owned by ownerGuid.
static bool UnitHasOwnRend(Unit* unit, ObjectGuid ownerGuid)
{
    for (uint32 id : REND_IDS)
        if (unit->GetAura(id, ownerGuid))
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
        return sConfigMgr->GetOption<bool>("WarriorAdditions.Enable", true)
            && sConfigMgr->GetOption<bool>("WarriorAdditions.RendSpread.Enable", true)
            && GetCaster() && GetCaster()->getClass() == CLASS_WARRIOR;
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

        // Resolve proc chance — check TalentProcList for a per-aura override, fall back to ProcChance.
        float procChance = sConfigMgr->GetOption<float>("WarriorAdditions.RendSpread.ProcChance", 100.0f);
        std::string talentProcStr = sConfigMgr->GetOption<std::string>("WarriorAdditions.RendSpread.TalentProcList", "");
        if (!talentProcStr.empty())
        {
            for (std::string_view entry : Acore::Tokenize(talentProcStr, ',', false))
            {
                auto parts = Acore::Tokenize(entry, ':', false);
                if (parts.size() != 2)
                    continue;
                try
                {
                    uint32 spellId = static_cast<uint32>(std::stoul(std::string(parts[0])));
                    float  chance  = std::stof(std::string(parts[1]));
                    if (spellId > 0 && caster->HasAura(spellId))
                    {
                        procChance = chance;
                        break;
                    }
                }
                catch (...) { continue; }
            }
        }
        procChance = std::min(100.0f, std::max(0.0f, procChance));
        if (procChance <= 0.0f || (procChance < 100.0f && !roll_chance_f(procChance)))
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
            bool hasOwnRend = UnitHasOwnRend(unit, casterGuid);
            if (!hasOwnRend || refreshOnSpread)
            {
                // AddAura bypasses the spell cast system (no facing/range checks).
                // CastSpell(triggered=true) still runs CheckRange which enforces
                // SPELL_FACING_FLAG_INFRONT for melee spells, silently failing for
                // units behind the caster.
                caster->AddAura(rendSpellId, unit);
                if (!hasOwnRend)
                {
                    // Spread: match source's remaining duration so it doesn't start at full
                    if (Aura* newAura = unit->GetAura(rendSpellId, casterGuid))
                    {
                        newAura->SetMaxDuration(sourceMaxDuration);
                        newAura->SetDuration(sourceDuration);
                    }
                }
                // Refresh (hasOwnRend): AddAura resets to full duration via TryRefreshStackOrCreate — don't override
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
        return sConfigMgr->GetOption<bool>("WarriorAdditions.Enable", true)
            && sConfigMgr->GetOption<bool>("WarriorAdditions.ThunderclapAP.Enable", true)
            && GetCaster() && GetCaster()->getClass() == CLASS_WARRIOR;
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

// ─────────────────────────────────────────────────────────────────────────────
// DW Level Scaling
//
// Grants dual-wielding warriors a hidden hit bonus that scales down to 0 at
// MaxLevel, compensating for the +19% DW white-miss penalty at lower levels.
// Completely invisible — implemented as a PASSIVE aura (no buff bar entry).
//
// Controlled by worldserver.conf:
//   WarriorAdditions.DWLevelScaling.Enable    0/1
//   WarriorAdditions.DWLevelScaling.MaxLevel  <level cap, default 60>
//   WarriorAdditions.DWLevelScaling.MaxBonus  <max % hit at level 1, default 19.0>
//
// Custom passive spell defined in mod_warrior_additions_dw_scaling_spell.sql
// ─────────────────────────────────────────────────────────────────────────────
static constexpr uint32 WARRIOR_DW_HIT_SPELL_ID = 900001;

static void ApplyDWHitBonus(Player* player)
{
    if (!sConfigMgr->GetOption<bool>("WarriorAdditions.Enable", true))
        return;
    if (!sConfigMgr->GetOption<bool>("WarriorAdditions.DWLevelScaling.Enable", true))
        return;
    if (player->getClass() != CLASS_WARRIOR)
        return;

    // Always remove first so we either cleanly re-apply or leave it gone
    player->RemoveAura(WARRIOR_DW_HIT_SPELL_ID);

    // Only applies when actually dual wielding (weapon in offhand, not shield)
    if (!player->GetWeaponForAttack(OFF_ATTACK, false))
        return;

    uint32 maxLevel = sConfigMgr->GetOption<uint32>("WarriorAdditions.DWLevelScaling.MaxLevel", 60);
    float  maxBonus = sConfigMgr->GetOption<float>("WarriorAdditions.DWLevelScaling.MaxBonus", 19.0f);
    float  minBonus = sConfigMgr->GetOption<float>("WarriorAdditions.DWLevelScaling.MinBonus", 0.0f);

    uint32 level = player->GetLevel();

    // Linear: maxBonus at level 1, minBonus at maxLevel, minBonus beyond
    float scaled = (level < maxLevel)
        ? maxBonus * (float)(maxLevel - level) / (float)(maxLevel - 1)
        : 0.0f;
    int32 bonus = static_cast<int32>(std::floor(std::max(minBonus, scaled)));
    if (bonus <= 0)
        return;

    if (Aura* aura = player->AddAura(WARRIOR_DW_HIT_SPELL_ID, player))
    {
        aura->SetMaxDuration(-1);
        aura->SetDuration(-1);
        if (AuraEffect* eff = aura->GetEffect(0))
            eff->ChangeAmount(bonus);
    }
}

class warrior_dw_level_scaling : public PlayerScript
{
public:
    warrior_dw_level_scaling() : PlayerScript("warrior_dw_level_scaling") {}

    void OnPlayerLogin(Player* player) override
    {
        ApplyDWHitBonus(player);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        ApplyDWHitBonus(player);
    }

    // Re-evaluate on any equip — detects offhand weapon being added or replaced
    void OnPlayerEquip(Player* player, Item* /*item*/, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        ApplyDWHitBonus(player);
    }

    // Re-evaluate when item removed — detects offhand being emptied
    void OnPlayerUnequip(Player* player, Item* /*item*/) override
    {
        ApplyDWHitBonus(player);
    }
};

void AddSC_warrior_additions()
{
    RegisterSpellScript(spell_warrior_thunderclap_rend_spread);
    RegisterSpellScript(spell_warrior_thunderclap_ap_scaling);
    new warrior_dw_level_scaling();
}
