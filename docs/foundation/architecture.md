# Architecture

## この文書の役割

- システム全体の責務分割と境界を示す
- 各サブシステムの詳細は専用文書へ分離する
- 実装前の主要論点を一覧できる入口にする

## 想定アーキテクチャ

初期案では、ドングルは以下の役割を持つ。

- キーボード側には BLE central / GATT client / HID host として動作する
- PC 側には USB HID デバイスとして動作する
- 単一の既知キーボードとの 1:1 接続を前提とする
- 常時給電の USB ドングルとして運用する
- 初期 MVP では `LaLapadGen2` を参照機として扱う
- 実装基盤は `nRF52840 + ZMK v0.3.x 系 zephyr v3.5.0+zmk-fixes` とする

## データフロー

1. ドングル起動
2. USB HID デバイスとして初期化を進める
3. 保存済み bond 情報と app metadata を読み出す
4. bond が無ければ自動でペアリング探索へ入る
5. bond があれば既知の対象キーボードを探索し接続する
6. キーボード入力とポインティング入力を受け取る
7. USB HID レポートへ変換して PC に送る
8. 切断時は USB 側を安全状態に戻し、scan restart を伴う再接続試行を継続する

## コンポーネント

### BLE 側

- BLE coordinator (`ble_manager`)
- startup coordinator
- BLE runtime init / callback registration
- 探索対象を絞るフィルタ
- explicit scan 制御
- 接続と security の管理
- reconnect scheduling
- ペアリング / bond 管理
- HID 入力受信

### USB 側

- USB HID descriptor 定義
- キーボード入力レポート送出
- ポインティング入力レポート送出
- 切断時の全 release 処理

### ローカル UI

- ペアリング開始操作
- bond 初期化操作
- 状態表示

### 永続化

- BLE stack が管理する bond 情報
- app 側が管理する補助 metadata

## 実装基盤

現時点では `nRF52840` を採用する。
初期試作基板は `Seeed XIAO nRF52840` を基準にする。
ソフトウェア基盤は `ZMK v0.3.x` と整合する `zmkfirmware/zephyr v3.5.0+zmk-fixes` を build 基盤とする。

この判断の理由は以下の通り。

- ZMK と同じ Zephyr 系の設計前提へ寄せやすい
- `ShiniNet/zmk-config-LalaPadGen2` と同じ `seeeduino_xiao_ble` board 名を使って検証しやすい
- 本プロジェクトの主戦場である `BLE central + bond + privacy + reconnect` を扱いやすい
- USB Device を備えた `nRF52840` 上で BLE と USB を同一 SoC に収められる
- `Seeed XIAO nRF52840` を実験用ドングルとして流用しやすい
- 認証済みモジュール前提の量産構成へ展開しやすい

## 技術的な難所

難しさは、BLE や USB の個別要素より、両者の境界にある。

- ペアリング、bond 永続化、再接続の状態遷移設計
- 切断時や異常時の stuck key / stuck button 防止
- キーボード入力とポインティング入力の USB 提示方法
- ZMK 向けにどこまで探索条件や接続条件を絞るか
- 最小 UI でどこまで復旧しやすさを作れるか
- `Zephyr USB device stack` 上で descriptor と report serialization をどう構成するか

## 設計原則

- 汎用性よりも ZMK 向け最適化を優先する
- 接続安定性が確認できるまでは機能追加を抑える
- 物理部品の追加よりソフトウェア実装で吸収する
- 設定アプリより先に単体で運用できることを目指す
- 初期設計は `LaLapadGen2 + Windows` で成立することを優先し、その後に一般化を検討する
- core の状態遷移と bridge 契約は platform 依存 API から切り離す

## 直近で決めるべき事項

- USB HID descriptor をどう最小構成にするか
- BLE 再接続の `fast -> backoff` 移行条件と attempt 間隔をどう定めるか
- explicit scan 方式での `scan stop -> connect -> fail/disconnect で scan restart` をどこまで単純に実装するか
- private address / directed advertisement を含む候補判定をどこまで吸収するか
- `BLE stack 管理 bond` とアプリ補助メタデータの境界を実装でどう分離するか
- 1 ボタン + RGB LED の UI をどこまで単純化するか
- `Zephyr USB device stack` 上で `single HID interface + report IDs` を続行するか
- `release profile` と `dev profile` の境界を build 設計でどう分けるか

## 詳細設計の置き場

- BLE: `docs/subsystems/ble.md`
- BLE runtime 分割: `docs/subsystems/ble-runtime.md`
- USB: `docs/subsystems/usb.md`
- Recovery UI: `docs/subsystems/recovery-ui.md`
- 状態遷移: `docs/cross-cutting/state-machine.md`
- 永続化: `docs/cross-cutting/persistence.md`
- Build / Distribution: `docs/foundation/build-and-distribution.md`
- 検証計画: `docs/validation/plan.md`
