#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "mem.h"

struct mu_error *create_errlist(size_t siz)
{
	struct mu_error *head, *elem, *prev = NULL;

	for (size_t i = 0; i < siz; i++) {
		elem = mallocz(sizeof(struct mu_error));
		if (elem == NULL) {
			goto fail;
		}
		elem->prev = prev;
		if (prev != NULL)
			prev->next = elem;
		else
			head = elem;
		prev = elem;
	}
	head->prev = elem;
	elem->next = head;
	return elem;

fail:
	elem = prev;
	while (elem != NULL) {
		prev = elem->prev;
		free(elem);
		elem = prev;
	}
	return NULL;
}

void free_errlist(struct mu_error **list)
{
	struct mu_error *elem = *list;

	if (elem == NULL)
		return;

	do {
		struct mu_error *next = elem->next;
		free(elem);
		elem = next;
	} while (elem != *list);

	*list = NULL;
}

#define MU_CHECK_ADD_ERROR(err) { \
	if (err->ret != 0) { \
		struct mu_error *elem; \
		if (err->ret == ENOMEM) { \
			fputs("Encountered ENOMEM while trying to process a different error, aborting.", stderr); \
			return; \
		} \
		elem = mallocz(sizeof(struct mu_error)); \
		if (elem == NULL) { \
			fputs("Encountered ENOMEM while trying to process a different error, aborting.", stderr); \
			return; \
		} \
		elem->prev = err->prev; \
		elem->next = err; \
		err->prev = elem; \
		err = elem; \
	} \
}

void push_error(struct mu_error **list, const char *file, const char *func, uint_least8_t line, const char *errmsg, int ret)
{
	struct mu_error *err = (*list)->prev;

	MU_CHECK_ADD_ERROR(err);

	err->file = file;
	err->func = func;
	err->line = line;

	strncpy(err->errmsg, errmsg, 255);
	err->errmsg[255] = '\0';

	err->ret = ret;

	*list = err;
}

void push_errf(struct mu_error **list, const char *file, const char *func, uint_least8_t line, int ret, const char *fmt, ...)
{
	struct mu_error *err = (*list)->prev;
	va_list ap;

	MU_CHECK_ADD_ERROR(err);

	err->file = file;
	err->func = func;
	err->line = line;

	va_start(ap, fmt);
	vsnprintf(err->errmsg, 256, fmt, ap);
	va_end(ap);

	err->ret = ret;

	*list = err;
}

int process_errors(struct mu_error *list)
{
	struct mu_error *elem = list;
	int ret = 0;

	if (elem->ret == 0) {
		return ret;
	}

	fputs("Encountered fatal error during processing: \n", stderr);
	do {
		ret = -1;
		fprintf(stderr, "\t%s:%"  PRIuLEAST8 " in %s, %s\n", elem->file, elem->line, elem->func, elem->errmsg);
		elem = elem->prev;
	} while (elem->ret != 0 && elem != list);

	return ret;
}
