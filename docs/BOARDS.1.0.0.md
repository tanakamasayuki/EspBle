# EspBle 1.0.0 — board build coverage (core 3.3.11)

- Targets: esp32s3, esp32, esp32c3, esp32c6, esp32h2, esp32p4
- Core versions: 3.3.11

Legend: ✅ builds · ❌ fails · — example absent in this version · · not applicable (no profile / board not in core)

## esp32s3

| Feature (example) | 3.3.11 |
| --- | --- |
| CompileSmoke (`CompileSmoke`) | ✅ |
| Gap (`Gap/AcceptList`) | ✅ |
| Gap (`Gap/Advertise`) | ✅ |
| Gap (`Gap/Beacon`) | ✅ |
| Gap (`Gap/Connect`) | ✅ |
| Gap (`Gap/ConnectionParameters`) | ✅ |
| Gap (`Gap/DirectedAdvertise`) | ✅ |
| Gap (`Gap/IBeacon`) | ✅ |
| Gap (`Gap/Mtu`) | ✅ |
| Gap (`Gap/PrivateAddress`) | ✅ |
| Gap (`Gap/Scan`) | ✅ |
| Gap (`Gap/ScanResponse`) | ✅ |
| Gap (`Gap/ServiceData`) | ✅ |
| Gatt (`Gatt/Alerts/AlertNotificationClient`) | ✅ |
| Gatt (`Gatt/Alerts/AlertNotificationServer`) | ✅ |
| Gatt (`Gatt/Alerts/ImmediateAlertClient`) | ✅ |
| Gatt (`Gatt/Alerts/ImmediateAlertServer`) | ✅ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusClient`) | ✅ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusServer`) | ✅ |
| Gatt (`Gatt/Alerts/ProximityClient`) | ✅ |
| Gatt (`Gatt/Alerts/ProximityServer`) | ✅ |
| Gatt (`Gatt/Basics/AutoReconnectClient`) | ✅ |
| Gatt (`Gatt/Basics/Client`) | ✅ |
| Gatt (`Gatt/Basics/IndicateClient`) | ✅ |
| Gatt (`Gatt/Basics/IndicateServer`) | ✅ |
| Gatt (`Gatt/Basics/NotifyServer`) | ✅ |
| Gatt (`Gatt/Basics/NusClient`) | ✅ |
| Gatt (`Gatt/Basics/NusServer`) | ✅ |
| Gatt (`Gatt/Basics/Server`) | ✅ |
| Gatt (`Gatt/Basics/SubscribeClient`) | ✅ |
| Gatt (`Gatt/Device/BatteryClient`) | ✅ |
| Gatt (`Gatt/Device/BatteryServer`) | ✅ |
| Gatt (`Gatt/Device/BondManagementClient`) | ✅ |
| Gatt (`Gatt/Device/BondManagementServer`) | ✅ |
| Gatt (`Gatt/Device/DeviceInfoClient`) | ✅ |
| Gatt (`Gatt/Device/DeviceInfoServer`) | ✅ |
| Gatt (`Gatt/Device/UserDataClient`) | ✅ |
| Gatt (`Gatt/Device/UserDataServer`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingPowerClient`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingPowerServer`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceClient`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceServer`) | ✅ |
| Gatt (`Gatt/Fitness/FitnessMachineClient`) | ✅ |
| Gatt (`Gatt/Fitness/FitnessMachineServer`) | ✅ |
| Gatt (`Gatt/Fitness/LocationNavigationClient`) | ✅ |
| Gatt (`Gatt/Fitness/LocationNavigationServer`) | ✅ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceClient`) | ✅ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceServer`) | ✅ |
| Gatt (`Gatt/Health/BloodPressureClient`) | ✅ |
| Gatt (`Gatt/Health/BloodPressureServer`) | ✅ |
| Gatt (`Gatt/Health/BodyCompositionClient`) | ✅ |
| Gatt (`Gatt/Health/BodyCompositionServer`) | ✅ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringClient`) | ✅ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringServer`) | ✅ |
| Gatt (`Gatt/Health/GlucoseClient`) | ✅ |
| Gatt (`Gatt/Health/GlucoseServer`) | ✅ |
| Gatt (`Gatt/Health/HealthThermometerClient`) | ✅ |
| Gatt (`Gatt/Health/HealthThermometerServer`) | ✅ |
| Gatt (`Gatt/Health/HeartRateClient`) | ✅ |
| Gatt (`Gatt/Health/HeartRateServer`) | ✅ |
| Gatt (`Gatt/Health/PulseOximeterClient`) | ✅ |
| Gatt (`Gatt/Health/PulseOximeterServer`) | ✅ |
| Gatt (`Gatt/Health/WeightScaleClient`) | ✅ |
| Gatt (`Gatt/Health/WeightScaleServer`) | ✅ |
| Gatt (`Gatt/Sensors/EnvironmentalClient`) | ✅ |
| Gatt (`Gatt/Sensors/EnvironmentalServer`) | ✅ |
| Gatt (`Gatt/Time/CurrentTimeClient`) | ✅ |
| Gatt (`Gatt/Time/CurrentTimeServer`) | ✅ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateClient`) | ✅ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateServer`) | ✅ |
| Hid (`Hid/CompositeKeyboardMouse`) | ✅ |
| Hid (`Hid/ConsumerControl`) | ✅ |
| Hid (`Hid/CustomClient`) | ✅ |
| Hid (`Hid/CustomDevice`) | ✅ |
| Hid (`Hid/KeyboardDevice`) | ✅ |
| Hid (`Hid/KeyboardHost`) | ✅ |
| Hid (`Hid/KeyboardNkro`) | ✅ |
| Hid (`Hid/Mouse`) | ✅ |
| Hid (`Hid/VendorDevice`) | ✅ |
| Hid (`Hid/VendorHost`) | ✅ |
| Info (`Info/ConnectionInspector`) | ✅ |
| Info (`Info/ScanDump`) | ✅ |
| Midi (`Midi/MidiDevice`) | ✅ |
| Midi (`Midi/MidiHost`) | ✅ |
| Security (`Security/JustWorksServer`) | ✅ |
| Security (`Security/NumericComparisonClient`) | ✅ |
| Security (`Security/NumericComparisonServer`) | ✅ |
| Security (`Security/RuntimePasskeyClient`) | ✅ |
| Security (`Security/RuntimePasskeyServer`) | ✅ |
| Security (`Security/StaticPasskeyClient`) | ✅ |
| Security (`Security/StaticPasskeyServer`) | ✅ |

