#include <stdlib.h>
#include <string.h>

#include "buffer.h"

Buffer *buffer_init(void)
{
  Buffer *buffer = malloc(sizeof(Buffer));

  buffer->data = NULL;
  buffer->size = 0;
  buffer->capacity = 0;

  return buffer;
}

void buffer_free(Buffer *buffer)
{
  free(buffer->data);
  free(buffer);
}

void buffer_append(Buffer *buffer, char *data)
{
  size_t len = strlen(data);

  while(buffer->size + len >= buffer->capacity) {
    buffer->capacity = (buffer->capacity == 0) ? 4096 : buffer->capacity * 2;
  }
  buffer->data = realloc(buffer->data, buffer->capacity);

  memcpy(buffer->data + buffer->size, data, len);
  buffer->size += len;
  buffer->data[buffer->size] = '\0';
}

void buffer_append_nl(Buffer *buffer, char *data)
{
  buffer_append(buffer, data);
  buffer_append(buffer, "\n");
}

void buffer_reset(Buffer *buffer) 
{
  buffer->size = 0;
  if(buffer->data) buffer->data[0] = '\0';
}
