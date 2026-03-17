# 독가스 데칼 클라이언트 비가시 문제 분석

## 1. 문제 현상

- **서버**: 독가스 데칼(GroundDecal)이 정상적으로 표시됨
- **클라이언트**: 독가스 데칼이 전혀 보이지 않음

---

## 2. 관련 클래스 구조

```
AActor
└── ADREffectActor          (Source/DaeRune/Public/Actor/DREffectActor.h)
    └── ADRPoisonGasActor   (Source/DaeRune/Public/Actor/DRPoisonGasActor.h)
```

- **ADREffectActor**: GameplayEffect를 적용하는 범용 액터. `AActor`를 직접 상속.
- **ADRPoisonGasActor**: 독가스 전용 액터. `UDecalComponent`로 바닥 시각 효과 표시, 2단계(Warning→Active) 색상 전환, 주기적 범위 체크로 GE 적용/제거.

---

## 3. 근본 원인 분석

### 3.1 핵심 문제: 액터 리플리케이션 미설정

**`ADREffectActor` 생성자** (`DREffectActor.cpp:9-15`):
```cpp
ADREffectActor::ADREffectActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>("SceneRoot"));
}
```

**`ADRPoisonGasActor` 생성자** (`DRPoisonGasActor.cpp:15-27`):
```cpp
ADRPoisonGasActor::ADRPoisonGasActor()
{
    InfiniteEffectApplicationPolicy = EEffectApplicationPolicy::ApplyOnOverlap;
    InfiniteEffectRemovalPolicy = EEffectRemovalPolicy::RemoveOnEndOverlap;
    bApplyEffectsToEnemies = true;

    GroundDecal = CreateDefaultSubobject<UDecalComponent>("GroundDecal");
    GroundDecal->SetupAttachment(GetRootComponent());
    GroundDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    GroundDecal->DecalSize = FVector(300.0f, 312.5f, 312.5f);
}
```

**두 생성자 모두 `bReplicates = true`를 설정하지 않는다.**

Unreal Engine에서 서버가 `SpawnActor`를 호출해도 `bReplicates = true`가 아닌 액터는 **서버에만 존재**하고 클라이언트에는 리플리케이트되지 않는다. 따라서 클라이언트 월드에는 `ADRPoisonGasActor` 인스턴스 자체가 없으므로, `UDecalComponent`도 존재하지 않아 데칼이 보이지 않는 것이다.

### 3.2 스폰 경로: 서버 전용 실행

독가스 액터 스폰 경로:
```
UDRPhase3::SpawnToxicGas()                    // 서버의 GameMode에서 호출
  → 타이머로 UDRPhase3::SpawnPoisonGasActor() 반복 실행
    → World->SpawnActor<ADRPoisonGasActor>()  // 서버에서만 스폰
```

`UDRPhase3`는 `ADRStageGameMode`의 하위 시스템이고, GameMode는 **서버에만 존재**한다. 따라서:

1. `SpawnPoisonGasActor()`는 서버에서만 실행됨 (`DRPhase3.cpp:1026-1060`)
2. `SpawnActor` 호출 시 `bReplicates = false`인 액터이므로 클라이언트에 리플리케이트되지 않음
3. 결과적으로 클라이언트 월드에는 독가스 액터가 존재하지 않음

### 3.3 데칼 색상 전환도 서버에서만 동작

```cpp
void ADRPoisonGasActor::TransitionToActive()   // DRPoisonGasActor.cpp:92-103
{
    CurrentPhase = EPoisonGasPhase::Active;
    if (DecalMID)
    {
        DecalMID->SetVectorParameterValue("Color", ActiveColor);
    }
}
```

- `CurrentPhase`가 `UPROPERTY(Replicated)`도 아니고, `DecalMID` 색상 변경은 로컬 호출임
- 하지만 이건 부차적 문제 — 액터 자체가 클라이언트에 없으므로 의미 없음

### 3.4 BeginPlay의 서버 전용 타이머

```cpp
void ADRPoisonGasActor::BeginPlay()            // DRPoisonGasActor.cpp:29-63
{
    Super::BeginPlay();

    // Decal 크기 동기화, Dynamic Material 생성
    GroundDecal->DecalSize = FVector(300.0f, DecalRadius, DecalRadius);
    if (DecalBaseMaterial)
    {
        DecalMID = UMaterialInstanceDynamic::Create(DecalBaseMaterial, this);
        GroundDecal->SetDecalMaterial(DecalMID);
        DecalMID->SetVectorParameterValue("Color", WarningColor);
    }

    // 3초 후 Active로 전환 (로컬 타이머)
    GetWorldTimerManager().SetTimer(
        PhaseTransitionTimerHandle, this,
        &ADRPoisonGasActor::TransitionToActive,
        3.0f, false
    );

    // 효과 판정 타이머 (서버에서만)
    if (HasAuthority())
    {
        GetWorldTimerManager().SetTimer(
            EffectCheckTimerHandle, this,
            &ADRPoisonGasActor::CheckNearbyTargets,
            EffectCheckInterval,
            true, 3.0f
        );
    }
}
```

- Decal Material 생성과 Phase 전환 타이머는 `HasAuthority()` 체크 없이 실행되므로, **만약 액터가 리플리케이트된다면** 클라이언트에서도 시각 효과가 동작할 것임
- 효과 판정(`CheckNearbyTargets`)은 올바르게 `HasAuthority()` 체크가 되어 있어, 서버에서만 GE를 적용함
- 즉, **비주얼 로직은 클라이언트에서 동작하도록 이미 설계되어 있지만**, 액터 리플리케이션이 빠져있어 무용지물인 상태

---

