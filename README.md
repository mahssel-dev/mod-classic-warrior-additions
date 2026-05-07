# mod-classic-warrior-additions

AzerothCore module for WoW 3.3.5 that adds Classic-style warrior mechanics missing from the WotLK baseline.

## Features

### Thunderclap → Rend Spread

When a warrior casts Thunderclap, their highest-rank Rend is spread to all nearby enemies that don't already have it. Inspired by the Classic-era Blood Frenzy talent fantasy where TC + Rend is the core AoE rotation loop.

- Spreads the warrior's own Rend (aura ownership preserved — warrior gets the ticks)
- Spread Rend matches the source's remaining duration, not a fresh application
- Configurable: require a specific talent aura (e.g. Blood Frenzy), control whether existing Rend on targets gets refreshed

### Thunderclap AP Scaling

Thunderclap gains bonus flat damage per hit based on the warrior's Strength: `floor(STR × multiplier)`. Makes Strength gear meaningful for the TC rotation rather than purely a stat-stick for auto-attacks.

## Configuration

Copy `conf/mod_warrior_additions.conf.dist` to your server's config directory and rename to `mod_warrior_additions.conf`.

| Key | Default | Description |
|-----|---------|-------------|
| `WarriorAdditions.RendSpread.SpreadSameRank` | `0` | `1` = source Rend must be on the current target; `0` = use highest rank found on any unit in range |
| `WarriorAdditions.RendSpread.RefreshOnSpread` | `1` | `1` = also refresh Rend on targets that already have it; `0` = skip those targets |
| `WarriorAdditions.RendSpread.RequiresTalentId` | `0` | Spell ID of a passive talent aura the warrior must have for spread to trigger (e.g. Blood Frenzy rank 1 = `29836`, rank 2 = `29859`). `0` = no requirement |
| `WarriorAdditions.ThunderclapAP.StrMultiplier` | `0.20` | Bonus damage per TC hit = `floor(STR × value)`. `0.0` disables. Max `5.0` |

## Installation

1. Clone into `modules/mod-classic-warrior-additions` inside your AzerothCore source
2. Rebuild
3. Apply `data/sql/db-world/base/mod_warrior_additions_spell_scripts.sql` to `acore_world`
4. Copy and rename the `.conf.dist` file, then restart the worldserver

## Requirements

- AzerothCore 3.3.5 (WotLK)
