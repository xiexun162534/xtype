#ifndef __STOPWATCH_H
#define __STOPWATCH_H

void handler_sigalrm (int which);

void stopwatch_init ();
void stopwatch_stop ();
void stopwatch_start ();

#endif
