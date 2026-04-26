# 자판기 로봇 (VendingMachineRobot) 전투 시스템 구현 계획

## 개요

자판기 로봇의 기본 공격, 패시브(잭팟), 스킬(공격속도 버프)을 구현한다.
기존 캐릭터(`ADRCharacter`)를 그대로 사용하며, `EPlayerCharacterClass::VendingMachineRobot`으로 구분된다.
전투 관련 시스템(GA, GE, Projectile, Tag 등)만 새로 추가한다.

### 기존 프로젝트 패턴

이 프로젝트의 GA 구현 패턴은 다음과 같다:
- **C++**: `BlueprintCallable` 함수(도구)와 `UPROPERTY`(설정값)만 제공
- **Blueprint**: `ActivateAbility`를 오버라이드하여 실제 어빌리티 흐름을 조립
- 예시: `UDRMeleeAttack`은 C++ 클래스가 완전히 빈 껍데기이고 모든 로직이 BP에 있음
- 예시: `UDRFireBolt`은 C++에 `SpawnProjectiles()` BlueprintCallable 함수만 제공하고 발사 타이밍/몽타주/타겟팅은 BP에서 처리

이 패턴을 따라 자판기 로봇 GA도 **C++은 최소한의 도구 함수만, 나머지는 Blueprint에서 구현**한다.

---

## 1. Gameplay Tags 추가

**파일**: `Source/DaeRune/Public/DRGameplayTags.h` / `Source/DaeRune/Private/DRGameplayTags.cpp`

### 추가할 태그

```
Abilities.VendingMachine.BasicAttack      // 기본 공격 GA 식별
Abilities.VendingMachine.AttackSpeedBuff  // 공격속도 버프 스킬 GA 식별
State.VendingMachine.JackpotReady         // 잭팟 스택 5 도달 상태 (UI 연동용)
Buff.VendingMachine.AttackSpeed           // 공격속도 버프 GE 식별/조회 태그
```

### 구현 상세

`DRGameplayTags.h`에 멤버 추가:
```cpp
FGameplayTag Abilities_VendingMachine_BasicAttack;
FGameplayTag Abilities_VendingMachine_AttackSpeedBuff;
FGameplayTag State_VendingMachine_JackpotReady;
FGameplayTag Buff_VendingMachine_AttackSpeed;
```

`DRGameplayTags.cpp`의 `InitializeNativeGameplayTags()`에 등록:
```cpp
GameplayTags.Abilities_VendingMachine_BasicAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("Abilities.VendingMachine.BasicAttack"), FString("자판기 로봇 기본 공격"));
GameplayTags.Abilities_VendingMachine_AttackSpeedBuff = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("Abilities.VendingMachine.AttackSpeedBuff"), FString("자판기 로봇 공격속도 버프 스킬"));
GameplayTags.State_VendingMachine_JackpotReady = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("State.VendingMachine.JackpotReady"), FString("잭팟 스택 5 도달"));
GameplayTags.Buff_VendingMachine_AttackSpeed = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("Buff.VendingMachine.AttackSpeed"), FString("공격속도 버프 활성 상태"));
```

**참고**: 기본 공격에 별도 Cooldown 태그를 추가하지 않는다. 발사 간격은 GA 내부 타이머로 제어한다.

---

## 2. 기본 공격 GA (잭팟 패시브 통합)

### 설계 원칙: C++ vs Blueprint 분담

기존 프로젝트 패턴을 따른다:
- **C++**: 타이머 관리, 투사체 스폰, 잭팟 카운터, 공격속도 조회 → `BlueprintCallable` 함수로 제공
- **Blueprint**: `ActivateAbility` 오버라이드, 발사 시작/중지 호출, 몽타주, VFX/SFX, UI 연동

### 2-1. C++ 클래스: `UDRVendingMachineBasicAttack`

**파일**:
- `Source/DaeRune/Public/AbilitySystem/Abilities/DRVendingMachineBasicAttack.h`
- `Source/DaeRune/Private/AbilitySystem/Abilities/DRVendingMachineBasicAttack.cpp`

**상속**: `UDRProjectileSpell` → `UDRDamageGameplayAbility` → `UDRGameplayAbility`

C++ 클래스는 `ActivateAbility()`를 오버라이드하지 않는다. Blueprint에서 호출할 도구 함수만 제공한다.

### 클래스 선언

