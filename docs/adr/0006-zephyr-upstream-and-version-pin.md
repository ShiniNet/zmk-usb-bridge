# ADR 0006: Zephyr Upstream and Version Pin

## Status

Accepted

## Context

- `Zephyr upstream` と `nRF Connect SDK` のどちらを正本にするか決める必要がある
- ローカルと CI で別 version の `Zephyr` を使うと再現性が落ちる
- 2026-04-03 時点の supported release では `Zephyr 4.3.0` が latest stable である
- working draft より stable release を優先したい

## Decision

- software base は `Zephyr upstream` とする
- `Zephyr upstream` は `v4.3.0` に pin する
- ローカルと GitHub Actions の両方で同じ `v4.3.0` を使う
- `latest` 追従ではなくタグ固定を正本にする
- bugfix 更新は `v4.3.x -> v4.3.y` の明示更新で扱う

## Consequences

- build 不具合や BLE / USB 挙動差分の切り分けがしやすくなる
- `latest` 追従による意図しない breaking change を避けやすい
- `nRF Connect SDK` 特有の管理層を増やさずに済む
- 将来の更新時は、文書、ローカル手順、CI 設定をそろえて更新する必要がある

## Follow-up

- build 手順と GitHub Actions で `v4.3.0` を明示する
