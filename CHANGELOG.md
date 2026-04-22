# Changelog

All notable code and architecture updates for `SISTEMAS-MMO` are recorded here.

## Unreleased

### Added
- `core/action.hpp` to define the live action contract: action kind, outcome, state, profile, and catalog.
- `CHANGELOG.md` as the running log for future code changes and additions.

### Changed
- `core/recovery.hpp` now follows the action contract and computes animation and recovery from stats, action weight, and hit/whiff outcome.
- `core/entity.hpp` keeps the live action state on the entity contract.
- Core documentation now tracks the action/recovery split.