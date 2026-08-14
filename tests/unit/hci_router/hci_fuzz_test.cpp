// Randomized fault injection for the three pure-C HCI modules.
//
// The broker feeds these modules whatever the controller and both logical
// hosts emit, so they must survive truncated, oversized, and self-inconsistent
// H4 packets without corrupting their own state. Every packet and every output
// buffer is a heap allocation of exactly the advertised size, so the sanitizers
// this file is built with report a single byte of over-read or over-write.
//
// The generator mixes fully random bytes with mutated well-formed packets:
// random bytes alone almost never pass the length checks that guard the
// interesting code paths.

#include "EspBleHciCommandScheduler.h"
#include "EspBleHciControllerPolicy.h"
#include "EspBleHciRouter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int failures;

void check(const char *name, bool value)
{
  if (!value && failures < 20) {
    std::printf("FAIL %s\n", name);
    ++failures;
  }
}

class Random {
public:
  explicit Random(uint64_t seed) : state_(seed ? seed : 0x9e3779b97f4a7c15ull) {}

  uint64_t next()
  {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 7;
    state_ ^= state_ << 17;
    return state_;
  }

  uint32_t below(uint32_t bound) { return bound ? (uint32_t)(next() % bound) : 0; }
  uint8_t byte() { return (uint8_t)next(); }
  bool chance(uint32_t percent) { return below(100) < percent; }

private:
  uint64_t state_;
};

// Owners the broker can pass in, including values the API must reject.
uint8_t random_owner(Random &random)
{
  static const uint8_t owners[] = {
    ESPBLE_HCI_ROUTE_NONE, ESPBLE_HCI_ROUTE_NIMBLE, ESPBLE_HCI_ROUTE_CLASSIC,
    ESPBLE_HCI_ROUTE_BOTH, 0x7f, 0xff,
  };
  return owners[random.below((uint32_t)(sizeof(owners) / sizeof(owners[0])))];
}

// Random 16-bit fields almost never collide, so the tables under test would
// stay empty. Draw handles and opcodes from small pools instead, so ownership
// records are created, matched, replaced, and removed during the run.
uint16_t favored_handle(Random &random)
{
  // More distinct handles than the router's connection table has slots, so the
  // table fills up and the exhaustion path runs.
  static const uint16_t handles[] = {
    0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008,
    0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x0040, 0x0041,
    0x0042, 0x0100, 0x0800, 0x0fff,
  };
  if (random.chance(20)) return (uint16_t)random.next();
  return handles[random.below((uint32_t)(sizeof(handles) / sizeof(handles[0])))];
}

uint16_t favored_opcode(Random &random)
{
  static const uint16_t opcodes[] = {
    0x0c03, 0x0c01, 0x0c31, 0x0c33, 0x0c35, 0x0c63, 0x0406, 0x0409,
    0x040b, 0x040f, 0x0411, 0x0413, 0x0419, 0x041d, 0x0803, 0x0804,
    0x1001, 0x1005, 0x2001, 0x2006, 0x200d, 0x2013, 0x0c14, 0x0c1a,
  };
  if (random.chance(20)) return (uint16_t)random.next();
  return opcodes[random.below((uint32_t)(sizeof(opcodes) / sizeof(opcodes[0])))];
}

void push_le16(std::vector<uint8_t> &packet, uint16_t value)
{
  packet.push_back((uint8_t)(value & 0xff));
  packet.push_back((uint8_t)(value >> 8));
}

std::vector<uint8_t> random_bytes(Random &random, size_t length)
{
  std::vector<uint8_t> packet(length);
  for (size_t i = 0; i < length; ++i) packet[i] = random.byte();
  return packet;
}

