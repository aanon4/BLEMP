//
//  mesh.c
//  Mesh
//
//  Created by tim on 1/8/15.
//  Copyright (c) 2015 tim. All rights reserved.
//

#include "mesh.h"
#include "meshsystem.h"

static const Mesh_Key MESH_KEY_INVALID = _MESH_KEY_INVALID;

//
// Mesh Manager
// This function processes incoming events from the network or application and runs the mesh syncing process.
//
Mesh_Status Mesh_Process(Mesh_Node* node, Mesh_Event event, unsigned char arg, Mesh_RSSI rssi)
{
  Mesh_Status status = MESH_OK;

  switch (node->state)
  {
  case MESH_STATE_IDLE:
    switch (event)
    {
      case MESH_EVENT_NEIGHBOR_DISCOVER:
      {
        // Ignore blacklisted node for a period of time
        if (node->ids[arg].flag.blacklisted)
        {
          if (!node->ids[arg].flag.ping)
          {
            node->ids[arg].flag.blacklisted = 0;
          }
          status = MESH_NOTFOUND;
          break;
        }
        Mesh_Neighbor* neighbor = NULL;
        status = Mesh_FindNeighbor(node, arg, &neighbor);
        switch (status)
        {
          case MESH_OK:
            // Neighbor already exists - update RSSI
            neighbor->rssi = (neighbor->rssi + rssi) / 2;
            break;

          case MESH_NOTFOUND:
            // This is a new neighbor.
            status = Mesh_AddNeighbor(node, arg, &neighbor);
            if (status == MESH_OK)
            {
              neighbor->flag.valid = 1;
              neighbor->retries = MESH_NEIGHBOR_FORWARD_LIMIT;
              neighbor->rssi = rssi;
            }
            break;

          default:
            break;
        }
        break;
      }

      case MESH_EVENT_KEEPALIVE:
        node->keepalive++;
        if (node->keepalive >= MESH_KEEPALIVE_SWEEP_TIME / MESH_KEEPALIVE_TIME)
        {
          // Forget nodes we've not heard from in a long time (skip SELF and GLOBAL)
          node->keepalive = 0;
          for (Mesh_NodeId id = 2; id < MESH_MAX_NODES; id++)
          {
            if (!node->ids[id].flag.ping && !node->ids[id].flag.client)
            {
              Mesh_ForgetNodeId(node, id);
            }
            else
            {
              node->ids[id].flag.ping = 0;
            }
          }
        }

        // If we synced recently we don't need to do it now
        if (Mesh_System_Tick() - node->lastsync > MESH_KEEPALIVE_TIME)
        {
          break;
        }
        // Fall through

      case MESH_EVENT_SYNC:
      case MESH_EVENT_RETRY:
        if (Mesh_GetChangeBits(node) != 0)
        {
          node->state = MESH_STATE_SWITCHTOMASTER;
          Mesh_System_MasterMode(node);
        }
        break;

      case MESH_EVENT_INCOMINGCONNECTION:
        // Incoming connection for sync - start to receive changes
        node->state = MESH_STATE_SYNCPERIPHERALREADING;
        node->sync.id = MESH_NODEID_INVALID;
        node->sync.ukv = node->values.values;
        node->sync.count = node->values.count;
        node->sync.activeneighbors = &node->neighbors.neighbors[MESH_MAX_NEIGHBORS - 1];
        status = Mesh_FindNeighbor(node, arg, &node->sync.neighbor);
        if (status == MESH_NOTFOUND)
        {
          status = Mesh_AddNeighbor(node, arg, &node->sync.neighbor);
        }
        if (status != MESH_OOM)
        {
          node->sync.neighborchangebit = MESH_NEIGHBOR_TO_CHANGEBIT(node, node->sync.neighbor);
          node->sync.changebits = node->neighbors.changebits & ~node->sync.neighborchangebit;
          // Update ticks on the address to keep it alive
          node->ids[arg].flag.ping = 1;
          node->sync.neighbor->flag.retry = 0;
          node->ids[arg].flag.blacklisted = 0;
          node->sync.neighbor->retries = 0;
#if ENABLE_MESH_MALLOC
          node->sync.value.buffer = Mesh_System_Malloc(MESH_MAX_VALUE_SIZE);
          if (node->sync.value.buffer == NULL)
          {
            status = MESH_OOM;
          }
#endif
        }
        break;

      case MESH_EVENT_DISCONNECTED:
        Mesh_System_PeripheralDone(node);
        break;

      default:
        break;
    }
    break;

  case MESH_STATE_SWITCHTOMASTER:
    switch (event)
    {
      case MESH_EVENT_INMASTERMODE:
        // In master mode - find someone to sync with
        node->lastsync = Mesh_System_Tick();
#if ENABLE_MESH_MALLOC
        node->sync.value.buffer = Mesh_System_Malloc(MESH_MAX_VALUE_SIZE);
        if (node->sync.value.buffer == NULL)
        {
          status = MESH_OOM;
          goto nosync;
        }
#endif
        {
        lookforsync:;
          node->sync.remainingbits = Mesh_GetChangeBits(node);
          unsigned char count = MESH_MAX_NEIGHBORS;
          // If we have a set connection priority, we do that first
          if (node->sync.priority == MESH_NODEID_INVALID || Mesh_FindNeighbor(node, node->sync.priority, &node->sync.neighbor) != MESH_OK)
          {
            node->sync.neighbor = &node->neighbors.neighbors[Mesh_System_RandomNumber(MESH_MAX_NEIGHBORS)];
          }
          node->sync.priority = MESH_NODEID_INVALID;

          for (; count; count--)
          {
            if (!node->sync.neighbor->flag.retry && node->sync.neighbor->flag.valid && (node->sync.remainingbits & MESH_NEIGHBOR_TO_CHANGEBIT(node, node->sync.neighbor)))
            {
              // Found one
              node->state = MESH_STATE_SYNCMASTERCONNECTING;
              Mesh_System_Connect(node);
              goto foundsync;
            }
            if (--node->sync.neighbor < &node->neighbors.neighbors[0])
            {
              node->sync.neighbor = &node->neighbors.neighbors[MESH_MAX_NEIGHBORS - 1];
            }
          }
        }
#if ENABLE_MESH_MALLOC
        Mesh_System_Free(node->sync.value.buffer);
        node->sync.value.buffer = NULL;
#endif
      nosync:;
        // No one left changed.
        node->sync.neighbor = NULL;
        node->state = MESH_STATE_SWITCHTOPERIPHERAL;
        Mesh_System_PeripheralMode(node);
      foundsync:;
        break;

      default:
        goto badstate;
    }
    break;

  case MESH_STATE_SYNCMASTERDISCONNECTING:
    switch (event)
    {
      case MESH_EVENT_DISCONNECTED:
        goto lookforsync;

      case MESH_EVENT_IOFAILED:
        goto mastersynctimeout;

      default:
        goto badstate;
    }
    break;

  case MESH_STATE_SWITCHTOPERIPHERAL:
    switch (event)
    {
      case MESH_EVENT_INPERIPHERALMODE:
      {
        node->state = MESH_STATE_IDLE;

        // Cleanup.
        unsigned short retrycount = 0;
        for (Mesh_Neighbor* neighbor = &node->neighbors.neighbors[MESH_MAX_NEIGHBORS - 1]; neighbor >= &node->neighbors.neighbors[0]; neighbor--)
        {
          // Invalid, old and rssi distant neighbors are removed
          if (neighbor->id)
          {
            // Forget neighbors which we can no longer reach.

            if (node->ids[neighbor->id].flag.client)
            {
              // We don't forget clients and ignore failures (they can be absent).
              if (neighbor->retries >= MESH_MAX_CLIENT_RETRIES)
              {
                neighbor->flag.retry = 0;
                neighbor->retries = 0;
                node->ids[neighbor->id].flag.ping = 0;
                // Remove ukv from the changebits
                Mesh_ChangeBits unmask = ~MESH_NEIGHBOR_TO_CHANGEBIT(node, neighbor);
                Mesh_UKV* ukv = node->values.values;
                for (unsigned short count = node->values.count; count; count--, ukv++)
                {
                  ukv->changebits &= unmask;
                }
              }
              else
              {
                goto checkretry;
              }
            }
            else if (neighbor->retries >= MESH_MAX_RETRIES)
            {
              Mesh_ForgetNeighbor(node, neighbor, 1);
            }
            else if (!neighbor->flag.valid)
            {
              Mesh_ForgetNeighbor(node, neighbor, 0);
            }
            else if (neighbor->flag.badrssi)
            {
              if (node->neighbors.count > MESH_NEIGHBOR_LIMIT)
              {
                Mesh_ForgetNeighbor(node, neighbor, 0);
              }
              else
              {
                neighbor->flag.badrssi = 0;
                goto checkretry;
              }
            }
            else
            {
            checkretry:;
              if (neighbor->flag.retry)
              {
                // Count the number of retries flagged to get an idea of how busy the network is
                neighbor->flag.retry = 0;
                retrycount += neighbor->retries;
              }
            }
          }
        }
        // If we still have pending changes, schedule a retry
        if (Mesh_GetChangeBits(node))
        {
          // Schedule a retry, giving an indication of how busy the network is so we
          // can backoff in an appropriate manner.
          Mesh_System_Retry(node, retrycount);
        }
        break;
      }

      default:
        goto badstate;
    }
    break;

  case MESH_STATE_SYNCMASTERCONNECTING:
    switch (event)
    {
      case MESH_EVENT_CONNECTIONFAILED:
      case MESH_EVENT_DISCONNECTED:
      case MESH_EVENT_IOFAILED:
      mastersynctimeout:;
        // Failed to connect/keep connected. Try again later
        node->sync.neighbor->flag.retry = 1;
        node->sync.neighbor->retries++;
        node->sync.neighbor = NULL;
        goto lookforsync;

      case MESH_EVENT_INVALIDNODE:
        node->ids[node->sync.neighbor->id].flag.blacklisted = 1;
        node->ids[node->sync.neighbor->id].flag.ping = 1;
        node->sync.neighbor->flag.valid = 0;
        Mesh_System_Disconnect(node);
        break;

      case MESH_EVENT_CONNECTED:
        node->state = MESH_STATE_SYNCMASTERWRITING;
        node->sync.neighborchangebit = MESH_NEIGHBOR_TO_CHANGEBIT(node, node->sync.neighbor);
        node->sync.changebits = node->neighbors.changebits & ~node->sync.neighborchangebit;
        node->sync.neighbor->flag.retry = 0;
        node->ids[node->sync.neighbor->id].flag.ping = 1;
        node->sync.neighbor->retries = 0;
        node->sync.id = MESH_NODEID_INVALID;
        node->sync.ukv = node->values.values;
        node->sync.activeneighbors = &node->neighbors.neighbors[MESH_MAX_NEIGHBORS - 1];
        node->sync.count = node->values.count;
        node->sync.value.key = MESH_KEY_INVALID;
        goto writing;

      default:
        goto badstate;
    }
    break;

  case MESH_STATE_SYNCMASTERREADING:
    switch (event)
    {
      case MESH_EVENT_READ:
        goto reading;

      case MESH_EVENT_DISCONNECTED:
        goto mastersynctimeout;

      case MESH_EVENT_IOFAILED:
        goto mastersyncdone;

      case MESH_EVENT_WROTE:
        if (node->ids[node->sync.neighbor->id].flag.client)
        {
          goto mastersyncdone;
        }
        else
        {
          Mesh_System_Read(node);
        }
        break;

      default:
        goto badstate;
    }
    break;

  case MESH_STATE_SYNCPERIPHERALREADING:
    switch (event)
    {
      case MESH_EVENT_WRITE:
      {
      reading:;
        // Update RSSI
        node->sync.neighbor->rssi = (node->sync.neighbor->rssi + rssi) / 2;
        if (node->sync.bufferlen == 0)
        {
          goto badstate;
        }
        else
        {
          for (unsigned char pos = 0; pos + 1 <= node->sync.bufferlen && status == MESH_OK;)
          {
            unsigned char* value = NULL;
            switch ((Mesh_Payload)node->sync.buffer[pos++])
            {
              case MESH_PAYLOADNODEID:
                if (pos + sizeof(Mesh_NodeAddress) > node->sync.bufferlen)
                {
                  status = MESH_BADPAYLOAD;
                }
                else
                {
                  node->sync.id = Mesh_InternNodeId(node, (Mesh_NodeAddress*)&node->sync.buffer[pos], 1);
                  pos += sizeof(Mesh_NodeAddress);
                  if (node->sync.id == MESH_NODEID_INVALID)
                  {
                    status = MESH_OOM;
                  }
                  else
                  {
                    // Update ping on an address whenever we see it
                    node->ids[node->sync.id].flag.ping = 1;
                  }
                }
                break;

              case MESH_PAYLOADUKV:
                if (pos + sizeof(Mesh_Key) + sizeof(Mesh_Version) + 1 > node->sync.bufferlen)
                {
                  status = MESH_BADPAYLOAD;
                }
                else
                {
                  node->sync.value.key = MESH_KEY_INVALID;
                  Mesh_System_memmove(&node->sync.value.key, (Mesh_Key*)&node->sync.buffer[pos], sizeof(Mesh_Key)); // mis-aligned
                  node->sync.value.version = *(Mesh_Version*)&node->sync.buffer[pos + sizeof(Mesh_Key)];
                  node->sync.value.length = node->sync.buffer[pos + sizeof(Mesh_Key) + sizeof(Mesh_Version)];
                  value = &node->sync.buffer[pos + sizeof(Mesh_Key) + sizeof(Mesh_Version) + 1];
                  pos += sizeof(Mesh_Key) + sizeof(Mesh_Version) + 1;
                  if (node->sync.id == MESH_NODEID_INVALID)
                  {
                    status = MESH_BADPAYLOAD;
                  }
                  else
                  {
                    if (pos + node->sync.value.length > node->sync.bufferlen)
                    {
                      unsigned char actual = node->sync.bufferlen - pos;
                      Mesh_System_memmove(node->sync.value.buffer, value, actual);
                      node->sync.value.offset = actual;
                      pos += actual;
                    }
                    else
                    {
                      pos += node->sync.value.length;
                    syncnow:;
                      status = Mesh_SyncValue(node, node->sync.id, node->sync.value.key, value, node->sync.value.length, node->sync.value.version, node->sync.changebits);
                      if (status == MESH_NOTFOUND)
                      {
                        // A new key, so we create it
                        status = Mesh_SetValueInternal(node, node->sync.id, node->sync.value.key, value, node->sync.value.length, 1, node->sync.value.version, node->sync.changebits);
#if ENABLE_MESH_TRIMMING
                        // We have no space for this new value. If Mesh Trimming is enabled, we are allowed to throw away
                        // old values which are not specifically for this node. This decreases efficiency if that UKV is
                        // retransmitted, but that will be rare (if ever). It also means nodes no longer hold the entire mesh
                        // state but only a subset.
                        if (status == MESH_OOM)
                        {
                          status = Mesh_Trim(node, length);
                          if (status == STATUS_OK)
                          {
                            status = Mesh_SetValueInternal(node, node->sync.id, key, value, length, 1, version, node->sync.changebits);
                          }
                        }
#endif
                        Mesh_System_ValueChanged(node, node->sync.id, node->sync.value.key, value, node->sync.value.length);
                      }
                      else if (status == MESH_CHANGE)
                      {
                        Mesh_System_ValueChanged(node, node->sync.id, node->sync.value.key, value, node->sync.value.length);
                        status = MESH_OK;
                      }
                      else if (status == MESH_NOCHANGE)
                      {
                        status = MESH_OK;
                      }
                    }
                  }
                }
                break;

              case MESH_PAYLOADUKVDATA:
              {
                unsigned char length = node->sync.bufferlen - pos;
                if (length > node->sync.value.length - node->sync.value.offset)
                {
                  length = node->sync.value.length - node->sync.value.offset;
                }
                Mesh_System_memmove(node->sync.value.buffer + node->sync.value.offset, node->sync.buffer + pos, length);
                node->sync.value.offset += length;
                pos += length;
                if (node->sync.value.offset == node->sync.value.length)
                {
                  value = node->sync.value.buffer;
                  goto syncnow;
                }
                break;
              }

              case MESH_PAYLOADNEIGHBORS:
                if (pos + sizeof(Mesh_NodeAddress) + sizeof(Mesh_RSSI) > node->sync.bufferlen)
                {
                  status = MESH_BADPAYLOAD;
                }
                else
                {
                  Mesh_NodeId id = Mesh_InternNodeId(node, (Mesh_NodeAddress*)&node->sync.buffer[pos], 0);
                  if (id != MESH_NODEID_INVALID)
                  {
                    Mesh_Neighbor* neighbor = NULL;
                    if (Mesh_FindNeighbor(node, id, &neighbor) == MESH_OK)
                    {
                      node->sync.changebits &= ~MESH_NEIGHBOR_TO_CHANGEBIT(node, neighbor);
                      // Only eliminate connections on a peripheral, not a master
                      // Never eliminate client nodes
                      if (node->state == MESH_STATE_SYNCPERIPHERALREADING && node->neighbors.count > MESH_NEIGHBOR_LIMIT && !node->ids[id].flag.client)
                      {
                        Mesh_RSSI nrssi = (Mesh_RSSI)node->sync.buffer[pos + sizeof(Mesh_NodeAddress)];
                        if (nrssi >= neighbor->rssi)
                        {
                          neighbor->flag.badrssi = 1;
                        }
                      }
                    }
                  }
                  pos += sizeof(Mesh_NodeAddress) + sizeof(Mesh_RSSI);
                }
                break;

              case MESH_PAYLOADRESET:
                for (Mesh_UKV* ukv = &node->values.values[node->values.count - 1]; ukv >= &node->values.values[0]; ukv--)
                {
                  ukv->changebits |= node->sync.neighborchangebit;
                }
                break;

              case MESH_PAYLOADDONE:
                switch (node->state)
                {
                  case MESH_STATE_SYNCMASTERREADING:
                    // We have all the differences from the neighbor - so we're done
                    node->state = MESH_STATE_SYNCMASTERDONE;
                    node->sync.neighbor->flag.valid = 1;
                    goto mastersyncdone;

                  case MESH_STATE_SYNCPERIPHERALREADING:
                    node->state = MESH_STATE_SYNCPERIPHERALWRITING;
                    node->sync.value.key = MESH_KEY_INVALID;
                    break;

                  default:
                    goto badstate;
                }
                break;

              default:
                status = MESH_BADPAYLOAD;
                break;
            }
          }
        }
        if (event == MESH_EVENT_READ)
        {
          Mesh_System_Read(node);
        }
        else
        {
          Mesh_System_WriteAck(node);
        }
        break;
      }

      case MESH_EVENT_DISCONNECTED:
        goto peripheraldone;

      default:
        goto badstate;
    }
    break;

  case MESH_STATE_SYNCMASTERWRITING:
    switch (event)
    {
      case MESH_EVENT_WROTE:
        // Update RSSI
        node->sync.neighbor->rssi = (node->sync.neighbor->rssi + rssi) / 2;
        goto writing;

      case MESH_EVENT_DISCONNECTED:
        goto mastersynctimeout;

      case MESH_EVENT_IOFAILED:
        goto mastersyncdone;

      default:
        goto badstate;
    }
    break;

  case MESH_STATE_SYNCPERIPHERALWRITING:
    switch (event)
    {
      case MESH_EVENT_READING:
      {
        // Update RSSI
        node->sync.neighbor->rssi = (node->sync.neighbor->rssi + rssi) / 2;
      writing:;
        unsigned char pos = 0;
        for (;;)
        {
          if (node->sync.neighbor->flag.reset)
          {
            node->sync.neighbor->flag.reset = 0;
            if (node->state == MESH_STATE_SYNCMASTERWRITING)
            {
              node->sync.buffer[pos++] = MESH_PAYLOADRESET;
            }
          }
          else if (node->sync.count == 0)
          {
            if (pos + 1 <= MESH_MAX_WRITE_SIZE)
            {
              node->sync.buffer[pos++] = MESH_PAYLOADDONE;
              switch (node->state)
              {
                case MESH_STATE_SYNCMASTERWRITING:
                  node->state = MESH_STATE_SYNCMASTERREADING;
                  break;

                case MESH_STATE_SYNCPERIPHERALWRITING:
                  node->state = MESH_STATE_SYNCPERIPHERALDONE;
                  node->sync.neighbor->flag.valid = 1;
                  // We have all the differences from the neighbor - we're done
                  break;

                default:
                  goto badstate;
              }
            }
            break;
          }
          else if (node->sync.activeneighbors)
          {
            Mesh_NodeId nid = node->sync.activeneighbors->id;
            if (node->sync.neighbor != node->sync.activeneighbors && node->sync.activeneighbors->flag.valid && (
              (!node->ids[nid].flag.client && node->sync.activeneighbors->retries < MESH_NEIGHBOR_FORWARD_LIMIT) ||
              ( node->ids[nid].flag.client && node->ids[nid].flag.ping && !(node->sync.remainingbits & MESH_NEIGHBOR_TO_CHANGEBIT(node, node->sync.activeneighbors)))
            ))
            {
              if (pos + 1 + sizeof(Mesh_NodeAddress) + sizeof(Mesh_RSSI) > MESH_MAX_WRITE_SIZE)
              {
                break;
              }
              node->sync.buffer[pos++] = MESH_PAYLOADNEIGHBORS;
              Mesh_System_memmove(&node->sync.buffer[pos], &node->ids[nid].address, sizeof(Mesh_NodeAddress));
              // When a peripheral sends it neighbors, we cannot use them to eliminate links on the master. To prevent this we set
              // the rssi to be the worst it can be. Also, if the neighbor is flagged as having a badrssi, we may eliminate it later on. Since we are still
              // forwarding data to it now we send it over the the connected node, but flag the RSSI to be the worst possible so the connected node wont
              // eliminate it.
              if (node->state == MESH_STATE_SYNCPERIPHERALWRITING || node->sync.activeneighbors->flag.badrssi)
              {
                node->sync.buffer[pos + sizeof(Mesh_NodeAddress)] = MESH_RSSI_WORST;
              }
              else
              {
                node->sync.buffer[pos + sizeof(Mesh_NodeAddress)] = node->sync.activeneighbors->rssi;
              }
              pos += sizeof(Mesh_NodeAddress) + sizeof(Mesh_RSSI);
            }
            node->sync.activeneighbors--;
            if (node->sync.activeneighbors < &node->neighbors.neighbors[0])
            {
              node->sync.activeneighbors = NULL;
            }
          }
          else if (!(node->sync.ukv->changebits & node->sync.neighborchangebit))
          {
            node->sync.ukv++;
            node->sync.count--;
          }
          else if (node->sync.id != node->sync.ukv->id)
          {
            if (pos + 1 + sizeof(Mesh_NodeAddress) > MESH_MAX_WRITE_SIZE)
            {
              break;
            }
            node->sync.id = node->sync.ukv->id;
            node->sync.buffer[pos++] = MESH_PAYLOADNODEID;
            Mesh_System_memmove(&node->sync.buffer[pos], &node->ids[node->sync.id].address, sizeof(Mesh_NodeAddress));
            pos += sizeof(Mesh_NodeAddress);
          }
          else
          {
            if (Mesh_System_memcmp(&node->sync.value.key, &MESH_KEY_INVALID, sizeof(Mesh_Key)) == 0)
            {
              if (pos + 1 + sizeof(Mesh_Key) + sizeof(Mesh_Version) + 1 > MESH_MAX_WRITE_SIZE)
              {
                break;
              }
              node->sync.value.offset = 0;
              node->sync.value.key = node->sync.ukv->key;
              node->sync.buffer[pos++] = MESH_PAYLOADUKV;
              Mesh_System_memmove(&node->sync.buffer[pos], &node->sync.ukv->key, sizeof(Mesh_Key)); // mis-aligned
              *(Mesh_Version*)&node->sync.buffer[pos + sizeof(Mesh_Key)] = node->sync.ukv->version;
              node->sync.buffer[pos + sizeof(Mesh_Key) + sizeof(Mesh_Version)] = node->sync.ukv->length;
              pos += sizeof(Mesh_Key) + sizeof(Mesh_Version) + 1;
              if (pos >= MESH_MAX_WRITE_SIZE)
              {
                break;
              }
            }
            else
            {
              if (pos + 1 + 1 > MESH_MAX_WRITE_SIZE) // At least 1 data byte
              {
                break;
              }
              node->sync.buffer[pos++] = MESH_PAYLOADUKVDATA;
            }

            unsigned char remaining = node->sync.ukv->length - node->sync.value.offset;
            if (pos + remaining > MESH_MAX_WRITE_SIZE)
            {
              remaining = MESH_MAX_WRITE_SIZE - pos;
            }
            Mesh_System_memmove(&node->sync.buffer[pos], MESH_UKV_VALUE(node->sync.ukv) + node->sync.value.offset, remaining);
            node->sync.value.offset += remaining;
            pos += remaining;

            if (node->sync.value.offset == node->sync.ukv->length)
            {
              node->sync.ukv->changebits &= ~node->sync.neighborchangebit;
              node->sync.ukv++;
              node->sync.count--;
              node->sync.value.key = MESH_KEY_INVALID;
            }
          }
        }
        node->sync.bufferlen = pos;
        if (event == MESH_EVENT_READING)
        {
          Mesh_System_ReadAck(node);
        }
        else
        {
          Mesh_System_Write(node);
        }
        break;
      }

      case MESH_EVENT_DISCONNECTED:
      case MESH_EVENT_IOFAILED:
        goto peripheraldone;

      default:
        goto badstate;
    }
    break;

  case MESH_STATE_SYNCPERIPHERALDONE:
  case MESH_STATE_SYNCMASTERDONE:
    switch (event)
    {
      case MESH_EVENT_WROTE:
      case MESH_EVENT_DISCONNECTED:
      case MESH_EVENT_IOFAILED:
      {
      mastersyncdone:;
        // Remove any invalid neighbors
        node->sync.neighbor = NULL;
        switch (node->state)
        {
          case MESH_STATE_SYNCMASTERDONE:
          case MESH_STATE_SYNCMASTERREADING:
          case MESH_STATE_SYNCMASTERWRITING:
            node->state = MESH_STATE_SYNCMASTERDISCONNECTING;
            Mesh_System_Disconnect(node);
            break;

          case MESH_STATE_SYNCPERIPHERALDONE:
          peripheraldone:;
            node->sync.neighbor = NULL;
            for (Mesh_Neighbor* neighbor = &node->neighbors.neighbors[MESH_MAX_NEIGHBORS - 1]; neighbor >= &node->neighbors.neighbors[0]; neighbor--)
            {
              if (neighbor->id)
              {
                // Invalid, old and rssi distant neighbors are removed
                if (neighbor->retries >= MESH_MAX_RETRIES)
                {
                  Mesh_ForgetNeighbor(node, neighbor, 1);
                }
                else if (!neighbor->flag.valid)
                {
                  Mesh_ForgetNeighbor(node, neighbor, 0);
                }
                else if (neighbor->flag.badrssi)
                {
                  if (node->neighbors.count > MESH_NEIGHBOR_LIMIT)
                  {
                    Mesh_ForgetNeighbor(node, neighbor, 0);
                  }
                  else
                  {
                    neighbor->flag.badrssi = 0;
                  }
                }
              }
            }
            // Do we have new changes to send? If we do, attempt to find the ideal neighbor we should send the changes to first.
            Mesh_Neighbor* neighbor;
            unsigned char changes = 0;
            for (Mesh_UKV* ukv = &node->values.values[node->values.count - 1]; ukv >= &node->values.values[0]; ukv--)
            {
              if (ukv->changebits)
              {
                changes = 1;
                if (Mesh_FindNeighbor(node, ukv->id, &neighbor) == MESH_OK && (ukv->changebits & MESH_NEIGHBOR_TO_CHANGEBIT(node, neighbor)))
                {
                  node->sync.priority = ukv->id;
                  break;
                }
              }
            }
#if ENABLE_MESH_MALLOC
            Mesh_System_Free(node->sync.value.buffer);
            node->sync.value.buffer = NULL;
#endif
            if (changes)
            {
              node->state = MESH_STATE_SWITCHTOMASTER;
              Mesh_System_PeripheralDone(node);
              Mesh_System_MasterMode(node);
            }
            else
            {
              node->state = MESH_STATE_IDLE;
              Mesh_System_PeripheralDone(node);
            }
            break;

          default:
            goto badstate;
        }
        break;
      }

      default:
        goto badstate;
    }
    break;

  default:
  badstate:;
    status = MESH_BADSTATE;
    break;
  }

  return status;
}

