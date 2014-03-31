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

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "wet.h"
#include "wet-util.h"
#include "wet-weather.h"

#define UNIT_BUFFER_MAX          WET_DATA_MAX
#define DEFAULT_CONSOLE_WIDTH    80
#define HELP_COMMAND_LEAD_SPACES 1
#define HELP_TEXT_LEAD_SPACES    4

#define DAYMASK 0
#define DAY0    (1 << 1)
#define DAY1    (1 << 2)
#define DAY2    (1 << 3)
#define DAY3    (1 << 4)
#define DAY4    (1 << 5)
#define DAYALL  (1 << 6)

#define FC_DAY_OPTS \
  "all", \
  "sunday", \
  "monday", \
  "tuesday", \
  "wednesday", \
  "thursday", \
  "friday", \
  "saturday", \
  "1", "today", \
  "2", "tomorrow", \
  "3", \
  "4", \
  "5"


const char *program_name;

/* main options given after program invocation */
static const char *main_command_options[] = {
  "cc",
  "loc",
  "fc",
  "imperial",
  "metric",
  "help",
  "version",
  NULL
};

/* options for "cc" command */
static const char *cc_options[] = {
  "last-updated",
  "temp", "temperature",
  "dewpoint",
  "text",
  "visibility",
  "humidity",
  "station",
  "feels-like",
  "wind",
  "moon",
  "uv",
  "barometer",
  NULL
};

/* options for "loc" command */
static const char *loc_options[] = {
  "latitude",
  "longitude",
  "name",
  NULL
};

/* specific day options for "fc" command */
static const char *fc_day_options[] = {
  FC_DAY_OPTS,
  NULL
};

/* options for "fc" command (this has to include day options as well) */
static const char *fc_options[] = {
  "dow",
  "high", "hi",
  "low", "lo",
  "sunset",
  "sunrise",
  "text",
  "cop",
  "humidity",
  "wind",
  "night",
  FC_DAY_OPTS,
  NULL
};

/* options for forecast_daypart_options */
static const char *fc_night_options[] = {
  "text",
  "cop",
  "humidity",
  "wind",
  NULL
};

static char *location = NULL;
static bool metric = true;
static bool default_display = false;
static struct weather w;

/* make this an almost mirror image of struct weather */
static struct {
  bool location_id;

  struct {
    bool all;
    bool last_updated;
    bool temperature;
    bool dewpoint;
    bool text;
    bool visibility;
    bool humidity;
    bool station;
    bool feels_like;
    bool wind;
    bool moon_phase;
    bool uv;
    bool barometer;
  } current_conditions;

  struct {
    bool all;
    bool lat;
    bool lon;
    bool name;
  } location;

  struct {
    bool all;
    bool day_of_week;
    bool high;
    bool sunset;
    bool low;
    bool sunrise;
    bool text;
    bool chance_precip;
    bool humidity;
    bool wind;

    struct {
      bool all;
      bool text;
      bool chance_precip;
      bool humidity;
      bool wind;
    } night;
  } forecasts[WET_FORECAST_DAYS];
} x;

static void
usage (bool error)
{
  fprintf ((!error) ? stdout : stderr,
           "Usage: %s COMMAND [OPTION] [LOCATION]\n",
           program_name);
}

static void
print_leadspace (int n)
{
  int i;

  for (i = 0; i < n; ++i)
    wet_putc (' ');
}

static void
print_text (int leadspace, bool leadonfirstline, const char *text)
{
  int c;
  int n;
  const char *p;

  n = wet_console_width ();

  if (leadonfirstline)
    print_leadspace (leadspace);

  for (c = leadspace, p = text; *p; ++c, ++p) {
    wet_putc (*p);
    if (c == (n - 2)) {
      if (*(p + 1) && (*p != ' ')) {
        if (*(p + 1) == ' ')
          ++p;
        else
          wet_putc ('-');
      }
      wet_putc ('\n');
      print_leadspace (leadspace);
      c = leadspace;
    }
  }
  wet_putc ('\n');
}

static void
print_help_cmd (const char *command, const char *text, ...)
{
  size_t n;
  va_list ap;


  print_leadspace (HELP_COMMAND_LEAD_SPACES);
  wet_puts (program_name);
  wet_putc (' ');
  wet_puts (command);
  wet_putc ('\n');

  n = strlen (text) * 2;
  /* make buffer twice the size of text just in case */
  char buffer[n];

  va_start (ap, text);
  vsnprintf (buffer, n, text, ap);
  va_end (ap);
  print_text (HELP_TEXT_LEAD_SPACES, true, buffer);
}

