# Natural Time — C Library

A revolutionary way to measure time, realigning human life with nature’s rhythms — now in portable C, with a Swift Package for Apple platforms. This library mirrors the authoritative `natural-time-js` behavior with tight numeric parity.

## The Natural Time Paradigm

- **Circular Time**: days measured in 360°, continuous solar movement (no hours/minutes) — use `time_deg`.
- **Location-aware**: longitude shifts local day/year start (time distance replaces time zones).
- **Solar‑anchored years**: years begin around the winter solstice (12:00 UTC alignment).
- **13 perfect moons**: 13×28 days (364), plus Rainbow day(s) to complete the year.

This is not a calendar tweak — it’s a return to observable celestial cycles.

## Try Natural Time

- Web app: `https://naturaltime.app/`
- App source: `https://github.com/sylvain441/natural-time-app`
- JS reference: `https://github.com/sylvain441/natural-time-js`

## Features

- Natural date: `nt_make_natural_date`, `nt_get_time_of_event`
- Sun events: sunrise/sunset, night start/end (−12°), golden hour (+6°)
- Moon: altitude/phase/illumination and moonrise/moonset/transit
- Mustaches: winter/summer sunrise/sunset + average angle
- Golden‑vector parity vs JS; CI on macOS/Linux/Windows

## Install, Build, Test

Prereqs: CMake, C compiler (Clang/GCC/MSVC), Git submodules.

```
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build
```

## Golden Vectors (generated on demand)

Vectors are not tracked. Generate from `natural-time-js`:

```
# In ../natural-time-js
npm run build
npm run vectors
cp vectors/vectors.json ../natural-time-core/tests/data/vectors.json

# Back here
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build
```

## C API (header: `include/natural_time.h`)

```
nt_err nt_make_natural_date(long long unix_ms_utc, double longitude_deg, nt_natural_date* out);
nt_err nt_get_time_of_event(const nt_natural_date* nd, long long event_unix_ms_utc, double* out_deg_or_nan);
nt_err nt_sun_events_for_date(const nt_natural_date* nd, double latitude_deg, nt_sun_events* out);
nt_err nt_moon_position_for_date(const nt_natural_date* nd, double latitude_deg, nt_moon_position* out);
nt_err nt_moon_events_for_date(const nt_natural_date* nd, double latitude_deg, nt_moon_events* out);
nt_err nt_mustaches_range(const nt_natural_date* nd, double latitude_deg, nt_mustaches* out);
void   nt_reset_caches(void);
```

## Swift Package (Apple)

SPM package under `packages/ios` with module `NaturalTime`.

```
import NaturalTime

let nd = try NaturalDate(
  unixMsUtc: Int64(Date().timeIntervalSince1970 * 1000),
  longitudeDeg: 0
)
let sun = try sunEvents(for: nd, latitude: 48.85)
let moonPos = try moonPosition(for: nd, latitude: 48.85)
let moonEvt = try moonEvents(for: nd, latitude: 48.85)
let must = try mustachesRange(for: nd, latitude: 48.85)
```

Tip: see the roadmap for a SwiftUI example that rotates a hand with `time_deg`.

## CI

GitHub Actions builds/tests on macOS, Ubuntu, Windows.

## Contribute

Natural Time welcomes contributions from free‑thinking minds. Reach out here or by email.

## License & Acknowledgments

- See `LICENSE` (Creative Commons Zero).
- Astronomy powered by Don Cross’ Astronomy Engine: `https://github.com/cosinekitty/astronomy`


