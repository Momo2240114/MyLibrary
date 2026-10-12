# プレイヤー通常ガード改善報告

## 変更内容と原因

通常ガードの開始・解除をPlayerControllerのState遷移へ接続し、ガード成功時に8粒の控えめな火花Particleを出す構成へ変更しました。判定解除は見た目のブレンドから分離しています。

コード・データから確認した原因は次のとおりです。ゲームを起動して症状を再現した結果ではありません。

- Guardの入力解放時にControllerが通常Stateへ戻す経路がなく、グラフのBlock_Endと約0.883秒位置のActionEndに依存していました。
- Guard開始時にColliderを有効化する処理が通常Guard Stateに接続されていませんでした。GuardColliderはStartで無効になります。
- 防御は盾Colliderとの接触だけに依存し、本体へ先に届いたHitBoxが通常被弾へ抜ける経路がありました。
- GuardBreak属性は通常Guardを拒否して通常Hitへ進むため、専用のBlock_Hit_Breakリアクションへ接続されていませんでした。
- OnGuardSucceededからPlayerの通常ガードリアクションを再生する接続がありませんでした。

## 入力と状態遷移

Guard開始時に本体と子階層のGuardColliderを有効化します。入力解放はControllerのUpdateで検出し、判定とMeshVFXを閉じてそのフレームの処理内でIdle/Moveへ戻します。Block_EndやActionSequenceの終了を待ちません。通常の解除ブレンドは初期値0.08秒です。

Guard中の回避は既存のキャンセル規則とCooldownを使い、複合Skillより先に判定します。通常攻撃・強攻撃も既存RequestActionから派生し、EnterActionでガード判定・表示・反応のアニメーション制御を解除します。通常Guardでは受付Gateが開いており、通常ガードリアクション中のみ初期値0.04秒のキャンセル待ちがあります。入力解放はこの待ち時間を使いません。

Guardから攻撃・回避・JustGuardへ派生した後と、GuardBreak後は、押しっぱなしのGuardで直ちに再発動しないよう再受付フラグを使います。Guard入力の解放で再受付します。既存の入力割り当て、攻撃の先行入力、回避の無敵・JustDodge受付は維持しました。

通常Guard、GuardHit、GuardBreakは既存GUARD State内の実行フラグで区別し、新しいPLAYER_ACTION番号やComponentを増やしていません。

## 判定とJustGuard

CollisionStepは既存のJustDodge→JustGuard→通常Guard→通常Hitの順序を維持しています。本体接触からの防御補完でもJustGuardを先に試します。成立後は同一攻撃の通常Hitを抑止します。

GuardColliderへ正面の許容角度と本体接触の補完を追加しました。Playerの初期角度は全幅160度（左右80度）です。近接攻撃は攻撃者、飛翔体は現在位置の水平方向で判定します。旧データは360度・本体補完なしで復元し、Boss側の保存設定は変更していません。

通常ガード成功はHPダメージを発生させません。GuardBreakは通常Hitと同じ攻撃力・技倍率・HurtBox倍率から算出し、GuardColliderのGuardBreakDamageRateで調整します。既定値は1.0で従来の被弾量を維持し、0ならHPダメージをなくせます。HPは一度だけ適用し、通常HitReactionとの二重処理を避けます。攻撃側のHitStop設定もGuardBreakで引き継ぎます。

通常Guardの専用GuardBreak処理はGuard/Activeが有効な場合に接続します。Skillの防御Phaseは既存の被弾・ひるみ抑制経路を維持します。

攻撃にCanBeGuarded/canBeGuardedを追加しました。falseでは通常GuardとJustGuardの両方を拒否します。旧HitBox/AttackTemplateはtrueで復元します。既存GuardBreak属性も再利用し、武器・Slash・派生AttackEffectのHitBoxへ両属性を渡します。一時的なCanBeGuardedはAttackTemplate終了・中断で元の設定へ戻します。

同じ攻撃の重複Guardは既存の攻撃ID・HitGroup・Activation履歴で防ぎます。Playerが受付を閉じた後も、受け止め済みの同一Activationを通常Hitとして再適用しないようHitBoxへ対象を記録します。

JustGuardの既存専用入力、受付イベント、反応Component、ImpactEffect、反撃Tagを維持しました。通常Guardの成功ActionはJustGuard成功時には呼ばれず、新しい通常ガードリアクション・火花を重複再生しません。ParryからGuardへ移る既存のJustGuard解除条件は維持しています。

## リアクションと通常Hit

