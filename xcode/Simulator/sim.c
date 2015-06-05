//
//  sim.c
//  Mesh
//
//  Created by tim on 1/17/15.
//  Copyright (c) 2015 tim. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include "mesh.h"
#include "sim.h"

static int nrNodes;
static unsigned char simDebug = 1;
int tick;

Mesh_Node nodes[NR_NODES];
Sim_Q natives[NR_NODES];


void simSetup(int nodeCount)
{
  nrNodes = nodeCount;
  tick = 0;
  
  static Mesh_NodeAddress dummyId;
  
  assert(nrNodes <= MESH_MAX_NODES);
  assert(NR_NODES <= MESH_MAX_NODES);
  
  memset(nodes, 0, sizeof(nodes));
  memset(natives, 0, sizeof(natives));
  
  srandom(0);
  
  for (int i = 0; i < nrNodes; i++)
  {
    dummyId.address[0] = i + 1;
    Mesh_NodeReset(&nodes[i], &dummyId);
  }
}

void simDiscover(void)
{
  int spread = nrNodes / 4 <= 1 ? nrNodes : nrNodes / 4;
  for (int i = 0; i < nrNodes; i++)
  {
    for (int m = 0; m < spread; m++)
    {
      int j = (m + i) % nrNodes;
      if (j != i && ((nodes[j].state == MESH_STATE_IDLE && natives[j].event == MESH_EVENT_NULL) || (nodes[j].state == MESH_STATE_SWITCHTOMASTER && natives[j].event == MESH_EVENT_INMASTERMODE)))
      {
        Mesh_Status status = Mesh_Process(&nodes[j], MESH_EVENT_NEIGHBOR_DISCOVER, Mesh_InternNodeId(&nodes[j], &nodes[i].ids[0].address, 1), 0);
        assert(status != MESH_BADSTATE);
        assert(status != MESH_BADPAYLOAD);
        assert(status != MESH_OOM);
      }
    }
  }
}

void simTick(void)
{
  // Run through each node, processing each one if an event is pending.
  for (int i = 0; i < nrNodes; i++)
  {
    Mesh_Event event = natives[i].event;
    assert(nodes[i].state == MESH_STATE_IDLE || event != MESH_EVENT_NULL);
    if (event != MESH_EVENT_NULL)
    {
      natives[i].event = MESH_EVENT_NULL;
      Mesh_Status status = Mesh_Process(&nodes[i], event, natives[i].arg, 0);
      assert(status != MESH_BADSTATE);
      assert(status != MESH_BADPAYLOAD);
      assert(status != MESH_OOM);
    }
  }
  
  if (simDebug)
  {
    printf("%4d  ", tick);
    for (int i = 0; i < nrNodes; i++)
    {
      printf(" %2d", nodes[i].state);
    }
    printf("\n");
  }

  // Schedule retries
  for (int i = 0; i < nrNodes; i++)
  {
    if (natives[i].event == MESH_EVENT_NULL && nodes[i].state == MESH_STATE_IDLE)
    {
      if (natives[i].retry == 1)
      {
        natives[i].retry = 0;
        Mesh_Status status = Mesh_Process(&nodes[i], MESH_EVENT_RETRY, 0, 0);
        assert(status != MESH_BADSTATE);
        assert(status != MESH_BADPAYLOAD);
        assert(status != MESH_OOM);
      }
      else if (natives[i].retry > 1)
      {
        natives[i].retry--;
      }
    }
  }
  
  tick++;
}

void simTicks(int nrTicks)
{
  while (nrTicks--)
  {
    simTick();
  }
}

int simCurrentTick(void)
{
  return tick;
}

void simSync(Mesh_Node* node)
{
  Sim_Q* q = NODE_TO_NATIVE(node);
  if (q->event == MESH_EVENT_NULL && node->state == MESH_STATE_IDLE)
  {
    Mesh_Process(node, MESH_EVENT_SYNC, 0, 0);
  }
}

Mesh_ChangeBits simChangeBits(Mesh_Node* node)
{
  return node->neighbors.changebits;
}

unsigned char simValidateState(void)
{
  for (int i = 0; i < nrNodes; i++)
  {
    Mesh_ChangeBits mask = simChangeBits(&nodes[i]);
    if (nodes[i].state != MESH_STATE_IDLE)
    {
      simDumpState();
      return 0;
    }
    Mesh_UKV* ukv = nodes[i].values.values;
    for (int j = 0; j < nodes[i].values.count; j++, ukv++)
    {
      if ((ukv->changebits & mask) != 0)
      {
        simDumpState();
        return 0;
      }
    }
#if ENABLE_MESH_MALLOC
    if (nodes[i].sync.value.buffer != NULL)
    {
      simDumpState();
      return 0;
    }
#endif
  }

  if (simDebug)
  {
    simDumpState();
  }

  return 1;
}

void simDumpState(void)
{    
  for (int i = 0; i < nrNodes; i++)
  {
    printf(" Node: %2d State: %2d\n", i, nodes[i].state);
    Mesh_ChangeBits mask = simChangeBits(&nodes[i]);
    Mesh_UKV* ukv = nodes[i].values.values;
    for (int j = 0; j < nodes[i].values.count; j++, ukv++)
    {
      printf("  N%d K%04x V%02x C%02x D%08x\n", nodes[i].ids[ukv->id].address.address[0] - 1, ukv->key, ukv->version, ukv->changebits & mask, *(int*)&ukv->data.value);
    }
  }
}
