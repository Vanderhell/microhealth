/*
 * microhealth — System health monitor for embedded systems.
 *
 * Collects runtime metrics (heap, stack, uptime, errors), checks thresholds,
 * and fires alerts. Designed to bridge MCU-Malloc-Tracker, nvlog, and iotspool.
 *
 * C99 · Zero dependencies · Zero allocations · Callback-driven · Portable
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microhealth
 */

#ifndef MHEALTH_H
#define MHEALTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Configuration ─────────────────────────────────────────────────────── */

/** Maximum number of registered metrics. */
#ifndef MHEALTH_MAX_METRICS
#define MHEALTH_MAX_METRICS 16
#endif

/** Enable snapshot history ring buffer. */
#ifndef MHEALTH_ENABLE_HISTORY
#define MHEALTH_ENABLE_HISTORY 1
#endif

/** Number of history snapshots (must be power of 2). */
#ifndef MHEALTH_HISTORY_DEPTH
#define MHEALTH_HISTORY_DEPTH 8
#endif

/* ── Error codes ───────────────────────────────────────────────────────── */

typedef enum {
    MHEALTH_OK          =  0,
    MHEALTH_ERR_NULL    = -1,
    MHEALTH_ERR_FULL    = -2,
    MHEALTH_ERR_INVALID = -3,
    MHEALTH_ERR_NOT_FOUND = -4,
} mhealth_err_t;

const char *mhealth_err_str(mhealth_err_t err);

/* ── Metric types ──────────────────────────────────────────────────────── */

/** Well-known metric IDs. Users can also define custom IDs starting at 0x80. */
typedef enum {
    MHEALTH_METRIC_HEAP_FREE     = 0x01,  /**< Free heap bytes.             */
    MHEALTH_METRIC_HEAP_MIN      = 0x02,  /**< Minimum ever free heap.      */
    MHEALTH_METRIC_HEAP_ALLOCS   = 0x03,  /**< Active allocation count.     */
    MHEALTH_METRIC_STACK_FREE    = 0x10,  /**< Stack high-water (free bytes).*/
    MHEALTH_METRIC_UPTIME_S      = 0x20,  /**< Uptime in seconds.           */
    MHEALTH_METRIC_ERROR_COUNT   = 0x30,  /**< Cumulative error counter.    */
    MHEALTH_METRIC_MCU_TEMP      = 0x40,  /**< MCU temperature (°C × 10).   */
    MHEALTH_METRIC_VBATT         = 0x50,  /**< Battery voltage (mV).        */
    MHEALTH_METRIC_CUSTOM_BASE   = 0x80,  /**< User-defined metrics start.  */
} mhealth_metric_id_t;

/* ── Threshold direction ───────────────────────────────────────────────── */

typedef enum {
    MHEALTH_ABOVE = 0,   /**< Alert when value > threshold (temp, errors). */
    MHEALTH_BELOW = 1,   /**< Alert when value < threshold (heap, batt).   */
} mhealth_direction_t;

/* ── Alert severity ────────────────────────────────────────────────────── */

typedef enum {
    MHEALTH_SEVERITY_OK       = 0,   /**< Metric within normal range.     */
    MHEALTH_SEVERITY_WARN     = 1,   /**< Warning threshold crossed.      */
    MHEALTH_SEVERITY_CRITICAL = 2,   /**< Critical threshold crossed.     */
} mhealth_severity_t;

const char *mhealth_severity_str(mhealth_severity_t sev);

/* ── Platform callback ─────────────────────────────────────────────────── */

/** Clock function — returns milliseconds. Same as microres/microlog. */
typedef uint32_t (*mhealth_clock_fn)(void);

/* ── Collector callback ────────────────────────────────────────────────── */

/**
 * Metric collector — reads the current value of a metric.
 *
 * @param ctx  User context (e.g., pointer to MCU-Malloc-Tracker state).
 * @return Current metric value as int32_t.
 *
 * Examples:
 *   - Heap free: return mmt_get_free_bytes(&tracker);
 *   - MCU temp: return (int32_t)(adc_read_temp() * 10);
 *   - Error count: return app.error_counter;
 */
typedef int32_t (*mhealth_collect_fn)(void *ctx);

/* ── Alert callback ────────────────────────────────────────────────────── */

/**
 * Alert event — passed to the alert callback when a threshold is crossed.
 */
typedef struct {
    uint8_t              metric_idx;   /**< Index in metrics array.        */
    uint8_t              metric_id;    /**< Metric ID (MHEALTH_METRIC_*).  */
    const char          *name;         /**< Metric name string.            */
    int32_t              value;        /**< Current value.                 */
    int32_t              threshold;    /**< Threshold that was crossed.    */
    mhealth_severity_t   severity;     /**< WARN or CRITICAL.             */
    mhealth_severity_t   prev_severity;/**< Previous severity (for edge). */
    uint32_t             timestamp_ms; /**< When it happened.             */
} mhealth_alert_t;

/**
 * Alert callback — called when a metric crosses a threshold.
 *
 * @param alert  Alert details.
 * @param ctx    User context from mhealth_t.
 *
 * Typical implementations:
 *   - Log to nvlog
 *   - Queue MQTT alert via iotspool
 *   - Log via microlog
 *   - Trigger panicdump if critical
 */
typedef void (*mhealth_alert_fn)(const mhealth_alert_t *alert, void *ctx);

/* ── Metric descriptor ─────────────────────────────────────────────────── */

typedef struct {
    const char          *name;         /**< Human-readable name.           */
    uint8_t              id;           /**< Metric ID.                     */
    mhealth_collect_fn   collect;      /**< Value collector callback.      */
    void                *collect_ctx;  /**< Context for collector.         */
    mhealth_direction_t  direction;    /**< ABOVE or BELOW.               */
    int32_t              warn_threshold;    /**< Warning level.            */
    int32_t              critical_threshold;/**< Critical level.           */
    bool                 enabled;      /**< Is this metric active?        */
} mhealth_metric_t;