## esp32

| Feature (example) | 3.3.11 |
| --- | --- |
| CompileSmoke (`CompileSmoke`) | ❌ |
| Gap (`Gap/AcceptList`) | ❌ |
| Gap (`Gap/Advertise`) | ❌ |
| Gap (`Gap/Beacon`) | ❌ |
| Gap (`Gap/Connect`) | ❌ |
| Gap (`Gap/ConnectionParameters`) | ❌ |
| Gap (`Gap/DirectedAdvertise`) | ❌ |
| Gap (`Gap/IBeacon`) | ❌ |
| Gap (`Gap/Mtu`) | ❌ |
| Gap (`Gap/PrivateAddress`) | ❌ |
| Gap (`Gap/Scan`) | ❌ |
| Gap (`Gap/ScanResponse`) | ❌ |
| Gap (`Gap/ServiceData`) | ❌ |
| Gatt (`Gatt/Alerts/AlertNotificationClient`) | ❌ |
| Gatt (`Gatt/Alerts/AlertNotificationServer`) | ❌ |
| Gatt (`Gatt/Alerts/ImmediateAlertClient`) | ❌ |
| Gatt (`Gatt/Alerts/ImmediateAlertServer`) | ❌ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusClient`) | ❌ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusServer`) | ❌ |
| Gatt (`Gatt/Alerts/ProximityClient`) | ❌ |
| Gatt (`Gatt/Alerts/ProximityServer`) | ❌ |
| Gatt (`Gatt/Basics/AutoReconnectClient`) | ❌ |
| Gatt (`Gatt/Basics/Client`) | ❌ |
| Gatt (`Gatt/Basics/IndicateClient`) | ❌ |
| Gatt (`Gatt/Basics/IndicateServer`) | ❌ |
| Gatt (`Gatt/Basics/NotifyServer`) | ❌ |
| Gatt (`Gatt/Basics/NusClient`) | ❌ |
| Gatt (`Gatt/Basics/NusServer`) | ❌ |
| Gatt (`Gatt/Basics/Server`) | ❌ |
| Gatt (`Gatt/Basics/SubscribeClient`) | ❌ |
| Gatt (`Gatt/Device/BatteryClient`) | ❌ |
| Gatt (`Gatt/Device/BatteryServer`) | ❌ |
| Gatt (`Gatt/Device/BondManagementClient`) | ❌ |
| Gatt (`Gatt/Device/BondManagementServer`) | ❌ |
| Gatt (`Gatt/Device/DeviceInfoClient`) | ❌ |
| Gatt (`Gatt/Device/DeviceInfoServer`) | ❌ |
| Gatt (`Gatt/Device/UserDataClient`) | ❌ |
| Gatt (`Gatt/Device/UserDataServer`) | ❌ |
| Gatt (`Gatt/Fitness/CyclingPowerClient`) | ❌ |
| Gatt (`Gatt/Fitness/CyclingPowerServer`) | ❌ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceClient`) | ❌ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceServer`) | ❌ |
| Gatt (`Gatt/Fitness/FitnessMachineClient`) | ❌ |
| Gatt (`Gatt/Fitness/FitnessMachineServer`) | ❌ |
| Gatt (`Gatt/Fitness/LocationNavigationClient`) | ❌ |
| Gatt (`Gatt/Fitness/LocationNavigationServer`) | ❌ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceClient`) | ❌ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceServer`) | ❌ |
| Gatt (`Gatt/Health/BloodPressureClient`) | ❌ |
| Gatt (`Gatt/Health/BloodPressureServer`) | ❌ |
| Gatt (`Gatt/Health/BodyCompositionClient`) | ❌ |
| Gatt (`Gatt/Health/BodyCompositionServer`) | ❌ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringClient`) | ❌ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringServer`) | ❌ |
| Gatt (`Gatt/Health/GlucoseClient`) | ❌ |
| Gatt (`Gatt/Health/GlucoseServer`) | ❌ |
| Gatt (`Gatt/Health/HealthThermometerClient`) | ❌ |
| Gatt (`Gatt/Health/HealthThermometerServer`) | ❌ |
| Gatt (`Gatt/Health/HeartRateClient`) | ❌ |
| Gatt (`Gatt/Health/HeartRateServer`) | ❌ |
| Gatt (`Gatt/Health/PulseOximeterClient`) | ❌ |
| Gatt (`Gatt/Health/PulseOximeterServer`) | ❌ |
| Gatt (`Gatt/Health/WeightScaleClient`) | ❌ |
| Gatt (`Gatt/Health/WeightScaleServer`) | ❌ |
| Gatt (`Gatt/Sensors/EnvironmentalClient`) | ❌ |
| Gatt (`Gatt/Sensors/EnvironmentalServer`) | ❌ |
| Gatt (`Gatt/Time/CurrentTimeClient`) | ❌ |
| Gatt (`Gatt/Time/CurrentTimeServer`) | ❌ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateClient`) | ❌ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateServer`) | ❌ |
| Hid (`Hid/CompositeKeyboardMouse`) | ❌ |
| Hid (`Hid/ConsumerControl`) | ❌ |
| Hid (`Hid/CustomClient`) | ❌ |
| Hid (`Hid/CustomDevice`) | ❌ |
| Hid (`Hid/KeyboardDevice`) | ❌ |
| Hid (`Hid/KeyboardHost`) | ❌ |
| Hid (`Hid/KeyboardNkro`) | ❌ |
| Hid (`Hid/Mouse`) | ❌ |
| Hid (`Hid/VendorDevice`) | ❌ |
| Hid (`Hid/VendorHost`) | ❌ |
| Info (`Info/ConnectionInspector`) | ❌ |
| Info (`Info/ScanDump`) | ❌ |
| Midi (`Midi/MidiDevice`) | ❌ |
| Midi (`Midi/MidiHost`) | ❌ |
| Security (`Security/JustWorksServer`) | ❌ |
| Security (`Security/NumericComparisonClient`) | ❌ |
| Security (`Security/NumericComparisonServer`) | ❌ |
| Security (`Security/RuntimePasskeyClient`) | ❌ |
| Security (`Security/RuntimePasskeyServer`) | ❌ |
| Security (`Security/StaticPasskeyClient`) | ❌ |
| Security (`Security/StaticPasskeyServer`) | ❌ |

