# Porting Guide

## Required Integration Points

- Provide caller-owned `mhealth_t`.
- Provide caller-owned metric slot storage.
- Optionally provide caller-owned history storage.
- Provide a monotonic modulo-`2^32` clock callback.
- Provide synchronous collectors that always return.

## Unsupported Assumptions

- No concurrent same-instance access without caller serialization.
- No guaranteed ISR safety.
- No cleanup/defer hooks.
- No persistence, logging, queueing, or transport built into the library.

## CMake

```cmake
find_package(microhealth CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE microhealth::microhealth)
```

## Plain C Build

Compile `src/mhealth.c` as C and include `include/mhealth.h`.

The formatter depends on the hosted C library `snprintf`. If that boundary is unavailable on a target, keep the core library build and gate formatter usage separately.
