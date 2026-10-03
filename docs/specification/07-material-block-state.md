# Material、MapColor、propertyとcanonical BlockState

本章は[ゲーム処理](04-gameplay.md)の基盤を、fieldと処理順の粒度で補う。[実行モデル](01-runtime.md)を適用する。既存native block paletteの仕様ではない。全Block subclass、全Guava collection、全failure diagnosticを閉じた仕様は未充足である。

## BS-01 Materialのinstance field

| field | 型 | allocation初期値／initializer |
|---|---|---|
| canBurn | boolean | false |
| replaceable | boolean | false |
| isTranslucent | boolean | false |
| materialMapColor | final MapColor reference | NULL、constructorで引数の同じrefを代入 |
| requiresNoTool | boolean | allocation false、instance initializerでtrue |
| mobilityFlag | int | 0 |
| isAdventureModeExempt | boolean | false |

constructorはrequiresNoTool initializerのあと、nullable color引数をmaterialMapColorへ一回代入する。NULL colorは合法。MapColor array indexから毎回解決するownerではない。4つのnamed subclassと匿名web subclassは追加instance fieldを持たない。

| 実class | 親完了後constructorの追加処理 | liquid | solid | blocksLight | blocksMovement |
|---|---|---:|---:|---:|---:|
| Material | なし | false | true | true | true |
| MaterialLiquid | replaceable=true、mobility=1をこの順でsetter経由 | true | false | **true（親を継承）** | false |
| MaterialLogic | adventureModeExempt=true | false | false | false | false |
| MaterialTransparent | replaceable=true | false | false | false | false |
| MaterialPortal | なし | false | false | false | false |
| Materialの匿名web subtype | なし | false | true | true | false |

表のbooleanは指定classのbody結果であり、原版のvirtual dispatchをflagの相関から推測するという意味ではない。特にLiquidはblocksLightをoverrideしない。webはblocksMovementのみoverrideする。

## BS-02 Materialの操作

getCanBurn、isReplaceable、isToolNotRequired、getMaterialMobility、getMaterialMapColorは対応fieldを返す。colorは同ref。isOpaqueはisTranslucent=trueならfalseで終わり、falseならその時点でvirtual blocksMovementを一度呼んで返す。isSolidやblocksLightから計算しない。

| setter | mutation | return | 原版access |
|---|---|---|---|
| setTranslucent | isTranslucent=true | this | private |
| setRequiresTool | requiresNoTool=false | this | protected |
| setBurning | canBurn=true | this | protected |
| setReplaceable | replaceable=true | this | public |
| setNoPushMobility | mobilityFlag=1 | this | protected |
| setImmovableMobility | mobilityFlag=2 | this | protected |
| setAdventureModeExempt | isAdventureModeExempt=true | this | protected |

MaterialにはisAdventureModeExempt getterは宣言されていない。setterのreturnを新objectにしない。静的チェーンはsetterを表の順に実行し、constructor内で同じsetterが既に呼ばれていても省略しない。

## BS-03 Materialの全35静的参照

各行は宣言順に新しい個体を生成する。同じMapColorまたは同じclassでも別Material。指定MapColorはnamed staticのrefであり、公開mapColorArrayの現在要素を引き直す規範ではない。「追加setter」はconstructor追加処理の後の順である。

