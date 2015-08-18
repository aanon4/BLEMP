/*
 * secure.h
 *
 *  Created on: Jun 3, 2015
 *      Author: tim
 */

#ifndef NRFMESH_SECURE_H_
#define NRFMESH_SECURE_H_

extern void secure_init(void);
extern void secure_set_passkey(uint8_t* passkey, int32_t timeout_ms);
extern void secure_set_keys(uint8_t* oob);
extern void secure_authenticate(uint16_t handle);
extern void secure_ble_event(ble_evt_t* event);
extern void secure_meshchange(Mesh_NodeId id, Mesh_Key key, uint8_t* value, uint8_t length);
extern void secure_reset_bonds(void);
extern uint8_t secure_get_irks(ble_gap_irk_t irks[BLE_GAP_WHITELIST_IRK_MAX_COUNT]);

#endif /* NRFMESH_SECURE_H_ */
