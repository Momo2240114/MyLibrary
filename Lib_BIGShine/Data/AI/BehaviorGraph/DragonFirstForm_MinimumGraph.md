# Dragon First Form Minimum BehaviorGraph

DragonAIComponent is an action executor. Production decisions should be authored in `AIBehaviorEditorWindow` as a BehaviorGraph asset.

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