| static名 | 実class | MapColor named ref | 追加setter順 |
|---|---|---|---|
| air | MaterialTransparent | airColor | なし |
| grass | Material | grassColor | なし |
| ground | Material | dirtColor | なし |
| wood | Material | woodColor | setBurning |
| rock | Material | stoneColor | setRequiresTool |
| iron | Material | ironColor | setRequiresTool |
| anvil | Material | ironColor | setRequiresTool → setImmovableMobility |
| water | MaterialLiquid | waterColor | setNoPushMobility |
| lava | MaterialLiquid | tntColor | setNoPushMobility |
| leaves | Material | foliageColor | setBurning → setTranslucent → setNoPushMobility |
| plants | MaterialLogic | foliageColor | setNoPushMobility |
| vine | MaterialLogic | foliageColor | setBurning → setNoPushMobility → setReplaceable |
| sponge | Material | yellowColor | なし |
| cloth | Material | clothColor | setBurning |
| fire | MaterialTransparent | airColor | setNoPushMobility |
| sand | Material | sandColor | なし |
| circuits | MaterialLogic | airColor | setNoPushMobility |
| carpet | MaterialLogic | clothColor | setBurning |
| glass | Material | airColor | setTranslucent → setAdventureModeExempt |
| redstoneLight | Material | airColor | setAdventureModeExempt |
| tnt | Material | tntColor | setBurning → setTranslucent |
| coral | Material | foliageColor | setNoPushMobility |
| ice | Material | iceColor | setTranslucent → setAdventureModeExempt |
| packedIce | Material | iceColor | setAdventureModeExempt |
| snow | MaterialLogic | snowColor | setReplaceable → setTranslucent → setRequiresTool → setNoPushMobility |
| craftedSnow | Material | snowColor | setRequiresTool |
| cactus | Material | foliageColor | setTranslucent → setNoPushMobility |
| clay | Material | clayColor | なし |
| gourd | Material | foliageColor | setNoPushMobility |
| dragonEgg | Material | foliageColor | setNoPushMobility |
| portal | MaterialPortal | airColor | setImmovableMobility |
| cake | Material | airColor | setNoPushMobility |
| web | Material (匿名web) | clothColor | setRequiresTool → setNoPushMobility |
| piston | Material | stoneColor | setImmovableMobility |
| barrier | Material | airColor | setRequiresTool → setImmovableMobility |

## BS-04 MapColor

fieldはfinal int colorValue、final int colorIndex。staticは64要素mapColorArrayと次表の36 named refs。constructorは0<=index<=63を検査し、colorIndex、colorValue、array[index]=thisの順。範囲外はfield代入前のIndexOutOfBoundsException。

同indexへ別個体をconstructすればarray slotは置換されるが、過去のnamed static refやMaterialに保持されたrefは変わらない。index36..63は通常初期化完了時NULL。既存色の再生成・keyでのinternを追加しない。

| index | named ref | RGB hexadecimal |
|---:|---|---|
| 0 | airColor | 000000 |
| 1 | grassColor | 7FB238 |
| 2 | sandColor | F7E9A3 |
| 3 | clothColor | C7C7C7 |
| 4 | tntColor | FF0000 |
| 5 | iceColor | A0A0FF |
| 6 | ironColor | A7A7A7 |
| 7 | foliageColor | 007C00 |
| 8 | snowColor | FFFFFF |
| 9 | clayColor | A4A8B8 |
| 10 | dirtColor | 976D4D |
| 11 | stoneColor | 707070 |
| 12 | waterColor | 4040FF |
| 13 | woodColor | 8F7748 |
| 14 | quartzColor | FFFCF5 |
| 15 | adobeColor | D87F33 |
| 16 | magentaColor | B24CD8 |
| 17 | lightBlueColor | 6699D8 |
| 18 | yellowColor | E5E533 |
| 19 | limeColor | 7FCC19 |
| 20 | pinkColor | F27FA5 |
| 21 | grayColor | 4C4C4C |
| 22 | silverColor | 999999 |
| 23 | cyanColor | 4C7F99 |
| 24 | purpleColor | 7F3FB2 |
| 25 | blueColor | 334CB2 |
| 26 | brownColor | 664C33 |
| 27 | greenColor | 667F33 |
| 28 | redColor | 993333 |
| 29 | blackColor | 191919 |
| 30 | goldColor | FAEE4D |
| 31 | diamondColor | 5CDBD5 |
| 32 | lapisColor | 4A80FF |
| 33 | emeraldColor | 00D93A |
| 34 | obsidianColor | 815631 |
| 35 | netherrackColor | 700200 |

getMapColor(shade)の係数はdefault220、shade0=180、1=220、2=255、3=135。その他の整数も220。RGB各channelを取り出し、channel×係数/255を整数計算し、alpha FFと結合したsigned32 ARGBを返す。元colorValueの高byteをalphaとして返さない。

