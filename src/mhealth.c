/*
 * microhealth - Implementation.
 *
 * SPDX-License-Identifier: MIT
 */

#include "mhealth.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static uint32_t saturating_inc_u32(uint32_t value, uint32_t amount)
{
    if (UINT32_MAX - value < amount) {
        return UINT32_MAX;
    }
    return value + amount;
}

static bool metric_id_is_known(uint8_t metric_id)
{
    switch (metric_id) {
    case MHEALTH_METRIC_HEAP_FREE:
    case MHEALTH_METRIC_HEAP_MIN:
    case MHEALTH_METRIC_HEAP_ALLOCS:
    case MHEALTH_METRIC_STACK_FREE:
    case MHEALTH_METRIC_UPTIME_S:
    case MHEALTH_METRIC_ERROR_COUNT:
    case MHEALTH_METRIC_MCU_TEMP:
    case MHEALTH_METRIC_VBATT:
        return true;
    default:
        return false;
    }
}

static bool metric_id_is_valid(uint8_t metric_id)
{
    return metric_id_is_known(metric_id) || metric_id >= MHEALTH_METRIC_CUSTOM_BASE;
}

static bool direction_is_valid(mhealth_direction_t direction)
{
    return direction == MHEALTH_ABOVE || direction == MHEALTH_BELOW;
}

static mhealth_severity_t evaluate_severity(
    mhealth_direction_t direction,
    int32_t warn_threshold,
    int32_t critical_threshold,
    int32_t value)
{
    if (direction == MHEALTH_ABOVE) {
        if (value >= critical_threshold) {
            return MHEALTH_SEVERITY_CRITICAL;
        }
        if (value >= warn_threshold) {
            return MHEALTH_SEVERITY_WARN;
        }
        return MHEALTH_SEVERITY_OK;
    }

    if (value <= critical_threshold) {
        return MHEALTH_SEVERITY_CRITICAL;
    }
    if (value <= warn_threshold) {
        return MHEALTH_SEVERITY_WARN;
    }
    return MHEALTH_SEVERITY_OK;
}

static bool instance_is_usable(const mhealth_t *hm)
{
    return hm != NULL && hm->initialized && hm->metric_slots != NULL &&
           hm->metric_capacity > 0U && hm->clock_fn != NULL;
}

