# Porting Guide

Two files: `mhealth.h` + `mhealth.c`. C99. Provide a clock function.

```cmake
add_library(microhealth STATIC lib/microhealth/src/mhealth.c)
target_include_directories(microhealth PUBLIC lib/microhealth/include)
```

Collectors are platform-specific — you write them to bridge your libraries.
