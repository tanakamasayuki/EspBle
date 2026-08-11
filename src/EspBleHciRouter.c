#include "EspBleHciRouter.h"

#include <string.h>

enum
{
  H4_COMMAND = 0x01,
  H4_ACL = 0x02,
  H4_SCO = 0x03,
  H4_EVENT = 0x04,
  H4_ISO = 0x05,
  EVT_CONNECTION_COMPLETE = 0x03,
  EVT_DISCONNECTION_COMPLETE = 0x05,
  EVT_AUTHENTICATION_COMPLETE = 0x06,
  EVT_ENCRYPTION_CHANGE = 0x08,
  EVT_READ_REMOTE_FEATURES_COMPLETE = 0x0b,
  EVT_READ_REMOTE_VERSION_COMPLETE = 0x0c,
  EVT_COMMAND_COMPLETE = 0x0e,
  EVT_COMMAND_STATUS = 0x0f,
  EVT_HARDWARE_ERROR = 0x10,
  EVT_NUMBER_COMPLETED_PACKETS = 0x13,
  EVT_MODE_CHANGE = 0x14,
  EVT_DATA_BUFFER_OVERFLOW = 0x1a,
  EVT_SYNCHRONOUS_CONNECTION_COMPLETE = 0x2c,
  EVT_ENCRYPTION_KEY_REFRESH_COMPLETE = 0x30,
  EVT_LE_META = 0x3e,
  LE_CONNECTION_COMPLETE = 0x01,
  LE_ENHANCED_CONNECTION_COMPLETE = 0x0a,
  OPCODE_HOST_NUMBER_COMPLETED_PACKETS = 0x0c35,
};

