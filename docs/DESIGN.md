# Design Notes

## Stable ABI

The public API no longer relies on layout-changing macros. Metric capacity and history capacity are runtime configuration values stored in the instance, and all public capacities and indexes use `size_t`.

## Two-Phase Checks

Each check is split into:

1. Phase A: sample the clock once, collect every enabled metric, and compute pending sample states and transitions.
2. Phase B: commit latest state, optional history, and counters, then deliver alerts in registration order.

This ensures callback queries observe a complete snapshot.

## Failure Semantics

Collector failures are explicit states, not values. A failed collection preserves the last valid value internally but marks the current sample as `MHEALTH_SAMPLE_COLLECTION_FAILED`.

Threshold comparisons are inclusive. Equal warn/critical thresholds are rejected and the library does not synthesize intermediate WARN transitions or apply hysteresis.

## Reentrancy Guard

The busy guard blocks same-instance mutation during checks and callbacks. It is not a mutex and does not make the library thread-safe.

## Volatile History

History is caller-provided RAM. It survives only until reinitialization, reset, or power loss.

Latest state is stored independently of history, so `mhealth_get_latest` remains usable when history capacity is zero.
