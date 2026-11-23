/* LD_PRELOAD shim for tests/run_clock.sh: time(NULL) returns the epoch seconds in $V32_FAKE_TIME,
 * so v32sim and the stock console logic (both start their clock from localtime(time(NULL)))
 * can be compared at any date, zone and daylight-saving state. Nothing else is touched. */
#include <stdlib.h>
#include <time.h>

time_t time (time_t *out)
{
    const char *e = getenv ("V32_FAKE_TIME");
    time_t t = e ? (time_t) strtoll (e, NULL, 10) : 0;
    if (out) *out = t;
    return t;
}
