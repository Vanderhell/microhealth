/*
 * microhealth test suite.
 *
 * Build: gcc -std=c99 -Wall -Wextra -I../include ../src/mhealth.c test_all.c -o test_all
 */

#include "mhealth.h"
#include <stdio.h>
#include <string.h>

/* ── Minimal test framework ────────────────────────────────────────────── */

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name) do {                                     \
    tests_run++;                                                \
    printf("  %-55s ", #name);                                  \
    name();                                                     \
    printf("PASS\n");                                           \
    tests_passed++;                                             \
} while (0)

#define ASSERT_EQ(expected, actual) do {                        \
    if ((expected) != (actual)) {                               \
        printf("FAIL\n    %s:%d: expected %d, got %d\n",       \
               __FILE__, __LINE__, (int)(expected), (int)(actual)); \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_TRUE(expr) do {                                  \
    if (!(expr)) {                                              \
        printf("FAIL\n    %s:%d: expected true\n",              \
               __FILE__, __LINE__);                             \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_FALSE(expr) do {                                 \
    if ((expr)) {                                               \
        printf("FAIL\n    %s:%d: expected false\n",             \
               __FILE__, __LINE__);                             \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_STR_EQ(expected, actual) do {                    \
    if (strcmp((expected), (actual)) != 0) {                     \
        printf("FAIL\n    %s:%d: expected \"%s\", got \"%s\"\n",\
               __FILE__, __LINE__, (expected), (actual));       \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

#define ASSERT_GE(val, minimum) do {                            \
    if ((int)(val) < (int)(minimum)) {                          \
        printf("FAIL\n    %s:%d: %d < %d\n",                   \
               __FILE__, __LINE__, (int)(val), (int)(minimum)); \
        tests_failed++; return;                                 \
    }                                                           \
} while (0)

/* ── Mock clock ────────────────────────────────────────────────────────── */

static uint32_t mock_time_ms = 0;
static uint32_t mock_clock(void) { return mock_time_ms; }

/* ── Mock collectors ───────────────────────────────────────────────────── */

static int32_t mock_heap_free = 8000;
static int32_t mock_temp = 250;      /* 25.0 °C × 10 */
static int32_t mock_errors = 0;
static int32_t mock_battery = 3700;  /* mV */

static int32_t collect_heap(void *ctx)    { (void)ctx; return mock_heap_free; }
static int32_t collect_temp(void *ctx)    { (void)ctx; return mock_temp; }
static int32_t collect_errors(void *ctx)  { (void)ctx; return mock_errors; }
static int32_t collect_battery(void *ctx) { (void)ctx; return mock_battery; }

/* ── Alert tracking ────────────────────────────────────────────────────── */

#define MAX_ALERTS 32
static mhealth_alert_t alert_log[MAX_ALERTS];
static int alert_count = 0;

static void alert_handler(const mhealth_alert_t *alert, void *ctx)
{
    (void)ctx;
    if (alert_count < MAX_ALERTS) {
        alert_log[alert_count++] = *alert;
    }
}

/* ── Setup ─────────────────────────────────────────────────────────────── */

static mhealth_t hm;

static void reset_all(void) {
    mock_time_ms  = 1000;
    mock_heap_free = 8000;
    mock_temp     = 250;
    mock_errors   = 0;
    mock_battery  = 3700;
    alert_count   = 0;
    memset(alert_log, 0, sizeof(alert_log));
    mhealth_init(&hm, mock_clock, 0);
    mhealth_set_alert(&hm, alert_handler, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Init
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_init) {
    reset_all();
    ASSERT_EQ(0, mhealth_metric_count(&hm));
    ASSERT_EQ(0, (int)mhealth_check_count(&hm));
    ASSERT_TRUE(mhealth_is_healthy(&hm));
}

TEST(test_init_null) {
    ASSERT_EQ(MHEALTH_ERR_NULL, mhealth_init(NULL, mock_clock, 0));
    ASSERT_EQ(MHEALTH_ERR_NULL, mhealth_init(&hm, NULL, 0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Registration
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_register_metric) {
    reset_all();
    int idx = mhealth_register(&hm, "heap_free", MHEALTH_METRIC_HEAP_FREE,
                                collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    ASSERT_EQ(0, idx);
    ASSERT_EQ(1, mhealth_metric_count(&hm));

    const mhealth_metric_t *m = mhealth_metric_at(&hm, 0);
    ASSERT_TRUE(m != NULL);
    ASSERT_STR_EQ("heap_free", m->name);
    ASSERT_TRUE(m->enabled);
}

TEST(test_register_multiple) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    mhealth_register(&hm, "temp", MHEALTH_METRIC_MCU_TEMP,
                      collect_temp, NULL, MHEALTH_ABOVE, 700, 850);
    mhealth_register(&hm, "errors", MHEALTH_METRIC_ERROR_COUNT,
                      collect_errors, NULL, MHEALTH_ABOVE, 10, 50);
    ASSERT_EQ(3, mhealth_metric_count(&hm));
}

TEST(test_register_null) {
    reset_all();
    ASSERT_EQ(MHEALTH_ERR_NULL, mhealth_register(NULL, "x", 0, collect_heap, NULL, MHEALTH_BELOW, 1, 0));
    ASSERT_EQ(MHEALTH_ERR_NULL, mhealth_register(&hm, NULL, 0, collect_heap, NULL, MHEALTH_BELOW, 1, 0));
    ASSERT_EQ(MHEALTH_ERR_NULL, mhealth_register(&hm, "x", 0, NULL, NULL, MHEALTH_BELOW, 1, 0));
}

TEST(test_register_invalid_thresholds) {
    reset_all();
    /* BELOW: warn must be >= critical */
    ASSERT_EQ(MHEALTH_ERR_INVALID,
        mhealth_register(&hm, "bad", 0, collect_heap, NULL, MHEALTH_BELOW, 100, 200));
    /* ABOVE: warn must be <= critical */
    ASSERT_EQ(MHEALTH_ERR_INVALID,
        mhealth_register(&hm, "bad", 0, collect_temp, NULL, MHEALTH_ABOVE, 800, 700));
}

TEST(test_register_full) {
    reset_all();
    for (int i = 0; i < MHEALTH_MAX_METRICS; i++) {
        ASSERT_GE(mhealth_register(&hm, "m", 0, collect_heap, NULL, MHEALTH_BELOW, 4000, 1000), 0);
    }
    ASSERT_EQ(MHEALTH_ERR_FULL,
        mhealth_register(&hm, "over", 0, collect_heap, NULL, MHEALTH_BELOW, 4000, 1000));
}

TEST(test_enable_disable) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    ASSERT_TRUE(hm.metrics[0].enabled);

    mhealth_enable(&hm, 0, false);
    ASSERT_FALSE(hm.metrics[0].enabled);

    mhealth_enable(&hm, 0, true);
    ASSERT_TRUE(hm.metrics[0].enabled);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Tick — healthy state
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_tick_healthy) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    mock_heap_free = 8000;  /* well above thresholds */

    int alerts = mhealth_tick(&hm);
    ASSERT_EQ(0, alerts);
    ASSERT_EQ(0, alert_count);
    ASSERT_TRUE(mhealth_is_healthy(&hm));
    ASSERT_EQ(MHEALTH_SEVERITY_OK, mhealth_metric_severity(&hm, 0));
    ASSERT_EQ(1, (int)mhealth_check_count(&hm));
}

TEST(test_tick_multiple_metrics_healthy) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    mhealth_register(&hm, "temp", MHEALTH_METRIC_MCU_TEMP,
                      collect_temp, NULL, MHEALTH_ABOVE, 700, 850);
    mock_heap_free = 8000;
    mock_temp = 250;

    int alerts = mhealth_tick(&hm);
    ASSERT_EQ(0, alerts);
    ASSERT_TRUE(mhealth_is_healthy(&hm));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Tick — threshold crossings
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_tick_warn_below) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);

    /* Start healthy */
    mock_heap_free = 8000;
    mhealth_tick(&hm);
    ASSERT_EQ(0, alert_count);

    /* Drop to warn level */
    mock_heap_free = 3500;
    mock_time_ms += 1000;
    int alerts = mhealth_tick(&hm);
    ASSERT_EQ(1, alerts);
    ASSERT_EQ(1, alert_count);
    ASSERT_EQ(MHEALTH_SEVERITY_WARN, (int)alert_log[0].severity);
    ASSERT_EQ(MHEALTH_SEVERITY_OK, (int)alert_log[0].prev_severity);
    ASSERT_EQ(3500, alert_log[0].value);
    ASSERT_STR_EQ("heap", alert_log[0].name);
}

TEST(test_tick_critical_below) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);

    mock_heap_free = 8000;
    mhealth_tick(&hm);

    /* Drop to critical */
    mock_heap_free = 500;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    ASSERT_EQ(1, alert_count);
    ASSERT_EQ(MHEALTH_SEVERITY_CRITICAL, (int)alert_log[0].severity);
    ASSERT_FALSE(mhealth_is_healthy(&hm));
}

TEST(test_tick_warn_above) {
    reset_all();
    mhealth_register(&hm, "temp", MHEALTH_METRIC_MCU_TEMP,
                      collect_temp, NULL, MHEALTH_ABOVE, 700, 850);

    mock_temp = 250;
    mhealth_tick(&hm);

    /* Temperature rises to warn */
    mock_temp = 750;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    ASSERT_EQ(1, alert_count);
    ASSERT_EQ(MHEALTH_SEVERITY_WARN, (int)alert_log[0].severity);
}

TEST(test_tick_recovery) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);

    /* Healthy → warn */
    mock_heap_free = 8000;
    mhealth_tick(&hm);
    mock_heap_free = 3000;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    ASSERT_EQ(1, alert_count);
    ASSERT_EQ(MHEALTH_SEVERITY_WARN, (int)alert_log[0].severity);

    /* Warn → OK (recovery) */
    mock_heap_free = 8000;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    ASSERT_EQ(2, alert_count);
    ASSERT_EQ(MHEALTH_SEVERITY_OK, (int)alert_log[1].severity);
    ASSERT_EQ(MHEALTH_SEVERITY_WARN, (int)alert_log[1].prev_severity);
    ASSERT_TRUE(mhealth_is_healthy(&hm));
}

TEST(test_tick_warn_to_critical) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);

    mock_heap_free = 8000;
    mhealth_tick(&hm);

    /* → WARN */
    mock_heap_free = 3000;
    mock_time_ms += 1000;
    mhealth_tick(&hm);

    /* → CRITICAL */
    mock_heap_free = 500;
    mock_time_ms += 1000;
    mhealth_tick(&hm);

    ASSERT_EQ(2, alert_count);
    ASSERT_EQ(MHEALTH_SEVERITY_WARN, (int)alert_log[0].severity);
    ASSERT_EQ(MHEALTH_SEVERITY_CRITICAL, (int)alert_log[1].severity);
    ASSERT_EQ(MHEALTH_SEVERITY_WARN, (int)alert_log[1].prev_severity);
}

