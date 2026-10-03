# 実行モデルと翻訳契約

対象版・完了条件は[総合仕様](README.md)。本章は数値・オブジェクト・主要時計の契約を定める。JVM／JDK／Guavaの全到達API、完全なThrowable階層、全schedulerを閉じた仕様ではない。

## RT-01 型と整数

| Java型 | C側に必要な意味 |
|---|---|
| boolean | false/true。packetのbyte表現と同じ型にしない |
| byte | signed8。読んだbyteをunsignedで使う操作は明示 |
| short | signed16。代入／cast時に下位16bitを採用 |
| char | unsigned16のUTF-16 code unit。Unicode scalarやUTF-8 byteではない |
| int | signed32、加減乗は下位32bit、2の補数解釈 |
| long | signed64、加減乗は下位64bit、2の補数解釈 |
| float | binary32。原版の演算・cast位置で丸める |
| double | binary64。floatとの昇格／縮小を省略しない |
| reference | nullable、identity、動的型、lifetimeを持つ |

Cのsigned overflowは使用せず、unsignedでmodulo演算したあと符号付き値を表す。`MIN_VALUE / -1`は元のMIN_VALUE、整数の0除算はArithmeticException。除算の商は0方向、剰余の符号は被除数側。負座標をchunkへ移す右shiftと0方向除算は同じではない。

shift距離はintでは下位5bit、longでは下位6bitを使用。`>>`は符号拡張、`>>>`はゼロ埋め。byte/short/charは通常intへ昇格してから演算し、compound assignmentは暗黙の縮小castを含む。

