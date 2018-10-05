#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/select.h>
#include <ctype.h>
#include <errno.h>

#include "xtype.h"
#include "protocol.h"
#include "error.h"
#include "io_tools.h"

struct player_info
{
  char id[XTYPE_ID_LENGTH];
  uint32_t position;
  int is_ready;
  int previous, next;
};

int server_fd;
uint32_t file_size;
int client_fd[XTYPE_MAX_PLAYERS];
struct player_info infos[XTYPE_MAX_PLAYERS];
int first_player;
int max_fd;
char *file_content;
int infos_dirty;
uint32_t server_status;


static struct args_t
{
  int socket_domain;
  int socket_type;
  int socket_protocol;
  int socket_port;
  const char *file_name;
} args;

static void touch_infos ()
{
  infos_dirty = 1;
}

static void read_args (int argc, char *argv[])
{
  if (argc != 3)
    error_exit ("Invalid arguments.");
  args.socket_domain = AF_INET;
  args.socket_type = SOCK_STREAM;
  args.socket_protocol = IPPROTO_TCP;
  {
    int count;
    count = sscanf (argv[1], "%d", &args.socket_port);
    if (count != 1)
      error_exit ("Invalid port number.");
  }
  args.file_name = argv[2];
}

static int player_new (const char *id, uint32_t position, int fd)
{
  int which;
  for (which = 0; which < XTYPE_MAX_PLAYERS; which++)
    {
      if (client_fd[which] == -1)
        break;
    }
  if (which == XTYPE_MAX_PLAYERS)
    return -1;

  client_fd[which] = fd;
  assert (strlen (id) < XTYPE_ID_LENGTH);
  strcpy (infos[which].id, id);
  infos[which].position = position;
  infos[which].is_ready = 0;
  {
    int before = -1, after = first_player;
    while (after != -1)
      {
        if (infos[after].position < position)
          break;
        before = after;
        after = infos[after].next;
      }
    infos[which].previous = before;
    infos[which].next = after;
    if (before != -1)
      infos[before].next = which;
    else
      first_player = which;
    if (after != -1)
      infos[after].previous = which;
  }

  return 0;
}

static void player_delete (int which)
{
  assert (which != -1);
  assert (client_fd[which] != -1);
  close (client_fd[which]);
  client_fd[which] = -1;
  if (first_player == which)
    {
      int next = infos[which].next;
      assert (infos[which].previous == -1);
      first_player = next;
      if (next != -1)
        {
          infos[next].previous = -1;
        }
    }
  else
    {
      int before = infos[which].previous;
      int after = infos[which].next;
      assert (before != -1);
      infos[before].next = after;
      if (after != -1)
        infos[after].previous = before;
    }
}

static void player_update (int which, uint32_t position)
{
  assert (which != -1);
  assert (client_fd[which] != -1);
  assert (position <= file_size);

  if (position > infos[which].position)
    {
      while (infos[which].previous != -1
             && position > infos[infos[which].previous].position)
        {
          int previous = infos[which].previous;
          infos[previous].next = infos[which].next;
          infos[which].previous = infos[previous].previous;
          infos[which].next = previous;
          infos[previous].previous = which;
          if (infos[which].previous != -1)
            {
              infos[infos[which].previous].next = which;
            }
          else
            {
              assert (first_player == previous);
              first_player = which;
            }
        }
    }
  else if (position < infos[which].position)
    {
      while (infos[which].next != -1
             && position < infos[infos[which].next].position)
        {
          int next = infos[which].next;
          infos[next].previous = infos[which].previous;
          infos[which].next = infos[next].next;
          infos[which].previous = next;
          infos[next].next = which;
          if (infos[which].next != -1)
            {
              infos[infos[which].next].previous = which;
            }
          if (infos[next].previous == -1)
            {
              assert (first_player == which);
              first_player = next;
            }
        }
    }
  infos[which].position = position;
}

static int fd2player (int fd)
{
  int i;
  for (i = 0; i < XTYPE_MAX_PLAYERS; i++)
    {
      if (client_fd[i] == fd)
        return i;
    }
  return -1;
}

void post_process (char *content, uint32_t size)
{
  uint32_t i;
  for (i = 0; i < size; i++)
    {
      if (isspace (content[i]))
        content[i] = ' ';
    }
}

static void server_init ()
{
  server_fd = socket (args.socket_domain, args.socket_type, args.socket_protocol);
  if (server_fd == -1)
    error_exit ("Cannot open socket.");
  {
    struct sockaddr_in server_address;
    bzero (&server_address, sizeof (server_address));
    server_address.sin_family = args.socket_domain;
    server_address.sin_addr.s_addr = htonl (INADDR_ANY);
    server_address.sin_port = htons (args.socket_port);
    if (bind (server_fd, (struct sockaddr *) &server_address, sizeof (server_address)) == -1)
      error_exit ("Cannot bind to port.");

    if (listen (server_fd, 2 * XTYPE_MAX_PLAYERS) == -1)
      error_exit ("Cannot listen.");
  }

  {
    int text_fd;
    struct stat file_stat;
    text_fd = open (args.file_name, O_RDONLY);
    if (text_fd == -1)
      error_exit ("Cannot open text file.");
    if (fstat (text_fd, &file_stat) == -1)
      error_exit ("Cannot get file stat.");
    file_size = file_stat.st_size;
    file_content = (char *) malloc (file_size);
    if (file_content == NULL)
      error_exit ("Cannot allocate memory for reading.");
    if (read_full (text_fd, file_content, file_size) == -1)
      error_exit ("Cannot read file.");
    post_process (file_content, file_size);
    close (text_fd);
  }

  {
    int i;
    for (i = 0; i < XTYPE_MAX_PLAYERS; i++)
      client_fd[i] = -1;
  }
  first_player = -1;
  server_status = XTYPE_SWAITING;
  infos_dirty = 0;
  max_fd = 2;
  if (server_fd > max_fd)
    max_fd = server_fd;
}

