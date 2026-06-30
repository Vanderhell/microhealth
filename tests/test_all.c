/*
 * microhealth runtime contract tests.
 */

#include "mhealth.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT_METRIC_CAPACITY 8U
#define DEFAULT_HISTORY_CAPACITY 8U
#define LARGE_CAPACITY 300U
#define ALERT_LOG_CAPACITY 32U

typedef int (*test_fn_t)(void);

static unsigned long g_tests_run = 0UL;
static unsigned long g_tests_passed = 0UL;
static unsigned long g_tests_failed = 0UL;
static unsigned long g_assertions = 0UL;

static int fail_line(const char *file, int line, const char *message)
{
    printf("FAIL\n    %s:%d: %s\n", file, line, message);
    return 0;
}

static int assert_long_eq(long expected, long actual, const char *file, int line, const char *expr)
{
    char buffer[256];
    g_assertions += 1UL;
    if (expected == actual) {
        return 1;
    }
    (void)snprintf(buffer, sizeof(buffer), "%s expected %ld, got %ld", expr, expected, actual);
    return fail_line(file, line, buffer);
}

static int assert_size_eq(size_t expected, size_t actual, const char *file, int line, const char *expr)
{
    char buffer[256];
    g_assertions += 1UL;
    if (expected == actual) {
        return 1;
    }
    (void)snprintf(
        buffer,
        sizeof(buffer),
        "%s expected %lu, got %lu",
        expr,
        (unsigned long)expected,
        (unsigned long)actual);
    return fail_line(file, line, buffer);
}

static int assert_true_impl(int value, const char *file, int line, const char *expr)
{
    char buffer[256];
    g_assertions += 1UL;
    if (value) {
        return 1;
    }
    (void)snprintf(buffer, sizeof(buffer), "%s expected true", expr);
    return fail_line(file, line, buffer);
}

static int assert_false_impl(int value, const char *file, int line, const char *expr)
{
    char buffer[256];
    g_assertions += 1UL;
    if (!value) {
        return 1;
    }
    (void)snprintf(buffer, sizeof(buffer), "%s expected false", expr);
    return fail_line(file, line, buffer);
}

static int assert_str_eq(const char *expected, const char *actual, const char *file, int line)
{
    char buffer[256];
    g_assertions += 1UL;
    if (expected != NULL && actual != NULL && strcmp(expected, actual) == 0) {
        return 1;
    }
    (void)snprintf(
        buffer,
        sizeof(buffer),
        "expected \"%s\", got \"%s\"",
        expected != NULL ? expected : "(null)",
        actual != NULL ? actual : "(null)");
    return fail_line(file, line, buffer);
}

