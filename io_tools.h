#ifndef __IO_TOOLS_H
#define __IO_TOOLS_H

#include <unistd.h>

int read_full (int socket_fd, void *buffer, size_t size);


int write_full (int socket_fd, void *buffer, size_t size);

#endif
