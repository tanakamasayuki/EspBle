# HID Report Descriptorを書く

> English: [GUIDE_HID_DESCRIPTORS.md](GUIDE_HID_DESCRIPTORS.md)

このリポジトリのHID仕様書（[Device](HID_DEVICE_SPEC.ja.md) /
[Host](HID_HOST_SPEC.ja.md)）はEspBleが実装している内容を記述します。この文書はその実践編で、
**自分でReport Descriptorを書くとき、byteが何と揃っていなければならないか、そしてHostを疑う前に
どう確かめるか**を扱います。

Report DescriptorはHostがreportをどう解釈するかを決める唯一の情報です。間違えてもerrorは
出ません。Hostはdeviceを無視するか、fieldを違うoffsetで読んで妙な動きをします。だから
「確かめ方」の章は文法の章と同じくらい重要です。

## 1. どの経路を使うか決める

EspBleには3経路あり、最初に当てはまったものが正解です。

| 経路 | 使う場面 | 自分で書くもの |
|---|---|---|
| 固定profile——`hidKeyboard()`、`hidMouse()`、`hidConsumerControl()`、`hidSystemControl()`、`hidGamepad()` | deviceがそれらのどれかである | 何も書かない。descriptorはconfigureしたprofileから合成され、BLEとClassicで同じmoduleが生成する |
| `hidVendor()` | Hostに理解させるのではなく、自分のapplicationへの私的なdata経路が欲しい | 何も書かない。`EspBleHidVendorConfig::reportSize`（既定63）byteのvendor定義report |
| `hidCustom()` | Hostに理解させる必要があり、固定profileのどれでもない——dial、pedal、control panel、変わった複合device | 生のdescriptor byte列と、reportごとの`addInputReport()` / `addOutputReport()` / `addFeatureReport()`宣言 |

`hidCustom()`は固定profileと同じHID serviceへ合成されるので、custom reportをkeyboardの隣に
置けます。そこから2つの規則が出ます。

- **report IDは一意でなければならず**、固定profileを有効にしている間はその予約ID（1〜6）が
  profileのものになります。固定profileを使わないなら1〜6も使えます。
- **custom reportは最大4件**（`EspBleHidCustom::MaxReports`）。

`addInputReport()`で宣言した内容はdescriptorと一致していなければなりません。宣言は
characteristicの大きさと`sendInput()`の経路を決め、descriptorはHostが読むものです。両者を
照合してくれる仕組みはありません。

## 2. byteの構造

descriptorはitemの平坦な並びです。各itemはtagと後続data byte数を符号化したprefix byteで
始まるので、descriptorは厳密に左から右へ読まれ、**global itemは変更されるまで有効なまま**です。

実際に使うitem:

| byte | item | 役割 |
|---|---|---|
| `05 xx` / `06 xx xx` | Usage Page（1 byte / 2 byte） | 以降のusageがどの語彙から来るか。`01` generic desktop、`07` keyboard、`0C` consumer、`FF00`以降はvendor定義 |
| `09 xx` / `0A xx xx` | Usage | この操作対象が何か |
| `19 xx` / `29 xx` | Usage Minimum / Maximum | 連続したusageの範囲。array fieldや、修飾キーのようなbit fieldで使う |
| `A1 01` … `C0` | Collection (Application) … End Collection | すべてのtop-level deviceに必要な包み。`A1 00`はPhysical、`A1 02`はLogical |
| `85 xx` | Report ID | これ以降は次のReport IDまでそのreportに属する |
| `15 xx` / `25 xx`（2 byteは`16` / `26`） | Logical Minimum / Maximum | 1 fieldの数値範囲。**最小が負ならそのfieldは符号付き**になる（`Report Size` bitの2の補数） |
| `75 xx` | Report Size | 1 fieldのbit数 |
| `95 xx` | Report Count | そのsizeのfieldを何個 |
| `81 xx` | Input | ここまでに宣言したfieldを出力する。data bitは`02`がData,Variable,Absolute（値）、constant bit付きの`00` / `01`がpadding、Arrayは押されているusageの一覧 |
| `91 xx` | Output | 同じくHost→device |
| `B1 xx` | Feature | 同じく、dataではなく設定 |

すべての`Input` / `Output` / `Feature` itemの前に、**usage page、usage（またはusage範囲）、
logical範囲、sizeとcount**が有効になっている必要があります。設定しなかったものは前の値を
保ちます——便利であると同時に、descriptorが意味不明にdecodeされる最大の原因です。

## 3. byteがreportへ並ぶ規則

- **fieldは宣言順に、byte内ではLSBから**詰まります。fieldはbyte境界を越えられます。paddingが
  勝手に入ることはありません。
- **reportは必ずbyte単位まで**constant Input itemでpaddingします。button 8個と4 bitのhatは
  12 bitなので、4 bitのconstantを足します。
