# Design Rationale

## 1. Callback-based collectors — no hard dependencies
microhealth collects metrics via user-provided callbacks. It never includes MCU-Malloc-Tracker, nvlog, or any other library. You wire the dependencies you have, skip what you don't. Testable in isolation with mock collectors.

## 2. Edge-triggered alerts — not level-triggered
Alerts fire only when severity CHANGES (OK→WARN, WARN→CRITICAL, etc.), not on every tick. Prevents log spam and notification fatigue.

## 3. Dual threshold (WARN + CRITICAL)
Two levels give you time to react. WARN = "pay attention." CRITICAL = "act now." Maps naturally to log levels and monitoring systems.

## 4. Directional comparison
Heap and battery alert BELOW threshold. Temperature and errors alert ABOVE. Per-metric, not global.

## 5. Rate-limited tick
check_interval_ms prevents wasting CPU. check_now() bypasses for on-demand inspection.

| Decision | Gains | Costs |
|----------|-------|-------|
| Callback collectors | Zero hard deps, testable | User writes wiring |
| Edge-triggered | No spam | Misses sub-tick oscillations |
| Dual threshold | Gradual escalation | Two values per metric |
| Rate limiting | CPU savings | Delayed detection |
