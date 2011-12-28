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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

struct event_t {
    const char *cmd;
    uid_t uid;
    gid_t gid;
};

static struct event_t event;

char *program_invocation_short_name;
static const char *suspend_cmd = NULL;
static long epochtime = 0;

static long
timespec_to_seconds(struct timespec_t *ts)
{
    return (ts->hour * 3600) + (ts->min * 60) + ts->sec;
}

static void
help(FILE *stream)
{
    fprintf(stream, "usage: %s <timespec>\n\n"
            "  -a, --at SEC          specify an absolute time in seconds from epoch\n"
            "  -c, --command \"CMD\"   execute CMD instead of default '%s'\n"
            "  -e, --event \"CMD\"     execute CMD after wakeup\n"
            "  -h, --help            display this help and exit\n\n"
            "timespec can be any combination of hours, minutes, and seconds\n"
            "specified by hH, mM, and sS, respecitively.\n\n",
            program_invocation_short_name,
            SUSPEND_COMMAND);
    fprintf(stream, "Examples:\n"
            "    %s 1h 20m 42S                   # 1 hour, 20 minutes, 42 seconds\n"
            "    %s 1h20M 2h                     # 3 hours, 20 minutes\n"
            "    %s -a $(date -d tomorrow +%%s)   # 24 hours\n",
            program_invocation_short_name,
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
        execlp("/bin/sh", command, "-c", command, NULL);
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
        { "at",      no_argument,       NULL, 'a' },
        { "command", required_argument, NULL, 'c' },
        { "event",   required_argument, NULL, 'e' },
        { "execute", required_argument, NULL, 'e' },
        { "help",    no_argument,       NULL, 'h' },
        { 0, 0, 0, 0 }
    };

    while((opt = getopt_long(argc, argv, "ac:e:h", opts, NULL)) != -1) {
        switch(opt) {
            case 'a':
                epochtime = 1;
                break;
            case 'c':
                suspend_cmd = optarg;
                break;
            case 'e':
                event.cmd = optarg;
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
                    accum += *f - '0';
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
    if(epochtime) {
        char *endptr;
        int64_t now, epoch, diff;

        epoch = strtoll(argv[optind], &endptr, 10);
        if(*endptr != '\0' || errno == ERANGE) {
            fprintf(stderr, "failed to parse absolute time: %s: %s",
                    argv[optind], strerror(errno == ERANGE ? ERANGE : EINVAL));
            return 1;
        }

        if(time((time_t *)&now) == (time_t)-1) {
            fprintf(stderr, "error: failed to get current time\n");
            return 1;
        }

        if(epoch < now) {
            fprintf(stderr, "error: cannot specify a time in the past\n");
            return 1;
        }

        diff = epoch - now;
        ts->hour = diff / 3600;
        diff %= 3600;
        ts->min = diff / 60;
        ts->sec = diff % 60;
    } 
    
    else {
        while(optind < argc) {
            if(parse_timefragment(argv[optind], ts) != 0) {
                fprintf(stderr, "error: failed to parse time: %s\n", argv[optind]);
                return 1;
            }
            optind++;
        }

        if(timespec_to_seconds(ts) == 0) {
            fprintf(stderr, "error: duration must be non-zero\n");
            return 1;
        }
    }

    return 0;
}

static void
signal_event(union sigval sival)
{
    struct event_t *event;

    event = sival.sival_ptr;
    /* don't execute anything as root. still sucks since the env is still the root one 
     * aaaaannnnd the signal event triggers a few times before that takes effect */
    /* TODO: fix $HOME */

    /*
    printf("uid %d\n", getuid());
    printf("gid %d\n", getgid());
    */
    if(setgid(event->uid) != 0) {
        fprintf(stderr, "%s", strerror(errno));
    }

    if(setgid(event->gid) != 0) {
        fprintf(stderr, "%s", strerror(errno));
    }

    /*
    printf("uid %d\n", getuid());
    printf("gid %d\n", getgid());
    */

    fprintf(stdout, "tock.\n");

    execl("/bin/sh", event->cmd, "-c", event->cmd, NULL);
    fprintf(stderr, "error: failed to execute command: %s: %s\n",
            (const char *)event->cmd, strerror(errno));
    exit(1);
}

static int
create_alarm(struct timespec_t *ts)
{
    struct itimerspec wakeup;
    struct sigevent sigev;
    union sigval sigv;
    timer_t timerid;

    char *env;

    memset(&wakeup, 0, sizeof(struct itimerspec));

    if(event.cmd) {
        memset(&sigev, 0, sizeof(struct sigevent));

        /* if run with sudo restore uid/gid on event */
        env = getenv("SUDO_UID");
        if(env != NULL) {
            event.uid = strtol(env, NULL, 10);
        }
        else { event.uid = getuid(); }

        env = getenv("SUDO_GID");
        if(env != NULL) {
            event.gid = strtol(env, NULL, 10);
        }
        else { event.gid = getgid(); }

        sigv.sival_ptr = (void *)&event;
        sigev.sigev_notify = SIGEV_THREAD;
        sigev.sigev_notify_function = signal_event;
        sigev.sigev_value = sigv;
    }

    /* init timer */
    if(timer_create(CLOCK_REALTIME_ALARM, event.cmd ? &sigev : NULL,
                &timerid) != 0) {
        perror("error: failed to create timer");
        return 1;
    }

    /* init itimerspec */
    if(clock_gettime(CLOCK_REALTIME_ALARM, &wakeup.it_value)) {
        perror("error: failed to get time from RTC");
        return 1;
    }

    /* set itimerspec to some future time */
    wakeup.it_value.tv_sec += timespec_to_seconds(ts);

    if(timer_settime(timerid, TIMER_ABSTIME, &wakeup, NULL)) {
        perror("error: failed to set wakeup time");
        return 1;
    }

    fprintf(stdout, "timer set for wakeup in: %ld hours %ld min %ld sec\n",
            ts->hour, ts->min, ts->sec);

    fprintf(stdout, "tick.\n");
    if(event.cmd == NULL) {
        fprintf(stdout, "tock.\n");
    }

    return 0;
}
int
main(int argc, char *argv[])
{
    struct timespec_t ts;

    if(argc <= 1) {
        fprintf(stderr, "error: no timespec specified (use -h for help)\n");
        help(stderr);
    }

    memset(&ts, 0, sizeof(struct timespec_t));

    if(parse_options(argc, argv) != 0) {
        return 1;
    }

    if(parse_timespec(optind, argc, argv, &ts) != 0) {
        return 2;
    }

    if(create_alarm(&ts) != 0) {
        return 3;
    }

    if(do_suspend(suspend_cmd ? suspend_cmd : SUSPEND_COMMAND) != 0) {
        return 4;
    }

    pthread_exit(NULL);
    return EXIT_SUCCESS;
}

/* vim: set et ts=4 sw=4: */
