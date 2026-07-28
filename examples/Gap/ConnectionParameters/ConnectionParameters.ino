// en: ConnectionParameters - tune a live connection. The parameters that decide
//     responsiveness and power draw cannot be chosen when connecting; they are
//     negotiated by the controller and then changed on the running link. This
//     sketch connects, shows what was negotiated, and switches between a
//     low-latency and a low-power profile, plus the 2M PHY.
// ja: ConnectionParameters - 確立済みの接続を調整する。応答性と消費電力を決める
//     パラメータは接続時に指定できず、controllerが決めた値で接続したあとに変更する。
//     この例では接続して交渉結果を表示し、低遅延profileと省電力profileを切り替え、
//     2M PHYへの変更も行う。
#include <EspBle.h>

// en: Any connectable peripheral works; Gap/Advertise advertises this UUID.
// ja: connectableなPeripheralなら何でもよい。Gap/AdvertiseがこのUUIDを出す。
static constexpr const char *TARGET_SERVICE_UUID = "1812";

EspBle ble;
EspBleConnectionId connectionId = 0;

// en: Units come straight from the BLE spec, so convert them when printing.
//     interval and latency are in 1.25 ms units, timeout in 10 ms units.
// ja: 単位はBLE仕様そのままなので、表示時に換算する。
//     intervalは1.25 ms単位、timeoutは10 ms単位。
static void printParameters(const char *label, const EspBleConnection &connection)
{
  Serial.printf(
    "%s interval=%u (%.2f ms) latency=%u timeout=%u (%u ms) phy=tx%u/rx%u\n",
    label,
    connection.connectionInterval,
    connection.connectionInterval * 1.25f,
    connection.peripheralLatency,
    connection.supervisionTimeout,
    static_cast<unsigned>(connection.supervisionTimeout) * 10,
    connection.txPhy,
    connection.rxPhy);
}

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Connection Parameters";
  if (!ble.begin(config))
  {
    Serial.printf("BLE init failed: %s (%s)\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionId != 0 || !scanResult.advertisesService(TARGET_SERVICE_UUID)) return;
    ble.scanner().stop();
    ble.connect(scanResult);
  });

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    // en: These are the values the controller chose. They were not requested.
    // ja: controllerが選んだ値。こちらから要求したものではない。
    printParameters("CONNECTED", connection);
    Serial.println("Commands: f fast, s slow, p 2M PHY, d disconnect");
  });

  // en: The result of a parameter change arrives here, not from the request.
  //     The peer may grant something other than what was asked for.
  // ja: 変更の結果は要求の戻り値ではなくこちらへ届く。
  //     相手が要求と違う値を返すこともある。
  ble.onConnectionParametersUpdated([](const EspBleConnection &connection) {
    printParameters("PARAMETERS", connection);
  });
  ble.onPhyUpdated([](const EspBleConnection &connection) {
    printParameters("PHY", connection);
  });

  ble.onConnectionFailed([](const EspBleConnectionFailure &failure) {
    // en: connect() returning true only means the request was accepted.
    // ja: connect() の true は要求を受け付けたという意味でしかない。
    Serial.printf("CONNECT_FAILED peer=%s detail=%s\n",
      failure.peerAddress.c_str(), failure.detail.c_str());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    Serial.printf("DISCONNECTED id=%u\n", connection.id);
  });

  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  ble.scanner().start(scanConfig);
  Serial.println("Scanning for a peripheral...");
}

void loop()
{
  if (Serial.available() > 0 && connectionId != 0)
  {
    const char command = Serial.read();
    if (command == 'f')
    {
      // en: Low latency: 15-30 ms interval, no skipping, 4 s supervision.
      //     Responsive, but the radio wakes often and burns current.
      // ja: 低遅延: interval 15-30 ms、skipなし、supervision 4秒。
      //     応答は速いが、無線が頻繁に起きるので電流を食う。
      Serial.printf("REQUEST fast accepted=%u\n",
        ble.updateConnectionParameters(connectionId, 12, 24, 0, 400) ? 1 : 0);
    }
    else if (command == 's')
    {
      // en: Low power: 400-500 ms interval, and the peripheral may skip 4
      //     events when it has nothing to send. Supervision must stay longer
      //     than (1 + latency) * maxInterval, hence 6 s here.
      // ja: 省電力: interval 400-500 ms、送るものがなければPeripheralは4回まで
      //     skipしてよい。supervisionは (1 + latency) * maxInterval より長くする
      //     必要があるため6秒にしている。
      Serial.printf("REQUEST slow accepted=%u\n",
        ble.updateConnectionParameters(connectionId, 320, 400, 4, 600) ? 1 : 0);
    }
    else if (command == 'p')
    {
      // en: 2M PHY doubles the symbol rate: shorter airtime, less energy per
      //     byte, shorter range. Both sides and both radios must support it.
      // ja: 2M PHYはシンボルレートが倍。電波に乗る時間が短くなり1バイトあたりの
      //     消費が減るが、距離は縮む。両側の無線が対応している必要がある。
      Serial.printf("REQUEST 2M PHY accepted=%u\n",
        ble.updatePhy(connectionId, EspBle::Phy2MMask, EspBle::Phy2MMask) ? 1 : 0);
    }
    else if (command == 'd')
    {
      ble.disconnect(connectionId);
    }
  }

  ble.update();
  delay(1);
}
