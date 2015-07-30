/*
 * meshinfo.h
 *
 *  Created on: Jul 29, 2015
 *      Author: tim
 */

#ifndef NRFMESH_MESHINFO_H_
#define NRFMESH_MESHINFO_H_

#define MESHINFO_KEY_ADVERTISING_INTERVAL 1 // 4-byts (ms)
#define MESHINFO_KEY_POWER_SOURCE         1 // 1-byte (0 == mains, 1 == battery)

extern void meshinfo_init(void);

#endif /* NRFMESH_MESHINFO_H_ */
