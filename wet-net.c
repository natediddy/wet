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

#include <errno.h> /* errno */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* malloc(), realloc(), free() */
#include <string.h> /* strlen(), strncpy(), strerror(), strstr(), strchr() */

#include <curl/curl.h> /* curl_*, CURL* */

#include "wet.h"
#include "wet-net.h"
#include "wet-util.h"

#define USERAGENT "WET (WEather Tool) / " WET_VERSION

#define WEATHER_DATA_URL \
  "http://wxdata.weather.com/wxdata/weather/local/%s?unit=%s&dayf=5&cc=*"

#define WEATHER_LOCATION_SEARCH_URL \
  "http://wxdata.weather.com/wxdata/search/search?where=%s"

#define DATA_UNKNOWN      "(not found)"
#define DATA_UNKNOWN_SIZE 12 /* includes null terminator */

#define URL_MAX 256

#define DIE_IF_ERROR          \
  if (curlstatus != CURLE_OK) \
    wet_die ("libcurl: %s", curl_easy_strerror (curlstatus))

struct writedata {
  char *buffer;
  size_t n;
};

static const char *encode_chars = "!@#$%^&*()=+{}[]|\\;':\",<>/? ";

static struct writedata data = {NULL, 0};
static CURLcode curlstatus   = CURLE_OK;
static CURL *curlptr         = NULL;

#define __assign_unknown(__r) strncpy (__r, DATA_UNKNOWN, DATA_UNKNOWN_SIZE)

#if 0
#define __assign(__p, __n, __c, __r) \
  do { \
    size_t __i = 0; \
    for (__p = __p + __n; (*__p && (*__p != __c)); ++p) \
      __r[__i++] = *__p; \
    __r[__i] = '\0'; \
  } while (0)

#define __find_and_assign(__p, __s, __pt, __c, __r) \
  do { \
    __p = strstr (__s, __pt); \
    if (__p && *__p) \
      __assign (__p, strlen (__pt), __c, __r); \
    else if (use_unknown_string) \
      __assign_unknown (__r); \
  } while (0)
#endif

#define __assign(__p, __n, __c, __r) \
  do { \
    size_t __i; \
    size_t __j = 0; \
    __p = __p + __n; \
    for (__i = 0; __p[__i] != __c; ++__i) \
      __r[__j++] = __p[__i]; \
    __r[__j] = '\0'; \
  } while (0)

#define __find_and_assign(__p, __s, __pt, __c, __r) \
  do { \
    __p = strstr (__s, __pt); \
    if (__p && *__p) { \
      size_t __i; \
      size_t __j = 0; \
      __p = __p + strlen (__pt); \
      for (__i = 0; __p[__i] != __c; ++__i) \
        __r[__j++] = __p[__i]; \
      __r[__j] = '\0'; \
    } else if (use_unknown_string) \
      __assign_unknown (__r); \
  } while (0)

