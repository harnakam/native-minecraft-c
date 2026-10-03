# 03 ネットワーク仕様 — Minecraft Java Edition 1.8.9 / protocol 47

## 1. 対象、根拠、充足範囲

この章の対象は、提供された MCP-919 の Minecraft 1.8.9 原版にある TCP 通信である。現行 C 実装の制限、独自保存形式、transaction rollback、独自 heap budget を protocol の規則にはしない。本文は原版の登録と serialization を自然言語で記述した仕様であり、Java 本体、mapping、JAR、resources を配布するものではない。

静的に照合した原版の中心は `net/minecraft/network/EnumConnectionState.java`、`PacketBuffer.java`、`NetworkManager.java`、`PacketThreadUtil.java`、`NettyCompressionDecoder.java` / `NettyCompressionEncoder.java`、`util/MessageDeserializer.java` / `MessageDeserializer2.java` / `MessageSerializer.java` / `MessageSerializer2.java`、`util/CryptManager.java` である。packet の根拠は各表に記載したクラスの `readPacketData` / `writePacketData`。handshake、login、status は `server/network/NetHandlerHandshakeTCP`、`NetHandlerLoginServer`、`NetHandlerStatusServer`、`client/network/NetHandlerLoginClient` を併読した。Play の補足の根拠は `network/NetHandlerPlayServer`、`client/network/NetHandlerPlayClient`、`entity/DataWatcher`、`world/chunk/Chunk`、`village/MerchantRecipeList` などである。

登録の総数は **111**。HANDSHAKING 1、STATUS 4、LOGIN 6、PLAY 100（clientbound 74、serverbound 26）であり、下表は全登録を含む。nested class の C04/C05/C06、S15/S16/S17 を独立登録として数える。ファイル名の接頭辞だけで ID を決めてはいけない。例えば `S19PacketEntityStatus` の登録 ID は **0x1A** である。

**充足するのは全111登録の wire schema と、この章で個別に述べた状態遷移・handler 規則である。全 Play handler と、その先の world/entity/item/container/GUI の全状態機械を閉じた完全仕様ではない。** 未記述部分は末尾で列挙する。ここで「確認」は原版の静的読解を意味する。この仕様作成では原版 JVM、ゲーム、observer、network を実行していない。

関連する仕様は [実行基盤](01-runtime.md)、[world・保存](02-world-storage.md)、[gameplay](04-gameplay.md)、[client・resources](05-client-resources.md)、[適合性](06-conformance.md)。同じバイト列でも、wire 上の値、packet object の参照、handler が採用するゲーム状態を分けて扱う。

## 2. 共通表記と値の読み書き

表中の列挙順は、そのまま wire 上の順である。`[ ... ]×n` は n 回の繰返し、`条件 → ...` は条件が真のときだけ存在する。表は外側 frame と packet ID を省いた **payload** を表す。

| 表記 | wire と原版 reader の意味 |
| --- | --- |
| `I8` / `U8` | 1 byte、signed / unsigned として取得。writer は整数の下位8 bitを出す。 |
| `I16` / `U16` | 2 byte big endian、signed / unsigned として取得。writer は下位16 bitを出す。 |
| `I32` / `I64` | 4 / 8 byte big endian、Java signed int / long。 |
| `F32` / `F64` | 4 / 8 byte big endian の IEEE 754 float / double。finite のみとする共通 codec 制限はない。 |
| `B` | 1 byte。writer は false=0、true=1。reader は byte が0以外なら true。 |
| `VI` | signed32 bitの VarInt。ZigZag ではない。 |
| `VL` | signed64 bitの VarLong。ZigZag ではない。 |
| `E(T)` | `VI` の enum ordinal を `T` の定数配列の index として取得。一般に未知値の fallback はない。 |
| `Str(n)` | `VI` UTF-8 byte length と、その長さの bytes。reader の decoded Java String 上限は n UTF-16 code unit。 |
| `Bytes` | `VI` byte length と、その長さの bytes。汎用 reader 自体には固定の最大長がない。 |
| `UUID` | most-significant bits の `I64`、least-significant bits の `I64`。16 bytes。 |
| `Pos` | BlockPos の packed `I64`。下記参照。 |
| `Chat` | `Str(32767)` による IChatComponent JSON。 |
| `NBT` | nullable の非圧縮 named-root NBT compound。下記参照。 |
| `Slot` | nullable ItemStack。下記参照。 |
| `Meta` | terminator で終わる DataWatcher stream。下記参照。 |
| `Rest(n)` | packet payload の残り全 bytes。reader は残り長が n 以下であることを確認。長さ prefix は追加しない。 |

### 2.1 VarInt、文字列、enum、失敗 prefix

VI/VL は7 bitずつ下位 group から読み、各 byte の bit7 が continuation である。VI の通常 writer は1〜5 bytes、VL は1〜10 bytesで、負値はそれぞれ5/10 bytesになる。reader は VI の6 byte目、VL の11 byte目を読んだ後に too-big を失敗とする。最短表現や最後の byte の余剰 high bits を検証しない。Java の32/64 bit演算・shift masking を維持する。例えば VI の -1 は `ff ff ff ff 0f`。

`Str(n)` reader は encoded length <0 または >4n を先に拒否し、bytes を読み UTF-8 decode 後、Java String.length()>n を拒否する。Java UTF-8 decoder の malformed input replacement と、UTF-16 code-unit 計数を単なる Unicode scalar 数に置き換えない。**汎用 writer の上限は encoded UTF-8 32767 bytes**である。reader の4n制限と対称ではなく、個別 packet の n も writer に直接渡さない。C01 の通常 constructor はメッセージを100 code unitsに切り詰めるが、その効果を汎用 writer の制限と混同しない。

`E(T)` の ordinal が配列外なら失敗する。これは U8 の専用 lookup と異なる。例外型、Netty/JDK allocator、Gson coercion、OOM の完全再現は、この wire 一覧だけでは閉じない。read は一般に field を順に代入し、失敗時に前の代入を rollback しない。write も途中まで出力した後に失敗し得る。atomic parser は実装側の設計であり、原版全 packet にある共通規則ではない。

### 2.2 Length、loop、reader/writer 非対称の監査

全111登録のread/writeを同じschema行へ照合する際、単にbytes幅を合わせるだけでは次の差を失う。ここに挙げない固定scalar部分は上表の読み順と書き順が対応する。汎用String/NBT/Slot/enumに内包される非対称は全利用packetに適用する。

