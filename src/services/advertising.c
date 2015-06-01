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

#include "nrfmesh/nrfmesh.h"
#include "advertising.h"

/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
void advertising_init(void)
{
    uint32_t      err_code;

    ble_gap_addr_t mac;
    err_code = sd_ble_gap_address_get(&mac);

    uint8_t data[] =
    {
    	0x02, 0x15,
    	MESH_UUID,
    	mac.addr[3], mac.addr[2], mac.addr[1], mac.addr[0],
    	0xCA
    };
    uint8_t flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    ble_advdata_manuf_data_t manu_data =
    {
    	.company_identifier = 0x004C,
    	.data.p_data = data,
    	.data.size = sizeof(data)
    };
    ble_advdata_t adv_data =
    {
        .flags.p_data = &flags,
        .flags.size = sizeof(flags),
    	.p_manuf_specific_data = &manu_data
    };
    ble_advdata_t scan_rsp =
    {
    	.name_type = BLE_ADVDATA_SHORT_NAME,
    	.short_name_len = 8
    };

    err_code = ble_advdata_set(&adv_data, &scan_rsp);
    APP_ERROR_CHECK(err_code);
}

void advertising_start(void)
{
    uint32_t err_code;

    // Start advertising
    ble_gap_adv_params_t adv_params =
    {
		.type        = BLE_GAP_ADV_TYPE_ADV_IND,
		.fp          = BLE_GAP_ADV_FP_ANY,
		.interval    = MSEC_TO_UNITS(APP_ADV_INTERVAL, UNIT_0_625_MS),
		.timeout     = APP_ADV_TIMEOUT_IN_SECONDS
    };

    err_code = sd_ble_gap_adv_start(&adv_params);
    APP_ERROR_CHECK(err_code);
}

void advertising_stop(void)
{
    uint32_t err_code;

    err_code = sd_ble_gap_adv_stop();
    APP_ERROR_CHECK(err_code);
}
