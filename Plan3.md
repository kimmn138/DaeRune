# Plan3: 로봇 청소기 (RobotVacuum) 플레이어 캐릭터 구현 계획

> 3번째 플레이어 캐릭터 "로봇 청소기"의 상세 구현 계획.
> 기존 캐릭터(Gardener, VendingMachine)의 구현 패턴을 분석하여 동일한 아키텍처(클래스 상속, GAS, 리플리케이션 패턴)로 설계했다.

---

## 0. 요구사항 요약

| 항목 | 내용 |
|---|---|
| **패시브: 탑승** | 다른 플레이어가 F키로 로봇 청소기 위에 탑승. 탑승자는 직접 이동 불가, 청소기를 따라 이동. 점프키로 하차. 로봇청소기끼리 탑쌓기 가능(단, 이미 위에 누가 타 있는 청소기에는 탑승 불가 → 3이 2에 타고, 2가 1에 타는 순서만 허용). 청소기/탑승자 중 누가 피격되든 둘 다 데미지 |
| **일반 공격 (LMB): 공기탄** | 짧게 누르면 기본탄 즉시 발사. 꾹 누르면 충전(0.5초당 1단계, 최대 3단계=1.5초). 단계에 따라 피해량/탄속 증가. 투사체는 2초 유지 후 서서히 소멸. 3단계 = 강화탄: 아군/적 무관 명중 대상 넉백 + 시전자는 발사 반대 방향으로 반동(발밑에 쏘면 로켓 점프). 예외: 돌진 상태에서는 전방 발사 시 뒤로 안 밀리지만, 아래로 쏘는 점프는 가능 |
| **1번 스킬 (Q): 더블 점프** | 물 30 소모, 아래로 물 분사하며 점프. 공중 사용 가능하되 공중 사용 후에는 착지 전까지 재사용 불가(지상 Q → 공중 Q = 총 2회 / 스페이스 점프 시작 → 공중 Q 1회만). 착지 지점 기준 Z 무관 반지름 200 이내 적에게 20, 400 이내 적에게 5 데미지 |
| **2번 스킬 (RMB): 돌진** | 홀드 중 충전(이동 가능). 게이지 5칸, 0.5초당 1칸. 1칸당 물 20 소모 + 이속버프 20% 스택(버튼 뗀 후 적용). 뗀 후 1초당 게이지 1칸/버프 20%씩 감소. 버프 중 적/플레이어/지형 충돌 시 게이지 0, 버프 전부 제거, 적은 게이지당 10, 타 플레이어는 게이지당 5 데미지. 게이지 5칸에 떼면 **지속 돌진**: 벽/적/플레이어 충돌 or 브레이크(S) 전까지 자동 전진, 게이지/버프 유지. 충돌 시 게이지 0 + 버프 제거 + **본인 50 데미지** (충돌 대상 데미지는 일반 돌진과 동일) |

---

## 1. 기존 시스템 분석 (설계 근거)

새 캐릭터의 모든 기능은 아래 검증된 기존 패턴을 그대로 따른다.

### 1.1 캐릭터 클래스 등록 흐름
- `EPlayerCharacterClass` (`AbilitySystem/Data/CharacterClassInfo.h:28`) — `Gardener, VendingMachine, Count(Hidden)`. 대기실 클래스 순환(`ADRPlayerController::ServerRequestChangeClass_Implementation`, `DRPlayerController.cpp:1246`)은 `% EPlayerCharacterClass::Count` 모듈로 연산이므로 **enum에 값만 추가하면 자동으로 선택 순환에 포함**된다.
- `UPlayerCharacterClassInfo` 데이터 에셋: `CharacterClassInformation` 맵(Primary/Vital GE, StartupAbilities, SkillIconWidgetClass, CharacterInfoWidgetClass) + `CharacterBPClasses` 맵(클래스 → 캐릭터 BP).
- 초기화 흐름: `ADRCharacter::InitAbilityActorInfo()` → `PlayerState->GetSelectedPlayerClass()` 반영 → `UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes()` / `GivePlayerStartupAbilities()`.

### 1.2 GA 상속 구조
```
UDRGameplayAbility            (StartupInputTag, WaterCost, CheckCost/ApplyCost — ExecCalc_WaterCost)
└─ UDRDamageGameplayAbility   (DamageEffectClass, Damage, Knockback 파라미터, CauseDamage(), MakeDamageEffectParamsFromClassDefaults())
   └─ UDRProjectileSpell      (ProjectileClass, SpawnProjectile() — 서버 전용 스폰 + DamageEffectParams 주입)
      ├─ UDRSeedCannon              (정원사: 각도/속도 커스텀 발사)
      └─ UDRVendingMachineBasicAttack (자판기: 타이머 연사, 스택, BlueprintImplementableEvent로 BP 연출 콜백)
```
- 홀드형 입력 처리 선례: `UDRVendingMachineBasicAttack` — ActivateAbility(BP)에서 `StartAutoFire()`, InputReleased(BP)에서 `StopAutoFire()`. **충전형 공기탄/돌진도 동일하게 "Pressed에서 GA 활성화 → C++ 타이머 → InputReleased에서 해소" 구조**를 쓴다.
- 채널링 + 자기 슬로우 GE 선례: `UDRWaterPump` (`SlowSelfEffectClass` 적용/제거, `EndAbility`에서 정리).

### 1.3 투사체
- `ADRProjectile` (`Actor/DRProjectile.h`): Sphere 오버랩 → 서버에서 `UDRAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams)`, `KnockbackChance` 굴림 후 `KnockbackForce` 세팅, `IsNotFriend()`로 아군 무시. `LifeSpan` 프로퍼티 있음.
- 파생 선례: `ADRSeedProjectile` — 내/외 반경 차등 효과(InnerRadius 200 / OuterRadius 400), 멀티캐스트 폭발 연출. **더블 점프의 200/400 차등 데미지도 이 패턴 재사용.**
- 넉백 전달 경로: `FDamageEffectParams.KnockbackForce` → `FDRGameplayEffectContext` → 타겟 AttributeSet의 `PostGameplayEffectExecute`에서 `LaunchCharacter()` (플레이어: `DRPlayerAttributeSet.cpp:272 ApplyHitReactAndKnockback`, 적: `DREnemyAttributeSet.cpp:159`).

### 1.4 입력
- GAS 입력: `UDRInputConfig`(InputTag ↔ InputAction) → `ADRPlayerController::AbilityInputTagPressed/Held/Released` → `UDRAbilitySystemComponent`(InputTag 캐시). 기존 태그 `InputTag.LMB / RMB / Q / E` 재사용.
- **F키(InteractAction)는 GAS 밖**: `ADRPlayerController::HandleInteract()` (`DRPlayerController.cpp:902`) — 우선순위(부품 > 사이트 > 드롭) 후 `ServerRequestInteract(AActor*)` 통합 RPC. 서버는 포인터를 신뢰하지 않고 거리/상태 재검증. **탑승도 이 통합 RPC에 분기 추가.**
- 점프: `StartJump/StopJump` (PlayerController). **하차 판정을 여기에 추가.**
- 상호작용 UI: `IDRInteractable::SetInteractionUIVisible(bool)` — 로컬 코스메틱, RPC 없음. 감지는 근접 오버랩(후보 등록) + 시점 라인트레이스(`FindPartByLineTrace`, `SetInteractableDetectionEnabled` 공용 함수).

### 1.5 이동속도 / 버프
- `MoveSpeed` 어트리뷰트 변경 → `OnMoveSpeedChanged` → `CharacterMovement->MaxWalkSpeed` 자동 반영 (`DRCharacter.cpp:794-800`). **이속버프는 MoveSpeed에 Multiplicative Modifier를 가진 스택형 GE로 구현하면 CMC 반영이 공짜.**
- 스택형 버프 조회 선례: `UDRVendingMachineBasicAttack::GetCurrentFireInterval()` — `GetActiveEffectsWithAllTags(Buff_VendingMachine_AttackSpeed)` → `Spec.GetStackCount()`.

### 1.6 돌진/충돌 선례 — ADRArmadilloEnemy (가장 중요한 참조)
`Character/DRArmadilloEnemy.h`: **C++는 이동(TickRollCharge)·충돌 감지(DetectRollCollision: 전방 SphereTrace)만 담당하고, 데미지/스턴은 델리게이트(`FOnRollImpact`)로 GA(BP)에 넘긴다.** 스턱 감지(StuckTimeLimit), 복제 플래그(`bIsRolling` RepNotify로 루프 사운드 동기화), 충돌 임팩트 멀티캐스트 사운드까지 갖춘 완성형 패턴. **로봇 청소기 돌진은 이 구조를 플레이어 버전으로 이식한다.**

### 1.7 물 소모 / AoE / 데미지 공유 관련
- 물 소모: `UDRGameplayAbility::WaterCost` + `ExecCalc_WaterCost` (CheckCost/ApplyCost 자동). 반복 소모가 필요하면 `UDRAbilitySystemLibrary::ApplyEffectSpecWithSetByCaller(ASC, Spec, Water_SetByCaller_Reduction, -20)` 사용.
- AoE: `UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius()` + `IsNotFriend()`.
- 플레이어 피격 파이프라인: `UDRPlayerAttributeSet::HandleIncomingDamage()` — 컨테이너 분배, 부품 소지 시 1.5배+강제 드롭(`CarryingPartDamageModifier`), `ApplyHitReactAndKnockback`. **탑승 데미지 공유 훅은 이 함수 뒤에 단다.**
- UI 갱신 신호 선례: `UDRAbilitySystemComponent::NotifyVendingMachineStacksChanged` → `ClientVendingMachineStacksChanged` → 델리게이트 → 위젯. **충전 단계/돌진 게이지 UI도 동일 패턴.**
- 클래스별 밸런스: `UGameBalanceConfig` 데이터 에셋 — 로봇 청소기 수치도 여기 구조체로 추가 가능(선택).

---

## 2. 신규/수정 파일 전체 목록

### 2.1 신규 C++ 파일
| 파일 | 내용 |
|---|---|
| `Public/Character/DRRobotVacuumCharacter.h` / `Private/Character/DRRobotVacuumCharacter.cpp` | `ADRCharacter` 파생. 탑승 마운트 측 상태(RiderOnTop, RideAttachPoint), 돌진 이동/충돌 감지, 지속 돌진 Tick, 공중 점프 소모 플래그(Landed 리셋) |
| `Public/AbilitySystem/Abilities/DRVacuumAirShot.h` / `.cpp` | `UDRProjectileSpell` 파생. 충전(0.5초/단계, 최대 3), 단계별 발사, 3단계 넉백/반동 |
| `Public/AbilitySystem/Abilities/DRVacuumJetJump.h` / `.cpp` | `UDRDamageGameplayAbility` 파생. WaterCost 30, 점프 임펄스, 200/400 2D 반경 차등 데미지, 공중 사용 제한 |
| `Public/AbilitySystem/Abilities/DRVacuumDash.h` / `.cpp` | `UDRDamageGameplayAbility` 파생. 게이지 충전 타이머, 이속버프 GE 적용/감쇠, 충돌 델리게이트 수신 → 데미지 처리, 지속 돌진 제어 |
| `Public/Actor/DRVacuumAirProjectile.h` / `.cpp` | `ADRProjectile` 파생. 2초 유지 후 페이드 소멸, 강화탄일 때 아군에게도 넉백(데미지 없이) |