static void server_end ()
{
  int i;
  for (i = 0; i < XTYPE_MAX_PLAYERS; i++)
    {
      if (client_fd[i] != -1)
        close (client_fd[i]);
    }
  close (server_fd);
  printf ("Bye.\n");
  exit (0);
}

static int server_type (int which, void *message)
{
  char c;
  assert (client_fd[which] != -1);
  get_typec (message, &c);
  if (c == file_content[infos[which].position])
    {
      if (send_pos (client_fd[which], infos[which].position + 1) == -1)
        return -1;
      player_update (which, infos[which].position + 1);
      touch_infos ();
    }
  return 0;
}

static int server_filer (int which, void *message)
{
  uint32_t window_width;
  int offset, size;
  get_filer (message, &window_width);
  offset = (int)infos[which].position - (int)window_width;
  if (offset < 0)
    offset = 0;
  size = XTYPE_MSG_MAXSIZE - (sizeof (struct xtype_header)) - (sizeof (struct xtype_file_header));
  if (offset + size > file_size)
    {
      size = file_size - offset;
    }
    
  return send_file_buffer (client_fd[which], file_content + offset, offset, size);
}


static int check_all_ready ()
{
  int which;
  if (first_player == -1)
    return 0;

  for (which = first_player; which != -1; which = infos[which].next)
    {
      if (!infos[which].is_ready)
        return 0;
    }

  /* All players are ready. */
  server_status = XTYPE_SRUNNING;
  for (which = first_player; which != -1; which = infos[which].next)
    {
      if (send_status (client_fd[which], XTYPE_SRUNNING) == -1)
        return -1;
      infos[which].is_ready = 0;
    }

  return 0;
}

static int server_ready (int which, void *message)
{
  uint32_t is_ready;
  if (server_status != XTYPE_SWAITING)
    return -1;
  get_ready (message, &is_ready);
  infos[which].is_ready = is_ready;
  if (is_ready)
    return check_all_ready ();
  return 0;
}

static int server_con (int which, void *message)
{
  char *id;
  get_con (message, &id);
  assert (strlen (id) < XTYPE_ID_LENGTH);
  strcpy (infos[which].id, id);
  touch_infos ();
  return 0;
}

static int distribute_infos ()
{
  struct xtype_info_header infos_tosend[XTYPE_MAX_PLAYERS];
  int which, i;
  for (which = first_player, i = 0; which != -1; which = infos[which].next, i++)
    {
      assert (i < XTYPE_MAX_PLAYERS);
      strcpy (infos_tosend[i].id, infos[which].id);
      infos_tosend[i].position = infos[which].position;
    }
  for (which = first_player; which != -1; which = infos[which].next)
    {
      send_info (client_fd[which], infos_tosend, i);
    }

  infos_dirty = 0;
  return 0;
}

static void server_run ()
{
  fd_set fds;
  int dirty_check_count = 0;
  FD_ZERO (&fds);
  FD_SET (server_fd, &fds);

  while (1)
    {
      fd_set this_set = fds;
      int select_count;
      struct timeval seconds_1;

      if (dirty_check_count > 10)
        {
          dirty_check_count = 0;
          if (distribute_infos () == -1)
            goto end_receive;
        }
      dirty_check_count++;
      seconds_1.tv_sec = 1;
      seconds_1.tv_usec = 0;
      while ((select_count = select (max_fd + 1, &this_set, NULL, NULL, &seconds_1)) == -1)
        {
          if (errno != EINTR)
            error_exit ("Cannot select.");
        }

      if (select_count == 0)
        {
          /* time out */
          if (infos_dirty)
            {
              if (distribute_infos () == -1)
                goto end_receive;
            }
        }
      else if (FD_ISSET (server_fd, &this_set))
        {
          int fd = accept (server_fd, NULL, NULL);
          if (player_new ("NULL", 0, fd) == -1)
            {
              print_error ("Too many players.");
              goto no_new_player;
            }
          if (fd > max_fd)
            max_fd = fd;
          FD_SET (fd, &fds);
          send_init (fd, file_size);
          touch_infos ();

        no_new_player:
          /* do nothing */;
        }
      else
        {
          int which;
          void *message;
          ssize_t read_size;
          for (which = 0; which < XTYPE_MAX_PLAYERS; which++)
            {
              if (client_fd[which] != -1 && FD_ISSET (client_fd[which], &this_set))
                break;
            }
          assert (which < XTYPE_MAX_PLAYERS);

          message = malloc (XTYPE_MSG_MAXSIZE);
          read_size = read_socket (client_fd[which], message);
          if (read_size == -1)
            goto end_receive;
          else if (read_size == 0)
            {
              /* connection closed */
              FD_CLR (client_fd[which], &fds);
              player_delete (which);
              distribute_infos ();
              check_all_ready ();
              goto end_receive;
            }
          
          switch (get_ptype (message))
            {
            case XTYPE_PTYPE:
              if (server_type (which, message) == -1)
                goto end_receive;
              break;

            case XTYPE_PFILER:
              if (server_filer (which, message) == -1)
                goto end_receive;
              break;

            case XTYPE_PREADY:
              if (server_ready (which, message) == -1)
                goto end_receive;
              break;

            case XTYPE_PCON:
              if (server_con (which, message) == -1)
                goto end_receive;
              break;

            default:
              goto end_receive;
              break;
            }

        end_receive:
          free (message);
        }
    }

 end:
  return ;
}

int main (int argc, char *argv[])
{
  read_args (argc, argv);
  server_init ();

  server_run ();

  server_end ();
  assert (0);
}
