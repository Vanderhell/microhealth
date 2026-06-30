#include "mhealth.h"

static int32_t wrong_collect(void *ctx)
{
    (void)ctx;
    return 0;
}

int main(void)
{
    mhealth_metric_config_t metric;
    metric.name = "x";
    metric.metric_id = MHEALTH_METRIC_HEAP_FREE;
    metric.collect_fn = wrong_collect;
    metric.collect_ctx = 0;
    metric.direction = MHEALTH_ABOVE;
    metric.warn_threshold = 1;
    metric.critical_threshold = 2;
    return 0;
}
