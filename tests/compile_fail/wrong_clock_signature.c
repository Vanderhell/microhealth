#include "mhealth.h"

static uint32_t wrong_clock(void)
{
    return 0U;
}

int main(void)
{
    mhealth_t hm;
    mhealth_metric_slot_t slots[1];
    mhealth_config_t config;

    config.metric_slots = slots;
    config.metric_capacity = 1U;
    config.history_meta = 0;
    config.history_samples = 0;
    config.history_capacity = 0U;
    config.clock_fn = wrong_clock;
    config.clock_ctx = 0;
    config.alert_fn = 0;
    config.alert_ctx = 0;
    config.check_interval_ms = 0U;
    return mhealth_init(&hm, &config);
}
