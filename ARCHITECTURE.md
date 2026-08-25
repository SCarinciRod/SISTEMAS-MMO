# SISTEMAS-MMO Architecture Guide

This file is the working agreement for the project. Read it before making architecture changes or adding new core systems.

## Current Focus

1. Build a stable shared entity foundation for players, NPCs, and monsters.
2. Keep state, definitions, and evaluation separated.
3. Use evolution as a consumer of world state, not as the center of the whole design.
4. Keep the codebase small, explicit, and easy to expand.

## Current Core Boundaries

- `core/species.hpp`: static species catalog.
- `core/action.hpp`: live action contract and profile catalog.
- `core/skill.hpp`: skill contract, tags, and skill catalog.
- `core/combat.hpp`: combat intent, cost, target validation, and aggro contract.
- `core/status.hpp`: active statuses, build-up control, and instant buffs.
- `core/item.hpp`: item contract, equipment schema, sockets, affixes, and set bonuses.
- `core/trigger.hpp`: generic trigger vocabulary.
- `core/evolution_profile.hpp`: species progression profiles.
- `core/evolution.hpp`: low-level rule evaluator.
- `core/stat.hpp`: primary and derived stat calculations.
- `core/experience.hpp`: level progression and experience curve contracts.
- `core/recovery.hpp`: action recovery timing calculations.
- `core/status.hpp`: status definitions and active status tables.
- `core/entity.hpp`: live entity state table.
- `core/zone.hpp`: zone state and activity.
- `core/event.hpp`: delayed event scheduler.
- `core/numeric.hpp`: checked/saturating integer operations for domain boundaries.
- `core/runtime.hpp`: aggregate world runtime.

For now these stay in `core` because there is no persistence layer or external content pipeline yet. When that exists, species data and evolution profiles are the first candidates for a content layer split, while runtime remains in `core`.

## Architectural Rules

1. Separate definition from runtime state.
2. Keep `EntityId` for the live instance and `SpeciesId` for the static definition.
3. Use zones to reduce active simulation cost in empty areas.
4. Prefer small trigger primitives combined into profiles instead of one-off special cases.
5. Keep rare or global behaviors as layered rules, not as ad hoc exceptions in the loop.
6. Avoid introducing persistence or networking concerns into `core` until the runtime model is stable.

## Development Rhythm

1. Decide the smallest concept that needs to exist.
2. Add the minimal type or table for that concept.
3. Compile immediately.
4. If the concept touches runtime, keep the runtime API small.
5. If a change expands into many exceptions, stop and split the model instead of forcing it.

## What We Should Build Next

1. Shared entity definition and state model.
2. Status, stat, action, skill, combat, and recovery calculation layer.
3. Simple lifecycle and combat hooks.
4. Then let evolution profiles consume those signals.
5. Keep `CHANGELOG.md` updated whenever a code change lands.

## Spec-Driven Plan

1. Freeze the stat vocabulary before adding more modifiers.
2. Define one contract for stat sources: base entity stats, equipment, statuses, passive effects, and temporary combat effects.
3. Introduce a dedicated modifier block instead of stretching `status::Modifiers` or `item::EffectProfile` into every future use case.
4. Keep recomputation pure: aggregate sources, derive effective stats, clamp resources, and only then apply side effects.
5. Write example tables for the formulas and modifier order before changing the runtime implementation.
6. Add equipment modifiers only after the spec defines how flat, percent, and boolean modifiers combine.

## Stats Contract Notes

- `stat::derive` should remain the deterministic base formula for primary -> derived conversion.
- `stat::Derived::max_weight` now represents the load capacity used by future weight penalties.
- `entity::Inventory` now stores live item instances and `entity::sync_load` can derive carried weight from them.
- `entity::Load` now carries current carried weight and encumbrance outputs, and `entity::refresh_record` applies the move-speed side of that penalty directly.
- `item::calculate_item_weight` and `item::calculate_inventory_weight` give the runtime a single, deterministic way to turn item instances into carried weight.
- `core/recovery.hpp` can consume encumbrance when calculating recovery tempo, so load can affect recovery without hardwiring it into every action profile.
- `experience::required_for_level` defines the tiered ladder curve for player leveling, while `experience::reward_for_level` defines the NPC/monster reward baseline.
- `entity::resolve_experience_reward` uses explicit blueprint rewards when present, falling back to the level-based reward curve.
- `entity::Progression` stores level and experience for all entities, but only players should consume gain logic in runtime.
- `item::EquipmentDefinition` now carries direct attack/defense, use requirements, weight, sockets, affix capacity, and an affix pool.
- `item::EquipmentAffixInstance` now represents the rolled live affix, and `item::make_equipment_affix_instance` scales rule data by tier.
- `item::Instance` now stores equipment affixes separately from the static item definition.
- `item::EffectProfile` still exists for unique/set-style effects, but it is not expressive enough by itself for the next wave of modifiers.
- New modifier variables should be grouped by layer: flat primary, flat derived, percent-derived, and boolean flags.
- The canonical evaluation order should be explicit in the spec before any code expands the stat model.