```cpp
UCLASS()
class DAERUNE_API UDRVendingMachineBasicAttack : public UDRProjectileSpell
{
    GENERATED_BODY()

public:
    // ============================================================
    // BlueprintCallable 함수 (Blueprint에서 호출하는 도구)
    // ============================================================

    /** 자동 연사 시작. LMB 홀드 시 Blueprint에서 호출. */
    UFUNCTION(BlueprintCallable, Category = "VendingMachine|BasicAttack")
    void StartAutoFire();

    /** 자동 연사 중지. LMB 해제 시 Blueprint에서 호출. */
    UFUNCTION(BlueprintCallable, Category = "VendingMachine|BasicAttack")
    void StopAutoFire();

    // ============================================================
    // BlueprintPure 함수 (Blueprint에서 상태 조회)
    // ============================================================

    /** 현재 잭팟 스택 수 반환 */
    UFUNCTION(BlueprintPure, Category = "VendingMachine|Jackpot")
    int32 GetJackpotStacks() const { return CurrentJackpotStacks; }

    /** 잭팟 발동 준비 상태인지 반환 */
    UFUNCTION(BlueprintPure, Category = "VendingMachine|Jackpot")
    bool IsJackpotReady() const { return CurrentJackpotStacks >= MaxJackpotStacks; }

    /** 현재 공격속도 반영된 발사 간격 반환 */
    UFUNCTION(BlueprintPure, Category = "VendingMachine|BasicAttack")
    float GetCurrentFireInterval() const;

    // ============================================================
    // BlueprintImplementableEvent (C++ → Blueprint 콜백)
    // ============================================================

    /** 일반 투사체 발사 직후 호출 (BP에서 VFX/SFX/몽타주 처리) */
    UFUNCTION(BlueprintImplementableEvent, Category = "VendingMachine|BasicAttack")
    void OnNormalShotFired();

    /** 잭팟 캡슐 발사 직후 호출 (BP에서 잭팟 연출 처리) */
    UFUNCTION(BlueprintImplementableEvent, Category = "VendingMachine|Jackpot")
    void OnCapsuleShotFired(int32 CapsuleTier);
    // CapsuleTier: 0 = Bronze, 1 = Silver, 2 = Gold

    /** 잭팟 스택 변경 시 호출 (BP에서 UI 업데이트) */
    UFUNCTION(BlueprintImplementableEvent, Category = "VendingMachine|Jackpot")
    void OnJackpotStacksChanged(int32 NewStacks, int32 MaxStacks);

protected:
    // ============================================================
    // EditDefaultsOnly 프로퍼티 (BP 에디터에서 설정)
    // ============================================================

    // --- 발사 ---
    /** 기본 발사 간격 (초). 공격속도 버프 없을 때의 간격. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|BasicAttack")
    float BaseFireInterval = 0.3f;

    /** 투사체 발사 소켓 태그 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|BasicAttack")
    FGameplayTag FireSocketTag;

    // --- 잭팟 캡슐 투사체 클래스 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
    int32 MaxJackpotStacks = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
    TSubclassOf<ADRProjectile> BronzeCapsuleClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
    TSubclassOf<ADRProjectile> SilverCapsuleClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
    TSubclassOf<ADRProjectile> GoldCapsuleClass;

    // --- 잭팟 캡슐 데미지 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
    float BronzeCapsuleDamage = 10.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
    float SilverCapsuleDamage = 30.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
    float GoldCapsuleDamage = 150.f;

    // --- 잭팟 캡슐 확률 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BronzeCapsuleChance = 0.5f;   // 50%

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SilverCapsuleChance = 0.4f;   // 40%
    // 금 캡슐 확률 = 1.0 - Bronze - Silver = 10%

private:
    int32 CurrentJackpotStacks = 0;
    FTimerHandle AutoFireTimerHandle;
    bool bIsFiring = false;

    /** 타이머 콜백: 한 발 발사 + 다음 발사 스케줄링 */
    void FireShotAndScheduleNext();

    /** 발사 로직 (서버 전용) - 스택에 따라 일반/캡슐 투사체 선택 */
    void ExecuteShot();

    /** 타겟 위치 계산 (카메라 에임 방향) */
    FVector CalculateTargetLocation() const;
};
```

### C++ 구현

