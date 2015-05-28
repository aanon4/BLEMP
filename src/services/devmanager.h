/*
 * devmanager.h
 *
 *  Created on: Mar 11, 2015
 *      Author: tim
 */

#ifndef DEVMANAGER_H_
#define DEVMANAGER_H_

#include <device_manager.h>
#include <pstorage.h>

#define SEC_PARAM_TIMEOUT                30 // Timeout for Pairing Request or Security Request (in seconds).
#define SEC_PARAM_BOND                   1  // Perform bonding.
#define SEC_PARAM_MITM                   0  // Man In The Middle protection not required.
#define SEC_PARAM_IO_CAPABILITIES        BLE_GAP_IO_CAPS_NONE // No I/O capabilities.
#define SEC_PARAM_OOB                    0 // Out Of Band data not available.
#define SEC_PARAM_MIN_KEY_SIZE           7 // Minimum encryption key size.
#define SEC_PARAM_MAX_KEY_SIZE           16 // Maximum encryption key size.


extern void device_manager_init(void);

#endif /* DEVMANAGER_H_ */