- **符号付きfieldは`Report Size` bitちょうどの2の補数です。**Logical Minimum -127 / Maximum
  127、size 8のmouse deltaは、-1を`0xFF`として送ります。範囲を広げたいならlogical範囲だけで
  なくsizeも広げます。
- **bit fieldは`Report Size` 1のfieldを`Report Count`個**——usage範囲の各usageに1 bitずつ。
  修飾キーやgamepadのbuttonがこれです。
- **arrayは`Report Size` 8のfieldを`Report Count`個で、各fieldがusage codeを持ちます**
  ——keyboardが同時押しを最大6個報告する仕組みで、array内の順序に意味が無い理由でもあります。
- **hat switch**は、中央と方向をまとめてlogical範囲で表す1 fieldです。EspBleのgamepadは8 bit
  1 field、Logical Minimum 0 / Maximum 8で宣言します——`0`が中央、`1`〜`8`が上から時計回りの
  8方向（`ESP_BLE_HID_GAMEPAD_HAT_*`）です。あわせてphysical範囲0〜315と単位degreeを宣言して
  おり、これがHostに「数値ではなく方向」と解釈させます。

report IDの位置はtransportで違い、ここは誰でも一度は引っかかります。

| | BLE（HOGP） | Classic（HID over BR/EDR） |
|---|---|---|
| 電波上のreport ID | payloadに入らない——reportごとにcharacteristicが別 | **payloadの先頭byte** |
| `sendInput()`へ渡すもの | payloadだけ | payloadだけ。IDはlibraryが付ける |
| Host側callbackが受けるもの | payload | payload。raw viewではIDがbyte 0 |
| descriptorのfield offset | payload起点 | payload起点。つまりIDのbyteは**含まない** |

このずれは実際にこのlibraryにあった不具合です。Classic HID hostがreport IDを使うdeviceからの
reportをすべて捨てていました——transportはIDを先頭に付けるのに、descriptorのoffsetはそれを
数えないためです。自分でraw reportを解析するなら同じbyteに注意してください。

## 4. 実例を辿る

[`Hid/CustomDevice`](../examples/Hid/CustomDevice/)はvendor定義のcontrol panelです。入力は
符号付きdial差分とbutton bit、出力はLED状態。hexの塊ではなく、判断の列として読みます。

1. `06 00 FF` — vendor定義usage page。「自分のcontrol panel」を表す標準pageが無いためです。
   Hostはこれ単体では動作せず、自分のapplicationが解釈します。
2. `09 01` / `A1 01` — collectionのusage、そしてApplication collection。top-level deviceには
   必ず必要です。
3. `85 01` — Report ID 1。次のReport IDまでがこのreportです。
4. `15 81 25 7F` — logical範囲 -127〜127。**これがfieldを符号付きにします。**
5. `75 08 95 02` — 8 bit fieldを2個。
6. `09 02 81 02` — usage、そしてInput(Data,Variable,Absolute)。この2 byteを出力します。
7. `15 00 25 01 75 08 95 01` — 続いて0〜1の範囲、8 bit fieldを1個……
8. `09 03 91 02` — ……をOutputとして。Hostが書く1 byteです。
9. `C0` — End Collection。

そしてsketch側で`custom.setReportMap(...)`、`custom.addInputReport(1, 2)`、
`custom.addOutputReport(1, 1)`——同じreport ID、同じ大きさです。descriptorを変えて宣言を
忘れると、characteristicの大きさが合わずHostは短く読みます。

Hostが単体で解釈する標準usageの例としては、固定profileが出すgamepad descriptor
（`EspBleHidGamepadDescriptor`）を読む価値があります。Peer testがbyte列を固定しており、
上の技法を39 field・payload 11 byteで全部使っています。

- 符号付き8 bitの軸6本（X、Y、Z、Rz、Rx、Ry）が1つのLogical Minimum -127 / Maximum 127と
  1つのsize・countを共有する——usageを6個並べ、Input itemは1つだけ。
- 8 bitのhat switchが自分のlogical範囲・physical範囲・単位を持ち、その後で単位を0へ戻して
  次のfieldへ漏らさない。
- button 32個をusage範囲（`Usage Minimum 1`、`Usage Maximum 0x20`）の1 bit fieldとして
  ——4 byte。32 bitはbyte境界に揃うのでpaddingは要りません。

6 + 1 + 4がpayload 11 byteで、Classicでは先頭にreport ID 3が付くため、gamepadのPeer testが
判定している`id=3 len=12`になります。

## 5. Classic: 何が作れるかを決める予算