//
// General id/key/value set function.
// Application should use the more generic Mesh_SetValue function below. This one allows more specific control over versioning, etc. and
// is used interally.
//
Mesh_Status Mesh_SetValueInternal(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, unsigned char* value, unsigned char length, unsigned char create, Mesh_Version version, Mesh_ChangeBits changebits)
{
  Mesh_UKV* values = node->values.values;
  unsigned short count;
  Mesh_NodeAddress* addr = NULL;
#if ENABLE_MESH_MALLOC
  unsigned char* data = NULL;
#endif

  for (count = node->values.count; count; count--, values++)
  {
    if (values->id == id && Mesh_System_memcmp(&values->key, &key, sizeof(Mesh_Key)) == 0)
    {
      if (Mesh_System_memcmp(MESH_UKV_VALUE(values), value, values->length))
      {
        if (key.rdonly)
        {
          return MESH_BADPERM;
        }
        values->version++;
        goto dosync;
      }
      return MESH_NOCHANGE;
    }
  }

  if (!create)
  {
    return MESH_NOTFOUND;
  }

  // Make sure we have space before we do anything
  count = node->values.count;
  if (count >= MESH_MAX_VALUES)
  {
    return MESH_OOM;
  }
  if (length > MESH_MAX_VALUE_SIZE)
  {
    return MESH_TOOBIG;
  }
  if (!MESH_UKV_CANINLINE(length))
  {
#if ENABLE_MESH_MALLOC
    data = Mesh_System_Malloc(length);
    if (data == NULL)
    {
      return MESH_OOM;
    }
#else
    return MESH_OOM;
#endif
  }
  
  // Insert the new id/key/value into the sorted UKV list
  addr = &node->ids[id].address;
  values = node->values.values;
  for (; count; count--, values++)
  {
    signed short r = (id == values->id ? 0 : Mesh_System_memcmp(&node->ids[values->id].address, addr, sizeof(Mesh_NodeAddress)));
    if (r == 0)
    {
      r = Mesh_System_memcmp(&values->key, &key, sizeof(Mesh_Key));
      if (r > 0)
      {
        goto move;
      }
      else if (r == 0)
      {
        return MESH_DUPLICATE;
      }
    }
    else if (r > 0)
    {
    move:;
      Mesh_System_memmove(values + 1, values, sizeof(Mesh_UKV) * count);
      break;
    }
  }
  // Goes at the end ...
  
  // Insert new id/key/value
  node->values.count++;
  values->id = id;
  values->key = key;
  values->length = length;
#if ENABLE_MESH_MALLOC
  if (data)
  {
    values->data.ptr = data;
  }
#endif
  values->version = version;
  node->ids[id].flag.ukv = 1;
dosync:;
  Mesh_System_memmove(MESH_UKV_VALUE(values), value, length);
  values->changebits = changebits;
  return MESH_OK;
}

