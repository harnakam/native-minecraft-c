# ワールド・NBT・保存・Chunk・生成の仕様

対象は Java 1.8.9 / MCP919。対象固定、Java の評価・数値・参照規則は [総合仕様](README.md) と [実行モデル](01-runtime.md)、packet の外形は [通信](03-network.md)、Entity／Item／TileEntity の個別動作は [ゲーム処理](04-gameplay.md) に従う。

Material／MapColorの全静的データとproperty／canonical state graphの構築は[MaterialとBlockState](07-material-block-state.md)へ分ける。全Block subclassと全Guava依存は両章の明示gapとして残る。

**本章は完成した全ワールド仕様ではない。** NBT と主要保存 schema、Chunk 配列、主要照明・tick の処理順を具体化したが、全 Block の state 変換、全 biome／GenLayer／構造物／装飾／TileEntity、完全な World 更新依存は未充足である。末尾の `WS-GAP` を解消せず「この章だけで完全互換 terrain を生成できる」と判定してはならない。

## WS-00 根拠と確度

今回読み取った原本はローカル `MCP-919/src/minecraft/net/minecraft/` 以下。公開文書には原本本文、mapping、JAR、観測 TSV、資産を含めない。識別子は責務と監査位置を示すものであり、未記載部分を原本読者へ丸投げする実装指示ではない。

| 範囲 | 原本の主な根拠 | 確度の限界 |
|---|---|---|
| NBT | `nbt/NBTBase`, 全 `NBTTag*`, `NBTSizeTracker`, `CompressedStreamTools` | binary と到達した read/write/get/copy の読取。JDK8 全 malformed UTF／allocation／例外生成は別依存 |
| ディスク | `world/chunk/storage/RegionFile`, `RegionFileCache`, `AnvilChunkLoader`, `AnvilSaveHandler`; `world/storage/SaveHandler`, `ThreadedFileIOBase` | 原本読取。下記2箇所の不自然な decompile は未確定 |
| metadata | `WorldSettings`, `WorldType`, `EnumDifficulty`, `GameRules`; `storage/WorldInfo`, `DerivedWorldInfo` | ctor、主要 scalar/ref、NBT schema、rules の更新順 |
| saved data | `WorldSavedData`, `storage/MapStorage`, `SaveDataMemoryStorage`, `MapData`, `SaveHandlerMP` | disk/cache/counter と map 保存 schema。測量・表示の完全仕様は未閉鎖 |
| Chunk | `world/chunk/Chunk`, `EmptyChunk`, `ChunkPrimer`, `NibbleArray`, `storage/ExtendedBlockStorage` | ctor、state/read/fill/height、主要 light 経路。全 entity/tile/collision 更新は未閉鎖 |
| tick/provider | `World`, `WorldServer`, `WorldServerMulti`, `WorldProvider*`; `client/multiplayer/WorldClient`, `ChunkProviderClient`; `world/gen/ChunkProviderServer`; `NextTickListEntry` | 下記に列挙した順序。mob/village/portal/redstone 等の全 leaf は別途仕様が必要 |
| terrain | `ChunkProviderGenerate`, `ChunkProviderFlat`, `ChunkProviderSettings`, `NoiseGeneratorImproved`, `NoiseGeneratorOctaves`, `NoiseGeneratorPerlin`, `NoiseGeneratorSimplex`, `gen/layer/GenLayer` | outer flow、基礎ノイズ、seed/order の読取。全生成の充足とは異なる |

この仕様作成では Java／ゲーム／observer を実行していない。既存の凍結観測は照合の補助に限る。surface の celestial angle は supplied decompile が別 local を失っていることを、既存の unchanged bytecode 読取記録が確認済みである。後述の修正式はその区別を保持する。既存 world-services 観測にも特殊 NaN の符号に1件の未解決 host 差があり、全 bit 入力の一致を保証しない。

## WS-01 NBT binary

### 1.1 primitive と tag

整数は big endian、signed two's complement。float は binary32、double は binary64。NBT の byte／short と、長さを表す unsigned16 は区別する。Java `writeFloat/writeDouble` の NaN canonicalization と `readFloat/readDouble` の raw bit 復元を実現する。wire byte 列が同じであることと、内部の NaN payload が同じであることは別判定にする。

| ID | 型 | payload |
|---:|---|---|
| 0 | End | 0 bytes |
| 1 | Byte | signed8、1 byte |
| 2 | Short | signed16、2 bytes |
| 3 | Int | signed32、4 bytes |
| 4 | Long | signed64、8 bytes |
| 5 | Float | 4 bytes |
| 6 | Double | 8 bytes |
| 7 | ByteArray | signed32 count、その個数の byte |
| 8 | String | unsigned16 MUTF-8 byte 長＋その byte 列 |
| 9 | List | element ID signed byte、signed32 count、名前なし element payload を count 個 |
| 10 | Compound | named tag の連続、最後に ID0 一つ |
| 11 | IntArray | signed32 count、count 個の big-endian signed32 |

1.8.9 に LongArray ID12 はない。未知 ID を将来版の tag として読まない。factory は未知 ID に NULL を返すため、その後の読取は呼出位置に応じた失敗となる。ここを「常に IOException」に置換するのは例外互換ではない。

named tag は `ID → name → payload`。ID0 では name/payload を省略する。Compound の内部でも同じ規則。List の element は name と ID を個々には持たない。`CompressedStreamTools.write` の root は Compound、name は空文字。空 root の自前例は `0A 00 00 00`。reader は root name を読むが戻り値に保持せず、root が Compound でなければ IOException を投げる。

Compound は String→NBTBase の HashMap。一つの key の重複入力は後の値が勝つ。保存順は Java HashMap のその時点の key iteration であり、辞書順・入力順という仕様ではない。意味比較で Compound 順を無視できても、元バイナリと同じ出力を要求する場合は map hashing／capacity／resize／iteration を閉じる必要がある。

### 1.2 Modified UTF-8

NBT の String と named tag の name は `DataInput.readUTF`／`DataOutput.writeUTF`。通常 UTF-8 と交換しない。

| UTF-16 code unit | 書込 byte 数・形式 |
|---|---|
| U+0001..U+007F | 1 byte、その値 |
| U+0000 | `C0 80`、2 bytes |
| U+0080..U+07FF | 2 bytes |
| U+0800..U+FFFF | 3 bytes |
| surrogate pair | 二つの unit を別々に3 bytes、合計6 bytes |
| lone surrogate | 当該 unit を3 bytes、replacement しない |

長さ prefix は byte 数であり UTF-16 length ではない。65535 bytes を超える書込は UTFDataFormatException。名前／文字列の読みが短ければ EOFException。読取は Java8 の MUTF-8 decoder の lead byte／continuation／不足 byte 検査に従い、UTF-8 の4 byte form を受容する設計にしない。

自前例: 空文字 `00 00`、NUL 一つ `00 02 C0 80`、`A` 一つ `00 01 41`。U+1F600 は UTF-16 `D83D DE00` として `00 06 ED A0 BD ED B8 80`。通常の UTF-8 `F0 9F 98 80` を保存してはならない。

**未充足 WS-GAP-UTF:** Java8 decoder が受容する非canonicalな2/3 byte sequence、raw NUL、overlong、全 malformed 入力の例外位置・消費 byte 数は本章では全列挙していない。再実装では JDK8 の当該契約を独立に仕様化し、String decoder を厳格な現代 UTF-8 decoder に置換しない。

### 1.3 read の量・深さ・失敗順

`NBTSizeTracker.read(bits)` は `bits/8` を累積してから上限を比較する。これは圧縮 byte 数・未圧縮 wire byte 数そのものではない。List と Compound は depth>512 で RuntimeException。root depth は0、子を読む際は+1。primitive の depth は独立には拒否しない。

| 到達箇所 | tracker 加算 bits |
|---|---:|
| End／Byte／Short／Int | 64／72／80／96 |
| Long／Float／Double | 128／96／128 |
| ByteArray header と要素 | 192、その後 `8×count` |
| IntArray header と要素 | 192、その後 `32×count` |
| String | 288、その後 `16×UTF16.length` |
| List | 296、その後 `32L×count` |
| Compound | 384 |
| Compound の key | `224+16×key.length` |
| Compound 同 key の上書き | 288 |

ByteArray／IntArray の要素加算では乗算が int32 で行われてから long へ拡張される箇所がある。overflow を広い安全整数へ変えて原版 tracker の結果を変えない。負 count は array allocation または list capacity 作成の時点で失敗し、負長を空として成功させない。Compound は自身の map を clear してから entries を順に追加する。途中失敗時に旧 map へ戻らない。

List は type→count の順に読む。type0かつcount>0 は「Missing type」RuntimeException。type0,count0 は合法空。容量用 list を新規に作ってから子を順に read/add する。`write` は先に tagType を現在の先頭 element ID、空なら0へ更新してから bytes を出すので、書込は内部状態を変える。append/set は End を追加せず、既存 type と不一致なら warn して追加／置換しない。

Compound の child read helper は factoryで子を生成してからreadし、そこで発生したIOExceptionだけをtag名/type付きCrashReport/ReportedExceptionへ包む。depth/size RuntimeException、unknown typeのNULL dereference等までこのcatchに含めない。keyのreadUTFはchild helperに入る前なので、同じIOExceptionでも到達位置でcatch経路が異なる。

saved NBT の compressed read は tracker.INFINITE を使う。PacketBuffer の NBT は2 MiB trackerを使う別境界であり、保存を一律2 MiBへ制限する規則ではない。保存の INFINITE でも depth と Java allocation/IO の失敗は残る。

### 1.4 getter／alias／copy

Compound の `hasKey(key,99)` は numeric ID1..6のいずれか。numeric getter はこの判定の後、その NBTPrimitive の変換を行い、欠損／異型では0。boolean は byte変換!=0。String 欠損／異型は空文字、ByteArray／IntArray は新しい長さ0配列、Compound は新しい空 Compound、List は新しい空 List を返す。既存 List は **要素数>0かつ要求 element type と保持 type が不一致**の時だけ新しい空 Listへ置換する。要素数0なら保持 type に関係なく既存 List を直接返す。適合 tag の戻りは直接参照であり copy ではない。

Float→int/short/byte は `MathHelper.floor_float`、Double→int/short/byte は `floor_double` を先に行ってから縮小。Float→long はJava cast、Double→long は `Math.floor` 後の cast。整数 primitive の狭めは下位bit。従って例えば Float -1.2 の int getter は -2であり、単純 cast の -1ではない。

array tag ctor/getter、Compound.setTag/getTag、適合する List element getter は同じ mutable object／array を保つ。`copy()` は各辺ごとに再帰 copy、byte/int配列も複製する。同じ child を二か所に保持した親を copy しても、子は二つになる。C の whole-graph snapshot の memo による alias 保存とは異なる。Compound.merge は相手の key iteration順に、双方がCompoundなら既存子へ再帰mergeし、そうでなければ相手の値のcopyをstoreする。全mergeで新しい親graphを作る処理ではない。cycle を通常の NBT copy/save/merge の成功入力として扱わない。

primitive equals は typeと値、浮動小数は Java `==`。NaNはこの値比較でfalse、+0/-0はtrue。浮動小数 hash は Float.floatToIntBits／Double.doubleToLongBits を使う。配列は内容比較、List は element 順＋tagType、Compound は map 内容。collection自身のidentity fast pathとprimitiveの値比較を混ぜない。toString/SNBT風表記の全文字列化契約は本章の未充足対象。

## WS-02 圧縮・ファイル配置・保存の所有者

### 2.1 圧縮は entry point ごとに選ぶ

| 対象 | 外側の形式 |
|---|---|
| `level.dat`, `level.dat_old`, player `<UUID>.dat` | GZIP stream 内の root Compound |
| 通常 MapStorage saved data `<name>.dat` | GZIP、root の `data:Compound` に subclass payload |
| `data/idcounts.dat` | **無圧縮**の root Compound、各 key は Short |
| `.mca` 内の chunk | sector length＋compression ID。原版 writer は2=zlib、readerは1=GZIPも受容 |
| PacketBuffer NBT | 無圧縮 named root または NULL marker、通信章参照 |

`CompressedStreamTools.readCompressed/writeCompressed` は buffered data stream と GZIP を使い finallyでcloseする。plain DataInput/DataOutput版は圧縮しない。raw File版のreadはfile absentならNULL。closeの例外も呼出経路のcatch/finally契約に含める。

