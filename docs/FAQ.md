# FAQ

## Is `microhealth` thread-safe?

No. One `mhealth_t` requires caller-side serialization for mutation and for queries that race with mutation.

## Is it ISR-safe?

Not by default. Calling checks from an ISR is only your responsibility if collectors, callbacks, and instance access are proven ISR-safe and non-concurrent on that platform.

## Does disabling history disable latest snapshots?

No. `mhealth_get_latest` works without history storage.

## Are failed collectors treated as zero?

No. Collection failure is an explicit sample state, not a numeric value.

## What happens when I disable a metric?

The metric becomes `MHEALTH_SAMPLE_DISABLED`, is excluded from aggregate severity, and emits no fabricated recovery event.

## What happens when I re-enable a metric?

It becomes `MHEALTH_SAMPLE_UNSAMPLED`. The next successful non-OK sample produces a fresh transition event.

## Are thresholds strict or inclusive?

Inclusive.

- ABOVE: `value >= warn`, `value >= critical`
- BELOW: `value <= warn`, `value <= critical`

Equal warn/critical thresholds are rejected.

## Is there hysteresis or debounce?

No.

## Can I keep `mhealth_alert_t *` after the callback returns?

No. Event pointers are transient and valid only during the callback.

## Can I call APIs from inside callbacks?

Query APIs are allowed. Same-instance mutating APIs return `MHEALTH_ERR_BUSY`.

## Is history persistent?

No. History is volatile caller-owned RAM and is lost on reinit, reset, watchdog, brownout, and power loss.

## Why do examples cast `const void *` callbacks to the public callback types?

The public API uses `void *` callback signatures for compatibility. Examples may use const-correct local helpers and cast once at assignment time.

## Does the formatter require the C library?

Yes. Snapshot formatting uses `snprintf`.