// Well-formed enough to clear the length checks, then optionally corrupted.
std::vector<uint8_t> structured_packet(Random &random)
{
  std::vector<uint8_t> packet;
  switch (random.below(8)) {
  case 0: {  // HCI command
    const uint8_t parameters = (uint8_t)random.below(8);
    packet.push_back(0x01);
    push_le16(packet, favored_opcode(random));
    packet.push_back(parameters);
    // Byte 0 of a connection-scoped command is the handle the policy checks.
    for (uint8_t i = 0; i < parameters; ++i) packet.push_back(random.byte());
    if (parameters >= 2) {
      const uint16_t handle = favored_handle(random);
      packet[4] = (uint8_t)(handle & 0xff);
      packet[5] = (uint8_t)(handle >> 8);
    }
    break;
  }
  case 1: {  // ACL data
    const uint8_t payload = (uint8_t)random.below(8);
    packet.push_back(0x02);
    push_le16(packet, favored_handle(random));
    packet.push_back(payload);
    packet.push_back(0);
    for (uint8_t i = 0; i < payload; ++i) packet.push_back(random.byte());
    break;
  }
  case 2: {  // Number Of Completed Packets, the only rebuilt event
    const uint8_t handles = (uint8_t)random.below(6);
    packet.push_back(0x04);
    packet.push_back(0x13);
    packet.push_back((uint8_t)(1 + 4 * handles));
    packet.push_back(handles);
    for (uint8_t i = 0; i < handles; ++i) {
      push_le16(packet, favored_handle(random));
      push_le16(packet, (uint16_t)random.below(4));
    }
    break;
  }
  case 3: {  // Connection-establishing and connection-scoped events
    static const uint8_t events[] = {0x03, 0x05, 0x08, 0x0e, 0x0f, 0x2c, 0x3e};
    const uint8_t parameters = (uint8_t)random.below(12);
    packet.push_back(0x04);
    packet.push_back(events[random.below(7)]);
    packet.push_back(parameters);
    for (uint8_t i = 0; i < parameters; ++i)
      packet.push_back(random.chance(40) ? 0 : random.byte());
    break;
  }
  case 4: {  // BR/EDR connection complete carrying a live handle
    packet.push_back(0x04);
    packet.push_back(random.chance(50) ? 0x03 : 0x2c);
    packet.push_back(11);
    packet.push_back(random.chance(80) ? 0 : random.byte());
    push_le16(packet, favored_handle(random));
    for (int i = 0; i < 8; ++i) packet.push_back(random.byte());
    break;
  }
  case 5: {  // LE connection complete, disconnection, or a command response
    switch (random.below(3)) {
    case 0:
      packet.push_back(0x04);
      packet.push_back(0x3e);
      packet.push_back(19);
      packet.push_back(random.chance(60) ? 0x01 : 0x0a);
      packet.push_back(random.chance(80) ? 0 : random.byte());
      push_le16(packet, favored_handle(random));
      for (int i = 0; i < 15; ++i) packet.push_back(random.byte());
      break;
    case 1:
      packet.push_back(0x04);
      packet.push_back(0x05);
      packet.push_back(4);
      packet.push_back(random.chance(80) ? 0 : random.byte());
      push_le16(packet, favored_handle(random));
      packet.push_back(random.byte());
      break;
    default: {
      const bool complete = random.chance(50);
      packet.push_back(0x04);
      packet.push_back(complete ? 0x0e : 0x0f);
      packet.push_back(complete ? 4 : 4);
      if (!complete) packet.push_back(random.byte());
      packet.push_back((uint8_t)random.below(3));
      push_le16(packet, favored_opcode(random));
      if (complete) packet.push_back(random.byte());
      break;
    }
    }
    break;
  }
  case 6: {  // Synchronous data and the events both hosts must receive
    if (random.chance(50)) {
      const uint8_t payload = (uint8_t)random.below(8);
      packet.push_back(0x03);
      push_le16(packet, favored_handle(random));
      packet.push_back(payload);
      for (uint8_t i = 0; i < payload; ++i) packet.push_back(random.byte());
    } else {
      packet.push_back(0x04);
      packet.push_back(random.chance(50) ? 0x10 : 0x1a);
      packet.push_back(1);
      packet.push_back(random.byte());
    }
    break;
  }
  default: {  // Set Event Mask family, the commands the policy rewrites
    static const uint16_t opcodes[] = {0x0c01, 0x0c63, 0x2001};
    const uint16_t opcode = opcodes[random.below(3)];
    packet.push_back(0x01);
    packet.push_back((uint8_t)(opcode & 0xff));
    packet.push_back((uint8_t)(opcode >> 8));
    packet.push_back(8);
    for (int i = 0; i < 8; ++i) packet.push_back(random.byte());
    break;
  }
  }

  const uint32_t mutations = random.below(4);
  for (uint32_t i = 0; i < mutations && !packet.empty(); ++i) {
    switch (random.below(3)) {
    case 0: packet[random.below((uint32_t)packet.size())] = random.byte(); break;
    case 1: packet.resize(random.below((uint32_t)packet.size() + 1)); break;
    default: packet.push_back(random.byte()); break;
    }
  }
  return packet;
}

