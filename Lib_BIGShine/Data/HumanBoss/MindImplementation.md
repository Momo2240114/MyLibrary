# HumanBoss：観察と駆け引きの拡張

## 構造

Player State / Animation / Attack Event / Hit・Guard結果 → GameObjectManagerの観測値と履歴 → HumanBossのIntent → 既存BehaviorTreeの条件・目的内候補 → 既存Skill / ActionSequence。

入力ボタンを読まない。PlayerControllerは変更していない。Managerは各FrameのObject更新前に、前Frameまでに確定した状態を一度取得し、全AIへ同じ値を公開する。PlayerとBossのObjectList順による先読みの差を作らない。

## 追加ファイル

- Code/Engine/GamePlay/Core/Object/PlayerActionObservation.h
- Code/Engine/GamePlay/Core/Object/GameObjectManager.Observation.cpp
- Code/Game/AI/Human/HumanBossMind.h
- Code/Game/AI/Human/HumanBossAIComponent.Mind.cpp
- Data/AttackTemplate/HumanBossGuardBreak.json
- Data/HumanBoss/MindImplementation.md

## 変更した既存ファイル

- GameObjectManager.h / .cpp：観測API、Frame開始時の収集、Hit/Guard/JustGuard接触の記録、Scene Clearで履歴破棄。
- HumanBossAIComponent.h / .cpp：Intentと判断レイヤーの接続、反応予算、被弾復帰、追加行動、保存・Debug。
- HumanBossActionComponent.h / .cpp：通常Guard成功通知、段間隔延長。
- SkillExecution.h / .cpp：実行中のDURATION Phaseだけを延長するAPI。
- HitBox.h / .cpp、GuardCollider.cpp：GuardBreak属性と通常Guardの可否。
- AttackTemplateManager.h / .cpp、AttackTemplateRuntime.cpp：GuardBreakのGUI、保存・復元、武器HitBoxへの反映と終了時の解除。
- BIGShine.vcxproj / .filters：新規C++ファイル4点を既存のObject/Human Filterへ登録。
- Data/AI/BehaviorGraph/HumanBoss.json：軽反撃、Whiff Punish、Feint、Intent別Shadow分岐。
- Data/Animation/StateData/HumanBoss/HumanBoss.json：GuardBreak用State。
- Data/HumanBoss/HumanBoss.json、Data/Scenes/HumanBossTest/Objects/HumanBoss.json：Mind初期値。
- Data/HumanBoss/README.md、Data/Scenes/HumanBossTest/README.md：説明追記。

## Intent一覧

NONE / OBSERVE / PRESSURE / BAIT / PUNISH / DEFEND / REPOSITION / DISENGAGE / CHASE / BREAK_GUARD / PUNISH_DODGE / COUNTER_ATTACK。

使用可能な意図の重みを状況から作り、一つを選んでDecisionLockTimeだけ維持する。意図の選択後にBehaviorTreeがActionを選ぶ。Action実行中は終了監視だけを続ける。ReadErrorChance（標準15%）で様子見を選ぶ余地があり、Reaction枝にもNONEを残す。

## Player行動履歴と攻撃段階

Managerで直近10件、標準6秒の戦闘行動を保持する。Action/ComboIndex/Animation切替/攻撃Animationの再開始を識別する。Idleを挟んだ弱攻撃やDodgeも連続使用として数える。

Attack Frequency、Weak Attack Rate、Heavy Attack Rate、Guard Frequency、Dodge Frequency、Same Action Count、Consecutive Dodges、Dodge Directionを提供する。左右/前後はDodge Stateが公開したAnim/DodgeInputから取得する。接近/後退傾向はPlayer自身の実変位から計算し、Bossが動いた分をPlayerの癖として数えない。

攻撃イベントと武器TemplateのactiveStart/Endを使い、NONE / STARTUP / ACTIVE / RECOVERYを区別する。移動量の衝突掃引による厳密な未来予測は行っていない。Boss側の既存距離・方向・高さ判定を維持し、届くSTARTUPのみ防御判断へ渡す。

## Reaction DelayとBudget

標準Delayは0.18秒。Phase倍率1.0/0.85/0.75と、Dodge・Guard・JustGuardそれぞれの倍率を適用する。同じ攻撃の繰り返しでは予測による短縮を加えるが、最低0.06秒は残す。同じ攻撃について一度だけ判断の機会を設け、毎Frameの再抽選を避ける。

Budget最大3、Neutral中に毎秒0.7回復。Dodge 1、Guard 0.65、JustGuard 1.5を開始成功時だけ消費する。予算が不足するActionは候補外。ダメージ自体を無効にしたり、Budget不足を無敵で補ったりしない。

同じReactionは完全禁止からSameReactionPenalty（標準0.35）の重み低下へ変更し、連続回数分を累乗する。個別Cooldownと共通Reaction/Dodge Cooldownは維持する。JustGuardの標準Weightは3/7/9、Cooldownは6/6/5秒。連打の記憶でWeightが上がっても常に選ばれるわけではない。

