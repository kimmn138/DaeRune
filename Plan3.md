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
- **라이더 착석 애니메이션**: 탑승자 하체 착석 표현 — 기존 클래스 ABP(`ABP_Gardener`, `ABP_VendingMachine`) 수정 + 착석 루프 시퀀스 — **상세 설계는 §14**

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
  2. 지불 가능 확인: 물이 20 미만이면 부족분 ×0.5를 체력으로 대납(`ExecCalc_WaterCost` 규칙). 체력마저 부족하면(지불 시 0 이하 — `CheckCost`와 동일 판정) 충전 일시 정지(게이지 유지, 다음 틱 재시도).
  3. 지불 가능하면 `ApplyEffectSpecWithSetByCaller(ASC, ChargeCostSpec, Cost_Water, +20)` — `ChargeCostEffect`(ExecCalc_WaterCost 실행 GE, `GE_Cost_*` 관례)가 물/체력 배분을 처리, `Gauge++`, `OnGaugeChanged` + ASC 노티파이(§9.2).
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

#### 13.4.3 노드 단위 구성 가이드 ★ (2026-07-14 에셋 실측 기반 — 13.4.1~2와 충돌 시 이 절이 우선)

> `ABP_Gardener`·VC 애님 에셋들의 실제 내부 구조(uasset 파싱)를 기준으로 작성. **현재 `ABP_VacuumCleaner`는 Event Graph(변수 수집)까지 완성, AnimGraph는 Output Pose 노드만 있는 빈 상태**다. 이 절만 보고 AnimGraph를 끝까지 배선할 수 있게 쓴다.

##### (0) 실측으로 확정된 사실 — 13.4.1~2 원안에서 바뀌는 것

| 항목 | 원안(§13.2/13.4) | 실측 (실제 에셋) | 결론 |
|---|---|---|---|
| 걷기 블렌드스페이스 | `BS_VC_Walk` 1D, 축 `Direction`[-180,180], 샘플 5개 | **`BC_VC_Walk` 2D** (`/Script/Engine.BlendSpace`), 축 `MoveForward` / `MoveRight`, 샘플 4개(F/B/L/R) — **Gardener `BS_Walk`와 동일 구성** | 2D 그대로 사용. Walk 상태에서 X/Y 핀에 `RightDirection`/`ForwardDirection` 바인딩 (Gardener 방식) |
| 걷기 입력 변수 | `Speed`+`Direction`(Calculate Direction) | ABP에 이미 `ForwardDirection`/`RightDirection` 구현됨 — `Dot(Normal(Velocity), GetActorForwardVector/RightVector)` ∈ [-1,1] | 그대로 사용. `Calculate Direction` 불필요 |
| 낙하 변수 | `bIsFalling` | 이미 **`bIsInAir`** 라는 이름으로 구현됨 | 전이 조건에 `bIsInAir` 사용 |
| 탑승 변수 | `bIsBoarded` | 이미 **`bIsBordered`** 라는 이름으로 구현됨 (`RiderOnTop != nullptr`, NotEqual 노드 확인) | 오타지만 동작 무관 — 그대로 사용 (이름 바꾸면 바인딩 재작업) |
| AimOffset | 사전작업 필요(Additive 설정) | **설정 완료**: `AO_VC_Aim`(1D, 축 `Pitch`) + 시퀀스 3종 `AAT_RotationOffsetMeshSpace` / `ABPT_RefPose` | 사전작업 없음. 바로 노드 배치 |
| AimAlpha | float 변수 + FInterpTo(5.0) | Gardener는 AO 노드의 **Alpha Input Type=Bool + AlphaBoolBlend**(내장 블렌드) 사용 | Bool 방식 채택 — float 변수/보간 로직 불필요 (아래 (2)) |
| 몽타주 5종 | 생성 예정 | **5종 모두 생성 완료** (`AM_VC_*`, 슬롯 DefaultSlot, `TP/` 폴더) | 등록만 하면 됨 |
| 스켈레톤 소켓 | `Muzzle`/`TestPartHand` 추가 예정 | **둘 다 추가 완료** | §13.8-1 소켓 작업 완료 처리 |
| 머리 체인 루트 본 | 에디터에서 확인 필요 | 본 계층 실측: `Armature_ProxyTrueRootJoint → Body → { Arm_L, Arm_R, Head → {Ear_L, Ear_R}, BodyDoorController, CleanerHead → {CleanerHead_002_L/R} }` | Boarding 브랜치 본 = **`Head`** (귀 2개 포함). `CleanerHead`는 전방 흡입부라 제외 |
| 에셋 폴더 | `Content/DaeRuneAssets/Characters/VacuumCleaner/` | 실제는 하위 **`…/VacuumCleaner/TP/`** | 경로 참조 시 주의 |

참고 — `ABP_Gardener` 실측 구조 (따라할 관례): 스테이트머신 "Main States"(Idle/Walk/Jump/FallLoop/Land/Stun + **State Alias `ToFalling`/`ToStun`**) → Save Cached Pose → Layered Blend Per Bone ×2 (브랜치 본 `Arm_001_L`/`Arm_001_R`, 부품들기 `Gardener_HoldingClenserPart`·`Gardener_WaterBlast_Hold` 유지포즈) → Slot(DefaultSlot) → AO_Gardener → Output. Idle은 Random Sequence Player(Idle1/2/3). 전이 조건은 이벤트그래프 변수 + Property Access 혼용.

##### (1) Event Graph 보완 — 변수 3개 추가

기존 수집 변수(`ForwardDirection`, `RightDirection`, `AimPitch`, `bIsInAir`, `bIsStunned`, `bIsBordered`, `bIsCarryingPart`, `bIsDashCharging`, `bIsJetJumping`, `bSustainedDash`)에 아래를 추가한다 (`Event Blueprint Update Animation` 체인 끝에):

| 변수 | 타입 | 계산 | 용도 |
|---|---|---|---|
| `Speed` | float | `VSizeXY(GetVelocity())` — 이미 그래프에 있는 VSizeXY 노드 출력을 변수 저장으로 분기 | Idle↔Walk 전이, WalkPlayRate |
| `WalkPlayRate` | float | `FClamp(Speed / 250.0, 0.6, 2.5)` (250 = BaseWalkSpeed 수동 일치) | Walk 재생속도 — 대쉬 이속버프·부품 감속 자동 반영(§13.1) |
| `bAimEnabled` | bool | `NOT(bDead OR bIsStunned OR bIsBordered OR bSustainedDash)` — **`bDead` 수집도 이때 추가** (`OwnerVacuum.bDead`, `DRCharacterBase.h:113`) | AO 노드 Bool Alpha 입력 (블렌드는 노드 내장 AlphaBoolBlend가 처리) |
| `BoardingWeight` | float | `FInterpTo(BoardingWeight, bIsBordered ? 1.0 : 0.0, DeltaTimeX, 8.0)` | Layered Blend Per Bone의 Blend Weights — 탑승/하차 스냅 방지 |

- `bIsCarryingPart`는 **현재 AnimGraph에서 미사용** (VC 전용 부품들기 시퀀스가 없음). 추후 애니 제작 시 Gardener처럼 `Arm_L`/`Arm_R` 브랜치 Layered Blend 추가에 사용 — 변수는 이미 수집 중이므로 삭제하지 말 것.

##### (2) AnimGraph 배선 — 노드 5개, 이 순서로

```
[State Machine "Locomotion"] → [DefaultSlot] → [AO_VC_Aim] → [Layered blend per bone] → [Output Pose]
```

| # | 노드 (팔레트 검색어) | 디테일 설정 | 핀 바인딩 |
|---|---|---|---|
| 1 | **State Machine** — 이름 `Locomotion` | 기본값 (Max Transitions Per Frame 1) | 내부는 (3) |
| 2 | **Slot 'DefaultSlot'** | Slot Name = `DefaultGroup.DefaultSlot` (기본값 그대로) | Source ← SM 출력 |
| 3 | **AO_VC_Aim** (에셋 드래그 = Rotation Offset Blend Space 노드) | **Alpha Input Type = `Bool`**, Blend Settings(AlphaBoolBlend): Blend In Time `0.25` / Blend Out Time `0.25` | `Pitch` 핀 ← `AimPitch`, `bEnabled`(Alpha bool) 핀 ← `bAimEnabled` |
| 4 | **Layered blend per bone** | Blend Mode = `Branch Filter`, Layer Setup → Branch Filters [0]: Bone Name = **`Head`**, Blend Depth = `0` · **Mesh Space Rotation Blend = ✔** | Base Pose ← AO 출력, Blend Poses 0 ← **Sequence Player `VC_Boarding`** (Loop ✔), Blend Weights 0 ← `BoardingWeight` |
| 5 | Output Pose | — | Result ← 4번 출력 |

- Save/Use Cached Pose는 **만들지 않는다** — Gardener가 캐시를 쓰는 이유는 로코모션 출력을 Layered Blend 2개(양팔)에 재사용하기 위함인데, VC는 SM 출력을 한 곳에서만 소비한다. VC 부품들기 애니가 생기면 그때 같은 패턴으로 리팩터.
- Slot이 AO/Boarding **앞**인 이유는 §13.4.1 그대로 (몽타주 중 조준·머리눌림 유지).
- 4번에서 `Head` 브랜치가 실제 "눌리는 머리"인지 1회 검증: 스켈레톤 에디터에서 `VC_Boarding` 재생 → 움직이는 본이 `Head`(+귀)인지 확인. 만약 Body까지 움직이면 Branch를 `Body`로 올리는 대신 §13.4.1의 Additive 대안으로 전환.

