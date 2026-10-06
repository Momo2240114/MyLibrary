# 第2形態・人型ボスAI基盤

## 実装範囲

HumanBossAIComponentは観察と行動選択、HumanBossActionComponentは共通SkillExecutionへの実行依頼を担当する。PlayerControllerやPlayerStateをボスへ追加する必要はない。

NEUTRAL → Player観察 → REACTION / OFFENSE / REPOSITION → 行動終了 → NEUTRAL。
実行中Actionがある間は次の行動を選ばない。攻撃終了後は距離調整時間を挟み、毎回0.5～1.5秒の観察時間を作る。観察中も攻撃予兆に反応できる。

SHADOW / DRAGON_SHADOW / SPECIALは共通Skillの要求先として登録済み。SPECIAL実行中も通常判断を停止し、正常終了後にNEUTRALへ戻る。現段階では外部Actionから要求する接続点で、これらの自動選択や実際の演出は未実装。

## 追加ファイル

- Code/Game/AI/Human/HumanBossAIComponent.h / .cpp
- Code/Game/AI/Human/HumanBossActionComponent.h / .cpp
- Data/Animation/StateData/HumanBoss/HumanBoss.json
- Data/HumanBoss/HumanBoss.json
- Data/HumanBoss/HumanBossWeapon.json
- Data/HumanBoss/HumanBossShield.json
- Data/HumanBoss/README.md

既存変更はBIGShine.vcxproj、BIGShine.vcxproj.filters、ComponentRegistry.cppへの登録のみ。作業開始前から存在したPlayer・Skill・Dragon・Sceneの変更は上書きしていない。

実ファイルは既存AIと同じCode/Game/AI配下のHumanへ配置。Visual StudioではCode\Game\Logics\AI\Human Filterへ別途登録。実ファイルの移動は行っていない。

## シーンへ配置する手順

既存MainSceneのSceneListやObject JSONは変更していない。配置用JSONは通常のGameObject保存形式。

1. Data/HumanBossのHumanBoss.json、HumanBossWeapon.json、HumanBossShield.jsonを、確認したいシーンのObjectsフォルダーへコピーする。
2. そのシーンのSceneList.jsonのObjectListへHumanBoss、HumanBossWeapon、HumanBossShieldをこの順番で追加する。別の保存済みシーンを用意して試す場合も、既存の床・PlayerModel・Cameraを含むシーンを使用する。
3. Editorでそのシーンを読み直す。HumanBossの初期位置は(-5, 0, -40)。床やプレイヤーの実際の位置に合わせて変更する。
4. HumanBossAIComponentのPlayer Objectは既存のPlayerModel。別名の場合は変更する。ボス本体へPlayerControllerComponentを追加しない。
5. AnimationControllerのAssetPathはHumanBoss。専用ライブラリを読み込み、Player用GraphAssetPathとStateAssetsを接続しない。
6. ボスの剣はHumanBossWeapon、盾はHumanBossShieldとしてボス階層へ接続済み。ModelAttachmentもHumanBossを参照する。剣HitBoxのActionPrefixはSwordのままなので、既存AttackTemplateがボス自身の子階層から解決できる。
7. 本体のCharacterCollider、HurtBox、GuardColliderと、カメラ候補用のLockOnTargetComponentは既存Componentを利用する。GuardColliderは初期無効、ボスのLayerはEnemy。HitReactionのDamageActionNameは既存Health_Hit、ReactionActionNameはHumanBossHit。

ボスHPは1000。通常攻撃と反撃は既存WeakAttack01/WeakAttack02を利用するため、ダメージ・衝撃・斬撃色はその設定を引き継ぐ。黒泥マテリアルや影演出の最終見た目は未調整。

## Player攻撃の検知

- PlayerControllerの現在Action/ComboIndex、現在AnimationState、Animation時刻を読む。
- ATTACKグループのActionについて、攻撃TemplateイベントまたはEnableHitBoxイベントの予兆時間と受付時間を調べる。
- 読み込み済みAttackTemplateにWeaponHitBoxがある場合は、そのactiveStartTime/activeEndTimeを加味する。
- 水平距離、Player正面との角度差、高さの差が許容内の場合だけ反応候補を作る。遠距離の空振りや逆方向の攻撃には反応しない。
- 到達距離と角度はAI用の調整値。実際の武器形状を使う掃引衝突予測ではない。Default Player Attack Reach / Half Angleと、Animation StateごとのReach Overridesを、武器や技の有効範囲に合わせて調整する。
- Action/コンボ段/Animation切替/攻撃Animation時刻の巻き戻りで攻撃の世代を更新する。同じ世代でNONEを選んだ後、同じ受付窓で再抽選しない。