| 結果 | アニメーション | モデルIndex | クリップ時間 |
| --- | --- | --- | --- |
| 通常ガード | Block_Hit_Seq | 415 | 約0.833秒 |
| GuardBreak | Block_Hit_Break_Seq | 416 | 約1.050秒 |

モデル内の実在クリップを確認し、Playerのグラフ・StateAssets・インラインSceneデータへ登録しました。反応中だけグラフの上書きを止め、通常Guardは終了後にGuardのループ姿勢へ戻します。キーを離せば終了待ちをせず解除します。

GuardBreakは判定とMeshVFXを閉じ、移動・攻撃・回避を制限します。アニメーションイベント、再生位置、編集可能な復帰時間で終了します。初期の最小復帰時間は0.65秒、通知や素材が不足した場合の上限は2.5秒です。終了後はIdle/Moveへ戻り、Guardキーの再受付を必要とします。

反応用RootMotionの移動適用は無効で、既存の移動を二重に加えていません。終了通知はStateExit時に呼ばない設定にし、キャンセル先の行動を終了させないようにしています。

通常HitはAttackSystem/HitReactionComponent/PlayerHitStateの構造を維持しました。Controllerの被弾入口でHit/ImpactReactedを確認し、Armorで抑えた衝撃をHit Stateへ移さないよう補正しました。通常Guardの成功は通常Hitの通知・Particle・被弾アニメーションへ進みません。通常Hitの方向計算、RootMotion、ノックバック、空中被弾の処理は変更していません。致死のGuardBreakでは最新の衝撃情報を使って既存の被弾経路へ接続します。

## 盾MeshVFXと火花Particle

盾の既存メッシュ・マスクを利用した仮アセットPlayerGuardShieldEnergyを追加しました。薄い青色とFresnelを使い、専用シェーダーは追加していません。盾のGltfModelは維持し、既存の常時MeshVFX表示はScene上で無効にして、ガード中の所有インスタンスへ表示を接続しました。既存Componentと編集データは残しています。

ガード開始時にEffectManagerのPoolから一度だけ生成し、既存ShieldのAttachment Transformを親にして位置・回転へ追従します。ローカル位置・回転・スケールはControllerから調整できます。初期スケールは1.03です。解除・派生・GuardBreak・JustGuard成功・操作ロックでStopEffectし、Poolへ返します。

持続演出にはControllerへのweakな寿命参照を渡します。所有Componentの削除・無効化でもEffectManagerが解放します。停止時は所有者も確認し、Poolで再利用された別の演出を古い参照で止めません。Updateで毎フレーム新規生成しません。

PlayerGuardSparksは既存SwordHitのStreaksとマスクを利用し、1回のBurstを8粒、寿命を約0.16〜0.28秒へ抑えています。明るい金色から透明へ変化し、接触法線を基準に散らします。Manager側の寿命は0.35秒です。接触点が取得できなかった場合は盾位置へ出します。粒数、サイズ、速度、色はParticle Editorでアセットを調整できます。

## Editor・保存と復元

- PlayerController Inspector: 解除ブレンド、通常反応のキャンセル時間、GuardBreak復帰時間、上限時間、2種類の反応アニメーション名。
- 同Inspector: MeshVFXアセット選択、盾Object名、ローカル位置・回転・スケール、火花Particleアセットと寿命。
- GuardCollider Inspector: GuardAngle、GuardBodyContact、ReactToGuardBreak、GuardBreakDamageRate。
- AttackTemplate/HitBox/AttackEffect Inspector: ガード可能属性、GuardBreak属性。
- Animation Editor: 反応クリップとPlayer_GuardReactionEndイベント。
- Particle/MeshVFX Editor: 仮演出の粒数、色、マスク、明るさ等。

設定はGuardSettings等へSerialize/Deserializeし、旧JSONは既定値で復元します。反応中フラグ、再受付フラグ、Pool参照は保存しません。MainSceneとHumanBossTestの両方を更新しました。HumanBossHeavySingleは既存GuardBreak属性を有効にしました。

## 変更ファイル

既存ソース（新規C++ファイルなし）:

