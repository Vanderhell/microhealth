# microhealth

[![CI](https://github.com/Vanderhell/microhealth/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/Vanderhell/microhealth/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C99](https://img.shields.io/badge/language-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)

Runtime system health monitor for embedded systems.

`C99` | `Zero dependencies` | `Zero allocations` | `Callback-driven` | `Portable`

## Why microhealth?

Your firmware can already have heap tracking, persistent logs, crash dumps, and MQTT queueing.
`microhealth` adds a small runtime monitor that continuously:

- collects metrics from your existing subsystems,
- compares values against WARN/CRITICAL thresholds,
- triggers alerts only on severity transitions.

That gives early warning before a device reaches failure state.

## Features

- Callback-based metric collection (`mhealth_collect_fn`)
- Directional thresholding (`MHEALTH_ABOVE` / `MHEALTH_BELOW`)
- Dual severity levels (WARN + CRITICAL)
- Edge-triggered alerts (no repeated spam on same severity)
- Configurable check interval + forced immediate check
- Snapshot history ring buffer
- Human-readable snapshot formatting for shell/log output

## Project Structure

- `include/mhealth.h` - public API
- `src/mhealth.c` - implementation
- `tests/test_all.c` - unit and scenario tests
- `docs/API_REFERENCE.md` - API overview
- `docs/DESIGN.md` - design rationale
- `docs/PORTING_GUIDE.md` - porting notes

## Quick Start

```c
#include "mhealth.h"

static int32_t collect_heap(void *ctx) { return (int32_t)mmt_get_free_bytes(ctx); }
static int32_t collect_temp(void *ctx) { (void)ctx; return (int32_t)(adc_read_temp() * 10); }

static void on_alert(const mhealth_alert_t *a, void *ctx) {
    (void)ctx;
    MLOG_WARN("HEALTH", "%s: %ld -> %s", a->name, (long)a->value,
              mhealth_severity_str(a->severity));
}

mhealth_t hm;
mhealth_init(&hm, HAL_GetTick, 5000);
mhealth_set_alert(&hm, on_alert, NULL);

mhealth_register(&hm, "heap_free", MHEALTH_METRIC_HEAP_FREE,
                 collect_heap, &tracker, MHEALTH_BELOW, 4000, 1000);
mhealth_register(&hm, "mcu_temp", MHEALTH_METRIC_MCU_TEMP,
                 collect_temp, NULL, MHEALTH_ABOVE, 700, 850);

while (1) {
    mhealth_tick(&hm);
}
```

## Build and Test

Linux/macOS (or CI-compatible shell):

```sh
make -C tests clean
make -C tests
```

Manual compile:

```sh
gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -Iinclude src/mhealth.c tests/test_all.c -o tests/test_all
./tests/test_all
```

## CI

GitHub Actions workflow is in `.github/workflows/ci.yml`.
It builds and runs tests on branch `master` for both `gcc` and `clang`.

## Release Readiness Checklist

- README with usage, build/test, docs links, and badges
- MIT license (copyright owner: Vanderhell)
- Contributing guide
- Changelog
- CI workflow
- Tests included

## Ecosystem Integrations

- [MCU-Malloc-Tracker](https://github.com/Vanderhell/MCU-Malloc-Tracker)
- [nvlog](https://github.com/Vanderhell/nvlog)
- [iotspool](https://github.com/Vanderhell/iotspool)
- [microlog](https://github.com/Vanderhell/microlog)
- [microsh](https://github.com/Vanderhell/microsh)
- [microcbor](https://github.com/Vanderhell/microcbor)
- [panicdump](https://github.com/Vanderhell/panicdump)

## Documentation

- [API Reference](docs/API_REFERENCE.md)
- [Design Rationale](docs/DESIGN.md)
- [Porting Guide](docs/PORTING_GUIDE.md)
- [Contributing](CONTRIBUTING.md)
- [Changelog](CHANGELOG.md)

## License

MIT - see [LICENSE](LICENSE).