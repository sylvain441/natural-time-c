#include "natural_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define VECTORS_PATH "tests/data/vectors.json"
#define DEG_EPS 1e-9
#define SUN_DEG_EPS 1e-3
#define MAX_MISMATCH_LOGS 20
#define MS_PER_DAY 86400000.0

static int read_file_into_buffer(const char *path, char **out_buf, size_t *out_len) {
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = (char*)malloc(len + 1);
  if (!buf) { fclose(f); return 0; }
  size_t n = fread(buf, 1, len, f);
  fclose(f);
  buf[n] = '\0';
  *out_buf = buf;
  *out_len = n;
  return 1;
}

static int find_next(const char *buf, size_t start, const char *needle, size_t *pos_out) {
  const char *p = strstr(buf + start, needle);
  if (!p) return 0;
  *pos_out = (size_t)(p - buf);
  return 1;
}

static int parse_int64_after(const char *buf, size_t start, const char *key, long long *out) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  size_t pos;
  if (!find_next(buf, start, pattern, &pos)) return 0;
  pos += strlen(pattern);
  while (buf[pos] == ' ' || buf[pos] == '\t') pos++;
  char *endp;
  long long v = strtoll(buf + pos, &endp, 10);
  if (endp == buf + pos) return 0;
  *out = v;
  return 1;
}

static int parse_double_after(const char *buf, size_t start, const char *key, double *out) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  size_t pos;
  if (!find_next(buf, start, pattern, &pos)) return 0;
  pos += strlen(pattern);
  while (buf[pos] == ' ' || buf[pos] == '\t') pos++;
  char *endp;
  double v = strtod(buf + pos, &endp);
  if (endp == buf + pos) return 0;
  *out = v;
  return 1;
}

static int parse_int_after(const char *buf, size_t start, const char *key, int *out) {
  long long v;
  if (!parse_int64_after(buf, start, key, &v)) return 0;
  *out = (int)v;
  return 1;
}

static int parse_bool_after(const char *buf, size_t start, const char *key, int *out) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  size_t pos;
  if (!find_next(buf, start, pattern, &pos)) return 0;
  pos += strlen(pattern);
  while (buf[pos] == ' ' || buf[pos] == '\t') pos++;
  if (strncmp(buf + pos, "true", 4) == 0) { *out = 1; return 1; }
  if (strncmp(buf + pos, "false", 5) == 0) { *out = 0; return 1; }
  // also accept 0/1
  return parse_int_after(buf, start, key, out);
}

