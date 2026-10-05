# 共通スキルとSkill Workspace

プレイヤーと敵は `SkillDefinition` と `SkillExecution` を共用します。
入力の解決はPlayerInputResolver、Playerの状態遷移はPlayerSkillState、
敵の意思決定とCooldown・Recoveryは既存AIへ残しています。
スキルごとにStateやタイムライン実行システムを増やす必要はありません。

## 作成の流れ

1. Editor上部のWorkspace Selectorで **Skill Workspace** を選択します。
2. Libraryで共有JSONを読み込み、Target ActorでAnimationControllerを持つ対象を選択します。
3. New SkillまたはCopyで定義を作り、各PhaseのActor Animationsから実在するStateを選択します。
4. Phasesタブのバーを横移動すると再生順を変更できます。幅を変えるとDuration終了へ切り替わります。
   Phaseは常に連続実行され、バーの移動で待機区間は追加されません。
   Animation End / Action Endの幅は表示用の推定値です。再生速度カーブやイベント待機時間は反映しません。
   フレーム目盛りは編集用で、Animationや演出のスクラブ再生は行いません。
5. Edit Animation Eventsで、選択Stateの既存Animation Sequencerを開きます。
   Attack / Trail / MeshVFX / Sound Templateと既存Actionを設定し、そのエディタでAnimationデータを保存します。
6. Action Sequenceタブで既存の時刻付きActionを編集します。
   PhaseのSequenceにはOwnerに登録されたSequence名を選択します。
   Enter Action / Exit Action / Cleanup ActionにはOwnerまたはGlobalの既存Actionを指定できます。
7. Player / AIタブで割り当て、Save & Apply Bindingを押します。
8. Save & Playで通常のRequestAction／ExecuteAIAction経路から再生します。
   入力・AI判断の禁止状態、Cooldown、キャンセル条件、Animation／Sequence不足では要求が拒否されます。
   Simulate SceneはScene全体を更新するため、実際の移動・攻撃・被弾も進みます。
   Stopは選択Actorの共通スキルを中断し、Scene更新を停止します。

LibraryのSaveは共通JSONだけを保存します。AIへの対応付け、ライブラリの参照先、
ActionSequenceなどActor側の変更は、別途Sceneを保存してください。
Animation Eventは既存Animationエディタ側の保存先へ保存します。
JSONへの未対応項目は保持し、Phaseの並べ替えでも対応する項目を一緒に移動します。

## プレイヤーへの接続

DefinitionのPlayer SlotをSkillA / SkillB / SkillCへ割り当てます。
同じスロットへ複数定義を割り当てることはできません。
Input BindingsでModifier Hold、Trigger Action、Priorityを編集できます。
入力の詳細と初期サンプルは [README.md](README.md) を参照してください。

`Action` が省略・空・`None`の定義は敵専用または未割り当てとしてPlayerの登録対象から外れます。
PlayerのA/B/Cは既存の識別子を維持し、共通実行への接続だけをPlayerSkillStateが担当します。

## AIへの接続

汎用敵では既存 `AIActionComponent` にAI Action NameとSkill名を対応付けます。
WorkspaceからComponentを追加することもできます。
対応は `Actions[].SkillName`、参照先は `SkillDefinitionsPath` としてSceneへ保存します。
BehaviorGraphは従来どおり登録されたAI Actionを呼び、既存AI BlackboardのActionEndedを待ちます。
SkillNameが空なら従来のAnimation State／Trigger／Execute Action経路を使用します。

Dragonでは既存Dragon ActionへSkill名を対応付けます。
`ActionDefinitions[].SkillName` と `SkillDefinitionsPath` を保存します。
既存のRequestAction、BehaviorGraph、重み付き攻撃選択、Cooldown、Recovery、
空中／地上の遷移はDragon側が引き続き管理します。
SkillNameを指定したActionのAnimation・演出進行は共通Phaseへ切り替わるため、
以前のSequence／IK／VFXが必要な場合はPhaseのSequenceやEnter Actionなどへ設定します。
旧設定項目は保持され、SkillNameを空にすると以前の実行経路へ戻ります。

同じActorで同時に複数のSkillExecutionは開始できません。
スキルのAnimation Stateは対象ActorのState名で解決します。
プレイヤー用と敵用でState名が異なる場合は、それぞれのDefinitionを作成してください。
共通DefinitionのCooldownは予約項目です。実際のDragonのCooldownは既存Action設定を使用します。

## 終了・中断

ANIMATION_END、DURATION、ACTION_ENDの3種類でPhaseを進めます。
ACTION_ENDは既存Player ActionEnd／汎用AI NotifyActionEnd／Dragon AIActionEnd・SequencerActionEndから通知します。
Phase終了通知が届いても、最後のPhaseまではPlayer／AIの行動全体を完了扱いにしません。
Animation EndのループStateは最初の時間折り返しで終了を判定します。
MaximumDuration超過や外部Animation変更では中断し、既存の終了経路へ戻ります。

Phase終了・中断では、そのPhaseが開始したActionSequenceの未実行予約だけを停止します。
既に発生したHitBox、Particle、MotionWarpなどは自動的には巻き戻しません。
必要な停止ActionをExit Action／Cleanup Actionへ指定し、Animation Eventには必要に応じて
callEndOnStateExitを設定してください。所有Componentの破棄中は予約停止と制御権解放だけを行い、
破棄中のComponentへ参照を戻す演出Actionは呼びません。

Player / AIタブは `Skill` BlackboardのName、Active、Phase、PhaseTime、Resultを表示します。
Resultは0=実行中／未実行、1=正常終了、2=中断、3=Phase開始失敗、4=時間超過、5=外部Animation変更です。

## 今回追加・変更したファイル

- 共通実行: `Code/Engine/GamePlay/Combat/SkillDefinition.h/.cpp`、`SkillExecution.h/.cpp`
- 作成画面: `Code/Editor/Workspace/SkillWorkspaceWindow.h/.cpp`
- 画面登録: `Code/Engine/GamePlay/Scene/SceneBase.cpp`
- AI接続: `Code/Engine/GamePlay/AI/AIActionComponent.h/.cpp`、`Code/Game/AI/Dragon/DragonAIComponent.h/.cpp`
- Player接続: `Code/Game/Charcter/Player/PlayerSkillDefinition.h/.cpp`、`PlayerSkillState.h/.cpp`、`PlayerControllerComponent.h/.cpp`
- Editorからの表示確認と共通制御権の参照: `Code/Engine/Graphics/Resource/Animation/AnimationControllerComponent.h`
- 登録: `BIGShine.vcxproj`、`BIGShine.vcxproj.filters`
- 説明: `Data/Skills/README.md`、`Data/Skills/SkillWorkspace.md`

実ファイルは既存Combat、Workspace、Player、AI分類へ配置しています。
Visual Studioでは共通実行は `Code\Engine\GamePlay`、作成画面は `Code\Editor\Workspace` のFilterへ登録しています。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。