### 2.2 保存ディレクトリ

world root の通常構成は `level.dat`, `level.dat_old`, `session.lock`, `region/`, `playerdata/`, `data/`。Nether は `DIM-1/region/`、End は `DIM1/region/`。AnvilSaveHandler は provider の実classでこの分岐を行う。単に dimension の符号を見て任意dimensionを同名 directoryへ割当てる規則ではない。

WorldServerMulti は base world の MapStorage／Scoreboard を共有する。dimensionごとに map counter を独立複製しない。chunk loader の保存先だけは provider ごとに分かれる。DerivedWorldInfo は5.5のdelegate/no-opを用いる。

### 2.3 session lock と level/player replacement

SaveHandler field initializer は現在時刻を `initializationTime` に記録する。ctor は world/data directoriesを作り、flagがtrueならplayerdataも作る。その後 session.lock を作り、big-endian long 一つを書いてcloseする。失敗は診断してRuntimeException。checkSessionLockは同longを読み、時刻不一致またはIO失敗でMinecraftException。OSのexclusive lock、WAL、全processで衝突しないnonceではない。

level readはlevel.datをGZIP→`Data`Compound→WorldInfoとして試す。caught Exceptionなら診断してlevel.dat_oldを試す。どちらも得られなければNULL。readする内容が正常でも欠損tagには後述のgetter defaultsが働く。

level writeは `Data` を持つ新しい root を構築後、次をtry内で実行する。

1. `level.dat_new` にGZIP書込、close。
2. 古い `level.dat_old` があればdelete。
3. `level.dat` をoldへrename。
4. `level.dat` が残ればdelete。
5. newをlevel.datへrename。
6. newが残ればdelete。

delete/renameのbooleanをすべて成功判定してtransaction化する仕様ではない。Exceptionは診断して戻る。AnvilSaveHandler.saveWorldInfoWithPlayerはこの前にversion=19133を設定する。baseのordinary saveがすべて自動でこの値へ補正するわけではない。

playerはEntityのwriteToNBTを実行してから `<UUID>.dat.tmp` をGZIP書込、既存.datをdelete、tmpを.datへrename。UUIDgetterをpath構築ごとに再評価する。readは存在する通常fileをGZIPread、file/parseのcaught Exceptionはwarn。そのtryを抜けてから player.readFromNBT を呼ぶため、Entity読込の例外をfileIOのcatchへ含めない。

`CompressedStreamTools.safeWrite` の別経路は `_tmp` に**raw** NBTをwrite→destination delete→まだ存在すればIOException→rename。level.datのGZIP replacementと混同しない。

**journalの原版有無:** ここで読んだ SaveHandler／Anvil／RegionFile にはC919の全player/item/mapを一括commitするmanifest journalやWAL、fsync barrierはない。単一chunkのsector更新も後述の順で直接行われる。原版の部分失敗と現在Cのdurability保護を同じ規則として書かない。

## WS-03 Anvil region

### 3.1 address/header

block→chunkは符号付き `>>4`、chunk→regionは符号付き `>>5`。region内localは `cx&31,cz&31`。例えば block x=-1はchunk=-1、region=-1、localchunk31。0方向の整数除算は使わない。

filenameは `r.<regionX>.<regionZ>.mca`。sector=4096bytes。headerは2sectors、最初のsectorが1024個のbig-endian location int、次が1024個のbig-endian timestamp int。entry添字は `localX+32*localZ`。

locationの上位24bitは開始sector、下位8bitは占有sector数。値0は未保存。timestampはUnix秒をsigned32へ狭めた値であり、tick数ではない。readerはtimestampでchunkを拒否しない。

payload開始は `startSector*4096`。そこにbig-endian signed32 length、compression byte、compressed bytes。lengthはcompression byteを含み、先頭のlength自身4bytesを含めない。原版writerはcompression2。その圧縮内NBTはrootCompound→LevelCompound。

### 3.2 allocation/write order

openはRandomAccessFile("rw")。**file長<4096**なら先頭からlocation/timestampのzero headerを作る（判定は<8192ではない）。sectorFree bitmapはheaderの0/1を使用済み、他はfreeとして始め、headerの有効範囲を使用済みにする。4096..8191bytesなどの破損fileで第二headerが不足する場合、通常fileへ補修せず読込IOExceptionのprefixが残る。完全な重複sector／破損headerの検出・修復処理はない。

圧縮bytes数をNとすると要求sector数は `floor((N+5)/4096)+1`。ちょうどsector境界でも余分な一sectorを要求する。要求>=256ならwriteは何もせず戻る。countは8bitだから巨大chunkを外部fileへ分ける後代の形式はない。

| 分岐 | 原版順序 |
|---|---|
| 旧startあり、旧count==新count | 同じstartへlength/type/payload上書き |
| 旧rangeを解放しfreeの連続runへ移動 | 旧free bitmap更新→最初の十分な連続runを探す→**location header更新**→new bitmapを使用済み→payload |
| 十分なrunがなくEOFへ追加 | EOF seek→zero sectors追加→sizeDelta増→payload→**location header更新** |

最後にtimestamp headerをcurrentMillis/1000のsigned32へ更新する。未使用padding、短縮前の残りbytesを必ず消去する処理はない。write/closeのIOExceptionは内部で診断され、呼出し側が必ず異常を知るとは限らない。

### 3.3 read/error/cache

readはsynchronized。local範囲外、offset0、割当sectorがfile範囲外、length<=0、length>count*4096、未知compressionではNULL。IOExceptionsもNULL。lengthチェックは「length+4がsector内」という強化guardではない。byte配列を確保してRandomAccessFile.readを呼び、readFullyと同じ不足検査に置換しない。展開/NBTの後続エラーはその後のentry pointで扱う。

RegionFileCacheはFile→RegionFileのglobal map。create/clearはsynchronized。256entries以上で**全部**close/clearする。LRU一件evictではない。読取要求でもregion directoryが作られ得る。close失敗をprintしてもcacheをclearする。

**未確定 WS-GAP-REGION-PADDING:** supplied RegionFile ctor の非4096整列fileの分岐は「remainder回zeroを書込」と読め、現在pointer/補数paddingに疑義がある。正常なheader/sector仕様は上記で確定するが、破損fileへのこの分岐を意図から修正してはならない。retained bytecodeのseek位置／loop bound／不足file3種類の静的・独立観測が必要。本作業では新実行していない。

## WS-04 chunk のディスク NBT

### 4.1 Level schema

rootに `Level:Compound` が必要。AnvilChunkLoaderはLevelがtype10、Sectionsがtype9か確認し、不適合ならlogしてNULL。Listのelement typeはgetTagList要求10の規則が作用する。

| Level key | write型 | 内容 |
|---|---|---|
| V | Byte | 1 |
| xPos, zPos | Int | chunk座標 |
| LastUpdate | Long | 現worldのtotal time |
| HeightMap | IntArray | 256、添字z*16+x、各列の光遮蔽高度 |
| TerrainPopulated, LightPopulated | Byte | boolean |
| InhabitedTime | Long | chunk滞在累積tick |
| Sections | List<Compound> | nonNULL sectionをsection配列順に保存。empty sectionも保存 |
| Biomes | ByteArray | 256、z*16+x、biomeIDの低8bit |
| Entities | List<Compound> | 各classのEntity NBT |
| TileEntities | List<Compound> | 各classのTileEntity NBT |
| TileTicks | List<Compound> | pendingupdatesがnonnullの場合だけ |

LastUpdateとInhabitedTimeは別。HeightMapは地形表面の最高nonair+1ではなくopacityで計算する。低密度地形・water・leaves・glass等でMaterial predicateやnonair判定を流用しない。

### 4.2 section packing

| section key | 型/通常長 |
|---|---|
| Y | Byte、section baseY>>4を低8bit |
| Blocks | ByteArray4096 |
| Data | ByteArray2048、metadata nibble |
| Add | ByteArray2048、必要時のみ、blockID上位4bit |
| BlockLight | ByteArray2048 |
| SkyLight | ByteArray2048。noSkyでもzero arrayを書込 |

section添字 `q=(localY<<8)|(localZ<<4)|localX`。char state `s` はunsigned16。保存分解は `Blocks[q]=(s>>4)&255`, `Data[q]=s&15`, `Add[q]=s>>12`。Addは初めてnonzero上位を見た時に配列をnewし、以前のzero部分はallocation default。復元は `s=(Add<<12)|((BlocksByte&255)<<4)|Data`。

読みはYをsignedbyteとして取得しEBS(Y<<4,hasSky)をnew。Blocksの実長でchar[]をnewし、DataとoptionalAddのNibbleArrayから復元する。通常値4096だからと入口で同一長を仮定しない。NBT getterの空arrayやsignedYが後続のarray境界例外を生み得る。setData→BlockLight→必要時SkyLight→removeInvalidBlocks→sections[Y]という変更順。noSkyのSkyLightは読まない。

### 4.3 queue/load と Entity/tick

saveChunkはsession lock確認がtry外。root/Level構築とwriteChunkToNBT、pending map登録はtry内でExceptionをlog。pendingのkeyはchunkXZ。現在being-written集合に含まれるcoordではputを省略し、IOqueue登録は行う。全save要求が必ずdiskへ順番に残る保証ではない。

writeNextIOはpending emptyならfalse。map iterationの最初のcoordをbeing-writtenへ追加→pending remove→nonnullNBTを書込→finallybeing-written remove→true。trueは処理したという意味で、disk成功を示すboolではない。ThreadedFileIOBaseはqueueのwriteNextIOがfalseになると項目をremoveしsaved counterを増す。queueIOは同IOobjectが既にあれば追加しない。待機はqueued/saved counterが一致するまでsleep10ms、待機中flagで処理sleepが変わる。

loadはpendingのNBTがあればそれを先に使い、なければRegionstream→NBT。保存座標が要求と違えば最初にchunkを構築した後にxPos/zPosを要求値へ書換え、**もう一度**readChunkFromNBT。最初のEntity/TileEntity constructor効果をundoしない。

Entity listは各compoundからclassを生成。生成NULLでもchunk.hasEntitiesをtrueにし、nonnullだけadd。Ridingのcompoundを辿り、各乗り物を生成/add/mountする。TileEntityも生成nonnullを登録する。全classのtagはゲーム章のschemaを使う。

TileTick compoundは `i:String` のBlock resource名、`x/y/z:Int`, `t:Int`, `p:Int`。tは `(int)(scheduledTime-worldTotalTime)`、pはpriority。readはiがStringなら名前、そうでなければ旧numericBlockIDとしてlookupしscheduleBlockUpdateする。保存対象rectangleはchunkorigin-2からorigin+18未満のXZで、隣接2blockも含み、当該抽出ではYをfilterしない。

**未確定 WS-GAP-ANVIL-FLUSH:** supplied AnvilChunkLoader.saveExtraDataのdecompileにはwriteNextIO=false時のloop終了が現れない。ThreadedFileIOBase/AnvilSaveHandler.flushの待機契約は読取済みだが、この独立flushメソッドの正常終了・busy-spinを本章では確定しない。retained bytecodeのbranch先を確認する必要がある。

## WS-05 level metadata と GameRules

### 5.1 WorldSettings／WorldType／Difficulty

WorldSettingsの状態はseed:long、GameType参照、mapFeatures:boolean、hardcore:boolean、WorldType参照、commands:boolean、bonusChest:boolean、worldName:String。最後のworldNameはgenerator optionsに使う名前で、levelの表示名とは別。通常ctorは最初の5値を直接保持、commands/bonus=false、options=""。WorldInfoから作るctorはseed→gametype→features→hardcore→terrainTypeのgetter順であり、commands/optionsまでcopyしない。enable系は同じreceiverを更新して返す。

GameTypeはNOT_SET=-1、SURVIVAL=0、CREATIVE=1、ADVENTURE=2、SPECTATOR=3。unknown numeric/name lookupはSURVIVALへ。DifficultyはPEACEFUL0/EASY1/NORMAL2/HARD3。difficulty lookupは `array[id%4]` であり、負idをbitmask正規化しないため負remainderはindex例外になり得る。

WorldTypeは16slotのmutable配列とnamed final参照。配列置換してもnamed参照は変わらない。