//
// Get a specific id/key value.
//
Mesh_Status Mesh_GetValue(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, unsigned char* value, unsigned char* length)
{
  Mesh_UKV* values = node->values.values;
  for (unsigned short count = node->values.count; count; count--, values++)
  {
    if (values->id == id && Mesh_System_memcmp(&values->key, &key, sizeof(Mesh_Key)) == 0)
    {
      if (value == NULL)
      {
        *length = values->length;
        return MESH_OK;
      }
      else if (*length >= values->length)
      {
        *length = values->length;
        Mesh_System_memmove(value, MESH_UKV_VALUE(values), values->length);
        return MESH_OK;
      }
      else
      {
        return MESH_OOM;
      }
    }
  }
  return MESH_NOTFOUND;
}

//
// Iterator to retrieve key/value pairs on this node for a succession of NodeIDs.
//
Mesh_Status Mesh_GetNthValue(Mesh_Node* node, Mesh_Key key, unsigned char nth, Mesh_NodeId* id, unsigned char* value, unsigned char* length)
{
  Mesh_UKV* values = node->values.values;
  for (unsigned short count = node->values.count; count; count--, values++)
  {
    if (Mesh_System_memcmp(&values->key, &key, sizeof(Mesh_Key)) == 0)
    {
      if (!nth--)
      {
        if (value == NULL)
        {
          *length = values->length;
          return MESH_OK;
        }
        else if (*length >= values->length)
        {
          *id = values->id;
          *length = values->length;
          Mesh_System_memmove(value, MESH_UKV_VALUE(values), values->length);
          return MESH_OK;
        }
        else
        {
          return MESH_OOM;
        }
      }
    }
  }
  return MESH_NOTFOUND;
}

