# Debugging

## Goal

- `Seeed XIAO nRF52840` へ `UF2` を書いた後でも、開発者が追加の debug probe なしで実機状態を追える導線を定義する
- `ZMK USB logging` に近い `USB CDC ACM` ベースの開発体験を第一候補にする

## Primary Path: USB CDC ACM Logging

- 第一候補は `USB HID + USB CDC ACM` の複合 device とする
- `HID` は引き続きホスト評価に使い、追加の `CDC ACM` を log/console 専用 COM port として使う
- build では local snippet `zub-usb-logging` を使う
- 運用感は `ZMK` の `zmk-usb-logging` snippet と同様に、`west build -S ...` で USB logging を有効化する方式に寄せる

## Why This Is The Primary Path

- 開発者が `J-Link` や `SWD probe` を持っていなくても運用できる
- Windows の `COM port` 監視だけで `LOG_INF/WRN/ERR` を読める
- `UF2` による書き込み導線をそのまま維持できる
- `ZMK USB logging` と近い運用にできる

## Build

- 通常 build に snippet `zub-usb-logging` を重ねて使う
- `config/user.conf` は application 側で既定マージされる

```sh
west build -b seeeduino_xiao_ble zmk-usb-bridge --pristine -S zub-usb-logging
```

## What The Snippet Changes

- `CDC ACM` の仮想 UART を 1 本追加する
- console / shell / bt monitor の出力先をその `CDC ACM` に切り替える
- `LOG_BUFFER_SIZE` と `LOG_PROCESS_THREAD_STARTUP_DELAY_MS` を増やして、monitor 接続前後でもログを拾いやすくする
- `USB HID-only release build` との識別のため、USB PID を `0x0012` に切り替える

## Flash

- 書き込みは通常どおり `zephyr.uf2` を `XIAO BLE` mass storage へコピーしてよい
- `UF2` 書き込み後にホストへ再接続すると、`HID` に加えて `CDC ACM` の `COM port` が見える想定

## Log Capture Procedure

1. `zub-usb-logging` build を `UF2` で書き込む
2. 実機を USB ホストへ接続する
3. Windows の `Device Manager` で追加された `COMx` を確認する
4. `PuTTY`、`Arduino Serial Monitor`、または同等の serial monitor でその `COMx` を開く
5. serial 設定は `115200 8N1, no flow control` を第一候補にする
6. 早い boot log を見たい場合は、monitor を先に開いてから board を reset する
7. 以後の `LOG_INF/WRN/ERR` と console 出力をその `COM port` で観測する

## Recommended Workflow

- 当面の実運用では `VSCode` の serial monitor を使って `COM port` を監視する
- build / flash / 実機操作は手元で行い、必要なログだけを Codex に戻して解析する
- これにより、`WSL` 経由の monitor 起動タイミング差で boot 冒頭が欠ける問題を避けやすい

### Manual Reset Workflow

1. `VSCode` の serial monitor で対象の `COM port` を開く
2. monitor が開いたことを確認してから board を手元で reset する
3. 出力された boot log を保存するかコピーする
4. 必要なログを Codex に渡して解析する

## Important Notes

- 第一候補では、`USB COM port` を監視すればよい
- ただしこれは `release` の USB descriptor と同一ではなく、`CDC ACM` を追加した開発専用構成になる
- Windows での最終 HID 列挙評価は、`release` の `HID-only` build でも別途確認する
- `ZMK` と同様に、早期 boot log が必要なら startup delay をさらに延ばす余地がある

## Secondary Path: SWD + RTT

- `J-Link` 等を持つ場合は `config/dev-rtt.conf` による `RTT` も補助候補として残す
- ただし現時点では `USB CDC ACM logging` を主導線とする