| slot | name | version/特徴 |
|---:|---|---|
| 0 | default | version1、versioned |
| 1 | flat | version0 |
| 2 | largeBiomes | version0 |
| 3 | amplified | notification=true |
| 4 | customized | version0 |
| 5 | debug_all_block_states | version0 |
| 8 | default_1_1 | version0、canBeCreated=false |
| 他 | NULL | hole |

parseは現在配列を昇順に走査しcase-insensitive name比較。defaultの保存generatorVersion0はDEFAULT_1_1へ解決、それ以外は当該type。世界の生成versionとsaveversion19133を混ぜない。

### 5.2 WorldInfo 状態と defaults

36fieldsを保持する。一つのWorldInfoをscalar mirrorへ複製しない。seed、terrainType、generatorOptions、spawnXYZ、totalTime、dayTime、sizeOnDisk、lastTimePlayed、playerTag、dimension、levelName、saveVersion、cleanWeatherTime、rainTime、raining、thunderTime、thundering、gameType、mapFeaturesEnabled、hardcore、allowCommands、initialized、difficulty、difficultyLocked、borderCenterX/Z、borderSize、borderSizeLerpTime、borderSizeLerpTarget、borderSafeZone、borderDamagePerBlock、borderWarningDistance、borderWarningTime、GameRulesが状態である。

noarg/default initializerはterrainType=DEFAULT、options=""、bordercenter0,size60000000,lerptime0,target0,safezone5,damage0.2,warningdistance5,warningtime15,newGameRules。他はallocation default、difficultyもNULL。settings/namectorはvirtualpopulateの後にname、NORMAL difficultyを設定する。初期化済みflagは自動trueにしない。

copyctorはdefaultのnewGameRulesを作った後、元のGameRules参照へ上書きしaliasを共有する。playerTag／terrainType／Stringも直接参照。**cleanWeatherTimeはcopyされず0のまま**。deepcopyした独立rulesを返す実装は一致しない。

### 5.3 `level.dat/Data` schema と読取 defaults

`N` はnumeric ID1..6、`B` は正確にByte、`S` は正確にString、`C` はCompound判定を示す。optionalのtype gateを「キーがある」だけへ弱めない。

| key | write型 | read/default |
|---|---|---|
| RandomSeed | Long | numeric getter、欠損0 |
| generatorName | String | Sならparse、unknownならDEFAULT |
| generatorVersion | Int | name分岐内、Nなら値、なければ0、versioned type解決 |
| generatorOptions | String | **generatorName分岐内だけ**Sなら読取、ほか"" |
| GameType | Int | lookup、欠損0=SURVIVAL |
| MapFeatures | Byte | Nならboolean、なければtrue |
| SpawnX/Y/Z | Int | 欠損各0 |
| Time | Long | total time、欠損0 |
| DayTime | Long | Nなら値、なければTime |
| LastPlayed, SizeOnDisk | Long | read getter。write LastPlayedは保存時currentMillis |
| LevelName | String | 欠損"" |
| version | Int | saveversion |
| clearWeatherTime, rainTime, thunderTime | Int | 欠損0 |
| raining, thundering, hardcore | Byte | numeric boolean、欠損false |
| initialized | Byte | Nならboolean、なければtrue |
| allowCommands | Byte | Nならboolean、なければgametype==CREATIVE |
| Player | Compound、省略可 | Cなら直接保持、Player.Dimensionからdimension取得 |
| GameRules | Compound | Cなら既存default rulesへmerge読取 |
| Difficulty | Byte、省略可 | NならgetByte→enum。欠損はNULL、NORMALではない |
| DifficultyLocked | Byte | Bのときだけboolean読取 |
| BorderCenterX/Z | Double | Nのとき更新、default0 |
| BorderSize | Double | Nのとき更新、default60000000 |
| BorderSizeLerpTime | Long | Nのとき更新、default0 |
| BorderSizeLerpTarget | Double | Nのとき更新、default0 |
| BorderSafeZone | Double | Nのとき更新、default5 |
| BorderDamagePerBlock | Double | Nのとき更新、default0.2 |
| BorderWarningBlocks/Time | **Double** | readはnumeric→int、default5/15 |

表はschemaであり、複数keyをまとめた行の横並びを評価順としない。writeはseed→generator名/version/options→GameType/features→SpawnX/Y/Z→Time/DayTime→**SizeOnDisk→LastPlayed**→name/version→cleanWeather→rainTime/raining→thunderTime/thundering→hardcore→**allowCommands→initialized**。borderはcenterX/Z→size→lerptime→**safezone→damage→lerptarget**→warningdistance/time、次にoptionalDifficulty→DifficultyLocked→GameRules→optionalPlayer。HashMap serializationはこのinsert順をそのまま出力順としない。getNBTTagCompoundは保持playerTag、cloneNBTCompound(arg)は引数tagを直接attachする。名称のcloneからPlayerのdeepcopyを推測しない。

NBT ctorはseed→generator分岐→GameType/features→SpawnX/Y/Z→Time/DayTime→**LastPlayed→SizeOnDisk**→name/version→cleanWeather→rainTime/raining→thunderTime/thundering→hardcore→**initialized→allowCommands**→Player/dimension→rules→difficulty/locked→bordercenterX/Z→size→lerptime→**lerptarget→safezone→damage**→warningdistance/time。後段例外で前段fieldsは残る。settings populateはseed→GameType→features→hardcore→terrainType→options→commandsのgetter順で、その都度元receiverを呼ぶ。mutable/virtual getterを一つのsnapshotへ共通化しない。

### 5.4 GameRules

TreeMap<String,Value>でUTF-16 lexicographic key順。初期15件:

| type | initial true | initial false/number |
|---|---|---|
| BOOLEAN_VALUE | doFireTick, mobGriefing, doMobSpawning, doMobLoot, doTileDrops, doEntityDrops, commandBlockOutput, naturalRegeneration, doDaylightCycle, logAdminCommands, showDeathMessages, sendCommandFeedback | keepInventory=false, reducedDebugInfo=false |
| NUMERICAL_VALUE | — | randomTickSpeed="3" |

ValueはString参照、boolean、int、double、declared type。ctorはtypeを代入してsetValueを呼ぶ。setValueはString代入→Boolean.parseBoolean→intをboolean由来の1/0→Integer.parseIntをtry→Double.parseDoubleをtry、各NumberFormatExceptionだけcatch。失敗parseは前のdoubleを保持できる。NULLはString=NULL/boolean=false/int=0のprefix後、Double側NPEがcatchされず出る。NULLを""へ変えない。

既存keyのsetOrCreateは同Valueを変更しtypeを保持、新規はANY_VALUEをnew。readFromNBTは既存defaultmapをclearせず、各keyのString getter結果をsetする。writeは全値をString tagとして出す。boolean/int gettersはそのcache、missingならfalse/0、missing Stringは""。getRulesは新しいsorted String[]、各Stringは同参照。areSameTypeは「stored type==要求type、または要求typeがANY」であり、stored ANYが全要求を満たす規則ではない。

Integer parseはJava8の符号／UTF-16 digit／overflow検査、Double parseはJava8のdecimal／hexadecimal／exponent／NaN／Infinity／suffix／trimを実現する。host strtodの成功だけで適合とはしない。全parse grammarとdecimal→binaryの正しい丸めは実行モデルの未充足JDK依存として追跡する。

### 5.5 DerivedWorldInfo／dimension共有

DerivedWorldInfo は親のnoarg初期化を実行してからtheWorldInfoを直接保持する。親の自分用rules/arrays/scalarsを作らずdelegateだけを置く構造ではない。

| method群 | 動作 |
|---|---|
| getNBTTagCompound, cloneNBTCompound | 同引数でdelegateへ呼出、exact結果を返す |
| seed, spawnXYZ, total/daytime, sizeOnDisk, playerNBT, name, version, lastPlayed | 各対応getterをdelegateへ呼出 |
| thunder/rainflagと時間、GameType、mapFeatures、hardcore、terrainType、commands、initialized、GameRules、difficulty/lockedのgetter | delegateへ呼出、refはalias |
| setSpawnX/Y/Z, setWorldTotalTime, setWorldTime, setSpawn, setWorldName, setSaveVersion | **実emptyoverride**。引数のposgetterも呼ばない |
| setThundering, setThunderTime, setRaining, setRainTime, setTerrainType, setAllowCommands, setServerInitialized, setDifficulty, setDifficultyLocked | **実emptyoverride** |
| getGeneratorOptions, cleanWeather getter/setter、border getter/setter、setMapFeaturesEnabled, setGameType, setHardcore | subclass overrideなし。親の自分用state/bodyを使用。全getterがdelegateという規則ではない |

WorldServerMulti のsuper引数はdelegate.getWorldInfo→newDerivedWorldInfo、その後通常WorldServerctor。childはdelegate保持→delegateborderへlistener登録。listenerのsize/transition/center/warningtime/warningdistance/damageamount/damagebuffer通知はその都度childborderへ対応更新する。initはdelegateMapStorage→delegateScoreboard→provider別village load/create/world再接続。saveLevelは実emptyoverride。これによりMultiのtotal/daytime setterは何も変えず、通常WorldServer.tickを継承してもbaseworldtimeをdimension数だけ増やすことにはならない。

## WS-06 player／Entity／Item の保存 schema

本節はdisk containerとの接続を定める。全entity subclassのbodyは[ゲーム処理](04-gameplay.md)の対象であり、以下だけで全entityを復元できない。

### 6.1 共通 Entity

| key | 型 | 注意 |
|---|---|---|
| id | String | chunk Entity/TileEntity factoryのclass名。Entity.writeToNBTそのものの共通wrapperとoptional save wrapperを区別 |
| Pos | List<Double>3 | X/Y/Z |
| Motion | List<Double>3 | X/Y/Z |
| Rotation | List<Float>2 | yaw/pitch |
| FallDistance | Float | — |
| Fire, Air | Short | signed縮小、Airはwatcher getter |
| OnGround, Invulnerable | Byte | boolean |
| Dimension, PortalCooldown | Int | — |
| UUIDMost, UUIDLeast | Long | **二つとも正確Long型**なら優先してUUIDをnew |
| UUID | String、旧入力 | exactLong pairがないときのlegacy fallback |
| CustomName | String、省略可 | 非空name時 |
| CustomNameVisible | Byte | custom name分岐の保存、読込はboolean |
| Silent | Byte、省略可 | true時。isSilentはwatcher byte4==1 |
| Riding | Compound、省略可 | ridingEntityのoptional save成功時、再帰 |
| CommandStats | Compound、省略可 | 各typeのName/Objective String pair |

readは最初にPos→Motion→RotationのListを取得し、Motion各軸→abs>10なら各0→Posとprev/last positions→Rotationとprev→setRotationYawHead→setRenderYawOffset→FallDistance→Fire→setAir→OnGround→Dimension→Invulnerable→PortalCooldown→UUID→setPosition→setRotation→非空CustomName→CustomNameVisible→CommandStats.read→setSilent→virtual subclass read→shouldSetPosAfterLoadingならposition再適用、の順。途中ThrowableはCrashReport/ReportedExceptionへ包む。NaNはabs>10を満たさない。nativeのfinite/coordinate envelopeは追加環境policyであり、これを原版readの拒否条件にしない。全position hooks/CommandStats/Riding error pathはゲーム章の未充足と結ぶ。

CommandStats内のtypeはSuccessCount→AffectedBlocks→AffectedEntities→AffectedItems→QueryResult。各typeに`<type>Name`と`<type>Objective`の正確String pairがある場合だけreadする。片方missingは既存slotをclearしない。双方nonemptyなら共有NULL配列を必要時二つの新しいString[5]へ置換して直接store、空文字は当該pair除去へ。全pairが無くなれば二fieldsを同じ共有NULL配列へ戻す。writeはnewCompound→type順に双方nonnullのpairをStringでstore→非emptyならCommandStatsへattach。empty時は入力親の既存CommandStatsを自動removeしない。

Living保存はHealF:Float、Health:Short(ceil healthをint→short)、HurtTime/DeathTime:Short、HurtByTimestamp:Int、AbsorptionAmount:Float、Attributes:List<Compound>、optionalActiveEffects:List<Compound>。書込中は装備modifierをattribute mapから外す→attribute保存→装備modifierを再applyする。finallyによる原状復旧ではない。例外時のprefixに注意。

attribute entryはName:String,Base:Double,optionalModifiers:List。modifierはName:String,Amount:Double,Operation:Int,UUIDMost/Least:Longで、saved対象だけ。effectはId/Amplifier:Byte、Duration:Int、Ambient/ShowParticles:Byte。未知attribute/effect／modifiersの失敗・filterは個別classの契約が必要。

