#include <stdlib.h>
#include <string.h>
#include "common.h"

/**
 * Returns a clone of the passed string. It must be free by caller
 *
 * @param const char* str Original string
 *
 * @return char*
 */
char *my_strdup(const char *str)
{
	char *new_str = NULL;
	if (str != NULL) {
		size_t size = strlen(str) + 1;
		new_str = malloc(size);
		if (new_str != NULL) {
			memcpy(new_str, str, size);
		}
	}
	return new_str;
}

/**
 * Copies the string pointed to by src, into a string at the buffer pointed to by dst
 * and returns a pointer to the terminating null byte in dst
 *
 * @param char       *dst
 * @param const char *src
 *
 * @return char*
 */
char *my_stpcpy(char *dst, const char *src)
{
	while ((*dst = *src)) {
		++dst;
		++src;
	}
	return dst;
}

/**
 * Frees the memory allocated for the buffer and resets the pointer
 *
 * @param struct buffer* buffer Buffer to free
 *
 * @return void
 */
void free_buffer(struct buffer *buffer)
{
	free(buffer->pointer);
	buffer->pointer = NULL;
	buffer->size = 0;
}
