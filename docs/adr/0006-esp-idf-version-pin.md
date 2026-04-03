# ADR 0006: ESP-IDF Version Pin

## Status

Accepted for prototyping

## Context

- ローカルと CI で別 version の `ESP-IDF` を使うと再現性が落ちる
- 2026-04-03 時点の stable documentation は `v5.5.3`
- pre-release より stable を優先したい

## Decision

- `ESP-IDF` は `v5.5.3` に pin する
- ローカルと GitHub Actions の両方で同じ `v5.5.3` を使う
- `release/v5.5` ではなく `v5.5.3` タグ固定を正本にする
- bugfix 更新は `v5.5.x -> v5.5.y` の明示更新で扱う

## Consequences

- build 不具合や BLE / USB 挙動差分の切り分けがしやすくなる
- `latest` 追従による意図しない breaking change を避けやすい
- 将来の更新時は、文書、ローカル手順、CI 設定をそろえて更新する必要がある

## Follow-up

- build 手順と GitHub Actions で `v5.5.3` を明示する