#define ASSERT_INT_EQ(expected, actual) do { \
    long expected_ = (long)(expected); \
    long actual_ = (long)(actual); \
    if (!assert_long_eq(expected_, actual_, __FILE__, __LINE__, #actual)) return 0; \
} while (0)

#define ASSERT_SIZE_EQ(expected, actual) do { \
    size_t expected_ = (expected); \
    size_t actual_ = (actual); \
    if (!assert_size_eq(expected_, actual_, __FILE__, __LINE__, #actual)) return 0; \
} while (0)

#define ASSERT_TRUE(expr) do { \
    int value_ = !!(expr); \
    if (!assert_true_impl(value_, __FILE__, __LINE__, #expr)) return 0; \
} while (0)

#define ASSERT_FALSE(expr) do { \
    int value_ = !!(expr); \
    if (!assert_false_impl(value_, __FILE__, __LINE__, #expr)) return 0; \
} while (0)

#define ASSERT_STR_EQ(expected, actual) do { \
    const char *expected_ = (expected); \
    const char *actual_ = (actual); \
    if (!assert_str_eq(expected_, actual_, __FILE__, __LINE__)) return 0; \
} while (0)

typedef struct {
    uint32_t now;
    size_t call_count;
} clock_state_t;

typedef struct {
    mhealth_collect_result_t result;
    int32_t value;
    size_t call_count;
} collector_state_t;

typedef struct {
    mhealth_t hm;
    mhealth_metric_slot_t metric_slots[DEFAULT_METRIC_CAPACITY];
    mhealth_history_meta_t history_meta[DEFAULT_HISTORY_CAPACITY];
    mhealth_sample_t history_samples[DEFAULT_METRIC_CAPACITY * DEFAULT_HISTORY_CAPACITY];
    clock_state_t clock;
    mhealth_alert_t alert_log[ALERT_LOG_CAPACITY];
    size_t alert_count;
} fixture_t;

typedef struct {
    mhealth_metric_slot_t metric_slots[LARGE_CAPACITY];
    mhealth_history_meta_t history_meta[LARGE_CAPACITY];
    mhealth_sample_t history_samples[LARGE_CAPACITY * 2U];
} large_storage_t;

typedef struct {
    fixture_t *fixture;
    collector_state_t *collector;
    mhealth_err_t mutation_statuses[6];
} busy_probe_t;

typedef struct {
    fixture_t *fixture;
    size_t callback_count;
    mhealth_err_t query_statuses[3];
    mhealth_err_t mutation_statuses[3];
    mhealth_alert_t observed_alert;
    mhealth_snapshot_meta_t latest_meta;
    mhealth_sample_t latest_samples[DEFAULT_METRIC_CAPACITY];
    size_t latest_count;
    mhealth_counters_t counters;
} alert_probe_t;

static uint32_t fixture_clock(void *ctx)
{
    clock_state_t *clock = (clock_state_t *)ctx;
    clock->call_count += 1U;
    return clock->now;
}

static mhealth_collect_result_t fixed_collect(void *ctx, int32_t *out_value)
{
    collector_state_t *collector = (collector_state_t *)ctx;
    collector->call_count += 1U;
    if (collector->result == MHEALTH_COLLECT_OK) {
        *out_value = collector->value;
    }
    return collector->result;
}

static void log_alert(const mhealth_alert_t *alert, void *ctx)
{
    fixture_t *fixture = (fixture_t *)ctx;
    if (fixture->alert_count < ALERT_LOG_CAPACITY) {
        fixture->alert_log[fixture->alert_count] = *alert;
        fixture->alert_count += 1U;
    }
}

static int init_fixture(fixture_t *fixture, size_t history_capacity)
{
    mhealth_config_t config;

    memset(fixture, 0, sizeof(*fixture));
    fixture->clock.now = 1000U;

    memset(&config, 0, sizeof(config));
    config.metric_slots = fixture->metric_slots;
    config.metric_capacity = DEFAULT_METRIC_CAPACITY;
    config.history_meta = history_capacity > 0U ? fixture->history_meta : NULL;
    config.history_samples = history_capacity > 0U ? fixture->history_samples : NULL;
    config.history_capacity = history_capacity;
    config.clock_fn = fixture_clock;
    config.clock_ctx = &fixture->clock;
    config.alert_fn = log_alert;
    config.alert_ctx = fixture;
    config.check_interval_ms = 50U;

    ASSERT_INT_EQ(MHEALTH_OK, mhealth_init(&fixture->hm, &config));
    return 1;
}

static int register_fixed_metric(
    fixture_t *fixture,
    const char *name,
    uint8_t metric_id,
    collector_state_t *collector,
    mhealth_direction_t direction,
    int32_t warn_threshold,
    int32_t critical_threshold,
    size_t *out_index)
{
    mhealth_metric_config_t metric;

    memset(&metric, 0, sizeof(metric));
    metric.name = name;
    metric.metric_id = metric_id;
    metric.collect_fn = fixed_collect;
    metric.collect_ctx = collector;
    metric.direction = direction;
    metric.warn_threshold = warn_threshold;
    metric.critical_threshold = critical_threshold;

    ASSERT_INT_EQ(MHEALTH_OK, mhealth_register(&fixture->hm, &metric, out_index));
    return 1;
}

static int run_case(const char *name, test_fn_t fn)
{
    int ok;
    g_tests_run += 1UL;
    printf("  %-58s ", name);
    ok = fn();
    if (ok) {
        g_tests_passed += 1UL;
        printf("PASS\n");
    } else {
        g_tests_failed += 1UL;
    }
    return ok;
}

static int test_harness_self_test(void)
{
    unsigned long before = g_assertions;
    g_assertions += 1UL;
    if (g_assertions != before + 1UL) {
        return fail_line(__FILE__, __LINE__, "self-test assertion count mismatch");
    }
    return 1;
}

static int test_init_and_registration_validation(void)
{
    mhealth_t hm;
    mhealth_metric_slot_t slots[2];
    mhealth_config_t config;
    collector_state_t collector = { MHEALTH_COLLECT_OK, 10, 0U };
    mhealth_metric_config_t metric;
    size_t index = 0U;

    memset(&hm, 0, sizeof(hm));
    memset(&config, 0, sizeof(config));
    ASSERT_INT_EQ(MHEALTH_ERR_INVALID, mhealth_init(&hm, &config));

    config.metric_slots = slots;
    config.metric_capacity = 2U;
    config.clock_fn = fixture_clock;
    config.clock_ctx = &(clock_state_t){ 0U, 0U };
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_init(&hm, &config));

    memset(&metric, 0, sizeof(metric));
    metric.name = "";
    metric.metric_id = 0x04U;
    metric.collect_fn = fixed_collect;
    metric.collect_ctx = &collector;
    metric.direction = (mhealth_direction_t)9;
    metric.warn_threshold = 1;
    metric.critical_threshold = 0;
    ASSERT_INT_EQ(MHEALTH_ERR_INVALID, mhealth_register(&hm, &metric, &index));

    metric.name = "heap";
    metric.metric_id = MHEALTH_METRIC_HEAP_FREE;
    metric.direction = MHEALTH_BELOW;
    metric.warn_threshold = 100;
    metric.critical_threshold = 10;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_register(&hm, &metric, &index));
    ASSERT_SIZE_EQ(0U, index);
    ASSERT_STR_EQ("heap", hm.metric_slots[0].name);

    metric.name = "other";
    ASSERT_INT_EQ(MHEALTH_ERR_INVALID, mhealth_register(&hm, &metric, &index));
    metric.name = "heap2";
    metric.metric_id = 0x04U;
    ASSERT_INT_EQ(MHEALTH_ERR_INVALID, mhealth_register(&hm, &metric, &index));
    return 1;
}

static int test_large_capacity_and_history_modes(void)
{
    fixture_t fixture;
    collector_state_t collector = { MHEALTH_COLLECT_OK, 8000, 0U };
    mhealth_check_result_t result;
    mhealth_snapshot_meta_t meta;
    mhealth_sample_t samples[DEFAULT_METRIC_CAPACITY];
    size_t sample_count = 0U;
    size_t history_count = 0U;
    large_storage_t storage;
    mhealth_t hm;
    mhealth_config_t config;
    mhealth_metric_config_t metric;
    size_t index = 0U;
    size_t i;

    ASSERT_TRUE(init_fixture(&fixture, 0U));
    ASSERT_TRUE(register_fixed_metric(
        &fixture, "heap", MHEALTH_METRIC_HEAP_FREE, &collector, MHEALTH_BELOW, 400, 100, &index));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&fixture.hm, &result));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_latest(&fixture.hm, &meta, samples, DEFAULT_METRIC_CAPACITY, &sample_count));
    ASSERT_SIZE_EQ(1U, sample_count);
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_history_count(&fixture.hm, &history_count));
    ASSERT_SIZE_EQ(0U, history_count);

    memset(&hm, 0, sizeof(hm));
    memset(&config, 0, sizeof(config));
    memset(&storage, 0, sizeof(storage));
    config.metric_slots = storage.metric_slots;
    config.metric_capacity = LARGE_CAPACITY;
    config.history_meta = storage.history_meta;
    config.history_samples = storage.history_samples;
    config.history_capacity = 2U;
    config.clock_fn = fixture_clock;
    config.clock_ctx = &fixture.clock;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_init(&hm, &config));

    memset(&metric, 0, sizeof(metric));
    metric.collect_fn = fixed_collect;
    metric.collect_ctx = &collector;
    metric.direction = MHEALTH_ABOVE;
    metric.warn_threshold = 600;
    metric.critical_threshold = 700;
    for (i = 0U; i < 120U; ++i) {
        static char names[120][16];
        (void)snprintf(names[i], sizeof(names[i]), "m%lu", (unsigned long)i);
        metric.name = names[i];
        metric.metric_id = (uint8_t)(MHEALTH_METRIC_CUSTOM_BASE + i);
        ASSERT_INT_EQ(MHEALTH_OK, mhealth_register(&hm, &metric, &index));
    }
    ASSERT_SIZE_EQ(120U, hm.metric_count);
    return 1;
}

