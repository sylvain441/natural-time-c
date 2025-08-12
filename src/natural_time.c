#include "natural_time.h"
#include <math.h>
#include <time.h>
#include <string.h>
#include "astronomy.h"  // vendor/astronomy include path wired from CMake

// Constants
static const int64_t MS_PER_DAY = 86400000LL;
static const int64_t END_OF_ARTIFICIAL_TIME = 1356091200000LL; // 2012-12-21T12:00:00Z

// Simple caches (not thread-safe). These speed up repeated queries that
// occur frequently in UI loops without meaningfully changing results.
typedef struct {
  int valid;
  int year;
  astro_seasons_t seasons;
} seasons_cache_t;
static seasons_cache_t g_seasons_cache_1 = {0};
static seasons_cache_t g_seasons_cache_2 = {0};

typedef struct {
  int valid;
  int64_t nadir;
  double latitude;
  double longitude;
  nt_sun_events value;
} sun_events_cache_t;
static sun_events_cache_t g_sun_events_cache = {0};

typedef struct {
  int valid;
  int year;
  double latitude;
  nt_mustaches value;
} moustaches_cache_t;
static moustaches_cache_t g_moustaches_cache = {0};

// 💡 timegm converts a UTC struct tm to Unix seconds since epoch.
// It exists on macOS; provide a fallback shim if needed.
static int64_t to_unix_ms_utc(int y, int m, int d, int hh, int mm, int ss, int ms) {
  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = y - 1900;
  tmv.tm_mon  = m - 1;
  tmv.tm_mday = d;
  tmv.tm_hour = hh;
  tmv.tm_min  = mm;
  tmv.tm_sec  = ss;
#if defined(_BSD_SOURCE) || defined(__APPLE__) || defined(__GNU_LIBRARY__)
  time_t secs = timegm(&tmv);
#else
  // Fallback: temporarily set TZ to UTC
  char *old_tz = getenv("TZ");
  setenv("TZ", "UTC", 1);
  tzset();
  time_t secs = mktime(&tmv);
  if (old_tz) { setenv("TZ", old_tz, 1); } else { unsetenv("TZ"); }
  tzset();
#endif
  return (int64_t)secs * 1000LL + (int64_t)ms;
}

static astro_time_t astro_time_from_unix_ms(int64_t unix_ms) {
  time_t secs = (time_t)(unix_ms / 1000LL);
  int ms = (int)(unix_ms % 1000LL);
  if (ms < 0) { ms += 1000; secs -= 1; }
  struct tm *gmt = gmtime(&secs);
  astro_utc_t utc;
  utc.year = gmt->tm_year + 1900;
  utc.month = gmt->tm_mon + 1;
  utc.day = gmt->tm_mday;
  utc.hour = gmt->tm_hour;
  utc.minute = gmt->tm_min;
  utc.second = (double)gmt->tm_sec + (ms / 1000.0);
  return Astronomy_TimeFromUtc(utc);
}

static astro_seasons_t seasons_for_year(int year) {
  // Two-entry cache with trivial eviction.
  if (g_seasons_cache_1.valid && g_seasons_cache_1.year == year) {
    return g_seasons_cache_1.seasons;
  }
  if (g_seasons_cache_2.valid && g_seasons_cache_2.year == year) {
    return g_seasons_cache_2.seasons;
  }
  astro_seasons_t s = Astronomy_Seasons(year);
  // Evict the older cache slot.
  g_seasons_cache_2 = g_seasons_cache_1;
  g_seasons_cache_1.valid = 1;
  g_seasons_cache_1.year = year;
  g_seasons_cache_1.seasons = s;
  return s;
}

static void add_days_to_utc(int *y, int *m, int *d, int days) {
  // Normalize by constructing a tm and letting timegm handle rollovers
  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = *y - 1900;
  tmv.tm_mon  = *m - 1;
  tmv.tm_mday = *d + days;
  tmv.tm_hour = 12; // keep midday to avoid DST issues in local conversions
#if defined(_BSD_SOURCE) || defined(__APPLE__) || defined(__GNU_LIBRARY__)
  time_t secs = timegm(&tmv);
#else
  char *old_tz = getenv("TZ");
  setenv("TZ", "UTC", 1);
  tzset();
  time_t secs = mktime(&tmv);
  if (old_tz) { setenv("TZ", old_tz, 1); } else { unsetenv("TZ"); }
  tzset();
#endif
  struct tm *gmt = gmtime(&secs);
  *y = gmt->tm_year + 1900;
  *m = gmt->tm_mon + 1;
  *d = gmt->tm_mday;
}

