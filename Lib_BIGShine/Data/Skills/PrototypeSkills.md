# 動作確認用プレイヤースキル

既存のPlayerInputResolver → RequestAction → PlayerSkillState → SkillExecutionを使用します。
追加Stateの名前は `スキル名_何段目`、保存ファイル名はASCIIの `スキル名_番号.json` です。
モデル内のAnimation名・Indexは変更していません。
通常攻撃のState、イベント、AttackTemplateも変更していません。

## 使用Animation

モデル: `Data/Model/SwordPlayer/CombatGirls_Sword_Shield.gltf`

| 入力 / スキル | 追加State | モデル内クリップ | Index |
| --- | --- | --- | --- |
| L1 + □ / PhantomRush | 1～6段目 | Combo_Attack_01_01 / 01_02 / 01_03 / Combo_Attack_02_01 / Combo_Attack_03_01 / SS_Sp_Skill1 | 60 / 61 / 62 / 65 / 70 / 6 |
| L1 + △ / Breakthrough | 1～3段目 | Charge Start / Loop / Run_Attack_02_Seq | 59 / 58 / 221 |
| L1 + ○ / RisingJudgment | 1～9段目 | Up_Floor_To_Air / Air_01 / SS_Attack2 / Air_02 / Combo_Attack_03_01 / SS_Attack3 / Air_to_Floor Start / Loop / End | 46 / 85 / 13 / 86 / 70 / 14 / 37 / 36 / 35 |

各State名はスキル名_段数です。速度Curve、途中ActionEnd、最終段の攻撃遅延と残像は [SkillPacing.md](SkillPacing.md) を参照してください。
通常攻撃のStateは変更せず、その既存モーション・イベントをスキル専用Stateへ流用しています。

## 移動・Phase終了

- PhantomRush: 6つの非ループStateを攻撃直後のActionEndで連続再生します。
- Breakthrough: Start／Loopは維持し、3段目の切り上げ後に上空へワープ、降下Startと着地Endへ接続します。最新値はSkillPacing.mdを参照してください。
  LockOn中の向きはPlayerの既存補正APIを使用します。距離条件や新しいMotionWarp処理は追加していません。
- RisingJudgment: 最初のPhaseで上方向Impulse 13を予約します。
  空中連撃5つと降下Startでは重力を停止して高さを維持します。
  降下Loopは縦速度-18で落下し、最短0.05秒の待機後にColliderの接地でEndへ進みます。
  接地しない場合は6秒でスキルを停止します。

移動は既存CharacterMovement → Collider更新へ予約します。
CharacterColliderはUseGravity=falseで縦速度を消すため、空中維持・降下中はUseGravity=true、
GravityScale=0を一時的に使用します。Phase切替・スキル終了・被弾・中断時には元の設定を戻します。
RootMotionは追加Stateでは無効にし、速度予約との二重移動を避けています。
ForwardSpeed / VerticalImpulse / SuspendGravity / VerticalSpeed / End / DurationはSkill Workspaceで編集できます。

## HitBoxと仮演出

新しい武器Colliderは作らず、既存WeaponのActionPrefix `Sword` のHitBoxを使います。
各攻撃PhaseにATTACK_TEMPLATEイベントを配置し、終了・State退出時にHitBoxを無効化します。

| AttackTemplate | 使用箇所 | 仮Damage | 対象の打ち上げ力 |
| --- | --- | --- | --- |
| SkillPrototypeSlash | PhantomRush 1～5段目 | 20 | 0 |
| SkillPrototypeFinisher | PhantomRush 6段目、Breakthrough 6段目、RisingJudgment 9段目 | 40 | 0 |
| SkillPrototypeLauncher | RisingJudgment 1段目、Breakthrough 3段目 | 20 | 13 |
| SkillPrototypeAirSlash | RisingJudgment 2～6段目 | 20 | 4 |

通常の仮HitStopは0.025秒、Scaleは0.75です。フィニッシュは0.055秒、Scaleは0.25へ変更しています。HitBoxはイベント開始から0～0.20秒を基本とし、途中接続時はイベント終了・State退出で閉じます。
形状・半径は既存WeaponのSword HitBoxを使用します。AttackTemplateの半径項目を変更しても、
現在のAttackTemplateRuntimeは武器Colliderの形状へ適用しないため、サイズ調整はWeapon側で行ってください。

MeshVFXには実在するPlayerSwordArc_Aqua、PlayerSwordArc_Strong、PlayerSwordArc_Strong_Green、
PlayerKickShockwave_Aquaを使用します。
Hit時のParticleはSwordHit_Aqua、フィニッシュはSwordHit_Aqua_Lv02です。
Trailイベントは既存Weapon_Trail_Start / Weapon_Trail_Stopを使用し、Weaponに設定済みのWindParticleも利用します。
CleanupActionはSkillEffect.StopTrailsとして、中断時にも武器Trailとモデル残像の生成を止めます。

