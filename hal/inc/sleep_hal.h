/*
 * Copyright (c) 2019 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHAN'TABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
 
 #ifndef __SLEEP_HAL_H
 #define __SLEEP_HAL_H

#include "hal_platform.h"

#if HAL_PLATFORM_SLEEP20

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "system_tick_hal.h"
#include "system_defs.h"
#include "interrupts_hal.h"
#include "platforms.h"
#include "assert.h"

#define HAL_SLEEP_VERSION 2

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Stop mode:
 *     What's disabled: The resources occupied by system, e.g. CPU, RGB, external flash etc.
 *     Wakeup sources: Any source that the platform supported.
 *     On-exit: It resumes the disabled resources and continue running.
 * 
 * Ultra-Low power mode:
 *     What's disabled: The resources occupied by system and all other sources those are not featured as wakeup source.
 *     Wakeup sources: Any source that the platform supported
 *     On-exit: It resumes the disabled resources (by restoring peripherals' configuration) and network connection if necessary and continue running.
 * 
 * Hibernate mode:
 *     What's disabled: Most of resources except particular pins and retention RAM
 *     Wakeup sources: Particular pins
 *     On-exit: Reset
 */
typedef enum hal_sleep_mode_t {
    HAL_SLEEP_MODE_NONE = 0,
    HAL_SLEEP_MODE_STOP = 1,
    HAL_SLEEP_MODE_ULTRA_LOW_POWER = 2,
    HAL_SLEEP_MODE_HIBERNATE = 3,
    HAL_SLEEP_MODE_MAX = 0x7F
} hal_sleep_mode_t;

// Bit mask enum value.
typedef enum hal_wakeup_source_type_t {
    HAL_WAKEUP_SOURCE_TYPE_UNKNOWN = 0x00,
    HAL_WAKEUP_SOURCE_TYPE_GPIO = 0x01,
    HAL_WAKEUP_SOURCE_TYPE_ADC = 0x02,
    HAL_WAKEUP_SOURCE_TYPE_DAC = 0x04,
    HAL_WAKEUP_SOURCE_TYPE_RTC = 0x08,
    HAL_WAKEUP_SOURCE_TYPE_LPCOMP = 0x10,
    HAL_WAKEUP_SOURCE_TYPE_UART = 0x20,
    HAL_WAKEUP_SOURCE_TYPE_I2C = 0x40,
    HAL_WAKEUP_SOURCE_TYPE_SPI = 0x80,
    HAL_WAKEUP_SOURCE_TYPE_TIMER = 0x100,
    HAL_WAKEUP_SOURCE_TYPE_CAN = 0x200,
    HAL_WAKEUP_SOURCE_TYPE_USB = 0x400,
    HAL_WAKEUP_SOURCE_TYPE_BLE = 0x800,
    HAL_WAKEUP_SOURCE_TYPE_NFC = 0x1000,
    HAL_WAKEUP_SOURCE_TYPE_NETWORK = 0x2000,
    HAL_WAKEUP_SOURCE_TYPE_MAX = 0x7FFFFFFF
} hal_wakeup_source_type_t;

typedef enum hal_sleep_wait_t {
    HAL_SLEEP_WAIT_NO_WAIT = 0,
    HAL_SLEEP_WAIT_CLOUD = 1,
    HAL_SLEEP_WAIT_MAX = 0x7F,
} hal_sleep_wait_t;

#if PLATFORM_ID > PLATFORM_GCC
static_assert(sizeof(hal_sleep_mode_t) == 1, "length of hal_sleep_mode_t should be 1-bytes aligned.");
static_assert(sizeof(hal_wakeup_source_type_t) == 4, "length of hal_wakeup_source_type_t should be 4-bytes aligned.");
static_assert(sizeof(hal_sleep_wait_t) == 1, "length of hal_sleep_wait_t should be 1-bytes aligned.");
#endif

/**
 * HAL sleep wakeup source base
 */
typedef struct hal_wakeup_source_base_t hal_wakeup_source_base_t;
typedef struct hal_wakeup_source_base_t {
    uint16_t size;
    uint16_t version;
    hal_wakeup_source_type_t type;
    hal_wakeup_source_base_t* next;
} hal_wakeup_source_base_t;

/**
 * HAL sleep wakeup source: GPIO
 */
typedef struct hal_wakeup_source_gpio_t {
    hal_wakeup_source_base_t base; // This must come first in order to use casting.
    uint16_t pin;
    InterruptMode mode; // Caution: This might not be 1-byte length, depending on linker options.
    uint8_t reserved;
} hal_wakeup_source_gpio_t;

/**
 * HAL sleep wakeup source: RTC
 */
typedef struct hal_wakeup_source_rtc_t {
    hal_wakeup_source_base_t base; // This must come first in order to use casting.
    system_tick_t ms;
} hal_wakeup_source_rtc_t;

/**
 * HAL sleep wakeup source: network
 */
typedef struct hal_wakeup_source_network_t {
    hal_wakeup_source_base_t base; // This must come first in order to use casting.
    network_interface_index index;
} hal_wakeup_source_network_t;

/**
 * HAL sleep configuration: speicify sleep mode and wakeup sources.
 */
typedef struct hal_sleep_config_t {
    uint16_t size;
    uint16_t version;
    hal_sleep_mode_t mode;
    hal_sleep_wait_t wait;
    uint16_t reserved;
    hal_wakeup_source_base_t* wakeup_sources;
} hal_sleep_config_t;

/**
 * Check if the given sleep configuration is valid or not.
 *
 * @param[in]     config          Sleep configuration that specifies sleep mode, wakeup sources etc.
 *
 * @returns     System error code.
 */
int hal_sleep_validate_config(const hal_sleep_config_t* config, void* reserved);

/**
 * Makes the device enter one of supported sleep modes.
 *
 * @param[in]     config          Sleep configuration that specifies sleep mode, wakeup sources etc.
 * @param[in,out] wakeup_source   Pointer to the wakeup source structure, which is allocated in heap.
 *                                It is caller's responsibility to free this piece of memory.
 *
 * @returns     System error code.
 */
int hal_sleep_enter(const hal_sleep_config_t* config, hal_wakeup_source_base_t** wakeup_source, void* reserved);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HAL_PLATFORM_SLEEP20

#endif /* __SLEEP_HAL_H */