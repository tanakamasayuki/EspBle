#include "EspBleHciAclCredits.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
int failures;
void check(const char *name, bool value) {
  if (!value) { std::printf("FAIL %s\n", name); ++failures; }
}

// Command Complete for Read Buffer Size: ACL 1021 bytes / 24 packets,
// synchronous 255 bytes / 8 packets.
const uint8_t ReadBufferSizeComplete[] = {
  0x04, 0x0e, 0x0b, 0x01, 0x05, 0x10, 0x00,
  0xfd, 0x03, 0xff, 0x18, 0x00, 0x08, 0x00};
}

int main()
{
  espble_hci_acl_credits_t credits;
  espble_hci_acl_credits_init(&credits, 4);
  uint8_t command[ESPBLE_HCI_ACL_CREDITS_MAX_COMMAND_LENGTH];

  check("configuration waits for the controller geometry",
    !espble_hci_acl_credits_ready(&credits) &&
    espble_hci_acl_credits_build_host_buffer_size(
      &credits, command, sizeof(command)) == 0 &&
    espble_hci_acl_credits_build_flow_control_enable(
      &credits, command, sizeof(command)) == 0);

  const uint8_t otherResponse[] = {
    0x04, 0x0e, 0x04, 0x01, 0x03, 0x0c, 0x00};
  check("unrelated response ignored", !espble_hci_acl_credits_observe_event(
    &credits, otherResponse, sizeof(otherResponse)));
  check("read buffer size accepted", espble_hci_acl_credits_observe_event(
    &credits, ReadBufferSizeComplete, sizeof(ReadBufferSizeComplete)));
  check("geometry recorded", credits.acl_data_length == 1021 &&
    credits.acl_packet_count == 24 && credits.synchronous_packet_count == 8);

  const size_t buffer_size_length = espble_hci_acl_credits_build_host_buffer_size(
    &credits, command, sizeof(command));
  const uint8_t expectedBufferSize[] = {
    0x01, 0x33, 0x0c, 0x07, 0xfd, 0x03, 0xff, 0x18, 0x00, 0x08, 0x00};
  check("host buffer size mirrors the controller",
    buffer_size_length == sizeof(expectedBufferSize) &&
    std::memcmp(command, expectedBufferSize, sizeof(expectedBufferSize)) == 0);

  const size_t enable_length = espble_hci_acl_credits_build_flow_control_enable(
    &credits, command, sizeof(command));
  const uint8_t expectedEnable[] = {0x01, 0x31, 0x0c, 0x01, 0x01};
  check("flow control enabled for ACL only",
    enable_length == sizeof(expectedEnable) &&
    std::memcmp(command, expectedEnable, sizeof(expectedEnable)) == 0);

  check("undersized buffers are refused",
    espble_hci_acl_credits_build_host_buffer_size(&credits, command, 10) == 0 &&
    espble_hci_acl_credits_build_flow_control_enable(&credits, command, 4) == 0);

  // Below the threshold nothing is sent, so the controller keeps the buffers.
  espble_hci_acl_credits_on_delivered(&credits, 0x0040);
  espble_hci_acl_credits_on_delivered(&credits, 0x0040);
  check("threshold not reached", espble_hci_acl_credits_build_credits(
    &credits, false, command, sizeof(command)) == 0);
  check("forced flush ignores the threshold",
    espble_hci_acl_credits_build_credits(&credits, true, command,
      sizeof(command)) == 9);
  const uint8_t expectedSingle[] = {
    0x01, 0x35, 0x0c, 0x05, 0x01, 0x40, 0x00, 0x02, 0x00};
  check("credits carry the handle and its count",
    std::memcmp(command, expectedSingle, sizeof(expectedSingle)) == 0);
  check("flushed credits are cleared", credits.pending_total == 0 &&
    espble_hci_acl_credits_build_credits(&credits, true, command,
      sizeof(command)) == 0);

  // Two connections reach the threshold together and share one command.
  espble_hci_acl_credits_on_delivered(&credits, 0x0040);
  espble_hci_acl_credits_on_delivered(&credits, 0x0041);
  espble_hci_acl_credits_on_delivered(&credits, 0x0041);
  check("still below threshold", espble_hci_acl_credits_build_credits(
    &credits, false, command, sizeof(command)) == 0);
  espble_hci_acl_credits_on_delivered(&credits, 0x0041);
  const size_t both_length = espble_hci_acl_credits_build_credits(
    &credits, false, command, sizeof(command));
  const uint8_t expectedBoth[] = {
    0x01, 0x35, 0x0c, 0x09, 0x02, 0x40, 0x00, 0x01, 0x00, 0x41, 0x00, 0x03, 0x00};
  check("one command credits every connection",
    both_length == sizeof(expectedBoth) &&
    std::memcmp(command, expectedBoth, sizeof(expectedBoth)) == 0);
  check("nothing left pending", credits.pending_total == 0);

  // The PB/BC flags ride in the upper handle bits and must not create a
  // second record for the same connection.
  espble_hci_acl_credits_on_delivered(&credits, 0x2040);
  espble_hci_acl_credits_on_delivered(&credits, 0x1040);
  check("flag bits stripped from the handle",
    espble_hci_acl_credits_build_credits(&credits, true, command,
      sizeof(command)) == 9 && command[4] == 1 && command[5] == 0x40 &&
    command[7] == 0x02);

  // A disconnected handle must not be credited: the controller already
  // released those buffers.
  espble_hci_acl_credits_on_delivered(&credits, 0x0040);
  espble_hci_acl_credits_on_delivered(&credits, 0x0041);
  espble_hci_acl_credits_forget_handle(&credits, 0x0040);
  check("forgotten handle drops its credits", credits.pending_total == 1);
  const size_t after_disconnect = espble_hci_acl_credits_build_credits(
    &credits, true, command, sizeof(command));
  check("only the live handle is credited", after_disconnect == 9 &&
    command[5] == 0x41 && command[7] == 0x01);
  espble_hci_acl_credits_forget_handle(&credits, 0x0777);

  // A buffer that fits only some handles must still be valid, with the rest
  // kept for the next command.
  for (uint16_t handle = 1; handle <= 4; ++handle)
    espble_hci_acl_credits_on_delivered(&credits, handle);
  const size_t partial = espble_hci_acl_credits_build_credits(
    &credits, true, command, 13);
  check("partial flush stays a valid command", partial == 13 &&
    command[3] == 0x09 && command[4] == 2);
  check("remaining credits survive", credits.pending_total == 2);
  check("second flush drains the rest", espble_hci_acl_credits_build_credits(
    &credits, true, command, sizeof(command)) == 13 &&
    credits.pending_total == 0);

  // More live connections than records: the excess is reported, not swapped in
  // over a tracked connection.
  espble_hci_acl_credits_init(&credits, 1);
  for (uint16_t handle = 1;
       handle <= ESPBLE_HCI_ACL_CREDITS_MAX_CONNECTIONS; ++handle)
    espble_hci_acl_credits_on_delivered(&credits, handle);
  espble_hci_acl_credits_on_delivered(&credits, 0x0ff0);
  check("overflowing handle counted as dropped", credits.dropped_credits == 1 &&
    credits.pending_total == ESPBLE_HCI_ACL_CREDITS_MAX_CONNECTIONS);
  check("every tracked handle is credited",
    espble_hci_acl_credits_build_credits(&credits, true, command,
      sizeof(command)) ==
        5u + 4u * ESPBLE_HCI_ACL_CREDITS_MAX_CONNECTIONS &&
    command[4] == ESPBLE_HCI_ACL_CREDITS_MAX_CONNECTIONS);

  // A host that stops consuming credits must not wrap the counter around and
  // hand the controller back more buffers than it delivered.
  espble_hci_acl_credits_init(&credits, 0xffff);
  for (uint32_t i = 0; i < 0xffff; ++i)
    espble_hci_acl_credits_on_delivered(&credits, 0x0040);
  check("counter saturates instead of wrapping", credits.pending_total == 0xffff &&
    credits.dropped_credits == 0);
  espble_hci_acl_credits_on_delivered(&credits, 0x0040);
  check("packets beyond the counter are reported as dropped",
    credits.pending_total == 0xffff && credits.dropped_credits == 1);

  // Null arguments and malformed events reach this module from the broker.
  espble_hci_acl_credits_init(nullptr, 4);
  espble_hci_acl_credits_on_delivered(nullptr, 0);
  espble_hci_acl_credits_forget_handle(nullptr, 0);
  check("null event rejected", !espble_hci_acl_credits_observe_event(
    &credits, nullptr, 14));
  check("truncated response rejected", !espble_hci_acl_credits_observe_event(
    &credits, ReadBufferSizeComplete, sizeof(ReadBufferSizeComplete) - 1));
  check("null output rejected", espble_hci_acl_credits_build_credits(
    &credits, true, nullptr, 32) == 0);

  const uint8_t failedRead[] = {
    0x04, 0x0e, 0x0b, 0x01, 0x05, 0x10, 0x12,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  espble_hci_acl_credits_init(&credits, 4);
  check("failed read buffer size leaves the loop unconfigured",
    !espble_hci_acl_credits_observe_event(&credits, failedRead,
      sizeof(failedRead)) && !espble_hci_acl_credits_ready(&credits));

  if (failures) return 1;
  std::puts("OK");
  return 0;
}
