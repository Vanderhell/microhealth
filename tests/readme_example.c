#include "mhealth.h"

#include <string.h>

typedef struct {
    uint32_t now_ms;
    int32_t heap_free;
} app_state_t;

static uint32_t app_clock(void *ctx)
{
    const app_state_t *state = (const app_state_t *)ctx;
    return state->now_ms;
}

static mhealth_collect_result_t collect_heap(void *ctx, int32_t *out_value)
{
    const app_state_t *state = (const app_state_t *)ctx;
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
    mhealth_health_status_t health;
    mhealth_snapshot_meta_t meta;
    mhealth_sample_t samples[1];
    size_t sample_count = 0U;
    size_t heap_index = 0U;

    memset(&config, 0, sizeof(config));
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

    memset(&metric, 0, sizeof(metric));
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

    if (mhealth_check_now(&hm, &result) != MHEALTH_OK) {
        return 3;
    }
    if (mhealth_get_health(&hm, &health) != MHEALTH_OK) {
        return 4;
    }
    if (mhealth_get_latest(&hm, &meta, samples, 1U, &sample_count) != MHEALTH_OK) {
        return 5;
    }
    return (heap_index == 0U && result.performed && sample_count == 1U) ? 0 : 6;
}
