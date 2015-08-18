/*
 * statistics.c
 *
 *  Created on: Apr 25, 2015
 *      Author: tim
 */

#include <nordic_common.h>
#include <ble.h>
#include <ble_gatts.h>
#include <ble_hci.h>
#include <app_error.h>
#include <nrf_soc.h>
#include <app_timer.h>

#include "uuids.h"
#include "statistics.h"

#if defined(INCLUDE_STATISTICS)

static uint16_t statistics_connection;
struct statistics stats;
struct statistics_timer stats_timer;


void statistics_init(void)
{
	uint32_t err_code;

	static const ble_gatts_attr_md_t metadata =
	{
		.read_perm = { 1, 2 },
		.vlen = 1,
		.vloc = BLE_GATTS_VLOC_USER
	};
	static const ble_uuid_t uuid =
	{
		.type = UUIDS_BASE_TYPE,
		.uuid = MESH_STATISTICS_UUID
	};
	static const ble_gatts_attr_t attr =
	{
		.p_uuid = (ble_uuid_t*)&uuid,
		.p_attr_md = (ble_gatts_attr_md_t*)&metadata,
		.init_len = sizeof(stats),
		.max_len = sizeof(stats),
		.p_value = (uint8_t*)&stats
	};
	static const ble_gatts_char_md_t characteristic =
	{
		.char_props.read = 1,
		.p_char_user_desc = "Stats",
		.char_user_desc_max_size = 5,
		.char_user_desc_size = 5
	};
	ble_gatts_char_handles_t newhandle;
	err_code = sd_ble_gatts_characteristic_add(primary_service_handle, &characteristic, &attr, &newhandle);
	APP_ERROR_CHECK(err_code);
}

uint32_t statistics_get_time(void)
{
	uint32_t err_code;
	uint32_t ticks;
	err_code = app_timer_cnt_get(&ticks);
	APP_ERROR_CHECK(err_code);
	return ticks;
}

#endif

