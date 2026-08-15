#pragma once

// ESP-IDF exposes one global callback slot for each HFP role and does not
// support running its Client and Audio Gateway profiles together. Keep the
// exclusion process-wide rather than tying it to one EspBleClassic instance.
bool espBleClassicAcquireHfpProfile(const void *owner);
void espBleClassicReleaseHfpProfile(const void *owner);

