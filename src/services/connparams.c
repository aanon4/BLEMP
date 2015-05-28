/*
 * connparams.c
 *
 *  Created on: Mar 11, 2015
 *      Author: tim
 */

#include <app_util.h>
#include <app_timer.h>
#include <app_error.h>
#include <device_manager.h>

#include "connparams.h"

/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in]   p_evt   Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
}

/**@brief Function for initializing the Connection Parameters module.
 */
void conn_params_init(void)
{
#if 0
    uint32_t               err_code;
    ble_conn_params_init_t conparam;

    memset(&conparam, 0, sizeof(conparam));

    conparam.p_conn_params                  = NULL;
    conparam.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    conparam.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    conparam.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    conparam.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    conparam.disconnect_on_fail             = true;
    conparam.evt_handler                    = on_conn_params_evt;
    conparam.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&conparam);
    APP_ERROR_CHECK(err_code);
#endif
}