##### (3) Locomotion SM — 상태 11개 + State Alias 3개

**상태 노드와 내부 재생 노드** (상태 이름은 그대로 지정 — 디버깅·전이표 대조용):

| 상태 | 내부 노드 | 노드 디테일 |
|---|---|---|
| `Idle` (Entry 연결) | **Random Sequence Player** | Entries 3개: `VC_Idle_1` Chance `0.85` / `VC_Idle_2` `0.075` / `VC_Idle_3` `0.075`, 전부 BlendTime `0.25`, Shuffle Mode ☐ |
| `Walk` | **BC_VC_Walk** (Blendspace Player) | Loop ✔. 핀: `MoveForward` ← `ForwardDirection`, `MoveRight` ← `RightDirection`, `PlayRate` ← `WalkPlayRate` |
| `Jump_Start` | Sequence Player `VC_Jump_Start` | Loop ☐ |
| `Jump_Loop` | Sequence Player `VC_Jump_Loop` | Loop ✔ |
| `Jump_Land` | Sequence Player `VC_Jump_Land` | Loop ☐ |
| `DJ_Start` | Sequence Player `VC_DoubleJump_Start` | Loop ☐ |
| `DJ_Loop` | Sequence Player `VC_DoubleJump_Loop` | Loop ✔ |
| `DJ_Land` | Sequence Player `VC_DoubleJump_Land` | Loop ☐ |
| `Dash_Charge` | Sequence Player `VC_Dash_Charge` | Loop ☐ (논루프 시퀀스는 종료 후 마지막 프레임 유지 = 만충 홀드 포즈). **PlayRate = 시퀀스 길이 ÷ 2.5s** — 에디터에서 실제 길이 확인 후 계산(원안 가정 5s → 2.0). 상태 재진입 시 처음부터 재생됨(상태 진입 = 노드 리셋, 기본 동작) |
| `Dash_Start` | Sequence Player `VC_Dash_Start` | Loop ☐ |
| `Dash_Loop` | Sequence Player `VC_Dash_Loop` | Loop ✔ |
| `Stunned` | Sequence Player `VC_Stun` | Loop ✔ |

**State Alias 3개** (우클릭 → Add State Alias — Gardener의 `ToFalling`/`ToStun` 관례):

| Alias 이름 | 커버 상태 (체크박스) | 나가는 전이 |
|---|---|---|
| `ToStun` | **전체** (Stunned 제외) | → `Stunned` |
| `ToFalling` | `Idle`, `Walk`, `Dash_Charge` | → `Jump_Start` |
| `ToDJ` | `Idle`, `Walk`, `Jump_Start`, `Jump_Loop`, `Jump_Land`, `Dash_Charge` | → `DJ_Start` (지상 Q + 공중 Q + 충전 중 Q 전부 커버) |

**전이표** — 조건식은 전부 ABP 변수만 사용(Result 핀에 직결, 함수 호출 없음 → 스레드세이프·경고 없음). Priority Order는 **숫자가 작을수록 먼저 평가**:

| 전이 | 조건 (Result 핀) | Blend | Priority | 비고 |
|---|---|---|---|---|
| ToStun → Stunned | `bIsStunned` | 0.1 | **1** | 최우선 — 대쉬/점프 중에도 즉시 |
| ToDJ → DJ_Start | `bIsJetJumping AND bIsInAir` | 0.1 | **2** | 일반 점프 전이보다 먼저 평가돼야 지상 Q가 Jump_Start로 새지 않음 |
| ToFalling → Jump_Start | `bIsInAir AND NOT bIsJetJumping` | 0.1 | 3 | 점프·낙하 공용 |
| Idle → Walk | `Speed > 3` | 0.15 | 3 | |
| Walk → Idle | `Speed <= 3` | 0.2 | 3 | |
| Idle → Dash_Charge | `bIsDashCharging AND NOT bIsInAir` | 0.2 | 3 | |
| Walk → Dash_Charge | `bIsDashCharging AND NOT bIsInAir` | 0.2 | 3 | |
| Jump_Start → Jump_Loop | **Automatic Rule** ✔ (조건 노드 없음) | 0.15 | 3 | 전이 디테일 "Automatic Rule Based on Sequence Player in State" 체크 |
| Jump_Loop → Jump_Land | `NOT bIsInAir` | 0.1 | 3 | |
| Jump_Land → Walk | `Speed > 3` | 0.2 | 3 | 조기 탈출(착지 직후 이동 반응성) |
| Jump_Land → Idle | Automatic Rule ✔ | 0.2 | 4 | Walk 조기탈출보다 뒤 |
| DJ_Start → DJ_Loop | Automatic Rule ✔ | 0.15 | 3 | |
| DJ_Loop → DJ_Land | `NOT bIsInAir` | 0.1 | 3 | **`bIsJetJumping`을 조건에 쓰지 않는다**(§13.4.2 레이스 회피) |
| DJ_Land → Walk | `Speed > 3` | 0.2 | 3 | |
| DJ_Land → Idle | Automatic Rule ✔ | 0.2 | 4 | |
| Dash_Charge → Dash_Start | `bSustainedDash` | 0.1 | 3 | 5칸 만충 해소 |
| Dash_Charge → Walk | `NOT bIsDashCharging AND NOT bSustainedDash AND Speed > 3` | 0.25 | 4 | 조기 해소 = 일반 돌진 → 가속 Walk 복귀 |
| Dash_Charge → Idle | `NOT bIsDashCharging AND NOT bSustainedDash` | 0.25 | 5 | |
| Dash_Start → Dash_Loop | Automatic Rule ✔ | 0.15 | 3 | |
| Dash_Loop → Walk | `NOT bSustainedDash AND Speed > 3` | 0.2 | 3 | 실제 연출은 Stop/Crush 몽타주가 슬롯에서 덮음 |
| Dash_Loop → Idle | `NOT bSustainedDash` | 0.2 | 4 | |
| Stunned → Walk | `NOT bIsStunned AND Speed > 3` | 0.25 | 3 | |
| Stunned → Idle | `NOT bIsStunned` | 0.25 | 4 | |

- Dash_Charge 중 점프/낙하는 별도 전이 불필요 — `ToFalling` alias가 Dash_Charge를 커버한다(위 표). 착지 후 충전이 유지 중이면 Jump_Land → Idle/Walk → Dash_Charge로 자동 재진입(조건이 계속 참).
- Stunned 진입이 alias `ToStun` 하나로 끝나는 대신, **DJ/Jump 상태에서 스턴 → 해제 시 공중이면** Stunned → Idle 후 `ToFalling`이 즉시 Jump_Start로 끌고 간다 — 별도 처리 불필요.

##### (4) 컴파일 검증 (Gardener에서 실제로 발견된 실수 예방)

1. 컴파일 후 경고 0 확인 — `ABP_Gardener` 에셋에는 **"Idle to Walk will never be taken, please connect something to Can Enter Transition"** 경고가 저장된 이력이 있다. 전이 그래프에서 조건을 Result 핀에 연결하지 않은 채 저장한 흔적 — Automatic Rule을 쓰는 전이 외에는 반드시 Result에 조건이 연결돼야 한다.
2. Automatic Rule 전이는 Result 연결이 필요 없다(체크박스가 조건을 대체) — 여기에 조건까지 이중으로 걸지 말 것.
3. AnimGraph 노드에 우클릭 → Disable 상태(반투명)로 남은 노드가 없는지 확인 — Gardener에는 Disabled 노드(구버전 SM "Main States" 외 `Locomotion` SM 잔재)가 남아 있다. VC는 깨끗하게.
4. PIE 검증 항목은 §13.8 그대로.

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

> 애님 파트 한정 체크리스트. **BP/GE/GA/DA를 포함한 전체 에디터 작업 순서는 §13.10이 총괄**한다.

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

### 13.10 에디터 통합 작업 가이드 ★ — BP/GE/GA/DA 전체 (이 순서대로 진행)

> C++ 구현(Phase A~E + §13.5/13.6 애님 훅)이 **모두 빌드 완료된 상태** 기준. 아래 순서는 의존성 순서다 — 각 단계는 이전 단계 산출물을 참조한다. 중간의 ✅ **검증 게이트**에서 PIE로 확인하고 넘어갈 것.
>
> **경로 관례** (기존 클래스 폴더 구조를 따름):
> - 캐릭터: `Content/Blueprints/Character/PlayerCharacter/VacuumCleaner/`
> - 어빌리티/GE: `Content/Blueprints/AbilitySystem/Player/VacuumCleaner/…` (GardenRobot / VendingMachine 폴더와 나란히)
> - 참고용 기존 에셋: `BP_GardenRobot`, `BP_VendingMachine`, `ABP_Gardener`, `ABP_VendingMachine`, `DA_PlayerCharacterClassInfo`(`Content/Blueprints/AbilitySystem/Data/`)

#### 0단계 — 사전 확인 (5분)

