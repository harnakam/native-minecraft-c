# C919 — Native C voxel multiplayer

Minecraft Java Edition 1.8.9（protocol 47）との完全互換を目標にした、独自C実装のクライアントと専用サーバーです。必要な箇所にはC++やライブラリも利用できます。**現在はCreativeの基本操作、LANマルチプレイ、NBT付きインベントリ、2×2クラフト、アイテムのドロップ・拾得を実装しています。完全互換には未到達です。** 具体的な残作業は [互換性の記録](docs/compatibility.md) に記載します。

Minecraftのコード、テクスチャ、音声、モデル、JAR、MCP本体・マッピングは公開リポジトリに含めません。地形と表示用マテリアルは独自生成です。JavaやMinecraftのインストールはC919同士のプレイに不要です。

## 対応範囲

| 項目 | 現在の対応 |
|---|---|
| 描画クライアント | Windows x64、Win32 + OpenGL |
| 専用サーバー | Windows / Linux / WSL、C11 + zlib |
| 通信 | protocol 47、status / offline login / play、VarInt framing、zlib圧縮受信 |
| マルチプレイ | チャンク受信、プレイヤー表示・移動同期、チャット、ブロック採掘・設置 |
| 地形・保存 | 独自シード地形、C919専用ワールド形式、gzip NBTプレイヤー・地面のアイテム、転送途中の復旧 |
| インベントリ | 45スロット・カーソル、個数・ダメージ・NBT保持、クリック・分割・Shift移動・交換・ドラッグ・確認と再同期 |
| クラフト | プレイヤーの2×2グリッド、静的レシピ139件、修理・染色・書籍/地図/旗の複製・花火 |
| ドロップ・拾得 | 個数とNBT、地面のアイテムの表示・移動、拾得待ち・結合・消滅、他プレイヤーへの同期 |
| 操作 | マウス視点、移動・基本衝突形状、Creative飛行、サーバーのホットバー、全336アイテムIDの基本Creative選択 |
| 接続先 | C919サーバー、または1.8.9のCreative / `online-mode=false` サーバー |
| 未対応 | Microsoft/Mojang認証、暗号化login、Anvilワールド保存、Survival、3×3作業台、外部コンテナ、Mob、レッドストーン、全ブロック挙動 |

独自サーバーのワールド範囲は X/Z = `[-48, 64)`、高さ256です。外部サーバーの任意のゲーム機能を再現するものではありません。プロトコルを実装したことと、バニラGUIとの全操作互換を検証したことは区別してください。実測したテスト範囲は [検証記録](docs/verification.md) に記載します。

インベントリは `<world>.players/<UUID>.dat`、地面のアイテムは `<world>.items.dat` に保存します。アイテムIDは1.8.9のリソース名文字列を使用し、古い数値IDも読み込めます。未知のNBTタグと元のルート名を保持します。転送の記録が残った場合は起動時に復旧します。保存ファイルと `.pending-*.dat`・`.transfer.dat` は一緒にバックアップしてください。この保存対応は、Anvilワールドの読み書きや全プレイヤーデータの挙動互換を意味しません。

インベントリを閉じると、カーソルとクラフト入力は1.8.9の動作に合わせて地面へ落ちます。満杯のCreativeインベントリが拾得した残量を消去する動作と、Shiftクラフトで一部だけ入った出力の残量が消える旧版の動作も再現しています。水流、継続着火・消火などのアイテム物理には未対応部分があります。地面のアイテムは1024件、各NBT保存データは2MiBが上限です。

## Windowsでビルド

