# BobTail ESC: JIS配列とKeymap Editor

この設定は、PC側を日本語106/109キーボードの設定にして使うためのものです。
ローマ字入力を想定しています。US配列への変更や、PCへのキー変換ソフトの導入は不要です。

## Keymap Editorへの接続

1. https://nickcoutsos.github.io/keymap-editor/ を開きます。
2. GitHubを使用する設定元を選び、自分のアカウントで連携を許可します。
3. `shoichi0512/zmk-config-BobTail` の `main` を選びます。
4. `config/BobTail.keymap` を開きます。配列図には `config/BobTail.json` を使います。
5. 記号には `JP_AT`（@）、`JP_DQT`（"）、`JP_UNDER`（_）など、`JP_`で始まる定義を選びます。

`config/keys_jp.h` がJIS用のキー定義です。zmk-locale-generatorと同じ形式のHID式を使ったローカル定義です。
Keymap Editor上での選択肢と表示は、接続後に確認してください。
半角/全角は `JP_ZKHK`、変換は `JP_HENKAN`、無変換は `JP_MUHENKAN` です。
IMEの動作はPCのIME設定にも依存します。

## 今回の変更

元のキーマップの記号を、JIS環境で同じ記号が出るように置き換えました。
キーの位置、レイヤー順、トラックボール、ノブ、Bluetooth操作は元の設定を引き継いでいます。
新しいトグル切替キーや独自の同時押しは、使いたい位置を決めてから追加できます。

元からある主な操作:

| 通常の割り当て | 短押し | 押し続ける |
| --- | --- | --- |
| Space | Space | レイヤー2（記号・Bluetooth） |
| Enter | Enter | レイヤー1（数字・矢印・Fキー） |
| F | F | レイヤー4（マウスボタン） |
| K | K | レイヤー5（スクロール関連） |

レイヤー4・5はトラックボールドライバーとも関係するので、用途や番号を変える前に確認してください。

## 保存から本体への反映

Keymap Editorは設定ファイルを保存するツールです。保存だけでは本体に反映されません。

1. 編集内容をGitHubに保存します。
2. GitHubのActionsでビルドが成功したことを確認します。Fork後は、最初にActionsを有効にする操作が必要な場合があります。
3. 成功した実行から `firmware` の成果物をダウンロードします。
4. 左右それぞれに対応するUF2ファイルを、本体の書き込み手順に従って書き込みます。

`settings_reset` は通常のキーマップ更新用ファイルではありません。
出荷時バージョンによる違いがあるため、本体の書き込み操作は購入時の案内も確認してください。

ZMK Studioで配列を保存したことがある場合は、書き込み後もその配列が優先されることがあります。
その場合は、Studio側の配列を記録してから `Restore Stock Settings` を実行すると、
今回のファームウェアに含めたキーマップが使われます。

## 動作確認

ビルド成功は、接続先PCでの実際の入力を保証するものではありません。
最初にメモ帳などで `@ " : ; [ ] { } ^ ~ _ |` とバックスラッシュを確認してください。
特に「ろ」「¥」「変換」「無変換」はOSやHID設定による違いに注意してください。

参考:

- https://github.com/nickcoutsos/keymap-editor/wiki/Keycodes-for-other-locales
- https://zmk.dev/docs/keymaps/behaviors/hold-tap
- https://zmk.dev/docs/features/studio
