# Requirements

## この文書の役割

- プロジェクト全体の前提、スコープ、受け入れ観点を定義する
- サブシステム個別の詳細設計は `docs/subsystems/` と `docs/cross-cutting/` に分離する
- 採用判断の理由は `docs/adr/` に記録する

## 現時点の前提

- 無改造の ZMK BLE キーボードと接続できることが必須
- ドングル 1 本に対してキーボード 1 台の固定利用を基本とする
- 初期リリースでポインティング入力を含める
- BIOS / UEFI など低レイヤ環境での動作は初期必須ではない
- BOM コストを優先し、開発工数は優先順位に含めない
- 実装基盤は `nRF52840` 系と `Zephyr upstream` を前提にする

## MVP の具体的な対象

- 初期の参照キーボードは `LaLapadGen2`
- 試作の開発ボードは `Seeed XIAO nRF52840`
- ホスト OS は `Windows` を優先する
- 他の ZMK キーボードへの一般化は将来拡張として扱い、MVP の受け入れ条件はまず `LaLapadGen2` で満たす

## 初期リリースの必須要件

### 接続

- 単一の ZMK BLE キーボードとペアリングできる
- bond 情報を永続化できる
- 電源投入後に既知のキーボードへ自動再接続できる
- 接続失敗時に利用者が復旧操作へ移りやすいこと
- bond 情報が存在しない場合は自動でペアリング待ちに入れること
- 既知キーボードへの再接続は継続的に試行できること

### USB 側機能

- PC から USB HID デバイスとして認識される
- キーボード入力を USB 側へ安定して転送できる
- ポインティング入力を USB 側へ安定して転送できる
- 切断やエラー時に stuck key / stuck button を防げること

### 運用性

- 物理ボタンや LED などの最小 UI で、ペアリングと復旧が成立する
- 設定アプリなしでも基本運用ができる
- 常時給電運用で安定して動作できる
- 1 個の外部ボタンと 1 個の RGB LED を前提に操作体系を成立させる
- ボタン長押しで bond 削除と再ペアリングへ移れること

### コスト

- 小規模生産でも成立する BOM を維持する
- ハードウェア部品の追加よりもソフトウェアで吸収できる構成を優先候補とする
- 認証済みモジュール前提で実装可能なこと

## 初期リリースのスコープ外

- 複数キーボードの切り替え
- 汎用 BLE 周辺機器への広範対応
- デスクトップ設定アプリ
- 低レイヤ環境での完全互換性保証
- 高度なホスト切り替え機能
- 中央 repository からの恒久 firmware 配布を前提にした運用

## Build / Distribution Requirements

- 開発時は `west build` を主経路として成立させたい
- 利用者が `fork + GitHub Actions + artifact download + local flashing` の流れで self-build できること
- 利用者ごとの `pairing name allowlist` を repository 追跡下の設定ファイル編集だけで変更できること
- 利用者に `menuconfig` や IDE 固有設定を必須にしないこと
- build 入力が repository 上で追跡可能で、再現しやすいこと
- ローカルと CI で同じ `Zephyr upstream` version を使い、固定 version で再現可能にすること
- `Seeed XIAO nRF52840` 向けに、利用者が `UF2` を主導線として書き込めること

## 今後の試作で確定する項目

以下はまだ未確定で、今後の試作と評価で決める。

- 起動後に入力可能になるまでの目標時間
- 再接続完了までの目標時間
- 接続失敗時のリトライ戦略
- 継続再接続がホストや電力、熱、BLE 安定性へ与える影響
- bond 初期化や復旧操作に許容される手順数
- USB HID descriptor の最小構成
- 対応対象とみなす ZMK キーボードの条件
- ボタン長押し時間と LED 表示パターン
- user config の対象項目をどこまで初期公開するか
- `release profile` と `dev profile` の差分をどこまで持つか
- artifact へ利用者向け簡易書き込み手順書を同梱するか
- `Zephyr USB device stack` 上で `single HID interface + report IDs` をどこまで素直に実現できるか

## 受け入れ判定の観点

MVP の受け入れ判定では、少なくとも以下を見られる状態にしたい。

- キーボード入力が継続的に安定している
- ポインティング入力が実用的に扱える
- 接続断からの復帰が利用者にとって予測しやすい
- 状態表示だけで次の操作を判断できる
- 部品構成がコスト目標から大きく外れていない
- fork repository 上の GitHub Actions だけで build artifact を取得できる
- 利用者設定変更の手順が ZMK ユーザーにとって過度に重くない
- 認証済みモジュール前提で量産移行の障害が少ない

## 関連文書

- 全体像: `docs/foundation/architecture.md`
- Build / Distribution: `docs/foundation/build-and-distribution.md`
- BLE 詳細: `docs/subsystems/ble.md`
- USB 詳細: `docs/subsystems/usb.md`
- Recovery UI 詳細: `docs/subsystems/recovery-ui.md`
- 状態遷移: `docs/cross-cutting/state-machine.md`
- 永続化: `docs/cross-cutting/persistence.md`
- 検証計画: `docs/validation/plan.md`
