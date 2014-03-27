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

#include <stdarg.h>
#include <string.h>

#include "wet.h"
#include "wet-util.h"
#include "wet-weather.h"

#define UNIT_BUFFER_MAX          WET_DATA_MAX
#define DEFAULT_CONSOLE_WIDTH    80
#define HELP_COMMAND_LEAD_SPACES 2
#define HELP_TEXT_LEAD_SPACES    8


enum {
  DAY_0,
  DAY_1,
  DAY_2,
  DAY_3,
  DAY_4
};

struct __wind_display_opts {
  bool all;
  bool gust;
  bool direction;
  bool speed;
  bool text;
};

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
  "temp",
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

/* options for "uv" option */
static const char *cc_uv_options[] = {
  "index",
  "text",
  NULL
};

/* options for "barometer" option */
static const char *cc_barometer_options[] = {
  "direction",
  "reading",
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
  "1", "today", /* this one is implied if no day option is given */
  "2", "tomorrow",
  "3",
  "4",
  "5",
  NULL
};

/* options for "fc" command (this has to include day options as well) */
static const char *fc_options[] = {
  "dow",
  "high",
  "low",
  "sunset",
  "sunrise",
  "text",
  "cop",
  "humidity",
  "wind",
  "night",
  "1", "today",
  "2", "tomorrow",
  "3",
  "4",
  "5",
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

static char *         location        = NULL;
static bool           metric          = true;
static bool           default_display = false;
static struct weather w;

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
    struct __wind_display_opts wind;

    struct {
      bool text;
    } moon_phase;

    struct {
      bool all;
      bool index;
      bool text;
    } uv;

    struct {
      bool all;
      bool direction;
      bool reading;
    } barometer;
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
    struct __wind_display_opts wind;

    struct {
      bool all;
      bool text;
      bool chance_precip;
      bool humidity;
      struct __wind_display_opts wind;
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
print_help_cmd (const char *command, const char *text, ...)
{
  int c;
  int max;
  size_t n;
  va_list ap;
  const char *p;


  for (c = 0; c < HELP_COMMAND_LEAD_SPACES; ++c)
    wet_putc (' ');

  wet_puts (program_name);
  wet_putc (' ');
  wet_puts (command);
  wet_putc ('\n');

  n = strlen (text);
  /* make buffer twice the size of text just in case */
  char buffer[n * 2];

  va_start (ap, text);
  vsnprintf (buffer, n * 2, text, ap);
  va_end (ap);

  for (c = 0; c < HELP_TEXT_LEAD_SPACES; ++c)
    wet_putc (' ');

  max = wet_console_width ();
  for (p = buffer; *p; ++p, ++c) {
    wet_putc (*p);
    if (c == (max - 2)) {
      if (*(p + 1) && (*p != ' ')) {
        if (*(p + 1) == ' ')
          ++p;
        else
          wet_putc ('-');
      }
      wet_putc ('\n');
      for (c = 0; c < HELP_TEXT_LEAD_SPACES; ++c)
        wet_putc (' ');
    }
  }
  wet_putc ('\n');
}

static void
help (const char *command, const char *option1)
{
  if (!command) {
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
                    "for help with a specific command.",
                    program_name);
    print_help_cmd ("version",
                    "Shows the version information of this program.");
    wet_puts ("If no option commands are given, a default set of basic "
              "weather data will be displayed.\n");
    wet_puts ("All weather data is obtained from www.weather.com.\n");
    wet_puts ("NOTE: instead of providing a LOCATION argument every time\n"
              "      the program is run, you can set the WET_LOCATION\n"
              "      environment variable to your location (e.g.\n"
              "      WET_LOCATION=\"new york city\").\n");
    wet_puts ("NOTE: you can also set the WET_UNITS environment variable\n"
              "      to your preferred set of units (e.g.\n"
              "      WET_UNITS=imperial or WET_UNITS=metric).\n");
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
      else if (wet_streqi (option1, "uv")) {
        print_help_cmd ("cc uv index",
                        "Shows the current ultra-violet index.");
        print_help_cmd ("cc uv text",
                        "Shows a brief description of the current "
                        "ultra-violet conditions.");
      } else if (wet_streqi (option1, "barometer")) {
        print_help_cmd ("cc barometer direction",
                        "Shows a description of the current condition of the "
                        "barometric pressure.");
        print_help_cmd ("cc barometer reading",
                        "Shows the current barometric pressure.");
      } else if (wet_streqi (option1, "wind"))
        print_help_cmd ("cc wind", "Shows current wind conditions.");
      else
        wet_die ("unknown option for `cc' -- `%s'", option1);
      return;
    }
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
                    "Shows current ultra-violet data from the sun. Note that "
                    "this command includes 2 of its own options (use "
                    "`%s help cc uv' to see them).",
                    program_name);
    print_help_cmd ("cc barometer",
                    "Shows current atmospheric pressure data. Note that this "
                    "command includes 2 of its own options (use "
                    "`%s help cc barometer' to see them).",
                    program_name);
    print_help_cmd ("cc wind", "Shows current wind conditions.");
    wet_puts ("If none of the `cc' options are provided, then ALL current "
              "conditions data will be displayed.\n");
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
        wet_die ("unknown option for `loc' -- `%s'", option1);
      return;
    }
    print_help_cmd ("loc latitude", "Shows the latitude of LOCATION.");
    print_help_cmd ("loc longitude", "Shows the longitude of LOCATION.");
    print_help_cmd ("loc name", "Shows the proper name of LOCATION.");
    return;
  }

  if (wet_streqi (command, "fc")) {
    if (option1) {
      if (wet_streqi (option1, "1") || wet_streqi (option1, "2") ||
          wet_streqi (option1, "3") || wet_streqi (option1, "4") ||
          wet_streqi (option1, "5") || wet_streqi (option1, "today") ||
          wet_streqi (option1, "tomorrow"))
        print_help_cmd ("fc [1-5]",
                        "Shows forecast for a specific day out of a 5-day "
                        "forecast (1=today, 2=tomorrow, etc.). If this "
                        "option is not given, only the forecast data for "
                        "today will be used.");
      else if (wet_streqi (option1, "dow"))
        print_help_cmd ("fc dow",
                        "Shows the name for the day of the week of the "
                        "forecast day.");
      else if (wet_streqi (option1, "high"))
        print_help_cmd ("fc high", "Shows the highest forecasted temperature.");
      else if (wet_streqi (option1, "low"))
        print_help_cmd ("fc low", "Shows the lowest forecasted temperature.");
      else if (wet_streqi (option1, "sunrise"))
        print_help_cmd ("fc sunrise", "Shows the time of sunrise.");
      else if (wet_streqi (option1, "sunset"))
        print_help_cmd ("fc sunset", "Shows the time of sunset.");
      else if (wet_streqi (option1, "text"))
        print_help_cmd ("fc text", "Shows a brief description of the forecast.");
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
        wet_die ("unknown option for `fc' -- `%s'", option1);
    }
    print_help_cmd ("fc [1-5]",
                    "Shows forecast for a specific day out of a 5 day "
                    "forecast (1=today, 2=tomorrow, etc.). If this option "
                    "is not given, only the forecast data for today will be "
                    "used.");
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
    return;
  }

  wet_die ("unknown command -- `%s'", command);
}

