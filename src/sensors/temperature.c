/*
 * temperature.c
 *
 *  Created on: Mar 16, 2015
 *      Author: tim
 */

#include <nordic_common.h>
#include <ble.h>
#include <nrf_soc.h>

#include "nrfmesh/nrfmesh.h"

#include "services/timer.h"

#include "temperature.h"

const Mesh_Key MESH_KEY_TEMPERATURE = { .key = 0x0010, .wrlocal = 1 };


void temperature_timer_handler(void)
{
  uint32_t err_code;

  int32_t temperature_value;
  err_code = sd_temp_get(&temperature_value);
  APP_ERROR_CHECK(err_code);

  temperature_value *= 25; // Convert 0.25C units to 0.01 units
  Mesh_SetValue(&mesh_node, MESH_NODEID_SELF, MESH_KEY_TEMPERATURE, (uint8_t*)&temperature_value, sizeof(temperature_value));
}

void temperature_init(void)
{
	// Read temp (so we have an initial value)
  temperature_timer_handler();
}
