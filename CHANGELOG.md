# Changelog

All notable code and architecture updates for `SISTEMAS-MMO` are recorded here.

## Unreleased

### Added

- `core/item.hpp` to define the item contract split into material, consumable, equipment, quest, and collectible item types.
- `ItemId`, `ItemTemplateId`, and `ItemSetId` in `core/id.hpp` for item runtime, templates, and equipment sets.
- `core/damage.hpp` and `core/material.hpp` to define the damage/material contracts for future map-object reactions and durability handling.
- `core/combat_damage.hpp` to define the combat-only damage resolution contract for entity versus entity fights.
- `core/action.hpp` to define the live action contract: action kind, outcome, state, profile, and catalog.
- `CHANGELOG.md` as the running log for future code changes and additions.
- Default action catalog factory with only `auto_attack` and `dodge` profiles for the skill-focused combat baseline.
- `core/skill.hpp` to define the skill contract with active, passive, and trigger modes plus chain, initial, finisher, and evolve patterns.
- `id::SkillId` and `invalid_skill_id` to give skills a stable identifier.
- `core/combat.hpp` to define combat intent, cost, target validation, and aggro state.
- `status::Kind::taunt` as a build-up control ailment that can force a temporary target override.
- `status::Kind::daze` as the renamed control ailment slot that replaces the old stun entry.
- A negative ailment catalog in `core/status.hpp` covering poison, burn, bleed, freeze, daze, silence, root, curse, and taunt.
- Instant buff definitions in `core/status.hpp` for shield, haste, regeneration, bless, and resistant.
- A full status catalog builder that combines the negative ailments and the new instant buffs.

### Changed

- Removed `accuracy` and `evasion` from the core stat model; action recovery now relies on the remaining stat biases plus hit/whiff outcome.
- `core/recovery.hpp` now follows the action contract and computes animation and recovery from stats, action weight, and hit/whiff outcome.
- `core/entity.hpp` keeps the live action state on the entity contract.
- `core/core.hpp` now aggregates `skill.hpp` alongside the rest of the core contract surface.
- `core/entity.hpp` now stores aggro state and target selection helpers.
- `core/entity.hpp` now applies and clears forced targets when taunt status is gained or removed.
- `core/combat.hpp::Resolution` now exposes the final resolved target explicitly.
- Core documentation now tracks the action/recovery split.
- `action::Kind::light_attack` was renamed to `action::Kind::auto_attack`.
- `core/status.hpp` now carries shared ailment modifiers, stack-stage tiers, and build-up capacity for richer status outcomes.
- `core/entity.hpp` now applies status percentage modifiers to derived stats during refresh.
- `core/entity.hpp` now tracks shield buffer, burn/regeneration cancellation, and daze/haste cancellation.
- `core/entity.hpp` now routes build-up scaling from active resistant and bless effects into status accumulation and decay.
