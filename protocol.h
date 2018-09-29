#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>
#include "xtype.h"

#define XTYPE_MAGIC_CODE 18273645u

#define XTYPE_PRSRV 0
#define XTYPE_PFILE 1
#define XTYPE_PTYPE 2
#define XTYPE_PINFO 3
#define XTYPE_PCTRL 4
#define XTYPE_PPOS 5
#define XTYPE_PINIT 6
#define XTYPE_PFILER 7
#define XTYPE_PSTATUS 8
#define XTYPE_PREADY 9
#define XTYPE_PCON 10

#define XTYPE_MSG_MAXSIZE (1u << 12)


struct xtype_header
{
  uint32_t magic_code;
  uint32_t size;
  uint8_t type;
  uint8_t padding[3];
};

struct xtype_file_header
{
  uint32_t offset;
};

struct xtype_type_header
{
  char c;
  uint8_t padding[3];
};

struct xtype_pos_header
{
  uint32_t position;
};


struct xtype_info_header
{
  char id[XTYPE_ID_LENGTH];
  uint32_t position;
};


struct xtype_init_header
{
  uint32_t file_size;
};

struct xtype_filer_header
{
  uint32_t window_width;
};

struct xtype_con_header
{
  char id[XTYPE_ID_LENGTH];
};

#define XTYPE_SWAITING 0
#define XTYPE_SRUNNING 1
#define XTYPE_SEND 2
#define XTYPE_SREADY 3
struct xtype_status_header
{
  uint32_t status;
};

struct xtype_ready_header
{
  uint8_t is_ready;
  uint8_t padding[3];
};


int read_socket (int socket_fd, void *buffer);
int get_ptype (void *message);
int get_file (void *message, void *buffer, uint32_t *offset_p, uint32_t *size_p);
int send_file (int socket_fd, int file_fd, uint32_t pos, uint32_t size);
int send_file_buffer (int socket_fd, const void *buffer, uint32_t pos, uint32_t size);
int get_typec (void *message, char *c_p);
int send_typec (int socket_fd, char c);
int get_pos (void *message, uint32_t *position_p);
int send_pos (int socket_fd, uint32_t position);
int get_info (void *message, struct xtype_info_header **infos_p, int *infos_count_p);
int send_info (int socket_fd, const struct xtype_info_header infos[], int infos_count);
int get_init (void *message, uint32_t *file_size_p);
int send_init (int socket_fd, uint32_t file_size);
int get_filer (void *message, uint32_t *window_width_p);
int send_filer (int socket_fd, uint32_t window_width);
int get_status (void *message, uint32_t *status_p);
int send_status (int socket_fd, uint32_t status);
int get_con (void *message, char **id_p);
int send_con (int socket_fd, const char *id);
int get_ready (void *message, uint32_t *is_ready_p);
int send_ready (int socket_fd, uint8_t is_ready);

#endif
