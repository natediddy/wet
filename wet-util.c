/*
 * wet - A command line tool for retrieving weather data.
 *
 * Copyright (C) 2013  Nathan Forbes
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h> /* tolower() */
#include <stdarg.h> /* va_* */
#include <string.h> /* memcmp(), strlen(), strchr() */
#ifdef _WIN32
# include <windows.h> /* CONSOLE_SCREEN_BUFFER_INFO,
                         GetConsoleScreenBufferInfo(), GetStdHandle(),
                         GetEnvironmentVariable() */
#else
# include <sys/ioctl.h> /* ioctl() */
# include <unistd.h> /* STDOUT_FILENO, TIOCGWINSZ */
#endif

#include "wet-util.h" /* includes <stdio.h>  - stdout, stderr, fputs(),
                                               fputc(), vfprintf()
                         includes <stdlib.h> - getenv() */

#define DEFAULT_CONSOLE_WIDTH 80

void wet_print (int out, const char *tag, const char *fmt, ...)
{
  va_list ap;
  FILE *stream;

  if (out == __WET_OUTPUT_STDOUT)
    stream = stdout;
  else
    stream = stderr;

  fputs (tag, stream);
  fputc (' ', stream);
  va_start (ap, fmt);
  vfprintf (stream, fmt, ap);
  va_end (ap);
  fputc ('\n', stream);
}

void
wet_puts (const char *fmt, ...)
{
  if (!strchr (fmt, '%')) {
    fputs (fmt, stdout);
    return;
  }

  va_list ap;

  va_start (ap, fmt);
  vfprintf (stdout, fmt, ap);
  va_end (ap);
}

void
wet_eputs (const char *fmt, ...)
{
  if (!strchr (fmt, '%')) {
    fputs (fmt, stderr);
    return;
  }

  va_list ap;

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
}

int
wet_console_width (void)
{
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO x;

  if (GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &x))
    return x.dwSize.X;
#else
  struct winsize x;

  if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &x) != -1)
    return x.ws_col;
#endif
  return DEFAULT_CONSOLE_WIDTH;
}

bool
wet_streq (const char *s1, const char *s2)
{
  size_t n;

  n = strlen (s1);
  return ((n == strlen (s2)) && (memcmp (s1, s2, n) == 0));
}

bool
wet_streqi(const char *s1, const char *s2)
{
  size_t n;
  char *l;
  const char *s;

  n = strlen (s1);
  if (n != strlen (s2))
    return false;

  char l1[n + 1];
  char l2[n + 1];

  for (l = l1, s = s1; *s; ++l, ++s)
    *l = tolower (*s);
  *l = '\0';

  for (l = l2, s = s2; *s; ++l, ++s)
    *l = tolower (*s);
  *l = '\0';

  return (memcmp (l1, l2, n) == 0);
}

char *
wet_getenv (const char *key)
{
  char *value;
#ifdef _WIN32
  char buffer[1024];

  if (GetEnvironmentVariable (key, buffer, 1024) == 0)
    value = buffer;
  else
    value = NULL;
#else
  value = getenv (key);
#endif
  return value;
}