Combat target priority should remain explicit: player-selected target by default, forced-target status when taunt is active, and threat table selection for monsters.

Status direction is now split clearly: build-up ailments for negative control and direct buffs for shield, haste, regeneration, bless, and resistant.

Skill direction is split in two axes:

- activation mode: active, passive, or trigger
- sequence pattern: chain, initial, finisher, or evolve

Future skill fusion should reuse the same skill catalog and relation model instead of introducing a separate one-off system.

## Build Boundaries

- `mmo_core` is the header-only foundation target.
- `mmo_persistence` owns compiled persistence/content adapters such as the optional Lua loader.
- `mmo_server` is the executable bootstrap target. Its current `main` remains minimal and is not yet a running authoritative server.
- `mmo_unit_tests` owns deterministic tests that isolate one core contract without the runtime loop or load scaling.
- `mmo_lua_disabled_tests` verifies the deterministic no-Lua adapter contract when `MMO_ENABLE_LUA=OFF`.
- `mmo_lua_integration_tests` crosses filesystem, a real Lua VM, validation, and `item::Catalog` when `MMO_ENABLE_LUA=ON`.
- `mmo_simulation_tests` exercises the headless world step with fixed logical time and no server loop.
- `mmo_stress_tests` retains integration, regression, and load scenarios while the suite is migrated incrementally.
- Shared assertions, execution, timing, and reporting live in `src/mmo/tests/support/test.hpp`; no external test dependency is required yet.
- The repository requires C++17. Compiler-specific extensions are disabled.

## Simulation Boundary

- `mmo::core` owns state, invariants, and deterministic calculations.
- `mmo::world` owns the stateless orchestration of one logical simulation step.
- `mmo::server` owns wall-clock pacing, sleep, catch-up, lifecycle, logging, and performance measurements.
- `world::step` receives `TickContext::tick_index` and `TickContext::simulation_time`. Gameplay never derives simulation time from wall clock inside the kernel.
- The canonical tick order is scheduled events, entity maintenance, periodic status application, then status expiration/build-up sweep.
- Hash containers may be used for lookup, but their iteration order must not determine simulation results. The current step copies and sorts all entity IDs before traversal.
- Per-tick sorting is a transitional correctness-first implementation. A persistent deterministic active-zone index may replace it after zone activity invariants are authoritative.
- Optional phase observation lets the server retain wall-clock phase metrics without feeding those measurements back into gameplay decisions.

## Runtime Invariants

- A due scheduled event is never treated as processed merely because it was removed from the scheduler. Infrastructure events mutate world state; domain events enter `World::event_outbox`; invalid or failed transitions enter `World::rejected_events`.
- Periodic status effects use their own `tick_interval`, advance `next_tick_at` after consumption, and catch up deterministically through the expiration boundary.
- Item identity text is owned by `item::Definition`. Content adapters must not publish `string_view` values backed by temporary or reallocating storage.
- The Lua item loader parses and validates the complete source before publishing definitions. A failed atomic load preserves the previous catalog and existing IDs are never overwritten.
- The Lua adapter calls `luaL_openlibs`; repository Lua files are trusted content with access to the standard Lua libraries, not untrusted sandboxed scripts.
- Entity placement can only be changed by `entity::Table`; its zone index must agree with every record after spawn, move, and erase.
- Inventory commands update load authoritatively. Transitional code that mutates `Record::inventory` directly must call `mark_inventory_load_dirty`; the simulation step recalculates a dirty load once and never polls clean inventory weight.
- Resource, damage, stat, stack, and modifier arithmetic saturates at the destination type instead of relying on signed overflow or narrowing casts.
- The server loop derives simulation deadlines from `(epoch, tick index, rate)`, so fractional tick durations do not accumulate drift, then passes each deadline explicitly to `world::step`.
- A paced loop keeps at most `LoopConfig::max_catch_up_ticks` overdue steps. Older steps are explicitly counted in `LoopStats::ticks_skipped`; lag, total/max tick work, and event/entity/status phase time are observable.
