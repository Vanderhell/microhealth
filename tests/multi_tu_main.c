#include "mhealth.h"

#include <string.h>

uint32_t multi_clock(void *ctx);
mhealth_collect_result_t multi_collect(void *ctx, int32_t *out_value);

int main(void)
{
    mhealth_t hm;
    mhealth_metric_slot_t metric_slots[1];
    mhealth_config_t config;
    mhealth_metric_config_t metric;
    size_t index = 0U;

    memset(&config, 0, sizeof(config));
    memset(&metric, 0, sizeof(metric));
    config.metric_slots = metric_slots;
    config.metric_capacity = 1U;
    config.history_meta = 0;
    config.history_samples = 0;
    config.history_capacity = 0U;
    config.clock_fn = multi_clock;
    config.clock_ctx = 0;
    config.alert_fn = 0;
    config.alert_ctx = 0;
    config.check_interval_ms = 0U;
    if (mhealth_init(&hm, &config) != MHEALTH_OK) {
        return 1;
    }

    metric.name = "x";
    metric.metric_id = MHEALTH_METRIC_HEAP_FREE;
    metric.collect_fn = multi_collect;
    metric.collect_ctx = 0;
    metric.direction = MHEALTH_ABOVE;
    metric.warn_threshold = 1;
    metric.critical_threshold = 2;
    if (mhealth_register(&hm, &metric, &index) != MHEALTH_OK) {
        return 2;
    }
    return index == 0U ? 0 : 3;
}
