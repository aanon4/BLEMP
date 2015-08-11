//
//  sim.h
//  Mesh
//
//  Created by tim on 1/12/15.
//  Copyright (c) 2015 tim. All rights reserved.
//

#ifndef Mesh_sim_h
#define Mesh_sim_h

#define NR_NODES    16

typedef struct Sim_Q
{
  char          ismaster;
  unsigned char retry;
  Mesh_Node*    connection;
  Mesh_Event    event;
  unsigned char arg;
} Sim_Q;

extern Mesh_Node nodes[NR_NODES];
extern Sim_Q natives[NR_NODES];

#define NODE_TO_NATIVE(NODE)    (&natives[((NODE) - nodes)])


extern void simSetup(int nrNodes);
extern void simDiscover(void);
extern void simTick(void);
extern void simTicks(int nrTicks);
extern int simCurrentTick(void);
extern void simSync(Mesh_Node* node);
extern unsigned char simValidateState(void);
extern void simDumpState(void);
extern Mesh_ChangeBits simChangeBits(Mesh_Node* node);

#endif
