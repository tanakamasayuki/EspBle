# Gatt/CurrentTimeServer

標準Current Time Service（`0x1805`）とRead・Notify可能なCurrent Time
Characteristic（`0x2A2B`）を公開します。値は10-byteの標準wire形式です。
Serialから`t`を送るとデモ時刻を1秒進め、購読中Clientへ通知します。

[CurrentTimeClient](../CurrentTimeClient/)と組み合わせて使用できます。
