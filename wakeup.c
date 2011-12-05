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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <getopt.h>

#ifndef SUSPEND_COMMAND
#  define SUSPEND_COMMAND "pm-suspend"
#endif

static long hour, min, sec;

extern char *program_invocation_short_name;

static void
help(FILE *stream)
{
    fprintf(stream, "usage: %s timespec\n\n", program_invocation_short_name);
    fprintf(stream, "  -h, --help        display this help and exit\n\n");
    fprintf(stream, "example: %s 1h20m30s\n", program_invocation_short_name);
    fprintf(stream, "will (should) wake up the system from suspend in\n");
    fprintf(stream, "1 hour 20 minutes and 30 seconds.\n");

    exit(stream == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
do_suspend(void)
{
    pid_t pid;
    int status;

    pid = vfork();
    if(pid == -1) {
        perror("fork");
        return;
    }

    if(pid == 0) {
        execlp(SUSPEND_COMMAND, SUSPEND_COMMAND, NULL);
        fprintf(stderr, "error: exec: %s\n", strerror(errno));
        _exit(0);
    }

    while(wait(&status) != pid);
    if(WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "error: %s exited with status %d\n", SUSPEND_COMMAND,
            WEXITSTATUS(status));
    }
}

static void
parse_options(int argc, char **argv)
{
    int opt;
    static struct option opts[] = {
        { "help", no_argument, NULL, 'h' },
        { 0, 0, 0, 0 }
    };

    while((opt = getopt_long(argc, argv, "h", opts, NULL)) != -1) {
        switch(opt) {
            case 'h':
                help(stdout);
                break;
            default:
            case '?':
                exit(1);
        }
    }

    /* optind is set globally */
}

static long
parse_timefragment(const char *fragment)
{
    const char *f;
    long accum = 0;

    for(f = fragment; *f != '\0'; f++) {
        switch(*f) {
            case 'h':
            case 'H':
                hour += accum;
                accum = 0;
                break;
            case 'm':
            case 'M':
                min += accum;
                accum = 0;
                break;
            case 's':
            case 'S':
                sec += accum;
                accum = 0;
                break;
            default:
                if(isdigit(*f)) {
                    accum += (*f - 48);

                    /* look ahead */
                    if(isdigit(f[1])) {
                        accum *= 10;
                    }
                } else {
                    fprintf(stderr, "illegal character in format: %c\n", *f);
                    return -1;
                }
        }
    }

    return 0;
}

static long
parse_timespec(int optind, int argc, char **argv)
{
    while(optind < argc) {
        if(parse_timefragment(argv[optind]) < 0) {
            fprintf(stderr, "failed to parse time: %s\n", argv[optind]);
            return 1;
        }
        optind++;
    }

    return 0;
}

static int
create_alarm(struct itimerspec *wakeup)
{
    timer_t id;

    /* init timer */
    if(timer_create(CLOCK_REALTIME_ALARM, NULL, &id) != 0) {
        perror("error: failed to create timer");
        return 1;
    }

    memset(wakeup, 0, sizeof(struct itimerspec));

    /* init itimerspec */
    if(clock_gettime(CLOCK_REALTIME_ALARM, &wakeup->it_value)) {
        perror("error: failed to get time from RTC");
        return 1;
    }

    /* set itimerspec to some future time */
    wakeup->it_value.tv_sec += sec + (min * 60) + (hour * 60 * 60);

    if(timer_settime(id, TIMER_ABSTIME, wakeup, NULL)) {
        perror("error: failed to set wakeup time");
        return 1;
    }

    return 0;
}

int
main(int argc, char *argv[])
{
    struct itimerspec wakeup;

    if(argc <= 1) {
        help(stderr);
    }

    parse_options(argc, argv);
    if(parse_timespec(optind, argc, argv) < 0) {
        return EXIT_FAILURE;
    }

    if(hour == 0 && min == 0 && sec == 0) {
        fprintf(stderr, "error: no timespec specified (use -h or --help for help)\n");
        return EXIT_FAILURE;
    }

    if(create_alarm(&wakeup) != 0) {
        return EXIT_FAILURE;
    }
    do_suspend();

    /* never excecutes. needs a signal callback to work */
    printf("tock.\n");

    return EXIT_SUCCESS;
}

/* vim: set et ts=4 sw=4: */