static void
print_separator (void)
{
  int i;
  int n;
  int w;

  w = wet_console_width ();
  n = w / 4;
  for (i = 0; i < n; ++i)
    wet_putc ('-');
  wet_putc ('\n');
}

static void
help (const char *command, const char *option1)
{
  size_t n;

  if (!command) {
    wet_puts ("Weather Tool (" WET_VERSION ") Main Options\n");
    print_separator ();
    print_help_cmd ("cc", "Shows current conditions.");
    print_help_cmd ("loc", "Shows information about LOCATION.");
    print_help_cmd ("fc", "Shows forecast predictions.");
    print_help_cmd ("imperial",
                    "Causes all measurements to use imperial units "
                    "(farenheit, miles, etc.)");
    print_help_cmd ("metric",
                    "Causes all measurements to use metric units "
                    "(celsius, kilometers, etc.). Note that this is the "
                    "default if no unit command is given.");
    print_help_cmd ("help",
                    "Shows help information and exits. Use `%s help COMMAND' "
                    "for help with the specific COMMAND.",
                    program_name);
    print_help_cmd ("version",
                    "Shows the version information of this program.");
    print_separator ();
    print_text (0, false,
                "If no option commands are given, a default set of basic "
                "weather data will be displayed.");
    wet_putc ('\n');
    n = strlen ("NOTE: ");
    print_text (n, false,
                "NOTE: Instead of providing a LOCATION argument every time, "
                "you can set the WET_LOCATION environment variable to your "
                "desired location (e.g. WET_LOCATION=\"New York City\").");
    wet_putc ('\n');
    print_text (n, false,
                "NOTE: You can also set the WET_UNITS environment variable "
                "to your preferred set of units (e.g. WET_UNITS=imperial or "
                "WET_UNITS=metric).");
    wet_putc ('\n');
    print_text (0, false,
                "All weather data is obtained from www.weather.com.");
    return;
  }

  if (wet_streqi (command, "cc")) {
    if (option1) {
      if (wet_streqi (option1, "last-updated"))
        print_help_cmd ("cc last-updated",
                        "Shows when the current conditions data was last "
                        "updated.");
      else if (wet_streqi (option1, "temp"))
        print_help_cmd ("cc temp", "Shows the current temperature.");
      else if (wet_streqi (option1, "dewpoint"))
        print_help_cmd ("cc dewpoint",
                        "Shows the current dewpoint temperature.");
      else if (wet_streqi (option1, "text"))
        print_help_cmd ("cc text",
                        "Shows a short, general description of the current "
                        "conditions (e.g. \"Partly Cloudy\").");
      else if (wet_streqi (option1, "visibility"))
        print_help_cmd ("cc visibility", "Shows the current visibility.");
      else if (wet_streqi (option1, "humidity"))
        print_help_cmd ("cc humidity", "Shows the current humidity.");
      else if (wet_streqi (option1, "station"))
        print_help_cmd ("cc station",
                        "Shows the station name from which local weather is "
                        "obtained.");
      else if (wet_streqi (option1, "feels-like"))
        print_help_cmd ("cc feels-like",
                        "Shows the temperature that it currently "
                        "\"feels like\".");
      else if (wet_streqi (option1, "moon"))
        print_help_cmd ("cc moon", "Shows the current phase of the Moon.");
      else if (wet_streqi (option1, "uv"))
        print_help_cmd ("cc uv",
                        "Shows current ultra-violet data from the sun.");
      else if (wet_streqi (option1, "barometer"))
        print_help_cmd ("cc barometer",
                        "Shows current atmospheric pressure data.");
      else if (wet_streqi (option1, "wind"))
        print_help_cmd ("cc wind", "Shows current wind conditions.");
      else
        wet_die (WET_EOP, "unknown option for `cc' -- `%s'", option1);
      return;
    }
    wet_puts ("Weather Tool (" WET_VERSION ") Current Conditions Options\n");
    print_separator ();
    print_help_cmd ("cc last-updated",
                    "Shows when the current conditions data was last "
                    "updated.");
    print_help_cmd ("cc temp", "Shows the current temperature.");
    print_help_cmd ("cc dewpoint", "Shows the current dewpoint temperature.");
    print_help_cmd ("cc text",
                    "Shows a short, general description of the current "
                    "conditions (e.g. \"Partly Cloudy\").");
    print_help_cmd ("cc visibility", "Shows the current visibility.");
    print_help_cmd ("cc humidity", "Shows the current humidity.");
    print_help_cmd ("cc station",
                   "Shows the station name from which local weather is "
                   "obtained.");
    print_help_cmd ("cc feels-like",
                    "Shows the temperature that it currently "
                    "\"feels like\".");
    print_help_cmd ("cc moon", "Shows the current phase of the Moon.");
    print_help_cmd ("cc uv",
                    "Shows current ultra-violet data from the sun.");
    print_help_cmd ("cc barometer",
                    "Shows current atmospheric pressure data.");
    print_help_cmd ("cc wind", "Shows current wind conditions.");
    print_separator ();
    print_text (0, false,
                "If none of the `cc' options are provided, then ALL current "
                "conditions data will be displayed.");
    return;
  }

  if (wet_streqi (command, "loc")) {
    if (option1) {
      if (wet_streqi (option1, "latitude"))
        print_help_cmd ("loc latitude", "Shows the latitude of LOCATION.");
      else if (wet_streqi (option1, "longitude"))
        print_help_cmd ("loc longitude", "Shows the longitude of LOCATION.");
      else if (wet_streqi (option1, "name"))
        print_help_cmd ("loc name", "Shows the proper name of LOCATION.");
      else
        wet_die (WET_EOP, "unknown option for `loc' -- `%s'", option1);
      return;
    }
    wet_puts ("Weather Tool (" WET_VERSION ") Location Options\n");
    print_separator ();
    print_help_cmd ("loc latitude", "Shows the latitude of LOCATION.");
    print_help_cmd ("loc longitude", "Shows the longitude of LOCATION.");
    print_help_cmd ("loc name", "Shows the proper name of LOCATION.");
    print_separator ();
    return;
  }

  if (wet_streqi (command, "fc")) {
    if (option1) {
      if (wet_streqi (option1, "1") || wet_streqi (option1, "2") ||
          wet_streqi (option1, "3") || wet_streqi (option1, "4") ||
          wet_streqi (option1, "5") || wet_streqi (option1, "today") ||
          wet_streqi (option1, "tomorrow"))
        print_help_cmd ("fc [1-5|today|tomorrow]",
                        "Shows forecast data for a specific day out of a "
                        "5 day forecast (1=today, 2=tomorrow, etc.). If this "
                        "option is not given, only the forecast data for "
                        "today will be used.");
      else if (wet_streqi (option1, "all"))
        print_help_cmd ("fc all",
                        "Shows forecast data for all days in the 5 day "
                        "forecast.");
      else if (wet_streqi (option1, "dow"))
        print_help_cmd ("fc dow",
                        "Shows the name for the day of the week of the "
                        "forecast day.");
      else if (wet_streqi (option1, "high"))
        print_help_cmd ("fc high",
                        "Shows the highest forecasted temperature.");
      else if (wet_streqi (option1, "low"))
        print_help_cmd ("fc low", "Shows the lowest forecasted temperature.");
      else if (wet_streqi (option1, "sunrise"))
        print_help_cmd ("fc sunrise", "Shows the time of sunrise.");
      else if (wet_streqi (option1, "sunset"))
        print_help_cmd ("fc sunset", "Shows the time of sunset.");
      else if (wet_streqi (option1, "text"))
        print_help_cmd ("fc text",
                        "Shows a brief description of the forecast.");
      else if (wet_streqi (option1, "cop"))
        print_help_cmd ("fc cop", "Shows the chance of precipitation.");
      else if (wet_streqi (option1, "humidity"))
        print_help_cmd ("fc humidity", "Shows the humidity.");
      else if (wet_streqi (option1, "night")) {
        print_help_cmd ("fc night text",
                        "Shows a brief description of the night forecast.");
        print_help_cmd ("fc night cop",
                        "Shows the chance of precipitation for the night.");
        print_help_cmd ("fc night humidity",
                        "Shows the humidity for the night.");
        print_help_cmd ("fc night wind",
                        "Shows wind conditions for the night.");
      } else if (wet_streqi (option1, "wind"))
        print_help_cmd ("fc wind",
                        "Shows the wind forecasts for the forecast day.");
      else
        wet_die (WET_EOP, "unknown option for `fc' -- `%s'", option1);
      return;
    }
    wet_puts ("Weather Tool (" WET_VERSION ") Forecast Options\n");
    print_separator ();
    print_help_cmd ("fc [1-5|today|tomorrow]",
                    "Shows forecast data for a specific day out of a 5 day "
                    "forecast (1=today, 2=tomorrow, etc.). If this option "
                    "is not given, only the forecast data for today will be "
                    "used.");
    print_help_cmd ("fc all",
                    "Shows forecast data for all days in the 5 day "
                    "forecast.");
    print_help_cmd ("fc dow",
                    "Shows the name for the day of the week of the forecast "
                    "day.");
    print_help_cmd ("fc high", "Shows the highest forecasted temperature.");
    print_help_cmd ("fc low", "Shows the lowest forecasted temperature.");
    print_help_cmd ("fc sunrise", "Shows the time of sunrise.");
    print_help_cmd ("fc sunset", "Shows the time of sunset.");
    print_help_cmd ("fc text", "Shows a brief description of the forecast.");
    print_help_cmd ("fc cop", "Shows the chance of precipitation.");
    print_help_cmd ("fc humidity", "Shows the humidity.");
    print_help_cmd ("fc night",
                    "Shows forecast information for the night of the "
                    "forecast day. Note that this command has 3 of its own "
                    "options (use `%s help fc night' to see them).",
                    program_name);
    print_help_cmd ("fc wind", "Shows wind forecasts for the forecast day.");
    print_separator ();
    return;
  }

  wet_die (WET_EOP, "unknown command -- `%s'", command);
}

