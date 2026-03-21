# API Reference

> **Header:** `#include "mhealth.h"` · **Version:** 1.0.0

## Key callbacks
- `mhealth_collect_fn` — `int32_t (*)(void *ctx)` — reads a metric
- `mhealth_alert_fn` — `void (*)(const mhealth_alert_t *, void *ctx)` — handles alerts

## Alert timing
Fires on severity TRANSITIONS only. Same severity = no alert.

## Thread safety
Not thread-safe. Tick from one thread. Collectors must not block.
