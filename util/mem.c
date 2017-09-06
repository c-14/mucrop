#include <limits.h>
#include <stdlib.h>
#include <string.h>

void *mallocz(size_t size)
{
	void *ptr = malloc(size);
	if (ptr)
		memset(ptr, 0, size);
	return ptr;
}

void *realloc_array(void *ptr, size_t nmemb, size_t size)
{
	if (nmemb >= INT_MAX / size)
		return NULL;
	return realloc(ptr, nmemb * size);
}