//
// Set a id/key/value from the local node.
//
Mesh_Status Mesh_SetValue(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, unsigned char* value, unsigned char length)
{
  // Keys which are read-only or write-local can only be set locally
  if ((key.wrlocal || key.rdonly) && id != MESH_NODEID_SELF)
  {
    return MESH_BADPERM;
  }
  Mesh_UKV* values = node->values.values;
  for (unsigned short count = node->values.count; count; count--, values++)
  {
    if (values->id == id && values->key.key == key.key && values->key.sub == key.sub && values->key.admin == key.admin)
    {
      // write-local needs to match
      if (values->key.wrlocal != key.wrlocal)
      {
        return MESH_BADPERM;
      }
      // Key must be read/write if we're modifying it
      if (values->key.rdonly || key.rdonly)
      {
        return MESH_BADPERM;
      }
      break;
    }
  }
  Mesh_Neighbor* neighbor;
  if (id != MESH_NODEID_SELF && node->sync.priority == MESH_NODEID_INVALID && Mesh_FindNeighbor(node, id, &neighbor) == MESH_OK)
  {
    node->sync.priority = id;
  }
  return Mesh_SetValueInternal(node, id, key, value, length, 1, 1, node->neighbors.changebits);
}

//
// Sync a specific id/key/value set.
// A sync will only succeed if we already have the matching UKV; we do not create new UKVs here.
//
Mesh_Status Mesh_SyncValue(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, unsigned char* value, unsigned char length, Mesh_Version version, Mesh_ChangeBits changebits)
{
  Mesh_UKV* current = node->values.values;
  unsigned short count;

  for (count = node->values.count; count; count--, current++)
  {
    if (current->id == id && Mesh_System_memcmp(&current->key, &key, sizeof(Mesh_Key)) == 0)
    {
      // Found a matching id/key. Now determine if we need to update the value
      unsigned char* ptr = MESH_UKV_VALUE(current);
      Mesh_Version verdiff = version - current->version;
      signed char valdiff = Mesh_System_memcmp(ptr, value, current->length);
      if ((verdiff != 0 && verdiff < MESH_VERSION_DIFF) || (verdiff == 0 && valdiff < 0))
      {
        // New version or same version but "bigger" value - update
        current->version = version;
        Mesh_System_memmove(ptr, value, current->length);
        current->changebits = changebits;
        return MESH_CHANGE;
      }
      if (verdiff == 0 && valdiff == 0)
      {
        // Version and value is identical. Remove any changebits we no longer need to sync
        current->changebits &= changebits;
      }
      return MESH_NOCHANGE;
    }
  }

  return MESH_NOTFOUND;
}

