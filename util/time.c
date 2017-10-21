#define _POSIX_C_SOURCE 199309L
#include <time.h>

/*
 * Returns the difference between two timespecs in ms
 * x _must_ be larger than y otherwise the result is undefined
 */
int difftimespec(struct timespec *x, struct timespec *y)
{
	int ret = 0;
	time_t tv_sec = x->tv_sec - y->tv_sec;

	if (x->tv_nsec < y->tv_nsec) {
		ret = (x->tv_nsec + (1e9) - y->tv_nsec) / 1e6;
		tv_sec -= 1;
	} else {
		ret = (x->tv_nsec - y->tv_nsec) / 1e6;
	}
	ret += tv_sec * 1e3;

	return ret;
}