TEST(test_no_alert_on_same_severity) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);

    mock_heap_free = 3000;
    mhealth_tick(&hm);
    ASSERT_EQ(1, alert_count);  /* OK → WARN */

    /* Still warn, different value but same severity */
    mock_heap_free = 2000;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    ASSERT_EQ(1, alert_count);  /* no new alert */
}

TEST(test_worst_severity) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    mhealth_register(&hm, "temp", MHEALTH_METRIC_MCU_TEMP,
                      collect_temp, NULL, MHEALTH_ABOVE, 700, 850);

    mock_heap_free = 3000;  /* WARN */
    mock_temp = 900;        /* CRITICAL */
    mhealth_tick(&hm);

    ASSERT_EQ(MHEALTH_SEVERITY_WARN, mhealth_metric_severity(&hm, 0));
    ASSERT_EQ(MHEALTH_SEVERITY_CRITICAL, mhealth_metric_severity(&hm, 1));
    ASSERT_EQ(MHEALTH_SEVERITY_CRITICAL, mhealth_worst_severity(&hm));
    ASSERT_FALSE(mhealth_is_healthy(&hm));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Disabled metrics
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_disabled_metric_skipped) {
    reset_all();
    int idx = mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                                collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    mhealth_enable(&hm, (uint8_t)idx, false);

    mock_heap_free = 100;  /* would be critical if enabled */
    mhealth_tick(&hm);
    ASSERT_EQ(0, alert_count);
    ASSERT_TRUE(mhealth_is_healthy(&hm));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Check interval rate limiting
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_rate_limiting) {
    mock_time_ms = 1000;
    alert_count = 0;
    mhealth_init(&hm, mock_clock, 5000);  /* min 5s between checks */
    mhealth_set_alert(&hm, alert_handler, NULL);
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    mock_heap_free = 8000;

    /* First tick always runs */
    ASSERT_EQ(0, mhealth_tick(&hm));
    ASSERT_EQ(1, (int)mhealth_check_count(&hm));

    /* Too soon — skipped */
    mock_time_ms += 2000;
    mhealth_tick(&hm);
    ASSERT_EQ(1, (int)mhealth_check_count(&hm));

    /* Still too soon */
    mock_time_ms += 2000;
    mhealth_tick(&hm);
    ASSERT_EQ(1, (int)mhealth_check_count(&hm));

    /* Now enough time passed */
    mock_time_ms += 2000;
    mhealth_tick(&hm);
    ASSERT_EQ(2, (int)mhealth_check_count(&hm));
}

