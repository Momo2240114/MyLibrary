# 攻撃スキルのテンポ調整

Animationの全尺を待つ構成から、振り抜き後のActionEndで戻り動作を切る構成へ変更しました。
速度は既存CurveComponent、演出は既存Animation Event／AttackTemplateを使用します。
スキル名による専用の実行分岐は追加していません。

## 最新の構成と接続

- PhantomRush: Combo_Attack_01_01 → 01_02 → 01_03 → Combo_Attack_02_01 → Combo_Attack_03_01 → SS_Sp_Skill1の6段。
- Breakthrough: 突進Start／Loop → Attack_Up_01_Seq → 上空へ4.5ワープ → 空中降下Start → 降下Loopで接地待ち → 着地Endの6段。
- RisingJudgment: 打ち上げ → 空中攻撃1 → SS_Attack2 → 空中攻撃2 → Combo_Attack_03_01 → SS_Attack3 → 降下Start → Loopで接地待ち → Endの9段。

中間攻撃はActionEndで接続します。PhaseのEndもAction Endへ設定しています。
PhantomRushの各接続時刻は0.50 / 0.46 / 0.43 / 0.48 / 0.46 / 1.10秒です。
RisingJudgmentの空中攻撃は0.48 / 0.50 / 0.44 / 0.48 / 0.54秒で次段へ進みます。
時間はAnimation内の時刻で、速度CurveやHitStopによって実際の経過秒数は変わります。

最終段の速度Curveを速め、VFXが先に見えてからHitBoxが有効になる構成です。
PhantomRushのフィニッシュHitBoxは0.86～1.04秒、BreakthroughとRisingJudgmentの着地HitBoxは0.24～0.44秒です。
最終段の接続はPhantomRushが1.10秒、着地フィニッシュは0.56秒です。

## Dodgeと同じモデル残像

既存のAfterimageComponentのAfterimageStart / AfterimageStop / AfterimageBurstを使用します。
攻撃Phaseの開始で生成を始め、退出時に止め、フィニッシュではBurstも追加します。
生成済みの残像は既存の寿命で消えます。Dodge用の描画・色・枚数・寿命設定そのものは変更していません。
SkillEffect.StopTrailsをCleanupActionへ設定し、被弾・キャンセル・通常終了でも武器Trailと残像生成を停止します。
ワープ中の移動も既存の残像サンプリングへ渡します。新しい残像システムは作っていません。

## モーションと迫力

PhantomRushは連続Comboクリップを維持し、速度Curveで振り抜き前後の緩急を付けました。
1～3段目へ剣閃を配置し、最後のMeshVFXを少し大きくしています。

Breakthroughの3段目はAttack_Up_01_Seq（Index 44）の切り上げです。
その後4段目で高さ4.5を加えるワープを行い、Attack_Air_to_Floor_01_Start_Seq（Index 37）で降下・接地待ち、
5段目はAttack_Air_to_Floor_01_End_Seq（Index 35）で着地フィニッシュへつなぎます。
突進Start／Loopは維持し、空中フィニッシュには接地待ち用のLoopを追加しています。
通常攻撃のStateやイベントそのものは変更していません。

フィニッシュ用AttackTemplateのHitStopを0.055秒／Scale 0.25、CameraShakeを0.22秒、
位置強度0.075、回転強度0.60へ変更しました。Damage、KnockBack、Cooldownは維持しています。

## Curveの使い方

Data/Curveへ次の11個のCurveTrackを追加し、PlayerModelの既存CurveComponentへ登録しています。
既存のTrack 0は維持しています。

- PhantomRush.ComboSpeed.json
- PhantomRush.FinishSpeed.json
- Breakthrough.StartSpeed.json
- Breakthrough.FinishSpeed.json
- Breakthrough.Acceleration.json
- Breakthrough.Rush.json
- Breakthrough.Brake.json
- RisingJudgment.LaunchSpeed.json
- RisingJudgment.AirSpeed.json
- RisingJudgment.DiveSpeed.json
- RisingJudgment.LandingSpeed.json

AnimationのSpeed Curveは再生速度そのものです。基準Speedへの倍率ではありません。
X軸は元クリップ全体の正規化時間、Y軸は再生速度です。
TrackがないActorではStateの基準Speedへ戻り、0による再生停止を防ぎます。

Forward Speed CurveはPhaseのForwardSpeedへ掛ける倍率です。
時間指定LoopではPhase経過／Duration、その他ではAnimationの正規化時間をX軸に使用します。
未設定・Track不足では元のForwardSpeedを使用します。