## BS-05 PropertyHelperと各property

PropertyHelperのfieldはfinal valueClass、final name。constructorはClassを先、nameを後に代入し、独自のNULL拒否を追加しない。getNameとgetValueClassは同refを返す。equalsは同一refならtrue、非NULLかつexact runtime class一致ならClass.equalsのあとString.equals。hashは31×Class.hashCode+String.hashCodeをsigned32 wrapする。

PropertyBoolのallowedValuesはboxed true、falseの順をGuava ImmutableSetへ入れる。getAllowedValuesは同集合。getName(value)はBoolean.toString相当だが、NULL receiverや変換の失敗を独自defaultへ置換しない。

PropertyIntegerはparent初期化後にmin<0を拒否、その次にmax<=minを拒否する。inclusive min..maxをInteger boxingしてHashSetへ入れ、そのiterationをImmutableSetへcopyする。allowed orderを数値sortへ変更しない。max=INT_MAXではsigned incrementのwrapを保持し、native安全guardは別境界として表示する。equalsはparent条件のあとallowedValues set equality、hashは31×parentHash+allowedHash。

PropertyEnumはparentのあとnameToValue mutable mapを生成し、引数collectionからImmutableSetを作る。そのあと**元collection**をiterationし、valueのIStringSerializable.getNameを呼び、既存serialized nameならIllegalArgumentException、なければputする。dedupしたallowed集合を代わりにiterationしない。equals/hashはPropertyHelperを継承し、allowedValuesを比較へ追加しない。

PropertyEnumのcreate(name,Class)はalways-true predicate、Class enum constantsを宣言順arrayとして取得、new list、filter view、collection constructorへ渡す。varargs overloadはarrayからlist、collection overloadは直接constructor。PropertyDirectionはPropertyEnum派生で、EnumFacingのfilter/serialized name/ordinalの契約を必要とする。

toStringやerror textはproperty getter、allowed collection、Guava Objects、registryを呼ぶ。単なる固定文字列でfailure-prefixを完了させない。Boolean/Integer cache、Class identity/hash、Guava iteration/duplicate/null rulesの全条件は未充足。

## BS-06 BlockState構築

BlockStateはblock、properties ImmutableList、validStates ImmutableListを保持する。constructor:

1. blockInのsame refを保持する。
2. **callerのproperty arrayそのもの**を、virtual getNameとString.compareToでsortする。copyしてからsortしない。
3. sorted property refsをImmutableListへcopyする。
4. whole property-value mapをkeyに持つLinkedHashMapと、state作業ArrayListを新規生成する。
5. property順にvirtual getAllowedValuesを呼び、Cartesian tupleを列挙する。各tupleからMapPopulatorで新しいproperty→value mapを生成する。
6. immutable map copyを渡し、blockとpropertiesを持つ新StateImplementationを一つ生成する。whole-map→stateをput、作業listへappendする。
7. 全state生成後に各stateのtransition tableを順に構築する。
8. 最後にvalidStatesを作業listのImmutableList.copyOfで設定する。

zero propertyは一つのempty tupleなので、一つの実stateを持つ。getBaseStateはvalidStates[0]、getValidStates/getProperties/getBlockは保持refを返す。registered meta16通りだけを生成してvalidStatesの代用品にしない。

## BS-07 StateImplementationとtransition

fieldはblock、ImmutableMap properties、当初NULLのImmutableTable propertyValueTable。blockとpropertiesはconstructor引数のsame ref。getPropertyNamesはproperties.keySetのunmodifiable collection view。getPropertiesはsame immutable map。equalsはidentity、hashCodeはproperties mapのhashである。

getValue(property)はcontainsKeyを先に検査し、なければIllegalArgumentException。あればproperty.getValueClass().cast(properties.get(property))を評価する。等価だが別refのpropertyもmap key規範で照合する。全keyをpointer identityだけで検査しない。

withProperty(property,value)は、key存在、property.getAllowedValues().contains(value)をこの順に検査する。どちらも通った後にcurrent value ref==valueならthis、それ以外はprebuilt table.get(property,value)を返す。samevalueだから最初にthisを返す近道は禁止。equalだが別refのboxed valueはtable経由になる。