//
// Attempt to find an existing neigbor.
//
Mesh_Status Mesh_FindNeighbor(Mesh_Node* node, Mesh_NodeId id, Mesh_Neighbor** neighbor)
{
  if (node->ids[id].flag.neighbor)
  {
    Mesh_Neighbor* neighbors = node->neighbors.neighbors;
    unsigned char count;

    for (count = MESH_MAX_NEIGHBORS; count; count--, neighbors++)
    {
      if (neighbors->id == id)
      {
        *neighbor = neighbors;
        return MESH_OK;
      }
    }
  }
  return MESH_NOTFOUND;
}

//
// Add a new neighbor, marking any UKVs so we will sync them to it.
//
Mesh_Status Mesh_AddNeighbor(Mesh_Node* node, Mesh_NodeId id, Mesh_Neighbor** neighbor)
{
  if (node->neighbors.count >= MESH_MAX_NEIGHBORS)
  {
    return MESH_OOM;
  }

  Mesh_Neighbor* current;
  for (current = &node->neighbors.neighbors[0]; current->id; current++)
    ;

  current->id = id;
  current->retries = 0;
  static const struct Mesh_NeighborFlags defaultFlags =
  {
    .reset = 1
  };
  current->flag = defaultFlags;
  
  // Mark all UKVs to be synced to the neighbor
  Mesh_ChangeBits bit = MESH_NEIGHBOR_TO_CHANGEBIT(node, current);
  for (Mesh_UKV* ukv = &node->values.values[node->values.count - 1]; ukv >= &node->values.values[0]; ukv--)
  {
    ukv->changebits |= bit;
  }
  // Add new neighbor to the changebits
  node->neighbors.changebits |= bit;
  
  // Set the neighbor flag on the id
  node->ids[id].flag.neighbor = 1;
  node->neighbors.count++;
  
  *neighbor = current;
  
  return MESH_OK;
}