BreakthroughはStartの基準速度16へAcceleration、Loopの基準速度30へRush、
剣フィニッシュの基準速度30へBrakeを掛けます。Brakeは剣の接触付近で0へ落とします。
新しい物理・MotionWarp処理は作らず、既存CharacterMovementへ予約します。

Skill WorkspaceのForward Speed Curve／Movement Curvesで移動Curveを指定できます。
Curveの値は既存Curve Editorで編集し、Sceneを保存してください。
単体Curve Assetは登録済みTrackと同じ初期値です。Assetだけを外部編集した場合は、
CurveComponent側でReloadしてからSceneへ保存してください。

## 今回の変更ファイル

- Code/Engine/GamePlay/Combat/SkillDefinition.h / .cpp
- Code/Engine/GamePlay/Combat/SkillExecution.cpp
- Code/Engine/Graphics/Resource/Animation/ModelAnimator.cpp
- Code/Editor/Workspace/SkillWorkspaceWindow.cpp
- Data/Skills/PlayerSkills.json
- Data/Scenes/MainScene/Objects/PlayerModel.json（CurveComponentのみ）
- Data/Animation/StateData/Player/States/PhantomRush_01～06.json
- Data/Animation/StateData/Player/States/Breakthrough_01～05.json
- Data/Animation/StateData/Player/States/RisingJudgment_01～09.json
- Data/AttackTemplate/SkillPrototypeFinisher.json
- Data/Skills/PrototypeSkills.md、SkillWorkspace.md

追加は上記11個のCurve Assetと、この説明書です。
新規C++ファイルはありません。既存project／Filter登録を維持しています。
Scene、Weapon、Cameraなどの既存の調整値は、この依頼に必要なCurve登録以外変更していません。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

Warp HeightはPhase開始時に現在のActorのY座標へ加える値です。既存Colliderの外部Transform変更検出でPhysXへ同期します。Breakthroughの降下速度は-18、最短接地待ちは0.18秒、接地待ちの上限は4秒です。

追加した専用StateはBreakthrough_04 / 05、PhantomRush_05 / 06、RisingJudgment_07 / 08 / 09です。WarpHeightの読み書き・Editor、既存残像停止の汎用Action、Owner名付きActionの解決を既存ソースへ追加しました。新しいC++ファイルはありません。

## Skill C入力と着地待ちの修正

KeyConfigのPlayer.SkillC（Tキー）を単独入力からSlot2へ接続しました。L1 + ○の既存経路は維持しています。
Breakthroughは4段目Startから5段目のLoopへ進み、接地後だけ6段目Endへ進みます。
RisingJudgmentも8段目LoopのEndをGROUND_CONTACTに固定し、接地後だけ9段目Endへ進みます。
両方のLoopでは降下速度を一時的に-32、最短待機0.05秒、最大待機4秒へ設定しました。時間上限ではスキルを中断し、未接地のEndを再生しません。
Endには地面の大きなPlayerKickShockwave_AquaとKickImpact_Aqua Particleを追加しています。攻撃は従来どおりEffectより少し後の0.24秒からです。
追加StateはBreakthrough_06.jsonです。既存Breakthrough_05.jsonはLoopを担当します。

## 上空の溜めと重いフィニッシュ

WarpはAnimationのChangeStateより先に適用します。降下StartはAnimation終了まで再生し、縦速度0で上空に保持します。
DiveSpeedは構え部分を0.3倍付近まで落とし、LoopもSkill.DiveLoopPoseで姿勢の溜めを作ります。
Loopの縦速度にはSkill.DiveDescentを掛け、Phase開始から0.30秒は降下せず、その後0.58秒までに急降下へ加速します。
このCurveのX軸はPhase経過秒です。AnimationのLoopが折り返しても、移動の溜めを繰り返しません。
Loopの最短時間は0.45秒。接地が連続0.05秒成立してからだけEndへ進みます。最大5秒では中断します。
EditorのVertical Speed Curve (seconds)から指定できます。未設定では従来の縦速度を使用します。

最終段は一様な低速ではなく、構えを溜め、振り抜きを加速し、命中付近で姿勢を止め、重い戻りへつなぐ速度Curveです。
PhantomRushの最終接続時刻は1.65秒、着地フィニッシュは0.90秒へ伸ばしました。
フィニッシュHitStopは0.09秒／Scale 0.15、CameraShakeは0.28秒、位置強度0.095、回転強度0.75です。
DamageとCooldownは変更していません。実機の見た目は未確認です。

今回追加したCurveはSkill.DiveDescent.jsonとSkill.DiveLoopPose.jsonです。
変更ソースはSkillDefinition.h/.cpp、SkillExecution.h/.cpp、SkillWorkspaceWindow.cpp。
変更データはPlayerSkills、PlayerModelのCurve、降下Start/Loop・最終段の専用State、既存4 Curve、SkillPrototypeFinisherです。

