/*
 * nrf51mesh.h
 *
 *  Created on: Mar 23, 2015
 *      Author: tim
 */

#ifndef NRF51MESH_H_
#define NRF51MESH_H_

#include "mesh/mesh.h"
#include "mesh/meshsystem.h"

#define	RETRY_FIXED			 		      50 // ms
#define	RETRY_VARIABLE		 		    50 // ms

#define	SCAN_INTERVAL		 		      MSEC_TO_UNITS(20, UNIT_0_625_MS) // waz 100
#define	SCAN_WINDOW		 	 		      MSEC_TO_UNITS(20, UNIT_0_625_MS) // waz 99
#define SCAN_TIMEOUT              (15 * MESH_ADVERTISING_PERIOD / 1000)
#define	CONNECT_SCAN_INTERVAL		  MSEC_TO_UNITS(20, UNIT_0_625_MS)
#define	CONNECT_SCAN_WINDOW			  MSEC_TO_UNITS(20, UNIT_0_625_MS)
#define	CONNECT_TIMEOUT		        (2 * MESH_ADVERTISING_PERIOD / 1000) // s
#define	RSSI_THRESHOLD				    0
#define	RSSI_SKIPCOUNT				    0

extern Mesh_Node mesh_node;

extern void nrfmesh_init(void);
extern void nrfmesh_start(void);
extern void nrfmesh_ble_event(ble_evt_t* event);
extern void nrfmesh_timer_handler(void);
extern void nrfmesh_meshchange(Mesh_NodeId id, Mesh_Key key, uint8_t* value, uint8_t length);

#endif /* NRF51MESH_H_ */
