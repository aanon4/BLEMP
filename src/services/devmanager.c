/*
 * devmanager.c
 *
 *  Created on: Mar 11, 2015
 *      Author: tim
 */

#include <app_error.h>

#include "devmanager.h"

static dm_application_instance_t g_AppHandle; // Application identifier allocated by device manager.


/**@brief Function for handling the Device Manager events.
 *
 * @param[in]   p_evt   Data associated to the device manager event.
 */
static uint32_t device_manager_evt_handler(dm_handle_t const    * p_handle,
                                           dm_event_t const     * p_event,
                                           api_result_t           event_result)
{
    APP_ERROR_CHECK(event_result);
    return NRF_SUCCESS;
}

/**@brief Function for the Device Manager initialization.
 */
void device_manager_init(void)
{
    uint32_t err_code;

    // Initialize persistent storage module.
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    // Clear all bonded centrals if the Bonds Delete button is pushed.
    dm_init_param_t init_data =
    {
    	.clear_persistent_data = true
    };

    err_code = dm_init(&init_data);
    APP_ERROR_CHECK(err_code);

    dm_application_param_t  register_param =
    {
		.sec_param.timeout      = SEC_PARAM_TIMEOUT,
		.sec_param.bond         = SEC_PARAM_BOND,
		.sec_param.mitm         = SEC_PARAM_MITM,
		.sec_param.io_caps      = SEC_PARAM_IO_CAPABILITIES,
		.sec_param.oob          = SEC_PARAM_OOB,
		.sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE,
		.sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE,
		.evt_handler            = device_manager_evt_handler,
		.service_type           = DM_PROTOCOL_CNTXT_NONE,
    };

    err_code = dm_register(&g_AppHandle, &register_param);
    APP_ERROR_CHECK(err_code);
}