### 2.2 수정 C++ 파일
| 파일 | 수정 내용 |
|---|---|
| `AbilitySystem/Data/CharacterClassInfo.h` | `EPlayerCharacterClass`에 `RobotVacuum` 추가 (**반드시 `Count` 앞에**) |
| `DRGameplayTags.h/.cpp` | §3의 태그 추가 |
| `Character/DRCharacter.h/.cpp` | 라이더 측 상태 `MountedOn`(RepNotify), `ServerNotifyTookDamage` 데미지 공유 훅, 탑승 중 이동 차단 |
| `Player/DRPlayerController.h/.cpp` | `HandleInteract()`에 탑승 분기, `ServerRequestInteract`에 RobotVacuum 검증 분기, `StartJump()`에 하차 분기, `Move()`에 탑승 중 이동 차단 + 지속 돌진 S 브레이크 감지, 라인트레이스에 청소기 후보 추가 |
| `AbilitySystem/DRPlayerAttributeSet.cpp` | `HandleIncomingDamage` 말미에 탑승 데미지 공유 훅 호출 (재전파 방지 태그 검사) |
| `AbilitySystem/DRAbilitySystemComponent.h/.cpp` | 게이지/충전 단계 UI 노티파이 델리게이트 추가 (자판기 패턴 일반화) |
| `AbilitySystem/Data/GameBalanceConfig.h` | (선택) `FRobotVacuumConfig` 구조체 추가 |

### 2.3 신규 콘텐츠(에디터 작업)
- `Content/Blueprints/Character/BP_RobotVacuumCharacter` (부모: `ADRRobotVacuumCharacter`) — 메시, RideAttachPoint 위치, 카메라, DeathMontages
- `GE_PrimaryAttributes_RobotVacuum`, `GE_VitalAttributes_RobotVacuum` (기존 클래스 GE 복제 후 수치 조정)
- `GA_VacuumAirShot`, `GA_VacuumJetJump`, `GA_VacuumDash` (각 C++ GA의 BP 파생, StartupInputTag = `InputTag.LMB / Q / RMB`)
- `GE_VacuumDashSpeedBuff` (Infinite, 스택형 최대 5, MoveSpeed ×1.2/스택, AssetTag `Buff.RobotVacuum.DashSpeed`)
- `GE_VacuumDashSelfDamage` (Instant, 자해 50)
- `BP_VacuumAirProjectile` (단계별 머티리얼/나이아가라)
- `DA_PlayerCharacterClassInfo`(기존 에셋)에 RobotVacuum 항목 + `CharacterBPClasses` 매핑 + StartupAbilities 3종 등록
- `WBP_SkillIcons_RobotVacuum`, `WBP_CharacterInfo_RobotVacuum` (Tab 설명창), 탑승 프롬프트 위젯(WBP_InteractionPrompt 재사용)
- 사운드/나이아가라: 충전 루프, 발사(단계별), 물 분사 점프, 돌진 루프, 충돌 임팩트
- **애니메이션**: `ABP_VacuumCleaner`, `BS_VC_Walk`, `AO_VC_Aim`, 몽타주 5종(`AM_VC_HitReact` / `AM_VC_Death` / `AM_VC_EnhancedAttack` / `AM_VC_Dash_Stop` / `AM_VC_Dash_Crush`) — **상세 설계는 §13**

---

## 3. 게임플레이 태그 추가 (`FDRGameplayTags`)

```
Abilities.RobotVacuum.AirShot        // 공기탄 GA 식별
Abilities.RobotVacuum.JetJump        // 더블 점프 GA 식별
Abilities.RobotVacuum.Dash           // 돌진 GA 식별

Buff.RobotVacuum.DashSpeed           // 돌진 이속버프 GE 태그 (스택 조회용)

State.Riding                         // 탑승 중(라이더에게 부여) — 이동/점프 GA 차단 근거
State.RobotVacuum.SustainedDash      // 지속 돌진 중 (강화탄 반동 예외 판정용)

Damage.MountShared                   // 탑승 공유 데미지 식별 (SourceAbilityTags에 실어 재전파 방지)

GameplayCue.Skill.VacuumAirShot
GameplayCue.Skill.VacuumJetJump
GameplayCue.Skill.VacuumDash
```
- 등록 위치: `InitializeNativeGameplayTags()` — 기존 `Abilities_VendingMachine_BasicAttack` 등록 코드 바로 아래에 동일 형식으로 추가.
- 입력 태그는 기존 `InputTag.LMB / RMB / Q` 재사용 (신규 불필요).

---

## 4. Phase A — 클래스 등록 + 캐릭터 스켈레톤

### 4.1 enum / 데이터 에셋
1. `EPlayerCharacterClass`에 `RobotVacuum` 추가(`VendingMachine` 다음, `Count` 앞). → 대기실 좌우 순환·`InitializePlayerDefaultAttributes`·`GivePlayerStartupAbilities`·스폰(`CharacterBPClasses`) 전부 기존 코드 그대로 동작.
2. `DA_PlayerCharacterClassInfo`에 RobotVacuum 엔트리 작성 (Primary/Vital GE, StartupAbilities는 Phase C~E에서 채움).

### 4.2 `ADRRobotVacuumCharacter : ADRCharacter`
```cpp
UCLASS()
class DAERUNE_API ADRRobotVacuumCharacter : public ADRCharacter
{
    GENERATED_BODY()
public:
    ADRRobotVacuumCharacter();
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(...) const override;
    virtual void Landed(const FHitResult& Hit) override;                  // 공중 Q 리셋
    virtual void MulticastHandleDeath_Implementation(const FVector&) override; // 사망 시 라이더 강제 하차

    // ===== 탑승(마운트 측) =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mount")
    TObjectPtr<USceneComponent> RideAttachPoint;      // 메시 상단, BP에서 위치 조정

    UPROPERTY(ReplicatedUsing = OnRep_RiderOnTop, BlueprintReadOnly, Category = "Mount")
    TObjectPtr<ADRCharacter> RiderOnTop;              // 내 위에 탄 캐릭터 (nullptr = 빈자리)

    bool CanBeMountedBy(ADRCharacter* Candidate) const; // 서버 검증 (§5.2)
    void MountRider(ADRCharacter* Rider);               // 서버 전용
    void DismountRider(bool bLaunchOff);                // 서버 전용

    UFUNCTION() void OnRep_RiderOnTop();

    // IDRInteractable — 탑승 프롬프트 UI
    virtual void SetInteractionUIVisible(bool bShow) override; // ※ ADRCharacter가 미구현이므로 이 클래스에서 인터페이스 상속

    // ===== 돌진(이동/충돌 담당 — GA와 협업, Armadillo 패턴) =====
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDashImpact, AActor*, HitActor, const FHitResult&, Hit);
    UPROPERTY(BlueprintAssignable) FOnDashImpact OnDashImpact;

    UPROPERTY(ReplicatedUsing = OnRep_SustainedDash, BlueprintReadOnly, Category = "Dash")
    bool bSustainedDash = false;                       // 지속 돌진 (RepNotify: 루프 사운드/VFX)

    void SetDashCollisionEnabled(bool bEnabled);       // GA가 버프 적용/해제 시 호출 (서버)
    void SetSustainedDash(bool bEnabled);              // GA가 호출 (서버)

    // ===== 더블 점프 =====
    bool bAirJumpUsed = false;                         // 서버 전용, Landed()에서 리셋

protected:
    virtual void BeginPlay() override;

    UFUNCTION() void OnCapsuleHit(UPrimitiveComponent*, AActor* OtherActor,
                                  UPrimitiveComponent*, FVector, const FHitResult& Hit);

    // 근접 감지 스피어 (부품 감지 패턴과 동일 — PC의 SetInteractableDetectionEnabled로 후보 등록)
    UPROPERTY(VisibleAnywhere, Category = "Mount")
    TObjectPtr<USphereComponent> MountDetectionSphere; // 반경 ~300

    // 탑승 프롬프트 위젯 (로컬 코스메틱)
    UPROPERTY(VisibleAnywhere, Category = "Mount")
    TObjectPtr<UWidgetComponent> MountPromptWidget;

    bool bDashCollisionArmed = false;                  // 서버: 버프 활성 중에만 충돌 판정
    float DashImpactMinSpeed = 700.f;                  // 오판 방지 최소 속도 (EditDefaultsOnly)
};
```
- `Tick`: `bSustainedDash && HasAuthority()`일 때 `AddMovementInput(GetActorForwardVector(), 1.f)` 강제. (조향: 컨트롤러 회전을 따르므로 마우스로 방향 조절 가능 — §7.4 결정사항 참조)
- `OnCapsuleHit`: 서버에서만. `bDashCollisionArmed && 속도 >= DashImpactMinSpeed && (Pawn 또는 수직면(ImpactNormal.Z < 0.7)의 WorldStatic)` 조건이면 `OnDashImpact.Broadcast()`. 바닥 착지가 충돌로 오판되지 않도록 노말 검사 필수.
- 참고: 캐릭터 자체가 `IDRInteractable`을 구현하도록 `ADRRobotVacuumCharacter`에 인터페이스 추가 (`class ADRRobotVacuumCharacter : public ADRCharacter, public IDRInteractable`).

### 4.3 `ADRCharacter` 공통 수정 (라이더 측 — 어떤 클래스든 탑승 가능하므로)
```cpp
// DRCharacter.h 에 추가
UPROPERTY(ReplicatedUsing = OnRep_MountedOn, BlueprintReadOnly, Category = "Mount")
TObjectPtr<ADRRobotVacuumCharacter> MountedOn;   // 내가 타고 있는 청소기

UFUNCTION() void OnRep_MountedOn();              // 클라: attach/detach + 이동모드 보정
bool IsMounted() const { return MountedOn != nullptr; }

// 서버 전용: 피격 데미지를 마운트 링크로 전파 (재전파 방지 포함, §5.4)
void PropagateSharedDamage(float Damage, const FGameplayEffectContextHandle& SourceContext);
```

### 4.4 완료 기준 (Phase A)
- 대기실에서 좌우 화살표로 RobotVacuum 선택 가능, 게임 시작 시 BP_RobotVacuumCharacter로 스폰, 어트리뷰트 초기화 로그 정상.

---

## 5. Phase B — 패시브: 탑승 시스템

### 5.1 감지 & 입력 (클라이언트, 기존 부품 패턴 재사용)
1. `MountDetectionSphere` 오버랩 시작/종료 → `ADRPlayerController::SetMountDetectionEnabled(bool, ADRRobotVacuumCharacter*)` (신규, 내부는 기존 공용 함수 `SetInteractableDetectionEnabled` 호출로 후보 집합 `OverlappedMounts` 관리).
2. `PlayerTick`의 기존 라인트레이스 주기(0.1초)에서 `FindMountByLineTrace()` (부품 `FindPartByLineTrace` 복제) — 감지 시 `SetCurrentDetectedInteractable`로 프롬프트 위젯 토글 (`SetInteractionUIVisible`).
3. `HandleInteract()` 우선순위 확장 (기존 로직 뒤에 추가):
```
부품 미소지 && CurrentDetectedPart        → 부품 픽업 (기존)
부품 소지 && 사이트 오버랩                 → 설치 (기존)
부품 소지 && 사이트 밖                     → 드롭 (기존)
부품 미소지 && CurrentDetectedMount 존재   → ServerRequestInteract(Mount)   [신규]
이미 탑승 중                               → 아무것도 안 함 (하차는 점프키)
```
   ※ **부품을 들고 있으면 탑승 불가** (기존 F키 의미 충돌 방지 + 밸런스).