16件の戦闘行動履歴、RecentPlayerAction、SameActionRepeatCount、GuardFrequency、DodgeFrequencyを保持し、BlackboardのHumanBossAIカテゴリへ公開する。反復行動に応じたParry率補正は未適用。

## 防御選択と成功通知

REACTION内だけで、BackDodge 40 / SideDodge 15 / Guard 20 / JustGuard 10 / NONE 15の重みを使用する。距離・Phase・Cooldown・素材・直前行動の条件を満たさない候補は除外するため、実際の確率は状況によって変わる。NONEは常に残す。

JustGuardは攻撃開始予定時刻の前後Just Guard Lead以内だけ候補になる。Phase 1では標準で無効、Phase 2/3で有効。Phase 1でも確認したい場合はJUST_GUARDとCOUNTER_ATTACKのPHASE_1 Enabledを有効にする。

回避は既存CharacterMovementへ速度を予約し、DodgeInputをローカル座標へ変換する。無敵は専用Animationの既存HurtBoxInvincibleOn/Offイベントで受付時間だけ開く。通常Guardは共通SkillのEnter/ExitAction、JustGuardはParry_L_Seq内の既存GuardColliderイベントを利用する。

JustGuardSystemが実際の接触を成立させてOwnerのOnJustGuardを呼んだ場合だけ成功を保存する。REACTION正常終了時に、その成功からCOUNTER_ATTACKを開始する。通常攻撃抽選ではCounterを選ばず、外部のHumanBoss_COUNTER_ATTACK要求も受け付けない。

側方回避の左右は現在の周回方向を使用する。壁・Arena中央を使う左右選択は未実装。

## Cooldownと連発防止

行動定義にPhase別Enabled / Weight / Cooldownと使用距離を保持する。成功した開始時だけCooldownと履歴を更新する。

Reactionには共通ReactionCooldown、Back/Side Dodgeには共通DodgeCooldown、通常攻撃には共通AttackCooldownも適用する。Counter、Guard、JustGuard、Shadow、DragonShadow、Specialは各行動定義のCooldownを使用する。Phase Reaction Cooldown Multipliersで反応性能を変更できる。

直前Action、直前Reaction、直前通常攻撃を別々に保持し、待機や距離調整を挟んでも同じ防御・同じ通常攻撃の連続選択を除外する。候補が一つしかないときも連発制限を解除せず、観察や距離調整を続ける。

## Action終了と中断

共通SkillExecutionがANIMATION_END / DURATION / ACTION_END / GROUND_CONTACTでPhaseを進める。ACTION_ENDではAnimation EventやSequence内からOwnerのActionEndを呼ぶ。複数PhaseではActionEndはそのPhaseの終了通知で、Skill全体の終了は最終Phase終了で確定する。

AIは共通実行のIsActiveとGetResultを読み、正常終了ならNEUTRALへ戻る。OFFENSE正常終了後はReposition Timeを挟む。Timeout・外部Animation変更などの異常終了ではCounterへ派生しない。

死亡、Player喪失/死亡、EnemyAILockedで実行を停止する。中断ではGuard/JustGuard/無敵窓を閉じ、攻撃Stateを抜けて残ったAnimation Eventの終了処理も呼ぶ。HumanBossHitはHitReactionの通常ダメージ処理を維持し、ひるみAnimationを再生してHit Recovery Durationだけ通常AIを休止する。

行動実行ComponentのUpdateではSkillを進めない。HumanBossAIのUpdateからTickを一度だけ呼ぶ。AIの標準UpdatePriorityは-20で、既存Movementへ先に予約する。

## HP Phase

HealthComponentの現在HP/最大HPから毎フレーム判定する。標準境界は70%と35%。Phaseが変わっても実行中Actionを中断せず、次の判断へPhase別設定を適用する。

## Animation

Data/Model/SwordPlayer/CombatGirls_Sword_Shield.gltfの実クリップを確認した。

- Combat Walk: Walk_Combat_Loop系列、index 350～359。
- Counter: Parry_Counter_Attack_Seq、index 201。
- 将来のExecution: Execution_03_Seq / Execution_Target_03_Seq、index 125 / 128。

