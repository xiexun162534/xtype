#include <unistd.h>

int read_full (int socket_fd, void *buffer, size_t size)
{
  size_t size_read = 0;
  while (size_read < size)
    size_read += read (socket_fd, buffer + size_read, size - size_read);
  return size_read;
}


int write_full (int socket_fd, void *buffer, size_t size)
{
  size_t size_write = 0;
  while (size_write < size)
    size_write += write (socket_fd, buffer + size_write, size - size_write);
  return size_write;
}
