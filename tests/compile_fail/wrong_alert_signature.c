#include "mhealth.h"

static void wrong_alert(mhealth_alert_t *alert, void *ctx)
{
    (void)alert;
    (void)ctx;
}

int main(void)
{
    mhealth_alert_fn fn = wrong_alert;
    return fn != 0;
}
