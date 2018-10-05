#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "xtype.h"
#include "error.h"
#include "protocol.h"
#include "io_tools.h"


int read_socket (int socket_fd, void *buffer)
{
  struct xtype_header *header;
  uint32_t magic_code, size;
  ssize_t read_size_header;
  ssize_t read_size_content;
  read_size_header = read_full (socket_fd, buffer, sizeof (struct xtype_header));
  if (read_size_header == 0)
    return 0;
  else if (read_size_header == -1)
    return -1;
  header = (struct xtype_header *) buffer;
  magic_code = ntohl (header->magic_code);
  size = ntohl (header->size);
  header->size = ntohl (header->size);
  header->magic_code = ntohl (header->magic_code);
  if (magic_code != XTYPE_MAGIC_CODE)
    {
      print_error ("Magic code not match (%d and %d).", magic_code, XTYPE_MAGIC_CODE);
      return -1;
    }
  if (size > XTYPE_MSG_MAXSIZE)
    {
      print_error ("Packet too large.");
      return -1;
    }
  
  read_size_content = read_full (socket_fd, buffer + sizeof (struct xtype_header), size - sizeof (struct xtype_header));
  if (read_size_content == 0)
    return 0;
  else if (read_size_content == -1)
    return -1;

  return read_size_header + read_size_content;
}

static int write_socket (int socket_fd, void *buffer)
{
  uint32_t size;
  struct xtype_header *header;
  header = (struct xtype_header *) buffer;
  size = ntohl (header->size);
  assert (size <= XTYPE_MSG_MAXSIZE);
  write_full (socket_fd, buffer, size);
  free (buffer);
  return 0;
}

static void set_header (void *buffer, uint32_t size, uint8_t type)
{
  struct xtype_header *header;
  header = (struct xtype_header *) buffer;
  header->magic_code = htonl (XTYPE_MAGIC_CODE);
  header->size = htonl (size);
  header->type = type;
}

int get_ptype (void *message)
{
  struct xtype_header *header;
  header = (struct xtype_header *) message;
  return header->type;
}

int get_file (void *message, void *buffer, uint32_t *pos_p, uint32_t *size_p)
{
  struct xtype_header *header;
  struct xtype_file_header *file_header;
  void *file_buffer;
  uint32_t size, position;
  header = (struct xtype_header *) message;
  file_header = (struct xtype_file_header *) (message + sizeof (struct xtype_header));
  file_buffer = message + (sizeof (struct xtype_header)) + (sizeof (struct xtype_file_header));
  size = header->size - (sizeof (struct xtype_header)) - (sizeof (struct xtype_file_header));
  position = ntohl (file_header->offset);
  memcpy (buffer, file_buffer, size);
  *pos_p = position;
  *size_p = size;
  return 0;
}

int send_file_buffer (int socket_fd, const void *file_buffer, uint32_t offset, uint32_t file_size)
{
  uint32_t size;
  void *buffer;
  void *file_buffer2;
  struct xtype_file_header *file_header;
  size = (sizeof (struct xtype_header)) + (sizeof (struct xtype_file_header)) + file_size;
  assert (size <= XTYPE_MSG_MAXSIZE);
  buffer = malloc (size);
  file_buffer2 = buffer + (sizeof (struct xtype_header)) + (sizeof (struct xtype_file_header));
  if (buffer == NULL)
    return -1;
  set_header (buffer, size, XTYPE_PFILE);
  file_header = (struct xtype_file_header *) (buffer + (sizeof (struct xtype_header)));
  file_header->offset = htonl (offset);
  memcpy (file_buffer2, file_buffer, file_size);
  return write_socket (socket_fd, buffer);
}

int send_file (int socket_fd, int file_fd, uint32_t offset, uint32_t file_size)
{
  uint32_t size;
  void *buffer;
  void *file_buffer;
  struct xtype_file_header *file_header;
  size = (sizeof (struct xtype_header)) + (sizeof (struct xtype_file_header)) + file_size;
  assert (size <= XTYPE_MSG_MAXSIZE);
  buffer = malloc (size);
  file_buffer = buffer + (sizeof (struct xtype_header)) + (sizeof (struct xtype_file_header));
  if (buffer == NULL)
    return -1;
  set_header (buffer, size, XTYPE_PFILE);
  file_header = (struct xtype_file_header *) (buffer + (sizeof (struct xtype_header)));
  file_header->offset = htonl (offset);
  if (lseek (file_fd, offset, SEEK_SET) == -1)
    return -1;
  if (read_full (file_fd, file_buffer, file_size) == -1)
    return -1;
  return write_socket (socket_fd, buffer);
}