```cpp
// ===================== StartAutoFire =====================
void UDRVendingMachineBasicAttack::StartAutoFire()
{
    if (bIsFiring) return;
    bIsFiring = true;

    // 첫 발 즉시 발사
    ExecuteShot();

    // 다음 발사 스케줄링
    const float Interval = GetCurrentFireInterval();
    GetWorld()->GetTimerManager().SetTimer(
        AutoFireTimerHandle, this,
        &UDRVendingMachineBasicAttack::FireShotAndScheduleNext,
        Interval, false);
}

// ===================== StopAutoFire =====================
void UDRVendingMachineBasicAttack::StopAutoFire()
{
    bIsFiring = false;
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AutoFireTimerHandle);
    }
}

// ===================== FireShotAndScheduleNext =====================
void UDRVendingMachineBasicAttack::FireShotAndScheduleNext()
{
    if (!bIsFiring) return;

    ExecuteShot();

    // 다음 발사 예약 (공격속도 변경 즉시 반영을 위해 매번 새로 스케줄링)
    const float Interval = GetCurrentFireInterval();
    GetWorld()->GetTimerManager().SetTimer(
        AutoFireTimerHandle, this,
        &UDRVendingMachineBasicAttack::FireShotAndScheduleNext,
        Interval, false);
}

// ===================== ExecuteShot =====================
void UDRVendingMachineBasicAttack::ExecuteShot()
{
    // 서버에서만 실행
    if (!GetAvatarActorFromActorInfo()->HasAuthority()) return;

    const FVector TargetLocation = CalculateTargetLocation();

    if (CurrentJackpotStacks >= MaxJackpotStacks)
    {
        // ===== 잭팟 발동 =====
        CurrentJackpotStacks = 0;

        const float Roll = FMath::FRand();

        TSubclassOf<ADRProjectile> SelectedClass;
        float SelectedDamage;
        int32 CapsuleTier;

        if (Roll < BronzeCapsuleChance)
        {
            SelectedClass = BronzeCapsuleClass;
            SelectedDamage = BronzeCapsuleDamage;
            CapsuleTier = 0;
        }
        else if (Roll < BronzeCapsuleChance + SilverCapsuleChance)
        {
            SelectedClass = SilverCapsuleClass;
            SelectedDamage = SilverCapsuleDamage;
            CapsuleTier = 1;
        }
        else
        {
            SelectedClass = GoldCapsuleClass;
            SelectedDamage = GoldCapsuleDamage;
            CapsuleTier = 2;
        }

        // 캡슐 투사체 스폰
        if (SelectedClass)
        {
            const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
                GetAvatarActorFromActorInfo(), FireSocketTag);
            FRotator Rotation = (TargetLocation - SocketLocation).Rotation();

            FTransform SpawnTransform;
            SpawnTransform.SetLocation(SocketLocation);
            SpawnTransform.SetRotation(Rotation.Quaternion());

            ADRProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRProjectile>(
                SelectedClass, SpawnTransform,
                GetOwningActorFromActorInfo(),
                Cast<APawn>(GetOwningActorFromActorInfo()),
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

            Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();
            Projectile->DamageEffectParams.BaseDamage = SelectedDamage;

            Projectile->FinishSpawning(SpawnTransform);
        }

        // Blueprint 콜백
        OnCapsuleShotFired(CapsuleTier);
        OnJackpotStacksChanged(CurrentJackpotStacks, MaxJackpotStacks);
    }
    else
    {
        // ===== 일반 공격 =====
        // 부모 클래스의 SpawnProjectile() 재사용 (ProjectileClass에 설정된 기본 투사체)
        SpawnProjectile(TargetLocation, FireSocketTag);

        CurrentJackpotStacks++;

        // Blueprint 콜백
        OnNormalShotFired();
        OnJackpotStacksChanged(CurrentJackpotStacks, MaxJackpotStacks);
    }
}

// ===================== GetCurrentFireInterval =====================
float UDRVendingMachineBasicAttack::GetCurrentFireInterval() const
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC) return BaseFireInterval;

    int32 BuffStacks = 0;
    FGameplayTagContainer BuffTagFilter;
    BuffTagFilter.AddTag(FDRGameplayTags::Get().Buff_VendingMachine_AttackSpeed);

    TArray<FActiveGameplayEffectHandle> ActiveEffects =
        ASC->GetActiveEffectsWithAllTags(BuffTagFilter);

    for (const FActiveGameplayEffectHandle& EffectHandle : ActiveEffects)
    {
        const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(EffectHandle);
        if (ActiveGE)
        {
            BuffStacks = ActiveGE->Spec.GetStackCount();
            break;
        }
    }

    const float SpeedMultiplier = 1.0f + (BuffStacks * 0.1f);
    return BaseFireInterval / SpeedMultiplier;
}

// ===================== CalculateTargetLocation =====================
FVector UDRVendingMachineBasicAttack::CalculateTargetLocation() const
{
    // 카메라 에임 방향으로 라인트레이스하여 타겟 위치 결정
    const AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor) return FVector::ZeroVector;

    const APlayerController* PC = Cast<APlayerController>(
        Cast<APawn>(AvatarActor)->GetController());
    if (!PC) return AvatarActor->GetActorForwardVector() * 5000.f + AvatarActor->GetActorLocation();

    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * 10000.f;

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(AvatarActor);

    if (GetWorld()->LineTraceSingleByChannel(HitResult, CameraLocation, TraceEnd, ECC_Visibility, Params))
    {
        return HitResult.ImpactPoint;
    }

    return TraceEnd;
}
```

### 2-2. Blueprint GA: `GA_VendingMachine_BasicAttack`

**경로**: `Content/Blueprints/AbilitySystem/VendingMachine/GA_VendingMachine_BasicAttack`

**부모 클래스**: `UDRVendingMachineBasicAttack`

Blueprint에서 `Event ActivateAbility`를 오버라이드하여 다음 흐름을 구성한다:

```
Event ActivateAbility
  │
  ├─ (선택적) Play Montage / VFX 시작 연출
  │
  └─ Call StartAutoFire()
       → C++에서 타이머 시작, 자동 연사 시작
       → 매 발사마다 OnNormalShotFired / OnCapsuleShotFired 이벤트 발생

Event OnNormalShotFired (BlueprintImplementableEvent)
  │
  └─ 일반 발사 VFX, SFX 재생 (총구 이펙트, 발사음 등)

Event OnCapsuleShotFired (int32 CapsuleTier) (BlueprintImplementableEvent)
  │
  ├─ CapsuleTier에 따라 분기 (Switch on Int)
  │   ├─ 0 (Bronze): 동 캡슐 발사 연출
  │   ├─ 1 (Silver): 은 캡슐 발사 연출
  │   └─ 2 (Gold):   금 캡슐 발사 연출 (화려한 이펙트)
  │
  └─ 잭팟 발동 SFX 재생

Event OnJackpotStacksChanged (int32 NewStacks, int32 MaxStacks)
  │
  └─ UI 위젯 업데이트 (잭팟 게이지 등)

Event InputReleased (또는 WaitInputRelease AbilityTask)
  │
  ├─ Call StopAutoFire()
  │    → C++에서 타이머 클리어
  │
  ├─ (선택적) Stop VFX / 종료 연출
  │
  └─ Call EndAbility()
```

**BP에서 설정할 프로퍼티 (디테일 패널)**:

