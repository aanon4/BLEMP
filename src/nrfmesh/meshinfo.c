/*
 * meshinfo.c
 *
 *  Created on: Jul 29, 2015
 *      Author: tim
 */

#include <ble.h>

#include "nrfmesh.h"
#include "meshinfo.h"

#include "services/advertising.h"

static const Mesh_Key MESH_KEY_INFO = _MESH_KEY_INFO;

void meshinfo_init(void)
{
  static const struct
  {
    char key_advertising_interval;
    unsigned value_advertising_interval;
    char key_power_source;
    char value_power_source;
  } __attribute__((packed)) info =
  {
    MESHINFO_KEY_ADVERTISING_INTERVAL,
    APP_ADV_INTERVAL,
    MESHINFO_KEY_POWER_SOURCE,
    1,
  };
  Mesh_SetValue(&mesh_node, MESH_NODEID_SELF, MESH_KEY_INFO, (unsigned char*)&info, sizeof(info));
}