ClassicではdescriptorがSDP recordに載り、このrecordには厳しい予算があります——
**descriptorと`name` / `description` / `provider`の合計で214 byte**（300 byteのpadのうち86 byteを
recordの標準属性が使います）。超過する構成は`begin()`が`ResourceExhausted`で拒否します。
backendは以前これをlogに出すだけで登録は成功として返し、結果としてHostから見えないdeviceが
起動していました。

Classic deviceを設計するときの帰結:

- **文字列を短くすればdescriptorのbyteが買えます**（逆も同様）。同じ214 byteから出ています。
- **複合構成は意図して選ぶ必要があります。**既定の文字列ではkeyboard + mouse + consumerが
  収まり、gamepadを加えると収まりません。
- **Classic HID hostが復号するのはkeyboardとmouseだけです。**それ以外は`onInputReport()`へ
  生で届きます——手書きdescriptorを検証すべき場所はまさにここです。
- BLEに同等の制限はありません。Report Mapはcharacteristicで、通常の値と同じように読まれます。

## 6. 確かめ方

読んだだけのdescriptorを信用しないでください。安い順に3段階あります。

**まず机上でdecodeする。**byte列をHID descriptor decoder（USB-IFのHID Descriptor Tool、または
web上のdecoder）へ入れ、算出されるreport sizeが`addInputReport()`の宣言と一致するか見ます。
想定外の合計sizeが出るなら、descriptorと宣言はすでに食い違っています。

**2台目の基板で生byteを見る。**これはこのリポジトリがやっている方法で、作れる中で最も有用な
検証台です。1台をdevice、もう1台で`hidHost()`（BLE）またはClassic HID hostを動かし、raw report
をhexで表示してoffsetを判定します。Classicではkeyboardとmouse以外を見る唯一の方法でもあります。
`peer/classic_hid_gamepad`が真似すべき型です——軸の負値が符号付きのままか、hatとbutton bit
fieldがdescriptorの宣言位置にあるか、profile間でreport IDが混ざらないか、releaseで全byteが0へ
戻るかを見ています。

**最後に実際のHostで試す。**Linuxは接続したHID deviceをhidraw経由で見せるので、`hid-tools`の
`hid-recorder`でHostが受け取ったdescriptorと届くreportの両方を確認できます。usageの選び方が
妥当かどうかは、携帯やPCに聞くのが一番早く、byteをいくら検算しても分かりません。

**descriptorを変えたら必ず再ペアリングする。**HostはReport Mapをbondごとにキャッシュします——
Windows、Android、macOS、iOS、BlueZのいずれもpairing時に一度だけ解析し、その結果を使い続けます。
descriptorを変えた後は、device側のbond削除だけでは足りません。Host側でもdeviceを削除
（ペアリング解除、「登録解除」）してからpairし直さないと、Hostは新しいreportを古いlayoutで
読み続けます。「descriptorを直したら動かなくなった」は、ほぼ確実にこれです。

失敗の見え方:

| 症状 | ありがちな原因 |
|---|---|
| Hostがdeviceを完全に無視する | Application collectionが無い、またはそのdevice classにHostが認めないusage page / usage |
| fieldが違うoffsetで読まれる | paddingが無い、またはreport IDのbyteを数えている（Classic）／数えていない（BLE） |
| 値が両極に飛ぶ | 負値を送るのにfieldが符号なし（Logical Minimum 0）、または`Report Size`が狭い |
| あるbuttonが効かない | bit fieldのcountとusage範囲が食い違い、そのbitが別のusageに属している |
| 最初のreportしか届かない | Hostがそのreportのcharacteristicを購読していない（BLE）、または宣言とdescriptorでsizeが違う |
| descriptorを変えるまで動いていたのに、変えたらfieldが崩れた | Hostがpairing時にキャッシュしたReport Mapを使い続けている——Host側でペアリングを解除してpairし直す |
| `begin()`が`ResourceExhausted`を返す（Classic） | 214 byteのSDP予算 |

## 7. チェックリスト

- [ ] すべてのInput / Output / Feature itemの前に、usage page、usage、logical範囲、size、
      countが有効になっている。
- [ ] すべてのreportがbyte単位までpaddingされている。
- [ ] 符号付きfieldはLogical Minimumが負で、`Report Size`が範囲に足りている。
- [ ] report IDが一意で、固定profileを有効にしている間は1〜6を取っていない。
- [ ] `addInputReport()` / `addOutputReport()` / `addFeatureReport()`がdescriptorのIDと
      byte数に一致している。
- [ ] Classicでは、descriptorと3つの文字列の合計が214 byte以内である。
- [ ] 生byteを2台目の基板で観測した（頭の中だけで確かめていない）。

関連: [HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md)、
[HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md)、
[BLE入門ガイドのHIDの章](GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う)、
report長と合成の上限は[EspBleを深く使う](GUIDE_ADVANCED.ja.md)。