int get_typec (void *message, char *c_p)
{
  struct xtype_header *header;
  struct xtype_type_header *type_header;
  header = (struct xtype_header *) message;
  type_header = (struct xtype_type_header *) (message + sizeof (struct xtype_header));
  
  *c_p = type_header->c;
  if (header->size != (sizeof (struct xtype_type_header)) + (sizeof (struct xtype_header)))
    return -1;

  return 0;
}

int send_typec (int socket_fd, char c)
{
  uint32_t size;
  void *buffer;
  struct xtype_type_header *typec_header;
  size = (sizeof (struct xtype_header)) + (sizeof (struct xtype_type_header));
  buffer = malloc (size);
  if (buffer == NULL)
    return -1;
  set_header (buffer, size, XTYPE_PTYPE);
  typec_header = (struct xtype_type_header *) (buffer + (sizeof (struct xtype_header)));
  typec_header->c = c;

  return write_socket (socket_fd, buffer);
}

int get_pos (void *message, uint32_t *position_p)
{
  struct xtype_header *header;
  struct xtype_pos_header *pos_header;
  header = (struct xtype_header *) message;
  pos_header = (struct xtype_pos_header *) (message + sizeof (struct xtype_header));
  
  *position_p = ntohl (pos_header->position);
  if (header->size != (sizeof (struct xtype_pos_header)) + (sizeof (struct xtype_header)))
    return -1;

  return 0;
}

int send_pos (int socket_fd, uint32_t position)
{
  uint32_t size;
  void *buffer;
  struct xtype_pos_header *pos_header;
  size = (sizeof (struct xtype_header)) + (sizeof (struct xtype_pos_header));
  buffer = malloc (size);
  if (buffer == NULL)
    return -1;
  set_header (buffer, size, XTYPE_PPOS);
  pos_header = (struct xtype_pos_header *) (buffer + (sizeof (struct xtype_header)));
  pos_header->position = htonl (position);

  return write_socket (socket_fd, buffer);
}

int get_info (void *message, struct xtype_info_header **infos_p, int *infos_count_p)
{
  struct xtype_header *header;
  struct xtype_info_header *infos;
  int infos_count;
  
  header = (struct xtype_header *) message;
  infos = (struct xtype_info_header *) (message + sizeof (struct xtype_header));
  if (header->size < sizeof (struct xtype_header))
    return -1;
  infos_count = (header->size - sizeof (struct xtype_header)) / sizeof (struct xtype_info_header);
  if ((header->size - sizeof (struct xtype_header)) % (sizeof (struct xtype_info_header)) != 0)
    return -1;

  {
    int i;
    for (i = 0; i < infos_count; i++)
      {
        infos[i].position = ntohl (infos[i].position);
      }
  }

  *infos_p = infos;
  *infos_count_p = infos_count;
  return 0;
}

int send_info (int socket_fd, const struct xtype_info_header infos[], int infos_count)
{
  uint32_t size;
  void *buffer;
  struct xtype_info_header *infos_buffer;
  size = (sizeof (struct xtype_header)) + infos_count * (sizeof (struct xtype_info_header));
  if (size > XTYPE_MSG_MAXSIZE)
    return -1;
  buffer = malloc (size);
  if (buffer == NULL)
    return -1;
  set_header (buffer, size, XTYPE_PINFO);
  infos_buffer = (struct xtype_info_header *) (buffer + (sizeof (struct xtype_header)));
  memcpy (infos_buffer, (void *) infos, infos_count * (sizeof (struct xtype_info_header)));
  {
    int i;
    for (i = 0; i < infos_count; i++)
      {
        infos_buffer[i].position = htonl (infos_buffer[i].position);
      }
  }

  return write_socket (socket_fd, buffer);
}

int get_init (void *message, uint32_t *file_size_p)
{
  struct xtype_header *header;
  struct xtype_init_header *init_header;
  header = (struct xtype_header *) message;
  init_header = (struct xtype_init_header *) (message + (sizeof (struct xtype_header)));
  if (header->size != (sizeof (struct xtype_header)) + (sizeof (struct xtype_init_header)))
    return -1;

  *file_size_p = ntohl (init_header->file_size);
  return 0;
}