### 6.2 Player

Player読込はLiving read→**profile UUIDでsaved UUIDを置換**→Inventory→selected→sleep→XP→Score→必要なwake→Spawn→Food→Capabilities→optionalEnderItems。xpSeedが0ならplayer.rand.nextIntを消費。保存UUIDをauthenticated/profileUUIDより優先しない。

| key | 型 | 条件／意味 |
|---|---|---|
| Inventory | List<Compound> | main slots0..35、armor100..103 |
| SelectedItemSlot | Int | currentItem。NBTreadで一律0..8へclampしない |
| Sleeping | Byte | read trueならwake処理が入る |
| SleepTimer | Short | signed |
| XpP | Float | fractional XP |
| XpLevel, XpTotal, XpSeed, Score | Int | signed |
| SpawnX/Y/Z | Int、省略可 | readは3keysともnumericの時だけBlockPosをnew |
| SpawnForced | Byte | spawnありの場合 |
| foodLevel, foodTickTimer | Int | FoodStats |
| foodSaturationLevel, foodExhaustionLevel | Float | FoodStats |
| abilities | Compound | 次の7fields |
| EnderItems | List<Compound> | 27slots、Slot unsigned byte |
| SelectedItem | Compound、省略可 | currentstackとItem両方nonnullの保存補助。Inventoryの代替読込authorityではない |

abilitiesはinvulnerable/flying/mayfly/instabuild/mayBuild:Byte、flySpeed/walkSpeed:Float。readはabilitiesCompound存在がgate、speedはflySpeed numericのgateで両速度を読む、mayBuildは正確Bytegate。FoodStatsはfoodLevel numericがgateで四値を読む。missing全体を無条件0で上書きしない。

Inventory.readはmain36/armor4を**新しい配列**にしてからListを先頭順に読む。Slotは `getByte&255`、同slotの後のnonnullstackが勝つ。範囲外slotのstackも生成してから置き先判定。count0/negativeを空slotへ自動正規化しない。writeはnonnullmain昇順→nonnullarmor昇順でSlotByteを付ける。元コメントの「+80 crafting」はこの実bodyの保存対象ではない。

### 6.3 ItemStack／落下 Item

stackはid:String(resource name)、Count:Byte、Damage:Short、optionaltag:Compound。writeはregistry名が得られなければminecraft:air、count/damageをsigned幅へ狭める。tagは直接alias。readはidStringならname lookup、そうでなければ旧ShortID、Countはsigned、Damageはsignedshortを読みnegativeのみ0へ。tagCompoundを直接attachし、nonnullItemならupdateItemStackNBTを呼ぶ。unknownItemはfactoryがNULLstackとする読込経路と、packetのunknownnonnullstackとは別契約。

EntityItem subclassはHealth:Short(先にbyte縮小)、Age:Short、PickupDelay:Short、optionalThrower/Owner:String、optionalItem:Compound。readHealthはShort&255、Age/PickupDelayはsigned、Itemが無効なら死ぬ分岐を持つ。拾取delay40などはgameplayactionの値であり、毎保存を40へ書換える規則ではない。

**未充足 WS-GAP-ENTITY-NBT:** 全Entity／TileEntityのid registry、class-specific全tag、mount・leash・attributes・commandstats・mob equipment・spawn data・block entity tickと復元副作用は、この節のschemaで埋まっていない。Minecraftの全保存互換には全subclassを仕様化する必要がある。

## WS-07 saved data と地図

### 7.1 WorldSavedData／MapStorage

WorldSavedDataはnullable final mapNameとdirty=false。markDirtyはvirtualsetDirty(true)。readFromNBT/writeToNBTは抽象であり、空成功を用意しない。MapStorageはsaveHandler、loadedDataMap、loadedDataList、idCountsの四fields。ctorは各collectionnew→saveHandler代入→loadIdCounts。

loadData(class,key)はcacheget、nonnullhitはrequested classを検証せずそのexactobjectを返す。missではsaveHandlerがあればnamefile取得→exists→Stringconstructor反射→GZIPopen/read/close→`data`Compoundでvirtualread。constructor失敗はlocalNULLのまま、construct成功後のread/IO Exceptionはpartiallocalobjectを保持する。outercatchはExceptionを診断。nonnulllocalをmap.put/list.addして返す。Errorはこのcatchの対象外。別classをrequestedしたhitを後のcheckcastで失敗させる順を先回りしない。

setDataはcontainsKey→旧valueをmap.remove→list.remove(query equals)→newput→list.add。NULLkey/value、duplicate同objectが別keyでlistに複数ある状態をHashMapとArrayListの規則で保持する。list全dedupをしない。

saveAllDataはliveindex/size loop、各valueのvirtualisDirty→dirtyならsaveData→virtualsetDirty(false)。saveData内のwriteToNBT/IO Exceptionはcaught診断されるため、save失敗でも後段dirty=falseが呼ばれ得る。isDirty/setDirtyの例外はこの内部catchで握り潰さない。saveHandler=NULLではsaveDataは書かず、dirtyclearは行われる。

counterはnamespaceString→LAST signedShort。未存在は0、存在はshort(value+1)のwrap、先にmap.put、その後idcounts.datへ**raw**CompoundShortを保存、最後にcapturedlocalShortをsigned拡張して返す。例:32767→-32768、-1→0。IO失敗はcaught診断して同localを返す。counterをNEXTunsigned16のglobal一個へ変えない。NULLkeyはHashMapでは合法だがNBT writeUTFで保存失敗し得る。

loadIdCountsはtry内でidCounts.clear→handlerNULLならreturn→idcountsfile→exists→rawNBTread→close→keyiteration。各entryが**NBTTagShortのinstance**の場合だけShort値をimportし、他のnumericは無視する。後段Exceptionでもclear/既importのprefixは戻さない。getUniqueDataIdの保存は全namespaceをHashMapの現在順で新CompoundへsetShortし、open→rawwrite→close。個別mapdataのGZIPとは別である。

SaveDataMemoryStorage ctorはMapStorage(NULL)を実行。loadDataはcachegetのみ、setDataはmapputのみ（loadedDataListを更新しない）、saveAllDataは実emptyoverride、getUniqueDataIdは常に0（idCounts未変更）。World.isRemoteを見てこの振舞いを推測するのではなく、実storage subtypeでdispatchする。

### 7.2 MapData 保存

MapDataはWorldSavedDataを継承、xCenter/zCenter:int、dimension/scale:byte、newbyte[16384]colors→newplayersArrayList→newplayersHashMap→newinsertion-order mapDecorationsのinitializer順。viewerとdecorationsはNBTに保存しない。colorsはこのMapDataのmutablebyte[]であり、NBTwriteは直接alias、128×128のNBTreadも入力arrayを直接attachする。**S34 constructorは矩形pixelを別byte[]へcopyする**ので、packetのpixelarrayとMapData.colorsを同じ参照にしない。受信適用は既存MapData.colorsへ書き込む。

| `data` key | 型 |
|---|---|
| dimension | Byte |
| xCenter,zCenter | Int |
| scale | Byte |
| width,height | Short、write各128 |
| colors | ByteArray |

readはdimension→centers→scaleをsignedbyteで取得して0..4clamp→width/heightShort。両方128ならcolorsはexactNBTbyte[]を直接attachし、length16384へ補正しない。異なるサイズではfresh16384を作り、center offsetをJava除算で算出してrow/column copy。suppliedbodyのbounds条件は `>=0 || <128` であり、正常化してANDへ修正しない。この条件による異常サイズarrayfailureは原版prefixを持つ。

map centerはcellsize `128*(1<<scale)`、各axisで `floor((coordinate+64)/cellsize)*cellsize+cellsize/2-64`。Java shift/cast/overflow位置を保持する。ItemMap.loadMapData staticはremoteに関係なくsignedidから"map_"key→class→World.loadItemData→checkcast→NULLならnewMapData/setItemData→exactref。ItemMap.getMapDataのremote miss NULLと混同しない。

地図pixelはunsignedbyte解釈でcolorIndex=byte/4、shade=byte&3。MapColor arrayは64slot、named0..35、残NULL。RGB multiplierはshade0=180,1=220,2=255,3=135、それ以外の直接method引数は220。各channel `(channel*multiplier)/255` のinteger結果、alpha255。index0の透明checker等のrenderer処理はクライアント章に属する。

### 7.3 MapInfoのdirty／packet cadence

MapInfoはouterMapDataへの参照、finalentityplayerObj、dirty=true、minX/minY=0、maxX/maxY=127、clean送信counter=0、測量counterfield_82569_d=0。ctorbodyはplayerの直接代入だけで、NULLplayerも保持する。

getPacket(stack)のdirty分岐は**dirty=false store→S34 NEW→stack.metadata→outer.scale→outer.decorations.values view→outer.colors→minX/minY→maxX+1-minX/maxY+1-minY→constructor**。NEWや後のgetter/collection/copyが失敗してもdirtyはfalseのまま。clean分岐は旧counterを取得→signedint32 counter++を先store→旧counter%5==0ならicons-onlyS34(width/height0)、それ以外NULL。初回cleancallからpacketを返し、dirtypacketはclean counterを増やさない。negativecounter/overflowにもJava remainderを使う。

MapInfo.update(x,y)はdirtyならminX→minY→maxX→maxYの順にMath.min/max。cleanならdirty=true→minX=x→minY=y→maxX=x→maxY=y。座標を0..127へclampしない。MapData.updateMapDataは先にWorldSavedData.markDirty（virtualsetDirty）を呼び、次にplayersArrayListのiterator順で各MapInfo.update。save用dirtyとviewer用dirtyは別fieldsである。

getMapInfo(player)はplayersHashMap.get→NULLならnewMapInfo→map.put→list.add。getMapPacketはmap.get→NULLならNULL、そうでなければgetPacketを呼ぶ。World引数はそのbodyでは使用しない。Entityのhash/equalsによるplayerkeylookupとlistの独立保持を、単一Cpointermapだけへ縮約しない。

S34 constructorはid→scale→`Collection.size→newVec4b[]→toArray→cast`→min/maxfields→newbyte[width*height]→X外/Y内のpixelcopy。iconは新array内に同Vec4b参照、colorsは新pixelarrayの値copy。setMapdataToはscale→decorations.clear→icon順に`icon-<i>`keyで同参照put→X外/Y内でpixelsを書込。range/lengthを後から一括preflightして全変更をundoする仕様ではない。wire細部は通信章。

**未充足 WS-GAP-MAP:** ItemMap.updateMapDataの全測量、HashMultiset→copyHighestCountFirstのtie順、property-sensitive getMapColor、updateVisiblePlayers/updateDecorationsの全virtual/read順、frame/scaledmap全live owner移行、S34→rendererの全統合はこのschemaとcadenceだけでは閉じない。currentnative first-seen tieや色tableを規範へ昇格しない。

## WS-08 座標・BlockState・Material

### 8.1 座標と参照

通常世界の有効blockは `-30000000<=x,z<30000000`、`0<=y<256`。chunk localはx/z&15、section=y>>4、localY=y&15。WorldBorderはこの固定validityと別の制約。無効getBlockStateはcurrentBlocks.air.defaultState。全methodが同じ無効値を返すわけではない。

BlockPosはVec3i親座標、MutableBlockPosは親を持ち別shadowx/y/zをvirtualgettersで読む一つのobject。immutablezero-offsetaddはsame ref、nonzeroaddはNEW→Xgetter/add→Ygetter/add→Zgetter/add→constructor。Javaintwrap、NEW前後のexception/allocation効果を保持する。toLongはX26bitを38shift、Y12bitを26shift、Z26bitを低位へpack、fromLongは各fieldを符号拡張。Y有効高さとpackedY12bit signed範囲は別。

ChunkCoordIntPairのlongkeyはlow32=Xunsignedbits、high32=Zunsignedbits。chunkkeyをpackedBlockPosと交換しない。Hash/equals/orderingは用途別に保ち、negative coordでも同じmask/shiftを使う。