//
// Forget a neighbor.
// Neighbors are forgotten for a number of reasons, some of which are considered errors.
// If a neighbor is trimmed to make space for new neighors, or a neighbor is identified as no
// being another mesh node, then this is not an error. If a neighbor is forgotten because it can
// no longer be reached, this is an error.
// For errors, we attempt send any pending changes to other neighbors and hope they can sync them to
// their final desinations. We also schedule a discovery to reconnect with the mesh.
//
Mesh_Status Mesh_ForgetNeighbor(Mesh_Node* node, Mesh_Neighbor* neighbor, unsigned char error)
{
  // Remove neighbor from the changebits
  Mesh_NodeId id = neighbor->id;
  Mesh_ChangeBits mask = MESH_NEIGHBOR_TO_CHANGEBIT(node, neighbor);
  Mesh_ChangeBits unmask = ~mask;
  node->neighbors.changebits &= unmask;

  // Remove any value changebits with this neighbor
  Mesh_UKV* ukv = node->values.values;
  for (unsigned short count = node->values.count; count; count--, ukv++)
  {
    if (ukv->changebits & mask)
    {
      // UKV needed to be synced with the node we're forgetting. If we're doing this due to a node
      // error then we still need to try to sync the value.
      // If the UKV has change bits for other nodes, we know we'll send it out. Otherwise, we set
      // some change bits and bump the version so we know it'll get synced out.
      if (error && !(ukv->changebits & unmask))
      {
        ukv->changebits = node->neighbors.changebits;
        ukv->version++;
      }
      else
      {
        ukv->changebits &= unmask;
      }
    }
  }
  
  // Invalidate any matching priority
  if (node->sync.priority == id)
  {
    node->sync.priority = MESH_NODEID_INVALID;
  }

  // Clear the neighbor
  Mesh_System_memset(neighbor, 0, sizeof(Mesh_Neighbor));
  node->ids[id].flag.neighbor = 0;
  node->neighbors.count--;

  // For error node, blacklist, else if the node has no UKVs we can remove it.
  if (error)
  {
    node->ids[id].flag.ping = 1;
    node->ids[id].flag.blacklisted = 1;
    // Schedule a new discovery to rebuild the mesh
    Mesh_System_ScheduleDiscovery(node);
  }
  else if (MESH_NODEID_FREE(node, id))
  {
    Mesh_System_memset(&node->ids[id], 0, sizeof(node->ids[0]));
  }

  return MESH_OK;
}