static int64_t calculate_year_start_ms(int artificial_year, double longitude_deg, int *out_duration_days) {
  // Get December solstices for Y and Y+1 (cached)
  astro_seasons_t s0 = seasons_for_year(artificial_year);
  astro_seasons_t s1 = seasons_for_year(artificial_year + 1);
  astro_utc_t u0 = Astronomy_UtcFromTime(s0.dec_solstice);
  astro_utc_t u1 = Astronomy_UtcFromTime(s1.dec_solstice);

  // Start new year: 12:00 UTC on solstice date; if solstice hour >= 12, use next day at 12:00
  int y0 = u0.year, m0 = u0.month, d0 = u0.day;
  double sol0_hour = u0.hour + (u0.minute/60.0) + (u0.second/3600.0);
  if (sol0_hour >= 12.0) add_days_to_utc(&y0, &m0, &d0, 1);
  int64_t startNewYear = to_unix_ms_utc(y0, m0, d0, 12, 0, 0, 0);

  int y1 = u1.year, m1 = u1.month, d1 = u1.day;
  double sol1_hour = u1.hour + (u1.minute/60.0) + (u1.second/3600.0);
  if (sol1_hour >= 12.0) add_days_to_utc(&y1, &m1, &d1, 1);
  int64_t endNewYear = to_unix_ms_utc(y1, m1, d1, 12, 0, 0, 0);

  int duration = (int)((endNewYear - startNewYear) / MS_PER_DAY);
  if (out_duration_days) *out_duration_days = duration; // 365 or 366

  // Localize by longitude (💡 time distance): shift by (-lon + 180) days/360
  double shift_ms = (-longitude_deg + 180.0) * (double)MS_PER_DAY / 360.0;
  return (int64_t)llround((double)startNewYear + shift_ms);
}

static int utc_year_from_unix_ms(int64_t unix_ms) {
  time_t secs = (time_t)(unix_ms / 1000LL);
  struct tm *gmt = gmtime(&secs);
  return gmt->tm_year + 1900;
}

void nt_reset_caches(void) {
  Astronomy_Reset();
  // Invalidate local caches
  g_seasons_cache_1.valid = 0; g_seasons_cache_2.valid = 0;
  g_sun_events_cache.valid = 0;
  g_moustaches_cache.valid = 0;
}

nt_err nt_make_natural_date(int64_t unix_ms_utc, double longitude_deg, nt_natural_date* out) {
  if (!out) return NT_ERR_INTERNAL;
  if (!(longitude_deg >= -180.0 && longitude_deg <= 180.0)) return NT_ERR_RANGE;
  if (unix_ms_utc <= 0) return NT_ERR_TIME;

  // Establish year context using the same two-step approach as JS (Y-1 then maybe Y).
  int utc_year = utc_year_from_unix_ms(unix_ms_utc);
  int duration_days = 365;
  int64_t year_start_ms = calculate_year_start_ms(utc_year - 1, longitude_deg, &duration_days);
  if (unix_ms_utc - year_start_ms >= (int64_t)duration_days * MS_PER_DAY) {
    year_start_ms = calculate_year_start_ms(utc_year, longitude_deg, &duration_days);
  }

  double time_since_year_start_days = (double)(unix_ms_utc - year_start_ms) / (double)MS_PER_DAY;

  // Populate fields mirroring JS logic
  out->unix_time = unix_ms_utc;
  out->longitude = longitude_deg;
  out->year_start = year_start_ms;
  out->year_duration = duration_days;

  int year_start_utc_year = utc_year_from_unix_ms(year_start_ms);
  int eat_year = utc_year_from_unix_ms(END_OF_ARTIFICIAL_TIME);
  out->year = year_start_utc_year - eat_year + 1;

  out->moon = (int32_t)floor(time_since_year_start_days / 28.0) + 1;
  out->week = (int32_t)floor(time_since_year_start_days / 7.0) + 1;
  out->week_of_moon = (int32_t)fmod(floor(time_since_year_start_days / 7.0), 4.0) + 1;

  int64_t eat_local = END_OF_ARTIFICIAL_TIME + (int64_t)((-longitude_deg + 180.0) * (double)MS_PER_DAY / 360.0);
  out->day = (int32_t)floor((double)(unix_ms_utc - eat_local) / (double)MS_PER_DAY);
  out->day_of_year = (int32_t)floor(time_since_year_start_days) + 1;
  out->day_of_moon = (int32_t)fmod(floor(time_since_year_start_days), 28.0) + 1;
  out->day_of_week = (int32_t)fmod(floor(time_since_year_start_days), 7.0) + 1;

  out->nadir = year_start_ms + (int64_t)floor(time_since_year_start_days) * MS_PER_DAY;
  out->time_deg = ((double)(unix_ms_utc - out->nadir)) * 360.0 / (double)MS_PER_DAY;
  if (out->time_deg >= 360.0) out->time_deg = 0.0; // formatting wrap safeguard

  out->is_rainbow_day = (out->day_of_year > 13 * 28) ? 1 : 0;

  return NT_OK;
}

