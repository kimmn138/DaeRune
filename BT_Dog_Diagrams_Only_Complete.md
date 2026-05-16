# BT / BTS Diagrams Only

## BT Tree ASCII Diagram

```text
Root
└─ Selector
   ├─ Service: BTS_FindNearestCleanserSite
   ├─ Service: BTS_FindNearestPlayer_Dog
   ├─ Service: BTS_CheckValidTarget
   │
   └─ Selector
      ├─ Decorator: Am I alive?
      │  ├─ Notify Observer: On Value Change
      │  ├─ Observer Aborts: Both
      │  ├─ Blackboard Key: Dead
      │  └─ Condition: Is Not Set
      │
      ├─ Decorator: Am I NOT Stunned?
      │  ├─ Notify Observer: On Value Change
      │  ├─ Observer Aborts: Self
      │  ├─ Blackboard Key: Stunned
      │  └─ Condition: Is Not Set
      │
      ├─ Sequence: Cleanser Site Branch
      │  ├─ Decorator: Do I not have a Target?
      │  │  ├─ Notify Observer: On Result Change
      │  │  ├─ Observer Aborts: Self
      │  │  ├─ Blackboard Key: TargetToFollow
      │  │  └─ Condition: Is Not Set
      │  │
      │  ├─ Task: MoveTo(TargetCleanserSiteLocation)
      │  ├─ Task: BTT_Attack_Dog(TargetCleanserSite)
      │  └─ Task: Wait(AttackSpeed)
      │
      └─ Selector: TargetToFollow Branch
         ├─ Decorator: Do I have a Target?
         │  ├─ Notify Observer: On Value Change
         │  ├─ Observer Aborts: Self
         │  ├─ Blackboard Key: TargetToFollow
         │  └─ Condition: Is Set
         │
         ├─ Sequence: Attack Target Branch
         │  ├─ Decorator: Am I Close Enough to Attack?
         │  │  ├─ Notify Observer: On Result Change
         │  │  ├─ Observer Aborts: None
         │  │  ├─ Blackboard Key: DistanceToTarget
         │  │  ├─ Condition: Is Less Than
         │  │  └─ Value: 500.0
         │  │
         │  ├─ Task: MoveTo(TargetToFollow)
         │  ├─ Task: BTT_Attack_Dog(TargetToFollow)
         │  └─ Task: Wait(AttackSpeed)
         │
         └─ Sequence: Approach Target Branch
            ├─ Decorator: Am I Close Enough to Approach?
            │  ├─ Notify Observer: On Result Change
            │  ├─ Observer Aborts: None
            │  ├─ Blackboard Key: DistanceToTarget
            │  ├─ Condition: Is Less Than Or Equal To
            │  └─ Value: 2000.0
            │
            ├─ Decorator: Am I Far Enough to Approach?
            │  ├─ Notify Observer: On Result Change
            │  ├─ Observer Aborts: Self
            │  ├─ Blackboard Key: DistanceToTarget
            │  ├─ Condition: Is Greater Than Or Equal To
            │  └─ Value: 500.0
            │
            ├─ Task: Wait
            └─ Task: MoveTo(TargetToFollow)
```

## BTS_FindNearestCleanserSite Data Flow Diagram

```text
[EventReceiveTickAI]
        │
        │  Interval = 1.0s
        ▼
[ControlledPawn]
        │
        ├──────────────────────────────────────────────────────────────┐
        │                                                              │
        ▼                                                              ▼
[GetClosestCleanserSite(ControlledPawn)]                  [GetActorLocation]
        │                                                              │
        ▼                                                              ▼
[ReturnValue]                                             [FromLocation]
        │                                                              │
        ▼                                                              │
[Cast To BP_DRCleanserSite]                               │
        │                                                              │
        ├─ Cast Failed ───────────────────────────────► [End]          │
        │                                                              │
        ▼                                                              │
[As BP_DRCleanserSite]                                     │
        │                                                              │
        ├──────────────► [Set Blackboard: TargetCleanserSite]           │
        │                    Value = As BP_DRCleanserSite              │
        │                                                              │
        ▼                                                              │
[GetClosestSurfacePoint] ◄────────────────────────────────┘
        │
        │  Target       = As BP_DRCleanserSite
        │  FromLocation = ControlledPawn.GetActorLocation
        ▼
[ClosestSurfacePoint]
        │
        ▼
[Set Blackboard: TargetCleanserSiteLocation]
        │
        │  Value = ClosestSurfacePoint
        ▼
[End]
```

## BTS_FindNearestPlayer_Dog Data Flow Diagram