static void
version (void)
{
  wet_puts (WET_PROGRAM_NAME " (WEather Tool) " WET_VERSION "\n"
            "Written by Nathan Forbes (2014)\n");
  exit (EXIT_SUCCESS);
}

static void
init_display_opts (void)
{
  int i;
#define __init_wind(__w) \
  __w.all = false; \
  __w.gust = false; \
  __w.direction = false; \
  __w.speed = false; \
  __w.text = false

  x.current_conditions.all = false;
  x.current_conditions.last_updated = false;
  x.current_conditions.temperature = false;
  x.current_conditions.dewpoint = false;
  x.current_conditions.text = false;
  x.current_conditions.visibility = false;
  x.current_conditions.humidity = false;
  x.current_conditions.station = false;
  x.current_conditions.feels_like = false;
  x.current_conditions.moon_phase.text = false;
  x.current_conditions.uv.all = false;
  x.current_conditions.uv.index = false;
  x.current_conditions.uv.text = false;
  x.current_conditions.barometer.all = false;
  x.current_conditions.barometer.direction = false;
  x.current_conditions.barometer.reading = false;
  __init_wind (x.current_conditions.wind);
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
    __init_wind (x.forecasts[i].wind);
    x.forecasts[i].night.all = false;
    x.forecasts[i].night.text = false;
    x.forecasts[i].night.chance_precip = false;
    x.forecasts[i].night.humidity = false;
    __init_wind (x.forecasts[i].night.wind);
  }
