# AI Workflow

## Goal

- Codex と Claude Code を、設計支援と実装支援に強く使う
- ただし repository の方向性や最終判断は人間が握る

## Recommended Split

### AI に任せること

- 設計文書の読解と影響範囲整理
- 実装差分の作成
- build / ZTEST / 実機ログの解析
- runbook、補助 script、テンプレート整備

### 人間が握ること

- 要件変更の承認
- 実機接続やハード操作
- commit / push / merge
- 仕様変更の最終レビュー

## Best Practices

1. 依頼の最初に、対象文書と受け入れ条件を渡す
2. 実装依頼では、対象 board / profile / host OS を明示する
3. 変更後は build / test / 実機ログのどれで検証するかを必ず指定する
4. AI に仕様変更をさせたときは、関連文書まで更新させる
5. AI の作業単位は「1つの目的 + 1つの確認手段」に寄せる

## Minimum Context To Provide

- 目的: 何を直したいか、何を増やしたいか
- 制約: 何を変えてよくて、何は変えてはいけないか
- 対象: `board`, `profile`, `host OS`, 実機有無
- 受け入れ条件: build 成功、ZTEST 成功、ログ上の期待挙動など
- 参考: `docs/foundation/...`、関連 subsystem doc、関連 validation status

## Suggested Development Loop

1. `docs/foundation/requirements.md` と関連 doc を確認する
2. AI に影響範囲と変更案を出させる
3. AI に差分を実装させる
4. workspace script で build / test を回す
5. 実機ログを必要に応じて AI に渡して解析する
6. 結果を `docs/validation/` へ戻す

## Evidence Package For Debugging

AI にデバッグを任せるときは、なるべく次をまとめて渡す。

- 実行コマンド
- `artifacts/builds/.../debug/build.log`
- `artifacts/builds/.../debug/build_meta.json`
- `artifacts/test_runs/<RunId>/serial.log`
- `artifacts/test_runs/<RunId>/summary.json`
- 実機で見えた現象の時系列

## Anti-Patterns

- 設計文書を見せずに大きな方針変更をさせる
- build 成功条件なしで広い実装を依頼する
- 実機ログを断片だけ渡して推測させる
- AI に commit / push 判断まで委ねる