### 5.2 서버 검증 & 실행
`ServerRequestInteract_Implementation`에 분기 추가:
```cpp
if (ADRRobotVacuumCharacter* Mount = Cast<ADRRobotVacuumCharacter>(Interactable))
{
    if (!Mount->CanBeMountedBy(DRCharacter)) return;
    const float DistSq = FVector::DistSquared(DRCharacter->GetActorLocation(), Mount->GetActorLocation());
    if (DistSq > FMath::Square(MaxInteractDistance)) return;
    Mount->MountRider(DRCharacter);
}
```
`CanBeMountedBy(Candidate)` 검증 규칙 (전부 서버):
1. `RiderOnTop == nullptr` — **이미 위에 누가 있으면 거부** (요구사항: 청소기1 위에 청소기2가 있으면 청소기3은 2 위에 못 탐... 이 아니라 "2 위에 3이 탄 후 2가 1에 타야 함" → 즉 "이미 라이더를 얹은 청소기에는 새로 못 탄다"는 규칙이 정확히 이 한 줄로 구현됨)
2. `Candidate->MountedOn == nullptr` — 이미 탑승 중인 캐릭터는 거부
3. 순환 방지: 나(마운트)로부터 `MountedOn` 체인을 따라 내려갔을 때 Candidate가 나오면 거부, Candidate 위 `RiderOnTop` 체인에 내가 있으면 거부 (1↔2 상호 탑승 같은 순환 차단)
4. `!IsDead()`, `!Candidate->IsCarryingPart()`, Candidate가 스턴 아님

`MountRider(Rider)` (서버):
```cpp
RiderOnTop = Rider;                       // 복제
Rider->MountedOn = this;                  // 복제
Rider->GetCharacterMovement()->SetMovementMode(MOVE_None);
Rider->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // 마운트와 상호 밀림 방지
Rider->AttachToComponent(RideAttachPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
Rider->GetAbilitySystemComponent()->AddLooseGameplayTag(State_Riding);
Rider->GetAbilitySystemComponent()->AddReplicatedLooseGameplayTag(State_Riding);
```
- **리플리케이션**: 액터 attach는 `AttachmentReplication`으로 자동 복제되지만 CMC를 가진 Character는 클라 CMC가 위치를 덮어쓰는 문제가 있으므로, `OnRep_MountedOn`에서 클라이언트도 `MOVE_None` + 재-attach를 보정한다 (부품의 `OnRep_bIsCarryingPart` 태그 동기화 패턴과 동일한 취지).
- 이동 입력 차단: `ADRPlayerController::Move()` 서두에 `if (DRCharacter->IsMounted()) return;` (탑승자는 시점 회전(Look)은 허용). GAS 어빌리티는 탑승 중에도 허용 — 청소기 탑을 이동 포탑으로 쓰는 것이 이 패시브의 재미 요소. (차단이 필요해지면 `State.Riding`을 GA들의 ActivationBlockedTags에 넣기만 하면 됨 — 결정사항 §10)

### 5.3 하차 (점프키)
- `ADRPlayerController::StartJump()` 서두에:
```cpp
if (DRCharacter->IsMounted()) { ServerRequestDismount(); return; }
```
- `ServerRequestDismount_Implementation` (서버): `MountedOn->DismountRider(true)` —
  1. Detach(`KeepWorldTransform`), 위치 = 마운트 위치 + 위 60 + 전방 80 (겹침 방지)
  2. `SetMovementMode(MOVE_Falling)` + `LaunchCharacter(위 300 + 마운트전방 200)` (폴짝 뛰어내리는 연출)
  3. Pawn 콜리전 응답 복원, `State.Riding` 태그 제거, 양쪽 포인터 nullptr
- **하차하는 캐릭터가 청소기이고 자기 위에도 라이더가 있으면**: 그 위 스택은 attach 관계가 유지되므로 통째로 함께 분리된다 (2+3이 1에서 내리면 2·3 서브타워 유지). 별도 처리 불필요 — 의도된 동작으로 명세화.

### 5.4 데미지 공유 (서버 전용)
- 위치: `UDRPlayerAttributeSet::HandleIncomingDamage()` 말미(사망 판정 이후, Damage > 0일 때) 에 훅 1줄:
```cpp
if (ADRCharacter* Char = Cast<ADRCharacter>(Props.TargetAvatarActor))
    Char->PropagateSharedDamage(LocalIncomingDamage, Props.EffectContextHandle);
```
- `ADRCharacter::PropagateSharedDamage(Damage, Context)`:
  1. **재전파 방지**: `UDRAbilitySystemLibrary` 게터로 Context의 `SourceAbilityTags`에 `Damage.MountShared`가 있으면 즉시 return.
  2. 전파 대상 = `MountedOn` + (자신이 청소기면) `RiderOnTop` — 직접 연결된 위/아래 1홉. 받은 쪽도 같은 훅을 타지만 MountShared 태그 때문에 **자기 피격 처리만 하고 재전파는 안 함** → 3층 탑에서 중간이 맞으면 위/아래 모두 전파되고, 끝단이 맞으면 인접→그 다음으로는 안 감.
     - **주의**: 요구사항은 "청소기와 탑승자 둘 다"이므로 1홉 전파가 명세에 부합. 3층 탑 전체 공유로 확장하려면 MountShared 수신 시에도 "받은 방향 반대쪽으로만" 전파하도록 방향 플래그를 추가하면 됨 (결정사항 §10).
  3. 전파 방법: `FDamageEffectParams` 구성 — `DamageGameplayEffectClass`는 기존 공용 GE_Damage, `BaseDamage = Damage`, `DamageType = Damage_Physical`, Debuff/Knockback/DeathImpulse 전부 0, `SourceAbilityTags = { Damage.MountShared }` → `UDRAbilitySystemLibrary::ApplyDamageEffect()`. Source ASC는 원 피격자의 ASC (킬크레딧/물 보상 왜곡 없음 — 플레이어→플레이어 데미지라 물 보상 로직 미발동).

### 5.5 사망/예외 처리
- `ADRRobotVacuumCharacter::MulticastHandleDeath_Implementation`(서버 경로): `RiderOnTop` 있으면 `DismountRider(false)` 강제 하차. 자신이 탑승 중이었다면 `MountedOn->DismountRider()` 경유로 링크 해제.
- `ADRCharacter` 사망 시(라이더 사망): `Die()` 오버라이드 지점에서 `IsMounted()`면 서버에서 하차 처리 후 사망 진행.
- 탑승 중 넉백: 라이더는 `MOVE_None`이라 `LaunchCharacter` 무효 — 데미지 공유만 적용되고 넉백은 마운트에만 걸림(자연스러움).

### 5.6 완료 기준 (Phase B)
- PIE 2인: F로 탑승 → 청소기 이동 시 라이더 추종(양 클라 시점 확인), 점프키 하차, 청소기 3대 탑쌓기(순서 규칙 검증: 라이더 있는 청소기에 탑승 시도 → 거부), 한쪽 피격 시 양쪽 체력 동시 감소, 마운트 사망 시 강제 하차.

---

## 6. Phase C — 일반 공격: 공기탄 (LMB)

### 6.1 `UDRVacuumAirShot : UDRProjectileSpell`
```cpp
USTRUCT(BlueprintType)
struct FVacuumShotStage
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly) float Damage = 10.f;
    UPROPERTY(EditDefaultsOnly) float ProjectileSpeed = 1500.f;
};

UCLASS()
class DAERUNE_API UDRVacuumAirShot : public UDRProjectileSpell
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) void StartCharging();   // ActivateAbility(BP)에서 호출
    UFUNCTION(BlueprintCallable) void ReleaseAndFire();   // InputReleased(BP)에서 호출

    UFUNCTION(BlueprintPure) int32 GetChargeStage() const;
    UFUNCTION(BlueprintImplementableEvent) void OnChargeStageChanged(int32 NewStage); // UI/SFX

protected:
    UPROPERTY(EditDefaultsOnly, Category="AirShot") TArray<FVacuumShotStage> Stages; // [0]기본,[1],[2],[3]강화
    UPROPERTY(EditDefaultsOnly, Category="AirShot") float ChargeInterval = 0.5f;      // 단계당 시간
    UPROPERTY(EditDefaultsOnly, Category="AirShot") float RecoilImpulse = 1200.f;     // 3단계 반동
    UPROPERTY(EditDefaultsOnly, Category="AirShot") float EnhancedKnockbackForce = 1000.f;
    UPROPERTY(EditDefaultsOnly, Category="AirShot") FGameplayTag FireSocketTag;       // CombatSocket.Weapon

private:
    double ChargeStartTime = 0.0;
    FTimerHandle ChargeUITimerHandle;   // 0.5초마다 OnChargeStageChanged 발화 (로컬 연출용)
    void FireShot(int32 Stage);         // 서버에서 투사체 스폰 + 반동
};
```
- **흐름** (자판기 BasicAttack과 동일한 Pressed→GA활성/Released→BP콜백 구조):
  1. LMB Pressed → GA 활성화(서버/클라 양쪽) → `StartCharging()`: `ChargeStartTime = GetWorld()->GetTimeSeconds()` 기록, UI 타이머 시작.
  2. LMB Released → BP InputReleased 이벤트 → `ReleaseAndFire()`: `Stage = FMath::Clamp(FloorToInt(HoldTime / ChargeInterval), 0, 3)`. 짧은 클릭(HoldTime < 0.5)은 Stage 0 = "즉시 발사" 요구 충족.
  3. `FireShot(Stage)` — `HasAuthority()`에서만:
     - 조준점: `UDRVendingMachineBasicAttack::CalculateTargetLocation()`과 동일한 카메라 라인트레이스 로직 (해당 함수를 `UDRProjectileSpell`의 protected 유틸로 승격해 재사용).
     - 스폰: 부모 `SpawnProjectile()`을 그대로 쓰되, `ADRVacuumAirProjectile`에 스폰 후 `ProjectileMovement->InitialSpeed/MaxSpeed = Stages[Stage].ProjectileSpeed`, `Velocity` 재설정. `DamageEffectParams.BaseDamage = Stages[Stage].Damage`. Stage 3이면 `KnockbackChance = 100`, `KnockbackForceMagnitude = EnhancedKnockbackForce`, `Projectile->bEnhanced = true`.
     - **반동 (Stage 3만)**: `FVector Recoil = -LaunchDir * RecoilImpulse;`
       ```cpp
       if (ASC->HasMatchingGameplayTag(State_RobotVacuum_SustainedDash) ||
           /* 돌진 이속버프 활성 */ HasDashBuff())
       {
           Recoil.X = Recoil.Y = 0.f;                    // 수평 반동 제거
           Recoil.Z = FMath::Max(Recoil.Z, 0.f);         // 아래로 쏜 경우의 상방 반동만 유지
       }
       Character->LaunchCharacter(Recoil, false, false); // 발밑 사격 = 로켓 점프
       ```
       (요구사항의 "돌진 상태" = 지속 돌진 + 이속버프 돌진 중 모두로 해석. §10 결정사항)
  4. 발사 후 `EndAbility`.
- 취소 안전: `EndAbility` 오버라이드에서 타이머 정리 (WaterPump 패턴).