#undef __init_wind
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
is_cc_uv_option (const char *opt)
{
  __is_option_func_body (opt, cc_uv_options);
}

static bool
is_cc_barometer_option (const char *opt)
{
  __is_option_func_body (opt, cc_barometer_options);
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
        is_cc_uv_option (v[i]) || is_cc_barometer_option (v[i]) ||
        is_loc_option (v[i]) || is_fc_option (v[i]) ||
        is_fc_day_option (v[i]) || is_fc_night_option (v[i]))
      continue;
    if (location)
      wet_die ("too many location arguments given");
    location = v[i];
    /* remove the location argument from the array */
    *c -= 1;
    for (j = i; v[j]; ++j)
      v[j] = v[j + 1];
  }

  if (!location)
    location = wet_getenv ("WET_LOCATION");

  if (!location || !*location)
    wet_die ("no location given and WET_LOCATION environment variable not "
             "set");
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
find_wanted_forecast_day (int *c, char **v)
{
  int day;
  size_t i;
  size_t j;

  day = -1;
  for (i = 2; v[2]; ++i) {
    if (wet_streq (v[i], "1") || wet_streqi (v[i], "today"))
      day = DAY_0;
    else if (wet_streq (v[i], "2") || wet_streqi (v[i], "tomorrow"))
      day = DAY_1;
    else if (wet_streq (v[i], "3"))
      day = DAY_2;
    else if (wet_streq (v[i], "4"))
      day = DAY_3;
    else if (wet_streq (v[i], "5"))
      day = DAY_4;
    if (day != -1) {
      *c -= 1;
      for (j = i; v[j]; ++j)
        v[j] = v[j + 1];
      break;
    }
  }
  return day;
}