std::vector<uint8_t> next_packet(Random &random)
{
  if (random.chance(35)) return random_bytes(random, random.below(24));
  return structured_packet(random);
}

void check_router_state(const espble_hci_router_t &router)
{
  check("router pending within capacity",
    router.pending_count <= ESPBLE_HCI_ROUTER_MAX_PENDING_COMMANDS);
  for (size_t i = 0; i < router.pending_count; ++i) {
    check("router pending owner is one host",
      router.pending[i].owner == ESPBLE_HCI_ROUTE_NIMBLE ||
      router.pending[i].owner == ESPBLE_HCI_ROUTE_CLASSIC);
  }
  for (size_t i = 0; i < ESPBLE_HCI_ROUTER_MAX_CONNECTIONS; ++i) {
    if (!router.connections[i].used) continue;
    check("router handle masked", router.connections[i].handle <= 0x0fff);
    check("router connection owner is one host",
      router.connections[i].owner == ESPBLE_HCI_ROUTE_NIMBLE ||
      router.connections[i].owner == ESPBLE_HCI_ROUTE_CLASSIC);
  }
}

void check_scheduler_state(const espble_hci_command_scheduler_t &scheduler)
{
  check("scheduler count within capacity",
    scheduler.count <= ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY);
  check("scheduler head within capacity",
    scheduler.head < ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY);
  for (size_t offset = 0; offset < scheduler.count; ++offset) {
    const size_t index = (scheduler.head + offset) %
      ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY;
    const espble_hci_command_scheduler_entry_t &entry = scheduler.entries[index];
    check("scheduler entry length within capacity",
      entry.length >= 4 && entry.length <= ESPBLE_HCI_COMMAND_MAX_LENGTH);
    check("scheduler entry stays a command", entry.packet[0] == 0x01);
    check("scheduler entry length matches its header",
      entry.length == (uint16_t)entry.packet[3] + 4u);
  }
  check("scheduler tracks no opcode while idle",
    scheduler.awaiting_response || scheduler.in_flight_opcode == 0);
}