| 프로퍼티 | 값 | 카테고리 |
|---------|-----|---------|
| `StartupInputTag` | `InputTag.LMB` | Input |
| `ProjectileClass` | `BP_VendingMachineProjectile` | (부모) |
| `DamageEffectClass` | 기존 공용 데미지 GE | (부모) |
| `Damage` | `30` (ScalableFloat) | (부모) Damage |
| `DamageType` | `Damage.Physical` | (부모) Damage |
| `BaseFireInterval` | `0.3` | VendingMachine\|BasicAttack |
| `FireSocketTag` | `CombatSocket.Weapon` | VendingMachine\|BasicAttack |
| `MaxJackpotStacks` | `5` | VendingMachine\|Jackpot |
| `BronzeCapsuleClass` | `BP_CapsuleBronze` | VendingMachine\|Jackpot |
| `SilverCapsuleClass` | `BP_CapsuleSilver` | VendingMachine\|Jackpot |
| `GoldCapsuleClass` | `BP_CapsuleGold` | VendingMachine\|Jackpot |
| `BronzeCapsuleDamage` | `10.0` | VendingMachine\|Jackpot |
| `SilverCapsuleDamage` | `30.0` | VendingMachine\|Jackpot |
| `GoldCapsuleDamage` | `150.0` | VendingMachine\|Jackpot |
| `BronzeCapsuleChance` | `0.5` | VendingMachine\|Jackpot |
| `SilverCapsuleChance` | `0.4` | VendingMachine\|Jackpot |

### 2-3. 동작 설명: LMB 홀드 연사

마우스 좌클릭(LMB)을 꾹 누르면 총처럼 연속 발사된다:

1. **LMB 누름** → ASC가 `InputTag.LMB`와 매칭되는 GA를 활성화
2. **ActivateAbility (BP)** → `StartAutoFire()` 호출
3. **StartAutoFire (C++)** → 첫 발 즉시 `ExecuteShot()` → 타이머 시작
4. **타이머 반복** → `BaseFireInterval`(0.3초) 간격으로 `ExecuteShot()` 반복 호출
   - 공격속도 버프가 있으면 간격이 줄어듦 (예: 스택 2 → 0.25초 간격)
   - 매 발사마다 타이머를 새로 설정하므로 버프 변경이 즉시 반영
5. **LMB 해제** → `InputReleased` 이벤트 (BP) → `StopAutoFire()` 호출 → 타이머 클리어 → `EndAbility()`

**잭팟 스택은 EndAbility 시 리셋하지 않는다** — 다음 LMB 홀드 시 이전 스택을 이어서 카운트한다.

---

## 3. 패시브: 잭팟 시스템 상세

### 잭팟 스택 관리

- **저장 위치**: 기본 공격 GA C++ 클래스의 `int32 CurrentJackpotStacks` 변수
- **증가**: 일반 투사체 발사 시 +1
- **소모**: 스택이 `MaxJackpotStacks`(5)에 도달하면 다음 발사 시 전부 소모하고 캡슐 발사
- **리셋 시점**: 잭팟 발동 시에만 0으로 리셋. GA 종료(LMB 해제) 시에는 리셋하지 않음
- **서버 권한**: `ExecuteShot()`이 `HasAuthority()` 체크 하에서만 실행되므로 스택도 서버에서만 관리

### 캡슐 확률 판정

`ExecuteShot()` 내부에서 `FMath::FRand()`로 0.0~1.0 범위의 난수를 생성:

```
Roll < 0.5              → 동 캡슐 (50%, 10 데미지)
0.5 ≤ Roll < 0.9        → 은 캡슐 (40%, 30 데미지)
0.9 ≤ Roll              → 금 캡슐 (10%, 150 데미지)
```

확률과 데미지는 모두 BP 에디터에서 조정 가능한 `UPROPERTY`로 노출.

### 캡슐 투사체 스폰 방식

캡슐 투사체 스폰은 부모 `SpawnProjectile()`을 재사용하지 않고 `ExecuteShot()` 내부에서 직접 스폰한다.

**이유**: 캡슐은 `ProjectileClass`(기본 투사체)와 다른 클래스를 사용하며, `BaseDamage`도 오버라이드해야 한다. 부모의 `SpawnProjectile()`은 항상 `ProjectileClass`와 `Damage` ScalableFloat를 사용하므로 캡슐에는 적합하지 않다.

**스폰 흐름**:
1. `SelectedClass` (동/은/금 캡슐 중 하나) 선택
2. `SpawnActorDeferred<ADRProjectile>(SelectedClass, ...)` 호출
3. `MakeDamageEffectParamsFromClassDefaults()`로 기본 데미지 파라미터 생성
4. `DamageEffectParams.BaseDamage`를 캡슐 데미지로 오버라이드
5. `FinishSpawning()` 호출

### 잭팟 스택 UI 연동

`OnJackpotStacksChanged` BlueprintImplementableEvent를 통해 BP에서 UI를 업데이트한다:
- 스택 변경될 때마다 `(현재 스택, 최대 스택)` 전달
- BP에서 `OverlayWidgetController`에 바인딩하거나 직접 위젯 갱신

---

## 4. 공격속도 버프 스킬

### 4-1. C++ 클래스: `UDRVendingMachineAttackSpeedBuff`

**파일**:
- `Source/DaeRune/Public/AbilitySystem/Abilities/DRVendingMachineAttackSpeedBuff.h`
- `Source/DaeRune/Private/AbilitySystem/Abilities/DRVendingMachineAttackSpeedBuff.cpp`