| 확인 | 방법 |
|---|---|
| C++ 클래스 5종이 에디터에 보이는지 | BP 생성 다이얼로그에서 부모 검색: `DRRobotVacuumCharacter`, `DRVacuumAirShot`, `DRVacuumJetJump`, `DRVacuumDash`, `DRVacuumAirProjectile` |
| 태그 등록 확인 | 프로젝트 세팅 → GameplayTags: `State.Riding`, `State.RobotVacuum.SustainedDash`, `Buff.RobotVacuum.DashSpeed`, `Abilities.RobotVacuum.*` 3종, `GameplayCue.Skill.Vacuum*` 3종, `Event.Dash.Brake`, `Damage.MountShared` |
| 애님 시퀀스 24종 | `Content/DaeRuneAssets/Characters/VacuumCleaner/` — `VC_*` + `VecuumCleaner_v0_1_1`(+`_Skeleton`, `_PhysicsAsset`) |

#### 1단계 — 스켈레톤 준비 + 애님 에셋 (§13.8-1~4 상세)

1. **소켓 2개 추가** — `VecuumCleaner_v0_1_1_Skeleton` 열기 → 본 트리 우클릭 → Add Socket:
   - `Muzzle` (발사구 본에 부착, 노즐 끝으로 이동) — §6.1 `CombatSocket.Weapon` → `GetCombatSocketLocation()` 조회 경로
   - `TestPartHand` (부품 파지 관례 소켓명, `DRCharacter.cpp:84`) — 부품 3인칭 부착용
   - 이때 **머리 체인 루트 본 이름**을 메모 (§13.4.1 Layered Blend Branch + §14와 무관하게 청소기 Boarding용)
2. **임포트 설정 일괄 확인** — `VC_*` 24종 전체: 디테일 → `EnableRootMotion = false` (특히 `VC_Dash_Start/Loop/Crush`). `VC_AimOffset_Down/Front/Up` 3종만: `Additive Anim Type = Mesh Space`, `Base Pose Type = Selected animation frame`, `Base Pose Animation = VC_AimOffset_Front`
3. **`AO_VC_Aim`** (Aim Offset 1D, 스켈레톤 지정 생성): Horizontal Axis 이름 `Pitch`, 범위 [-90, 90], 샘플 3개 배치(Down=-90 / Front=0 / Up=+90)
4. **`BS_VC_Walk`** (Blend Space 1D): 축 이름 `Direction`, 범위 [-180, 180], 샘플 5개: `VC_Walk_Backward`(-180) / `VC_Walk_Left`(-90) / `VC_Walk_Forward`(0) / `VC_Walk_Right`(+90) / `VC_Walk_Backward`(+180)
5. **몽타주 5종** — 시퀀스 우클릭 → Create → Create AnimMontage, 전부 슬롯 `DefaultGroup.DefaultSlot`:

| 몽타주 | 원본 | Blend In / Out | 비고 |
|---|---|---|---|
| `AM_VC_HitReact` | `VC_HitReact` | 0.1 / 0.2 | |
| `AM_VC_Death` | `VC_Death` | 0.1 / **0.0** | 끝 포즈 유지 → Dissolve 연계 |
| `AM_VC_EnhancedAttack` | `VC_EnhancedAttack` | 0.1 / 0.2 | (선택) 총구 VFX AnimNotify — 코스메틱만(§13.0-3) |
| `AM_VC_Dash_Stop` | `VC_Dash_Stop` | **0.05** / 0.2 | 급정거 스냅감 |
| `AM_VC_Dash_Crush` | `VC_Dash_Crush` | **0.05** / 0.2 | |

#### 2단계 — `ABP_VacuumCleaner` 생성

`Content/Blueprints/Character/PlayerCharacter/VacuumCleaner/`에 Animation Blueprint 생성 (스켈레톤 `VecuumCleaner_v0_1_1_Skeleton`).
- **Event Graph**: §13.3 표 그대로 — `Initialize`에서 `TryGetPawnOwner → Cast to DRRobotVacuumCharacter → OwnerVacuum` 캐싱, `Update`에서 12개 변수 수집 (`bIsBoarded = IsValid(RiderOnTop)` 주의 — §14.4의 라이더용 `bIsRiding`과 혼동 금지)
- **AnimGraph**: §13.4.1 파이프라인(SM → Slot → AO → Layered Blend Per Bone(머리 체인, `bIsBoarded`) → Output) + §13.4.2 스테이트머신/전이표 그대로
- Dash_Charge PlayRate 2.0(§13.4.2 표), Idle Random Sequence Player 가중치 0.85/0.075/0.075

#### 3단계 — `BP_RobotVacuumCharacter` 생성 (디테일 전체)

부모 `ADRRobotVacuumCharacter`, 위치 `…/PlayerCharacter/VacuumCleaner/`.

| 항목 | 설정 | 근거 |
|---|---|---|
| Mesh → Skeletal Mesh | `VecuumCleaner_v0_1_1` | |
| Mesh → Location/Rotation | Z = -캡슐 HalfHeight, Yaw = -90° | 표준 관례 (BP_GardenRobot 값 참고) |
| Mesh → Anim Class | `ABP_VacuumCleaner` | 2단계 산출물 |
| Mesh → Physics Asset | `VecuumCleaner_v0_1_1_PhysicsAsset` 자동 확인 | |
| Capsule Half Height / Radius | 메시에 맞게 축소 (낮고 넓은 로봇 — 예: 55 / 45에서 시작) | 캡슐이 곧 돌진 충돌체(§8.3) |
| Class Defaults → `PlayerCharacterClass` | `Robot Vacuum` | 어트리뷰트 초기화 클래스 키 |
| Combat → `WeaponTipSocketName` | `Muzzle` | 1단계 소켓 |
| Combat → `HitReactMontages` | `[AM_VC_HitReact]` | §13.5-① (코드 작업 0) |
| Combat\|Death → `DeathMontages` | `[AM_VC_Death]` | §13.5-② |
| Mount → `RideAttachPoint` 위치 | Z를 메시 상단으로 (기본 90 → 캡슐/메시 보고 조정) | 라이더 발이 뚫거나 뜨면 §14.7-4처럼 미세조정 |
| Mount → `MountPromptWidget` → Widget Class | `WBP_PartInteraction` 복제 → `WBP_MountInteraction` ("F 탑승" 텍스트) | 로컬 코스메틱(§5.1) |
| Mount → `MountSharedDamageEffectClass` | `GE_Damage` | §5.4 — 미설정 시 경고 로그 + 공유 무시 |
| Dash → `DashImpactSound` | 임팩트 사운드 (임시로 기존 Armadillo 것 재사용 가능) | Phase E 멀티캐스트 |
| Dash → `DashImpactMinSpeed` | 700 기본 유지 | §8.3 |
| SpringArm/Camera | BP_GardenRobot 값 복사 후 로봇 높이에 맞게 Z 조정 | |
| FirstPersonMesh | **미지정** (1차 3인칭 완결) | §13.7 |

#### 4단계 — GE 4종 생성 (`…/AbilitySystem/Player/VacuumCleaner/Effects/`)

| GE | 만드는 법 | 핵심 설정 |
|---|---|---|
| `GE_PrimaryAttributes_RobotVacuum` | `GE_DRPrimaryAttributes`(Player/Effects) 복제 | Instant. Modifier: `MaxHealth` / `MaxWater` / `MoveSpeed` Override — 기존 클래스 값 복사 후 **MoveSpeed만 상향** (빠른 기동 콘셉트, 밸런스는 §9.4) |
| `GE_VitalAttributes_RobotVacuum` | `GE_VitalAttributes` 복제 | Instant. `Health = MaxHealth`, `Water = MaxWater` (Attribute Based) — 원본 그대로면 복제만 |
| `GE_VacuumDashSpeedBuff` | 신규 | **Infinite**. Modifier: `MoveSpeed`, Op **Multiply**, Magnitude **1.2** (GAS 곱연산 합산 규칙: 5스택 = ×(1+0.2×5) = ×2.0). Stacking: **Aggregate by Target, Stack Limit 5**. Asset Tag(태그 컴포넌트): `Buff.RobotVacuum.DashSpeed` — 공기탄 무반동 판정(`HasDashBuff`)이 이 태그로 조회 |
| `GE_VacuumDashSelfDamage` | 신규 | Instant. Modifier: `IncomingDamage`, Op Add, Magnitude **50** — 메타 어트리뷰트 경유라 컨테이너 파이프라인(`HandleIncomingDamage`)을 그대로 탄다. ※ 부수효과: HitReact 발동(§13.9-A8 — 1차 방치), 라이더 데미지 공유 1홉 전파(§5.4 스펙상 자연스러움) |

**신규 코스트 GE 2건 추가** — 둘 다 기존 `GE_Cost_WaterPump`(GardenRobot/Skill2) 복제 (Instant, Executions에 `ExecCalc_WaterCost`, SetByCaller 태그 `Cost.Water` — 값은 코드가 주입). 물 부족 시 부족분 ×0.5 체력 대납이 ExecCalc에서 자동 처리된다:
- `GE_Cost_VacuumJetJump` → `GA_VacuumJetJump`의 **Cost Gameplay Effect Class**에 지정 (7단계 ②) — 미지정 시 `CheckCost`는 통과해도 `ApplyCost`가 no-op이라 물 30이 안 깎인다(`DRGameplayAbility.cpp:65`)
- `GE_Cost_VacuumDashCharge` → `GA_VacuumDash`의 **Charge Cost Effect**에 지정 (7단계 ③, §8.2 틱 충전용)

(구버전 계획의 `GE_WaterReduction` 재사용은 폐기 — 체력 대납 없이 물만 깎는 방식이었음)