static void
fill_weather_struct (struct weather *w)
{
  int day;
  char *p;
  char *t0;
  char *t1;
  char *t2;
  char *t3;
  bool use_unknown_string;

  p = strstr (data.buffer, "<error>");
  if (p && *p) {
    p = p + strlen ("<error>");
    t0 = strstr (p, "<err type=\"");
    if (t0 && *t0) {
      __assign (t0, strlen ("<err type=\""), '"', w->error.type);
      t1 = strchr (t0, '>');
      if (t1 && *t1)
        __assign (t1, 1, '<', w->error.text);
    }
  }

  if (*w->error.type && *w->error.text)
    return;

  use_unknown_string = false;
  /* units {{{ */
  __find_and_assign (p, data.buffer, "<ut>", '<', w->units.temperature);
  __find_and_assign (p, data.buffer, "<ud>", '<', w->units.distance);
  __find_and_assign (p, data.buffer, "<us>", '<', w->units.speed);
  __find_and_assign (p, data.buffer, "<up>", '<', w->units.pressure);
  __find_and_assign (p, data.buffer, "<ur>", '<', w->units.rainfall);
  /* }}} units */



  use_unknown_string = true;
  /* location {{{ */
  p = strstr (data.buffer, "<loc id=");
  if (p && *p) {
    __find_and_assign (t0, p, "<dnam>", '<', w->location.name);
    __find_and_assign (t0, p, "<lat>", '<', w->location.lat);
    __find_and_assign (t0, p, "<lon>", '<', w->location.lon);
  } else {
    __assign_unknown (w->location.name);
    __assign_unknown (w->location.lat);
    __assign_unknown (w->location.lon);
  }
  /* }}} location */



  /* current_conditions {{{ */
  p = strstr (data.buffer, "<cc>");
  if (p && *p) {
    __find_and_assign (t0, p, "<lsup>", '<',
                       w->current_conditions.last_updated);
    __find_and_assign (t0, p, "<tmp>", '<',
                       w->current_conditions.temperature);
    __find_and_assign (t0, p, "<dewp>", '<', w->current_conditions.dewpoint);
    __find_and_assign (t0, p, "<t>", '<', w->current_conditions.text);
    __find_and_assign (t0, p, "<vis>", '<', w->current_conditions.visibility);
    __find_and_assign (t0, p, "<hmid>", '<', w->current_conditions.humidity);
    __find_and_assign (t0, p, "<obst>", '<', w->current_conditions.station);
    __find_and_assign (t0, p, "<flik>", '<',
                       w->current_conditions.feels_like);
    t0 = strstr (p, "<moon>");
    if (t0 && *t0)
      __find_and_assign (t1, t0, "<t>", '<',
                         w->current_conditions.moon_phase.text);
    else
      __assign_unknown (w->current_conditions.moon_phase.text);
    t0 = strstr (p, "<uv>");
    if (t0 && *t0) {
      __find_and_assign (t1, t0, "<i>", '<', w->current_conditions.uv.index);
      __find_and_assign (t1, t0, "<t>", '<', w->current_conditions.uv.text);
    } else {
      __assign_unknown (w->current_conditions.uv.index);
      __assign_unknown (w->current_conditions.uv.text);
    }
    t0 = strstr (p, "<bar>");
    if (t0 && *t0) {
      __find_and_assign (t1, t0, "<d>", '<',
                         w->current_conditions.barometer.direction);
      __find_and_assign (t1, t0, "<r>", '<',
                         w->current_conditions.barometer.reading);
    } else {
      __assign_unknown (w->current_conditions.barometer.direction);
      __assign_unknown (w->current_conditions.barometer.reading);
    }
    t0 = strstr (p, "<wind>");
    if (t0 && *t0) {
      __find_and_assign (t1, t0, "<gust>", '<',
                         w->current_conditions.wind.gust);
      __find_and_assign (t1, t0, "<d>", '<',
                         w->current_conditions.wind.direction);
      __find_and_assign (t1, t0, "<s>", '<',
                         w->current_conditions.wind.speed);
      __find_and_assign (t1, t0, "<t>", '<', w->current_conditions.wind.text);
    } else {
      __assign_unknown (w->current_conditions.wind.gust);
      __assign_unknown (w->current_conditions.wind.direction);
      __assign_unknown (w->current_conditions.wind.speed);
      __assign_unknown (w->current_conditions.wind.text);
    }
  } else {
    __assign_unknown (w->current_conditions.last_updated);
    __assign_unknown (w->current_conditions.temperature);
    __assign_unknown (w->current_conditions.dewpoint);
    __assign_unknown (w->current_conditions.text);
    __assign_unknown (w->current_conditions.visibility);
    __assign_unknown (w->current_conditions.humidity);
    __assign_unknown (w->current_conditions.station);
    __assign_unknown (w->current_conditions.feels_like);
    __assign_unknown (w->current_conditions.moon_phase.text);
    __assign_unknown (w->current_conditions.uv.index);
    __assign_unknown (w->current_conditions.uv.text);
    __assign_unknown (w->current_conditions.barometer.direction);
    __assign_unknown (w->current_conditions.barometer.reading);
    __assign_unknown (w->current_conditions.wind.gust);
    __assign_unknown (w->current_conditions.wind.direction);
    __assign_unknown (w->current_conditions.wind.speed);
    __assign_unknown (w->current_conditions.wind.text);
  }
  /* }}} current_conditions */



  /* forecasts {{{ */
  p = strstr (data.buffer, "<dayf>");
  if (p && *p) {
    for (day = 0; day < WET_FORECAST_DAYS; ++day) {
      t0 = strstr (p, "<day d=");
      if (t0 && *t0) {
        __find_and_assign (t1, t0, "t=\"", '"',
                           w->forecasts[day].day_of_week);
        __find_and_assign (t1, t0, "<hi>", '<', w->forecasts[day].high);
        __find_and_assign (t1, t0, "<suns>", '<', w->forecasts[day].sunset);
        __find_and_assign (t1, t0, "<low>", '<', w->forecasts[day].low);
        __find_and_assign (t1, t0, "<sunr>", '<', w->forecasts[day].sunrise);
        t1 = strstr (t0, "<part p=\"d\">");
        if (t1 && *t1) {
          t2 = strstr (t1, "<wind>");
          if (t2 && *t2) {
            __find_and_assign (t3, t2, "<s>", '<',
                               w->forecasts[day].wind.speed);
            __find_and_assign (t3, t2, "<gust>", '<',
                               w->forecasts[day].wind.gust);
            __find_and_assign (t3, t2, "<d>", '<',
                               w->forecasts[day].wind.direction);
            __find_and_assign (t3, t2, "<t>", '<',
                               w->forecasts[day].wind.text);
          } else {
            __assign_unknown (w->forecasts[day].wind.gust);
            __assign_unknown (w->forecasts[day].wind.direction);
            __assign_unknown (w->forecasts[day].wind.speed);
            __assign_unknown (w->forecasts[day].wind.text);
          }
          __find_and_assign (t2, t1, "<t>", '<', w->forecasts[day].text);
          __find_and_assign (t2, t1, "<ppcp>", '<',
                             w->forecasts[day].chance_precip);
          __find_and_assign (t2, t1, "<hmid>", '<',
                             w->forecasts[day].humidity);
        } else {
          __assign_unknown (w->forecasts[day].text);
          __assign_unknown (w->forecasts[day].chance_precip);
          __assign_unknown (w->forecasts[day].humidity);
          __assign_unknown (w->forecasts[day].wind.gust);
          __assign_unknown (w->forecasts[day].wind.direction);
          __assign_unknown (w->forecasts[day].wind.speed);
          __assign_unknown (w->forecasts[day].wind.text);
        }
        t1 = strstr (t0, "<part p=\"n\">");
        if (t1 && *t1) {
          t2 = strstr (t1, "<wind>");
          if (t2 && *t2) {
            __find_and_assign (t3, t2, "<s>", '<',
                               w->forecasts[day].night.wind.speed);
            __find_and_assign (t3, t2, "<gust>", '<',
                               w->forecasts[day].night.wind.gust);
            __find_and_assign (t3, t2, "<d>", '<',
                               w->forecasts[day].night.wind.direction);
            __find_and_assign (t3, t2, "<t>", '<',
                               w->forecasts[day].night.wind.text);
          } else {
            __assign_unknown (w->forecasts[day].night.wind.gust);
            __assign_unknown (w->forecasts[day].night.wind.direction);
            __assign_unknown (w->forecasts[day].night.wind.speed);
            __assign_unknown (w->forecasts[day].night.wind.text);
          }
          __find_and_assign (t2, t1, "<t>", '<',
                             w->forecasts[day].night.text);
          __find_and_assign (t2, t1, "<ppcp>", '<',
                             w->forecasts[day].night.chance_precip);
          __find_and_assign (t2, t1, "<hmid>", '<',
                             w->forecasts[day].night.humidity);
        } else {
          __assign_unknown (w->forecasts[day].night.text);
          __assign_unknown (w->forecasts[day].night.chance_precip);
          __assign_unknown (w->forecasts[day].night.humidity);
          __assign_unknown (w->forecasts[day].night.wind.gust);
          __assign_unknown (w->forecasts[day].night.wind.direction);
          __assign_unknown (w->forecasts[day].night.wind.speed);
          __assign_unknown (w->forecasts[day].night.wind.text);
        }
      } else {
        __assign_unknown (w->forecasts[day].day_of_week);
        __assign_unknown (w->forecasts[day].high);
        __assign_unknown (w->forecasts[day].sunset);
        __assign_unknown (w->forecasts[day].low);
        __assign_unknown (w->forecasts[day].sunrise);
      }
      t1 = strstr (t0, "</day>");
      if (t1 && *t1) {
        p = t1;
        continue;
      }
      break;
    }
  } else {
    for (day = 0; day < WET_FORECAST_DAYS; ++day) {
      __assign_unknown (w->forecasts[day].day_of_week);
      __assign_unknown (w->forecasts[day].high);
      __assign_unknown (w->forecasts[day].sunset);
      __assign_unknown (w->forecasts[day].low);
      __assign_unknown (w->forecasts[day].sunrise);
      __assign_unknown (w->forecasts[day].text);
      __assign_unknown (w->forecasts[day].chance_precip);
      __assign_unknown (w->forecasts[day].humidity);
      __assign_unknown (w->forecasts[day].wind.gust);
      __assign_unknown (w->forecasts[day].wind.direction);
      __assign_unknown (w->forecasts[day].wind.speed);
      __assign_unknown (w->forecasts[day].wind.text);
      __assign_unknown (w->forecasts[day].night.text);
      __assign_unknown (w->forecasts[day].night.chance_precip);
      __assign_unknown (w->forecasts[day].night.humidity);
      __assign_unknown (w->forecasts[day].night.wind.gust);
      __assign_unknown (w->forecasts[day].night.wind.direction);
      __assign_unknown (w->forecasts[day].night.wind.speed);
      __assign_unknown (w->forecasts[day].night.wind.text);
    }
  }
  /* }}} forecasts */
}

