#include "EspBleClassicHfpInternal.h"

#include <mutex>

namespace
{
std::mutex hfpProfileMutex;
const void *activeHfpProfile = nullptr;
}

bool espBleClassicAcquireHfpProfile(const void *owner)
{
  if (!owner) return false;
  std::lock_guard<std::mutex> lock(hfpProfileMutex);
  if (activeHfpProfile && activeHfpProfile != owner) return false;
  activeHfpProfile = owner;
  return true;
}

void espBleClassicReleaseHfpProfile(const void *owner)
{
  std::lock_guard<std::mutex> lock(hfpProfileMutex);
  if (activeHfpProfile == owner) activeHfpProfile = nullptr;
}
