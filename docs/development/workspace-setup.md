# Workspace Setup

## Goal

- WSL2 + VSCode 上で `zmk-usb-bridge` を日常開発しやすい workspace を揃える
- build / test / 実機ログ取得を、AI と人間の両方が同じ入口から扱えるようにする

## Recommended Layout

この repository は west manifest repository として使う。
workspace root は repository の親 directory を想定する。

```text
<workspace>/
  .vscode/
  scripts/
  zmk-usb-bridge/
  .west/
  zephyr/
  modules/
  bootloader/
  tools/
  build/
  artifacts/
```

## Initial Setup

workspace root で次を実行する。

```sh
./scripts/bootstrap_wsl_ubuntu.sh
west init -l zmk-usb-bridge
west update --fetch-opt=--filter=tree:0
./scripts/install_zephyr_sdk.sh
west zephyr-export
```

## Daily Workflow

```sh
./scripts/build_zmk_usb_bridge.sh --profile release
./scripts/build_zmk_usb_bridge.sh --profile dev-usb-logging
./scripts/run_zmk_usb_bridge_ztests.sh --suite pairing_filter
```

- build artifact は `artifacts/builds/` に保存する
- ZTEST artifact は `artifacts/tests/` に保存する
- Windows 実機ログは `artifacts/test_runs/` に保存する
- Zephyr SDK は workspace 直下 `toolchains/zephyr-sdk-*` を既定場所にする

## VSCode Notes

- VSCode では workspace root を開く
- `.vscode/tasks.json` から build / west update / test を起動できるようにする
- `build/`、`artifacts/`、`zephyr/`、`modules/` は検索ノイズになりやすいので除外する

## Why This Layout

- `west init -l zmk-usb-bridge` と整合する
- repository を clean に保ちつつ、ローカル build 生成物を workspace 側へ逃がせる
- AI に「workspace root でこの script を回す」と渡せば再現しやすい
