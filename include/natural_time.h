// Natural Time — C ABI (v0.1)
#ifndef NATURAL_TIME_H
#define NATURAL_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

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
  double highest_altitude; // degrees at daily transit
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

 // Formatting helpers (parity with JS NaturalDate string methods)
 // All functions write a NUL-terminated string into `buffer` up to `buffer_size` bytes
 // and return NT_OK on success. If inputs are invalid or buffer is too small, NT_ERR_RANGE is returned.
 // The output is always well-formed and truncated if needed.

 // Formats the full string: "YYY)MM)DD TTT°dd NT±LLL.L" or "YYY)RAINBOW(+)? TTT°dd NT±LLL.L"
 nt_err nt_format_string(const nt_natural_date* nd,
                         int time_decimals,
                         double time_rounding,
                         char* buffer,
                         size_t buffer_size);

 // Formats only the date part. Default separator is ')'. Uses RAINBOW (+ for day 366).
 nt_err nt_format_date_string(const nt_natural_date* nd,
                              char separator,
                              char* buffer,
                              size_t buffer_size);

 // Formats only the time part as TTT°dd with given decimals and rounding (e.g. 2 and 0.01)
 nt_err nt_format_time_string(const nt_natural_date* nd,
                              int decimals,
                              double rounding,
                              char* buffer,
                              size_t buffer_size);

 // Split time into integer degrees and fractional part scaled by 10^decimals
 // after applying optional rounding increment. Example: decimals=2 => scale=100.
 // Wraps 360.00 to 0.00 like JS formatting.
 nt_err nt_time_split_scaled(const nt_natural_date* nd,
                             int decimals,
                             double rounding,
                             int32_t* out_integer,
                             int32_t* out_fraction,
                             int32_t* out_scale);

 // Formats longitude: "NTZ" when |lon| < 0.5, otherwise "NT±D(.d)?"
 nt_err nt_format_longitude_string(double longitude_deg,
                                   int decimals,
                                   char* buffer,
                                   size_t buffer_size);

 // Individual components used by date string
 nt_err nt_format_year_string(int32_t year,
                              char* buffer,
                              size_t buffer_size);
 nt_err nt_format_moon_string(int32_t moon,
                              char* buffer,
                              size_t buffer_size);
 nt_err nt_format_day_of_moon_string(int32_t day_of_moon,
                                     char* buffer,
                                     size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // NATURAL_TIME_H