static void
parse_opt (int c, char **v)
{
  int i;
  int day;

  program_name = v[0];

  find_wanted_location (&c, v);
  find_wanted_units (&c, v);

  if (c == 1) {
    if (!location) {
      usage (true);
      exit (EXIT_FAILURE);
    }
    default_display = true;
    return;
  }

  if (wet_streqi (v[1], "help")) {
    if (c > 4)
      wet_die ("too many arguments for `help'");
    else if (c == 4)
      help (v[2], v[3]);
    else if (c == 3)
      help (v[2], NULL);
    else
      help (NULL, NULL);
    exit (EXIT_SUCCESS);
  }

  if (wet_streqi (v[1], "version")) {
    if (c > 2)
      wet_die ("too many arguments for `version'");
    version ();
  }

  init_display_opts ();

  if (wet_streqi (v[1], "cc")) {
    if (!v[2]) {
      x.current_conditions.all = true;
      return;
    }
    if (wet_streqi (v[2], "last-updated"))
      x.current_conditions.last_updated = true;
    else if (wet_streqi (v[2], "temp"))
      x.current_conditions.temperature = true;
    else if (wet_streqi (v[2], "dewpoint"))
      x.current_conditions.dewpoint = true;
    else if (wet_streqi (v[2], "text"))
      x.current_conditions.text = true;
    else if (wet_streqi (v[2], "visibility"))
      x.current_conditions.visibility = true;
    else if (wet_streqi (v[2], "humidity"))
      x.current_conditions.humidity = true;
    else if (wet_streqi (v[2], "station"))
      x.current_conditions.station = true;
    else if (wet_streqi (v[2], "feels-like"))
      x.current_conditions.feels_like = true;
    else if (wet_streqi (v[2], "wind"))
      x.current_conditions.wind.all = true;
    else if (wet_streqi (v[2], "moon"))
      x.current_conditions.moon_phase.text = true;
    else if (wet_streqi (v[2], "uv")) {
      if (!v[3]) {
        x.current_conditions.uv.all = true;
        return;
      }
      if (wet_streqi (v[3], "index"))
        x.current_conditions.uv.index = true;
      else if (wet_streqi (v[3], "text"))
        x.current_conditions.uv.text = true;
      else
        wet_die ("unknown `cc uv' option -- `%s'", v[3]);
    } else if (wet_streqi (v[2], "barometer")) {
      if (!v[3]) {
        x.current_conditions.barometer.all = true;
        return;
      }
      if (wet_streqi (v[3], "direction"))
        x.current_conditions.barometer.direction = true;
      else if (wet_streqi (v[3], "reading"))
        x.current_conditions.barometer.reading = true;
      else
        wet_die ("unknown `cc barometer' option -- `%s'", v[3]);
    } else
      wet_die ("unknown `cc' option -- `%s'", v[2]);
    return;
  }

  if (wet_streqi (v[1], "loc")) {
    if (!v[2]) {
      x.location.all = true;
      return;
    }
    if (wet_streqi (v[2], "latitude"))
      x.location.lat = true;
    else if (wet_streqi (v[2], "longitude"))
      x.location.lon = true;
    else if (wet_streqi (v[2], "name"))
      x.location.name = true;
    else
      wet_die ("unknown `loc' option -- `%s'", v[2]);
    return;
  }

  if (wet_streqi (v[1], "fc")) {
    if (!v[2]) {
      for (i = 0; i < WET_FORECAST_DAYS; ++i)
        x.forecasts[i].all = true;
      return;
    }
    day = find_wanted_forecast_day (&c, v);
    for (i = 0; i < WET_FORECAST_DAYS; ++i) {
      if ((day == -1) || (i == day)) {
        if (!v[2])
          x.forecasts[day].all = true;
        else if (wet_streqi (v[2], "dow"))
          x.forecasts[i].day_of_week = true;
        else if (wet_streqi (v[2], "high"))
          x.forecasts[i].high = true;
        else if (wet_streqi (v[2], "low"))
          x.forecasts[i].low = true;
        else if (wet_streqi (v[2], "sunset"))
          x.forecasts[i].sunset = true;
        else if (wet_streqi (v[2], "sunrise"))
          x.forecasts[i].sunrise = true;
        else if (wet_streqi (v[2], "text"))
          x.forecasts[i].text = true;
        else if (wet_streqi (v[2], "cop"))
          x.forecasts[i].chance_precip = true;
        else if (wet_streqi (v[2], "humidity"))
          x.forecasts[i].humidity = true;
        else if (wet_streqi (v[2], "wind"))
          x.forecasts[i].wind.all = true;
        else if (wet_streqi (v[2], "night")) {
          if (!v[3])
            x.forecasts[i].night.all = true;
          else if (wet_streqi (v[3], "text"))
            x.forecasts[i].night.text = true;
          else if (wet_streqi (v[3], "cop"))
            x.forecasts[i].night.chance_precip = true;
          else if (wet_streqi (v[3], "humidity"))
            x.forecasts[i].night.humidity = true;
          else if (wet_streqi (v[3], "wind"))
            x.forecasts[i].night.wind.all = true;
          else
            wet_die ("unknown `fc night' option -- `%s'", v[3]);
        }
      }
    }
    return;
  }
}

