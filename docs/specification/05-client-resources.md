# クライアント、リソース、描画と操作

本章は[総合仕様](README.md)のclient契約を具体化する。ユーザーが用意した同じ資産を入力として検証する。MinecraftのPNG、音声、モデル、font、翻訳データ自体を配布しない。描画の全計算・全GUIの操作を本文だけで作れるところまでは未充足である。

## CL-01 起動とsession

`client.main.Main`は未知optionを許すparserを用いる。accessTokenとversionは必須引数、server/UUID/assets/resourcePack等は別optional引数である。versionが1.8.9であることとMCP開発launcherが渡す表示versionは区別する。

| option | 初期値／意味 |
|---|---|
| port | 25565 |
| gameDir | 現在directory |
| width / height | 854 / 480 |
| proxyPort | 8080、proxyHostがある場合のSOCKS接続 |
| username | Playerにsystem time %1000を連結 |
| userProperties / profileProperties | 空JSON object |
| userType | legacy |
| assetsDir / resourcePackDir | 未指定ならgameDirのassets／resourcepacks |
| demo / fullscreen / checkGlErrors | flag presence |

sessionのprofile ID、name、accessToken、userTypeとproperty mapは別field。offline name UUID、online認証済みprofile、launch sessionを混ぜない。外部launcher／account providerがcredentialsを渡す境界は原版ゲームbodyと分ける。secret値を例示ファイル・spec witness・公開logへ書かない。

提供`Start`はdevelopment defaultsを先頭へ連結してMainへ渡すhelperである。concatは最初の配列のruntime component typeを保つcopyOfのあと後半をarraycopyする。この開発helperのtoken0をonline認証の仕様にしない。

Mainの全proxy error、Authenticator設置条件、JSON property serializer、shutdown hook、server接続へのhandoffは未充足である。一般的に正しいproxy設定へ元条件を修正しない。

## CL-02 ResourceLocation

文字列constructorは最初のcolonを探す。colonがあればpathはcolonの後ろ。domainをcolon前へ設定するのはindex>1のときだけで、index0または1はdefault domainになる。colonなしもdefault。default domainはminecraft、domainはJavaのlocale依存toLowerCase、pathはNULLを許さず、その大小文字を維持する。

例: `stone`→`minecraft:stone`、`:stone`→`minecraft:stone`、`a:stone`→`minecraft:stone`、`ab:Stone`→`ab:Stone`。modern版の厳しいnamespace/path文字集合をこのconstructorへ加えない。equalsはdomainとpathのString.equals、hashは31×domainHash+pathHash。

ModelResourceLocationは最初の`#`でvariantを分ける。prefixを短縮する条件もindex>1。variantが空ならnormal、非空ならlocale依存lowercase。`ResourceLocation`のequalsと派生のequalsの型判定差も原版どおり保持する。識別子をCの大文字小文字無視mapへ入れない。

## CL-03 pack優先順とreload

SimpleReloadableResourceManagerはdomainごとのFallbackResourceManager、reload listenerのlist、domainのLinkedHashSetを持つ。reloadは既存domain managerとdomain setをclearし、指定pack listの先頭から登録、そのあとlistenerを登録順に通知する。listener listはreloadごとにclearしない。新listenerのregister時はappendのあと直ちに一回通知する。

getResourceはpack listの後ろから探す。本文resourceと`path.mcmeta`を独立に探し、先に見つかった上位packのmetadataを保持し、最初の本文を返す。従ってmetadataと本文は別packから来ることがある。domainがない場合・本文がない場合はFileNotFoundException。

getAllResourcesはpack listの先頭から本文があるpackを列挙し、それぞれのpack自身のmetadataを付ける。上位metadataを全entryへ使わない。結果なしは例外。取得したstreamのcloseとlistener例外時のprefixはconsumer側の契約に属する。zip／folder／default／server packの全sanitization、download hash、cache、reload pipelineは不足として残す。

## CL-04 language

Localeはlanguage listの順、domain集合の順、getAllResourcesの順で`lang/<language>.lang`を読む。propertiesを最初にclearし、後から同じkeyを読んだ値で置換する。textは通常UTF-8。空行と最初の文字が`#`の行を無視し、最初の`=`で二つに分ける。key/value全体をtrimする処理を追加しない。

数値formatの`%(数字$)?[数字とdot]*[df]`を、同じposition指定を持つstring formatへ置換する。key欠落はkey自身を返す。formatはJava String.formatを用い、IllegalFormatException時はformat errorのprefixと元のtemplateを返す。ICU shaping／bidi、全locale、chat component translationsは別契約である。

unicode判定は全valueのUTF-16 unit総数と、値256以上のunit数を数え、そのfloat比率をdoubleへ昇格して0.1と比較する。空集合の0/0を特別な割合0へ置換しない。使用するfont binary自体はユーザー資産である。

