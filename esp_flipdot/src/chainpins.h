#pragma once

// Direct access to the pins of the v3 chain and the cycle counter, always available in IRAM.

#ifdef SIMULATE
#include "simulatechain.h"
#else

#include <esp_attr.h>
#include <esp_cpu.h>
#include <soc/gpio_reg.h>
#include <soc/soc.h>

#include "config.h"

static inline void IRAM_ATTR chainpin(uint8_t pin, bool high) {
  REG_WRITE(high ? GPIO_OUT_W1TS_REG : GPIO_OUT_W1TC_REG, 1UL << pin);
}
static inline void IRAM_ATTR setdata(bool high)   { chainpin(CHAIN_PIN_DATA, high); }
static inline void IRAM_ATTR setsclk(bool high)   { chainpin(CHAIN_PIN_SCLK, high); }
static inline void IRAM_ATTR setlatch(bool high)  { chainpin(CHAIN_PIN_LATCH, high); }
static inline void IRAM_ATTR setnoe(bool high)    { chainpin(CHAIN_PIN_NOE, high); }
static inline void IRAM_ATTR setdirx1(bool high)  { chainpin(CHAIN_PIN_DIR_X1, high); }
static inline void IRAM_ATTR setdirrow(bool high) { chainpin(CHAIN_PIN_DIR_ROW, high); }
static inline bool IRAM_ATTR readreturn()         { return REG_READ(GPIO_IN_REG) & (1UL << CHAIN_PIN_RETURN); }
static inline bool IRAM_ATTR readnoe()            { return REG_READ(GPIO_IN_REG) & (1UL << CHAIN_PIN_NOE); }  // The pad itself
static inline uint32_t IRAM_ATTR chaincycles()    { return esp_cpu_get_cycle_count(); }

#endif