nt_err nt_get_time_of_event(const nt_natural_date* nd, int64_t event_unix_ms_utc, double* out_deg_or_nan) {
  if (!nd || !out_deg_or_nan) return NT_ERR_INTERNAL;
  // Convert an event timestamp to natural degrees within current natural day; return 0 if out of range.
  if (event_unix_ms_utc < nd->nadir || event_unix_ms_utc > nd->nadir + MS_PER_DAY) {
    *out_deg_or_nan = 0.0;
    return NT_OK;
  }
  *out_deg_or_nan = ((double)(event_unix_ms_utc - nd->nadir)) * 360.0 / (double)MS_PER_DAY;
  if (*out_deg_or_nan >= 360.0) *out_deg_or_nan = 0.0;
  return NT_OK;
}

static int is_summer_season(int day_of_year, double latitude_deg) {
  int north = (latitude_deg >= 0.0) ? 1 : 0;
  const int SUMMER_START_DAY = 91;
  const int SUMMER_END_DAY = 273;
  if (north) {
    return (day_of_year >= SUMMER_START_DAY && day_of_year <= SUMMER_END_DAY) ? 1 : 0;
  } else {
    return (day_of_year <= SUMMER_START_DAY || day_of_year >= SUMMER_END_DAY) ? 1 : 0;
  }
}

static double event_time_or_default(const nt_natural_date* nd, astro_search_result_t res, int is_summer, int is_summer_default) {
  if (res.status != ASTRO_SUCCESS) {
    if (is_summer) {
      return is_summer_default ? 360.0 : 0.0;
    } else {
      return 180.0;
    }
  }
  astro_utc_t u = Astronomy_UtcFromTime(res.time);
  int64_t ms = to_unix_ms_utc(u.year, u.month, u.day, u.hour, u.minute, (int)floor(u.second), (int)round((u.second - floor(u.second)) * 1000.0));
  double deg = 0.0;
  nt_get_time_of_event(nd, ms, &deg);
  return deg;
}

nt_err nt_sun_events_for_date(const nt_natural_date* nd, double latitude_deg, nt_sun_events* out) {
  if (!nd || !out) return NT_ERR_INTERNAL;
  if (!(latitude_deg >= -90.0 && latitude_deg <= 90.0)) return NT_ERR_RANGE;

  // Cache by (nadir day, latitude, longitude)
  if (g_sun_events_cache.valid &&
      g_sun_events_cache.nadir == nd->nadir &&
      g_sun_events_cache.latitude == latitude_deg &&
      g_sun_events_cache.longitude == nd->longitude) {
    *out = g_sun_events_cache.value;
    return NT_OK;
  }

  astro_observer_t obs = Astronomy_MakeObserver(latitude_deg, nd->longitude, 0.0);
  astro_time_t nadir_time = astro_time_from_unix_ms(nd->nadir);
  int summer = is_summer_season(nd->day_of_year, latitude_deg);

  // Sunrise
  astro_search_result_t rise = Astronomy_SearchRiseSetEx(BODY_SUN, obs, DIRECTION_RISE, nadir_time, 1.0, 0.0);
  // Sunset
  astro_search_result_t set = Astronomy_SearchRiseSetEx(BODY_SUN, obs, DIRECTION_SET, nadir_time, 1.0, 0.0);
  // Night start: when altitude crosses -12 going down
  astro_search_result_t night_start = Astronomy_SearchAltitude(BODY_SUN, obs, DIRECTION_SET, nadir_time, 2.0, -12.0);
  // Night end: when altitude crosses -12 going up
  astro_search_result_t night_end = Astronomy_SearchAltitude(BODY_SUN, obs, DIRECTION_RISE, nadir_time, 2.0, -12.0);
  // Morning golden hour: altitude crosses +6 going up
  astro_search_result_t morning_golden = Astronomy_SearchAltitude(BODY_SUN, obs, DIRECTION_RISE, nadir_time, 2.0, +6.0);
  // Evening golden hour: altitude crosses +6 going down
  astro_search_result_t evening_golden = Astronomy_SearchAltitude(BODY_SUN, obs, DIRECTION_SET, nadir_time, 2.0, +6.0);

  out->sunrise_deg = event_time_or_default(nd, rise, summer, 0);
  out->sunset_deg = event_time_or_default(nd, set, summer, 1);
  out->night_start_deg = event_time_or_default(nd, night_start, summer, 1);
  out->night_end_deg = event_time_or_default(nd, night_end, summer, 0);
  out->morning_golden_deg = event_time_or_default(nd, morning_golden, summer, 0);
  out->evening_golden_deg = event_time_or_default(nd, evening_golden, summer, 1);

  // Store cache
  g_sun_events_cache.valid = 1;
  g_sun_events_cache.nadir = nd->nadir;
  g_sun_events_cache.latitude = latitude_deg;
  g_sun_events_cache.longitude = nd->longitude;
  g_sun_events_cache.value = *out;

  return NT_OK;
}