TEST(test_check_now_ignores_interval) {
    mock_time_ms = 1000;
    mhealth_init(&hm, mock_clock, 60000);  /* 60s interval */
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    mock_heap_free = 8000;

    mhealth_tick(&hm);
    ASSERT_EQ(1, (int)mhealth_check_count(&hm));

    /* check_now bypasses interval */
    mock_time_ms += 100;
    mhealth_check_now(&hm);
    ASSERT_EQ(2, (int)mhealth_check_count(&hm));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: History
 * ═══════════════════════════════════════════════════════════════════════════ */

#if MHEALTH_ENABLE_HISTORY
TEST(test_history_stored) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);

    mock_heap_free = 8000;
    mhealth_tick(&hm);
    ASSERT_EQ(1, mhealth_history_count(&hm));

    mock_heap_free = 7000;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    ASSERT_EQ(2, mhealth_history_count(&hm));
}

TEST(test_history_latest) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);

    mock_heap_free = 5555;
    mock_time_ms = 42000;
    mhealth_tick(&hm);

    mhealth_snapshot_t snap;
    ASSERT_EQ(MHEALTH_OK, mhealth_latest(&hm, &snap));
    ASSERT_EQ(5555, snap.values[0]);
    ASSERT_EQ(42000, (int)snap.timestamp_ms);
}

