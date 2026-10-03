# 仕様の充足表

版: 0.1、2026-10-03。[総合仕様](README.md)の完成要求を監査する。

**判定: 未完成。MD文書群だけで全Minecraftを再実装できる状態ではない。**

原版ASTの全1613ファイルは構文解析成功。合計10283211 bytes、2067 named types、344 anonymous bodies、7446 fields、15232 methods、2006 constructorsを数えた。これは「調査母数」であり、個別契約を仕様本文と検証へ結び付けた数ではない。classfileのsynthetic、暗黙constructor、initializer、外部依存closureも別途必要である。

## 分野別の母数と不足

| 分野 | 原版Java files | 本文 | 判定 |
|---|---:|---|---|
| Runtime / utility / crash / event / profiler | 89 | [実行モデル](01-runtime.md) | 部分記述 |
| World / NBT / village | 215 | [ワールドと保存](02-world-storage.md) | 部分記述 |
| Network（client networkを含む） | 139 | [通信](03-network.md) | 部分記述 |
| Block / Item / Entity / inventory等 | 563 | [ゲーム処理](04-gameplay.md) | 部分記述 |
| Server / command | 97 | [実行モデル](01-runtime.md)とゲーム処理 | 部分記述 |
| Client（network/stream以外） | 476 | [クライアント](05-client-resources.md) | 部分記述 |
| Realms / stream / development Start | 34 | [クライアント](05-client-resources.md) | 部分記述 |
| 合計 | 1613 |  | 未完成 |

クラス名・method名の一覧を作っても個別契約を認定しない。現在の`memberContracts`は空である。これは本文に有用な具体的仕様がないという意味ではなく、全宣言を一意IDから全文・依存・例外・検証へ結ぶ全域台帳がまだないという意味である。従って充足率の百分率は掲示しない。

全分野と外部依存の不足を下の機械可読データへ列挙する。仕様不足を元のJavaファイルを公開して解消しない。本文へ処理・数式・状態遷移を記述し、その依存を閉じる。

## 調査の固定と判定方法

fingerprintはslash区切りの全Java相対パスをUTF-8 byte順でsortし（大文字小文字を区別）、各ファイルについて UTF-8 path + NUL + 小文字hex SHA-256 + LF をSHA-256へ順番に与える。OS固有のPath比較順には依存しない。元ファイルをcopy/exportする方式ではない。構文解析は既存のjavalangを調査toolとして使用し、製品dependencyへ追加していない。公開の文書checkerは標準Pythonのみで動く。

- 標準監査の成功: 文書・schema・リンク・母数の整合性。
- source指定時の成功: 上記と原版全ファイルのfingerprint一致。
- `--require-complete`の終了code2: 全体がまだ不足するという正しい判定。
- `complete=true`: member binding、synthetic/initializer、依存、gap0、独立reviewなしには宣言できない。

schemaVersion1はdraft専用であり、checkerは`complete=true`を常に拒否する。母数やflagや架空reviewを変更して完成へ昇格できない。完成認定には、匿名bodyを含む全member identity、実在する本文anchor、各caseと依存、synthetic/initializer、独立reviewと版hashを検査できる新しいbinding schemaが必要になる。本版の簡単な台帳だけでその認定を代用しない。

母数変更は対象profile変更またはfingerprint差として扱う。欠落field、未記述method、利用できない外部serviceを除外して完成率を上げない。

## 機械可読の充足データ

