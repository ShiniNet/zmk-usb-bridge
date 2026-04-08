# Contributing

このリポジトリは、設計文書を先に固めながら Zephyr / ZMK ベースの BLE-to-USB bridge を試作する前提で運用します。

## Workspace Layout

- west workspace root は manifest repository の親 directory を想定する
- repository は `zmk-usb-bridge/` に置く
- ローカル作業用 script は workspace 直下 `scripts/` に置く
- build 出力は workspace 直下 `build/` と `artifacts/` に集約する

例:

```text
<workspace>/
  .vscode/
  scripts/
  zmk-usb-bridge/
```

## First-Time Setup

workspace root で次を実行する。

```sh
./scripts/bootstrap_wsl_ubuntu.sh
west init -l zmk-usb-bridge
west update --fetch-opt=--filter=tree:0
./scripts/install_zephyr_sdk.sh
west zephyr-export
```

## Daily Commands

workspace root を前提に、日常導線は script に寄せる。

```sh
./scripts/build_zmk_usb_bridge.sh --profile release
./scripts/build_zmk_usb_bridge.sh --profile dev-usb-logging
./scripts/run_zmk_usb_bridge_ztests.sh --suite pairing_filter
```

Windows 実機ログ取得は PowerShell から次を使う。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "\\wsl.localhost\<distro>\...\scripts\start_zmk_usb_bridge_session_windows.ps1" -ComPort COM12
```

## Design-First Rules

- 仕様変更前に `docs/foundation/requirements.md` と関連 ADR を確認する
- 全体方針の変更は `foundation/`、機能詳細は `subsystems/` / `cross-cutting/` に反映する
- 実測や bring-up で判断した内容は `docs/validation/` に残す
- `README.md` は end-user 向け説明を維持し、設計議論を戻し過ぎない

## Implementation Rules

- 変更は requirements と current architecture を壊さない範囲で進める
- 実装タスクには必ず build か test の確認手順を付ける
- 実機依存のバグ修正では、可能なら ZTEST かロジック分離で再発防止を入れる
- 生成物やローカルキャッシュは repository ではなく workspace 側へ逃がす

## AI Collaboration

- Codex / Claude Code へ依頼するときは、対象文書、対象ファイル、受け入れ条件を最初に渡す
- AI には commit / push を任せず、差分作成と検証補助までに限定する
- AI へ実機ログを渡すときは `artifacts/test_runs/<RunId>/serial.log` と `summary.json` を基本セットにする
- 仕様に関わる変更を AI に依頼した場合は、どの文書を更新したかまで確認する

## Pull Request Readiness

人間が commit/push する前に、少なくとも次を確認する。

- `release` build が通る
- 必要なら `dev-usb-logging` build も通る
- 対応する ZTEST がある場合は実行する
- 設計差分があるなら関連文書が更新されている