### 6.2 `ADRVacuumAirProjectile : ADRProjectile`
- `bEnhanced` (스폰 시 주입, `ExposeOnSpawn`).
- **수명**: 부모 `LifeSpan` 대신 자체 타이머 — `ActiveDuration = 2.0f` 경과 시 `StartFade()`: 콜리전 off + `ProjectileMovement->StopMovementImmediately()` 유지/감속 + 나이아가라·머티리얼 페이드(`FadeDuration = 0.5f`) 후 `Destroy()`. 클라 연출은 복제된 `Destroyed()` 경로 + BP 타임라인.
- **아군 넉백 (강화탄)**: 부모 `OnSphereOverlap`은 `IsNotFriend()`에서 아군을 걸러버리므로 오버라이드:
```cpp
void OnSphereOverlap(...) override
{
    // 아군(다른 플레이어) + 강화탄 → 데미지 없이 넉백만
    if (bEnhanced && OtherActor != SourceAvatar &&
        !UDRAbilitySystemLibrary::IsNotFriend(SourceAvatar, OtherActor))
    {
        if (!bHit) OnHit();
        if (HasAuthority())
        {
            if (ACharacter* Ally = Cast<ACharacter>(OtherActor))
                Ally->LaunchCharacter(ComputeKnockback(), true, true);  // 45도 상향, 부모와 동일 공식
            Destroy();
        }
        return;
    }
    Super::OnSphereOverlap(...);   // 적: 기존 파이프라인 (KnockbackChance 100 → AttributeSet에서 LaunchCharacter)
}
```
  ※ 아군 플레이어 넉백은 클라 CMC 소유라 서버 `LaunchCharacter`만으로는 부드럽지 않을 수 있음 → 필요 시 대상 캐릭터에 `ClientLaunchCharacter` Client RPC 추가 (기존 `MulticastTeleportToSlot` 스타일). 1차 구현은 서버 Launch로 검증.

### 6.3 완료 기준 (Phase C)
- 클릭 즉발 / 0.5·1.0·1.5초 홀드별 데미지·탄속 차이 / 투사체 2초 후 페이드 / 3단계: 적·아군 넉백, 시전자 후방 반동, 발밑 사격 로켓 점프 / 지속 돌진 중 전방 사격 시 무반동·발밑 사격 점프 가능.

---

## 7. Phase D — 1번 스킬: 더블 점프 (Q)

### 7.1 `UDRVacuumJetJump : UDRDamageGameplayAbility`
- 클래스 기본값: `WaterCost = 30`, `StartupInputTag = InputTag.Q`, `DamageType = Damage.Physical`, `DamageEffectClass = GE_Damage`(공용).
```cpp
UPROPERTY(EditDefaultsOnly) float JumpImpulseZ = 900.f;
UPROPERTY(EditDefaultsOnly) float InnerRadius = 200.f;   UPROPERTY(EditDefaultsOnly) float InnerDamage = 20.f;
UPROPERTY(EditDefaultsOnly) float OuterRadius = 400.f;   UPROPERTY(EditDefaultsOnly) float OuterDamage = 5.f;
```
- **사용 가능 판정** — `CanActivateAbility` 오버라이드 (WaterCost는 부모 `CheckCost`가 처리):
```cpp
ADRRobotVacuumCharacter* Vac = Cast<ADRRobotVacuumCharacter>(ActorInfo->AvatarActor);
if (Vac->GetCharacterMovement()->IsFalling() && Vac->bAirJumpUsed) return false;
```
- **ActivateAbility** (서버 권한 처리, 연출은 GameplayCue):
  1. `CommitAbility` (물 30 차감 — 기존 ExecCalc_WaterCost 경로).
  2. 공중이면 `bAirJumpUsed = true` (지상 사용은 마킹 안 함 → "지상 Q → 공중 Q" 총 2회, "스페이스 점프 → 공중 Q" 1회 규칙이 그대로 구현됨). 리셋은 `ADRRobotVacuumCharacter::Landed()`.
  3. `Character->LaunchCharacter(FVector(0,0,JumpImpulseZ), false, true)` — Z 오버라이드로 낙하 중에도 일정한 점프.
  4. **AoE 데미지 (Z 무관 = 2D 반경)**:
     ```cpp
     TArray<AActor*> Candidates;
     UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(this, Candidates, {Avatar}, OuterRadius * 1.5f, Origin); // 구체는 3D라 여유 반경으로 수집
     for (AActor* T : Candidates)
     {
         if (!UDRAbilitySystemLibrary::IsNotFriend(Avatar, T)) continue;      // 적만
         const float Dist2D = FVector::DistXY(Origin, T->GetActorLocation()); // Z 무시
         if (Dist2D > OuterRadius) continue;
         FDamageEffectParams P = MakeDamageEffectParamsFromClassDefaults(T);
         P.BaseDamage = (Dist2D <= InnerRadius) ? InnerDamage : OuterDamage;  // SeedProjectile 내/외 패턴
         UDRAbilitySystemLibrary::ApplyDamageEffect(P);
     }
     ```
  5. 물 분사 VFX/SFX: `GameplayCue.Skill.VacuumJetJump` ExecuteGameplayCue (기존 GameplayCue_Skill_WaterPump 등록 방식과 동일하게 태그 추가).
  6. `EndAbility` (인스턴트).

### 7.2 완료 기준 (Phase D)
- 물 30 차감·부족 시 발동 불가 / 지상 Q→공중 Q 2연속 성공, 3연속 실패 / 스페이스 점프 후 Q 1회만 / 착지 후 다시 가능 / 근접 적 20·원거리 적 5 데미지, 높이 차이 있는 적(Z 차이)에도 2D 반경으로 적중.

---

## 8. Phase E — 2번 스킬: 돌진 (RMB)

### 8.1 역할 분담 (Armadillo 패턴 이식)
- **GA (`UDRVacuumDash`)**: 게이지 충전 타이머, 물 소모, 버프 GE 적용/감쇠, 충돌 델리게이트 수신 후 데미지/자해 판정, 지속 돌진 시작/종료 명령.
- **캐릭터 (`ADRRobotVacuumCharacter`)**: 충돌 감지(`OnCapsuleHit` → `OnDashImpact` 브로드캐스트), 지속 돌진 자동 전진 Tick, `bSustainedDash` 복제/연출.

### 8.2 `UDRVacuumDash : UDRDamageGameplayAbility`
```cpp
public:
    UFUNCTION(BlueprintCallable) void StartChargingGauge();  // ActivateAbility(BP)
    UFUNCTION(BlueprintCallable) void ReleaseGauge();        // InputReleased(BP)
    UFUNCTION(BlueprintCallable) void OnBrakePressed();      // S 브레이크 (PC 경유 Gameplay Event)
    UFUNCTION(BlueprintPure)     int32 GetGauge() const { return Gauge; }
    UFUNCTION(BlueprintImplementableEvent) void OnGaugeChanged(int32 NewGauge, int32 MaxGauge); // UI

protected:
    UPROPERTY(EditDefaultsOnly) int32 MaxGauge = 5;
    UPROPERTY(EditDefaultsOnly) float ChargeInterval = 0.5f;      // 1칸 충전 시간
    UPROPERTY(EditDefaultsOnly) float DecayInterval  = 1.0f;      // 1칸 감소 시간
    UPROPERTY(EditDefaultsOnly) float WaterPerGauge  = 20.f;
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UGameplayEffect> SpeedBuffEffect;      // GE_VacuumDashSpeedBuff
    UPROPERTY(EditDefaultsOnly) TSubclassOf<UGameplayEffect> SelfDamageEffect;     // GE_VacuumDashSelfDamage (50)
    UPROPERTY(EditDefaultsOnly) float EnemyDamagePerGauge  = 10.f;
    UPROPERTY(EditDefaultsOnly) float PlayerDamagePerGauge = 5.f;

private:
    int32 Gauge = 0;
    bool  bReleased = false;
    FTimerHandle ChargeTimerHandle, DecayTimerHandle;
    FActiveGameplayEffectHandle SpeedBuffHandle;
    UFUNCTION() void HandleDashImpact(AActor* HitActor, const FHitResult& Hit);
    void TickCharge();   void TickDecay();
    void ApplyBuffStacks(int32 Stacks);   void ClearAllBuff();
    void FinishDash(bool bFromImpact, AActor* HitActor);
```
- **충전 (서버 타이머, 0.5초)** `TickCharge()`:
  1. `Gauge >= MaxGauge`면 대기(추가 소모 없음).
  2. 물 20 확인: `UDRAttributeSet::GetWater() >= WaterPerGauge` — 부족하면 충전 일시 정지(게이지 유지, 다음 틱 재시도).
  3. 충분하면 `ApplyEffectSpecWithSetByCaller(ASC, WaterCostSpec, Water_SetByCaller_Reduction, -20)` (기존 물 감소 GE 재사용), `Gauge++`, `OnGaugeChanged` + ASC 노티파이(§9.2).
  - 충전 중 이동 제한 없음(요구사항). 이동속도 페널티도 없음.
- **해소** `ReleaseGauge()` (서버):
  - `Gauge == 0` → 그냥 `EndAbility`.
  - `Gauge < MaxGauge` → **일반 돌진**: `ApplyBuffStacks(Gauge)` — `GE_VacuumDashSpeedBuff`(Infinite, `MoveSpeed × (1 + 0.2×스택)` Multiplicative Modifier, StackLimit 5)를 스택 수만큼 적용. MoveSpeed 어트리뷰트 변경 → 기존 `OnMoveSpeedChanged` 바인딩이 CMC에 자동 반영. `Character->SetDashCollisionEnabled(true)`. `DecayTimerHandle` 시작(1초): `TickDecay()` — `Gauge--`, 버프 스택 1 제거(`ASC->RemoveActiveGameplayEffect(SpeedBuffHandle, 1)` 또는 스택 재적용), 0이 되면 `FinishDash(false)`.
  - `Gauge == MaxGauge` → **지속 돌진**: 버프 5스택 적용, 감쇠 타이머 없음, `Character->SetSustainedDash(true)` (복제 → `State.RobotVacuum.SustainedDash` 태그도 서버에서 Loose 태그로 부여 — 공기탄 반동 예외 판정용), `SetDashCollisionEnabled(true)`.
- **충돌** `HandleDashImpact(HitActor, Hit)` (캐릭터 델리게이트, 서버):
  1. `const int32 G = Gauge;` — 판정 시점 게이지 저장.
  2. 대상 데미지: 적(`IsNotFriend` true)이면 `G × 10`, 타 플레이어면 `G × 5` — `MakeDamageEffectParamsFromClassDefaults(HitActor)` 후 `BaseDamage` 덮어쓰고 `ApplyDamageEffect()` (직접 호출이라 아군 필터에 안 걸림). 지형(WorldStatic)이면 대상 데미지 없음.
  3. 지속 돌진 중이었으면 자신에게 `SelfDamageEffect`(50) 적용 (`CreateAndApplyEffectSpec` 헬퍼).
  4. `FinishDash(true, HitActor)`: `Gauge = 0`, `ClearAllBuff()`(스택 전량 제거), `SetSustainedDash(false)`, `SetDashCollisionEnabled(false)`, 임팩트 멀티캐스트 사운드(Armadillo `MulticastPlayRollImpactSound` 패턴), `EndAbility`.
- **브레이크 (S)**: 지속 돌진 중 후진 입력 감지 — `ADRPlayerController::Move()`에서:
```cpp
if (Vac && Vac->bSustainedDash && InputAxisVector.Y < -0.5f)
    ServerRequestDashBrake();   // 신규 Server RPC → ASC에 GameplayEvent(Event.Dash.Brake) 전송
```
  GA는 `WaitGameplayEvent(Event.Dash.Brake)`(BP) 또는 C++ 델리게이트로 수신 → `FinishDash(false)` — 자기 데미지 없음, 게이지/버프는 소멸.
