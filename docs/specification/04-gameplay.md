# 04 ゲームプレイ、アイテム、インベントリ、エンティティ

この章の対象は Minecraft Java Edition 1.8.9 のゲーム規則である。現行 C 実装の機能一覧ではなく、提供された原版のクラスと処理を基準にする。**本章は完全仕様の完成版ではない**。インベントリ、クラフト、いくつかの tick と保存処理は具体化したが、全 Block/Item の仮想動作、全 mob、全コマンドなどには末尾で列挙する未充足条項がある。それらを実装者の推測で補っても、完全互換とは判定しない。

通信のフレーム・各パケットのフィールドは通信章、チャンク・生成・照明・保存ファイルの外殻はワールド章、Java 数値/NBT/文字列/コレクションの共通規則は実行環境章と合わせて適用する。本章中の `null` は参照の欠如であり、個数0の非 null スタックと区別する。

参照: [通信](03-network.md)、[ワールドと保存](02-world-storage.md)、[実行モデル](01-runtime.md)。Material/MapColor全静的データ、property equality、canonical state graphとBlock constructorの順は[MaterialとBlockState](07-material-block-state.md)へ分ける。

## 1. 原版に従う状態と評価規則

### 1.1 所有者と参照同一性

- 一つの Entity は、最も派生した型から Entity 基底まで同じオブジェクトである。Player、Living、Entity に別々の位置・UUID・World・DataWatcher を持たせない。SP、ACP、MP の追加状態も同じ継承オブジェクトに属する。
- Item と Block の登録オブジェクト、レシピのテンプレート、実行中の ItemStack、NBT、各 inventory、Container、Slot、落下 Item Entity を区別する。Slot は inventory と slot index を参照する窓であり、独立した authoritative スタックを所有しない。
- `setInventorySlotContents`、NBT compound の `setTag`、結果として同じ参照を返す getter は、指定がない限り参照を共有する。値が同じだからコピーしてよいという規則はない。
- ItemStack の `copy`/`splitStack` と NBT の `copy` は原版の各辺ごとの再帰コピーである。同じ子が二か所から参照されていた場合、その二辺を一個の子にまとめるコピーではない。一方、移植先の全体取引 snapshot は、全 live owner を一つの memo で複製して alias を保存する必要がある。これは原版メソッドとは別の運用機構である。
- ネットワークと保存は各出現箇所を符号化する。読み戻した別々の ItemStack/NBT は新しい個体になる。参照共有を維持するための独自 identity ID を通常の Minecraft NBT やパケットへ加えない。
- 同じ ItemStack が複数スロット、クラフト格子、別プレイヤー、EntityItem に共有される状態は合法な到達状態として扱う。個数0または負数だけを理由に全 owner から消さない。

### 1.2 数値、仮想呼出し、失敗

整数の加減乗算、byte/short 縮小、符号付き shift、unsigned shift、float/double 演算の丸め位置は Java の規則に従う。C の signed overflow、引数評価順、実数から整数への未定義変換に依存しない。float 定数を double 定数に置き換えたり、途中演算を一括 double 化したりしない。NaN、無限大、負の0に原版にない修復を加えない。

一つの式でも receiver を先に捕捉し、次に引数を左から右へ評価する。引数 callback がフィールドを書き換えても、捕捉済み receiver は切り替えない。次の独立した文はその時点のフィールドを再読込する。たとえば inventory animation のスタック receiver は World getter より前、GUI overlay の renderer receiver は font getter より前に捕捉する。

原版が例外を投げる箇所より前の変更は残る。クリック、NBT 読込、listener、World 呼出しなどに暗黙 rollback を追加しない。原版の捕捉された Exception と捕捉されない Error、移植環境のメモリ不足・foreign heap・未翻訳依存の失敗を区別する。必要な依存を実行できない枝は明示失敗であり、空 callback による成功ではない。原版自身の空の基底メソッドは、その基底の動作として実装できる。

### 1.3 tick と権限

サーバーの論理 tick は20回/秒を目標とするが、実時間の Timer、tick 番号、World 時刻、乱数、GUI frame の partial tick は別状態である。10 tick を1 tick の大きい移動量にまとめない。クライアント予測とサーバー権限判定を分離し、ゲームモード、capabilities、所持品、開いた Container、距離、World remote 属性を、それぞれ原版が読む位置で読む。

PlayerCapabilities は `disableDamage=false`、`isFlying=false`、`allowFlying=false`、`isCreativeMode=false`、`allowEdit=true`、fly speed `0.05F`、walk speed `0.1F` で始まる。Creative 設定は damage 無効・飛行許可・creative を true にするが、既存 isFlying は保持する。Spectator は damage 無効・飛行許可・isFlying=true、creative=false。その他のモードは飛行/creative/damage 無効を false にする。allowEdit は Adventure/Spectator で false、それ以外 true。能力パケットによる flags/speed 更新と GameType の変更は別の権限経路であり、S39 を受けただけで GameType を変更しない。

## 2. Block、Material、Item の登録

### 2.1 登録に必要な情報

Block 登録は整数 ID、ResourceLocation、同じ Block オブジェクトの対応を持つ。Item には独立の登録と Block→Item の対応がある。Block ID と Item ID が同じでも同じ Java 個体ではない。ItemBlock のない Block を、数値 ID だけで Item に変換しない。未知の Item 参照の登録 ID は -1、null Item は0という区別を保つ。

登録の初期化順は、オブジェクト割当て、constructor、各 setter、registry store の順を保存する。途中で例外が起きた場合、先行して生成・変更されたオブジェクトは残り、後続登録を先取りしない。ResourceLocation の省略 namespace は `minecraft` として解釈する。登録表の名前は翻訳キー、表示文字列、モデル名とは別である。

