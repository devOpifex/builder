#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stddef.h>

typedef struct Buffer_t {
	char *data;
	size_t size;
	size_t capacity;
} Buffer;

Buffer *buffer_init(void);
void buffer_free(Buffer *buffer);
void buffer_append(Buffer *buffer, char *data);
void buffer_append_nl(Buffer *buffer, char *data);
void buffer_reset(Buffer *buffer);

#endif