World.getChunkFromBlockCoordsは同じWorld/posをreceiverとしてXgetter→signed>>4→Zgetter→signed>>4→publicgetChunkFromChunkCoords。座標getter以前にchunkProviderを捕捉しない。coordsのbaseはその時点のchunkProviderをreceiverとしてprovideChunkを呼び、NULL結果も直接返す。isChunkLoadedはcurrentprovider.chunkExists→trueならallowEmpty短絡、必要時だけ再読currentprovider.provideChunk→isEmpty。chunk座標lookupとisBlockLoadedのvalidity判定を同じAPIへ統合しない。

### 8.2 二種類のstate番号

| 用途 | 変換 |
|---|---|
| `Block.getStateId(state)` / 逆変換 | `blockRegistryID + (metadata<<12)`、low12がblock |
| `Block.BLOCK_STATE_IDS` identity登録 | `(blockRegistryID<<4)\|getMetaFromState(state)`、low4がmetadata |
| EBS char／ChunkPrimer short／chunk network／Anvil分解 | 二番目のidentity登録番号 |

元Block registryの実block順、各BlockStateのvalidStates生成順にidentitymapへ登録。metadataの穴は存在し、異なるvalidstateが同番号へputされるclassもある。単純なblockID>>4のscalarだけでは元IBlockStateのproperty/identityを再現しない。未登録stateのidentitylookupは-1、逆listの欠損はNULL。holeがどのair fallbackを使うかは呼出classごとの規則。

BlockState propertyは名前によるsort、allowedValues、Cartesianproduct、immutablepropertymap、transitiontableから構築する。propertyを同名Stringだけのownerにしない。withPropertyはcanonicalstateへ移り、毎回同値のstateをnewしない。全198block登録のconstructor/default/property/metadata/stateID変換表はゲーム章とWS-GAP-BLOCK-STATEで閉じる必要がある。

### 8.3 Materialの実stateとoverride

MaterialはcanBurn=false,replaceable=false,isTranslucent=false,materialMapColor=ctor引数,requiresNoTool=true,mobilityFlag=0,isAdventureModeExempt=falseの7fields。constructorはrequiresNoTool initializerと色代入、他fieldsをresetしない。色NULLはSource合法。四namedsubclassとweb匿名には追加instancefieldなし。

| class | isLiquid | isSolid | blocksLight | blocksMovement | ctorの追加 |
|---|---|---|---|---|---|
| Material | false | true | true | true | — |
| MaterialLiquid | true | false | **true（base）** | false | replaceable=true→mobility=1 |
| MaterialTransparent | false | false | false | false | replaceable=true |
| MaterialLogic | false | false | false | false | adventureExempt=true |
| MaterialPortal | false | false | false | false | — |
| web匿名 | false | true | true | false | 宣言chainでrequiresTool/mobility1 |

isOpaqueはtranslucentならfalse、そうでなければvirtualblocksMovement。requiresToolはrequiresNoTool=false、burningはcanBurn=true、replaceableはtrue、nopushは1、immovableは2、setterはsame receiver。isAdventureModeExempt fieldに対応する公開getterを発明しない。

35個のnamedstaticsは宣言順に別allocation。以下のchainは左から順に行い、最後にstaticへstore。MapColor namedrefは同じcolor値であっても別物へ再生成しない。最初airもMaterialTransparentのNEWがMapColor.airColor引数の初期化より先。

| static | runtime class | MapColor | 追加chain（base/subclassctor後） |
|---|---|---|---|
| air | Transparent | air | — |
| grass | base | grass | — |
| ground | base | dirt | — |
| wood | base | wood | burning |
| rock | base | stone | requiresTool |
| iron | base | iron | requiresTool |
| anvil | base | iron | requiresTool→immovable |
| water | Liquid | water | nopush（ctorでも実行） |
| lava | Liquid | tnt | nopush（ctorでも実行） |
| leaves | base | foliage | burning→translucent→nopush |
| plants | Logic | foliage | nopush |
| vine | Logic | foliage | burning→nopush→replaceable |
| sponge | base | yellow | — |
| cloth | base | cloth | burning |
| fire | Transparent | air | nopush |
| sand | base | sand | — |
| circuits | Logic | air | nopush |
| carpet | Logic | cloth | burning |
| glass | base | air | translucent→adventureExempt |
| redstoneLight | base | air | adventureExempt |
| tnt | base | tnt | burning→translucent |
| coral | base | foliage | nopush |
| ice | base | ice | translucent→adventureExempt |
| packedIce | base | ice | adventureExempt |
| snow | Logic | snow | replaceable→translucent→requiresTool→nopush |
| craftedSnow | base | snow | requiresTool |
| cactus | base | foliage | translucent→nopush |
| clay | base | clay | — |
| gourd | base | foliage | nopush |
| dragonEgg | base | foliage | nopush |
| portal | Portal | air | immovable |
| cake | base | air | nopush |
| web | anonymous | cloth | requiresTool→nopush |
| piston | base | stone | immovable |
| barrier | base | air | requiresTool→immovable |

Material.blocksLightとBlock.getLightOpacity、Material.colorとBlock.getMapColor(state)は別。wool、planks、stone、stained等のproperty-sensitiveoverrideをMaterial色一個へ縮約しない。currentnative registryfactsはこのclosureのexplicitadapterであり、全Block翻訳の代用品ではない。

## WS-09 Chunk の内部所有・読取・変更

### 9.1 constructor/defaults

Chunkの22instancefieldsはsection[16]、biomebyte[256]、precipitationheightint[256]、updateskybool[256]、loadedflag、worldref、heightint[256]、finalchunkX/Z、gapflag、tilemap、entitylists[16]、terrainflag、lightflag、field_150815_m、modifiedflag、hasentities、lastSaveTime、heightMinimum、inhabitedTime、queuedLightChecks、tilePositionQueue。collection/arraysは独立した実owner。通常ctor順はsection→biome→precipitation→skybools→tilemap→queuedLightChecks4096→queue→entitylistarray→world/X/Z→heightarray→16個のClassInheritanceMultiMap<Entity>→precipitationを-999→biomesを-1。他scalarsはallocationdefault。

primerctorはこのctorを先に実行、provider.hasNoSkyを取得後、x0..15→z0..15→y0..255の順で全65536statesを読む。state.block.material != **Material.air identity**のときだけsectionを必要時newしてEBS.set。nonair判定をID!=0で代用しない。全airの普通ChunkとEmptyChunkは別class/owner。

ChunkPrimerはshort[65536]、index `(x<<12)|(z<<8)|y`。getはcombinedindex範囲だけ検査し、座標を個別clampしない。保存shortはsigned promotionされるため32768..65535がnegative lookupになり得る。hole/negativeはprimerがcapturedしたdefaultair。setはidentitymap IDをshort縮小し、unregistered -1もFFFFとして保存する。

EBSはyBase,blockRefCount,tickRefCount,char[4096],blockNibble,optionalSkyNibble。getholeはcurrentBlocks.air.defaultState。setはoldstate取得→oldblock非air/oldrandomのcounts減算→newblock非air/newrandomのcounts加算→newstateID lookup/char store。後段失敗でcounts前段をundoしない。removeInvalidBlocksはcounts0にしてx→y→zの順でget/read/count。holeのrawcharをair0へ書換えない。isEmptyはblockRefCount==0、needsRandomTickはtickRefCount>0。

NibbleArrayは2048byte、index `(y<<8)|(z<<4)|x`、byteindex=index>>1、evenlow4、oddhigh4。setはvalue&15、反対nibbleを保持。arrayctorは引数ref代入後length==2048を検査する。arraygetter/setDataはdirectref、recountやdeepcopyを自動実行しない。

### 9.2 get/height/top

getBlockStateはDEBUGなら固定bedrock/displaystate分岐、通常はY範囲/sectionを見てEBS、missingはair。Block/State取得中ThrowableはReportedExceptionへ包む経路がある。全getをhealthyNULLで統一しない。

getTopFilledSegmentはsectionarrayを15→0に走査して最初の**nonnull**のyBase、なし0。EBS.isEmptyはこの選択に使わない。heightMapindex=z*16+x。generateHeightMap/SkylightMapはx→z、各columnのprecipitation=-999、topfilled+16から1へ下がって`opacity!=0`なblockのy+1をheightにする。見つからない列で既存heightを必ず0へresetするbodyではない。heightMinimumはINT_MAXから発見heightのmin。最後modified=true。

World.getHeightはXZ有効か→isChunkLoaded(allowEmpty=true)→chunkheight、unloaded0、XZ無効ならseaLevel+1、最後newBlockPos(元X,height,元Z)。元posのYを使わない。World.getTopSolidOrLiquidBlockはcapturedchunk、y=topfilled+16からdown候補を読み、**blocksMovement&&material!=leaves**で止めて候補の一つ上を返す。名前からisLiquidを条件へ追加しない。

### 9.3 setBlockState の変更順

Chunk.setBlockStateはlocalXZ/Y→precipitationcacheinvalidate→oldheight→oldstate取得。stateidentityが同じならNULL。missingsectionでnewblock==Blocks.airならNULL、それ以外はEBSをnew/attachしnewsectionheightflagを算出→EBS.set。

blockclassが変わればserverでold.breakBlock、remoteでoldがTileProviderならremoveTileEntity。その後EBSからblockを再読し要求blockと違えばNULL。newsectionheightflagならgenerateSkylightMap、そうでなければopacityとoldheightに応じてrelight、必要時skylightcolumnflag。oldTileProviderのcachedtile.update→serverかつblock変更ならnew.onBlockAdded→newTileProviderの既存tile取得/必要時create+world.set→tile.update→modified=true→oldstateを返す。

World.setBlockStateはvalidity→serverDEBUG拒否→chunklookup→newblockcapture→chunk.set。戻りNULLならfalse。成功後old/newopacityまたはemission違いでcheckLight。flag2かつ(!remoteまたはflag4なし)かつchunk.isPopulatedならmarkBlockForUpdate。serverflag1ならneighbor notify、newblockcomparatoroverrideならcomparatorupdate。最後true。callbackが同state/sectionを変更し得るため再読箇所を固定snapshotへ変えない。

### 9.4 network fill / cache lifecycle

S21のouterwireは通信章参照。Chunk.fillChunkはmaskのsection昇順で、全stateplane→全blocklightplane→skyありなら全skylightplane→fullなら256biomes、という順。各stateは**little endian unsigned16**。NBTのBlocks/Data/Addとは違う。

maskありmissingsectionはnewして即attach、getDataの実lengthだけ二byteずつstore。fullでmaskなしは既存sectionをNULLへ。lighting arraycopyは既存nibblebyte[]へ、biomeも既存arrayへcopy。selectednonnullsectionをremoveInvalidBlocks→light/terrainflags=true→generateHeightMap→全tile.updateContainingBlockInfo。余分なinputbytesをこのbodyは検査しない。不足入力で途中state/lightが変わるprefixを保持する。

NetHandlerPlayClient.handleChunkDataはthreadguard後、full&&mask==0ならdoPreChunk(false)で**payloadが256でもunload**しreturn。full&&mask!=0はdoPreChunk(true)、invalidate→chunklookup→fill→renderupdate、partialまたはproviderがSurface以外ならresetRelightChecks。mask0/256を「空chunk再作成」とする後代の推測を混ぜない。

ChunkProviderClientはsharedEmptyChunk(world,0,0)、LongHashMapcache、list。provide未存在はexactsharedEmpty。loadはnewordinaryChunk→mapadd→listadd→loadedtrue。二度load同座標ではmapを置換してもlistの旧entryを一律除去しない。unloadはprovide→!chunk.isEmptyならonChunkUnload→mapremove→listremove(capturedchunk)。allairordinaryはEmptyChunkではなく、isEmptyのoverrideを無言でnonaircountへ置換しない。

Clienttickcacheはlist順にchunk.func_150804_b(elapsedMillis>5)を呼ぶ。時間計測開始とgetter再評価を保持する。servercacheのloadはdrop集合remove→cache→disk→missgenerator/dummy→cache/list登録→onChunkLoad→populate。provideはfindingSpawnPoint/overrideでdummyを選ぶ。saveChunksはlistcopy、fullならextra、needsSavingならsave→modifiedfalse、非fullでは24件でfalse。unloadは最大100dropkeys、onUnload→savedata→extra→cache/listremove→dropremove、最後loader.chunkTick→generator.unload。

needsSaving(force)はforce=trueなら `(hasEntities && totalTime!=lastSaveTime) || isModified` を判定、force=falseなら `hasEntities && totalTime>=lastSaveTime+600L` を判定し、該当しなければisModifiedを返す。long加算のwrapとworld time getterの到達箇所を保持する。通常Chunk.isEmptyは常にfalseであり、EBSのempty判定とは別。

