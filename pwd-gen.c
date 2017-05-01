/*
 * Copyright (C) 2017 Niko Rosvall <niko@byteptr.com>
 */

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "entry.h"
#include "utils.h"
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

/*Generates random number between 0 and max.
 *Function should generate uniform distribution.
 */
static unsigned int rand_between(unsigned int min, unsigned int max)
{
    int r;
    const unsigned int range = 1 + max - min;
    const unsigned int buckets = RAND_MAX / range;
    const unsigned int limit = buckets * range;

    /*Create equal size buckets all in a row, then fire randomly towards
     *the buckets until you land in one of them. All buckets are equally
     *likely. If you land off the end of the line of buckets, try again.
     */
    do
    {
        r = rand();
    } while (r >= limit);

    return min + (r / buckets);
}

/* Simply generate secure password
 * and output it to the stdout, use clock_gettime for
 * seeding srand
 *
 * Caller must free the return value.
 */
char *generate_password(int length)
{
    if(length < 1 || length > RAND_MAX)
        return NULL;

    char *pass = NULL;
    char *alpha = "abcdefghijklmnopqrstuvwxyz" \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    "0123456789?)(/%#!?)=";
    unsigned int max;
    unsigned int number;
    struct timespec tspec;

#ifdef __MACH__
    /*OS X does not have clock_gettime, use clock_get_time*/
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    tspec.tv_sec = mts.tv_sec;
    tspec.tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_MONOTONIC, &tspec);
#endif

    srand(tspec.tv_nsec);
    max = strlen(alpha) - 1;
    pass = tmalloc((length + 1) * sizeof(char));

    for(int j = 0; j < length; j++)
    {
        number = rand_between(0, max);
        pass[j] = alpha[number];
    }

	pass[length] = '\0';

    fprintf(stdout, "%s\n", pass);

    return pass;
}
