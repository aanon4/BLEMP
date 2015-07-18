//
//  mesh.h
//  Mesh
//
//  Created by tim on 1/8/15.
//  Copyright (c) 2015 tim. All rights reserved.
//

#ifndef Mesh_mesh_h
#define Mesh_mesh_h

//
// Total RAM usage is defined as follow:
//
// 49 + 4 * ceil(MESH_MAX_NEIGHBORS / 8) + (MESH_MAX_NODES * 7) + (MESH_MAX_NEIGHBORS * 6) +
//   (MESH_MAX_VALUES * (5 + MESH_DEFAULT_VALUE_SIZE + ceil(MESH_MAX_NEIGHBORS / 8)))
//

//
// The maximum number of nodes supported in the mesh.
//
#if !defined(MESH_MAX_NODES)
#define MESH_MAX_NODES                16
#endif

//
// The maximum number of neighbors (directly accessible nodes) supported by each node.
// NOTE: sizeof(Mesh_ChangeBits) * 8 >= MESH_MAX_NEIGHBOR
//
#if !defined(MESH_MAX_NEIGHBORS)
#define MESH_MAX_NEIGHBORS             8
#endif

//
// The maximum number of values supported by each node.
//
#if !defined(MESH_MAX_VALUES)
#define MESH_MAX_VALUES                (MESH_MAX_NODES * 8)
#endif

//
// Enable mesh trimming.
// Trimming allows for more unique values in the network than defined by MESH_MAX_VALUES.
// Each node will see every value, but will only cache the most recent changes.
//
#if !defined(ENABLE_MESH_TRIMMING)
#define ENABLE_MESH_TRIMMING           0
#endif

//
// The default, preallocate space for each value.
// If a value doesn't fit into this size and malloc is available, it is used to allocate a large space.
//
#if !defined(MESH_DEFAULT_VALUE_SIZE)
#define MESH_DEFAULT_VALUE_SIZE       (sizeof(unsigned char*))
#endif

//
// The maximum value size a value can be.
//
#if !defined(MESH_MAX_VALUE_SIZE)
#define MESH_MAX_VALUE_SIZE           32
#endif

//
// Enable mesh malloc.
// Without malloc, mesh values are restricted to a maximum size of MESH_DEFAULT_VALUE_SIZE bytes.
//
#if !defined(ENABLE_MESH_MALLOC)
#define ENABLE_MESH_MALLOC             1
#endif

//
// The UUID used to identify this network.
//
#if !defined(MESH_UUID)
#define MESH_UUID   0x21, 0xAB, 0xBA, 0x78, 0x15, 0x75, 0x4D, 0xBB, 0x9C, 0x8B, 0xCB, 0x68, 0x08, 0xB2, 0x30, 0xED
#endif

//
// Limit the number of neighbor nodes to allow space for new nodes to join.
//
#define MESH_NEIGHBOR_LIMIT           (MESH_MAX_NEIGHBORS - 4)

//
// Limit the number of retries when connecting to a neighbor. After enough failures a neighbor is forgotten.
//
#define	MESH_MAX_RETRIES              16

//
// Only tell neighbors about our other neighbors if they are reliable (have few retries).
//
#define MESH_NEIGHBOR_FORWARD_LIMIT   (MESH_MAX_RETRIES / 4)

//
// Nodes periodically send keepalive messages. If nodes messages aren't seen after 2 sweeps the node
// will be forgotten (assumed removed or failed).
//
#define MESH_KEEPALIVE_TIME           (15 * 60)                     // 15 minutes
#define MESH_KEEPALIVE_SWEEP_TIME     (2 * MESH_KEEPALIVE_TIME)     // 30 minutes

//
// Standard BLE sizes
//
#define MESH_NODEADDRESS_SIZE          6  // MAC address
#define MESH_MAX_WRITE_SIZE           20
#define MESH_MAX_READ_SIZE            20

#define MESH_BUFFER_SIZE              (MESH_MAX_WRITE_SIZE > MESH_MAX_READ_SIZE ? MESH_MAX_WRITE_SIZE : MESH_MAX_READ_SIZE)


typedef unsigned char Mesh_NodeId;
#define MESH_NODEID_INVALID           ((Mesh_NodeId)-1)
#define MESH_NODEID_SELF              ((Mesh_NodeId)0)
#define MESH_NODEID_GLOBAL            ((Mesh_NodeId)1)

typedef struct
{
  unsigned short  key;
  unsigned char   sub:4;
  unsigned char   rdonly:1;
  unsigned char   wrlocal:1;
  unsigned char   unused:1;
  unsigned char   admin:1;
} __attribute__((packed)) Mesh_Key;
//#define	MESH_KEY_INTERNAL             0xFF00
//#define MESH_KEY_KEEPALIVE            (MESH_KEY_INTERNAL + 0)
//#define MESH_KEY_INVALID              0xFFFF

