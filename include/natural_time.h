// Natural Time — C ABI (v0.1)
#ifndef NATURAL_TIME_H
#define NATURAL_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
  int32_t year;            // natural year index
  int32_t moon;            // 1..14
  int32_t week;            // 1..53
  int32_t week_of_moon;    // 1..4
  int64_t unix_time;       // input timestamp (ms UTC)
  double  longitude;       // [-180, 180]
  int32_t day;             // days since END_OF_ARTIFICIAL_TIME
  int32_t day_of_year;     // 1..yearDuration
  int32_t day_of_moon;     // 1..28
  int32_t day_of_week;     // 1..7
  int32_t is_rainbow_day;  // bool
  double  time_deg;        // 0..360
  int64_t year_start;      // ms UTC at local year start
  int32_t year_duration;   // 365 or 366
  int64_t nadir;           // ms UTC start of natural day at longitude
} nt_natural_date;

typedef enum { NT_OK=0, NT_ERR_RANGE=1, NT_ERR_TIME=2, NT_ERR_INTERNAL=3 } nt_err;

typedef struct {
  double sunrise_deg;
  double sunset_deg;
  double night_start_deg;
  double night_end_deg;
  double morning_golden_deg;
  double evening_golden_deg;
} nt_sun_events;

typedef struct {
  double altitude;      // clamped to >= 0 like moon
} nt_sun_position;

typedef struct {
  double altitude;      // clamped to >= 0 like JS
  double phase_deg;     // 0..360
} nt_moon_position;

typedef struct {
  double moonrise_deg;
  double moonset_deg;
  double highest_altitude; // degrees
} nt_moon_events;

typedef struct {
  double winter_sunrise_deg;
  double winter_sunset_deg;
  double summer_sunrise_deg;
  double summer_sunset_deg;
  double average_angle_deg;
} nt_mustaches;

nt_err nt_make_natural_date(int64_t unix_ms_utc, double longitude_deg, nt_natural_date* out);
nt_err nt_get_time_of_event(const nt_natural_date* nd, int64_t event_unix_ms_utc, double* out_deg_or_nan);
nt_err nt_sun_events_for_date(const nt_natural_date* nd, double latitude_deg, nt_sun_events* out);
nt_err nt_sun_position_for_date(const nt_natural_date* nd, double latitude_deg, nt_sun_position* out);
nt_err nt_moon_position_for_date(const nt_natural_date* nd, double latitude_deg, nt_moon_position* out);
nt_err nt_moon_events_for_date(const nt_natural_date* nd, double latitude_deg, nt_moon_events* out);
nt_err nt_mustaches_range(const nt_natural_date* nd, double latitude_deg, nt_mustaches* out);

void nt_reset_caches(void);

#ifdef __cplusplus
}
#endif

#endif // NATURAL_TIME_H