| 対象 | reader / writer 条件とacceptanceの差 |
| --- | --- |
| 全String利用packet | readerの個別nと4n-byte検査に対してwriterは汎用32767-byte上限。writerが出せる長文でも受信側のnで拒否され得る。 |
| 全B利用packet、C03系、C0F | readerはnonzero true、writerは0/1。noncanonical true byteを保持しない。 |
| C00Handshake | requestedStateのVIは `getById`。範囲外はNULL、既知でもPLAY/HANDSHAKING intentをserver handlerは拒否する。generic E(T) readerとは異なる。hostはreader255だがwriterに255の個別チェックはない。 |
| LOGIN S01 | DER decodeがinvalid keyならNULLを返すdependency branch。writerは実PublicKey.getEncodedを必要とする。Bytesが読めたことはkeyの有効性を保証しない。 |
| LOGIN S02 | readerは両Stringを読んでからJava UUID.fromString。writerはNULL UUIDを空文字化できる。UUID parseのJDK8許容形式はcanonical36字のみに縮小しない。 |
| C02 | INTERACT_ATだけhit vector。writerはそのbranchでnonnull hitVecを必要とし、F64のVec3 fieldsをF32へcastする。 |
| C07 / S10 | readerはfacingのmodulo lookup、writerはlookup後のenum index。未知のbyteが再serializationで元byteへ戻る保証はない。 |
| C08 | offsetsはreader U8/16F、writer float×16→int→low8。faceはrawU8で、facing enum lookupをしない。Slot条件は§7。 |
| C0C、C13、S39、S08 | readerは既知flag bitsだけobject化する。writerはそのflagsから再構成し、unknown bitsを送らない。 |
| C14 | writerはtextをsubstring0..32767にして汎用Stringを書く。positionのBはtargetBlock!=NULLで生成。reader false branchはpositionを読まない。 |
| C15、S01、S07、S41、S38のgameMode | 専用lookup。difficultyはU8%4、chatVisibilityはsignedI8%3、gameModeは未知IDでSURVIVALへfallback。VI enum配列のfail-on-unknownとは異なる。 |
| C17 / S3F | readerは残り長の32767 /1048576上限を確認。writerは内側ByteBufのreadable dataを書き、そのsource reader indexも進める。汎用write自体に同じpayload上限チェックはない。通常constructorはreadableBytesでなくdata.writerIndexに上限検査する。 |
| S0C / S0F | readerはmetadata listへdecode、writerはconstructorで保持したDataWatcher ownerからwriteする。decoded empty-constructor packetをそのままrewriterできるという共通契約ではない。 |
| S0E | data>0の同じ条件でvelocity欄がある。data<=0 branchは読み書きしない。条件はI32 dataそのもので、typeやnonnull velocityで決めない。 |
| S13 / S22 / S26 / S30 / S34 / S3A | countを読んでarrayをallocateする箇所ではnegative lengthが失敗。S30だけcountはsignedI16。他はVI。writerはarray lengthを出す。 |
| S20 | propertyCount I32 / modifierCount VIのloopはnegativeなら0回。各modifierのoperation I8は読み終えてからAttributeModifier constructorが0..2を検証する。未知operationをgeneric scalarとして採用し続けない。 |
| S21 / S26 | S21はsectionMaskをsignedshortのint値で保持、S26は `readShort &65535`。両wire bitsは16。S26 raw長はmask+skyからallocate、S21はBytes lengthを独立に採用する。 |
| S24 | readerはVI blockIdに4095 maskしてlookup。writerはblock registry IDを出す。未知元VIのbitpatternを保持するわけではない。 |
| S27 | negative countはcapacity付きList生成で失敗。writerのoffset baseは元doubleからint、readerのbaseはwire F32をdoubleにした後int。 |
| S2A | reader未知particle→BARRIER、args0。writerはselected typeのargCountだけargs配列を読む。独立のargs lengthやarray余りは送らない。 |
| S2D | inventoryTypeのcase-sensitive `EntityHorse` 比較により両IOでtailが決まる。slotCountによるtailではない。 |
| S30 | windowId代入→signed count読取→array allocate→各Slotの順。default instanceのNULL arrayでwriteするとwindow byteの後、array.lengthアクセスで失敗する。 |
| S34 | icon countでarrayallocate、widthだけ先に読む。width>0でrectangle fields/Bytes。writerのbranchは内部int width>0であり、byte narrowing後のwire幅を再判定しない。 |
| S37 | negative VI countなら新mapは空。unknownStatはvalueまで消費後捨てる。duplicate recognized statはmapのlast valueとなり、writerのcountはそのmap.size。 |
| S38 | record count/property countはloopでnegativeなら0回。actionでrecord fieldsを選ぶ。writerはstored list/propertiesのsizeと現在values順を使う。 |
| S3B | mode0/2だけdisplay/renderType。readerの未知renderType名はINTEGERへfallback。modeはI8にreadするがwriterのbranchは内部値に対する0/2比較。 |
| S3C | E(Action)後、REMOVEだけscoreなし。name/objectiveはREMOVEでも読む/書く。 |
| S3E | action0/2のinfo、0/3/4のplayersは独立条件。negative playerCountはloop0回。actionをbyteにnarrowする前のwriter内部値で条件を判定する。 |
| S42 / S44 / S45 | 各enumのbranchは§6と同じ。enumがinvalidならbranchを空として読み終えるのではなくE(T)で失敗。S44のtimeはVL、S42 deathMessageはString。 |

これらcountには一律の「最大1024件」等のoriginal制限はない。list loopでnegativeを0回とする箇所と、array/capacity allocationで例外になる箇所を統一しない。S20 snapshots、S38 records、S3E playersのreaderはstored collectionへappendする。通常decoderは新packet instanceを使うが、同じinstanceへreadを繰り返した場合のclear規則を勝手に加えない。optional branchで省略されたfieldを毎回NULL/0へresetする共通規則もない。

共通decoderの「payload全消費」は個別readerが値を厳密検証することと異なる。例えばnegative loop countの後に余分なentry bytesがあればdecoderのtrailing-bytes検査で失敗するが、count直後で終わるframeならloop0回でdecodeできる場合がある。任意のnative実装が未知flag、unknownregistry、全negative count、非最短VI、全payload shapeをより早くrejectするときは、その差を明示する。

### 2.3 座標、角度、速度

`Pos` の bits は X=上位26、Y=中間12、Z=下位26。小さい自前の定義式として、packed = `((X & 0x3ffffff) << 38) | ((Y & 0xfff) << 26) | (Z & 0x3ffffff)`。unpack は各幅の符号拡張であり、Yも signed12 bitである。writer は BlockPos の実 getter を通す。mutable position の superclass raw fields を直接読む仕様にはしない。

entity の固定座標 I32 は1/32 block、relative I8 displacement も1/32 block。S0E などの通常送信用 constructor は座標を `floor(position * 32)` で整数化する。angle I8 は signed として取得し、通常 entity angle は `value * 360 / 256` 度で使う。S0E の wire 順は **pitch→yaw**、他の多数は yaw→pitchである。velocity I16 は1/8000 block/tick。S12 と S0E の通常 constructor は各 double motion を [-3.9,+3.9] に比較で制限し、8000を掛け Java int castした後 short出力する。wire reader が全 I16 値を同じ clampで制限するわけではない。

Java float subtraction、int/long wrap、double→int cast、NaN の比較、符号付き byte の取り出しは [実行基盤](01-runtime.md) の契約に従う。entity ID に共通の「正数のみ」制限はない。VI は0・負値も表現し、lookup の成否は handler / entity owner が決める。

## 3. Frame、接続 state、圧縮、暗号

### 3.1 TCP の境界と dispatch

圧縮なしは `VI(frameByteLength), VI(packetId), payload`。frameByteLength は packetId bytes と payload bytes の合計であり、length 自身を含めない。TCP segment/read ごとを packet として扱わない。splitter は prefix または body が不足したら reader index を元に戻して次の bytes を待つ。frame length の prefix は **最大3 bytes / 21 bit**で、3 byteすべて continuation なら失敗。最大表現値は2097151。ゼロ長 frame は packet decoder が dispatchしない。

packet ID は current state と受信方向で解決する。同じ ID を state 間で共有してよい。未知 ID は decode失敗。packet の reader 終了後に frame内 bytes が残る場合も失敗となる。packet が `Rest` を読む場合は、その部分を含めて消費済みとなる。新 packet object は登録クラスの empty constructorから作り、その後 reader を呼ぶ。

接続 state の内部数値は HANDSHAKING=-1、PLAY=0、STATUS=1、LOGIN=2。新接続は HANDSHAKING。C00Handshake の requestedState で許される通常 intent は STATUS=1 または LOGIN=2。これは protocolVersion=47 と別の値である。

### 3.2 圧縮

圧縮有効時の frame body は `VI(uncompressedDataLength), body` となる。bodyに packet ID が含まれる。

| 条件 | body |
| --- | --- |
| 未圧縮 packet 長 < threshold | dataLength=0、その後に raw `VI(packetId),payload` |
| 未圧縮 packet 長 ≥ threshold | dataLength=未圧縮長、その後に zlib stream |

zlib は通常の Deflater/Inflater wrapperを持つもので、gzip でも raw DEFLATE でもない。threshold は byte数で、packet IDを含む。threshold<0で pipelineから圧縮/decompressionを外す。S03 LOGIN と S46 PLAY が thresholdを通知する。local channelでは wire 圧縮を切り替えない。

原版 compressed decoder は dataLength!=0 の場合、dataLength<threshold、または >2097152 を拒否する。dataLength=0 branchに「raw長がthreshold未満か」の検査はない。原版は expected lengthの zero-initialized byte arrayに一度 `inflate` し、戻り長、finished、残り compressed bytes を別途検査していない。したがって「厳密な zlib消費と展開長一致」を原版 reader の既存保証と書いてはいけない。valid writer streamを正しく解くことと、malformed inputへの target behavior を分けて検証する。

frameの21 bit上限、compressed branchの2MiB上限、NBT tracker の上限は別々である。一般 Bytes/listの packet内 lengthが自動的に2MiB以内になるという共通規則ではない。提供 Source の `NetworkManager.setCompressionTreshold` には既存 compressor更新 branchで pipeline名 `decompress` を取得する箇所がある。この再設定 branchの実 classfile / provider結果は本章では未検証であり、通常の一回の有効化と区別する。