//
// Remove all traces of this node - any neighbor info, key/values and interned
// address. Once this is all gone, flag that the hash should be advertised. We won't sync
// the values back from a neighbor unless they've changed (which means the node was alive
// after all).
//
Mesh_Status Mesh_ForgetNodeId(Mesh_Node* node, Mesh_NodeId id)
{
  // Remove any values with this id
  Mesh_UKV* ukv = node->values.values;
  for (unsigned short count = node->values.count; count; count--, ukv++)
  {
	// Found the beginning
	if (ukv->id == id)
	{
	  // Look for the end
	  unsigned short i;
	  for (i = 0; i < count && ukv[i].id == id; i++)
	  {
#if ENABLE_MESH_MALLOC
		if (!MESH_UKV_CANINLINE(ukv[i].length))
		{
		  Mesh_System_Free(ukv[i].data.ptr);
		}
#endif
	  }

	  // Remove the entries between the two
	  Mesh_System_memmove(ukv, ukv + i, sizeof(Mesh_UKV) * (count - i));
	  node->values.count -= i;
	  break;
	}
  }

  // Remove any neighbor with this id
  Mesh_Neighbor* neighbor = node->neighbors.neighbors;
  for (unsigned char count = MESH_MAX_NEIGHBORS; count; count--, neighbor++)
  {
    if (neighbor->id == id)
    {
      Mesh_ForgetNeighbor(node, neighbor, 0);
      return MESH_OK;
    }
  }

  // No neighbor so remove from the node address intern table ourself
  Mesh_System_memset(&node->ids[id], 0, sizeof(node->ids[0]));

  return MESH_OK;
}

