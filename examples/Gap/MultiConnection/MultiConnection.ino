// en: One Central holding several Peripheral connections at once. Each has its
//     own connection ID, and every operation names the connection it applies
//     to — nothing in this library is implicitly "the current connection".
// ja: 1つのCentralが複数のPeripheral接続を同時に保持する例。接続ごとにIDがあり、
//     すべての操作は対象の接続を明示する。このlibraryに「現在の接続」という暗黙の
//     対象は無い。
#include <EspBle.h>

EspBle ble;

// en: The peripherals this sketch collects. Three is the limit on the original
//     ESP32's bundled host; other targets allow more.
// ja: このsketchが集める接続。無印ESP32の同梱hostでは上限3で、他のtargetは
//     もっと多く扱える。
static constexpr size_t MaximumPeers = 3;
EspBleConnectionId peers[MaximumPeers] = {};
size_t peerCount = 0;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Multi Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    if (peerCount < MaximumPeers) peers[peerCount++] = connection.id;
    Serial.printf("Connected %s as id %u (%u held)\n",
      connection.peerAddress.c_str(), static_cast<unsigned>(connection.id),
      static_cast<unsigned>(ble.connectionCount()));
    // en: More peers may be waiting, so scanning resumes rather than stopping
    //     at the first one.
    // ja: まだ他のpeerが待っている可能性があるため、最初の1台で止めずにscanを
    //     再開する。
    if (peerCount < MaximumPeers) ble.scanner().start();
  });

  ble.onDisconnected([](const EspBleConnection &connection) {
    // en: An ID belongs to one connection for its whole life, so removing the
    //     right entry is a matter of matching it — a later connection to the
    //     same address gets a different ID.
    // ja: IDは1つの接続に一生対応するので、一致するものを外せばよい。同じaddressへ
    //     後から接続しても別のIDになる。
    for (size_t index = 0; index < peerCount; ++index)
    {
      if (peers[index] != connection.id) continue;
      peers[index] = peers[--peerCount];
      break;
    }
    Serial.printf("Disconnected id %u (%u held)\n",
      static_cast<unsigned>(connection.id),
      static_cast<unsigned>(ble.connectionCount()));
    ble.scanner().start();
  });

  // en: Results of the reads below. One callback serves every connection, and
  //     the result names which one it came from — that is why the connection ID
  //     travels with it rather than being remembered by the caller.
  // ja: 下のreadの結果はここへ届く。callbackは全接続で共通で、結果がどの接続の
  //     ものかを持っている。だから呼び出し側が覚えておく必要がない。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf("id %u battery read success=%u value=%u\n",
      static_cast<unsigned>(result.connectionId), result.success ? 1 : 0,
      result.value.length() > 0 ? static_cast<uint8_t>(result.value[0]) : 0);
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    // en: Only devices offering the test service are collected, and only while
    //     there is room. Without the second check a full Central keeps trying
    //     connections the host will refuse.
    // ja: test serviceを公開する機器だけを、空きがあるときだけ集める。後者の
    //     確認が無いと、満杯のCentralがhostに拒否される接続を試み続ける。
    if (!result.connectable || peerCount >= MaximumPeers) return;
    if (!result.advertisesService("180f")) return;
    ble.scanner().stop();
    ble.connect(result);
  });
  ble.scanner().start();

  Serial.println("Collecting up to three peripherals. Send 'r' or 'd'.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'r')
    {
      // en: Reading from every peer in turn. The connection ID is part of the
      //     call, so there is no ambiguity about which peer answered.
      // ja: 各peerへ順に読み出す。接続IDが呼び出しの一部なので、どのpeerが
      //     答えたか曖昧にならない。
      for (size_t index = 0; index < peerCount; ++index)
        ble.readCharacteristic(peers[index], "180f", "2a19");
    }
    else if (command == 'd' && peerCount > 0)
    {
      ble.disconnect(peers[0]);
    }
  }

  ble.update();
  delay(1);
}