neighborpopulateは隣の存在を北→東→南→西→北西→南東→南西→北東の順に先取得。その後current/east/south/southeast、west/south/southwest、north/east/northeast、northwest/north/westが揃う各分岐を元順に実行。terrainpopulatedならpopulateChunk、未ならpopulate。populationを単にnewchunk直後一回へ移さない。

Chunk.getRandomWithSeed(s)はworldseedへ `long(int32(x*x*4987142))`、`long(int32(x*5947611))`、`long(int32(z*z))*4392871L`、`long(int32(z*389711))` を左から足したsigned64結果をsとxorしてnewRandomする。X側の二乗後係数乗算はint32、Z側4392871の乗算はlongであり、全項を最初から64bitで計算する代用は異なる。

## WS-10 光・高さ更新

### 10.1 光値の読取

Sky/block lightは0..15nibble、EnumSkyBlockのdefaultはSKY15/BLOCK0。World.getLightForはnegativeYを0のnewposへ、invalid/unloadedはenumdefault、それ以外chunkへ。noSkyのSKYを先に0にするのはgetLightFromNeighborsForの別分岐。ChunkmissingsectionではcanSeeSkyならenumdefault、見えなければ0、既存sectionではSky/noSkyまたはBlocknibble。全未ロードを0で統一しない。

World.setLightForはvalid→blockloadedの時だけchunk.set→world.notifyLightSet。Chunk.setLightForはsectionmissingならnewEBSを即attach→generateSkylightMap→modified=true、次にSkyならnoSkyでない時だけskywrite、Blockならblockwrite。unknown enum／noSkySkyでも前段section生成/modifiedを省略しない。World.checkLightはskyありならSkyを先実行し、その結果に関係なくBlockも実行してboolean ORを返す。

neighborbrightnessはup→east→west→south→northの順でmax、downは読まない。`getLight(pos,true)`は先にXZbounds、neighborbrightnessblockを検査、**negativeYなら0を即return**、Y>=256を255のnewposへ。XZ外は15。`getLight(pos)`単引数もnegativeYなら0／highYは255だが、XZboundsを同じ位置では検査せずchunkのskylightSubtract0で読む別method。通常ambient lightは `max(blockLight,skyLight-skylightSubtracted)`。combinedpackedlightは `sky<<20 | max(block,minimumEmission)<<4`。

### 10.2 generateSkylightMap／relight

skylight generationはheight更新後、skyありなら列ごとにlight15/topfilled+15。現在blockopacityを引くが、opacity0かつlightが既に15でなければ減衰1。light>0ならnonnullsectionにwriteしworld.notifyLightSet。次yへ下げ、y<=0またはlight<=0で終了。noSkyではこれらskywriteを行わずheight/modifiedは更新する。

relightBlockはoldheightを&255、candidate=max(requestY,oldheight)から下のopacity0を走査。heightが変わればworld.markBlocksDirtyVertical→heightstore。下降した範囲はsky15、上昇範囲はsky0を書き、次にcandidateの下へlight15からopacity(max1)を減算/clamp0。heightMinimum更新→horizontalneighborsと自身の高さ区間へskycheck→modified。heightmapとlight arrayを別transactionで更新する仕様ではない。

gapは更新columns flagを持ち、areaLoaded(center,16)の時だけx→z走査。flagclear→ownheight→horizontalneighborlowesthorizonのmin→自身/四隣のheight差区間check。fast booltrueでは一column後returnしgapflagの後段clearは未実行。通常全走査後gapfalse。

### 10.3 World.checkLightFor 二段queue

areaLoaded(pos,17,false)がfalseならfalse、変更なし。32,768intの既存queueを再利用。offsetを+32した6bitX/Y/Z、darkening時はexpectedlight4bitを18shiftにpack。中心offsetは133152。

rawlightはSkyかつcanSeeSkyなら15。それ以外currentblockを読み、base=Sky0/Blockemission、opacity=blockopacity。opacity>=15でもemission>0ならopacity=1、opacity<1なら1。opacity>=15なら0、base>=14ならbase、それ以外EnumFacing.values順でneighborLight-opacityのmax、14到達なら早期return。

1. newraw>currentなら中心をqueueへ。
2. newraw<currentなら中心+expectedcurrentをqueueへ。queueをFIFO走査、actual==expectedならlightを0へ。expected>0かつ中心からManhattan距離<17なら六隣を元Facing順で読み、neighbor==expected-max(1,neighboropacity)でqueue容量内ならappend。
3. darken後readindexを0へ戻し、同queueをFIFO再走査。各位置actual/rawを再読、違えばrawstore。rawが増加、距離<17、容量がlength-6より小さいならwest→east→down→up→north→southを読み、隣light<rawだけappend。
4. 正常終了true。trueは値が変わったことを意味しない。

queueはvisited setではない。duplicates、容量による切捨て、減光途中の0と再計算順、六隣の順を保持する。別の「収束するまで全域BFS」で同じ最終光になるとしてもcallback／tick/prefix互換ではない。

relight round robinはqueuedLightChecksを0へreset可能、tickごと最大8entries、4096で終了。entryからsection=`q%16`,x=`q/16%16`,z=`q/256`、先にcounter++、各entry内localY0..15。missingsectionは境界blockだけ、存在sectionはairMaterialだけ、六隣emission>0にcheckLightした後自身をcheck。full lightpopulation／func_150811_f等の全failure/neighbor伝播はWS-GAP-LIGHTの残部。

## WS-11 世界の初期化・時計・tick

### 11.1 owner／constructorの順

Worldの40instancefieldsを次のgroupで保持する。finalでも参照先array/list/metadataはmutableであり、全体を独立scalar DTOへ二重化しない。

| group | fieldsと型 |
|---|---|
| 基本 | seaLevel:int、scheduledUpdatesAreImmediate:boolean |
| entity/tile lists | loadedEntityList、unloadedEntityList、loadedTileEntityList、tickableTileEntities、addedTileEntityList、tileEntitiesToBeRemoved、playerEntities、weatherEffects |
| index/render | entitiesById:IntHashMap、cloudColour:long、skylightSubtracted:int |
| random/weather | updateLCG:int、DIST_HASH_MAGIC:int、prevRainingStrength/rainingStrength/prevThunderingStrength/thunderingStrength:float、lastLightningBolt:int、rand:Random |
| 接続owner | provider、worldAccesses:List、chunkProvider、saveHandler、worldInfo、findingSpawnPoint:boolean、mapStorage、villageCollectionObj、theProfiler |
| その他owner/tick | theCalendar、worldScoreboard、isRemote:boolean、activeChunkSet:Set、ambientTickCountdown:int、spawnHostileMobs/spawnPeacefulMobs/processingLoadedTiles:boolean、worldBorder、lightUpdateBlockList:int[] |

WorldはseaLevel63、八つのEntity/TileEntity/player/weather list、entitiesById map、cloudcolor16777215、updateLCG用temporarynewRandom.nextInt、別world.randnewRandom、worldAccesseslist、Calendar、Scoreboard、activeChunkSetをdeclared initializer順に構築する。ctorbodyはworld.rand.nextInt(12000)ambient→spawnHostile/Peacefultrue→int[32768]lightqueue→savehandler/profiler/info/provider/remoteassign→provider.getWorldBorder。worldseedからworld.randをsetSeedするbodyではない。

WorldProviderの9fieldsはworldObj、terrainType、generatorSettings、worldChunkMgr、isHellWorld、hasNoSky、lightBrightnessTable(float[16])、dimensionId、colorsSunriseSunset(float[4])。二arrayを別allocationし、他はallocationdefault。staticmoonPhaseFactorsも同値の別arrayを返さず、そのmutablearray参照を保つ。Surface/Hell/Endはこれを継承する同じ最派生ownerである。

Provider.registerWorldはworldObj代入→capturedworldIn.getWorldInfo().terrainType→同worldInから再getWorldInfo().options→virtualregisterChunkManager→virtualbrightnessTable。registermanagerが現在this.worldObjを交換できるので最初のworldInと後のfieldを混ぜない。

WorldClientの追加6fieldsはsendQueue、clientChunkProvider、entityList、entitySpawnQueue、mc、previousActiveChunkSet。親ctor後、newentityList→newspawnQueue→Minecraft singleton取得→newpreviousChunkSetのinitializerを実行。sendQueue/cacheはこの時点でNULL。

WorldClientはsuper引数を左からnewSaveHandlerMP→newWorldInfo(settings,"MpServer")→providerfactory(dimension)→profiler→trueとしてWorldを実行。childinit後sendQueue→difficulty→spawn(8,64,8)→provider.registerWorld→createChunkProvider→newSaveDataMemoryStorage→initialSkylight→initialWeather。remoteboolだけではmemoryprovider構築を再現できない。

WorldServerの追加16fieldsはmcServer、theEntityTracker、thePlayerManager、pendingTickListEntriesHashSet、pendingTickListEntriesTreeSet、entitiesByUuid、theChunkProviderServer、disableLevelSaving、allPlayersSleeping、updateEntityTick、worldTeleporter、mobSpawner、villageSiege、blockEventQueue、blockEventCacheIndex、pendingTickListEntriesThisTick。親ctor後の明示initializerはhashset→TreeSet→UUIDmap→SpawnerAnimals→VillageSiege(this)→二つの別ServerBlockEventListを持つarray→thisTicklist。この段階のvirtualcallで見たchildfieldのpartial状態を、ctor末尾に全部zero-resetしない。

WorldServerはWorldsuper→childdeclaredstate→serverref→EntityTracker→PlayerManager→providerregister→createChunkProvider→Teleporter→initialSky→initialWeather→borderworldsize。`init()`でMapStorage(savehandler)、village load/new、ServerScoreboard/saveDataを接続しbordermetadataを反映。ctorとinitを一つの成功stubへまとめない。

### 11.2 providerの分岐・空の本体

surface dimension0、Hell-1、End1。providerfactoryのそれ以外はNULLのため到達したcallerで失敗し得る。HellはnoSky/isHelltrue、hellBiome manager、Hellgenerator、angle0.5、respawn不可。EndはnoSkytrue、skyBiome manager、Endgenerator、angle0、spawn(100,50,0)、ground50、respawn不可。surfaceはflat／debug／customized／normalをactualWorldType identityで分岐し、biome manager／generator constructorを呼ぶ。

brightnessTable16は各iについてfloat `f=1-i/15`, `brightness=((1-f)/(3*f+1))*(1-base)+base`。surface/Endbase0、Hellbase0.1。cloudheight等のrenderleafはクライアント章で閉じる。

surface celestialはsignedtime remainder24000→binary32 `(remainder+partial)/24000-0.25`→negativeなら+1→>1なら-1。wrapped値をuとして保持、cosine値v=`1-(cos(double(u)*π)+1)/2`をfloatへ。戻りは `u+(v-u)/3`、元のfloatroundingを保持。supplieddecompileの`f+(f-f)/3`を採用しない。moonphase=`(int)(time/24000%8+8)%8`、phasefactor=[1,.75,.5,.25,0,.25,.5,.75]。

skylightSubtractedはcelestial→lookupMathHelper.cos→`1-(cos*2+0.5)`を0..1clamp→1-値→rain効果×`1-(rain*5)/16`→thunder効果同式→1-値→float11倍のintcast。rain/thunder補間の順と各float/doublecastを保持する。初期sky計算は初期weatherより前。baseweatherだけでなくWorldClientのemptyupdateWeatheroverrideも実Sourceである。

### 11.3 server/client tick outer order

World.base.tickはupdateWeatherのみ。WorldServer.tickは次の順。

1. baseweather、hardcoreならdifficultyHARDを必要時設定。
2. biome manager cache cleanup。
3. 全player sleep成立なら、doDaylightCycle時daytimeを次24000境界へ、wake/resetweather。
4. gamerule等が許せばmobspawn。peaceful周期はtotalTime%400。
5. queuedchunks unload。
6. sky subtraction(partial1)算出、変更時fieldstore。
7. totalTime+1、doDaylightCycleならdayTime+1。
8. scheduled ticks(false)→blocks/chunks→PlayerManager instances→villages→siege→portal stale cleanup(totalTime)→queuedblockevents。