#if ENABLE_MESH_TRIMMING
//
// Trim the mesh of old UKVs to make space for new ones.
//
// Mesh trimming is used to trim values from the mesh that that are old and not specifically needed by
// this node. Trimming is useful if the mesh contains more data than can be stored on a specific node.
// Trimming essentially turns each mesh node into a cache of recently changed values. Without trimming
// each node holds an entire copy of the values in the network. This can be useful if you want to read
// a value from a node which is not directly accessible.
//
Mesh_Status Mesh_Trim(Mesh_Node* node, unsigned char space)
{
  Mesh_UKV* selected = NULL;

  // Look for the oldest UKV which frees enough space but isn't a local or id'd value.
  // Start at a random offset into our UKVs so we dont always penalize lower ids.
  unsigned short count = node->values.count;
  Mesh_UKV* ukv = node->values.values + Mesh_System_RandomNumber(count);
  while (count--)
  {
    // Only consider UKVs which aren't SELF or GLOBAL, have no pending changes, and are big enough
    Mesh_NodeId nid = ukv->id;
    if (nid >= MESH_NODEID_FIRST_AVAILABLE && ukv->changebits == 0 && ukv->length >= space)
    {
      // Keep track of the oldest that matches our criteria
      if (selected == NULL)
      {
        selected = ukv;
      }
      else if (!node->ids[nid].flag.ping)
      {
        // Best option
        selected = ukv;
        break;
      }
    }
    if (--ukv < &node->values.values[0])
    {
      ukv = &node->values.values[node->values.count - 1];
    }
  }
  if (!selected)
  {
    return MESH_OOM;
  }

  // Free the selected UKV
#if ENABLE_MESH_MALLOC
  if (!MESH_UKV_CANINLINE(selected->length))
  {
    Mesh_System_Free(selected->data.ptr);
  }
#endif
  Mesh_NodeId nid = selected->id;
  // Remove the entries between the two
  Mesh_System_memmove(selected, selected + 1, sizeof(Mesh_UKV) * (node->values.count - (selected - node->values.values) - 1));
  node->values.count--;

  // Are they any remaining UKVs for the selected node
  if (selected[0].id != nid && (selected == node->values.values || selected[-1].id != nid))
  {
    node->ids[nid].flag.ukv = 0;
    // If id nolonger in use, remove it
    if (MESH_NODEID_FREE(node, nid))
    {
      Mesh_System_memset(&node->ids[nid], 0, sizeof(node->ids[0]));
    }
  }

  return MESH_OK;
}
#endif

//
// Reset the node and set it's MAC address.
//
Mesh_Status Mesh_NodeReset(Mesh_Node* node, Mesh_NodeAddress* address)
{
  static const Mesh_NodeAddress globalAddress =
  {
    .address = { 0, 0, 0, 0, 0, 0 }
  };
  static const Mesh_NodeAddress clientAddress =
  {
    .address = { 1, 0, 0, 0, 0, 0 }
  };
  Mesh_System_memset(node, 0, sizeof(Mesh_Node));
  node->state = MESH_STATE_IDLE;
  node->sync.priority = MESH_NODEID_INVALID;
  Mesh_InternNodeId(node, address, 1);
  node->ids[MESH_NODEID_SELF].flag.ukv = 1; // Force SELF it always be in use
  Mesh_InternNodeId(node, (Mesh_NodeAddress*)&globalAddress, 1);
  node->ids[MESH_NODEID_GLOBAL].flag.ukv = 1; // Force GLOBAL always to be in use
  Mesh_InternNodeId(node, (Mesh_NodeAddress*)&clientAddress, 1);
  node->ids[MESH_NODEID_CLIENT].flag.ukv = 1; // Force CLIENT always to be in use
  node->ids[MESH_NODEID_CLIENT].flag.client = 1;
  return MESH_OK;
}

//
// Sync any pending changes from the node if idle.
//
Mesh_Status Mesh_Sync(Mesh_Node* node)
{
  if (node->state == MESH_STATE_IDLE)
  {
    return Mesh_Process(node, MESH_EVENT_SYNC, 0, 0);
  }
  else
  {
    return MESH_BUSY;
  }
}

//
// Intern a NodeAddress and return the NodeId representing it.
//
Mesh_NodeId Mesh_InternNodeId(Mesh_Node* node, Mesh_NodeAddress* id, unsigned char create)
{
  Mesh_NodeId free = MESH_NODEID_INVALID;
  Mesh_NodeId bfree = MESH_NODEID_INVALID;

  // Look through the set of node ids and find a match or a space for a new one.
  for (unsigned char i = 0; i < MESH_MAX_NODES; i++)
  {
    if (Mesh_System_memcmp(&node->ids[i].address, id, sizeof(Mesh_NodeAddress)) == 0)
    {
      if (create)
      {
        node->ids[i].flag.ping = 1;
      }
      return i;
    }
    if (free == MESH_NODEID_INVALID && !(node->ids[i].flag.neighbor || node->ids[i].flag.ukv))
    {
      if (!node->ids[i].flag.blacklisted)
      {
        free = i;
      }
      else if (bfree == MESH_NODEID_INVALID || !node->ids[i].flag.ping)
      {
        bfree = i;
      }
    }
  }
  
  if (!create)
  {
    return MESH_NODEID_INVALID;
  }

  // If we couldn't find a free spot, reuse the oldest blacklisted node
  if (free == MESH_NODEID_INVALID)
  {
    free = bfree;
  }

  // If not found, try to find space for a new one
  if (free != MESH_NODEID_INVALID)
  {
    static const struct Mesh_NodeFlags defaultFlags =
    {
      .ping = 1
    };
    Mesh_System_memmove(&node->ids[free].address, id, sizeof(Mesh_NodeAddress));
    node->ids[free].flag = defaultFlags;
  }

  return free;
}

//
// Get the NodeAddress associated with the NodeId.
//
Mesh_NodeAddress* Mesh_GetNodeAddress(Mesh_Node* node, Mesh_NodeId id)
{
  return &node->ids[id].address;
}

//
// Calculate the set of neighbors to send pending changes to.
//
Mesh_ChangeBits Mesh_GetChangeBits(Mesh_Node* node)
{
  Mesh_ChangeBits changes = 0;
  for (Mesh_UKV* ukv = &node->values.values[node->values.count - 1]; ukv >= &node->values.values[0]; ukv--)
  {
    changes |= ukv->changebits;
  }
  return changes;
}
