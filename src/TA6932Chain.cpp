#include "TA6932Chain.h"

namespace {
const uint8_t CMD_DATA_AUTO_INC = 0x40;  // write display data, auto-increment address
const uint8_t CMD_ADDRESS_0     = 0xC0;  // start at address 0
const uint8_t CMD_DISPLAY_ON    = 0x88;  // | brightness 0-7
}

TA6932Chain::TA6932Chain(const TA6932Pins* pins, uint8_t chipCount)
  : pins_(pins), chipCount_(chipCount > MAX_CHIPS ? MAX_CHIPS : chipCount)
{
}

void TA6932Chain::begin(uint8_t brightness)
{
  brightness_ = brightness & 0x07;
  for (uint8_t c = 0; c < chipCount_; c++) {
    pinMode(pins_[c].stb, OUTPUT);
    digitalWrite(pins_[c].stb, HIGH);   // idle
    pinMode(pins_[c].clk, OUTPUT);
    pinMode(pins_[c].din, OUTPUT);
  }
  clear();
  for (uint8_t c = 0; c < chipCount_; c++) {
    writeChip(c);
  }
}

void TA6932Chain::setBrightness(uint8_t brightness)
{
  brightness_ = brightness & 0x07;
  for (uint8_t c = 0; c < chipCount_; c++) {
    sendCommand(c, CMD_DISPLAY_ON | brightness_);
  }
}

void TA6932Chain::set(uint8_t pos, uint8_t segments)
{
  uint8_t chip = pos / DIGITS_PER_CHIP;
  if (chip >= chipCount_) {
    return;
  }
  uint8_t& cell = buffer_[chip][pos % DIGITS_PER_CHIP];
  if (cell != segments) {
    cell = segments;
    dirty_[chip] = true;
  }
}

void TA6932Chain::clear()
{
  for (uint8_t c = 0; c < chipCount_; c++) {
    memset(buffer_[c], 0, DIGITS_PER_CHIP);
    dirty_[c] = true;
  }
}

void TA6932Chain::flush()
{
  for (uint8_t c = 0; c < chipCount_; c++) {
    if (dirty_[c]) {
      writeChip(c);
    }
  }
}

void TA6932Chain::sendCommand(uint8_t chip, uint8_t command)
{
  const TA6932Pins& p = pins_[chip];
  digitalWrite(p.stb, LOW);
  shiftOut(p.din, p.clk, LSBFIRST, command);
  digitalWrite(p.stb, HIGH);
}

void TA6932Chain::writeChip(uint8_t chip)
{
  const TA6932Pins& p = pins_[chip];

  sendCommand(chip, CMD_DATA_AUTO_INC);

  digitalWrite(p.stb, LOW);
  shiftOut(p.din, p.clk, LSBFIRST, CMD_ADDRESS_0);
  for (uint8_t i = 0; i < DIGITS_PER_CHIP; i++) {
    shiftOut(p.din, p.clk, LSBFIRST, buffer_[chip][i]);
  }
  digitalWrite(p.stb, HIGH);

  sendCommand(chip, CMD_DISPLAY_ON | brightness_);
  dirty_[chip] = false;
}
