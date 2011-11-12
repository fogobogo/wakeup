#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
    timer_t id;

    struct itimerspec alarm;

    int errno;

    if(argc < 2) {
        printf("usage: %s [seconds until wakeup]\n", argv[0]);
        exit(1);
    }

    /* doesn't accept CLOCK_REALTIME_ALARM */
    if(timer_create(CLOCK_REALTIME_ALARM, NULL, &id)) {
        perror("");
    }

    /* the only place that seems to actually accept it */
    if(clock_gettime(CLOCK_REALTIME_ALARM, &alarm.it_value)) {
        perror("");
    }

    alarm.it_value.tv_sec += atoi(argv[1]);
    alarm.it_value.tv_nsec = 0;

    alarm.it_interval.tv_sec = 0;
    alarm.it_interval.tv_nsec = 0;

    /* set timer */
    if(timer_settime(id, TIMER_ABSTIME, &alarm, 0)) {
        perror("");
    }

    printf("tick.\n");

    while(1);

    /* never executes */
    printf("tock.\n");

    return 0;
}


