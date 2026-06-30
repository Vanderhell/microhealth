#include "mhealth.h"

static uint32_t cpp_clock(void *ctx) {
    (void)ctx;
    return 0U;
}

static mhealth_collect_result_t cpp_collect(void *ctx, int32_t *out_value) {
    (void)ctx;
    *out_value = 0;
    return MHEALTH_COLLECT_OK;
}

int main() {
    mhealth_t hm{};
    mhealth_metric_slot_t metric_slots[1]{};
    mhealth_config_t config{};
    mhealth_metric_config_t metric{};
    size_t index = 0U;

    config.metric_slots = metric_slots;
    config.metric_capacity = 1U;
    config.clock_fn = cpp_clock;

    if (mhealth_init(&hm, &config) != MHEALTH_OK) {
        return 1;
    }

    metric.name = "cpp";
    metric.metric_id = MHEALTH_METRIC_HEAP_FREE;
    metric.collect_fn = cpp_collect;
    metric.direction = MHEALTH_ABOVE;
    metric.warn_threshold = 1;
    metric.critical_threshold = 2;
    if (mhealth_register(&hm, &metric, &index) != MHEALTH_OK) {
        return 2;
    }

    return index == 0U ? 0 : 3;
}
