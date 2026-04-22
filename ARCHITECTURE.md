# SISTEMAS-MMO Architecture Guide

This file is the working agreement for the project. Read it before making architecture changes or adding new core systems.

## Current Focus

1. Build a stable shared entity foundation for players, NPCs, and monsters.
2. Keep state, definitions, and evaluation separated.
3. Use evolution as a consumer of world state, not as the center of the whole design.
4. Keep the codebase small, explicit, and easy to expand.

## Current Core Boundaries

- `core/species.hpp`: static species catalog.
- `core/trigger.hpp`: generic trigger vocabulary.
- `core/evolution_profile.hpp`: species progression profiles.
- `core/evolution.hpp`: low-level rule evaluator.
- `core/stat.hpp`: primary and derived stat calculations.
- `core/status.hpp`: status definitions and active status tables.
- `core/entity.hpp`: live entity state table.
- `core/zone.hpp`: zone state and activity.
- `core/event.hpp`: delayed event scheduler.
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
2. Status and stat calculation layer.
3. Simple lifecycle and combat hooks.
4. Then let evolution profiles consume those signals.
