#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <netdb.h>
#include <ncursesw/curses.h>
#include "xtype.h"
#include "error.h"
#include "interface.h"
#include "stopwatch.h"
#include "game.h"


struct ifinfo_t  ifinfo;
struct args_t args;

static struct termios term_in_backup;
static int window_height;
static int window_width;

int get_window_width ()
{
  return window_width;
}


static void print_player_info (struct player_info info)
{
  int head_length;
  int bar_length;
  int mark_length;
  int i;
  head_length = strlen(info.id) + 1;
  printw ("%s ", info.id);
  for (i = head_length; i < XTYPE_ID_LENGTH + 1; i++)
    {
      printw ("%c", ' ');
    }
  bar_length = window_width - XTYPE_ID_LENGTH - 1;
  if (info.position >= ifinfo.file_size)
    {
      assert (info.position == ifinfo.file_size);
      for (i = 0; i < bar_length; i++)
        printw ("%c", '*');
    }
  else
    {
      mark_length = bar_length * info.position / ifinfo.file_size;
      for (i = 0; i < mark_length; i++)
        printw ("%c", '#');
      for (i = mark_length; i < bar_length; i++)
        printw ("%c", '-');
    }
}

static void scroll_print ()
{
  int i;
  int head, tail;
  int offset = (int)ifinfo.position - (int)ifinfo.offset_buffer;
  if (offset >= ifinfo.text_size || offset < 0)
    {
      head = 0;
      tail = 0;
    }
  else
    {
      tail = offset + window_width / 2;
      if (tail > ifinfo.text_size)
        tail = ifinfo.text_size;
      head = tail - window_width;
      if (head < 0)
        {
          tail += -head;
          if (tail > ifinfo.text_size)
            tail = ifinfo.text_size;
          head = 0;
        }
    }

  assert ((offset >= head && offset < tail) || tail == head);

  if (tail > head)
    {
      char c = ifinfo.text_buffer[tail];
      ifinfo.text_buffer[tail] = 0;
      printw ("%s", &ifinfo.text_buffer[head]);
      ifinfo.text_buffer[tail] = c;
    }
  printw ("\n");
  for (i = head; i < tail; i++)
    {
      if (i != offset)
        printw ("%c", ' ');
      else
        printw ("%c", '|');
    }
}


static void draw_running ()
{
  /* print time */
  {
    time_t cached_duration = ifinfo.duration;
    int seconds = cached_duration % 60;
    int minutes = (cached_duration / 60) % 60;
    int hours = cached_duration / 60 / 60;
    printw ("%d:%02d:%02d\n", hours, minutes, seconds);
  }

  {
    int i;
    for (i = 0; i + 1 < window_height - 5 && i < ifinfo.infos_count; i++)
      {
        print_player_info (ifinfo.infos[i]);
        printw ("\n");
      }
    if (ifinfo.infos_count + 1 < window_height - 4)
      {
        for (i = ifinfo.infos_count; i + 1 < window_height - 4; i++)
          printw ("\n");
      }
    else if (ifinfo.infos_count + 1 == window_height - 4)
      {
        print_player_info (ifinfo.infos[ifinfo.infos_count - 1]);
        printw ("\n");
      }
    else
      {
        printw ("...\n");
      }
  }

  {
    struct player_info this_info = {"<ME>", ifinfo.position};
    print_player_info (this_info);
  }

  if (ifinfo.position < ifinfo.file_size)
    {
      scroll_print ();
    }
  else
    {
      printw ("Finished.\n");
    }
}