## 4. 영향 범위

| 항목 | 서버 | 클라이언트 |
|------|------|-----------|
| 독가스 액터 존재 | O | X (리플리케이트 안 됨) |
| 바닥 데칼 표시 | O | X (액터 자체가 없음) |
| Warning→Active 색상 전환 | O | X |
| 독가스 데미지/슬로우 GE 적용 | O (서버 권한) | GE는 ASC 리플리케이션으로 전달됨 |
| Overlap 카운트 관리 | O (서버 전용) | X |

**참고**: GE(GameplayEffect) 적용 자체는 서버에서 ASC를 통해 이루어지므로, 데미지/슬로우 효과는 클라이언트 캐릭터에게도 정상 적용된다. 문제는 **시각적 표시(데칼)**만 누락되는 것이다.

---

## 5. 해결 방안

### 방안 A: `bReplicates = true` 설정 (권장)

`ADRPoisonGasActor` 생성자에 리플리케이션을 활성화한다:

```cpp
ADRPoisonGasActor::ADRPoisonGasActor()
{
    bReplicates = true;   // ← 추가: 서버에서 스폰 시 클라이언트에도 리플리케이트
    // ... 기존 코드 ...
}
```

**장점**:
- 가장 간단한 수정
- 액터가 클라이언트에 리플리케이트되면 `BeginPlay()`가 클라이언트에서도 실행됨
- DecalComponent, Dynamic Material, 색상 전환 타이머가 자연스럽게 동작
- GE 판정 로직은 이미 `HasAuthority()` 체크가 있으므로 서버에서만 실행됨

**고려사항**:
- `CurrentPhase`를 `UPROPERTY(ReplicatedUsing=OnRep_CurrentPhase)`로 변경하면, 액터 리플리케이트 후 Warning→Active 전환도 정확히 동기화 가능
- 단, 현재 타이머 기반 전환(3초)이 `BeginPlay`에서 시작되므로, 클라이언트가 액터 생성 직후 `BeginPlay`를 실행하면 거의 동시에 타이머가 돌아 큰 차이 없음
- `SetLifeSpan(10.0f)`은 리플리케이트되는 함수이므로 클라이언트에서도 수명이 동기화됨

### 방안 B: `CurrentPhase` 리플리케이션 추가 (선택적 보강)

만약 정확한 페이즈 동기화가 필요하다면:

```cpp
// 헤더
UPROPERTY(ReplicatedUsing = OnRep_CurrentPhase)
EPoisonGasPhase CurrentPhase = EPoisonGasPhase::Warning;

UFUNCTION()
void OnRep_CurrentPhase();

// 구현
void ADRPoisonGasActor::OnRep_CurrentPhase()
{
    if (CurrentPhase == EPoisonGasPhase::Active && DecalMID)
    {
        DecalMID->SetVectorParameterValue("Color", ActiveColor);
    }
}

void ADRPoisonGasActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADRPoisonGasActor, CurrentPhase);
}
```

**장점**:
- 클라이언트 접속이 늦거나, 액터 생성과 리플리케이션 사이 시간 차이가 있을 때도 정확한 색상 표시
- 네트워크 지연으로 인한 색상 불일치 방지

### 방안 C: Multicast RPC로 비주얼 스폰 (대안)

액터 리플리케이션 대신, 서버에서 Multicast RPC를 통해 클라이언트에게 데칼을 직접 스폰하는 방식. 그러나 이 방법은 불필요하게 복잡하고, `bReplicates`로 충분히 해결 가능하므로 비권장.

---

## 6. 결론

**근본 원인**: `ADRPoisonGasActor`(및 부모 `ADREffectActor`)의 생성자에 `bReplicates = true`가 없어서, 서버에서 스폰된 독가스 액터가 클라이언트에 리플리케이트되지 않는다. 클라이언트 월드에 액터가 존재하지 않으므로 `UDecalComponent`도 없고, 데칼이 보이지 않는다.

**권장 수정**: `ADRPoisonGasActor` 생성자에 `bReplicates = true` 한 줄 추가. 비주얼 로직(DecalComponent, Dynamic Material, 타이머 기반 색상 전환)은 이미 클라이언트에서 동작하도록 설계되어 있으므로, 리플리케이션만 활성화하면 즉시 해결된다.

---

## 7. 관련 파일 목록

| 파일 | 역할 |
|------|------|
| `Source/DaeRune/Public/Actor/DRPoisonGasActor.h` | 독가스 액터 헤더 (DecalComponent, 페이즈, 타이머 정의) |
| `Source/DaeRune/Private/Actor/DRPoisonGasActor.cpp` | 독가스 액터 구현 (BeginPlay, 색상 전환, 범위 체크) |
| `Source/DaeRune/Public/Actor/DREffectActor.h` | 부모 클래스 헤더 (GE 적용 정책 정의) |
| `Source/DaeRune/Private/Actor/DREffectActor.cpp` | 부모 클래스 구현 (GE 적용/제거 로직) |
| `Source/DaeRune/Private/Phase/DRPhase3.cpp:1026-1060` | 독가스 스폰 로직 (서버 GameMode에서 실행) |
| `Content/Blueprints/Actor/Area/BP_PosionGas.uasset` | 독가스 블루프린트 (머티리얼/GE 클래스 설정) |
| `Content/Blueprints/Actor/Area/Material/M_PoisonGasDecal.uasset` | 데칼 베이스 머티리얼 |
| `Content/Blueprints/Actor/Area/GE_PoisonDamage.uasset` | 독가스 데미지 GE |
| `Content/Blueprints/Actor/Area/GE_PoisonSlow.uasset` | 독가스 슬로우 GE |
