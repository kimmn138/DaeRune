# Plan2: Phase3 스폰 포인트 VFX 및 수원지 VFX 구현 계획

## 목차
1. [개요](#1-개요)
2. [일반 적 스폰 포인트 VFX](#2-일반-적-스폰-포인트-vfx)
3. [엘리트 보스 스폰 포인트 VFX](#3-엘리트-보스-스폰-포인트-vfx)
4. [수원지(WaterSource) VFX](#4-수원지watersource-vfx)
5. [파일 수정 목록](#5-파일-수정-목록)
6. [구현 순서](#6-구현-순서)

---

## 1. 개요

### 구현 목표
| 구분 | 대상 액터 | VFX 동작 | 제어 주체 |
|------|-----------|----------|-----------|
| A | 일반 적 스폰 포인트 (4개, 태그: `Phase3EnemySpawnPoint`) | Phase3 시작 시 활성화 → Phase3 종료 시 비활성화 | `UDRPhase3` |
| B | 엘리트 보스 스폰 포인트 (태그: `Phase3SpawnPoint`) | 엘리트 웨이브 시작 시 활성화 → 엘리트 스폰 완료 후 비활성화 | `UDRPhase3` |
| C | 수원지 (`ADRWaterSource`) | 사용 가능 시 이펙트 2개 ON → 사용 후 리차지 중 OFF → 리차지 완료 시 다시 ON | `ADRWaterSource` 자체 |

### 설계 원칙
- **서버 권한 모델**: VFX 상태 변경은 서버에서 결정, 클라이언트에서 시각적으로 표현
- **기존 패턴 준수**: 프로젝트에서 사용 중인 `UNiagaraComponent` + `bAutoActivate = false` + `Activate()`/`Deactivate()` 패턴 사용
- **Blueprint 설정 가능**: 나이아가라 에셋은 `EditDefaultsOnly`로 Blueprint에서 할당
- **멀티플레이어 동기화**: Multicast RPC 또는 RepNotify를 통해 모든 클라이언트에서 VFX 동기화

---

## 2. 일반 적 스폰 포인트 VFX

### 2.1 현재 구조 분석

**일반 적 스폰 포인트**:
- 레벨에 배치된 일반 `AActor`로, `Phase3EnemySpawnPoint` 태그를 가짐
- `UDRPhase3::FindEnemySpawnPoints()`에서 `TActorIterator`로 탐색하여 `TArray<TObjectPtr<AActor>> EnemySpawnPoints`에 저장 (정확히 4개)
- 현재 이 액터들은 위치 정보만 제공하며, VFX 컴포넌트는 없음

### 2.2 구현 방안: Multicast RPC를 통한 SpawnSystemAtLocation

스폰 포인트 액터가 일반 `AActor`이므로, `UDRPhase3`에서 Multicast RPC를 호출하여 모든 클라이언트에서 나이아가라를 스폰/제거하는 방식을 사용합니다.

> **왜 Multicast RPC인가?**
> - 스폰 포인트 액터는 단순 `AActor`로 `UNiagaraComponent`를 가지고 있지 않음
> - `UDRPhase3`는 `UObject`(Phase 클래스)이므로 직접 Multicast를 호출할 수 없음
> - 따라서 `ADRStageGameState`에 Multicast RPC를 추가하여 모든 클라이언트에 VFX 명령을 전달

### 2.3 상세 구현

#### Step 1: `ADRStageGameState`에 Multicast RPC 추가

**파일**: `Source/DaeRune/Public/Game/DRStageGameState.h`

```cpp
// === 추가할 헤더 ===
#include "NiagaraSystem.h"

// === 추가할 멤버 변수 ===
protected:
    // Phase3 일반 적 스폰 포인트 VFX 컴포넌트 캐시 (클라이언트 로컬)
    UPROPERTY()
    TArray<TObjectPtr<UNiagaraComponent>> EnemySpawnPointVFXComponents;

public:
    // Phase3 일반 적 스폰 포인트 VFX 활성화
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ActivateEnemySpawnPointVFX(const TArray<FVector>& SpawnPointLocations, UNiagaraSystem* NiagaraAsset);

    // Phase3 일반 적 스폰 포인트 VFX 비활성화
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_DeactivateEnemySpawnPointVFX();
```

**파일**: `Source/DaeRune/Private/Game/DRStageGameState.cpp`

```cpp
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

void ADRStageGameState::Multicast_ActivateEnemySpawnPointVFX_Implementation(
    const TArray<FVector>& SpawnPointLocations, UNiagaraSystem* NiagaraAsset)
{
    if (!NiagaraAsset) return;

    // 기존 VFX 정리
    for (UNiagaraComponent* Comp : EnemySpawnPointVFXComponents)
    {
        if (Comp && IsValid(Comp))
        {
            Comp->DeactivateImmediate();
            Comp->DestroyComponent();
        }
    }
    EnemySpawnPointVFXComponents.Empty();

    // 각 스폰 포인트 위치에 나이아가라 스폰
    for (const FVector& Location : SpawnPointLocations)
    {
        UNiagaraComponent* NewComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            NiagaraAsset,
            Location,
            FRotator::ZeroRotator,
            FVector(1.f),
            false,  // bAutoDestroy = false (Phase3 내내 유지)
            true,   // bAutoActivate = true
            ENCPoolMethod::None,
            true
        );

        if (NewComp)
        {
            EnemySpawnPointVFXComponents.Add(NewComp);
        }
    }
}

void ADRStageGameState::Multicast_DeactivateEnemySpawnPointVFX_Implementation()
{
    for (UNiagaraComponent* Comp : EnemySpawnPointVFXComponents)
    {
        if (Comp && IsValid(Comp))
        {
            Comp->DeactivateImmediate();
            Comp->DestroyComponent();
        }
    }
    EnemySpawnPointVFXComponents.Empty();
}
```

#### Step 2: `UDRPhase3`에 VFX 에셋 프로퍼티 및 호출 로직 추가

**파일**: `Source/DaeRune/Public/Phase/DRPhase3.h`

```cpp
// === 추가할 전방 선언 ===
class UNiagaraSystem;

// === 추가할 멤버 변수 (private 섹션) ===
    // 일반 적 스폰 포인트에 표시할 나이아가라 에셋
    UPROPERTY(EditDefaultsOnly, Category = "Phase3|VFX")
    TObjectPtr<UNiagaraSystem> EnemySpawnPointNiagaraSystem;
```

**파일**: `Source/DaeRune/Private/Phase/DRPhase3.cpp`

`OnPhaseStart()`의 `FindEnemySpawnPoints()` 호출 이후에 VFX 활성화 로직 추가:

```cpp
void UDRPhase3::OnPhaseStart()
{
    // ... 기존 코드 (LoadPhase3ConfigFromBalanceConfig, SetupPhaseObjective 등) ...

    if (UWorld* World = GameMode->GetWorld())
    {
        InitializeActiveSpawnPoints();
        FindEnemySpawnPoints();

        // === 추가: 일반 적 스폰 포인트 VFX 활성화 ===
        if (GameState && EnemySpawnPointNiagaraSystem)
        {
            TArray<FVector> SpawnPointLocations;
            for (const TObjectPtr<AActor>& SpawnPoint : EnemySpawnPoints)
            {
                if (SpawnPoint)
                {
                    SpawnPointLocations.Add(SpawnPoint->GetActorLocation());
                }
            }
            GameState->Multicast_ActivateEnemySpawnPointVFX(
                SpawnPointLocations, EnemySpawnPointNiagaraSystem);
        }
        // === 추가 끝 ===

        DefenseStartTime = World->GetTimeSeconds();
        // ... 나머지 기존 코드 ...
    }

    // ... 나머지 기존 코드 ...
}
```

`OnPhaseEnd()`에 VFX 비활성화 로직 추가 (타이머 정리 전):

```cpp
void UDRPhase3::OnPhaseEnd()
{
    Super::OnPhaseEnd();

    // === 추가: 일반 적 스폰 포인트 VFX 비활성화 ===
    if (GameState)
    {
        GameState->Multicast_DeactivateEnemySpawnPointVFX();
    }
    // === 추가 끝 ===

    // 기존 타이머 정리 코드 ...
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(WaveTimerHandle);
        // ... 나머지 정리 코드 ...
    }
    // ...
}
```

### 2.4 동작 흐름

```
Phase3 시작
  └→ OnPhaseStart()
      └→ FindEnemySpawnPoints()  (4개 스폰 포인트 탐색)
      └→ Multicast_ActivateEnemySpawnPointVFX(위치 4개, 나이아가라 에셋)
          └→ [모든 클라이언트] SpawnSystemAtLocation() × 4
              └→ 4개 스폰 포인트에서 포탈 VFX 재생 시작

Phase3 진행 중...
  └→ 웨이브 1~5 동안 VFX 계속 표시

Phase3 종료
  └→ OnPhaseEnd()
      └→ Multicast_DeactivateEnemySpawnPointVFX()
          └→ [모든 클라이언트] DeactivateImmediate() + DestroyComponent() × 4
              └→ 모든 스폰 포인트 VFX 제거
```

### 2.5 Blueprint 설정

1. `BP_DRStageGameMode`의 Phase3 CDO에서 `EnemySpawnPointNiagaraSystem` 할당
2. 기존 `Content/Blueprints/VFX/LightningPortal/NS_Lightning.uasset` 같은 포탈 이펙트를 사용하거나, 새 나이아가라 시스템 제작

---

## 3. 엘리트 보스 스폰 포인트 VFX

### 3.1 현재 구조 분석

**엘리트 보스 스폰 포인트**:
- 레벨에 배치된 `AActor`로, `Phase3SpawnPoint` 태그를 가짐
- `StartNextWave()`에서 `Modifier.bSpawnEliteBoss == true`일 때 `TActorIterator`로 탐색
- 발견된 각 보스 스폰 포인트 위치에서 `SpawnEliteMonster(Location)` 호출
- 엘리트 사망 시 `OnEliteEnemyDeath()` → `EliteBosses` 배열에서 제거 → 전부 죽으면 `bEliteBossSpawned = false`

### 3.2 구현 방안: Multicast RPC + 타이머를 통한 VFX 제어

엘리트 VFX는 **웨이브 시작 시 ON → 엘리트 스폰 완료 후 OFF** 패턴입니다. 즉시 비활성화하면 VFX가 보이지 않을 수 있으므로, 타이머를 사용하여 일정 시간(기본 3초) 동안 VFX를 보여준 뒤 비활성화합니다.

### 3.3 상세 구현

#### Step 1: `ADRStageGameState`에 엘리트 스폰 포인트 VFX Multicast RPC 추가

**파일**: `Source/DaeRune/Public/Game/DRStageGameState.h`

```cpp
// === 추가할 멤버 변수 ===
protected:
    // 엘리트 보스 스폰 포인트 VFX 컴포넌트 캐시 (클라이언트 로컬)
    UPROPERTY()
    TArray<TObjectPtr<UNiagaraComponent>> EliteSpawnPointVFXComponents;

public:
    // 엘리트 보스 스폰 포인트 VFX 활성화
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ActivateEliteSpawnPointVFX(const TArray<FVector>& SpawnPointLocations, UNiagaraSystem* NiagaraAsset);

    // 엘리트 보스 스폰 포인트 VFX 비활성화
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_DeactivateEliteSpawnPointVFX();
```

**파일**: `Source/DaeRune/Private/Game/DRStageGameState.cpp`

```cpp
void ADRStageGameState::Multicast_ActivateEliteSpawnPointVFX_Implementation(
    const TArray<FVector>& SpawnPointLocations, UNiagaraSystem* NiagaraAsset)
{
    if (!NiagaraAsset) return;

    // 기존 VFX 정리
    for (UNiagaraComponent* Comp : EliteSpawnPointVFXComponents)
    {
        if (Comp && IsValid(Comp))
        {
            Comp->DeactivateImmediate();
            Comp->DestroyComponent();
        }
    }
    EliteSpawnPointVFXComponents.Empty();

    for (const FVector& Location : SpawnPointLocations)
    {
        UNiagaraComponent* NewComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            NiagaraAsset,
            Location,
            FRotator::ZeroRotator,
            FVector(1.f),
            false,  // bAutoDestroy = false (타이머로 수동 제거)
            true,   // bAutoActivate = true
            ENCPoolMethod::None,
            true
        );

        if (NewComp)
        {
            EliteSpawnPointVFXComponents.Add(NewComp);
        }
    }
}

void ADRStageGameState::Multicast_DeactivateEliteSpawnPointVFX_Implementation()
{
    for (UNiagaraComponent* Comp : EliteSpawnPointVFXComponents)
    {
        if (Comp && IsValid(Comp))
        {
            Comp->DeactivateImmediate();
            Comp->DestroyComponent();
        }
    }
    EliteSpawnPointVFXComponents.Empty();
}
```

#### Step 2: `UDRPhase3`에 엘리트 VFX 에셋 프로퍼티 및 호출 로직 추가

**파일**: `Source/DaeRune/Public/Phase/DRPhase3.h`

```cpp
// === 추가할 멤버 변수 (private 섹션) ===
    // 엘리트 보스 스폰 포인트에 표시할 나이아가라 에셋
    UPROPERTY(EditDefaultsOnly, Category = "Phase3|VFX")
    TObjectPtr<UNiagaraSystem> EliteSpawnPointNiagaraSystem;

    // 엘리트 스폰 포인트 VFX 표시 시간 (초)
    UPROPERTY(EditDefaultsOnly, Category = "Phase3|VFX")
    float EliteSpawnVFXDuration = 3.0f;

    // 엘리트 스폰 포인트 VFX 비활성화 타이머
    FTimerHandle EliteSpawnVFXTimerHandle;
```

**파일**: `Source/DaeRune/Private/Phase/DRPhase3.cpp`

`StartNextWave()` 내 엘리트 보스 스폰 로직 수정 (기존 for문 부분을 대체):

```cpp
if (Modifier.bSpawnEliteBoss && EliteBossClass)
{
    if (CleanserSites.Num() > 0 && CleanserSites[0])
    {
        // === 변경: 보스 스폰 포인트 위치를 먼저 수집 ===
        TArray<FVector> BossSpawnLocations;
        TArray<AActor*> BossSpawnPointActors;
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            AActor* BossSpawnPoint = *It;
            if (BossSpawnPoint && BossSpawnPoint->ActorHasTag(BossSpawnPointTag))
            {
                BossSpawnLocations.Add(BossSpawnPoint->GetActorLocation());
                BossSpawnPointActors.Add(BossSpawnPoint);
            }
        }

        // === 추가: 엘리트 스폰 포인트 VFX 활성화 ===
        if (GameState && EliteSpawnPointNiagaraSystem && BossSpawnLocations.Num() > 0)
        {
            GameState->Multicast_ActivateEliteSpawnPointVFX(
                BossSpawnLocations, EliteSpawnPointNiagaraSystem);
        }

        // 기존 스폰 로직 (수집한 위치 사용)
        for (const FVector& Location : BossSpawnLocations)
        {
            SpawnEliteMonster(Location);
        }

        // === 추가: 일정 시간 후 VFX 비활성화 ===
        if (GameState && BossSpawnLocations.Num() > 0)
        {
            if (UWorld* SpawnWorld = GetWorld())
            {
                SpawnWorld->GetTimerManager().ClearTimer(EliteSpawnVFXTimerHandle);

                TWeakObjectPtr<ADRStageGameState> WeakGameState(GameState);
                SpawnWorld->GetTimerManager().SetTimer(
                    EliteSpawnVFXTimerHandle,
                    [WeakGameState]()
                    {
                        if (ADRStageGameState* GS = WeakGameState.Get())
                        {
                            GS->Multicast_DeactivateEliteSpawnPointVFX();
                        }
                    },
                    EliteSpawnVFXDuration,
                    false
                );
            }
        }
        // === 추가 끝 ===
    }
}
```

`OnPhaseEnd()` 및 `BeginDestroy()`에 타이머 정리 추가:

```cpp
// OnPhaseEnd() 내 타이머 정리 블록에 추가:
World->GetTimerManager().ClearTimer(EliteSpawnVFXTimerHandle);

// 엘리트 VFX도 정리
if (GameState)
{
    GameState->Multicast_DeactivateEliteSpawnPointVFX();
}

// BeginDestroy() 내 타이머 정리 블록에도 추가:
World->GetTimerManager().ClearTimer(EliteSpawnVFXTimerHandle);
```

### 3.4 동작 흐름

```
웨이브 N 시작 (bSpawnEliteBoss == true)
  └→ StartNextWave()
      └→ 보스 스폰 포인트 위치 수집 (BossSpawnLocations)
      └→ Multicast_ActivateEliteSpawnPointVFX(위치들, 나이아가라 에셋)
          └→ [모든 클라이언트] 보스 스폰 포인트에 포탈 VFX 표시
      └→ SpawnEliteMonster() × N (각 보스 스폰 포인트마다)
          └→ 엘리트 보스 스폰 완료
      └→ SetTimer(EliteSpawnVFXDuration = 3.0초)
          └→ 3초 후: Multicast_DeactivateEliteSpawnPointVFX()
              └→ [모든 클라이언트] 보스 스폰 포인트 VFX 제거

Phase3 종료
  └→ OnPhaseEnd()
      └→ ClearTimer(EliteSpawnVFXTimerHandle)
      └→ Multicast_DeactivateEliteSpawnPointVFX()  (혹시 남아있을 VFX 정리)
```

### 3.5 Blueprint 설정

1. `BP_DRStageGameMode`의 Phase3 CDO에서 `EliteSpawnPointNiagaraSystem` 에셋 할당
2. `EliteSpawnVFXDuration` 값 설정 (기본 3초, 필요 시 조절)
3. 일반 스폰 포인트와 다른 시각적 이펙트 사용 권장 (더 강렬한 색상/크기)

---

## 4. 수원지(WaterSource) VFX

### 4.1 현재 구조 분석

**ADRWaterSource**:
- `ADREffectActor`를 상속, 레벨에 배치되는 독립적 액터
- `bIsAvailable` (bool, `ReplicatedUsing = OnRep_bIsAvailable`): 사용 가능 여부
- 상태 전이: `Available → Used → Recharging (30초) → Available`
- 3개의 BlueprintImplementableEvent: `OnWaterSourceUsed`, `OnWaterSourceRecharged`, `OnAvailabilityChanged`
- `OnRep_bIsAvailable()` → `OnAvailabilityChanged(bIsAvailable)` 호출 (클라이언트 동기화)

**기존 VFX 에셋**:
- `Content/Blueprints/VFX/WaterSource/NE_drop_effects03.uasset` - 물방울 이펙트
- 관련 머터리얼/텍스처 다수 존재

### 4.2 구현 방안: C++ UNiagaraComponent 2개 추가

수원지는 독립 액터이므로, C++에서 `UNiagaraComponent` 2개를 직접 추가하고 `OnRep_bIsAvailable()`에서 제어합니다.

### 4.3 상세 구현

#### Step 1: `ADRWaterSource`에 나이아가라 컴포넌트 2개 추가

**파일**: `Source/DaeRune/Public/Actor/DRWaterSource.h`

```cpp
// === 추가할 전방 선언 ===
class UNiagaraComponent;
class UNiagaraSystem;

// === 추가할 멤버 변수 (public 섹션) ===
public:
    // 수원지 사용 가능 시 표시할 나이아가라 에셋 1 (예: 물기둥/분수)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Source|VFX")
    TObjectPtr<UNiagaraSystem> AvailableVFXSystem1;

    // 수원지 사용 가능 시 표시할 나이아가라 에셋 2 (예: 반짝이/물방울)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Water Source|VFX")
    TObjectPtr<UNiagaraSystem> AvailableVFXSystem2;

// === 추가할 멤버 변수 (protected 섹션) ===
protected:
    // 나이아가라 컴포넌트 1
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water Source|VFX")
    TObjectPtr<UNiagaraComponent> AvailableVFXComponent1;

    // 나이아가라 컴포넌트 2
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Water Source|VFX")
    TObjectPtr<UNiagaraComponent> AvailableVFXComponent2;

    // VFX 활성화/비활성화 헬퍼
    void UpdateAvailabilityVFX(bool bAvailable);

    virtual void BeginPlay() override;
```

#### Step 2: 생성자에서 컴포넌트 생성 및 초기화

**파일**: `Source/DaeRune/Private/Actor/DRWaterSource.cpp`

```cpp
// === 추가할 헤더 ===
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

ADRWaterSource::ADRWaterSource()
{
    // 나이아가라 컴포넌트 1 생성
    AvailableVFXComponent1 = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AvailableVFX1"));
    AvailableVFXComponent1->SetupAttachment(RootComponent);
    AvailableVFXComponent1->bAutoActivate = false;

    // 나이아가라 컴포넌트 2 생성
    AvailableVFXComponent2 = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AvailableVFX2"));
    AvailableVFXComponent2->SetupAttachment(RootComponent);
    AvailableVFXComponent2->bAutoActivate = false;
}
```

> **주의**: `ADREffectActor` 생성자에서 `RootComponent`가 설정되지 않았을 경우, `SetupAttachment(RootComponent)` 호출이 실패할 수 있습니다. 이 경우 `ADREffectActor`의 생성자를 확인하고, 필요 시 `ADRWaterSource` 생성자에서 `RootComponent`를 먼저 생성해야 합니다.

#### Step 3: BeginPlay에서 에셋 할당 및 초기 VFX 상태 설정

```cpp
void ADRWaterSource::BeginPlay()
{
    Super::BeginPlay();

    // 에셋 할당
    if (AvailableVFXSystem1 && AvailableVFXComponent1)
    {
        AvailableVFXComponent1->SetAsset(AvailableVFXSystem1);
    }

    if (AvailableVFXSystem2 && AvailableVFXComponent2)
    {
        AvailableVFXComponent2->SetAsset(AvailableVFXSystem2);
    }

    // 초기 상태에 맞춰 VFX 설정
    UpdateAvailabilityVFX(bIsAvailable);
}
```

#### Step 4: VFX 활성화/비활성화 헬퍼 함수

```cpp
void ADRWaterSource::UpdateAvailabilityVFX(bool bAvailable)
{
    if (AvailableVFXComponent1)
    {
        if (bAvailable)
        {
            AvailableVFXComponent1->Activate(true);
        }
        else
        {
            AvailableVFXComponent1->Deactivate();
        }
    }

    if (AvailableVFXComponent2)
    {
        if (bAvailable)
        {
            AvailableVFXComponent2->Activate(true);
        }
        else
        {
            AvailableVFXComponent2->Deactivate();
        }
    }
}
```

#### Step 5: `OnRep_bIsAvailable()`에서 VFX 제어

기존 `OnRep_bIsAvailable()` 수정:

```cpp
void ADRWaterSource::OnRep_bIsAvailable()
{
    // VFX 상태 업데이트
    UpdateAvailabilityVFX(bIsAvailable);

    // 기존 Blueprint 이벤트 호출 유지
    OnAvailabilityChanged(bIsAvailable);
}
```

#### Step 6: 서버에서도 VFX 업데이트 (SetWaterSourceAvailable)

서버는 `OnRep` 콜백이 자동 호출되지 않으므로 `SetWaterSourceAvailable()`을 수정:

```cpp
void ADRWaterSource::SetWaterSourceAvailable(bool bNewAvailable)
{
    if (!HasAuthority()) return;

    if (bIsAvailable != bNewAvailable)
    {
        bIsAvailable = bNewAvailable;

        // 서버에서 VFX 업데이트
        UpdateAvailabilityVFX(bIsAvailable);

        // 서버에서 수동으로 RepNotify 로직 실행 (Blueprint 이벤트 포함)
        OnRep_bIsAvailable();
    }
}
```

### 4.4 동작 흐름

```
수원지 초기 상태 (bIsAvailable = true)
  └→ BeginPlay()
      └→ SetAsset() × 2
      └→ UpdateAvailabilityVFX(true)
          └→ AvailableVFXComponent1->Activate()  [이펙트 1 표시]
          └→ AvailableVFXComponent2->Activate()  [이펙트 2 표시]

플레이어가 수원지 사용
  └→ FillPlayerWater() [서버]
      └→ SetWaterSourceAvailable(false)
          └→ UpdateAvailabilityVFX(false) [서버 로컬]
              └→ Deactivate() × 2  [이펙트 1, 2 숨김]
          └→ OnRep_bIsAvailable() [서버 → Blueprint 이벤트]
      └→ bIsAvailable = false 복제 → [클라이언트]
          └→ OnRep_bIsAvailable()
              └→ UpdateAvailabilityVFX(false) [클라이언트]
                  └→ Deactivate() × 2

30초 후 리차지 완료
  └→ OnSourceRecharged() [서버]
      └→ SetWaterSourceAvailable(true)
          └→ UpdateAvailabilityVFX(true) [서버 로컬]
              └→ Activate() × 2  [이펙트 1, 2 다시 표시]
          └→ OnRep_bIsAvailable() [서버 → Blueprint 이벤트]
      └→ bIsAvailable = true 복제 → [클라이언트]
          └→ OnRep_bIsAvailable()
              └→ UpdateAvailabilityVFX(true) [클라이언트]
                  └→ Activate() × 2
```

### 4.5 Blueprint 설정

1. `BP_WaterSource` 블루프린트에서 `AvailableVFXSystem1`, `AvailableVFXSystem2` 할당
2. 컴포넌트의 상대 위치/회전은 블루프린트 에디터에서 조절 가능 (`VisibleAnywhere`)
3. 추천 에셋 조합:
   - **VFX 1**: 물기둥/분수 효과 (수직 방향, 수원지 위로 솟아오르는 물)
   - **VFX 2**: 주변 물방울/반짝이 효과 (수원지 주변에 흩뿌려지는 파티클)
4. 기존 `Content/Blueprints/VFX/WaterSource/NE_drop_effects03.uasset` 활용 가능

---

## 5. 파일 수정 목록

### 수정이 필요한 파일

| # | 파일 | 변경 유형 | 설명 |
|---|------|-----------|------|
| 1 | `Source/DaeRune/Public/Game/DRStageGameState.h` | 수정 | Multicast RPC 4개 선언, VFX 컴포넌트 캐시 배열 2개 추가 |
| 2 | `Source/DaeRune/Private/Game/DRStageGameState.cpp` | 수정 | Multicast RPC 4개 구현, 헤더 include 추가 |
| 3 | `Source/DaeRune/Public/Phase/DRPhase3.h` | 수정 | VFX 에셋 프로퍼티 2개, 타이머 핸들 1개, VFX 표시 시간 프로퍼티 1개, 전방 선언 추가 |
| 4 | `Source/DaeRune/Private/Phase/DRPhase3.cpp` | 수정 | OnPhaseStart/OnPhaseEnd/StartNextWave/BeginDestroy에 VFX 호출 추가 |
| 5 | `Source/DaeRune/Public/Actor/DRWaterSource.h` | 수정 | 나이아가라 에셋 2개, 컴포넌트 2개, 헬퍼 함수, BeginPlay 오버라이드 선언 추가 |
| 6 | `Source/DaeRune/Private/Actor/DRWaterSource.cpp` | 수정 | 생성자 수정, BeginPlay/UpdateAvailabilityVFX 구현, OnRep_bIsAvailable/SetWaterSourceAvailable 수정 |

### 새로 생성할 파일

없음 (기존 파일 수정만으로 구현 가능)

### Blueprint 설정이 필요한 항목

| # | Blueprint | 설정 항목 |
|---|-----------|-----------|
| 1 | `BP_DRStageGameMode` (Phase3 CDO) | `EnemySpawnPointNiagaraSystem` 에셋 할당 |
| 2 | `BP_DRStageGameMode` (Phase3 CDO) | `EliteSpawnPointNiagaraSystem` 에셋 할당 |
| 3 | `BP_DRStageGameMode` (Phase3 CDO) | `EliteSpawnVFXDuration` 값 설정 (기본 3초) |
| 4 | `BP_WaterSource` | `AvailableVFXSystem1` 에셋 할당 |
| 5 | `BP_WaterSource` | `AvailableVFXSystem2` 에셋 할당 |
| 6 | `BP_WaterSource` | 컴포넌트 위치/회전 조절 (필요 시) |

---

## 6. 구현 순서

### Phase 1: 수원지 VFX (가장 독립적, 즉시 테스트 가능)
1. `DRWaterSource.h` 수정 - 컴포넌트/에셋/헬퍼 선언 추가
2. `DRWaterSource.cpp` 수정 - 생성자, BeginPlay, UpdateAvailabilityVFX, OnRep, SetAvailable 수정
3. 컴파일 후 `BP_WaterSource`에서 에셋 할당 및 테스트
4. PIE에서 수원지 사용/리차지 시 VFX 토글 확인

### Phase 2: 일반 적 스폰 포인트 VFX
1. `DRStageGameState.h` 수정 - Enemy 관련 Multicast RPC 2개 선언, VFX 컴포넌트 캐시 추가
2. `DRStageGameState.cpp` 수정 - Multicast RPC 2개 구현
3. `DRPhase3.h` 수정 - `EnemySpawnPointNiagaraSystem` 프로퍼티 추가
4. `DRPhase3.cpp` 수정 - `OnPhaseStart`/`OnPhaseEnd`에 VFX 호출 추가
5. 컴파일 후 `BP_DRStageGameMode`에서 에셋 할당 및 테스트
6. Phase3 시작/종료 시 스폰 포인트 VFX 확인

### Phase 3: 엘리트 보스 스폰 포인트 VFX
1. `DRStageGameState.h` 수정 - Elite 관련 Multicast RPC 2개 선언, VFX 컴포넌트 캐시 추가
2. `DRStageGameState.cpp` 수정 - Multicast RPC 2개 구현
3. `DRPhase3.h` 수정 - `EliteSpawnPointNiagaraSystem`, 타이머 관련 프로퍼티 추가
4. `DRPhase3.cpp` 수정 - `StartNextWave`에 VFX 호출, `OnPhaseEnd`/`BeginDestroy`에 타이머 정리 추가
5. 컴파일 후 `BP_DRStageGameMode`에서 에셋 할당 및 테스트
6. 엘리트 웨이브에서 VFX 표시/소멸 확인

### 최종 테스트
- 멀티플레이어 PIE (2인 이상)에서 전체 Phase3 진행
- 모든 클라이언트에서 VFX 동기화 확인
- 수원지 사용/리차지 VFX 토글 확인
- Phase3 종료 후 모든 VFX 정리 확인

---

## 부록: 주의사항

### A. ADREffectActor의 RootComponent 확인
`ADRWaterSource` 생성자에서 `SetupAttachment(RootComponent)` 호출 시, 부모 `ADREffectActor`에서 `RootComponent`가 이미 생성되어 있는지 확인해야 합니다. `ADREffectActor`의 생성자를 확인하여 `RootComponent`가 설정되어 있는지 검증 필요.

### B. `bAutoDestroy` 설정
- **일반 적 스폰 포인트 VFX**: `bAutoDestroy = false` (Phase3 내내 유지해야 하므로)
- **엘리트 보스 스폰 포인트 VFX**: `bAutoDestroy = false` (타이머로 수동 제거)
- `DeactivateImmediate()` + `DestroyComponent()`로 수동 정리

### C. Phase3 중간 합류 클라이언트 (Late Join)
Multicast RPC는 호출 시점에 접속된 클라이언트에만 전달됩니다. 중간 합류 클라이언트를 위해서는 `ADRStageGameState`에 복제 프로퍼티를 추가하여 현재 VFX 상태를 저장하는 것을 고려할 수 있습니다. 하지만 현재 프로젝트에서는 게임 시작 후 합류가 제한적이므로, 이 부분은 추후 필요 시 구현합니다.

### D. 나이아가라 에셋 권장사항
- **일반 스폰 포인트**: 루프 재생(Loop)하는 포탈/소환진 이펙트 (Phase3 내내 표시)
- **엘리트 스폰 포인트**: 강렬한 색상(빨간/보라)의 짧은 버스트 + 페이드아웃 이펙트
- **수원지 VFX 1**: 물기둥/분수 (루프, 수직 방향)
- **수원지 VFX 2**: 물방울/반짝이 파티클 (루프, 주변 확산)