typedef unsigned char Mesh_Version;
#define MESH_VERSION_DIFF             ((Mesh_Version)0x7F)

typedef unsigned char Mesh_ChangeBits; // sizeof(Mesh_ChangeBits) == MESH_MAX_NEIGHBORS / 8
#define MESH_NEIGHBOR_TO_CHANGEBIT(NODE, NEIGHBOR)  ((Mesh_ChangeBits)(1 << ((NEIGHBOR) - ((NODE)->neighbors.neighbors))))

typedef unsigned long Mesh_Tick;
typedef signed char Mesh_RSSI;
#define MESH_RSSI_WORST               (-128)

typedef struct Mesh_NodeAddress
{
  unsigned char address[MESH_NODEADDRESS_SIZE];
} Mesh_NodeAddress;

//
// A unique id/key/value in the network.
//
typedef struct Mesh_UKV
{
  Mesh_NodeId     id;
  Mesh_Version    version;
  Mesh_Key        key;
  Mesh_ChangeBits changebits;
  unsigned char   length;
  union
  {
	  unsigned char  value[MESH_DEFAULT_VALUE_SIZE];
#if ENABLE_MESH_MALLOC
	  unsigned char* ptr;
#endif
  }               data;
} Mesh_UKV;

#define	MESH_UKV_CANINLINE(L)	((L) <= sizeof(((Mesh_UKV*)0)->data))
#if ENABLE_MESH_MALLOC
#define	MESH_UKV_VALUE(V)     ((V)->length > sizeof((V)->data) ? (V)->data.ptr : (V)->data.value)
#else
#define	MESH_UKV_VALUE(V)     ((V)->length > sizeof((V)->data) ? ((unsigned char*)0) : (V)->data.value)
#endif

//
// State of a neighboring node.
//
typedef struct Mesh_Neighbor
{
  Mesh_NodeId   id;
  unsigned char retries;
  Mesh_RSSI     rssi;
  unsigned short handle;
} Mesh_Neighbor;

//
// Mesh states.
//
typedef enum Mesh_State
{
  MESH_STATE_IDLE = 0,
  MESH_STATE_SWITCHTOMASTER,
  MESH_STATE_SWITCHTOPERIPHERAL,
  MESH_STATE_SYNCMASTERCONNECTING,
  MESH_STATE_SYNCMASTERREADING,
  MESH_STATE_SYNCMASTERWRITING,
  MESH_STATE_SYNCMASTERDONE,
  MESH_STATE_SYNCMASTERDISCONNECTING,
  MESH_STATE_SYNCPERIPHERALWRITING,
  MESH_STATE_SYNCPERIPHERALREADING,
  MESH_STATE_SYNCPERIPHERALDONE
} Mesh_State;

//
// The state defining a mesh node.
// There is only one of these per device.
//
typedef struct Mesh_Node
{
  Mesh_State    state;
  Mesh_Tick     lastsync;
  unsigned char keepalive;

  //
  // All nodes known to this node.
  //
  struct
  {
    Mesh_NodeAddress address;
    struct Mesh_NodeFlags
    {
      unsigned char neighbor:1;     // inuse as a neighbor
      unsigned char ukv:1;          // inuse in a key/value
      unsigned char blacklisted:1;  // id has been blacklisted

      unsigned char ping:1;         // id seen recently

      // These flags are only valid when 'neighbor' is set
      unsigned char reset:1;        // neighbor is new
      unsigned char valid:1;        // neighbor is valid
      unsigned char retry:1;        // neighbor needs retry
      unsigned char badrssi:1;      // neighbor candidate for removal
    } flag;
  }             ids[MESH_MAX_NODES];

  //
  // id/key/values known by this node.
  //
  struct
  {
    unsigned short  count;
    Mesh_UKV        values[MESH_MAX_VALUES];
  }             values;

  //
  // Neighbor nodes we directly communicate with.
  //
  struct
  {
    Mesh_ChangeBits changebits;
    unsigned char   count;
    Mesh_Neighbor   neighbors[MESH_MAX_NEIGHBORS];
  }             neighbors;

  //
  // State used to manage syncing.
  //
  struct
  {
    Mesh_Neighbor*  neighbor;
    Mesh_NodeId     id;
    Mesh_NodeId     priority;
    Mesh_ChangeBits neighborchangebit;
    Mesh_ChangeBits changebits;
    Mesh_UKV*       ukv;
    unsigned short  count;
    unsigned char   buffer[MESH_BUFFER_SIZE];
    unsigned char   bufferlen;
    Mesh_Neighbor*  activeneighbors;
    struct
    {
#if MESH_MAX_VALUE_SIZE <= MESH_MAX_WRITE_SIZE - 5
      // Not used if we can always fit a value into a write buffer
      unsigned char   buffer[0];
#elif ENABLE_MESH_MALLOC
      unsigned char*  buffer;
#else
      unsigned char   buffer[MESH_MAX_VALUE_SIZE];
#endif
      Mesh_Key        key;
      Mesh_Version    version;
      unsigned char   offset;
      unsigned char   length;
    } value;
  }               sync;
} Mesh_Node;

