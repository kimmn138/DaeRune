# DRPoisonGasActor 데칼 머티리얼 교체 방식 구현 계획

## 현재 구현 분석

### 현재 방식 (단일 데칼 + MID 색상 변경)

**데칼 컴포넌트**: `GroundDecal` 1개 사용
- 생성자에서 `UDecalComponent` 1개 생성
- `DecalBaseMaterial`로부터 `UMaterialInstanceDynamic`(MID) 생성
- MID의 `"Color"` 파라미터를 통해 색상 전환

**색상 전환 흐름**:
1. `BeginPlay()`: MID 생성 → `WarningColor` (노란색 `1.0, 0.8, 0.0, 0.6`) 설정
2. 3초 타이머 (`PhaseTransitionTimerHandle`) → `TransitionToActive()` 호출
3. `TransitionToActive()`: MID의 `"Color"`를 `ActiveColor` (보라색 `0.5, 0.0, 0.8, 0.8`)로 변경

**관련 프로퍼티** (`DRPoisonGasActor.h`):
```cpp
// 데칼 컴포넌트 (1개)
UPROPERTY(VisibleAnywhere)
TObjectPtr<UDecalComponent> GroundDecal;

// 머티리얼 (1개 - MID의 베이스)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
TObjectPtr<UMaterialInterface> DecalBaseMaterial;

// 색상 (MID에서 동적 변경용)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
FLinearColor WarningColor = FLinearColor(1.0f, 0.8f, 0.0f, 0.6f);

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
FLinearColor ActiveColor = FLinearColor(0.5f, 0.0f, 0.8f, 0.8f);

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
float DecalRadius = 312.5f;

// 런타임 MID
UPROPERTY()
TObjectPtr<UMaterialInstanceDynamic> DecalMID;
```

---

## 변경 목표

**단일 데칼 + MID 색상 변경** → **단일 데칼 + 머티리얼 교체**로 전환

- 데칼 컴포넌트는 `GroundDecal` 1개를 그대로 유지
- 경고/활성화 각각 별도의 머티리얼을 에디터에서 할당
- 3초 경과 시 `SetDecalMaterial()`로 머티리얼 자체를 교체
- MID 생성 및 런타임 색상 변경 로직 제거

---

## 상세 구현 계획

### 1단계: 헤더 파일 수정 (`DRPoisonGasActor.h`)

#### 1-1. 데칼 컴포넌트 — 변경 없음

`GroundDecal`은 그대로 유지합니다. 이름도 변경하지 않습니다.

```cpp
// 변경 없음 - 그대로 유지
UPROPERTY(VisibleAnywhere)
TObjectPtr<UDecalComponent> GroundDecal;
```

#### 1-2. 머티리얼 프로퍼티 변경

**제거할 프로퍼티 (4개)**:
```cpp
// 1. 기존 단일 베이스 머티리얼 → 경고/활성화 2개로 대체
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
TObjectPtr<UMaterialInterface> DecalBaseMaterial;

// 2. 경고 색상 → 머티리얼 자체 색상 사용하므로 불필요
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
FLinearColor WarningColor = FLinearColor(1.0f, 0.8f, 0.0f, 0.6f);

// 3. 활성화 색상 → 머티리얼 자체 색상 사용하므로 불필요
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
FLinearColor ActiveColor = FLinearColor(0.5f, 0.0f, 0.8f, 0.8f);

// 4. MID → 동적 머티리얼 인스턴스 생성 자체가 불필요
UPROPERTY()
TObjectPtr<UMaterialInstanceDynamic> DecalMID;
```

**추가할 프로퍼티 (2개)**:
```cpp
// 경고 단계 머티리얼 (에디터에서 할당, 3초 동안 표시)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
TObjectPtr<UMaterialInterface> WarningDecalMaterial;

// 활성화 단계 머티리얼 (에디터에서 할당, 3초 후 교체)
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
TObjectPtr<UMaterialInterface> ActiveDecalMaterial;
```

