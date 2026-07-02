#ifndef COMMON_H
#define COMMON_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#define WEAK_VALUES_META_NAME "thoth.weak.values"

struct buffer {
	size_t size;
	void   *pointer;
};

char *my_strdup(const char *str);
char *my_stpcpy(char *dst, const char *src);
void free_buffer(struct buffer *buffer);

#ifdef __cplusplus
}
#endif
#endif // COMMON_H