nt_err nt_sun_position_for_date(const nt_natural_date* nd, double latitude_deg, nt_sun_position* out) {
  if (!nd || !out) return NT_ERR_INTERNAL;
  if (!(latitude_deg >= -90.0 && latitude_deg <= 90.0)) return NT_ERR_RANGE;

  astro_observer_t obs = Astronomy_MakeObserver(latitude_deg, nd->longitude, 0.0);
  astro_time_t t = astro_time_from_unix_ms(nd->unix_time);

  astro_equatorial_t sun_eq = Astronomy_Equator(BODY_SUN, &t, obs, EQUATOR_OF_DATE, ABERRATION);
  if (sun_eq.status != ASTRO_SUCCESS) return NT_ERR_INTERNAL;
  astro_horizon_t hor = Astronomy_Horizon(&t, obs, sun_eq.ra, sun_eq.dec, REFRACTION_NORMAL);
  double alt = hor.altitude;
  if (alt < 0) alt = 0; // clamp for a visible altitude similar to moon

  // Highest altitude (daily transit) at hour angle 0 around natural nadir anchor
  astro_hour_angle_t transit = Astronomy_SearchHourAngleEx(BODY_SUN, obs, 0.0, astro_time_from_unix_ms(nd->nadir), +1);
  double highest = (transit.status == ASTRO_SUCCESS) ? transit.hor.altitude : 0.0;

  out->altitude = alt;
  out->highest_altitude = highest;
  return NT_OK;
}

nt_err nt_moon_position_for_date(const nt_natural_date* nd, double latitude_deg, nt_moon_position* out) {
  if (!nd || !out) return NT_ERR_INTERNAL;
  if (!(latitude_deg >= -90.0 && latitude_deg <= 90.0)) return NT_ERR_RANGE;

  astro_observer_t obs = Astronomy_MakeObserver(latitude_deg, nd->longitude, 0.0);
  astro_time_t t = astro_time_from_unix_ms(nd->unix_time);

  astro_equatorial_t moon_eq = Astronomy_Equator(BODY_MOON, &t, obs, EQUATOR_OF_DATE, ABERRATION);
  if (moon_eq.status != ASTRO_SUCCESS) return NT_ERR_INTERNAL;
  astro_horizon_t hor = Astronomy_Horizon(&t, obs, moon_eq.ra, moon_eq.dec, REFRACTION_NORMAL);
  double alt = hor.altitude;
  if (alt < 0) alt = 0; // clamp like JS

  astro_angle_result_t phase = Astronomy_MoonPhase(t);
  if (phase.status != ASTRO_SUCCESS) return NT_ERR_INTERNAL;

  out->altitude = alt;
  out->phase_deg = phase.angle;
  return NT_OK;
}

