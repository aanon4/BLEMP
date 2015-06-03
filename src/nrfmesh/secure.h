/*
 * secure.h
 *
 *  Created on: Jun 3, 2015
 *      Author: tim
 */

#ifndef NRFMESH_SECURE_H_
#define NRFMESH_SECURE_H_

extern void secure_set_keys(uint8_t* oob);
extern uint8_t secure_authenticate(uint16_t handle);
extern void secure_ble_event(ble_evt_t* event);
extern uint8_t secure_check_authorization(ble_evt_t* event);

#endif /* NRFMESH_SECURE_H_ */