// Drives the routing side: outgoing ownership, incoming routing, and the
// per-host copy whose Number Of Completed Packets rebuild is the only place
// the router writes into a caller buffer.
void exercise_router(Random &random, espble_hci_router_t &router)
{
  const std::vector<uint8_t> packet = next_packet(random);
  const uint8_t *data = packet.empty() ? nullptr : packet.data();

  // Answering a command the router is actually waiting for reaches the
  // ownership hand-back, including responses that arrive out of order.
  if (!packet.empty() && router.pending_count > 0 && random.chance(30)) {
    const uint16_t opcode =
      router.pending[random.below(router.pending_count)].opcode;
    const uint8_t response[] = {
      0x04, random.chance(50) ? (uint8_t)0x0e : (uint8_t)0x0f, 0x04,
      (uint8_t)random.below(3), (uint8_t)(opcode & 0xff),
      (uint8_t)(opcode >> 8), 0x00};
    const espble_hci_route_t route =
      espble_hci_router_route_incoming(&router, response, sizeof(response));
    check("answered command returns to one host",
      route == ESPBLE_HCI_ROUTE_NONE || route == ESPBLE_HCI_ROUTE_NIMBLE ||
      route == ESPBLE_HCI_ROUTE_CLASSIC);
    check_router_state(router);
    return;
  }

  switch (random.below(4)) {
  case 0: {
    const espble_hci_router_result_t result = espble_hci_router_track_outgoing(
      &router, (espble_hci_route_t)random_owner(random), data, packet.size());
    check("track_outgoing result in range",
      result >= ESPBLE_HCI_ROUTER_OK &&
      result <= ESPBLE_HCI_ROUTER_BUFFER_TOO_SMALL);
    break;
  }
  case 1: {
    const espble_hci_route_t route =
      espble_hci_router_route_incoming(&router, data, packet.size());
    check("route_incoming stays within the known routes",
      route == ESPBLE_HCI_ROUTE_NONE || route == ESPBLE_HCI_ROUTE_NIMBLE ||
      route == ESPBLE_HCI_ROUTE_CLASSIC || route == ESPBLE_HCI_ROUTE_BOTH);
    break;
  }
  case 2: {
    (void)espble_hci_router_owns_handle(
      &router, (espble_hci_route_t)random_owner(random),
      (uint16_t)random.next());
    break;
  }
  default: {
    // Deliberately undersized capacities must be reported, never written past.
    const size_t capacity = random.chance(30)
      ? random.below((uint32_t)packet.size() + 1)
      : packet.size() + random.below(8);
    std::vector<uint8_t> output(capacity);
    size_t output_length = 0xdead;
    const espble_hci_route_t host = random.chance(50)
      ? ESPBLE_HCI_ROUTE_NIMBLE : ESPBLE_HCI_ROUTE_CLASSIC;
    const espble_hci_router_result_t result = espble_hci_router_packet_for_host(
      &router, host, data, packet.size(),
      capacity ? output.data() : nullptr, capacity, &output_length);
    if (result == ESPBLE_HCI_ROUTER_OK) {
      check("copied packet fits the buffer", output_length <= capacity);
      check("copied packet is not empty", output_length > 0);
      // A malformed Number Of Completed Packets event fails valid_event() and
      // is copied through untouched, so only inspect what the rebuild path
      // produced: a header whose record count fills the reported length.
      const bool rebuilt = output_length >= 4 && output[0] == 0x04 &&
        output[1] == 0x13 && (size_t)output[2] + 3u == output_length &&
        output_length == 4u + 4u * (size_t)output[3];
      if (rebuilt) {
        for (uint8_t i = 0; i < output[3]; ++i) {
          const uint16_t handle = (uint16_t)output[4 + 4 * i] |
            ((uint16_t)output[5 + 4 * i] << 8);
          check("rebuilt handle belongs to the target host",
            espble_hci_router_owns_handle(&router, host, handle));
        }
      }
    }
    break;
  }
  }
  check_router_state(router);
}

void exercise_scheduler(
  Random &random, espble_hci_command_scheduler_t &scheduler)
{
  const std::vector<uint8_t> packet = next_packet(random);
  const uint8_t *data = packet.empty() ? nullptr : packet.data();

  // Complete the in-flight transaction the way a controller would, so the
  // credit accounting and opcode match run alongside the malformed input.
  if (scheduler.awaiting_response && random.chance(40)) {
    const uint16_t opcode = random.chance(80) ? scheduler.in_flight_opcode
                                              : (uint16_t)random.next();
    const uint8_t response[] = {
      0x04, 0x0e, 0x04, (uint8_t)random.below(4), (uint8_t)(opcode & 0xff),
      (uint8_t)(opcode >> 8), 0x00};
    const espble_hci_command_scheduler_result_t result =
      espble_hci_command_scheduler_on_event(&scheduler, response, sizeof(response));
    check("response either completes or reports a mismatch",
      result == ESPBLE_HCI_COMMAND_SCHEDULER_OK ||
      result == ESPBLE_HCI_COMMAND_SCHEDULER_RESPONSE_MISMATCH);
    check_scheduler_state(scheduler);
    return;
  }

  switch (random.below(5)) {
  case 0:
    (void)espble_hci_command_scheduler_enqueue(
      &scheduler, (uint8_t)random.below(4), data, packet.size());
    break;
  case 1: {
    uint8_t owner = 0xff;
    const uint8_t *queued = nullptr;
    size_t length = 0;
    if (espble_hci_command_scheduler_peek(&scheduler, &owner, &queued, &length) ==
        ESPBLE_HCI_COMMAND_SCHEDULER_OK) {
      check("peek returns a stored packet", queued != nullptr);
      check("peek length within capacity",
        length >= 4 && length <= ESPBLE_HCI_COMMAND_MAX_LENGTH);
      check("peek length matches the stored header",
        length == (size_t)queued[3] + 4u);
    }
    break;
  }
  case 2:
    (void)espble_hci_command_scheduler_mark_sent(&scheduler);
    break;
  case 3:
    (void)espble_hci_command_scheduler_on_event(&scheduler, data, packet.size());
    break;
  default:
    (void)espble_hci_command_scheduler_remove_owner(
      &scheduler, (uint8_t)random.below(4));
    break;
  }
  check_scheduler_state(scheduler);
}