## 空振り反撃とCounter

Managerは実HitだけでなくGuard/JustGuardも接触として数える。攻撃が一度は届きそうな予兆だった、PlayerがRECOVERYへ入った、接触がなかった、という条件で短いWhiff Windowを予約する。距離・CooldownとPunishChanceでWHIFF_PUNISH候補へつなぐ。遠距離の空振りへ防御反応を返さない。

通常Guardの実成功、または回避後のWhiffに対してLightCounterChanceでLIGHT_COUNTERを予約する。JustGuardの実成功は既存COUNTER_ATTACK（Parry）へつなぐ。予約はWhiffWindowと同じ短い秒数で期限切れになり、遠い相手をいつまでもCounterで追い続けない。

SPECIAL_COUNTERは既存の独立カテゴリを維持する。Execution_03_Seq / Execution_Target_03_Seqの二者同期、KnockDown、操作制限は未実装。

## Dodge Punish / Guard対策

Phase 2以降でDELAY_ATTACKを解禁する。専用行動は0.16秒の予兆→Attack1→0.24秒待機→Attack2。秒数は既存Action DefinitionsのPhase Durationで変更できる。

通常Combo中にDodgeを観測した場合も、確率で次のSS_Guard段間隔へDodgePunishDelay（標準0.1秒）を一度だけ追加する。実行中のコピーだけを延長し、保存データを変更しない。最大待機はMaximumComboDelayで制限する。攻撃方向を振り始めた後まで追尾させる処理は追加していない。

GuardBreakはPhase 2以降、Guard頻度と保持時間がしきい値以上で、Guard中の相手へ一度通常攻撃でPressureをかけた履歴がある場合に候補となる。即時発動や毎回の正解選択にしない。0.4秒の予兆後にGuardBreak攻撃を出す。通常Guardでは防げず、DodgeとJustGuardは対処手段として残す。

GuardBreak属性は旧Template/HitBoxでfalse。今回のTemplateは既存Sword HitBoxの経路へ接続した。通常攻撃Templateは変更していない。GuardBreak用Templateはダメージ倍率0.9、衝撃12で、既存HitReactionへ渡す。

## Observe / Bait / Feint

Observeは既存Strafe/待機Action。Baitは接近して攻撃距離で止まり、BaitWaitだけ相手の行動を待つ。Playerが攻撃すればReaction判断、動かなければ通常攻撃または低頻度Feintへ進む。

ATTACK_FEINTは攻撃判定のない予備動作→中止→短い待機。PlayerのDodgeが観測されればDodge Punish候補へ接続する。FeintChance、FeintCooldown、FeintRecentPenaltyとActionのCooldownを持たせた。

## 被弾とPhase

被弾のダメージ・ひるみ・吹き飛びは既存処理を維持する。短時間に2Hit以上受けた場合はHitEscapeRecovery（標準0.15秒）へ復帰待ちを短縮し、長い待ちを連打のたびに延ばし続けない。被弾後はRepositionを優先候補にできる。これは無敵や被ダメージ軽減ではない。Budget切れや読み負けでは引き続き被弾する。

Phase 1は観察を長めにし、Phase 2でDodge対策・GuardBreak・Feintを解禁。Phase 3は観察時間を短くし、Pressureの意図重みを上げる。反応のDelayもPhase別に変わるが、NONEと読み違いは残す。HP境界と行動のEnabled/Weight/Cooldownは既存Editorで調整する。

## Shadowとの接続・未実装

IntentのCHASEはSHADOW_SHIFT_APPROACH、DISENGAGE/REPOSITIONはBACK、PUNISH/BAITはSIDE/BEHINDの候補へつなぐ。使用可否・距離・Cooldown・個別Skillの存在で絞る。データ未設定の場合は通常Approach/Repositionへ戻る。

ShadowShift本体、ShadowShift Feintの演出、左右別の追尾攻撃、DragonShadowの実体、Arena大技、Execution同期は未実装。Dodge方向の履歴は記録・表示済みで、これらの追加Actionから参照できる。

## Editor / Debug

HumanBoss InspectorのMind Settingsで、反応時間、予測短縮、防御予算/回復/消費、同一防御ペナルティ、読み違い率、Decision Lock、Guard/Dodge判定しきい値、Counter/Punish/Bait/Feint確率、ディレイ秒数、被弾後復帰、Phase倍率を編集・保存できる。

既存Action Definitionsで全Actionの距離・Phase別Enabled/Weight/Cooldown・Skill Phaseを編集できる。BehaviorTree EditorでIntentに結び付く条件とActionを編集する。

DebugはIntent、Action、Last Reaction/連続回数、攻撃/弱/強/Guard/Dodge率、同一行動数、Dodge方向件数、Budget、Delay、Decision Lock、Whiff/Feint残り時間、接近/後退、Aggression、Confidence、Phase、選択理由を表示する。