## esp32c3

| Feature (example) | 3.3.11 |
| --- | --- |
| CompileSmoke (`CompileSmoke`) | ✅ |
| Gap (`Gap/AcceptList`) | ✅ |
| Gap (`Gap/Advertise`) | ✅ |
| Gap (`Gap/Beacon`) | ✅ |
| Gap (`Gap/Connect`) | ✅ |
| Gap (`Gap/ConnectionParameters`) | ✅ |
| Gap (`Gap/DirectedAdvertise`) | ✅ |
| Gap (`Gap/IBeacon`) | ✅ |
| Gap (`Gap/Mtu`) | ✅ |
| Gap (`Gap/PrivateAddress`) | ✅ |
| Gap (`Gap/Scan`) | ✅ |
| Gap (`Gap/ScanResponse`) | ✅ |
| Gap (`Gap/ServiceData`) | ✅ |
| Gatt (`Gatt/Alerts/AlertNotificationClient`) | ✅ |
| Gatt (`Gatt/Alerts/AlertNotificationServer`) | ✅ |
| Gatt (`Gatt/Alerts/ImmediateAlertClient`) | ✅ |
| Gatt (`Gatt/Alerts/ImmediateAlertServer`) | ✅ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusClient`) | ✅ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusServer`) | ✅ |
| Gatt (`Gatt/Alerts/ProximityClient`) | ✅ |
| Gatt (`Gatt/Alerts/ProximityServer`) | ✅ |
| Gatt (`Gatt/Basics/AutoReconnectClient`) | ✅ |
| Gatt (`Gatt/Basics/Client`) | ✅ |
| Gatt (`Gatt/Basics/IndicateClient`) | ✅ |
| Gatt (`Gatt/Basics/IndicateServer`) | ✅ |
| Gatt (`Gatt/Basics/NotifyServer`) | ✅ |
| Gatt (`Gatt/Basics/NusClient`) | ✅ |
| Gatt (`Gatt/Basics/NusServer`) | ✅ |
| Gatt (`Gatt/Basics/Server`) | ✅ |
| Gatt (`Gatt/Basics/SubscribeClient`) | ✅ |
| Gatt (`Gatt/Device/BatteryClient`) | ✅ |
| Gatt (`Gatt/Device/BatteryServer`) | ✅ |
| Gatt (`Gatt/Device/BondManagementClient`) | ✅ |
| Gatt (`Gatt/Device/BondManagementServer`) | ✅ |
| Gatt (`Gatt/Device/DeviceInfoClient`) | ✅ |
| Gatt (`Gatt/Device/DeviceInfoServer`) | ✅ |
| Gatt (`Gatt/Device/UserDataClient`) | ✅ |
| Gatt (`Gatt/Device/UserDataServer`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingPowerClient`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingPowerServer`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceClient`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceServer`) | ✅ |
| Gatt (`Gatt/Fitness/FitnessMachineClient`) | ✅ |
| Gatt (`Gatt/Fitness/FitnessMachineServer`) | ✅ |
| Gatt (`Gatt/Fitness/LocationNavigationClient`) | ✅ |
| Gatt (`Gatt/Fitness/LocationNavigationServer`) | ✅ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceClient`) | ✅ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceServer`) | ✅ |
| Gatt (`Gatt/Health/BloodPressureClient`) | ✅ |
| Gatt (`Gatt/Health/BloodPressureServer`) | ✅ |
| Gatt (`Gatt/Health/BodyCompositionClient`) | ✅ |
| Gatt (`Gatt/Health/BodyCompositionServer`) | ✅ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringClient`) | ✅ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringServer`) | ✅ |
| Gatt (`Gatt/Health/GlucoseClient`) | ✅ |
| Gatt (`Gatt/Health/GlucoseServer`) | ✅ |
| Gatt (`Gatt/Health/HealthThermometerClient`) | ✅ |
| Gatt (`Gatt/Health/HealthThermometerServer`) | ✅ |
| Gatt (`Gatt/Health/HeartRateClient`) | ✅ |
| Gatt (`Gatt/Health/HeartRateServer`) | ✅ |
| Gatt (`Gatt/Health/PulseOximeterClient`) | ✅ |
| Gatt (`Gatt/Health/PulseOximeterServer`) | ✅ |
| Gatt (`Gatt/Health/WeightScaleClient`) | ✅ |
| Gatt (`Gatt/Health/WeightScaleServer`) | ✅ |
| Gatt (`Gatt/Sensors/EnvironmentalClient`) | ✅ |
| Gatt (`Gatt/Sensors/EnvironmentalServer`) | ✅ |
| Gatt (`Gatt/Time/CurrentTimeClient`) | ✅ |
| Gatt (`Gatt/Time/CurrentTimeServer`) | ✅ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateClient`) | ✅ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateServer`) | ✅ |
| Hid (`Hid/CompositeKeyboardMouse`) | ✅ |
| Hid (`Hid/ConsumerControl`) | ✅ |
| Hid (`Hid/CustomClient`) | ✅ |
| Hid (`Hid/CustomDevice`) | ✅ |
| Hid (`Hid/KeyboardDevice`) | ✅ |
| Hid (`Hid/KeyboardHost`) | ✅ |
| Hid (`Hid/KeyboardNkro`) | ✅ |
| Hid (`Hid/Mouse`) | ✅ |
| Hid (`Hid/VendorDevice`) | ✅ |
| Hid (`Hid/VendorHost`) | ✅ |
| Info (`Info/ConnectionInspector`) | ✅ |
| Info (`Info/ScanDump`) | ✅ |
| Midi (`Midi/MidiDevice`) | ✅ |
| Midi (`Midi/MidiHost`) | ✅ |
| Security (`Security/JustWorksServer`) | ✅ |
| Security (`Security/NumericComparisonClient`) | ✅ |
| Security (`Security/NumericComparisonServer`) | ✅ |
| Security (`Security/RuntimePasskeyClient`) | ✅ |
| Security (`Security/RuntimePasskeyServer`) | ✅ |
| Security (`Security/StaticPasskeyClient`) | ✅ |
| Security (`Security/StaticPasskeyServer`) | ✅ |

