#ifndef __INTERFACE_H
#define __INTERFACE_H

#include <time.h>
#include "protocol.h"

#include <netinet/in.h>

struct args_t
{
  char id[XTYPE_ID_LENGTH];
  int socket_domain;
  int socket_type;
  int socket_protocol;
  struct sockaddr_storage socket_address;
};

extern struct args_t args;


struct player_info
{
  char id[XTYPE_ID_LENGTH];
  uint32_t position;
};


#define XTYPE_GAME_WAITING 0
#define XTYPE_GAME_RUNNING 1
#define XTYPE_GAME_END 2
#define XTYPE_GAME_READY 3

struct ifinfo_t
{
  uint8_t game_state;
  uint32_t file_size;
  char *text_buffer;
  uint32_t offset_buffer;
  uint32_t position;
  uint32_t text_size;
  struct player_info infos[XTYPE_MAX_PLAYERS];
  int infos_count;
  time_t duration;
  int me_ready;
} ifinfo;

extern struct ifinfo_t ifinfo;

int get_window_width ();
void draw ();

#endif
