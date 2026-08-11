#include "EspBleHciControllerPolicy.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
int failures;
void check(const char *name, bool value) {
  if (!value) { std::printf("FAIL %s\n", name); ++failures; }
}
}

int main()
{
  espble_hci_controller_policy_t policy;
  espble_hci_controller_policy_init(&policy);

  struct ScopeSet {
    espble_hci_command_scope_t scope;
    const uint16_t *opcodes;
    size_t count;
  };
  const uint16_t sharedRead[] = {
    0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1009};
  const uint16_t nimbleRadio[] = {
    0x2002, 0x2003, 0x2005, 0x2006, 0x2008, 0x2009, 0x200a, 0x200b,
    0x200c, 0x200d, 0x2018};
  const uint16_t classicRadio[] = {
    0x0405, 0x0409, 0x040b, 0x040f, 0x0419, 0x041f, 0x080f,
    0x0c13, 0x0c14, 0x0c18, 0x0c1a, 0x0c1e, 0x0c24, 0x0c2f, 0x0c3a,
    0x0c43, 0x0c45, 0x0c47, 0x0c52, 0x0c56, 0x0c5b};
  const uint16_t nimbleConnection[] = {
    0x2016, 0x2019, 0x201a, 0x2022, 0x2030};
  const uint16_t classicConnection[] = {
    0x0411, 0x0413, 0x041b, 0x041c, 0x0803, 0x0804, 0x080d, 0x0c37};
  const uint16_t sharedConnection[] = {0x0406, 0x041d, 0x1405};
  const uint16_t merged[] = {0x0c01, 0x0c63, 0x2001};
  const uint16_t virtualized[] = {0x0c03, 0x0c31, 0x0c33};
  const uint16_t hostCredit[] = {0x0c35};
  const ScopeSet scopeSets[] = {
    {ESPBLE_HCI_COMMAND_SCOPE_SHARED_READ, sharedRead,
      sizeof(sharedRead) / sizeof(sharedRead[0])},
    {ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_RADIO, nimbleRadio,
      sizeof(nimbleRadio) / sizeof(nimbleRadio[0])},
    {ESPBLE_HCI_COMMAND_SCOPE_CLASSIC_RADIO, classicRadio,
      sizeof(classicRadio) / sizeof(classicRadio[0])},
    {ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_CONNECTION, nimbleConnection,
      sizeof(nimbleConnection) / sizeof(nimbleConnection[0])},
    {ESPBLE_HCI_COMMAND_SCOPE_CLASSIC_CONNECTION, classicConnection,
      sizeof(classicConnection) / sizeof(classicConnection[0])},
    {ESPBLE_HCI_COMMAND_SCOPE_SHARED_CONNECTION, sharedConnection,
      sizeof(sharedConnection) / sizeof(sharedConnection[0])},
    {ESPBLE_HCI_COMMAND_SCOPE_CONTROLLER_MERGED, merged,
      sizeof(merged) / sizeof(merged[0])},
    {ESPBLE_HCI_COMMAND_SCOPE_CONTROLLER_VIRTUAL, virtualized,
      sizeof(virtualized) / sizeof(virtualized[0])},
    {ESPBLE_HCI_COMMAND_SCOPE_HOST_CREDIT, hostCredit,
      sizeof(hostCredit) / sizeof(hostCredit[0])},
  };
  for (const ScopeSet &set : scopeSets) {
    for (size_t i = 0; i < set.count; ++i) {
      check("classify observed opcode",
        espble_hci_controller_policy_classify_opcode(set.opcodes[i]) ==
          set.scope);
      const uint8_t command[] = {
        0x01, static_cast<uint8_t>(set.opcodes[i] & 0xff),
        static_cast<uint8_t>(set.opcodes[i] >> 8), 0x00};
      const bool nimbleOnly =
        set.scope == ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_RADIO ||
        set.scope == ESPBLE_HCI_COMMAND_SCOPE_NIMBLE_CONNECTION;
      const bool classicOnly =
        set.scope == ESPBLE_HCI_COMMAND_SCOPE_CLASSIC_RADIO ||
        set.scope == ESPBLE_HCI_COMMAND_SCOPE_CLASSIC_CONNECTION;
      check("authorize observed NimBLE opcode",
        espble_hci_controller_policy_authorize(0, command, sizeof(command)) ==
          (classicOnly ? ESPBLE_HCI_COMMAND_WRONG_HOST :
            ESPBLE_HCI_COMMAND_AUTHORIZED));
      check("authorize observed Classic opcode",
        espble_hci_controller_policy_authorize(1, command, sizeof(command)) ==
          (nimbleOnly ? ESPBLE_HCI_COMMAND_WRONG_HOST :
            ESPBLE_HCI_COMMAND_AUTHORIZED));
    }
  }
  const uint8_t unknown[] = {0x01, 0x00, 0xfc, 0x00};
  check("unknown opcode remains unclassified",
    espble_hci_controller_policy_classify_opcode(0xfc00) ==
      ESPBLE_HCI_COMMAND_SCOPE_UNKNOWN);
  check("unknown opcode rejected in dual-host policy",
    espble_hci_controller_policy_authorize(0, unknown, sizeof(unknown)) ==
      ESPBLE_HCI_COMMAND_UNCLASSIFIED);
  const uint8_t truncated[] = {0x01, 0x09, 0x10, 0x01};
  check("truncated known opcode rejected",
    espble_hci_controller_policy_authorize(0, truncated, sizeof(truncated)) ==
      ESPBLE_HCI_COMMAND_INVALID_PACKET);

  const uint8_t classicMask[] = {
    0x01, 0x01, 0x0c, 0x08, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xbf, 0x1d};
  const uint8_t nimbleMask[] = {
    0x01, 0x01, 0x0c, 0x08, 0x90, 0x88, 0x00, 0x02,
    0x00, 0x80, 0x00, 0x20};
  const uint8_t unionMask[] = {
    0x01, 0x01, 0x0c, 0x08, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xbf, 0x3d};
  uint8_t output[sizeof(classicMask)] = {};

  check("cache classic mask", espble_hci_controller_policy_rewrite_command(
    &policy, 1, classicMask, sizeof(classicMask), output, sizeof(output)) ==
      ESPBLE_HCI_CONTROLLER_POLICY_REWRITTEN);
  check("single host mask unchanged",
    std::memcmp(output, classicMask, sizeof(output)) == 0);
  check("union NimBLE and Classic", espble_hci_controller_policy_rewrite_command(
    &policy, 0, nimbleMask, sizeof(nimbleMask), output, sizeof(output)) ==
      ESPBLE_HCI_CONTROLLER_POLICY_REWRITTEN &&
    std::memcmp(output, unionMask, sizeof(output)) == 0);

  const uint8_t page2Classic[] = {
    0x01, 0x63, 0x0c, 0x08, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};
  const uint8_t page2Nimble[] = {
    0x01, 0x63, 0x0c, 0x08, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00};
  const uint8_t page2Union[] = {
    0x01, 0x63, 0x0c, 0x08, 0x01, 0x00, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00};
  check("cache page 2 classic", espble_hci_controller_policy_rewrite_command(
    &policy, 1, page2Classic, sizeof(page2Classic), output, sizeof(output)) ==
      ESPBLE_HCI_CONTROLLER_POLICY_REWRITTEN);
  check("union page 2 independently", espble_hci_controller_policy_rewrite_command(
    &policy, 0, page2Nimble, sizeof(page2Nimble), output, sizeof(output)) ==
      ESPBLE_HCI_CONTROLLER_POLICY_REWRITTEN &&
    std::memcmp(output, page2Union, sizeof(output)) == 0);

  const uint8_t leMask[] = {
    0x01, 0x01, 0x20, 0x08, 0x7f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};
  check("LE mask uses separate cache", espble_hci_controller_policy_rewrite_command(
    &policy, 0, leMask, sizeof(leMask), output, sizeof(output)) ==
      ESPBLE_HCI_CONTROLLER_POLICY_REWRITTEN &&
    std::memcmp(output, leMask, sizeof(output)) == 0);

  espble_hci_controller_policy_remove_host(&policy, 1);
  check("removed host no longer contributes", 
    espble_hci_controller_policy_rewrite_command(
      &policy, 0, nimbleMask, sizeof(nimbleMask), output, sizeof(output)) ==
        ESPBLE_HCI_CONTROLLER_POLICY_REWRITTEN &&
    std::memcmp(output, nimbleMask, sizeof(output)) == 0);

  const uint8_t reset[] = {0x01, 0x03, 0x0c, 0x00};
  check("recognize HCI Reset",
    espble_hci_controller_policy_is_reset(reset, sizeof(reset)));
  check("virtualize reset while dual host active",
    espble_hci_controller_policy_virtual_action(reset, sizeof(reset)) ==
      ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_COMPLETE);
  const uint8_t flowControl[] = {0x01, 0x31, 0x0c, 0x01, 0x01};
  const uint8_t hostBufferSize[] = {
    0x01, 0x33, 0x0c, 0x07, 0xff, 0x03, 0xff, 0x10, 0x00, 0x0a, 0x00};
  const uint8_t completedPackets[] = {
    0x01, 0x35, 0x0c, 0x05, 0x01, 0x80, 0x00, 0x01, 0x00};
  check("virtualize flow control setting",
    espble_hci_controller_policy_virtual_action(
      flowControl, sizeof(flowControl)) ==
        ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_COMPLETE);
  check("virtualize host buffer size",
    espble_hci_controller_policy_virtual_action(
      hostBufferSize, sizeof(hostBufferSize)) ==
        ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_COMPLETE);
  check("consume host completed packets without response",
    espble_hci_controller_policy_virtual_action(
      completedPackets, sizeof(completedPackets)) ==
        ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_NO_RESPONSE);
  check("other command passthrough", espble_hci_controller_policy_rewrite_command(
    &policy, 0, reset, sizeof(reset), output, sizeof(output)) ==
      ESPBLE_HCI_CONTROLLER_POLICY_PASSTHROUGH);
  const uint8_t malformedMask[] = {0x01, 0x01, 0x0c, 0x07, 0, 0, 0, 0};
  const uint8_t malformedReset[] = {0x01, 0x03, 0x0c, 0x01, 0x00};
  check("reject malformed HCI Reset classification",
    !espble_hci_controller_policy_is_reset(
      malformedReset, sizeof(malformedReset)));
  check("reject malformed virtual command",
    espble_hci_controller_policy_virtual_action(
      malformedReset, sizeof(malformedReset)) ==
        ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_INVALID_PACKET);
  const uint8_t overflowingCompletedPackets[] = {
    0x01, 0x35, 0x0c, 0x01, 0x40};
  check("reject completed-packet count whose encoded length overflows",
    espble_hci_controller_policy_virtual_action(
      overflowingCompletedPackets, sizeof(overflowingCompletedPackets)) ==
        ESPBLE_HCI_CONTROLLER_POLICY_VIRTUAL_INVALID_PACKET);
  check("malformed mask rejected", espble_hci_controller_policy_rewrite_command(
    &policy, 0, malformedMask, sizeof(malformedMask), output, sizeof(output)) ==
      ESPBLE_HCI_CONTROLLER_POLICY_INVALID_PACKET);
  check("invalid host rejected", espble_hci_controller_policy_rewrite_command(
    &policy, 2, classicMask, sizeof(classicMask), output, sizeof(output)) ==
      ESPBLE_HCI_CONTROLLER_POLICY_INVALID_PACKET);

  if (failures) return 1;
  std::puts("OK");
  return 0;
}