static void
fill_location_id (struct weather *w)
{
  bool use_unknown_string;
  char *p;

  use_unknown_string = false;
  __find_and_assign (p, data.buffer, "<loc id=\"", '"', w->location_id);
}

#undef __assign_unknown
#undef __assign
#undef __find_and_assign

static size_t
writefunction (void *buf, size_t size, size_t nmemb, void *data)
{
  size_t n;
  struct writedata *p;

  p = (struct writedata *) data;
  n = size * nmemb;

  p->buffer = (char *) realloc (p->buffer, p->n + n + 1);
  if (!p->buffer)
    return 0;

  memcpy (&(p->buffer[p->n]), buf, n);
  p->n += n;
  p->buffer[p->n] = 0;
  return n;
}

static void
writedata_reset (void)
{
  if (data.buffer) {
    free (data.buffer);
    data.buffer = NULL;
  }

  data.buffer = (char *) malloc (sizeof (char));
  if (!data.buffer)
    wet_die (strerror (errno));
  data.n = 0;
}

static void
net_init (const char *url)
{
  curlptr = curl_easy_init ();
  if (!curlptr) {
    curlstatus = CURLE_FAILED_INIT;
    DIE_IF_ERROR;
  }

  curlstatus = curl_easy_setopt (curlptr, CURLOPT_URL, url);
  DIE_IF_ERROR;

  curlstatus = curl_easy_setopt (curlptr, CURLOPT_USERAGENT, USERAGENT);
  DIE_IF_ERROR;

  curlstatus = curl_easy_setopt (curlptr, CURLOPT_FOLLOWLOCATION, 1L);
  DIE_IF_ERROR;

  writedata_reset ();
  curlstatus = curl_easy_setopt (curlptr, CURLOPT_WRITEDATA, (void *) &data);
  DIE_IF_ERROR;

  curlstatus =
    curl_easy_setopt (curlptr,
                      CURLOPT_WRITEFUNCTION,
                      (void *) &writefunction);
  DIE_IF_ERROR;
}

