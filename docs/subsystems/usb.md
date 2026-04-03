# USB

## Goal

- PC から USB HID デバイスとして安定認識される
- キーボード入力とポインティング入力を USB 側へ実用的に橋渡しする

## Scope

- USB HID descriptor
- Keyboard report
- Pointing report
- 切断時の全 release
- USB 側エラー時の扱い

## Responsibilities

- HID デバイス初期化
- レポート変換と送出
- 複数入力種別の取り扱い
- 異常時の stuck key / stuck button 防止
- report 種別ごとの送出順序と排他制御

## MVP Baseline

- Windows では `Keyboard + Mouse` の複合 USB HID デバイスとして提示する
- `Keyboard`、`Consumer Control`、`Mouse / Pointer` の 3 論理機能を扱う
- キーボード report は `HKRO 6-key`、Consumer Control は `basic`、Pointer は `相対移動 + ボタン 1-5 + 縦横スクロール` を前提にする
- キーボード側で生成されたマウス系イベントは追加解釈せず、そのまま橋渡しする
- bond erase 後に再ペアリングへ戻っても、USB 側の提示形態は変えない
- boot protocol は MVP では必須にせず、将来追加の余地だけ残す
- 実装の第一案は `Zephyr USB device stack` 上で行う

## Presentation Model

- USB 側は 1 台の複合 HID デバイスとして見せる
- 内部構成の第一案は `single HID interface + report IDs` とする
- 少なくとも `Keyboard`、`Consumer Control`、`Mouse / Pointer` の論理機能を持つ
- Windows 優先のため、OS 標準の扱いに寄せた構成を採る
- BIOS / UEFI 向けの boot protocol 最適化は MVP の必須条件にしない

## Report ID Layout

- `Report ID 1`: Keyboard
- `Report ID 2`: Consumer Control
- `Report ID 3`: Mouse / Pointer

この並びは、既存 ZMK の HID/HOG 実装パターンに寄せることで、橋側の変換と検証を単純化する意図を持つ。

## Report Mapping

### Bridge Input Contract

- BLE HOG から受ける notification payload は、`report reference` で識別された各 report の `body` 部分とみなす
- bridge 内部では `handle -> {report id, report type, logical role}` の table を持ち、payload 単体から role を復元する
- USB 送信用の内部表現では `report_id` を付け直した report struct へ再構成する
- MVP の logical role は `keyboard`、`consumer`、`mouse_input` の 3 種を必須とする
- `mouse_feature` と `led_output` は bridge の primary input path には含めない

### Keyboard

- BLE 側で受けたキーボード入力を、標準的な USB HID keyboard report に変換する
- 変換方針は `ZMK キーボードとして通常期待されるキー入力を崩さない` ことを優先する
- report format は、可能な限り既存 ZMK の `HKRO` keyboard report 構成に一致させる
- MVP の第一案は `8 modifier bits + reserved + 6 key array`
- host LED output や追加 feature report は MVP の必須対象にしない

### Consumer Control

- メディアキーなどの consumer usage は独立 report として扱う
- `basic` usage 範囲を前提に、標準的な ZMK キーボード利用で発生しうる consumer input を橋渡し対象に含める

### Pointer

- 相対 X/Y 移動を扱う
- ボタンは `1-5` を扱う
- 縦スクロールを扱う
- 横スクロールを扱う
- キーボード側でジェスチャがこれらのイベントへ変換されている前提で、ドングル側では追加解釈をしない
- 既存 ZMK の mouse report 構成に可能な限り寄せる

## Transmission Policy

### Ordering Policy

- `Keyboard`: 順序厳守で送る
- `Consumer Control`: 基本は keyboard と同様に順序重視で送る
- `Mouse buttons`: press / release 順序を潰さずに送る
- `Mouse movement / wheel`: 鮮度重視とし、未送信分を加算して集約してよい

### Concurrency / Serialization

- USB IN endpoint への送信自体は 1 本ずつ直列化する
- `Keyboard` と `Consumer Control` は最新上書きではなく、状態遷移を壊さないことを優先する
- `Mouse movement / wheel` は高頻度更新をそのまま全部送らず、次送信までに集約してよい
- `Mouse buttons` は movement 集約に巻き込まず、明示的な状態変化として扱う

### Priority Rules

- 通常時は `Keyboard` と `Consumer Control` の整合性を優先する
- `Mouse movement / wheel` は少数の送信に畳んでもよい
- 切断時や bond erase 時の `all release` は通常送信より優先する
- `all release` の優先化は専用キューではなく、最優先フラグ方式で扱う

### Disconnect / Recovery Behavior

- BLE 切断時は `release_pending` のような最優先フラグを立て、通常送信より先に `keyboard release all` を送る
- `consumer release all` も keyboard と同じ safe-state 手順に含める
- その後に `mouse button release all` と `zero movement / zero scroll` を送る
- bond erase 時も同様に USB 側を安全状態へ戻してから再ペアリングへ進む
- 再接続待機中に新しい入力を送らない

## Zephyr 実装境界

- descriptor 定義は USB 専用モジュールに閉じ込める
- report serializer は bridge core から `report body` を受け取り、USB stack 向けの送信バッファへ変換する
- HID report descriptor は Zephyr の HID 定義やサンプルを参考に構成する
- `single HID interface + report IDs` が Zephyr 実装都合で不自然な場合だけ、multi-interface を比較候補に上げる
- USB stack の切り替えや API 差分は core 層に漏らさない

## State / Data

### 決定済み

- report 構成の正本は `Presentation Model` と `Report Mapping`
- 送信順序と集約の正本は `Transmission Policy`
- 異常時 safe state の正本は `Disconnect / Recovery Behavior`
- 追加の USB 専用 UI は MVP では増やさない

### 未決定

- 単一 interface 上での report 長差をどう管理するか
- 通常 report の破棄 / 再構成をどこまで許容するか
- `Zephyr USB device stack` 上で composite HID をどう構成するか

## Failure Handling

- BLE 切断時の全 release 手順
- USB 側 enumeration 失敗時の扱い
- レポート欠落や重複送出の抑止
- BLE 切断や bond erase 時の safe state 遷移は `Disconnect / Recovery Behavior` を正本とする
- movement / wheel 集約によって pointer の体感遅延が増えすぎないこと

## Constraints

- BIOS / UEFI 完全対応は初期必須ではない
- 実装容易性より BOM コスト優先
- Windows での実用安定性を最優先する

## Validation Needed

- Windows で複合 HID として安定して認識されるか
- 切断時に stuck key / stuck button を防げるか
- Consumer Control の stuck を防げるか
- `HKRO` keyboard report が Windows で期待通り動作するか
- ボタン 1-5 と縦横スクロールが期待通り動作するか
- ポインティング入力の遅延や欠落が実用範囲か
- report ID 方式で Windows 側の相性問題が出ないか
- `single HID interface + report IDs` が Zephyr 実装で破綻しないか

## Related ADRs

- `docs/adr/0001-platform-selection.md`
- `docs/adr/0003-usb-hid-minimum-descriptor.md`
