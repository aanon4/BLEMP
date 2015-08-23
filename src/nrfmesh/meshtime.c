/*
 * meshtime.c
 *
 *  Created on: Aug 21, 2015
 *      Author: tim
 */


#include <ble.h>
#include <nordic_common.h>
#include <ble_gatts.h>
#include <ble_hci.h>
#include <app_error.h>
#include <nrf_soc.h>

#include "nrfmesh.h"
#include "uuids.h"
#include "meshtime.h"

const Mesh_Key MESH_KEY_KEEPALIVE = _MESH_KEY_KEEPALIVE;
const Mesh_Key MESH_KEY_TIME = _MESH_KEY_TIME;

static struct
{
  uint8_t   count;
  uint8_t   keepalive;
  uint8_t   stratum;
  int32_t   timediff;
} meshtime_state =
{
  .stratum = 255
};

void meshtime_timer_handler(void)
{
  meshtime_state.count++;
  if (meshtime_state.count >= MESH_TIMESYNC_TIME / 60)
  {
    meshtime_state.count = 0;
    Mesh_TimeStratum time = {};
    Mesh_System_GetTimeStratum(&mesh_node, MESH_NODEID_SELF, &time);
    Mesh_SetValue(&mesh_node, MESH_NODEID_GLOBAL, MESH_KEY_TIME, (uint8_t*)&time, sizeof(time));
    meshtime_state.keepalive++;
    Mesh_SetValue(&mesh_node, MESH_NODEID_SELF, MESH_KEY_KEEPALIVE, (uint8_t*)&meshtime_state.keepalive, sizeof(meshtime_state.keepalive));
    Mesh_Process(&mesh_node, MESH_EVENT_KEEPALIVE, 0, 0);
  }
}

void Mesh_System_SetTimeStratum(Mesh_Node* node, Mesh_NodeId id, Mesh_TimeStratum* remotetime)
{
  if (remotetime->stratum > meshtime_state.stratum)
  {
    // Ignore worse stratum
  }
  else
  {
    Mesh_Tick remotediff = remotetime->time - Mesh_System_Tick();
    if (remotetime->stratum < meshtime_state.stratum)
    {
      // Lower stratum times are definitive
      meshtime_state.stratum = remotetime->stratum + (node->ids[id].flag.client ? 1 : 0);
      meshtime_state.timediff = remotediff;
    }
    else
    {
      // Similar stratum times are averaged with our own
      meshtime_state.timediff = (meshtime_state.timediff + remotediff) / 2;
    }
  }
}

void Mesh_System_GetTimeStratum(Mesh_Node* node, Mesh_NodeId id, Mesh_TimeStratum* localtime)
{
  localtime->stratum = meshtime_state.stratum;
  localtime->time = Mesh_System_Tick() + meshtime_state.timediff;
}

void meshtime_init(void)
{
  Mesh_TimeStratum time = {};
  Mesh_SetValue(&mesh_node, MESH_NODEID_GLOBAL, MESH_KEY_TIME, (uint8_t*)&time, sizeof(time));

#if 1
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
    .uuid = 10
  };
  static const ble_gatts_attr_t attr =
  {
    .p_uuid = (ble_uuid_t*)&uuid,
    .p_attr_md = (ble_gatts_attr_md_t*)&metadata,
    .init_len = sizeof(meshtime_state),
    .max_len = sizeof(meshtime_state),
    .p_value = (uint8_t*)&meshtime_state
  };
  static const ble_gatts_char_md_t characteristic =
  {
    .char_props.read = 1,
    .p_char_user_desc = "Time",
    .char_user_desc_max_size = 4,
    .char_user_desc_size = 4
  };
  ble_gatts_char_handles_t newhandle;
  err_code = sd_ble_gatts_characteristic_add(primary_service_handle, &characteristic, &attr, &newhandle);
  APP_ERROR_CHECK(err_code);
#endif
}
