#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "xtype.h"
#include "interface.h"
#include "protocol.h"
#include "error.h"
#include "stopwatch.h"

static int socket_fd;
static volatile sig_atomic_t should_exit = 0;

static void int_hand()
{
  should_exit = 1;
}

void game_init ()
{
  /* init socket */
  socket_fd = socket (args.socket_domain, args.socket_type, args.socket_protocol);
  if (socket_fd == -1)
    error_exit ("Cannot open socket.");
  if (connect (socket_fd, (struct sockaddr *) &args.socket_address, sizeof (args.socket_address)) == -1)
    error_exit ("Cannot connect to server.");

  if (send_con (socket_fd, args.id) == -1)
    error_exit ("Cannot send connect request to server.");

  signal(SIGINT, int_hand);
}

void game_reset ()
{
  ifinfo.position = 0;
  ifinfo.me_ready = 0;
  ifinfo.duration = 0;
}

static int update_position (uint32_t position)
{
  if (position < ifinfo.offset_buffer || position >= ifinfo.offset_buffer + ifinfo.text_size)
    {
      if (send_filer (socket_fd, get_window_width ()) == -1)
        return -1;
    }
  ifinfo.position = position;
  return 0;
}

void game_run ()
{
  fd_set this_set;
  FD_ZERO (&this_set);
  FD_SET (STDIN_FILENO, &this_set);
  FD_SET (socket_fd, &this_set);
  while (!should_exit)
    {
      fd_set test_set = this_set;
      while (select (socket_fd + 1, &test_set, NULL, NULL, NULL) == -1)
        {
          if (errno != EINTR)
            error_exit ("Cannot select.");
        }
      if (FD_ISSET (STDIN_FILENO, &test_set))
        {
          /* input from terminal */
          int c = getchar ();
          if (c == -1)
            error_exit ("Cannot get input from terminal.");

          switch (ifinfo.game_state)
            {
            case XTYPE_GAME_WAITING:
              {
                switch (c)
                  {
                  case 'r':
                    /* get ready */
                    ifinfo.me_ready = 1;
                    if (send_ready (socket_fd, 1) == -1)
                      error_exit ("Cannot send ready information.");
                    draw ();
                    break;

                  case 'c':
                    /* not ready */
                    ifinfo.me_ready = 0;
                    if (send_ready (socket_fd, 0) == -1)
                      error_exit ("Cannot send ready information.");
                    draw ();
                    break;
                    
                  case 'q':
                    /* quit game */
                    goto quit;
                    break;
                    
                  default:
                    /* do nothing */
                    break;
                  }
              }
              break;

            case XTYPE_GAME_RUNNING:
              {
                if (send_typec (socket_fd, c) == -1)
                  error_exit ("Cannot send character to server.");
              }
              break;

            case XTYPE_GAME_END:
            case XTYPE_GAME_READY:
              {
                /* do nothing */
              }
              break;

            default:
              assert (0);
            }
        }
      else if (FD_ISSET (socket_fd, &test_set))
        {
          /* message from server */
          void *message = malloc (XTYPE_MSG_MAXSIZE);
          int ptype;
          ssize_t read_size;
          read_size = read_socket (socket_fd, message);
          if (read_size == -1)
            goto end_receive;
          else if (read_size == 0)
            {
              error_exit ("Connection closed.");
            }

          ptype = get_ptype (message);
          switch (ptype)
            {
            case XTYPE_PPOS:
              {
                uint32_t position;
                get_pos (message, &position);
                if (update_position (position) == -1)
                  goto end_receive;
                draw ();
              }
              break;

            case XTYPE_PFILE:
              {
                if (get_file (message, ifinfo.text_buffer, &ifinfo.offset_buffer, &ifinfo.text_size) == -1)
                  goto end_receive;
                draw ();
              }
              break;

            case XTYPE_PINFO:
              {
                struct xtype_info_header *infos;
                int infos_count;
                int i;
                if (get_info (message, &infos, &infos_count) == -1)
                  goto end_receive;
                for (i = 0; i < infos_count; i++)
                  {
                    strcpy (ifinfo.infos[i].id, infos[i].id);
                    ifinfo.infos[i].position = infos[i].position;
                  }
                ifinfo.infos_count = infos_count;
                draw ();
              }
              break;

            case XTYPE_PINIT:
              {
                uint32_t this_file_size;
                get_init (message, &this_file_size);
                ifinfo.file_size = this_file_size;
                /* ??? */
                draw ();
              }
              break;

            case XTYPE_PSTATUS:
              {
                uint32_t status;
                get_status (message, &status);
                switch (status)
                  {
                  case XTYPE_SWAITING:
                    ifinfo.game_state = XTYPE_GAME_WAITING;
                    break;

                  case XTYPE_SRUNNING:
                    if (send_filer (socket_fd, get_window_width ()) == -1)
                      goto end_receive;
                    ifinfo.game_state = XTYPE_GAME_RUNNING;
                    stopwatch_start ();
                    break;

                  case XTYPE_SEND:
                    ifinfo.game_state = XTYPE_GAME_END;
                    stopwatch_stop ();
                    game_reset ();
                    break;

                  case XTYPE_SREADY:
                    ifinfo.game_state = XTYPE_GAME_READY;
                    break;

                  default:
                    goto end_receive;
                    break;
                  }

                draw ();
              }
              break;
              
            default:
              goto end_receive;
              break;
            }

        end_receive:
          free (message);
        }
      else
        {
          assert (0);
        }
    }

 quit:
  return ;
}

void game_end ()
{
  close (socket_fd);
}
