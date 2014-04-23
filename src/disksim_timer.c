/*
 * disksim_timer.c
 *
 *  Created on: Apr 21, 2014
 *      Author: zyz915
 */

#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>

#define NUM_TIMERS		32

static long long total_milli[NUM_TIMERS] = {0};
static struct timeval start[NUM_TIMERS], stop[NUM_TIMERS];

void timer_start(int handle) {
	gettimeofday(start + handle, NULL);
}

void timer_end(int handle) {
	gettimeofday(stop + handle, NULL);
	total_milli[handle] += (stop[handle].tv_sec - start[handle].tv_sec) * 1000000ll
			+ (stop[handle].tv_usec - start[handle].tv_usec);
}

void timer_reset(int handle) {
	total_milli[handle] = 0;
}

void timer_reset_all() {
	memset(total_milli, 0, sizeof(long long) * NUM_TIMERS);
}

long long timer_millisecond(int handle) {
	return total_milli[handle];
}

long long timer_microsecond(int handle) {
	return total_milli[handle] / 1000;
}
