# ADR 0007: Artifact and Debug Console Policy

## Status

Accepted

## Context

- 利用者向けには簡潔な書き込み手順を用意したい
- 一方で、開発者向けには診断しやすい build 出力も残したい
- `Seeed XIAO nRF52840` は Zephyr 公式 docs で `UF2` 書き込み導線を持つ
- 初期 bring-up では追加の debug console が欲しいが、release artifact の USB HID 提示形態は崩したくない

## Decision

- GitHub Actions artifact には `zephyr.uf2` と `zephyr.hex` の両方を含める
- `zephyr.uf2` は利用者向け主導線とする
- `zephyr.hex` と build metadata は診断・保守向けの補助導線とする
- debug console は `dev profile` で USB CDC ACM を許容する
- Windows 実機 log capture の標準操作は workspace 直下の PowerShell helper script とする
- `\\wsl.localhost\...` 経由の helper script 実行では `ExecutionPolicy Bypass` を許容する
- 詳細なデバッグが必要な場合は外部 SWD probe を用いる
- `release profile` では不要な debug interface を減らす

## Consequences

- 利用者向け手順は短く保ちやすい
- 追加の probe が無くても試作導線を開始しやすい
- log 保存と summary 生成を host 側で揃えやすい
- 深い調査では SWD probe が前提になる
- `dev profile` と `release profile` の責務分離が必要になる

## Follow-up

- `docs/foundation/build-and-distribution.md` に詳細を集約する
- validation で artifact 構成と debug console 方針を確認する