**상속**: `UDRGameplayAbility` (데미지를 주지 않으므로 `DRDamageGameplayAbility` 불필요)

기존 패턴에 따라 C++ 클래스는 최소한으로 구성. `ActivateAbility`를 오버라이드하지 않고 BlueprintCallable 도구만 제공한다. 하지만 이 스킬은 "GE 적용 후 즉시 EndAbility"로 매우 단순하므로, **BP에서 직접 GE를 적용하는 것만으로 충분**하다.

### 클래스 선언

```cpp
UCLASS()
class DAERUNE_API UDRVendingMachineAttackSpeedBuff : public UDRGameplayAbility
{
    GENERATED_BODY()

protected:
    /** 적용할 공격속도 버프 GE 클래스 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Skill")
    TSubclassOf<UGameplayEffect> AttackSpeedBuffEffect;
};
```

C++ 코드는 이것이 전부다. `AttackSpeedBuffEffect` 프로퍼티만 제공하고 나머지는 Blueprint에서 구현한다.

### 4-2. Blueprint GA: `GA_VendingMachine_AttackSpeedBuff`

**경로**: `Content/Blueprints/AbilitySystem/VendingMachine/GA_VendingMachine_AttackSpeedBuff`

**부모 클래스**: `UDRVendingMachineAttackSpeedBuff`

Blueprint에서 `Event ActivateAbility`를 오버라이드:

```
Event ActivateAbility
  │
  ├─ CommitAbility (CheckCost + ApplyCost)
  │   ├─ 실패 → EndAbility (bCancelled = true)
  │   └─ 성공 → 계속
  │
  ├─ Get AbilitySystemComponent (from OwnerActor)
  │
  ├─ MakeOutgoingGameplayEffectSpec (AttackSpeedBuffEffect)
  │
  ├─ ApplyGameplayEffectSpecToSelf
  │   → GE 스택 정책에 의해 자동으로 중첩/지속시간 리셋 처리
  │
  ├─ (선택적) 버프 적용 VFX/SFX 재생
  │
  └─ EndAbility (bCancelled = false)
```

**BP에서 설정할 프로퍼티**:

| 프로퍼티 | 값 |
|---------|-----|
| `StartupInputTag` | `InputTag.Q` |
| `WaterCost` | `50.0` |
| `AttackSpeedBuffEffect` | `GE_VendingMachine_AttackSpeedBuff` |

---

## 5. 공격속도 버프 방안 비교

### 문제 정의

스킬(Q) 사용 시 공격속도 10% 증가, 10초 지속, 중첩 가능, 중첩 시 지속시간 10초로 리셋.
기본 공격 GA의 타이머 발사 간격에 어떻게 반영할 것인가?

### 방안 A: GE 스택 카운트 직접 조회 (Modifier 없는 순수 태그 GE) — 추천

**개요**: GE에 Attribute Modifier 없이 `GrantedTags`만 설정. 기본 공격 GA가 매 발사마다 ASC에서 해당 GE의 스택 수를 직접 조회하여 발사 간격 계산.

**GE 설정**:
| 항목 | 값 |
|------|-----|
| Duration Policy | `Has Duration` (10초) |
| Stacking Type | `Aggregate by Target` |
| Stack Limit Count | 0 (무제한) |
| Stack Duration Refresh Policy | `Refresh on Successful Application` |
| Stack Expiration Policy | `Clear Entire Stack` |
| Modifiers | 없음 |
| GrantedTags | `Buff.VendingMachine.AttackSpeed` |

**기본 공격 GA에서 조회** (`GetCurrentFireInterval()`):
```cpp
// ASC에서 Buff.VendingMachine.AttackSpeed 태그 GE를 찾아 스택 수 확인
// 발사 간격 = BaseFireInterval / (1 + StackCount * 0.1)
```

**장점**:
- AttributeSet 수정 불필요, 기존 코드 영향 제로
- GE 스택 정책만으로 중첩/리셋/만료 자동 처리
- 자판기 로봇 전용 로직으로 완전 캡슐화
- 구현이 가장 단순

**단점**:
- 공격속도가 Attribute가 아니므로 다른 시스템에서 참조 어려움
- 다른 소스(디버프, 아이템 등)에서 공격속도를 수정하려면 확장 어려움

---

### 방안 B: AttackSpeed Attribute 추가 (GE Modifier로 직접 수정)

**개요**: `UDRAttributeSet`에 `AttackSpeed` Attribute 추가. GE Modifier가 이 Attribute를 직접 수정. 기본 공격 GA는 Attribute 값만 읽으면 됨.

**AttributeSet 변경**:
```cpp
UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackSpeed, Category = "Primary Attributes")
FGameplayAttributeData AttackSpeed;  // 기본값 1.0
ATTRIBUTE_ACCESSORS(UDRAttributeSet, AttackSpeed);
```

**GE Modifier 설정**:
- Attribute: `AttackSpeed`
- Op: `Additive`
- Magnitude: `+0.1` (스택당)

**기본 공격 GA에서 조회**:
```cpp
float AttackSpeed = AttrSet->GetAttackSpeed(); // 기본 1.0, 버프 시 1.1, 1.2 ...
return BaseFireInterval / FMath::Max(AttackSpeed, 0.1f);
```