void exercise_policy(Random &random, espble_hci_controller_policy_t &policy)
{
  const std::vector<uint8_t> packet = next_packet(random);
  const uint8_t *data = packet.empty() ? nullptr : packet.data();
  const uint8_t host = (uint8_t)random.below(4);

  switch (random.below(5)) {
  case 0:
    (void)espble_hci_controller_policy_is_reset(data, packet.size());
    break;
  case 1: {
    const espble_hci_controller_policy_virtual_action_t action =
      espble_hci_controller_policy_virtual_action(data, packet.size());
    check("virtual action in range",
      action >= ESPBLE_HCI_CONTROLLER_POLICY_PHYSICAL &&
      action <= ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_INVALID_PACKET);
    break;
  }
  case 2: {
    const uint16_t opcode = (uint16_t)random.next();
    const espble_hci_command_scope_t scope =
      espble_hci_controller_policy_classify_opcode(opcode);
    check("scope in range",
      scope >= ESPBLE_HCI_COMMAND_SCOPE_UNKNOWN &&
      scope <= ESPBLE_HCI_COMMAND_SCOPE_HOST_CREDIT);
    // A command nobody classified must never be silently allowed through.
    check("handle-addressed commands are classified",
      !espble_hci_controller_policy_targets_handle(opcode) ||
      scope != ESPBLE_HCI_COMMAND_SCOPE_UNKNOWN);
    break;
  }
  case 3: {
    const espble_hci_command_authorization_t authorization =
      espble_hci_controller_policy_authorize(host, data, packet.size());
    check("authorization in range",
      authorization >= ESPBLE_HCI_COMMAND_AUTHORIZED &&
      authorization <= ESPBLE_HCI_COMMAND_WRONG_HOST);
    break;
  }
  default: {
    const size_t capacity = random.chance(30)
      ? random.below((uint32_t)packet.size() + 1)
      : packet.size() + random.below(4);
    std::vector<uint8_t> output(capacity);
    const espble_hci_controller_policy_result_t result =
      espble_hci_controller_policy_rewrite_command(
        &policy, host, data, packet.size(),
        capacity ? output.data() : nullptr, capacity);
    if (result == ESPBLE_HCI_CONTROLLER_POLICY_REWRITTEN) {
      check("rewrite keeps the command header",
        capacity >= 12 && output[0] == 0x01 && output[3] == 8);
      check("rewrite preserves the opcode",
        output[1] == packet[1] && output[2] == packet[2]);
      // The written mask is the union, so it can only add bits.
      for (size_t i = 0; i < 8; ++i)
        check("rewritten mask keeps the requesting host's bits",
          (output[4 + i] & packet[4 + i]) == packet[4 + i]);
    }
    break;
  }
  }

  if (random.chance(5))
    espble_hci_controller_policy_remove_host(&policy, host);
}