static int test_threshold_boundaries_and_limits(void)
{
    fixture_t fixture;
    collector_state_t above = { MHEALTH_COLLECT_OK, 0, 0U };
    collector_state_t below = { MHEALTH_COLLECT_OK, 0, 0U };
    mhealth_metric_status_t status;
    size_t index = 0U;

    ASSERT_TRUE(init_fixture(&fixture, 4U));
    ASSERT_TRUE(register_fixed_metric(
        &fixture, "temp", MHEALTH_METRIC_MCU_TEMP, &above, MHEALTH_ABOVE, 10, 20, &index));
    ASSERT_TRUE(register_fixed_metric(
        &fixture, "heap", MHEALTH_METRIC_HEAP_FREE, &below, MHEALTH_BELOW, -10, -20, &index));

    above.value = 9;
    below.value = -9;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&fixture.hm, &(mhealth_check_result_t){ 0 }));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_metric_status(&fixture.hm, 0U, &status));
    ASSERT_INT_EQ(MHEALTH_SEVERITY_OK, status.current_sample.severity);

    above.value = 10;
    below.value = -10;
    fixture.clock.now += 100U;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&fixture.hm, &(mhealth_check_result_t){ 0 }));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_metric_status(&fixture.hm, 0U, &status));
    ASSERT_INT_EQ(MHEALTH_SEVERITY_WARN, status.current_sample.severity);
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_metric_status(&fixture.hm, 1U, &status));
    ASSERT_INT_EQ(MHEALTH_SEVERITY_WARN, status.current_sample.severity);

    above.value = INT32_MAX;
    below.value = INT32_MIN;
    fixture.clock.now += 100U;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&fixture.hm, &(mhealth_check_result_t){ 0 }));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_metric_status(&fixture.hm, 0U, &status));
    ASSERT_INT_EQ(MHEALTH_SEVERITY_CRITICAL, status.current_sample.severity);
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_metric_status(&fixture.hm, 1U, &status));
    ASSERT_INT_EQ(MHEALTH_SEVERITY_CRITICAL, status.current_sample.severity);
    return 1;
}