## esp32c6

| Feature (example) | 3.3.11 |
| --- | --- |
| CompileSmoke (`CompileSmoke`) | ✅ |
| Gap (`Gap/AcceptList`) | ✅ |
| Gap (`Gap/Advertise`) | ✅ |
| Gap (`Gap/Beacon`) | ✅ |
| Gap (`Gap/Connect`) | ✅ |
| Gap (`Gap/ConnectionParameters`) | ✅ |
| Gap (`Gap/DirectedAdvertise`) | ✅ |
| Gap (`Gap/IBeacon`) | ✅ |
| Gap (`Gap/Mtu`) | ✅ |
| Gap (`Gap/PrivateAddress`) | ✅ |
| Gap (`Gap/Scan`) | ✅ |
| Gap (`Gap/ScanResponse`) | ✅ |
| Gap (`Gap/ServiceData`) | ✅ |
| Gatt (`Gatt/Alerts/AlertNotificationClient`) | ✅ |
| Gatt (`Gatt/Alerts/AlertNotificationServer`) | ✅ |
| Gatt (`Gatt/Alerts/ImmediateAlertClient`) | ✅ |
| Gatt (`Gatt/Alerts/ImmediateAlertServer`) | ✅ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusClient`) | ✅ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusServer`) | ✅ |
| Gatt (`Gatt/Alerts/ProximityClient`) | ✅ |
| Gatt (`Gatt/Alerts/ProximityServer`) | ✅ |
| Gatt (`Gatt/Basics/AutoReconnectClient`) | ✅ |
| Gatt (`Gatt/Basics/Client`) | ✅ |
| Gatt (`Gatt/Basics/IndicateClient`) | ✅ |
| Gatt (`Gatt/Basics/IndicateServer`) | ✅ |
| Gatt (`Gatt/Basics/NotifyServer`) | ✅ |
| Gatt (`Gatt/Basics/NusClient`) | ✅ |
| Gatt (`Gatt/Basics/NusServer`) | ✅ |
| Gatt (`Gatt/Basics/Server`) | ✅ |
| Gatt (`Gatt/Basics/SubscribeClient`) | ✅ |
| Gatt (`Gatt/Device/BatteryClient`) | ✅ |
| Gatt (`Gatt/Device/BatteryServer`) | ✅ |
| Gatt (`Gatt/Device/BondManagementClient`) | ✅ |
| Gatt (`Gatt/Device/BondManagementServer`) | ✅ |
| Gatt (`Gatt/Device/DeviceInfoClient`) | ✅ |
| Gatt (`Gatt/Device/DeviceInfoServer`) | ✅ |
| Gatt (`Gatt/Device/UserDataClient`) | ✅ |
| Gatt (`Gatt/Device/UserDataServer`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingPowerClient`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingPowerServer`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceClient`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceServer`) | ✅ |
| Gatt (`Gatt/Fitness/FitnessMachineClient`) | ✅ |
| Gatt (`Gatt/Fitness/FitnessMachineServer`) | ✅ |
| Gatt (`Gatt/Fitness/LocationNavigationClient`) | ✅ |
| Gatt (`Gatt/Fitness/LocationNavigationServer`) | ✅ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceClient`) | ✅ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceServer`) | ✅ |
| Gatt (`Gatt/Health/BloodPressureClient`) | ✅ |
| Gatt (`Gatt/Health/BloodPressureServer`) | ✅ |
| Gatt (`Gatt/Health/BodyCompositionClient`) | ✅ |
| Gatt (`Gatt/Health/BodyCompositionServer`) | ✅ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringClient`) | ✅ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringServer`) | ✅ |
| Gatt (`Gatt/Health/GlucoseClient`) | ✅ |
| Gatt (`Gatt/Health/GlucoseServer`) | ✅ |
| Gatt (`Gatt/Health/HealthThermometerClient`) | ✅ |
| Gatt (`Gatt/Health/HealthThermometerServer`) | ✅ |
| Gatt (`Gatt/Health/HeartRateClient`) | ✅ |
| Gatt (`Gatt/Health/HeartRateServer`) | ✅ |
| Gatt (`Gatt/Health/PulseOximeterClient`) | ✅ |
| Gatt (`Gatt/Health/PulseOximeterServer`) | ✅ |
| Gatt (`Gatt/Health/WeightScaleClient`) | ✅ |
| Gatt (`Gatt/Health/WeightScaleServer`) | ✅ |
| Gatt (`Gatt/Sensors/EnvironmentalClient`) | ✅ |
| Gatt (`Gatt/Sensors/EnvironmentalServer`) | ✅ |
| Gatt (`Gatt/Time/CurrentTimeClient`) | ✅ |
| Gatt (`Gatt/Time/CurrentTimeServer`) | ✅ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateClient`) | ✅ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateServer`) | ✅ |
| Hid (`Hid/CompositeKeyboardMouse`) | ✅ |
| Hid (`Hid/ConsumerControl`) | ✅ |
| Hid (`Hid/CustomClient`) | ✅ |
| Hid (`Hid/CustomDevice`) | ✅ |
| Hid (`Hid/KeyboardDevice`) | ✅ |
| Hid (`Hid/KeyboardHost`) | ✅ |
| Hid (`Hid/KeyboardNkro`) | ✅ |
| Hid (`Hid/Mouse`) | ✅ |
| Hid (`Hid/VendorDevice`) | ✅ |
| Hid (`Hid/VendorHost`) | ✅ |
| Info (`Info/ConnectionInspector`) | ✅ |
| Info (`Info/ScanDump`) | ✅ |
| Midi (`Midi/MidiDevice`) | ✅ |
| Midi (`Midi/MidiHost`) | ✅ |
| Security (`Security/JustWorksServer`) | ✅ |
| Security (`Security/NumericComparisonClient`) | ✅ |
| Security (`Security/NumericComparisonServer`) | ✅ |
| Security (`Security/RuntimePasskeyClient`) | ✅ |
| Security (`Security/RuntimePasskeyServer`) | ✅ |
| Security (`Security/StaticPasskeyClient`) | ✅ |
| Security (`Security/StaticPasskeyServer`) | ✅ |

