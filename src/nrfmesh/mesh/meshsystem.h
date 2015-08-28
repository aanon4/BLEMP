//
//  meshsystem.h
//  Mesh
//
//  Created by tim on 1/9/15.
//  Copyright (c) 2015 tim. All rights reserved.
//

#ifndef Mesh_meshsystem_h
#define Mesh_meshsystem_h

#if defined(__APPLE__)

#include <string.h>
#define Mesh_System_memmove     memmove
#define Mesh_System_memcmp      memcmp
#define Mesh_System_memset      memset

#elif defined(NRF51)

#include <string.h>
#define	Mesh_System_memmove     memmove
#define	Mesh_System_memcmp      memcmp
#define	Mesh_System_memset      memset

#if ENABLE_MESH_MALLOC
#define	Mesh_System_Malloc      umm_malloc
#define	Mesh_System_Free        umm_free
#include "umm_malloc/umm_malloc.h"
#endif

#define Mesh_System_ReadAck(NODE)
#define Mesh_System_WriteAck(NODE)

#endif

//
// API used by Mesh to interact with BLE system.
//
extern void Mesh_System_MasterMode(Mesh_Node* node);
extern void Mesh_System_PeripheralMode(Mesh_Node* node);
extern void Mesh_System_PeripheralDone(Mesh_Node* node);
extern Mesh_Status Mesh_System_Connect(Mesh_Node* node);
extern void Mesh_System_Disconnect(Mesh_Node* node);
extern void Mesh_System_Write(Mesh_Node* node);
extern void Mesh_System_Read(Mesh_Node* node);
#if !defined(Mesh_System_WriteAck)
extern void Mesh_System_WriteAck(Mesh_Node* node);
#endif
#if !defined(Mesh_System_ReadAck)
extern void Mesh_System_ReadAck(Mesh_Node* node);
#endif
extern void Mesh_System_ValueChanged(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, unsigned char* value, unsigned char length);
extern void Mesh_System_Retry(Mesh_Node* node, unsigned short retrycount);
extern void Mesh_System_ScheduleDiscovery(Mesh_Node* node);
extern Mesh_Tick Mesh_System_Tick(void);
extern void Mesh_System_RandomNumber(unsigned char* buffer, unsigned char length);
#if ENABLE_MESH_MALLOC
#if !defined(Mesh_System_Malloc)
extern unsigned char* Mesh_System_Malloc(unsigned short length);
#endif
#if !defined(Mesh_System_Free)
extern void Mesh_System_Free(unsigned char* memory);
#endif
#endif
extern void Mesh_System_GetClock(Mesh_Node* node, Mesh_NodeId id, Mesh_Clock* localtime);
extern void Mesh_System_SetClock(Mesh_Node* node, Mesh_NodeId id, Mesh_Clock* remotetime);

#endif
