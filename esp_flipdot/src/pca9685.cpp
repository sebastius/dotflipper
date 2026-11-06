#include <Wire.h>

#include "config.h"
#include "pca9685.h"

#define I2C_OFF_RETRIES 3

I2CStats i2cstats;
bool i2cfault = false;

bool i2cwrite(byte address, byte reg, byte content) {
#ifdef DEBUG
  Serial.print("I2C: ");
  if (address < 0x10) {
    Serial.print("0");
  }
  Serial.print(address, HEX);
  Serial.print(" ");
  if (reg < 0x10) {
    Serial.print("0");
  }
  Serial.print(reg, HEX);
  Serial.print(" ");
  if (content < 0x10) {
    Serial.print("0");
  }
  Serial.println(content, HEX);
#endif
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(content);
  uint8_t result = Wire.endTransmission();

  i2cstats.writes++;
  if (result != 0) {
    if (result == 2 || result == 3) {
      i2cstats.nack++;
    } else if (result == 5) {
      i2cstats.timeout++;
    } else {
      i2cstats.other++;
    }
    if (content == 0x00 && reg >= 0x06 && reg <= 0x45) {
      i2cstats.offfailed++;
    }
  }
  return result == 0;
}

// An output that stays on burns a coil and its drivers: the PCA9685 has no timeout of its own.
void i2coff(byte address, byte reg) {
  if (i2cwrite(address, reg, 0x00)) {
    return;
  }
  for (uint8_t i = 0; i < I2C_OFF_RETRIES; i++) {
    if (i2cwrite(address, reg, 0x00)) {
      return;
    }
  }
  emergencystop();
}

// Bit 4 of ALL_LED_OFF_H switches a whole chip off, All Call reaches every chip at once.
// The chips themselves get it too, in case one missed it. Only a restart clears this.
void emergencystop() {
  if (i2cfault) {
    return;
  }
  i2cfault = true;
  for (uint8_t i = 0; i < I2C_OFF_RETRIES; i++) {
    if (i2cwrite(0x70, 0xFD, 0x10)) {
      break;
    }
  }
  for (uint8_t panel = 0; panel < PANELS_X * PANELS_Y; panel++) {
    for (uint8_t chip = 0; chip < 4; chip++) {
      i2cwrite(0x40 + panel * 4 + chip, 0xFD, 0x10);
    }
    for (uint8_t chip = 0; chip < 2; chip++) {
      i2cwrite(0x60 + panel * 2 + chip, 0xFD, 0x10);
    }
  }
  Serial.println("I2C fault: emergency stop, restart needed");
}

void resetPCA9685() {
  for (uint8_t x = 0x40; x < 0x6F; x++) {
    i2cwrite(x, 0x00, 0x80); // ???
    i2cwrite(x, 0x00, 0b00000001); // Awake and responds to 'all call'
    i2cwrite(x, 0x01, 0b00000100); //
    i2cwrite(x, 0x00, 0b00010001); // Sleep mode and responds to 'all call'
    i2cwrite(x, 0xFE, 0x03);       // Set PWM frequency to 1526hz (maximum), formula is round( (osc_clock / ( 4096*update_rate ) ) - 1 ), so round ( ( 25mhz / (4096*1526) ) - 1 ) = 0x03
    i2cwrite(x, 0x00, 0b00000001); // Awake and responds to 'all call'
    everythingoff(x);
  }
  i2cstats = {};  // Addresses without a chip always fail, those don't count
}

void everythingoff(uint8_t address) {
  // Write to the ALL_LED registers to switch them all off.
  i2cwrite(address, 0xFA, 0x00);
  i2cwrite(address, 0xFB, 0x00);
  i2cwrite(address, 0xFC, 0x00);
  i2cwrite(address, 0xFD, 0x00);
}

static void infiniteloop() {
  while (1) {
    yield(); // so the ESP doesnt crash
  }
}

void i2cscanner() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning I2C bus...");

  nDevices = 0;
  for (address = 1; address < 127; address++ ) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      Serial.print((address < 0x10) ? "0" : "");
      Serial.println(address, HEX);
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      Serial.print((address < 0x10) ? "0" : "");
      Serial.println(address, HEX);
    }
  }
  Serial.println("I2C scan done\n");
  if (nDevices == 0) {
    Serial.println("No I2C devices found, infinite loop now so you can check everything.");
    infiniteloop();
  } else {
    Serial.print(nDevices, DEC);
    Serial.print(" I2C device");
    Serial.print((nDevices > 1) ? "s" : "");
    Serial.println(" found");
  }
}
