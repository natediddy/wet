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

#include <string.h> /* memset() */

#include "wet.h"
#include "wet-net.h"
#include "wet-util.h"
#include "wet-weather.h"

static void
init_weather_struct (struct weather *w)
{
  int i;

  memset (w, 0, sizeof (struct weather));

#define __init_wind(__w)   \
  __w.gust[0] = '\0';      \
  __w.direction[0] = '\0'; \
  __w.speed[0] = '\0';     \
  __w.text[0] = '\0'

  w->location_id[0] = '\0';
  w->error.type[0] = '\0';
  w->error.text[0] = '\0';
  w->units.distance[0] = '\0';
  w->units.speed[0] = '\0';
  w->units.temperature[0] = '\0';
  w->units.rainfall[0] = '\0';
  w->units.pressure[0] = '\0';
  w->current_conditions.last_updated[0] = '\0';
  w->current_conditions.temperature[0] = '\0';
  w->current_conditions.dewpoint[0] = '\0';
  w->current_conditions.text[0] = '\0';
  w->current_conditions.visibility[0] = '\0';
  w->current_conditions.humidity[0] = '\0';
  w->current_conditions.station[0] = '\0';
  w->current_conditions.feels_like[0] = '\0';
  w->current_conditions.moon_phase.text[0] = '\0';
  w->current_conditions.uv.index[0] = '\0';
  w->current_conditions.uv.text[0] = '\0';
  w->current_conditions.barometer.direction[0] = '\0';
  w->current_conditions.barometer.reading[0] = '\0';
  __init_wind (w->current_conditions.wind);
  w->location.lat[0] = '\0';
  w->location.lon[0] = '\0';
  w->location.name[0] = '\0';

  for (i = 0; i < WET_FORECAST_DAYS; ++i) {
    w->forecasts[i].day_of_week[0] = '\0';
    w->forecasts[i].high[0] = '\0';
    w->forecasts[i].sunset[0] = '\0';
    w->forecasts[i].low[0] = '\0';
    w->forecasts[i].sunrise[0] = '\0';
    w->forecasts[i].text[0] = '\0';
    w->forecasts[i].chance_precip[0] = '\0';
    w->forecasts[i].humidity[0] = '\0';
    __init_wind (w->forecasts[i].wind);
    w->forecasts[i].night.text[0] = '\0';
    w->forecasts[i].night.chance_precip[0] = '\0';
    w->forecasts[i].night.humidity[0] = '\0';
    __init_wind (w->forecasts[i].night.wind);
  }
#undef __init_wind
}

bool
wet_weather (struct weather *w, const char *location, bool metric)
{
  init_weather_struct (w);
  wet_net_get_location_id (w, location);

  if (!*w->location_id)
    wet_die (WET_EWEATHER, "failed to find location '%s'", location);

  wet_net_get_weather_data (w, metric);
  if (*w->error.type || *w->error.text)
    return false;
  return true;
}