int send_init (int socket_fd, uint32_t file_size)
{
  uint32_t size;
  void *buffer;
  struct xtype_init_header *init_header;
  size = (sizeof (struct xtype_header)) + (sizeof (struct xtype_init_header));
  buffer = malloc (size);
  if (buffer == NULL)
    return -1;
  set_header (buffer, size, XTYPE_PINIT);
  init_header = (struct xtype_init_header *) (buffer + (sizeof (struct xtype_header)));
  init_header->file_size = htonl (file_size);

  return write_socket (socket_fd, buffer);
}

int get_filer (void *message, uint32_t *window_width_p)
{
  struct xtype_header *header;
  struct xtype_filer_header *filer_header;
  header = (struct xtype_header *) message;
  filer_header = (struct xtype_filer_header *) (message + (sizeof (struct xtype_header)));
  if (header->size != (sizeof (struct xtype_header)) + (sizeof (struct xtype_filer_header)))
    return -1;

  *window_width_p = ntohl (filer_header->window_width);
  return 0;
}

int send_filer (int socket_fd, uint32_t window_width)
{
  uint32_t size;
  void *buffer;
  struct xtype_filer_header *filer_header;
  size = (sizeof (struct xtype_header)) + (sizeof (struct xtype_filer_header));
  buffer = malloc (size);
  if (buffer == NULL)
    return -1;
  set_header (buffer, size, XTYPE_PFILER);
  filer_header = (struct xtype_filer_header *) (buffer + (sizeof (struct xtype_header)));
  filer_header->window_width = htonl (window_width);

  return write_socket (socket_fd, buffer);
}

int get_status (void *message, uint32_t *status_p)
{
  struct xtype_header *header;
  struct xtype_status_header *status_header;
  header = (struct xtype_header *) message;
  status_header = (struct xtype_status_header *) (message + (sizeof (struct xtype_header)));
  if (header->size != (sizeof (struct xtype_header)) + (sizeof (struct xtype_status_header)))
    return -1;

  *status_p = ntohl (status_header->status);
  return 0;
}

int send_status (int socket_fd, uint32_t status)
{
  uint32_t size;
  void *buffer;
  struct xtype_status_header *status_header;
  size = (sizeof (struct xtype_header)) + (sizeof (struct xtype_status_header));
  buffer = malloc (size);
  if (buffer == NULL)
    return -1;
  set_header (buffer, size, XTYPE_PSTATUS);
  status_header = (struct xtype_status_header *) (buffer + (sizeof (struct xtype_header)));
  status_header->status = htonl (status);

  return write_socket (socket_fd, buffer);
}

int get_ready (void *message, uint32_t *is_ready_p)
{
  struct xtype_header *header;
  struct xtype_ready_header *ready_header;
  header = (struct xtype_header *) message;
  ready_header = (struct xtype_ready_header *) (message + (sizeof (struct xtype_header)));
  if (header->size != (sizeof (struct xtype_header)) + (sizeof (struct xtype_ready_header)))
    return -1;

  *is_ready_p = ready_header->is_ready;
  return 0;
}

int send_ready (int socket_fd, uint8_t is_ready)
{
  uint32_t size;
  void *buffer;
  struct xtype_ready_header *ready_header;
  size = (sizeof (struct xtype_header)) + (sizeof (struct xtype_ready_header));
  buffer = malloc (size);
  if (buffer == NULL)
    return -1;
  set_header (buffer, size, XTYPE_PREADY);
  ready_header = (struct xtype_ready_header *) (buffer + (sizeof (struct xtype_header)));
  ready_header->is_ready = is_ready;

  return write_socket (socket_fd, buffer);
}

int get_con (void *message, char **id_p)
{
  struct xtype_header *header;
  struct xtype_con_header *con_header;
  header = (struct xtype_header *) message;
  con_header = (struct xtype_con_header *) (message + (sizeof (struct xtype_header)));
  if (header->size != (sizeof (struct xtype_header)) + (sizeof (struct xtype_con_header)))
    return -1;

  *id_p = con_header->id;
  return 0;
}

int send_con (int socket_fd, const char *id)
{
  uint32_t size;
  void *buffer;
  struct xtype_con_header *con_header;
  size = (sizeof (struct xtype_header)) + (sizeof (struct xtype_con_header));
  buffer = malloc (size);
  if (buffer == NULL)
    return -1;
  set_header (buffer, size, XTYPE_PCON);
  con_header = (struct xtype_con_header *) (buffer + (sizeof (struct xtype_header)));
  strcpy (con_header->id, id);

  return write_socket (socket_fd, buffer);
}
