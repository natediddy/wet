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

#ifndef WET_WEATHER_H
#define WET_WEATHER_H

#include "wet.h"

#define WET_FORECAST_DAYS 5
#define WET_DATA_MAX   1024

struct __wind {
  char gust[WET_DATA_MAX];
  char direction[WET_DATA_MAX];
  char speed[WET_DATA_MAX];
  char text[WET_DATA_MAX];
};

struct weather {
  char location_id[WET_DATA_MAX];

  struct {
    char type[WET_DATA_MAX];
    char text[WET_DATA_MAX];
  } error;

  struct {
    char distance[WET_DATA_MAX];
    char speed[WET_DATA_MAX];
    char temperature[WET_DATA_MAX];
    char rainfall[WET_DATA_MAX];
    char pressure[WET_DATA_MAX];
  } units;

  struct {
    char text[WET_DATA_MAX];
    char link[WET_DATA_MAX];
  } severe_weather_alert;

  struct {
    char last_updated[WET_DATA_MAX];
    char temperature[WET_DATA_MAX];
    char dewpoint[WET_DATA_MAX];
    char text[WET_DATA_MAX];
    char visibility[WET_DATA_MAX];
    char humidity[WET_DATA_MAX];
    char station[WET_DATA_MAX];
    char feels_like[WET_DATA_MAX];
    struct __wind wind;

    struct {
      char text[WET_DATA_MAX];
    } moon_phase;

    struct {
      char index[WET_DATA_MAX];
      char text[WET_DATA_MAX];
    } uv;

    struct {
      char direction[WET_DATA_MAX];
      char reading[WET_DATA_MAX];
    } barometer;
  } current_conditions;

  struct {
    char lat[WET_DATA_MAX];
    char lon[WET_DATA_MAX];
    char name[WET_DATA_MAX];
  } location;

  struct {
    char day_of_week[WET_DATA_MAX];
    char high[WET_DATA_MAX];
    char sunset[WET_DATA_MAX];
    char low[WET_DATA_MAX];
    char sunrise[WET_DATA_MAX];
    char text[WET_DATA_MAX];
    char chance_precip[WET_DATA_MAX];
    char humidity[WET_DATA_MAX];
    struct __wind wind;

    struct {
      char text[WET_DATA_MAX];
      char chance_precip[WET_DATA_MAX];
      char humidity[WET_DATA_MAX];
      struct __wind wind;
    } night;
  } forecasts[WET_FORECAST_DAYS];
};

bool wet_weather (struct weather *, const char *, bool);

#endif /* WET_WEATHER_H */

