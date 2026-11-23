// Prayer times for several days, kept sorted by date and persisted in flash.
#pragma once

#include <Arduino.h>
#include <FS.h>

enum Prayer : uint8_t { FAJR, SUNRISE, DHUHR, ASR, MAGHRIB, ISHA, PRAYER_COUNT };

static const uint16_t NO_TIME = 0xFFFF;

struct DayTimes {
  uint16_t year;
  uint8_t  month;
  uint8_t  day;
  uint16_t minutes[PRAYER_COUNT];   // minutes since local midnight

  uint32_t key() const { return dateKey(year, month, day); }

  // yyyymmdd as a sortable number
  static uint32_t dateKey(uint16_t y, uint8_t m, uint8_t d) { return y * 10000UL + m * 100UL + d; }
};

class PrayerTable
{
public:
  static const uint8_t CAPACITY = 40;   // enough for a monthly download plus a few days

  void clear() { count_ = 0; }
  uint8_t size() const { return count_; }

  // Drops all days before todayKey, then adds/replaces the given days.
  void merge(const DayTimes* days, uint8_t n, uint32_t todayKey);

  // nullptr if the day is not in the table
  const DayTimes* find(uint32_t key) const;

  // number of stored days from key onwards (incl. key)
  uint8_t daysFrom(uint32_t key) const;

  // The file is bound to a city id, a file of another city is ignored on load.
  bool save(fs::FS& fs, const char* path, const char* cityId) const;
  bool load(fs::FS& fs, const char* path, const char* cityId);

private:
  void sort();

  DayTimes days_[CAPACITY];
  uint8_t count_ = 0;
};
