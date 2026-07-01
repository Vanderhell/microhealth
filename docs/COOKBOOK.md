# Cookbook

Practical integration patterns for `microhealth`.

## 1. Minimal no-history setup

Use this when you only need the latest result and aggregate health:

```c
mhealth_t hm;
mhealth_metric_slot_t metric_slots[2];
mhealth_config_t config = {0};

config.metric_slots = metric_slots;
config.metric_capacity = 2U;
config.history_meta = NULL;
config.history_samples = NULL;
config.history_capacity = 0U;
config.clock_fn = platform_clock;
config.clock_ctx = platform_ctx;

mhealth_init(&hm, &config);
```

## 2. Fixed-size history ring

Use caller-owned RAM for volatile snapshots:

```c
mhealth_history_meta_t history_meta[8];
mhealth_sample_t history_samples[8 * 4];

config.history_meta = history_meta;
config.history_samples = history_samples;
config.history_capacity = 8U;
config.metric_capacity = 4U;
```

Storage size is `history_capacity * metric_capacity` samples.

## 3. Register a BELOW metric

Use `MHEALTH_BELOW` for resources that become unhealthy when the value drops:

```c
metric.name = "heap_free";
metric.metric_id = MHEALTH_METRIC_HEAP_FREE;
metric.collect_fn = heap_collect;
metric.collect_ctx = heap_ctx;
metric.direction = MHEALTH_BELOW;
metric.warn_threshold = 4096;
metric.critical_threshold = 1024;
```

Contract: `warn_threshold > critical_threshold`.

## 4. Register an ABOVE metric

Use `MHEALTH_ABOVE` for counters or temperatures that become unhealthy when the value rises:

```c
metric.name = "mcu_temp";
metric.metric_id = MHEALTH_METRIC_MCU_TEMP;
metric.collect_fn = temp_collect;
metric.collect_ctx = temp_ctx;
metric.direction = MHEALTH_ABOVE;
metric.warn_threshold = 700;
metric.critical_threshold = 850;
```

Contract: `warn_threshold < critical_threshold`.

## 5. Drive checks from one owner context

Serialize access through one loop, task, or thread:

```c
mhealth_check_result_t result;

if (mhealth_tick(&hm, &result) == MHEALTH_OK && result.performed) {
    /* inspect result.transition_count or query latest state */
}
```

Use `mhealth_check_now` for forced checks that bypass the interval.

## 6. Handle alert callbacks

Alert callbacks are synchronous and observe committed state:

```c
static void on_alert(const mhealth_alert_t *alert, void *ctx)
{
    (void)ctx;
    /* log or mirror alert immediately */
}
```

Do not retain `alert` after the callback returns.

## 7. Read the latest snapshot

```c
mhealth_snapshot_meta_t meta;
mhealth_sample_t samples[4];
size_t sample_count = 0U;

if (mhealth_get_latest(&hm, &meta, samples, 4U, &sample_count) == MHEALTH_OK) {
    /* samples[0..sample_count-1] are a stable copy */
}
```

This works even when history is disabled.

## 8. Format a snapshot

```c
char buf[256];
size_t written = 0U;
size_t required = 0U;
mhealth_err_t rc;

rc = mhealth_snapshot_format(&hm, &meta, samples, sample_count,
                             buf, sizeof(buf), &written, &required);
```

- `MHEALTH_OK`: full formatted output fits.
- `MHEALTH_ERR_TRUNCATED`: buffer prefix is valid and NUL-terminated, but too small.
- `buf == NULL && buf_size == 0`: required-size query.

## 9. Disable and re-enable a metric

```c
mhealth_enable(&hm, metric_index, false);
mhealth_enable(&hm, metric_index, true);
```

- Disable marks the current sample as disabled and emits no recovery event.
- Re-enable marks the metric unsampled.
- The next successful non-OK sample emits a fresh transition.

## 10. C++ consumer

The public header supports C++ inclusion and links against the C-built library:

```cmake
find_package(microhealth CONFIG REQUIRED)
target_link_libraries(app PRIVATE microhealth::microhealth)
```

See `tests/consumer_cpp.cpp`.

## 11. Installed package consumer

After installation:

```sh
cmake --install build --prefix install
cmake -S tests/install_consumer -B build/install-consumer -DCMAKE_PREFIX_PATH=$PWD/install
cmake --build build/install-consumer
```

See `tests/install_consumer/`.

## 12. What not to do

- Do not share one `mhealth_t` across threads without external serialization.
- Do not call same-instance mutating APIs from collectors or alert callbacks.
- Do not treat history as persistent evidence.
- Do not assume disabled or failed samples are valid zero values.

