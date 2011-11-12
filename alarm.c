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
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

/* gcc alarm.c -o alarm -lrt */

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

    struct itimerspec alarm;

    int rv; /* return code */
    int errno;

    if(argc < 2) {
        printf("usage: %s [time]\n", argv[0]);
        exit(1);
    }

    /* god i hate arg parsing */
    for(i = 1; i < argc; i++) {
        arg = argv[i];
        c = 0;
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
                        accum += atoi(&tmp);

                        if(isdigit(arg[c + 1])) {
                            accum *= 10;
                        }
                    }
                    break;
            } /* switch end */
            c++;
        } /* while end */
    }

    printf("wake from suspend in: %u hours %u min %u sec\n", hour, min, sec);

    /* convert to seconds */
    hour *= 3600;
    min *= 60;

    /* init timer */
    rv = timer_create(CLOCK_REALTIME_ALARM, NULL, &id);
    if(rv != 0) {
        perror("");
        exit(1);
    }

    /* init itimerspec */
    rv = clock_gettime(CLOCK_REALTIME_ALARM, &alarm.it_value);
    if(rv != 0) {
        perror("");
        exit(1);
    }

    /* set itimerspec to some future date */
    alarm.it_value.tv_sec += (hour + min + sec);
    alarm.it_interval.tv_sec = 1;
    alarm.it_interval.tv_nsec = 0;

    /* set timer */
    rv = timer_settime(&id, TIMER_ABSTIME, &alarm, 0);
    if(rv != 0) {
        perror("");
        exit(1);
    }

    printf("tick.\n");

    sleep(1);

    /* never excecutes. needs a signal callback to work */
    printf("tock.\n");

    return 0;
}


