#include <stdarg.h>
#include <stdio.h>

char *safe_sprintf(const char *format, ...)
{
	va_list ap, aq;
	char *c;
	int siz;

	va_start(ap, format);
	va_copy(aq, ap);
	siz = vsnprintf(NULL, 0, format, aq);
	va_end(aq);

	c = mallocz(siz + 1);
	assert(c != NULL);

	vsnprintf(c, siz + 1, format, ap);
	va_end(ap);

	return c;
}
