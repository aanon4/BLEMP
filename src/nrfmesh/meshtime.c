/*
 * meshtime.c
 *
 *  Created on: Aug 21, 2015
 *      Author: tim
 */


#include <ble.h>
#include <nordic_common.h>
#include <ble_gatts.h>
#include <ble_hci.h>
#include <app_error.h>
#include <nrf_soc.h>

#include "nrfmesh.h"
#include "uuids.h"
#include "timer.h"
#include "meshtime.h"

#define MESHTIME_NR_CLOCKS          MESH_MAX_NEIGHBORS
#define MESHTIME_CONFIDENCE_UP      (1000 * 60 * 60 * 2)  // 2 hour
#define MESHTIME_CONFIDENCE_DOWN    (1000 * 60 * 60 * 12) // 12 hours
#define MESHTIME_SCALEOFFSET        30LL

const Mesh_Key MESH_KEY_KEEPALIVE = _MESH_KEY_KEEPALIVE;
const Mesh_Key MESH_KEY_TIME = _MESH_KEY_TIME;

typedef struct
{
  uint32_t time;              // External time for last update
  Mesh_NodeId id;             // ID of clock
  uint8_t  confidence;        // Confidence clock is correct. The higher the better
  uint64_t tick;              // Tick value for last update
  uint64_t tickdiff;          // Ticks used to calculate scale
  uint64_t scale;             // Number of ticks per second (34.30 fixed point)
} meshtime_clock;

static struct
{
  uint8_t         keepalive;
  meshtime_clock* clock;  // Selected clock
  meshtime_clock  clocks[MESHTIME_NR_CLOCKS];
} meshtime_state;

void meshtime_timer_handler(void)
{
  if (TIMER_N_MINUTES(MESH_TIMESYNC_UPDATE / 60))
  {
    Mesh_Clock time = {};
    Mesh_System_GetClock(&mesh_node, MESH_NODEID_SELF, &time);
    Mesh_SetValue(&mesh_node, MESH_NODEID_GLOBAL, MESH_KEY_TIME, (uint8_t*)&time, sizeof(time));
  }
  if (TIMER_N_MINUTES(MESH_TIMESYNC_TIME / 60))
  {
    meshtime_state.keepalive++;
    Mesh_SetValue(&mesh_node, MESH_NODEID_SELF, MESH_KEY_KEEPALIVE, (uint8_t*)&meshtime_state.keepalive, sizeof(meshtime_state.keepalive));
    Mesh_Process(&mesh_node, MESH_EVENT_KEEPALIVE, 0, 0);
  }
}

uint64_t meshtime_tick(void)
{
  static uint64_t bigticks;
  static uint32_t lastticks;

  uint32_t err_code;

  uint32_t count;
  err_code = app_timer_cnt_get(&count); // MAX == 0x00FFFFFF
  APP_ERROR_CHECK(err_code);

  if (count < lastticks)
  {
    bigticks += 0x0000000001000000LL;
  }
  lastticks = count;
  bigticks = (bigticks & 0xFFFFFFFFFF000000) | count;

  // Convert machine ticks to milliseconds
  return (bigticks * APP_TIMER_PRESCALER * 1000) / 32768;
}

static void meshtime_updateclock(Mesh_NodeId id, Mesh_Clock* remotetime)
{
  uint64_t tick = meshtime_tick();

  meshtime_clock* clock = &meshtime_state.clocks[0];
  if (!mesh_node.ids[id].flag.client)
  {
    meshtime_clock* free = NULL;
    meshtime_clock* old = NULL;
    for (clock++; clock < &meshtime_state.clocks[MESHTIME_NR_CLOCKS]; clock++)
    {
      // Found matching clock
      if (clock->id == id)
      {
        goto found;
      }
      // Found an empty clock we may use if we don't find a match
      else if (!clock->time)
      {
        if (!free)
        {
          free = clock;
        }
      }
      // Found an old clock we can reuse, as long as it's at least MESHTIME_MAX_TICKDIFF old
      else if (tick - clock->tick > MESHTIME_CONFIDENCE_UP && (!old || clock->tick < old->tick))
      {
        old = clock;
      }
    }
    // Use a free clock if we can
    if (free)
    {
      clock = free;
    }
    // Otherwise use the oldest
    else if (old)
    {
      clock = old;
      clock->time = 0;
    }
    // If all else fails, just ignore this clock
    else
    {
      return;
    }
  }
found:;

  if (clock->time)
  {
    uint64_t tickdiff = tick - clock->tick;
    if (tickdiff)
    {
      uint64_t lastdiff = clock->tickdiff;
      if (tickdiff + lastdiff > MESHTIME_CONFIDENCE_UP)
      {
        lastdiff = tickdiff > MESHTIME_CONFIDENCE_UP ? 0 : MESHTIME_CONFIDENCE_UP - tickdiff;
      }
      clock->tickdiff = tickdiff + lastdiff;
      uint64_t scale = ((((uint64_t)(remotetime->time - clock->time)) << MESHTIME_SCALEOFFSET) + clock->scale * lastdiff) / clock->tickdiff;

      // The confidence of the clock reflects how likely it is to be accurate - having the correct time now and the correct time in the future.
      // The higher the value, the likelier it is that the clock is good.
      // The clock can never be better than the remote clock it sync's against, so that limits the confidence. We also limit it if we don't have
      // a sufficiently long sample time. We assume that if subsequent calculations of the scale are the same, the clock is running accurately, but
      // may not have the correct base time yet.
      uint64_t confidence;
      if (scale < clock->scale)
      {
        confidence = (255 * scale) / clock->scale;
      }
      else if (clock->scale < scale)
      {
        confidence = (255 * clock->scale) / scale;
      }
      else
      {
        confidence = 0;
      }
      if (clock->tickdiff < MESHTIME_CONFIDENCE_UP)
      {
        confidence = (confidence * clock->tickdiff) / MESHTIME_CONFIDENCE_UP;
      }
      confidence = (confidence * remotetime->confidence) >> 8;

      clock->confidence = (uint8_t)confidence;
      clock->scale = scale;
    }
  }
  else
  {
    clock->tickdiff = 0;
    clock->scale = (1LL << MESHTIME_SCALEOFFSET) / 1000LL;
    clock->confidence = 1;
    clock->id = id;
  }
  clock->time = remotetime->time;
  clock->tick = tick;
}