buildPropertyValueTableは既に非NULLならIllegalStateException。HashBasedTableを生成し、properties key順・allowed value順に処理する。current値と同refだけをskip。他の値ではmutable HashMap copyへ一key置換し、constructorのwhole-map registryからequals keyでcanonical stateを引き、tableへ入れる。最後にImmutableTable.copyOfを代入する。stateを呼出しごとに作る関数ではない。

cyclePropertyはallowed collectionを取得してからcurrent valueをgetValueし、iterator各value.equals(current)で位置を探す。次があれば次、末尾なら新iteratorの先頭。見つからなければexhausted iterator.nextの失敗へ到達する。ordinal+1のmoduloに置換しない。

## BS-08 CartesianとMapPopulator

Cartesianはiterables、iterators、resultsの実arrayとindexを持つiterator。tuple returnはresults.cloneであり、tupleごとの別arrayである。最後のdimensionが先に進む。全component.iteratorはconstruct時に順に取得する。zero-dimensional productは一empty tuple。

componentが空の最初のhasNextではindex=0へ変更後にcomponentを調べ、最初のfalseでendOfDataを呼ぶ。endOfDataはindex=-1、iterators要素をNULL fill、results要素をNULL fill。**その最初のhasNextのreturnはtrue**。次のhasNextはfalseになる。この分岐は実classfileでも静的確認した。

静的control flowから、hasNext→nextはnext内の再hasNextでfalseになりNoSuchElementException、一方最初からnextを呼ぶと内部hasNextがtrueのあとindex=-1でarray accessへ進む、と読める。この二つの例外は新しいJVM実行観測で証明したものではない。全backtrack／iterator exceptionの状態遷移はまだ未充足。

MapPopulator.createMapは新LinkedHashMapを生成してpopulateMapへ渡す。populateMapはvalues.iteratorをkeys.iteratorより先に取得する。各keyに対してvalue.next、map.putの順。keyの方が多ければ途中next失敗、valueが余れば最後のhasNextでNoSuchElementException。失敗前にmapへ済んだputを戻さず、正常はsame mapを返す。

## BS-09 Blockの25instance fieldとconstructor

fieldはdisplayOnCreativeTab、fullBlock、lightOpacity、translucent、lightValue、useNeighborBrightness、blockHardness、blockResistance、enableStats、needsRandomTick、isBlockContainer、minX/minY/minZ/maxX/maxY/maxZ、stepSound、blockParticleGravity、blockMaterial、blockMapColor、slipperiness、blockState、defaultBlockState、unlocalizedName。displayOnCreativeTabはCreativeTabs参照、allocation初期値NULLでこのconstructorでは変更しない。MaterialとMapColorは独立final参照。

two-argument constructorはenableStats=true、stepSound=soundTypeStone、blockParticleGravity=1.0F、slipperiness=0.6Fを設定する。次にMaterial ref、MapColor ref、final setBlockBounds(0,0,0,1,1,1)、virtual isOpaqueCube→fullBlock、**二度目のvirtual isOpaqueCube**→lightOpacity(255/0)、捕捉されたmaterial引数.blocksLightの否定→translucent、virtual createBlockState→blockState、そのgetBaseState→final setDefaultStateの順。

setBlockBoundsはfinalでありvirtual hookではない。isOpaqueCubeとcreateBlockStateはmost-derived override。createBlockStateの時点でblockState/defaultBlockStateはまだNULL。NULL material/colorを同じ位置で先行拒否しない。colorNULLは保持できるがmaterialNULLはblocksLightを呼ぶ段階等で失敗する。失敗前のbounds/field storesは残る。

one-argument constructorはsame materialのgetMaterialMapColorを取得してtwo-argumentへ渡す。Material refとcolor refを後で連動させない。base getMapColorはstate引数を使わずblockMapColor、getMaterialはblockMaterial。base createBlockStateは新empty property arrayを渡したBlockState、base getStateFromMetaはmetadataを無視しdefault state、base getMetaFromStateはNULLまたはpropertyなしなら0、propertyありなら例外。

