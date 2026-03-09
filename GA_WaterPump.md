Event ActivateAbility
-> Cast To BP_GardenRobot(Object: Get Avatar from Actor Info(Target : Self))
-> Sequence
    -> Then 0
        -> Is Locally Controlled(Target: Self)
            -> True
                -> Play Montage(In Skeletal Mesh Component: GetFirstPersonMesh(BP_GardenRobot), Montage to Play: AM_FP_HoseBlast) 아래로 넘어감
            -> PlayMontageAndWait(Montage to Play: AM_HoseBlast)
            -> Wait Gameplay Event(Event Tag: Event.Montage.WaterPump, Only Trigger Once: True, Only Match Exact: True)
                -> Event Received
                    -> Set in Water Loop(Target: Get Avatar Actor from Actor Info(Target: Self), In Loop: True)
                    ->ApplyGameplayEffectToOwner(Target: Self, Gameplay Effect Class: GE Water Pump_SlowSelf, Return Value: Slow Gameplay Effect Handle)
                    -> Start Water Pump Loop
                    -> Cast To BP_GardenRobot(Target: Get Avatar Actor from Actor Info(Target: Self))
                    -> Play Montage(In Skeletal Mesh Component: GetMesh(BP_GardenRobot), Montage to Play: AM_InHoseBlast)
                    -> Set Timer by Event(Event: Cost, Time: Damage Delta Time, Looping: True, Return Value: Damage and Cost Timer)
    -> Then 1
        -> Wait Input Release
            -> On Release
                -> Set in Water Loop(Target: Get Avatar Actor from Actor Info(Target: Self))
                -> Stop Water Pump Loop
                -> Montage Stop(Target: GetAnimInstance(GetMesh(BP_GardenRobot)), In Blend Out Time: 0.2, Montage: AM_InHoseBlast)
                -> RemoveGameplayEffectFromOwnerWithHandle(Handle: Slow Gameplay Effect Handle)
                -> Clear and Invalidate Timer by Handle(Handle: Damage and Cost Timer)
                ->End Ability

Event Cost
-> CommitAbilityCost
    
Event OnDamageTickReached(Return Value: Target Actor)
-> Branch(Condition: Is Not Friend(First Actor: Get Avatar Actor from Actor Info(Target: Self), Second Actor: Target Actor))
    -> True
        -> Cast To DREnemy(Object: Target Actor)
        -> Has Authority
            -> True
                -> Apply Damage Effect(Damage Effect Params: Make Damage Effect Params from Class Defaults(Target: Self, Target Actor: Target Actor))
    -> False
        -> Cast To DRCharacter(Object: Target Actor)
        -> Make Target Data Handle from Actors(Target: Self, Target Actor: DRCharacter)
        -> ApplyGameplayEffectToTarget(Target Data: Make Target Data Handle from Actors의 반환값, Gameplay Effect Class: GE Water Pump_GrantWater)