専用ライブラリではPlayerの入力受付、コンボ通知、カメラ操作、Player用JustDodgeイベントを除き、AttackTemplateと防御受付だけを残した。DodgeとCombat WalkのRootMotion移動は無効にし、AIの速度予約と二重加算されないようにしている。Attackは既存のRootMotion方針を引き継ぐ。

Counterの武器判定は既存WeakAttack02を0.2～0.5秒のイベントへ接続した仮調整値。実際の振りに合わせてSequencerで調整する。Counter用Animation名HumanBossCounterは、確認済みindex 201を指すボス専用State名。

## 拡張方法

Action Definitions内の各カテゴリのEnabledを有効にし、必要なAnimation/Sequence/EnterAction/ExitAction/CleanupActionを指定する。共通SkillのPhasesで攻撃→待機→追撃を構成できる。待機PhaseにもIdleなど実在するAnimationを指定し、End=DURATIONで秒数を設定する。

- SHADOW: ShadowShift専用Component/Actionを追加し、SHADOW_SHIFT_BACK/SIDE/BEHIND/APPROACHのSkillから呼ぶ。復元後の0.2～0.5秒待機は独立Phaseにする。Enable Shadowと使用距離も設定する。
- DRAGON_SHADOW: 専用Emitter/Actionを用意し、DRAGON_HEAD_BREATH/BITE/CLAW/TAIL/WINGへ指定する。Enable Dragon Shadowを有効にする。
- SPECIAL: 専用SequenceをSPECIAL_ATTACKへ指定し、Enable Specialを有効にする。OwnerのHumanBoss_SPECIAL_ATTACKを呼ぶと通常判断が止まり、共通Skill終了後に復帰する。
- SPECIAL_COUNTER: Executionの二者同期、Playerの操作制限、KnockDownは専用Actionで実装して接続する。通常Counter成功経路とは独立したカテゴリ。

DELAY_ATTACKはDodgeFrequency、GUARD_BREAKはGuardFrequencyが0.3以上のときOFFENSE候補になる。現在は両方とも素材未設定・無効。ShadowShift本体、フェイント、DragonShadow本体、Arena大技、Execution同期、第一形態からの自動切替は未実装。

## Editor調整項目

戦闘距離、観察時間、歩行/Strafe/回避速度、反応予兆時間、攻撃到達範囲、NONEを含む反応Weight、共通Cooldown、ひるみ休止時間、HP境界、Phase別反応倍率、特殊カテゴリ使用可否、各ActionのPhase別Enabled/Weight/Cooldown、行動距離、共通SkillのPhase列とActionSequence接続。

Debug表示はPhase/State/Action/Last Action/Target Distance、Reaction/Attack/Dodge/Shadow Cooldown、Player攻撃検知理由、Player行動履歴、実行結果/素材不足。行動別の残りCooldownはAction Definitions内に表示する。

## コメント・Tracy・ファイル形式

日本語コメントは判断と実行の責務、予兆判定、NONEの再抽選抑止、履歴、終了/中断、座標変換、保存復元に追加した。既存のBS_PROFILE_SCOPEをStart/Update/ObservePlayer/SelectAction/BeginAction/Gui/Serialize/Deserializeと行動実行Tick/Begin/Stopで利用し、共通Skillの既存計測も維持した。

今回作成/変更したファイルはUTF-8 BOMなし・CRLF。ソースの厳密UTF-8読込、ローカルinclude、宣言と定義、JSON構造、Project/Filter登録、Action通知と中断経路を静的確認した。

## ビルド後の確認

1. 本体・剣・盾がボス自身の階層へ接続され、Playerと違う対象としてロックオンできるか。
2. Combat Walk/Strafeが注視を保ち、RootMotionで移動が二重にならないか。
3. WeakAttack01/02がPlayerへHitし、剣HitBoxが攻撃終了・被弾・死亡で閉じるか。
4. Playerが遠距離や逆方向へ空振りしても反応せず、近距離でもNONEが発生するか。
5. Back/Side Dodgeの方向と無敵受付が一致するか。壁付近の側方回避は今後調整が必要。
6. 通常Guard/JustGuardが実際の接触で成立し、Phase 2以降に成功時だけCounterへ接続するか。
7. Counterの振りと仮の判定時間を合わせられるか。
8. HP境界、行動Cooldown、観察時間、連発制限、ひるみ/死亡/演出ロックを確認する。
9. Editor保存・再読込で設定とSkill Phase列が復元されるか。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
