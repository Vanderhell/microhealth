#include "mhealth.h"

int main(void)
{
    mhealth_t hm;
    return (int)sizeof(hm.metrics) + MHEALTH_MAX_METRICS;
}