現在登録されている全スキルの最終段へ重いテンポを適用しました。PhantomRushはFinishSpeed、BreakthroughとRisingJudgmentはLandingSpeed、HealはSkill.HeavyCastを使用します。Healの回復量・Cooldownは維持し、打撃用のHitStop／CameraShakeは追加していません。

## フィニッシュ低速区間の緩和

スローに見える長い低速区間を短縮する方向でCurve速度を上げました。PhantomRushの最終段の最低速度は1.05、着地Endは0.95、Healは1.0、空中Startは0.75です。
振り抜きの加速と短い命中の溜めは残し、戻りの速度も上げています。HitStopは0.06秒／Scale 0.25へ戻しました。CameraShake、HitBox時刻、接続時刻、接地待ち、Damage、Cooldown、Dodgeの残像設定は維持しています。

## 降下Loopの残像停止

Breakthroughの5段目とRisingJudgmentの8段目では、Phase開始時にAfterimageStopを呼び、Loopの開始イベントでAfterimageClearを呼びます。Loopには残像開始・Burstイベントを置きません。落下中の生成と直前の残像を止め、連撃中・着地Endの残像は維持しています。

降下Loopの基準縦速度をBreakthrough／RisingJudgmentとも-42へ変更しました。最初の0.30秒の溜めと既存加速Curve、接地確認後のEnd接続は維持しています。

## 残像を外して判定付きEffectへ変更

攻撃スキルの全StateからAfterimageStart／AfterimageBurstを外しました。開始時に残像を停止・消去します。DodgeのAfterimageComponentや設定は変更していません。
連撃、空中斬撃、切り上げ、剣フィニッシュの仮AttackTemplateへ短寿命の判定付きMeshVFXを追加しました。
武器判定とEffect判定は同じ攻撃Instance／HitGroupを共有し、単一Hit・最大1回を維持します。
着地EndはSkillPrototypeLandingImpactを使用します。大きなPlayerKickShockwave_Aqua、KickImpact_Aqua Particle、Sphere範囲判定を一つのRuntime Effectから生成します。
EffectのXZスケール3、判定のローカル半径1.4、判定有効0.025～0.20秒、寿命0.65秒は仮値です。見た目と実際の範囲は実機で調整してください。
着地EndのAnimation時刻0.20秒に生成し、Endの前には発生しません。着地用の武器HitBoxは無効にし、範囲判定へ置き換えました。
変更は既存AttackTemplateのattackEffectsとスキルStateのイベントのみで、新しい描画・判定システムは追加していません。

## 地上突進と盾攻撃への再構成

BreakthroughはCharge_Run_Attack_Start_Seq → Charge_Run_Attack_Loop_Seq → Run_Attack_02_Seq（Index 221）の3段へ変更しました。
打ち上げ・ワープ・降下は実行順に含めません。旧4～6段目のAssetは削除せず、未使用として残しています。
Shield ObjectへGuardColliderと別のHitBoxを追加しました。ActionPrefixはShield、既存のShiledTop／ShiledMiddleノードへ追従します。
SkillPrototypeShieldRushは突進Start／Loopと締めの盾打撃で使用します。締めでは盾打撃の後に剣の判定付き斬撃も発生します。

PhaseのEnableGuard／PreventFlinchはStart／Loopだけtrueです。Guardは通常Guardとして有効化し、JustGuardにはしません。
ひるみ抑制は既存Runtime Armorを一時的に上げ、HitStateへの強制遷移を抑えます。Guardを通過した攻撃のHPダメージと死亡処理は維持します。
3段目へ入る時と、終了・中断時には開始前のGuard受付・JustGuard・Armor設定へ戻します。
各PhaseのExitActionにはSkillEffect.StopShieldAttackを指定し、中断時にも盾HitBoxを閉じます。
防御データは共通SkillPhaseへ保存しますが、反映処理はPlayerSkillStateが担当します。Skill名による分岐はありません。
Skill WorkspaceのEnable Guard／Prevent Flinchで編集できます。

変更ソース: SkillDefinition.h/.cpp、PlayerSkillState.h/.cpp、PlayerControllerComponent.h/.cpp、SkillWorkspaceWindow.cpp。
変更データ: PlayerSkills.json、Shield.json、PlayerModelのCurve登録、Breakthrough_01～03.json。
追加Asset: SkillPrototypeShieldRush.json、Breakthrough.RunFinishSpeed.json。新しいC++ファイルはありません。
ビルドおよび実行確認は行っていません。静的確認のみ実施しました。
