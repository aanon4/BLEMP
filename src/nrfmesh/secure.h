/*
 * secure.h
 *
 *  Created on: Jun 3, 2015
 *      Author: tim
 */

#ifndef NRFMESH_SECURE_H_
#define NRFMESH_SECURE_H_

extern void secure_init(void);
extern void secure_set_keys(uint8_t* passkey, uint32_t timeout_ms, uint8_t* oob, uint8_t* irk);
extern uint8_t secure_authenticate(uint16_t handle);
extern void secure_ble_event(ble_evt_t* event);
extern void secure_mesh_valuechanged(Mesh_NodeId id, Mesh_Key key, uint8_t* value, uint8_t length);

#define MESH_KEY_LTK0     (MESH_KEY_INTERNAL + 0x10)

#endif /* NRFMESH_SECURE_H_ */
