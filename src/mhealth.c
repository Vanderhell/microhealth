/*
 * microhealth — Implementation.
 *
 * SPDX-License-Identifier: MIT
 * https://github.com/Vanderhell/microhealth
 */

#include "mhealth.h"
#include <string.h>
#include <stdio.h>

/* ── Error / name strings ──────────────────────────────────────────────── */

const char *mhealth_err_str(mhealth_err_t err)
{
    switch (err) {
    case MHEALTH_OK:            return "ok";
    case MHEALTH_ERR_NULL:      return "null pointer";
    case MHEALTH_ERR_FULL:      return "metrics full";
    case MHEALTH_ERR_INVALID:   return "invalid config";
    case MHEALTH_ERR_NOT_FOUND: return "not found";
    default:                    return "unknown error";
    }
}

const char *mhealth_severity_str(mhealth_severity_t sev)
{
    switch (sev) {
    case MHEALTH_SEVERITY_OK:       return "OK";
    case MHEALTH_SEVERITY_WARN:     return "WARN";
    case MHEALTH_SEVERITY_CRITICAL: return "CRITICAL";
    default:                        return "?";
    }
}

/* ── Internal helpers ──────────────────────────────────────────────────── */

/** Evaluate severity for a single metric. */
static mhealth_severity_t evaluate_severity(const mhealth_metric_t *m,
                                             int32_t value)
{
    if (m->direction == MHEALTH_BELOW) {
        /* Alert when value drops below threshold (heap, battery) */
        if (value <= m->critical_threshold) return MHEALTH_SEVERITY_CRITICAL;
        if (value <= m->warn_threshold)     return MHEALTH_SEVERITY_WARN;
    } else {
        /* Alert when value rises above threshold (temp, errors) */
        if (value >= m->critical_threshold) return MHEALTH_SEVERITY_CRITICAL;
        if (value >= m->warn_threshold)     return MHEALTH_SEVERITY_WARN;
    }
    return MHEALTH_SEVERITY_OK;
}

#if MHEALTH_ENABLE_HISTORY
/** Push a snapshot into the history ring. */
static void history_push(mhealth_t *hm, const mhealth_snapshot_t *snap)
{
    memcpy(&hm->history[hm->hist_head], snap, sizeof(*snap));
    hm->hist_head = (hm->hist_head + 1) % MHEALTH_HISTORY_DEPTH;
    if (hm->hist_count < MHEALTH_HISTORY_DEPTH) {
        hm->hist_count++;
    }
}
#endif

/* ── Init ──────────────────────────────────────────────────────────────── */

mhealth_err_t mhealth_init(mhealth_t *hm, mhealth_clock_fn clock,
                            uint32_t check_interval_ms)
{
    if (hm == NULL || clock == NULL) return MHEALTH_ERR_NULL;

    memset(hm, 0, sizeof(*hm));
    hm->clock             = clock;
    hm->check_interval_ms = check_interval_ms;
    hm->last_check_ms     = 0;
    hm->check_count       = 0;

    return MHEALTH_OK;
}

void mhealth_set_alert(mhealth_t *hm, mhealth_alert_fn fn, void *ctx)
{
    if (hm == NULL) return;
    hm->alert_fn  = fn;
    hm->alert_ctx = ctx;
}

/* ── Metric registration ───────────────────────────────────────────────── */

