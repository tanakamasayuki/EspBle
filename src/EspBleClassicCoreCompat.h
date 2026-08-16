#ifndef ESP_BLE_CLASSIC_CORE_COMPAT_H
#define ESP_BLE_CLASSIC_CORE_COMPAT_H

// Names the Bluetooth Classic sources use that not every supported Arduino-ESP32
// version publishes. Each one is a rename or an addition in the ESP-IDF a core
// bundles, not a change in behaviour, so a fallback here is what the older header
// would have said. Include this after the ESP-IDF Bluetooth headers.
//
// Nothing here compensates for a difference in the stack itself: those belong in
// the code that uses them, where the difference can be handled rather than hidden.

// ESP-IDF v5.5.5 corrected the spelling of the SBC allocation method and kept the
// old name as a deprecated alias. Cores below it publish only the old spelling.
// Both are 0x2.
#if !defined(ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR) && \
  defined(ESP_A2D_SBC_CIE_ALLOC_MTHD_SRN)
#define ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR ESP_A2D_SBC_CIE_ALLOC_MTHD_SRN
#endif

// ESP-IDF v5.5.5 published the SDP pad limit that the HID device record shares
// with its strings. The bound belongs to the host that stores the record, so the
// value the bundled host was built with is the one to check a descriptor against.
#ifndef ESP_HIDD_APP_DESC_LIST_LEN_MAX
#define ESP_HIDD_APP_DESC_LIST_LEN_MAX 2048
#endif

#endif