**장점**:
- GAS 정식 패턴 완전 준수
- 다른 시스템에서 공격속도 참조/수정 용이 (UI, 디버프, 아이템 등)
- Attribute 변경 시 `OnRep`으로 자동 클라이언트 동기화
- GE 스택 조회 코드 불필요 (Attribute 값만 읽으면 됨)

**단점**:
- `UDRAttributeSet` 수정 필요 (보일러플레이트: GetLifetimeReplicatedProps, PreAttributeChange, OnRep 등)
- 모든 캐릭터가 `AttackSpeed` Attribute를 갖게 됨 (자판기 로봇만 사용하더라도)
- PrimaryAttributes GE에서 초기값 설정 필요

---

### 방안 C: GA 내부 float 변수 + GE Duration 감시

**개요**: GA에 `AttackSpeedMultiplier` 변수를 두고, 공격속도 버프 GA가 ASC의 GE 적용/제거 콜백을 통해 이 변수를 수정.

**장점**:
- AttributeSet 변경 불필요
- 자판기 로봇 전용으로 캡슐화

**단점**:
- GA 간 직접 참조 → 높은 결합도
- GE 만료 콜백 처리 복잡
- 지속시간 리셋, 스택 관리를 수동 구현
- 멀티플레이어 동기화 별도 처리 필요

---

### 방안 비교표

| 항목 | A: GE 스택 조회 | B: Attribute 추가 | C: GA 내부 변수 |
|------|-----------------|-------------------|-----------------|
| 기존 코드 수정 범위 | 없음 (태그만 추가) | AttributeSet 수정 | 없음 |
| 구현 복잡도 | 낮음 | 중 | 상 |
| 확장성 | 낮음 | 높음 | 낮음 |
| GAS 패턴 준수 | 부분적 | 완전 | 비표준 |
| UI 연동 용이성 | 중 | 상 | 하 |
| 멀티플레이어 동기화 | 자동 (GE 리플리케이션) | 자동 (Attribute 리플리케이션) | 수동 |
| 다른 캐릭터 영향 | 없음 | Attribute 추가됨 | 없음 |
| 스택/지속시간 관리 | GE 정책 자동 | GE 정책 자동 | 수동 |

### 추천: 방안 A

현재 공격속도를 다른 소스에서 수정할 계획이 없으므로 가장 단순한 방안 A를 채택.
향후 확장 필요 시 방안 B로 마이그레이션 가능 (GE 스택 조회 코드만 Attribute 조회로 교체).

---

## 6. GE: `GE_VendingMachine_AttackSpeedBuff`

**Blueprint GE**: `Content/Blueprints/AbilitySystem/VendingMachine/GE_VendingMachine_AttackSpeedBuff`

### GE 상세 설정

| 항목 | 값 | 설명 |
|------|-----|------|
| **Duration Policy** | `Has Duration` | 지속시간 있는 효과 |
| **Duration Magnitude** | `Scalable Float = 10.0` | 10초 지속 |
| **Stacking Type** | `Aggregate by Target` | 대상 기준 스택 집계 |
| **Stack Limit Count** | `0` (무제한) | 스택 상한 없음 |
| **Stack Duration Refresh Policy** | `Refresh on Successful Application` | 새 스택 시 10초 리셋 |
| **Stack Period Reset Policy** | `Reset on Successful Application` | Period 리셋 |
| **Stack Expiration Policy** | `Clear Entire Stack` | 만료 시 모든 스택 제거 |
| **Modifiers** | 없음 | Attribute 수정하지 않음 |
| **Granted Tags** | `Buff.VendingMachine.AttackSpeed` | 스택 조회용 태그 |

### 스택 동작 시나리오

```
t=0s   Q 사용 → 스택 1, 10초 타이머 시작 → 발사간격 0.3/1.1 ≈ 0.273초
t=3s   Q 사용 → 스택 2, 10초로 리셋      → 발사간격 0.3/1.2 = 0.250초
t=8s   Q 사용 → 스택 3, 10초로 리셋      → 발사간격 0.3/1.3 ≈ 0.231초
t=18s  지속시간 만료 → 모든 스택 제거     → 발사간격 0.3초 (원래대로)
```

---

## 7. 투사체 목록

### 총 4종 투사체 Blueprint

모두 기존 `ADRProjectile`을 부모 클래스로 사용. C++ 서브클래스 불필요.

| Blueprint 이름 | 용도 | 데미지 | 비고 |
|----------------|------|--------|------|
| `BP_VendingMachineProjectile` | 기본 공격 투사체 | 30 | GA의 `ProjectileClass`에 설정 |
| `BP_CapsuleBronze` | 잭팟 동 캡슐 | 10 | 50% 확률, 캡슐 모델 A |
| `BP_CapsuleSilver` | 잭팟 은 캡슐 | 30 | 40% 확률, 캡슐 모델 B |
| `BP_CapsuleGold` | 잭팟 금 캡슐 | 150 | 10% 확률, 캡슐 모델 C |

**경로**: `Content/Blueprints/AbilitySystem/VendingMachine/`

### 각 투사체 BP 설정

**공통 설정**:
- `Sphere` (USphereComponent): 콜리전 반경
- `ProjectileMovement` (UProjectileMovementComponent): 속도, 중력 스케일
- `LifeSpan`: 15초 (기본값)