#### 1-3. DecalRadius — 변경 없음

```cpp
// 변경 없음 - 그대로 유지
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
float DecalRadius = 312.5f;
```

#### 1-4. 최종 헤더 Visual 프로퍼티 모습

변경 전:
```cpp
UPROPERTY(VisibleAnywhere)
TObjectPtr<UDecalComponent> GroundDecal;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
TObjectPtr<UMaterialInterface> DecalBaseMaterial;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
FLinearColor WarningColor = FLinearColor(1.0f, 0.8f, 0.0f, 0.6f);

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
FLinearColor ActiveColor = FLinearColor(0.5f, 0.0f, 0.8f, 0.8f);

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
float DecalRadius = 312.5f;

UPROPERTY()
TObjectPtr<UMaterialInstanceDynamic> DecalMID;
```

변경 후:
```cpp
UPROPERTY(VisibleAnywhere)
TObjectPtr<UDecalComponent> GroundDecal;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
TObjectPtr<UMaterialInterface> WarningDecalMaterial;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
TObjectPtr<UMaterialInterface> ActiveDecalMaterial;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
float DecalRadius = 312.5f;
```

---

### 2단계: 생성자 수정 (`DRPoisonGasActor.cpp` - 생성자)

생성자의 `GroundDecal` 생성 코드는 **완전히 그대로 유지**합니다.

```cpp
// 변경 없음 - 그대로 유지 (24-27줄 부근)
GroundDecal = CreateDefaultSubobject<UDecalComponent>("GroundDecal");
GroundDecal->SetupAttachment(GetRootComponent());
GroundDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
GroundDecal->DecalSize = FVector(300.0f, 312.5f, 312.5f);
```

생성자에서는 아무것도 수정할 필요가 없습니다.

---

### 3단계: BeginPlay 수정 (`DRPoisonGasActor.cpp` - BeginPlay)

#### 3-1. 기존 MID 생성/색상 설정 코드 제거

```cpp
// 제거할 코드 (34-43줄 부근)
GroundDecal->DecalSize = FVector(300.0f, DecalRadius, DecalRadius);

if (DecalBaseMaterial)
{
    DecalMID = UMaterialInstanceDynamic::Create(DecalBaseMaterial, this);
    GroundDecal->SetDecalMaterial(DecalMID);
    DecalMID->SetVectorParameterValue("Color", WarningColor);
}
```

#### 3-2. 새 초기화 코드 추가

```cpp
// DecalRadius 적용 (기존과 동일)
GroundDecal->DecalSize = FVector(300.0f, DecalRadius, DecalRadius);

// 경고 머티리얼을 초기 머티리얼로 설정
if (WarningDecalMaterial)
{
    GroundDecal->SetDecalMaterial(WarningDecalMaterial);
}
```

**핵심 차이점**:
- 기존: `UMaterialInstanceDynamic::Create()` → MID 생성 → `SetVectorParameterValue("Color", WarningColor)`
- 변경: `WarningDecalMaterial`을 직접 `SetDecalMaterial()`로 설정 (MID 불필요)
- `DecalRadius` 적용 로직은 동일하게 유지

---

### 4단계: TransitionToActive 수정 (`DRPoisonGasActor.cpp`)

#### 4-1. 기존 MID 색상 변경 코드

```cpp
// 기존 코드 (93-104줄 부근)
void ADRPoisonGasActor::TransitionToActive()
{
    CurrentPhase = EPoisonGasPhase::Active;

    if (DecalMID)
    {
        DecalMID->SetVectorParameterValue("Color", ActiveColor);
    }
}
```

#### 4-2. 머티리얼 교체 코드로 변경

```cpp
void ADRPoisonGasActor::TransitionToActive()
{
    CurrentPhase = EPoisonGasPhase::Active;

    // 경고 머티리얼 → 활성화 머티리얼로 교체
    if (ActiveDecalMaterial)
    {
        GroundDecal->SetDecalMaterial(ActiveDecalMaterial);
    }
}
```