[MSYS2](https://www.msys2.org/) のUCRT64環境で必要な既存ツールを使います。依存ライブラリはzlibだけです。

```sh
pacman -S --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-zlib mingw-w64-ucrt-x86_64-python
```

PowerShellから：

```powershell
.\scripts\build.ps1
```

MSYS2 UCRT64シェルから：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

zlibは静的にリンクし、生成したクライアントはWindows標準のDLLを利用します。配布バイナリやインストーラーはこのリポジトリに含めません。

## 起動とマルチプレイ

別々のPowerShellでサーバーとクライアントを起動します。

```powershell
.\build\c919-server.exe
.\build\c919-client.exe --host 127.0.0.1 --port 25565 --name PlayerOne
.\build\c919-client.exe --host 127.0.0.1 --port 25565 --name PlayerTwo
```

LANへ公開する場合：

```powershell
.\build\c919-server.exe --bind 0.0.0.0 --port 25565 --world saves/world.c919 --seed 919
.\build\c919-client.exe --host 192.168.1.10 --name PlayerOne
```

`192.168.1.10` はサーバー機の実際のIPに置き換えてください。必要に応じてTCP 25565をWindowsファイアウォールで許可します。標準ではループバックだけで待ち受けます。Offlineモードにはアカウントの本人確認がありません。信頼できるLANで利用してください。

既存1.8.9クライアントからはマルチプレイの接続先にC919サーバーのIPとポートを指定します。C919クライアントを既存サーバーに接続するときは、その管理者が許可した `online-mode=false` サーバーを使います。Onlineモードの認証要求はエラーとして表示します。

| 操作 | キー |
|---|---|
| 移動 / 視点 | WASD / マウス |
| ジャンプ・飛行上昇 | Space |
| 飛行下降 / 高速移動 | Shift / Ctrl |
| Creative飛行切替 | F |
| 採掘 / 設置 | マウス左 / 右 |
| ブロック選択 | 1〜9 |
| インベントリ / 閉じる | E / E・Esc |
| インベントリの個数分割・移動 | 右クリック / 左クリック・Shiftクリック |
| スロットとホットバーの交換 | インベントリで1〜9 |
| 個数配分 | カーソルにアイテムを持って左・右ドラッグ |
| アイテムを落とす | Q / Ctrl+Q（全部）、インベントリ外の左・右クリック |
| Creative選択 / ページ | インベントリでC / PageUp・PageDown・ホイール |
| インベントリのキーボード操作 | Tab・矢印で選択、Enterで左クリック、Spaceで右クリック |
| チャット / 送信 | T / Enter |
| マウス解放・再開 | Esc |

## Linux / WSLサーバー

GCC、zlib開発ヘッダー、Python 3が必要です。CMakeがある場合はWindowsと同じCMake / CTestコマンドを使えます。CMakeなしでも次のスクリプトでビルドと通信テストを行えます。

```sh
sh scripts/test-linux.sh
./build-linux/c919-server --bind 127.0.0.1 --world saves/world.c919
```

Linux版には描画クライアントを含めません。CLIクライアントは `./build-linux/c919-client --headless --host 127.0.0.1 --run-seconds 5` で接続できます。

## 構成と拡張

既存の `src/minecraft/net/minecraft` ディレクトリ構成を使用しています。

- `network/`: バイトコーデック、フレーミング、圧縮、ソケット、offline UUID。
- `world/`: チャンク、独自地形、ブロック状態、保存形式。
- `nbt/`: 厳密なNBTコーデック、参照ビュー、gzipの安全な保存と読込。
- `item/`・`block/`: 1.8.9のアイテム定義、基本ブロック状態・衝突形状。
- `inventory/`: Slotの所有・コピー・通信、個数を保持するクリック処理。
- `crafting/`: レシピ照合、NBT付き特殊レシピ、出力消費・残留アイテム。
- `entity/item/`: 地面のアイテムの所有・物理・拾得・結合・NBTとパケット。
- `util/`: プレイヤーとアイテム保存のトランザクション記録・復旧。
- `server/`: サーバー主体のプレイヤー・編集処理と同期。
- `client/`: ネットワークの受信状態、操作、Win32/OpenGL描画。
- `tests/`: Cの境界値テストと独立したPython TCPクライアントによる通信テスト。

通信仕様は [protocol 47](docs/protocol47.md)、設計は [design](docs/design.md) を参照してください。未実装機能の空ハンドラーや偽の成功表示は用意していません。

任意の相互運用テストには、[PrismarineJSのMinecraft通信実装](https://github.com/PrismarineJS/node-minecraft-protocol) を一時的に使えます。通常ビルド・実行にはNode.jsもこのライブラリも不要です。

```sh
npm install --prefix .local/interop --ignore-scripts --no-audit --no-fund minecraft-protocol@1.68.0
node tests/interop_prismarine.cjs
```

Node.js 22以降が必要です。Linuxでは `C919_SERVER=./build-linux/c919-server` を指定してください。`.local/` の依存パッケージやテストデータは公開対象に含めません。

## 公開時の確認

```sh
python scripts/audit_public.py
```

この監査はGitの登録ファイルをソース・文書・設定の許可リストで検証します。`.gitignore` でも `MCP-919/`、アセット、JAR、保存データ、ビルド生成物を除外しています。新しいリソースを加える場合は出所と再配布権を確認してから監査方針を変更してください。
