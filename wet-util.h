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

#ifndef WET_UTIL_H
#define WET_UTIL_H

#include <stdio.h>
#include <stdlib.h>

#include "wet.h"

#define __WET_OUTPUT_STDOUT 0
#define __WET_OUTPUT_STDERR 1
#define __WET_TAG_MAX       256

#define wet_putc(c)  fputc (c, stdout)
#define wet_eputc(c) fputc (c, stderr)

#ifdef WET_DEBUG
# define wet_debug(...) \
  do { \
    char __tag[__WET_TAG_MAX]; \
    snprintf (__tag, __WET_TAG_MAX, "%s: DEBUG:", program_name); \
    wet_print (__WET_OUTPUT_STDOUT, __tag, __VA_ARGS__); \
  } while (0)
#else
# define wet_debug(...)
#endif

#define wet_error(...) \
  do { \
    char __tag[__WET_TAG_MAX]; \
    snprintf (__tag, __WET_TAG_MAX, "%s: error:", program_name); \
    wet_print (__WET_OUTPUT_STDERR, __tag, __VA_ARGS__); \
  } while (0)

#define wet_die(e, ...) \
  do { \
    char __tag[__WET_TAG_MAX]; \
    snprintf (__tag, __WET_TAG_MAX, "%s: error:", program_name); \
    wet_print (__WET_OUTPUT_STDERR, __tag, __VA_ARGS__); \
    exit (e); \
  } while (0)

#define wet_free(p) \
  do { \
    if (!p) \
      break; \
    free (p); \
    p = NULL; \
  } while (0)

void wet_print (int, const char *, const char *, ...);
void wet_puts (const char *, ...);
void wet_eputs (const char *, ...);
int wet_console_width (void);
bool wet_streq (const char *, const char *);
bool wet_streqi (const char *, const char *);
int wet_str2int (const char *);
size_t wet_str2size (const char *);
char *wet_getenv (const char *);

#endif /* WET_UTIL_H */