static void
version (void)
{
  wet_puts (WET_PROGRAM_NAME " (WEather Tool) " WET_VERSION "\n"
            "Written by Nathan Forbes (2014)\n");
  exit (WET_ESUCCESS);
}

static void
init_display_opts (void)
{
  int i;

  x.current_conditions.all = false;
  x.current_conditions.last_updated = false;
  x.current_conditions.temperature = false;
  x.current_conditions.dewpoint = false;
  x.current_conditions.text = false;
  x.current_conditions.visibility = false;
  x.current_conditions.humidity = false;
  x.current_conditions.station = false;
  x.current_conditions.feels_like = false;
  x.current_conditions.moon_phase = false;
  x.current_conditions.uv = false;
  x.current_conditions.barometer = false;
  x.current_conditions.wind = false;

  x.location.all = false;
  x.location.lat = false;
  x.location.lon = false;
  x.location.name = false;

  for (i = 0; i < WET_FORECAST_DAYS; ++i) {
    x.forecasts[i].all = false;
    x.forecasts[i].day_of_week = false;
    x.forecasts[i].high = false;
    x.forecasts[i].sunset = false;
    x.forecasts[i].low = false;
    x.forecasts[i].sunrise = false;
    x.forecasts[i].text = false;
    x.forecasts[i].chance_precip = false;
    x.forecasts[i].humidity = false;
    x.forecasts[i].wind = false;
    x.forecasts[i].night.all = false;
    x.forecasts[i].night.text = false;
    x.forecasts[i].night.chance_precip = false;
    x.forecasts[i].night.humidity = false;
    x.forecasts[i].night.wind = false;
  }
}

