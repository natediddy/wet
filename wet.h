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

#ifndef WET_H
#define WET_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef PACKAGE_NAME
# define WET_PROGRAM_NAME PACKAGE_NAME
#else
# define WET_PROGRAM_NAME "wet"
#endif

#ifdef PACKAGE_VERSION
# define WET_VERSION PACKAGE_VERSION
#else
# define WET_VERSION "1.5.5"
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef __cplusplus
typedef char bool;
#  define false ((bool) 0)
#  define true ((bool) 1)
# endif
#endif

#define WET_ESUCCESS 0
#define WET_EOP      1
#define WET_ELOC     2
#define WET_ENET     3
#define WET_ESYS     4
#define WET_EWEATHER 5

extern const char *program_name;

#endif /* WET_H */

