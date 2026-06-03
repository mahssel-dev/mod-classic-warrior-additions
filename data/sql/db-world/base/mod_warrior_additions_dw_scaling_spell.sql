-- Custom passive spell for WarriorAdditions DW Level Scaling.
-- SPELL_ATTR0_PASSIVE (64) hides it from the player's buff bar entirely.
-- EffectAura_1 = 99 = SPELL_AURA_MOD_HIT_CHANCE.
-- Amount is overridden at runtime via AuraEffect::ChangeAmount — SQL value is 0.
DELETE FROM `spell_dbc` WHERE `ID` = 900001;

INSERT INTO `spell_dbc`
(`ID`, `Attributes`, `CastingTimeIndex`, `DurationIndex`, `RangeIndex`,
 `EquippedItemClass`, `Effect_1`, `EffectBasePoints_1`,
 `ImplicitTargetA_1`, `EffectAura_1`,
 `Name_Lang_enUS`, `Name_Lang_enGB`, `Name_Lang_koKR`, `Name_Lang_frFR`,
 `Name_Lang_deDE`, `Name_Lang_Mask`,
 `SchoolMask`,
 `EffectChainAmplitude_1`, `EffectChainAmplitude_2`, `EffectChainAmplitude_3`,
 `EffectBonusMultiplier_1`, `EffectBonusMultiplier_2`, `EffectBonusMultiplier_3`)
VALUES
(900001, 64, 1, 21, 1,
 -1, 6, -1,
 1, 99,
 'Warrior DW Hit Scaling', '', '', '', '', 0,
 1,
 1, 1, 1,
 1, 1, 1);