int mhealth_register(mhealth_t *hm, const char *name, uint8_t id,
                      mhealth_collect_fn collect, void *collect_ctx,
                      mhealth_direction_t direction,
                      int32_t warn, int32_t critical)
{
    if (hm == NULL || name == NULL || collect == NULL) return MHEALTH_ERR_NULL;
    if (hm->num_metrics >= MHEALTH_MAX_METRICS) return MHEALTH_ERR_FULL;

    /* Validate: warn must be less severe than critical */
    if (direction == MHEALTH_BELOW) {
        if (warn < critical) return MHEALTH_ERR_INVALID;
    } else {
        if (warn > critical) return MHEALTH_ERR_INVALID;
    }

    uint8_t idx = hm->num_metrics;
    mhealth_metric_t *m = &hm->metrics[idx];

    m->name               = name;
    m->id                 = id;
    m->collect            = collect;
    m->collect_ctx        = collect_ctx;
    m->direction          = direction;
    m->warn_threshold     = warn;
    m->critical_threshold = critical;
    m->enabled            = true;

    hm->last_severity[idx] = MHEALTH_SEVERITY_OK;
    hm->num_metrics++;

    return (int)idx;
}

mhealth_err_t mhealth_enable(mhealth_t *hm, uint8_t index, bool enabled)
{
    if (hm == NULL) return MHEALTH_ERR_NULL;
    if (index >= hm->num_metrics) return MHEALTH_ERR_NOT_FOUND;
    hm->metrics[index].enabled = enabled;
    return MHEALTH_OK;
}

/* ── Core check logic ──────────────────────────────────────────────────── */

static int perform_check(mhealth_t *hm)
{
    uint32_t now = hm->clock();
    int alerts_fired = 0;

    mhealth_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snap.timestamp_ms = now;
    snap.num_metrics  = hm->num_metrics;

    for (uint8_t i = 0; i < hm->num_metrics; i++) {
        mhealth_metric_t *m = &hm->metrics[i];

        if (!m->enabled) {
            snap.values[i]     = 0;
            snap.severities[i] = (uint8_t)MHEALTH_SEVERITY_OK;
            continue;
        }

        /* Collect current value */
        int32_t value = m->collect(m->collect_ctx);
        snap.values[i] = value;

        /* Evaluate severity */
        mhealth_severity_t sev = evaluate_severity(m, value);
        snap.severities[i] = (uint8_t)sev;

        /* Fire alert on severity transitions */
        mhealth_severity_t prev = hm->last_severity[i];
        if (sev != prev && hm->alert_fn != NULL) {
            mhealth_alert_t alert = {
                .metric_idx    = i,
                .metric_id     = m->id,
                .name          = m->name,
                .value         = value,
                .threshold     = (sev == MHEALTH_SEVERITY_CRITICAL)
                                    ? m->critical_threshold
                                    : m->warn_threshold,
                .severity      = sev,
                .prev_severity = prev,
                .timestamp_ms  = now,
            };
            hm->alert_fn(&alert, hm->alert_ctx);
            alerts_fired++;
        }

        hm->last_severity[i] = sev;
    }

#if MHEALTH_ENABLE_HISTORY
    history_push(hm, &snap);
#endif

    hm->last_check_ms = now;
    hm->check_count++;

    return alerts_fired;
}

/* ── Runtime ───────────────────────────────────────────────────────────── */

int mhealth_tick(mhealth_t *hm)
{
    if (hm == NULL || hm->clock == NULL) return 0;

    /* Rate limiting */
    if (hm->check_interval_ms > 0) {
        uint32_t now = hm->clock();
        uint32_t elapsed = now - hm->last_check_ms;
        if (elapsed < hm->check_interval_ms && hm->check_count > 0) {
            return 0;
        }
    }

    return perform_check(hm);
}

int mhealth_check_now(mhealth_t *hm)
{
    if (hm == NULL || hm->clock == NULL) return 0;
    return perform_check(hm);
}

/* ── Query ─────────────────────────────────────────────────────────────── */

mhealth_err_t mhealth_latest(const mhealth_t *hm, mhealth_snapshot_t *out)
{
    if (hm == NULL || out == NULL) return MHEALTH_ERR_NULL;
    if (hm->check_count == 0) return MHEALTH_ERR_NOT_FOUND;

#if MHEALTH_ENABLE_HISTORY
    uint8_t last = (hm->hist_head + MHEALTH_HISTORY_DEPTH - 1) % MHEALTH_HISTORY_DEPTH;
    memcpy(out, &hm->history[last], sizeof(*out));
    return MHEALTH_OK;
#else
    /* Without history, we can't retrieve the last snapshot */
    (void)hm; (void)out;
    return MHEALTH_ERR_NOT_FOUND;
#endif
}