static void
display (void)
{
  int day;

#define __display_wind(__w, __n) \
  do { \
    wet_puts ("%sº %s %s%s", \
              __w.direction, __w.text, __w.speed, w.units.speed); \
    if (!wet_streqi (__w.gust, "n/a")) \
      wet_puts (" (gusts %s%s)", __w.gust, w.units.speed); \
    if (__n) \
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
              w.units.temperature, w.forecasts[DAY_0].high,
              w.units.temperature, w.forecasts[DAY_0].low,
              w.units.temperature, w.current_conditions.visibility,
              w.units.distance, w.current_conditions.humidity,
              w.current_conditions.dewpoint, w.units.temperature,
              w.forecasts[DAY_0].sunrise, w.forecasts[DAY_0].sunset);
    __display_wind (w.current_conditions.wind, true);
    return;
  }

  if (x.current_conditions.all) {
    wet_puts ("Current Conditions (%s)\n"
              "-----------------------------\n"
              "Last Updated:        %s\n"
              "Temperature:         %sº%s\n"
              "Dew Point:           %sº%s\n"
              "Visibility:          %s%s\n"
              "Humidity:            %s%%\n"
              "Local Station:       %s\n"
              "Feels Like:          %sº%s\n"
              "Moon:                %s\n"
              "UV Index:            %s (%s)\n"
              "Barometric Pressure: %s%s (%s)\n"
              "Wind:                ",
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
    __display_wind (w.current_conditions.wind, true);
  } else if (x.location.all)
    wet_puts ("Location\n"
              "------------"
              "Name:      %s\n"
              "Latitude:  %s\n"
              "Longitude: %s\n",
              w.location.name,
              w.location.lat,
              w.location.lon);
  else if (x.current_conditions.last_updated)
    wet_puts (w.current_conditions.last_updated);
  else if (x.current_conditions.temperature)
    wet_puts ("%sº%s",
              w.current_conditions.temperature, w.units.temperature);
  else if (x.current_conditions.dewpoint)
    wet_puts ("%sº%s", w.current_conditions.dewpoint, w.units.temperature);
  else if (x.current_conditions.text)
    wet_puts (w.current_conditions.text);
  else if (x.current_conditions.visibility)
    wet_puts ("%s%s", w.current_conditions.visibility, w.units.distance);
  else if (x.current_conditions.humidity)
    wet_puts ("%s%%", w.current_conditions.humidity);
  else if (x.current_conditions.station)
    wet_puts (w.current_conditions.station);
  else if (x.current_conditions.feels_like)
    wet_puts ("%sº%s", w.current_conditions.feels_like, w.units.temperature);
  else if (x.current_conditions.wind.all)
    __display_wind (w.current_conditions.wind, false);
  else if (x.current_conditions.moon_phase.text)
    wet_puts (w.current_conditions.moon_phase.text);
  else if (x.current_conditions.uv.all)
    wet_puts ("%s %s",
              w.current_conditions.uv.index, w.current_conditions.uv.text);
  else if (x.current_conditions.uv.index)
    wet_puts (w.current_conditions.uv.index);
  else if (x.current_conditions.uv.text)
    wet_puts (w.current_conditions.uv.text);
  else if (x.current_conditions.barometer.all)
    wet_puts ("%s %s",
              w.current_conditions.barometer.direction,
              w.current_conditions.barometer.reading);
  else if (x.current_conditions.barometer.direction)
    wet_puts (w.current_conditions.barometer.direction);
  else if (x.current_conditions.barometer.reading)
    wet_puts (w.current_conditions.barometer.reading);
  else if (x.location.lat)
    wet_puts (w.location.lat);
  else if (x.location.lon)
    wet_puts (w.location.lon);
  else if (x.location.name)
    wet_puts (w.location.name);
  else {
    for (day = 0; day < WET_FORECAST_DAYS; ++day) {
      if (x.forecasts[day].all) {
        wet_puts ("Forecast for %s (%s)\n"
                  "----------------------\n"
                  "High                      %sº%s\n"
                  "Low                       %sº%s\n"
                  "Sunset                    %s\n"
                  "Sunrise                   %s\n"
                  "Chance of Precipitation:  %s%%\n"
                  "Humidity                  %s%%\n"
                  "Wind:                     ",
                  w.forecasts[day].day_of_week, w.forecasts[day].text,
                  w.forecasts[day].high, w.units.temperature,
                  w.forecasts[day].low, w.units.temperature,
                  w.forecasts[day].sunset,
                  w.forecasts[day].sunrise,
                  w.forecasts[day].chance_precip,
                  w.forecasts[day].humidity);
        __display_wind (w.forecasts[day].wind, true);
        wet_putc ('\n');
        wet_puts ("  %s Night (%s)\n"
                  "  -----------------------\n"
                  "  Chance of Precipitation:  %s%%\n"
                  "  Humidity:                 %s%%\n"
                  "  Wind:                     ",
                  w.forecasts[day].day_of_week, w.forecasts[day].night.text,
                  w.forecasts[day].night.chance_precip,
                  w.forecasts[day].night.humidity);
        __display_wind (w.forecasts[day].night.wind, true);
      } else if (x.forecasts[day].day_of_week)
        wet_puts (w.forecasts[day].day_of_week);
      else if (x.forecasts[day].high)
        wet_puts ("%sº%s", w.forecasts[day].high, w.units.temperature);
      else if (x.forecasts[day].low)
        wet_puts ("%sº%s", w.forecasts[day].low, w.units.temperature);
      else if (x.forecasts[day].sunset)
        wet_puts (w.forecasts[day].sunset);
      else if (x.forecasts[day].sunrise)
        wet_puts (w.forecasts[day].sunrise);
      else if (x.forecasts[day].text)
        wet_puts (w.forecasts[day].text);
      else if (x.forecasts[day].chance_precip)
        wet_puts ("%s%%", w.forecasts[day].chance_precip);
      else if (x.forecasts[day].humidity)
        wet_puts ("%s%%", w.forecasts[day].humidity);
      else if (x.forecasts[day].wind.all)
        __display_wind (w.forecasts[day].wind, false);
      else if (x.forecasts[day].night.all) {
        wet_puts ("Forecast for %s night (%s)\n"
                  "--------------------------\n"
                  "Chance of Precipitation: %s%%\n"
                  "Humidity:                %s%%\n"
                  "Wind:                    ",
                  w.forecasts[day].day_of_week,
                  w.forecasts[day].night.text,
                  w.forecasts[day].night.chance_precip,
                  w.forecasts[day].night.humidity);
        __display_wind (w.forecasts[day].night.wind, true);
      } else if (x.forecasts[day].night.text)
        wet_puts (w.forecasts[day].night.text);
      else if (x.forecasts[day].night.chance_precip)
        wet_puts ("%s%%", w.forecasts[day].night.chance_precip);
      else if (x.forecasts[day].night.humidity)
        wet_puts ("%s%%", w.forecasts[day].night.humidity);
      else if (x.forecasts[day].night.wind.all)
        __display_wind (w.forecasts[day].night.wind, false);
    }
  }

#undef __display_wind

  wet_putc ('\n');
}

int
main (int argc, char **argv)
{
  parse_opt (argc, argv);

  if (!wet_weather (&w, location, metric)) {
    if (*w.error.text)
      wet_die ("weather: %s", w.error.text);
    wet_die ("failed to retrieve weather data");
  }
  display ();
  exit (EXIT_SUCCESS);
  return 0; /* for compiler */
}

