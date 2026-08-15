# 無印ESP32 Classic Audio拡張計画

## 結論

EspBleの責務はBluetooth Classicのprofile、接続制御、codec negotiation、Bluetooth上を流れる
media payloadの受け渡しまでとする。PCMの加工、codecのencode/decode、ring buffer、resample、gain、
I2S/DAC/USB Audio/board speakerへの出力はEspBleへ入れない。

この境界は[PCMFlow](https://github.com/tanakamasayuki/PCMFlow)とEspUsbHostの既存方針に合わせる。
EspBleはPCMFlowへ依存せず、両者を接続する再利用可能な処理は独立したPCMFlow sibling libraryへ置く。
Bluetoothを使わないdeviceやPCMFlow利用者へEspBle依存を持ち込まない。

## 調査で確認したAPI境界

基準にしているESP-IDF v5.5.5では、A2DPのexternal codec APIはsinkへ未decodeのmedia frameを渡し、
sourceからencode済みmedia frameを送る。`esp_a2d_audio_buff_t`はdata、data length、encoded frame数、
timestampを持ち、受信bufferには明示的な解放規約がある。したがってA2DPの自然な境界はPCMではなく、
SBC等のencode済みframeとnegotiated codec configurationである。

HFPのVoice over HCI APIはHCI synchronous data packetのpayload、(e)SCO connection handle、bad-frame
情報を渡す。external codecを選ぶとCVSD/mSBCのencode/decodeはapplication側の責務になる。A2DP、HFP
ともESP-IDFのKconfigはinternal codecを将来削除予定とし、新規設計にはexternal codecを推奨している。

PCMFlow側の`PCMSource` / `PCMSink`はsample rate、channel数、bit depthを持つdecode済みPCM frameの
pull/push interfaceである。Bluetooth packetをそのまま渡すinterfaceではない。このためEspBleの公開APIを
PCMFlow型へ合わせず、codec adapterがBluetooth media frameとPCM frameを変換する。

EspUsbHostも同じ分離を採用している。core APIはUSB Audio固有のstream情報、raw input payload、format付き
output requestを公開し、PCMFlowを使うexampleだけが両libraryをincludeして`readFrames()`からUSB bufferへ
書き込む。EspBleでもこの依存方向を維持する。

## EspBleに含めるもの

- A2DP Sink / Sourceの初期化、接続、切断、stream状態
- negotiated codec capability/configuration、connection handle、media MTU
- A2DP Sinkのencode済みmedia frame callbackと所有権規約
- A2DP Sourceのencode済みmedia frame送信とbackpressure
- AVRCP Controller / Targetの操作、metadata、absolute volume等のcontrol event
- HFP Hands-Free / Audio Gatewayのservice-level connectionとcall control
- HFP Voice over HCIのSCO payload送受信、codec、frame quality情報
- profile停止時にcallbackが残らないlifetime barrier

## EspBleに含めないもの

- SBC、AAC、CVSD、mSBC等のencode/decode実装
- PCM ring buffer、sample rate変換、channel変換、gain、mixing
- I2S、DAC、USB Audio、filesystem、network、speaker/microphone固有処理
- PCMFlowへのcompile/link依存
- Bluetooth callback内での重いdecode、encode、device I/O

ESP-IDF由来のcodec実装をarchiveへ含めることで最初だけPCM callbackを作る案は採用しない。短期的には
簡単でも、将来削除予定のinternal codecへAPIと配布物を固定し、PCMFlowの分離方針とも逆になるためである。

## 公開APIの形

EspBle固有の小さな値型を定義し、Bluedroidの型とownershipをそのまま利用者へ露出しない。少なくとも
次の情報を表現する。

- profile role、connection ID、remote address、stream state
- codec IDとcodec-specific configuration bytes
- timestamp、encoded frame数、payload pointer/length、bad-frame flag
- 受信bufferのcallback内限定view、または明示release可能なowned buffer
- 送信bufferを消費したか、queue fullでretry可能かが分かる結果

受信の第一実装はcallback内限定viewを基本とし、保持が必要な利用者はcopyする。Bluedroid bufferの解放は
backendがcallback終了後に一度だけ行う。source送信はEspBleがcaller dataをqueueへcopyする方式から始め、
非同期ownership移譲は必要性とmemory効果を実測してから追加する。これにより公開APIから
`esp_a2d_audio_buff_free()`等を隠し、別hostへ置換可能な疎結合を保つ。

PCMFlow連携は、codec adapterがA2DP/HFP payloadをdecodeして`PCMSource`として提供するか、
`PCMSink`から受けたPCMをencodeしてEspBleへ送る形にする。既存のPCMFlowG711/G722/Opusも、packetを
decoderへpushして`PCMSource::readFrames()`で取り出し、encoderの`PCMSink::writeFrames()`からpacket
callbackを呼ぶ同じ形を採っている。PCMFlow coreへ新しい抽象を増やさず、この実績のあるinterfaceを再利用する。

利用者の構成を単純にする第一候補は、独立した`PCMFlowBluetooth` siblingを一つ追加する形である。このlibrary
だけがEspBleとPCMFlowへ依存し、標準SBC、mSBC、CVSDのcodec wrapperとA2DP/HFP adapterを提供する。
EspBle、PCMFlow core、PCMFlowDeviceは互いに変更なしで利用でき、sketchは概ねBluetooth source/sinkと
PCM source/sinkを接続するだけになる。codecを一種類ずつ別repositoryへ分ける案は再利用性は高いが、A2DPと
HFPを使う利用者の導入物が増えるため、最初の構成にはしない。

codec backendの第一候補はEspressif公式`esp_audio_codec`のSBCである。ESP-IDF v5.5.5のexternal codec
方針と整合し、標準SBCとmSBCのencode/decode、SBC decodeのPLCを提供する。Arduino libraryとして必要部分を
再現可能に同梱できるか、license、binary/source形式、ESP32でのheap/stack、toolchain ABIを技術検証してから
採否を決める。CVSDは同componentにないため別実装の選定が必要である。codec backendが確定するまで
`PCMFlowBluetooth`の公開APIを特定実装の型へ結び付けない。

## archive設定（完了）

Classic host archiveへ次を追加し、2026-08-11にclean生成とlink checkを完了した。

```text
CONFIG_BT_A2DP_ENABLE=y
CONFIG_BT_A2DP_USE_EXTERNAL_CODEC=y
CONFIG_BT_AVRCP_CT_COVER_ART_ENABLED=n
CONFIG_BT_HFP_ENABLE=y
CONFIG_BT_HFP_CLIENT_ENABLE=y
CONFIG_BT_HFP_AG_ENABLE=y
CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=y
CONFIG_BT_HFP_USE_EXTERNAL_CODEC=y
CONFIG_BT_HFP_WBS_ENABLE=y
```

A2DPが選択するAVRCPは有効にするが、最初のarchiveでは不要なcover art / GOEPを外す。HFP ClientとAGは
同じarchiveに含められるが、ESP-IDF v5.5.5の制約により同時実行はサポートしない。roleは起動時に排他選択する。

生成scriptのlink checkへA2DP Sink/Source、AVRCP CT/TG、HFP Client/AG、各external media APIの
必須symbolを追加する。archive更新後はsizeやsymbol総数を仕様値として固定せず、固定入力からのclean再生成、
格納物との一致、必須symbol、意図しないunprefixed symbolの有無をrelease gateにする。

## 実装順

1. **完了:** archiveのA2DP/AVRCP/HFP external codec設定とlink checkを追加し、clean再生成する。
2. **完了:** Classic-only A2DP Sinkの接続、codec negotiation、encoded SBC受信、停止を公開API化し、
   ESP32同士のexternal-codec転送で実機確認した。
3. **完了:** A2DP Sourceの固定SBC endpoint、接続、start/suspend、encoded SBC copy送信、
   `WouldBlock` backpressureを公開API化し、Sinkとの100 packet実機転送で確認した。
4. **基本操作完了・相互運用継続:** AVRCP CT/TG、passthrough、absolute volume、volume notification、
   Controller側のmetadata / play-status要求と応答eventを公開API化した。ESP32同士でPlayと音量を実機確認済み。
   ESP-IDF公開TG APIにmetadata / play-status応答送信がないため、その受信は外部Targetとの相互運用で確認する。
5. **完了:** HFP ClientをVoice over HCI / external codecで追加し、SLC、発信、call state、mSBC
   双方向raw payload、bad-frame、packet statistics、audio切断をESP32同士で確認した。
6. **完了:** HFP Audio Gatewayを同じtransport APIへ追加し、Client/AGのprocess-wide runtime排他、
   自動SLC応答、発信、call state、mSBC双方向payloadをESP32同士で検証した。AG設定でmSBC/CVSDを選択可能にし、
   CVSD双方向payload、SCO切断・再接続も確認した。
7. **A2DP実装済み・修正依頼中:** `PCMFlowBluetooth`にexternal SBC decoderとA2DP Sink adapterが実装された。
   実機probeでdecoder reset時のfilter履歴残留を検出したため、[修正依頼書](REQUEST_PCMFLOWBLUETOOTH_A2DP_VALIDATION.ja.md)に引き継いだ。
   mSBC/CVSDとHFP adapterは後続phaseとする。
8. **EspBle側完了・PCMFlowBluetooth側E2E依頼中:** EspBle側にA2DP Sink raw media exampleがある。
   PCMFlowBluetooth側のintegration fixtureとI2S/board speaker実例は各担当libraryで追加する。
9. **dual-host基本完了:** BLE GATT接続中のHFP SLC・発信・mSBC SCO双方向payloadに加え、A2DP SBC
   encode済みmedia転送とAVRCP Play / absolute volumeを確認した。SCO/stream中と切断後もGATT readは継続し、
   broker異常診断は0だった。音声固有の性能最適化と外部機器相互運用は後続課題とする。

AVRCPはA2DPより先に初期化する。Targetのvolume notificationはAVRCP規約どおりone-shotであり、
`Changed`受信後の再登録はController側applicationが行う。metadata文字列はcallback配送前にEspBle所有の
`String`へcopyし、Bluedroid callback bufferを公開しない。EspBleはmetadataを保持するplayer databaseや
UIを提供せず、PCMFlowBluetoothもAVRCPへ依存しない。

## HFP実装前調査

ESP-IDF v5.5.5のVoice over HCI / external codec APIでは、Client / AGともaudio state eventから
同期connection handle、CVSDまたはmSBC、推奨送信frame sizeを取得できる。受信callbackの
`esp_hf_audio_buff_t`はapplicationが必ず解放する所有権であり、EspBleはcallback中限定viewを配送して
復帰直後に一度だけ解放する。送信は専用allocatorへcopyしたbufferをAPI成功時にBluedroidが消費し、
失敗時だけEspBleが解放する。この境界はA2DPと同じ公開ownershipへ正規化できる。

HFP送信APIにはqueue-fullを返す仕組みがなく、受理後のcontroller側discardはpacket statisticsで後から
観測する。このためA2DPの`WouldBlock`と同じ意味を偽装せず、HFP sendは「Bluedroidへownershipを渡した」
結果とpacket statisticsを別々に公開する。mSBC受信は57 byte frameの末尾にpaddingを含む場合があるため、
EspBleはraw payloadを改変せず、PCMFlowBluetooth decoder側がframe長を解釈する。

ESP-IDFはHFP ClientとAGの同時実行をサポートしない。両roleは別classに分離するが、runtimeでは排他にし、
共通値型でSLC、audio state、call indicator、volume、AT response、同期payloadを表す。Clientの最小到達点は
SLC接続、着信/発信/応答/終了、audio接続、CVSD/mSBC双方向raw payload、切断である。AGは2台実機probe用の
応答とcall state modelを持たせるが、電話帳や電話網をEspBle内に実装しない。

公開AGは`EspBleClassicHfpAudioGateway`としてClientとは別classにする。接続・audio・raw payload・packet
statisticsはClientと同じ値型、copy送信、callback寿命を使う。AG固有部分は次の小さいtelephony境界に限定する。

- `begin(config)`のconfigにoperator名、subscriber番号、network/signal/roaming/batteryの初期値を持たせ、
  HFからのCIND/COPS/CNUM/indicator照会へAG backendが現在値を自動応答する。
- `onDialRequested`、`onAnswerRequested`、`onHangupRequested`、`onDtmf`、`onVoiceRecognitionRequested`を
  application eventとして`update()`から配送する。電話網の成功・失敗をEspBleが推測しない。
- applicationは`reportIncomingCall`、`reportOutgoingCall`、`reportCallActive`、`reportCallEnded`と
  `setNetworkStatus`で単一call modelを更新する。各操作は必要なAT最終応答とindicatorを一貫して送る。
- CLCCは保持中の単一call modelから自動応答する。複数call、三者通話、電話帳は初期scope外とし、将来必要なら
  model/provider interfaceを追加する。低水準Bluedroid列挙値や可変長C文字列を公開APIへ漏らさない。
- Client/AGはprocess-wide profile gateを共有し、一方の`begin()`中は他方を`InvalidState`で拒否する。
  `end()`はaudio callback barrier完了後にgateを解放する。これは同一`EspBleClassic` instance内だけでなく、
  backendのglobal callback slot全体を保護する。

2026-08-11のESP32同士のprobeではWBS/mSBC、同期handle 384、推奨送信frame 57 byteが選択された。
Clientから57 byteを送るとAG受信viewは58 byteとなり、追加byteは0、先頭57 byteのchecksumは同一だった。
AGが57 byteへ戻して送信するとClient受信viewは60 byteになった。無音・欠落時には60 byteの
`badFrame=true`も届く。したがってadapterはview全体を一つのmSBC frameと誤認せず、57 byte frameを
切り出して末尾paddingを捨て、`badFrame`時はpayload内容ではなくPLCへ進める。

同日のCVSD probeではAGの`preferredAudioCodec=Cvsd`から標準`+BAC/+BCS` negotiationを通し、両roleで
CVSD、推奨送信frame 120 byteが選択された。120-byte受信viewと双方向sendを確認し、SCO切断後に同一callのまま
再接続してもCVSDと120-byte frameが再設定された。さらにSLC全体を切断・再接続して再発信した場合も同じcodecと
transportへ復旧した。この値はESP32同士の観測値であり固定仕様にはせず、adapterは
接続eventのcodec、handle、`preferredFrameSize`が変わるたびにcodec stateとqueueをresetする。

## PCMFlowBluetoothへの引き継ぎ

`../PCMFlowBluetooth/SPEC.ja.md`を契約として初期releaseのA2DP Sinkを実装済みである。
公開責務、API、queue、thread、callback lifetime、SBC backend候補、完了条件を確定し、EspBleの実機probeで
codec configuration、encoded frame境界、buffer寿命、停止時callback保証を確認して反映した。HFPのSCO
payload境界も公開Client/AG実機probe後に追記済みで、mSBC/CVSD adapterを独立phaseとして実装開始できる。

2026-08-11にローカルEspBleの独自Classic hostを使う技術probeを行い、既知SBCのA2DP送信からPCM復号までを確認した。
その過程でOI SBC合成filter履歴のreset漏れを検出した。PCMFlowBluetooth側の変更は担当側で行うため、再現条件、
修正候補、host回帰、実機E2E要件を[専用依頼書](REQUEST_PCMFLOWBLUETOOTH_A2DP_VALIDATION.ja.md)へ分離した。

要件書には、EspBle / PCMFlowBluetooth / PCMFlow / device libraryの責務、公開classと依存方向、
SBC・mSBC・CVSD backendの選定とlicense、buffering/backpressure、heap/stack上限、thread/callback規約、
対応role、example構成、Arduino package方法、unit・実機integrationの完了条件を含める。device固有I/Oは
要件にも実装にも取り込まず、PCMSource / PCMSinkを介して既存libraryへ接続する。

最初の実機到達点はClassic-only A2DP Sinkで、接続、stream開始、SBC frame連続受信、切断、再接続、
callback停止保証を確認する。音がspeakerから出ることはEspBle単体の完了条件にせず、PCMFlowBluetoothと
PCMFlowのPCM境界までをintegration完了条件にする。実deviceでの再生・録音確認はPCMFlowDevice等との
end-to-end確認として別に行う。

## 配布形式の決定

次回のClassic拡張では、NimBLEはソース同梱、Classic Bluedroidは再現可能な名前空間化済み`.a`のままとする。
これは暫定放置ではなく、役割の違いに基づく意図的なmixed distributionである。

- NimBLEはEspBle側のpatchを追跡し、SoCごとのArduino build条件へ適用するためsourceが適する。
- BluedroidはESP-IDFの生成header、Kconfig、component依存が大きく、Arduino libraryとしてsource buildすると
  build時間と再現性を悪化させるためarchiveが適する。
- Classic archiveはIDF tag、toolchain、Kconfig、symbol namespace、link checkを固定して再生成可能にする。
- ESP-IDF componentへupstreamする際はarchiveそのものではなく、host分離、HCI broker、公開境界のsource差分を
  提案対象にする。

両者を同じ形式へ揃えること自体をrelease条件にはしない。将来再評価するのはArduino-ESP32/ESP-IDFのABI更新、
Bluedroid hostの独立component化、またはNimBLE archive化で明確な保守上の利益が出た場合に限定する。