### 3.3 暗号と session authentication

online-mode TCP loginでは server が1024 bit RSA key pairを持つ。S01で serverId、X.509 DER形式 RSA public key、4 byte verify tokenを送る。verify token生成は原版 login handlerの共有 `java.util.Random.nextBytes`。client の AES shared secretは128 bit KeyGeneratorで生成する。

authentication hash入力は、serverId の ISO-8859-1 bytes、AES key bytes、RSA public DER bytesをこの順に連結した SHA-1。digestを signed BigIntegerとして base16文字列化する。負なら `-` が付き、固定40桁の zero paddingをしない。client は session serviceの `joinServer(profile,accessToken,hash)`、server は `hasJoinedServer(GameProfile(null,name),hash)` を用いる。access tokenは Minecraft TCP packetに載せない。

C01EncryptionResponseの二つの Bytesは、AES keyと verify tokenをそれぞれ RSA暗号化した ciphertext。提供 Sourceは key algorithm `RSA` を Cipher transformationに渡す。通常 JCEの `RSA/ECB/PKCS1Padding` 解釈に依存する部分であり、任意の crypto providerが同じという保証ではない。server は tokenを先に復号・比較し、その後 AES secretを復号する。nonce不一致は login失敗。

response自体は暗号化前に送信し、client はその送信完了 callbackで AESを有効化する。server は response処理で shared key採用後 AESを有効化する。以後は双方向 **AES/CFB8/NoPadding** の連続 stream。key bytesを IVとして使い、各方向に別 Cipher stateを持つ。packet境界で Cipherをresetしない。frame length prefixを含めた TCP stream全体が暗号化される。処理順は送信側で packet serialization→compression→frame length→encryption、受信側でその逆。

authlib の HTTP endpoint、TLS、HTTP response JSON、profile property signatureの検証、token更新は原版 Minecraft handlerから外部 session serviceに委譲される。これらの完全仕様は本章で未調査である。online authentication全体を再実装できると判断する際の明示 gapとする。

## 4. HANDSHAKING / STATUS / LOGIN 全登録

方向 `SB` は client→server、`CB` は server→client。class名は登録先を一意に識別するためのもので、元 mappingの配布を意味しない。

| State | 方向 | ID | 登録 class | payload（順序固定） |
| --- | --- | --- | --- | --- |
| HANDSHAKING | SB | 0x00 | C00Handshake | `VI protocolVersion, Str(255) host, U16 port, VI requestedState` |
| STATUS | SB | 0x00 | C00PacketServerQuery | 空 |
| STATUS | SB | 0x01 | C01PacketPing | `I64 clientTime` |
| STATUS | CB | 0x00 | S00PacketServerInfo | `Str(32767) statusJson` |
| STATUS | CB | 0x01 | S01PacketPong | `I64 clientTime` |
| LOGIN | SB | 0x00 | C00PacketLoginStart | `Str(16) playerName` |
| LOGIN | SB | 0x01 | C01PacketEncryptionResponse | `Bytes encryptedSecret, Bytes encryptedVerifyToken` |
| LOGIN | CB | 0x00 | S00PacketDisconnect | `Chat reason` |
| LOGIN | CB | 0x01 | S01PacketEncryptionRequest | `Str(20) serverId, Bytes publicKeyDER, Bytes verifyToken` |
| LOGIN | CB | 0x02 | S02PacketLoginSuccess | `Str(36) uuidText, Str(16) playerName` |
| LOGIN | CB | 0x03 | S03PacketEnableCompression | `VI threshold` |

### 4.1 Status JSON と応答

statusJsonは JSON object。非NULLの source fieldだけ出力する。`description` は Chat componentのJSON値、`version` は `{name: string, protocol: int}`、`players` は `{max: int, online: int}` と optional `sample`、`favicon` は string。sampleは `{id: UUID text, name: string}` の配列。source writerは sampleがNULLまたは空なら省略する。favicon画像を packetの別binary fieldとして加えない。

status serverは最初の queryで handled=trueをstoreして現在のstatusを送る。query繰返しでは接続をcloseする。Pingには同じ I64をPongにして送信し、その後接続closeを要求する。LOGINでないSTATUS intentには protocolVersion47一致検査を適用しない。

### 4.2 Login 状態と採用順

serverの内部状態は HELLO→KEY→AUTHENTICATING→READY_TO_ACCEPT→ACCEPTED、または重複UUID時の DELAY_ACCEPT。これは wire `EnumConnectionState` とは別の状態機械である。

1. LOGIN intentを受けるとまず接続stateをLOGINにする。version>47なら outdated server、<47なら outdated clientとして S00とclose。同じ47なら login handlerを設置。
2. C00は HELLOで処理する。nameから UUID未設定profileを作る。online-modeかつ非localなら KEYへ移しS01、offlineまたはlocalなら READY_TO_ACCEPTへ進む。
3. KEYで C01を処理し、nonce比較→shared secret field代入→AUTHENTICATING state→AES有効化→authentication thread開始の順。threadでhashと session serviceを扱う。成功した authenticated profileを採用して READY_TO_ACCEPT。
4. online認証の不成功・service unavailableは dedicated serverでは拒否。single-player serverでは offline profileでのfallback branchがある。client側はLAN接続設定なら authentication例外を警告して継続する branchがある。LAN例外を全remote接続の免除にはしない。
5. 不完全profileの offline UUIDは `UUID.nameUUIDFromBytes(UTF8("OfflinePlayer:" + name))`。名前の大文字小文字を変えない。MD5にUUID version3/variant bitsを設定する。
6. admission policyの許可後 ACCEPTED。compression threshold≥0かつ非localなら S03を送り、そのsend完了callbackで server pipelineを変更する。続いて S02。clientは S03受信時にcompressionを変更し、S02受信時に profile採用→PLAY state→NetHandlerPlayClient設置。
7. 同UUIDの既存playerが残る場合は DELAY_ACCEPTで旧playerの消失を待つ。server tick updateで受付を進める。login timeoutはカウンタのpostincrementと600比較で判定される。これは通常20Hz時の約30秒で、全packetにwall-clock600msという制限ではない。

S02のUUID fieldは binary UUIDではなく Stringである。readerはUUID文字列parseを呼ぶ。原版 writerのNULL UUID時の空文字と readerの成功条件は対称ではない。handlerで通常は完成したprofileを送る。

## 5. PLAY serverbound 全26登録

classの共通 packageは `net.minecraft.network.play.client`。C03の三種類だけnested class名を示す。

| ID | 登録 class | payload（順序固定） |
| --- | --- | --- |
| 0x00 | C00PacketKeepAlive | `VI key` |
| 0x01 | C01PacketChatMessage | `Str(100) message` |
| 0x02 | C02PacketUseEntity | `VI entityId, E(Action) action`; INTERACT_ATだけ `F32 hitX, F32 hitY, F32 hitZ` |
| 0x03 | C03PacketPlayer | `B onGround` |
| 0x04 | C03PacketPlayer.C04PacketPlayerPosition | `F64 x, F64 y, F64 z, B onGround` |
| 0x05 | C03PacketPlayer.C05PacketPlayerLook | `F32 yaw, F32 pitch, B onGround` |
| 0x06 | C03PacketPlayer.C06PacketPlayerPosLook | `F64 x, F64 y, F64 z, F32 yaw, F32 pitch, B onGround` |
| 0x07 | C07PacketPlayerDigging | `E(Action) action, Pos position, U8 facingIndex` |
| 0x08 | C08PacketPlayerBlockPlacement | `Pos position, U8 face, Slot heldStack, U8 hitX, U8 hitY, U8 hitZ` |
| 0x09 | C09PacketHeldItemChange | `I16 hotbarIndex` |
| 0x0A | C0APacketAnimation | 空 |
| 0x0B | C0BPacketEntityAction | `VI entityId, E(Action) action, VI auxiliaryData` |
| 0x0C | C0CPacketInput | `F32 strafe, F32 forward, I8 flags` |
| 0x0D | C0DPacketCloseWindow | `I8 windowId` |
| 0x0E | C0EPacketClickWindow | `I8 windowId, I16 slotId, I8 button, I16 actionNumber, I8 mode, Slot clickedReturn` |
| 0x0F | C0FPacketConfirmTransaction | `I8 windowId, I16 actionNumber, B accepted` |
| 0x10 | C10PacketCreativeInventoryAction | `I16 slotId, Slot stack` |
| 0x11 | C11PacketEnchantItem | `I8 windowId, I8 button` |
| 0x12 | C12PacketUpdateSign | `Pos position, [Str(384) componentJson]×4` |
| 0x13 | C13PacketPlayerAbilities | `I8 flags, F32 flySpeed, F32 walkSpeed` |
| 0x14 | C14PacketTabComplete | `Str(32767) text, B hasPosition`; trueだけ `Pos position` |
| 0x15 | C15PacketClientSettings | `Str(7) language, I8 viewDistance, I8 chatVisibility, B chatColors, U8 modelParts` |
| 0x16 | C16PacketClientStatus | `E(EnumState) state` |
| 0x17 | C17PacketCustomPayload | `Str(20) channel, Rest(32767) data` |
| 0x18 | C18PacketSpectate | `UUID target` |
| 0x19 | C19PacketResourcePackStatus | `Str(40) hash, E(Action) action` |

