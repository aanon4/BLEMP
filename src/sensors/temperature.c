/*
 * temperature.c
 *
 *  Created on: Mar 16, 2015
 *      Author: tim
 */

#include <nordic_common.h>
#include <ble.h>

#include "nrfmesh/nrfmesh.h"
#include "meshkeys.h"

#include "services/timer.h"
#include "services/i2c.h"

#include "temperature.h"


static app_timer_id_t timer;

static void temperature_measure(void* __ignore__, uint16_t __size__)
{
	static const uint8_t select_temp[] = { 0x00 };
	uint8_t temperature_value[2] = {};

	i2c_write(TEMPERATURE_ADDRESS, (uint8_t*)select_temp, sizeof(select_temp), true);
	i2c_read(TEMPERATURE_ADDRESS, temperature_value, sizeof(temperature_value), true);

	Mesh_SetValue(&mesh_node, MESH_NODEID_SELF, MESH_KEY_TEMPERATURE, temperature_value, sizeof(temperature_value));
}

static void temperature_measure_irq(void* __ignore__)
{
	app_sched_event_put(NULL, 0, temperature_measure);
}

void temperature_timer_handler(void)
{
	static const uint8_t initiate_read[] = { 0x01, 0xE1 };
	uint32_t err_code;

	i2c_write(TEMPERATURE_ADDRESS, (uint8_t*)initiate_read, sizeof(initiate_read), true);

	err_code = app_timer_start(timer, MS_TO_TICKS(TEMPERATURE_WAIT_MS), NULL);
	APP_ERROR_CHECK(err_code);
}

void temperature_init(void)
{
	static const uint8_t setup[] = { 0x01, 0x61 };
	uint32_t err_code;

	// Setup the temperature sensor
	i2c_write(TEMPERATURE_ADDRESS, (uint8_t*)setup, sizeof(setup), true);

	// We need a one-shot timer to read the temp because we have to wait between initiating a read
	// and being able to get the result
	err_code = app_timer_create(&timer, APP_TIMER_MODE_SINGLE_SHOT, temperature_measure_irq);
	APP_ERROR_CHECK(err_code);

	// Read temp (so we have an initial value)
	temperature_timer_handler();
}
