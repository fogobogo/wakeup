/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 * 
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

/* gcc wakeup.c -o wakeup -lrt */
#define SUSPEND_COMMAND "pm-suspend"

void
help(char *name)
{
    printf("usage: %s [time]\n", name);
    printf("\n");
    printf("-h\tprint this help.\n");
    printf("\n");
    printf("example: %s 1h20m30s\n", name);
    printf("will (should) wake up the system from suspend in\n");
    printf("1 hour 20 minutes and 30 seconds.\n");
}


int
main(int argc, char *argv[])
{
    int i, c;
    int accum = 0;
    unsigned int hour = 0;
    unsigned int min = 0;
    unsigned int sec = 0;

    char *arg;
    char tmp;

    timer_t id;

    struct itimerspec wakeup;

    int errno;

    if(argc <= 1) {
        help(argv[0]);
        exit(1);
    }

    /* god i hate arg parsing */
    for(i = 1; i < argc; i++) {
        arg = argv[i];
        c = 0;

        if(strcmp(argv[i],"--help") == 0) {
            help(argv[0]);
            exit(1);
        }

        else if(strcmp(argv[i], "-h") == 0) {
            help(argv[0]);
            exit(1);
        }

        while(arg[c] != '\0') {
            switch(arg[c]) {
                case 'h': 
                    hour = accum;
                    accum = 0;
                case 'm':
                    min = accum;
                    accum = 0;
                case 's':
                    sec = accum;
                    accum = 0;
                default:
                    if (isdigit(arg[c])) {
                        /* copy char convert to int */
                        tmp = arg[c];
                        accum = accum + atoi(&tmp);

                        if(isdigit(arg[c + 1])) {
                            accum *= 10;
                        }
                    }
                    break;
            } /* switch end */
            c++;
        } /* while end */
    }

    if(hour == 0 && min == 0 && sec == 0)
    {
        printf("I don't understand. :<\n");
        printf("try -h or --help for help\n");
        exit(1);
    }

    /* init timer */
    if(timer_create(CLOCK_REALTIME_ALARM, NULL, &id)) {
        if(errno == EPERM) {
            printf("Oops. Insufficent rights, can't do that. :/\n");
        }
        perror("");
        exit(1);
    }


    /* init itimerspec */
    if(clock_gettime(CLOCK_REALTIME_ALARM, &wakeup.it_value)) {
        perror("clock_gettime");
        exit(1);
    }

    /* convert to seconds */
    hour *= 3600;
    min *= 60;

    /* set itimerspec to some future time */
    wakeup.it_value.tv_sec += (hour + min + sec);
    wakeup.it_value.tv_nsec = 0;

    wakeup.it_interval.tv_sec = 0;
    wakeup.it_interval.tv_nsec = 0;

    /* set timer */
    if(timer_settime(id, TIMER_ABSTIME, &wakeup, NULL)) {
        perror("timer_settime");
        exit(1);
    }

    printf("Ok. will wakeup from suspend in: %u hours %u min %u sec\n", (hour / 3600), (min / 60), sec);
    printf("tick.\n");

    system(SUSPEND_COMMAND);

    /* never excecutes. needs a signal callback to work */
    printf("tock.\n");

    return 0;
}

