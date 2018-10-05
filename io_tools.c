#include <unistd.h>

ssize_t read_full (int socket_fd, void *buffer, size_t size)
{
  size_t size_read = 0;
  while (size_read < size)
    {
      ssize_t this_size;
      this_size = read (socket_fd, buffer + size_read, size - size_read);
      if (this_size == -1)
        return -1;
      else if (this_size == 0)
        return 0;
      size_read += this_size;
    }
  return size_read;
}


ssize_t write_full (int socket_fd, void *buffer, size_t size)
{
  size_t size_write = 0;
  while (size_write < size)
    {
      ssize_t this_size;
      this_size = write (socket_fd, buffer + size_write, size - size_write);
      if (this_size == -1)
        return -1;
      size_write += this_size;
    }
  return size_write;
}
