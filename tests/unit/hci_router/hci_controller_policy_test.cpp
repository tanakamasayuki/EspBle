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
  check("other command passthrough", espble_hci_controller_policy_rewrite_command(
    &policy, 0, reset, sizeof(reset), output, sizeof(output)) ==
      ESPBLE_HCI_CONTROLLER_POLICY_PASSTHROUGH);
  const uint8_t malformedMask[] = {0x01, 0x01, 0x0c, 0x07, 0, 0, 0, 0};
  const uint8_t malformedReset[] = {0x01, 0x03, 0x0c, 0x01, 0x00};
  check("reject malformed HCI Reset classification",
    !espble_hci_controller_policy_is_reset(
      malformedReset, sizeof(malformedReset)));
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
