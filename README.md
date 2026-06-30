# microhealth

[![CI](https://github.com/Vanderhell/microhealth/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/Vanderhell/microhealth/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-yellow.svg)](LICENSE)

`microhealth` is a small C health-check library for embedded and low-level systems. It uses caller-owned storage, synchronous collectors, explicit sample states, and deterministic threshold evaluation.

## Status

- C99 library target with C11 test coverage.
- C++ header inclusion and C-built library consumption covered.
- Zero third-party runtime dependencies.
- Snapshot formatting uses the C library `snprintf`.
- No heap allocation inside the library.
- No tag-backed `1.0.0` release exists in this repository history. Current repairs are tracked as `Unreleased`.

## Core Contracts

- Public structure layout does not change with consumer macros.
- `mhealth_init` takes caller-owned metric storage and optional caller-owned history storage.
- Metric names are borrowed. They must remain valid and immutable until reinitialization.
- Collectors return `MHEALTH_COLLECT_OK`, `MHEALTH_COLLECT_UNAVAILABLE`, or `MHEALTH_COLLECT_ERROR`.
- Collection failures are not numeric values and do not synthesize `WARN` or `CRITICAL`.
- ABOVE thresholds are inclusive: `warn <= value < critical`, `critical <= value`.
- BELOW thresholds are inclusive: `warn >= value > critical`, `critical >= value`.
- Equal warn/critical thresholds are rejected.
- `mhealth_tick` samples the clock once per attempted tick.
- Rate-limited skips report success with `performed == false` and do not collect, update history, or fire alerts.
- Alert callbacks run only after the full latest snapshot, history entry, and counters are committed.
- Same-instance mutating reentry from collectors and alert callbacks returns `MHEALTH_ERR_BUSY`.
- Query APIs remain callable during callbacks.
- Disabled, unsampled, valid, and collection-failed samples are distinct states.
- History is volatile RAM only. It is lost on reinitialization, reset, or power loss.
- One `mhealth_t` is not thread-safe. The caller must serialize same-instance access.
- ISR use is unsupported unless the platform guarantees no concurrent access and ISR-safe callbacks/collectors.

## Quick Start

```c
#include "mhealth.h"
#include <string.h>

typedef struct {
    uint32_t now_ms;
    int32_t heap_free;
} app_state_t;

static uint32_t app_clock(void *ctx)
{
    app_state_t *state = (app_state_t *)ctx;
    return state->now_ms;
}

static mhealth_collect_result_t collect_heap(void *ctx, int32_t *out_value)
{
    app_state_t *state = (app_state_t *)ctx;
    *out_value = state->heap_free;
    return MHEALTH_COLLECT_OK;
}

static void on_alert(const mhealth_alert_t *alert, void *ctx)
{
    (void)alert;
    (void)ctx;
}

int main(void)
{
    app_state_t app = { 1000U, 6000 };
    mhealth_t hm;
    mhealth_metric_slot_t metric_slots[1];
    mhealth_history_meta_t history_meta[2];
    mhealth_sample_t history_samples[2];
    mhealth_config_t config;
    mhealth_metric_config_t metric;
    mhealth_check_result_t result;
    size_t heap_index = 0U;

    memset(&config, 0, sizeof(config));
    memset(&metric, 0, sizeof(metric));
    config.metric_slots = metric_slots;
    config.metric_capacity = 1U;
    config.history_meta = history_meta;
    config.history_samples = history_samples;
    config.history_capacity = 2U;
    config.clock_fn = app_clock;
    config.clock_ctx = &app;
    config.alert_fn = on_alert;
    config.alert_ctx = &app;
    config.check_interval_ms = 100U;

    if (mhealth_init(&hm, &config) != MHEALTH_OK) {
        return 1;
    }

    metric.name = "heap_free";
    metric.metric_id = MHEALTH_METRIC_HEAP_FREE;
    metric.collect_fn = collect_heap;
    metric.collect_ctx = &app;
    metric.direction = MHEALTH_BELOW;
    metric.warn_threshold = 4000;
    metric.critical_threshold = 1000;
    if (mhealth_register(&hm, &metric, &heap_index) != MHEALTH_OK) {
        return 2;
    }

    if (mhealth_tick(&hm, &result) != MHEALTH_OK) {
        return 3;
    }

    return (heap_index == 0U && result.performed) ? 0 : 4;
}
```

The compiled copy of this example lives in `tests/readme_example.c`.

## Build And Test

### CMake

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Makefile

```sh
make -C tests clean
make -C tests
```

## Package Consumption

Install and consume with `find_package`:

```sh
cmake --install build --prefix install
cmake -S tests/install_consumer -B build/install-consumer -DCMAKE_PREFIX_PATH=$PWD/install
cmake --build build/install-consumer
```

## Documentation

- [API Reference](docs/API_REFERENCE.md)
- [Design Notes](docs/DESIGN.md)
- [Porting Guide](docs/PORTING_GUIDE.md)
- [Changelog](CHANGELOG.md)

## License

MIT - see [LICENSE](LICENSE)