TEST(test_history_at_offset) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);

    for (int i = 0; i < 5; i++) {
        mock_heap_free = 1000 * (i + 1);
        mock_time_ms = (uint32_t)((i + 1) * 1000);
        mhealth_tick(&hm);
    }

    mhealth_snapshot_t snap;
    /* offset=0 → most recent (5000) */
    ASSERT_EQ(MHEALTH_OK, mhealth_history_at(&hm, 0, &snap));
    ASSERT_EQ(5000, snap.values[0]);

    /* offset=2 → 3rd from end (3000) */
    ASSERT_EQ(MHEALTH_OK, mhealth_history_at(&hm, 2, &snap));
    ASSERT_EQ(3000, snap.values[0]);

    /* offset out of range */
    ASSERT_EQ(MHEALTH_ERR_NOT_FOUND, mhealth_history_at(&hm, 99, &snap));
}

TEST(test_history_wraps) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);

    /* Push more than HISTORY_DEPTH entries */
    for (int i = 0; i < MHEALTH_HISTORY_DEPTH + 5; i++) {
        mock_heap_free = (int32_t)(100 * (i + 1));
        mock_time_ms = (uint32_t)((i + 1) * 1000);
        mhealth_tick(&hm);
    }

    ASSERT_EQ(MHEALTH_HISTORY_DEPTH, mhealth_history_count(&hm));

    /* Most recent should be the last written */
    mhealth_snapshot_t snap;
    mhealth_history_at(&hm, 0, &snap);
    ASSERT_EQ(100 * (MHEALTH_HISTORY_DEPTH + 5), snap.values[0]);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Snapshot format
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_snapshot_format) {
    reset_all();
    mhealth_register(&hm, "heap_free", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    mhealth_register(&hm, "mcu_temp", MHEALTH_METRIC_MCU_TEMP,
                      collect_temp, NULL, MHEALTH_ABOVE, 700, 850);

    mock_heap_free = 3500;
    mock_temp = 250;
    mock_time_ms = 5000;
    mhealth_tick(&hm);

    mhealth_snapshot_t snap;
    mhealth_latest(&hm, &snap);

    char buf[256];
    int len = mhealth_snapshot_format(&hm, &snap, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "heap_free") != NULL);
    ASSERT_TRUE(strstr(buf, "3500") != NULL);
    ASSERT_TRUE(strstr(buf, "WARN") != NULL);
    ASSERT_TRUE(strstr(buf, "mcu_temp") != NULL);
    ASSERT_TRUE(strstr(buf, "OK") != NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Edge cases
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_tick_null) {
    ASSERT_EQ(0, mhealth_tick(NULL));
    ASSERT_EQ(0, mhealth_check_now(NULL));
}

TEST(test_no_alert_callback) {
    reset_all();
    hm.alert_fn = NULL;  /* no callback */
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);
    mock_heap_free = 500;
    /* Should not crash */
    mhealth_tick(&hm);
    ASSERT_EQ(MHEALTH_SEVERITY_CRITICAL, mhealth_metric_severity(&hm, 0));
}

