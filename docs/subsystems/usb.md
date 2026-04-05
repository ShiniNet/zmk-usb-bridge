# USB

## Goal

- PC から USB HID デバイスとして安定認識される
- キーボード入力を USB 側へ実用的に橋渡しする
- consumer / ポインティング入力は存在する場合に橋渡しできるようにする

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
- USB descriptor の第一案は `Keyboard`、`Consumer Control`、`Mouse / Pointer` の 3 論理機能を含める
- MVP の接続成立条件として必須なのは `Keyboard` のみとし、`Consumer Control` と `Mouse / Pointer` は optional capability とする
- キーボード report は `HKRO 6-key`、Consumer Control は `basic`、Pointer は `相対移動 + ボタン 1-5 + 縦横スクロール` を第一案とする
- キーボード側で生成されたマウス系イベントは追加解釈せず、そのまま橋渡しする
- bond erase 後に再ペアリングへ戻っても、USB 側の提示形態は変えない
- boot protocol は MVP では必須にせず、将来追加の余地だけ残す
- 実装の第一案は `Zephyr USB device stack` 上で行う

## Current Spike Status

- `Seeed XIAO nRF52840` 向けに `Zephyr USB device stack` で最小列挙スパイクを実装済み
- 現在の descriptor は `single HID interface + report IDs` で `Keyboard(1) + Consumer(2) + Mouse(3)` を提示する
- Consumer Control は `16-bit single usage`、Pointer は `relative X/Y + wheel + AC Pan` を `8-bit` 幅で送る
- bridge 内部では mouse 値を `int16_t` で保持し、USB 送信時に飽和させる
- HOG mouse input は peer 実装差を許容し、`compact 5-byte` と `buttons + 4x s16le = 9-byte` の両方を受けられるようにする
- host 未構成時は report を破棄し、構成後に列挙確認と safe-state 送出を優先する
- HOG input は `HID ready` 以後だけ bridge へ通し、disconnect / bond erase / reset 後は stale notification を破棄する
- mouse の集約は `HOG input queue` 上で `連続かつ同一 buttons 状態` の report だけを対象にし、button 変化や keyboard / consumer をまたいで畳まない
- host 未構成中に要求された `all release` は pending として保持し、再構成後の最初の通常 report より先に flush する

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
- MVP の logical role では `keyboard` を必須とし、`consumer` と `mouse_input` は optional capability とする
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
- 現行実装では dedicated USB worker をまだ置かず、`HOG input worker` が queue から取り出す直前に contiguous な mouse report を集約する
- そのため集約範囲は `すでに queue に積まれている連続 mouse report` に限り、time-based flush や cross-role reordering は行わない

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
- HOG client reset 時点で bridge の入力受付を閉じ、message queue に残っていた旧 connection 由来の notification も USB へ渡さない
- USB host が未構成で即時送信できない場合は `all release` 要求を保持し、次に通常 report を送る前に safe-state を優先 flush する

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
- 現行の queue-drain 型 mouse 集約で十分か、専用 USB TX worker へ進めるか
- Consumer usage の許容範囲を `16-bit single usage` のまま固定するか、Windows 実測で allowlist を追加するか

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
- `consumer` や `pointer` を有効化した場合に、ボタン 1-5 と縦横スクロールが期待通り動作するか
- `pointer` を有効化した場合に、入力遅延や欠落が実用範囲か
- report ID 方式で Windows 側の相性問題が出ないか
- `single HID interface + report IDs` が Zephyr 実装で破綻しないか

## Related ADRs

- `docs/adr/0001-platform-selection.md`
- `docs/adr/0003-usb-hid-minimum-descriptor.md`