### 5.1 Serverbound enum、flags、特殊値

| Packet / field | 値 |
| --- | --- |
| C02 Action | 0 INTERACT、1 ATTACK、2 INTERACT_AT |
| C07 Action | 0 START_DESTROY_BLOCK、1 ABORT_DESTROY_BLOCK、2 STOP_DESTROY_BLOCK、3 DROP_ALL_ITEMS、4 DROP_ITEM、5 RELEASE_USE_ITEM |
| C07 facing | 通常0 DOWN、1 UP、2 NORTH、3 SOUTH、4 WEST、5 EAST。readerは `EnumFacing.getFront` を使い、unsigned値の `abs(index % 6)` でlookupする。単純な0..5拒否ではない。 |
| C08 face | 通常0..5は同じ面順。255はitem air-use constructorの値。C07のenum lookupをC08に適用しない。 |
| C08 hit coordinates | unsigned byteを16.0Fで割る。writerはfloat×16→Java int cast→低8 bit。別の0..1 clampをcodecに加えない。 |
| C08 item-use constructor | position=(-1,-1,-1)、face=255、offsets=0。hand enumは存在しない。 |
| C0B Action | 0 START_SNEAKING、1 STOP_SNEAKING、2 STOP_SLEEPING、3 START_SPRINTING、4 STOP_SPRINTING、5 RIDING_JUMP、6 OPEN_INVENTORY |
| C0C flags | bit0 jumping、bit1 sneaking。その他bitsにfieldはない。 |
| C0E mode | 0 normal、1 shift、2 hotbar number、3 creative clone、4 drop、5 drag、6 collect。container bodyの条件とeffectsは[gameplay](04-gameplay.md)。 |
| C0E slot | -999はwindow外の操作で使う。slotId自体はsigned16で、それ以外の負値を一律NULLslotとしない。 |
| C0E mode5 button | phase=`button & 3`、drag mode=`(button >> 2) & 3`。通常の組はstart 0/4/8、add 1/5/9、end 2/6/10。creative gating等はcontainer側。 |
| C13 flags | bit0 invulnerable、bit1 flying、bit2 allowFlying、bit3 creativeMode。 |
| C15 chatVisibility | 0 FULL、1 SYSTEM、2 HIDDEN。readerの専用lookupはsigned値 `%3` によるindexで、VI enum readerではない。 |
| C15 modelParts | cape=1、jacket=2、left sleeve=4、right sleeve=8、left trouser leg=16、right trouser leg=32、hat=64。 |
| C16 EnumState | 0 PERFORM_RESPAWN、1 REQUEST_STATS、2 OPEN_INVENTORY_ACHIEVEMENT |
| C19 Action | 0 SUCCESSFULLY_LOADED、1 DECLINED、2 FAILED_DOWNLOAD、3 ACCEPTED |

C0E actionNumberは signed short。原版containerのtransaction counterはshortでwrapする。同じ接続で前のresponseが未着でも次のwindowClickが禁止されるというpacket規則はない。constructorはreturned ItemStackをcopyしてpacketに保持する。wireとsaveを通った stackにJava reference identityは残らない。

## 6. PLAY clientbound 全74登録

classの共通 packageは `net.minecraft.network.play.server`。表の補足記号は下の詳細節に展開する。

