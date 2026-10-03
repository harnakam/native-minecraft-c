# 適合性、仕様契約と検証

本章は[総合仕様](README.md)の完了判定を定める。仕様完成、C実装完成、テスト成功は三つの別判定である。本版では仕様完成と全実装適合を主張しない。

## CF-01 契約の必須情報

class／field／method／constructor／initializerの一つを、以下の情報で閉じる。

| 項目 | 内容 |
|---|---|
| contractId | 対象版・class・member・overloadを区別する一意ID |
| target | 宣言class／動的class、型、修飾、親、instance/static |
| input | 各引数の型・値域・NULL・alias・入場時fieldとglobal状態 |
| orderedEffects | branch、読み直し、virtual call、field書込、queue/I/O、乱数消費を順序付きで記述 |
| output | returnの型／値／alias、返却後receiverと外部objectの状態 |
| failure | throw型・点・catch/finally・済んだmutation、出力prefix、後続不実行 |
| dependencies | JDK/library/他classの契約ID。未定義呼出しを残さない |
| evidence | source reading／static bytecode／actual original execution／native replayの区別 |
| cases | 正常・境界・異常・alias・再入・virtual overrideの必要条件 |
| exclusions | 当該証拠で未検証の範囲。仕様対象の削除とは別 |

章の説明があっても、このmember bindingが存在しなければ個別仕様完了として数えない。宣言一覧、method名表、line number、API summaryをalgorithmやfailure-prefixの記述と同一視しない。

## CF-02 classごとの閉鎖

class contractは全宣言fieldに加え、instance/static initializers、暗黙constructor、anonymous body、enum constant body、bridge/accessor等の実到達を扱う。すべてのvirtual呼出し先を最派生receiverと紐付ける。対象外の任意subclassを扱う場合は、そのprofileを明示する。

AST censusは1613ファイル・15232method・2006constructorを数えたが、synthetic memberやinitializerの完全台帳ではない。この母数だけを全JVMclosureと呼ばない。個別契約とfull依存graphが未作成なので、現時点のmember coverageは「未認定」である。

## CF-03 証拠の階段

| 証拠 | 言ってよいこと | 言ってはいけないこと |
|---|---|---|
| source reading | 読んだbodyの分岐・field・呼出し順 | decompilerの疑わしい部分も実行確認済み |
| static classfile | 実classfileのcontrol flow、descriptor、field等 | その入力で実際に例外が起きた |
| original execution | 固定した原版・fixture条件での観測 | 全classinit/concurrency/arbitrary input適合 |
| native replay | 固定原版観測との対象channel差分 | 除外channelまで完全一致 |
| integration | 実client/server間、保存再開、描画等の対象case | 別game mode・未実装機能も互換 |

原版を観測するcodeはexpectedを独自formulaで再計算しない。対象の実field／getter／constructor／setterの実行とidentityを記録する。対象classをpatch、Unsafe、private field書込、別同名実装で置換した観測を「原版」と数えない。

## CF-04 観測record

各recordにはprofile/version、caseId、stepIndex、channel、receiver identity、argument identity、value type、value、exception、random state／call count、changed objectを含む。field順、call順、packet順、NBT型、collection orderを保存する。

identityはpointer addressそのものをcross-process比較せず、観測中に初出したobjectへ一意symbolを与える。同じobjectは同じsymbol、同値別objectは別symbol。NULLは専用symbol。float/doubleはbinary bitと必要な数値表示を分け、NaNを全部同じ文字へnormalizeしない。

exact比較の対象は順序付きrecord列である。sort、欠落行消去、packetの統合、unknown NBT除去、空とNULL同一化で差分を0へしない。許すplatform差／非決定性はchannelごとに条件と理由を明記し、raw corpusと除外行のlistを保存する。

## CF-05 入力の設計

必須caseの種類:

- 値域: 0、±1、MIN/MAX、閾値直前／一致／直後、負座標、空、length1、最大length。
- 浮動小数: ±0、NaN、Infinity、subnormal、丸め、saturating cast、bit pattern。
- 参照: NULL、同一object、同値別object、aliasするfield/引数、共有static、子型。
- 順序: callbackがreceiverを変更、getter再評価で値が変わる、constructor中virtual call、再入。
- 失敗: 各依存が段階ごとに例外、partial arraycopy、IO failure、init failure、connection close。
- 時間: tick境界、frame複数tick、pause、lag、時計後退、taskが新taskをenqueue。
- 永続: save→reload→意味比較、旧tag、未知tag、破損sector、session lock、dimension切替。
- wire: 1byte分割、複数packet結合、threshold、圧縮/暗号切替前後、型・長さ不一致。