**차별화 설정**:
| 항목 | 기본 투사체 | 동 캡슐 | 은 캡슐 | 금 캡슐 |
|------|-----------|---------|---------|---------|
| Mesh | 기본 투사체 메시 | 캡슐 모델 A | 캡슐 모델 B | 캡슐 모델 C |
| ImpactEffect | 기본 히트 이펙트 | 동색 이펙트 | 은색 이펙트 | 금색 이펙트 |
| ImpactSound | 기본 히트 사운드 | 동 히트음 | 은 히트음 | 금 히트음 |

---

## 8. 새로 추가/수정할 파일 목록

### 새 C++ 파일 (4개)

| 파일 | 내용 |
|------|------|
| `Source/DaeRune/Public/AbilitySystem/Abilities/DRVendingMachineBasicAttack.h` | BlueprintCallable 도구 함수 + UPROPERTY 선언 |
| `Source/DaeRune/Private/AbilitySystem/Abilities/DRVendingMachineBasicAttack.cpp` | 타이머 관리, 투사체 스폰, 잭팟 로직, 공격속도 조회 |
| `Source/DaeRune/Public/AbilitySystem/Abilities/DRVendingMachineAttackSpeedBuff.h` | AttackSpeedBuffEffect UPROPERTY만 선언 |
| `Source/DaeRune/Private/AbilitySystem/Abilities/DRVendingMachineAttackSpeedBuff.cpp` | 생성자만 (거의 빈 파일) |

### 수정 C++ 파일 (2개)

| 파일 | 변경 내용 |
|------|----------|
| `Source/DaeRune/Public/DRGameplayTags.h` | 4개 태그 멤버 추가 |
| `Source/DaeRune/Private/DRGameplayTags.cpp` | 4개 태그 등록 |

### 새 Blueprint 에셋 (7개)

| 에셋 | 부모 클래스 | 설명 |
|------|------------|------|
| `GA_VendingMachine_BasicAttack` | `UDRVendingMachineBasicAttack` | 기본 공격 GA (ActivateAbility BP 구현) |
| `GA_VendingMachine_AttackSpeedBuff` | `UDRVendingMachineAttackSpeedBuff` | 공격속도 버프 GA (ActivateAbility BP 구현) |
| `GE_VendingMachine_AttackSpeedBuff` | `UGameplayEffect` | 공격속도 버프 GE (스택 정책) |
| `BP_VendingMachineProjectile` | `ADRProjectile` | 기본 공격 투사체 |
| `BP_CapsuleBronze` | `ADRProjectile` | 동 캡슐 투사체 |
| `BP_CapsuleSilver` | `ADRProjectile` | 은 캡슐 투사체 |
| `BP_CapsuleGold` | `ADRProjectile` | 금 캡슐 투사체 |

**경로**: 모두 `Content/Blueprints/AbilitySystem/VendingMachine/`

---

## 9. 데이터 에셋 설정 (에디터에서)

### `PlayerCharacterClassInfo` 데이터 에셋

`CharacterClassInformation` 맵에 `VendingMachineRobot` 항목 추가:

```
VendingMachineRobot:
  PrimaryAttributes: GE_VendingMachine_PrimaryAttributes (MaxHealth, MaxWater, MoveSpeed)
  VitalAttributes: GE_VendingMachine_VitalAttributes (초기 Health, Water)
  StartupAbilities:
    - GA_VendingMachine_BasicAttack     (StartupInputTag = InputTag.LMB)
    - GA_VendingMachine_AttackSpeedBuff (StartupInputTag = InputTag.Q)
  DeathAbilities: (기존 공통 사망 GA 재사용)
```

`CharacterBPClasses` 맵에 `VendingMachineRobot → BP_VendingMachineCharacter` 추가

---

## 10. 전체 흐름 요약

### 기본 공격 + 잭팟 흐름

```
[LMB 꾹 누름]
  → ASC가 InputTag.LMB 매칭 GA 활성화
  → Event ActivateAbility (Blueprint)
    → StartAutoFire() 호출 (C++ BlueprintCallable)

StartAutoFire() (C++):
  → ExecuteShot() 즉시 1회 (첫 발)
  → SetTimer(GetCurrentFireInterval()) → 0.3초(÷공격속도) 후 FireShotAndScheduleNext

FireShotAndScheduleNext() (C++ 타이머 콜백, 반복):
  → ExecuteShot()
  → SetTimer(GetCurrentFireInterval()) → 다음 발사 예약

ExecuteShot() (C++, 서버 전용):
  CurrentJackpotStacks < MaxJackpotStacks(5)?
    ├─ Yes → SpawnProjectile() (부모 함수, 기본 투사체 30 데미지)
    │        → CurrentJackpotStacks++
    │        → OnNormalShotFired() → BP에서 발사 VFX/SFX
    │        → OnJackpotStacksChanged(스택, 최대) → BP에서 UI 갱신
    │
    └─ No  → CurrentJackpotStacks = 0
             → FMath::FRand() 확률 판정
             ├─ 50% → SpawnActorDeferred(BronzeCapsule, 10 데미지)
             ├─ 40% → SpawnActorDeferred(SilverCapsule, 30 데미지)
             └─ 10% → SpawnActorDeferred(GoldCapsule, 150 데미지)
             → OnCapsuleShotFired(Tier) → BP에서 잭팟 연출
             → OnJackpotStacksChanged(0, 5) → BP에서 UI 갱신

[LMB 해제]
  → Event InputReleased (Blueprint)
    → StopAutoFire() 호출 (C++ BlueprintCallable)
    → EndAbility()
  (JackpotStacks는 리셋하지 않음, 다음 LMB에서 이어서 카운트)
```