static int test_collection_failure_and_health_queries(void)
{
    fixture_t fixture;
    collector_state_t heap = { MHEALTH_COLLECT_ERROR, 1000, 0U };
    collector_state_t temp = { MHEALTH_COLLECT_OK, 25, 0U };
    mhealth_check_result_t result;
    mhealth_metric_status_t status;
    mhealth_health_status_t health;
    size_t index = 0U;

    ASSERT_TRUE(init_fixture(&fixture, 4U));
    ASSERT_TRUE(register_fixed_metric(
        &fixture, "heap", MHEALTH_METRIC_HEAP_FREE, &heap, MHEALTH_BELOW, 4000, 1000, &index));
    ASSERT_TRUE(register_fixed_metric(
        &fixture, "temp", MHEALTH_METRIC_MCU_TEMP, &temp, MHEALTH_ABOVE, 70, 85, &index));

    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&fixture.hm, &result));
    ASSERT_SIZE_EQ(1U, result.collection_failure_count);
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_metric_status(&fixture.hm, 0U, &status));
    ASSERT_INT_EQ(MHEALTH_SAMPLE_COLLECTION_FAILED, status.current_sample.status);
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_health(&fixture.hm, &health));
    ASSERT_FALSE(health.healthy);

    heap.result = MHEALTH_COLLECT_OK;
    heap.value = 500;
    fixture.clock.now += 100U;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&fixture.hm, &result));
    ASSERT_SIZE_EQ(1U, result.transition_count);
    ASSERT_SIZE_EQ(1U, fixture.alert_count);
    ASSERT_INT_EQ(MHEALTH_SEVERITY_CRITICAL, fixture.alert_log[0].current_severity);
    return 1;
}