## CL-05 GUI scale

ScaledResolutionはdisplayWidth/HeightからscaleFactor=1で開始する。guiScale0は候補上限1000として扱う。次のscaleでも幅320以上かつ高さ240以上になり、現在scaleが上限未満ならincrementを繰返す。

unicodeでscaleが奇数、かつ1でない場合だけscaleを1減らす。doubleのscaledWidth/Heightはpixel寸法÷scale、整数結果はMathHelper.ceiling_double_int。物理pixel、double GUI寸法、整数GUI寸法を区別する。hit testは原版のmouse座標変換を使い、描画だけ拡大してclick位置を放置しない。

## CL-06 KeyBindingと入力

KeyBindingのpressedとpressTimeは別の状態。setKeyBindStateはheld boolean、onTickはpressTimeを増やす。isPressedはpressTime0ならfalse、それ以外なら一回減らしてtrue。キーdownのpoll一つへ統合すると一frame中の複数pressを失う。keyCode0はonTick/state更新から無視される。

constructorはdescription、current keyCode、default keyCode、categoryをこの順で設定し、global list、int→binding map、category setへ順に登録する。setKeyCodeだけではhashを再構築しない。resetはmapをclearしてlist順に登録し直す。各unpressはpressTime=0、pressed=falseの順。重複codeとcategory/nameのlocale sortも同じcollection契約を使う。

| 操作 | 原版LWJGL key code |
|---|---|
| forward / left / back / right | 17 / 30 / 31 / 32 |
| jump / sneak / sprint | 57 / 42 / 29 |
| inventory / drop / chat / player list | 18 / 16 / 20 / 15 |
| command / screenshot / perspective / fullscreen | 53 / 60 / 63 / 87 |
| attack / use / pick | -100 / -99 / -98 |
| hotbar1..9 | 2..10 |

mouse codeはOSのVK番号ではない。mouse button indexと負codeへの変換を明示したbackendを持つ。mouseSensitivity初期値0.5F、invertMouse false、FOV70.0F、framerate limit120、vsync true。smooth camera、raw eventの順、GUI inputを許す条件、focus復帰、scroll、sprint/flyの二度押し、KeyBinding pressの消費順は未充足。

## CL-07 screenと操作状態

画面遷移はcurrentScreen、focus、mouse grab、worldの有無、playerの生死、connectionを同時に扱う。menu、singleplayer選択／作成、multiplayer／server list、接続待機、disconnect、pause、inventory、container、chat、death、credits、options、resource packs、Realms等を対象から外さない。

各screenの仕様には初期化・resize・tick・input・draw・close、buttonのenable条件、keyboard repeat、escape、tab、text input、clipboard、selected item、scroll、tooltip、loading/errorを含める。原版のlabelはユーザーのlanguage資産から解決し、公開の日本語仕様へ元の翻訳ファイルを丸ごとコピーしない。

Container clickのゲーム効果は[ゲーム処理](04-gameplay.md)。clientは予測とC0E、server側結果とS32、拒否後再同期を扱う。画面を閉じたときcursor／craft inputのdropはゲーム契約であり、表示componentの消滅だけで完了しない。全screenの座標／tab／render分岐は未充足。

## CL-08 block／item model JSON

ModelBlock deserializerはelementsのlistとparent名の二つを調べる。両方ない、または両方ある場合はJsonParseException。texturesはstring map、ambientocclusionはdefault true、displayはitem camera transform。parent chain、textureの`#`参照、builtin model、cycleは独立の解決契約。

BlockPartのfrom/toは3要素float、各座標[-16,32]。rotation originは1/16へscale、axisはx/y/z、angleは0/±22.5/±45、rescaleはdefault false。facesはEnumFacingでkeyを解釈し、0faceは拒否。shadeはdefault true、存在時にはboolean型を検査する。

faceはtexture、cullface、tintindex、UVを持つ。UV指定は4個、rotationは0/90/180/270のみ。UVなしのfaceは向きとfrom/toから補われるので、全faceへ同じUVを入れない。16単位のmodel座標、blockworld座標、atlas UV、camera transformを区別する。

ItemTransformVec3fはrotation default(0,0,0)、translation default(0,0,0)、scale default(1,1,1)。各vectorは3個のfloat。translationを1/16倍してから各axisを[-1.5,1.5]へclamp、scaleは各axisを[-4,4]へclampする。rotationの単位とmatrix適用順はcamera transform側へ結ぶ。親からのdefault vectorを別物へ変更するcopy/alias差も記録する。

本版はJSON fieldの一部とfailureを確定した段階。全face vertex winding、UVlock、normal、shade、AO、weighted variantsのselection、item generated layer、perspective transform、builtin entity modelを再実装する全formulaは未充足である。BlockModelShapesのstate→model cacheはIdentityHashMapであり、metadataだけをkeyとするcacheへ置換しない。