WorldServer.updateEntitiesはplayerなしのcounterで1200以降earlyreturn、playerありならreset後World.updateEntities。上のtickとupdateEntitiesはserver外側loopで呼ぶ別operation、entityupdateを上のstep8へ適当に埋めない。外側server全phaseは実行モデル/ゲーム章の未充足も参照。

WorldClient.tickはbase→totalTime+1→gameruleならdayTime+1→spawnqueueから最大10re-entry（先remove、loadedlistになければspawn）→clientchunkcachetick→updateBlocks。clientのtimeはserverpacketによる更新もあり、非正daytimepacketのgamerule切替は通信章。frameからnative50msを回す現在Cと、原版Timer/Minecraft fulltickは区別する。

### 11.4 weather

provider.noSkyならbaseweather全体earlyreturn。serverはcleanWeather>0でdecrementし、rain/thunderTimeを現在flag?1:2へ。thunderTime<=0ならthundering時nextInt12000+3600、非時nextInt168000+12000。positiveは--し0でflagtoggle。rainはraining時nextInt12000+12000、非時nextInt168000+12000。各strengthはprevを先store→double±0.01→floatcast→clamp0..1。天候のrandはworld.rand、terrain generator.randでもentity.randでもsharedMathでもない。

### 11.5 active/random block tick

activeChunkSetをclear、playerlistliveindexedloop、各floor(posX/16),floor(posZ/16)を中心にrenderDistanceの正方形coordsをadd。HashSetのiteration順を固定XZsortへ変更しない。ambientcountpositiveなら--。playerがあればworld.randで一人を選びnextInt11を三回で周囲positionを作りcheckLight。

serverupdateBlocksはそのbase処理後、active set iteration。DEBUGはchunktickだけ。通常はmood/checklight→chunk.func_150804_b(false)→lightningchance(nextInt100000)→ice/snow/rainchance(nextInt16)→randomticks。chance条件のshortcircuitとworld.rand消費は実block/天候条件で異なる。

速いblock位置選択はsigned32LCG `s=3*s+1013904223`、`r=s>>2`、x=r&15,z=(r>>8)&15,y=(r>>16)&15。各nonnull/random-neededsectionを配列順に、randomTickSpeed回更新してEBSstate→block.getTickRandomly→randomTick(world,pos,state,world.rand)。randomTickSpeed<=0はloop0回、巨大値をnativebudgetで制限するなら別policy。

### 11.6 scheduled tick

NextTickListEntryはprocessglobalentryIDを消費、position/blockを直接保持。equalsはpositionとBlock.isEqual、hashはposition。sorted比較はtime→priority→entryID。priority差はsigned32減算のwrap。unorderedsetとTreeSetは別stateで、size不一致はIllegalStateException。

updateBlockTickはentryをnewしてから即時flag/materialを調べる。即時かつnonairでrequiresUpdatesならradius8areaLoaded→currentstate非airかつexactblockならupdateTick、**area未loadまたはstate不一致でもこのbranchからreturn**。requiresUpdatesでない即時nonair分岐はdelay1。通常areaLoaded後、nonairだけscheduledTime=delay+worldtotalとpriorityをsetするが、**airもhashsetに未存在ならhash→treeadd**され、time/priorityはallocationdefault0のまま。scheduleBlockUpdateのdisk復元版はpriority先store、nonairだけtime設定、areaLoadedのgateなしでairも登録する。

tickUpdatesはDEBUGfalse。最大1000entriesをearliestから取り、forceでないfutureはbreak。各entryをtree/hashからremove→thisTicklistadd。thisTickをiteratorで先remove→areaLoaded(currentpos)→currentstate非airかつBlock.isEqualならupdateTick。Throwableはcrashreport/ReportedException、unloadedならdelay0でreschedule。成功時listclear、returnはtreeが非emptyか。失敗済entryの除去やそれまでのblockchangesをundoしない。

### 11.7 未閉鎖の世界tick依存

World.updateEntitiesのweatherEffects／unloaded cleanup／riding／entity iteration／tile removals／tile tick／pending tile append、block event二重queue、neighbor notificationのFacing順、redstone/comparator、fluids、fire、crop、falling、portal、mob/village/siege等の全leafはまだ本章に完全なmethod契約がない。emptybasebodyと未翻訳failureを区別し、nativeの「動かないが成功」をその代用にしない。

## WS-12 terrain／biome／noise

この節は通常生成のouter flowと基礎ノイズを実装できる粒度へ記述するが、WS-GAP-GENを含むため最終terrain出力の完全仕様ではない。同じseedで似た山ができるだけのnoise置換を互換としない。

### 12.1 generator construction と通常chunk

ChunkProviderGenerateはworld→features→currentterrainType→newRandom(seed)→ImprovedOctaves16,16,8→SimplexPerlin4→ImprovedOctaves10,16,8の順に同じrandomを使う。constructorのノイズperm作成が次のrandomstateを決める。densitydouble[825]、parabolicfloat[25]、weight=10/sqrt_float(dx²+dz²+0.2F)、dx/dz=-2..2。nonnulloptionsをJSONfactoryにparseしsettings/oceanblockを決めworldseaLevelをsetする。optionsNULLでは到達後のsettingsNULLが失敗を生み得るので自動default補完しない。

provideChunk(x,z)はgenerator.rand.setSeed(`x*341873128712L+z*132897987541L`)→newPrimer→bareterrain→16×16biomes取得→surface replacement→caves→ravines→mineshafts→villages→strongholds→temples→monuments→newChunk(primer)→256biomebytes→generateSkylightMap。各featureflagとglobalmapFeaturesがgate。worldseedはノイズpermと別populationのseedに使われ、このperchunkseed式へ余分にxorしない。

bareterrainはbiome10×10をchunkcoord*4-2で取得、density5×33×5をchunkcoord*4で作る。各4×4cell、32verticalcells、1cellを4×8×4へ増分補間。density>0はstone、それ以外Y<seaLevelは設定oceanblock、残りはprimer既存air。loop順・段ごとのdouble加算を保持し、まとめたtrilinear式でFMA/再結合しない。

surface replacementはPerlinSimplex4の16×16arrayをscale0.0625で作る。x外→z内、biomearray/stonenoise添字はz+x*16、biome.genTerrainBlocks(world,同rand,primer,worldX,worldZ,noise)。grass/dirt/sand/bedrock処理は各biomevirtualbodyへ依存する。

### 12.2 default settings

| noise/shape項目 | value |
|---|---:|
| coordinateScale, heightScale | 684.412F |
| upperLimitScale, lowerLimitScale | 512F |
| depthNoiseScaleX/Z/exponent | 200F /200F /0.5F |
| mainNoiseScaleX/Y/Z | 80F /160F /80F |
| baseSize, stretchY | 8.5F /12F |
| biomeDepthWeight/Offset | 1F /0F |
| biomeScaleWeight/Offset | 1F /0F |
| seaLevel, fixedBiome, biomeSize, riverSize | 63 /-1 /4 /4 |
| useLavaOceans | false |
| caves/dungeons/strongholds/villages/mineshafts/temples/monuments/ravines/waterLakes/lavaLakes | 各true |
| dungeonChance/waterLakeChance/lavaLakeChance | 8 /4 /80 |

| ore | veinSize | count | minY | maxY（exclusive sampling bound） |
|---|---:|---:|---:|---:|
| dirt | 33 | 10 | 0 | 256 |
| gravel | 33 | 8 | 0 | 256 |
| granite/diorite/andesite | 各33 | 各10 | 0 | 80 |
| coal | 17 | 20 | 0 | 128 |
| iron | 9 | 20 | 0 | 64 |
| gold | 9 | 2 | 0 | 32 |
| redstone | 8 | 8 | 0 | 16 |
| diamond | 8 | 1 | 0 | 16 |
| lapis | 7 | 1 | center16 | spread16（均一min/maxではない） |

JSON emptystringはdefaultfactory、parsecaughtExceptionもdefaultfactory。Serializerのfield readは元順、field-by-fielddefaults、try中の途中failureでそれ以前のfactorywritesが残る経路がある。全Gson/JsonUtils grammar、特殊値、部分失敗はWS-GAP-SETTINGSに残す。

### 12.3 densityのblend

各5×5gridpointに周囲biome5×5を読む。depth=`depthOffset+biome.minHeight*depthWeight`、scale=`scaleOffset+biome.maxHeight*scaleWeight`はfloat。AMPLIFIEDかつdepth>0ならdepth=1+2*depth,scale=1+4*scale。weight=parabolic/(depth+2)、neighbor.minHeight>center.minHeightなら半分。weightedscale/depthとweightsumをfloat順次加算し除算、scale=0.9*scale+0.1、depth=(4*depth-1)/8。

depthnoise=sample/8000。negativeなら-値*0.3、その後3倍-2。negativebranchは/2→下限-1→/1.4→/2、positivebranchは上限1→/8。biomedepthへnoise*0.2を足しbaseSize/8を乗じ、densitycenterY=baseSize+4*その値。

各verticalgridY0..32のgradientは `(Y-centerY)*stretchY*128/256/biomeScale`、negativegradientは4倍。lower/upperは各noise/設定limit、selector=(mainNoise/10+1)/2、denormalizeClamp(lower,upper,selector)-gradient。Y>29ではfloat(Y-29)/3をdouble化した重みで-10へblend。配列increment順はxgrid→zgrid→ygrid。

### 12.4 Improved／Octaves／Simplex／Perlin

Improvedctorはpermint[512]→nextDouble×256をX/Y/Zへ3回→perm0..255初期化→i0..255でnextInt(256-i)+iとのswap→perm[i+256]=perm[i]。同じrandomを各octaveへ渡す。setSeedした別randomを各octaveに渡す実装は異なる。

fade(t)=t³*(t*(6*t-15)+10)、lerp(t,a,b)=a+t*(b-a)。gradはhash&15の三成分係数とoffsetのdot。係数の16entryは次の三行であり、典型的なPerlinのbranch版へ無検証置換しない。

| component | index0..15 |
|---|---|
| X | 1,-1,1,-1,1,-1,1,-1,0,0,0,0,1,0,-1,0 |
| Y | 1,1,-1,-1,0,0,0,0,1,-1,1,-1,1,-1,1,-1 |
| Z | 0,0,0,0,1,1,-1,-1,1,1,-1,-1,0,1,0,-1 |

通常arrayはX→Z→Yの順。sample coord=offset+index*scale+constructoroffset、castintして元値<castなら--、cell&255、fractionを引きfade。permをX/Y/Zの順に組合せ、四つのX interpolation→二つのY→一つZ、最後1/noiseScaleを乗じ既存arrayへ加算。Ycell cacheはjY==0またはcellY変化時だけ中間値を再計算する元bodyの条件を保持する。ySize==1は専用2Dpathで、Yoffset/constructorYを使わず、別二成分grad／四cornerの順を使う。単に通常3Dのy0sliceではない。

OctavesはoutputNULLならnewsize、既存ならarray**全length**をzero。frequency=1から各octave、X/Zoffset*frequency*scaleをfloor_longでinteger/fractionへ分離し、integerをsignedremainder16777216で折返して戻す。Yにはこのmodを行わない。populateへの各scaleは元scale*frequency、noiseScale=frequency、最後frequency/=2。2DoverloadはyOffset10,ySize1,yScale1で、渡された最後のexponent引数をこの委譲では使わない。

Simplexctorの3doubleoffset/permshuffleも同じdraw構造。2DはF=(sqrt3-1)/2、G=(3-sqrt3)/6でskew/unskew、二offset比較はx>z、equalはZ先。integerhelperは `v>0?cast(v):cast(v)-1` であり、0と負整数でも通常floorより1小さい。三cornerのpermgradientindex%12、各attenuation=`0.5-x²-z²`、negative0、ほかattenuation⁴*dot、合計×70。scalar単点methodはconstructoroffsetを加えないがarray版はoffset/scaleを加えてsampleする。array版loopはZ→X。

Simplexgradient12は(±1,±1,0)四個、(±1,0,±1)四個、(0,±1,±1)四個の原版順。2Ddotは先頭2components。Perlinと名付けられたclassはこのSimplexのoctaves。単点はfrequency1→各sample(x*f,z*f)/f加算→f/=2。array版はreuse十分長なら全lengthzero、短ければnew。二係数a=b=1からscaleX/Z*a*b、amplitude0.55/b、a*=渡されたp、b*=渡された第二倍率（通常0.5）。ImprovedOctavesと同じamplitude契約ではない。