TEST(test_query_null_safety) {
    ASSERT_EQ(MHEALTH_SEVERITY_OK, mhealth_metric_severity(NULL, 0));
    ASSERT_EQ(MHEALTH_SEVERITY_OK, mhealth_worst_severity(NULL));
    ASSERT_TRUE(mhealth_is_healthy(NULL));
    ASSERT_EQ(0, mhealth_metric_count(NULL));
    ASSERT_TRUE(mhealth_metric_at(NULL, 0) == NULL);
    ASSERT_EQ(0, (int)mhealth_check_count(NULL));
}

TEST(test_err_str) {
    ASSERT_STR_EQ("ok",           mhealth_err_str(MHEALTH_OK));
    ASSERT_STR_EQ("null pointer", mhealth_err_str(MHEALTH_ERR_NULL));
    ASSERT_STR_EQ("metrics full", mhealth_err_str(MHEALTH_ERR_FULL));
    ASSERT_STR_EQ("not found",    mhealth_err_str(MHEALTH_ERR_NOT_FOUND));
    ASSERT_STR_EQ("unknown error",mhealth_err_str((mhealth_err_t)99));
}

TEST(test_severity_str) {
    ASSERT_STR_EQ("OK",       mhealth_severity_str(MHEALTH_SEVERITY_OK));
    ASSERT_STR_EQ("WARN",     mhealth_severity_str(MHEALTH_SEVERITY_WARN));
    ASSERT_STR_EQ("CRITICAL", mhealth_severity_str(MHEALTH_SEVERITY_CRITICAL));
}

