/*
 ============================================================================
 Name        : main.c
 Author      : Tim Wilkinson
 Version     :
 Copyright   : (c) Tim Wilkinson
 Description : Hello World in C
 ============================================================================
 */

#include <softdevice_handler.h>
#include <pstorage.h>

#include "services/gpio.h"
#include "services/i2c.h"
#include "services/timer.h"
#include "services/scheduler.h"
#include "services/advertising.h"
#include "services/gap.h"
#include "services/devinfo.h"
#include "services/connparams.h"
#include "services/oneminutetimer.h"

#include "nrfmesh/uuids.h"
#include "nrfmesh/nrfmesh.h"
#include "nrfmesh/statistics.h"
#include "nrfmesh/keepalive.h"

#include "sensors/temperature.h"
#include "sensors/access.h"



/**@brief Function for error handling, which is called when an error has occurred.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name.
 */
void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    // This call can be used for debug purposes during application development.
    // @note CAUTION: Activating this code will write the stack to flash on an error.
    //                This function should NOT be used in a final product.
    //                It is intended STRICTLY for development/debugging purposes.
    //                The flash write will happen EVEN if the radio is active, thus interrupting
    //                any communication.
    //                Use with care. Un-comment the line below to use.
    // ble_debug_assert_handler(error_code, line_num, p_file_name);

    // On assert, the system can only recover on reset.
   NVIC_SystemReset();
}

/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t* p_ble_evt)
{
  ble_conn_params_on_ble_evt(p_ble_evt);
  nrfmesh_ble_event(p_ble_evt);
  access_ble_event(p_ble_evt);
}

/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    pstorage_sys_event_handler(sys_evt);
}

void Mesh_System_ValueChanged(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, uint8_t* value, uint8_t length)
{
}

/*
 * The beginning ...
 */
int main(void)
{
	uint32_t err_code;

	gpio_init();
	scheduler_init();
	// NB: If I put this init macro in it's one function in it's own service file, things stop working. No idea why :-(
  APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);

	// Initialize the SoftDevice handler module.
	SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, true);

	// Enable BLE stack
	ble_enable_params_t ble_enable_params =
	{
		.gatts_enable_params.service_changed = 1
	};
	err_code = sd_ble_enable(&ble_enable_params);
	APP_ERROR_CHECK(err_code);

	// Register with the SoftDevice handler module for BLE events.
	err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
	APP_ERROR_CHECK(err_code);

	// Register with the SoftDevice handler module for System events.
	err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
	APP_ERROR_CHECK(err_code);

	// Power setup
	sd_ble_gap_tx_power_set(TX_POWER);

	// Service setup
	gap_params_init();
	devinfo_init();
	advertising_init();
	conn_params_init();
	i2c_init();
	oneminutetimer_init();

	// Mesh setup
  nrfmesh_init();

  // Sensor setup
	temperature_init();
	access_init();

	// Begin ...
	advertising_start();
	while (1)
	{
		app_sched_execute();
		err_code = sd_app_evt_wait();
		APP_ERROR_CHECK(err_code);
	}

	return 0;
}