- Code/Game/Charcter/Player/PlayerControllerComponent.h
- Code/Game/Charcter/Player/PlayerControllerComponent.cpp
- Code/Game/Charcter/Player/PlayerActionState.cpp
- Code/Engine/Physics/GuardCollider.h
- Code/Engine/Physics/GuardCollider.cpp
- Code/Engine/Physics/HitBox.h
- Code/Engine/Physics/HitBox.cpp
- Code/Engine/Physics/CollisionStepSystem.cpp
- Code/Engine/GamePlay/System/GuardSystem.h
- Code/Engine/GamePlay/System/GuardSystem.cpp
- Code/Engine/GamePlay/System/JustGuardSystem.cpp
- Code/Engine/GamePlay/System/AttackSystem.h
- Code/Engine/GamePlay/System/AttackSystem.cpp
- Code/Engine/GamePlay/System/EffectManager.h
- Code/Engine/GamePlay/System/EffectManager.cpp
- Code/Engine/GamePlay/Combat/AttackTemplateManager.h
- Code/Engine/GamePlay/Combat/AttackTemplateManager.cpp
- Code/Engine/GamePlay/Combat/AttackTemplateRuntime.cpp
- Code/Engine/GamePlay/Combat/AttackEffectHitComponent.h
- Code/Engine/GamePlay/Combat/AttackEffectHitComponent.cpp
- Code/Engine/Graphics/Resource/Animation/AnimationState.h

既存データ:

- Data/Animation/GraphData/Player.json
- Data/Animation/StateData/Player/Player.json
- Data/Scenes/MainScene/Objects/PlayerModel.json
- Data/Scenes/MainScene/Objects/Shield.json
- Data/Scenes/HumanBossTest/Objects/PlayerModel.json
- Data/Scenes/HumanBossTest/Objects/Shield.json
- Data/AttackTemplate/HumanBossHeavySingle.json

新規データ:

- Data/Animation/StateData/Player/States/Block_Hit_Seq.json
- Data/Animation/StateData/Player/States/Block_Hit_Break_Seq.json
- Data/VFX/PlayerGuardShieldEnergy.json
- Data/Particle/PlayerGuardSparks.json

報告: PlayerGuardImplementation.md。

## コメント・Tracy・ファイル形式・Visual Studio登録

入力解除と判定の分離、方向判定、本体接触の補完、GuardBreakのダメージ処理、通常Hitとの分離、Poolの寿命と所有者確認、保存・復元へ日本語コメントを追加しました。

既存のBS_PROFILE_SCOPEを維持し、PlayerControllerのUpdate、Guard判定、ガード反応更新、Collider切り替え、MeshVFX開始、火花生成へ同じマクロで計測を追加しました。Getterへ無制限にZoneを追加していません。

対象ファイルはUTF-8、BOMなし、CRLFです。厳密なUTF-8読み込みと改行形式を静的に確認しました。Visual Studioの実際の警告確認は未実施です。

21個の既存ソースについて、.vcxproj登録と.vcxproj.filters登録を別々に確認しました。新規データは既存のStateData、VFX、Particleフォルダーへ配置し、StateAssets・Controller設定・EffectManagerから参照します。実ファイルの移動、Filter変更、新規C++の登録はありません。

## 静的確認と手動確認

静的に確認した内容:

- 入力処理がCollisionStepより前に実行される更新経路。
- 新規関数の宣言・定義、追加include、C++の括弧対応、既存APIとの接続。
- 2つの反応名・Indexとモデルの対応、4箇所のAnimationDataとStateAssetsの対応。
- ConditionOpの列挙順（Equal=0）と保存値の対応、グラフのガード条件、終了イベント、キャンセル・正常終了・中断の処理経路。
- 保存項目と復元項目、旧データの既定値、表示用フラグと実行用フラグの区別。
- Particle/MeshVFXと使用するメッシュ・マスクの参照。
- JustGuard優先、同一攻撃の重複抑止、GuardBreakのHP一回適用、通常Hit抑止。
- 持続Effectのweakな寿命管理、明示停止とPool再利用時の所有者確認。

手動で確認する必要がある内容:

- 押下・解放、Guardを押したままの通常攻撃・強攻撃・回避、複数同時入力。
- 正面・斜め・背後からの攻撃、盾を抜けて本体へ届く攻撃、飛翔体。
- 通常ガードのHP防御、Block_Hit、反応中の解放・キャンセル。
- GuardBreak、復帰時間、HP倍率、入力の再受付、ガード不能攻撃。
- JustGuardの専用演出・反撃Tagと通常Guardの非重複、Skill防御Phase、JustDodge。
- 盾へのMeshVFX追従、連続した出入り、所有Object/Component削除時の解放。
- 火花の接触位置・粒数・明るさ、通常Hitの方向と見た目。
- Editor保存・再読み込み、ビルド時の構文・文字コード警告。

これらの実際の挙動・演出は確認済みとは扱っていません。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