TEST(test_boundary_values) {
    reset_all();
    mhealth_register(&hm, "heap", MHEALTH_METRIC_HEAP_FREE,
                      collect_heap, NULL, MHEALTH_BELOW, 4000, 1000);

    /* Exactly at warn threshold → WARN */
    mock_heap_free = 4000;
    mhealth_tick(&hm);
    ASSERT_EQ(MHEALTH_SEVERITY_WARN, mhealth_metric_severity(&hm, 0));

    /* Exactly at critical threshold → CRITICAL */
    mock_heap_free = 1000;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    ASSERT_EQ(MHEALTH_SEVERITY_CRITICAL, mhealth_metric_severity(&hm, 0));

    /* One above warn → OK */
    mock_heap_free = 4001;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    ASSERT_EQ(MHEALTH_SEVERITY_OK, mhealth_metric_severity(&hm, 0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tests: Real-world scenario
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST(test_full_scenario) {
    reset_all();

    /* Register 4 metrics like a real device */
    mhealth_register(&hm, "heap_free",  MHEALTH_METRIC_HEAP_FREE,    collect_heap,    NULL, MHEALTH_BELOW, 4000, 1000);
    mhealth_register(&hm, "mcu_temp",   MHEALTH_METRIC_MCU_TEMP,     collect_temp,    NULL, MHEALTH_ABOVE, 700, 850);
    mhealth_register(&hm, "errors",     MHEALTH_METRIC_ERROR_COUNT,  collect_errors,  NULL, MHEALTH_ABOVE, 10, 50);
    mhealth_register(&hm, "battery",    MHEALTH_METRIC_VBATT,        collect_battery, NULL, MHEALTH_BELOW, 3300, 3000);

    ASSERT_EQ(4, mhealth_metric_count(&hm));

    /* Tick 1: all healthy */
    mock_heap_free = 8000; mock_temp = 300; mock_errors = 0; mock_battery = 3700;
    mhealth_tick(&hm);
    ASSERT_TRUE(mhealth_is_healthy(&hm));
    ASSERT_EQ(0, alert_count);

    /* Tick 2: heap dropping, temp rising */
    mock_heap_free = 3500; mock_temp = 720;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    ASSERT_EQ(2, alert_count);  /* heap WARN + temp WARN */
    ASSERT_FALSE(mhealth_is_healthy(&hm));
    ASSERT_EQ(MHEALTH_SEVERITY_WARN, mhealth_worst_severity(&hm));

    /* Tick 3: heap critical, temp still warn, battery low */
    mock_heap_free = 500; mock_battery = 3100;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    /* heap WARN→CRITICAL, battery OK→WARN */
    ASSERT_EQ(4, alert_count);
    ASSERT_EQ(MHEALTH_SEVERITY_CRITICAL, mhealth_worst_severity(&hm));

    /* Tick 4: recovery */
    mock_heap_free = 8000; mock_temp = 300; mock_battery = 3800;
    mock_time_ms += 1000;
    mhealth_tick(&hm);
    ASSERT_TRUE(mhealth_is_healthy(&hm));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== microhealth test suite ===\n\n");

    printf("[Init]\n");
    RUN_TEST(test_init);
    RUN_TEST(test_init_null);

    printf("\n[Registration]\n");
    RUN_TEST(test_register_metric);
    RUN_TEST(test_register_multiple);
    RUN_TEST(test_register_null);
    RUN_TEST(test_register_invalid_thresholds);
    RUN_TEST(test_register_full);
    RUN_TEST(test_enable_disable);

    printf("\n[Tick - Healthy]\n");
    RUN_TEST(test_tick_healthy);
    RUN_TEST(test_tick_multiple_metrics_healthy);

    printf("\n[Tick - Threshold Crossings]\n");
    RUN_TEST(test_tick_warn_below);
    RUN_TEST(test_tick_critical_below);
    RUN_TEST(test_tick_warn_above);
    RUN_TEST(test_tick_recovery);
    RUN_TEST(test_tick_warn_to_critical);
    RUN_TEST(test_no_alert_on_same_severity);
    RUN_TEST(test_worst_severity);

    printf("\n[Disabled Metrics]\n");
    RUN_TEST(test_disabled_metric_skipped);

    printf("\n[Rate Limiting]\n");
    RUN_TEST(test_rate_limiting);
    RUN_TEST(test_check_now_ignores_interval);

#if MHEALTH_ENABLE_HISTORY
    printf("\n[History]\n");
    RUN_TEST(test_history_stored);
    RUN_TEST(test_history_latest);
    RUN_TEST(test_history_at_offset);
    RUN_TEST(test_history_wraps);
#endif

    printf("\n[Snapshot Format]\n");
    RUN_TEST(test_snapshot_format);

    printf("\n[Edge Cases]\n");
    RUN_TEST(test_tick_null);
    RUN_TEST(test_no_alert_callback);
    RUN_TEST(test_query_null_safety);
    RUN_TEST(test_err_str);
    RUN_TEST(test_severity_str);
    RUN_TEST(test_boundary_values);

    printf("\n[Full Scenario]\n");
    RUN_TEST(test_full_scenario);

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FAILED", tests_failed);
    printf(" ===\n\n");

    return tests_failed > 0 ? 1 : 0;
}