- **정리 보장**: `EndAbility` 오버라이드에서 타이머 2종 해제 + 버프 잔량 제거 + `SetSustainedDash(false)` + `SetDashCollisionEnabled(false)` (사망/스턴에 의한 강제 취소 대비, WaterPump `EndAbility` 패턴).
- **스턴 연동**: `Debuff.Stun` 부여 시 이 GA가 취소되도록 GA의 `ActivationBlockedTags`/BP `CancelAbilitiesWithTag` 설정.

### 8.3 캐릭터 측 충돌 감지 상세
- `BeginPlay`에서 `GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &...::OnCapsuleHit)` (+ `SetNotifyRigidBodyCollision(true)`).
- 판정 조건 (전부 서버):
  - `bDashCollisionArmed` (버프 적용 중에만 true)
  - `GetVelocity().Size2D() >= DashImpactMinSpeed` (기본 이속보다 확실히 빠를 때만 — 벽에 스치기만 해도 터지는 오판 방지)
  - Pawn(적/타 플레이어) 또는 `ImpactNormal.Z < 0.7`인 WorldStatic(수직면 = 벽/지형지물, 바닥 제외)
  - 자기 라이더/자기 마운트는 제외 (탑 상태에서 서로 충돌 판정 금지)
- 중복 발화 방지: 1회 브로드캐스트 후 `bDashCollisionArmed = false` (GA가 FinishDash에서 재확인).
- 지속 돌진 전진: `Tick`에서 `AddMovementInput(GetActorForwardVector())` — CMC 경로라 리플리케이션/경사면 처리가 공짜. 컨트롤러 회전을 따라 조향 가능(§10).

### 8.4 완료 기준 (Phase E)
- 0.5초당 게이지 1칸·물 20 차감, 물 부족 시 충전 정지 / 뗀 후 스택당 20% 이속(CMC 속도 로그로 확인)·1초당 1칸 감쇠 / 버프 중 적 충돌: 게이지×10 데미지 + 버프 소멸 / 타 플레이어 충돌: 게이지×5 / 5칸 해소 시 자동 전진·게이지 유지 / 벽 충돌 시 본인 50 + 전체 초기화 / S 브레이크로 무피해 정지 / 지속 돌진 중 강화 공기탄 전방 무반동·하방 로켓 점프 정상.

---

## 9. Phase F — UI / 연출 / 마무리

### 9.1 스킬 아이콘·설명창
- `WBP_SkillIcons_RobotVacuum` 제작 → `DA_PlayerCharacterClassInfo`의 `SkillIconWidgetClass`에 지정 (기존 클래스별 위젯 로딩 흐름 그대로).
- `WBP_CharacterInfo_RobotVacuum` (Tab 홀드 설명창) → `CharacterInfoWidgetClass`.

### 9.2 게이지/충전 단계 HUD
- `UDRAbilitySystemComponent`에 자판기 잭팟 패턴을 일반화한 델리게이트 추가:
```cpp
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnVacuumGaugeChanged, int32 /*Gauge*/, int32 /*Max*/);
FOnVacuumGaugeChanged OnVacuumDashGaugeChanged;
FOnVacuumGaugeChanged OnVacuumChargeStageChanged;
// + Client RPC (ClientVendingMachineStacksChanged 패턴 복제)
```
- `UOverlayWidgetController::BindCallbacksToDependencies()`에서 구독 → WBP로 브로드캐스트 (CLAUDE.md "Adding Status Effect UI" 절차 준수).
- 소유 클라 즉각 반응이 필요한 충전 단계 표시는 GA의 로컬 타이머(`OnChargeStageChanged` BIE)로 예측 표시하고, 서버 노티파이는 보정용.

### 9.3 사운드/VFX 동기화
- 발사/점프/돌진 임팩트: `ADRCharacter`의 자판기 사운드 멀티캐스트 패턴(`MulticastPlayVendingCoinShot` 등)을 따라 `ADRRobotVacuumCharacter`에 추가 — 반복성 이벤트는 `Unreliable`, 1회성 임팩트는 `Reliable`.
- 지속 돌진 루프 사운드: `bSustainedDash` RepNotify에서 시작/정지 (Armadillo `OnRep_IsRolling` 패턴).
- 충전 이펙트/탄 색상: 단계별 나이아가라 파라미터를 BP에서 설정.

### 9.4 밸런스 외부화 (선택)
- `UGameBalanceConfig`에 `FRobotVacuumConfig`(RecoilImpulse, WaterPerGauge, DamagePerGauge 등) 추가하면 재컴파일 없이 조정 가능. 1차 구현은 GA `EditDefaultsOnly`로 충분.

---

## 10. 명세 외 설계 결정 사항 (구현 전 확인 권장)

| # | 항목 | 제안 기본값 | 근거 |
|---|---|---|---|
| 1 | 탑승 중 라이더의 공격/스킬 사용 | **허용** | 이동 포탑 시너지가 패시브의 존재 이유. 차단하려면 `State.Riding`을 GA ActivationBlockedTags에 추가만 하면 됨 |
| 2 | 부품 소지 중 탑승 | **불가** | F키 의미 충돌(설치/드롭 우선) + 운반 페널티 유지 |
| 3 | 데미지 공유 범위 | **직접 연결 1홉 (마운트↔라이더)** | 명세 문면("로봇청소기와 탑승자") 그대로. 3층 탑 전체 공유가 원하는 동작이면 방향 플래그로 확장 |
| 4 | 지속 돌진 중 조향 | **마우스 회전으로 방향 전환 가능** | 컨트롤러 불가면 재미 반감. 불가로 바꾸려면 시작 시 방향 고정 벡터 저장 |
| 5 | "돌진 상태" (강화탄 무반동 예외)의 정의 | **지속 돌진 + 이속버프 돌진 둘 다** | 일반 돌진도 충돌 데미지가 있는 "돌진"이므로 |
| 6 | S 브레이크 시 게이지/버프 | **즉시 전량 소멸 (자기 데미지 없음)** | "충돌 전 안전 정지" 보상으로 데미지만 면제 |
| 7 | 하차 시 위 스택 | **서브타워 통째로 함께 하차** | attach 구조상 자연스럽고 추가 코드 불필요 |
| 8 | 탑승 대상 감지 방식 | 부품과 동일한 오버랩+라인트레이스 | 기존 공용 함수 재사용, UI 일관성 |

---

## 11. 리플리케이션/권한 요약표

| 상태 | 소유 | 복제 방식 | 클라 처리 |
|---|---|---|---|
| `RiderOnTop` / `MountedOn` | 서버 | RepNotify | attach/이동모드/콜리전 보정, 프롬프트 갱신 |
| `State.Riding` 태그 | 서버 | Replicated Loose Tag | 입력 차단 UI |
| 탑승/하차/상호작용 | 클라 요청 | Server RPC (`ServerRequestInteract` / `ServerRequestDismount`) | 거리·상태는 서버 재검증 |
| 공유 데미지 | 서버 전용 | GE 적용(어트리뷰트 복제) | — |
| 충전 단계/게이지 | 서버 판정 | ASC Client RPC 노티파이 | 로컬 타이머로 예측 표시 |
| 투사체 | 서버 스폰 | bReplicates + 무브먼트 복제 | 페이드 연출 로컬 |
| 이속버프 | 서버 GE | MoveSpeed 어트리뷰트 복제 → OnMoveSpeedChanged | CMC 속도 자동 반영 |
| `bSustainedDash` | 서버 | RepNotify | 루프 사운드/VFX |
| 반동/점프/넉백 | 서버 `LaunchCharacter` | CMC 이동 복제 | (아군 넉백 부드러움 필요 시 Client RPC 보강) |
| `bAirJumpUsed` | 서버 전용 | 복제 불필요 (판정이 서버) | CanActivate 실패는 GAS가 처리 |
| `bIsDashCharging` / `bIsJetJumping` (애님 전용, §13.6) | 서버 (GA가 세팅) | Replicated (RepNotify 불필요 — ABP가 매 프레임 읽음) | ABP 스테이트 머신 전이 조건 |
| 몽타주 (HitReact/Death/EnhancedAttack/Dash_Stop/Crush) | 서버 재생 결정 | GAS 몽타주 복제(ASC ReplicatedAnimMontage) 또는 DeathMontageIndex 복제 | 시뮬 프록시 자동 재생 (§13.5) |

---

## 12. 구현 순서 (커밋 단위)

1. **Phase A**: enum + 태그 + `ADRRobotVacuumCharacter` 스켈레톤 + BP/DA 등록 → 로비 선택·스폰 확인
2. **Phase C**: 공기탄 (충전/단계 발사/2초 페이드/강화 넉백/반동) — 돌진 예외는 스텁
3. **Phase D**: 더블 점프 (물 소모/공중 제한/2D AoE)
4. **Phase E**: 돌진 (게이지/버프/충돌/지속 돌진/브레이크) + 공기탄 반동 예외 연결
5. **Phase B**: 탑승 (감지/RPC/attach/하차/탑쌓기/데미지 공유) — 가장 리스크 큰 리플리케이션 작업이므로 다른 기능 안정화 후
6. **Phase F**: UI/사운드/밸런스/설명창

> **애니메이션(§13) 병행 순서**: Phase A 직후 ABP 뼈대 + 로코모션/점프/스턴(§13.3~13.4) → Phase C에서 `AM_VC_EnhancedAttack` + HitReact/Death 몽타주 등록(§13.5) → Phase D에서 `bIsJetJumping` 플래그 연결 → Phase E에서 대쉬 스테이트/몽타주(`bIsDashCharging`, Dash_Stop/Crush) → Phase B에서 Boarding 레이어.

각 Phase 종료 시 PIE 2~3인(리슨 서버 + 클라이언트)으로 검증. 특히 확인할 것:
- 클라이언트 시점에서의 탑승 위치 동기화(attach + MOVE_None 보정)
- 리슨 서버 호스트 vs 원격 클라의 충전 타이밍 오차(서버 시간 기준 판정 확인)
- 탑승 중 사망/레벨 전환/리스폰 시 링크 정리 (EndPlay/Die 경로)

---

## 13. 애니메이션 시스템 설계 (3인칭)

> 3인칭 애니메이션/모델링 완성본 기준 상세 설계.
> 에셋 위치: `Content/DaeRuneAssets/Characters/VacuumCleaner/` — 스켈레탈 메시 `VecuumCleaner_v0_1_1` (+ `_Skeleton`, `_PhysicsAsset`), 애니 시퀀스 `VC_*` 24종, 얼굴/바디 텍스처.

### 13.0 설계 전제 (코드 근거)

