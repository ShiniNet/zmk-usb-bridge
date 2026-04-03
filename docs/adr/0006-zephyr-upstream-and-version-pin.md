# ADR 0006: Zephyr Upstream and Version Pin

## Status

Accepted

## Context

- `Zephyr upstream` と `nRF Connect SDK` のどちらを正本にするか決める必要がある
- ローカルと CI で別 version の `zephyr` を使うと再現性が落ちる
- `ShiniNet/zmk-config-LalaPadGen2` は `zmk v0.3.0` を使っており、その `app/west.yml` は `zmkfirmware/zephyr v3.5.0+zmk-fixes` を参照している
- `seeeduino_xiao_ble` の board 名もこの系統に揃えたい

## Decision

- software base の方針は `Zephyr upstream を正本にする` のままとする
- ただし実際の build 基盤としては、`ZMK v0.3.0` と整合する `zmkfirmware/zephyr v3.5.0+zmk-fixes` を pin する
- ローカルと GitHub Actions の両方で同じ `v3.5.0+zmk-fixes` を使う
- `latest` 追従ではなくタグ固定を正本にする
- bugfix 更新は明示更新で扱う

## Consequences

- `ShiniNet/zmk-config-LalaPadGen2` と board 名、Zephyr 系譜、周辺知見を揃えやすくなる
- build 不具合や BLE / USB 挙動差分の切り分けがしやすくなる
- `latest` 追従による意図しない breaking change を避けやすい
- `nRF Connect SDK` 特有の管理層を増やさずに済む
- 将来の更新時は、文書、ローカル手順、CI 設定をそろえて更新する必要がある

## Follow-up

- build 手順と GitHub Actions で `v3.5.0+zmk-fixes` を明示する
