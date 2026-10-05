# プレイヤー複合入力スキル

`PlayerControllerComponent` の `SkillDefinitionsPath` は、既定で
`Data/Skills/PlayerSkills.json` を参照します。定義はComponentのStartで読み込みます。
Skill WorkspaceのSave & Apply Bindingから待機中のActorへ再読込できます。
Scene JSONに参照項目がない場合も既定値を使用します。

敵との共通実行と作成画面の使い方は [SkillWorkspace.md](SkillWorkspace.md) を参照してください。

## 入力

`Inputs` にModifierのInput Action名、TriggerのInput Action名、優先度、要求する
SkillA / SkillB / SkillCを指定します。ModifierはHold、Triggerは押下Frameで判定します。
優先度が同じ場合は配列の先頭を優先します。物理ボタン番号はDefinitionで使用しません。
既定のModifierはKeyConfigに未登録の場合だけL1へ割り当てます。

| 入力 | Action | 初期状態 |
| --- | --- | --- |
| L1 + △（StrongAttack） | SkillA | 確認用の2 Phase定義あり |
| L1 + □（WeakAttack） | SkillB | 定義追加待ち。入力だけ消費 |
| L1 + ○（JustGuard） | SkillC | 定義追加待ち。入力だけ消費 |

複合入力が成立したFrameは、実行不可・空中・キャンセル不可でも通常Actionへ流しません。
Modifier保持中の対応する通常Actionも抑制します。強攻撃の溜め予約は破棄するため、
複合入力に使った△を離しても強攻撃は発生しません。既存の単独SkillA/B入力は維持しています。
Definitionがない単独SkillA/Bは従来のグラフ駆動経路を使用します。

## 定義とPhase

`Skills` に `Name`、`Action`、`Enabled`、`Cooldown`、`CleanupAction`、`Phases` を指定します。
Cooldownは将来用のデータのみで、消費や待機時間はまだ管理しません。
実行可否の追加条件は `CanExecuteSkill` へ集約してください。

各Phaseの設定は次のとおりです。

| 項目 | 意味 |
| --- | --- |
| Animation | 既存Animation State名。必須 |
| End | ANIMATION_END / DURATION / ACTION_END |
| Duration | DURATIONで使用する秒数 |
| MaximumDuration | 終了通知漏れの停止上限。既定10秒 |
| BlendDuration | 次のAnimationへのブレンド秒数。既定0.1秒 |
| Sequence | OwnerのActionSequence名。省略可 |
| EnterAction / ExitAction | Phase開始／終了時の既存Action名。省略可 |
| CanDodgeCancel / CanActionBranch / CanBufferInput | Phase開始時の受付窓。既定false |

ANIMATION_ENDは再生時間と実際のサンプル尺を比較します。この設定では途中のActionEndを
全体終了へ流しません。ACTION_ENDでは、Animation EventまたはSequenceの終了Actionから
既存の `ActionEnd` / `NotifyActionEnd` を呼ぶと次の更新でPhaseが進みます。
MaximumDuration超過、Animation喪失、Phase開始失敗では全体を終了します。
最後のPhase終了は既存NotifyActionEndへ渡し、先行入力、Guard、Idle / Moveへの復帰を維持します。

Skill中は通常Animation Graphの自動遷移を抑制し、EventとRootMotionは従来どおり更新します。
同じAnimationを連続指定しても、Phase開始ごとに再生時間とEventを初期化します。
キャンセル窓は既存Animation Eventから変更でき、派生先はPlayer State Graphのルールで設定します。
SkillCも同じGUIで設定できます。

## 演出の設定

Skill WorkspaceのPhase時間軸は順序と区間設定を編集します。演出はAnimation SequencerでAttack / Trail / VFX / Sound Templateを
配置するか、Action Sequence Editorで既存Actionを指定時刻に配置し、PhaseのSequenceへ指定します。
HitBox開閉、Particle、Camera Actionも登録済みActionを使用します。
MotionWarpは `MotionWarp.Play` / `MotionWarp.Stop` を使用できます。
各Actionが必要とするComponentと、AttackTemplate / VFX / Soundなどの素材は事前に配置してください。

ActionはOwnerのLocal Registryを優先し、見つからなければGlobal Registryを参照します。
他ObjectのActionには既存のObject名付きActionを使用してください。
SequenceをPhaseで開始すると、そのPhase終了・中断時に残りの予約と終了通知を停止します。
後から別のSequenceが始まった場合は世代番号が変わるため、そのSequenceは停止しません。
Sequenceは同じComponent内で同時に複数再生できない既存仕様を維持しています。

Sequenceの予約停止は既に開始した演出を巻き戻しません。中断時に必要なHitBox無効化、
Particle停止、MotionWarp停止などは、既存の即時SequenceをCleanupActionへ指定してください。
Animation Eventには必要に応じて `callEndOnStateExit` を設定してください。
EnterAction / ExitAction / CleanupActionは演出用とし、Player Stateの再帰的な強制遷移は設定しないでください。

確認用SkillAは、素材とState名が存在する `SS_Sp_Skill1` → `SS_Attack2` です。
最初のAnimationに保存されているMeshVFX Eventをそのまま使用します。
SkillB/Cを追加する場合は、同じ形式でActionだけを変えたDefinitionをSkillsへ追加します。

## 実機確認項目

ビルド・実行は行っていません。コード上の経路確認に加えて、以下を実機で確認してください。

1. △単独は従来の通常／溜め強攻撃となる。
2. L1保持後の△押下はSkillAだけとなり、△を離しても強攻撃が発生しない。
3. 同FrameのL1 + △もSkillAだけとなる。
4. L1 + □はWeakAttackへ流れない。初期定義ではSkillBは未設定のため発動しない。
5. △保持後にL1だけを押してもSkillAが発動しない。
6. SkillA中に1つ目のAnimationのMeshVFXが発生し、2つ目へ進む。
7. SkillA終了後にIdle / Moveへ復帰する。既存の受付窓と派生ルールによる遷移も確認する。
8. 被弾やキャンセル時に、スキルが開始したSequenceの予約Actionが後から発生しない。

Player ControllerのGUIでSkill Active / Current Skill / Current Phase / Skill Input /
Current Animationを確認できます。