#define __is_option_func_body(__o, __a) \
  size_t __i; \
  for (__i = 0; __a[__i]; ++__i) \
    if (wet_streqi (__o, __a[__i])) \
      return true; \
  return false

static bool
is_main_command_option (const char *opt)
{
  __is_option_func_body (opt, main_command_options);
}

static bool
is_cc_option (const char *opt)
{
  __is_option_func_body (opt, cc_options);
}

static bool
is_loc_option (const char *opt)
{
  __is_option_func_body (opt, loc_options);
}

static bool
is_fc_option (const char *opt)
{
  __is_option_func_body (opt, fc_options);
}

static bool
is_fc_day_option (const char *opt)
{
  __is_option_func_body (opt, fc_day_options);
}

static bool
is_fc_night_option (const char *opt)
{
  __is_option_func_body (opt, fc_night_options);
}

#undef __is_option_func_body

static void
find_wanted_location (int *c, char **v)
{
  size_t i;
  size_t j;

  for (i = 1; v[i]; ++i) {
    if (is_main_command_option (v[i]) || is_cc_option (v[i]) ||
        is_loc_option (v[i]) || is_fc_option (v[i]) ||
        is_fc_night_option (v[i]))
      continue;
    if (location)
      wet_die (WET_EOP, "too many location arguments given");
    location = v[i];
    /* remove the location argument from the array */
    *c -= 1;
    for (j = i; v[j]; ++j)
      v[j] = v[j + 1];
  }

  if (!location)
    location = wet_getenv ("WET_LOCATION");

  if (!location || !*location)
    wet_die (WET_ELOC, "no location given and WET_LOCATION not set");
}