static void
net_finish (void)
{
  if (curlptr)
    curl_easy_cleanup (curlptr);

  if (data.buffer)
    free (data.buffer);

  curlptr = NULL;
  data.buffer = NULL;

  curlstatus = CURLE_OK;
  data.n = 0;
}

static void
net_encode_string (char *buffer, size_t max, const char *str)
{
  size_t n;
  size_t nstr;
  size_t pos;
  bool ch_enc;
  const char *e;
  const char *s;

  nstr = strlen (str);
  n = nstr;

  for (s = str; *s; ++s) {
    for (e = encode_chars; *e; ++e) {
      if (*s == *e) {
        n += 2;
        break;
      }
    }
  }

  if (n == nstr) {
    strncpy (buffer, str, n);
    buffer[n] = '\0';
    return;
  }

  pos = 0;
  for (s = str; *s; ++s) {
    ch_enc = false;
    for (e = encode_chars; *e; ++e) {
      if (*s == *e) {
        snprintf (buffer + pos, 4, "%%%X", *e);
        pos += 3;
        ch_enc = true;
        break;
      }
    }
    if (!ch_enc)
      buffer[pos++] = *s;
  }
  buffer[pos] = '\0';
}

void
wet_net_get_weather_data (struct weather *w, bool metric)
{
  char url[URL_MAX];

  snprintf (url, URL_MAX, WEATHER_DATA_URL,
            w->location_id, (!metric) ? "" : "m");
  net_init (url);
  curlstatus = curl_easy_perform (curlptr);
  DIE_IF_ERROR;
  fill_weather_struct (w);
  net_finish ();
}

void
wet_net_get_location_id (struct weather *w, const char *query)
{
  size_t n;
  size_t ne;
  char url[URL_MAX];

  n = strlen (query);
  /* Make equery extra extra large just in case.
     wet_net_encode_string() does not do any safety checks... */
  ne = n * 10;
  char equery[ne];

  net_encode_string (equery, ne, query);
  snprintf (url, URL_MAX, WEATHER_LOCATION_SEARCH_URL, equery);

  net_init (url);
  curlstatus = curl_easy_perform (curlptr);
  DIE_IF_ERROR;
  fill_location_id (w);
  net_finish ();
}

