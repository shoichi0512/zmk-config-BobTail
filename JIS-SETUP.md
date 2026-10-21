# BobTail ESC: JIS配列とKeymap Editor

この設定は、PC側を日本語106/109キーボードの設定にして使うためのものです。
ローマ字入力を想定しています。US配列への変更や、PCへのキー変換ソフトの導入は不要です。

## Keymap Editorへの接続

1. https://nickcoutsos.github.io/keymap-editor/ を開きます。
2. GitHubを使用する設定元を選び、自分のアカウントで連携を許可します。
3. `shoichi0512/zmk-config-BobTail` の `main` を選びます。
4. `config/BobTail.keymap` を開きます。配列図には `config/BobTail.json` を使います。
5. 記号には `JIS_AT`（@）、`JIS_DQT`（"）、`JIS_UNDER`（_）など、`JIS_`で始まるマクロ動作を選びます。

JIS用の動作は `config/BobTail.keymap` の `macros` に登録しています。
公開版Keymap Editorは、独自ヘッダー内のJISキーコードを選択肢として読み込みません。
そのため、名前付きの標準ZMKマクロを使い、エディターから動作を選択できる形にしています。
画面では `JIS_AT` や `&jp_at` という名前で表示される場合があります。
通常の `&kp` のキーコード欄ではなく、割り当てる動作としてJIS用マクロを選びます。
`MACROS` タブでは各動作の内容も確認できます。
半角/全角は `JIS_ZKHK`、変換は `JIS_HENKAN`、無変換は `JIS_MUHENKAN` です。
IMEの動作はPCのIME設定にも依存します。

各マクロは、キーを押したときに押下、離したときに解放を送ります。
押している間のOSのキーリピートと、Shift/Ctrl等との併用を想定した構成です。
キーコードを指定する必要があるLayer-TapやMod-Tapの引数には、これらのマクロを直接指定できません。
JIS記号を短押し側に使いたい場合は、その組み合わせ専用のHold-Tap動作を定義します。

## 現在の配列

Keyball39のPDFに合わせた配列です。詳細は [KEYBALL39-MAPPING.md](KEYBALL39-MAPPING.md) を参照してください。
右親指の `MO(1)` を押すと数字・記号、左親指側の `MO(3)` を押すとショートカットになります。
かな・英数キーは短押しで入力切替、長押しでShiftです。SpaceとTabは通常の単独キーです。

レイヤー4・5はトラックボールドライバーと関係するので、番号を変える前に確認してください。

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

- https://zmk.dev/docs/keymaps/behaviors/macros
- https://zmk.dev/docs/keymaps/behaviors/hold-tap
- https://zmk.dev/docs/features/studio
