#include "util.h"
#include <stdlib.h>
#include <sys/time.h>

struct timeval get_time() {
    struct timeval tval;
    gettimeofday( &tval, NULL );
    return tval;
}

long get_usec_elapsed( struct timeval* after, struct timeval* before ) {
    struct timeval result;
    timersub( after, before, &result );
    return result.tv_sec * 1000000L + result.tv_usec;
}