static void
find_wanted_units (int *c, char **v)
{
  size_t i;
  ssize_t j;
  const char *evar;

  j = -1;
  for (i = 1; v[i]; ++i) {
    if (wet_streqi (v[i], "imperial")) {
      j = i;
      metric = false;
      break;
    }
    if (wet_streqi (v[i], "metric")) {
      j = i;
      break;
    }
  }

  if (j != -1) {
    *c -= 1;
    /* remove the unit option from array */
    for (; v[j]; ++j)
      v[j] = v[j + 1];
    return;
  }

  evar = wet_getenv ("WET_UNITS");
  if (evar && *evar) {
    if (wet_streqi (evar, "imperial"))
      metric = false;
    else if (wet_streqi (evar, "metric"))
      metric = true;
    else
      wet_error ("ignoring invalid value for environment variable WET_UNITS");
  }
}

static int
find_wanted_forecast_days (int *c, char **v)
{
  int day;
  size_t i;
  size_t j;

  day = DAYMASK;
  for (i = 1; v[i]; ++i) {
    if (is_fc_day_option (v[i])) {
      if (wet_streq (v[i], "1") || wet_streqi (v[i], "today"))
        day |= DAY0;
      else if (wet_streq (v[i], "2") || wet_streqi (v[i], "tomorrow"))
        day |= DAY1;
      else if (wet_streq (v[i], "3"))
        day |= DAY2;
      else if (wet_streq (v[i], "4"))
        day |= DAY3;
      else if (wet_streq (v[i], "5"))
        day |= DAY4;
      else if (wet_streqi (v[i], "all"))
        day |= DAYALL;
      *c -= 1;
      for (j = i; v[j]; ++j)
        v[j] = v[j + 1];
      i--;
    }
  }

  if (day == DAYMASK)
    day |= DAY0;

  return day;
}

