# Debugging

## Goal

- `Seeed XIAO nRF52840` へ `UF2` を書いた後でも、開発者が追加の debug probe なしで実機状態を追える導線を定義する
- `ZMK USB logging` に近い `USB CDC ACM` ベースの開発体験を第一候補にする

## Primary Path: USB CDC ACM Logging

- 第一候補は `USB HID + USB CDC ACM` の複合 device とする
- `HID` は引き続きホスト評価に使い、追加の `CDC ACM` を log/console 専用 COM port として使う
- build では local snippet `zub-usb-logging` を使う
- 運用感は `ZMK` の `zmk-usb-logging` snippet と同様に、workspace 直下 `scripts/build_zmk_usb_bridge.sh --profile dev-usb-logging` で USB logging を有効化する方式に寄せる
- Windows での host-side log capture は workspace 直下 `scripts/start_zmk_usb_bridge_session_windows.ps1` を第一候補にする
- script は repository 追跡外の作業用 helper として置き、`ExecutionPolicy Bypass` 付きで起動する

## Why This Is The Primary Path

- 開発者が `J-Link` や `SWD probe` を持っていなくても運用できる
- Windows の `COM port` 監視だけで `LOG_INF/WRN/ERR` を読める
- `UF2` による書き込み導線をそのまま維持できる
- `ZMK USB logging` と近い運用にできる

## Build

- 通常 build では local build script の `dev-usb-logging profile` を使う
- `config/user.conf` は application 側で既定マージされる

```sh
cd /home/terunet/work/Dev_zmk_usb_bridge
./scripts/build_zmk_usb_bridge.sh --profile dev-usb-logging
```

## What The Snippet Changes

- `CDC ACM` の仮想 UART を 1 本追加する
- console / shell / bt monitor の出力先をその `CDC ACM` に切り替える
- `LOG_BUFFER_SIZE` と `LOG_PROCESS_THREAD_STARTUP_DELAY_MS` を増やして、monitor 接続前後でもログを拾いやすくする
- `USB HID-only release build` との識別のため、USB PID を `0x0012` に切り替える

## Flash

- 書き込みは通常どおり `zephyr.uf2` を `XIAO BLE` mass storage へコピーしてよい
- `UF2` 書き込み後にホストへ再接続すると、`HID` に加えて `CDC ACM` の `COM port` が見える想定

## Preferred Host Tool

- Windows PowerShell から workspace 直下 `scripts/start_zmk_usb_bridge_session_windows.ps1` を呼ぶ
- `\\wsl.localhost\...` 経由の script 実行では security warning が出るため、`ExecutionPolicy Bypass` を付ける
- script は `COM port` の open、log 保存、summary 生成までまとめて扱う
- 実行結果は workspace の `artifacts/test_runs/<RunId>/` に保存する

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "\\wsl.localhost\<distro>\...\scripts\start_zmk_usb_bridge_session_windows.ps1" -ComPort COM12
```

### Optional Helpers

- `COM port` が未確定なら `-ListPorts` を先に使う
- `COM port` が固定なら `-ComPort COMx` を明示して使う
- 早い boot log を見たい場合は、script の `Monitoring started.` を確認してから board を reset する

## Log Capture Procedure

1. `dev-usb-logging profile` を build し、artifact から `UF2` を取り出して書き込む
2. 実機を USB ホストへ接続する
3. 必要なら Windows の `Device Manager` または script の `-ListPorts` で `COMx` を確認する
4. Windows PowerShell から `ExecutionPolicy Bypass` 付きで `start_zmk_usb_bridge_session_windows.ps1` を起動する
5. 早い boot log を見たい場合は、script の `Monitoring started.` を確認してから board を reset する
6. 以後の `LOG_INF/WRN/ERR` と console 出力を terminal 上で観測する
7. 停止後は `artifacts/test_runs/<RunId>/` の `serial.log` と `summary.json` を必要に応じて共有する

## Recommended Workflow

- 当面の実運用では `start_zmk_usb_bridge_session_windows.ps1` を標準導線にする
- build / flash / 実機操作は手元で行い、build は `scripts/build_zmk_usb_bridge.sh` に統一したうえで必要なログだけを Codex に戻して解析する
- script 側で `serial.log` と `summary.json` まで揃うので、run artifact をそのまま共有しやすい
- `ExecutionPolicy Bypass` を command line 側で明示することで、`\\wsl.localhost\...` 実行時の手動応答を避ける

### Script-Based Reset Workflow

1. Windows PowerShell から `ExecutionPolicy Bypass` 付きで script を起動する
2. `Monitoring started.` を確認してから board を手元で reset する
3. session を止めるまで terminal 上で log を確認する
4. `artifacts/test_runs/<RunId>/` の出力を必要に応じて Codex に渡して解析する

## Important Notes

- 第一候補では、`USB COM port` を script 経由で監視すればよい
- ただしこれは `release` の USB descriptor と同一ではなく、`CDC ACM` を追加した開発専用構成になる
- Windows での最終 HID 列挙評価は、`release` の `HID-only` build でも別途確認する
- `ZMK` と同様に、早期 boot log が必要なら startup delay をさらに延ばす余地がある
- workspace 直下 `scripts/` は repository 追跡対象ではない前提で運用する

## Secondary Path: SWD + RTT

- `J-Link` 等を持つ場合は `config/dev-rtt.conf` による `RTT` も補助候補として残す
- ただし現時点では `USB CDC ACM logging` を主導線とする