static mhealth_err_t require_ready(const mhealth_t *hm)
{
    if (hm == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (!instance_is_usable(hm)) {
        return MHEALTH_ERR_STATE;
    }
    return MHEALTH_OK;
}

static mhealth_err_t require_mutable(mhealth_t *hm)
{
    mhealth_err_t status = require_ready(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (hm->busy) {
        return MHEALTH_ERR_BUSY;
    }
    return MHEALTH_OK;
}

static size_t history_index(const mhealth_t *hm, size_t slot_index)
{
    return slot_index * hm->metric_capacity;
}

static void snapshot_copy_from_slots(const mhealth_t *hm, mhealth_sample_t *out_samples)
{
    size_t i;
    for (i = 0; i < hm->metric_count; ++i) {
        out_samples[i] = hm->metric_slots[i].current_sample;
    }
}

static void history_store_snapshot(
    mhealth_t *hm,
    const mhealth_snapshot_meta_t *meta)
{
    size_t base;
    size_t i;

    if (hm->history_capacity == 0U) {
        return;
    }

    hm->history_meta[hm->history_head].meta = *meta;
    base = history_index(hm, hm->history_head);
    for (i = 0; i < meta->sample_count; ++i) {
        hm->history_samples[base + i] = hm->metric_slots[i].current_sample;
    }

    hm->history_head = (hm->history_head + 1U) % hm->history_capacity;
    if (hm->history_count < hm->history_capacity) {
        hm->history_count += 1U;
    }
}

static void check_result_clear(mhealth_check_result_t *out_result)
{
    if (out_result != NULL) {
        out_result->performed = false;
        out_result->transition_count = 0U;
        out_result->collection_failure_count = 0U;
        out_result->timestamp_ms = 0U;
    }
}

static mhealth_err_t validate_config(const mhealth_config_t *config)
{
    size_t total_history_samples;

    if (config == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (config->metric_slots == NULL || config->metric_capacity == 0U) {
        return MHEALTH_ERR_INVALID;
    }
    if (config->clock_fn == NULL) {
        return MHEALTH_ERR_INVALID;
    }
    if (config->history_capacity == 0U) {
        if (config->history_meta != NULL || config->history_samples != NULL) {
            return MHEALTH_ERR_INVALID;
        }
        return MHEALTH_OK;
    }
    if (config->history_meta == NULL || config->history_samples == NULL) {
        return MHEALTH_ERR_INVALID;
    }
    if (config->metric_capacity > SIZE_MAX / config->history_capacity) {
        return MHEALTH_ERR_INVALID;
    }
    total_history_samples = config->metric_capacity * config->history_capacity;
    if (total_history_samples / config->metric_capacity != config->history_capacity) {
        return MHEALTH_ERR_INVALID;
    }
    return MHEALTH_OK;
}

static mhealth_err_t validate_metric_config(const mhealth_metric_config_t *metric)
{
    if (metric == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (metric->name == NULL || metric->name[0] == '\0') {
        return MHEALTH_ERR_INVALID;
    }
    if (metric->collect_fn == NULL) {
        return MHEALTH_ERR_INVALID;
    }
    if (!direction_is_valid(metric->direction)) {
        return MHEALTH_ERR_INVALID;
    }
    if (!metric_id_is_valid(metric->metric_id)) {
        return MHEALTH_ERR_INVALID;
    }
    if (metric->direction == MHEALTH_ABOVE) {
        if (metric->warn_threshold >= metric->critical_threshold) {
            return MHEALTH_ERR_INVALID;
        }
    } else if (metric->warn_threshold <= metric->critical_threshold) {
        return MHEALTH_ERR_INVALID;
    }
    return MHEALTH_OK;
}

const char *mhealth_err_str(mhealth_err_t err)
{
    switch (err) {
    case MHEALTH_OK:
        return "ok";
    case MHEALTH_ERR_NULL:
        return "null pointer";
    case MHEALTH_ERR_INVALID:
        return "invalid argument";
    case MHEALTH_ERR_FULL:
        return "capacity full";
    case MHEALTH_ERR_NOT_FOUND:
        return "not found";
    case MHEALTH_ERR_STATE:
        return "invalid state";
    case MHEALTH_ERR_BUSY:
        return "busy";
    case MHEALTH_ERR_TRUNCATED:
        return "truncated";
    default:
        return "unknown error";
    }
}

const char *mhealth_severity_str(mhealth_severity_t severity)
{
    switch (severity) {
    case MHEALTH_SEVERITY_OK:
        return "OK";
    case MHEALTH_SEVERITY_WARN:
        return "WARN";
    case MHEALTH_SEVERITY_CRITICAL:
        return "CRITICAL";
    default:
        return "INVALID";
    }
}

mhealth_err_t mhealth_init(mhealth_t *hm, const mhealth_config_t *config)
{
    mhealth_err_t status;
    size_t i;

    if (hm == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (hm->initialized && hm->busy) {
        return MHEALTH_ERR_BUSY;
    }

    status = validate_config(config);
    if (status != MHEALTH_OK) {
        return status;
    }

    memset(hm, 0, sizeof(*hm));
    hm->metric_slots = config->metric_slots;
    hm->metric_capacity = config->metric_capacity;
    hm->history_meta = config->history_meta;
    hm->history_samples = config->history_samples;
    hm->history_capacity = config->history_capacity;
    hm->clock_fn = config->clock_fn;
    hm->clock_ctx = config->clock_ctx;
    hm->alert_fn = config->alert_fn;
    hm->alert_ctx = config->alert_ctx;
    hm->check_interval_ms = config->check_interval_ms;
    hm->initialized = true;

    memset(hm->metric_slots, 0, hm->metric_capacity * sizeof(hm->metric_slots[0]));
    for (i = 0; i < hm->history_capacity; ++i) {
        hm->history_meta[i].meta.timestamp_ms = 0U;
        hm->history_meta[i].meta.sample_count = 0U;
    }
    if (hm->history_capacity > 0U) {
        memset(hm->history_samples, 0,
               hm->history_capacity * hm->metric_capacity * sizeof(hm->history_samples[0]));
    }

    return MHEALTH_OK;
}

mhealth_err_t mhealth_set_alert(mhealth_t *hm, mhealth_alert_fn alert_fn, void *alert_ctx)
{
    mhealth_err_t status = require_mutable(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    hm->alert_fn = alert_fn;
    hm->alert_ctx = alert_ctx;
    return MHEALTH_OK;
}

mhealth_err_t mhealth_register(
    mhealth_t *hm,
    const mhealth_metric_config_t *metric,
    size_t *out_index)
{
    mhealth_err_t status;
    size_t i;
    mhealth_metric_slot_t slot;

    if (out_index == NULL) {
        return MHEALTH_ERR_NULL;
    }

    status = require_mutable(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    status = validate_metric_config(metric);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (hm->metric_count >= hm->metric_capacity) {
        return MHEALTH_ERR_FULL;
    }

    for (i = 0; i < hm->metric_count; ++i) {
        if (strcmp(hm->metric_slots[i].name, metric->name) == 0) {
            return MHEALTH_ERR_INVALID;
        }
        if (hm->metric_slots[i].metric_id == metric->metric_id) {
            return MHEALTH_ERR_INVALID;
        }
    }

    memset(&slot, 0, sizeof(slot));
    slot.name = metric->name;
    slot.metric_id = metric->metric_id;
    slot.collect_fn = metric->collect_fn;
    slot.collect_ctx = metric->collect_ctx;
    slot.direction = metric->direction;
    slot.warn_threshold = metric->warn_threshold;
    slot.critical_threshold = metric->critical_threshold;
    slot.enabled = true;
    slot.current_sample.metric_id = metric->metric_id;
    slot.current_sample.value = 0;
    slot.current_sample.severity = MHEALTH_SEVERITY_OK;
    slot.current_sample.status = MHEALTH_SAMPLE_UNSAMPLED;
    slot.last_valid_value = 0;
    slot.last_valid_severity = MHEALTH_SEVERITY_OK;

    hm->metric_slots[hm->metric_count] = slot;
    *out_index = hm->metric_count;
    hm->metric_count += 1U;
    return MHEALTH_OK;
}

mhealth_err_t mhealth_enable(mhealth_t *hm, size_t index, bool enabled)
{
    mhealth_metric_slot_t *slot;
    mhealth_err_t status = require_mutable(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (index >= hm->metric_count) {
        return MHEALTH_ERR_NOT_FOUND;
    }

    slot = &hm->metric_slots[index];
    slot->enabled = enabled;
    slot->current_sample.metric_id = slot->metric_id;
    slot->current_sample.value = slot->last_valid_value;
    slot->current_sample.severity = slot->last_valid_severity;
    slot->current_sample.status = enabled ? MHEALTH_SAMPLE_UNSAMPLED : MHEALTH_SAMPLE_DISABLED;
    if (enabled) {
        slot->last_valid_severity = MHEALTH_SEVERITY_OK;
    }

    return MHEALTH_OK;
}

static void enter_busy(mhealth_t *hm)
{
    hm->busy = true;
}

static void leave_busy(mhealth_t *hm)
{
    hm->busy = false;
}

static mhealth_err_t perform_check(
    mhealth_t *hm,
    uint32_t timestamp_ms,
    mhealth_check_result_t *out_result)
{
    mhealth_snapshot_meta_t latest_meta;
    size_t transition_count = 0U;
    size_t failure_count = 0U;
    size_t i;

    latest_meta.timestamp_ms = timestamp_ms;
    latest_meta.sample_count = hm->metric_count;

    enter_busy(hm);
    for (i = 0; i < hm->metric_count; ++i) {
        mhealth_metric_slot_t *slot = &hm->metric_slots[i];
        int32_t value = 0;
        mhealth_collect_result_t collect_result;

        memset(&slot->pending_sample, 0, sizeof(slot->pending_sample));
        slot->pending_emit_alert = false;
        slot->pending_previous_severity = MHEALTH_SEVERITY_OK;
        slot->pending_sample.metric_id = slot->metric_id;

        if (!slot->enabled) {
            slot->pending_sample.value = slot->last_valid_value;
            slot->pending_sample.severity = slot->last_valid_severity;
            slot->pending_sample.status = MHEALTH_SAMPLE_DISABLED;
            continue;
        }

        collect_result = slot->collect_fn(slot->collect_ctx, &value);
        if (collect_result == MHEALTH_COLLECT_OK) {
            mhealth_severity_t previous_severity =
                slot->current_sample.status == MHEALTH_SAMPLE_VALID
                    ? slot->current_sample.severity
                    : MHEALTH_SEVERITY_OK;
            mhealth_severity_t current_severity = evaluate_severity(
                slot->direction,
                slot->warn_threshold,
                slot->critical_threshold,
                value);

            slot->pending_sample.value = value;
            slot->pending_sample.severity = current_severity;
            slot->pending_sample.status = MHEALTH_SAMPLE_VALID;

            if (current_severity != previous_severity) {
                slot->pending_emit_alert = true;
                slot->pending_previous_severity = previous_severity;
                transition_count += 1U;
            }
        } else {
            failure_count += 1U;
            slot->pending_sample.value = slot->last_valid_value;
            slot->pending_sample.severity = slot->last_valid_severity;
            slot->pending_sample.status = MHEALTH_SAMPLE_COLLECTION_FAILED;
        }
    }

    hm->latest_meta = latest_meta;
    for (i = 0; i < hm->metric_count; ++i) {
        mhealth_metric_slot_t *slot = &hm->metric_slots[i];

        slot->current_sample = slot->pending_sample;
        if (slot->pending_sample.status == MHEALTH_SAMPLE_VALID) {
            slot->last_valid_value = slot->pending_sample.value;
            slot->last_valid_severity = slot->pending_sample.severity;
        }
    }

    history_store_snapshot(hm, &latest_meta);
    hm->last_check_ms = timestamp_ms;
    hm->has_last_check = true;
    hm->counters.check_count = saturating_inc_u32(hm->counters.check_count, 1U);
    hm->counters.transition_count = saturating_inc_u32(
        hm->counters.transition_count,
        transition_count > UINT32_MAX ? UINT32_MAX : (uint32_t)transition_count);
    hm->counters.collection_failure_count = saturating_inc_u32(
        hm->counters.collection_failure_count,
        failure_count > UINT32_MAX ? UINT32_MAX : (uint32_t)failure_count);

    if (out_result != NULL) {
        out_result->performed = true;
        out_result->transition_count = transition_count;
        out_result->collection_failure_count = failure_count;
        out_result->timestamp_ms = timestamp_ms;
    }

    for (i = 0; i < hm->metric_count; ++i) {
        const mhealth_metric_slot_t *slot = &hm->metric_slots[i];
        if (slot->pending_emit_alert && hm->alert_fn != NULL) {
            mhealth_alert_t alert;
            alert.metric_index = i;
            alert.metric_id = slot->metric_id;
            alert.name = slot->name;
            alert.value = slot->current_sample.value;
            alert.direction = slot->direction;
            alert.warn_threshold = slot->warn_threshold;
            alert.critical_threshold = slot->critical_threshold;
            alert.previous_severity = slot->pending_previous_severity;
            alert.current_severity = slot->current_sample.severity;
            alert.timestamp_ms = timestamp_ms;
            hm->alert_fn(&alert, hm->alert_ctx);
        }
    }

    leave_busy(hm);
    return MHEALTH_OK;
}

mhealth_err_t mhealth_tick(mhealth_t *hm, mhealth_check_result_t *out_result)
{
    uint32_t timestamp_ms;
    mhealth_err_t status = require_mutable(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (out_result == NULL) {
        return MHEALTH_ERR_NULL;
    }

    check_result_clear(out_result);
    timestamp_ms = hm->clock_fn(hm->clock_ctx);
    if (hm->has_last_check) {
        if ((uint32_t)(timestamp_ms - hm->last_check_ms) < hm->check_interval_ms) {
            out_result->performed = false;
            out_result->timestamp_ms = timestamp_ms;
            return MHEALTH_OK;
        }
    }

    return perform_check(hm, timestamp_ms, out_result);
}

mhealth_err_t mhealth_check_now(mhealth_t *hm, mhealth_check_result_t *out_result)
{
    uint32_t timestamp_ms;
    mhealth_err_t status = require_mutable(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (out_result == NULL) {
        return MHEALTH_ERR_NULL;
    }

    check_result_clear(out_result);
    timestamp_ms = hm->clock_fn(hm->clock_ctx);
    return perform_check(hm, timestamp_ms, out_result);
}

mhealth_err_t mhealth_clear_history(mhealth_t *hm)
{
    size_t i;
    mhealth_err_t status = require_mutable(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    hm->history_head = 0U;
    hm->history_count = 0U;
    for (i = 0; i < hm->history_capacity; ++i) {
        hm->history_meta[i].meta.timestamp_ms = 0U;
        hm->history_meta[i].meta.sample_count = 0U;
    }
    return MHEALTH_OK;
}

mhealth_err_t mhealth_get_counters(const mhealth_t *hm, mhealth_counters_t *out_counters)
{
    mhealth_err_t status = require_ready(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (out_counters == NULL) {
        return MHEALTH_ERR_NULL;
    }
    *out_counters = hm->counters;
    return MHEALTH_OK;
}

mhealth_err_t mhealth_get_metric_count(const mhealth_t *hm, size_t *out_count)
{
    mhealth_err_t status = require_ready(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (out_count == NULL) {
        return MHEALTH_ERR_NULL;
    }
    *out_count = hm->metric_count;
    return MHEALTH_OK;
}

mhealth_err_t mhealth_get_metric_status(
    const mhealth_t *hm,
    size_t index,
    mhealth_metric_status_t *out_status)
{
    const mhealth_metric_slot_t *slot;
    mhealth_err_t status = require_ready(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (out_status == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (index >= hm->metric_count) {
        return MHEALTH_ERR_NOT_FOUND;
    }

    slot = &hm->metric_slots[index];
    out_status->name = slot->name;
    out_status->metric_id = slot->metric_id;
    out_status->enabled = slot->enabled;
    out_status->direction = slot->direction;
    out_status->warn_threshold = slot->warn_threshold;
    out_status->critical_threshold = slot->critical_threshold;
    out_status->last_valid_value = slot->last_valid_value;
    out_status->current_sample = slot->current_sample;
    return MHEALTH_OK;
}

mhealth_err_t mhealth_get_health(
    const mhealth_t *hm,
    mhealth_health_status_t *out_status)
{
    size_t i;
    size_t sampled_count = 0U;
    size_t enabled_count = 0U;
    size_t failure_count = 0U;
    mhealth_severity_t worst = MHEALTH_SEVERITY_OK;
    bool healthy = true;
    mhealth_err_t status = require_ready(hm);

    if (status != MHEALTH_OK) {
        return status;
    }
    if (out_status == NULL) {
        return MHEALTH_ERR_NULL;
    }

    for (i = 0; i < hm->metric_count; ++i) {
        const mhealth_metric_slot_t *slot = &hm->metric_slots[i];
        if (!slot->enabled) {
            continue;
        }
        enabled_count += 1U;
        if (slot->current_sample.status == MHEALTH_SAMPLE_VALID) {
            sampled_count += 1U;
            if (slot->current_sample.severity > worst) {
                worst = slot->current_sample.severity;
            }
            if (slot->current_sample.severity != MHEALTH_SEVERITY_OK) {
                healthy = false;
            }
        } else {
            if (slot->current_sample.status == MHEALTH_SAMPLE_COLLECTION_FAILED) {
                failure_count += 1U;
            }
            healthy = false;
        }
    }

    out_status->healthy = healthy;
    out_status->worst_severity = worst;
    out_status->metric_count = hm->metric_count;
    out_status->enabled_metric_count = enabled_count;
    out_status->sampled_metric_count = sampled_count;
    out_status->collection_failure_count = failure_count;
    return MHEALTH_OK;
}

mhealth_err_t mhealth_get_latest(
    const mhealth_t *hm,
    mhealth_snapshot_meta_t *out_meta,
    mhealth_sample_t *out_samples,
    size_t sample_capacity,
    size_t *out_sample_count)
{
    mhealth_err_t status = require_ready(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (out_meta == NULL || out_sample_count == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (sample_capacity > 0U && out_samples == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (hm->counters.check_count == 0U) {
        return MHEALTH_ERR_NOT_FOUND;
    }
    if (sample_capacity < hm->latest_meta.sample_count) {
        return MHEALTH_ERR_INVALID;
    }

    *out_meta = hm->latest_meta;
    *out_sample_count = hm->latest_meta.sample_count;
    if (hm->latest_meta.sample_count > 0U) {
        snapshot_copy_from_slots(hm, out_samples);
    }
    return MHEALTH_OK;
}

mhealth_err_t mhealth_get_history_count(const mhealth_t *hm, size_t *out_count)
{
    mhealth_err_t status = require_ready(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (out_count == NULL) {
        return MHEALTH_ERR_NULL;
    }
    *out_count = hm->history_count;
    return MHEALTH_OK;
}

mhealth_err_t mhealth_get_history(
    const mhealth_t *hm,
    size_t offset,
    mhealth_snapshot_meta_t *out_meta,
    mhealth_sample_t *out_samples,
    size_t sample_capacity,
    size_t *out_sample_count)
{
    size_t slot_index;
    size_t base;
    mhealth_err_t status = require_ready(hm);
    if (status != MHEALTH_OK) {
        return status;
    }
    if (out_meta == NULL || out_sample_count == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (sample_capacity > 0U && out_samples == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (offset >= hm->history_count) {
        return MHEALTH_ERR_NOT_FOUND;
    }

    slot_index = (hm->history_head + hm->history_capacity - 1U - offset) % hm->history_capacity;
    *out_meta = hm->history_meta[slot_index].meta;
    if (sample_capacity < out_meta->sample_count) {
        return MHEALTH_ERR_INVALID;
    }

    *out_sample_count = out_meta->sample_count;
    base = history_index(hm, slot_index);
    if (out_meta->sample_count > 0U) {
        memcpy(out_samples, &hm->history_samples[base],
               out_meta->sample_count * sizeof(out_samples[0]));
    }
    return MHEALTH_OK;
}

static const char *sample_status_str(mhealth_sample_status_t status)
{
    switch (status) {
    case MHEALTH_SAMPLE_UNSAMPLED:
        return "UNSAMPLED";
    case MHEALTH_SAMPLE_VALID:
        return "VALID";
    case MHEALTH_SAMPLE_DISABLED:
        return "DISABLED";
    case MHEALTH_SAMPLE_COLLECTION_FAILED:
        return "COLLECTION_FAILED";
    default:
        return "INVALID";
    }
}

static mhealth_err_t append_format(
    char *buf,
    size_t buf_size,
    size_t *cursor,
    size_t *required,
    const char *fmt,
    ...)
{
    int rc;
    va_list args;

    va_start(args, fmt);
    rc = vsnprintf(
        buf != NULL && *cursor < buf_size ? buf + *cursor : NULL,
        buf != NULL && *cursor < buf_size ? buf_size - *cursor : 0U,
        fmt,
        args);
    va_end(args);

    if (rc < 0) {
        if (buf != NULL && buf_size > 0U) {
            buf[(buf_size - 1U)] = '\0';
        }
        return MHEALTH_ERR_INVALID;
    }

    *required += (size_t)rc;
    if (buf != NULL && buf_size > 0U) {
        if (*cursor < buf_size) {
            size_t available = buf_size - *cursor;
            size_t advance = (size_t)rc < available ? (size_t)rc : available - 1U;
            *cursor += advance;
            buf[*cursor] = '\0';
        } else {
            buf[buf_size - 1U] = '\0';
        }
    }

    return MHEALTH_OK;
}

mhealth_err_t mhealth_snapshot_format(
    const mhealth_t *hm,
    const mhealth_snapshot_meta_t *meta,
    const mhealth_sample_t *samples,
    size_t sample_count,
    char *buf,
    size_t buf_size,
    size_t *out_written,
    size_t *out_required)
{
    size_t cursor = 0U;
    size_t required = 0U;
    size_t i;
    mhealth_err_t status;

    if (hm == NULL || meta == NULL || out_written == NULL || out_required == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (sample_count > 0U && samples == NULL) {
        return MHEALTH_ERR_NULL;
    }
    if (sample_count != meta->sample_count) {
        return MHEALTH_ERR_INVALID;
    }
    if (buf == NULL && buf_size != 0U) {
        return MHEALTH_ERR_NULL;
    }
    if (buf != NULL && buf_size > 0U) {
        buf[0] = '\0';
    }

    status = append_format(
        buf, buf_size, &cursor, &required,
        "snapshot timestamp_ms=%lu sample_count=%lu\n",
        (unsigned long)meta->timestamp_ms,
        (unsigned long)sample_count);
    if (status != MHEALTH_OK) {
        return status;
    }

    for (i = 0; i < sample_count; ++i) {
        const char *name = "<unknown>";
        const char *severity = mhealth_severity_str(samples[i].severity);
        const char *sample_state = sample_status_str(samples[i].status);
        size_t slot_index;

        for (slot_index = 0; slot_index < hm->metric_count; ++slot_index) {
            if (hm->metric_slots[slot_index].metric_id == samples[i].metric_id) {
                name = hm->metric_slots[slot_index].name;
                break;
            }
        }

        status = append_format(
            buf, buf_size, &cursor, &required,
            "%lu %s id=%u value=%ld severity=%s status=%s\n",
            (unsigned long)i,
            name,
            (unsigned)samples[i].metric_id,
            (long)samples[i].value,
            severity,
            sample_state);
        if (status != MHEALTH_OK) {
            return status;
        }
    }

    *out_required = required;
    *out_written = cursor;
    if (buf != NULL && buf_size > 0U && required >= buf_size) {
        return MHEALTH_ERR_TRUNCATED;
    }
    return MHEALTH_OK;
}