**핵심 변경**:
- 기존: `DecalMID->SetVectorParameterValue("Color", ActiveColor)` — MID의 Color 파라미터 변경
- 변경: `GroundDecal->SetDecalMaterial(ActiveDecalMaterial)` — 머티리얼 자체를 교체
- 동일한 `GroundDecal` 컴포넌트에 다른 머티리얼을 끼워넣는 방식

---

### 5단계: 기타 코드에서 제거된 프로퍼티 참조 확인

제거되는 프로퍼티: `DecalBaseMaterial`, `WarningColor`, `ActiveColor`, `DecalMID`

| 위치 | 참조 | 처리 |
|------|------|------|
| 생성자 | 참조 없음 | 수정 불필요 |
| BeginPlay | `DecalBaseMaterial`, `DecalMID`, `WarningColor` | 3단계에서 대체 |
| TransitionToActive | `DecalMID`, `ActiveColor` | 4단계에서 대체 |
| EndPlay | 참조 없음 (타이머 정리만) | 수정 불필요 |

`GroundDecal`은 유지되므로 해당 참조는 모두 정상 동작합니다.

---

### 6단계: 블루프린트 업데이트 (`BP_PosionGas`)

#### 6-1. 사라지는 프로퍼티

블루프린트 에디터의 디테일 패널에서 다음 프로퍼티가 사라집니다:
- `DecalBaseMaterial` — 삭제됨
- `WarningColor` — 삭제됨
- `ActiveColor` — 삭제됨

#### 6-2. 새로 나타나는 프로퍼티

"Poison Gas | Visual" 카테고리에 새 프로퍼티가 나타납니다:
- **`WarningDecalMaterial`** — 여기에 경고용 머티리얼 인스턴스 할당
- **`ActiveDecalMaterial`** — 여기에 활성화용 머티리얼 인스턴스 할당

#### 6-3. 머티리얼 할당 예시

1. **WarningDecalMaterial** 슬롯:
   - `MI_PoisonGasWarningDecal` 할당 (경고 색상/패턴이 적용된 머티리얼 인스턴스)

2. **ActiveDecalMaterial** 슬롯:
   - `MI_PoisonGasDecal` 할당 (기존 활성화 색상/패턴이 적용된 머티리얼 인스턴스)

#### 6-4. 유지되는 것들

- `GroundDecal` 컴포넌트 — 이름/위치/회전 모두 그대로
- `DecalRadius` — 그대로 유지
- 기타 Effects, Detection 카테고리 프로퍼티 — 변경 없음

---

### 7단계: 머티리얼 에셋 준비 (에디터에서 수동 작업)

현재 존재하는 독가스 머티리얼:
- `Content/Blueprints/Actor/Area/Material/M_PoisonGasDecal.uasset` (베이스 머티리얼)
- `Content/Blueprints/Actor/Area/Material/MI_PoisonGasDecal.uasset` (머티리얼 인스턴스)

#### 필요한 작업

**기존 머티리얼 인스턴스 활용 + 경고용 새로 생성** (권장):

1. **`MI_PoisonGasDecal`** → `ActiveDecalMaterial`로 사용 (기존 것 그대로 활용)
2. **`MI_PoisonGasWarningDecal`** 새로 생성 → `WarningDecalMaterial`로 사용
   - `M_PoisonGasDecal`에서 파생된 새 머티리얼 인스턴스 생성
   - 색상 파라미터를 경고색(노란색/주황색 등)으로 설정
   - 저장 경로: `Content/Blueprints/Actor/Area/Material/MI_PoisonGasWarningDecal`

> **참고**: 전기장 머티리얼이 이미 동일한 2개 패턴으로 존재합니다:
> - `M_ElectricFieldDecal` / `MI_ElectricFieldDecal` (활성화용)
> - `M_ElectricFieldWarningDecal` / `MI_ElectricFieldWarningDecal` (경고용)
>
> 독가스도 이 패턴을 따르면 프로젝트 내 일관성이 유지됩니다.