### 공격속도 버프 흐름

```
[Q 누름]
  → ASC가 InputTag.Q 매칭 GA 활성화
  → Event ActivateAbility (Blueprint)
    → CommitAbility()
      → CheckCost(): Water >= 50 확인
      → ApplyCost(): Water 50 소모
    → MakeOutgoingGameplayEffectSpec(AttackSpeedBuffEffect)
    → ApplyGameplayEffectSpecToSelf()
      ├─ 첫 사용:  GE 생성, 스택 1, 10초 타이머
      ├─ 중첩 사용: 스택 +1, 지속시간 10초 리셋
      └─ 만료:     모든 스택 일괄 제거
    → (선택적) 버프 VFX/SFX
    → EndAbility() (즉발)

[기본 공격 발사 시 공격속도 반영]
  → GetCurrentFireInterval() (C++)
    → ASC에서 Buff.VendingMachine.AttackSpeed 태그 GE 검색
    → 스택 수 조회
    → return BaseFireInterval / (1 + 스택수 × 0.1)

  발사 간격 예시:
    스택 0 → 0.3 / 1.0 = 0.300초
    스택 1 → 0.3 / 1.1 ≈ 0.273초
    스택 2 → 0.3 / 1.2 = 0.250초
    스택 3 → 0.3 / 1.3 ≈ 0.231초
```

---

## 11. 멀티플레이어 고려사항

| 항목 | 서버 | 클라이언트 |
|------|------|-----------|
| 투사체 스폰 | 서버에서만 (`HasAuthority()`) | `ADRProjectile` 자동 리플리케이트 |
| 잭팟 스택 | 서버에서만 관리 | `OnJackpotStacksChanged` 이벤트로 UI 갱신 |
| 캡슐 확률 결정 | 서버에서 `FMath::FRand()` | 투사체 리플리케이션으로 결과 전달 |
| 공격속도 버프 GE | 서버에서 적용 | GE 리플리케이션으로 자동 동기화 |
| Water 소모 | 서버에서 처리 | Attribute 리플리케이션으로 동기화 |
| 발사 간격 | 서버에서 타이머 관리 | 클라이언트는 투사체 수신만 처리 |

---

## 12. 구현 순서

### Phase 1: C++ 코드 (컴파일 필요)
1. **Gameplay Tags 추가** (`DRGameplayTags.h/.cpp`) — 4개 태그
2. **기본 공격 GA C++ 클래스** (`DRVendingMachineBasicAttack.h/.cpp`)
   - BlueprintCallable: `StartAutoFire()`, `StopAutoFire()`
   - BlueprintPure: `GetJackpotStacks()`, `IsJackpotReady()`, `GetCurrentFireInterval()`
   - BlueprintImplementableEvent: `OnNormalShotFired()`, `OnCapsuleShotFired()`, `OnJackpotStacksChanged()`
   - Private: `ExecuteShot()`, `FireShotAndScheduleNext()`, `CalculateTargetLocation()`
3. **공격속도 버프 GA C++ 클래스** (`DRVendingMachineAttackSpeedBuff.h/.cpp`)
   - `AttackSpeedBuffEffect` UPROPERTY만 선언
4. **빌드 및 컴파일 확인**

### Phase 2: Blueprint 에셋 (에디터에서)
5. **투사체 BP 4종 생성** — `ADRProjectile` 기반, 메시/이펙트/사운드 설정
6. **GE_VendingMachine_AttackSpeedBuff 생성** — 스택 정책 설정
7. **GA_VendingMachine_BasicAttack BP 생성**
   - ActivateAbility에서 `StartAutoFire()` 호출
   - InputReleased에서 `StopAutoFire()` → `EndAbility()` 호출
   - `OnNormalShotFired`, `OnCapsuleShotFired`, `OnJackpotStacksChanged` 이벤트 구현
   - 디테일 패널에서 프로퍼티 설정 (투사체 클래스, 데미지, 확률, 간격 등)
8. **GA_VendingMachine_AttackSpeedBuff BP 생성**
   - ActivateAbility에서 `CommitAbility` → `ApplyGameplayEffectSpecToSelf` → `EndAbility`
9. **PlayerCharacterClassInfo에 VendingMachineRobot 추가**
10. **캐릭터 BP 생성 및 메시/소켓 설정**

### Phase 3: 테스트
11. **기능 테스트**
    - LMB 홀드 시 0.3초 간격 연사 확인
    - LMB 해제 시 발사 즉시 중지 확인
    - 잭팟 5스택 후 캡슐 발사 확인 (스택 리셋, 다음 발사부터 다시 카운트)
    - 캡슐 확률 분포 확인 (다수 시행)
    - 캡슐별 데미지 차이 확인 (10, 30, 150)
    - Q 스킬 사용 → 발사 간격 감소 확인
    - Q 스킬 중첩 → 추가 감소 + 지속시간 리셋 확인
    - 버프 만료 후 발사 간격 원복 확인
    - Water 50 미만 시 Q 스킬 사용 불가 확인
    - 멀티플레이어: 투사체 리플리케이션, GE 동기화 확인