**未充足 WS-GAP-NOISE-BITS:** 上記は読取による式と順序で、新実行witnessではない。全perm、全corner、中間float/double、floor端値、cachebranch、既存大array、negative/globalcoordに対するbit corpusをまだ仕様付属のgoldenとして持たない。libm sqrt／Java8doubleの環境差を含め、式が同じだけでbit一致を主張しない。

### 12.5 biome layer構築／seed

GenLayerはJavaRandomではないsigned64wrapの独自state。transitionを `T(s,a)=s*(s*6364136223846793005+1442695040888963407)+a` と定義する。baseseedはconstructorseedからT(_,seed)三回。worldseed初期化は**自分のworldGenSeedへ引数seedを先store**→nonnullparent.initWorldGenSeed(seed)→現在の自分のworldseedへT(_,baseSeed)三回。chunkseedはworldseedからT(_,x),T(_,z),T(_,x),T(_,z)。nextInt(bound)は `(chunkSeed>>24)%bound` をintへ、negativeなら+bound、返却前にT(_,worldSeed)でstate更新。

default layer順はIsland1→FuzzyZoom2000→AddIsland1→Zoom2001→AddIsland2/50/70→RemoveTooMuchOcean2→AddSnow2→AddIsland3→Edge COOL_WARM2/HEAT_ICE2/SPECIAL3→Zoom2002/2003→AddIsland4→AddMushroom5→DeepOcean4。このsharedparentからriverinit100とBiome200を分ける。

Biome枝はZoom1000から2回→BiomeEdge1000→Hills1000（riverinitを2zoomした別枝を引数）→RareBiome1001→biomeSize回Zoom1000+k、k0にAddIsland3、k1またはsize1にShore1000→Smooth1000。River枝はriverinitから2zoom+riverSizezoom→River1→Smooth1000。二枝をRiverMix100→VoronoiZoom10。RiverMixとVoronoiをseed初期化、戻り三elementは[RiverMix,Voronoi,RiverMix]のalias。

biomeSize/riverSize default4、LARGE_BIOMESはbiomeSize6、CUSTOMIZED非空optionsはJSON値。各layer.getIntsのneighbor/border/sample/tie、WorldChunkManager/BiomeCache/IntCacheの寿命とreuse、BiomeGenBase registry／climate／spawnlist／mutated variantsの全表は**未充足 WS-GAP-BIOME**。このpipelineだけではbiomearrayを生成できない。

### 12.6 populate と他generator

normalpopulationはBlockFalling.fallInstantly=true→origin/chunk+16 biome取得→rand.setSeed(worldSeed)→nextLong二回をそれぞれ`/2*2+1`のsigned奇数へ→rand.setSeed((x*a+z*b)^worldSeed)。structuresはmineshaft→village（flagを保持）→stronghold→temple→monument。条件を満たすwaterlake→lavalake→設定回数dungeon→biome.decorate→worldgencreatures→16×16ice/snow。最後fallInstantly=falseだがfinallyではない。例外でtrueを残し得る。

waterlakeは非desert/desertHills、enabled、非village、nextInt(waterChance)==0の順。lavalake条件は非village→nextInt(lavaChance/10)==0→enabledであり、disabledでも前のrandomを消費し得る。位置のX/ZはnextInt16+8、waterYnextInt256、lavaYnextInt(nextInt248+8)。seaLevel以上では追加nextInt(lavaChance/8)条件。各generator.generateの成否とconstructordrawsは別仕様が必要。

flatはFlatGeneratorInfo文字列parse→構造物factory/featuremap→layerから256高のcachedstate、airはcacheNULL→非air最後高度をseaLevelへ。provideはY→X→Zにcacheをprimerへ→構造物generate→Chunk→biomes→sky。flatgrammarversion、blocks/name/meta、省略default、layerheightoverflow、featureoptions／generate/populate全bodyは**WS-GAP-FLAT**。

Hell/End/Debugは通常generatorのparameter変更ではなく別class。各constructor乱数／noise配列、density／surface／cave／population、End island distance/noise、debug全validstates展示座標は**WS-GAP-DIM-GEN**として残る。baseWorld seedから一律通常生成してdimensionだけ色変更する代用は不適合。

## WS-13 現在のCとの区別

本節は原版規範ではなく移行時の注意。現在地は [porting.md](../porting.md)／[compatibility.md](../compatibility.md) と該当Cbodyを併読する。

| 現在のnative依存 | 原版との区別 |
|---|---|
| `world/NativeWorld.c,h` のmc_world/mc_chunk、C919WRL1 | 独自dense地形と保存。Anvil/RegionFileではない。air-only sectionのallocation履歴は保存から再構成できない |
| `MCGameplayStorage` とV3wholegraphjournal | 全player/item/mapのatomicnativecommit/recovery。原版session.lock/levelrename/sectorwriteにはない |
| C919Crafting/C919Cursor/C919Workbench | 独自復旧tag。原版Player NBTがcraftinggrid/cursor/openwindowをこの名称で保存する規則ではない |
| C919EntityId | native snapshot補助。原版EntityのdiskidentityはUUIDであり、processEntityID再利用規範ではない |
| C919MapIdCounts／NextId | nativejournalのprojection/import。原版counterauthorityはMapStorageのper-key LAST Short、memoryoverride0 |
| NativeMapData store／nativeS34受信・測量 | 実SourceMapData cache/bytes/viewersと同一authorityへ全経路を移すまで別adapter。二つのmap ownerを完成仕様にしない |
| NativeBlockStateRuntime／block facts | namedblock/state/materialのexplicitimmutable境界。全Block virtualproperty/behaviorの翻訳ではない |
| NativeDenseChunkView/height/lightopacityfacts | requirednativeleaf。SourceChunkのentity/tile/lighting/terrain生命周期と同一classではない |
| heapbudget／file2MiB envelope／coordinatefinite limits | native安全policy。原版NBT INFINITE saved readやWorldvalidityへ混ぜない |
| snapshot/adopt/GC | C lifetime/atomicity。Sourcecopyの辺ごと複製／Java exception prefixを置換しない |

## WS-14 適合試験と未充足一覧

以下はこれから実装・仕様を判定する観測契約であり、今回の新実行結果ではない。既存Cの大量unit数をこの章の全充足に換算しない。

### 14.1 最低限の具体ケース

| 範囲 | 必要入力 | 観測 |
|---|---|---|
| NBT各型 | min/max整数、±0、NaN/Infinity、empty/1/many配列 | exactbytes、read値、NaNcanonicalization、remainingcursor |
| MUTF-8 | NUL、各境界unit、pair/lone、65535/65536bytes、malformed全分類 | exactbytes／例外位置／consumedbytes、decodedUTF16 |
| List/Compound | emptytype、type0positive、unknowntype、duplicatekey、512/513depth | fieldsのpartialclear/add、tracker量、exceptionとprefix |
| region | compressedN=4090/4091/4096付近、samecount/reuse/append、255/256sectors | headeroffset、bitmap、timestamp、payload順、IOfailure各step |
| level/player | new/oldfallback、rename/deletefalse、GZIPfail、NBTentityreadfail | fileの残存、catch境界、profileUUID、部分field |
| chunkNBT | allairnonnullsection、noSky、Add初発位置、signedY、shortarrays | exactpackedbytes、constructororder、section/tile/entityalias |
| fill | full/partial/mask0、skylight有無、short/extra payload | 前段mutation、recount/flags/height、mask0unload、render/lightreset |
| height/light | allair、opacity0solid、water/emitter、hole、allocatedemptytop | height/lowest/precipitation、notify順、queueappend/drop、old-light消去 |
| counter/cache | missing0→1、32767wrap、NULLkey、IOwritefail、cachewrongclass | LASTexactShort、localreturn、dirtyclear、partialobjectcache、alias |
| tick | future/equal/priority/intwrap、1000/1001、unloaded、throwingblock | queue/hash/list状態、rngdraw、time順、failureprefix |
| generation | seed0/1/-1/MIN/MAX、chunk±0/1/31/32、loadorder順逆 | perm/noisebits、biomes/statearray、rng終状態、feature/worldwrites |

### 14.2 完全仕様化までの残部

| Gap ID | 名前・必要な閉包 | 足りない観測／記載 |
|---|---|---|
| WS-GAP-UTF | Java8 DataInputStream/DataOutputStream UTF/float | malformed全grammar、consume/errorとbyte完全一致 |
| WS-GAP-NBT-TEXT | NBT toString、escapedString、primitive文字列化 | 全float/Double表記、escape、NULL、collection iteration |
| WS-GAP-REGION-PADDING | RegionFile ctor非整列分岐 | bytecodeのwrite位置/boundと破損fileprefix |
| WS-GAP-ANVIL-FLUSH | AnvilChunkLoader.saveExtraData | loop終了のretainedbytecode確定、IOqueue競合 |
| WS-GAP-DERIVED | DerivedWorldInfo/WorldServerMultiの完全継承閉包 | 親のcrash-report/未override状態、border listener再入、保存・tick外側呼出を含む全観測 |
| WS-GAP-ENTITY-NBT | 全Entity/TileEntity保存class、CommandResultStats、mount/leash、attrs/potions | classregistry/tag全表、virtualread/write副作用とerrorprefix |
| WS-GAP-BLOCK-STATE | Block registry198、各property/metadata、BlockState/StateImplementation、Guava到達 | state列挙全identity/alias、同番号collisionとhole、全blockgetter/tick |
| WS-GAP-MAP | ItemMap.updateMapData、MapData.updateVisiblePlayers/updateDecorations、Guava HashMultiset/tie、MapRenderer | survey全body、freshallocation/iteration、viewer/frame/decoration全契約、cadenceから実送信までのlive同owner |
| WS-GAP-LIGHT | Chunk.func_150809_p/func_150811_f/recheck系残部、EmptyChunk、Blockopacity/emission | neighborpopulation/loadedbounds/noSky全flow、全blockfactsとpartialexceptions |
| WS-GAP-WORLD-TICK | World.updateEntities/blockevents/neighbors、WorldServer全tick依存 | list/reentrantiteration、tile生命周期、mob/portal/village/siege/redstone全契約 |
| WS-GAP-SETTINGS | ChunkProviderSettings.Factory/Serializer、Gson/JsonUtils | 全JSONfield/grammar/default、partialparse、outofrange/NaN |
| WS-GAP-NOISE-BITS | Improved/Octaves/Simplex/Perlin、MathHelperMath | fullpermutation/arraybits/cachebranch/exception、platformmath |
| WS-GAP-BIOME | GenLayer全subclass＋IntCache、WorldChunkManager/BiomeCache、BiomeGenBase全subclass | 全getIntsneighbor/tie/seed、registry/climate/colors/spawn/terrain/decorator |
| WS-GAP-FLAT | FlatGeneratorInfo/FlatLayerInfo/ChunkProviderFlat | 全文字列grammar、layer/featurealiasとpopulation |
| WS-GAP-DIM-GEN | ChunkProviderHell/End/Debug、MapGenCavesHell | noise/density、bedrock、islands、populate、展示stateindex |
| WS-GAP-GEN | MapGenBase/Caves/Ravine、gen/structure全class、BiomeDecorator、gen/feature全class、SpawnerAnimals | 全範囲seed・shape・structure保存schema・randomdraw・blockwrites・load順 |
| WS-GAP-SAVE-CONVERSION | AnvilSaveConverter、旧McRegion loader/format、ISaveFormat/SaveFormatOld | versionconversion、旧chunklayout、backup/delete/progress/error |
| WS-GAP-BOUNDARY | JDK collections/IO/GZIP/zlib/reflection/threads/calendar、fullThrowable | completehash/treebins/identityiteration、allocation/close/catch/OSエラー |

`WS-GAP-GEN`を具体化する実装順は、registry/state closure→GenLayer各getInts→Biome/manager/cache→Noise/Settings→通常bareterrain/surface→caves/ravines→structures→biomefeatures/decorator→population/entities→Hell/End/flat/debug→Anvil保存/reload相互試験。途中で「常にairを返すChunk」「成功する空generator」「paletteから推測するMaterial」を置かない。

本章の確定部分も、実装の検証ではimmutable入力manifest、原版とnativeの区別、実行した／していない証拠、元field/ref/rng/IOprefixの観測を[適合性](06-conformance.md)へ結びつける。未充足を除外してcomplete=trueを作るのではなく、文書全体のcomplete=falseに残す。