void draw ()
{
  sigset_t oldset;
  sigset_t newset;
  /* block SIGWINCH */
  {
    if (sigprocmask (0, NULL, &oldset) == -1)
      error_exit ("Cannot get signal mask.");
    sigemptyset (&newset);
    sigaddset (&newset, SIGWINCH);
    if (sigprocmask (SIG_BLOCK, &newset, &oldset) == -1)
      error_exit ("Cannot set signal mask.");
  }

  /* clear screen */
  // printf ("\033[2J\033[H\033[3J");
  move(0, 0);

  switch (ifinfo.game_state)
    {
    case XTYPE_GAME_WAITING:
      if (ifinfo.me_ready)
        {
          printw ("[Ready]\n");
          printw ("Press C to cancel.\n");
        }
      else
        {
          printw ("[Not Ready]\n");
          printw ("Press R to get ready.\n");
        } 
      printw ("Press Q to quit.\n");
      break;

    case XTYPE_GAME_RUNNING:
      draw_running ();
      break;

    case XTYPE_GAME_READY:
      printw ("Ready\n");
      break;

    case XTYPE_GAME_END:
      printw ("End\n");
      break;

    default:
      assert (0);
      break;
    }
  
  //fflush (stdout);
  refresh();
  
  /* unblock SIGWINCH */
  if (sigprocmask (SIG_SETMASK, &oldset, NULL) == -1)
    error_exit ("Cannot set signal mask.");
}



static void handler_sigwinch (int which)
{
  struct winsize ws;
  if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
    error_exit ("Cannot get new window size.");
  window_height = ws.ws_row;
  window_width = ws.ws_col;
  resizeterm (window_height, window_width);
  draw ();
}

void interface_init ()
{
  ifinfo.game_state = XTYPE_GAME_WAITING;
  ifinfo.me_ready = 0;
  ifinfo.infos_count = 0;
  if (!isatty (STDIN_FILENO))
    error_exit ("Standard input is not a terminal.");
  if (!isatty (STDOUT_FILENO))
    error_exit ("Standard output is not a terminal.");

  {
    struct termios term_in;
    if (tcgetattr (STDIN_FILENO, &term_in) == -1)
      error_exit ("Cannot get terminal attributes.");
    term_in_backup = term_in;
    term_in.c_lflag &= ~ICANON;
    term_in.c_lflag &= ~ECHO;
    if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &term_in) == -1)
      error_exit ("Cannot set terminal attributes.");
  }

  if (signal (SIGWINCH, &handler_sigwinch) == SIG_ERR)
    error_exit ("Cannot set signal handler.");
  
  ifinfo.text_buffer = malloc (XTYPE_MSG_MAXSIZE - (sizeof (struct xtype_header)) - (sizeof (struct xtype_file_header)));
  if (ifinfo.text_buffer == NULL)
    error_exit ("Cannot allocate memory for text.");

  /* get window height and window width */
  handler_sigwinch (SIGWINCH);
}

void interface_end ()
{
  if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &term_in_backup) == -1)
    error_exit ("Cannot restore terminal attributes.");
}


void read_args (int argc, char *argv[])
{
  if (argc != 4)
    error_exit ("Invalid arguments.");
  args.socket_domain = AF_INET;
  args.socket_type = SOCK_STREAM;
  args.socket_protocol = IPPROTO_TCP;
  strncpy (args.id, argv[3], XTYPE_ID_LENGTH - 1);
  args.id[XTYPE_ID_LENGTH - 1] = '\0';

  {
    struct addrinfo *ai, *ai_get;
    getaddrinfo (argv[1], argv[2], NULL, &ai_get);
    for (ai = ai_get; ai != NULL && ai->ai_family != AF_INET; ai = ai->ai_next)
      /* nothing */;
    if (ai == NULL)
      error_exit ("Cannot resolve hostname.");

    memcpy (&args.socket_address, ai->ai_addr, ai->ai_addrlen);
    freeaddrinfo (ai_get);
  }
}

static void crt_begin()
{
  initscr();
  curs_set(0);
}

static void crt_end()
{
  refresh();
  curs_set(1);
  endwin();
}

int main (int argc, char *argv[])
{
  read_args (argc, argv);
  stopwatch_init ();

  crt_begin();

  game_init ();
  interface_init ();

  game_run ();

  interface_end ();
  game_end ();

  crt_end();
  
  // printf ("\033[2J\033[H\033[3J");
  printf ("Bye.\n");
  exit (0);
}