native lifetime／undersized／foreign heap／OOM／transaction failureは別のC安全性caseとして必要だが、原版の正常bodyの観測を置き換えない。

## CF-06 双方向マルチプレイ

適合試験は原版client→C server、C client→原版server、C同士を分けて行う。status成功はlogin成功でも、login成功はPlay全handler適合でもない。

シナリオはoffline/online認証、compression有無、暗号login、join、keepalive、同時複数player、移動・teleport／relative flags、chunk/unload/bulk、採掘・設置、inventory全mode、transaction拒否／再同期、chat、ability、death/respawn、dimension、entity spawn／metadata／destroy、scoreboard/team、map、resource pack、disconnect/reconnect、保存再開を含む。

world差分はchunk/state/TE/entity/inventory/capability/time等を比較する。TCP byte列のみの成功ではゲーム状態の正しさを判定しない。packet decoderが読めることと、そのhandlerで実作用が起きることを別のcoverage itemにする。

## CF-07 保存適合

byte roundtripとsemantic roundtripを分ける。NBT compound orderが原版のunordered mapに依存する場合、raw bytesは保持し、意味比較はkeyと型を明示して別channelにする。全tagをStringへ変換した比較は不可。

Anvilではlocation/timestamp/length/compression/sector allocation、chunk/level/player/map/TE/entityのtag、pending save、session lock、失敗前の書込が対象。C919独自journalの保護はそのnative形式の試験であり、原版のdisk sequenceと同じとは数えない。

## CF-08 描画・入力・音適合

同じ資産、window寸法、GUI scale、settings、locale、world、camera、tick/partial tick、renderer条件を固定する。geometry/UV/light/state trace、pixel、UI hit test、screen遷移、sound eventと実audioを別channelへ分ける。

shader/GPU依存に許容差が必要な場合は、対象channelと理由を先に定義する。一律「ほぼ同じ」、見た目が似ている、画像一枚を表示できた、音eventをlogしたという条件を完成判定に使わない。

## CF-09 文書監査

標準監査:

```sh
python scripts/check_specification.py
```

リンク、必要文書、UTF-8、重複ID、coverage schema、source分野合計、不足の明示を検査する。**成功は文書の整合性であり完全仕様の証明ではない。**

提供原版のfingerprint監査:

```sh
python scripts/check_specification.py --source-root MCP-919/src/minecraft
```

全Javaファイルのpath/hash fingerprintと母数を確認する。原版本文やJARを出力・コピーしない。個別memberの意味をhashから証明しない。

完成要求の監査:

```sh
python scripts/check_specification.py --require-complete
```

本版は不足が存在するため終了code2になる。これを緑にするためcomplete flag、分野status、member数を虚偽で変更しない。schema1はdraft専用で、complete=true自体を拒否する。将来完成認定を提供する際は、匿名body・synthetic・initializerを含む全member identity、実在anchor、各case／依存、独立reviewと文書hashを検査する新binding schemaへ移行する。機械のflag検査だけで意味の完成を証明できない。

## CF-10 公開検査と実装検査

公開indexは既存のallowlistを使う。

```sh
python scripts/audit_public.py
git diff --check
```

原版、mapping、JAR、資産、save、private witnessは対象外。新しい仕様資料もこの制約を満たす。公開checkerは標準Pythonだけで動作し、原版なしのcheckoutでも標準監査ができる。

実装変更時には既存CMake／scripts/test-linux.shを基準に、関連C test、TCP／save integration、strict Windows/WSL、ASan/UBSan等を実行する。文書だけの変更ではCのarchiveを作り直したと報告しない。以前のMaterial原版100case/3796比較行の一致はその対象closureの証拠であり、全Minecraftまたは本書全体の充足証拠ではない。

## CF-11 完成の署名

仕様完成の署名は、対象集合、全契約binding、gap0、依存閉鎖、独立再実装レビュー、検査log、版hashを含む。実装完成の署名はさらに全Sourceownerのlive接続、全機能matrix、原版crossplay、Anvil、認証、ユーザー資産による表示・音、failure/recoveryの実試験を含む。

本版はその署名を持たない。`coverage.md`はどこを次に仕様化すべきかの母数であり、未記述部分を手順のように見せる空handler／架空成功値の仕様ではない。
