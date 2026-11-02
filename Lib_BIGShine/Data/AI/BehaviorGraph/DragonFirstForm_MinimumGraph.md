# Dragon First Form Minimum BehaviorGraph

## 現在のドラゴンAIのコメントBox

独立した行動・条件のBoxには横方向の専用列を割り当て、無関係なBoxを下の段へ折り返しません。下方向へ伸ばすのは実際の接続に沿った派生Box、同じ枝の条件、内側の行動Boxです。Rootと直下の判断ノードは、派生を含むBox全体の左右端の中央へ配置します。再整列もこの配置方針を維持します。横方向の移動はホイール押し込みドラッグで行えます。

`DragonFirstForm.json`の96ノードを、18個の行動Boxと6個の判断・共通条件Boxへ整理しています。
行動Boxには、行動の実行ノードだけでなく、その行動へ進む条件ノードも含めています。
Boxのタイトルは行動名、説明文は現在設定されている選択条件と判断意図です。

- 空中：着地攻撃、空中拡散ブレス、空中火球。
- 地上近接：背後への尻尾、向き直し、胴体下への噛みつき、近距離の離陸、正面の噛みつき。
- 地上候補：突進、回転尻尾、歩いて接近。
- 火球・ブレス：通常火球、火球薙ぎ払い、前方火球、三連火球、火炎ビーム薙ぎ払い、拡散ブレス、前方ブレス。

通常火球と火球薙ぎ払いは中距離と遠距離の枝で共有されています。
同じ処理を複製せず、共有Boxへ両方の参照リンクを残しています。
既存の条件値、Action名、Priority、抽選重み、97本のリンクは変更していません。
配置によって評価順が変わらないよう、Order by X PositionはOFFにしています。

### 判断条件を確認する操作

1. AI Behavior Graphでドラゴンのオブジェクトを選択します。メインシーンでは`TestTarget`が対象です。
2. InspectorのComment Boxesで行動名を選ぶと、そのBoxへ表示を移動します。
3. Box上の説明で判断意図を、Dragon Decision Contextで現在の行動と各条件のBlackboard現在値・比較条件・Priorityを確認できます。
4. IncomingはBox外から接続しているノードです。共有火球では中距離と遠距離の両参照元が表示されます。
5. Organize Action Areasで再整列しても、このドラゴングラフのBox分類・タイトル・説明・色は保持します。
6. 手動で変更した配置や説明はSaveでグラフAssetへ保存します。
7. Boxのタイトル部分を左ドラッグすると、Boxと内側のノードを移動できます。境界のドラッグはサイズ変更です。
8. Focus selected linksがONの場合、選択したBox・ノードに関係する線を強調します。全体を確認する場合はOFFにしてください。

表示値は現在Blackboardへ公開されている値です。過去の判断時点を記録した履歴ではありません。
使用可能条件を満たしても、抽選重みと直近の使用履歴によって他の候補が選ばれる場合があります。
地上待機のState値は0です。Boolは数値欄では0/1で表示します。

### 現在の設定で注意して読む点

- 近距離離陸：水平距離6.3以下。正面条件はありません。
- 正面噛みつき：水平距離4.8未満。
- 中距離の火球系：水平距離16以上22未満。
- 遠距離の火球・ブレス：水平距離22以上。
- 突進：水平距離16以上。
- 既存ノード名Low HP Spinの条件はDamagePhase >= 0です。現在は低HP限定ではないため、Boxもその条件に沿って説明しています。

上記は現在のグラフ設定です。エディターで比較値を変更した場合は、説明文も合わせて更新してください。

---

DragonAIComponent is an action executor. Production decisions should be authored in `AIBehaviorEditorWindow` as a BehaviorGraph asset.

Animation transitions should be authored in `AnimationEditorWindow` / `AnimationControllerComponent` graph assets.
DragonAIComponent only sends animation triggers named `To_<AnimationStateName>` by default.
Direct `ChangeState` is kept only as a debug/fallback path by disabling `Animation Graph Driven`.

## Blackboard Category

Use the DragonAIComponent `Blackboard Category` value. The default is:

```text
DragonAI
```

## Runtime Keys

DragonAIComponent writes these keys every frame:

```text
HasTarget : bool
TargetDistance : float
TargetHorizontalDistance : float
TargetAngle : float
IsTargetInFront : bool
IsTargetInSide : bool
IsTargetBehind : bool
IsTargetUnderBody : bool
CanUseBite : bool
CanUseFireBall : bool
CanUseSpreadBreath : bool
CanUseTailSweep : bool
CanUseTakeOff : bool
CanUseRunCharge : bool
CanUseSpinSweep : bool
CurrentAction : int
ActionEnded : bool
IsActing : bool
HpRate : float
DamagePhase : int
```