1. **로컬 플레이어는 자기 3인칭 메시를 보지 못한다.** `GetMesh()->SetOwnerNoSee(true)`, 1인칭은 카메라 부착 `FirstPersonMesh`(OnlyOwnerSee) 별도 (`DRCharacter.cpp:63-71`). 따라서 이 문서의 3인칭 애니는 **① 다른 플레이어가 보는 모션 ② 대기실(SetWaitingRoomVisibility로 3P 노출) ③ 사망 카메라 연출**용이다. → 설계 우선순위는 "시뮬레이티드 프록시에서 반드시 재생되는가"(네트워크 동기화)이며, 셀프 뷰 품질은 고려 대상이 아니다.
2. **바퀴 주행 로봇**이라 발-지면 접지 개념이 없다. 이족 캐릭터의 "풀바디 몽타주 재생 중 발 미끄러짐" 아티팩트가 문제되지 않으므로, 상/하체 레이어 분리 없이 **단일 풀바디 그래프 + 슬롯 1개**로 단순하게 간다.
3. **게임플레이 판정을 AnimNotify에 두지 않는다.** 발사·데미지·충돌·게이지 판정은 전부 코드/타이머 주도(§6~8)이고 애니는 순수 연출이다. 근거: 메시 `VisibilityBasedAnimTickOption` 기본값(OnlyTickPoseWhenRendered)에서는 오프스크린 캐릭터의 노티파이 발화가 보장되지 않음(리슨 서버 호스트가 안 보는 캐릭터 등). 노티파이는 총구 VFX 같은 코스메틱에만 허용.
4. **루트모션 미사용.** 모든 이동(걷기/점프/대쉬 전진)은 CMC가 담당(§8.3 `AddMovementInput`). 임포트된 시퀀스에 루트모션이 있으면 `EnableRootMotion = false` 확인 (특히 Dash_Start/Loop/Crush).
5. 기존 C++ 파이프라인이 이미 몽타주를 전제로 완성돼 있다: `HitReactMontages` 배열 + 랜덤 선택(`DRCharacterBase.cpp:67-83`), `DeathMontages` + 서버 인덱스 복제 + `PlayDeathMontage_Internal()`(`DRCharacter.cpp:892-953`), 스턴은 `bIsStunned` RepNotify(`DRCharacterBase.h:68`). → **새 시스템을 만들지 않고 배열 등록/플래그 추가만으로 연결한다.**
6. 얼굴 표정은 ABP 소관이 아니다 — `UDRFacialExpressionComponent`가 머티리얼 스왑으로 처리(피격 표정은 `MulticastPlayHitReactFacial`). `VC_Display_Face_*` 텍스처를 이 컴포넌트에 연결(별도 작업).

### 13.1 애니메이션 분류 총괄표 ★

"무엇을 ABP에 두고, 무엇을 몽타주로 만들고, 무엇을 GA가 재생하는가"의 최종 답.

| 애니 시퀀스 | 분류 | 사용 위치 | 트리거 / 데이터 소스 | 재생 주체 |
|---|---|---|---|---|
| `VC_Idle_1/2/3` | ABP | Locomotion SM `Idle` 상태 (Random Sequence Player) | `Speed < 3` | ABP |
| `VC_Walk_Forward/Backward/Left/Right` | ABP | `BS_VC_Walk` (1D 블렌드스페이스) → SM `Walk` 상태 | `Speed`, `Direction` | ABP |
| `VC_Jump_Start/Loop/Land` | ABP | SM 점프 스테이트 3개 | `bIsFalling` (CMC) | ABP |
| `VC_DoubleJump_Start/Loop/Land` | ABP | SM 더블점프 스테이트 3개 | `bIsJetJumping`(신규 복제 플래그) + `bIsFalling` | ABP |
| `VC_AimOffset_Down/Front/Up` | ABP | `AO_VC_Aim` (1D 에임오프셋, Pitch) | `GetBaseAimRotation().Pitch` | ABP |
| `VC_Boarding` | ABP | Layered Blend Per Bone (머리 본 체인) | `RiderOnTop != nullptr` (§4.2 복제 프로퍼티) | ABP |
| `VC_Dash_Charge` | ABP | SM `Dash_Charge` 상태 | `bIsDashCharging`(신규 복제 플래그) | ABP |
| `VC_Dash_Start` → `VC_Dash_Loop` | ABP | SM `Dash_Start`→`Dash_Loop` 상태 | `bSustainedDash` (§4.2 복제 프로퍼티) | ABP |
| `VC_Dash_Stop` | **몽타주** `AM_VC_Dash_Stop` | DefaultSlot | S 브레이크 → `FinishDash(false)` | **GA_VacuumDash** |
| `VC_Dash_Crush` | **몽타주** `AM_VC_Dash_Crush` | DefaultSlot | 돌진 충돌 → `FinishDash(true)` | **GA_VacuumDash** |
| `VC_EnhancedAttack` | **몽타주** `AM_VC_EnhancedAttack` | DefaultSlot | **3단계 강화탄 발사 시에만** (`FireShot(3)`) | **GA_VacuumAirShot** |
| `VC_HitReact` | **몽타주** `AM_VC_HitReact` | DefaultSlot | 피격 → `Effects.HitReact` 태그 어빌리티 활성화 | **GA_HitReact (기존 공용)** |
| `VC_Death` | **몽타주** `AM_VC_Death` | DefaultSlot | `MulticastHandleDeath` → `PlayDeathMontage_Internal` | **기존 C++ (ADRCharacter)** |
| `VC_Stun` | ABP | SM `Stunned` 상태 (루프) | `bIsStunned` (기존 복제 플래그) | ABP |

**분류 기준** (앞으로 애니 추가 시에도 이 기준 적용):
- **몽타주** = ① 1회성 이벤트 반응이고 ② 어떤 상태에서든 끼어들어야 하며 ③ 서버가 발생 시점을 결정하는 것. GAS 몽타주 복제(GA에서 재생 시 ASC가 시뮬 프록시로 자동 전파) 또는 기존 사망 파이프라인이 동기화를 공짜로 해결.
- **ABP 스테이트** = 지속시간이 가변(루프)이거나 이동과 결합된 것. 복제 bool 플래그를 매 프레임 읽어 전이 — `bIsStunned` 선례와 동일 패턴. 몽타주로 만들면 루프 제어/중단 처리가 오히려 복잡해짐.
- 강화 공격이 아닌 **일반 공기탄(0~2단계)은 애니 없음** — 요구사항 그대로("그냥 일반 공격에서는 재생이 안돼"). **일반 돌진(게이지 1~4칸)도 전용 애니 없음** — 이속버프 상태로 걷는 것뿐이므로 걷기 재생속도 가속(§13.4 `WalkPlayRate`)으로 표현.

### 13.2 신규 애님 에셋 목록 (에디터 생성물)

| 에셋 | 종류 | 내용 |
|---|---|---|
| `ABP_VacuumCleaner` | Animation Blueprint | 스켈레톤 `VecuumCleaner_v0_1_1_Skeleton`. §13.3~13.4 전체 구현. 배치: `Content/Blueprints/Character/PlayerCharacter/VacuumCleaner/` (Gardener/VendingMachine 폴더 관례) |
| `BS_VC_Walk` | Blend Space 1D | 축 `Direction` [-180, 180]. 샘플 5개: Backward(-180) / Left(-90) / Forward(0) / Right(+90) / Backward(+180). Walk_Backward를 양 끝에 중복 배치해 좌↔우 후방 회전 시 뒤집힘 방지 |
| `AO_VC_Aim` | Aim Offset 1D | 축 `Pitch` [-90, 90]. 샘플: Down(-90) / Front(0) / Up(+90). **사전 작업**: 3개 시퀀스 디테일에서 `Additive Anim Type = Mesh Space`, `Base Pose Type = Selected animation frame`, Base = `VC_AimOffset_Front` (에임오프셋은 Mesh Space 어디티브 필수) |
| `AM_VC_HitReact` | 몽타주 | 슬롯 DefaultGroup.DefaultSlot, BlendIn 0.1 / BlendOut 0.2 |
| `AM_VC_Death` | 몽타주 | 슬롯 DefaultSlot. BlendOut 0 (사망 끝 포즈 유지 — 직후 Dissolve가 이어받음) |
| `AM_VC_EnhancedAttack` | 몽타주 | 슬롯 DefaultSlot. (선택) 총구 나이아가라용 AnimNotify 1개 — 코스메틱 한정(§13.0-3) |
| `AM_VC_Dash_Stop` | 몽타주 | 슬롯 DefaultSlot. BlendIn 0.05 (급정거 스냅감) |
| `AM_VC_Dash_Crush` | 몽타주 | 슬롯 DefaultSlot. BlendIn 0.05 |

슬롯은 **DefaultGroup.DefaultSlot 하나만 사용** (신규 슬롯 생성 불필요). 청소기는 상/하체 분리 개념이 약하고(§13.0-2), 슬롯을 나누면 동시 재생 우선순위 문제만 생긴다. 몽타주끼리는 나중 것이 이전 것을 중단 — 의도된 동작(피격이 공격 모션을 끊는 등).

### 13.3 ABP_VacuumCleaner — Event Graph (변수 수집)

`Event Blueprint Initialize Animation`: `TryGetPawnOwner` → `Cast to ADRRobotVacuumCharacter` → 변수 `OwnerVacuum`에 캐싱 (매 프레임 캐스트 방지).
`Event Blueprint Update Animation`: `IsValid(OwnerVacuum)` 가드 후 아래 수집. (성능이 필요해지면 Property Access + Thread Safe Update로 이전 — 1차는 이벤트 그래프로 충분)

| ABP 변수 | 타입 | 계산식 (소스) |
|---|---|---|
| `Speed` | float | `OwnerVacuum.GetVelocity().Size2D()` |
| `Direction` | float | `Calculate Direction(Velocity, OwnerVacuum.GetActorRotation())` → [-180, 180]. 스트레이프 캐릭터(`bUseControllerRotationYaw=true`, `DRCharacter.cpp:101`)이므로 이 값이 4방향 걷기의 축 |
| `bIsFalling` | bool | `OwnerVacuum.CharacterMovement.IsFalling()` |
| `bIsStunned` | bool | `OwnerVacuum.bIsStunned` (기존, `DRCharacterBase.h:68`) |
| `bDead` | bool | `OwnerVacuum.bDead` (기존, `DRCharacterBase.h:113`) |
| `bIsJetJumping` | bool | `OwnerVacuum.bIsJetJumping` (신규, §13.6) |
| `bIsDashCharging` | bool | `OwnerVacuum.bIsDashCharging` (신규, §13.6) |
| `bSustainedDash` | bool | `OwnerVacuum.bSustainedDash` (§4.2) |
| `bIsBoarded` | bool | `OwnerVacuum.RiderOnTop != nullptr` (§4.2) |
| `AimPitch` | float | `NormalizeAxis(OwnerVacuum.GetBaseAimRotation().Pitch)` → Clamp [-90, 90]. **`GetBaseAimRotation()`은 시뮬 프록시에서 `RemoteViewPitch`(uint8 양자화)를 자동 사용하므로 별도 복제 불필요.** 약간의 계단현상은 정상 — 거슬리면 FInterpTo(속도 10~15)로 스무딩 |
| `WalkPlayRate` | float | `Speed / ReferenceWalkSpeed` → Clamp [0.6, 2.5]. `ReferenceWalkSpeed`는 ABP 로컬 float(기본 250 = `BaseWalkSpeed`와 수동 일치). **돌진 이속버프(최대 +100%) 시 걷기 재생속도가 자동으로 빨라져 일반 돌진 표현을 대체(§13.1)** |
| `AimAlpha` | float | 목표값 `(!bDead && !bIsStunned && !bIsBoarded && !bSustainedDash) ? 1 : 0` 을 FInterpTo(5.0)로 보간 — 에임오프셋 on/off 팝 방지 |

### 13.4 ABP_VacuumCleaner — AnimGraph

#### 13.4.1 최종 파이프라인 (노드 순서와 이유)

