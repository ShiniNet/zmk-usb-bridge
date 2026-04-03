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

## External Interfaces

- PC ホストの USB HID stack
- BLE 入力受信層
- 状態遷移管理

## Presentation Model

- USB 側は 1 台の複合 HID デバイスとして見せる
- 内部構成の第一案は `single HID interface + report IDs` とする
- 少なくとも `Keyboard`、`Consumer Control`、`Mouse / Pointer` の論理機能を持つ
- Windows 優先のため、OS 標準の扱いに寄せた構成を採る
- BIOS / UEFI 向けの boot protocol 最適化は MVP の必須条件にしない
- ただし将来の boot protocol 追加を極端に難しくする構成は避ける

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

### LaLapadGen2 Bridge Baseline

- `Keyboard input = report id 1` を `zmk_hid_keyboard_report_body` 相当として受ける
- `Consumer input = report id 2` を `zmk_hid_consumer_report_body` 相当として受ける
- `Mouse input = report id 3` を `zmk_hid_mouse_report_body` 相当として受ける
- `Mouse input` の body は `buttons + d_x + d_y + d_scroll_y + d_scroll_x` の順を前提にする
- local config に HID override が無い限り、keyboard は `8 modifiers + reserved + 6 keys`、consumer は `basic usages x 6` の前提で bridge する
- `Mouse feature = report id 3 / feature` は smooth scrolling の解像度倍率用であり、MVP では USB report 変換対象にしない

### Internal State Model

- keyboard は `latest full-state report` として保持し、USB 送信時はそのまま再構成する
- consumer も `latest full-state report` として保持する
- mouse は `buttons state` と `movement/scroll delta accumulator` を分けて保持する
- BLE 側から mouse full-state body を受けても、USB 送信ポリシー上は `buttons` と `delta` の性質を分けて扱ってよい
- disconnect / bond erase / recovery 移行時は keyboard、consumer、mouse state をすべて safe state に戻す

### Translation Boundary

- BLE notification handler は report body の妥当性確認と internal state 更新だけを行う
- USB serializer は internal state から `report_id` 付き USB report を組み立てる
- report body の長さが期待値と一致しない場合は、その report を破棄して診断ログを残す
- unknown report id / report type / handle は USB 側へ流さず、bridge の状態を壊さない

## Transmission Policy

### Ordering Policy

- `Keyboard`: 順序厳守で送る
- `Consumer Control`: 基本は keyboard と同様に順序重視で送る
- `Mouse buttons`: press / release 順序を潰さずに送る
- `Mouse movement / wheel`: 鮮度重視とし、未送信分を加算して集約してよい

### Concurrency / Serialization

- USB IN endpoint への送信自体は 1 本ずつ直列化する
- ただし report 生成段階では、keyboard 系と pointer 系で性質の違う扱いを許容する
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
- `all release` 実行時は未送信の通常 report を安全側に倒して破棄または再構成してよい

### Flush Policy

- `Mouse movement / wheel` は固定タイマーを持たない
- 未送信分は次の USB 送信可能タイミングまで加算保持する
- `Mouse button` 状態変化時は、その時点の movement / wheel を含めて flush してよい
- BLE 切断、bond erase、再接続待機移行時は movement / wheel の保留分を 0 化して安全終了する

### Accumulation / Saturation

- movement / wheel の内部保留値は符号付き 16 bit の表現範囲を前提とする
- 加算時は wraparound させず、report field 上限で飽和させる
- overflow 分を精密に保持し続けるより、MVP では安全な飽和を優先する
- 成功送信後は movement / wheel の保留値を 0 に戻す

## State / Data

### 決定済み

- report 構成の正本は `Presentation Model` と `Report Mapping`
- 送信順序と集約の正本は `Transmission Policy`
- 異常時 safe state の正本は `Disconnect / Recovery Behavior`
- 追加の USB 専用 UI は MVP では増やさない

### 未決定

- 単一 interface 上での report 長差をどう管理するか
- 通常 report の破棄 / 再構成をどこまで許容するか

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

## Open Questions

- 単一 interface 上で report 長差を実装上どう扱うか
- `release_pending` を `keyboard release all` と `mouse safe state` の両方成功後に解除する実装で十分か
- 通常 report の破棄 / 再構成をどの境界で許可するか

## Validation Needed

- Windows で複合 HID として安定して認識されるか
- 切断時に stuck key / stuck button を防げるか
- Consumer Control の stuck を防げるか
- `HKRO` keyboard report が Windows で期待通り動作するか
- Consumer Control の `basic` usage 範囲が期待通り動作するか
- ボタン 1-5 と縦横スクロールが期待通り動作するか
- ポインティング入力の遅延や欠落が実用範囲か
- 再接続待機中や bond erase 後に不要な入力が残らないか
- report ID 方式で Windows 側の相性問題が出ないか
- movement / wheel 集約で操作感が悪化しないか
- `all release` 優先送信が実際に stuck input 防止に効くか
- 最優先フラグ方式で通常 report と競合したときに破綻しないか
- 飽和加算が実用上問題ないか

## Related ADRs

- `docs/adr/0001-platform-selection.md`
- `docs/adr/0003-usb-hid-minimum-descriptor.md`
