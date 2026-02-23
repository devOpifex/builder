#ifndef __BUFFER_H__
#define __BUFFER_H__

typedef struct Buffer_t {
	char *data;
	size_t size;
	size_t capacity;
} Buffer;

Buffer *buffer_init();
void buffer_append(Buffer *buffer, char *data);
void buffer_reset(Buffer *buffer);

#endif
