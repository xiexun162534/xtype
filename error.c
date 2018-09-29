#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int print_error (const char *format, ...)
{
  va_list ap;
  int r = 0;


  va_start (ap, format);
  r += fprintf (stderr, "xtype: ");
  r += vfprintf (stderr, format, ap);
  r += fprintf (stderr, "\n");
  return r;
}

void error_exit (const char *format, ...)
{
  va_list ap;
  int r = 0;


  va_start (ap, format);
  r += fprintf (stderr, "xtype: ");
  r += vfprintf (stderr, format, ap);
  r += fprintf (stderr, "\n");
  exit (1);
}

