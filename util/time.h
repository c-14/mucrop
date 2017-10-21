#ifndef MU_TIME_H
#define MU_TIME_H

/*
 * Returns the difference between two timespecs in ms
 * x _must_ be larger than y otherwise the result is undefined
 */
extern int difftimespec(struct timespec *x, struct timespec *y);

#endif
