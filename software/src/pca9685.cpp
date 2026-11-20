#ifndef HARDWARE_V3  // Only the v1 boards have PCA9685 drivers

#include "config.h"
#include "pca9685.h"
#include "i2cpins.h"

// Bit-banged I2C. The driver of the ESP32 takes about 85 us per write and how long depends on
// what happens to be in the flash cache, which made the pulses twice as long in some builds.
// Everything that runs during a pulse is in IRAM, so it takes the same time in every build.

#define I2C_OFF_RETRIES 3
#define I2C_STUCK_MICROS 50  // A line that doesn't come up in this time is stuck

#define I2C_OK    0
#define I2C_NACK  2
#define I2C_STUCK 5

I2CStats i2cstats;
bool i2cfault = false;
uint32_t i2ccyclesperus;

static uint32_t halfbit;  // in cycles

static inline void IRAM_ATTR waitcycles(uint32_t cycles) {
  uint32_t start = i2ccycles();
  while (i2ccycles() - start < cycles) {
  }
}

static bool IRAM_ATTR releasescl() {
  sclrelease();
  uint32_t start = i2ccycles();
  while (!sclread()) {
    if (i2ccycles() - start > I2C_STUCK_MICROS * i2ccyclesperus) {
      return false;
    }
  }
  return true;
}

static bool IRAM_ATTR i2cstart() {
  if (!sdaread() || !sclread()) {
    return false;
  }
  sdalow();
  waitcycles(halfbit);
  scllow();
  return true;
}

static void IRAM_ATTR i2cstop() {
  waitcycles(halfbit / 4);
  sdalow();
  waitcycles(halfbit);
  releasescl();
  waitcycles(halfbit);
  sdarelease();
  waitcycles(halfbit);
}

// SDA only changes a while after SCL went low, the level shifters may delay them differently
static uint8_t IRAM_ATTR i2cbyte(uint8_t value) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    waitcycles(halfbit / 4);
    if (value & mask) {
      sdarelease();
    } else {
      sdalow();
    }
    waitcycles(halfbit - halfbit / 4);
    if (!releasescl()) {
      return I2C_STUCK;
    }
    waitcycles(halfbit);
    scllow();
  }
  waitcycles(halfbit / 4);
  sdarelease();
  waitcycles(halfbit - halfbit / 4);
  if (!releasescl()) {
    return I2C_STUCK;
  }
  waitcycles(halfbit);
  bool acknowledged = !sdaread();
  scllow();
  return acknowledged ? I2C_OK : I2C_NACK;
}

void i2cbegin() {
  i2ccyclesperus = getCpuFrequencyMhz();
  halfbit = i2ccyclesperus * 1000000UL / I2C_CLOCK / 2;

  pinMode(I2C_SDA, INPUT);
  pinMode(I2C_SCL, INPUT);
  sdarelease();
  sclrelease();
#ifndef SIMULATE
  REG_WRITE(GPIO_OUT_W1TC_REG, SDA_MASK | SCL_MASK);
  // A software restart keeps the GPIO matrix: the pins can still belong to the I2C peripheral
  // of the firmware before an OTA update, and then they ignore us.
  REG_WRITE(GPIO_FUNC0_OUT_SEL_CFG_REG + I2C_SDA * 4, SIG_GPIO_OUT_IDX | GPIO_FUNC0_OEN_SEL);
  REG_WRITE(GPIO_FUNC0_OUT_SEL_CFG_REG + I2C_SCL * 4, SIG_GPIO_OUT_IDX | GPIO_FUNC0_OEN_SEL);
#endif

  // A chip can be left halfway a byte by a reset, it lets go of SDA after some clocks
  for (uint8_t i = 0; i < 9 && !sdaread(); i++) {
    scllow();
    waitcycles(halfbit);
    releasescl();
    waitcycles(halfbit);
  }
  scllow();
  i2cstop();
}

bool IRAM_ATTR i2cwrite(byte address, byte reg, byte content) {
  uint8_t result = I2C_STUCK;
  if (i2cstart()) {
    result = i2cbyte(address << 1);
    if (result == I2C_OK) {
      result = i2cbyte(reg);
    }
    if (result == I2C_OK) {
      result = i2cbyte(content);
    }
    i2cstop();
  }

  i2cstats.writes++;
  if (result != I2C_OK) {
    if (result == I2C_NACK) {
      i2cstats.nack++;
    } else {
      i2cstats.timeout++;
    }
    if (content == 0x00 && reg >= 0x06 && reg <= 0x45) {
      i2cstats.offfailed++;
    }
  }
  return result == I2C_OK;
}

// An output that stays on burns a coil and its drivers: the PCA9685 has no timeout of its own.
void IRAM_ATTR i2coff(byte address, byte reg) {
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
void IRAM_ATTR emergencystop() {
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
    // A device acknowledges its address
    error = I2C_STUCK;
    if (i2cstart()) {
      error = i2cbyte(address << 1);
      i2cstop();
    }

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      Serial.print((address < 0x10) ? "0" : "");
      Serial.println(address, HEX);
      nDevices++;
    } else if (error == I2C_STUCK) {
      Serial.print("Bus stuck at address 0x");
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

#endif