static void format_iso8601(long long unix_ms, char *out, size_t out_size) {
  time_t secs = (time_t)(unix_ms / 1000LL);
  struct tm *gmt = gmtime(&secs);
  snprintf(out, out_size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
           gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
           gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
}

int main(void) {
  char *json = NULL; size_t len = 0;
  if (!read_file_into_buffer(VECTORS_PATH, &json, &len)) {
    fprintf(stderr, "Failed to read %s\n", VECTORS_PATH);
    return 1;
  }

  size_t cursor = 0;
  int checked = 0;
  int failures = 0;
  // Per-event mismatch counters and max deltas
  const char *event_names[6] = {"sunrise", "sunset", "night_start", "night_end", "morning_golden", "evening_golden"};
  long long max_unix_ms[6] = {0};
  double max_lon[6] = {0};
  double max_lat[6] = {0};
  double max_delta[6] = {0};
  double max_c_value[6] = {0};
  double max_js_value[6] = {0};
  long long mismatch_counts[6] = {0};
  int logged = 0;
  // Stats across all cases
  double sum_delta_deg[6] = {0};
  double max_delta_deg[6] = {0};
  long long event_counts[6] = {0};
  // Sun/Moon stats
  long long sun_alt_checked = 0;
  double sun_sum_alt_delta = 0, sun_max_alt_delta = 0;
  long long moon_checked = 0;
  double moon_sum_alt_delta = 0, moon_max_alt_delta = 0;
  double moon_sum_phase_delta = 0, moon_max_phase_delta = 0;
  double moon_sum_rise_delta = 0, moon_max_rise_delta = 0;
  double moon_sum_set_delta = 0, moon_max_set_delta = 0;
  double moon_sum_transit_alt_delta = 0, moon_max_transit_alt_delta = 0;

  while (1) {
    size_t case_pos;
    if (!find_next(json, cursor, "\"unix_ms_utc\":", &case_pos)) break;

    long long unix_ms;
    if (!parse_int64_after(json, case_pos, "unix_ms_utc", &unix_ms)) break;

    double longitude;
    if (!parse_double_after(json, case_pos, "longitude", &longitude)) break;

    size_t expect_pos;
    if (!find_next(json, case_pos, "\"expect\":", &expect_pos)) break;

    // Parse expected fields from expect{}
    int exp_year, exp_moon, exp_week, exp_week_of_moon;
    long long exp_unix_time, exp_year_start, exp_nadir;
    int exp_day, exp_day_of_year, exp_day_of_moon, exp_day_of_week, exp_year_duration, exp_is_rainbow_day;
    double exp_time_deg, exp_longitude_dup;

    if (!parse_int_after(json, expect_pos, "year", &exp_year)) break;
    if (!parse_int_after(json, expect_pos, "moon", &exp_moon)) break;
    if (!parse_int_after(json, expect_pos, "week", &exp_week)) break;
    if (!parse_int_after(json, expect_pos, "week_of_moon", &exp_week_of_moon)) break;
    if (!parse_int64_after(json, expect_pos, "unix_time", &exp_unix_time)) break;
    if (!parse_int64_after(json, expect_pos, "year_start", &exp_year_start)) break;
    if (!parse_int64_after(json, expect_pos, "nadir", &exp_nadir)) break;
    if (!parse_int_after(json, expect_pos, "day", &exp_day)) break;
    if (!parse_int_after(json, expect_pos, "day_of_year", &exp_day_of_year)) break;
    if (!parse_int_after(json, expect_pos, "day_of_moon", &exp_day_of_moon)) break;
    if (!parse_int_after(json, expect_pos, "day_of_week", &exp_day_of_week)) break;
    if (!parse_int_after(json, expect_pos, "year_duration", &exp_year_duration)) break;
    if (!parse_bool_after(json, expect_pos, "is_rainbow_day", &exp_is_rainbow_day)) break;
    if (!parse_double_after(json, expect_pos, "longitude", &exp_longitude_dup)) break;
    if (!parse_double_after(json, expect_pos, "time_deg", &exp_time_deg)) break;

    nt_natural_date nd = {0};
    nt_err err = nt_make_natural_date(unix_ms, longitude, &nd);
    if (err != NT_OK) { failures++; break; }

    // Compare ints exactly
    if (nd.unix_time != exp_unix_time) failures++;
    if (nd.year != exp_year) failures++;
    if (nd.moon != exp_moon) failures++;
    if (nd.week != exp_week) failures++;
    if (nd.week_of_moon != exp_week_of_moon) failures++;
    if (nd.day != exp_day) failures++;
    if (nd.day_of_year != exp_day_of_year) failures++;
    if (nd.day_of_moon != exp_day_of_moon) failures++;
    if (nd.day_of_week != exp_day_of_week) failures++;
    if (nd.year_duration != exp_year_duration) failures++;
    if ((nd.is_rainbow_day ? 1:0) != exp_is_rainbow_day) failures++;

    // Compare doubles with epsilon
    if (fabs(nd.longitude - exp_longitude_dup) > 0.0) failures++;
    if (llabs(nd.year_start - exp_year_start) != 0) failures++;
    if (llabs(nd.nadir - exp_nadir) != 0) failures++;
    if (fabs(nd.time_deg - exp_time_deg) > DEG_EPS) failures++;

    // Sun events parity
    double latitude;
    if (parse_double_after(json, case_pos, "latitude", &latitude)) {
      double exp_sunrise, exp_sunset, exp_night_start, exp_night_end, exp_morning_golden, exp_evening_golden;
      if (parse_double_after(json, expect_pos, "sunrise_deg", &exp_sunrise) &&
          parse_double_after(json, expect_pos, "sunset_deg", &exp_sunset) &&
          parse_double_after(json, expect_pos, "night_start_deg", &exp_night_start) &&
          parse_double_after(json, expect_pos, "night_end_deg", &exp_night_end) &&
          parse_double_after(json, expect_pos, "morning_golden_deg", &exp_morning_golden) &&
          parse_double_after(json, expect_pos, "evening_golden_deg", &exp_evening_golden)) {
        nt_sun_events se;
        if (nt_sun_events_for_date(&nd, latitude, &se) == NT_OK) {
          double cvals[6] = { se.sunrise_deg, se.sunset_deg, se.night_start_deg, se.night_end_deg, se.morning_golden_deg, se.evening_golden_deg };
          double jvals[6] = { exp_sunrise, exp_sunset, exp_night_start, exp_night_end, exp_morning_golden, exp_evening_golden };
          for (int ei = 0; ei < 6; ++ei) {
            double delta = fabs(cvals[ei] - jvals[ei]);
            if (delta > 180.0) delta = 360.0 - delta; // minimal circular diff
            sum_delta_deg[ei] += delta;
            if (delta > max_delta_deg[ei]) max_delta_deg[ei] = delta;
            event_counts[ei]++;
            if (delta > SUN_DEG_EPS) {
              mismatch_counts[ei]++;
              if (delta > max_delta[ei]) {
                max_delta[ei] = delta;
                max_unix_ms[ei] = unix_ms;
                max_lon[ei] = longitude;
                max_lat[ei] = latitude;
                max_c_value[ei] = cvals[ei];
                max_js_value[ei] = jvals[ei];
              }
              if (logged < MAX_MISMATCH_LOGS) {
                char iso[32];
                format_iso8601(unix_ms, iso, sizeof(iso));
                fprintf(stderr, "Mismatch %s at %s lon=%.2f lat=%.2f: C=%.6f JS=%.6f Δ=%.6f\n",
                        event_names[ei], iso, longitude, latitude, cvals[ei], jvals[ei], delta);
                logged++;
              }
              failures++;
            }
          }
        } else {
          failures++;
        }

        // Sun altitude parity
        double exp_sun_altitude;
        if (parse_double_after(json, expect_pos, "sun_altitude", &exp_sun_altitude)) {
          nt_sun_position sp;
          if (nt_sun_position_for_date(&nd, latitude, &sp) == NT_OK) {
            double d = fabs(sp.altitude - exp_sun_altitude);
            sun_sum_alt_delta += d; if (d > sun_max_alt_delta) sun_max_alt_delta = d; sun_alt_checked++;
          }
        }

        // Moon parity (position + events)
        nt_moon_position mp;
        nt_moon_events me;
        if (nt_moon_position_for_date(&nd, latitude, &mp) == NT_OK &&
            nt_moon_events_for_date(&nd, latitude, &me) == NT_OK) {
          double exp_moon_alt, exp_moon_phase;
          double exp_moonrise, exp_moonset, exp_highest_alt;
          if (parse_double_after(json, expect_pos, "altitude", &exp_moon_alt) &&
              parse_double_after(json, expect_pos, "phase_deg", &exp_moon_phase) &&
              parse_double_after(json, expect_pos, "moonrise_deg", &exp_moonrise) &&
              parse_double_after(json, expect_pos, "moonset_deg", &exp_moonset) &&
              parse_double_after(json, expect_pos, "highest_altitude", &exp_highest_alt)) {
            double d_alt = fabs(mp.altitude - exp_moon_alt);
            double d_phase = fabs(mp.phase_deg - exp_moon_phase);
            if (d_phase > 180.0) d_phase = 360.0 - d_phase;
            double d_rise = fabs(me.moonrise_deg - exp_moonrise);
            if (d_rise > 180.0) d_rise = 360.0 - d_rise;
            double d_set = fabs(me.moonset_deg - exp_moonset);
            if (d_set > 180.0) d_set = 360.0 - d_set;
            double d_trans = fabs(me.highest_altitude - exp_highest_alt);

            moon_sum_alt_delta += d_alt; if (d_alt > moon_max_alt_delta) moon_max_alt_delta = d_alt;
            moon_sum_phase_delta += d_phase; if (d_phase > moon_max_phase_delta) moon_max_phase_delta = d_phase;
            moon_sum_rise_delta += d_rise; if (d_rise > moon_max_rise_delta) moon_max_rise_delta = d_rise;
            moon_sum_set_delta += d_set; if (d_set > moon_max_set_delta) moon_max_set_delta = d_set;
            moon_sum_transit_alt_delta += d_trans; if (d_trans > moon_max_transit_alt_delta) moon_max_transit_alt_delta = d_trans;
            moon_checked++;
          }
        }
      }
    }

    checked++;
    cursor = expect_pos + 8; // move forward inside this case; next search finds next case
  }

  free(json);
  if (checked == 0) {
    fprintf(stderr, "No cases parsed from %s\n", VECTORS_PATH);
    return 2;
  }
  if (failures != 0) {
    fprintf(stderr, "Parity test failed: %d failures out of %d checked\n", failures, checked);
    for (int ei = 0; ei < 6; ++ei) {
      if (mismatch_counts[ei] > 0) {
        char iso[32];
        format_iso8601(max_unix_ms[ei], iso, sizeof(iso));
        fprintf(stderr, "  worst %s: Δ=%.6f at %s lon=%.2f lat=%.2f (C=%.6f JS=%.6f), count=%lld\n",
                event_names[ei], max_delta[ei], iso, max_lon[ei], max_lat[ei], max_c_value[ei], max_js_value[ei], mismatch_counts[ei]);
      }
    }
    return 3;
  }
  // When parity is exact (no mismatches), still compute and print average epsilons (should be ~0)
  printf("parity ok (%d cases)\n", checked);
  if (sun_alt_checked > 0) {
    printf("sun avg alt Δ: %.6f deg (max %.6f)\n", sun_sum_alt_delta / sun_alt_checked, sun_max_alt_delta);
  }
  for (int ei = 0; ei < 6; ++ei) {
    if (event_counts[ei] > 0) {
      double avg_deg = sum_delta_deg[ei] / (double)event_counts[ei];
      double avg_ms = avg_deg * (MS_PER_DAY / 360.0);
      double max_ms = max_delta_deg[ei] * (MS_PER_DAY / 360.0);
      printf("avg epsilon %s: %.9f deg (%.3f ms), max: %.9f deg (%.3f ms) over %lld events\n",
             event_names[ei], avg_deg, avg_ms, max_delta_deg[ei], max_ms, event_counts[ei]);
    }
  }
  if (moon_checked > 0) {
    printf("moon avg alt Δ: %.6f deg (max %.6f)\n", moon_sum_alt_delta / moon_checked, moon_max_alt_delta);
    printf("moon avg phase Δ: %.6f deg (max %.6f)\n", moon_sum_phase_delta / moon_checked, moon_max_phase_delta);
    printf("moon avg moonrise Δ: %.6f deg (max %.6f)\n", moon_sum_rise_delta / moon_checked, moon_max_rise_delta);
    printf("moon avg moonset Δ: %.6f deg (max %.6f)\n", moon_sum_set_delta / moon_checked, moon_max_set_delta);
    printf("moon avg transit altitude Δ: %.6f deg (max %.6f)\n", moon_sum_transit_alt_delta / moon_checked, moon_max_transit_alt_delta);
  }
  // Mustaches parity check (lightweight sample of first N cases)
  size_t pos = 0; int samples = 0; const int MAX_SAMPLES = 2000;
  double sum_ws_rise=0, sum_ws_set=0, sum_ss_rise=0, sum_ss_set=0, sum_angle=0;
  double max_ws_rise=0, max_ws_set=0, max_ss_rise=0, max_ss_set=0, max_angle=0; int must_count=0;
  while (samples < MAX_SAMPLES && find_next(json, pos, "\"unix_ms_utc\":", &pos)) {
    long long unix_ms2; double lon2, lat2; size_t expect2;
    if (!parse_int64_after(json, pos, "unix_ms_utc", &unix_ms2)) break;
    if (!parse_double_after(json, pos, "longitude", &lon2)) break;
    if (!parse_double_after(json, pos, "latitude", &lat2)) break;
    if (!find_next(json, pos, "\"expect\":", &expect2)) break;
    nt_natural_date nd2; if (nt_make_natural_date(unix_ms2, lon2, &nd2) != NT_OK) break;
    nt_mustaches m;
    if (nt_mustaches_range(&nd2, lat2, &m) == NT_OK) {
      double ws_rise, ws_set, ss_rise, ss_set, angle;
      if (parse_double_after(json, expect2, "winter_sunrise_deg", &ws_rise) &&
          parse_double_after(json, expect2, "winter_sunset_deg", &ws_set) &&
          parse_double_after(json, expect2, "summer_sunrise_deg", &ss_rise) &&
          parse_double_after(json, expect2, "summer_sunset_deg", &ss_set) &&
          parse_double_after(json, expect2, "average_angle_deg", &angle)) {
        double d1=fabs(m.winter_sunrise_deg-ws_rise); if(d1>max_ws_rise) max_ws_rise=d1; sum_ws_rise+=d1;
        double d2=fabs(m.winter_sunset_deg-ws_set);  if(d2>max_ws_set)  max_ws_set=d2;  sum_ws_set+=d2;
        double d3=fabs(m.summer_sunrise_deg-ss_rise);if(d3>max_ss_rise) max_ss_rise=d3; sum_ss_rise+=d3;
        double d4=fabs(m.summer_sunset_deg-ss_set);  if(d4>max_ss_set)  max_ss_set=d4;  sum_ss_set+=d4;
        double d5=fabs(m.average_angle_deg-angle);   if(d5>max_angle)   max_angle=d5;   sum_angle+=d5;
        must_count++;
      }
    }
    samples++;
    pos = expect2 + 8;
  }
  if (must_count>0) {
    printf("mustaches avg Δ: winter rise %.6f, winter set %.6f, summer rise %.6f, summer set %.6f, angle %.6f (maxs %.6f/%.6f/%.6f/%.6f/%.6f) over %d samples\n",
      sum_ws_rise/must_count, sum_ws_set/must_count, sum_ss_rise/must_count, sum_ss_set/must_count, sum_angle/must_count,
      max_ws_rise, max_ws_set, max_ss_rise, max_ss_set, max_angle, must_count);
  }
  return 0;
}


