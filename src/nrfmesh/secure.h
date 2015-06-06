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
extern void secure_set_keys(uint8_t* oob, uint8_t* irk);
extern void secure_authenticate(uint16_t handle);
extern void secure_ble_event(ble_evt_t* event);
extern void secure_reset_bonds(void);

#define MESH_SECURE_MAX_BONDS     4

#define MESH_KEY_LTK_FIRST        (MESH_KEY_INTERNAL + 0x10)
#define MESH_KEY_LTK_LAST         (MESH_KEY_LTK_FIRST + MESH_SECURE_MAX_BONDS)

#endif /* NRFMESH_SECURE_H_ */
