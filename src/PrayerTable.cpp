#include "PrayerTable.h"

namespace {
const uint32_t FILE_MAGIC   = 0x54515741;   // "AWQT"
const uint8_t  FILE_VERSION = 1;

struct FileHeader {
  uint32_t magic;
  uint8_t  version;
  uint8_t  count;
  char     city[18];
};
}

void PrayerTable::merge(const DayTimes* days, uint8_t n, uint32_t todayKey)
{
  // drop the past
  uint8_t kept = 0;
  for (uint8_t i = 0; i < count_; i++) {
    if (days_[i].key() >= todayKey) {
      days_[kept++] = days_[i];
    }
  }
  count_ = kept;

  // add or replace
  for (uint8_t i = 0; i < n; i++) {
    uint32_t k = days[i].key();
    if (k < todayKey) {
      continue;
    }
    bool replaced = false;
    for (uint8_t j = 0; j < count_; j++) {
      if (days_[j].key() == k) {
        days_[j] = days[i];
        replaced = true;
        break;
      }
    }
    if (!replaced && count_ < CAPACITY) {
      days_[count_++] = days[i];
    }
  }

  sort();
}

void PrayerTable::sort()
{
  // insertion sort, the table is small and mostly sorted already
  for (uint8_t i = 1; i < count_; i++) {
    DayTimes d = days_[i];
    int8_t j = i - 1;
    while (j >= 0 && days_[j].key() > d.key()) {
      days_[j + 1] = days_[j];
      j--;
    }
    days_[j + 1] = d;
  }
}

const DayTimes* PrayerTable::find(uint32_t key) const
{
  for (uint8_t i = 0; i < count_; i++) {
    if (days_[i].key() == key) {
      return &days_[i];
    }
  }
  return nullptr;
}

uint8_t PrayerTable::daysFrom(uint32_t key) const
{
  uint8_t n = 0;
  for (uint8_t i = 0; i < count_; i++) {
    if (days_[i].key() >= key) {
      n++;
    }
  }
  return n;
}

bool PrayerTable::save(fs::FS& fs, const char* path, const char* cityId) const
{
  FileHeader h = {};
  h.magic   = FILE_MAGIC;
  h.version = FILE_VERSION;
  h.count   = count_;
  strncpy(h.city, cityId, sizeof(h.city) - 1);

  // write to a temporary file first, a power cut then never leaves a broken file
  String tmp = String(path) + ".tmp";
  File f = fs.open(tmp, "w");
  if (!f) {
    return false;
  }
  bool ok = f.write(reinterpret_cast<const uint8_t*>(&h), sizeof(h)) == sizeof(h)
         && f.write(reinterpret_cast<const uint8_t*>(days_), sizeof(DayTimes) * count_) == sizeof(DayTimes) * count_;
  f.close();
  if (!ok) {
    fs.remove(tmp);
    return false;
  }
  fs.remove(path);
  return fs.rename(tmp, path);
}

bool PrayerTable::load(fs::FS& fs, const char* path, const char* cityId)
{
  File f = fs.open(path, "r");
  if (!f) {
    return false;
  }
  FileHeader h;
  bool ok = f.read(reinterpret_cast<uint8_t*>(&h), sizeof(h)) == sizeof(h)
         && h.magic == FILE_MAGIC
         && h.version == FILE_VERSION
         && h.count <= CAPACITY
         && strncmp(h.city, cityId, sizeof(h.city) - 1) == 0
         && f.read(reinterpret_cast<uint8_t*>(days_), sizeof(DayTimes) * h.count) == sizeof(DayTimes) * h.count;
  f.close();
  count_ = ok ? h.count : 0;
  if (ok) {
    sort();
  }
  return ok;
}
