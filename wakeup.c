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
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef SUSPEND_COMMAND
#  define SUSPEND_COMMAND "pm-suspend"
#endif

struct timespec_t {
    long hour;
    long min;
    long sec;
};

extern char *program_invocation_short_name;
static const char *suspend_cmd;
static const char *user_cmd = NULL;

static long
timespec_to_seconds(struct timespec_t *ts)
{
    return (ts->hour * 3600) + (ts->min * 60) + ts->sec;
}

static void
help(FILE *stream)
{
    fprintf(stream, "usage: %s <timespec>\n\n"
            "  -c, --command CMD     execute CMD instead of default '%s'\n"
            "  -e, --execute CMD     execute CMD _after_ waking up\n"
            "  -h, --help            display this help and exit\n\n"
            "timespec can be any combination of hours, minutes, and seconds\n"
            "specified by hH, mM, and sS, respecitively.\n\n"
            "Examples:\n"
            "    %s 1h 20m 42S  # 1 hour, 20 minutes, 42 seconds\n"
            "    %s 1h20M 2h    # 3 hours, 20 minutes\n",
            program_invocation_short_name,
            SUSPEND_COMMAND,
            program_invocation_short_name,
            program_invocation_short_name);

    exit(stream == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int
do_suspend(const char *command)
{
    pid_t pid;
    int status;

    pid = vfork();
    if(pid == -1) {
        perror("fork");
        return 1;
    }

    if(pid == 0) {
        execlp(command, command, NULL);
        fprintf(stderr, "error: failed to execute %s: %s\n", command,
                strerror(errno));
        _exit(127);
    }

    while(wait(&status) != pid);
    if(WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "error: %s exited with status %d\n", command,
            WEXITSTATUS(status));
        return 1;
    }

    fprintf(stdout, "tick.\n");

    return 0;
}

static int
parse_options(int argc, char **argv)
{
    int opt;
    static struct option opts[] = {
        { "command", required_argument, NULL, 'c' },
        { "execute", required_argument, NULL, 'e' },
        { "help",    no_argument,       NULL, 'h' },
        { 0, 0, 0, 0 }
    };

    while((opt = getopt_long(argc, argv, "c:e:h", opts, NULL)) != -1) {
        switch(opt) {
            case 'c':
                suspend_cmd = optarg;
                break;
            case 'e':
                user_cmd = optarg;
                break;
            case 'h':
                help(stdout);
                break;
            default:
            case '?':
                return 1;
        }
    }

    /* optind is set globally */
    return 0;
}

static int
parse_timefragment(const char *fragment, struct timespec_t *ts)
{
    const char *f;
    long accum = 0;

    for(f = fragment; *f != '\0'; f++) {
        switch(*f) {
            case 'h':
            case 'H':
                ts->hour += accum;
                accum = 0;
                break;
            case 'm':
            case 'M':
                ts->min += accum;
                accum = 0;
                break;
            case 's':
            case 'S':
                ts->sec += accum;
                accum = 0;
                break;
            default:
                if(isdigit(*f)) {
                    accum *= 10;
                    accum += (*f - 48);
                } else {
                    fprintf(stderr, "illegal character in format: %c\n", *f);
                    return 1;
                }
        }
    }

    return 0;
}

static int
parse_timespec(int optind, int argc, char **argv, struct timespec_t *ts)
{
    while(optind < argc) {
        if(parse_timefragment(argv[optind], ts) < 0) {
            fprintf(stderr, "failed to parse time: %s\n", argv[optind]);
            return 1;
        }
        optind++;
    }

    if(timespec_to_seconds(ts) == 0) {
        fprintf(stderr, "error: duration must be non-zero\n");
        return 1;
    }

    return 0;
}

static int
create_alarm(struct itimerspec *wakeup, struct timespec_t *ts, struct sigevent sigev)
{
    timer_t timerid;

    /* init timer */
    if(timer_create(CLOCK_REALTIME_ALARM, &sigev, &timerid) != 0) {
        perror("error: failed to create timer");
        return 1;
    }

    /* init itimerspec */
    if(clock_gettime(CLOCK_REALTIME_ALARM, &wakeup->it_value)) {
        perror("error: failed to get time from RTC");
        return 1;
    }

    /* set itimerspec to some future time */
    wakeup->it_value.tv_sec += timespec_to_seconds(ts);

    if(timer_settime(timerid, TIMER_ABSTIME, wakeup, NULL)) {
        perror("error: failed to set wakeup time");
        return 1;
    }

    printf("timer set for wakeup in: %ld hours %ld min %ld sec\n",
            ts->hour, ts->min, ts->sec);

    return 0;
}

static void
signal_function(union sigval sival)
{
    fprintf(stdout, "tock.\n");
    /* houston, we are having a problem */
    /*
    system((char *)sival.sival_ptr);
    */
    exit(0);
}


int
main(int argc, char *argv[])
{
    struct itimerspec wakeup;
    struct timespec_t ts;
    struct sigevent sigev;
    union sigval sival;

    if(argc <= 1) {
        fprintf(stderr, "error: no timespec specified (use -h for help)\n");
        help(stderr);
    }

    memset(&ts, 0, sizeof(struct timespec_t));
    memset(&wakeup, 0, sizeof(struct itimerspec));
    memset(&sigev, 0, sizeof(struct sigevent));


    if(parse_options(argc, argv) != 0) {
        return EXIT_FAILURE;
    }

    if(parse_timespec(optind, argc, argv, &ts) != 0) {
        return EXIT_FAILURE;
    }

    sival.sival_ptr = (void *)user_cmd;
    /* init a signal event */
    sigev.sigev_notify = SIGEV_THREAD;
    sigev.sigev_notify_function = signal_function;
    sigev.sigev_value = sival;

    if(create_alarm(&wakeup, &ts, sigev) != 0) {
        return EXIT_FAILURE;
    }

    if(do_suspend(suspend_cmd ? suspend_cmd : SUSPEND_COMMAND) != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* vim: set et ts=4 sw=4: */
