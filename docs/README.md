# Design Docs

`docs/` は設計文書の置き場です。`README.md` はエンドユーザー向け説明に留め、この配下で設計判断を整理します。

## 構成

### `foundation/`

プロジェクト全体で共有する前提、要件、全体アーキテクチャを置く。

- `foundation/requirements.md`: スコープ、必須要件、受け入れ観点
- `foundation/architecture.md`: 全体像、責務分割、技術論点
- `foundation/build-and-distribution.md`: 開発環境、ビルド導線、配布モデル

### `subsystems/`

BLE、USB、Recovery UI など、機能単位の詳細設計を置く。

- `subsystems/ble.md`
- `subsystems/usb.md`
- `subsystems/recovery-ui.md`

### `cross-cutting/`

状態遷移や永続化のように、複数サブシステムをまたぐ設計を置く。

- `cross-cutting/state-machine.md`
- `cross-cutting/persistence.md`

### `validation/`

試作、検証観点、合否基準、結果記録の入口を置く。

- `validation/plan.md`
- `validation/debugging.md`

### `adr/`

採用判断や比較結果を ADR として残す。

- `adr/0001-platform-selection.md`
- `adr/0002-ble-identification-strategy.md`
- `adr/0003-usb-hid-minimum-descriptor.md`
- `adr/0004-ble-stack-baseline.md`
- `adr/0005-build-and-distribution-workflow.md`
- `adr/0006-zephyr-upstream-and-version-pin.md`
- `adr/0007-artifact-and-debug-console-policy.md`

## 運用ルール

- 全体前提は `foundation/` に書く
- 詳細設計は `subsystems/` か `cross-cutting/` に分ける
- 「なぜそれを選んだか」は本文ではなく ADR に残す
- 「まだ決めていないこと」は `Open Questions` と `Validation Needed` に明示する
- 試作コードでしか確かめられない論点は `validation/` で管理する