/* ── Metric snapshot ───────────────────────────────────────────────────── */

/** Snapshot of all metric values at a point in time. */
typedef struct {
    int32_t  values[MHEALTH_MAX_METRICS]; /**< Value per metric.          */
    uint8_t  severities[MHEALTH_MAX_METRICS]; /**< Severity per metric.   */
    uint32_t timestamp_ms;                /**< When snapshot was taken.    */
    uint8_t  num_metrics;                 /**< Number of valid entries.    */
} mhealth_snapshot_t;

/* ── Health monitor instance ───────────────────────────────────────────── */

typedef struct {
    mhealth_metric_t     metrics[MHEALTH_MAX_METRICS];
    uint8_t              num_metrics;
    mhealth_severity_t   last_severity[MHEALTH_MAX_METRICS];

    mhealth_alert_fn     alert_fn;     /**< Alert callback.               */
    void                *alert_ctx;    /**< Context for alert callback.   */
    mhealth_clock_fn     clock;        /**< Clock function.               */

    uint32_t             last_check_ms;/**< Timestamp of last check.      */
    uint32_t             check_interval_ms; /**< Min ms between checks.   */
    uint32_t             check_count;  /**< Total checks performed.       */

#if MHEALTH_ENABLE_HISTORY
    mhealth_snapshot_t   history[MHEALTH_HISTORY_DEPTH];
    uint8_t              hist_head;
    uint8_t              hist_count;
#endif
} mhealth_t;

/* ── Init ──────────────────────────────────────────────────────────────── */

/**
 * Initialise health monitor.
 *
 * @param hm              Instance (caller-allocated).
 * @param clock           Clock function (required).
 * @param check_interval  Minimum ms between tick checks (0 = every tick).
 * @return MHEALTH_OK on success.
 */
mhealth_err_t mhealth_init(mhealth_t *hm, mhealth_clock_fn clock,
                            uint32_t check_interval_ms);

/**
 * Set the alert callback. Called whenever a metric crosses a threshold.
 */
void mhealth_set_alert(mhealth_t *hm, mhealth_alert_fn fn, void *ctx);

/* ── Metric registration ───────────────────────────────────────────────── */

/**
 * Register a metric to monitor.
 *
 * @param hm         Health monitor.
 * @param name       Metric name (static/const string).
 * @param id         Metric ID (MHEALTH_METRIC_* or custom).
 * @param collect    Collector callback.
 * @param collect_ctx Context for collector.
 * @param direction  MHEALTH_ABOVE or MHEALTH_BELOW.
 * @param warn       Warning threshold.
 * @param critical   Critical threshold.
 * @return Metric index (0-based) or negative error.
 */
int mhealth_register(mhealth_t *hm, const char *name, uint8_t id,
                      mhealth_collect_fn collect, void *collect_ctx,
                      mhealth_direction_t direction,
                      int32_t warn, int32_t critical);

/**
 * Enable or disable a metric by index.
 */
mhealth_err_t mhealth_enable(mhealth_t *hm, uint8_t index, bool enabled);

/* ── Runtime ───────────────────────────────────────────────────────────── */

/**
 * Tick the health monitor — call from main loop or periodic timer.
 *
 * Collects all enabled metrics, compares against thresholds, fires alerts
 * on transitions (OK→WARN, WARN→CRITICAL, CRITICAL→OK, etc.), and stores
 * snapshot in history.
 *
 * Respects check_interval_ms — returns immediately if called too soon.
 *
 * @param hm  Health monitor instance.
 * @return Number of alerts fired (0 if no threshold crossings).
 */
int mhealth_tick(mhealth_t *hm);

/**
 * Force an immediate check regardless of interval.
 * @return Number of alerts fired.
 */
int mhealth_check_now(mhealth_t *hm);

/* ── Query ─────────────────────────────────────────────────────────────── */

/** Get the latest snapshot (last tick result). */
mhealth_err_t mhealth_latest(const mhealth_t *hm, mhealth_snapshot_t *out);

/** Get current severity of a specific metric. */
mhealth_severity_t mhealth_metric_severity(const mhealth_t *hm, uint8_t index);

/** Get the worst severity across all metrics. */
mhealth_severity_t mhealth_worst_severity(const mhealth_t *hm);

/** Is the system healthy? (all metrics OK) */
bool mhealth_is_healthy(const mhealth_t *hm);

/** Get number of registered metrics. */
uint8_t mhealth_metric_count(const mhealth_t *hm);

/** Get metric descriptor by index. */
const mhealth_metric_t *mhealth_metric_at(const mhealth_t *hm, uint8_t index);

/** Get total check count. */
uint32_t mhealth_check_count(const mhealth_t *hm);

/* ── History ───────────────────────────────────────────────────────────── */

#if MHEALTH_ENABLE_HISTORY
/** Get number of stored history snapshots. */
uint8_t mhealth_history_count(const mhealth_t *hm);

/**
 * Get a history snapshot by offset (0 = most recent).
 * @return MHEALTH_OK or MHEALTH_ERR_NOT_FOUND.
 */
mhealth_err_t mhealth_history_at(const mhealth_t *hm, uint8_t offset,
                                  mhealth_snapshot_t *out);
#endif

/* ── Utility ───────────────────────────────────────────────────────────── */

/** Format a snapshot into a human-readable string (for debug shell / log). */
int mhealth_snapshot_format(const mhealth_t *hm, const mhealth_snapshot_t *snap,
                             char *buf, uint32_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* MHEALTH_H */