float/double→int/longはNaNが0、範囲外が端値へ飽和、範囲内は0方向。続いてbyte/shortへ縮小する場合は整数下位bitを用いる。負の0、Infinity、NaNを入力拒否へ一律変更しない。原版のguardがある場所だけ、そのguardを実行する。[Java 8変換規範](https://docs.oracle.com/javase/specs/jls/se8/html/jls-5.html)が境界の根拠になる。

## RT-02 浮動小数とMathHelper

fast-math、式の再結合、FMA化、float中間値をdoubleへ置換する最適化で観測を変えない。bit比較、NaN比較、signed zero、cast直前の値、乱数の結果を分けて試験する。Java8の非strictfpと対象JVMの実演算も、bit完全一致を要求する境界では固定する。

`floor_double`はcast結果iを先に作り、元値<iならi-1、そうでなければi。`ceiling_double_int`は元値>iならi+1。通常の数学的floor/ceilだけでは、NaN、端値、overflowした追加減算を説明できない。`clamp`は下限比較、上限比較の順であり、NaNならどちらもfalseになる。

`MathHelper.sin`／cosはlibm直接呼出しではない。65536個のbinary32 sin表を用い、sinはbinary32の角度×10430.378Fをintへcastして65535でmask、cosは同じ積に16384.0Fを足してからcast／maskする。表生成に用いるdouble式・Math.sinの結果の縮小も版固定が必要。全table、atan2補助table、全MathHelper関数のbit契約は未充足として追跡する。

Java8のMath／StrictMathの対象APIをCの同名関数だけで適合扱いしない。特にGaussianのsqrt/log、sin表生成、NaN payloadを、原版とnativeの区別が付く形で照合する。

## RT-03 評価順と失敗prefix

式のoperandと引数は左から順に評価する。`&&`／`||`／conditionalは選ばれた側だけを評価する。途中で例外が起きると後続は実行しない。receiver取得、引数、dynamic dispatch、fieldやarrayのcheckを別の段階として記録する。[Java8式の規範](https://docs.oracle.com/javase/specs/jls/se8/html/jls-15.html)を適用する。

単純なarray代入ではarray参照とindexを先に評価し、RHS評価後にnull／boundsを検査する。compound array代入では要素を読む段階の検査がRHSより先になる。共通の安全チェックを関数入口へ移すと、その前後の副作用が変わる。

メソッド仕様の失敗記録は、throwable型、throw発生点、捕捉先、済んだfield書込、他receiver書込、乱数状態、外部呼出し列を含む。出力変数不変だけでは、元bodyに入った後の例外を証明できない。反射がNULL receiverを拒否した結果を、body自身のNULL入力実行と数えない。

## RT-04 オブジェクトと初期化

allocation直後のinstance primitiveは0／false、referenceはNULL。constructorは親constructor、当該classのfield initializerとinstance initializer、constructor bodyの順に働く。親constructor内のvirtual callは最派生overrideへdispatchでき、子field初期化前の0／NULLを観測する。Cでは親と子が別objectにならない。

最派生allocation、constructor引数評価、親への引数、field初期化、各bodyを一括して初期値tableへ置換しない。new式の対象allocationは当該constructor引数の評価より先であり、allocation失敗時に引数の副作用を先に済ませない。overrideがfield、world、random、queueを読む場合に順序が露出する。constructor完了前の失敗でも外部staticやプロセス効果は元どおり残る。

class初期化はinstance初期化と別の状態機械を持つ。宣言staticの初期値、親初期化、原版のtext順、同thread再入、別thread待機、初期化失敗と次回アクセスのthrowableを規定する必要がある。現在のnative rooted statics serviceはこれら全JVM挙動の完成仕様ではない。[Java8初期化規範](https://docs.oracle.com/javase/specs/jls/se8/html/jls-12.html)を適用し、実到達classごとの詳細は充足表へ結ぶ。

`instanceof NULL`はfalse。参照castはNULLを許し、動的型不一致はClassCastException。`==`はidentity、`equals`は動的dispatch。enumは宣言順ordinal、固有name、同一constant identityを保持し、EnumSet／EnumMap等は別のcollection契約として定める。

## RT-05 配列・文字列・collections

配列はlength不変、identityあり、element初期値あり。primitive配列とreference配列、成分型と多次元配列を区別する。reference array storeは動的component typeを検査し、`System.arraycopy`にはoverlap、primitive型不一致、referenceの途中ArrayStoreExceptionによるprefix copyの契約がある。

StringはUTF-16 code unit列。length、substring index、hash、比較、case conversion、UUID名生成にbyte列の長さを使わない。`String.hashCode`は各unitについてsigned32の31倍＋unit。通常UTF-8、DataInputのModified UTF-8、NBTStringの保持表現、JSON escapeは別codec。未paired surrogateのreplacementはそのcodec契約に属する。

collectionはnullable、iteration order、identity/equals/hash、mutable view、fail-fast、copy、builder、empty singleton、duplicate key、entry replacementを個別に指定する。JDK HashMapとLinkedHashMap、Guava ImmutableMap／ImmutableList／ImmutableSet／ImmutableTableを一つのnative vectorへ寄せない。PropertyをkeyとするmapはString専用mapではない。

Guava17のBlockState到達closureは、property sort、allowed values、Cartesian product、MapPopulator、immutable property map、transition tableを含む。全collection semanticsと全iterator failure-prefixは本版では未閉鎖。数学的直積の一般定義だけで元のiteratorを実装しない。

## RT-06 Java Random

乱数は各所有者のstreamとprocess側seed serviceを区別する。LCG内部48bit状態s、multiplier `0x5DEECE66D`、addend11、mask `2^48-1`。setSeedは入力seed XOR multiplierをmaskし、Gaussian cacheを無効化する。next(bits)は更新を一度行い、上位bitsを返す。[Java8 Random](https://docs.oracle.com/javase/8/docs/api/java/util/Random.html)が互換計算を規定する。

| 操作 | 消費と生成 |
|---|---|
| nextInt() | next(32)、signed32 |
| nextInt(bound) | bound>0。2冪ならbound×next(31)の上位、他は剰余とsigned32 overflowを含むrejectionを繰返す |
| nextLong() | next(32)を2回。上位を32bit左へ置き、下位のsigned32を符号拡張して加算 |
| nextBoolean() | next(1) |
| nextFloat() | next(24)／2^24、binary32 |
| nextDouble() | next(26)とnext(27)を結合して2^53で除す |
| nextBytes() | nextIntの低byteから最大4byteずつ。最後の不足byteでも一回を消費 |
| nextGaussian() | cacheがあれば消費0。なければ2つのuniformから半径を作るpolar rejection、sqrt/log、2つのうち一つをcache |

boundが2冪でないとき、bits-nextValue+(bound-1)のsigned32値が負なら再抽選する。Cの無限精度式で判定しない。Gaussianはradius>=1または0で再抽選する。generatorをobjectごとコピーして同一streamとして扱ったり、失敗transactionでprocess seedを巻き戻したりしない。

## RT-07 Timer

元fieldはticksPerSecond、lastHRTime、elapsedTicks、renderPartialTicks、timerSpeed、elapsedPartialTicks、lastSyncSysClock、lastSyncHRClock、counter、timeSyncAdjustment。timerSpeed初期値1.0F、adjustment1.0D、残りはdefaultのあとconstructorがticksPerSecond、system millis、nanoTime/1000000を順に設定する。

updateTimerの状態遷移:

1. system millis i、前syncとの差j、nanoTime/1000000の整数k、k/1000.0の秒dをこの順で取得する。
2. 0<=j<=1000ならcounterにjを加える。counter>1000ならcounter/(k-lastSyncHRClock)を計算し、adjustmentへ差の`0.20000000298023224`倍を足し、HR syncとcounterを更新する。更新後counter<0ならHR syncのみ更新する。
3. それ以外のjではlastHRTime=dにする。
4. system sync=i、経過秒=(d-lastHRTime)×adjustment、lastHRTime=d、経過秒を比較順で[0,1]へclampする。
5. elapsedPartialTicksをdoubleへ昇格し、経過秒×timerSpeed×ticksPerSecondを足し、全体をfloatへ縮小する。
6. int castでelapsedTicksを取得し、そのcast値をfloatへ戻してpartialから引く。そのあとelapsedTicks>10のみ10へ制限する。
7. renderPartialTicksに残りpartialを代入する。

10tick上限はpartial減算後である。全deltaを最初に10tickへclampする処理と違う。負値やNaNを勝手に0へ変えず、castと条件の挙動を維持する。基準はTimer、Minecraft.getSystemTime、Java numeric conversion。

## RT-08 クライアントframeとtask

runGameLoopの主要順序はclose request検査、Timer更新、scheduledTasksのlock付きdrain、elapsedTicks回runTick、音listener更新、framebufferとcamera/world描画、GUI／achievement／present、次frameのpause判定、計測／FPS／frame制限である。

pause済みかつworldがある場合もTimer全体を更新し、更新前renderPartialTicksだけを戻す。pause判定はframe末尾で更新し、singleplayer、pauseするscreen、公開LANでないintegrated serverの条件を使う。multiplayerのmenu表示だけではserver gameをpauseしない。

scheduled taskはpacketのcallbackをtickへ引き渡す所有者を持つ。PacketThreadUtilは別threadで同じpacket/handlerをcaptureしたtaskを登録したあとThreadQuickExitExceptionを投げる。元threadでhandler後半を続けない。enqueue時にpacketをdeep-copyして別receiverへ変えない。全runTickのbranch順、task例外のlog/CrashReportは未充足。

## RT-09 サーバー時計とtick

runは起動後にcurrentTimeをmillisで設定し、累積遅延を0にする。各反復はnowとdeltaを取得する。delta>2000かつ前warningから15000以上の条件を満たした場合だけdeltaを2000に制限しwarningを更新する。負deltaは0。全sleep過剰を常時2000へ制限する契約ではない。

累積遅延にdeltaを足しcurrentTime=now。Overworldの全playerが睡眠状態ならtickを一度呼び遅延を0へ戻す。それ以外は**遅延>50**の間50を引きtickを呼ぶ。最後にmax(1,50-遅延)msをsleepする。`>=50`へ変更しない。

tickCounterを増やし、time/light/entity更新を行う。900tick毎のplayer/world save、100slotのtick timing、snooper周期等がある。updateTimeLightAndEntitiesはscheduled task、dimension world tick／entity、entity tracker、network endpoints、player manager、追加tickableの順を別契約として閉じる必要がある。本版はこの全child body／failure policyの実装可能な記述を完了していない。

## RT-10 未充足と照合ケース

未充足: 全MathHelper tableと関数、Unicode locale/case／formatの全契約、JDK8全到達collections、Guava17全到達API、static initialization concurrencyとThrowable、全Minecraft.runTick／server scheduler／CrashReport。

数値試験はMIN/MAX、-1/0/1、shift31/32/63/64、signed zero、NaN、Infinity、float丸め境界を含む。所有者試験は同じ参照／同値別参照／NULL／constructor中override／静的再入。乱数試験は固定seed、bound1/2/3/大値、rejection、Gaussian cache。時計試験は後退、1000/1001、counter1000/1001、partial10超、pause、task発生中のdrainを含む。各試験を元のmethod個別契約へ結ぶまでは、全章完成と判定しない。