## esp32h2

| Feature (example) | 3.3.11 |
| --- | --- |
| CompileSmoke (`CompileSmoke`) | ✅ |
| Gap (`Gap/AcceptList`) | ✅ |
| Gap (`Gap/Advertise`) | ✅ |
| Gap (`Gap/Beacon`) | ✅ |
| Gap (`Gap/Connect`) | ✅ |
| Gap (`Gap/ConnectionParameters`) | ✅ |
| Gap (`Gap/DirectedAdvertise`) | ✅ |
| Gap (`Gap/IBeacon`) | ✅ |
| Gap (`Gap/Mtu`) | ✅ |
| Gap (`Gap/PrivateAddress`) | ✅ |
| Gap (`Gap/Scan`) | ✅ |
| Gap (`Gap/ScanResponse`) | ✅ |
| Gap (`Gap/ServiceData`) | ✅ |
| Gatt (`Gatt/Alerts/AlertNotificationClient`) | ✅ |
| Gatt (`Gatt/Alerts/AlertNotificationServer`) | ✅ |
| Gatt (`Gatt/Alerts/ImmediateAlertClient`) | ✅ |
| Gatt (`Gatt/Alerts/ImmediateAlertServer`) | ✅ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusClient`) | ✅ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusServer`) | ✅ |
| Gatt (`Gatt/Alerts/ProximityClient`) | ✅ |
| Gatt (`Gatt/Alerts/ProximityServer`) | ✅ |
| Gatt (`Gatt/Basics/AutoReconnectClient`) | ✅ |
| Gatt (`Gatt/Basics/Client`) | ✅ |
| Gatt (`Gatt/Basics/IndicateClient`) | ✅ |
| Gatt (`Gatt/Basics/IndicateServer`) | ✅ |
| Gatt (`Gatt/Basics/NotifyServer`) | ✅ |
| Gatt (`Gatt/Basics/NusClient`) | ✅ |
| Gatt (`Gatt/Basics/NusServer`) | ✅ |
| Gatt (`Gatt/Basics/Server`) | ✅ |
| Gatt (`Gatt/Basics/SubscribeClient`) | ✅ |
| Gatt (`Gatt/Device/BatteryClient`) | ✅ |
| Gatt (`Gatt/Device/BatteryServer`) | ✅ |
| Gatt (`Gatt/Device/BondManagementClient`) | ✅ |
| Gatt (`Gatt/Device/BondManagementServer`) | ✅ |
| Gatt (`Gatt/Device/DeviceInfoClient`) | ✅ |
| Gatt (`Gatt/Device/DeviceInfoServer`) | ✅ |
| Gatt (`Gatt/Device/UserDataClient`) | ✅ |
| Gatt (`Gatt/Device/UserDataServer`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingPowerClient`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingPowerServer`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceClient`) | ✅ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceServer`) | ✅ |
| Gatt (`Gatt/Fitness/FitnessMachineClient`) | ✅ |
| Gatt (`Gatt/Fitness/FitnessMachineServer`) | ✅ |
| Gatt (`Gatt/Fitness/LocationNavigationClient`) | ✅ |
| Gatt (`Gatt/Fitness/LocationNavigationServer`) | ✅ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceClient`) | ✅ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceServer`) | ✅ |
| Gatt (`Gatt/Health/BloodPressureClient`) | ✅ |
| Gatt (`Gatt/Health/BloodPressureServer`) | ✅ |
| Gatt (`Gatt/Health/BodyCompositionClient`) | ✅ |
| Gatt (`Gatt/Health/BodyCompositionServer`) | ✅ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringClient`) | ✅ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringServer`) | ✅ |
| Gatt (`Gatt/Health/GlucoseClient`) | ✅ |
| Gatt (`Gatt/Health/GlucoseServer`) | ✅ |
| Gatt (`Gatt/Health/HealthThermometerClient`) | ✅ |
| Gatt (`Gatt/Health/HealthThermometerServer`) | ✅ |
| Gatt (`Gatt/Health/HeartRateClient`) | ✅ |
| Gatt (`Gatt/Health/HeartRateServer`) | ✅ |
| Gatt (`Gatt/Health/PulseOximeterClient`) | ✅ |
| Gatt (`Gatt/Health/PulseOximeterServer`) | ✅ |
| Gatt (`Gatt/Health/WeightScaleClient`) | ✅ |
| Gatt (`Gatt/Health/WeightScaleServer`) | ✅ |
| Gatt (`Gatt/Sensors/EnvironmentalClient`) | ✅ |
| Gatt (`Gatt/Sensors/EnvironmentalServer`) | ✅ |
| Gatt (`Gatt/Time/CurrentTimeClient`) | ✅ |
| Gatt (`Gatt/Time/CurrentTimeServer`) | ✅ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateClient`) | ✅ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateServer`) | ✅ |
| Hid (`Hid/CompositeKeyboardMouse`) | ✅ |
| Hid (`Hid/ConsumerControl`) | ✅ |
| Hid (`Hid/CustomClient`) | ✅ |
| Hid (`Hid/CustomDevice`) | ✅ |
| Hid (`Hid/KeyboardDevice`) | ✅ |
| Hid (`Hid/KeyboardHost`) | ✅ |
| Hid (`Hid/KeyboardNkro`) | ✅ |
| Hid (`Hid/Mouse`) | ✅ |
| Hid (`Hid/VendorDevice`) | ✅ |
| Hid (`Hid/VendorHost`) | ✅ |
| Info (`Info/ConnectionInspector`) | ✅ |
| Info (`Info/ScanDump`) | ✅ |
| Midi (`Midi/MidiDevice`) | ✅ |
| Midi (`Midi/MidiHost`) | ✅ |
| Security (`Security/JustWorksServer`) | ✅ |
| Security (`Security/NumericComparisonClient`) | ✅ |
| Security (`Security/NumericComparisonServer`) | ✅ |
| Security (`Security/RuntimePasskeyClient`) | ✅ |
| Security (`Security/RuntimePasskeyServer`) | ✅ |
| Security (`Security/StaticPasskeyClient`) | ✅ |
| Security (`Security/StaticPasskeyServer`) | ✅ |

