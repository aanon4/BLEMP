/*
 * uuids.h
 *
 *  Created on: Mar 17, 2015
 *      Author: tim
 */

#ifndef UUIDS_H_
#define UUIDS_H_

#define	UUIDS_BASE_UUID				0x1D, 0xAE, 0x74, 0x7C, 0x62, 0xA9, 0x95, 0x9C, 0x9A, 0x44, 0xBD, 0x5F, ( 0 ), ( 0 ), 0xAD, 0x79
#define	UUIDS_BASE_TYPE				2

#define	UUIDS_SERVICE_UUID			0x0001
#define	MESH_SYNC_UUID				  0x0002
#if defined(TESTING_KEEPALIVE)
#define	MESH_KEEPALIVE_UUID			0x0008
#endif
#if defined(INCLUDE_STATISTICS)
#define	MESH_STATISTICS_UUID		0x0009
#endif

extern uint16_t primary_service_handle;

extern void uuids_init(void);

#endif /* UUIDS_H_ */