| ID | 登録 class | payload（順序固定） |
| --- | --- | --- |
| 0x00 | S00PacketKeepAlive | `VI key` |
| 0x01 | S01PacketJoinGame | `I32 entityId, U8 gameModeAndHardcore, I8 dimension, U8 difficulty, U8 maxPlayers, Str(16) worldType, B reducedDebugInfo` |
| 0x02 | S02PacketChat | `Chat component, I8 type` |
| 0x03 | S03PacketTimeUpdate | `I64 totalWorldTime, I64 worldTime` |
| 0x04 | S04PacketEntityEquipment | `VI entityId, I16 equipmentSlot, Slot stack` |
| 0x05 | S05PacketSpawnPosition | `Pos position` |
| 0x06 | S06PacketUpdateHealth | `F32 health, VI foodLevel, F32 saturation` |
| 0x07 | S07PacketRespawn | `I32 dimension, U8 difficulty, U8 gameMode, Str(16) worldType` |
| 0x08 | S08PacketPlayerPosLook | `F64 x, F64 y, F64 z, F32 yaw, F32 pitch, U8 relativeFlags` |
| 0x09 | S09PacketHeldItemChange | `I8 hotbarIndex` |
| 0x0A | S0APacketUseBed | `VI playerId, Pos bedPosition` |
| 0x0B | S0BPacketAnimation | `VI entityId, U8 animationType` |
| 0x0C | S0CPacketSpawnPlayer | `VI entityId, UUID profileId, I32 x, I32 y, I32 z, I8 yaw, I8 pitch, I16 heldItemId, Meta metadata` |
| 0x0D | S0DPacketCollectItem | `VI collectedEntityId, VI collectorEntityId`。count fieldなし。 |
| 0x0E | S0EPacketSpawnObject | `VI entityId, I8 objectType, I32 x, I32 y, I32 z, I8 pitch, I8 yaw, I32 data`; **data>0だけ** `I16 velocityX, I16 velocityY, I16 velocityZ` |
| 0x0F | S0FPacketSpawnMob | `VI entityId, U8 mobType, I32 x, I32 y, I32 z, I8 yaw, I8 pitch, I8 headPitch, I16 velocityX, I16 velocityY, I16 velocityZ, Meta metadata` |
| 0x10 | S10PacketSpawnPainting | `VI entityId, Str(13) artTitle, Pos hangingPosition, U8 horizontalFacing` |
| 0x11 | S11PacketSpawnExperienceOrb | `VI entityId, I32 x, I32 y, I32 z, I16 experience` |
| 0x12 | S12PacketEntityVelocity | `VI entityId, I16 motionX, I16 motionY, I16 motionZ` |
| 0x13 | S13PacketDestroyEntities | `VI count, [VI entityId]×count` |
| 0x14 | S14PacketEntity | `VI entityId` |
| 0x15 | S14PacketEntity.S15PacketEntityRelMove | `VI entityId, I8 deltaX, I8 deltaY, I8 deltaZ, B onGround` |
| 0x16 | S14PacketEntity.S16PacketEntityLook | `VI entityId, I8 yaw, I8 pitch, B onGround` |
| 0x17 | S14PacketEntity.S17PacketEntityLookMove | `VI entityId, I8 deltaX, I8 deltaY, I8 deltaZ, I8 yaw, I8 pitch, B onGround` |
| 0x18 | S18PacketEntityTeleport | `VI entityId, I32 x, I32 y, I32 z, I8 yaw, I8 pitch, B onGround` |
| 0x19 | S19PacketEntityHeadLook | `VI entityId, I8 headYaw` |
| 0x1A | S19PacketEntityStatus | **`I32 entityId`**, `I8 opcode` |
| 0x1B | S1BPacketEntityAttach | **`I32 entityId, I32 vehicleId`**, `U8 leash` |
| 0x1C | S1CPacketEntityMetadata | `VI entityId, Meta metadata` |
| 0x1D | S1DPacketEntityEffect | `VI entityId, I8 effectId, I8 amplifier, VI duration, I8 hideParticles` |
| 0x1E | S1EPacketRemoveEntityEffect | `VI entityId, U8 effectId` |
| 0x1F | S1FPacketSetExperience | `F32 experienceBar, VI experienceLevel, VI totalExperience` |
| 0x20 | S20PacketEntityProperties | `VI entityId, I32 propertyCount, [Str(64) name, F64 baseValue, VI modifierCount, [UUID id, F64 amount, I8 operation]×modifierCount]×propertyCount` |
| 0x21 | S21PacketChunkData | `I32 chunkX, I32 chunkZ, B groundUp, I16 sectionMask, Bytes data` |
| 0x22 | S22PacketMultiBlockChange | `I32 chunkX, I32 chunkZ, VI count, [I16 localPosition, VI stateRegistryId]×count` |
| 0x23 | S23PacketBlockChange | `Pos position, VI stateRegistryId` |
| 0x24 | S24PacketBlockAction | `Pos position, U8 eventId, U8 eventParameter, VI blockId` |
| 0x25 | S25PacketBlockBreakAnim | `VI breakerId, Pos position, U8 progress` |
| 0x26 | S26PacketMapChunkBulk | `B hasSkylight, VI count, [I32 chunkX, I32 chunkZ, U16 sectionMask]×count, [raw chunk bytes]×count` |
| 0x27 | S27PacketExplosion | `F32 x, F32 y, F32 z, F32 strength, I32 count, [I8 offsetX, I8 offsetY, I8 offsetZ]×count, F32 motionX, F32 motionY, F32 motionZ` |
| 0x28 | S28PacketEffect | `I32 effectId, Pos position, I32 data, B global` |
| 0x29 | S29PacketSoundEffect | `Str(256) name, I32 x, I32 y, I32 z, F32 volume, U8 pitch` |
| 0x2A | S2APacketParticles | `I32 particleId, B longDistance, F32 x, F32 y, F32 z, F32 offsetX, F32 offsetY, F32 offsetZ, F32 speed, I32 count, [VI argument]×particleArgumentCount` |
| 0x2B | S2BPacketChangeGameState | `U8 state, F32 value` |
| 0x2C | S2CPacketSpawnGlobalEntity | `VI entityId, I8 type, I32 x, I32 y, I32 z` |
| 0x2D | S2DPacketOpenWindow | `U8 windowId, Str(32) inventoryType, Chat title, U8 slotCount`; inventoryTypeが正確に `EntityHorse` のときだけ `I32 entityId` |
| 0x2E | S2EPacketCloseWindow | `U8 windowId` |
| 0x2F | S2FPacketSetSlot | **`I8 windowId, I16 slotId`**, `Slot stack` |
| 0x30 | S30PacketWindowItems | `U8 windowId, I16 signedCount, [Slot stack]×signedCount` |
| 0x31 | S31PacketWindowProperty | `U8 windowId, I16 propertyIndex, I16 value` |
| 0x32 | S32PacketConfirmTransaction | `U8 windowId, I16 actionNumber, B accepted` |
| 0x33 | S33PacketUpdateSign | `Pos position, [Chat line]×4` |
| 0x34 | S34PacketMaps | `VI mapId, I8 scale, VI iconCount, [U8 packedIcon, I8 x, I8 z]×iconCount, U8 width`; width>0だけ `U8 height, U8 startX, U8 startZ, Bytes colorsPatch` |
| 0x35 | S35PacketUpdateTileEntity | `Pos position, U8 tileType, NBT compound` |
| 0x36 | S36PacketSignEditorOpen | `Pos position` |
| 0x37 | S37PacketStatistics | `VI count, [Str(32767) statId, VI value]×count` |
| 0x38 | S38PacketPlayerListItem | `E(Action) action, VI count, [action別record]×count`。§6.1。 |
| 0x39 | S39PacketPlayerAbilities | `I8 flags, F32 flySpeed, F32 walkSpeed`。flagsはC13と同じ。 |
| 0x3A | S3APacketTabComplete | `VI count, [Str(32767) match]×count` |
| 0x3B | S3BPacketScoreboardObjective | `Str(16) objectiveName, I8 mode`; mode=0/2だけ `Str(32) displayName, Str(16) renderType` |
| 0x3C | S3CPacketUpdateScore | `Str(40) playerName, E(Action) action, Str(16) objectiveName`; action!=REMOVEだけ `VI score` |
| 0x3D | S3DPacketDisplayScoreboard | `I8 displayPosition, Str(16) objectiveName` |
| 0x3E | S3EPacketTeams | `Str(16) teamName, I8 action`; §6.2の条件fields。 |
| 0x3F | S3FPacketCustomPayload | `Str(20) channel, Rest(1048576) data` |
| 0x40 | S40PacketDisconnect | `Chat reason` |
| 0x41 | S41PacketServerDifficulty | `U8 difficulty`。difficultyLocked fieldなし。 |
| 0x42 | S42PacketCombatEvent | `E(Event) event`; §6.3の条件fields。 |
| 0x43 | S43PacketCamera | `VI entityId` |
| 0x44 | S44PacketWorldBorder | `E(Action) action`; §6.4の条件fields。 |
| 0x45 | S45PacketTitle | `E(Type) type`; TITLE/SUBTITLEだけ `Chat component`、TIMESだけ `I32 fadeIn, I32 stay, I32 fadeOut`、CLEAR/RESETは追加なし。 |
| 0x46 | S46PacketSetCompressionLevel | `VI threshold` |
| 0x47 | S47PacketPlayerListHeaderFooter | `Chat header, Chat footer` |
| 0x48 | S48PacketResourcePackSend | `Str(32767) url, Str(40) hash` |
| 0x49 | S49PacketUpdateEntityNBT | `VI entityId, NBT compound` |

S10の artTitle上限は原版 `"SkullAndRoses".length()` = **13**。この長さ定数を画像幅やUTF-8byte数から推測しない。

### 6.1 S38 player-list record

| Action ordinal | 各recordの順序 |
| --- | --- |
| 0 ADD_PLAYER | `UUID id, Str(16) name, VI propertyCount, [Str(32767) propertyName, Str(32767) propertyValue, B hasSignature, (trueだけ Str(32767) signature)]×propertyCount, VI gameMode, VI ping, B hasDisplayName, (trueだけ Chat displayName)` |
| 1 UPDATE_GAME_MODE | `UUID id, VI gameMode` |
| 2 UPDATE_LATENCY | `UUID id, VI ping` |
| 3 UPDATE_DISPLAY_NAME | `UUID id, B hasDisplayName, (trueだけ Chat displayName)` |
| 4 REMOVE_PLAYER | `UUID id` |

profile propertiesはsignedness booleanで条件付きsignatureを持つ。UUIDとproperty valueを、皮膚画像本体と同一視しない。ADDがSpawnPlayerより先に必要となる client-side NetworkPlayerInfo lookupと、skin/textureの取得は別契約である。

### 6.2 S3E teams と scoreboard 値

S3E action 0=create、1=remove、2=update info、3=add players、4=remove players。0/2だけ次の順で `Str(32) displayName, Str(16) prefix, Str(16) suffix, I8 friendlyFlags, Str(32) nameTagVisibility, I8 chatColor`。0/3/4だけ続けて `VI playerCount, [Str(40) playerName]×playerCount`。action1はteamName/actionのみ。

friendlyFlags bit0 friendlyFire、bit1 seeFriendlyInvisibles。nameTagVisibilityの通常名は `always`、`never`、`hideForOtherTeams`、`hideForOwnTeam`。chatColorはsigned byteで-1 resetも表現する。S3B mode 0=create、1=remove、2=update。renderTypeは `integer` / `hearts`。S3C Action 0=CHANGE、1=REMOVE。S3D positionの通常値0=list、1=sidebar、2=belowName、3..18=team-color sidebar。packet readerの値受理とScoreboard handlerの既存object条件は区別する。

### 6.3 S42 combat

| Event ordinal | 追加fields |
| --- | --- |
| 0 ENTER_COMBAT | なし |
| 1 END_COMBAT | `VI duration, I32 opponentEntityId` |
| 2 ENTITY_DIED | `VI playerEntityId, I32 killerEntityId, Str(32767) deathMessage` |

deathMessageは **Chat fieldではない**。通常constructorはcombat trackerのdeath componentをunformatted text化した文字列にする。JSON Chatとしてdecodeしない。opponent/killer不在の通常値は-1。

### 6.4 S44 world-border