## esp32p4

| Feature (example) | 3.3.11 |
| --- | --- |
| CompileSmoke (`CompileSmoke`) | ❌ |
| Gap (`Gap/AcceptList`) | ❌ |
| Gap (`Gap/Advertise`) | ❌ |
| Gap (`Gap/Beacon`) | ❌ |
| Gap (`Gap/Connect`) | ❌ |
| Gap (`Gap/ConnectionParameters`) | ❌ |
| Gap (`Gap/DirectedAdvertise`) | ❌ |
| Gap (`Gap/IBeacon`) | ❌ |
| Gap (`Gap/Mtu`) | ❌ |
| Gap (`Gap/PrivateAddress`) | ❌ |
| Gap (`Gap/Scan`) | ❌ |
| Gap (`Gap/ScanResponse`) | ❌ |
| Gap (`Gap/ServiceData`) | ❌ |
| Gatt (`Gatt/Alerts/AlertNotificationClient`) | ❌ |
| Gatt (`Gatt/Alerts/AlertNotificationServer`) | ❌ |
| Gatt (`Gatt/Alerts/ImmediateAlertClient`) | ❌ |
| Gatt (`Gatt/Alerts/ImmediateAlertServer`) | ❌ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusClient`) | ❌ |
| Gatt (`Gatt/Alerts/PhoneAlertStatusServer`) | ❌ |
| Gatt (`Gatt/Alerts/ProximityClient`) | ❌ |
| Gatt (`Gatt/Alerts/ProximityServer`) | ❌ |
| Gatt (`Gatt/Basics/AutoReconnectClient`) | ❌ |
| Gatt (`Gatt/Basics/Client`) | ❌ |
| Gatt (`Gatt/Basics/IndicateClient`) | ❌ |
| Gatt (`Gatt/Basics/IndicateServer`) | ❌ |
| Gatt (`Gatt/Basics/NotifyServer`) | ❌ |
| Gatt (`Gatt/Basics/NusClient`) | ❌ |
| Gatt (`Gatt/Basics/NusServer`) | ❌ |
| Gatt (`Gatt/Basics/Server`) | ❌ |
| Gatt (`Gatt/Basics/SubscribeClient`) | ❌ |
| Gatt (`Gatt/Device/BatteryClient`) | ❌ |
| Gatt (`Gatt/Device/BatteryServer`) | ❌ |
| Gatt (`Gatt/Device/BondManagementClient`) | ❌ |
| Gatt (`Gatt/Device/BondManagementServer`) | ❌ |
| Gatt (`Gatt/Device/DeviceInfoClient`) | ❌ |
| Gatt (`Gatt/Device/DeviceInfoServer`) | ❌ |
| Gatt (`Gatt/Device/UserDataClient`) | ❌ |
| Gatt (`Gatt/Device/UserDataServer`) | ❌ |
| Gatt (`Gatt/Fitness/CyclingPowerClient`) | ❌ |
| Gatt (`Gatt/Fitness/CyclingPowerServer`) | ❌ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceClient`) | ❌ |
| Gatt (`Gatt/Fitness/CyclingSpeedCadenceServer`) | ❌ |
| Gatt (`Gatt/Fitness/FitnessMachineClient`) | ❌ |
| Gatt (`Gatt/Fitness/FitnessMachineServer`) | ❌ |
| Gatt (`Gatt/Fitness/LocationNavigationClient`) | ❌ |
| Gatt (`Gatt/Fitness/LocationNavigationServer`) | ❌ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceClient`) | ❌ |
| Gatt (`Gatt/Fitness/RunningSpeedCadenceServer`) | ❌ |
| Gatt (`Gatt/Health/BloodPressureClient`) | ❌ |
| Gatt (`Gatt/Health/BloodPressureServer`) | ❌ |
| Gatt (`Gatt/Health/BodyCompositionClient`) | ❌ |
| Gatt (`Gatt/Health/BodyCompositionServer`) | ❌ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringClient`) | ❌ |
| Gatt (`Gatt/Health/ContinuousGlucoseMonitoringServer`) | ❌ |
| Gatt (`Gatt/Health/GlucoseClient`) | ❌ |
| Gatt (`Gatt/Health/GlucoseServer`) | ❌ |
| Gatt (`Gatt/Health/HealthThermometerClient`) | ❌ |
| Gatt (`Gatt/Health/HealthThermometerServer`) | ❌ |
| Gatt (`Gatt/Health/HeartRateClient`) | ❌ |
| Gatt (`Gatt/Health/HeartRateServer`) | ❌ |
| Gatt (`Gatt/Health/PulseOximeterClient`) | ❌ |
| Gatt (`Gatt/Health/PulseOximeterServer`) | ❌ |
| Gatt (`Gatt/Health/WeightScaleClient`) | ❌ |
| Gatt (`Gatt/Health/WeightScaleServer`) | ❌ |
| Gatt (`Gatt/Sensors/EnvironmentalClient`) | ❌ |
| Gatt (`Gatt/Sensors/EnvironmentalServer`) | ❌ |
| Gatt (`Gatt/Time/CurrentTimeClient`) | ❌ |
| Gatt (`Gatt/Time/CurrentTimeServer`) | ❌ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateClient`) | ❌ |
| Gatt (`Gatt/Time/ReferenceTimeUpdateServer`) | ❌ |
| Hid (`Hid/CompositeKeyboardMouse`) | ❌ |
| Hid (`Hid/ConsumerControl`) | ❌ |
| Hid (`Hid/CustomClient`) | ❌ |
| Hid (`Hid/CustomDevice`) | ❌ |
| Hid (`Hid/KeyboardDevice`) | ❌ |
| Hid (`Hid/KeyboardHost`) | ❌ |
| Hid (`Hid/KeyboardNkro`) | ❌ |
| Hid (`Hid/Mouse`) | ❌ |
| Hid (`Hid/VendorDevice`) | ❌ |
| Hid (`Hid/VendorHost`) | ❌ |
| Info (`Info/ConnectionInspector`) | ❌ |
| Info (`Info/ScanDump`) | ❌ |
| Midi (`Midi/MidiDevice`) | ❌ |
| Midi (`Midi/MidiHost`) | ❌ |
| Security (`Security/JustWorksServer`) | ❌ |
| Security (`Security/NumericComparisonClient`) | ❌ |
| Security (`Security/NumericComparisonServer`) | ❌ |
| Security (`Security/RuntimePasskeyClient`) | ❌ |
| Security (`Security/RuntimePasskeyServer`) | ❌ |
| Security (`Security/StaticPasskeyClient`) | ❌ |
| Security (`Security/StaticPasskeyServer`) | ❌ |

## Failure details

- `CompileSmoke` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `CompileSmoke` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/AcceptList` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/AcceptList` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/Advertise` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Advertise` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/Beacon` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Beacon` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/Connect` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Connect` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/ConnectionParameters` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/ConnectionParameters` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/DirectedAdvertise` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/DirectedAdvertise` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/IBeacon` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/IBeacon` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/Mtu` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Mtu` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/PrivateAddress` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/PrivateAddress` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/Scan` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/Scan` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/ScanResponse` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/ScanResponse` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gap/ServiceData` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gap/ServiceData` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Alerts/AlertNotificationClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Alerts/AlertNotificationClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Alerts/AlertNotificationServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Alerts/AlertNotificationServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Alerts/ImmediateAlertClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Alerts/ImmediateAlertClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Alerts/ImmediateAlertServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Alerts/ImmediateAlertServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Alerts/PhoneAlertStatusClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Alerts/PhoneAlertStatusClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Alerts/PhoneAlertStatusServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Alerts/PhoneAlertStatusServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Alerts/ProximityClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Alerts/ProximityClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Alerts/ProximityServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Alerts/ProximityServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Basics/AutoReconnectClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Basics/AutoReconnectClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Basics/Client` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Basics/Client` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Basics/IndicateClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Basics/IndicateClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Basics/IndicateServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Basics/IndicateServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Basics/NotifyServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Basics/NotifyServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Basics/NusClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Basics/NusClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Basics/NusServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Basics/NusServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Basics/Server` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Basics/Server` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Basics/SubscribeClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Basics/SubscribeClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Device/BatteryClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Device/BatteryClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Device/BatteryServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Device/BatteryServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Device/BondManagementClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Device/BondManagementClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Device/BondManagementServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Device/BondManagementServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Device/DeviceInfoClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Device/DeviceInfoClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Device/DeviceInfoServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Device/DeviceInfoServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Device/UserDataClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Device/UserDataClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Device/UserDataServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Device/UserDataServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Fitness/CyclingPowerClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Fitness/CyclingPowerClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Fitness/CyclingPowerServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Fitness/CyclingPowerServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Fitness/CyclingSpeedCadenceClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Fitness/CyclingSpeedCadenceClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Fitness/CyclingSpeedCadenceServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Fitness/CyclingSpeedCadenceServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Fitness/FitnessMachineClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Fitness/FitnessMachineClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Fitness/FitnessMachineServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Fitness/FitnessMachineServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Fitness/LocationNavigationClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Fitness/LocationNavigationClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Fitness/LocationNavigationServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Fitness/LocationNavigationServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Fitness/RunningSpeedCadenceClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Fitness/RunningSpeedCadenceClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Fitness/RunningSpeedCadenceServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Fitness/RunningSpeedCadenceServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/BloodPressureClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/BloodPressureClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/BloodPressureServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/BloodPressureServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/BodyCompositionClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/BodyCompositionClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/BodyCompositionServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/BodyCompositionServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/ContinuousGlucoseMonitoringClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/ContinuousGlucoseMonitoringClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/ContinuousGlucoseMonitoringServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/ContinuousGlucoseMonitoringServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/GlucoseClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/GlucoseClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/GlucoseServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/GlucoseServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/HealthThermometerClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/HealthThermometerClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/HealthThermometerServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/HealthThermometerServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/HeartRateClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/HeartRateClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/HeartRateServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/HeartRateServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/PulseOximeterClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/PulseOximeterClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/PulseOximeterServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/PulseOximeterServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/WeightScaleClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/WeightScaleClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Health/WeightScaleServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Health/WeightScaleServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Sensors/EnvironmentalClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Sensors/EnvironmentalClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Sensors/EnvironmentalServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Sensors/EnvironmentalServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Time/CurrentTimeClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Time/CurrentTimeClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Time/CurrentTimeServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Time/CurrentTimeServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Time/ReferenceTimeUpdateClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Time/ReferenceTimeUpdateClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Gatt/Time/ReferenceTimeUpdateServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Gatt/Time/ReferenceTimeUpdateServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Hid/CompositeKeyboardMouse` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/CompositeKeyboardMouse` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Hid/ConsumerControl` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/ConsumerControl` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Hid/CustomClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/CustomClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Hid/CustomDevice` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/CustomDevice` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Hid/KeyboardDevice` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardDevice` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Hid/KeyboardHost` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardHost` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Hid/KeyboardNkro` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/KeyboardNkro` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Hid/Mouse` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/Mouse` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Hid/VendorDevice` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/VendorDevice` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Hid/VendorHost` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Hid/VendorHost` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Info/ConnectionInspector` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Info/ConnectionInspector` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Info/ScanDump` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Info/ScanDump` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Midi/MidiDevice` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Midi/MidiDevice` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Midi/MidiHost` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Midi/MidiHost` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Security/JustWorksServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/JustWorksServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Security/NumericComparisonClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/NumericComparisonClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Security/NumericComparisonServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/NumericComparisonServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Security/RuntimePasskeyClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/RuntimePasskeyClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Security/RuntimePasskeyServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/RuntimePasskeyServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Security/StaticPasskeyClient` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyClient` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`
- `Security/StaticPasskeyServer` @ esp32 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.h:11:2: error: #error "EspBle requires the NimBLE backend bundled with Arduino-ESP32"`
- `Security/StaticPasskeyServer` @ esp32p4 / 3.3.11: `/home/runner/work/EspBle/EspBle/src/EspBle.cpp:3:10: fatal error: esp_bt.h: No such file or directory`

