# ADR 0004: BLE Stack Baseline

## Status

Accepted for prototyping

## Context

- MVP の試作基盤は `ESP32-S3`、具体的には `Seeed XIAO ESP32-S3` を前提にしている
- BLE central / observer と USB device の両立が必要
- bond 主体の known-device reconnect、privacy、scan 継続中の connect attempt が必要
- `private address` や `directed advertisement` の解決は、可能な限り stack / controller に委ねたい

## Decision

- MVP の BLE stack 第一案は `ESP-IDF NimBLE host + ESP controller` とする
- 必須機能の前提は以下とする
- `Central` と `Observer` role を有効にする
- bond は NimBLE の永続化機構で保持する
- `CONFIG_BT_NIMBLE_HOST_ALLOW_CONNECT_WITH_SCAN` を有効にする
- `CONFIG_BT_NIMBLE_ENABLE_CONN_REATTEMPT` を有効にする
- `CONFIG_BT_NIMBLE_MAX_CONN_REATTEMPT` は暫定で `3` とする
- privacy 関連 API を有効にする
- host-based privacy より controller 側 privacy 解決を優先候補にする
- resolving list / accept list は `scan / connect 中に自由更新できる` 前提を置かず、初期化時または bond 更新直後に反映する

## Options Considered

- `ESP-IDF NimBLE host + ESP controller`
- `ESP-IDF Bluedroid`
- 代替 MCU / 別 stack

## Evaluation Criteria

- BLE central 実装の成立性
- privacy / bond-aware reconnect の扱いやすさ
- scan 継続中の connect attempt の実現性
- `ESP32-S3` 上での試作容易性

## Consequences

- `ESP32-S3` 前提のまま、bond 主体 reconnect と privacy の設計を具体 API / Kconfig に落としやすくなる
- `CONFIG_BT_NIMBLE_HOST_ALLOW_CONNECT_WITH_SCAN` と connection reattempt 設定を使う前提で、現行の reconnect 方針と整合を取りやすい
- privacy 解決を application 独自ロジックで吸収せずに済む可能性が高い
- 一方で、target stack が application へどこまで identity 解決済み情報を渡すか、accept list / resolving list をどこまで素直に使えるかは試作確認が残る
- これが満たせない場合は、MVP の reconnect 実装か stack 選定の見直しが必要になる

## Follow-up

- BLE スパイクで、privacy / directed advertisement / accept list / reconnect 実装可否を確認する
