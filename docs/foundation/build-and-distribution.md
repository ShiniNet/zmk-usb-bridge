# Build and Distribution

## Goal

- ローカル試作と GitHub Actions ベース self-build の両立方針を定義する
- build 入力、利用者設定、artifact の正本を明確にする

## MVP Baseline

- `zmk-usb-bridge` は独立した `Zephyr upstream` application として扱う
- workspace 正本は repository 同梱の `west.yml` とする
- 試作ボードは `Seeed XIAO nRF52840`
- `Zephyr upstream` は `v4.3.0` に pin する
- 開発の主経路は `west build` とする
- 利用者配布は `fork -> GitHub Actions -> artifact download -> local flashing` を標準導線にする
- 利用者設定は repository 追跡下の設定フラグメントで扱う
- 中央 repository から恒久 firmware release を配る前提は置かない

## Local Workflow

- source tree は Zephyr application として管理する
- build は `west build -b xiao_ble/nrf52840` を主経路にする
- 初回依存取得は `west init -l ShiniNet/zmk-usb-bridge && west update` を第一候補にする
- `UF2` 書き込みを試作主導線とし、必要に応じて外部 debug probe による `west flash` を補助導線にする
- bring-up で詳細診断が必要な場合は、SWD probe を使った `J-Link` 系デバッグを許容する

## Version Policy

- ローカルと GitHub Actions は同じ `Zephyr upstream v4.3.0` を使う
- `latest` や working draft ではなく、supported release のタグ固定を使う
- bugfix 取り込みは `v4.3.x -> v4.3.y` の明示更新で扱う
- version 更新時は、文書、ローカル手順、CI 設定を同時に更新する

## Build Profiles

- `release profile` は利用者向け artifact の正本とする
- `release profile` では開発専用 console を必須にしない
- `dev profile` は bring-up と調査のための補助構成とする
- `dev profile` では USB CDC ACM や追加ログを許容する
- `release profile` では USB HID の提示形態を優先し、不要な debug interface を減らす

## Configuration Model

### Fragment Roles

- `prj.conf`: project 共通既定値
- `boards/<board>_<qualifiers>.conf`: board 固有既定値
- `boards/<board>_<qualifiers>.overlay`: board 固有の devicetree 差分
- `config/user.conf`: upstream が最小デフォルトを保持し、利用者が fork 上で編集する設定
- `config/ci.conf`: CI 実行都合の差分だけを持つ設定

### Composition Rules

- build 入力の正本は `prj.conf`、board conf、overlay、user conf とする
- 標準順序は `prj.conf -> boards/xiao_ble_nrf52840.conf -> config/user.conf -> config/ci.conf` を第一候補にする
- 利用者意味論を変える設定は `config/ci.conf` に置かない
- CI でもローカルと同じ設定入力を使い、workflow 側で別の正本を増やさない

## User Configuration

- 利用者が直接編集するのは `config/user.conf` のような単一ファイルを第一候補にする
- `menuconfig` や workflow input を利用者設定の正本にしない
- 初回 pairing 用 `local name allowlist` は user config から設定可能にする
- user config は commit され、build 入力として履歴追跡できる状態を保つ

### Initial User-Facing Settings

- `CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST_ENABLED`
- `CONFIG_ZMK_USB_BRIDGE_PAIRING_NAME_ALLOWLIST`

### Current Constraints

- allowlist は `comma-separated exact match`
- 名前ごとの長さと件数には実装上限がある
- allowlist は `初回 pairing の補助条件` にのみ使い、bond 後の識別正本には使わない

## CI / Artifact Model

- build は upstream repository または利用者 fork 上の GitHub Actions で実行する
- 配布単位の第一候補は `workflow artifact`
- 標準 artifact は `release profile` の build 結果を格納する
- `Seeed XIAO nRF52840` 向けの主導線は `zephyr.uf2` とする
- 補助導線として `zephyr.hex` と build metadata を含める
- 外部 debug probe 利用者向けには `west flash` 前提の補助手順を許容する

## Open Questions

- artifact に利用者向け簡易書き込み手順書を同梱するか
- `release profile` と `dev profile` の差分をどこまで持つか
- `UF2` を主導線にしたとき、量産向け custom board artifact をどう並列管理するか

## Validation Needed

- `west build` が日常開発経路として安定するか
- `config/user.conf` 編集だけで allowlist 変更が完結するか
- fork 上 GitHub Actions artifact だけで利用者が self-build できるか
- `zephyr.uf2` 主導線と `zephyr.hex` 補助導線の二本立てが有効か
- `dev profile` の USB CDC ACM が bring-up 導線として十分成立するか
- `release profile` と `dev profile` の責務分離が運用上破綻しないか

## Related Documents

- `docs/foundation/requirements.md`
- `docs/foundation/architecture.md`
- `docs/adr/0005-build-and-distribution-workflow.md`
- `docs/adr/0006-zephyr-upstream-and-version-pin.md`
- `docs/adr/0007-artifact-and-debug-console-policy.md`