static mhealth_collect_result_t busy_collect(void *ctx, int32_t *out_value)
{
    busy_probe_t *probe = (busy_probe_t *)ctx;
    mhealth_metric_config_t metric;
    mhealth_config_t config;
    mhealth_check_result_t result;
    size_t index = 0U;

    *out_value = probe->collector->value;

    memset(&metric, 0, sizeof(metric));
    metric.name = "late";
    metric.metric_id = MHEALTH_METRIC_CUSTOM_BASE;
    metric.collect_fn = fixed_collect;
    metric.collect_ctx = probe->collector;
    metric.direction = MHEALTH_ABOVE;
    metric.warn_threshold = 1;
    metric.critical_threshold = 2;

    memset(&config, 0, sizeof(config));
    config.metric_slots = probe->fixture->metric_slots;
    config.metric_capacity = DEFAULT_METRIC_CAPACITY;
    config.clock_fn = fixture_clock;
    config.clock_ctx = &probe->fixture->clock;

    probe->mutation_statuses[0] = mhealth_init(&probe->fixture->hm, &config);
    probe->mutation_statuses[1] = mhealth_register(&probe->fixture->hm, &metric, &index);
    probe->mutation_statuses[2] = mhealth_enable(&probe->fixture->hm, 0U, false);
    probe->mutation_statuses[3] = mhealth_set_alert(&probe->fixture->hm, log_alert, probe->fixture);
    probe->mutation_statuses[4] = mhealth_tick(&probe->fixture->hm, &result);
    probe->mutation_statuses[5] = mhealth_clear_history(&probe->fixture->hm);
    return probe->collector->result;
}

static void alert_probe_callback(const mhealth_alert_t *alert, void *ctx)
{
    alert_probe_t *probe = (alert_probe_t *)ctx;
    mhealth_metric_config_t metric;
    mhealth_check_result_t result;
    size_t index = 0U;

    probe->callback_count += 1U;
    probe->observed_alert = *alert;
    probe->query_statuses[0] = mhealth_get_latest(
        &probe->fixture->hm,
        &probe->latest_meta,
        probe->latest_samples,
        DEFAULT_METRIC_CAPACITY,
        &probe->latest_count);
    probe->query_statuses[1] = mhealth_get_counters(&probe->fixture->hm, &probe->counters);
    probe->query_statuses[2] = mhealth_get_metric_count(&probe->fixture->hm, &index);

    memset(&metric, 0, sizeof(metric));
    metric.name = "late";
    metric.metric_id = MHEALTH_METRIC_CUSTOM_BASE;
    metric.collect_fn = fixed_collect;
    metric.direction = MHEALTH_ABOVE;
    metric.warn_threshold = 1;
    metric.critical_threshold = 2;

    probe->mutation_statuses[0] = mhealth_register(&probe->fixture->hm, &metric, &index);
    probe->mutation_statuses[1] = mhealth_tick(&probe->fixture->hm, &result);
    probe->mutation_statuses[2] = mhealth_enable(&probe->fixture->hm, 0U, false);
}

static int test_busy_guard_and_committed_alert_queries(void)
{
    fixture_t fixture;
    fixture_t alert_fixture;
    collector_state_t collector = { MHEALTH_COLLECT_OK, 500, 0U };
    collector_state_t alert_collector = { MHEALTH_COLLECT_OK, 8000, 0U };
    busy_probe_t busy_probe;
    alert_probe_t alert_probe;
    mhealth_metric_config_t metric;
    size_t index = 0U;
    size_t i;

    ASSERT_TRUE(init_fixture(&fixture, 4U));
    memset(&busy_probe, 0, sizeof(busy_probe));
    memset(&metric, 0, sizeof(metric));
    busy_probe.fixture = &fixture;
    busy_probe.collector = &collector;

    metric.name = "heap";
    metric.metric_id = MHEALTH_METRIC_HEAP_FREE;
    metric.collect_fn = busy_collect;
    metric.collect_ctx = &busy_probe;
    metric.direction = MHEALTH_BELOW;
    metric.warn_threshold = 4000;
    metric.critical_threshold = 1000;

    ASSERT_INT_EQ(MHEALTH_OK, mhealth_register(&fixture.hm, &metric, &index));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&fixture.hm, &(mhealth_check_result_t){ 0 }));
    for (i = 0U; i < 6U; ++i) {
        ASSERT_INT_EQ(MHEALTH_ERR_BUSY, busy_probe.mutation_statuses[i]);
    }

    ASSERT_TRUE(init_fixture(&alert_fixture, 4U));
    memset(&alert_probe, 0, sizeof(alert_probe));
    alert_probe.fixture = &alert_fixture;
    ASSERT_TRUE(register_fixed_metric(
        &alert_fixture,
        "heap",
        MHEALTH_METRIC_HEAP_FREE,
        &alert_collector,
        MHEALTH_BELOW,
        4000,
        1000,
        &index));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_set_alert(&alert_fixture.hm, alert_probe_callback, &alert_probe));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&alert_fixture.hm, &(mhealth_check_result_t){ 0 }));
    alert_collector.value = 500;
    alert_fixture.clock.now += 100U;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&alert_fixture.hm, &(mhealth_check_result_t){ 0 }));
    ASSERT_SIZE_EQ(1U, alert_probe.callback_count);
    ASSERT_INT_EQ(MHEALTH_OK, alert_probe.query_statuses[0]);
    ASSERT_INT_EQ(MHEALTH_OK, alert_probe.query_statuses[1]);
    ASSERT_INT_EQ(MHEALTH_OK, alert_probe.query_statuses[2]);
    ASSERT_INT_EQ(MHEALTH_ERR_BUSY, alert_probe.mutation_statuses[0]);
    ASSERT_INT_EQ(MHEALTH_ERR_BUSY, alert_probe.mutation_statuses[1]);
    ASSERT_INT_EQ(MHEALTH_ERR_BUSY, alert_probe.mutation_statuses[2]);
    ASSERT_INT_EQ(MHEALTH_SEVERITY_CRITICAL, alert_probe.observed_alert.current_severity);
    ASSERT_SIZE_EQ(1U, alert_probe.latest_count);
    ASSERT_INT_EQ(MHEALTH_SAMPLE_VALID, alert_probe.latest_samples[0].status);
    return 1;
}