#define MESH_NODEID_FREE(NODE, ID) \
  (!((NODE)->ids[ID].flag.neighbor || (NODE)->ids[ID].flag.ukv || (NODE)->ids[ID].flag.blacklisted))

//
// Mesh events.
//
typedef enum Mesh_Event
{
  MESH_EVENT_NULL = 0,
  MESH_EVENT_NEIGHBOR_DISCOVER,
  MESH_EVENT_INMASTERMODE,
  MESH_EVENT_INPERIPHERALMODE,
  MESH_EVENT_CONNECTED,
  MESH_EVENT_CONNECTIONFAILED,
  MESH_EVENT_DISCONNECTED,
  MESH_EVENT_INCOMINGCONNECTION,
  MESH_EVENT_READING,
  MESH_EVENT_READ,
  MESH_EVENT_WRITE,
  MESH_EVENT_WROTE,
  MESH_EVENT_IOFAILED,
  MESH_EVENT_INVALIDNODE,
  MESH_EVENT_SYNC,
  MESH_EVENT_RETRY,
  MESH_EVENT_KEEPALIVE
} Mesh_Event;

//
// Mesh errors.
//
typedef enum Mesh_Status
{
  MESH_OK = 0,
  MESH_NOTFOUND,
  MESH_DUPLICATE,
  MESH_OOM,
  MESH_CHANGE,
  MESH_NOCHANGE,
  MESH_BADSTATE,
  MESH_BADPAYLOAD,
  MESH_BUSY,
  MESH_TOOBIG,
  MESH_BADPERM,
} Mesh_Status;

//
// Mesh payload types.
// These are used in the internal communication sync payload exchanged between nodes.
//
typedef enum Mesh_Payload
{
  MESH_PAYLOADNODEID = 1,
  MESH_PAYLOADUKV = 2,
  MESH_PAYLOADNEIGHBORS = 3,
  MESH_PAYLOADRESET = 4,
  MESH_PAYLOADUKVDATA = 5,
  MESH_PAYLOADDONE = 127,
} Mesh_Payload;

//
// Mesh processor. This event driver function manage the mesh.
//
extern Mesh_Status Mesh_Process(Mesh_Node* node, Mesh_Event event, unsigned char arg, Mesh_RSSI rssi);

//
// Get, set and sync values to the mesh.
//
extern Mesh_Status Mesh_GetValue(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, unsigned char* value, unsigned char* length);
extern Mesh_Status Mesh_SetValue(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, unsigned char* value, unsigned char length);
extern Mesh_Status Mesh_GetNthValue(Mesh_Node* node, Mesh_Key key, unsigned char nth, Mesh_NodeId* id, unsigned char* value, unsigned char* length);
extern Mesh_Status Mesh_SetValueInternal(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, unsigned char* value, unsigned char length, unsigned char create, Mesh_Version version, Mesh_ChangeBits changebits);
extern Mesh_Status Mesh_SyncValue(Mesh_Node* node, Mesh_NodeId id, Mesh_Key key, unsigned char* value, unsigned char length, Mesh_Version version, Mesh_ChangeBits changebits);

//
// Manage neighbors and nodes in the mesh.
//
extern Mesh_Status Mesh_FindNeighbor(Mesh_Node* node, Mesh_NodeId id, Mesh_Neighbor** neighbor);
extern Mesh_Status Mesh_AddNeighbor(Mesh_Node* node, Mesh_NodeId id, Mesh_Neighbor** neighbor);
extern Mesh_Status Mesh_ForgetNeighbor(Mesh_Node* node, Mesh_Neighbor* neighbor, unsigned char error);
extern Mesh_Status Mesh_ForgetNodeId(Mesh_Node* node, Mesh_NodeId id);

#if ENABLE_MESH_TRIMMING
extern Mesh_Status Mesh_Trim(Mesh_Node* node, unsigned char space);
#endif

extern Mesh_Status Mesh_NodeReset(Mesh_Node* node, Mesh_NodeAddress* address);
extern Mesh_NodeId Mesh_InternNodeId(Mesh_Node* node, Mesh_NodeAddress* id, unsigned char create);
extern Mesh_NodeAddress* Mesh_GetNodeAddress(Mesh_Node* node, Mesh_NodeId id);
extern Mesh_ChangeBits Mesh_GetChangeBits(Mesh_Node* node);
extern Mesh_Status Mesh_Sync(Mesh_Node* node);

#endif