static void
parse_opt (int c, char **v)
{
  int i;
  int j;
  int k;
  int day;

  program_name = v[0];

  find_wanted_location (&c, v);
  find_wanted_units (&c, v);

  if (c == 1) {
    if (!location) {
      usage (true);
      exit (WET_ELOC);
    }
    default_display = true;
    return;
  }

  if (wet_streqi (v[1], "help")) {
    if (c > 4)
      wet_die (WET_EOP, "too many arguments for `help'");
    else if (c == 4)
      help (v[2], v[3]);
    else if (c == 3)
      help (v[2], NULL);
    else
      help (NULL, NULL);
    exit (WET_ESUCCESS);
  }

  if (wet_streqi (v[1], "version")) {
    if (c > 2)
      wet_die (WET_EOP, "too many arguments for `version'");
    version ();
  }

  init_display_opts ();

  if (wet_streqi (v[1], "cc")) {
    if (!v[2]) {
      x.current_conditions.all = true;
      return;
    }
    for (i = 2; v[i]; ++i) {
      if (wet_streqi (v[i], "last-updated"))
        x.current_conditions.last_updated = true;
      else if (wet_streqi (v[i], "temp"))
        x.current_conditions.temperature = true;
      else if (wet_streqi (v[i], "dewpoint"))
        x.current_conditions.dewpoint = true;
      else if (wet_streqi (v[i], "text"))
        x.current_conditions.text = true;
      else if (wet_streqi (v[i], "visibility"))
        x.current_conditions.visibility = true;
      else if (wet_streqi (v[i], "humidity"))
        x.current_conditions.humidity = true;
      else if (wet_streqi (v[i], "station"))
        x.current_conditions.station = true;
      else if (wet_streqi (v[i], "feels-like"))
        x.current_conditions.feels_like = true;
      else if (wet_streqi (v[i], "wind"))
        x.current_conditions.wind = true;
      else if (wet_streqi (v[i], "moon"))
        x.current_conditions.moon_phase = true;
      else if (wet_streqi (v[i], "uv"))
        x.current_conditions.uv = true;
      else if (wet_streqi (v[i], "barometer"))
        x.current_conditions.barometer = true;
      else
        wet_die (WET_EOP, "unknown `cc' option -- `%s'", v[i]);
    }
    return;
  }

  if (wet_streqi (v[1], "loc")) {
    if (!v[2]) {
      x.location.all = true;
      return;
    }
    for (i = 2; v[i]; ++i) {
      if (wet_streqi (v[i], "latitude"))
        x.location.lat = true;
      else if (wet_streqi (v[i], "longitude"))
        x.location.lon = true;
      else if (wet_streqi (v[i], "name"))
        x.location.name = true;
      else
        wet_die (WET_EOP, "unknown `loc' option -- `%s'", v[i]);
    }
    return;
  }

#define __is_specified_day(__d, __i) \
  (__d & DAYALL) || \
   (((__d & DAY0) && (__i == 0)) || \
    ((__d & DAY1) && (__i == 1)) || \
    ((__d & DAY2) && (__i == 2)) || \
    ((__d & DAY3) && (__i == 3)) || \
    ((__d & DAY4) && (__i == 4)))

  if (wet_streqi (v[1], "fc")) {
    day = find_wanted_forecast_days (&c, v);
    if (!v[2]) {
      for (i = 0; i < WET_FORECAST_DAYS; ++i)
        if (__is_specified_day (day, i))
          x.forecasts[i].all = true;
      return;
    }
    for (i = 2; v[i]; ++i) {
      for (j = 0; j < WET_FORECAST_DAYS; ++j) {
        if (__is_specified_day (day, j)) {
          if (wet_streqi (v[i], "dow"))
            x.forecasts[j].day_of_week = true;
          else if (wet_streqi (v[i], "high"))
            x.forecasts[j].high = true;
          else if (wet_streqi (v[i], "low"))
            x.forecasts[j].low = true;
          else if (wet_streqi (v[i], "sunset"))
            x.forecasts[j].sunset = true;
          else if (wet_streqi (v[i], "sunrise"))
            x.forecasts[j].sunrise = true;
          else if (wet_streqi (v[i], "text"))
            x.forecasts[j].text = true;
          else if (wet_streqi (v[i], "cop"))
            x.forecasts[j].chance_precip = true;
          else if (wet_streqi (v[i], "humidity"))
            x.forecasts[j].humidity = true;
          else if (wet_streqi (v[i], "wind"))
            x.forecasts[j].wind = true;
          else if (wet_streqi (v[i], "night")) {
            if (!v[i + 1]) {
              x.forecasts[j].night.all = true;
              continue;
            }
            for (k = i + 1; v[k]; ++k) {
              if (wet_streqi (v[k], "text"))
                x.forecasts[j].night.text = true;
              else if (wet_streqi (v[k], "cop"))
                x.forecasts[j].night.chance_precip = true;
              else if (wet_streqi (v[k], "humidity"))
                x.forecasts[j].night.humidity = true;
              else if (wet_streqi (v[k], "wind"))
                x.forecasts[j].night.wind = true;
              else
                wet_die (WET_EOP, "unknown `fc night' option -- `%s'", v[k]);
              i = k;
            }
          } else
            wet_die (WET_EOP, "unknown `fc' option -- `%s'", v[i]);
        }
      }
    }
  }

#undef __is_specified_day
}

static void
print_forecast_data (int day, bool night, const char *text, ...)
{
  va_list ap;

  if (day == 0) {
    if (night)
      wet_puts ("tonight");
    else
      wet_puts ("today");
  } else {
    wet_puts ("%s", w.forecasts[day].day_of_week);
    if (night)
      wet_puts (" night");
  }
  wet_puts ("'s ");

  va_start (ap, text);
  vfprintf (stdout, text, ap);
  va_end (ap);
  wet_putc ('\n');
}

