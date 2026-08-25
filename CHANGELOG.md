# Changelog

All notable code and architecture updates for `SISTEMAS-MMO` are recorded here.

## Unreleased

### Added

- `mmo::world::World` as the authoritative runtime aggregate for spatial mutations, zone population, active-zone indexing, and event dispatch.
- Deterministic sparse simulation tests for authoritative spawn/move/erase, canonical active order, wake/sleep, rejected player-zone sleep, bounded status catch-up, and active-set scale.
- `world::TickContext`, `world::TickStats`, and a deterministic headless `world::step` with explicit logical time and canonical entity traversal.
- A dedicated `mmo_simulation_tests` integration target covering events, dirty load, periodic status cadence, expiration, resource invariants, canonical ordering, and repeated simulation.
- A reusable internal test harness and a separate `mmo_unit_tests` target for deterministic event validation and scheduler tests.
- Conditional Lua OFF and Lua ON integration targets covering the disabled adapter, repository content, validation, numeric bounds, ownership, table sources, collisions, and atomic publication.
- Minimal GitHub Actions CI for GCC warnings-as-errors, Clang ASan/UBSan, and Windows MSVC builds with CTest.
- Reproducible CMake build with separate core, persistence, server, and stress-test targets plus CTest integration.
- Cross-compiler warning options and opt-in ASan, UBSan, TSan, warnings-as-errors, and Lua build switches.
- `core/numeric.hpp` with saturating arithmetic for resource, damage, stat, modifier, meter, and stack boundaries.
- Fixed-timestep regressions for 60 Hz precision, deterministic scheduled time, bounded catch-up, and dirty load synchronization.

- `core/item.hpp` to define the item contract split into material, consumable, equipment, quest, and collectible item types.
- `core/logger.hpp` with a lightweight logger and exception helpers for stress testing and runtime diagnostics.
- `persistence/context.hpp` as the seed for database context initialization and storage layout.
- `server/loop.hpp` with a real tick loop that processes events, load sync, and status sweeps.
- Stress tests in `src/mmo/tests/stress_tests.cpp` that run heavy loops and log failures.
- `core/experience.hpp` for tiered experience curves, rewards, and gain progression.
- `core/entity.hpp` progression state for level and experience, plus player-only gain logic.
- `core/entity.hpp` experience reward helper for NPC/monster blueprints.
- `core/item.hpp` equipment contracts for direct attack/defense, use requirements, weight, modification slots, affix slots, and modifier pools.
- `core/item.hpp` equipment affix instances and tier-based scaling helpers.
- `core/item.hpp` deterministic item and inventory weight helpers.
- `core/stat.hpp` `max_weight` as a derived stat for load capacity plus an encumbrance helper for load-based movement and recovery effects.
- `core/entity.hpp` load state on the live entity contract so carried weight can be tracked by the runtime.
- `core/entity.hpp` a minimal inventory contract plus a helper to sync carried weight from live item instances.
- `core/recovery.hpp` recovery overloads that can consume encumbrance alongside derived stats.
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

- Entity, zone, and zone-membership storage now uses ordered `std::map`/`std::set` indexes with worst-case `O(log N)` lookup and mutation; the active hot path is canonical `(ZoneId, EntityId)` without a global per-tick sort.
- Spatial `entity::Table` mutations are private to `mmo::world::World`; player presence and explicit wake requests now share one zone-activity definition, and sleep with players is rejected.
- Sleeping zones skip full entity work. Periodic status catch-up on wake is bounded to four applications per status before absolute-time expiration is swept.
- `server::run_loop` now acts as the pacing adapter around `world::step` and aggregates logical tick stats while retaining wall-clock phase metrics.
- Periodic status resource mutation now crosses an explicit `entity::Table` boundary and records periodic deaths using simulation time.
- Lua adapter coverage moved out of the stress executable into conditional integration targets; GCC/Linux CI installs Lua 5.4 and exercises `MMO_ENABLE_LUA=ON`.
- Basic event contract tests moved out of the mixed stress executable; volume and runtime-dispatch scenarios remain there.
- Fixed the server aggregate header include and made the existing stress harness C++17-compatible.
- Documented verified build and test commands in `README.md` and target boundaries in `ARCHITECTURE.md`.
- Due events now cross an explicit dispatch boundary: migrations and zone activity update the world, domain notifications are retained in an outbox, and failed events are retained for diagnosis.
- Periodic statuses now honor their configured cadence, consume each deadline once, and catch up deterministically after delayed ticks.
- Item names and descriptions are owned strings, removing the Lua loader's process-global lifetime workaround.
- Added regressions for event dispatch, periodic status timing, and item identity ownership.
- Entity placement mutation is controlled by `entity::Table`, with validation and a diagnostic that checks record/index agreement.
- Inventory addition is atomic on rejection and reports partial success only when a partial stack was actually committed.
- Health, mana, damage, derived stats, status modifiers, stacks, and build-up calculations now clamp or saturate at numeric boundaries.
- The server loop now uses drift-free tick deadlines, deterministic simulation time, bounded catch-up, explicit skipped/late tick counters, and total/max/per-phase timing metrics.
- Inventory load uses a dirty flag, replacing full inventory-weight recalculation on every tick; direct transitional mutations have an explicit invalidation API.

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
