# API Reference

Header: `#include "mhealth.h"`

## Initialization

`mhealth_init(mhealth_t *hm, const mhealth_config_t *config)`

- `metric_slots` and `metric_capacity` are required.
- `clock_fn` is required.
- `history_capacity == 0` disables history and requires `history_meta == NULL` and `history_samples == NULL`.
- `history_capacity > 0` requires caller-owned `history_meta` and flattened `history_samples`.
- Reinitialization during an active check or callback returns `MHEALTH_ERR_BUSY`.

## Registration

`mhealth_register(mhealth_t *hm, const mhealth_metric_config_t *metric, size_t *out_index)`

- Rejects null output pointers.
- Rejects empty names, null collectors, duplicate names, duplicate IDs, invalid direction values, equal thresholds, and undocumented reserved IDs below `MHEALTH_METRIC_CUSTOM_BASE`.
- Successful registration leaves the metric enabled and unsampled.
- Failed registration does not change metric count, history, counters, or alert state.

## Collection

Collectors use:

```c
typedef mhealth_collect_result_t (*mhealth_collect_fn)(void *ctx, int32_t *out_value);
```

- `MHEALTH_COLLECT_OK` writes a metric value.
- `MHEALTH_COLLECT_UNAVAILABLE` and `MHEALTH_COLLECT_ERROR` are non-numeric sample states.
- Enabled metrics with failed or unsampled current data make aggregate health unhealthy.

## Checks

`mhealth_tick` and `mhealth_check_now` return `mhealth_err_t` and fill `mhealth_check_result_t`.

- `performed == false` means a rate-limited skip.
- `transition_count` counts severity transitions even if no alert callback is installed.
- `collection_failure_count` counts failed collectors in that check.
- `mhealth_check_now` bypasses the interval and becomes the next rate-limit reference.

## Alerts

`mhealth_alert_t` contains the metric index, ID, name, value, direction, warn threshold, critical threshold, previous severity, current severity, and timestamp.

- Alert delivery is synchronous and best-effort.
- Alert pointers are valid only during the callback.
- Same-instance mutating operations from callbacks return `MHEALTH_ERR_BUSY`.
- Query APIs are allowed during callbacks and observe fully committed state.

## Query APIs

- `mhealth_get_metric_count`
- `mhealth_get_metric_status`
- `mhealth_get_health`
- `mhealth_get_latest`
- `mhealth_get_history_count`
- `mhealth_get_history`
- `mhealth_get_counters`

These APIs never encode invalid use as healthy success. Uninitialized instances return `MHEALTH_ERR_STATE`.

## Sample States

- `MHEALTH_SAMPLE_UNSAMPLED`
- `MHEALTH_SAMPLE_VALID`
- `MHEALTH_SAMPLE_DISABLED`
- `MHEALTH_SAMPLE_COLLECTION_FAILED`

History and latest snapshots preserve these states directly.