#### 5단계 — `DA_PlayerCharacterClassInfo` 등록

`Content/Blueprints/AbilitySystem/Data/DA_PlayerCharacterClassInfo` 열기:
1. `Character Class Information` 맵에 키 **`Robot Vacuum`** 추가:
   - `PrimaryAttributes = GE_PrimaryAttributes_RobotVacuum`, `VitalAttributes = GE_VitalAttributes_RobotVacuum`
   - `StartupAbilities = []` (7단계에서 채움), `DeathAbilities` = 기존 클래스 항목 복사
   - `SkillIconWidgetClass` / `CharacterInfoWidgetClass` = **임시로 기존 것**(예: Gardener) 지정 — 전용 위젯은 Phase F
2. `Character BP Classes` 맵에 **`Robot Vacuum` → `BP_RobotVacuumCharacter`** 추가
3. `Common Abilities`에 `GA_HitReact` 포함 확인 (§13.5-①)
4. (선택) `DA_TutorialPlayerCharacterClassInfo`에도 동일 등록 — 튜토리얼에서 선택 가능하게 하려면

> ✅ **검증 게이트 1 (§4.4)**: PIE 2인 → 로비에서 클래스 순환에 RobotVacuum 등장 → 선택 후 스테이지 스폰 → 이동/점프/카메라/이속 정상, 상대 화면에서 걷기 애니(§13 로코모션) 재생.

#### 6단계 — `BP_VacuumAirProjectile` 생성

부모 `ADRVacuumAirProjectile`, 위치 `…/VacuumCleaner/Abilities/BasicAttack/`.
- 컴포넌트: Sphere(루트, 반경 ~15–25) 자식으로 StaticMesh 또는 Niagara(공기탄 외형, **Collision = NoCollision**)
- 디테일: `ActiveDuration = 2.0` / `FadeDuration = 0.5` 확인, `ImpactEffect`(나이아가라) / `ImpactSound` 지정, `LoopingSound`(선택)
- 이벤트 그래프: **Event OnFadeStarted** → 타임라인 0.5초 → 머티리얼 Opacity / 나이아가라 스케일 페이드아웃 (서버·클라 각자 로컬 재생 — RepNotify 경로로 이미 호출됨)
- (선택) Event BeginPlay에서 `bEnhanced` 분기 → 강화탄 크기/색 강조

#### 7단계 — GA 3종 생성 (`…/VacuumCleaner/Abilities/…`)

**공통 클래스 설정** (3종 모두): `Instancing Policy = Instanced Per Actor` (**필수** — C++이 멤버 타이머/게이지 상태 사용), `Net Execution Policy = Local Predicted` (자판기 GA 관례와 동일하게).

**① `GA_VacuumAirShot`** (부모 `UDRVacuumAirShot`, `…/BasicAttack/`)

| 프로퍼티 | 값 |
|---|---|
| Startup Input Tag | `InputTag.LMB` |
| Ability Tags | `Abilities.RobotVacuum.AirShot` |
| Fire Socket Tag | `CombatSocket.Weapon` |
| Projectile Class | `BP_VacuumAirProjectile` |
| Damage Effect Class / Damage Type | `GE_Damage` / `Damage.Physical` |
| Stages | 4개 기본값(10/15/22/35, 1500~2600) 확인 — 밸런스 조정처 |
| Charge Interval / Recoil Impulse / Enhanced Knockback Force | 0.5 / 1200 / 1000 |
| Enhanced Attack Montage | `AM_VC_EnhancedAttack` |

이벤트 그래프: `Event ActivateAbility` → `StartCharging` → **`Wait Input Release`** 태스크 → `ReleaseAndFire` (EndAbility는 C++ 내부에서 호출 — BP에서 부르지 말 것). `OnChargeStageChanged` 이벤트는 비워두거나 `IsLocallyControlled` 분기 후 충전 SFX(Phase F).

**② `GA_VacuumJetJump`** (부모 `UDRVacuumJetJump`, `…/Skill1_JetJump/`)

| 프로퍼티 | 값 |
|---|---|
| Startup Input Tag | `InputTag.Q` |
| Ability Tags | `Abilities.RobotVacuum.JetJump` |
| Damage Effect Class / Damage Type | `GE_Damage` / `Damage.Physical` |
| Water Cost | 30 (C++ 기본값 확인만) |
| **Costs → Cost Gameplay Effect Class** | **`GE_Cost_VacuumJetJump`** (4단계 생성) — **누락 시 물이 안 깎임**: `CommitAbility`의 `ApplyCost`가 이 GE를 통해서만 차감한다(`DRGameplayAbility.cpp:65`) |
| Jump Impulse Z / Inner·Outer Radius / Inner·Outer Damage | 900 / 200·400 / 20·5 |

이벤트 그래프: **비워둔다** — C++ `ActivateAbility`가 전부 처리(즉발). BP에서 ActivateAbility 이벤트를 추가하지 말 것.
코스트 동작(§ExecCalc_WaterCost 공통 규칙): 물 30 미만이면 부족분 ×0.5를 체력으로 대납. 체력마저 부족하면(지불 후 0 이하) `CheckCost`가 **발동 자체를 차단**(`DRGameplayAbility.cpp:40-51`).

**③ `GA_VacuumDash`** (부모 `UDRVacuumDash`, `…/Skill2_Dash/`)

| 프로퍼티 | 값 |
|---|---|
| Startup Input Tag | `InputTag.RMB` |
| Ability Tags | `Abilities.RobotVacuum.Dash` |
| Activation Blocked Tags | `Debuff.Stun` (§8.2 — 스턴 중 발동 차단) |
| Max Gauge / Charge·Decay Interval / Water Per Gauge | 5 / 0.5·1.0 / 20 |
| Speed Buff Effect / Self Damage Effect / Charge Cost Effect | `GE_VacuumDashSpeedBuff` / `GE_VacuumDashSelfDamage` / `GE_Cost_VacuumDashCharge` |
| Enemy·Player Damage Per Gauge | 10 / 5 |
| Dash Stop·Crush Montage | `AM_VC_Dash_Stop` / `AM_VC_Dash_Crush` |
| Damage Effect Class / Damage Type | `GE_Damage` / `Damage.Physical` (충돌 데미지 파라미터의 기반) |

이벤트 그래프: `Event ActivateAbility` → `StartChargingGauge` → 병렬 2개: ① `Wait Input Release` → `ReleaseGauge` ② `Wait Gameplay Event`(Tag=`Event.Dash.Brake`, Only Trigger Once=false) → `OnBrakePressed`. `OnGaugeChanged`는 Phase F까지 비워둠.
코스트 동작: **Costs → Cost Gameplay Effect Class는 비워둔다** (Water Cost도 0 유지) — 대쉬는 발동 시 1회 지불이 아니라 `TickCharge()`가 0.5초마다 `Charge Cost Effect`(`GE_Cost_VacuumDashCharge`)로 20씩 지불한다. 물 부족 시 부족분 ×0.5 체력 대납(ExecCalc), **물·체력 모두 부족하면 그 틱은 충전 일시 정지**(게이지 유지, 다음 틱 재시도 — `DRVacuumDash.cpp TickCharge`).
스턴 시 **진행 중 취소**까지 원하면: 기존 스턴 처리(`GE_Debuff_Stun` / 스턴 어빌리티)가 `Abilities.RobotVacuum.Dash`를 Cancel 하는 경로 확인 후 태그 추가(§8.2).

#### 8단계 — `GC_VacuumJetJump` 게임플레이 큐

기존 `GameplayCue_Skill_WaterPump` 노티파이 에셋과 같은 위치/방식으로 **GameplayCueNotify_Burst** 생성:
- Gameplay Cue Tag: `GameplayCue.Skill.VacuumJetJump`
- Burst Effects: 물 분사 나이아가라 + 점프 SFX, 스폰 위치 = Cue Parameters `Location` (C++이 시전 위치를 넣어줌, §7.1-5)
- (`GameplayCue.Skill.VacuumAirShot` / `VacuumDash` 큐는 Phase F에서)

#### 9단계 — 마무리 등록 + 기존 BP 3건 수정

1. `DA_PlayerCharacterClassInfo` → RobotVacuum `StartupAbilities = [GA_VacuumAirShot, GA_VacuumJetJump, GA_VacuumDash]`
2. **`BP_GardenRobot` / `BP_VendingMachine`**: `Mount → MountSharedDamageEffectClass = GE_Damage` (라이더 측 데미지 공유 — 미설정 시 경고 로그)
3. (§14 병행 시) `ABP_Gardener` / `ABP_VendingMachine` 착석 레이어 + `GR_Sit_Loop`/`VM_Sit_Loop` — §14.7 체크리스트

> ✅ **검증 게이트 2 (스킬)**: §6.3(공기탄: 즉발/홀드 단계/2초 페이드/강화 넉백·반동·로켓점프) → §7.2(더블점프: 물 30/공중 1회/2D AoE) → §8.4(돌진: 게이지/버프/충돌/지속/브레이크) 순서로 PIE 2인 확인.
> ✅ **검증 게이트 3 (탑승, §5.6)**: F 탑승/점프 하차/탑쌓기 3-2-1/부품 소지 차단/데미지 공유/사망 하차 — 클라 화면 위치 동기화 중점.
> ✅ **검증 게이트 4 (애니)**: §13.8 PIE 항목 (a)~(h) + (§14 적용 시) §14.8 — 전부 **상대 클라이언트 화면**에서.

