#include "mhealth.h"

#include <string.h>

typedef struct {
    uint32_t now_ms;
    int32_t heap_free;
} app_state_t;

static uint32_t consumer_clock(void *ctx)
{
    const app_state_t *state = (const app_state_t *)ctx;
    return state->now_ms;
}

static mhealth_collect_result_t consumer_collect(void *ctx, int32_t *out_value)
{
    const app_state_t *state = (const app_state_t *)ctx;
    *out_value = state->heap_free;
    return MHEALTH_COLLECT_OK;
}

int main(void)
{
    app_state_t app = { 100U, 9000 };
    mhealth_t hm;
    mhealth_metric_slot_t metric_slots[1];
    mhealth_config_t config;
    mhealth_metric_config_t metric;
    mhealth_check_result_t result;
    size_t index = 0U;

    memset(&config, 0, sizeof(config));
    memset(&metric, 0, sizeof(metric));
    config.metric_slots = metric_slots;
    config.metric_capacity = 1U;
    config.clock_fn = consumer_clock;
    config.clock_ctx = &app;
    if (mhealth_init(&hm, &config) != MHEALTH_OK) {
        return 1;
    }

    metric.name = "heap";
    metric.metric_id = MHEALTH_METRIC_HEAP_FREE;
    metric.collect_fn = consumer_collect;
    metric.collect_ctx = &app;
    metric.direction = MHEALTH_BELOW;
    metric.warn_threshold = 4000;
    metric.critical_threshold = 1000;
    if (mhealth_register(&hm, &metric, &index) != MHEALTH_OK) {
        return 2;
    }

    if (mhealth_check_now(&hm, &result) != MHEALTH_OK) {
        return 3;
    }

    return (index == 0U && result.performed) ? 0 : 4;
}
