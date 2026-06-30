/*
 * microhealth - System health monitor for embedded systems.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef MICROHEALTH_MHEALTH_H_INCLUDED
#define MICROHEALTH_MHEALTH_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MHEALTH_OK = 0,
    MHEALTH_ERR_NULL = -1,
    MHEALTH_ERR_INVALID = -2,
    MHEALTH_ERR_FULL = -3,
    MHEALTH_ERR_NOT_FOUND = -4,
    MHEALTH_ERR_STATE = -5,
    MHEALTH_ERR_BUSY = -6,
    MHEALTH_ERR_TRUNCATED = -7
} mhealth_err_t;

const char *mhealth_err_str(mhealth_err_t err);

typedef enum {
    MHEALTH_METRIC_HEAP_FREE = 0x01,
    MHEALTH_METRIC_HEAP_MIN = 0x02,
    MHEALTH_METRIC_HEAP_ALLOCS = 0x03,
    MHEALTH_METRIC_STACK_FREE = 0x10,
    MHEALTH_METRIC_UPTIME_S = 0x20,
    MHEALTH_METRIC_ERROR_COUNT = 0x30,
    MHEALTH_METRIC_MCU_TEMP = 0x40,
    MHEALTH_METRIC_VBATT = 0x50,
    MHEALTH_METRIC_CUSTOM_BASE = 0x80
} mhealth_metric_id_t;

typedef enum {
    MHEALTH_ABOVE = 0,
    MHEALTH_BELOW = 1
} mhealth_direction_t;

typedef enum {
    MHEALTH_SEVERITY_OK = 0,
    MHEALTH_SEVERITY_WARN = 1,
    MHEALTH_SEVERITY_CRITICAL = 2
} mhealth_severity_t;

const char *mhealth_severity_str(mhealth_severity_t severity);

typedef enum {
    MHEALTH_SAMPLE_UNSAMPLED = 0,
    MHEALTH_SAMPLE_VALID = 1,
    MHEALTH_SAMPLE_DISABLED = 2,
    MHEALTH_SAMPLE_COLLECTION_FAILED = 3
} mhealth_sample_status_t;

typedef enum {
    MHEALTH_COLLECT_OK = 0,
    MHEALTH_COLLECT_UNAVAILABLE = 1,
    MHEALTH_COLLECT_ERROR = -1
} mhealth_collect_result_t;

typedef uint32_t (*mhealth_clock_fn)(void *ctx);
typedef mhealth_collect_result_t (*mhealth_collect_fn)(void *ctx, int32_t *out_value);

typedef struct {
    uint8_t metric_id;
    int32_t value;
    mhealth_severity_t severity;
    mhealth_sample_status_t status;
} mhealth_sample_t;

typedef struct {
    uint32_t timestamp_ms;
    size_t sample_count;
} mhealth_snapshot_meta_t;

typedef struct {
    const char *name;
    uint8_t metric_id;
    mhealth_collect_fn collect_fn;
    void *collect_ctx;
    mhealth_direction_t direction;
    int32_t warn_threshold;
    int32_t critical_threshold;
} mhealth_metric_config_t;

typedef struct {
    size_t metric_index;
    uint8_t metric_id;
    const char *name;
    int32_t value;
    mhealth_direction_t direction;
    int32_t warn_threshold;
    int32_t critical_threshold;
    mhealth_severity_t previous_severity;
    mhealth_severity_t current_severity;
    uint32_t timestamp_ms;
} mhealth_alert_t;

typedef void (*mhealth_alert_fn)(const mhealth_alert_t *alert, void *ctx);

typedef struct {
    const char *name;
    uint8_t metric_id;
    mhealth_collect_fn collect_fn;
    void *collect_ctx;
    mhealth_direction_t direction;
    int32_t warn_threshold;
    int32_t critical_threshold;
    bool enabled;
    mhealth_sample_t current_sample;
    int32_t last_valid_value;
    mhealth_severity_t last_valid_severity;
    mhealth_sample_t pending_sample;
    mhealth_severity_t pending_previous_severity;
    bool pending_emit_alert;
} mhealth_metric_slot_t;

typedef struct {
    mhealth_snapshot_meta_t meta;
} mhealth_history_meta_t;

typedef struct {
    mhealth_metric_slot_t *metric_slots;
    size_t metric_capacity;
    mhealth_history_meta_t *history_meta;
    mhealth_sample_t *history_samples;
    size_t history_capacity;
    mhealth_clock_fn clock_fn;
    void *clock_ctx;
    mhealth_alert_fn alert_fn;
    void *alert_ctx;
    uint32_t check_interval_ms;
} mhealth_config_t;

typedef struct {
    uint32_t check_count;
    uint32_t transition_count;
    uint32_t collection_failure_count;
} mhealth_counters_t;

typedef struct {
    bool healthy;
    mhealth_severity_t worst_severity;
    size_t metric_count;
    size_t enabled_metric_count;
    size_t sampled_metric_count;
    size_t collection_failure_count;
} mhealth_health_status_t;

typedef struct {
    const char *name;
    uint8_t metric_id;
    bool enabled;
    mhealth_direction_t direction;
    int32_t warn_threshold;
    int32_t critical_threshold;
    int32_t last_valid_value;
    mhealth_sample_t current_sample;
} mhealth_metric_status_t;

typedef struct {
    bool performed;
    size_t transition_count;
    size_t collection_failure_count;
    uint32_t timestamp_ms;
} mhealth_check_result_t;

typedef struct {
    mhealth_metric_slot_t *metric_slots;
    size_t metric_capacity;
    size_t metric_count;
    mhealth_history_meta_t *history_meta;
    mhealth_sample_t *history_samples;
    size_t history_capacity;
    size_t history_head;
    size_t history_count;
    mhealth_clock_fn clock_fn;
    void *clock_ctx;
    mhealth_alert_fn alert_fn;
    void *alert_ctx;
    uint32_t check_interval_ms;
    uint32_t last_check_ms;
    bool has_last_check;
    bool initialized;
    bool busy;
    mhealth_snapshot_meta_t latest_meta;
    mhealth_counters_t counters;
} mhealth_t;

mhealth_err_t mhealth_init(mhealth_t *hm, const mhealth_config_t *config);
mhealth_err_t mhealth_set_alert(mhealth_t *hm, mhealth_alert_fn alert_fn, void *alert_ctx);
mhealth_err_t mhealth_register(
    mhealth_t *hm,
    const mhealth_metric_config_t *metric,
    size_t *out_index);
mhealth_err_t mhealth_enable(mhealth_t *hm, size_t index, bool enabled);
mhealth_err_t mhealth_tick(mhealth_t *hm, mhealth_check_result_t *out_result);
mhealth_err_t mhealth_check_now(mhealth_t *hm, mhealth_check_result_t *out_result);
mhealth_err_t mhealth_clear_history(mhealth_t *hm);

mhealth_err_t mhealth_get_counters(const mhealth_t *hm, mhealth_counters_t *out_counters);
mhealth_err_t mhealth_get_metric_count(const mhealth_t *hm, size_t *out_count);
mhealth_err_t mhealth_get_metric_status(
    const mhealth_t *hm,
    size_t index,
    mhealth_metric_status_t *out_status);
mhealth_err_t mhealth_get_health(
    const mhealth_t *hm,
    mhealth_health_status_t *out_status);
mhealth_err_t mhealth_get_latest(
    const mhealth_t *hm,
    mhealth_snapshot_meta_t *out_meta,
    mhealth_sample_t *out_samples,
    size_t sample_capacity,
    size_t *out_sample_count);
mhealth_err_t mhealth_get_history_count(const mhealth_t *hm, size_t *out_count);
mhealth_err_t mhealth_get_history(
    const mhealth_t *hm,
    size_t offset,
    mhealth_snapshot_meta_t *out_meta,
    mhealth_sample_t *out_samples,
    size_t sample_capacity,
    size_t *out_sample_count);

mhealth_err_t mhealth_snapshot_format(
    const mhealth_t *hm,
    const mhealth_snapshot_meta_t *meta,
    const mhealth_sample_t *samples,
    size_t sample_count,
    char *buf,
    size_t buf_size,
    size_t *out_written,
    size_t *out_required);

#ifdef __cplusplus
}
#endif

#endif /* MICROHEALTH_MHEALTH_H_INCLUDED */