#### 자주 하는 실수 (사전 경고)

- GA `Instancing Policy`를 기본값(Non-Instanced)으로 두면 **충전/게이지가 전혀 동작하지 않음** (멤버 상태 사용 불가)
- `MountSharedDamageEffectClass` 누락 → 데미지 공유만 조용히 빠짐 (로그 경고 확인)
- AimOffset 시퀀스를 Additive로 안 바꾸면 `AO_VC_Aim`에서 포즈 폭발
- `BS_VC_Walk` 축 범위를 [0,360]으로 만들면 `Calculate Direction`([-180,180])과 불일치 → 후진 시 뒤집힘
- 몽타주 슬롯을 새로 만들면 ABP Slot 노드(`DefaultSlot`)와 안 맞아 재생 안 됨
- `DA_PlayerCharacterClassInfo`의 `CharacterBPClasses` 누락 → 로비에서 선택은 되지만 스폰 시 폴백(스폰 안 됨/기본 클래스) — `Find()` 널체크 폴백 경로

---

## 14. 탑승자(라이더) 착석 애니메이션 설계

> 로봇 청소기에 탄 **라이더의 하체를 앉은 자세로** 표현하는 설계.
> §13이 청소기 본체(마운트 측)의 애니라면, §14는 **그 위에 탄 다른 캐릭터(라이더 측)**의 애니다.
> 코드 의존은 Phase B 완료분(`MountedOn` 복제)뿐이므로 Phase B 이후 언제든 병행 가능한 순수 코스메틱 작업.

### 14.0 요구사항과 설계 전제 (코드 근거)

**요구**: 탑승 중 라이더의 **하체 = 앉은 자세 고정**, **상체 = 기존 동작 유지** (Idle/에임/공격 몽타주). 탑승 중 어빌리티 사용이 허용되는 "이동 포탑" 컨셉(§10-1)이므로 상체가 착석에 묶이면 안 된다.

**전제** (전부 기존 코드에서 검증된 사실):
1. **탑승 중 라이더의 로코모션 SM은 Idle에 고정된다.** `MountRider()`가 `SetMovementMode(MOVE_None)`을 걸고(§5.2, Phase B 구현 완료), `ACharacter::GetVelocity()`는 CMC Velocity를 반환하므로 탑승 중 `Speed = 0` → 어떤 클래스 ABP든 Walk/Jump 상태로 새지 않는다. 착석 레이어의 베이스가 항상 Idle이라는 보장.
2. **라이더 몸통은 탑승 중에도 마우스를 따라 회전한다.** `bUseControllerRotationYaw = true`(`DRCharacter.cpp:101`)는 attach 상태에서도 액터 회전을 갱신한다 — 포탑 조준의 시각적 근거. 착석 애니는 이 yaw 회전과 독립(인플레이스 루프).
3. **로컬 플레이어는 자기 3인칭을 못 본다**(§13.0-1). 이 작업의 수혜자는 타 플레이어 시점·대기실·사망캠이며, 검증도 상대 클라이언트 화면에서 한다. 본인 1인칭(FP)은 변화 없음.
4. **게임플레이 판정 없음.** 순수 연출 — AnimNotify에 판정을 두지 않는 원칙(§13.0-3) 그대로.
5. **신호는 이미 복제되어 있다.** `ADRCharacter::MountedOn`(RepNotify, `DOREPLIFETIME`)이 Phase B에서 구현 완료 — 서버/소유 클라/시뮬 프록시 모두에서 매 프레임 폴링 가능. **신규 C++ 작업 0.**

### 14.1 신호 소스 선택: `MountedOn` 폴링 (State.Riding 태그 아님)

| 후보 | 채택 | 근거 |
|---|---|---|
| `MountedOn != nullptr` 폴링 | **O** | 이미 전 머신에 복제된 프로퍼티. `bIsStunned`/`RiderOnTop` 폴링과 동일 패턴(§13.3). RepNotify 도착 순서 의존 없음 |
| `State.Riding` 태그 조회 | X | ASC가 PlayerState에 있어 ABP에서 조회 체인이 김(Pawn→PlayerState→ASC). 태그는 GA 차단용(§10-1) 용도로 유지 |
| 신규 복제 bool 추가 | X | `MountedOn`과 중복 — 상태 이원화만 발생 |

- ABP에서 읽는 법: `TryGetPawnOwner → Cast to ADRCharacter`(각 ABP의 기존 오너 캐싱 관례) → `MountedOn` 프로퍼티(`BlueprintReadOnly`) `IsValid` 검사. `IsMounted()`(BlueprintCallable const → BP에서 pure 노드)를 써도 동일.

### 14.2 합성 방식 — 최종단 Layered Blend Per Bone (핵심 설계)

각 라이더 ABP의 **Output Pose 직전(최종단)**에 삽입:

```
[기존 파이프라인: Locomotion SM → Slot 'DefaultSlot' → AimOffset → ...]
  → [Layered Blend Per Bone]
       Base Pose   = 기존 파이프라인 결과
       Blend Pose 0 = Sit_Loop (Sequence Player, 루프)
       Branch Filters = 좌/우 다리 체인 루트 본 2개 (Blend Depth 0)
       Blend Weights 0 = SitBlendWeight (0↔1 보간 변수)
       Mesh Space Rotation Blend = true
  → [Output Pose]
```

**설계 결정의 이유**:
- **왜 최종단(Slot·AimOffset 뒤)인가**: 어빌리티 몽타주는 DefaultSlot에서 **풀바디**로 재생된다(§13.2 — 슬롯 1개 원칙). 착석 레이어가 슬롯보다 뒤에 있어야 몽타주가 재생돼도 **하체는 착석을 유지**하고 상체만 몽타주를 따른다. §13.4.1에서 Boarding 레이어를 Slot 뒤에 둔 것과 같은 논리.
- **왜 브랜치가 thigh(양쪽 다리 체인)이고 pelvis가 아닌가**: pelvis는 spine의 부모라서 pelvis를 브랜치로 잡으면 전신이 착석 포즈로 덮여 상체 유지 요구가 깨진다. 좌/우 대퇴 본 2개(Blend Depth 0 = 해당 본+자식 전부)만 교체하면 다리만 접힌다.
- **왜 상체 전용 슬롯 방식이 아닌가**: "풀바디 착석 상태 + UpperBody 슬롯" 구조로 바꾸면 기존 모든 GA 몽타주의 슬롯 재지정이 필요 — 기존 두 클래스의 완성된 몽타주 파이프라인(§13.0-5)을 건드리게 됨. 기각 (결정 B1).
- **블렌드 보간**: `SitBlendWeight`를 이벤트그래프에서 `FInterpTo(현재, 목표, DeltaTimeX, 8.0)`로 갱신 — 탑승/하차 순간 스냅 방지 (§13.4.1 Boarding 가중치와 동일 수치).
- **IK 미사용**: 발-시트 접지는 IK 대신 `RideAttachPoint` 위치 튜닝으로 해결(코드 기본 Z=90, `BP_RobotVacuumCharacter`에서 조정). 청소기 윗면은 평평한 단일 시트라 정적 오프셋으로 충분.

### 14.3 신규 애님 에셋 (클래스별)

| 라이더 클래스 | 시퀀스 | 적용 | 비고 |
|---|---|---|---|
| Gardener | `GR_Sit_Loop` | **O** | 다리 체인 존재 전제. 인플레이스, 루프, 루트모션 off |
| VendingMachine | `VM_Sit_Loop` | **O (다리 유무 확인 후)** | 스켈레톤에 다리 체인이 없으면 미적용(B3 유사 처리) — 자판기 형태 특성상 "기울어 얹힘" 어디티브로 대체 검토 |
| RobotVacuum (청소기가 라이더인 경우 — 탑쌓기) | 없음 | **X (미적용)** | 바퀴 로봇이라 착석 개념 없음. `MOVE_None → Speed 0 → Idle` 루프가 자연스러운 "정지 탑재" 표현(결정 B3). 필요 시 서스펜션 눌림 어디티브로 확장 |

- **임포트 설정**: `EnableRootMotion = false`, Loop 활성, **Additive 아님(Override)** — 다리 본을 통째로 교체하는 방식이므로.
- `Sit_Start` / `Sit_End` 전이 시퀀스는 1차 미사용 — FInterpTo(8.0) 블렌드가 전이를 담당 (결정 B2).

### 14.4 ABP별 수정 절차 (공통 패턴)

대상: `ABP_Gardener`, `ABP_VendingMachine` (`Content/Blueprints/Character/PlayerCharacter/<클래스>/`). `ABP_VacuumCleaner`는 착석 미적용(14.3)이므로 수정 없음. (`ABP_DRCharacter`(기본 캐릭터)를 실제 플레이에 쓰고 있다면 동일 절차 적용.)

1. **Event Graph** — 변수 2개 추가:
   | 변수 | 타입 | 계산식 |
   |---|---|---|
   | `bIsRiding` | bool | `IsValid(OwnerCharacter.MountedOn)` (오너 캐스팅 캐시는 각 ABP 기존 관례 재사용) |
   | `SitBlendWeight` | float | `FInterpTo(SitBlendWeight, bIsRiding ? 1.0 : 0.0, DeltaTimeX, 8.0)` |