---

## 수정 대상 파일 요약

| 파일 | 변경 내용 | 작업 방식 |
|------|----------|----------|
| `Source/DaeRune/Public/Actor/DRPoisonGasActor.h` | `DecalBaseMaterial`/`WarningColor`/`ActiveColor`/`DecalMID` 제거, `WarningDecalMaterial`/`ActiveDecalMaterial` 추가 | C++ 코드 수정 |
| `Source/DaeRune/Private/Actor/DRPoisonGasActor.cpp` | BeginPlay과 TransitionToActive만 수정 (생성자 변경 없음) | C++ 코드 수정 |
| `Content/Blueprints/Actor/Area/BP_PosionGas.uasset` | 새 프로퍼티에 머티리얼 할당 | 에디터에서 수동 |
| `Content/Blueprints/Actor/Area/Material/` | 경고용 머티리얼 인스턴스 `MI_PoisonGasWarningDecal` 생성 | 에디터에서 수동 |

---

## 변경 전후 비교

### 프로퍼티 매핑

| 기존 | 변경 후 | 비고 |
|------|---------|------|
| `GroundDecal` (UDecalComponent) | `GroundDecal` (UDecalComponent) | **변경 없음** |
| `DecalBaseMaterial` (UMaterialInterface) | `WarningDecalMaterial` + `ActiveDecalMaterial` (UMaterialInterface x2) | 1개 → 2개 |
| `WarningColor` (FLinearColor) | 제거 | 머티리얼 자체 색상 사용 |
| `ActiveColor` (FLinearColor) | 제거 | 머티리얼 자체 색상 사용 |
| `DecalMID` (UMaterialInstanceDynamic) | 제거 | MID 생성 불필요 |
| `DecalRadius` (float) | `DecalRadius` (float) | **변경 없음** |

### 동작 흐름 비교

**기존**:
```
Spawn
  → 생성자: GroundDecal 컴포넌트 생성
  → BeginPlay: MID 생성, WarningColor 설정, GroundDecal에 MID 할당
  → [3초 대기]
  → TransitionToActive: MID의 "Color" 파라미터를 ActiveColor로 변경
```

**변경 후**:
```
Spawn
  → 생성자: GroundDecal 컴포넌트 생성 (변경 없음)
  → BeginPlay: GroundDecal에 WarningDecalMaterial 할당
  → [3초 대기]
  → TransitionToActive: GroundDecal에 ActiveDecalMaterial 할당 (머티리얼 교체)
```

---

## 주의사항

1. **리플리케이션**: `SetDecalMaterial()`은 로컬 호출이지만, `TransitionToActive()`가 타이머로 서버/클라이언트 모두에서 독립적으로 실행되므로 문제 없습니다. 기존 MID 색상 변경과 동일한 패턴입니다.

2. **`DREffectActor` 부모 클래스**: 데칼 관련 코드가 없으므로 수정 불필요합니다.

3. **다른 EffectActor 서브클래스에 영향 없음**: 이 변경은 `DRPoisonGasActor`에만 국한됩니다.

4. **기존 블루프린트 설정값**: `DecalBaseMaterial`, `WarningColor`, `ActiveColor` 프로퍼티가 삭제되므로 블루프린트에서 기존에 설정했던 값들이 사라집니다. 컴파일 전에 기존 블루프린트의 설정값을 메모해 두세요.

5. **타이머 로직 변경 없음**: `PhaseTransitionTimerHandle` (3초 일회성), `EffectCheckTimerHandle` (0.2초 주기, 3초 초기 딜레이) 타이머는 완전히 그대로 유지됩니다.

6. **`#include` 변경 없음**: `UDecalComponent`, `UMaterialInterface`는 이미 포함되어 있고, `UMaterialInstanceDynamic` 헤더 제거도 불필요합니다 (다른 곳에서 사용할 수 있으므로).
