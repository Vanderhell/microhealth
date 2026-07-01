# Support

Use the right channel for the right kind of question.

## Open a bug report when

- the documented API behaves differently than specified,
- a build, test, install, or consumer fixture fails reproducibly,
- a sanitizer, compiler, or static-analysis finding points to a real defect,
- snapshot, callback, or threshold behavior contradicts the documented contract.

Use the bug report template and include exact commands and diagnostics.

## Open a feature request when

- you want an in-scope API or behavior change,
- you want additional examples, fixtures, or CI coverage,
- you need a new well-known metric ID.

## Use support questions for

- integration guidance,
- choosing history capacity or metric capacity,
- interpreting sample states,
- CMake `find_package` usage,
- callback, thread, or ISR limitations.

## Before asking

- read `README.md`,
- read `docs/API_REFERENCE.md`,
- read `docs/COOKBOOK.md`,
- check `docs/FAQ.md`,
- confirm whether your usage is inside supported scope.

## Project scope reminders

- No heap allocation in the library.
- No built-in logger, transport, persistence, or scheduler.
- No thread-safety claim for one `mhealth_t`.
- No ISR-safety claim by default.

