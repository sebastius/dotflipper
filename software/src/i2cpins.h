#pragma once

// Direct access to the I2C pins and the cycle counter, always available in IRAM.
// The output register stays 0, so enabling the output pulls a line low and
// disabling it lets the pull-up take over: that's how open drain works.

#ifdef SIMULATE
#include "simulatepins.h"
#else

#include <esp_attr.h>
#include <esp_cpu.h>
#include <soc/gpio_reg.h>
#include <soc/gpio_sig_map.h>
#include <soc/soc.h>

#include "config.h"

#define SDA_MASK (1UL << I2C_SDA)
#define SCL_MASK (1UL << I2C_SCL)

static inline void IRAM_ATTR sdalow()     { REG_WRITE(GPIO_ENABLE_W1TS_REG, SDA_MASK); }
static inline void IRAM_ATTR sdarelease() { REG_WRITE(GPIO_ENABLE_W1TC_REG, SDA_MASK); }
static inline void IRAM_ATTR scllow()     { REG_WRITE(GPIO_ENABLE_W1TS_REG, SCL_MASK); }
static inline void IRAM_ATTR sclrelease() { REG_WRITE(GPIO_ENABLE_W1TC_REG, SCL_MASK); }
static inline bool IRAM_ATTR sdaread()    { return REG_READ(GPIO_IN_REG) & SDA_MASK; }
static inline bool IRAM_ATTR sclread()    { return REG_READ(GPIO_IN_REG) & SCL_MASK; }
static inline uint32_t IRAM_ATTR i2ccycles() { return esp_cpu_get_cycle_count(); }

#endif