```
[Locomotion State Machine]
  → [Slot 'DefaultGroup.DefaultSlot']        ← 몽타주 5종이 여기서 SM 출력을 덮음
  → [AO_VC_Aim]  (Pitch=AimPitch, Alpha=AimAlpha)
  → [Layered Blend Per Bone]  (Base=위 결과, Blend Pose 0=VC_Boarding 루프 재생,
                               Branch=머리 체인 루트 본, Blend Weights=bIsBoarded ? 1 : 0)
  → [Output Pose]
```

- **Slot을 SM 바로 뒤(에임/보딩 앞)에 두는 이유**: 몽타주 재생 중에도 에임오프셋·머리눌림이 위에 합성된다 → ① `AM_VC_EnhancedAttack` 중 상하 조준 유지 ② 라이더를 태운 채 피격당해도 머리 눌림 유지. 반대로 Death 중 조준이 붙는 문제는 `AimAlpha`의 `!bDead` 조건이 차단.
- **Boarding을 Layered Blend Per Bone으로 하는 이유**: 탑승은 "머리만 눌린 채 계속 주행 가능"해야 한다(§5 — 이동 포탑). 풀바디 상태로 만들면 걷기와 배타적이 됨. Branch 본은 스켈레톤 트리에서 머리 체인의 루트 본 이름 확인 후 지정(`VecuumCleaner_v0_1_1_Skeleton` 열어서 확인), `Mesh Space Rotation Blend = true` 권장. Blend Weights는 0↔1 스냅 대신 이벤트그래프에서 FInterpTo(8.0)로 보간하면 탑승/하차 순간이 부드럽다.
  - 대안: `VC_Boarding`을 어디티브(Local Space, Base=`VC_Idle_1`)로 임포트 설정 후 `Apply Additive` — 걷기 바운스와의 합성이 더 자연스러우면 교체. 1차는 Layered Blend로 검증.

#### 13.4.2 Locomotion State Machine 상세

```
                          ┌──────────────────────────── [Stunned] (VC_Stun 루프)
                          │  진입: bIsStunned (전 상태 Alias, Priority 1)
                          │  탈출: !bIsStunned → Idle/Walk
                          │
[Idle] ⇄ [Walk] ──────────┼─→ [Jump_Start] → [Jump_Loop] → [Jump_Land] ─→ Idle/Walk
  │        │              │      (bIsFalling && !bIsJetJumping)   (!bIsFalling)
  │        │              │
  │        │              ├─→ [DJ_Start] → [DJ_Loop] → [DJ_Land] ─→ Idle/Walk
  │        │              │      (bIsJetJumping && bIsFalling)    (!bIsFalling)
  │        │              │      ※ 점프 상태들(Alias)에서도 진입 가능 (공중 Q)
  │        │
  └────────┴─→ [Dash_Charge] ─→ [Dash_Start] → [Dash_Loop] ─→ Idle/Walk
       (bIsDashCharging && !bIsFalling)  (bSustainedDash)   (!bSustainedDash)
                  │
                  └─→ Idle/Walk (!bIsDashCharging && !bSustainedDash — 조기 해소=일반 돌진)
                  └─→ Jump_Start (bIsFalling — 충전 중 점프/낙하)
```

**상태별 재생 내용**:

| 상태 | 재생 노드 | 설정 |
|---|---|---|
| `Idle` | **Random Sequence Player**: `VC_Idle_1`(가중치 0.85) / `VC_Idle_2`(0.075) / `VC_Idle_3`(0.075) | 루프마다 가중치로 선택 → "평소 Idle_1, 가끔 딴짓" 자동. 대안(연출 통제 필요 시): Idle_1 루프 + 이벤트그래프 랜덤 타이머(8~15s)로 Blend Poses by Int 전환 |
| `Walk` | `BS_VC_Walk` (X=`Direction`) | `PlayRate = WalkPlayRate` — 이속버프/부품 소지 감속이 자동 반영 |
| `Jump_Start` | `VC_Jump_Start` (루프 off) | 끝나면 자동 전이 |
| `Jump_Loop` | `VC_Jump_Loop` (루프 on) | 체공 시간 가변 대응 |
| `Jump_Land` | `VC_Jump_Land` (루프 off) | |
| `DJ_Start` | `VC_DoubleJump_Start` (루프 off) | 물 분사 GameplayCue(§7.1-5)와 동시 발생 |
| `DJ_Loop` | `VC_DoubleJump_Loop` (루프 on) | |
| `DJ_Land` | `VC_DoubleJump_Land` (루프 off) | |
| `Dash_Charge` | `VC_Dash_Charge` (루프 off, 마지막 프레임 유지) | **PlayRate = 애니 길이(5s) ÷ (MaxGauge × ChargeInterval = 2.5s) = 2.0** — 게이지 만충 타이밍과 애니 클라이맥스 동기화. §8.2 수치 변경 시 재계산. 상태 진입마다 처음부터 재생(Reset on Becoming Relevant) |
| `Dash_Start` | `VC_Dash_Start` (루프 off) | 지속 돌진 시작 |
| `Dash_Loop` | `VC_Dash_Loop` (루프 on) | 지속 돌진 자동 전진 중 계속 |
| `Stunned` | `VC_Stun` (루프 on) | 디버프 지속시간 가변 대응 |

**전이 규칙 상세** (블렌드 시간, 우선순위):

| 전이 | 조건 | Blend | 비고 |
|---|---|---|---|
| Idle → Walk | `Speed > 3` | 0.15 | |
| Walk → Idle | `Speed <= 3` | 0.2 | |
| (Idle/Walk Alias) → Jump_Start | `bIsFalling && !bIsJetJumping` | 0.1 | 일반 점프·낙하 공용. 스페이스 점프는 CMC `Jump()` 경로라 별도 플래그 불필요 |
| Jump_Start → Jump_Loop | Automatic (Time Remaining < 0.1) | 0.15 | 짧은 점프에서 Start 스킵 방지: Automatic 사용 |
| Jump_Loop → Jump_Land | `!bIsFalling` | 0.1 | |
| Jump_Land → Idle/Walk | Automatic + 조기 탈출 `Speed > 3` (Blend 0.2) | 0.2 | 착지 직후 이동 반응성 확보 |
| (지상+일반점프 Alias) → DJ_Start | `bIsJetJumping && bIsFalling` | 0.1 | **Priority: 일반 점프 전이보다 높게** — 지상 Q 사용 시 Jump_Start가 아닌 DJ_Start로 |
| DJ_Start → DJ_Loop | Automatic | 0.15 | |
| DJ_Loop → DJ_Land | `!bIsFalling` | 0.1 | **탈출 조건에 bIsJetJumping을 쓰지 않는다** — 플래그 해제(서버 `Landed()`)의 복제 도착 순서와 무관하게 동작해야 함 (레이스 회피) |
| DJ_Land → Idle/Walk | Automatic + 조기 탈출 | 0.2 | |
| (Idle/Walk Alias) → Dash_Charge | `bIsDashCharging && !bIsFalling` | 0.2 | 충전 중 이동 가능(§8.2)이므로 Walk에서도 진입. 풀바디 오버라이드지만 바퀴 로봇이라 무해(§13.0-2) |
| Dash_Charge → Dash_Start | `bSustainedDash` | 0.1 | 5칸 만충 해소 |
| Dash_Charge → Idle/Walk | `!bIsDashCharging && !bSustainedDash` | 0.25 | 조기 해소(게이지 1~4) = 일반 돌진 → 가속된 Walk로 자연 복귀 |
| Dash_Charge → Jump_Start | `bIsFalling` | 0.1 | 충전 중 점프 허용. 착지 후에도 충전 유지 중이면 Idle/Walk 경유로 Dash_Charge 재진입 (조건이 계속 참) |
| Dash_Start → Dash_Loop | Automatic | 0.15 | |
| Dash_Loop → Idle/Walk | `!bSustainedDash` | 0.2 | 브레이크/충돌의 실제 연출은 같은 순간 GA가 재생하는 Dash_Stop/Crush **몽타주가 슬롯에서 덮으므로** SM 복귀는 몽타주 뒤에 자연스럽게 드러남 |
| (전 상태 Alias) → Stunned | `bIsStunned` | 0.1 | **전이 Priority 최상위(1)** — 대쉬/점프 중에도 즉시 스턴. §8.2의 스턴 시 GA 취소와 함께 동작 |
| Stunned → Idle/Walk | `!bIsStunned` | 0.25 | |

- **Death는 SM 상태로 만들지 않는다.** `PlayDeathMontage_Internal()`이 `StopAllMontages(0.1)` 후 몽타주를 재생하고(`DRCharacter.cpp:946-948`), 베이스가 `VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones`로 바꿔 오프스크린에서도 재생을 보장한다. 몽타주 종료 후는 Dissolve → Destroy 흐름이라 SM 복귀 상태가 필요 없음. `bDead` 동안 `AimAlpha = 0`으로 조준만 차단.
- Walk의 4방향 커버리지: `Direction`이 대각(-45° 등)일 때 블렌드스페이스가 Forward/Left를 자동 보간. 후방 대각(±135°)은 Backward와 L/R 사이 보간 — 어색하면 샘플 위치를 ±150으로 조정.

### 13.5 몽타주 5종 상세 명세

#### 공통: GAS 몽타주 복제 경로
GA 안에서 `PlayMontageAndWait` 어빌리티 태스크(BP) 또는 `UAbilityTask_PlayMontageAndWait`(C++)로 재생하면, **ASC가 PlayerState에 있는 이 프로젝트 구조에서도 아바타(캐릭터)의 `GetMesh()`에 재생되고, `ReplicatedAnimMontage` 경로로 시뮬레이티드 프록시에 자동 전파**된다. 별도 멀티캐스트 RPC를 만들지 말 것. 시전자 본인 화면(1인칭)에는 어차피 안 보이므로(§13.0-1), 시전자용 연출은 BIE 콜백(자판기 `BlueprintImplementableEvent` 패턴, §1.2)으로 카메라 셰이크/FP 이펙트를 따로 쏜다.

#### ① `AM_VC_HitReact` — 기존 파이프라인 재사용 (코드 작업 0)
- **연결**: `BP_RobotVacuumCharacter` 디테일 → `Combat > HitReactMontages` 배열에 등록 (1개면 그것만 재생, 여러 개면 랜덤 — `DRCharacterBase.cpp:67-83`).
- **트리거 흐름(기존)**: 피격 → `UDRPlayerAttributeSet::ApplyHitReactAndKnockback`(`DRPlayerAttributeSet.cpp:272`)이 `Effects.HitReact` 태그로 어빌리티 활성화 → 공용 `GA_HitReact`(`Content/Blueprints/AbilitySystem/Enemy/Abilities/GA_HitReact.uasset`)가 `GetHitReactMontage()` 재생.
- **확인 필요**: `DA_PlayerCharacterClassInfo`의 `CommonAbilities`에 `GA_HitReact`가 들어있는지 (`GivePlayerStartupAbilities`가 CommonAbilities를 부여 — `DRAbilitySystemLibrary.cpp:171`). 기존 두 클래스가 피격 몽타주를 재생하고 있다면 이미 포함된 것.
- 넉백(`ApplyHitReactAndKnockback`의 `LaunchCharacter`)과 동시 재생됨 — 별도 처리 불필요.

