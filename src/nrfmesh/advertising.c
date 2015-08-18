/*
 * advertising.c
 *
 *  Created on: Mar 11, 2015
 *      Author: tim
 */

#include <ble.h>
#include <nrf.h>
#include <ble_srv_common.h>
#include <ble_advdata.h>
#include <app_error.h>

#include "mesh/mesh.h"

#include "uuids.h"
#include "advertising.h"

static uint8_t advertising_id[4];
static uint8_t advertising_set;


void advertising_init(void)
{
  uint32_t      err_code;

  ble_gap_addr_t mac;
  err_code = sd_ble_gap_address_get(&mac);
  APP_ERROR_CHECK(err_code);

  advertising_id[0] = mac.addr[3];
  advertising_id[1] = mac.addr[2];
  advertising_id[2] = mac.addr[1];
  advertising_id[3] = mac.addr[0];
}

void advertising_set_0(void)
{
  uint32_t err_code;

  uint8_t data[] =
  {
    0x02, 0x15,
    REVERSE_UUID(MESH_SERVICE_BASE_UUID),
    advertising_id[0], advertising_id[1], advertising_id[2], advertising_id[3],
    0xCA
  };
  ble_advdata_manuf_data_t manu_data =
  {
    .company_identifier = 0x004C,
    .data =
    {
      .p_data = data,
      .size = sizeof(data)
    }
  };
  ble_advdata_t adv_data =
  {
    .flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
    .p_manuf_specific_data = &manu_data
  };

  ble_advdata_service_data_t service_data[] =
  {
    {
      .service_uuid = 0,
      .data =
      {
        .size = sizeof(advertising_id),
        .p_data = advertising_id
      }
    }
  };
  ble_advdata_t scan_data =
  {
    .p_service_data_array = service_data,
    .service_data_count = 1
  };

  err_code = ble_advdata_set(&adv_data, &scan_data);
  APP_ERROR_CHECK(err_code);

  advertising_set = 0;
}

void advertising_set_1(void)
{
  uint32_t err_code;

  ble_advdata_service_data_t service_data[] =
  {
    {
      .service_uuid = 1,
      .data =
      {
        .size = sizeof(advertising_id),
        .p_data = advertising_id
      }
    }
  };
  ble_advdata_t scan_data =
  {
    .p_service_data_array = service_data,
    .service_data_count = 1
  };

  err_code = ble_advdata_set(NULL, &scan_data);
  APP_ERROR_CHECK(err_code);

  advertising_set = 1;
}

void advertising_start(void)
{
  uint32_t err_code;

  // Start advertising
  ble_gap_adv_params_t adv_params =
  {
    .type        = BLE_GAP_ADV_TYPE_ADV_IND,
    .fp          = BLE_GAP_ADV_FP_ANY,
    .interval    = MSEC_TO_UNITS(advertising_set == 0 ? MESH_ADVERTISING_PERIOD : MESH_ADVERTISING_FAST_PERIOD, UNIT_0_625_MS),
    .timeout     = APP_ADV_TIMEOUT_IN_SECONDS
  };

  err_code = sd_ble_gap_adv_start(&adv_params);
  APP_ERROR_CHECK(err_code);
}

void advertising_stop(void)
{
  sd_ble_gap_adv_stop();
}