| Action ordinal | 追加fields（順序固定） |
| --- | --- |
| 0 SET_SIZE | `F64 targetDiameter` |
| 1 LERP_SIZE | `F64 oldDiameter, F64 targetDiameter, VL transitionMillis` |
| 2 SET_CENTER | `F64 centerX, F64 centerZ` |
| 3 INITIALIZE | `F64 centerX, F64 centerZ, F64 oldDiameter, F64 targetDiameter, VL transitionMillis, VI worldSize, VI warningDistance, VI warningTime` |
| 4 SET_WARNING_TIME | `VI warningTime` |
| 5 SET_WARNING_BLOCKS | `VI warningDistance` |

INITIALIZE末尾は **distance→time**。transitionはmillisecondのlongで、VIではない。lerpの現在値、時計、center getterのvirtual reevaluationは[world](02-world-storage.md)のWorldBorder契約。

### 6.5 その他の enum・通常値・constructor変換

| Field | 規則 |
| --- | --- |
| S01 gameModeAndHardcore | bit3がhardcore。残りからGameTypeをlookup。通常gamemode0 survival、1 creative、2 adventure、3 spectator。 |
| S01/S07 difficulty、S41 | 0 peaceful、1 easy、2 normal、3 hard。原版lookupは `%4`。S41にlocked booleanを追加しない。 |
| S01/S07 worldType | nameをWorldType.parseWorldType。未知名でNULLならDEFAULTに置換。 |
| S02 type | 通常0 chat、1 system、2 game-info。 |
| S03 worldTime | daylight cycle停止の通常constructorはtimeをlongで負にする。結果0なら-1。totalWorldTimeはそのまま。Long.MIN_VALUEのnegationはJava wrap。 |
| S04 equipmentSlot | 0 held、1 boots、2 leggings、3 chestplate、4 helmet。 |
| S08 relativeFlags | X=1、Y=2、Z=4、Y_ROT=8、X_ROT=16。位置/角度それぞれrelative。 |
| S10 facing | horizontal lookup順は0 SOUTH、1 WEST、2 NORTH、3 EAST。U8を `abs(index %4)` でlookup。 |
| S1B leash | 0 riding、1 leash。vehicleId=-1はdetachで使う。 |
| S27 offsets | explosion座標をJava int castしたbaseとのI8差。negative座標でfloorと同じとは限らない。writerは元doubleからbaseを作り、wire座標はF32へ丸めるため極端値でreader baseとの差も残る。 |
| S29 position/pitch | constructorはcoordinate×8→Java int cast、pitch×63F→Java int cast。getterのcoordinateはint→float→/8F→double。pitch getterはU8/63F。Source constructor末尾のclampはlocal pitchへ代入し、stored integer pitchをclampし直さない。 |
| S2A particle ID | 0..41。36 ITEM_CRACKはargs2、37 BLOCK_CRACKと38 BLOCK_DUSTはargs1、他はargs0。未知IDはreaderでBARRIER=35へ置換する。 |
| S2A args | packetにargCount fieldはない。selected particle typeにより必要個数のVIを読む。 |
| S45 Type | 0 TITLE、1 SUBTITLE、2 TIMES、3 CLEAR、4 RESET |
| S37 unknown stat | stat ID lookupがNULLでも対応valueのVIは消費する。unknown entryをdecoded mapへ保存しない。 |

## 7. NBT、ItemStack、metadata の wire boundary

### 7.1 NBT

packet NBTは **gzipしない**。NULLはTAG_Endの1 byte `00`。非NULLは通常TAG_Compound=10、root name、compound payload、終端のnamed-root binary NBT。root nameは通常空文字でも、wireのname fieldを削除しない。NBT stringはPacketBufferのUTF-8/VI Stringではなく DataInput/DataOutput の modified UTF-8・U16長契約である。各tagの型、配列length、compound/list、deep copy/alias、unknown fieldsは[world・保存](02-world-storage.md)を参照する。

PacketBuffer readerは先頭0をNULLとして返す。非0ならreader indexを戻し、`NBTSizeTracker(2097152)` を使ってroot compoundを読む。trackerはtagごとの見積もりを積むもので、単なる「wire byte length≤2097152」と等価ではない。NBT深さ、named tag、class castの失敗prefixとJava modified UTF-8の詳細を通常UTF-8に置換しない。独自実装がtagごとのlimitに加えてpacket/heap/aggregate limitを持つ場合は、その差を適合性で明記する。

### 7.2 Slot

| 条件 | bytes |
| --- | --- |
| NULL ItemStack | `I16 -1` のみ |
| 非NULL ItemStack | `I16 itemId, I8 stackSize, I16 damage, NBT sharedTag` |

readerはitemIdが **任意の負数ならNULL**としてそこで終了する。非負ならsigned count、signed damageを読み、Item registry lookup結果からItemStackをconstructして、その後NBTをsetする。非NULLの count0や負countをNULLへ正規化してはいけない。ItemStackのconstructorはnegative damageを0へ補正する。未知の非負itemIdはItem lookupがNULLとなるが、それを負IDとして先にreturnしない。

writerは非NULL stackのitem registry id、stackSize低8 bit、metadata低16 bitを出力する。sharedTagはItem.isDamageableまたはItem.getShareTagがtrueのときだけ元tagを選び、それ以外はNULLを出力する。unknown/null Itemを含む非NULL stackのwriterが正常に完結するとは限らない。NBTを常に送る仕様でも、全NBTを常に落とす仕様でもない。

codecは通常のitem max stack、armor適合、slot capacity、creative権限を検査しない。正常inventory操作の制約と、wire上のsigned countを分ける。S30の同じ stack参照が二つのslotにある場合も、serializationは各出現を独立に書き、decodeでは別objectになる。NBT semantic equalityはwirebyte列一致だけでは表せない。

### 7.3 DataWatcher stream

entryは `U8 header` と type別payload。headerはtype上位3 bit、index下位5 bit。stream終端は **0x7F**。entry count fieldはない。

| type | payload |
| --- | --- |
| 0 | `I8` |
| 1 | `I16` |
| 2 | `I32` |
| 3 | `F32` |
| 4 | `Str(32767)` |
| 5 | `Slot` |
| 6 | **`I32 x, I32 y, I32 z`**。packed Posではない。 |
| 7 | `F32 rotationX, F32 rotationY, F32 rotationZ` |

readerは最初の終端まで読み、entry順をlistとして保持する。empty streamはNULL listとなる。duplicate indexをwire parserでmapへ潰す規則はない。0x7Fはtype3/index31のheaderと同じbitpatternで、wire上では常にterminatorとして解釈される。generic type別payloadを読んでから終端判定する順ではない。

原版addObjectがtype判定に用いるruntime class mapではtype6はexact BlockPos。MutableBlockPosを同じshapeだけで自動登録しない。一方、既にtype6と明示されたWatchableObjectのwriterではBlockPosへcheckcastし、subtypeのactual gettersを使う。metadata compareにはVec3i.equalsなど実値のObject equalityが関わる。entityごとのindex意味、初期登録、dirty state、client update通知順は[gameplay](04-gameplay.md)に属し、全entity catalogの充足は本章では主張しない。

## 8. Chunk、block state、map patch

### 8.1 S21/S26 raw chunk layout

maskのbit0..15はY section0..15。n=`bitCount(mask & 0xffff)`。選択sectionは昇順。各sectionは16×16×16 blocks。raw byte数は次式である。

`n * (8192 + 2048 + (hasSkylight ? 2048 : 0)) + (groundUp ? 256 : 0)`

rawの並びは、(1)選択した全sectionのblock states、(2)選択した全sectionのblock light、(3)必要なら全sectionのsky light、(4)groundUpならbiome256 bytes。sectionごとにstate/light/skyをinterleaveしない。

block stateは4096個の16 bit値を **low byte→high byte**で出す。外側のI16 big endianと異なる。local indexは `(y << 8) | (z << 4) | x`、xが最も速い。lightは各block4 bitのNibbleArrayで、even indexはlow nibble、oddはhigh nibble。biomeは16×16 columnである。

S21にはhasSkylight fieldがない。world providerのhasNoSkyから判定する。S26はhasSkylightをBで送り、各chunkはgroundUp=trueのfull form。**S26は全chunk headerを先に並べ、その後各chunkのrawbytesを連続して出す**。各rawの長さprefixはなく、maskとskylight flagから算出する。