```text
[EventReceiveTickAI]
        │
        ▼
[Get Blackboard: HasFirstAttacker]
        │
        ├─ True  ─────────────────────────────────────────────────────────► [End]
        │
        └─ False
             │
             ▼
       [ControlledPawn]
             │
             ▼
       [GetAIController]
             │
             ▼
       [Cast To BP_DRAIController]
             │
             ├─ Cast Failed ─────────────────────────────────────────────► [End]
             │
             ▼
       [Set Variable: BPDRAIController]
             │
             ▼
       [BPDRAIController.GetPlayers]
             │
             ▼
       [Set Variable: PlayerWithinRange]
             │
             ▼
       [Initialize Variables]
             │
             ├─ BleedingActor            = None
             ├─ NormalActor              = None
             ├─ BleedingClosestDistance  = 99999999999.0
             └─ NormalClosestDistance    = 99999999999.0
             │
             ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ For Each Loop: PlayerWithinRange                                             │
│                                                                             │
│ [Array Element]                                                              │
│       │                                                                      │
│       ▼                                                                      │
│ [IsValid(Array Element)?]                                                    │
│       │                                                                      │
│       ├─ False ───────────────────────────────────────────────► [Continue]   │
│       │                                                                      │
│       └─ True                                                                │
│            │                                                                 │
│            ▼                                                                 │
│      [Set Variable: Actor = Array Element]                                   │
│            │                                                                 │
│            ▼                                                                 │
│      [Cast To CombatInterface]                                               │
│            │                                                                 │
│            ▼                                                                 │
│      [CombatInterface.IsDead]                                                │
│            │                                                                 │
│            ├─ True ─────────────────────────────────────────► [Continue]     │
│            │                                                                 │
│            └─ False                                                          │
│                 │                                                            │
│                 ▼                                                            │
│           [ControlledPawn.GetDistanceTo(Actor)]                              │
│                 │                                                            │
│                 ▼                                                            │
│           [Set Variable: CalculatedDistance]                                 │
│                 │                                                            │
│                 ▼                                                            │
│           [CalculatedDistance <= BleedingSearchDistance(3000.0)?]            │
│                 │                                                            │
│                 ├─ False ───────────────────────────────────► [Continue]     │
│                 │                                                            │
│                 └─ True                                                       │
│                      │                                                       │
│                      ▼                                                       │
│                [Actor.GetAbilitySystemComponent]                             │
│                      │                                                       │
│                      ▼                                                       │
│                [HasMatchingGameplayTag: Debuff.Bleed?]                       │
│                      │                                                       │
│                      ├─ True                                                 │
│                      │    │                                                  │
│                      │    ▼                                                  │
│                      │ [CalculatedDistance < BleedingClosestDistance?]       │
│                      │    │                                                  │
│                      │    ├─ False ─────────────────────────► [Continue]     │
│                      │    │                                                  │
│                      │    └─ True                                            │
│                      │         │                                             │
│                      │         ├─ [Set Variable: BleedingActor = Actor]       │
│                      │         └─ [Set Variable: BleedingClosestDistance      │
│                      │                         = CalculatedDistance]         │
│                      │               │                                      │
│                      │               ▼                                      │
│                      │           [Continue]                                  │
│                      │                                                       │
│                      └─ False                                                │
│                           │                                                  │
│                           ▼                                                  │
│                     [CalculatedDistance <= NormalSearchDistance?]            │
│                           │                                                  │
│                           ├─ False ─────────────────────────► [Continue]     │
│                           │                                                  │
│                           └─ True                                            │
│                                │                                             │
│                                ▼                                             │
│                          [CalculatedDistance < NormalClosestDistance?]       │
│                                │                                             │
│                                ├─ False ────────────────────► [Continue]     │
│                                │                                             │
│                                └─ True                                       │
│                                     │                                        │
│                                     ├─ [Set Variable: NormalActor = Actor]    │
│                                     └─ [Set Variable: NormalClosestDistance   │
│                                                     = CalculatedDistance]    │
│                                           │                                  │
│                                           ▼                                  │
│                                       [Continue]                             │
└─────────────────────────────────────────────────────────────────────────────┘
             │
             ▼
[Loop Completed]
             │
             ▼
[IsValid(BleedingActor)?]
             │
             ├─ True
             │    │
             │    ├─ [Set Blackboard: DistanceToTarget = BleedingClosestDistance]
             │    ├─ [Set Blackboard: TargetToFollow   = BleedingActor]
             │    ├─ [ControlledPawn → Cast To DREnemy]
             │    └─ [Set DREnemy.IsAggroed = True]
             │          │
             │          ▼
             │        [End]
             │
             └─ False
                  │
                  ▼
            [IsValid(NormalActor)?]
                  │
                  ├─ True
                  │    │
                  │    ├─ [Set Blackboard: DistanceToTarget = NormalClosestDistance]
                  │    ├─ [Set Blackboard: TargetToFollow   = NormalActor]
                  │    ├─ [ControlledPawn → Cast To DREnemy]
                  │    └─ [Set DREnemy.IsAggroed = True]
                  │          │
                  │          ▼
                  │        [End]
                  │
                  └─ False
                       │
                       ▼
                 [BPDRAIController.HasCombatTimedOut(TimeoutSeconds = 3.0)?]
                       │
                       ├─ False ─────────────────────────────────────────────► [End]
                       │
                       └─ True
                            │
                            ├─ [Set Blackboard: TargetToFollow = None]
                            ├─ [ControlledPawn → Cast To DREnemy]
                            └─ [Set DREnemy.IsAggroed = False]
                                  │
                                  ▼
                                [End]
```