Manager Player Observation DebugのOverride player perceptionを有効にすると、実際の入力やPlayer Stateを変えず、AIに渡すAction ID、攻撃段階、弱/強/Guard/Dodge、接触、頻度、攻撃経過、発生までの時間、保持時間、連続回数、Dodge方向を編集できる。Publish next simulated actionで次の行動世代を発行する。位置・生死は実値を維持する。

Force Intent Debug / Force Phase Debug / Reaction Budget Debug / Freeze Budget Debugは戦闘中だけの確認用。これらの上書きはSceneへ保存せず、Scene Load/ClearでManagerの知覚上書きも破棄する。

## 静的確認とプレイテスト

各STEPで宣言/定義、Managerの通知順、旧JSON省略時の既定値、Budget開始成功時の消費、非ゼロDelay、個別Cooldown、Phase終了/中断、TreeのLinkとAction名、GuardBreak保存・復元・終了時解除をコード上で確認する。新規C++は既存Filterへ登録し、実フォルダーは移動しない。日本語コメントと既存BS_PROFILE_SCOPEでManager観測/Mind更新を追跡する。

プレイテストでは以下を確認する。

1. 弱攻撃連打を記憶し、Dodge/Guard/低頻度Parryが変化するか。Budget切れでは押し込めるか。
2. 遠距離/逆方向の空振りへ防御せず、回避後の空振りRecoveryへ時々反撃するか。
3. Dodge連打で段間隔やDELAY_ATTACKが増え、振り始め後の攻撃は正しい回避で空振りするか。
4. Guardを続けるとPressureを挟んでGuardBreakが増え、予兆からDodge/JustGuardで対処できるか。
5. Observe/Bait/Feintが連発されず、Counter後に観察へ戻るか。
6. 被弾連打・死亡・Target消失・Scene再Loadで予算/終了待ち/Sequenceが不正に残らないか。
7. 各Debug上書きを切り替え、解除後は実際の観測に戻るか。保存・再読込では通常のMind設定だけ復元されるか。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

回避確認による段間の追加待機DodgePunishDelayの初期値は0.1秒。通常コンボの短い段間と、専用DELAY_ATTACKの0.24秒待機を区別し、振り自体は高速のままにする。

## 弱攻撃連打による行動封じへの対策

HumanBossActionComponentはSkill PhaseのPreventFlinchを参照し、振り始めから最後のAttackTemplate判定が閉じるまでFlinchArmor（初期値10）を適用する。予備動作、段間、最終段の終了硬直では元のArmorへ戻す。弱い衝撃によるひるみと吹き飛びだけを抑え、ダメージは通常通り受ける。衝撃値がArmorを超える強攻撃は中断できる。

1秒以内に2回ひるんだ場合、短い復帰待ちの後にREPOSITIONへ接続し、HitEscapeProtection（初期値0.6秒）の間は同じArmorで連打による再中断を抑える。新たな被弾で保護時間を延長する処理は、ひるみが実際に成立した場合だけ行う。死亡、演出ロック、行動中断では元のArmorへ復元する。

HumanBoss本体とHumanBossTestのHPを1000から12000へ調整し、HealthComponentとHitReactionComponentの値を合わせた。MainSceneのドラゴン（TestTarget）のHealthComponentはHP10000で、人型はその1.2倍とする。Playerの弱攻撃は現状100～150ダメージのため、以前のHPでは約7～10回で倒せる設定だった。

調整箇所はHumanBossActionComponentのFlinch Armor、AIのMind SettingsのHitEscapeProtection、Action DefinitionsのPrevent Flinch During Strike、HealthComponentのMaxHealth。弱攻撃の押し付けで攻撃が止まり続けないこと、強攻撃では中断できること、終了硬直に反撃できること、停止後に耐性が残らないことをプレイ時に確認する。

## RootMotionの素材別設定と停止表示

RootMotionのsourceNodeNameはStateの表示名ではなく、モデル内の元クリップ名で決める。SS系クリップは空欄でモデル既定のrootを使い、リターゲット素材はpelvisを指定する。HumanBossComboFinisherとHumanBossGuardBreakはSS_Attack3なので既定値を使う。SS_HitGround_Mediumは表示名にSSがあるが、使用クリップはBlock_Start_SeqとHit_Combat系なのでpelvisを使う。Parry、Counter、AttackTell、CombatWalkもpelvisを使う。単独Stateアセットとライブラリの設定を合わせ、移動用のRootMotion Scaleは従来の値を維持する。

Player死亡、Boss死亡、演出ロック、対象喪失では停止への移行時に一度だけSkillとBehaviorGraphを止め、ひるみ待ちを解除する。停止中に毎フレーム観察時間を再抽選しない。AI InspectorのAI statusとPlayer HPで停止理由を確認でき、対象が生存へ戻りロックも解除されれば通常更新へ戻る。

HumanBossActionComponentには現在PhaseのAnimation、終了条件、経過秒、Duration、Timeout、アニメーション時刻と長さを表示する。生存中に止まったように見える場合は、終了通知待ち、時間制限、素材不足、AI停止理由を区別して確認する。共通Skillの既存Timeoutと停止処理は維持する。