2. **AnimGraph** — Output Pose 직전에 §14.2 구성의 Layered Blend Per Bone 삽입. Blend Pose 0에 해당 클래스 `*_Sit_Loop` Sequence Player 연결.
3. 컴파일 → 각 클래스로 PIE 검증(14.7).

**혼동 주의** — 청소기 ABP의 두 플래그는 서로 다른 것:
- `bIsBoarded`(§13.3) = `RiderOnTop != nullptr` — **내 위에 누가 탐** → 머리 눌림 레이어 (마운트 측)
- `bIsRiding`(§14) = `MountedOn != nullptr` — **내가 남 위에 탐** → 착석 레이어 (라이더 측, 청소기는 미적용)

### 14.5 상호작용 매트릭스 (몽타주/스턴/사망/하차)

| 상황 | 동작 | 근거/처리 |
|---|---|---|
| 탑승 중 공격/스킬 몽타주 | 상체만 몽타주, 하체 착석 유지 | 착석 레이어가 Slot 뒤(§14.2) — 추가 처리 불필요 |
| 탑승 중 HitReact | 상체 반동만, 하체 착석 유지 | 동일 |
| 탑승 중 스턴 | 상체 스턴 표현, 하체 착석 유지 | 라이더는 청소기 위에 그대로 있으므로 착석 유지가 자연스러움 (결정 B4). 전신 스턴이 필요하면 `bIsStunned`일 때 SitBlendWeight 목표 0 조건 1개 추가 |
| 탑승 중 사망 | 하차 → 착석 해제 → 사망 몽타주 | `MulticastHandleDeath`가 사망 처리 **전에** `DismountRider(false)` 호출(Phase B 구현) → `MountedOn` null 복제 → bIsRiding false. 복제가 1~2프레임 늦어도 사망 몽타주가 상체를 덮고 하체는 블렌드아웃 — 허용 오차 |
| 점프키 하차 | 착석 해제 + 점프/낙하 전이 자연 발생 | 서버 `MOVE_Falling` + Launch(§5.3) → `bIsFalling` true → 각 ABP 기존 점프 상태 진입. SitBlendWeight 0 보간과 짧게 겹치지만 블렌드가 흡수 |
| 탑승 중 넉백 피격 | 착석 유지 (움직이지 않음) | 라이더는 MOVE_None이라 LaunchCharacter 무효(§5.5) — 데미지 공유만 적용 |
| 탑쌓기 위층 청소기 | 착석 애니 없음 (Idle) | 14.3 결정 B3 |
| 대기실/사망캠 | 정상 표시 | 대기실엔 탑승 없음. 사망캠은 위 사망 행과 동일 흐름 |

### 14.6 리플리케이션/성능 근거

- `MountedOn`은 Phase B에서 `DOREPLIFETIME` 등록 완료 — 서버·소유 클라·시뮬 프록시 전부에서 값이 보인다. ABP는 **매 프레임 값 폴링**만 하므로 RepNotify 도착 순서/유실에 의존하지 않는다 (§13.4.2 DJ_Loop 탈출 조건과 같은 레이스 회피 원칙).
- 폴링 비용 = 포인터 유효성 검사 1회/프레임 — 무시 가능. Thread Safe Update Animation으로 이전할 경우 Property Access로 `MountedOn`을 직접 읽으면 됨.
- 리슨 서버 호스트: 서버에서 `MountRider()`가 직접 값을 세팅하므로 호스트 화면도 같은 프레임에 반영.

### 14.7 에디터 작업 체크리스트 (순서대로)

1. **스켈레톤 확인**: Gardener/VendingMachine 스켈레톤 트리에서 **좌/우 다리 체인 루트 본 이름** 확인·기록 (§13.8-1의 머리 체인 확인과 동일 요령). VendingMachine에 다리 체인이 없으면 14.3의 대체 방침 결정
2. 착석 시퀀스 임포트: `GR_Sit_Loop`, `VM_Sit_Loop` — 루트모션 off, 루프 on, Override
3. `ABP_Gardener` 수정 (14.4 절차) → 컴파일
4. `ABP_VendingMachine` 수정 → 컴파일
5. `BP_RobotVacuumCharacter`의 `RideAttachPoint` 위치 튜닝 — 착석한 라이더의 발/엉덩이가 청소기 윗면에 맞도록 (Z 기본 90에서 조정)
6. (선택) 클래스별 앉은키 차이가 크면: `MountRider()`에서 라이더 클래스별 attach 오프셋을 주는 C++ 확장 — 1차 미구현 (결정 B6)

### 14.8 PIE 2인 검증 항목 (전부 상대 클라이언트 화면에서)

- (a) F 탑승 → 하체가 ~0.2초에 걸쳐 착석으로 블렌드 인, 상체는 Idle 유지
- (b) 청소기 주행 중 라이더 착석 유지 + 라이더 마우스 회전 시 몸통(yaw)이 따라 도는지 (포탑 조준)
- (c) 탑승 중 공격 → 상체만 몽타주 재생, 하체 착석 유지
- (d) 탑승 중 상하 에임 → 상체 에임오프셋 정상 (착석 레이어와 간섭 없음)
- (e) 점프키 하차 → 착석 해제 + 점프/착지 전이 자연스러움
- (f) 청소기 탑쌓기 → 위층 청소기는 착석 없이 Idle
- (g) 탑승 중 피격 HitReact 상체 반동 / 탑승 중 사망 → 하차 후 사망 몽타주
- (h) 리슨 서버 호스트 시점과 원격 클라 시점의 착석 상태 동일성

### 14.9 설계 결정사항 (구현 전 확인 권장)

| # | 항목 | 제안 기본값 | 근거 |
|---|---|---|---|
| B1 | 합성 방식 | **최종단 Layered Blend Per Bone (양쪽 thigh 브랜치)** | 풀바디 착석 상태 + 상체 슬롯 방식은 기존 전 GA 몽타주의 슬롯 재작업 유발(§13.2 DefaultSlot 단일 원칙과 충돌) |
| B2 | Sit_Start/End 전이 애니 | **없음 — FInterpTo(8.0) 블렌드** | 탑승/하차가 즉각적 동작이라 0.2초 블렌드로 충분. 어색하면 시퀀스 추가 |
| B3 | RobotVacuum 라이더(탑쌓기) 착석 | **미적용 (Idle 유지)** | 바퀴 로봇에 착석 개념 없음. 필요 시 서스펜션 눌림 어디티브 |
| B4 | 스턴 중 착석 | **유지** | 물리적으로 청소기 위에 있으므로. 전신 스턴 연출 필요 시 가중치 목표 0 조건 1줄 |
| B5 | 골반(pelvis) 포함 여부 | **미포함 (thigh 체인만)** | pelvis는 spine의 부모 — 포함 시 상체까지 덮임. 앉음새가 어색하면 어디티브 pelvis lean 보조 |
| B6 | 클래스별 시트 오프셋 | **단일 RideAttachPoint (1차)** | 클래스 간 체형 차가 크면 MountRider에서 클래스별 오프셋 확장 |
| B7 | FP(1인칭) 처리 | **변화 없음** | 본인 시점에선 자기 몸이 안 보임(§13.0-1). 몰입 연출 원하면 탑승 중 FP 카메라 높이 보정만 별도 검토 |

---

## 15. 플레이 테스트 이슈 트러블슈팅 ★ (2026-07-14 PIE 2인 테스트에서 발견)

> 발견된 문제 6건의 원인 분석과 해결책. **§15.2 / §15.4 / §15.6의 C++ 수정은 이미 적용 완료** — 에디터 작업(GE/BP/ABP)만 남은 항목에는 ☐ 표시. 각 항목 끝의 "검증"대로 재테스트할 것.

### 15.0 전제 — 애니메이션 "안 보임" 판정 전에 반드시 확인할 것

**본인 화면에서는 본인 3인칭 애니메이션이 원래 안 보인다.** `GetMesh()->SetOwnerNoSee(true)`(`DRCharacter.cpp:73`, §13.0-1) 때문에 시전자 화면에는 자기 메시 자체가 렌더되지 않는다. 따라서:
- 호스트가 돌진하며 자기 화면을 보는 것 → 아무 애니도 안 보이는 게 **정상**
- 클라가 더블점프하며 자기 화면을 보는 것 → 안 보이는 게 **정상**
- **올바른 관찰 방법**: A가 스킬 사용 → **B의 화면에서 A의 캐릭터**를 관찰 (§13.8 검증 항목 전부 이 방식). 또는 임시로 `SetOwnerNoSee(false)`로 끄고 셀프 뷰 확인 후 되돌리기.

§15.3(크러시)·§15.5(DJ/대쉬 시작) 증상 중 일부는 이 관찰 방법 문제일 가능성이 있으므로, 아래 수정 후 반드시 상대 화면 기준으로 재검증한다.

### 15.1 이슈 요약표

