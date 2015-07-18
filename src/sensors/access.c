/*
 * access.c
 *
 *  Created on: Apr 2, 2015
 *      Author: tim
 */

#include <string.h>

#include <app_error.h>
#include <nordic_common.h>
#include <ble.h>
#include <ble_gatts.h>

#include "nrfmesh/nrfmesh.h"

#include "sensoruuids.h"
#include "temperature.h"
#include "access.h"

#define	MAX_PKT_SIZE	(GATT_MTU_SIZE_DEFAULT - 1)

static uint16_t access_handle;
static uint16_t access_attr_handle;

void access_init(void)
{
	uint32_t err_code;

	// Add  service
	static const ble_gatts_attr_md_t metadata =
	{
		.read_perm = { 1, 1 },
		.write_perm = { 1, 1 },
		.rd_auth = 1,
		.vlen = 1,
		.vloc = BLE_GATTS_VLOC_STACK
	};
	static const ble_uuid_t uuid =
	{
		.type = UUIDS_BASE_TYPE,
		.uuid = ACCESS_ACCESS_UUID
	};
	static const ble_gatts_attr_t attr =
	{
		.p_uuid = (ble_uuid_t*)&uuid,
		.p_attr_md = (ble_gatts_attr_md_t*)&metadata,
		.init_len = 0,
		.max_len = BLE_GATTS_VAR_ATTR_LEN_MAX
	};
	static const ble_gatts_char_md_t characteristic =
	{
		.char_props.read = 1,
		.p_char_user_desc = "Access",
		.char_user_desc_max_size = 6,
		.char_user_desc_size = 6
	};
	ble_gatts_char_handles_t newhandle;
	err_code = sd_ble_gatts_characteristic_add(primary_service_handle, &characteristic, &attr, &newhandle);
	APP_ERROR_CHECK(err_code);
	access_attr_handle = newhandle.value_handle;
}

void access_ble_event(ble_evt_t* event)
{
	uint32_t err_code;

	switch (event->header.evt_id)
	{
	case BLE_GAP_EVT_CONNECTED:
		access_handle = event->evt.gap_evt.conn_handle;
		break;

	case BLE_GAP_EVT_DISCONNECTED:;
		access_handle = BLE_CONN_HANDLE_INVALID;
		break;

	case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
		if (event->evt.gatts_evt.params.authorize_request.request.read.handle == access_attr_handle && event->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_READ)
		{
			struct Entry
			{
			  Mesh_NodeAddress address;
			  uint8_t temperature[2];
			};
			uint8_t buffer[3 * sizeof(struct Entry)];

			uint16_t offset = event->evt.gatts_evt.params.authorize_request.request.read.offset;
			uint8_t idx = offset / sizeof(struct Entry);
			struct Entry* ptr = (struct Entry*)buffer;
			for (; (uint8_t*)ptr < &buffer[sizeof(buffer)]; ptr++, idx++)
			{
				Mesh_NodeId id;
				uint8_t len;
				len = sizeof(ptr->temperature);
				if (Mesh_GetNthValue(&mesh_node, MESH_KEY_TEMPERATURE, idx, &id, ptr->temperature, &len) != MESH_OK)
				{
					break;
				}
				memmove(&ptr->address, Mesh_GetNodeAddress(&mesh_node, id), sizeof(Mesh_NodeAddress));
			}
			uint8_t* start = buffer + offset % sizeof(struct Entry);
			ble_gatts_rw_authorize_reply_params_t reply =
			{
			  .type = BLE_GATTS_AUTHORIZE_TYPE_READ,
			  .params.read =
			  {
				.gatt_status = BLE_GATT_STATUS_SUCCESS,
				.update = 1,
				.offset = offset,
				.len = MAX(0, MIN(MAX_PKT_SIZE, (uint8_t*)ptr - start)),
				.p_data = start
			  }
			};
			err_code = sd_ble_gatts_rw_authorize_reply(access_handle, &reply);
			APP_ERROR_CHECK(err_code);
		}
		break;

	default:
		break;
	}
}

