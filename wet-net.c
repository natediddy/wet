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

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "wet.h"
#include "wet-net.h"
#include "wet-util.h"

#define PORT               80
#define HOST               "wxdata.weather.com"
#define GET                "GET %s HTTP/1.1\r\nHost: " HOST "\r\n\r\n"
#define WEATHER_DATA_PATH  "/wxdata/weather/local/%s?unit=%s&dayf=5&cc=*"
#define WEATHER_LOCID_PATH "/wxdata/search/search?where=%s"

#define HEADER_DELIMITER "\r\n\r\n"

#define HEADERMAX    1024
#define GETMAX        512
#define URLPATHMAX    256
#define STATUSTEXTMAX 128

#define DATA_UNKNOWN      "(not found)"
#define DATA_UNKNOWN_SIZE 12 /* includes null terminator */

struct headerdata {
  int status;
  size_t content_length;
  char status_text[STATUSTEXTMAX];
};

static const char *encode_chars = "!@#$%^&*()=+{}[]|\\;':\",<>/? ";
static char *content = NULL;

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

  p = strstr (content, "<error>");
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
  __find_and_assign (p, content, "<ut>", '<', w->units.temperature);
  __find_and_assign (p, content, "<ud>", '<', w->units.distance);
  __find_and_assign (p, content, "<us>", '<', w->units.speed);
  __find_and_assign (p, content, "<up>", '<', w->units.pressure);
  __find_and_assign (p, content, "<ur>", '<', w->units.rainfall);
  /* }}} units */



  use_unknown_string = true;
  /* location {{{ */
  p = strstr (content, "<loc id=");
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
  p = strstr (content, "<cc>");
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
  p = strstr (content, "<dayf>");
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
  __find_and_assign (p, content, "<loc id=\"", '"', w->location_id);
}

#undef __assign_unknown
#undef __assign
#undef __find_and_assign

static void
encode_string (char *buffer, size_t max, const char *str)
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

static void
retrieve_header (int sock, char *buffer, size_t n)
{
  bool stop;
  size_t pos;
  ssize_t n_read;

  stop = false;
  pos = 0;
  buffer[0] = '\0';

  while (!stop) {
    n_read = read (sock, buffer + pos, 1);
    if (n_read > 0) {
      pos += n_read;
      buffer[pos] = '\0';
      if (strstr (buffer, HEADER_DELIMITER))
        stop = true;
      continue;
    }
    break;
  }
}

static void
read_header (struct headerdata *hd, const char *header)
{
  size_t i;
  size_t j;
  char status_buffer[64];
  char content_length_buffer[64];
  char *p;

  hd->status = -1;
  hd->content_length = 0;
  hd->status_text[0] = '\0';

  status_buffer[0] = '\0';
  p = strstr (header, "HTTP/1.1 ");
  if (p && *p) {
    p = p + strlen ("HTTP/1.1 ");
    j = 0;
    for (i = 0; (p[i] && (p[i] != ' ')); ++i)
      status_buffer[j++] = p[i];
    status_buffer[j] = '\0';
    hd->status = wet_str2int (status_buffer);
    if (p[i] == ' ')
      ++i;
    j = 0;
    for (; (p[i] && (p[i] != '\r')); ++i)
      hd->status_text[j++] = p[i];
    hd->status_text[j] = '\0';
  }

  content_length_buffer[0] = '\0';
  p = strstr (header, "Content-Length:");
  if (p && *p) {
    p = p + strlen ("Content-Length:");
    while (*p && (*p == ' '))
      p++;
    j = 0;
    for (i = 0; (p[i] && (p[i] != '\r')); ++i)
      content_length_buffer[j++] = p[i];
    content_length_buffer[j] = '\0';
    hd->content_length = wet_str2size_t (content_length_buffer);
  }
}

static void
retrieve_content (int sock, ssize_t n)
{
  bool stop;
  size_t pos;
  ssize_t n_read;

  stop = false;
  pos = 0;
  content[0] = '\0';

  while (!stop) {
    n_read = read (sock, content + pos, 1);
    if (n_read > 0) {
      pos += n_read;
      content[pos] = '\0';
      if (pos >= n)
        stop = true;
      continue;
    }
    break;
  }
}

static void
http_get_request (const char *path)
{
  int sock;
  long n_haddr;
  ssize_t n_write;
  char get[GETMAX];
  char header[HEADERMAX];
  struct sockaddr_in a;
  struct headerdata hd;
  struct hostent *h;

  sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == -1)
    wet_die ("failed to create socket: %s", strerror (errno));

  h = gethostbyname (HOST);
  if (!h) {
    close (sock);
    wet_die ("failed to get host information");
  }

  memcpy (&n_haddr, h->h_addr, h->h_length);
  a.sin_addr.s_addr = n_haddr;
  a.sin_port = htons (PORT);
  a.sin_family = AF_INET;

  wet_debug ("connecting to: \"%s%s\"", HOST, path);
  if (connect (sock, (struct sockaddr *) &a, sizeof (a)) == -1) {
    close (sock);
    wet_die ("failed to connect socket: %s", strerror (errno));
  }

  snprintf (get, GETMAX, GET, path);

  n_write = write (sock, get, strlen (get));
  if (n_write < 0) {
    close (sock);
    wet_die ("failed to send GET request: %s", strerror (errno));
  }

  retrieve_header (sock, header, HEADERMAX);
  memset (&hd, 0, sizeof (struct headerdata));
  read_header (&hd, header);

  wet_debug ("http status: %i (%s)", hd.status, hd.status_text);
  if (hd.status != 200) {
    close (sock);
    wet_die ("http: %i (%s)", hd.status, hd.status_text);
  }

  if (content) {
    free (content);
    content = NULL;
  }

  content = (char *) malloc (hd.content_length + 1);
  if (!content) {
    close (sock);
    wet_die ("failed to allocate memory: %s", strerror (errno));
  }

  retrieve_content (sock, hd.content_length);
  close (sock);
}

static void
cleanup (void)
{
  if (content) {
    free (content);
    content = NULL;
  }
}

void
wet_net_get_weather_data (struct weather *w, bool metric)
{
  char path[URLPATHMAX];

  snprintf (path, URLPATHMAX, WEATHER_DATA_PATH,
            w->location_id, (!metric) ? "" : "m");
  http_get_request (path);
  fill_weather_struct (w);
}

void
wet_net_get_location_id (struct weather *w, const char *query)
{
  size_t n;
  char path[URLPATHMAX];

  /* Make equery extra extra large just in case.
     wet_net_encode_string() does not do any safety checks... */
  n = strlen (query) * 10;
  char equery[n];

  atexit (cleanup);
  encode_string (equery, n, query);
  snprintf (path, URLPATHMAX, WEATHER_LOCID_PATH, equery);
  http_get_request (path);
  fill_location_id (w);
}