// Saturation and null-argument paths a random walk reaches only by luck. They
// are the ones that matter under fault injection: every table in the broker is
// fixed size, and a caller that loses a buffer must be rejected, not crash.
void exercise_limits()
{
  espble_hci_router_t router;
  espble_hci_router_init(&router);

  // One command per pending slot, then one too many.
  for (int i = 0; i < ESPBLE_HCI_ROUTER_MAX_PENDING_COMMANDS; ++i) {
    const uint8_t command[] = {0x01, (uint8_t)i, 0x0c, 0x00};
    check("pending slot accepted", espble_hci_router_track_outgoing(
      &router, ESPBLE_HCI_ROUTE_CLASSIC, command, sizeof(command)) ==
        ESPBLE_HCI_ROUTER_OK);
  }
  const uint8_t overflow_command[] = {0x01, 0xfe, 0x0c, 0x00};
  check("pending overflow rejected", espble_hci_router_track_outgoing(
    &router, ESPBLE_HCI_ROUTE_CLASSIC, overflow_command,
    sizeof(overflow_command)) == ESPBLE_HCI_ROUTER_QUEUE_FULL);

  // Host Number Of Completed Packets never occupies a slot, so it still passes.
  const uint8_t credits[] = {0x01, 0x35, 0x0c, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00};
  check("credit command bypasses the pending table",
    espble_hci_router_track_outgoing(&router, ESPBLE_HCI_ROUTE_NIMBLE, credits,
      sizeof(credits)) == ESPBLE_HCI_ROUTER_OK);

  espble_hci_router_init(&router);
  for (int i = 0; i < ESPBLE_HCI_ROUTER_MAX_CONNECTIONS; ++i) {
    const uint8_t connected[] = {
      0x04, 0x03, 0x0b, 0x00, (uint8_t)(i + 1), 0x00,
      0, 0, 0, 0, 0, 0, 0, 0};
    check("connection slot accepted", espble_hci_router_route_incoming(
      &router, connected, sizeof(connected)) == ESPBLE_HCI_ROUTE_CLASSIC);
  }
  const uint8_t extra_connection[] = {
    0x04, 0x03, 0x0b, 0x00, 0x7f, 0x00, 0, 0, 0, 0, 0, 0, 0, 0};
  check("connection overflow is not routed", espble_hci_router_route_incoming(
    &router, extra_connection, sizeof(extra_connection)) ==
      ESPBLE_HCI_ROUTE_NONE);
  check("connection overflow left no ownership",
    !espble_hci_router_owns_handle(&router, ESPBLE_HCI_ROUTE_CLASSIC, 0x007f));

  // The same exhaustion must stop LE and synchronous links from being tracked.
  const uint8_t extra_le_connection[] = {
    0x04, 0x3e, 0x13, 0x01, 0x00, 0x7e, 0x00,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  check("LE connection overflow is not routed", espble_hci_router_route_incoming(
    &router, extra_le_connection, sizeof(extra_le_connection)) ==
      ESPBLE_HCI_ROUTE_NONE);
  const uint8_t extra_sco_connection[] = {
    0x04, 0x2c, 0x11, 0x00, 0x7d, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  check("synchronous connection overflow is not routed",
    espble_hci_router_route_incoming(&router, extra_sco_connection,
      sizeof(extra_sco_connection)) == ESPBLE_HCI_ROUTE_NONE);

  // SCO belongs to Classic only; the LE host must not be able to send it.
  const uint8_t sco[] = {0x03, 0x40, 0x00, 0x01, 0x00};
  check("Classic may send SCO", espble_hci_router_track_outgoing(
    &router, ESPBLE_HCI_ROUTE_CLASSIC, sco, sizeof(sco)) ==
      ESPBLE_HCI_ROUTER_OK);
  check("NimBLE may not send SCO", espble_hci_router_track_outgoing(
    &router, ESPBLE_HCI_ROUTE_NIMBLE, sco, sizeof(sco)) ==
      ESPBLE_HCI_ROUTER_INVALID_PACKET);

  // Controller-wide failures reach both hosts.
  const uint8_t hardware_error[] = {0x04, 0x10, 0x01, 0x01};
  check("hardware error reaches both hosts", espble_hci_router_route_incoming(
    &router, hardware_error, sizeof(hardware_error)) == ESPBLE_HCI_ROUTE_BOTH);

  // Host Number Of Completed Packets is answered by the controller with
  // nothing at all, so the policy must consume it without expecting a response.
  const uint8_t host_credits[] = {
    0x01, 0x35, 0x0c, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00};
  check("host credit command needs no response",
    espble_hci_controller_policy_virtual_action(
      host_credits, sizeof(host_credits)) ==
        ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_NO_RESPONSE);
  check("LE connection-scoped command is classified",
    espble_hci_controller_policy_classify_opcode(0x2016) ==
      ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_CONNECTION);

  espble_hci_command_scheduler_t scheduler;
  espble_hci_command_scheduler_init(&scheduler);
  for (int i = 0; i < ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY; ++i) {
    const uint8_t command[] = {0x01, (uint8_t)i, 0x0c, 0x00};
    check("scheduler slot accepted", espble_hci_command_scheduler_enqueue(
      &scheduler, 0, command, sizeof(command)) ==
        ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  }
  const uint8_t overflow[] = {0x01, 0xfd, 0x0c, 0x00};
  check("scheduler overflow rejected", espble_hci_command_scheduler_enqueue(
    &scheduler, 0, overflow, sizeof(overflow)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_QUEUE_FULL);

  uint8_t owner = 0;
  const uint8_t *queued = nullptr;
  size_t length = 0;
  check("peek rejects a null scheduler", espble_hci_command_scheduler_peek(
    nullptr, &owner, &queued, &length) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET);
  check("peek rejects null outputs", espble_hci_command_scheduler_peek(
    &scheduler, nullptr, &queued, &length) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET);
  check("mark_sent rejects a null scheduler",
    espble_hci_command_scheduler_mark_sent(nullptr) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET);
  check("on_event rejects a null scheduler",
    espble_hci_command_scheduler_on_event(nullptr, nullptr, 0) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET);
  check("remove_owner rejects a null scheduler",
    espble_hci_command_scheduler_remove_owner(nullptr, 0) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET);
}

}  // namespace

int main(int argc, char **argv)
{
  const uint64_t seed = argc > 1 ? std::strtoull(argv[1], nullptr, 0) : 0x5eed1234ull;
  const uint32_t iterations = argc > 2 ? (uint32_t)std::strtoul(argv[2], nullptr, 0) : 200000;

  exercise_limits();

  Random random(seed);
  espble_hci_router_t router;
  espble_hci_command_scheduler_t scheduler;
  espble_hci_controller_policy_t policy;
  espble_hci_router_init(&router);
  espble_hci_command_scheduler_init(&scheduler);
  espble_hci_controller_policy_init(&policy);

  for (uint32_t iteration = 0; iteration < iterations && failures == 0; ++iteration) {
    // Restarting the modules mid-run covers the re-registration path, where a
    // stale entry from the previous session would surface as a leaked handle.
    if (random.chance(1)) {
      espble_hci_router_init(&router);
      espble_hci_command_scheduler_init(&scheduler);
      espble_hci_controller_policy_init(&policy);
    }
    switch (random.below(3)) {
    case 0: exercise_router(random, router); break;
    case 1: exercise_scheduler(random, scheduler); break;
    default: exercise_policy(random, policy); break;
    }
  }

  // NULL arguments reach these modules whenever a caller loses a buffer.
  espble_hci_router_init(nullptr);
  espble_hci_command_scheduler_init(nullptr);
  espble_hci_controller_policy_init(nullptr);
  check("null router rejected", espble_hci_router_track_outgoing(
    nullptr, ESPBLE_HCI_ROUTE_NIMBLE, nullptr, 0) ==
      ESPBLE_HCI_ROUTER_INVALID_PACKET);
  check("null router incoming rejected", espble_hci_router_route_incoming(
    nullptr, nullptr, 4) == ESPBLE_HCI_ROUTE_NONE);
  check("null scheduler rejected", espble_hci_command_scheduler_enqueue(
    nullptr, 0, nullptr, 0) == ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET);
  check("null policy rejected", espble_hci_controller_policy_rewrite_command(
    nullptr, 0, nullptr, 0, nullptr, 0) ==
      ESPBLE_HCI_CONTROLLER_POLICY_INVALID_PACKET);

  if (failures) return 1;
  std::puts("OK");
  return 0;
}