mhealth_severity_t mhealth_metric_severity(const mhealth_t *hm, uint8_t index)
{
    if (hm == NULL || index >= hm->num_metrics) return MHEALTH_SEVERITY_OK;
    return hm->last_severity[index];
}

mhealth_severity_t mhealth_worst_severity(const mhealth_t *hm)
{
    if (hm == NULL) return MHEALTH_SEVERITY_OK;

    mhealth_severity_t worst = MHEALTH_SEVERITY_OK;
    for (uint8_t i = 0; i < hm->num_metrics; i++) {
        if (hm->metrics[i].enabled && hm->last_severity[i] > worst) {
            worst = hm->last_severity[i];
        }
    }
    return worst;
}

bool mhealth_is_healthy(const mhealth_t *hm)
{
    return mhealth_worst_severity(hm) == MHEALTH_SEVERITY_OK;
}

uint8_t mhealth_metric_count(const mhealth_t *hm)
{
    if (hm == NULL) return 0;
    return hm->num_metrics;
}

const mhealth_metric_t *mhealth_metric_at(const mhealth_t *hm, uint8_t index)
{
    if (hm == NULL || index >= hm->num_metrics) return NULL;
    return &hm->metrics[index];
}

uint32_t mhealth_check_count(const mhealth_t *hm)
{
    if (hm == NULL) return 0;
    return hm->check_count;
}

/* ── History ───────────────────────────────────────────────────────────── */

#if MHEALTH_ENABLE_HISTORY
uint8_t mhealth_history_count(const mhealth_t *hm)
{
    if (hm == NULL) return 0;
    return hm->hist_count;
}

mhealth_err_t mhealth_history_at(const mhealth_t *hm, uint8_t offset,
                                  mhealth_snapshot_t *out)
{
    if (hm == NULL || out == NULL) return MHEALTH_ERR_NULL;
    if (offset >= hm->hist_count) return MHEALTH_ERR_NOT_FOUND;

    /* offset=0 → most recent */
    int idx = (int)hm->hist_head - 1 - (int)offset;
    if (idx < 0) idx += MHEALTH_HISTORY_DEPTH;

    memcpy(out, &hm->history[idx], sizeof(*out));
    return MHEALTH_OK;
}
#endif

/* ── Utility ───────────────────────────────────────────────────────────── */

int mhealth_snapshot_format(const mhealth_t *hm, const mhealth_snapshot_t *snap,
                             char *buf, uint32_t buf_size)
{
    if (hm == NULL || snap == NULL || buf == NULL || buf_size == 0) return -1;

    int pos = 0;
    int remaining;

    remaining = (int)buf_size - pos;
    if (remaining > 0) {
        int n = snprintf(buf + pos, (size_t)remaining,
                         "Health @ %lu.%03lu s:\n",
                         (unsigned long)(snap->timestamp_ms / 1000),
                         (unsigned long)(snap->timestamp_ms % 1000));
        if (n > 0 && n < remaining) pos += n;
    }

    for (uint8_t i = 0; i < snap->num_metrics && i < hm->num_metrics; i++) {
        if (!hm->metrics[i].enabled) continue;

        mhealth_severity_t sev = (mhealth_severity_t)snap->severities[i];
        remaining = (int)buf_size - pos;
        if (remaining <= 0) break;

        int n = snprintf(buf + pos, (size_t)remaining,
                         "  %-16s %6ld  [%s]\n",
                         hm->metrics[i].name,
                         (long)snap->values[i],
                         mhealth_severity_str(sev));
        if (n > 0 && n < remaining) pos += n;
    }

    return pos;
}
