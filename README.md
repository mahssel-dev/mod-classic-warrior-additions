# mod-classic-warrior-additions

AzerothCore module for WoW 3.3.5 that adds Classic-style warrior mechanics missing from the WotLK baseline.

## Features

### Thunderclap → Rend Spread

When a warrior casts Thunderclap, their highest-rank Rend is spread to all nearby enemies that don't already have it. Inspired by the Classic-era Blood Frenzy talent fantasy where TC + Rend is the core AoE rotation loop.

- Spreads the warrior's own Rend (aura ownership preserved — warrior gets the ticks)
- Spread Rend matches the source's remaining duration, not a fresh application (Can keep a snapped shot rend going in theory)
- Configurable: require a specific talent aura (e.g. Blood Frenzy), control whether existing Rend on targets gets refreshed

### Thunderclap AP Scaling

Thunderclap gains bonus flat damage per hit based on the warrior's Strength: `floor(STR × multiplier)`. Though Thunderclap already scales with Attack Power in WotLK this can amplify that scaling with a cap of x5.

### Dual Wield Level Scaling

Dual-wielding warriors receive a hidden hit bonus at lower levels that scales linearly to zero at the server's level cap. Compensates for the +19% white-miss DW penalty that makes leveling feel unnecessarily punishing before gear is available.

- Completely invisible — implemented as a `SPELL_ATTR0_PASSIVE` aura, never appears in the buff bar
- Recalculates on login, level change, and weapon equip/unequip
- Configurable max bonus and level cap

## Configuration

Copy `conf/mod_warrior_additions.conf.dist` to your server's config directory and rename to `mod_warrior_additions.conf`.

| Key | Default | Description |
|-----|---------|-------------|
| `WarriorAdditions.Enable` | `1` | Master switch — disables the entire module when `0`, regardless of per-feature settings |
| `WarriorAdditions.RendSpread.Enable` | `1` | Enable/disable the Rend spread feature |
| `WarriorAdditions.RendSpread.SpreadSameRank` | `0` | `1` = source Rend must be on the current target; `0` = use highest rank found on any unit in range |
| `WarriorAdditions.RendSpread.RefreshOnSpread` | `1` | `1` = also refresh Rend on targets that already have it; `0` = skip those targets |
| `WarriorAdditions.RendSpread.RequiresTalentId` | `0` | Spell ID of a passive talent aura the warrior must have for spread to trigger (e.g. Blood Frenzy rank 1 = `29836`, rank 2 = `29859`). `0` = no requirement |
| `WarriorAdditions.RendSpread.ProcChance` | `100` | Base % chance for the spread to trigger. Used when no `TalentProcList` entry matches |
| `WarriorAdditions.RendSpread.TalentProcList` | `""` | Comma-separated `spellId:chance` pairs. First matching aura overrides `ProcChance`. Example: `"29836:50,29859:100"` |
| `WarriorAdditions.ThunderclapAP.Enable` | `1` | Enable/disable the AP scaling feature entirely |
| `WarriorAdditions.ThunderclapAP.StrMultiplier` | `0.20` | Bonus damage per TC hit = `floor(STR × value)`. `0.0` disables. Max `5.0` |
| `WarriorAdditions.DWLevelScaling.Enable` | `1` | Enable/disable the DW level scaling feature |
| `WarriorAdditions.DWLevelScaling.MaxLevel` | `60` | Level at which bonus reaches 0. Set to your server's level cap |
| `WarriorAdditions.DWLevelScaling.MaxBonus` | `19.0` | Hit% bonus at level 1. `19.0` fully compensates the DW miss penalty |
| `WarriorAdditions.DWLevelScaling.MinBonus` | `0.0` | Floor bonus — never drops below this value even at or above MaxLevel. `0` = no floor |

## Installation

1. Clone into `modules/mod-classic-warrior-additions` inside your AzerothCore source
2. Rebuild
3. Apply both SQL files to `acore_world`:
   - `data/sql/db-world/base/mod_warrior_additions_spell_scripts.sql`
   - `data/sql/db-world/base/mod_warrior_additions_dw_scaling_spell.sql`
4. Copy and rename the `.conf.dist` file, then restart the worldserver

## Requirements

- AzerothCore 3.3.5 (WotLK)