## CL-09 texture animation

animation metadataはframetime default1、width/height default-1、interpolate defaultfalse。default値以外の寸法は正のint範囲。framesは整数indexまたはobject(index,time)。objectのindexは0以上、timeが明示された場合は1以上、省略は-1の継承指定として保持する。

実画像の寸法、mipmap、frame抽出、順序、texture upload、interpolation、clockはTextureAtlasSprite／TextureMapの別契約。metadata parserのindex checkだけで画像内範囲のcheckを代用しない。PNG pixel自体と`.mcmeta`ファイル内容はユーザー資産として入力する。

## CL-10 描画pipeline

描画はTimerのpartial tickから位置／角度を補間し、camera、frustum、world、entity、tile entity、particle、weather、hand、GUI、framebuffer presentを順に処理する。第1人称／第3人称、inside-opaque、water/lava fog、dimension、sleep/death、FOV変化も観測対象。

render layerはSOLID、CUTOUT_MIPPED、CUTOUT、TRANSLUCENTを区別する。透過geometryの順、alpha/depth/cull/blend状態、lightmap、AO、entity shadow、model tint、damage overlay、glint、map textureを単色cubeの描画と同一視しない。chunk meshingとuploadにはrender thread境界がある。

bit／pixel完全比較のために、window pixel寸法、GUI scale、OpenGL機能、VBO／framebuffer／mipmap、graphics option、resource pack、locale、GPU/backendをfixture条件として固定する。許容するplatform差は、具体的なchannel・位置・誤差を適合性profileへ列挙する。本版では一律pixel toleranceを定義していない。

全RenderGlobal、EntityRenderer、RenderChunk、BlockFluidRenderer、BlockModelRenderer、FaceBakery、FontRenderer、entity/TE renderer、shader program、particle rendererの計算契約は不足。クライアントの完成判定をWin32 windowが開くことに縮小しない。

## CL-11 音

sound registryとsound event、選択weight、volume/pitch、attenuation、streaming、repeat delay、listener position／orientation、category gain、pause/stopは別の状態を持つ。resource reload時の再登録と欠落soundの処理も必要。OpenAL／Paulscodeの対象版とcodecの呼出し意味を固定する。

本版は音pipelineの完了条件だけを定め、全sounds.json schema、event合成、SoundManager/Source lifecycleの具体的分岐は未充足。音声ファイルを生成したり原版のOGGを公開repositoryへ入れて解消しない。

## CL-12 外部依存版と統合

提供version manifestに含まれる主要依存を固定する。これはJARを公開する表ではない。Cで同じversionをそのまま採用する義務ではなく、必要な意味を置換する際の基準である。

| 基準依存 | 版／到達する責務 |
|---|---|
| Guava | 17.0、collections/cache/task/base |
| Gson | 2.2.4、JSON treeとserializer |
| Netty | netty-all4.0.23.Final、channel/buffer/pipeline。別にMojang netty1.7.7項目あり |
| Authlib | 1.5.21、profile/property/session |
| Realms | 1.7.59、外部serviceとGUI |
| LWJGL | 2.9.4 nightly20150209／2.9.2 nightly20140822。OS条件で選択するmanifest ruleがある |
| JInput／JUtils | 2.0.5／1.0.0 |
| Commons Lang/IO/Codec/Compress | 3.3.2／2.4／1.9／1.8.1 |
| ICU | icu4j-core-mojang51.2 |
| JOpt Simple | 4.6 |
| Apache HttpClient/Core | 4.3.3／4.3.2 |
| Log4j | 2.0-beta9 |
| Paulscode | SoundSystem20120107と提供codec/library群 |
| Twitch | 6.5、external-platform4.5 |

JNA／OSHI／logging／compression等を含むmanifest全条件・native選択・各library APIは個別契約へ閉じる必要がある。Realms／Twitchの現在のservice可用性と、1.8.9時点のclient動作は別のprofileとして記録する。提供資料だけでexternal serviceを再現する契約は未充足であり、無条件成功callbackへ置換しない。

## CL-13 クライアントの適合ケース

同一資産・解像度・localeで、起動→menu→world作成→保存→再開、status→login→join→respawn→dimension移動→disconnect、resource reload→欠落resource→復旧、resize→unicode scale、重複key→hash reset、pressの複数消費、各Container mode、mouse focus復帰を観測する。

render casesはopaque/cutout/liquid/translucent、model parent/texture cycle、empty/corrupt metadata、animation、全camera、shadow/light/fog、map、font formatting、particleとsoundを含む。現Cのbody geometryや基本fontでこれらのcase全体を適合済みと扱わない。
