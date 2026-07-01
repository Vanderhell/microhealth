# Changelog

## Unreleased

## 1.0.0 - 2026-07-01

### Changed

- Replaced macro-sized public layouts with caller-owned runtime storage configuration.
- Added explicit initialization, state, busy, truncation, and status-returning check/query APIs.
- Switched collectors to explicit success/failure result codes and preserved sample-status fidelity in latest and historical snapshots.
- Committed checks in two phases so callbacks observe complete state.
- Added deterministic reentrancy rejection for same-instance mutation during collection and alert delivery.
- Added CMake builds, install/export packaging, compile-fail fixtures, C++ consumer coverage, and external `find_package` consumer validation.
- Rewrote public docs around actual thresholds, rate limits, callback timing, volatile history, and execution limits.
