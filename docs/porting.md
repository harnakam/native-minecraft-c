# Java原本からの移植記録

目標は、提供されたJava原本をC/C++へ移植することです。元のパッケージとクラス配置を`src/minecraft/net/minecraft`に保ち、各メソッドの状態・分岐・通知・呼び出し順を追える形にします。フォルダーだけ一致する独自実装を、移植済みとは数えません。

既存のクライアントと専用サーバーは、限定したCreative操作を実行する独自実装です。これを動作確認の接続先として使いながら、原本クラスを順に移植しています。以下のクラスにも未移植の依存関係があります。プロジェクト全体の翻訳は未完了です。

| 原本クラス | C側 | 移植・接続した処理 | 残る境界 |
|---|---|---|---|
| `InventoryCrafting` | `inventory/InventoryCrafting.{c,h}` | 幅・高さ・stackList・eventHandler、取得・減算・設定・除去・clear、名前・field・開閉等の原本メソッド。減算／設定直後のonCraftMatrixChangedをクラフト消費と残材配置から呼ぶ | `ItemStack`は既存の値型`mc_slot`。Javaのnull・参照共有・符号付きstackSize・例外の完全再現は未移植。Container/Slot全体とIChatComponentは未移植。表示名は翻訳キーのアダプター |
| `InventoryCraftResult` | `inventory/InventoryCraftResult.{c,h}` | 一枠の結果、要求個数にかかわらず全体を返すdecrStackSize、設定・除去・clear等。結果生成・取得・閉鎖へ接続 | 同じItemStack依存。既存クラフト管理はまだ原本のクラス階層ではない |
| `C08PacketPlayerBlockPlacement` | `network/play/client/C08PacketPlayerBlockPlacement.{c,h}` | 空・使用・配置のコンストラクタ、stackコピー、read/write/getters、processPacket。クライアント送信とサーバー受信へ接続 | BlockPosとPacketBufferは既存コーデックのアダプター。NBT付きstackは所有コピー。Cではメモリ／入力失敗をboolで返し、出力を保持する |
| `MapData`と`MapInfo` | `world/storage/MapData.{c,h}` | getMapInfo、updateVisiblePlayers、updateDecorations、updateMapData、getMapPacket。初回全体・変更矩形・5回ごとのアイコン通知、更新カウンタとdirty範囲 | ItemFrame依存未移植。中心計算とNBT読書きは既存`world/map.c`のアダプター。基底WorldSavedDataや一般的なJava collection APIは未移植 |
| `ItemMap` | `item/ItemMap.{c,h}` | getMapData、updateMapData、onUpdate、createMapDataPacket。主所持品36枠を原本順で更新し、選択中だけ測量。装備も含む40枠へMapInfoのパケットを要求 | World/Chunk/Block/MapColor/EntityPlayerは既存の依存アダプター。onCreated、tooltip、クラス継承全体は未移植 |

Cの所有管理、既存の所持品へのattach、原子的な出力、ソケット、Win32/OpenGL、保存ジャーナルは環境接続処理です。原本にある空メソッドは、その意味を保持します。必要な処理が未実装の場合は、空メソッドを追加して完成扱いにしません。

原本の処理順による差も維持します。InventoryCraftingのremove/clearは通知せず、decr/setは直後に通知します。MapInfoの更新カウンタとパケットカウンタはプレイヤーごとの状態で、NBTには保存しません。保存不要のtickでも、この状態を破棄しません。C08の空コンストラクタは位置を未設定のままにし、読み込みまたは明示的な構築前には送信できません。位置のYは符号付き12bit、向きと接触点はunsigned byteとして読み、浮動小数からbyteへの変換はJavaのint飽和・下位8bit化に合わせます。

差分が残る依存先も記録します。値型ItemStackでは、原本の書籍複製レシピが返す入力参照とcount=0の残材共有を表現できません。地図色の同率投票は既存アダプターが最初の出現色を選び、原本Guavaのidentity-hash由来の順序をまだ移植していません。未読み込みチャンクの生成、完全な各次元、ItemFrameは未対応です。地図96件、viewer64件、装飾256件・キー128byte、非負のSlot地図ID32767という現在の上限も原本と異なります。

次の移植対象はItemStack、Slot、Containerとcraftingの原本クラスです。その後もGUI・コントローラー・クライアント／サーバーハンドラー、ワールド、各ブロック・エンティティなどの原本クラスを順に置換します。クラス名とメソッド名の対応、呼び出し接続、原本で実行した差分検証、依存先の差を確認してから移植範囲を広げます。

公開対象はC/C++側とテスト・文書です。Java原本、MCP本体・マッピング、ゲームJAR、画像・音声・モデル、非公開の差分検証ハーネスと出力はGitに含めません。公開前にはGit indexの実際のblobを監査します。検証結果は[検証記録](verification.md)に記載します。