## Minimum Graph Shape

Portfolio layout rule:

```text
Root
  Health Phase Gate
    Collapse / Death Phase
    Dark Leak Phase
      Dark Leak State Gate
        Idle State
        Battle State
          Distance / Angle Decision
            Special SpinSweep
            UnderBody TakeOff
            Behind TailSweep
            Side Turn
            Close Bite
            Far Breath
            Mid FireBall
            Approach
    Cracked Phase
      Cracked State Gate
    Normal Phase
      Normal State Gate
```

Place parent nodes above their children. Inputs should visually enter from the top, and child decisions/actions should flow downward to the next row.

Available PlayAction names:

```text
DragonAI_Idle
DragonAI_WalkApproach
DragonAI_RunCharge
DragonAI_TurnToPlayer
DragonAI_Bite
DragonAI_FireBall
DragonAI_SpreadBreath
DragonAI_TurnTailSweep
DragonAI_SpinTailSweep
DragonAI_TakeOff
DragonAI_AirHold
DragonAI_AirFireBall
DragonAI_AirSpreadBreath
DragonAI_LandingAttack
DragonAI_GetHit
DragonAI_Death
```

```text
Root
  Selector
    Sequence: IsActing == false, HasTarget == true, CanUseBite == true, TargetDistance <= 4.0, IsTargetInFront == true
      PlayAction: DragonAI_Bite
        WaitForActionEnd: true
        ActionEndCategory: DragonAI
        ActionEndKey: ActionEnded

    Sequence: IsActing == false, HasTarget == true, CanUseFireBall == true, TargetDistance <= 10.0, IsTargetInFront == true
      PlayAction: DragonAI_FireBall
        WaitForActionEnd: true
        ActionEndCategory: DragonAI
        ActionEndKey: ActionEnded

    Sequence: IsActing == false, HasTarget == true
      PlayAction: DragonAI_WalkApproach
        WaitForActionEnd: true
        ActionEndCategory: DragonAI
        ActionEndKey: ActionEnded
```

## Action End

Action completion should be sent from Sequencer or Animation Event:

```text
DragonAI_NotifyActionEnd
```

If the event is missing, DragonAIComponent uses `ActionTimeout` on the current action and then ends the action with a warning status.

## Minimum Animation Graph

Use `DragonFirstForm_Test` or an equivalent graph asset with `UseTransitionGraph = true`.
Create trigger transitions from `__Any__` to the dragon states:

```text
To_PlainsDragon_IdleBreathe -> PlainsDragon_IdleBreathe
To_PlainsDragon_Walk -> PlainsDragon_Walk
To_PlainsDragon_Run -> PlainsDragon_Run
To_PlainsDragon_BiteAttack -> PlainsDragon_BiteAttack
To_PlainsDragon_SpitFireBall -> PlainsDragon_SpitFireBall
To_PlainsDragon_SpreadFire -> PlainsDragon_SpreadFire
To_PlainsDragon_Turn90L_RM -> PlainsDragon_Turn90L_RM
To_PlainsDragon_TakeOff -> PlainsDragon_TakeOff
To_PlainsDragon_FlyStationary -> PlainsDragon_FlyStationary
To_PlainsDragon_FlyStationarySpitFireBall -> PlainsDragon_FlyStationarySpitFireBall
To_PlainsDragon_FlyStationarySpreadFire -> PlainsDragon_FlyStationarySpreadFire
To_PlainsDragon_FlyStationaryToLanding -> PlainsDragon_FlyStationaryToLanding
To_PlainsDragon_GetHit1 -> PlainsDragon_GetHit1
To_PlainsDragon_Death -> PlainsDragon_Death
```

Recommended visual return transitions:

```text
BiteAttack / SpitFireBall / SpreadFire / Turn90L_RM / GetHit1 -> IdleBreathe on AnimationEnd
TakeOff -> FlyStationary on AnimationEnd
AirFireBall / AirSpreadBreath -> FlyStationary on AnimationEnd
LandingAttack -> IdleBreathe on AnimationEnd
```

The visual return transition does not replace gameplay completion.
Sequencer or Animation Event should still call `DragonAI_NotifyActionEnd` for:

```text
DragonAI_Bite
DragonAI_FireBall
DragonAI_SpreadBreath
DragonAI_TurnTailSweep
DragonAI_TakeOff
DragonAI_LandingAttack
DragonAI_RunCharge
DragonAI_SpinTailSweep
DragonAI_Death
```
