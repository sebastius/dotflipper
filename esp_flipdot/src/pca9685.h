#pragma once

#include <Arduino.h>

//  PCA9685 cheatsheet
//  Datasheet at
//  https://www.nxp.com/products/analog/interfaces/ic-bus/ic-led-controllers/16-channel-12-bit-pwm-fm-plus-ic-bus-led-controller:PCA9685
//
//  Specific for this flipdot controller:
//
//  All adressing is done through I2C
//
//  Addresses 0x40 - 0x5F columndrivers (4 on a board, for 32 colums)
//            0x60 - 0x6F rowdriver (2 on a bord, for 16 rows)
//            0x70        all call
//
//  Note: The PCF9685 does have adress ranges up to 7F but after 0x6F
//        there are some adresses used for other purposes (0x70 ALL_CALL for instance)
//        to make $math easier, i chose not to go above 6F (48 chips distributed over 8 boards)
//        In theory you could add two more boards. Since i only have 8 flipdot panels of this type
//        I won't bother. ;)
//
//  Registers PCA9685:
//  Registers 0x00 - Mode 0 register (see below)
//            0x07 - led 00 on   - data 0x10 ON, 0x00 OFF
//            0x09 - led 00 off  - data 0x10 ON, 0x00 OFF
//            0x43 - led 15 on
//            0x45 - led 15 off
//            0xfb - all leds on
//            0xfd - all leds off
//            Note: there are a few more registers around each LED
//            But if you write to the 0x07 and 0x09 registers (and onward),
//            you can ignore the others.
//
//  Mode 0    0x00 - Normal operation
//            0x80 - Reset

// Counters since the last resetPCA9685(), to find out how reliable the bus and the pulses are.
// Pulses are measured from the column going on until the row goes off, in CPU cycles.
struct I2CStats {
  uint32_t writes;
  uint32_t nack;       // No acknowledge from the chip
  uint32_t timeout;    // A line stuck low
  uint32_t other;      // Not used anymore, stays 0
  uint32_t offfailed;  // Failed writes that should have switched an output off
  uint32_t pulses;
  uint32_t pulsemax;
  uint64_t pulsetotal;
};

extern I2CStats i2cstats;

// Set by emergencystop(), stays set until a restart. Nothing may flip anymore.
extern bool i2cfault;

extern uint32_t i2ccyclesperus;

void i2cbegin();
bool i2cwrite(byte address, byte reg, byte content);  // False when the chip didn't take it
void i2coff(byte address, byte reg);  // Switch an output off, whatever it takes
void emergencystop();
void resetPCA9685();
void everythingoff(uint8_t address);
void i2cscanner();