static uint16_t read_le16(const uint8_t *data)
{
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static bool one_host(espble_hci_route_t owner)
{
  return owner == ESPBLE_HCI_ROUTE_NIMBLE || owner == ESPBLE_HCI_ROUTE_CLASSIC;
}

static bool valid_event(const uint8_t *packet, size_t length)
{
  return packet != NULL && length >= 3 && packet[0] == H4_EVENT &&
    (size_t)packet[2] + 3u <= length;
}

static espble_hci_route_t owner_for_handle(
  const espble_hci_router_t *router, uint16_t handle)
{
  handle &= 0x0fff;
  for (size_t i = 0; i < ESPBLE_HCI_ROUTER_MAX_CONNECTIONS; ++i)
  {
    if (router->connections[i].used && router->connections[i].handle == handle)
    {
      return (espble_hci_route_t)router->connections[i].owner;
    }
  }
  return ESPBLE_HCI_ROUTE_NONE;
}

static bool set_handle_owner(
  espble_hci_router_t *router, uint16_t handle, espble_hci_route_t owner)
{
  handle &= 0x0fff;
  for (size_t i = 0; i < ESPBLE_HCI_ROUTER_MAX_CONNECTIONS; ++i)
  {
    if (router->connections[i].used && router->connections[i].handle == handle)
    {
      router->connections[i].owner = (uint8_t)owner;
      return true;
    }
  }
  for (size_t i = 0; i < ESPBLE_HCI_ROUTER_MAX_CONNECTIONS; ++i)
  {
    if (!router->connections[i].used)
    {
      router->connections[i].used = true;
      router->connections[i].handle = handle;
      router->connections[i].owner = (uint8_t)owner;
      return true;
    }
  }
  return false;
}

static void remove_handle(espble_hci_router_t *router, uint16_t handle)
{
  handle &= 0x0fff;
  for (size_t i = 0; i < ESPBLE_HCI_ROUTER_MAX_CONNECTIONS; ++i)
  {
    if (router->connections[i].used && router->connections[i].handle == handle)
    {
      router->connections[i].used = false;
      return;
    }
  }
}

static espble_hci_route_t take_command_owner(
  espble_hci_router_t *router, uint16_t opcode)
{
  for (size_t i = 0; i < router->pending_count; ++i)
  {
    if (router->pending[i].opcode == opcode)
    {
      espble_hci_route_t owner = (espble_hci_route_t)router->pending[i].owner;
      if (i + 1u < router->pending_count)
      {
        memmove(&router->pending[i], &router->pending[i + 1u],
          (router->pending_count - i - 1u) * sizeof(router->pending[0]));
      }
      --router->pending_count;
      return owner;
    }
  }
  return ESPBLE_HCI_ROUTE_NONE;
}

void espble_hci_router_init(espble_hci_router_t *router)
{
  if (router != NULL) memset(router, 0, sizeof(*router));
}

bool espble_hci_router_owns_handle(
  const espble_hci_router_t *router, espble_hci_route_t owner,
  uint16_t handle)
{
  return router != NULL && one_host(owner) &&
    owner_for_handle(router, handle) == owner;
}

espble_hci_router_result_t espble_hci_router_track_outgoing(
  espble_hci_router_t *router, espble_hci_route_t owner,
  const uint8_t *packet, size_t length)
{
  if (router == NULL || !one_host(owner) || packet == NULL || length == 0)
  {
    return ESPBLE_HCI_ROUTER_INVALID_PACKET;
  }

  if (packet[0] == H4_COMMAND)
  {
    if (length < 4 || (size_t)packet[3] + 4u > length)
    {
      return ESPBLE_HCI_ROUTER_INVALID_PACKET;
    }
    const uint16_t opcode = read_le16(&packet[1]);
    // This is host-to-controller flow-control accounting, not a command that
    // produces Command Complete or Command Status (Core Vol. 4, Part E, 7.3.40).
    if (opcode == OPCODE_HOST_NUMBER_COMPLETED_PACKETS)
      return ESPBLE_HCI_ROUTER_OK;
    if (router->pending_count == ESPBLE_HCI_ROUTER_MAX_PENDING_COMMANDS)
    {
      return ESPBLE_HCI_ROUTER_QUEUE_FULL;
    }
    espble_hci_pending_command_t *entry = &router->pending[router->pending_count++];
    entry->opcode = opcode;
    entry->owner = (uint8_t)owner;
    return ESPBLE_HCI_ROUTER_OK;
  }

  if (packet[0] == H4_ACL)
  {
    if (length < 5 || (size_t)read_le16(&packet[3]) + 5u > length)
    {
      return ESPBLE_HCI_ROUTER_INVALID_PACKET;
    }
    espble_hci_route_t actual = owner_for_handle(router, read_le16(&packet[1]));
    return actual == owner ? ESPBLE_HCI_ROUTER_OK : ESPBLE_HCI_ROUTER_UNKNOWN_HANDLE;
  }

  if (packet[0] == H4_SCO)
  {
    if (length < 4 || (size_t)packet[3] + 4u > length)
      return ESPBLE_HCI_ROUTER_INVALID_PACKET;
    return owner == ESPBLE_HCI_ROUTE_CLASSIC ? ESPBLE_HCI_ROUTER_OK :
      ESPBLE_HCI_ROUTER_INVALID_PACKET;
  }
  if (packet[0] == H4_ISO)
  {
    return owner == ESPBLE_HCI_ROUTE_NIMBLE ? ESPBLE_HCI_ROUTER_OK :
      ESPBLE_HCI_ROUTER_INVALID_PACKET;
  }
  return ESPBLE_HCI_ROUTER_INVALID_PACKET;
}

espble_hci_route_t espble_hci_router_route_incoming(
  espble_hci_router_t *router, const uint8_t *packet, size_t length)
{
  if (router == NULL || packet == NULL || length == 0) return ESPBLE_HCI_ROUTE_NONE;

  if (packet[0] == H4_ACL)
  {
    if (length < 5 || (size_t)read_le16(&packet[3]) + 5u > length)
      return ESPBLE_HCI_ROUTE_NONE;
    return owner_for_handle(router, read_le16(&packet[1]));
  }
  if (packet[0] == H4_SCO)
    return length >= 4 && (size_t)packet[3] + 4u <= length
      ? ESPBLE_HCI_ROUTE_CLASSIC : ESPBLE_HCI_ROUTE_NONE;
  if (packet[0] == H4_ISO) return ESPBLE_HCI_ROUTE_NIMBLE;
  if (!valid_event(packet, length)) return ESPBLE_HCI_ROUTE_NONE;

  const uint8_t event = packet[1];
  if (event == EVT_COMMAND_COMPLETE)
  {
    return packet[2] >= 3 ? take_command_owner(router, read_le16(&packet[4])) :
      ESPBLE_HCI_ROUTE_NONE;
  }
  if (event == EVT_COMMAND_STATUS)
  {
    return packet[2] >= 4 ? take_command_owner(router, read_le16(&packet[5])) :
      ESPBLE_HCI_ROUTE_NONE;
  }
  if (event == EVT_CONNECTION_COMPLETE)
  {
    if (packet[2] < 3) return ESPBLE_HCI_ROUTE_NONE;
    if (packet[3] == 0 && !set_handle_owner(router, read_le16(&packet[4]),
      ESPBLE_HCI_ROUTE_CLASSIC)) return ESPBLE_HCI_ROUTE_NONE;
    return ESPBLE_HCI_ROUTE_CLASSIC;
  }
  if (event == EVT_LE_META)
  {
    if (packet[2] < 1) return ESPBLE_HCI_ROUTE_NONE;
    const uint8_t subevent = packet[3];
    if ((subevent == LE_CONNECTION_COMPLETE ||
         subevent == LE_ENHANCED_CONNECTION_COMPLETE) && packet[2] >= 4 &&
        packet[4] == 0 && !set_handle_owner(router, read_le16(&packet[5]),
          ESPBLE_HCI_ROUTE_NIMBLE)) return ESPBLE_HCI_ROUTE_NONE;
    return ESPBLE_HCI_ROUTE_NIMBLE;
  }
  if (event == EVT_SYNCHRONOUS_CONNECTION_COMPLETE)
  {
    if (packet[2] < 3) return ESPBLE_HCI_ROUTE_NONE;
    if (packet[3] == 0 && !set_handle_owner(router, read_le16(&packet[4]),
      ESPBLE_HCI_ROUTE_CLASSIC)) return ESPBLE_HCI_ROUTE_NONE;
    return ESPBLE_HCI_ROUTE_CLASSIC;
  }
  if (event == EVT_DISCONNECTION_COMPLETE)
  {
    if (packet[2] < 4) return ESPBLE_HCI_ROUTE_NONE;
    const uint16_t handle = read_le16(&packet[4]);
    espble_hci_route_t owner = owner_for_handle(router, handle);
    remove_handle(router, handle);
    return owner;
  }
  if (event == EVT_NUMBER_COMPLETED_PACKETS)
  {
    if (packet[2] < 1 || packet[3] > (uint8_t)((packet[2] - 1u) / 4u))
      return ESPBLE_HCI_ROUTE_NONE;
    espble_hci_route_t route = ESPBLE_HCI_ROUTE_NONE;
    for (uint8_t i = 0; i < packet[3]; ++i)
      route = (espble_hci_route_t)(route |
        owner_for_handle(router, read_le16(&packet[4u + 4u * i])));
    return route;
  }

  // Events whose first parameter is a status followed by a connection handle.
  if (event == EVT_AUTHENTICATION_COMPLETE || event == EVT_ENCRYPTION_CHANGE ||
      event == EVT_READ_REMOTE_FEATURES_COMPLETE ||
      event == EVT_READ_REMOTE_VERSION_COMPLETE ||
      event == EVT_MODE_CHANGE || event == EVT_ENCRYPTION_KEY_REFRESH_COMPLETE)
  {
    return packet[2] >= 3 ? owner_for_handle(router, read_le16(&packet[4])) :
      ESPBLE_HCI_ROUTE_NONE;
  }
  if (event == EVT_HARDWARE_ERROR || event == EVT_DATA_BUFFER_OVERFLOW)
    return ESPBLE_HCI_ROUTE_BOTH;

  // With the custom Bluedroid build compiled Classic-only, all non-LE
  // asynchronous events belong to it.  Broadcasting them would let NimBLE
  // consume events it did not request and corrupt its state.
  return ESPBLE_HCI_ROUTE_CLASSIC;
}

espble_hci_router_result_t espble_hci_router_packet_for_host(
  const espble_hci_router_t *router, espble_hci_route_t host,
  const uint8_t *packet, size_t length,
  uint8_t *output, size_t capacity, size_t *output_length)
{
  if (router == NULL || !one_host(host) || packet == NULL || output == NULL ||
      output_length == NULL)
    return ESPBLE_HCI_ROUTER_INVALID_PACKET;

  if (valid_event(packet, length) && packet[1] == EVT_NUMBER_COMPLETED_PACKETS)
  {
    if (packet[2] < 1 || packet[3] > (uint8_t)((packet[2] - 1u) / 4u))
      return ESPBLE_HCI_ROUTER_INVALID_PACKET;
    if (capacity < 4) return ESPBLE_HCI_ROUTER_BUFFER_TOO_SMALL;
    output[0] = H4_EVENT;
    output[1] = EVT_NUMBER_COMPLETED_PACKETS;
    output[3] = 0;
    size_t used = 4;
    for (uint8_t i = 0; i < packet[3]; ++i)
    {
      const uint8_t *record = &packet[4u + 4u * i];
      if (owner_for_handle(router, read_le16(record)) != host) continue;
      if (used + 4u > capacity) return ESPBLE_HCI_ROUTER_BUFFER_TOO_SMALL;
      memcpy(&output[used], record, 4);
      used += 4;
      ++output[3];
    }
    if (output[3] == 0) return ESPBLE_HCI_ROUTER_NOT_FOR_HOST;
    output[2] = (uint8_t)(used - 3u);
    *output_length = used;
    return ESPBLE_HCI_ROUTER_OK;
  }

  if (capacity < length) return ESPBLE_HCI_ROUTER_BUFFER_TOO_SMALL;
  memcpy(output, packet, length);
  *output_length = length;
  return ESPBLE_HCI_ROUTER_OK;
}
