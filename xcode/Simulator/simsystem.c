//
//  simsystem.c
//  Mesh Mesh_System interface for the simulator.
//
//  Created by tim on 1/12/15.
//  Copyright (c) 2015 tim. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <memory.h>
#include "mesh.h"
#include "meshsystem.h"
#include "sim.h"


void Mesh_System_MasterMode(Mesh_Node* node)
{
  Sim_Q* q = NODE_TO_NATIVE(node);
  assert(q->ismaster == 0);
  q->ismaster = 1;
  q->retry = 0;
  assert(q->event == MESH_EVENT_NULL);
  q->event = MESH_EVENT_INMASTERMODE;
  q->arg = 0;
}

void Mesh_System_PeripheralMode(Mesh_Node* node)
{
  Sim_Q* q = NODE_TO_NATIVE(node);
  assert(q->ismaster == 1);
  q->ismaster = 0;
  assert(q->event == MESH_EVENT_NULL);
  q->event = MESH_EVENT_INPERIPHERALMODE;
  q->arg = 0;
}

void Mesh_System_PeripheralDone(Mesh_Node* node)
{
}

void Mesh_System_ValueChanged(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, unsigned char* value, unsigned char length)
{
}

void Mesh_System_Connect(Mesh_Node* node)
{
  Sim_Q* q = NODE_TO_NATIVE(node);
  assert(q->ismaster == 1);
  assert(q->connection == NULL);
  Mesh_NodeId target = node->sync.neighbor->id;
  for (int i = 0; i < NR_NODES; i++)
  {
    if (memcmp(&node->ids[target].address, &nodes[i].ids[MESH_NODEID_SELF].address, sizeof(Mesh_NodeAddress)) == 0)
    {
      // Found
      Sim_Q* p = &natives[i];
      if (nodes[i].state != MESH_STATE_IDLE || p->connection != NULL)
      {
        // Target is busy - so we fail to connect
        q->event = MESH_EVENT_CONNECTIONFAILED;
        q->arg = 0;
        return;
      }
      // Connect
      assert(p->ismaster == 0);
      assert(p->connection == NULL);
      q->connection = &nodes[i];
      p->connection = node;
      assert(p->event == MESH_EVENT_NULL);
      p->event = MESH_EVENT_INCOMINGCONNECTION;
      p->arg = Mesh_InternNodeId(&nodes[i], &node->ids[MESH_NODEID_SELF].address, 1);
      assert(q->event == MESH_EVENT_NULL);
      q->event = MESH_EVENT_CONNECTED;
      q->arg = 0;
      return;
    }
  }
  assert("Unknown Node" == 0);
}

void Mesh_System_Disconnect(Mesh_Node* node)
{
  Sim_Q* q = NODE_TO_NATIVE(node);
  assert(q->connection != NULL);
  Mesh_Node* target = q->connection;
  Sim_Q* p = NODE_TO_NATIVE(target);
  assert(p->connection == node);
  q->connection = NULL;
  p->connection = NULL;
  assert(q->event == MESH_EVENT_NULL);
  assert(p->event == MESH_EVENT_NULL);
  q->event = MESH_EVENT_DISCONNECTED;
  q->arg = 0;
  p->event = MESH_EVENT_DISCONNECTED;
  p->arg = 0;
}

void Mesh_System_Write(Mesh_Node* node)
{
  Sim_Q* q = NODE_TO_NATIVE(node);
  assert(q->connection != NULL);
  Sim_Q* p = NODE_TO_NATIVE(q->connection);
  assert(p->connection == node);
  assert(q->ismaster == 1);
  assert(p->event == MESH_EVENT_NULL);
  memcpy(q->connection->sync.buffer, node->sync.buffer, node->sync.bufferlen);
  q->connection->sync.bufferlen = node->sync.bufferlen;
  p->event = MESH_EVENT_WRITE;
  p->arg = 0;
}

void Mesh_System_WriteAck(Mesh_Node* node)
{
  Sim_Q* q = NODE_TO_NATIVE(node);
  assert(q->connection != NULL);
  Sim_Q* p = NODE_TO_NATIVE(q->connection);
  assert(p->connection == node);
  assert(p->event == MESH_EVENT_NULL);
  p->event = MESH_EVENT_WROTE;
  p->arg = 0;
}

void Mesh_System_Read(Mesh_Node* node)
{
  Sim_Q* q = NODE_TO_NATIVE(node);
  assert(q->connection != NULL);
  assert(q->ismaster == 1);
  Sim_Q* p = NODE_TO_NATIVE(q->connection);
  assert(p->connection == node);
  assert(p->event == MESH_EVENT_NULL);
  p->event = MESH_EVENT_READING;
  p->arg = 0;
}

void Mesh_System_ReadAck(Mesh_Node* node)
{
  Sim_Q* q = NODE_TO_NATIVE(node);
  assert(q->connection != NULL);
  Sim_Q* p = NODE_TO_NATIVE(q->connection);
  assert(p->connection == node);
  assert(q->ismaster == 0);
  assert(p->event == MESH_EVENT_NULL);
  memcpy(q->connection->sync.buffer, node->sync.buffer, node->sync.bufferlen);
  q->connection->sync.bufferlen = node->sync.bufferlen;
  p->event = MESH_EVENT_READ;
  p->arg = 0;
}

void Mesh_System_Retry(Mesh_Node* node, unsigned short retrycount)
{
  Sim_Q* q = NODE_TO_NATIVE(node);
  assert(q->connection == NULL);
  assert(q->ismaster == 0);
  assert(q->event == MESH_EVENT_NULL);
  Mesh_System_RandomNumber(&q->retry, sizeof(q->retry));
  q->retry = 1 + q->retry % (retrycount * 8);
}

void Mesh_System_ScheduleDiscovery(Mesh_Node* node)
{
}


unsigned char* Mesh_System_Malloc(unsigned short length)
{
  return malloc(length);
}

void Mesh_System_Free(unsigned char* memory)
{
  free(memory);
}

Mesh_Tick Mesh_System_Tick(void)
{
  // Just needs to be some sort of periodic increating integer
  return (Mesh_Tick)time(NULL);
}

void Mesh_System_RandomNumber(unsigned char* buffer, unsigned char length)
{
  while (length--)
  {
    *buffer++ = (unsigned char)random();
  }
}