攻撃用Sound素材を特定できていないため、SoundイベントのActionは空です。
各Stateの「仮Sound」イベントへ、既存Audio Workspaceで登録した攻撃音Actionを設定してください。
空イベントは実際の音を再生しません。

## 個別Cooldown

| スキル | 初期スロット | Cooldown |
| --- | --- | --- |
| PhantomRush | SkillSlot0 | 3.0秒 |
| Breakthrough | SkillSlot1 | 5.0秒 |
| RisingJudgment | SkillSlot2 | 8.0秒 |

- 設定値: PlayerSkills.jsonのSkills[].Cooldown。既存の共通Serialize / Deserializeで保存・読み込みます。
- 残り時間: PlayerControllerComponent::skillCooldowns。Skill名をキーにし、JSONへは保存しません。
- 開始: PlayerSkillState::Enterで共通実行の開始成功を確認してNotifySkillStartedを呼びます。
  入力しただけ、使用条件不成立、素材検証失敗、Cooldown中、最初のPhase開始失敗では開始しません。
- 更新: PlayerControllerComponent::UpdateからUpdateSkillCooldownsを呼び、渡されたTimeScale適用済み時間で0まで減らします。
  Player操作がGameModeで停止している間は、既存State更新と同じくCooldown更新も停止します。
- 判定: CanExecuteSkillへIsSkillCooldownReadyを統合しています。入力・先行入力消費・State開始時に確認します。
- リセット: Scene開始時とReset Skill Cooldownsボタン。再読込・中断ではリセットしません。
- 外部参照: GetRemainingSkillCooldown、GetSkillCooldownDuration、IsSkillCooldownReady。
- GUI: Skill WorkspaceのCooldown、Phaseの移動設定、Player / AIタブとPlayer GUIの残り時間・リセットボタン。

複合入力の優先順位は既存方式を維持しています。
Cooldown中の複合入力はそのFrameで消費し、対応する通常攻撃へ流しません。
他スキルのCooldownは開始せず、通常の単独入力や別スキルの入力経路も維持しています。

## 後から調整する項目

Cooldown、前進速度、突進Loop時間、上昇Impulse、降下速度、接地待ちの上限、BlendDuration、
PhantomRushの再生速度、HitBoxイベント時間、Weapon側の形状、Damage、KnockBack、打ち上げ力、
HitStop、MeshVFXの位置・向き・サイズ・寿命、Particle、Sound Actionを調整してください。

## 変更・追加ファイル

変更したソース:

- Code/Engine/GamePlay/Combat/SkillDefinition.h / SkillDefinition.cpp
- Code/Engine/GamePlay/Combat/SkillExecution.h / SkillExecution.cpp
- Code/Game/Charcter/Player/PlayerControllerComponent.h / PlayerControllerComponent.cpp
- Code/Game/Charcter/Player/PlayerSkillState.cpp
- Code/Editor/Workspace/SkillWorkspaceWindow.cpp

変更したデータ・説明:

- Data/Skills/PlayerSkills.json
- Data/Animation/GraphData/Player.json
- Data/Scenes/MainScene/Objects/PlayerModel.json
- Data/Skills/README.md
- Data/Skills/SkillWorkspace.md

追加したStateファイル（Data/Animation/StateData/Player/States）:

- PhantomRush_01.json、PhantomRush_02.json、PhantomRush_03.json、PhantomRush_04.json
- Breakthrough_01.json、Breakthrough_02.json、Breakthrough_03.json
- RisingJudgment_01.json、RisingJudgment_02.json、RisingJudgment_03.json
- RisingJudgment_04.json、RisingJudgment_05.json、RisingJudgment_06.json

追加したAttackTemplateファイル（Data/AttackTemplate）:

- SkillPrototypeSlash.json
- SkillPrototypeFinisher.json
- SkillPrototypeLauncher.json
- SkillPrototypeAirSlash.json

追加した説明: Data/Skills/PrototypeSkills.md。
新しいC++ファイルはありません。既存ソースのVisual Studio登録・Filterを維持しています。
State JSONはGraphとPlayerModelのStateAssetsへ登録し、AttackTemplate JSONは既存の自動読み込み対象です。

ビルドおよび実行確認は行っていません。
コード上で確認可能な範囲の静的確認のみ実施しました。

各スキルは固定の入力へ結び付けず、PlayerのSkillLoadoutから装備します。追加したHealと差し替え方法は [SkillLoadout.md](SkillLoadout.md) を参照してください。

攻撃スキルの残像は現在無効です。SkillPrototypeSlash／AirSlash／Launcher／FinisherのattackEffectsへ判定付き斬撃を追加し、着地Endは専用のSkillPrototypeLandingImpactで大きな衝撃波と範囲判定を生成します。最新の設定はSkillPacing.mdを参照してください。