Block登録後はregistry順に各Blockの全valid statesを巡回し、`BLOCK_STATE_IDS`のIDを `(Block ID << 4) | getMetaFromState(state)` としてidentity対応へ登録する。これは`Block.getStateId`の `Block ID + (metadata << 12)`とは異なる符号化である。後者の逆変換はIDのlow12bitと次の4bitを分離してgetStateFromMetaへ渡す。Chunk storage/パケットがどちらのAPIを使うかを通信/ワールド章で指定する。複数の合法stateが同metadataへ潰れる場合もあるため、単に198×16個の架空stateを作らない。逆引き・重複storeの規則は[MaterialとBlockStateのBS-11](07-material-block-state.md#bs-11-objectintidentitymap)のObjectIntIdentityMap契約へ従う。

各 Block について少なくとも次の全事実が必要である。下の ID/name 表だけでこれらが完成したとは扱わない。

| 分類 | 必要な仕様 |
|---|---|
| 型と初期状態 | 実際の subclass、Material 参照、default IBlockState、各 property の型・許容値・順序 |
| 物理 | collision AABB、selection bounds、solid/opaque/full cube、slipperiness、hardness、blast resistance、harvest/tool 判定 |
| 状態変換 | metadata→state、state→metadata、actual state、rotation/接続形状、legacy state ID と palette |
| 作用 | placement、neighbor update、scheduled/random tick、entity collision、activation、破壊、drop、fortune/silk touch |
| 周辺系 | light emission/opacity、redstone weak/strong power、TileEntity 作成/保持、render layer、MapColor、sound |

Material は flags の別ミラーを作らず実 Material を参照する。Liquid/Logic/Transparent/Portal/匿名派生の override を base の flags から推測しない。MapColor の named static 参照と公開64要素配列は別の参照位置であり、配列要素を置き換えても named static 参照は変更しない。

各 Item には subclass、stack limit、max damage、hasSubtypes、container Item、tool material、装備属性、食物/薬効、使用時間/EnumAction、onUpdate/onCreated/onItemUse/onRightClick/onUseFinish 等が必要である。空の base onUpdate を全 Item の動作として流用しない。ItemMap のような override に実際に dispatch する。

ToolMaterial の数値は次のとおり。`EMERALD` はこの原版でダイヤモンド装備に使う enum 名である。

| 材質 | harvest level | max uses | 適正素材効率 | 対 Entity 加算 damage | enchantability |
|---|---:|---:|---:|---:|---:|
| WOOD | 0 | 59 | 2.0F | 0.0F | 15 |
| STONE | 1 | 131 | 4.0F | 1.0F | 5 |
| IRON | 2 | 250 | 6.0F | 2.0F | 14 |
| EMERALD | 3 | 1561 | 8.0F | 3.0F | 10 |
| GOLD | 0 | 32 | 12.0F | 0.0F | 22 |

### 2.2 ItemStack

ItemStack は Item 参照、signed int count、damage、animationsToGo、nullable NBT compound、frame/Adventure 判定 cache 等を持つ。通常 constructor は負の damage を0へ変更するが、count の符号を修正しない。`copy` は新 ItemStack、同じ Item、count/damage、NBT があれば深いコピーを作る。アニメーションや cache を元と同じ値へ丸ごと複写しない。

`splitStack(n)` は先に count=n の新スタックを constructor で作り、NBT をコピーし、その後元の count から n を引く。n の入力に「必ず1以上、元 count 以下」という一般防衛条件を追加しない。割当てまたは NBT copy が失敗した時に先行/後行状態が異なるため順序を固定する。

等価判定を一種類に統合しない。

| 判定 | 比較するもの |
|---|---|
| `areItemStacksEqual` | 両 null、または Item identity・count・damage・NBT の構造等価 |
| `isItemEqual` | Item identity と damage。count/NBT は比較しない |
| `areItemStackTagsEqual` | null 組合せと NBT 構造等価 |
| inventory の merge | 当該メソッドが指定する subtype 条件、stackability、NBT、容量 |

NBT 保存は `id` を登録 resource name（登録名なしは `minecraft:air`）、`Count` を signed byte、`Damage` を signed short にし、tag があれば**同じ NBT 参照**を `tag` に入れる。読込は id が String 型なら名前解決、そうでなければ short ID 解決→Count byte→Damage short/負を0→tag が Compound 型なら直接参照→Item の updateItemStackNBT の順。tag がない読込は既存 tag を消さない。新規 load factory は読後 Item が null なら null を返す。wire で未知 Item を読んだ時の挙動を、この保存 factory と同一視しない。

`updateAnimation` は animationsToGo>0 のとき先に1減らし、その後 Item.onUpdate を呼ぶ。Item が null で呼出しに失敗しても減算は済んでいる。`onCrafting` は対応 objectCraftStats の増加、次に Item.onCreated の順。対応 stat が null である Item のために架空 stat を作らない。

## 3. Inventory、Container、Slot

### 3.1 InventoryPlayer の実体

mainInventory は36参照、armorInventory は4参照。main0..8が hotbar、9..35が保管領域。armor0..3は足・脚・胴・頭。cursor は別の一参照であり40スロットの配列に含めない。currentItem は signed int 状態で、current stack getter は hotbar 範囲外の場合 null を返す。Player の実際の inventory 参照を各 statement で読む。

初期の`getSizeInventory` は40で、実getterはその時点のmain配列length+4を返す。stack limit は64。getHasStack/空判定は原版の null 検査であり、count0のスタックは空ではない。decrStackSize は既存 count<=要求なら元の参照を返してスロットを null、そうでなければ split し、残り count==0 のときだけ null にする。

挿入 `addItemStackToInventory` は null、count==0、Item==null なら false。damaged stack は最初の null main に copy を入れ、animation=5、入力 count=0。空きなしでも Creative なら入力 count=0 として true。非 damaged は、同 Item/許可 metadata/NBT の既存スタックを先に選び、その後最初の null スロットを選ぶ部分挿入を、count が減らなくなるまで繰り返す。新しい空スロットには count0 の新スタックと入力 NBT copy を作り、Item limit と64の両方まで加算し animation=5。残量が変わらず Creative なら残量を0にする。既存の非 null count0を null スロットとして扱わない。

animation 更新は main の live length を条件に index 昇順。毎回 non null stack を捕捉してから、その時点の Player.worldObj と Player/currentItem を読む。二スロットが同じ stack を指していれば二回更新する。armor はこのループに含まれない。

Inventory NBT は main を Slot byte0..35、armor を100..103に保存する。読込は新しい配列を作ってから入力 List 順に処理し、Slot byteを`&255`で解釈、有効なnon-null load結果の同じ Slot の重複は後勝ち、範囲外は配置しない。未知Item等でload結果nullの後続行は、先行して配置したstackを消さない。cursor はこの Inventory List に保存しない。player close/save 処理と独自 crash journal を混同しない。

### 3.2 レイアウトと Slot の通知

| Container | result | crafting grid | armor | main9..35 | hotbar0..8 | 合計 |
|---|---|---|---|---|---|---:|
| Player | 0 | 1..4、2×2 row-major | 5..8、頭→足 | 9..35 | 36..44 | 45 |
| Workbench | 0 | 1..9、3×3 row-major | なし | 10..36 | 37..45 | 46 |

Armor Slot の inventory index は39,38,37,36、limit1。対応 ItemArmor の armorType に加え、頭 Slot だけ pumpkin/skull を許可する。cursor は Container の最後の slot ではない。

Slot は inventory/index/slotNumber/画面座標を持つ。getStack は inventory getter、putStack は direct setter の後 onSlotChanged、onSlotChanged は inventory.markDirty。decr は inventory へ委譲する。SlotCrafting は通常挿入不可。Result inventory の decr は要求個数に関係なく結果全体を返して null にする。

InventoryCrafting の set/decr は Container.onCraftMatrixChanged を通知する。removeStackFromSlot は参照を取り出して null にするだけで、この通知をしない。grid の各変更ごとに result が再計算される。結果を保存した別 authoritative owner にせず、同じ result inventory を更新する。

Container の slot list と比較用 stack list は順序付きで別物。addSlot は現在 list size を Slot.slotNumber にして同じ Slot を追加し、比較用 list には null。getInventory は新 list に各現在 stack の**同参照**を入れる。detectAndSendChanges は各 live slot を比較し、変化した stack を copy または null にして比較用 list に置き、その snapshot を listener に送る。listener の重複登録は例外、登録成功時には初期全スロット通知の後差分検出を行う。

transaction IDはContainerごとのsigned shortで、getNextTransactionIDは先に1増加しその値を返す。Player引数は読まない。canCraftは「操作禁止player集合に含まれない」という述語。false設定は同Playerを集合へadd、true設定はremoveである。window ID、transaction ID、packet受信順を一つの世代番号として扱わない。

### 3.3 slotClick 共通契約

入力は slotId、button、mode、Player。最初に Player.inventory を捕捉する。戻り値は初期 null。mode0の通常クリックや mode1で指定する「操作前の stack copy」が取引照合に使われ、更新後全スロットを戻す API ではない。

範囲外 index の list access、null receiver、原版が認めない enum などの例外はそれぞれ到達箇所で発生する。「不正なら全処理を先頭で無害化」しない。通信入口に追加防衛制約を置く場合、それは原版の内部メソッド仕様と別の制約として記述する。

進行中 dragEvent が0でない状態に mode5以外を与えると、まず drag を reset してその呼出しは終了する。reset は dragEvent=0 と集合 clear、dragMode は保持する。

### 3.4 mode0 通常クリック、mode1 Shift

button は0/1の枝だけを処理する。slot=-999 の外側クリックは、cursor があれば左で exact cursor を drop(randomChoice=true)して null、右で split(1) を drop し残 count==0 だけ null にする。他の負 slot は当該通常/Shift 枝で null を返す。

mode0で Slot が存在する場合、まず旧 target が非 null ならその copy を戻り値として保存する。

| target | cursor | 原版の変更 |
|---|---|---|
| null | 非 null、slot valid | 左は cursor count、右は1を要求し slot limit で制限。split した新参照を配置。cursor count==0なら null |
| 非 null、take可 | null | 左は全個数、右は `(count+1)/2`。Slot.decr の戻り値を cursorへ。捕捉した旧 target count==0なら put null。その後 pickup |
| 非 null、take可 | slot valid、同 Item/厳密 metadata/NBT | 左全 cursor、右1を、slot capacity、次に cursor max-target count で制限。cursor.split の戻り値は破棄し、target countへ加算 |
| 非 null、take可 | slot valid、異なる stack | cursor count<=slot limit の場合、exact cursor と exact旧 target を交換 |
| 非 null、take可 | slot invalid | 同 Item、cursor max>1、subtype 条件/NBT一致、target count>0、合計<=cursor maxなら target全体をcursorへ回収し decr/pickup |

既存 Slot の枝の末尾で onSlotChanged を呼ぶ。容量判定で移動しなかった場合にも通知する経路がある。

mode1は take 可 Slot へ virtual transferStackInSlot を呼ぶ。非 null 戻り値を copy してクリック戻り値にする。転送後も元 Slot に**同じ Item identity**の stack が残れば retrySlotClick を再帰実行する。ここで metadata/NBTまで比較して再試行を止めない。

Player Shift は result→9..44逆順、grid/armor→9..44順、main→hotbar、hotbar→main。空の適正 armor Slot があればそちらを先に選ぶ。Workbench Shift は result→10..45逆順、main→hotbar、hotbar→main、grid→全 player slots。

基本 mergeItemStack は範囲の始点含む・終点含まない。stackableなら同 Item、元が subtype を持つ場合だけ metadata、一致NBTで既存スタックを先に埋める。この helper 自体は通常の Slot validity/limit を全面適用せず、Item max を使う。その後最初の null Slot に元の全量 copy を置き元 count=0として終了する。空への配置に原版にない max clamp を足さない。

### 3.5 mode2..6

**mode2（number key）**: button0..8。target.canTake を確認し、対応 hotbar stack を捕捉する。同 inventory で slot valid、または hotbar null なら交換可。それ以外は first empty main があれば処理可。非空 target は先に target.copy を hotbar に配置する。元 hotbar がその Slot に適合しない場合は addItemStackToInventory へ渡し、その戻り値を無視して target全体を取り去る。元 hotbar を直接 target へ置く別枝もある。空 target に valid な hotbar を移す枝は exact参照全体を移し、limit を clampしない。これにより特殊入力では過大 stack や旧挿入 helper の残量消失が起こり得る。

**mode3（middle clone）**: Creative、cursor null、slot>=0、target.hasStack の場合だけ target.copy を取り、その countを Item maxにしてcursorへ。

**mode4（drop）**: cursor null、slot>=0、target.hasStack/canTake。button0は1、その他は全量 decr。pickup hookを呼んでからその戻り stackを drop(randomChoice=true)する。

**mode5（drag）**: 旧eventを捕捉してからevent=`button & 3`をstoreする。旧event1→新event2、または旧新eventが同値の場合だけ先へ進み、その他はreset。cursor nullもreset。drag mode=`(button >> 2) & 3`。event0で mode0/1を許可、mode2は Creative のみ、集合を clearしてevent1。event1は valid/canDrag/canAdd 条件を満たし、cursor count>集合size の Slot を同参照で HashSet に加える。event2は集合を HashSet iterator 順で処理する。sorted slot順に置き換えない。

event2は cursor.copy を template、残量 jを元countで開始。各 Slot で再度条件を読み、template.copy に対し mode0なら floor(float count/集合size)、mode1なら1、mode2なら Item limitを分配量とする。既存 countを加え、Item max、次に Slot limitで clamp。jから純増分を減らし putStack。全処理後 template.count=j、j<=0ならcursor null、そうでなければtemplateをcursorにしreset。mode2のためだけに残量計算を別実装にしない。

**mode6（collect/double click）**: cursor非null、clicked Slotが null/空/take不可のとき開始。button0は昇順、それ以外は降順で二巡する。canAdd/canTake/canMerge を満たす同種 stack からcursorのmaxまで取る。第一巡は満杯 stackを飛ばし、第二巡はそれも対象。要求量はmin(cursor空容量,target count)。decr後にcursorへ加える値は**要求量**であり、特殊inventoryが返したcountではない。decr戻りstack.count<=0ならput null、その戻りstackでpickupを呼ぶ。末尾で detectAndSendChanges。戻り値はnull。

### 3.6 close と server transaction

基本 onContainerClosed は exact cursor を drop(randomChoice=false)してcursor null。Player Containerは続けて四つの格子を通知なし removeし一つずつdrop、result null。Workbenchは Worldがremoteでない場合だけ九格子を remove/dropし、明示的なresult clearはしない。Player Container interactionはtrue、Workbenchは位置の Block が実 crafting_table、続いて中心から距離平方<=64でなければfalse。

サーバー click の順序は thread 確認/必要なら enqueue→markActive→window ID/current Container/getCanCraft。Spectator は操作せず現在 snapshot を送る。通常は **slotClickを実行した後** claimed clickedItem と戻り値を比較する。

- 一致: S32 accepted=true、isChangingQuantityOnly=true、Container差分通知、held item更新、flag=false。acceptedを契機に原版にないS30/cursor全再同期を無条件送信しない。
- 不一致: windowごとのpending actionを保存、S32false、getCanCraftをfalseへ、**操作後状態**の全 inventory snapshot/cursorを通知する。実行済みクリックを巻き戻さない。
- C0F: pending shortとuid一致、現在window一致、canCraft=false、非Spectatorなら再許可する。C0Fのaccepted boolを独自 rollbackスイッチにしない。
- client S32拒否: 対応 Container があるときC0F(window,action,true)を返信するだけで、Source handlerに予測 rollbackはない。window0 snapshotはplayer inventory Container、その他は一致するopen Container。S30自体にcursorはない。
- server S2Eはpayload window IDに依存せず closeScreenAndDropStack。通常ユーザーcloseのC0D送信と区別し、S2EへのC0D返信を生成しない。

## 4. クラフト

### 4.1 manager、順序、通常レシピ

標準 registry は **静的365件と動的8件の373件**。365だけを全 registry の件数としない。登録後 sort は Shaped と Shapeless の組合せなら Shapedを先、その他は recipe size降順、比較同順位では元の登録順を保持する。レシピ全体の順序は後の数値表と特殊位置表を合わせて再現する。

findMatchingRecipe は list iterator順の最初の matches=trueに対して getCraftingResultを返す。resultがnullでも次を探さない。remainingも別に同じmatch探索を行う。未一致の場合は新しい配列に格子の**同じ入力参照**を返す。recipe list、Shapedの素材配列、Shapelessの素材list、output template はlive参照であり、getRecipeList/getRecipeOutputで隠れたcopyを作らない。

Shapedは offset X→Y、各位置で鏡像true→falseの順に試す。照合は格子内外の3×3全部を X→Yの順で読み、形の外はnullを要求する。Item identity、metadata（素材32767はwildcard）だけで照合し、素材countや通常素材NBTは照合しない。2×2の範囲外getterはnull。結果はoutput.copy。copyIngredientNBTはconstructor後falseであり、標準365件では変更されない。もしtrueなら格子index順でNBT持ち入力をcopyして結果tagへ順次上書きし、最後の対象が残る。

Shapelessは必要素材listのworking copyを作り、格子index順の各non-null stackについて最初に Item/meta条件一致する素材を一つ除去する。同素材の選択順を並べ替えない。countを素材数に使わない。未知素材があればfalse、最後に必要listが空ならtrue。通常 remainingは各入力Itemのcontainer Itemから新スタックを作る。

### 4.2 結果の取得と alias

SlotCrafting.decrでamountCraftedへ `min(要求個数,現在result count)`を先に加算する。result inventoryが全量を返す規則とは別である。pickupは最初にonCrafting(output)、次にcurrent gridのremaining探索、格子index順に一個消費してremainingを置く。

amountCrafted>0ならoutput.onCrafting（stat→Item.onCreated）、その後amount=0、次にworkbench、木/その他pickaxe、furnace、hoe、bread、cake、sword、enchanting table、bookshelf、metadata1 golden appleなどのAchievement条件を順番に評価する。レシピpreview時にこのhookを実行しない。

各格子は最初に旧input/remainingを捕捉し、input non-nullならdecr1。remaining非nullなら、消費後格子がnullならsame remainingをset、そうでなければInventoryPlayerへ挿入、falseならsame remainingをdrop(false)。remainingを常にcopyしてaliasを消さない。特に written bookのremainingは元入力と同参照で、格子に残る入力・挿入処理・落下entityが同じスタックを指し得る。

Shiftやnumberではdestination copyがonCreatedより前に置かれる。地図拡大で作成IDが増えても、既にコピーしたdestinationは旧IDのままという枝を「直感的な新ID」に修正しない。

### 4.3 八つの動的レシピ

| 全registry index（0始まり） | レシピ | 照合・結果・remaining |
|---:|---|---|
| 0 | RecipesArmorDyes | leather armorちょうど一個とdye一個以上。結果はarmor.copy/count1。既存の有効colorも色平均へ含める |
| 1 | RecipeFireworks | 下の三種類。matchesがcached結果fieldをnullへ戻してから組み立て、getCraftingResultはそのcopy |
| 2 | Banner pattern追加 | banner一つ、既存patterns<6、最初に成立したEnumBannerPattern。結果copy/count1にPattern/Colorをappend |
| 70 | RecipeBookCloning | matchesはwritten book一つとwritable book占有cell一つ以上だけを要求。resultはgeneration<2ならblank cell数/generation+1、>=2ならnull。tag欠落はgeneration getterで例外。remainingは最初のItemEditableBookをsame refで返して探索終了 |
| 71 | RecipesMapCloning | filled map一つ、empty map占有cell一つ以上。他Item不可。新filled mapの個数はblank cell数+1、旧metadata。display nameだけ移す、NBT全体コピーではない |
| 72 | RecipesMapExtending | 3×3紙の外周/filled map中央、取得MapDataがnon-nullでscale<4。旧stack.copy/count1、map_is_scaling byte1 |
| 216 | RecipeRepairItem | 同Itemの二stack、両count==1、Item最大stack1かつdamageable。残durability合計+floor(maxDamage×5/100)、damage下限0。NBTを引き継がない新stack |
| 280 | Banner複製 | 同base colorのbanner二つ、patternsあり一つ/なし一つ。pattern側copy/count1を結果とremainingへそれぞれ作る |

MapExtendingのgetRecipeOutputはempty map/count0の継承templateであり、実getCraftingResultとは別物。FireworksのgetRecipeOutputはその時点のcached結果same参照、それ以外の上記動的recipeはnull。StatList初期化が読むtemplateを実際の動的結果で置き換えない。matchesとgetCraftingResultも独立bodyであり、callerが前者を省略した場合に後者が全入力検査を繰り返すとは仮定しない。

Armor染色は各dyeの羊用float RGBを255倍してintにし、各channel合計と各色max channel合計を計算する。既存armor色を含む枝はchannelをfloat/255へ戻してから加算するため、整数だけの平均に変えない。各channelを色数で整数除算し、平均brightnessと平均channel最大値でfloat正規化してintへ変換する。0除算・NaN変換は共通Java規則。ItemArmor.hasColorはdisplay.colorの**Int型**を要求し、wrong-type display getterが返すunattached empty compoundを自動接続しない。

Fireworks分類は素材を格子順に数える。gunpowder1..3、paper1、dye/形状修飾なしならrocket。firework starを含む場合だけExplosions listとFlight byteを構成し、cached結果へ各star.Explosionを直接共有する。starなしrocketにはFireworks/Flight tagを作らない。star生成はgunpowder1、paper0、star0、dye>=1、shape素材合計<=1。Typeは通常0、fire charge1、gold nugget2、skull3、feather4、diamondはTrail、glowstone dustはFlicker。Colorsはdye占有cell順。fadeはgunpowder0/paper0/star1/dye>=1、形状等なしでstar.copy/count1にFadeColors。wrong-type Explosionのunattached fallbackに書いたfadeを「修復」で接続しない。

Banner special patternは素材一つとdye最大一つ。dyeがなくても特殊素材で成立する場合はColor初期値0。通常mask patternは3×3、`#`の場所が同metadataのdye、空白の場所がnullまたはbanner。pattern enum順が優先順位。Patternsがwrong list subtypeのときのunattached list fallbackを保存する。patternデータ表は後掲。

### 4.4 furnace

furnaceはinput0/fuel1/output2、burnTime/currentFuel/cookTime/totalCookTimeを別に持つ。通常cook time200。tickでは旧burningを捕捉→burnTime>0なら1減算（remoteでも実行）→server枝。燃焼中、またはfuel/inputありの場合に処理する。未燃焼でcanSmeltなら燃料時間を両fuel fieldへ設定し、燃焼開始成功でfuelを1減らし、0ならcontainer Itemへ。燃焼中かつcanSmeltならcook++、totalと等しくなった時だけcook=0→total再計算→smelt→dirty。燃えない/精錬不能なら該当枝でcook=0、別の冷却枝はcookを2減らし0..totalへclamp。burning状態が変わればBlockFurnace.setState、最後に必要ならmarkDirty。

燃料判定順はItemBlock/air除外→wooden slab150→Material.wood300→coal block16000→WOOD tool/sword/hoe200→stick100→coal1600→lava bucket20000→sapling100→blaze rod2400→0。outputへの自動挿入不可、上面input、横fuel、下面output/fuel、下面fuel抜取りはwater bucket/bucketだけ。canSmeltはsameItem、個数/Item上限/Inventory上限を確認する。XPは出力Slot取得時に扱い、smelt tickで即playerへ加算しない。

FurnaceRecipesは二つのlive mapを持つ。入力ItemStack→出力same参照、出力ItemStack→XP。入力照合はItem identityと**登録側metadata32767のwildcard**、count/NBTを比較しない。getSmeltingResultは出力をcopyしない。キーはstack identityであり同値stackに自動統合しない。標準26精錬表は後掲。

## 5. Entity と生存モード

### 5.1 初期化、識別、乱数

Entity IDはprocess static signed intのpostincrementで、0/負値もwire合法。UUIDは実オブジェクトを持つ。EntityごとのRandom、World.Random、Math.randomのprocess-global streamを分離する。Entity constructorのRandom→UUID(nextLong二回)→World/位置/dimension→DataWatcher基底項目→virtual entityInitの順序を保存する。constructorで使う乱数をsaveタグにしたり、dropごとにseedを作り直したりしない。Math.randomやno-arg seed factoryの外部process状態はgameplay snapshot abortで巻き戻さない。

DataWatcherは定義されたindex/typeの実値、変更フラグ、owner通知を持つ。base項目0/1/3/2/4とItemの10等を一つのwatcherへ登録する。metadata listの先頭をItemStack index10と決めつけない。silentはflag bitではなく原版のbyte getterで値がちょうど1の場合。

原版のWorld、Entity、Living、Player、SP/ACP/MP constructorの全fields・virtual early calls・static registryが必要である。本章はそれらの全initial field表を掲載していない。既存移植のconstructor実装があることを、Markdownのみのconstructor仕様完了としない。

### 5.2 Entity.moveEntity

noClipならbboxを指定deltaだけoffsetして位置をbboxへ合わせて終える。通常はweb flagを解消し、X/Z deltaを0.25、Yを0.05000000074505806倍しmotion三軸を0。旧位置/旧requested delta/bboxを捕捉する。onGroundかつsneaking Playerは、0.05ずつX、Z、両軸の順に縮めて一ブロック下の支持collisionを失わない量へ調整する。

expanded bboxに対するcollision listを取得し、**Y→X→Z**の順に各boxのoffset制限を適用、その軸ごとにbboxを移動する。stepHeight>0、旧groundまたは下向きY衝突、水平衝突なら二つのstep経路を比較する。水平展開bboxから先に高さを求める経路と元bboxから求める経路を作り、水平距離平方がstrict大なら前者、同値なら後者。下降collisionを適用し、元の非step距離平方>=step距離平方なら元経路に戻す。

位置をbboxに合わせ、requested/actual差からhorizontal/vertical collisionを設定、onGroundはvertical衝突かつ旧requestedY<0。足元はfloor(posY-0.20000000298023224D)。airなら下がfence/wall/gateのときそのBlockへ置換。updateFallState、X/Z衝突によるmotion0、Y衝突のBlock.onLanded、歩行距離/足音/Block衝突の順へ進む。doBlockCollisionsのThrowableはCrashReportで包む。

その後isWetを捕捉。bboxを各axis0.001D縮めた範囲がflammableならdealFireDamage(1)、捕捉wetがfalseならfire++しfire==0でsetFire(8)。flammableでなくfire<=0なら-fireResistance。捕捉wetかつfire>0ならfizz sound（pitch1.6F+(rand-rand)×0.4F）、続けてfire=-fireResistance。portal、fall/歩行音の各callback内部全bodyは未充足表にある。

### 5.3 Living の通常移動

水/溶岩以外、またはPlayer飛行による該当免除ではfriction=0.91F、onGroundなら足元slipperiness×0.91F。acceleration係数は `0.16277136F/(friction³)`、groundはAI move speedを掛け、空中はjumpMovementFactor。moveFlying後、足元とgroundを再読込する。ladderはmotionX/Zを±0.15F相当へ制限、fallDistance=0、motionY下限-0.15D、sneaking Playerの負Yを0。moveEntity後、水平衝突かつladderでmotionY=0.2D。

通常gravityはmotionYから0.08D、次にYを0.9800000190734863倍、X/Zをfloat frictionのdouble昇格値で減衰。remoteの欠落chunk枝はposY>0ならY=-0.1、そうでなければ0で代用する。水はdrag0.8F/acceleration0.02F、Depth Striderを3以下にし非groundで半分、dragを0.54600006Fへ、accelerationをmove speedへ混合。Y減衰0.800000011920929の後-0.02D。溶岩は三軸0.5の後Y-0.02D。液体内の壁越え判定でY=0.30000001192092896Dを使う。これらをNative固定32座標補間と混同しない。

入力sneak/sprint/jump、飛行・ride、knockback、jump potion、全Living onUpdate/onLivingUpdateの連結は未充足である。上記式だけで完全なPlayer physicsとは判定しない。

### 5.4 EntityItem、drop、pickup

Item Entityはactual ItemStack参照をwatcherに保持する。tick冒頭stack nullならdead。それ以外はEntity super tick、pickupDelay>0かつ32767以外を減算、prev位置保存、Y gravity=-0.03999999910593033D、pushOutOfBlocks/noClip、moveの順。integer cast位置が変化またはticksExisted%25==0でlava/mergeチェック。lavaはY=0.20000000298023224D、X/Zを各rand差×0.2F、音pitchに別randを使う。serverは近傍merge。

通常friction0.98F、groundでslipperiness×0.98F、XZ減衰、Y×0.9800000190734863D、groundY×-0.5D。ageは-32768以外で増加、water処理、serverでage>=6000ならdead。pickupDelay32767は無限待機。近傍mergeはbbox水平0.5拡大、同Item/meta条件/NBT、最大stack、無限age/delay等を確認し、原版の大小によるreceiver再帰を保存する。統合側へ合計count、delay最大、age最小、watcher更新、元entitydead。

`dropPlayerItemWithRandomChoice(stack,boolean)`のbooleanは**unused**であり、必ずdropItem(stack,false,false)へ委譲する。クリック中のtrue指定をscatter指定と解釈しない。dropItemはstack nullまたはcount==0ならnull。negative countは同じ早期return条件ではない。Yは先にposY-0.30000001192092896D、その後float eyeHeightのdouble昇格値を加える。Item Entity constructor→delay40→traceItem時だけthrower名→scatter/direct速度→virtual joinEntityItemWithWorld→traceItem時だけdropStatの順。ordinary spawn拒否を例外に変更せず、drop戻りEntityは保持する。eyeHeightは1.62F、sleepingなら0.2F、そうでなくsneakingなら0.08Fを引く。virtual sleeping→必要時virtual sneakingの順を保つ。

scatterはPlayer.randのnextFloat二回で速度半径r×0.5F、角度r×floatPI×2.0Fを作り、X=-MathHelper.sin(angle)×半径、Z=cos×半径をfloatで計算してdoubleへ、Y=0.20000000298023224D。directはyaw/pitchを `/180.0F×floatPI`で変換し、floatでX=-sin(yaw)×cos(pitch)×0.3F、Z=cos(yaw)×cos(pitch)×0.3F、Y=-sin(pitch)×0.3F+0.1Fを作る。次にPlayer.rand四回を、jitter角度、0.02F×振幅、Y差の二回の順に使う。X/Z jitterだけMath.cos/sin(double角度)とdouble振幅、Yは(rand-rand)×0.1Fのfloat結果。MathHelper表sin/cosとMathのdouble関数を統一しない。

pickupはserver側でstackと挿入前countを捕捉。delay==0、ownerがnullまたは`6000-age<=200`またはownerがPlayer名と同値、InventoryPlayer挿入trueの短絡順を通る。挿入は入力stack自体を変更する。その後Itemがlog/log2、leather、diamond、blaze rodの順にAchievementを確認し、diamondかつthrowerがあればWorldのthrower名検索、別PlayerならdiamondsToYou。silentでないときだけItem自身のrand二回を消費しpop sound（volume0.2F、pitch=((r-r)×0.7F+1.0F)×2.0F）。次にPlayer.onItemPickupへ挿入前countを渡し、入力stack.count<=0でdead。部分挿入でもcollectに渡す値は挿入前countである。共有bookが別playerへ入ったとき元格子のsame stackがcount0へ変わるケースを保存する。onItemPickup内部のstat/networkとmerge callback全prefixは未充足条項として残す。

### 5.5 FoodStats、damage、status、AI

FoodStats defaultsはfood20、saturation5.0F、exhaustion0、timer0、prevFood20。addStatsはfoodを20以下、saturation加算を `foodAmount×modifier×2.0F`として現在food以下へ。exhaustionは40.0F以下。tickでは先にPlayer.worldObj.getDifficulty、次にprevFood保存。その後exhaustion>4.0F（>=ではない）なら4減らし、sat>0ならsatを1減らし下限0、そうでなくPEACEFUL以外ならfoodを1減らす。

naturalRegenerationがtrue、food>=18、Player.shouldHealの短絡条件を満たすとtimer++、80でheal1.0F→exhaustion3.0F加算→timer0。そうでなくfood<=0ならtimer++、80でhealth>10、またはHARD、またはhealth>1かつNORMALならstarve damage1.0F、その後timer0。それ以外はtimer0。Healthは条件で複数回getter評価され、先読みで統合しない。

Living damageの核はarmor補正→potion/enchantment補正→absorption控除→health/CombatTrackerの順。armor値を用いる基本係数は `(25-armor)/25`。absorption前amountを捕捉し、`max(amount-absorption,0)`、使用absorptionを差引き、残damage非0ならhealth読込/set、trackDamage、続くabsorption更新という原版store順を維持する。hurtResistantTimeが上半分の期間はlastDamage以下を拒否し、それより大きい分だけ追加damage、通常枝はlastDamage/無敵時間/hurt時間を更新する。これだけでは全attack、armor破壊、shield相当の剣blocking、difficulty、critical、enchant、death/drop/XPを閉じていない。

PotionEffect.combineは高amplifierならamplifier/duration置換、同amplifierで長durationならduration置換、そうでなく非ambientがambientへ届けばambient更新。showParticlesは最後に常に相手値。duration>0のtickでPotion.isReady→必要ならperformEffect→duration減算、その後duration>0を返す。Potion種類ごとのisReady間隔・attribute modifier・瞬間効果・NBT flags/ID/speedは未充足である。

EntityAITasksは候補と実行中を二つの順序付きListで持つ。tickCount postincrement%tickRate（通常3）==0の回だけ候補開始を評価する。実行中ならcanUse/continueがfalseでreset/removalし、同じ候補について続くcanUse/shouldExecuteがtrueならその回に再start/addできる。実行継続中の候補は再startしない。その他の回は実行中continueだけ判定し、最後に実行中をupdateTask。mutexBitsのANDが0なら共存可。優先度数値が同等/劣る候補は実行中の競合taskで拒否し、より高優先でも実行中taskがinterrupt不可なら拒否する。登録Listのiterator順をpriority sortで置換しない。各mobのtask登録順・priority/mutex・pathfinding・sensorと攻撃/繁殖等は未充足表に列挙する。

## 6. redstone と TileEntity

### 6.1 確認した wire 更新

RedstoneWireはPOWER0..15のstateを持ち、entity collision bboxはnull、opaque/full cubeはfalse。支持はsolid top surfaceまたはglowstone。server neighbor changeで支持がなくなればdropしてair、支持があれば周辺電力を計算する。

電力更新は旧state/POWERを捕捉→比較位置のwire電力→canProvidePower=false→World indirect power取得→trueへ戻す。周囲水平各方向について高さ/normal cube条件で同高、上、下のwire電力を読む。隣wire最大と比較して減衰1、外部電力を再比較して新POWERを決める。Worldの現在stateが捕捉旧stateと**同参照**の場合にだけflags2でsetする。更新位置と六近傍を集合に加える。周辺更新は集合を新Listへsnapshot→集合clear→snapshot順notifyNeighbors。例外時にfinallyでcanProvidePowerを戻す改変を勝手に加えない。

強電力はcanProvidePower=falseなら0、そうでなければweakへの委譲。weakはPOWER0なら0、UPならPOWER、それ以外はwire接続集合による方向条件。repeater/comparator/torch/button/lever/pressure plate/piston/observer相当追加などの仕様をここから推測しない。1.8.9にないBlockを加えない。

### 6.2 TileEntity 共通と未閉包

TileEntityはWorld、BlockPos、invalid/dirty、Block metadata/type cacheを同じownerに持ち、ChunkのmapとWorldのloaded/tickable/added/removal listsに同参照で参加する。indexとlistを別コピーで作らない。Chunk unloadはtileをWorld removal queueへ渡し、Entity listsをWorld unloadへ渡す。callbackの途中例外があれば通知済みprefixが残る。NBTはid/x/y/zと派生内容、通信は個別updateパケット/BlockEvent/GUI fieldsを適用する。

完全仕様には furnace以外のChest/EnderChest/Hopper/Dispenser/Dropper/BrewingStand/EnchantingTable/Beacon/Sign/Skull/Banner/MobSpawner/CommandBlock/FlowerPot/Note/Jukebox/Pistonなどを、各tick・方向inventory・NBT・block update・interactionまで閉じる必要がある。本章は型名を認識しただけの実装を成功条件にしない。

Comparatorのinventory fullness helperは、各non-null stackについて `float count / min(inventory limit,Item limit)`を合計、inventory sizeで割り、floor(float値×14)に「non-null stackが一つでもあった」なら1を加える。count>0条件へ変えない。NaN/空inventory/異常countもJava数値規則へ従う。

## 7. maps、statistics、scoreboard、commands

### 7.1 地図のownerと作成

filled map ItemStackのdamageはMapStorage名 `map_`+signed IDを決める。getMapDataはfresh name→World.loadItemData(MapData class,name)→実MapData cast。cache hitはsame参照、部分読込後colors/nameがnullでもrepairしない。missがremoteならnull。server missはmap unique ID取得→same入力stack.damage更新→新MapData(name)→scale3→WorldInfo spawn X、別のWorldInfo spawn Z→center→provider dimension→markDirty→setItemData。これらをコピーのNative mapへ戻して二つのauthorityを持たせない。

empty map右使用はID取得→filled map新stack→新MapDataをsetItemData **してから** scale/center/dimension/dirty設定→入力countをJava int wrapで1減算。count<=0ならfilled mapを直接return。残りがあればfilled copyをinventoryへ挿入し、失敗時は元filled mapをdrop、useStatの順、入力same stackをreturn。Creative/remote用の早期枝を原版にないまま追加しない。

onCreatedはmap_is_scalingのNBT booleanがtrueなら旧MapData取得→新IDをsame stackに設定→新MapData割当て/name→旧scale+1をbyte化しupper4のみclamp→旧center/dimensionを元にcenter計算→dirty→setItemData。tagを置換せずscaling markerを保持する。Player引数は使われないメソッドであり、このためだけにPlayerを読む副作用を加えない。

centerの計算はscaleに対するsize=`128×2^scale`、各axisについて `floor((coord+64)/size)×size+size/2-64`。入力double/中間int/scale narrowingを実際の原版型で行う。負座標を単なる整数切捨てで計算しない。

### 7.2 更新・表示・保存

MapDataはdimension byte、centerX/Z int、scale byte、colors byte[16384]、decorationsの順序付きmap、player MapInfo list/identity mapを持つ。colors indexはx+z×128。プレイヤーとItemFrame、NBT Decorationsは別の更新経路。名前Stringの値等価とplayer参照identityを混同しない。nullable profile nameによるnull keyも到達し得る。

onUpdateはremoteで先にreturnする。Playerのmain36についてhotbar→残りの順でItem.updateAnimation/virtual Item.onUpdateを実行し、その後main+armor40のmap packet生成を行う。MapInfoのterrain survey counter、stripe更新継続、dirty rectangle、packet counterは別。初回は全128×128 dirty、変化範囲はmin/maxを拡大して次packetに送ってdirty解除、非dirtyのicons-onlyはcounter postincrementで5回ごとの枝。duplicate map stackの複数packet呼出しはcounterを複数回進める。

S34はid/scale/decorations/rectangleを持ち、wireにdimension/centerはない。constructorのCollection→typed Vec4b array、pixel rectangle copy、readのVec4b割当て/byte取得、setMapdataToのmap/icon clear→icon挿入→pixel書込を原版順で行う。zero width読込は旧height/offset/pixelsを一律clearしない。異常array/type/寸法のpartial prefixesは通信/実行環境章の詳細仕様が必要。

MapData NBT、WorldSavedData dirty、MapStorage cache/list/saveAllDataとdisk idcountsの流れを接続する。getMapPacketだけを実装して永続MapDataとは呼ばない。terrain surveyの全height/Material/MapColor頻度・水深/NoSky疑似色・dirty column継続、frame rotation/off-map icon、Decorations numeric conversion、map savedata異常幅/heightの全条項は未充足である。

### 7.3 statistics と scoreboard

StatBaseはUTF16 ID/実subclassに基づくequals/hash、objectiveCriteria参照を持つ。craft statsは実CraftingManagerとFurnaceRecipesの出力Item集合から作り、特殊recipeのnull output Itemに架空statを作らない。StatFileWriterはStatBaseをkeyとしてcounter/progress tupleを保持する。increaseはcurrent counter+signed amount、Achievementはparent gatingを通る。progress setter/getterはsame参照。全登録ID/parent41件/別名merge/formatters/JSON schemaは独立した必須データである。

EntityPlayerMP.addStatはnull statでno-op。counter更新→scoreboard criteriaに対応するobjective snapshot→各objectiveのplayer name score取得/増加→StatisticsFile送信条件の順。Achievement gatingでcounterが増えなくてもscoreboardへの到達を独自に省略しない。resetStatはcounter0とscore resetの対応経路。dirty statisticsはclock/tick差分、コピー/clear、packet送信の順を保つ。

Scoreboardのown stateはobjective名map、criteria identity→objective list、player名→objective/Score map、team名map、player名→team map、19 display slots。Objective/Score/Teamはsameboard参照を持つ。getObjectivesFromCriteriaは新list snapshotでsameobjectiveを返す。Score増加はcriteria readonlyなら例外、set/getValueのforceUpdateで最初の0でもhookが起きる。base hooksが空であることとServerScoreboardの保存/network hooksを区別する。全team flags/color/prefix/suffix、display slots、S3B..S3E等とScoreboardSaveDataは未充足。

### 7.4 command dispatchと必要コマンド

CommandHandlerは入力trim→先頭slash一つ除去→ASCII space文字でsplit→command名map lookup→username argument index判定。unknown/permission拒否はchat error。selector対象引数があればEntity順にUUID文字列へ置換して実行し、成功件数を数え最後に元引数へ戻す。単一実行はaffectedEntities=1。WrongUsage、CommandException、その他Throwableを別のerror表示へ捕捉し、最後SUCCESS_COUNTへ件数を設定する。selector中のmutation/exceptionのprefixとcommand aliasesの上書き規則は詳細閉包が必要。

標準登録対象は次のとおり。この列挙は各commandの引数/権限/効果の完成仕様ではない。

| 登録群 | コマンド |
|---|---|
| 共通 | time, gamemode, difficulty, defaultgamemode, kill, toggledownfall, weather, xp, tp, give, replaceitem, stats, effect, enchant, particle, me, seed, help, debug, tell, say, spawnpoint, setworldspawn, gamerule, clear, testfor, spreadplayers, playsound, scoreboard, execute, trigger, achievement, summon, setblock, fill, clone, testforblocks, blockdata, testforblock, tellraw, worldborder, title, entitydata |
| dedicated | op, deop, stop, save-all, save-off, save-on, ban-ip, pardon-ip, ban, banlist, pardon, kick, list, whitelist, setidletimeout |
| integratedのみ | publish |

各commandは権限level、aliases、tab completion、selector/相対座標/数値範囲、chat JSON/NBT parsing、sender位置/World、stats、実行結果packet/saveまで個別に定義する。特にexecuteの多段sender/World、fill/cloneの順とneighbor通知、scoreboardの操作/overflow/selectorを汎用「成功」で済ませない。

## 8. 保存とロードのゲーム状態順序

Entity外側read/writeは基底位置/motion/rotation/UUID等を扱ってからvirtual派生bodyを呼ぶ。Player.readEntityFromNBTが呼ぶsuperはLivingのbodyであり、外側Entity UUID readを再実行しない。Playerbodyは次の順。

1. Living super読込。
2. entityUniqueIDをprofile UUIDへreset。
3. Inventory List読込、SelectedItemSlot、Sleeping、SleepTimer。
4. XpP、XpLevel、XpTotal、XpSeed。XpSeed==0ならPlayer.rand.nextIntを実行。
5. Score、sleepingなら現在位置BlockPosを作りwake(true,true,false)。
6. SpawnX/Y/Zがすべてnumeric tagならspawn位置、SpawnForced。
7. FoodStats、PlayerCapabilities、EnderItemsがList型ならEnderChest読込。

PlayerwriteはLiving super→Inventory→Selected→Sleeping/Timer→XP→Score→nonnull spawn→Food→Capabilities→EnderItems→nonnull current stack/ItemならSelectedItem compound。保存済みPlayer UUIDをprofile UUIDより優先しない。XpSeedのdrawをUUID segmentテストで代用しない。

EnderChestは27スロットのsame inventory、List入力前に既存27slotをclear、Slot byte範囲内を入力順に配置。PlayerCapabilities保存はfresh abilities compoundへ各boolean、fly/walk speed。読込はabilities Compound型のとき、booleanを読み、flySpeedがnumeric型なら両speedを読む。mayBuildがByte型のときだけallowEditを読む。速度の欠落で全defaultsをresetしない。

死亡/respawn、dimension移動、portal cooldown、keepInventory、EnderChest、XP保存/複製、bed spawn、gameProfileとUUID、achievement/scoreboardを含む完全なsave/load連結は未充足。移植先journalのdurable commitは別の運用契約であり、Minecraft内部メソッドの例外prefixを変更しない。

## 9. 受入条件と未充足条項

### 9.1 この章の具体化済み部分を検証する最小系列

同じItem/NBTでも別identity、同identityを二slot、count0/-1/127、未知Item、metadata負→constructor補正、NBT wrong type/欠落、現currentItem変更を用意する。全mode/button/layoutで、クリック戻り値、各owner参照、count、NBT、notification、drop/stat orderを比較する。Shift再帰、drag中の別mode、fullinventory number key、output右クリック、result→cursor merge、book remainder共有を含める。

取引ではacceptedにS30が必ずあると仮定せず、拒否後stateが実行済みであること、C0Fでunlockすること、S2Eは全IDでC0D不返信を確認する。craft全373順序とstatic365データ、offset/mirror、count0 input、matchestrue/resultnull、remaining再探索、NBT copy/alias、map onCreated前destinationcopyを比較する。

Physicsは各axis collision順、step tie、sneakedge、water/lava/ladder、friction float丸め、fall/noClip/webを対照する。Itemはdelay32767/age-32768、merge順、silent1対0/2/-1、ownerexpiry、sharedstackpickupとsave再読込、Random draw回数/streamを比較する。

失敗検証は各allocation/callback/IO位置へ障害を入れ、その時点のstate prefix、output未置換、current fieldの再読込、capturedreceiver、foreign edge検出を記録する。成功結果だけの一致を完全互換の証拠にしない。

### 9.2 完全仕様化に必須だが本章で未調査・未完了の条項

以下は「任意の将来改善」ではなく、Markdownだけから完全互換を実装するために追加が必要な仕様である。原版のファイル名を示しても、そのbody仕様が文章化されたことにはならない。

| 領域 | 残る原版クラス・必要な仕様 |
|---|---|
| Block全体 | 全registered Block subclassのconstructor全field、property state変換、placement/activation、collision、break/drop、scheduled/random tick、neighbor/light/sound/render。ID/name表だけでは欠落 |
| Item全体 | ItemBlock/ItemSlab/ItemDoor、tool/weapon/armor、food、bucket、bow/projectile、potion、book、spawn egg、ride/lead/fishing、use/release/finish、durability/Unbreaking、attributes、Adventure CanPlaceOn/CanDestroy |
| Container派生 | Chest/Furnace/Hopper/Dispenser/Merchant/Repair/Enchantment/Brewing/Horse/Beacon等のslot順、shift、distance、property listener、cost/outputhook、closeとNBT |
| Player全tick | EntityPlayer.onUpdate/onLivingUpdate、EntityPlayerSP.onLivingUpdate、MP更新、使用中item、sprint/jump/fly、equipment、XP、sleep、ride、respawn/dimension、collision/能力連結 |
| Entity基底全body | entity registry ID/name/class、全constructor fields、fire/water/portal/void、mount/passenger、fall、block collision、押し合い、AABB/query、外側NBT全field/異常値/例外、tracking/lifecycle |
| Living/戦闘 | attackEntityFrom/damageEntity全分岐、armor/enchant/potion、knockback/critical/sprint、revenge/target、death/drop/XP、attributes/modifier、CombatTracker、equipment/activeeffects save/network |
| mob全種 | EntityLiving/Creature/Ageable/Animal/Tameable、Zombie/Skeleton/Spider/Creeper/Enderman/Slime/Ghast/Blaze/Witch/Wither/Dragon/Guardian/Villager/Wolf/Ocelot/Horse/家畜等、全attributes・task登録・spawn条件・loot・specialdamage・NBT |
| AI/navigation | EntityAITasks以外の全EntityAI*、PathNavigate*、PathFinder/NodeProcessor、look/move/jump helpers、target range/LOS/team、priority/mutex、water/door/rail costs、Random draw/再探索順 |
| 非Living Entity | Arrow/Throwable/Fireball/FishingHook/Boat/Minecart/TNT/FallingBlock/XPOrb/Lightning/ItemFrame/Painting等のtick・collision・interaction・drop/save/network |
| potion/enchant | 全effect/enchant ID、levels、readiness、duration/ambient/particles、attribute/instant effect、brewing Ingredient recipe graph、EnchantmentHelper選択順/Random/repair cost |
| redstone | Torch/Repeater/Comparator/Lever/Button/PressurePlate/TripWire/DaylightDetector/Piston/rail/trapdoor/door、scheduled tick優先度/再入/neighbor順、quasi-connectivity、Tile state/移動 |
| TileEntity全種 | 本章列挙の全Tile subclassの完全constructor/tick/IInventory/sided/NBT/blockevent/packet、World/Chunk追加・除去の全cycle |
| map全更新 | terrain survey全loop、material/height/water/NoSky/MapColor、player/frame/NBT Decorations、MapInfo counters/dirty/拡大/欠落Data、NBT幅高さ/colors、全部の異常prefix |
| stats/scoreboard | 全StatList/AchievementList/criteria/formatters、StatisticsFile JSON progress/IO/dirty/network、TeamとServerScoreboard全hooks、保存/packet、criteria readonly全型 |
| commands | 表中全commandの完整引数文法/権限/aliases/effects、PlayerSelector全filter/sort/limit、CommandBase numeric/coord/chat、CommandResultStats、command block/RCON/chat feedback |
| 登録data | Block全property/合法state ID、Block Itemのvariant変換、Item全flags/limits/food/equipment、Entity/Potion/Enchantment/Biome等全numeric registryと初期化依存順 |

未充足領域を追加する際は、クラスのown fields→constructor/initializer→各body→到達依存→保存/通信→失敗prefixの順で閉包を作る。別の簡略ルールを足して外観が似た時点で済ませない。完全仕様の達成条件はこの表の全条項が本文または他の仕様章に具体化され、章間矛盾がなくなることである。

## 10. 数値登録付録

付録はresource assetではなく、登録identifier・ゲーム規則の数値事実を文章表にしたもの。元Java body、obfuscated mapping、class byte、JAR、私的oracle出力は含めない。Block/Itemの完全behaviorは2節と9.2節の追加条項が必要である。

### 10.1 Block ID/name

後掲表のBlock IDは0..197の198件。ItemBlock登録されないBlockも含む。

| ID | registry path | 同IDのBlock Item型 |
|---:|---|---|
| 0 | air | 登録なし |
| 1 | stone | ItemMultiTexture |
| 2 | grass | ItemColored |
| 3 | dirt | ItemMultiTexture |
| 4 | cobblestone | ItemBlock |
| 5 | planks | ItemMultiTexture |
| 6 | sapling | ItemMultiTexture |
| 7 | bedrock | ItemBlock |
| 8 | flowing_water | 登録なし |
| 9 | water | 登録なし |
| 10 | flowing_lava | 登録なし |
| 11 | lava | 登録なし |
| 12 | sand | ItemMultiTexture |
| 13 | gravel | ItemBlock |
| 14 | gold_ore | ItemBlock |
| 15 | iron_ore | ItemBlock |
| 16 | coal_ore | ItemBlock |
| 17 | log | ItemMultiTexture |
| 18 | leaves | ItemLeaves |
| 19 | sponge | ItemMultiTexture |
| 20 | glass | ItemBlock |
| 21 | lapis_ore | ItemBlock |
| 22 | lapis_block | ItemBlock |
| 23 | dispenser | ItemBlock |
| 24 | sandstone | ItemMultiTexture |
| 25 | noteblock | ItemBlock |
| 26 | bed | 登録なし |
| 27 | golden_rail | ItemBlock |
| 28 | detector_rail | ItemBlock |
| 29 | sticky_piston | ItemPiston |
| 30 | web | ItemBlock |
| 31 | tallgrass | ItemColored |
| 32 | deadbush | ItemBlock |
| 33 | piston | ItemPiston |
| 34 | piston_head | 登録なし |
| 35 | wool | ItemCloth |
| 36 | piston_extension | 登録なし |
| 37 | yellow_flower | ItemMultiTexture |
| 38 | red_flower | ItemMultiTexture |
| 39 | brown_mushroom | ItemBlock |
| 40 | red_mushroom | ItemBlock |
| 41 | gold_block | ItemBlock |
| 42 | iron_block | ItemBlock |
| 43 | double_stone_slab | 登録なし |
| 44 | stone_slab | ItemSlab |
| 45 | brick_block | ItemBlock |
| 46 | tnt | ItemBlock |
| 47 | bookshelf | ItemBlock |
| 48 | mossy_cobblestone | ItemBlock |
| 49 | obsidian | ItemBlock |
| 50 | torch | ItemBlock |
| 51 | fire | 登録なし |
| 52 | mob_spawner | ItemBlock |
| 53 | oak_stairs | ItemBlock |
| 54 | chest | ItemBlock |
| 55 | redstone_wire | 登録なし |
| 56 | diamond_ore | ItemBlock |
| 57 | diamond_block | ItemBlock |
| 58 | crafting_table | ItemBlock |
| 59 | wheat | 登録なし |
| 60 | farmland | ItemBlock |
| 61 | furnace | ItemBlock |
| 62 | lit_furnace | ItemBlock |
| 63 | standing_sign | 登録なし |
| 64 | wooden_door | 登録なし |
| 65 | ladder | ItemBlock |
| 66 | rail | ItemBlock |
| 67 | stone_stairs | ItemBlock |
| 68 | wall_sign | 登録なし |
| 69 | lever | ItemBlock |
| 70 | stone_pressure_plate | ItemBlock |
| 71 | iron_door | 登録なし |
| 72 | wooden_pressure_plate | ItemBlock |
| 73 | redstone_ore | ItemBlock |
| 74 | lit_redstone_ore | 登録なし |
| 75 | unlit_redstone_torch | 登録なし |
| 76 | redstone_torch | ItemBlock |
| 77 | stone_button | ItemBlock |
| 78 | snow_layer | ItemSnow |
| 79 | ice | ItemBlock |
| 80 | snow | ItemBlock |
| 81 | cactus | ItemBlock |
| 82 | clay | ItemBlock |
| 83 | reeds | 登録なし |
| 84 | jukebox | ItemBlock |
| 85 | fence | ItemBlock |
| 86 | pumpkin | ItemBlock |
| 87 | netherrack | ItemBlock |
| 88 | soul_sand | ItemBlock |
| 89 | glowstone | ItemBlock |
| 90 | portal | 登録なし |
| 91 | lit_pumpkin | ItemBlock |
| 92 | cake | 登録なし |
| 93 | unpowered_repeater | 登録なし |
| 94 | powered_repeater | 登録なし |
| 95 | stained_glass | ItemCloth |
| 96 | trapdoor | ItemBlock |
| 97 | monster_egg | ItemMultiTexture |
| 98 | stonebrick | ItemMultiTexture |
| 99 | brown_mushroom_block | ItemBlock |
| 100 | red_mushroom_block | ItemBlock |
| 101 | iron_bars | ItemBlock |
| 102 | glass_pane | ItemBlock |
| 103 | melon_block | ItemBlock |
| 104 | pumpkin_stem | 登録なし |
| 105 | melon_stem | 登録なし |
| 106 | vine | ItemColored |
| 107 | fence_gate | ItemBlock |
| 108 | brick_stairs | ItemBlock |
| 109 | stone_brick_stairs | ItemBlock |
| 110 | mycelium | ItemBlock |
| 111 | waterlily | ItemLilyPad |
| 112 | nether_brick | ItemBlock |
| 113 | nether_brick_fence | ItemBlock |
| 114 | nether_brick_stairs | ItemBlock |
| 115 | nether_wart | 登録なし |
| 116 | enchanting_table | ItemBlock |
| 117 | brewing_stand | 登録なし |
| 118 | cauldron | 登録なし |
| 119 | end_portal | 登録なし |
| 120 | end_portal_frame | ItemBlock |
| 121 | end_stone | ItemBlock |
| 122 | dragon_egg | ItemBlock |
| 123 | redstone_lamp | ItemBlock |
| 124 | lit_redstone_lamp | 登録なし |
| 125 | double_wooden_slab | 登録なし |
| 126 | wooden_slab | ItemSlab |
| 127 | cocoa | 登録なし |
| 128 | sandstone_stairs | ItemBlock |
| 129 | emerald_ore | ItemBlock |
| 130 | ender_chest | ItemBlock |
| 131 | tripwire_hook | ItemBlock |
| 132 | tripwire | 登録なし |
| 133 | emerald_block | ItemBlock |
| 134 | spruce_stairs | ItemBlock |
| 135 | birch_stairs | ItemBlock |
| 136 | jungle_stairs | ItemBlock |
| 137 | command_block | ItemBlock |
| 138 | beacon | ItemBlock |
| 139 | cobblestone_wall | ItemMultiTexture |
| 140 | flower_pot | 登録なし |
| 141 | carrots | 登録なし |
| 142 | potatoes | 登録なし |
| 143 | wooden_button | ItemBlock |
| 144 | skull | 登録なし |
| 145 | anvil | ItemAnvilBlock |
| 146 | trapped_chest | ItemBlock |
| 147 | light_weighted_pressure_plate | ItemBlock |
| 148 | heavy_weighted_pressure_plate | ItemBlock |
| 149 | unpowered_comparator | 登録なし |
| 150 | powered_comparator | 登録なし |
| 151 | daylight_detector | ItemBlock |
| 152 | redstone_block | ItemBlock |
| 153 | quartz_ore | ItemBlock |
| 154 | hopper | ItemBlock |
| 155 | quartz_block | ItemMultiTexture |
| 156 | quartz_stairs | ItemBlock |
| 157 | activator_rail | ItemBlock |
| 158 | dropper | ItemBlock |
| 159 | stained_hardened_clay | ItemCloth |
| 160 | stained_glass_pane | ItemCloth |
| 161 | leaves2 | ItemLeaves |
| 162 | log2 | ItemMultiTexture |
| 163 | acacia_stairs | ItemBlock |
| 164 | dark_oak_stairs | ItemBlock |
| 165 | slime | ItemBlock |
| 166 | barrier | ItemBlock |
| 167 | iron_trapdoor | ItemBlock |
| 168 | prismarine | ItemMultiTexture |
| 169 | sea_lantern | ItemBlock |
| 170 | hay_block | ItemBlock |
| 171 | carpet | ItemCloth |
| 172 | hardened_clay | ItemBlock |
| 173 | coal_block | ItemBlock |
| 174 | packed_ice | ItemBlock |
| 175 | double_plant | ItemDoublePlant |
| 176 | standing_banner | 登録なし |
| 177 | wall_banner | 登録なし |
| 178 | daylight_detector_inverted | 登録なし |
| 179 | red_sandstone | ItemMultiTexture |
| 180 | red_sandstone_stairs | ItemBlock |
| 181 | double_stone_slab2 | 登録なし |
| 182 | stone_slab2 | ItemSlab |
| 183 | spruce_fence_gate | ItemBlock |
| 184 | birch_fence_gate | ItemBlock |
| 185 | jungle_fence_gate | ItemBlock |
| 186 | dark_oak_fence_gate | ItemBlock |
| 187 | acacia_fence_gate | ItemBlock |
| 188 | spruce_fence | ItemBlock |
| 189 | birch_fence | ItemBlock |
| 190 | jungle_fence | ItemBlock |
| 191 | dark_oak_fence | ItemBlock |
| 192 | acacia_fence | ItemBlock |
| 193 | spruce_door | 登録なし |
| 194 | birch_door | 登録なし |
| 195 | jungle_door | 登録なし |
| 196 | acacia_door | 登録なし |
| 197 | dark_oak_door | 登録なし |

### 10.2 独立 Item ID/name と Block Item 対応

次の表は明示numeric登録された187 Item。10.1表のBlock Item登録150件と合わせて標準Item registryは337件になる。Block由来Itemは同Block ID/nameを用いるが、10.1表の登録なし48件を自動追加しない。実subclassを表に示す。各variantの名前/metadata変換、constructorの全flagsと仮想動作は未充足データである。426は欠番。

| ID | registry path | 型 |
|---:|---|---|
| 256 | iron_shovel | ItemSpade |
| 257 | iron_pickaxe | ItemPickaxe |
| 258 | iron_axe | ItemAxe |
| 259 | flint_and_steel | ItemFlintAndSteel |
| 260 | apple | ItemFood |
| 261 | bow | ItemBow |
| 262 | arrow | Item |
| 263 | coal | ItemCoal |
| 264 | diamond | Item |
| 265 | iron_ingot | Item |
| 266 | gold_ingot | Item |
| 267 | iron_sword | ItemSword |
| 268 | wooden_sword | ItemSword |
| 269 | wooden_shovel | ItemSpade |
| 270 | wooden_pickaxe | ItemPickaxe |
| 271 | wooden_axe | ItemAxe |
| 272 | stone_sword | ItemSword |
| 273 | stone_shovel | ItemSpade |
| 274 | stone_pickaxe | ItemPickaxe |
| 275 | stone_axe | ItemAxe |
| 276 | diamond_sword | ItemSword |
| 277 | diamond_shovel | ItemSpade |
| 278 | diamond_pickaxe | ItemPickaxe |
| 279 | diamond_axe | ItemAxe |
| 280 | stick | Item |
| 281 | bowl | Item |
| 282 | mushroom_stew | ItemSoup |
| 283 | golden_sword | ItemSword |
| 284 | golden_shovel | ItemSpade |
| 285 | golden_pickaxe | ItemPickaxe |
| 286 | golden_axe | ItemAxe |
| 287 | string | ItemReed |
| 288 | feather | Item |
| 289 | gunpowder | Item |
| 290 | wooden_hoe | ItemHoe |
| 291 | stone_hoe | ItemHoe |
| 292 | iron_hoe | ItemHoe |
| 293 | diamond_hoe | ItemHoe |
| 294 | golden_hoe | ItemHoe |
| 295 | wheat_seeds | ItemSeeds |
| 296 | wheat | Item |
| 297 | bread | ItemFood |
| 298 | leather_helmet | ItemArmor |
| 299 | leather_chestplate | ItemArmor |
| 300 | leather_leggings | ItemArmor |
| 301 | leather_boots | ItemArmor |
| 302 | chainmail_helmet | ItemArmor |
| 303 | chainmail_chestplate | ItemArmor |
| 304 | chainmail_leggings | ItemArmor |
| 305 | chainmail_boots | ItemArmor |
| 306 | iron_helmet | ItemArmor |
| 307 | iron_chestplate | ItemArmor |
| 308 | iron_leggings | ItemArmor |
| 309 | iron_boots | ItemArmor |
| 310 | diamond_helmet | ItemArmor |
| 311 | diamond_chestplate | ItemArmor |
| 312 | diamond_leggings | ItemArmor |
| 313 | diamond_boots | ItemArmor |
| 314 | golden_helmet | ItemArmor |
| 315 | golden_chestplate | ItemArmor |
| 316 | golden_leggings | ItemArmor |
| 317 | golden_boots | ItemArmor |
| 318 | flint | Item |
| 319 | porkchop | ItemFood |
| 320 | cooked_porkchop | ItemFood |
| 321 | painting | ItemHangingEntity |
| 322 | golden_apple | ItemAppleGold |
| 323 | sign | ItemSign |
| 324 | wooden_door | ItemDoor |
| 325 | bucket | ItemBucket |
| 326 | water_bucket | ItemBucket |
| 327 | lava_bucket | ItemBucket |
| 328 | minecart | ItemMinecart |
| 329 | saddle | ItemSaddle |
| 330 | iron_door | ItemDoor |
| 331 | redstone | ItemRedstone |
| 332 | snowball | ItemSnowball |
| 333 | boat | ItemBoat |
| 334 | leather | Item |
| 335 | milk_bucket | ItemBucketMilk |
| 336 | brick | Item |
| 337 | clay_ball | Item |
| 338 | reeds | ItemReed |
| 339 | paper | Item |
| 340 | book | ItemBook |
| 341 | slime_ball | Item |
| 342 | chest_minecart | ItemMinecart |
| 343 | furnace_minecart | ItemMinecart |
| 344 | egg | ItemEgg |
| 345 | compass | Item |
| 346 | fishing_rod | ItemFishingRod |
| 347 | clock | Item |
| 348 | glowstone_dust | Item |
| 349 | fish | ItemFishFood |
| 350 | cooked_fish | ItemFishFood |
| 351 | dye | ItemDye |
| 352 | bone | Item |
| 353 | sugar | Item |
| 354 | cake | ItemReed |
| 355 | bed | ItemBed |
| 356 | repeater | ItemReed |
| 357 | cookie | ItemFood |
| 358 | filled_map | ItemMap |
| 359 | shears | ItemShears |
| 360 | melon | ItemFood |
| 361 | pumpkin_seeds | ItemSeeds |
| 362 | melon_seeds | ItemSeeds |
| 363 | beef | ItemFood |
| 364 | cooked_beef | ItemFood |
| 365 | chicken | ItemFood |
| 366 | cooked_chicken | ItemFood |
| 367 | rotten_flesh | ItemFood |
| 368 | ender_pearl | ItemEnderPearl |
| 369 | blaze_rod | Item |
| 370 | ghast_tear | Item |
| 371 | gold_nugget | Item |
| 372 | nether_wart | ItemSeeds |
| 373 | potion | ItemPotion |
| 374 | glass_bottle | ItemGlassBottle |
| 375 | spider_eye | ItemFood |
| 376 | fermented_spider_eye | Item |
| 377 | blaze_powder | Item |
| 378 | magma_cream | Item |
| 379 | brewing_stand | ItemReed |
| 380 | cauldron | ItemReed |
| 381 | ender_eye | ItemEnderEye |
| 382 | speckled_melon | Item |
| 383 | spawn_egg | ItemMonsterPlacer |
| 384 | experience_bottle | ItemExpBottle |
| 385 | fire_charge | ItemFireball |
| 386 | writable_book | ItemWritableBook |
| 387 | written_book | ItemEditableBook |
| 388 | emerald | Item |
| 389 | item_frame | ItemHangingEntity |
| 390 | flower_pot | ItemReed |
| 391 | carrot | ItemSeedFood |
| 392 | potato | ItemSeedFood |
| 393 | baked_potato | ItemFood |
| 394 | poisonous_potato | ItemFood |
| 395 | map | ItemEmptyMap |
| 396 | golden_carrot | ItemFood |
| 397 | skull | ItemSkull |
| 398 | carrot_on_a_stick | ItemCarrotOnAStick |
| 399 | nether_star | ItemSimpleFoiled |
| 400 | pumpkin_pie | ItemFood |
| 401 | fireworks | ItemFirework |
| 402 | firework_charge | ItemFireworkCharge |
| 403 | enchanted_book | ItemEnchantedBook |
| 404 | comparator | ItemReed |
| 405 | netherbrick | Item |
| 406 | quartz | Item |
| 407 | tnt_minecart | ItemMinecart |
| 408 | hopper_minecart | ItemMinecart |
| 409 | prismarine_shard | Item |
| 410 | prismarine_crystals | Item |
| 411 | rabbit | ItemFood |
| 412 | cooked_rabbit | ItemFood |
| 413 | rabbit_stew | ItemSoup |
| 414 | rabbit_foot | Item |
| 415 | rabbit_hide | Item |
| 416 | armor_stand | ItemArmorStand |
| 417 | iron_horse_armor | Item |
| 418 | golden_horse_armor | Item |
| 419 | diamond_horse_armor | Item |
| 420 | lead | ItemLead |
| 421 | name_tag | ItemNameTag |
| 422 | command_block_minecart | ItemMinecart |
| 423 | mutton | ItemFood |
| 424 | cooked_mutton | ItemFood |
| 425 | banner | ItemBanner |
| 427 | spruce_door | ItemDoor |
| 428 | birch_door | ItemDoor |
| 429 | jungle_door | ItemDoor |
| 430 | acacia_door | ItemDoor |
| 431 | dark_oak_door | ItemDoor |
| 2256 | record_13 | ItemRecord |
| 2257 | record_cat | ItemRecord |
| 2258 | record_blocks | ItemRecord |
| 2259 | record_chirp | ItemRecord |
| 2260 | record_far | ItemRecord |
| 2261 | record_mall | ItemRecord |
| 2262 | record_mellohi | ItemRecord |
| 2263 | record_stal | ItemRecord |
| 2264 | record_strad | ItemRecord |
| 2265 | record_ward | ItemRecord |
| 2266 | record_11 | ItemRecord |
| 2267 | record_wait | ItemRecord |

### 10.3 静的クラフト365件

`ID:metadata`、`ID:*`はmetadata32767のwildcard、`.`は空cell。入力はcountを要求しない。形付きはrow-majorで行を`/`区切り、形なしは必要素材の順序付きlist。出力は`ID:metadata × count`。indexは特殊8件を挿入した373件全体の0始まりindex。数値表は、既存公開の365静的素材/結果事実を自然言語表に展開したものであり、そのCアルゴリズムを原版仕様の代替にはしない。

| 全体index | 形 | 入力 | 出力 |
|---:|---|---|---|
| 3 | 3×3 | 5:* 5:* 5:* / . 280:0 . / . 280:0 . | 270:0 × 1 |
| 4 | 3×3 | 4:* 4:* 4:* / . 280:0 . / . 280:0 . | 274:0 × 1 |
| 5 | 3×3 | 265:0 265:0 265:0 / . 280:0 . / . 280:0 . | 257:0 × 1 |
| 6 | 3×3 | 264:0 264:0 264:0 / . 280:0 . / . 280:0 . | 278:0 × 1 |
| 7 | 3×3 | 266:0 266:0 266:0 / . 280:0 . / . 280:0 . | 285:0 × 1 |
| 8 | 3×3 | . 280:0 287:0 / 280:0 . 287:0 / . 280:0 287:0 | 261:0 × 1 |
| 9 | 3×3 | 266:0 266:0 266:0 / 266:0 266:0 266:0 / 266:0 266:0 266:0 | 41:0 × 1 |
| 10 | 3×3 | 265:0 265:0 265:0 / 265:0 265:0 265:0 / 265:0 265:0 265:0 | 42:0 × 1 |
| 11 | 3×3 | 264:0 264:0 264:0 / 264:0 264:0 264:0 / 264:0 264:0 264:0 | 57:0 × 1 |
| 12 | 3×3 | 388:0 388:0 388:0 / 388:0 388:0 388:0 / 388:0 388:0 388:0 | 133:0 × 1 |
| 13 | 3×3 | 351:4 351:4 351:4 / 351:4 351:4 351:4 / 351:4 351:4 351:4 | 22:0 × 1 |
| 14 | 3×3 | 331:0 331:0 331:0 / 331:0 331:0 331:0 / 331:0 331:0 331:0 | 152:0 × 1 |
| 15 | 3×3 | 263:0 263:0 263:0 / 263:0 263:0 263:0 / 263:0 263:0 263:0 | 173:0 × 1 |
| 16 | 3×3 | 296:0 296:0 296:0 / 296:0 296:0 296:0 / 296:0 296:0 296:0 | 170:0 × 1 |
| 17 | 3×3 | 341:0 341:0 341:0 / 341:0 341:0 341:0 / 341:0 341:0 341:0 | 165:0 × 1 |
| 18 | 3×3 | 371:0 371:0 371:0 / 371:0 371:0 371:0 / 371:0 371:0 371:0 | 266:0 × 1 |
| 19 | 3×3 | . 412:0 . / 391:0 393:0 39:* / . 281:0 . | 413:0 × 1 |
| 20 | 3×3 | . 412:0 . / 391:0 393:0 40:* / . 281:0 . | 413:0 × 1 |
| 21 | 3×3 | 360:0 360:0 360:0 / 360:0 360:0 360:0 / 360:0 360:0 360:0 | 103:0 × 1 |
| 22 | 3×3 | 5:* 5:* 5:* / 5:* . 5:* / 5:* 5:* 5:* | 54:0 × 1 |
| 23 | 3×3 | 49:* 49:* 49:* / 49:* 381:0 49:* / 49:* 49:* 49:* | 130:0 × 1 |
| 24 | 3×3 | 4:* 4:* 4:* / 4:* . 4:* / 4:* 4:* 4:* | 61:0 × 1 |
| 25 | 3×3 | . 331:0 . / 331:0 89:* 331:0 / . 331:0 . | 123:0 × 1 |
| 26 | 3×3 | 20:* 20:* 20:* / 20:* 399:0 20:* / 49:* 49:* 49:* | 138:0 × 1 |
| 27 | 3×3 | 409:0 409:0 409:0 / 409:0 409:0 409:0 / 409:0 409:0 409:0 | 168:1 × 1 |
| 28 | 3×3 | 409:0 409:0 409:0 / 409:0 351:0 409:0 / 409:0 409:0 409:0 | 168:2 × 1 |
| 29 | 3×3 | 409:0 410:0 409:0 / 410:0 410:0 410:0 / 409:0 410:0 409:0 | 169:0 × 1 |
| 30 | 3×3 | 334:0 . 334:0 / 334:0 334:0 334:0 / 334:0 334:0 334:0 | 299:0 × 1 |
| 31 | 3×3 | 334:0 334:0 334:0 / 334:0 . 334:0 / 334:0 . 334:0 | 300:0 × 1 |
| 32 | 3×3 | 265:0 . 265:0 / 265:0 265:0 265:0 / 265:0 265:0 265:0 | 307:0 × 1 |
| 33 | 3×3 | 265:0 265:0 265:0 / 265:0 . 265:0 / 265:0 . 265:0 | 308:0 × 1 |
| 34 | 3×3 | 264:0 . 264:0 / 264:0 264:0 264:0 / 264:0 264:0 264:0 | 311:0 × 1 |
| 35 | 3×3 | 264:0 264:0 264:0 / 264:0 . 264:0 / 264:0 . 264:0 | 312:0 × 1 |
| 36 | 3×3 | 266:0 . 266:0 / 266:0 266:0 266:0 / 266:0 266:0 266:0 | 315:0 × 1 |
| 37 | 3×3 | 266:0 266:0 266:0 / 266:0 . 266:0 / 266:0 . 266:0 | 316:0 × 1 |
| 38 | 3×3 | 172:0 172:0 172:0 / 172:0 351:0 172:0 / 172:0 172:0 172:0 | 159:15 × 8 |
| 39 | 3×3 | 20:0 20:0 20:0 / 20:0 351:0 20:0 / 20:0 20:0 20:0 | 95:15 × 8 |
| 40 | 3×3 | 172:0 172:0 172:0 / 172:0 351:1 172:0 / 172:0 172:0 172:0 | 159:14 × 8 |
| 41 | 3×3 | 20:0 20:0 20:0 / 20:0 351:1 20:0 / 20:0 20:0 20:0 | 95:14 × 8 |
| 42 | 3×3 | 172:0 172:0 172:0 / 172:0 351:2 172:0 / 172:0 172:0 172:0 | 159:13 × 8 |
| 43 | 3×3 | 20:0 20:0 20:0 / 20:0 351:2 20:0 / 20:0 20:0 20:0 | 95:13 × 8 |
| 44 | 3×3 | 172:0 172:0 172:0 / 172:0 351:3 172:0 / 172:0 172:0 172:0 | 159:12 × 8 |
| 45 | 3×3 | 20:0 20:0 20:0 / 20:0 351:3 20:0 / 20:0 20:0 20:0 | 95:12 × 8 |
| 46 | 3×3 | 172:0 172:0 172:0 / 172:0 351:4 172:0 / 172:0 172:0 172:0 | 159:11 × 8 |
| 47 | 3×3 | 20:0 20:0 20:0 / 20:0 351:4 20:0 / 20:0 20:0 20:0 | 95:11 × 8 |
| 48 | 3×3 | 172:0 172:0 172:0 / 172:0 351:5 172:0 / 172:0 172:0 172:0 | 159:10 × 8 |
| 49 | 3×3 | 20:0 20:0 20:0 / 20:0 351:5 20:0 / 20:0 20:0 20:0 | 95:10 × 8 |
| 50 | 3×3 | 172:0 172:0 172:0 / 172:0 351:6 172:0 / 172:0 172:0 172:0 | 159:9 × 8 |
| 51 | 3×3 | 20:0 20:0 20:0 / 20:0 351:6 20:0 / 20:0 20:0 20:0 | 95:9 × 8 |
| 52 | 3×3 | 172:0 172:0 172:0 / 172:0 351:7 172:0 / 172:0 172:0 172:0 | 159:8 × 8 |
| 53 | 3×3 | 20:0 20:0 20:0 / 20:0 351:7 20:0 / 20:0 20:0 20:0 | 95:8 × 8 |
| 54 | 3×3 | 172:0 172:0 172:0 / 172:0 351:8 172:0 / 172:0 172:0 172:0 | 159:7 × 8 |
| 55 | 3×3 | 20:0 20:0 20:0 / 20:0 351:8 20:0 / 20:0 20:0 20:0 | 95:7 × 8 |
| 56 | 3×3 | 172:0 172:0 172:0 / 172:0 351:9 172:0 / 172:0 172:0 172:0 | 159:6 × 8 |
| 57 | 3×3 | 20:0 20:0 20:0 / 20:0 351:9 20:0 / 20:0 20:0 20:0 | 95:6 × 8 |
| 58 | 3×3 | 172:0 172:0 172:0 / 172:0 351:10 172:0 / 172:0 172:0 172:0 | 159:5 × 8 |
| 59 | 3×3 | 20:0 20:0 20:0 / 20:0 351:10 20:0 / 20:0 20:0 20:0 | 95:5 × 8 |
| 60 | 3×3 | 172:0 172:0 172:0 / 172:0 351:11 172:0 / 172:0 172:0 172:0 | 159:4 × 8 |
| 61 | 3×3 | 20:0 20:0 20:0 / 20:0 351:11 20:0 / 20:0 20:0 20:0 | 95:4 × 8 |
| 62 | 3×3 | 172:0 172:0 172:0 / 172:0 351:12 172:0 / 172:0 172:0 172:0 | 159:3 × 8 |
| 63 | 3×3 | 20:0 20:0 20:0 / 20:0 351:12 20:0 / 20:0 20:0 20:0 | 95:3 × 8 |
| 64 | 3×3 | 172:0 172:0 172:0 / 172:0 351:13 172:0 / 172:0 172:0 172:0 | 159:2 × 8 |
| 65 | 3×3 | 20:0 20:0 20:0 / 20:0 351:13 20:0 / 20:0 20:0 20:0 | 95:2 × 8 |
| 66 | 3×3 | 172:0 172:0 172:0 / 172:0 351:14 172:0 / 172:0 172:0 172:0 | 159:1 × 8 |
| 67 | 3×3 | 20:0 20:0 20:0 / 20:0 351:14 20:0 / 20:0 20:0 20:0 | 95:1 × 8 |
| 68 | 3×3 | 172:0 172:0 172:0 / 172:0 351:15 172:0 / 172:0 172:0 172:0 | 159:0 × 8 |
| 69 | 3×3 | 20:0 20:0 20:0 / 20:0 351:15 20:0 / 20:0 20:0 20:0 | 95:0 × 8 |
| 73 | 3×3 | 35:0 35:0 35:0 / 35:0 35:0 35:0 / . 280:0 . | 425:15 × 1 |
| 74 | 3×3 | 35:1 35:1 35:1 / 35:1 35:1 35:1 / . 280:0 . | 425:14 × 1 |
| 75 | 3×3 | 35:2 35:2 35:2 / 35:2 35:2 35:2 / . 280:0 . | 425:13 × 1 |
| 76 | 3×3 | 35:3 35:3 35:3 / 35:3 35:3 35:3 / . 280:0 . | 425:12 × 1 |
| 77 | 3×3 | 35:4 35:4 35:4 / 35:4 35:4 35:4 / . 280:0 . | 425:11 × 1 |
| 78 | 3×3 | 35:5 35:5 35:5 / 35:5 35:5 35:5 / . 280:0 . | 425:10 × 1 |
| 79 | 3×3 | 35:6 35:6 35:6 / 35:6 35:6 35:6 / . 280:0 . | 425:9 × 1 |
| 80 | 3×3 | 35:7 35:7 35:7 / 35:7 35:7 35:7 / . 280:0 . | 425:8 × 1 |
| 81 | 3×3 | 35:8 35:8 35:8 / 35:8 35:8 35:8 / . 280:0 . | 425:7 × 1 |
| 82 | 3×3 | 35:9 35:9 35:9 / 35:9 35:9 35:9 / . 280:0 . | 425:6 × 1 |
| 83 | 3×3 | 35:10 35:10 35:10 / 35:10 35:10 35:10 / . 280:0 . | 425:5 × 1 |
| 84 | 3×3 | 35:11 35:11 35:11 / 35:11 35:11 35:11 / . 280:0 . | 425:4 × 1 |
| 85 | 3×3 | 35:12 35:12 35:12 / 35:12 35:12 35:12 / . 280:0 . | 425:3 × 1 |
| 86 | 3×3 | 35:13 35:13 35:13 / 35:13 35:13 35:13 / . 280:0 . | 425:2 × 1 |
| 87 | 3×3 | 35:14 35:14 35:14 / 35:14 35:14 35:14 / . 280:0 . | 425:1 × 1 |
| 88 | 3×3 | 35:15 35:15 35:15 / 35:15 35:15 35:15 / . 280:0 . | 425:0 × 1 |
| 89 | 3×3 | 5:* 5:* 5:* / 5:* 264:0 5:* / 5:* 5:* 5:* | 84:0 × 1 |
| 90 | 3×3 | 287:0 287:0 . / 287:0 341:0 . / . . 287:0 | 420:0 × 2 |
| 91 | 3×3 | 5:* 5:* 5:* / 5:* 331:0 5:* / 5:* 5:* 5:* | 25:0 × 1 |
| 92 | 3×3 | 5:* 5:* 5:* / 340:0 340:0 340:0 / 5:* 5:* 5:* | 47:0 × 1 |
| 93 | 3×3 | 289:0 12:* 289:0 / 12:* 289:0 12:* / 289:0 12:* 289:0 | 46:0 × 1 |
| 94 | 3×3 | 280:0 . 280:0 / 280:0 280:0 280:0 / 280:0 . 280:0 | 65:0 × 3 |
| 95 | 3×3 | 5:* 5:* 5:* / 5:* 5:* 5:* / . 280:0 . | 323:0 × 3 |
| 96 | 3×3 | 335:0 335:0 335:0 / 353:0 344:0 353:0 / 296:0 296:0 296:0 | 354:0 × 1 |
| 97 | 3×3 | 265:0 . 265:0 / 265:0 280:0 265:0 / 265:0 . 265:0 | 66:0 × 16 |
| 98 | 3×3 | 266:0 . 266:0 / 266:0 280:0 266:0 / 266:0 331:0 266:0 | 27:0 × 6 |
| 99 | 3×3 | 265:0 280:0 265:0 / 265:0 76:* 265:0 / 265:0 280:0 265:0 | 157:0 × 6 |
| 100 | 3×3 | 265:0 . 265:0 / 265:0 70:* 265:0 / 265:0 331:0 265:0 | 28:0 × 6 |
| 101 | 3×3 | 265:0 . 265:0 / 265:0 . 265:0 / 265:0 265:0 265:0 | 380:0 × 1 |
| 102 | 3×3 | 5:0 . . / 5:0 5:0 . / 5:0 5:0 5:0 | 53:0 × 4 |
| 103 | 3×3 | 5:2 . . / 5:2 5:2 . / 5:2 5:2 5:2 | 135:0 × 4 |
| 104 | 3×3 | 5:1 . . / 5:1 5:1 . / 5:1 5:1 5:1 | 134:0 × 4 |
| 105 | 3×3 | 5:3 . . / 5:3 5:3 . / 5:3 5:3 5:3 | 136:0 × 4 |
| 106 | 3×3 | 5:4 . . / 5:4 5:4 . / 5:4 5:4 5:4 | 163:0 × 4 |
| 107 | 3×3 | 5:5 . . / 5:5 5:5 . / 5:5 5:5 5:5 | 164:0 × 4 |
| 108 | 3×3 | . . 280:0 / . 280:0 287:0 / 280:0 . 287:0 | 346:0 × 1 |
| 109 | 3×3 | 4:* . . / 4:* 4:* . / 4:* 4:* 4:* | 67:0 × 4 |
| 110 | 3×3 | 45:* . . / 45:* 45:* . / 45:* 45:* 45:* | 108:0 × 4 |
| 111 | 3×3 | 98:* . . / 98:* 98:* . / 98:* 98:* 98:* | 109:0 × 4 |
| 112 | 3×3 | 112:* . . / 112:* 112:* . / 112:* 112:* 112:* | 114:0 × 4 |
| 113 | 3×3 | 24:* . . / 24:* 24:* . / 24:* 24:* 24:* | 128:0 × 4 |
| 114 | 3×3 | 179:* . . / 179:* 179:* . / 179:* 179:* 179:* | 180:0 × 4 |
| 115 | 3×3 | 155:* . . / 155:* 155:* . / 155:* 155:* 155:* | 156:0 × 4 |
| 116 | 3×3 | 280:0 280:0 280:0 / 280:0 35:* 280:0 / 280:0 280:0 280:0 | 321:0 × 1 |
| 117 | 3×3 | 280:0 280:0 280:0 / 280:0 334:0 280:0 / 280:0 280:0 280:0 | 389:0 × 1 |
| 118 | 3×3 | 266:0 266:0 266:0 / 266:0 260:0 266:0 / 266:0 266:0 266:0 | 322:0 × 1 |
| 119 | 3×3 | 41:* 41:* 41:* / 41:* 260:0 41:* / 41:* 41:* 41:* | 322:1 × 1 |
| 120 | 3×3 | 371:0 371:0 371:0 / 371:0 391:0 371:0 / 371:0 371:0 371:0 | 396:0 × 1 |
| 121 | 3×3 | 371:0 371:0 371:0 / 371:0 360:0 371:0 / 371:0 371:0 371:0 | 382:0 × 1 |
| 122 | 3×3 | . 76:* . / 76:* 406:0 76:* / 1:0 1:0 1:0 | 404:0 × 1 |
| 123 | 3×3 | . 266:0 . / 266:0 331:0 266:0 / . 266:0 . | 347:0 × 1 |
| 124 | 3×3 | . 265:0 . / 265:0 331:0 265:0 / . 265:0 . | 345:0 × 1 |
| 125 | 3×3 | 339:0 339:0 339:0 / 339:0 345:0 339:0 / 339:0 339:0 339:0 | 395:0 × 1 |
| 126 | 3×3 | 4:* 4:* 4:* / 4:* 261:0 4:* / 4:* 331:0 4:* | 23:0 × 1 |
| 127 | 3×3 | 4:* 4:* 4:* / 4:* . 4:* / 4:* 331:0 4:* | 158:0 × 1 |
| 128 | 3×3 | 5:* 5:* 5:* / 4:* 265:0 4:* / 4:* 331:0 4:* | 33:0 × 1 |
| 129 | 3×3 | . 340:0 . / 264:0 49:* 264:0 / 49:* 49:* 49:* | 116:0 × 1 |
| 130 | 3×3 | 42:* 42:* 42:* / . 265:0 . / 265:0 265:0 265:0 | 145:0 × 1 |
| 131 | 3×3 | 20:* 20:* 20:* / 406:0 406:0 406:0 / 126:* 126:* 126:* | 151:0 × 1 |
| 132 | 3×3 | 265:0 . 265:0 / 265:0 54:* 265:0 / . 265:0 . | 154:0 × 1 |
| 133 | 3×3 | 280:0 280:0 280:0 / . 280:0 . / 280:0 44:0 280:0 | 416:0 × 1 |
| 134 | 2×3 | 5:* 5:* / 5:* 280:0 / . 280:0 | 271:0 × 1 |
| 135 | 2×3 | 5:* 5:* / . 280:0 / . 280:0 | 290:0 × 1 |
| 136 | 2×3 | 4:* 4:* / 4:* 280:0 / . 280:0 | 275:0 × 1 |
| 137 | 2×3 | 4:* 4:* / . 280:0 / . 280:0 | 291:0 × 1 |
| 138 | 2×3 | 265:0 265:0 / 265:0 280:0 / . 280:0 | 258:0 × 1 |
| 139 | 2×3 | 265:0 265:0 / . 280:0 / . 280:0 | 292:0 × 1 |
| 140 | 2×3 | 264:0 264:0 / 264:0 280:0 / . 280:0 | 279:0 × 1 |
| 141 | 2×3 | 264:0 264:0 / . 280:0 / . 280:0 | 293:0 × 1 |
| 142 | 2×3 | 266:0 266:0 / 266:0 280:0 / . 280:0 | 286:0 × 1 |
| 143 | 2×3 | 266:0 266:0 / . 280:0 / . 280:0 | 294:0 × 1 |
| 144 | 3×2 | 265:0 265:0 265:0 / 265:0 265:0 265:0 | 101:0 × 16 |
| 145 | 3×2 | 20:* 20:* 20:* / 20:* 20:* 20:* | 102:0 × 16 |
| 146 | 3×2 | 334:0 334:0 334:0 / 334:0 . 334:0 | 298:0 × 1 |
| 147 | 3×2 | 334:0 . 334:0 / 334:0 . 334:0 | 301:0 × 1 |
| 148 | 3×2 | 265:0 265:0 265:0 / 265:0 . 265:0 | 306:0 × 1 |
| 149 | 3×2 | 265:0 . 265:0 / 265:0 . 265:0 | 309:0 × 1 |
| 150 | 3×2 | 264:0 264:0 264:0 / 264:0 . 264:0 | 310:0 × 1 |
| 151 | 3×2 | 264:0 . 264:0 / 264:0 . 264:0 | 313:0 × 1 |
| 152 | 3×2 | 266:0 266:0 266:0 / 266:0 . 266:0 | 314:0 × 1 |
| 153 | 3×2 | 266:0 . 266:0 / 266:0 . 266:0 | 317:0 × 1 |
| 154 | 3×2 | 95:0 95:0 95:0 / 95:0 95:0 95:0 | 160:0 × 16 |
| 155 | 3×2 | 95:1 95:1 95:1 / 95:1 95:1 95:1 | 160:1 × 16 |
| 156 | 3×2 | 95:2 95:2 95:2 / 95:2 95:2 95:2 | 160:2 × 16 |
| 157 | 3×2 | 95:3 95:3 95:3 / 95:3 95:3 95:3 | 160:3 × 16 |
| 158 | 3×2 | 95:4 95:4 95:4 / 95:4 95:4 95:4 | 160:4 × 16 |
| 159 | 3×2 | 95:5 95:5 95:5 / 95:5 95:5 95:5 | 160:5 × 16 |
| 160 | 3×2 | 95:6 95:6 95:6 / 95:6 95:6 95:6 | 160:6 × 16 |
| 161 | 3×2 | 95:7 95:7 95:7 / 95:7 95:7 95:7 | 160:7 × 16 |
| 162 | 3×2 | 95:8 95:8 95:8 / 95:8 95:8 95:8 | 160:8 × 16 |
| 163 | 3×2 | 95:9 95:9 95:9 / 95:9 95:9 95:9 | 160:9 × 16 |
| 164 | 3×2 | 95:10 95:10 95:10 / 95:10 95:10 95:10 | 160:10 × 16 |
| 165 | 3×2 | 95:11 95:11 95:11 / 95:11 95:11 95:11 | 160:11 × 16 |
| 166 | 3×2 | 95:12 95:12 95:12 / 95:12 95:12 95:12 | 160:12 × 16 |
| 167 | 3×2 | 95:13 95:13 95:13 / 95:13 95:13 95:13 | 160:13 × 16 |
| 168 | 3×2 | 95:14 95:14 95:14 / 95:14 95:14 95:14 | 160:14 × 16 |
| 169 | 3×2 | 95:15 95:15 95:15 / 95:15 95:15 95:15 | 160:15 × 16 |
| 170 | 3×2 | 5:0 280:0 5:0 / 5:0 280:0 5:0 | 85:0 × 3 |
| 171 | 3×2 | 5:2 280:0 5:2 / 5:2 280:0 5:2 | 189:0 × 3 |
| 172 | 3×2 | 5:1 280:0 5:1 / 5:1 280:0 5:1 | 188:0 × 3 |
| 173 | 3×2 | 5:3 280:0 5:3 / 5:3 280:0 5:3 | 190:0 × 3 |
| 174 | 3×2 | 5:4 280:0 5:4 / 5:4 280:0 5:4 | 192:0 × 3 |
| 175 | 3×2 | 5:5 280:0 5:5 / 5:5 280:0 5:5 | 191:0 × 3 |
| 176 | 3×2 | 4:* 4:* 4:* / 4:* 4:* 4:* | 139:0 × 6 |
| 177 | 3×2 | 48:* 48:* 48:* / 48:* 48:* 48:* | 139:1 × 6 |
| 178 | 3×2 | 112:* 112:* 112:* / 112:* 112:* 112:* | 113:0 × 6 |
| 179 | 3×2 | 280:0 5:0 280:0 / 280:0 5:0 280:0 | 107:0 × 1 |
| 180 | 3×2 | 280:0 5:2 280:0 / 280:0 5:2 280:0 | 184:0 × 1 |
| 181 | 3×2 | 280:0 5:1 280:0 / 280:0 5:1 280:0 | 183:0 × 1 |
| 182 | 3×2 | 280:0 5:3 280:0 / 280:0 5:3 280:0 | 185:0 × 1 |
| 183 | 3×2 | 280:0 5:4 280:0 / 280:0 5:4 280:0 | 187:0 × 1 |
| 184 | 3×2 | 280:0 5:5 280:0 / 280:0 5:5 280:0 | 186:0 × 1 |
| 185 | 2×3 | 5:0 5:0 / 5:0 5:0 / 5:0 5:0 | 324:0 × 3 |
| 186 | 2×3 | 5:1 5:1 / 5:1 5:1 / 5:1 5:1 | 427:0 × 3 |
| 187 | 2×3 | 5:2 5:2 / 5:2 5:2 / 5:2 5:2 | 428:0 × 3 |
| 188 | 2×3 | 5:3 5:3 / 5:3 5:3 / 5:3 5:3 | 429:0 × 3 |
| 189 | 2×3 | 5:4 5:4 / 5:4 5:4 / 5:4 5:4 | 430:0 × 3 |
| 190 | 2×3 | 5:5 5:5 / 5:5 5:5 / 5:5 5:5 | 431:0 × 3 |
| 191 | 3×2 | 5:* 5:* 5:* / 5:* 5:* 5:* | 96:0 × 2 |
| 192 | 2×3 | 265:0 265:0 / 265:0 265:0 / 265:0 265:0 | 330:0 × 3 |
| 193 | 3×2 | 5:* . 5:* / . 5:* . | 281:0 × 4 |
| 194 | 3×2 | 20:* . 20:* / . 20:* . | 374:0 × 3 |
| 195 | 3×2 | 265:0 . 265:0 / 265:0 265:0 265:0 | 328:0 × 1 |
| 196 | 3×2 | . 369:0 . / 4:* 4:* 4:* | 379:0 × 1 |
| 197 | 3×2 | 5:* . 5:* / 5:* 5:* 5:* | 333:0 × 1 |
| 198 | 3×2 | 265:0 . 265:0 / . 265:0 . | 325:0 × 1 |
| 199 | 3×2 | 336:0 . 336:0 / . 336:0 . | 390:0 × 1 |
| 200 | 3×2 | 76:* 331:0 76:* / 1:0 1:0 1:0 | 356:0 × 1 |
| 201 | 3×2 | 35:* 35:* 35:* / 5:* 5:* 5:* | 355:0 × 1 |
| 202 | 2×2 | . 265:0 / 265:0 . | 359:0 × 1 |
| 203 | 2×2 | 5:* 5:* / 5:* 5:* | 58:0 × 1 |
| 204 | 2×2 | 12:0 12:0 / 12:0 12:0 | 24:0 × 1 |
| 205 | 2×2 | 12:1 12:1 / 12:1 12:1 | 179:0 × 1 |
| 206 | 2×2 | 24:0 24:0 / 24:0 24:0 | 24:2 × 4 |
| 207 | 2×2 | 179:0 179:0 / 179:0 179:0 | 179:2 × 4 |
| 208 | 2×2 | 1:0 1:0 / 1:0 1:0 | 98:0 × 4 |
| 209 | 2×2 | 405:0 405:0 / 405:0 405:0 | 112:0 × 1 |
| 210 | 2×2 | 4:* 406:0 / 406:0 4:* | 1:3 × 2 |
| 211 | 2×2 | 3:0 13:* / 13:* 3:0 | 3:1 × 4 |
| 212 | 2×2 | 1:3 1:3 / 1:3 1:3 | 1:4 × 4 |
| 213 | 2×2 | 1:1 1:1 / 1:1 1:1 | 1:2 × 4 |
| 214 | 2×2 | 1:5 1:5 / 1:5 1:5 | 1:6 × 4 |
| 215 | 2×2 | 409:0 409:0 / 409:0 409:0 | 168:0 × 1 |
| 217 | 2×2 | 332:0 332:0 / 332:0 332:0 | 80:0 × 1 |
| 218 | 2×2 | 337:0 337:0 / 337:0 337:0 | 82:0 × 1 |
| 219 | 2×2 | 336:0 336:0 / 336:0 336:0 | 45:0 × 1 |
| 220 | 2×2 | 348:0 348:0 / 348:0 348:0 | 89:0 × 1 |
| 221 | 2×2 | 406:0 406:0 / 406:0 406:0 | 155:0 × 1 |
| 222 | 2×2 | 287:0 287:0 / 287:0 287:0 | 35:0 × 1 |
| 223 | 2×2 | 265:0 265:0 / 265:0 265:0 | 167:0 × 1 |
| 224 | 2×2 | 346:0 . / . 391:0 | 398:0 × 1 |
| 225 | 2×2 | 415:0 415:0 / 415:0 415:0 | 334:0 × 1 |
| 226 | 1×3 | 5:* / 280:0 / 280:0 | 269:0 × 1 |
| 227 | 1×3 | 4:* / 280:0 / 280:0 | 273:0 × 1 |
| 228 | 1×3 | 265:0 / 280:0 / 280:0 | 256:0 × 1 |
| 229 | 1×3 | 264:0 / 280:0 / 280:0 | 277:0 × 1 |
| 230 | 1×3 | 266:0 / 280:0 / 280:0 | 284:0 × 1 |
| 231 | 1×3 | 5:* / 5:* / 280:0 | 268:0 × 1 |
| 232 | 1×3 | 4:* / 4:* / 280:0 | 272:0 × 1 |
| 233 | 1×3 | 265:0 / 265:0 / 280:0 | 267:0 × 1 |
| 234 | 1×3 | 264:0 / 264:0 / 280:0 | 276:0 × 1 |
| 235 | 1×3 | 266:0 / 266:0 / 280:0 | 283:0 × 1 |
| 236 | 1×3 | 318:0 / 280:0 / 288:0 | 262:0 × 4 |
| 237 | 3×1 | 296:0 351:3 296:0 | 357:0 × 8 |
| 238 | 3×1 | 338:0 338:0 338:0 | 339:0 × 3 |
| 239 | 3×1 | 80:* 80:* 80:* | 78:0 × 6 |
| 240 | 3×1 | 4:* 4:* 4:* | 44:3 × 6 |
| 241 | 3×1 | 1:0 1:0 1:0 | 44:0 × 6 |
| 242 | 3×1 | 24:* 24:* 24:* | 44:1 × 6 |
| 243 | 3×1 | 45:* 45:* 45:* | 44:4 × 6 |
| 244 | 3×1 | 98:* 98:* 98:* | 44:5 × 6 |
| 245 | 3×1 | 112:* 112:* 112:* | 44:6 × 6 |
| 246 | 3×1 | 155:* 155:* 155:* | 44:7 × 6 |
| 247 | 3×1 | 179:* 179:* 179:* | 182:0 × 6 |
| 248 | 3×1 | 5:0 5:0 5:0 | 126:0 × 6 |
| 249 | 3×1 | 5:2 5:2 5:2 | 126:2 × 6 |
| 250 | 3×1 | 5:1 5:1 5:1 | 126:1 × 6 |
| 251 | 3×1 | 5:3 5:3 5:3 | 126:3 × 6 |
| 252 | 3×1 | 5:4 5:4 5:4 | 126:4 × 6 |
| 253 | 3×1 | 5:5 5:5 5:5 | 126:5 × 6 |
| 254 | 3×1 | 296:0 296:0 296:0 | 297:0 × 1 |
| 255 | 1×3 | 265:0 / 280:0 / 5:* | 131:0 × 2 |
| 256 | 2×1 | 54:* 131:* | 146:0 × 1 |
| 257 | 1×2 | 44:1 / 44:1 | 24:1 × 1 |
| 258 | 1×2 | 182:0 / 182:0 | 179:1 × 1 |
| 259 | 1×2 | 44:7 / 44:7 | 155:1 × 1 |
| 260 | 1×2 | 155:0 / 155:0 | 155:2 × 2 |
| 261 | 1×2 | 44:5 / 44:5 | 98:3 × 1 |
| 262 | 2×1 | 35:0 35:0 | 171:0 × 3 |
| 263 | 2×1 | 35:1 35:1 | 171:1 × 3 |
| 264 | 2×1 | 35:2 35:2 | 171:2 × 3 |
| 265 | 2×1 | 35:3 35:3 | 171:3 × 3 |
| 266 | 2×1 | 35:4 35:4 | 171:4 × 3 |
| 267 | 2×1 | 35:5 35:5 | 171:5 × 3 |
| 268 | 2×1 | 35:6 35:6 | 171:6 × 3 |
| 269 | 2×1 | 35:7 35:7 | 171:7 × 3 |
| 270 | 2×1 | 35:8 35:8 | 171:8 × 3 |
| 271 | 2×1 | 35:9 35:9 | 171:9 × 3 |
| 272 | 2×1 | 35:10 35:10 | 171:10 × 3 |
| 273 | 2×1 | 35:11 35:11 | 171:11 × 3 |
| 274 | 2×1 | 35:12 35:12 | 171:12 × 3 |
| 275 | 2×1 | 35:13 35:13 | 171:13 × 3 |
| 276 | 2×1 | 35:14 35:14 | 171:14 × 3 |
| 277 | 2×1 | 35:15 35:15 | 171:15 × 3 |
| 278 | 形なし | 339:0 + 339:0 + 339:0 + 334:0 | 340:0 × 1 |
| 279 | 形なし | 340:0 + 351:0 + 288:0 | 386:0 × 1 |
| 281 | 1×2 | 5:* / 5:* | 280:0 × 4 |
| 282 | 1×2 | 263:0 / 280:0 | 50:0 × 4 |
| 283 | 1×2 | 263:1 / 280:0 | 50:0 × 4 |
| 284 | 1×2 | 86:* / 50:* | 91:0 × 1 |
| 285 | 1×2 | 54:* / 328:0 | 342:0 × 1 |
| 286 | 1×2 | 61:* / 328:0 | 343:0 × 1 |
| 287 | 1×2 | 46:* / 328:0 | 407:0 × 1 |
| 288 | 1×2 | 154:* / 328:0 | 408:0 × 1 |
| 289 | 1×2 | 280:0 / 4:* | 69:0 × 1 |
| 290 | 1×2 | 331:0 / 280:0 | 76:0 × 1 |
| 291 | 2×1 | 1:0 1:0 | 70:0 × 1 |
| 292 | 2×1 | 5:* 5:* | 72:0 × 1 |
| 293 | 2×1 | 265:0 265:0 | 148:0 × 1 |
| 294 | 2×1 | 266:0 266:0 | 147:0 × 1 |
| 295 | 1×2 | 341:0 / 33:* | 29:0 × 1 |
| 296 | 1×1 | 41:* | 266:0 × 9 |
| 297 | 1×1 | 42:* | 265:0 × 9 |
| 298 | 1×1 | 57:* | 264:0 × 9 |
| 299 | 1×1 | 133:* | 388:0 × 9 |
| 300 | 1×1 | 22:* | 351:4 × 9 |
| 301 | 1×1 | 152:* | 331:0 × 9 |
| 302 | 1×1 | 173:* | 263:0 × 9 |
| 303 | 1×1 | 170:* | 296:0 × 9 |
| 304 | 1×1 | 165:* | 341:0 × 9 |
| 305 | 1×1 | 266:0 | 371:0 × 9 |
| 306 | 1×1 | 360:0 | 362:0 × 1 |
| 307 | 1×1 | 86:* | 361:0 × 4 |
| 308 | 1×1 | 338:0 | 353:0 × 1 |
| 309 | 1×1 | 17:0 | 5:0 × 4 |
| 310 | 1×1 | 17:1 | 5:1 × 4 |
| 311 | 1×1 | 17:2 | 5:2 × 4 |
| 312 | 1×1 | 17:3 | 5:3 × 4 |
| 313 | 1×1 | 162:0 | 5:4 × 4 |
| 314 | 1×1 | 162:1 | 5:5 × 4 |
| 315 | 1×1 | 1:0 | 77:0 × 1 |
| 316 | 1×1 | 5:* | 143:0 × 1 |
| 317 | 形なし | 351:4 + 351:1 + 351:1 + 351:15 | 351:13 × 4 |
| 318 | 形なし | 39:0 + 40:0 + 281:0 | 282:0 × 1 |
| 319 | 形なし | 86:0 + 353:0 + 344:0 | 400:0 × 1 |
| 320 | 形なし | 375:0 + 39:0 + 353:0 | 376:0 × 1 |
| 321 | 形なし | 351:0 + 351:15 + 351:15 | 351:7 × 3 |
| 322 | 形なし | 351:4 + 351:1 + 351:9 | 351:13 × 3 |
| 323 | 形なし | 289:0 + 377:0 + 263:0 | 385:0 × 3 |
| 324 | 形なし | 289:0 + 377:0 + 263:1 | 385:0 × 3 |
| 325 | 形なし | 377:0 + 341:0 | 378:0 × 1 |
| 326 | 形なし | 98:0 + 106:0 | 98:1 × 1 |
| 327 | 形なし | 4:0 + 106:0 | 48:0 × 1 |
| 328 | 形なし | 1:3 + 406:0 | 1:1 × 1 |
| 329 | 形なし | 1:3 + 4:0 | 1:5 × 2 |
| 330 | 形なし | 351:15 + 35:0 | 35:0 × 1 |
| 331 | 形なし | 351:14 + 35:0 | 35:1 × 1 |
| 332 | 形なし | 351:13 + 35:0 | 35:2 × 1 |
| 333 | 形なし | 351:12 + 35:0 | 35:3 × 1 |
| 334 | 形なし | 351:11 + 35:0 | 35:4 × 1 |
| 335 | 形なし | 351:10 + 35:0 | 35:5 × 1 |
| 336 | 形なし | 351:9 + 35:0 | 35:6 × 1 |
| 337 | 形なし | 351:8 + 35:0 | 35:7 × 1 |
| 338 | 形なし | 351:7 + 35:0 | 35:8 × 1 |
| 339 | 形なし | 351:6 + 35:0 | 35:9 × 1 |
| 340 | 形なし | 351:5 + 35:0 | 35:10 × 1 |
| 341 | 形なし | 351:4 + 35:0 | 35:11 × 1 |
| 342 | 形なし | 351:3 + 35:0 | 35:12 × 1 |
| 343 | 形なし | 351:2 + 35:0 | 35:13 × 1 |
| 344 | 形なし | 351:1 + 35:0 | 35:14 × 1 |
| 345 | 形なし | 351:0 + 35:0 | 35:15 × 1 |
| 346 | 形なし | 351:1 + 351:15 | 351:9 × 2 |
| 347 | 形なし | 351:1 + 351:11 | 351:14 × 2 |
| 348 | 形なし | 351:2 + 351:15 | 351:10 × 2 |
| 349 | 形なし | 351:0 + 351:15 | 351:8 × 2 |
| 350 | 形なし | 351:8 + 351:15 | 351:7 × 2 |
| 351 | 形なし | 351:4 + 351:15 | 351:12 × 2 |
| 352 | 形なし | 351:4 + 351:2 | 351:6 × 2 |
| 353 | 形なし | 351:4 + 351:1 | 351:5 × 2 |
| 354 | 形なし | 351:5 + 351:9 | 351:13 × 2 |
| 355 | 形なし | 265:0 + 318:0 | 259:0 × 1 |
| 356 | 形なし | 368:0 + 377:0 | 381:0 × 1 |
| 357 | 形なし | 369:0 | 377:0 × 2 |
| 358 | 形なし | 37:0 | 351:11 × 1 |
| 359 | 形なし | 38:0 | 351:1 × 1 |
| 360 | 形なし | 352:0 | 351:15 × 3 |
| 361 | 形なし | 38:1 | 351:12 × 1 |
| 362 | 形なし | 38:2 | 351:13 × 1 |
| 363 | 形なし | 38:3 | 351:7 × 1 |
| 364 | 形なし | 38:4 | 351:1 × 1 |
| 365 | 形なし | 38:5 | 351:14 × 1 |
| 366 | 形なし | 38:6 | 351:7 × 1 |
| 367 | 形なし | 38:7 | 351:9 × 1 |
| 368 | 形なし | 38:8 | 351:7 × 1 |
| 369 | 形なし | 175:0 | 351:11 × 2 |
| 370 | 形なし | 175:1 | 351:13 × 2 |
| 371 | 形なし | 175:4 | 351:1 × 2 |
| 372 | 形なし | 175:5 | 351:9 × 2 |

### 10.4 Banner pattern データ

`#`はdye、`.`は空白。通常maskではbannerは`.`の場所に置く。special素材は同Item/metaで照合する。BASEはクラフトpatternではない。上から原版enum順。

| enum name | 保存code | mask / special素材 |
|---|---|---|
| BASE | b | クラフトなし |
| SQUARE_BOTTOM_LEFT | bl | ... / ... / #.. |
| SQUARE_BOTTOM_RIGHT | br | ... / ... / ..# |
| SQUARE_TOP_LEFT | tl | #.. / ... / ... |
| SQUARE_TOP_RIGHT | tr | ..# / ... / ... |
| STRIPE_BOTTOM | bs | ... / ... / ### |
| STRIPE_TOP | ts | ### / ... / ... |
| STRIPE_LEFT | ls | #.. / #.. / #.. |
| STRIPE_RIGHT | rs | ..# / ..# / ..# |
| STRIPE_CENTER | cs | .#. / .#. / .#. |
| STRIPE_MIDDLE | ms | ... / ### / ... |
| STRIPE_DOWNRIGHT | drs | #.. / .#. / ..# |
| STRIPE_DOWNLEFT | dls | ..# / .#. / #.. |
| STRIPE_SMALL | ss | #.# / #.# / ... |
| CROSS | cr | #.# / .#. / #.# |
| STRAIGHT_CROSS | sc | .#. / ### / .#. |
| TRIANGLE_BOTTOM | bt | ... / .#. / #.# |
| TRIANGLE_TOP | tt | #.# / .#. / ... |
| TRIANGLES_BOTTOM | bts | ... / #.# / .#. |
| TRIANGLES_TOP | tts | .#. / #.# / ... |
| DIAGONAL_LEFT | ld | ##. / #.. / ... |
| DIAGONAL_RIGHT | rd | ... / ..# / .## |
| DIAGONAL_LEFT_MIRROR | lud | ... / #.. / ##. |
| DIAGONAL_RIGHT_MIRROR | rud | .## / ..# / ... |
| CIRCLE_MIDDLE | mc | ... / .#. / ... |
| RHOMBUS_MIDDLE | mr | .#. / #.# / .#. |
| HALF_VERTICAL | vh | ##. / ##. / ##. |
| HALF_HORIZONTAL | hh | ### / ### / ... |
| HALF_VERTICAL_MIRROR | vhr | .## / .## / .## |
| HALF_HORIZONTAL_MIRROR | hhb | ... / ### / ### |
| BORDER | bo | ### / #.# / ### |
| CURLY_BORDER | cbo | special 106:0 |
| CREEPER | cre | special 397:4 |
| GRADIENT | gra | #.# / .#. / .#. |
| GRADIENT_UP | gru | .#. / .#. / #.# |
| BRICKS | bri | special 45:0 |
| SKULL | sku | special 397:1 |
| FLOWER | flo | special 38:8 |
| MOJANG | moj | special 322:1 |

### 10.5 精錬26件

Item/meta表記は上と同じ。出力はすべてcount1。XPはfloat。

| 入力 | 出力 | XP |
|---|---|---:|
| 15:* | 265:0 | 0.7F |
| 14:* | 266:0 | 1.0F |
| 56:* | 264:0 | 1.0F |
| 12:* | 20:0 | 0.1F |
| 319:* | 320:0 | 0.35F |
| 363:* | 364:0 | 0.35F |
| 365:* | 366:0 | 0.35F |
| 411:* | 412:0 | 0.35F |
| 423:* | 424:0 | 0.35F |
| 4:* | 1:0 | 0.1F |
| 98:0 | 98:2 | 0.1F |
| 337:* | 336:0 | 0.3F |
| 82:* | 172:0 | 0.35F |
| 81:* | 351:2 | 0.2F |
| 17:* | 263:1 | 0.15F |
| 162:* | 263:1 | 0.15F |
| 129:* | 388:0 | 1.0F |
| 392:* | 393:0 | 0.35F |
| 87:* | 405:0 | 0.1F |
| 19:1 | 19:0 | 0.15F |
| 349:0 | 350:0 | 0.35F |
| 349:1 | 350:1 | 0.35F |
| 16:* | 263:0 | 0.1F |
| 73:* | 331:0 | 0.7F |
| 21:* | 351:4 | 0.2F |
| 153:* | 406:0 | 0.2F |

### 10.6 色変換に使う数値

染料metadataは羊毛/Block色metadataと逆順である。EnumDyeColor.byDyeDamageの範囲外はdye damage0のBLACKへ、byMetadataの範囲外はBlock色meta0のWHITEへ、FireworksのItemDye配列添字は`metadata & 15`へ進む。これらの範囲外処理を統一しない。羊RGBの配列はgetterからsame参照を返す。armorの染色では次のfloat三値、Fireworksでは別のRGB整数を使う。未染色leatherのgetColorは10511680、non-leatherは-1、setColor(non-leather)は例外。

| 名前 | Block色meta | dye damage | 羊/armor用 R,G,B（float） | Fireworks用RGB整数 |
|---|---:|---:|---|---:|
| WHITE | 0 | 15 | 1.0, 1.0, 1.0 | 15790320 |
| ORANGE | 1 | 14 | 0.85, 0.5, 0.2 | 15435844 |
| MAGENTA | 2 | 13 | 0.7, 0.3, 0.85 | 12801229 |
| LIGHT_BLUE | 3 | 12 | 0.4, 0.6, 0.85 | 6719955 |
| YELLOW | 4 | 11 | 0.9, 0.9, 0.2 | 14602026 |
| LIME | 5 | 10 | 0.5, 0.8, 0.1 | 4312372 |
| PINK | 6 | 9 | 0.95, 0.5, 0.65 | 14188952 |
| GRAY | 7 | 8 | 0.3, 0.3, 0.3 | 4408131 |
| SILVER | 8 | 7 | 0.6, 0.6, 0.6 | 11250603 |
| CYAN | 9 | 6 | 0.3, 0.5, 0.6 | 2651799 |
| PURPLE | 10 | 5 | 0.5, 0.25, 0.7 | 8073150 |
| BLUE | 11 | 4 | 0.2, 0.3, 0.7 | 2437522 |
| BROWN | 12 | 3 | 0.4, 0.3, 0.2 | 5320730 |
| GREEN | 13 | 2 | 0.4, 0.5, 0.2 | 3887386 |
| RED | 14 | 1 | 0.6, 0.2, 0.2 | 11743532 |
| BLACK | 15 | 0 | 0.1, 0.1, 0.1 | 1973019 |

MapColorの配列は64参照、標準初期化はindex0..35。constructorはindex範囲0..63を検査しcolor/indexをstoreして、その参照をarray[index]へ置く。既存同indexのnamed参照を変更しない。getMapColorのshade0/1/2/3は係数180/220/255/135、他整数は220。RGB各channelに整数で`channel×係数/255`、alpha255を付ける。map byteの高6bitがcolor index、低2bitがshade。index0の画面上checker/透明処理はrenderer別仕様であり、getMapColorそのもののalpha255を変更しない。

| color index | named参照 | RGB整数 |
|---:|---|---:|
| 0 | airColor | 0 |
| 1 | grassColor | 8368696 |
| 2 | sandColor | 16247203 |
| 3 | clothColor | 13092807 |
| 4 | tntColor | 16711680 |
| 5 | iceColor | 10526975 |
| 6 | ironColor | 10987431 |
| 7 | foliageColor | 31744 |
| 8 | snowColor | 16777215 |
| 9 | clayColor | 10791096 |
| 10 | dirtColor | 9923917 |
| 11 | stoneColor | 7368816 |
| 12 | waterColor | 4210943 |
| 13 | woodColor | 9402184 |
| 14 | quartzColor | 16776437 |
| 15 | adobeColor | 14188339 |
| 16 | magentaColor | 11685080 |
| 17 | lightBlueColor | 6724056 |
| 18 | yellowColor | 15066419 |
| 19 | limeColor | 8375321 |
| 20 | pinkColor | 15892389 |
| 21 | grayColor | 5000268 |
| 22 | silverColor | 10066329 |
| 23 | cyanColor | 5013401 |
| 24 | purpleColor | 8339378 |
| 25 | blueColor | 3361970 |
| 26 | brownColor | 6704179 |
| 27 | greenColor | 6717235 |
| 28 | redColor | 10040115 |
| 29 | blackColor | 1644825 |
| 30 | goldColor | 16445005 |
| 31 | diamondColor | 6085589 |
| 32 | lapisColor | 4882687 |
| 33 | emeraldColor | 55610 |
| 34 | obsidianColor | 8476209 |
| 35 | netherrackColor | 7340544 |

## 11. 根拠と検証範囲

本文の具体規則は提供された `net.minecraft` 以下の次の原版を読み、Java bodyを転載せず仕様化した。

- `block/Block`、`block/BlockRedstoneWire`、`block/material/Material`/各派生、`block/material/MapColor`、`item/Item`/`ItemStack`。
- `entity/player/InventoryPlayer`/`EntityPlayer`/`PlayerCapabilities`、`inventory/Container`/`ContainerPlayer`/`ContainerWorkbench`/`Slot`/`SlotCrafting`/`InventoryCrafting`/`InventoryCraftResult`/`InventoryEnderChest`。
- `item/crafting/CraftingManager`、`ShapedRecipes`、`ShapelessRecipes`、各動的recipe、`FurnaceRecipes`、`tileentity/TileEntityFurnace`/`TileEntityBanner`。
- `entity/Entity`/`EntityLivingBase`/`entity/item/EntityItem`、`entity/ai/EntityAITasks`、`util/FoodStats`、`potion/PotionEffect`。
- `item/ItemMap`/`ItemEmptyMap`、`world/storage/MapData`/`MapStorage`、`stats/StatBase`/`StatFileWriter`/`StatisticsFile`、`scoreboard/Scoreboard`/`ScoreObjective`/`Score`。
- `network/NetHandlerPlayServer`、`client/network/NetHandlerPlayClient`、`command/CommandHandler`/`ServerCommandManager`。

この仕様作成ではJava実行、game bootstrap、新しいobserver、build、ゲームのnetwork試験を実行していない。既存の対照検証結果は原版全体を網羅する証拠ではない。静的365表の行数、登録表の整合、記載した特別recipe位置、精錬26件は文書生成時に静的に照合する。本章に未充足として残した内容を、テスト通過済みまたは翻訳完了とは表示しない。