| # | 증상 | 근본 원인 | 상태 |
|---|---|---|---|
| 1 | 지속 돌진이 클라 캐릭터만 제자리 | `Tick`의 전진 입력이 `HasAuthority()` 조건 — 원격 클라 폰은 CMC ServerMove가 서버 입력을 덮음 | ✅ C++ 수정 완료 (§15.2) |
| 2 | DJ AoE가 아군 플레이어에게 데미지 | `BP_DRVacuumCleaner`에 액터 태그 `Player` 누락 → `IsNotFriend`가 아군 판정 실패 | ✅ C++ 수정 완료 + ☐ BP 확인 (§15.4) |
| 3 | Q 한 번 눌렀는데 2회 발동 | `AbilityInputTagHeld`가 홀드 중 매 프레임 `TryActivateAbility` — 즉발 스킬이 다음 프레임 재발동 | ✅ 태그 추가 완료 + ☐ GE 생성/지정 (§15.6) |
| 4 | Dash_Start 애니 양쪽 다 안 보임 | ABP SM 배선 문제(실측: 미연결 전이 경고가 에셋에 저장돼 있음) + 관찰 방법(§15.0) 가능성 | ☐ ABP 수정 (§15.5) |
| 5 | DJ 애니 전혀 안 보임 | 동일 — ABP 전이 조건/알리아스 배선 문제 유력 | ☐ ABP 수정 (§15.5) |
| 6 | 돌진 충돌(Crush) 몽타주가 클라 화면에서만 안 보임 | GAS 몽타주 복제 경로 문제 또는 관찰 방법. 진단 후 필요 시 Multicast 재생으로 전환 | ☐ 진단 → 필요 시 C++ (§15.3) |

### 15.2 [수정 완료] 지속 돌진 — 클라이언트 캐릭터가 제자리에 서 있음

**원인**: `ADRRobotVacuumCharacter::Tick`이 `if (HasAuthority() && bSustainedDash ...)` 조건으로 `AddMovementInput`을 주입했다. CMC(CharacterMovementComponent)의 클라이언트 예측 구조에서 **플레이어가 조종하는 폰의 이동 입력 원천은 소유 클라이언트**다: 클라가 매 프레임 자기 입력을 `ServerMove` RPC로 보내고, 서버는 그것을 재생/검증한다. 서버에서 원격 클라 폰에 `AddMovementInput`을 해봤자 다음 `ServerMove`가 도착하는 순간 "클라 입력에는 전진이 없었다"로 덮여버려 실제로 움직이지 않는다. 리슨 서버 호스트 폰은 서버=소유 클라라서 정상 동작했던 것 — 정확히 관찰된 증상과 일치.

**수정 (적용됨, `DRRobotVacuumCharacter.cpp` Tick)**:
```cpp
// 변경 전: if (HasAuthority() && bSustainedDash && !bDead)
// 변경 후:
if (bSustainedDash && !bDead && IsLocallyControlled())
{
    AddMovementInput(GetActorForwardVector(), 1.f);
}
```
- `IsLocallyControlled()` = 리슨 호스트 본인 + 원격 소유 클라 양쪽 커버, 시뮬 프록시 제외.
- `bSustainedDash`는 복제 프로퍼티(§4.2)라 소유 클라에도 도착한다. 해소 직후 ~1 RTT 지연은 체감 무시 가능.
- 충돌 판정(`OnCapsuleHit`)·데미지·게이지는 여전히 전부 서버 전용 — 치트 표면 증가 없음.

**검증**: 클라이언트로 5칸 만충 → 해소 → 자동 전진 + 마우스 조향 확인. 호스트 화면에서도 클라 캐릭터가 전진하는지 확인(CMC 이동 복제).

### 15.3 돌진 충돌(Crush) 몽타주 — 클라이언트 화면에서만 안 보임

**전제 확인**: "클라이언트에서 안 보인다"가 **시전한 클라 본인 화면**이라면 §15.0에 의해 정상이다. 문제가 되는 케이스는 "**호스트가 충돌했는데 클라 화면에서 호스트 캐릭터의 Crush가 안 나옴**" (시뮬레이티드 프록시 방향 복제 실패)뿐이다.

**구조 리마인드**: `FinishDash()`는 서버에서만 실행되고(`HandleDashImpact`/`OnBrakePressed` 모두 `HasAuthority()` 가드), 몽타주는 서버 GA 인스턴스의 `PlayMontageAndWait`로 재생된다. 이때 ASC의 `ReplicatedAnimMontage` 경로로 시뮬 프록시에 자동 전파되는 것이 §13.5의 설계 전제였다. 전파 전제 조건: ① 서버 ASC에서 `PlayMontage` 호출(✓), ② 각 클라에서 해당 플레이어의 `InitAbilityActorInfo` 완료 — ActorInfo가 아바타 메시/AnimInstance를 알아야 함(`OnRep_PlayerState`에서 호출, `DRCharacter.cpp:250-257` — 시뮬 프록시 포함 전 클라에서 실행됨 ✓), ③ ABP에 Slot 노드 존재(✓ — 서버 화면에서는 보이므로).

**진단 절차** (§15.0 확인 후에도 재현되면):
1. 서버 로그에서 `[VacuumDash] 돌진 종료 — 사유: 충돌, 몽타주: AM_VC_Dash_Crush` 확인 (기존 추가 로그).
2. 같은 상황에서 클라 화면에 `HitReact` 몽타주는 보이는지 비교 — **HitReact가 보이면** 복제 인프라는 정상이고 Crush 경로만 문제, **HitReact도 안 보이면** ②의 초기화 타이밍 문제(공통 인프라).
3. 유력 후보: 몽타주 재생 직후 같은 프레임의 `EndAbility` 조합(fire-and-forget + 즉시 종료)은 "재생 후 대기"하는 HitReact류 GA와 다른 유일한 구조적 차이점.

**해결책 (진단 결과와 무관하게 확실한 방법)** ☐: GAS 몽타주 복제 의존을 버리고 **캐릭터 Multicast RPC로 전환** — 프로젝트에 이미 같은 패턴의 선례가 있다(`MulticastPlayDashImpactSound`, `MulticastHandleDeath`).

```cpp
// DRRobotVacuumCharacter.h (public)
UFUNCTION(NetMulticast, Unreliable)
void MulticastPlayFinishMontage(UAnimMontage* Montage);

// DRRobotVacuumCharacter.cpp
void ADRRobotVacuumCharacter::MulticastPlayFinishMontage_Implementation(UAnimMontage* Montage)
{
    if (GetNetMode() == NM_DedicatedServer || !Montage) return;
    if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
    {
        AnimInstance->Montage_Play(Montage);
    }
}

// DRVacuumDash.cpp FinishDash() — PlayMontageAndWait 블록을 통째로 교체:
if (FinishMontage && Vacuum)
{
    Vacuum->MulticastPlayFinishMontage(FinishMontage);
}
```
- 전 머신(호스트+전 클라)에서 재생 — 시전 클라 본인도 재생되지만 OwnerNoSee라 화면에 안 보일 뿐 무해.
- Unreliable로 충분(1회성 코스메틱). Montage 인자는 GA BP에 지정된 에셋 레퍼런스라 전 클라에 로드돼 있음.
- 같은 증상이 `AM_VC_EnhancedAttack`(AirShot 3단계)에서도 확인되면 동일 헬퍼로 교체(§13.5-③의 "GAS 복제로 충분" 전제를 §15에서 정정).

**검증**: 호스트 돌진-충돌 → 클라 화면에서 호스트 캐릭터 Crush 재생 / 클라 돌진-충돌 → 호스트 화면에서 재생 / 3인 테스트 시 제3자 화면에서도 재생.

### 15.4 [수정 완료] 더블 점프 AoE — 아군 플레이어 오폭

**원인**: 아군 판정 `UDRAbilitySystemLibrary::IsNotFriend()`(`DRAbilitySystemLibrary.cpp:415-422`)는 **두 액터가 모두 액터 태그 `"Player"`를 갖고 있으면 아군**으로 판정한다. 이 태그는 C++이 아니라 **각 캐릭터 BP의 디테일 → Actor → Tags에 수동 입력**돼 있었다(uasset 실측: `BP_GardenRobot`에는 `Player` 태그 있음, **`BP_DRVacuumCleaner`에는 없음**). 청소기에 태그가 없으니 `IsNotFriend(청소기, 다른 플레이어) == true` → JetJump AoE의 아군 필터를 통과해 아군이 맞았다.

**파급 범위 (같은 원인으로 잘못 동작하던 것들 — 태그 수정으로 일괄 해결)**:
- 돌진 충돌 데미지의 적/아군 구분(`HandleDashImpact`의 `bEnemy`): 아군 플레이어가 G×5가 아닌 **G×10(적 배율)**을 맞고 있었음
- 적 처치 시 물 보상의 플레이어 판정(`DREnemyAttributeSet.cpp:97`의 `bSourceIsPlayer`): 청소기가 적을 잡으면 **물 보상이 지급되지 않았을 가능성** — 수정 후 확인
- 적 AI/투사체 등 `IsNotFriend`·`"Player"` 태그를 쓰는 모든 경로

**수정 (적용됨, `DRCharacter.cpp` 생성자)**: BP 수동 태그에 의존하지 않도록 플레이어 베이스 클래스가 태그를 보장한다.
```cpp
ADRCharacter::ADRCharacter()
{
    Tags.AddUnique(FName("Player"));   // IsNotFriend 아군 판정용 — BP 누락 사고 원천 차단
    ...
}
```
- 기존 BP(`BP_GardenRobot` 등)에 이미 있는 `Player`와 중복돼도 `ActorHasTag` 판정에 무해.
- ☐ **주의 1**: BP가 Tags 배열을 에디터에서 한 번이라도 수정했다면 BP 저장값이 C++ 기본값을 통째로 덮는다. 컴파일 후 `BP_DRVacuumCleaner` 디테일에서 Tags에 `Player`가 보이는지 1회 확인하고, 안 보이면 BP에 수동 추가.
- ☐ **주의 2**: 반대로 태그 배열에 오타 값(예: `player` 소문자)이 있으면 정리.