static uint8_t meshtime_getconfidence(meshtime_clock* clock)
{
  // We decay the confidence value of the clock the longer it is since it was synchronized
  uint64_t since = meshtime_tick() - clock->tick;
  uint32_t decay = 0;
  if (since < MESHTIME_CONFIDENCE_DOWN)
  {
    decay = (100 * (MESHTIME_CONFIDENCE_DOWN - since)) / MESHTIME_CONFIDENCE_DOWN;
  }
  return (uint8_t)((clock->confidence * (900 + decay)) / 1000);
}

void Mesh_System_SetClock(Mesh_Node* node, Mesh_NodeId id, Mesh_Clock* remotetime)
{
  meshtime_updateclock(id, remotetime);

  // Generate our own clock based on the clocks we know about with the highest confidence
  meshtime_clock* selected = &meshtime_state.clocks[0];
  uint8_t selectedconfidence = meshtime_getconfidence(selected);
  for (meshtime_clock* clock = &meshtime_state.clocks[1]; clock < &meshtime_state.clocks[MESHTIME_NR_CLOCKS]; clock++)
  {
    if (clock->time)
    {
      uint8_t clockconfidence = meshtime_getconfidence(clock);
      if (clockconfidence > selectedconfidence)
      {
        selected = clock;
        selectedconfidence = clockconfidence;
      }
    }
  }
  meshtime_state.clock = selected;

  Mesh_Clock time = {};
  Mesh_System_GetClock(&mesh_node, MESH_NODEID_SELF, &time);
  Mesh_SetValue(&mesh_node, MESH_NODEID_GLOBAL, MESH_KEY_TIME, (uint8_t*)&time, sizeof(time));
}

uint32_t meshtime_currenttime(void)
{
  if (meshtime_state.clock->time)
  {
    return meshtime_state.clock->time + (uint32_t)(((meshtime_tick() - meshtime_state.clock->tick) * meshtime_state.clock->scale) >> MESHTIME_SCALEOFFSET);
  }
  else
  {
    return 0;
  }
}

void Mesh_System_GetClock(Mesh_Node* node, Mesh_NodeId id, Mesh_Clock* localtime)
{
  localtime->confidence = meshtime_getconfidence(meshtime_state.clock);
  localtime->time = meshtime_currenttime();
}

void meshtime_init(void)
{
  Mesh_Clock time = {};
  Mesh_SetValue(&mesh_node, MESH_NODEID_GLOBAL, MESH_KEY_TIME, (uint8_t*)&time, sizeof(time));
  meshtime_state.clock = &meshtime_state.clocks[0];
#if 1
  uint32_t err_code;

  static const ble_gatts_attr_md_t metadata =
  {
    .read_perm = { 1, 2 },
    .vlen = 1,
    .vloc = BLE_GATTS_VLOC_USER
  };
  static const ble_uuid_t uuid =
  {
    .type = UUIDS_BASE_TYPE,
    .uuid = 10
  };
  static const ble_gatts_attr_t attr =
  {
    .p_uuid = (ble_uuid_t*)&uuid,
    .p_attr_md = (ble_gatts_attr_md_t*)&metadata,
    .init_len = sizeof(meshtime_state),
    .max_len = sizeof(meshtime_state),
    .p_value = (uint8_t*)&meshtime_state
  };
  static const ble_gatts_char_md_t characteristic =
  {
    .char_props.read = 1,
    .p_char_user_desc = "Time",
    .char_user_desc_max_size = 4,
    .char_user_desc_size = 4
  };
  ble_gatts_char_handles_t newhandle;
  err_code = sd_ble_gatts_characteristic_add(primary_service_handle, &characteristic, &attr, &newhandle);
  APP_ERROR_CHECK(err_code);
#endif
}