static int test_disable_reenable_rate_limit_and_wrap(void)
{
    fixture_t fixture;
    collector_state_t collector = { MHEALTH_COLLECT_OK, 500, 0U };
    mhealth_check_result_t result;
    mhealth_metric_status_t status;
    size_t index = 0U;

    ASSERT_TRUE(init_fixture(&fixture, 4U));
    ASSERT_TRUE(register_fixed_metric(
        &fixture, "heap", MHEALTH_METRIC_HEAP_FREE, &collector, MHEALTH_BELOW, 4000, 1000, &index));

    fixture.clock.call_count = 0U;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_tick(&fixture.hm, &result));
    ASSERT_TRUE(result.performed);
    ASSERT_SIZE_EQ(1U, fixture.clock.call_count);

    ASSERT_INT_EQ(MHEALTH_OK, mhealth_enable(&fixture.hm, index, false));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_metric_status(&fixture.hm, index, &status));
    ASSERT_INT_EQ(MHEALTH_SAMPLE_DISABLED, status.current_sample.status);
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_enable(&fixture.hm, index, true));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_metric_status(&fixture.hm, index, &status));
    ASSERT_INT_EQ(MHEALTH_SAMPLE_UNSAMPLED, status.current_sample.status);

    fixture.clock.now += 49U;
    fixture.clock.call_count = 0U;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_tick(&fixture.hm, &result));
    ASSERT_FALSE(result.performed);
    ASSERT_SIZE_EQ(1U, fixture.clock.call_count);

    fixture.clock.now += 1U;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_tick(&fixture.hm, &result));
    ASSERT_TRUE(result.performed);
    ASSERT_SIZE_EQ(2U, fixture.alert_count);

    fixture.clock.now = UINT32_MAX - 10U;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&fixture.hm, &result));
    fixture.clock.now = 25U;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_tick(&fixture.hm, &result));
    ASSERT_FALSE(result.performed);
    fixture.clock.now = 41U;
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_tick(&fixture.hm, &result));
    ASSERT_TRUE(result.performed);
    return 1;
}

