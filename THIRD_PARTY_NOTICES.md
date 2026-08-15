# Third-party notices / 第三者コンポーネントのライセンス

EspBle's original code is licensed under the root [MIT License](LICENSE).
This distribution also contains the following third-party components under
their own licenses. Those components are not relicensed under MIT.

EspBle独自コードにはrootの[MIT License](LICENSE)が適用されます。この配布物には、
それぞれ上流のライセンスが適用される以下の第三者コンポーネントも含まれます。
第三者コンポーネントをMITへ再ライセンスするものではありません。

## Vendored NimBLE host source / 同梱NimBLE hostソース

The NimBLE host source bundled for the original ESP32 carries its license and
attribution files alongside the source:

- [License](src/nimble_esp32/LICENSE)
- [Notice and attributions](src/nimble_esp32/NOTICE)
- [Pinned upstream versions](src/nimble_esp32/VERSIONS)

無印ESP32向けに同梱するNimBLE hostソースのライセンス、帰属表示、固定した上流versionは、
ソースと同じディレクトリに収録しています。

## Precompiled Classic-only Bluedroid host / Classic専用Bluedroid host

`src/esp32/libespble_bluedroid_classic.a` is built from ESP-IDF v5.5.5 and
contains ESP-IDF Bluetooth code plus TinyCrypt-derived code. Its complete
notices, license texts, provenance and artifact hashes are stored next to the
archive:

- [Notice and modifications](src/esp32/NOTICE)
- [Artifact manifest](src/esp32/MANIFEST.json)
- [License texts](src/esp32/LICENSES/)

`src/esp32/libespble_bluedroid_classic.a`はESP-IDF v5.5.5から生成し、
ESP-IDFのBluetoothコードとTinyCrypt由来コードを含みます。必要な帰属表示、
ライセンス全文、変更内容、来歴、成果物hashはarchiveと同じディレクトリに収録しています。
