# Changelog

## [1.0.0] — 2026-03-20

### Added

- Metric registration with configurable collectors and thresholds.
- ABOVE/BELOW directional comparison.
- WARN + CRITICAL dual thresholds.
- Edge-triggered alert callbacks (fire on severity transitions only).
- Rate-limited tick with configurable check interval.
- Force-check via check_now().
- Snapshot history ring buffer.
- Human-readable snapshot formatting.
- 32 tests covering thresholds, transitions, recovery, history, rate limiting.