#### ② `AM_VC_Death` — 기존 파이프라인 재사용 (코드 작업 0)
- **연결**: `BP_RobotVacuumCharacter` → `Combat|Death > DeathMontages` 배열에 등록, `DeathMontagePlayRate` 필요 시 조정.
- **동작(기존)**: 서버가 `DeathMontageIndex` 랜덤 결정(복제) → 전 머신 동일 몽타주, `StopAllMontages(0.1)`로 진행 중 몽타주 정리 후 재생, 멀티캐스트/RepNotify 이중 도착에도 1회만(`bDeathMontagePlayed`). 사망 시 라이더 강제 하차(§5.5)로 `bIsBoarded`도 풀림.
- 몽타주 BlendOut 0으로 끝 포즈 유지 → Dissolve 연출이 이어받음.

#### ③ `AM_VC_EnhancedAttack` — GA_VacuumAirShot (Phase C에 추가)
- **재생 지점**: `ReleaseAndFire()`에서 `Stage == 3`일 때만, `FireShot(3)` 호출과 동시에 재생 (0~2단계는 몽타주 없음 — 요구사항).
- **구현**: C++ `UDRVacuumAirShot`에 `UPROPERTY(EditDefaultsOnly, Category="AirShot") TObjectPtr<UAnimMontage> EnhancedAttackMontage;` 추가 → BP `GA_VacuumAirShot`에서 지정 → 발사 직전 `UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(..., EnhancedAttackMontage)` 생성 후 `ReadyForActivation()` (완료 대기 불필요 — 발사·반동·EndAbility는 기존 §6.1 흐름 그대로, 몽타주는 fire-and-forget).
- **주의**: `EndAbility`가 몽타주를 중단하지 않도록 GA의 `bStopMontageWhenAbilityEnds`(태스크 기본 true)를 false로 — 발사 즉시 EndAbility하는 §6.1 구조에서 몽타주가 잘리는 것 방지. (또는 몽타주 종료를 기다렸다가 EndAbility — 연사감이 중요하므로 전자 권장)
- 반동 `LaunchCharacter`(§6.1-3)와 동시 재생 — 몽타주는 인플레이스라 충돌 없음.

#### ④ `AM_VC_Dash_Stop` / ⑤ `AM_VC_Dash_Crush` — GA_VacuumDash (Phase E에 추가)
- **재생 지점**: `FinishDash(bFromImpact, HitActor)`(§8.2) 안에서:
  - `bFromImpact == true` → `AM_VC_Dash_Crush` (일반 돌진 충돌·지속 돌진 충돌 공통 — 충돌=게이지 소멸 이벤트의 공통 연출)
  - `bFromImpact == false && 직전 상태가 지속 돌진` → `AM_VC_Dash_Stop` (S 브레이크)
  - `bFromImpact == false && 일반 돌진 자연 감쇠 종료(게이지 0)` → **몽타주 없음** (가속이 서서히 풀리는 것뿐)
- **구현**: `UDRVacuumDash`에 `DashStopMontage` / `DashCrushMontage` 프로퍼티(EditDefaultsOnly) → PlayMontageAndWait fire-and-forget (③과 동일 요령). `SetSustainedDash(false)`로 SM이 Dash_Loop에서 빠지는 프레임을 몽타주가 슬롯에서 덮으므로 전환이 자연스럽다(§13.4.2).
- **자해 데미지와의 몽타주 경합 주의**: 지속 돌진 충돌 시 본인 50 자해(§8.2)가 `ApplyHitReactAndKnockback` → `GA_HitReact`를 발동시켜 **HitReact 몽타주가 Crush 몽타주를 덮을 수 있다.** 1차 구현은 그대로 두고(둘 다 "충격" 모션이라 시각적 위화감 적음), 거슬리면 자해 GE에 전용 태그를 달아 `ApplyHitReactAndKnockback`에서 HitReact 태그 발화를 스킵하는 예외 1줄 추가.

### 13.6 캐릭터 C++ 추가분 (애님 전용 — §4.2 스켈레톤에 병합)

```cpp
// ===== ADRRobotVacuumCharacter 에 추가 (ABP 전이용 복제 플래그) =====
public:
    // 대쉬 게이지 충전 중 (GA_VacuumDash가 서버에서 세팅) — ABP Dash_Charge 상태 전이용
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Animation")
    bool bIsDashCharging = false;

    // 더블 점프(제트 점프) 체공 중 (GA_VacuumJetJump가 세팅, Landed()에서 해제)
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Animation")
    bool bIsJetJumping = false;

    void SetDashCharging(bool bNew);   // 서버 전용 (GA 타이머가 서버에서만 돌므로 자연 충족)
    void SetJetJumping(bool bNew);     // 서버 전용
```
- `GetLifetimeReplicatedProps`에 `DOREPLIFETIME` 2종 추가. RepNotify 불필요 — ABP가 매 프레임 폴링하는 순수 상태 값 (즉발 이벤트가 아님).
- **세팅 지점**:
  - `GA_VacuumDash::StartChargingGauge()` → `SetDashCharging(true)` / `ReleaseGauge()`·`EndAbility()` → `SetDashCharging(false)` (EndAbility 정리 목록 §8.2에 추가)
  - `GA_VacuumJetJump::ActivateAbility()` → `SetJetJumping(true)` / `ADRRobotVacuumCharacter::Landed()` → `SetJetJumping(false)` (`bAirJumpUsed` 리셋과 같은 지점, §4.2)
  - 공중 Q 재사용(지상 Q → 공중 Q) 시엔 이미 true → 그대로 유지, DJ_Start 상태 재진입은 SM이 "상태 진입 시 처음부터 재생"으로 처리하고 싶으면 DJ_Loop→DJ_Start 재전이 조건(`bIsFalling && 속도 Z > 0` 등)을 추가. 1차는 미구현(공중 Q 연출은 GameplayCue 물분사가 담당).
- `bSustainedDash`(§4.2) / `RiderOnTop`(§4.2) / `bIsStunned`·`bDead`(베이스 기존)는 이미 복제되므로 추가 작업 없음.

### 13.7 1인칭(FP) 처리 방침

- `FirstPersonMesh`는 카메라 부착 + OnlyOwnerSee(`DRCharacter.cpp:63-68`), 클래스별 `ABP_FP_*` 별도 운용(선례: `ABP_FP_Gardener`, `ABP_FP_VendingMachine`).
- **현재 3인칭 애니만 존재 → 1차 구현은 3인칭 전용으로 완결.** FP 메시는 미지정(또는 스태틱 노즐 파츠만 부착)해도 게임플레이 검증에 지장 없음 — 본인 눈에 보이는 것은 FP뿐이고, 3인칭은 타 플레이어/대기실/사망 카메라에서 검증(§13.0-1).
- FP 애니 제작 후: `ABP_FP_VacuumCleaner` 신규 생성. FP는 **로컬 전용 코스메틱이라 복제 고려 불필요** — GA의 BIE 콜백에서 FP 몽타주 재생, 로코모션은 동일 변수 로직 복사.

### 13.8 에디터 작업 체크리스트 (순서대로)

1. **스켈레톤 준비** (`VecuumCleaner_v0_1_1_Skeleton`):
   - 소켓 추가: 발사구 소켓(예: `Muzzle`) → BP에서 `WeaponTipSocketName`에 지정 (`SpawnProjectile(TargetLocation, SocketTag)`가 `GetCombatSocketLocation`으로 조회, §6.1의 `FireSocketTag = CombatSocket.Weapon` 경로), 부품 파지 소켓(기존 관례 `TestPartHand`, `DRCharacter.cpp:84` — 3인칭 부품 메시 부착용)
   - 머리 체인 루트 본 이름 확인 (§13.4.1 Layered Blend Branch용)
2. 시퀀스 임포트 설정 확인: 전체 `EnableRootMotion = false`, AimOffset 3종만 Additive(Mesh Space, Base=`VC_AimOffset_Front`) 설정
3. `AO_VC_Aim`, `BS_VC_Walk` 생성 (§13.2 표)
4. 몽타주 5종 생성 (우클릭 → Create → AnimMontage), 슬롯 DefaultSlot·블렌드 값 §13.2 표대로
5. `ABP_VacuumCleaner` 생성 → §13.3 Event Graph → §13.4 AnimGraph
6. `BP_RobotVacuumCharacter` 연결: Mesh = `VecuumCleaner_v0_1_1`, AnimClass = `ABP_VacuumCleaner`, `HitReactMontages = [AM_VC_HitReact]`, `DeathMontages = [AM_VC_Death]`, `WeaponTipSocketName = Muzzle`, PhysicsAsset 확인
7. GA 프로퍼티 지정: `GA_VacuumAirShot.EnhancedAttackMontage`, `GA_VacuumDash.DashStopMontage / DashCrushMontage`

**PIE 2인 검증 항목** (전부 **상대 클라이언트 화면**에서 확인 — 본인 화면엔 3인칭이 안 보임):
- (a) 4방향 걷기·대각 보간·이속버프 시 재생속도 가속 (b) 점프 3단 / 더블점프 3단 전이, 지상 Q가 DJ_Start로 가는지 (c) 스턴 루프 진입·해제 (d) 탑승 시 머리 눌림 + 눌린 채 주행 (e) 충전(만충 타이밍=애니 클라이맥스) → 지속 돌진 Start/Loop → S 브레이크 Stop 몽타주 / 충돌 Crush 몽타주 (f) 3단계 발사 시에만 EnhancedAttack 재생 (g) 피격 HitReact, 사망 몽타주 후 Dissolve (h) 상하 에임오프셋 추종(RemoteViewPitch 양자화 계단현상은 정상), 탑승·스턴·사망·지속돌진 중 에임 차단

### 13.9 애님 설계 결정사항 (구현 전 확인 권장)

| # | 항목 | 제안 기본값 | 근거 |
|---|---|---|---|
| A1 | Dash_Charge 중 이동 시 풀바디 오버라이드 | **허용 (걷기와 블렌드 안 함)** | 바퀴 로봇이라 슬라이딩 아티팩트 무해(§13.0-2). 어색하면 Layered Blend로 몸통만 charge 재생 |
| A2 | 일반 돌진(게이지 1~4) 전용 애니 | **없음 — Walk PlayRate 가속으로 표현** | 일반 돌진은 "이속버프 걷기"일 뿐(§8.2). Dash_Start/Loop는 지속 돌진 전용 |
| A3 | Crush 몽타주 적용 범위 | **일반 돌진 충돌 + 지속 돌진 충돌 공통** | 충돌=게이지 소멸의 공통 피드백. 지속 돌진만으로 한정하려면 FinishDash 분기 1줄 |
| A4 | DoubleJump_Start를 몽타주가 아닌 SM으로 | **SM** | 체공 시간 가변(Loop)·착지(Land)와 한 흐름. 몽타주로 하면 Loop/Land를 따로 처리해야 해서 손해 |
| A5 | Idle 랜덤 방식 | **Random Sequence Player (가중치 0.85/0.075/0.075)** | 노드 1개로 끝. 연출 통제(특정 순서·최소 간격)가 필요해지면 타이머+Blend Poses by Int로 교체 |
| A6 | Boarding 합성 방식 | **Layered Blend Per Bone (머리 체인)** | 눌린 채 주행 가능해야 함. 스켈레톤이 단순해 브랜치 분리가 안 예쁘면 Additive 방식으로 교체(§13.4.1) |
| A7 | 지속 돌진 중 에임오프셋 | **차단 (AimAlpha 0)** | 돌진 조향은 몸통 회전(컨트롤러 yaw)이 담당 — 머리만 따로 움직이면 어색 |
| A8 | 충돌 자해 50의 HitReact가 Crush를 덮는 문제 | **1차는 방치** | 둘 다 충격 모션이라 위화감 적음. 필요 시 자해 GE 태그 예외(§13.5-⑤) |
