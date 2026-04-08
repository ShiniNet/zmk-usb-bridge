# Prompt Templates

## 1. Design Review

```text
zmk-usb-bridge の設計レビューをしてください。

前提:
- 参照文書: docs/foundation/requirements.md, docs/foundation/architecture.md
- 対象: <対象ファイル or 対象設計>
- 変えてはいけないこと: <制約>

見てほしい点:
- requirements との整合
- 将来の実装難所
- テストしにくい箇所
- 文書更新が必要な箇所
```

## 2. Implementation Task

```text
zmk-usb-bridge に次の変更を実装してください。

目的:
- <やりたいこと>

制約:
- board: seeeduino_xiao_ble
- profile: release / dev-usb-logging
- 参照文書: <関連 doc>
- commit/push はしない

受け入れ条件:
- <build or test 条件>
- 必要なら docs も更新する
```

## 3. Debug From Serial Log

```text
dev-usb-logging build の実機ログを解析してください。

現象:
- <何が起きたか>

入力:
- serial.log
- summary.json
- 関連 build.log

期待する出力:
- 原因候補を優先順位付きで列挙
- 次に取るべきログ or 実験を提案
- 必要なら修正差分を作る
```

## 4. Add A ZTEST

```text
この変更に対応する ZTEST を追加してください。

対象:
- <対象ロジック>

制約:
- まずは実機非依存ロジックを優先
- test suite は tests/ 配下に置く
- workspace script から回せる形にする

確認:
- どの board で実行するかも提示する
```

## 5. Validation Runbook

```text
次の検証を実機で回すための runbook を作ってください。

検証したいこと:
- <仮説>

環境:
- keyboard: <機種>
- board: seeeduino_xiao_ble
- host OS: Windows

ほしい出力:
- 手順
- pass criteria
- 取るべきログ
- docs/validation に残すべき結果テンプレート
```