base getActualStateはsame stateを返す。setDefaultStateとgetDefaultStateは直接refを設定／返すfinal method。各subclassのmetadata/actualstate/color等をbaseで成功させない。

## BS-10 IDとlive owner

Block.getStateIdはblock registry ID + (metadata<<12)。getStateByIdはlow12bitをBlock ID、bits12..15をmetadataとしてgetStateFromMetaへ渡す。一方BLOCK_STATE_IDSとsection charは通常(Block ID<<4)|metadata。legacy item damage、state graph identity、network/storage palette、Block IDを同じintegerと扱わない。

registry全198constructorとdefault、metadata alias、property allowed values、28subclass color override、sound setter等は[ゲーム処理](04-gameplay.md)のID/name表だけでは未閉鎖。live Chunk/World/render/mapへ切替える際にはSource stateのone ownerを使い、NativeBlockとSourceBlockの二つのauthoritative material/stateを残さない。

## BS-11 ObjectIntIdentityMap

このmapは二つの独立した索引を持つ。`identityMap`はexpectedMaxSize引数512で生成するJDK8 IdentityHashMapであり、keyのequals/hashCodeではなく参照同一性で整数を対応させる。`objectList`は新しい空ArrayListであり、整数indexから参照を取得する。この二つを常に一対一の対応へ修復してはいけない。

`put(key, value)`は、まずidentityMapへkey→boxed valueをstoreする。次にobjectListのsizeがvalue以下である間NULLを末尾に追加し、最後にindex valueへkeyをstoreする。負のvalueでもidentityMapへのstoreは先に完了し、list.setの範囲検査で失敗する。伸長中の失敗でも、それまでのmap更新やNULL追加は残る。原版にない上限clamp、先行負数拒否、rollbackを加えない。

同じkeyを別valueへ再登録しても古いlist位置は消さない。別keyを同じvalueへ登録するとlist位置は後のkeyになるが、前のkeyのidentityMap対応は残る。したがって`get(key)`と`getByValue(get(key))`が同じ参照になるという条件は、全到達状態で成立するとは限らない。NULL keyもIdentityHashMapへ登録でき、listのNULLは未登録穴と区別できない。

`get(key)`はidentityMapのlookup結果がNULLなら-1、それ以外はboxed integerの値を返す。登録済みvalueが-1の場合と未登録を区別する追加flagはない。`getByValue(value)`は0以上かつlist.size未満ならそのlist要素を返し、それ以外はNULL。どちらのlookupも他方の索引を検査しない。

`iterator()`は、その時点で取得したobjectList.iteratorをGuava17 Iterators.filterとPredicates.notNullで包んだもの。NULL以外の参照をlistのindex順に返す。重複した同じ参照は一回へまとめない。このclassにはsize/clear/removeの独自methodはない。filterの先読み、remove可否、ArrayListのfail-fast検査や並行変更の例外時点はJDK8/Guava17の契約にも依存し、それら全APIの閉鎖はBS-12の未充足として残る。

根拠: `net/minecraft/util/ObjectIntIdentityMap.java`のfield initializerと全4method。これは原版bodyの静的照合であり、例外や並行変更の原版実行を済ませた証拠ではない。

## BS-12 根拠と未充足

根拠: Materialと四派生、匿名web、MapColor、IProperty/PropertyHelper/Bool/Integer/Enum/Direction、BlockState/StateImplementation/BlockStateBase、Cartesian/MapPopulator、Blockの対象body。Materialのbounded original100case/3796比較行一致は既存実装の当該closureの証拠であり、ここでBlockやGuava全体の原版実行をした意味ではない。

未充足: Guava17/JDK8の全到達API、class/static/constructor失敗の全Throwable、全Cartesian backtrack、全Blockのconstructor/setter/property/meta/override、SoundType全動作、全registry initialization、DEBUG Barrier/generator、live owner切替、全map survey色dispatch。本版は特にMaterial／property／stateの構造と順序を記述した基盤であり、これら未充足を黙って実装者へ委ねない。