**검증**: PIE 2인 → 아군 옆에서 Q — 로그 확인: `[VacuumJetJump] AoE 판정 종료 — 후보 N / 적중 0` (아군만 있을 때). 적 스폰 후에는 적만 적중 로그에 나와야 함. 돌진으로 아군 충돌 시 `[VacuumDash] 충돌 — ... (플레이어), 데미지 G×5` 확인.

### 15.5 Dash_Start·더블점프 애니메이션 미재생 — ABP 배선 점검

**실측 근거** (`ABP_VacuumCleaner.uasset` 파싱, 07-14 17:45 저장본):
1. 상태들은 존재한다: `Idle / Walk / Jump / FallLoop / Land / Stun / DJ_Start / DJ_Loop / DJ_Land / Dash_Charge / Dash_Start / Dash_Loop` + 알리아스 `ToStun / ToFalling / ToDJ / ToLand / ToDJLand`. (점프 계열이 §13.4.3의 `Jump_Start/Loop/Land` 대신 Gardener식 `Jump/FallLoop/Land` 이름 — 동작에는 무관)
2. **스테이트머신이 2개다**: `AnimGraphNode_StateMachine_0 "Main States"` + `AnimGraphNode_StateMachine_1 "Locomotion"`, 그리고 **Disabled 노드 흔적**(`ENodeEnabledState::Disabled`)이 저장돼 있다 — ABP_Gardener를 복제해 시작한 흔적. **새 상태(DJ_*/Dash_*)를 Output에 연결 안 된 쪽 SM에 만들었을 가능성이 가장 유력한 원인.**
3. **미연결 전이 경고가 에셋에 저장돼 있다**: "`ToStun to Stun will never be taken, please connect something to Can Enter Transition`" — 최소 1개 전이가 조건 미연결 상태로 저장됐다. 같은 실수가 DJ/Dash 전이에도 있을 수 있다(§13.4.3-(4)에서 예고한 바로 그 실수).

**수정 체크리스트** ☐ (순서대로):
1. **AnimGraph 정리**: Output Pose에서 역추적해 실제 연결된 SM이 어느 것인지 확인 → **연결 안 된 SM은 삭제**, Disabled 노드 전부 삭제. 남은 SM 하나에 모든 상태가 있어야 한다. 없으면 §13.4.3-(3) 표대로 이설.
2. **컴파일 경고 0**: "`... will never be taken`" 경고가 하나라도 있으면 해당 전이 그래프에서 조건을 Result 핀에 연결. Automatic Rule 전이(Jump→FallLoop류, DJ_Start→DJ_Loop, Dash_Start→Dash_Loop, *_Land→Idle)는 전이 디테일 "Automatic Rule Based on Sequence Player in State" 체크로 대체(조건 연결 불필요).
3. **전이 조건 실제 값 대조** (§13.4.3-(3) 표 기준, ABP 실제 변수명 사용):
   - `ToDJ` alias → `DJ_Start`: `bIsJetJumping AND bIsInAir`, **Priority가 일반 점프(ToFalling)보다 앞**(숫자 작게). alias 커버 상태에 `Jump/FallLoop/Land/Dash_Charge` 포함 확인 — 빠지면 공중 Q에서 DJ 재생 안 됨.
   - `Dash_Charge → Dash_Start`: `bSustainedDash`. `Dash_Charge → Idle/Walk` 탈출 조건은 `NOT bIsDashCharging AND NOT bSustainedDash` — **`NOT bSustainedDash`가 빠져 있으면 만충 해소 프레임에 Idle로 새서 Dash_Start를 건너뛴다** (Dash_Start 미재생의 전형적 원인).
   - `Idle/Walk → Dash_Charge`: `bIsDashCharging AND NOT bIsInAir` — 이 전이가 없으면 SM이 Dash_Charge 상태에 있지 않아 Dash_Start 진입 경로 자체가 없다.
4. **상태 내부 시퀀스 확인**: `DJ_Start=VC_DoubleJump_Start(루프☐)`, `DJ_Loop=VC_DoubleJump_Loop(루프✔)`, `DJ_Land=VC_DoubleJump_Land(루프☐)`, `Dash_Start=VC_Dash_Start(루프☐)`, `Dash_Loop=VC_Dash_Loop(루프✔)`. 시퀀스 미지정(빈 상태)이면 스킵/T포즈처럼 보인다.
5. **플래그 값 자체 검증** (ABP 수정 전 5분 진단): PIE에서 콘솔 `ShowDebug Animation` — 현재 SM 상태와 ABP 변수 값이 화면에 표시된다. Q 사용 순간 `bIsJetJumping`이 true로 바뀌는지, 만충 해소 순간 `bSustainedDash`가 true인지 확인. **true인데 상태가 안 바뀌면 100% 전이 배선 문제**, false면 C++/복제 문제(그 경우만 코드 재조사).

**검증**: §15.0 방식(상대 화면)으로 — 지상 Q: DJ 3단 재생 / 공중 Q: 낙하 중에도 DJ 재생 / 만충 해소: Dash_Charge 클라이맥스 → **Dash_Start 1회 재생** → Dash_Loop 루프 → 충돌 시 Crush 몽타주가 Loop를 덮음.

### 15.6 [태그 추가 완료] Q 홀드 시 더블 점프 2연발

**원인**: 입력 시스템이 홀드 중 **매 프레임** 활성화를 시도한다 — `UDRAbilitySystemComponent::AbilityInputTagHeld()`(`DRAbilitySystemComponent.cpp:88-103`)가 `if (!AbilitySpec->IsActive()) TryActivateAbility(...)`. JetJump는 즉발(같은 프레임에 EndAbility)이라 다음 프레임엔 이미 `IsActive()==false` → 재활성화. 첫 발동으로 공중에 뜬 상태라 두 번째 활성화도 통과(`CanActivateAbility`: 공중 + `bAirJumpUsed==false`) → 공중 점프까지 즉시 소모. "한 번 눌렀는데 2번"의 정체는 **지상 점프 + 다음 프레임 공중 점프**다.

**해결책: 쿨다운 GE (GAS 표준 경로 — 추가 C++ 코드 0)**. `CommitAbility()`가 이미 있으므로(`DRVacuumJetJump.cpp:45`) Cooldown GE만 지정하면 커밋 시 자동 적용되고, 쿨다운 태그가 살아있는 동안 `CanActivateAbility`가 자동 차단한다.

- ✅ 네이티브 태그 추가 완료: `Cooldown.RobotVacuum.JetJump` (`DRGameplayTags.h/.cpp` — `Cooldown_Fire_FireBolt` 관례)
- ☐ **GE 생성**: `GE_Cooldown_VacuumJetJump` (위치: `…/VacuumCleaner/Abilities/Skill1_JetJump/`)
  - Duration Policy = **Has Duration**, Duration Magnitude = **0.3**
  - Components → Target Tags Gameplay Effect Component(구 Granted Tags) → Add Tags: **`Cooldown.RobotVacuum.JetJump`**
  - Modifier 없음 (태그 부여가 전부)
- ☐ **GA 지정**: `GA_VacuumJetJump` → Class Defaults → Costs → **Cooldown Gameplay Effect Class = `GE_Cooldown_VacuumJetJump`**
- 수치 근거: 0.3초는 홀드 스팸(매 프레임)을 확실히 끊으면서, 의도된 콤보 "지상 Q → 정점 부근 공중 Q"(점프 정점까지 보통 0.4초+)를 방해하지 않는다. 콤보가 답답하면 0.2까지 낮춰 조정.
- 참고(차선책, 미채택): `AbilityInputTagHeld`에서 즉발형 어빌리티를 Pressed 전용으로 분리하는 입력 구조 개편 — LMB 충전형과 정책 분기가 필요해 수정 범위가 커서 쿨다운 방식 우선.

**검증**: Q 꾹 누르기 → 로그에 `[VacuumJetJump] 발동` 1회만 + 물 30만 차감. 지상 Q → 정점에서 Q 콤보(2회째)는 여전히 되는지 확인 (0.3초 경과 후 공중 1회는 스펙).

### 15.7 수정 후 통합 재검증 순서

1. **C++ 빌드** (에디터 종료 후 Build.bat, 또는 에디터에서 Live Coding Ctrl+Alt+F11) — §15.2/15.4/15.6 반영
2. **에디터 작업**: `GE_Cooldown_VacuumJetJump` 생성·지정(§15.6) → `BP_DRVacuumCleaner` Tags에 `Player` 확인(§15.4) → ABP SM 정리·전이 수정(§15.5)
3. **PIE 2인**(리슨 서버 + 클라), 관찰은 항상 **상대 화면**(§15.0):
   - (a) 클라 지속 돌진: 전진 + 마우스 조향 (§15.2)
   - (b) 아군 옆 Q: `적중 0` 로그 (§15.4)
   - (c) Q 홀드: 1회만 발동 (§15.6)
   - (d) DJ 3단 애니 (§15.5)
   - (e) 만충 해소: Dash_Charge → Dash_Start → Dash_Loop (§15.5)
   - (f) 충돌 Crush가 상대 화면에서 재생 (§15.3 — 안 되면 Multicast 전환 적용)