## BTS_CheckValidTarget Data Flow Diagram

```text
[EventReceiveTickAI]
        │
        ▼
[Get Blackboard: TargetToFollow]
        │
        ▼
[IsValid(TargetToFollow)?]
        │
        ├─ False ─────────────────────────────────────────────────────────────► [End]
        │
        └─ True
             │
             ▼
       [Cast TargetToFollow To Actor]
             │
             ├─ Cast Failed ─────────────────────────────────────────────────► [End]
             │
             ▼
       [Set Variable: CurrentTarget = Casted Actor]
             │
             ├──────────────────────────────────────────────────────────────┐
             │                                                              │
             ▼                                                              ▼
[CurrentTarget.GetActorLocation]                         [ControlledPawn.GetActorLocation]
             │                                                              │
             └──────────────────────┬───────────────────────────────────────┘
                                    ▼
                          [Distance(Vector)]
                                    │
                                    ▼
                 [Set Blackboard: DistanceToTarget]
                                    │
                                    ▼
                    [Cast CurrentTarget To CombatInterface]
                                    │
                                    ├─ Cast Failed ─────────────────────────► [End]
                                    │
                                    ▼
                         [CombatInterface.IsDead]
                                    │
        ┌───────────────────────────┴───────────────────────────┐
        │                                                       │
        ▼                                                       ▼
[IsDead == True]                                      [IsDead == False]
        │                                                       │
        ▼                                                       ▼
[Reset Target / Focus / Aggro State]          [CurrentTarget.GetAbilitySystemComponent]
        │                                                       │
        ▼                                                       ▼
      [End]                                  [HasMatchingGameplayTag: Debuff.Bleed?]
                                                                │
                         ┌──────────────────────────────────────┴──────────────────────────────────────┐
                         │                                                                             │
                         ▼                                                                             ▼
              [Debuff.Bleed == True]                                                        [Debuff.Bleed == False]
                         │                                                                             │
                         ▼                                                                             ▼
 [ControlledPawn.GetDistanceTo(CurrentTarget)]                              [ControlledPawn.GetDistanceTo(CurrentTarget)]
                         │                                                                             │
                         ▼                                                                             ▼
          [Distance > 3000.0?]                                                           [Distance > 1500.0?]
                         │                                                                             │
       ┌─────────────────┴─────────────────┐                                 ┌────────────────────────┴────────────────────────┐
       │                                   │                                 │                                                 │
       ▼                                   ▼                                 ▼                                                 ▼
[False]                            [True]                              [False]                                          [True]
       │                                   │                                 │                                                 │
       ▼                                   ▼                                 ▼                                                 ▼
     [End]                  [Check Combat Timeout]                         [End]                                  [Check Combat Timeout]
                                           │                                                                               │
                                           └───────────────────────────────┬───────────────────────────────────────────────┘
                                                                           ▼
                                                       [ControlledPawn.GetAIController]
                                                                           │
                                                                           ▼
                                                         [Cast To BP_DRAIController]
                                                                           │
                                            ┌──────────────────────────────┴──────────────────────────────┐
                                            │                                                             │
                                            ▼                                                             ▼
                                    [Cast Failed]                                                  [Cast Succeeded]
                                            │                                                             │
                                            ▼                                                             ▼
                                          [End]                         [HasCombatTimedOut(TimeoutSeconds = 3.0)?]
                                                                                                          │
                                                                                  ┌───────────────────────┴───────────────────────┐
                                                                                  │                                               │
                                                                                  ▼                                               ▼
                                                                            [False]                                           [True]
                                                                                  │                                               │
                                                                                  ▼                                               ▼
                                                                                [End]                         [Reset Target / Focus / Aggro State]
                                                                                                                                  │
                                                                                                                                  ▼
                                                                                                                                [End]
```

```text
[Reset Target / Focus / Aggro State]
        │
        ├─ [Set Blackboard: TargetToFollow  = None]
        ├─ [Set Blackboard: FirstAttacker   = None]
        ├─ [Set Blackboard: HasFirstAttack  = False]
        ├─ [Set Blackboard: DistanceToTarget = 0]
        │
        ▼
[ControlledPawn.GetAIController]
        │
        ▼
[ClearFocus]
        │
        ▼
[ControlledPawn]
        │
        ▼
[Cast To DREnemy]
        │
        ├─ Cast Failed ─────────────────────────────────────────────────────► [End Reset]
        │
        ▼
[Set DREnemy.IsAggroed = False]
        │
        ▼
[End Reset]
```
