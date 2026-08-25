# PCMFlowBluetooth A2DP検証・修正依頼

更新日: 2026-08-11

> **完了:** PCMFlowBluetooth 1.0.0でdecoder reset修正、host回帰、無印ESP32 2台E2E、
> SBC encoderとM5Stack Core2 exampleが追加され、1.0.1でPCMFlow接続exampleも修正された。
> 結果は[PCMFlowBluetoothの完了報告](https://github.com/tanakamasayuki/PCMFlowBluetooth/blob/main/docs/A2DP_VALIDATION_REPORT.ja.md)を参照。
> 以下は依頼時点の再現条件と受け入れ基準を残した履歴文書である。

この文書は `../PCMFlowBluetooth` の担当作業への依頼書である。EspBle側では実機を使った再現と
境界確認だけを行い、PCMFlowBluetoothのsource、test、仕様書は変更しない。

## 確定した不具合: SBC decoder reset後にPCM履歴が残る

### 症状

同一の48 kHz stereo SBCベクタを次の順序で復号すると、フレーム数やerror counterは正常でも、
suspend前とresume後でPCM sample hashとpeakが変化する。

1. A2DP接続とcodec negotiationを完了する。
2. 944-byte、SBC 8-frameの既知ベクタを複数packet送信してPCMを取得する。
3. Sourceをsuspendし、同じ接続でstartする。
4. decoderへ同じベクタ列をもう一度渡す。
5. 切断・再接続後にも同じ操作を行う。

観測時は初回にpeak 32768 / hash `55228248`、resume後にpeak 12403 / hash `1832f219`となった。
drop、invalid frame、decode failure、PCM overflowは発生していない。このためA2DP transportの欠損ではなく、
decode stateの初期化不足と判断できる。

### 原因候補

vendored OI SBCの `OI_CODEC_SBC_DecoderReset()` はdecoder contextと外部scratch領域へのpointerを
再構築するが、scratch領域内のsynthesis filter buffer自体は初期化しない。
`SbcDecoder::reset()`でcontextだけを消去すると、前streamのfilter historyが次streamへ残る。
初回も `malloc()` の未初期化内容に依存する可能性がある。

### 修正依頼

- backend resetを呼ぶ前に、所有しているdecoder scratch領域全体をゼロ初期化する。
- `reset()`前後で同じSBC列を復号し、PCM sample列のhashが完全一致するhost回帰試験を追加する。
- peakやzero-crossingだけではsample差を検出できないため、sample単位の比較または安定したhashを使う。
- begin直後、suspend/resume、切断/再接続のいずれでも同じ初期状態になることを確認する。

修正候補を適用した技術probeでは、すべての開始条件でpeak 12403 / hash `e511d892`へ一致した。
この値そのものよりも、同一build・同一vectorで開始条件を跨いで一致することを契約とする。

## 実機E2E test追加依頼

無印ESP32を2台使うpytest-embedded fixtureをPCMFlowBluetooth側へ追加してほしい。

- DUT: `EspBleClassicA2dpSink` + `EspBleA2dpSinkAdapter`
- peer: `EspBleClassicA2dpSource`
- dependency: siblingのEspBleをlocal directory指定し、PCMFlowBluetooth自身もlocal sourceを使用
- build option: 不要（`EspBleClassic`利用時にEspBleが独自Classic hostを自動選択）
- input: 既存の事前生成済み48 kHz stereo SBC vector
- lifecycle: connect → start → decode → suspend → resume → decode → disconnect → reconnect → decode
- assert: negotiated PCM format、PCM frame数、sample hash、signal peak、packet/frame counter、全error counter
- codec-configuredとconnectedのcallback順序は固定しない。実機ではcodecが先に来る場合がある。

EspBleの実測media MTUは995 byteで、上記vectorの944-byte/8-frame payloadは1 packetとして送信できる。
media callback、control callback、PCM decodeの3境界をstubなしで通すことを目的とする。

## 未実装だが初期A2DP releaseを妨げない項目

- HFP Client / Audio Gateway用mSBC adapter
- HFP CVSD adapter
- PCMからA2DP Sourceへ送るSBC encoder adapter
- PCMFlowDevice等を接続したspeaker / microphone実出力
- 長時間連続受信時のheap、queue、callback latency測定

device I/OはPCMFlowBluetoothへ取り込まず、PCMSource / PCMSink境界の外側で検証する。
HFPとA2DP Sourceは独立phaseとして扱う。

## 完了報告に含めてほしいもの

- 修正内容と追加した回帰条件
- host test結果
- 無印ESP32 2台E2Eの結果と使用したArduino-ESP32 / EspBle revision
- PCM format、MTU、payload境界、drop/decode/overflowの観測値
- 残る制限と次phaseへ送る項目
