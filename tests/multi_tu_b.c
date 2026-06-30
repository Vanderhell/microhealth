#include "mhealth.h"

mhealth_collect_result_t multi_collect(void *ctx, int32_t *out_value)
{
    (void)ctx;
    *out_value = 0;
    return MHEALTH_COLLECT_OK;
}
