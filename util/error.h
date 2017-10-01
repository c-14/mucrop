#ifndef mu_ERROR_H
#define mu_ERROR_H

#include <errno.h>
#include <stdint.h>

#if EDOM > 0
#define MUERROR(e) (-(e))
#define MUERRNO(e) (-(e))
#else
#define MUERROR(e) (e)
#define MUERRNO(e) (e)
#endif

#define MU_PUSH_ERRNO(x, y) push_error(x, __FILE__, __func__, __LINE__, strerror(y), y)
#define MU_PUSH_ERRSTR(x, y) push_error(x, __FILE__, __func__, __LINE__, y, -1)
#define MU_PUSH_ERRF(x, y, ...) push_errfr(x, __FILE__, __func__, __LINE__, -1, y, __VA_ARGS__)

#define MU_RET_ERRNO(x, y) { MU_PUSH_ERRNO(x, y); return MUERROR(y); }
#define MU_RET_ERRSTR(x, y) { MU_PUSH_ERRSTR(x, y); return -1; }

struct mu_error {
	struct mu_error *next;
	struct mu_error *prev;

	char errmsg[256];
	const char *file;
	const char *func;
	uint_least8_t line;

	int ret;
};

extern struct mu_error *create_errlist(size_t siz);
extern int process_errors(struct mu_error *list);
extern void free_errlist(struct mu_error **list);

extern void push_error(struct mu_error **list, const char *file, const char *func, uint_least8_t line, const char *errmsg, int ret);
extern void push_errf(struct mu_error **list, const char *file, const char *func, uint_least8_t line, int ret, const char *fmt, ...);

#endif
