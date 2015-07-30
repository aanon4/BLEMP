/*
 * meshkeepalive.c
 *
 *  Created on: Mar 25, 2015
 *      Author: tim
 */

#include <string.h>

#include <ble.h>

#if defined(TESTING_KEEPALIVE)

#include <nordic_common.h>
#include <ble_gatts.h>
#include <ble_hci.h>
#include <app_error.h>
#include <nrf_soc.h>

#include "nrfmesh.h"
#include "uuids.h"
#include "keepalive.h"

static uint16_t keepalive_connection;
static uint16_t keepalive_handle;

#endif

const Mesh_Key MESH_KEY_KEEPALIVE = _MESH_KEY_KEEPALIVE;


static uint8_t keepalive_count;

void meshkeepalive_timer_handler(void)
{
	keepalive_count++;
	if (keepalive_count >= MESH_KEEPALIVE_TIME / 60)
	{
		keepalive_count = 0;
		uint8_t data = 0;
		uint8_t length = sizeof(data);
		Mesh_GetValue(&mesh_node, MESH_NODEID_SELF, MESH_KEY_KEEPALIVE, &data, &length);
		data++;
		Mesh_SetValue(&mesh_node, MESH_NODEID_SELF, MESH_KEY_KEEPALIVE, &data, length);
		Mesh_Process(&mesh_node, MESH_EVENT_KEEPALIVE, 0, 0);
	}
}

void meshkeepalive_init(void)
{
	uint32_t err_code;

	uint8_t data = 0;
	Mesh_SetValue(&mesh_node, MESH_NODEID_SELF, MESH_KEY_KEEPALIVE, &data, sizeof(data));


#if defined(TESTING_KEEPALIVE)
	static const ble_gatts_attr_md_t metadata =
	{
		.read_perm = { 1, 1 },
		.write_perm = { 1, 1 },
		.rd_auth = 1,
		.wr_auth = 1,
		.vlen = 1,
		.vloc = BLE_GATTS_VLOC_STACK
	};
	static const ble_uuid_t uuid =
	{
		.type = UUIDS_BASE_TYPE,
		.uuid = MESH_KEEPALIVE_UUID
	};
	static const ble_gatts_attr_t attr =
	{
		.p_uuid = (ble_uuid_t*)&uuid,
		.p_attr_md = (ble_gatts_attr_md_t*)&metadata,
		.init_len = 0,
		.max_len = MESH_MAX_READ_SIZE
	};
	static const ble_gatts_char_md_t characteristic =
	{
		.char_props.read = 1,
		.p_char_user_desc = "Keepalive",
		.char_user_desc_max_size = 9,
		.char_user_desc_size = 9
	};
	ble_gatts_char_handles_t newhandle;
	err_code = sd_ble_gatts_characteristic_add(primary_service_handle, &characteristic, &attr, &newhandle);
	APP_ERROR_CHECK(err_code);
	keepalive_handle = newhandle.value_handle;
#endif
}

#if defined(TESTING_KEEPALIVE)

void meshkeepalive_ble_event(ble_evt_t* event)
{
	uint32_t err_code;

	switch (event->header.evt_id)
	{
	case BLE_GAP_EVT_CONNECTED:
		keepalive_connection = event->evt.gap_evt.conn_handle;
		break;

	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		if (event->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_READ && event->evt.gatts_evt.params.authorize_request.request.read.handle == keepalive_handle)
		{
			uint8_t i = 0;
			uint8_t keepalives[mesh_node.values.count];
			for (Mesh_UKV* v = mesh_node.values.values; v < &mesh_node.values.values[mesh_node.values.count]; v++)
			{
				if (memcmp(&v->key, &MESH_KEY_KEEPALIVE, sizeof(Mesh_Key)) == 0)
				{
					keepalives[i++] = MESH_UKV_VALUE(v)[0];
				}
			}

			ble_gatts_rw_authorize_reply_params_t reply =
			{
				.type = BLE_GATTS_AUTHORIZE_TYPE_READ,
				.params.read =
				{
					.gatt_status = BLE_GATT_STATUS_SUCCESS,
					.update = 1,
					.offset = 0,
					.len = i,
					.p_data = keepalives
				}
			};
			err_code = sd_ble_gatts_rw_authorize_reply(keepalive_connection, &reply);
			APP_ERROR_CHECK(err_code);
		}
		break;
	}
}

#endif