static int test_latest_history_format_queries_and_counters(void)
{
    fixture_t fixture;
    collector_state_t heap = { MHEALTH_COLLECT_OK, 3000, 0U };
    collector_state_t temp = { MHEALTH_COLLECT_ERROR, 0, 0U };
    mhealth_snapshot_meta_t meta;
    mhealth_sample_t samples[DEFAULT_METRIC_CAPACITY];
    size_t sample_count = 0U;
    size_t history_count = 0U;
    size_t written = 0U;
    size_t required = 0U;
    mhealth_counters_t counters;
    char buffer[256];
    size_t index = 0U;

    ASSERT_TRUE(init_fixture(&fixture, 4U));
    ASSERT_TRUE(register_fixed_metric(
        &fixture, "heap", MHEALTH_METRIC_HEAP_FREE, &heap, MHEALTH_BELOW, 4000, 1000, &index));
    ASSERT_TRUE(register_fixed_metric(
        &fixture, "temp", MHEALTH_METRIC_MCU_TEMP, &temp, MHEALTH_ABOVE, 700, 850, &index));

    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&fixture.hm, &(mhealth_check_result_t){ 0 }));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_latest(&fixture.hm, &meta, samples, DEFAULT_METRIC_CAPACITY, &sample_count));
    ASSERT_SIZE_EQ(2U, sample_count);
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_history_count(&fixture.hm, &history_count));
    ASSERT_SIZE_EQ(1U, history_count);
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_counters(&fixture.hm, &counters));
    ASSERT_INT_EQ(1, counters.check_count);
    ASSERT_INT_EQ(1, counters.collection_failure_count);

    ASSERT_INT_EQ(MHEALTH_OK, mhealth_enable(&fixture.hm, 0U, false));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_latest(&fixture.hm, &meta, samples, DEFAULT_METRIC_CAPACITY, &sample_count));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_snapshot_format(&fixture.hm, &meta, samples, sample_count, NULL, 0U, &written, &required));
    ASSERT_SIZE_EQ(0U, written);
    ASSERT_TRUE(required > 0U);
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_snapshot_format(&fixture.hm, &meta, samples, sample_count, buffer, sizeof(buffer), &written, &required));
    ASSERT_TRUE(strstr(buffer, "DISABLED") != NULL);
    ASSERT_TRUE(strstr(buffer, "COLLECTION_FAILED") != NULL);
    return 1;
}

static int test_null_state_invalid_index_and_two_instances(void)
{
    mhealth_t hm;
    mhealth_check_result_t result;
    mhealth_health_status_t health;
    mhealth_metric_status_t status;
    fixture_t a;
    fixture_t b;
    collector_state_t heap_a = { MHEALTH_COLLECT_OK, 8000, 0U };
    collector_state_t heap_b = { MHEALTH_COLLECT_OK, 500, 0U };
    size_t index = 0U;

    memset(&hm, 0, sizeof(hm));
    ASSERT_INT_EQ(MHEALTH_ERR_STATE, mhealth_tick(&hm, &result));
    ASSERT_INT_EQ(MHEALTH_ERR_STATE, mhealth_get_health(&hm, &health));

    ASSERT_TRUE(init_fixture(&a, 2U));
    ASSERT_TRUE(init_fixture(&b, 2U));
    ASSERT_TRUE(register_fixed_metric(
        &a, "heap_a", MHEALTH_METRIC_HEAP_FREE, &heap_a, MHEALTH_BELOW, 4000, 1000, &index));
    ASSERT_TRUE(register_fixed_metric(
        &b, "heap_b", MHEALTH_METRIC_HEAP_FREE, &heap_b, MHEALTH_BELOW, 4000, 1000, &index));

    ASSERT_INT_EQ(MHEALTH_ERR_NOT_FOUND, mhealth_get_metric_status(&a.hm, 7U, &status));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&a.hm, &result));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_check_now(&b.hm, &result));
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_health(&a.hm, &health));
    ASSERT_TRUE(health.healthy);
    ASSERT_INT_EQ(MHEALTH_OK, mhealth_get_health(&b.hm, &health));
    ASSERT_FALSE(health.healthy);
    return 1;
}

int main(void)
{
    int ok = 1;

    printf("=== microhealth contract tests ===\n\n");

    ok &= run_case("test_harness_self_test", test_harness_self_test);
    ok &= run_case("test_init_and_registration_validation", test_init_and_registration_validation);
    ok &= run_case("test_large_capacity_and_history_modes", test_large_capacity_and_history_modes);
    ok &= run_case("test_threshold_boundaries_and_limits", test_threshold_boundaries_and_limits);
    ok &= run_case("test_collection_failure_and_health_queries", test_collection_failure_and_health_queries);
    ok &= run_case("test_busy_guard_and_committed_alert_queries", test_busy_guard_and_committed_alert_queries);
    ok &= run_case("test_disable_reenable_rate_limit_and_wrap", test_disable_reenable_rate_limit_and_wrap);
    ok &= run_case("test_latest_history_format_queries_and_counters", test_latest_history_format_queries_and_counters);
    ok &= run_case("test_null_state_invalid_index_and_two_instances", test_null_state_invalid_index_and_two_instances);

    printf(
        "\n=== results: %lu tests, %lu passed, %lu failed, %lu assertions ===\n",
        g_tests_run,
        g_tests_passed,
        g_tests_failed,
        g_assertions);

    return ok ? 0 : 1;
}
