// Copyright (c) Acconeer AB, 2019-2022
// All rights reserved
// This file is subject to the terms and conditions defined in the file
// 'LICENSES/license_acconeer.txt', (BSD 3-Clause License) which is part
// of this source code package.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "acc_definitions_common.h"
#include "acc_hal_integration.h"
#include "acc_integration.h"
#include "acc_integration_log.h"
#include "acc_libgpiod.h"
#include "acc_libspi.h"


#define SENSOR_COUNT (1)               /**< @brief The number of sensors available on the board */

#define PIN_SENSOR_INTERRUPT (25)      /**< @brief Gpio Interrupt Sensor BCM:25 J5:22, connect to sensor GPIO 5 */
#define PIN_SENSOR_ENABLE    (27)      /**< @brief SPI Sensor enable BCM:27 J5:13 */

#define ACC_BOARD_REF_FREQ  (26000000) /**< @brief The reference frequency assumes 26 MHz on reference board */
#define ACC_BOARD_SPI_SPEED (15000000) /**< @brief The SPI speed of this board */
#define ACC_BOARD_BUS       (0)        /**< @brief The SPI bus of this board */
#define ACC_BOARD_CS        (0)        /**< @brief The SPI device of the board */


/**
 * @brief Sensor states
 */
typedef enum
{
	SENSOR_DISABLED,
	SENSOR_ENABLED,
} acc_board_sensor_state_t;


static acc_board_sensor_state_t sensor_state = SENSOR_DISABLED;

static const gpio_config_t pin_config[] =
{
	{PIN_SENSOR_INTERRUPT, GPIO_DIR_INPUT_INTERRUPT},
	{PIN_SENSOR_ENABLE, GPIO_DIR_OUTPUT_LOW},
	{0, GPIO_DIR_UNKNOWN},
};

static uint32_t spi_speed = ACC_BOARD_SPI_SPEED;


static void board_deinit(void)
{
	acc_libgpiod_deinit();

	acc_libspi_deinit();
}


static bool acc_board_gpio_init(void)
{
	static bool init_done = false;

	if (init_done)
	{
		return true;
	}

	if (!acc_libgpiod_init(pin_config))
	{
		fprintf(stderr, "Unable to initialize gpio\n");
		return false;
	}

	init_done = true;
	return true;
}


static bool acc_board_init(void)
{
	static bool init_done = false;
	bool        result    = true;

	if (init_done)
	{
		return true;
	}

	if (atexit(board_deinit))
	{
		fprintf(stderr, "Unable to set exit function 'board_deinit()'\n");
		result = false;
	}

	if (result)
	{
		result = acc_libspi_init();
	}

	if (result)
	{
		init_done = true;
	}

	return result;
}


static void acc_board_start_sensor(acc_sensor_id_t sensor)
{
	(void)sensor;

	if (sensor_state != SENSOR_DISABLED)
	{
		return;
	}

	if (!acc_libgpiod_set(PIN_SENSOR_ENABLE, PIN_HIGH))
	{
		fprintf(stderr, "%s: Unable to activate enable_pin for sensor.\n", __func__);
		assert(false);
	}

	acc_integration_sleep_ms(5);
	sensor_state = SENSOR_ENABLED;
}


static void acc_board_stop_sensor(acc_sensor_id_t sensor)
{
	(void)sensor;

	if (sensor_state != SENSOR_DISABLED)
	{
		// Disable sensor
		if (!acc_libgpiod_set(PIN_SENSOR_ENABLE, PIN_LOW))
		{
			fprintf(stderr, "%s: Unable to deactivate enable_pin for sensor.\n", __func__);
			assert(false);
		}

		sensor_state = SENSOR_DISABLED;
	}
}


static bool acc_board_wait_for_sensor_interrupt(acc_sensor_id_t sensor_id, uint32_t timeout_ms)
{
	(void)sensor_id;
	return acc_libgpiod_wait_for_interrupt(PIN_SENSOR_INTERRUPT, timeout_ms);
}


static float acc_board_get_ref_freq(void)
{
	return ACC_BOARD_REF_FREQ;
}


static void acc_board_sensor_transfer(acc_sensor_id_t sensor_id, uint8_t *buffer, size_t buffer_length)
{
	(void)sensor_id;

	bool result = acc_libspi_transfer(spi_speed, buffer, buffer_length);
	assert(result);
}


const acc_hal_t *acc_hal_integration_get_implementation(void)
{
	if (!acc_board_init())
	{
		return NULL;
	}

	if (!acc_board_gpio_init())
	{
		return NULL;
	}

	static acc_hal_t hal =
	{
		.properties.sensor_count          = SENSOR_COUNT,
		.properties.max_spi_transfer_size = MAX_SPI_TRANSFER_SIZE,

		.sensor_device.power_on                = acc_board_start_sensor,
		.sensor_device.power_off               = acc_board_stop_sensor,
		.sensor_device.wait_for_interrupt      = acc_board_wait_for_sensor_interrupt,
		.sensor_device.transfer                = acc_board_sensor_transfer,
		.sensor_device.get_reference_frequency = acc_board_get_ref_freq,

		.sensor_device.hibernate_enter = NULL,
		.sensor_device.hibernate_exit  = NULL,

		.os.mem_alloc = malloc,
		.os.mem_free  = free,
		.os.gettime   = acc_integration_get_time,

		.log.log_level = ACC_LOG_LEVEL_INFO,
		.log.log       = acc_integration_log,

		.optimization.transfer16 = NULL,
	};

	return &hal;
}
