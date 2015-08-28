/*
 * meshinfo.h
 *
 *  Created on: Jul 29, 2015
 *      Author: tim
 */

#ifndef NRFMESH_MESHINFO_H_
#define NRFMESH_MESHINFO_H_

#define MESHINFO_KEY_ADVERTISING_INTERVAL 1 // 4-bytes (ms)
#define MESHINFO_KEY_POWER_SOURCE         2 // 1-byte (0 == mains, 1 == battery)
#define MESHINFO_KEY_TIMESYNC_INTERVAL    3 // 4-bytes (s)

extern void meshinfo_init(void);

#endif /* NRFMESH_MESHINFO_H_ */