static void
display (void)
{
  int day;

#define __display_wind(__w) \
  do { \
    wet_puts ("%sº %s", __w.direction, __w.text); \
    if ((wet_str2int (__w.speed) != 0) && isdigit (*__w.speed)) \
      wet_puts (" at %s%s", __w.speed, w.units.speed); \
    if (!wet_streqi (__w.gust, "n/a")) \
      wet_puts (" (%s%s gusts)", __w.gust, w.units.speed); \
    wet_putc ('\n'); \
  } while (0)

  if (default_display) {
    wet_puts ("%s (%s, %s)\n"
              "%sº%s and %s (feels like %sº%s)\n"
              "Today's high:    %sº%s\n"
              "Today's low:     %sº%s\n"
              "Visibility:      %s%s\n"
              "Humidity:        %s%%\n"
              "Dew Point:       %sº%s\n"
              "Sunrise:         %s\n"
              "Sunset:          %s\n"
              "Wind Conditions: ",
              w.location.name, w.location.lat, w.location.lon,
              w.current_conditions.temperature, w.units.temperature,
              w.current_conditions.text, w.current_conditions.feels_like,
              w.units.temperature, w.forecasts[0].high,
              w.units.temperature, w.forecasts[0].low,
              w.units.temperature, w.current_conditions.visibility,
              w.units.distance, w.current_conditions.humidity,
              w.current_conditions.dewpoint, w.units.temperature,
              w.forecasts[0].sunrise, w.forecasts[0].sunset);
    __display_wind (w.current_conditions.wind);
    return;
  }

  if (x.current_conditions.all) {
    wet_puts ("Current Conditions - %s\n"
              "----------------\n"
              "Last Updated        %s\n"
              "Temperature         %sº%s\n"
              "Dew Point           %sº%s\n"
              "Visibility          %s%s\n"
              "Humidity            %s%%\n"
              "Local Station       %s\n"
              "Feels Like          %sº%s\n"
              "Moon                %s\n"
              "UV Index            %s (%s)\n"
              "Barometric Pressure %s%s (%s)\n"
              "Wind                ",
              w.current_conditions.text,
              w.current_conditions.last_updated,
              w.current_conditions.temperature, w.units.temperature,
              w.current_conditions.dewpoint, w.units.temperature,
              w.current_conditions.visibility, w.units.distance,
              w.current_conditions.humidity,
              w.current_conditions.station,
              w.current_conditions.feels_like, w.units.temperature,
              w.current_conditions.moon_phase.text,
              w.current_conditions.uv.index,
              w.current_conditions.uv.text,
              w.current_conditions.barometer.reading, w.units.rainfall,
              w.current_conditions.barometer.direction);
    __display_wind (w.current_conditions.wind);
  }

  if (x.location.all)
    wet_puts ("%s\n"
              "----------------\n"
              "Latitude  %s\n"
              "Longitude %s\n",
              w.location.name,
              w.location.lat,
              w.location.lon);

  if (x.current_conditions.last_updated)
    wet_puts ("last updated - %s\n", w.current_conditions.last_updated);

  if (x.current_conditions.temperature)
    wet_puts ("current temperature - %sº%s\n",
              w.current_conditions.temperature, w.units.temperature);

  if (x.current_conditions.dewpoint)
    wet_puts ("current dew point - %sº%s\n",
              w.current_conditions.dewpoint, w.units.temperature);

  if (x.current_conditions.text)
    wet_puts ("%s\n", w.current_conditions.text);

  if (x.current_conditions.visibility)
    wet_puts ("current visibility - %s%s\n",
              w.current_conditions.visibility, w.units.distance);

  if (x.current_conditions.humidity)
    wet_puts ("current humidity - %s%%\n", w.current_conditions.humidity);

  if (x.current_conditions.station)
    wet_puts ("current local station - %s\n", w.current_conditions.station);

  if (x.current_conditions.feels_like)
    wet_puts ("currently feels like - %sº%s\n",
              w.current_conditions.feels_like, w.units.temperature);

  if (x.current_conditions.wind) {
    wet_puts ("current wind conditions - ");
    __display_wind (w.current_conditions.wind);
  }

  if (x.current_conditions.moon_phase)
    wet_puts ("current moon phase - %s\n",
              w.current_conditions.moon_phase.text);

  if (x.current_conditions.uv)
    wet_puts ("current uv index - %s (%s)\n",
              w.current_conditions.uv.index, w.current_conditions.uv.text);

  if (x.current_conditions.barometer)
    wet_puts ("current barometric pressure - %s%s (%s)\n",
              w.current_conditions.barometer.reading,
              w.units.rainfall,
              w.current_conditions.barometer.direction);

  if (x.location.lat)
    wet_puts ("latitude - %s\n", w.location.lat);

  if (x.location.lon)
    wet_puts ("longitude - %s\n", w.location.lon);

  if (x.location.name)
    wet_puts ("location name - %s\n", w.location.name);

  for (day = 0; day < WET_FORECAST_DAYS; ++day) {
    if (x.forecasts[day].all) {
      wet_puts ("Forecast for ");
      if (day == 0)
        wet_puts ("today (%s)", w.forecasts[day].day_of_week);
      else if (day == 1)
        wet_puts ("tomorrow (%s)", w.forecasts[day].day_of_week);
      else
        wet_puts (w.forecasts[day].day_of_week);
      if (*w.forecasts[day].text)
        wet_puts (" - %s", w.forecasts[day].text);
      wet_puts ("\n--------------\n"
                "high                    - %sº%s\n"
                "low                     - %sº%s\n"
                "sunset                  - %s\n"
                "sunrise                 - %s\n"
                "chance of precipitation - %s%%\n"
                "humidity                - %s%%\n"
                "wind                    - ",
                w.forecasts[day].high, w.units.temperature,
                w.forecasts[day].low, w.units.temperature,
                w.forecasts[day].sunset,
                w.forecasts[day].sunrise,
                w.forecasts[day].chance_precip,
                w.forecasts[day].humidity);
      __display_wind (w.forecasts[day].wind);
      wet_putc ('\n');
      if (day == 0)
        wet_puts ("  Tonight");
      else if (day == 1)
        wet_puts ("  Tomorrow night");
      else
        wet_puts ("  %s night", w.forecasts[day].day_of_week);
      if (*w.forecasts[day].night.text)
        wet_puts (" - %s", w.forecasts[day].night.text);
      wet_puts ("\n  --------------\n"
                "  chance of precipitation - %s%%\n"
                "  humidity                - %s%%\n"
                "  wind                    - ",
                w.forecasts[day].night.chance_precip,
                w.forecasts[day].night.humidity);
      __display_wind (w.forecasts[day].night.wind);
      wet_putc ('\n');
      continue;
    }
    if (x.forecasts[day].day_of_week)
      wet_puts ("%s\n", w.forecasts[day].day_of_week);
    if (x.forecasts[day].high)
      print_forecast_data (day, false, "high - %sº%s",
                           w.forecasts[day].high, w.units.temperature);
    if (x.forecasts[day].low)
      print_forecast_data (day, false, "low - %sº%s",
                           w.forecasts[day].low, w.units.temperature);
    if (x.forecasts[day].sunset)
      print_forecast_data (day, false, "sunset - %s",
                           w.forecasts[day].sunset);
    if (x.forecasts[day].sunrise)
      print_forecast_data (day, false, "sunrise - %s",
                           w.forecasts[day].sunrise);
    if (x.forecasts[day].text)
      print_forecast_data (day, false, "%s", w.forecasts[day].text);
    if (x.forecasts[day].chance_precip)
      print_forecast_data (day, false, "chance of precipitation - %s%%",
                           w.forecasts[day].chance_precip);
    if (x.forecasts[day].humidity)
      print_forecast_data (day, false, "humidity - %s%%",
                           w.forecasts[day].humidity);
    if (x.forecasts[day].wind) {
      if (day == 0)
        wet_puts ("today's wind - ");
      else
        wet_puts ("%s's wind - ", w.forecasts[day].day_of_week);
      __display_wind (w.forecasts[day].wind);
    }
    if (x.forecasts[day].night.all) {
      wet_puts ("Forecast for ");
      if (day == 0)
        wet_puts ("tonight");
      else if (day == 1)
        wet_puts ("tomorrow night");
      else
        wet_puts ("%s night", w.forecasts[day].day_of_week);
      if (*w.forecasts[day].night.text)
        wet_puts (" - %s\n", w.forecasts[day].night.text);
      wet_puts ("--------------\n"
                "chance of precipitation - %s%%\n"
                "humidity                - %s%%\n"
                "wind                    - ",
                w.forecasts[day].night.chance_precip,
                w.forecasts[day].night.humidity);
      __display_wind (w.forecasts[day].night.wind);
      continue;
    }
    if (x.forecasts[day].night.text)
      print_forecast_data (day, true, "%s", w.forecasts[day].night.text);
    if (x.forecasts[day].night.chance_precip)
      print_forecast_data (day, true, "chance of precipitation - %s%%",
                           w.forecasts[day].night.chance_precip);
    if (x.forecasts[day].night.humidity)
      print_forecast_data (day, true, "humidity - %s%%",
                           w.forecasts[day].night.humidity);
    if (x.forecasts[day].night.wind) {
      if (day == 0)
        wet_puts ("tonight");
      else if (day == 1)
        wet_puts ("tomorrow night");
      else
        wet_puts ("%s night", w.forecasts[day].day_of_week);
      wet_puts ("'s wind - ");
      __display_wind (w.forecasts[day].night.wind);
    }
  }

#undef __display_wind
}

int
main (int argc, char **argv)
{
  parse_opt (argc, argv);

  if (!wet_weather (&w, location, metric)) {
    if (*w.error.text)
      wet_die (WET_EWEATHER, "weather: %s", w.error.text);
    wet_die (WET_ENET, "failed to retrieve weather data");
  }
  display ();
  exit (WET_ESUCCESS);
  return 0; /* for compiler */
}