通常S21 extractionは指定maskのnonnullsectionを採用し、groundUp=trueならemptysectionを省く。groundUp=trueかつmask=0はclient handlerでchunk unloadとなり、fillより前にreturnする。通常の「空のchunkをload」の値として扱わない。groundUp=falseは既存chunkの部分更新。packet readerの Bytes読み自体にはraw長と上式の一致検査はなく、Chunk.fillChunk側のarray/section処理で失敗prefixが生じ得る。

chunk loader、heightMap、tile entity更新、lighting、section allocation、terrain state registryは[world・保存](02-world-storage.md)。fullchunk受信がそのまま全周辺chunk生成や全lighting完了を意味しない。

### 8.2 Block IDsと packed local position

S21の16 bitstate、S22/S23のVIstateは原版 `Block.BLOCK_STATE_IDS` のregistry IDである。通常はblock IDとmetadataの組のregistry値を使う。`Block.getStateId` が別箇所で使う `blockId + (metadata << 12)` と、全用途を一つの式にまとめない。state holes、aliases、invalid lookupは[world・保存](02-world-storage.md)に従う。

S22 localPositionはX=bits12..15、Z=bits8..11、Y=bits0..7。chunk coordinatesからworld X/Zを作る。S24はreaderでblockIdを4095 maskしてBlock lookupする。eventId / parameterの意味はblockごとのevent handlerで決まり、常にinstrument/pitchという型名にはしない。

### 8.3 S34 maps

packedIconは上位nibble type、下位nibble rotation。X/Zはsignedbyteのmap-relative値。packetにmap center、dimension、player object、decoration keyはない。width=0はcolor patchなしで、height/startX/startZ/Bytesも存在しない。width>0ではrectangleを128×128 colorsへ適用する。sourceIndexは `x + z*width`、destinationは `startX+x + (startZ+z)*128`。

readerはwidth/height/patchlengthの幾何学的一致をpacket codecで強制しない。constructorのrectangle copy、applyのarrayアクセスは各実bodyの順で、invalidrectangle / shortarray時にpartial stateが残り得る。iconsは読まれた配列から既存decorationsをclearして入れ直す。map colorsのbyteは色indexとshadeに解釈されるが、array reference alias / save / MapInfo dirty cadenceは[world・保存](02-world-storage.md)、ItemMap survey・visibilityは[gameplay](04-gameplay.md)に属する。

## 9. Window、confirmation、entity lifecycle の確認済み handler 規則

### 9.1 Window mappingと resync

player inventoryContainerのwindowは0。通常のplayer slot mappingは0=result、1..4=2×2 inputs、5..8=helmet/chest/leggings/boots、9..35=main、36..44=hotbar。InventoryPlayerの内部indexとは別である。workbenchは0=result、1..9=3×3 inputs、10..36=player main9..35、37..45=hotbar0..8。他container固有mappingは[gameplay](04-gameplay.md)で定義する。

S2D slotCountは常に後続S30全slot数を意味しない。例えばcrafting tableの通常OpenWindowはslotCount=0でも、containerは46slotを持つ。horseだけにentity tailが付く。window IDのread符号は表のとおりで、S2E/S30/S32がunsigned、S2FとC0D/C0E/C0Fがsignedであることを統一しない。

Source client S2F handlerは次の順で分岐する。

1. window=-1ならglobal cursorへpacket stackをsetする。
2. window=0かつslot36..44ならinventoryContainerを更新する。incoming非NULLが以前より増加、または以前NULLならincoming.animationsToGo=5。その後slotへ採用。
3. それ以外はwindowIdが現在openContainerと一致し、かつwindow!=0またはcreative GUIのinventory tab条件を満たす場合、現在containerへputする。

**この対象原版には S2F window=-2 をInventoryPlayer indexに変換する専用分岐はない。** 後のMinecraft protocol仕様を逆輸入しない。S30はwindow0ならinventoryContainerへ全slots、その他はcurrent openContainerのwindow一致時だけ全slotsをputする。S30そのものにcursor fieldはない。

server C0Eはcurrent window一致とgetCanCraftを確認する。spectatorはsnapshotを返すbranch。通常は **slotClickを実行してから**、そのreturned stackとclient clickedReturnを `ItemStack.areItemStacksEqual` で比較する。

- 一致ならS32 accepted=trueをsendし、isChangingQuantityOnly=true→detectAndSendChanges→updateHeldItem→falseの順。これは「クリック自体を全拒否まで保留する」方式ではない。
- 不一致ならwindow/actionを記録し、S32 false→getCanCraft=false→current slot list採取→updateCraftingInventory。既に行ったslotClick effectsをrollbackしない。updateCraftingInventoryはS30とcursor S2F(-1,-1)を送る。
- client S32 negativeはwindow0または現在openContainerに対応する場合だけC0F accepted=trueを送る。accepted=trueのS32にC0Fを返す一般規則はない。
- server C0Fはstored action、current window、現在getCanCraft=false、非spectatorを確認しgetCanCraft=trueにする。C0F.accepted自体をこの条件で参照しない。

rejection後にsnapshotとcursorをどう待つか、UI inputをどう止めるかという独自client policyを原版handlerの別条件として追加しない。IDs再利用、slot updateの到着順、window寿命を現在containerの参照で処理する。

### 9.2 Closeとcreative

server C0DはpacketのwindowIdと比較せずplayer.closeContainerを呼ぶ。client S2EもpacketのwindowIdと比較せずplayer.closeScreenAndDropStackを呼ぶ。**server強制closeにC0D replyはない**。通常SP.closeScreenはC0Dをqueueした後にlocal close。SP.closeScreenAndDropStackはcursor=NULL→親close→GUIをNULLにする順。crafting grid/resultのcleanupはGUIのonGuiClosedなど後続virtual callbackの条件にも依存し、「全IDのS2E受信で常に全gridをclear」としない。

server C10はcreative限定。slots1..44の直接putと負slotのdropを区別する。非NULLstackはItem非NULL、metadata≥0、count>0かつ≤64を確認する。item maxStackSize=1を理由にcount2をcodecまたはこの直接putから拒否しない。BlockEntityTagにx/y/zがある場合のserver tile NBT置換が先に行われる。負slot dropはitemDropThreshold<200ならthreshold+=20、dropPlayerItemWithRandomChoice(...,true)、nonnullEntityItemにage=4800。threshold decrement、drop trajectory、owner/pickup、stats、saveは[gameplay](04-gameplay.md)。

### 9.3 Movement、entity、keep-alive

C03/C04/C05/C06は同系packetでmoving/rotating flagが型で決まる。riding歩行送信ではX/Zをmotion値、Y=-999でC06を使うbranchがある。通常SP.onUpdateWalkingPlayerのsprint/sneak action→movement selection→report state store順、distance threshold、20tick heartbeatは[gameplay](04-gameplay.md)と[client](05-client-resources.md)を参照する。S08ではrelativeの各coordinateに現在posを加え、absoluteの各coordinateでは対応motionを0にする。relative pitch/yawには現在角度を加える。その後setPositionAndRotation→C06で現在posX、boundingBox.minY、posZ、yaw、pitch、onGround=falseを送る。初回terrain完了branchはprevPosを現在posへstore→doneLoadingTerrain=true→GUI closeの順。これはwindow transaction番号ではない。

entity Spawn→metadata→velocity/move→collect/destroyは、同entity IDを使うが同じpacketではない。S0E type2はEntityItemで、そのItemStackはDataWatcher index10/type5で届く。S0EにはSlotが含まれない。S0Dはcountを載せずcollect effect/removalを処理する。spawn時のposition constructor motionをdata>0の場合だけ後段velocityで置き換えることと、data0のvelocity欄不在を保持する。

S0Eの通常client type対応はboat1、item2、minecart10、TNT50、ender crystal51、arrow60、snowball61、egg62、large fireball63、small fireball64、ender pearl65、wither skull66、falling block70、item frame71、ender eye72、potion73、experience bottle75、firework76、leash knot77、armor stand78、fish hook90。dataは型ごとに意味が違う。falling-block state、minecart variant、frame facing、projectile owner、fireball accelerationのconstructor/effect詳細まで本章で閉じたとはしない。mobTypeはEntityList registry、playerはprofile lookupを使う。

