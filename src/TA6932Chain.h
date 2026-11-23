// Minimal driver for several TA6932 LED driver chips (16 digits each).
//
// Every chip has its own STB pin; CLK/DIN may be shared or separate.
// All digits are addressed through one continuous buffer:
// chip 0 = positions 0-15, chip 1 = 16-31, chip 2 = 32-47, ...
//
// Protocol based on the TA6932 library by Allen Chak (MIT License),
// https://github.com/allenchak/TA6932
#pragma once

#include <Arduino.h>

struct TA6932Pins {
  uint8_t stb;
  uint8_t clk;
  uint8_t din;
};

class TA6932Chain
{
public:
  static const uint8_t DIGITS_PER_CHIP = 16;
  static const uint8_t MAX_CHIPS = 4;

  // pins must stay valid for the lifetime of the object (use a static array)
  TA6932Chain(const TA6932Pins* pins, uint8_t chipCount);

  void begin(uint8_t brightness = 7);

  // 0 (dim) .. 7 (bright)
  void setBrightness(uint8_t brightness);

  // writes into the buffer, flush() sends it
  void set(uint8_t pos, uint8_t segments);
  void clear();

  // sends only the chips whose content changed
  void flush();

  uint8_t digitCount() const { return chipCount_ * DIGITS_PER_CHIP; }

private:
  void sendCommand(uint8_t chip, uint8_t command);
  void writeChip(uint8_t chip);

  const TA6932Pins* pins_;
  uint8_t chipCount_;
  uint8_t brightness_ = 7;
  uint8_t buffer_[MAX_CHIPS][DIGITS_PER_CHIP] = {};
  bool dirty_[MAX_CHIPS] = {};
};
