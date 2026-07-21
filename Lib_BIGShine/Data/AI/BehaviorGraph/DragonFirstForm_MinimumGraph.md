# Dragon First Form Minimum BehaviorGraph

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