<!-- coverage:start -->
```json
{
  "schemaVersion": 1,
  "profile": "Minecraft Java 1.8.9 / MCP919 / protocol 47",
  "editionDate": "2026-10-03",
  "complete": false,
  "sourceJavaFiles": 1613,
  "sourceBytes": 10283211,
  "sourceFingerprintSHA256": "f4abe0ce50fcbcbd0659af7724fad093873259963f6bbf7361019f72e0534104",
  "declarationCensus": {
    "namedTypes": 2067,
    "anonymousBodies": 344,
    "fields": 7446,
    "methods": 15232,
    "constructors": 2006
  },
  "censusParseErrors": 0,
  "syntheticAndInitializerCoverageComplete": false,
  "memberContracts": [],
  "memberCoverageMeaning": "この個別binding台帳では未認定。章の具体的記述が存在することとは別。",
  "domains": [
    {
      "id": "RUNTIME",
      "sourceFiles": 89,
      "document": "01-runtime.md",
      "status": "partial",
      "gaps": [
        "Java8と依存ライブラリの全到達API・例外・static初期化・collection契約",
        "MathHelper全表と全メソッドのbit規範、full task/failure prefix"
      ]
    },
    {
      "id": "WORLD",
      "sourceFiles": 215,
      "document": "02-world-storage.md",
      "status": "partial",
      "gaps": [
        "全Chunk/World/WorldServer/WorldClientの個別member契約",
        "GenLayer/noise/biome/structure/照明の全branchと乱数消費",
        "全保存型の未知tag/default/失敗順と実Anvil適合"
      ]
    },
    {
      "id": "NETWORK",
      "sourceFiles": 139,
      "document": "03-network.md",
      "status": "partial",
      "gaps": [
        "111登録wire schemaをゲーム作用へ結ぶ全Play/Login/Status handler契約",
        "全DataWatcher index/entity type/enum/registry効果",
        "認証・サービス依存を含む全state/timeout/recoveryの契約"
      ]
    },
    {
      "id": "GAMEPLAY",
      "sourceFiles": 563,
      "document": "04-gameplay.md",
      "status": "partial",
      "gaps": [
        "全Block/Item constructor/property/metadata/overrideと全recipeの依存閉鎖",
        "全Entity/Mob AI/pathfinding/戦闘/effect/enchantsの計算順",
        "全TileEntity/redstone/village/command/scoreboard/stat/mapの個別処理"
      ]
    },
    {
      "id": "SERVER",
      "sourceFiles": 97,
      "document": "01-runtime.md",
      "status": "partial",
      "gaps": [
        "全起動・設定・権限・ban/whitelist/op・player lifecycle",
        "全scheduler/tracker/dimension/command/networkTick/保存の例外prefix"
      ]
    },
    {
      "id": "CLIENT",
      "sourceFiles": 476,
      "document": "05-client-resources.md",
      "status": "partial",
      "gaps": [
        "全GUIのlayout/input/遷移と全renderer/shader/particle/font計算",
        "全資産schema/model bake/texture/audio/reload処理",
        "入力eventから全runTickとphysics/predictionへの処理順"
      ]
    },
    {
      "id": "INTEGRATIONS",
      "sourceFiles": 34,
      "document": "05-client-resources.md",
      "status": "partial",
      "gaps": [
        "Realms/Twitchと外部serviceの全request/response/state/error契約",
        "platform/native/library選択と全launcher/proxy/sessionの契約"
      ]
    }
  ],
  "externalDependencies": [
    {
      "id": "JDK8",
      "status": "partial",
      "gaps": [
        "全到達API、各例外とconcurrency、locale/platform"
      ]
    },
    {
      "id": "Guava17",
      "status": "partial",
      "gaps": [
        "全到達collection/builder/view/iterator/cache/task"
      ]
    },
    {
      "id": "Gson224",
      "status": "partial",
      "gaps": [
        "全JSON serializer/deserializerの型・default・error"
      ]
    },
    {
      "id": "Netty4023",
      "status": "partial",
      "gaps": [
        "channel/buffer/pipeline/refcountと全state/error"
      ]
    },
    {
      "id": "Authlib1521",
      "status": "partial",
      "gaps": [
        "全profile/property/session/service契約"
      ]
    },
    {
      "id": "GraphicsAudioPlatform",
      "status": "partial",
      "gaps": [
        "LWJGL/OpenGL/OpenAL/Paulscode/ICU/JInput/platformの全到達意味"
      ]
    },
    {
      "id": "RealmsTwitchServices",
      "status": "partial",
      "gaps": [
        "歴史profileと現在service可用性の独立定義"
      ]
    }
  ],
  "explicitScopeExclusions": [],
  "reviews": []
}
```
<!-- coverage:end -->

## この版の次の確定対象

1. 全class/field/member/initializerに一意な契約IDと本文を割り当てる台帳。
2. 次のBlock/property/stateのconstructor、collection、transition graphを独立実装できる数式とfailure-prefixへ閉じる。
3. world generation、lighting、Mob/TE/redstone、全handler、全render/GUI/audioの不足を、メソッド単位で順に仕様化する。
4. 外部依存の到達closureと外部serviceのprofileを閉じる。
5. 第三者が原版を読まずに別実装を作って観測照合するレビュー。

上記は仕様作成の作業対象である。空handler、nativeの成功stub、推測の定数を入れる実装指示ではない。
