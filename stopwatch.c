#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include "xtype.h"
#include "error.h"
#include "interface.h"

static time_t start_time;


void stopwatch_update ()
{
  time_t current = time (NULL);
  ifinfo.duration = current - start_time;
  draw ();
}

void handler_sigalrm (int which)
{
  stopwatch_update ();
}

void stopwatch_init ()
{
  if (signal (SIGALRM, &handler_sigalrm) == SIG_ERR)
    error_exit ("Cannot set signal handler for SIGALRM");
}

void stopwatch_start ()
{
  {
    time (&start_time);
    if (start_time == -1)
      error_exit ("Cannot get current time.");
  }

  start_time = time (NULL);
  {
    struct itimerval second;
    second.it_interval.tv_sec = 1;
    second.it_interval.tv_usec = 0;
    second.it_value.tv_sec = 1;
    second.it_value.tv_usec = 0;
    if (setitimer (ITIMER_REAL, &second, NULL) == -1)
      error_exit ("Cannot set timer.");
  }
}


void stopwatch_stop ()
{
  struct itimerval second;
  second.it_interval.tv_sec = 0;
  second.it_interval.tv_usec = 0;
  second.it_value.tv_sec = 0;
  second.it_value.tv_usec = 0;
  if (setitimer (ITIMER_REAL, &second, NULL) == -1)
    error_exit ("Cannot set timer.");
}
