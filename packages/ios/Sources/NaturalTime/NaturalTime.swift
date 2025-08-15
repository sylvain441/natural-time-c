import Foundation
import CNaturalTime

public struct NaturalDate {
    public let year: Int32
    public let moon: Int32
    public let week: Int32
    public let weekOfMoon: Int32
    public let unixTime: Int64
    public let longitude: Double
    public let day: Int32
    public let dayOfYear: Int32
    public let dayOfMoon: Int32
    public let dayOfWeek: Int32
    public let isRainbowDay: Bool
    public let timeDeg: Double
    public let yearStart: Int64
    public let yearDuration: Int32
    public let nadir: Int64

    public init(unixMsUtc: Int64, longitude: Double) throws {
        var c = nt_natural_date()
        let err = nt_make_natural_date(unixMsUtc, longitude, &c)
        guard err == NT_OK else { throw NSError(domain: "NaturalTimeCore", code: Int(err.rawValue), userInfo: nil) }
        year = c.year; moon = c.moon; week = c.week; weekOfMoon = c.week_of_moon
        unixTime = c.unix_time; self.longitude = c.longitude; day = c.day; dayOfYear = c.day_of_year
        dayOfMoon = c.day_of_moon; dayOfWeek = c.day_of_week; isRainbowDay = c.is_rainbow_day != 0
        timeDeg = c.time_deg; yearStart = c.year_start; yearDuration = c.year_duration; nadir = c.nadir
    }
    fileprivate var cStruct: nt_natural_date {
        nt_natural_date(
            year: year, moon: moon, week: week, week_of_moon: weekOfMoon,
            unix_time: unixTime, longitude: longitude, day: day, day_of_year: dayOfYear,
            day_of_moon: dayOfMoon, day_of_week: dayOfWeek, is_rainbow_day: isRainbowDay ? 1:0,
            time_deg: timeDeg, year_start: yearStart, year_duration: yearDuration, nadir: nadir
        )
    }

    // MARK: - Formatting helpers (bridge to C API)

    public func toYearString() -> String {
        var buf = [CChar](repeating: 0, count: 16)
        _ = nt_format_year_string(year, &buf, buf.count)
        return String(cString: buf)
    }

    public func toMoonString() -> String {
        var buf = [CChar](repeating: 0, count: 8)
        _ = nt_format_moon_string(moon, &buf, buf.count)
        return String(cString: buf)
    }

    public func toDayOfMoonString() -> String {
        var buf = [CChar](repeating: 0, count: 8)
        _ = nt_format_day_of_moon_string(dayOfMoon, &buf, buf.count)
        return String(cString: buf)
    }

    public func toLongitudeString(decimals: Int32 = 1) -> String {
        var buf = [CChar](repeating: 0, count: 16)
        _ = nt_format_longitude_string(longitude, decimals, &buf, buf.count)
        return String(cString: buf)
    }

    public func toTimeString(decimals: Int32 = 2, rounding: Double = 0.01) -> String {
        var c = cStruct
        var buf = [CChar](repeating: 0, count: 32)
        _ = nt_format_time_string(&c, decimals, rounding, &buf, buf.count)
        return String(cString: buf)
    }

    public func splitTime(scaleDecimals: Int32 = 2, rounding: Double = 0.01) -> (integer: Int32, fraction: Int32, scale: Int32) {
        var c = cStruct
        var integer: Int32 = 0
        var fraction: Int32 = 0
        var scale: Int32 = 0
        _ = nt_time_split_scaled(&c, scaleDecimals, rounding, &integer, &fraction, &scale)
        return (integer, fraction, scale)
    }

    public func toDateString(separator: Character = ")") -> String {
        var c = cStruct
        var buf = [CChar](repeating: 0, count: 48)
        let sepC: CChar = String(separator).utf8.first.map { CChar($0) } ?? CChar(41) // ')'
        _ = nt_format_date_string(&c, sepC, &buf, buf.count)
        return String(cString: buf)
    }

    public func toString(timeDecimals: Int32 = 2, timeRounding: Double = 0.01) -> String {
        var c = cStruct
        var buf = [CChar](repeating: 0, count: 96)
        _ = nt_format_string(&c, timeDecimals, timeRounding, &buf, buf.count)
        return String(cString: buf)
    }
}

public struct SunEvents {
    public let sunrise: Double
    public let sunset: Double
    public let nightStart: Double
    public let nightEnd: Double
    public let morningGolden: Double
    public let eveningGolden: Double
}

public struct SunPosition {
    public let altitude: Double
    public let highestAltitude: Double
}

public struct MoonPosition {
    public let altitude: Double
    public let phaseDeg: Double
}

public struct MoonEvents {
    public let moonrise: Double
    public let moonset: Double
    public let highestAltitude: Double
}

public struct MustachesRange {
    public let winterSunrise: Double
    public let winterSunset: Double
    public let summerSunrise: Double
    public let summerSunset: Double
    public let averageAngle: Double
}

@inline(__always)
private func mapErr(_ err: nt_err) throws {
    if err != NT_OK { throw NSError(domain: "NaturalTimeCore", code: Int(err.rawValue), userInfo: nil) }
}

public func sunEvents(for nd: NaturalDate, latitude: Double) throws -> SunEvents {
    var c = nd.cStruct
    var out = nt_sun_events()
    try mapErr(nt_sun_events_for_date(&c, latitude, &out))
    return SunEvents(
        sunrise: out.sunrise_deg,
        sunset: out.sunset_deg,
        nightStart: out.night_start_deg,
        nightEnd: out.night_end_deg,
        morningGolden: out.morning_golden_deg,
        eveningGolden: out.evening_golden_deg
    )
}

public func moonPosition(for nd: NaturalDate, latitude: Double) throws -> MoonPosition {
    var c = nd.cStruct
    var out = nt_moon_position()
    try mapErr(nt_moon_position_for_date(&c, latitude, &out))
    return MoonPosition(altitude: out.altitude, phaseDeg: out.phase_deg)
}

public func sunPosition(for nd: NaturalDate, latitude: Double) throws -> SunPosition {
    var c = nd.cStruct
    var out = nt_sun_position()
    try mapErr(nt_sun_position_for_date(&c, latitude, &out))
    return SunPosition(altitude: out.altitude, highestAltitude: out.highest_altitude)
}

public func moonEvents(for nd: NaturalDate, latitude: Double) throws -> MoonEvents {
    var c = nd.cStruct
    var out = nt_moon_events()
    try mapErr(nt_moon_events_for_date(&c, latitude, &out))
    return MoonEvents(moonrise: out.moonrise_deg, moonset: out.moonset_deg, highestAltitude: out.highest_altitude)
}

public func mustachesRange(for nd: NaturalDate, latitude: Double) throws -> MustachesRange {
    var c = nd.cStruct
    var out = nt_mustaches()
    try mapErr(nt_mustaches_range(&c, latitude, &out))
    return MustachesRange(
        winterSunrise: out.winter_sunrise_deg,
        winterSunset: out.winter_sunset_deg,
        summerSunrise: out.summer_sunrise_deg,
        summerSunset: out.summer_sunset_deg,
        averageAngle: out.average_angle_deg
    )
}

public func getTimeOfEvent(for nd: NaturalDate, eventUnixMs: Int64) -> Double {
    var c = nd.cStruct
    var deg: Double = .nan
    if nt_get_time_of_event(&c, eventUnixMs, &deg) == NT_OK { return deg }
    return .nan
}

public func resetCaches() {
    nt_reset_caches()
}


