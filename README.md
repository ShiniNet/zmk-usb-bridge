# zmk-usb-bridge

`zmk-usb-bridge` は、ZMK ベース BLE キーボード向けの専用 USB レシーバーを目指すプロジェクトです。
PC とは USB HID デバイスとして接続し、キーボードとは BLE で接続することで、PC 側 Bluetooth 実装に左右されにくい入力経路を提供することを狙います。

## これは何か

このプロジェクトは、既存の ZMK BLE キーボードを無改造で使える BLE-to-USB ブリッジの検討と実装を目的としています。
キーボードから見れば、通常の BLE ホストやドングルの一種として振る舞う想定です。

## 何を重視するか

- 無改造の ZMK BLE キーボードと接続できること
- USB 側で安定したキーボード入力とポインティング入力を提供できること
- 再接続や復旧の挙動が堅牢で安定していること
- 個人開発者が入手しやすい部品で構築できること

## 初期スコープ

- 単一の ZMK キーボードとの 1:1 利用
- USB 給電専用の小型ドングル
- キーボード入力の USB HID ブリッジ
- ポインティング入力の USB HID ブリッジ
- ペアリング情報と bond 情報の永続化

## 初期スコープ外

- BIOS / UEFI など低レイヤ環境での完全対応
- 複数キーボードの切り替え運用
- 汎用 BLE 周辺機器との広範な互換性
- フル機能のデスクトップ設定アプリケーション
- TFFディスプレイによるステータス表示
- タッチパネルによる操作

## ドキュメント

- 設計文書の入口: [docs/README.md](docs/README.md)
- 要件整理: [docs/foundation/requirements.md](docs/foundation/requirements.md)
- 想定アーキテクチャと技術論点: [docs/foundation/architecture.md](docs/foundation/architecture.md)

## GitHub Actions での self-build

このリポジトリは、GitHub Actions から `Seeed XIAO nRF52840` 向け firmware をビルドし、`UF2` を artifact として取得できるようにしています。
フォーク利用時の基本導線は次のとおりです。

1. この repository を fork する
2. 必要なら `config/user.conf` を編集して、自分の利用条件に合わせる
3. fork 側で `Build Firmware` workflow を実行する
4. Actions の実行結果から artifact をダウンロードする
5. 展開した artifact 内の `zephyr.uf2` を board の bootloader drive へコピーする

標準の `release profile` artifact は、利用者向けに `zephyr.uf2` と `README.txt` だけを含む最小構成です。
調査用の追加成果物が必要な場合は、同じ run に含まれる `-debug` artifact を使います。
bring-up 用には `workflow_dispatch` から `dev-usb-logging` profile も選べます。

## 備考

このリポジトリは、ZMK 標準の dongle mode をそのまま利用する前提ではありません。
ドングル自体が BLE ホストとして既存の ZMK キーボードを受け入れる、無改造接続型のブリッジを対象としています。
