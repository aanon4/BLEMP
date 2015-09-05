/*
 * uuids.c
 *
 *  Created on: Mar 17, 2015
 *      Author: tim
 */

#include <ble.h>
#include <app_error.h>

#include "mesh/mesh.h"

#include "uuids.h"

uint16_t primary_service_handle;

void uuids_init(void)
{
  static const ble_uuid128_t base_uuid = { { UUIDS_BASE_UUID } };
  uint32_t err_code;
  uint8_t uuids_type;

  err_code = sd_ble_uuid_vs_add(&base_uuid, &uuids_type);
  APP_ERROR_CHECK(err_code);
  // If the following fails, change UUIDS_BASE_TYPE so it doesn't. The value remains
  // constant unless we add another uuid earlier in the code. This allows us to use a
  // known constant when creating UUIDs elsewhere, and to make them constant also.
  if (uuids_type != UUIDS_BASE_TYPE)
  {
    APP_ERROR_CHECK(NRF_ERROR_INTERNAL);
  }

  // Create the primary service
  static const ble_uuid_t service_uuid =
  {
      .type = UUIDS_BASE_TYPE,
      .uuid = MESH_SERVICE_UUID
  };
  err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &service_uuid, &primary_service_handle);
  APP_ERROR_CHECK(err_code);
}