client S00 keep-aliveは同keyのC00をqueueする。server C00は現在のping keyと一致する場合、elapsed millisecondsからpingを `(previous*3+elapsed)/4` で更新する。これはInt/Long演算を含む。keep-aliveをinventory confirmationとして扱わない。C09のserver適用は0..8だけで、invalidslotではwarnする。Netty TCP pipelineのReadTimeoutHandlerは30秒であり、login600ticksと別である。

## 10. Custom payloadと JSON 内部構造

### 10.1 Vanilla custom channels

channel名はcase-sensitive String。C17/S3Fの外側にはpayloadの内側lengthを付けない。外側packet readerは残りbytesをPacketBufferとして渡す。vanilla handlerが内部fieldを読む場合、必ずしも全残りbytesの消費一致を検査しない。

| 方向 / channel | 内側wireと確認した採用条件 |
| --- | --- |
| SB `MC\|BEdit` | `Slot book`。writable book NBT validation後、held writable bookとitem一致ならpages tagを採用。wholeinventory replacementではない。 |
| SB `MC\|BSign` | `Slot book`。written book NBT validation後、held writable bookならauthorはserver player名、title/pagesを採用しwritten_bookへ変更。 |
| SB `MC\|TrSel` | `I32 recipeIndex`。現在ContainerMerchantなら選択。 |
| SB `MC\|AdvCdm` | `I8 targetKind`; 0なら`I32 x,y,z`、1なら`I32 entityId`; 続いて`Str(current readableBytes) command, B trackOutput`。command block有効、permission2、creative条件。 |
| SB `MC\|Beacon` | `I32 primaryEffect, I32 secondaryEffect`。ContainerBeacon、payment slotがあると1個消費→fields set→markDirty。 |
| SB `MC\|ItemName` | dataが1byte以上なら`Str(32767)`をallowed character filterして長さ≤30ならanvil名へ。empty dataは空名。 |
| CB `MC\|TrList` | `I32 windowId, U8 tradeCount`; 各tradeは`Slot firstBuy, Slot sell, B hasSecondBuy, (trueならSlot secondBuy), B disabled, I32 uses, I32 maxUses`。現在GuiMerchant/window一致時にrecipe list採用。 |
| CB `MC\|Brand` | `Str(32767) brand`。 |
| CB `MC\|BOpen` | handlerは追加fieldを読まず、held written_bookならbook GUIを開く。 |

`REGISTER`/`UNREGISTER`やmod/plugin独自channelをvanillaの登録packet一覧に増やさない。未知channelのmod-level契約は本章の対象外。book NBT validationのページ長、generation、内容、command permissions/trade/remaindersは[gameplay](04-gameplay.md)。payload例外をlogしてreturnするbranchがあるため、全内部decode失敗でTCPを必ずcloseするとは言えない。

### 10.2 Chat JSON

ChatはJSON primitiveのGson getAsString、component配列、またはcomponent objectとしてdecodeする。top-level配列は最初のcomponentをreceiverとし後続をsiblingsにappendし、空配列はNULLを返す。objectのcontent選択順はtext→translate→score→selector。translateはkeyとoptional with、scoreはname/objective必須とoptional value、selectorは文字列。extraはsiblings配列で、存在するempty extraは失敗。primitiveのserialization、empty styles、translation argumentsの簡略化により、意味が同じcomponentでも固定の唯一JSONbyte列を要求しない。

styleはoptional bold、italic、underlined、strikethrough、obfuscated、color、insertion、clickEvent、hoverEvent。clickEventはaction/value、hoverEventはaction/component valueで、recognized actionかつallowInChatのものだけ採用する。親styleとsiblingsの継承、translationのformat/locale、score/selectorの解決、allowed URL/UI動作、resource font/renderは[client・resources](05-client-resources.md)。Gson/JDKのmalformed JSON/coercion/exception message全体は未閉包である。

clickのrecognized namesはopen_url、open_file、run_command、twitch_user_info、suggest_command、change_page。このうちopen_fileとtwitch_user_infoはallowInChat=falseで採用しない。hoverはshow_text、show_achievement、show_item、show_entityで、四つともallowInChat=true。未知actionはそのeventを採用しない。後のversionのfont/hex-color/hover contentsなどを1.8.9 JSON fieldsに加えない。

C12 signは4×Str(384)を個別にJSON component化する。S33は4×ChatでStr上限32767。同じsign line用途でもcodecの上限を統一しない。S42のunformatted Stringをここへ混ぜない。

## 11. Tick handoff、参照、queue、切断

`PacketThreadUtil.checkThreadAndEnqueue` は現在threadが指定world/clientのthreadでなければ、同じpacket参照とhandlerでprocessPacketを行うscheduled taskを追加し、ThreadQuickExitExceptionで元の呼出しを抜ける。NetworkManagerの受信はその制御例外をcatchする。main threadならそのまま次のhandler文へ進む。全packetがこのhelperを使うとは限らず、keep-aliveやcompression等を無条件に同じtickqueueへ移さない。

NetworkManagerは未open channelへの送信をqueueする。open時はoutbound queueをflushした上でpacketをdispatch。送信packetのclassからconnection stateを求め、必要ならautoReadを停止し、event loopでstate変更→writeAndFlush→listener処理を行う。`processReceivedPackets` はqueued outbound flush、ITickable listener.update、channel.flushの順で、login受理のtick更新を含む。全tickとpacketごとの永続journal、graph clone/adoptは原版NetworkManagerの契約ではない。

packet constructorsでcopyされるItemStack/NBT、directly retained array/list/position、handlerがゲームobjectへ採用する参照は各source methodごとに違う。wireを渡るとoccurrenceごとに新規objectになる。local in-memory channelはserializationを通さないため、remoteTCPと同じcopy境界だと仮定しない。UUID/nameによるregistry lookupとheap pointer identityも別である。

切断packetはLOGIN S00またはPLAY S40のChat。stateに合ったIDで出す。status Pong後のcloseはChat disconnect packetを必ず送るという規則ではない。channel inactive、timeout、decode例外、disconnect GUI callback、server logout/save/craft-close処理を一つの「close meansrollback」にまとめない。

## 12. 実装可能性の監査と残る仕様 gap

| 領域 | この章での到達点 | 残る具体的仕様 |
| --- | --- | --- |
| 全packet registry | 111/111、4state、全方向、全ID、実registered class | classfileと提供decompile Sourceが異なる箇所の体系的照合は未実施。 |
| 全packet wire schema | 111/111のread/write、型・順・条件・lengthを静的確認 | 全constructor overload/getter alias、全malformed input exception prefixの網羅的明文化・実行比較は未実施。 |
| TCP codecs | frame、VI/VL、compression、AES、RSA/hash、NBT/Slot/Meta/Chunkの境界 | Netty全例外/allocator/provider、repeat compression設定 branch、JCE provider差、raw NaN sign/payloadのtarget-JVM差。 |
| Login / status | 通常状態、online/local/offline、timeout、compression/AES切替、status response | authlib HTTP/TLS/token/profile署名、全server admission policy、duplicate login/logoutとsaveの完全順。 |
| Play handlers | window/rejection/close、creative、keep-alive、chunk unload、custom channelの上記branch | 全74CB/26SB handlerの全branch・呼出先world/entity/item/container/GUI状態機械。 |
| Entity wire意味 | fixed coords、velocities、angles、S0Etype対応、metadata generic types | 全EntityList ID、全entity watcher index/opcode、trackerの視距離/period/順序、collect sound/particle/interpolationと消滅寿命。 |
| World wire意味 | chunk byte layout、state ID用途、map patch、border actionfields | 全block/state/property registry、tileType/action/effect/sound/particle意味、lighting、全chunk/tile/entity効果。 |
| UI / resources | Chat JSON、window IDs/slotsの上記mapping、resourcepack packet | 全container type/slot mapping、GUI callback、skin/texture/profile fetch、resourcepack URL/hash検査/download/caching/reload。 |
| Persistence / identity | wireとsaveとnativeobject参照を分離 | server永続化と例外時partial prefix、multi-owner aliasを全handlerにわたって閉じる仕様。 |

全schemaが書けたことは、全Minecraft互換の完成判定とはしない。実装者は表からpacket codecを構築できるが、未記述handler・外部依存をguessや空成功で埋めることは本仕様の充足にならない。未調査のgame semanticsはcoverageのgapとして残し、[適合性](06-conformance.md)で原版・境界・検証状況を独立に評価する。