nt_err nt_moon_events_for_date(const nt_natural_date* nd, double latitude_deg, nt_moon_events* out) {
  if (!nd || !out) return NT_ERR_INTERNAL;
  if (!(latitude_deg >= -90.0 && latitude_deg <= 90.0)) return NT_ERR_RANGE;

  astro_observer_t obs = Astronomy_MakeObserver(latitude_deg, nd->longitude, 0.0);
  astro_time_t nadir_time = astro_time_from_unix_ms(nd->nadir);

  astro_search_result_t moonrise = Astronomy_SearchRiseSetEx(BODY_MOON, obs, DIRECTION_RISE, nadir_time, 1.0, 0.0);
  astro_search_result_t moonset  = Astronomy_SearchRiseSetEx(BODY_MOON, obs, DIRECTION_SET,  nadir_time, 1.0, 0.0);
  astro_hour_angle_t transit = Astronomy_SearchHourAngleEx(BODY_MOON, obs, 0.0, nadir_time, +1);

  // Convert found times to degrees within natural day, else 0
  double to_deg_val = 0.0;
  if (moonrise.status == ASTRO_SUCCESS) {
    astro_utc_t u = Astronomy_UtcFromTime(moonrise.time);
    int64_t ms = to_unix_ms_utc(u.year, u.month, u.day, u.hour, u.minute, (int)floor(u.second), (int)round((u.second - floor(u.second)) * 1000.0));
    nt_get_time_of_event(nd, ms, &to_deg_val);
    out->moonrise_deg = to_deg_val;
  } else {
    out->moonrise_deg = 0.0;
  }
  if (moonset.status == ASTRO_SUCCESS) {
    astro_utc_t u = Astronomy_UtcFromTime(moonset.time);
    int64_t ms = to_unix_ms_utc(u.year, u.month, u.day, u.hour, u.minute, (int)floor(u.second), (int)round((u.second - floor(u.second)) * 1000.0));
    nt_get_time_of_event(nd, ms, &to_deg_val);
    out->moonset_deg = to_deg_val;
  } else {
    out->moonset_deg = 0.0;
  }
  out->highest_altitude = (transit.status == ASTRO_SUCCESS) ? transit.hor.altitude : 0.0;
  return NT_OK;
}

nt_err nt_mustaches_range(const nt_natural_date* nd, double latitude_deg, nt_mustaches* out) {
  if (!nd || !out) return NT_ERR_INTERNAL;
  if (!(latitude_deg >= -90.0 && latitude_deg <= 90.0)) return NT_ERR_RANGE;

  int current_year = utc_year_from_unix_ms(nd->unix_time);

  // Cache by (year, latitude)
  if (g_moustaches_cache.valid &&
      g_moustaches_cache.year == current_year &&
      g_moustaches_cache.latitude == latitude_deg) {
    *out = g_moustaches_cache.value;
    return NT_OK;
  }

  astro_seasons_t seasons = seasons_for_year(current_year);
  if (seasons.status != ASTRO_SUCCESS) return NT_ERR_INTERNAL;

  // Build NaturalDate at exact solstice instants at longitude 0 (as in JS), then compute sun events at given latitude.
  astro_utc_t wutc = Astronomy_UtcFromTime(seasons.dec_solstice);
  astro_utc_t sutc = Astronomy_UtcFromTime(seasons.jun_solstice);
  int64_t wms = to_unix_ms_utc(wutc.year, wutc.month, wutc.day, wutc.hour, wutc.minute, (int)floor(wutc.second), (int)round((wutc.second - floor(wutc.second)) * 1000.0));
  int64_t sms = to_unix_ms_utc(sutc.year, sutc.month, sutc.day, sutc.hour, sutc.minute, (int)floor(sutc.second), (int)round((sutc.second - floor(sutc.second)) * 1000.0));

  nt_natural_date winter_nd;
  nt_natural_date summer_nd;
  if (nt_make_natural_date(wms, 0.0, &winter_nd) != NT_OK) return NT_ERR_INTERNAL;
  if (nt_make_natural_date(sms, 0.0, &summer_nd) != NT_OK) return NT_ERR_INTERNAL;

  nt_sun_events wse, sse;
  if (nt_sun_events_for_date(&winter_nd, latitude_deg, &wse) != NT_OK) return NT_ERR_INTERNAL;
  if (nt_sun_events_for_date(&summer_nd, latitude_deg, &sse) != NT_OK) return NT_ERR_INTERNAL;

  double avg;
  if (latitude_deg >= 0.0) {
    avg = ((wse.sunrise_deg - sse.sunrise_deg) + (sse.sunset_deg - wse.sunset_deg)) / 4.0;
  } else {
    avg = ((sse.sunrise_deg - wse.sunrise_deg) + (wse.sunset_deg - sse.sunset_deg)) / 4.0;
  }
  if (avg < 0.0) avg = 0.0;
  if (avg > 90.0) avg = 90.0;

  out->winter_sunrise_deg = wse.sunrise_deg;
  out->winter_sunset_deg = wse.sunset_deg;
  out->summer_sunrise_deg = sse.sunrise_deg;
  out->summer_sunset_deg = sse.sunset_deg;
  out->average_angle_deg = avg;

  // Store cache
  g_moustaches_cache.valid = 1;
  g_moustaches_cache.year = current_year;
  g_moustaches_cache.latitude = latitude_deg;
  g_moustaches_cache.value = *out;
  return NT_OK;
}


