# 부품 1인칭/3인칭 분리 표시 구현 계획

## 목표
플레이어가 부품(CleanserPart)을 집었을 때, 1인칭 시점(주인)과 3인칭 시점(다른 플레이어)에서 각각 독립적인 부품 메시를 보여줘서 애니메이션과 자연스럽게 어울리도록 한다.

## 현재 구조 분석

### 현재 메시 시스템 (DRCharacter)
- `FirstPersonMesh` (1P): `OnlyOwnerSee(true)`, 카메라에 부착, 주인만 보임
- `GetMesh()` (3P): `OwnerNoSee(true)`, 주인에게는 안 보이고 다른 플레이어에게만 보임
- `Weapon`: 3P 메시에 부착, `OwnerNoSee(true)`

### 현재 부품 부착 방식 (DRCleanserPart)
- `PartMesh` (UStaticMeshComponent)가 루트 컴포넌트
- 픽업 시: 액터 전체를 `Character->GetMesh()` (3P 메시)의 `TestPartHand` 소켓에 부착
- 모든 플레이어에게 동일한 메시가 보임 (1P/3P 구분 없음)
- **문제점**: 주인은 3P 메시가 안 보이므로(`OwnerNoSee`) 부품이 3P 메시에 붙어있으면 주인에게도 안 보이거나, 보이더라도 1P 애니메이션과 위치가 맞지 않음

### 관련 코드 위치
| 파일 | 역할 |
|------|------|
| `Source/DaeRune/Public/Actor/DRCleanserPart.h` | 부품 액터 선언 |
| `Source/DaeRune/Private/Actor/DRCleanserPart.cpp` | 부품 픽업/드롭/설치 로직 |
| `Source/DaeRune/Public/Character/DRCharacter.h` | 플레이어 캐릭터 선언 (1P/3P 메시) |
| `Source/DaeRune/Private/Character/DRCharacter.cpp` | 캐릭터 메시 가시성, 부품 픽업 |

---

## 설계 방향

### 핵심 아이디어
**`ADRCharacter`에 1인칭용 부품 메시 컴포넌트를 추가**하고, 부품 픽업 시:
- 기존 `ADRCleanserPart` 액터는 **3P 메시**의 `TestPartHand` 소켓에 부착 → `OwnerNoSee(true)` 설정 → 다른 플레이어에게만 보임
- `ADRCharacter`의 새로운 1P 부품 메시는 **1P 메시(FirstPersonMesh)**의 `TestPartHand` 소켓에 부착 → `OnlyOwnerSee(true)` 설정 → 주인에게만 보임

### 이 설계를 선택한 이유
1. **책임 분리**: 캐릭터가 자신의 1P 표시를 관리 (WaterPump 3P 빔과 동일한 패턴)
2. **부품 액터 독립성**: `ADRCleanserPart`는 캐릭터의 1P/3P 메시 구조를 알 필요 없음
3. **리플리케이션 단순화**: `CarriedPart`가 이미 리플리케이트되므로 `OnRep_CarriedPart`에서 1P 메시 동기화 가능
4. **기존 패턴과 일관성**: WaterPump 빔이 이미 동일한 1P/3P 분리 패턴을 사용 중

---

## 상세 구현 단계

### 1단계: ADRCharacter에 1인칭 부품 메시 컴포넌트 추가

#### DRCharacter.h 수정

```cpp
// ========== 부품 1인칭 표시 ==========

// 1인칭 시점에서 보이는 부품 메시 (주인에게만 보임)
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Part System")
TObjectPtr<UStaticMeshComponent> FirstPersonPartMesh;
```

**위치**: `FirstPersonMesh` 선언 근처 또는 부품 시스템 섹션에 추가

#### DRCharacter.cpp 생성자 수정

```cpp
// 1인칭 부품 메시 생성 (픽업 전에는 비활성)
FirstPersonPartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonPartMesh"));
FirstPersonPartMesh->SetupAttachment(FirstPersonMesh, FName("TestPartHand"));
FirstPersonPartMesh->SetOnlyOwnerSee(true);       // 주인에게만 보임
FirstPersonPartMesh->bCastDynamicShadow = false;
FirstPersonPartMesh->CastShadow = false;
FirstPersonPartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
FirstPersonPartMesh->SetVisibility(false);         // 초기에는 숨김
```

**핵심 포인트**:
- `SetupAttachment(FirstPersonMesh, "TestPartHand")` — 1P 메시의 소켓에 직접 부착
- `SetOnlyOwnerSee(true)` — 주인 클라이언트에서만 렌더링
- 초기 `SetVisibility(false)` — 부품을 들고 있지 않을 때는 안 보임

---

### 2단계: ADRCleanserPart의 3P 부품 메시에 OwnerNoSee 설정

#### DRCleanserPart.cpp `PickupPart()` 수정

현재 코드:
```cpp
void ADRCleanserPart::PickupPart(ADRCharacter* Character)
{
    // ... (기존 로직)
    AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
    // ...
}
```

수정 후:
```cpp
void ADRCleanserPart::PickupPart(ADRCharacter* Character)
{
    // ... (기존 로직 유지)

    // 3P 메시에 부착 (기존과 동일)
    AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);

    // 3P 부품은 다른 플레이어에게만 보이도록 설정
    PartMesh->SetOwnerNoSee(true);

    // 캐릭터의 1인칭 부품 메시 활성화
    Character->ShowFirstPersonPart(PartMesh->GetStaticMesh());

    // ... (기존 사운드, 태그 로직)
}
```

#### DRCleanserPart.cpp `OnRep_bIsCarried()` 수정

현재 코드 (bIsCarried == true 분기):
```cpp
AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
```

수정 후:
```cpp
AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);

// 3P 부품은 다른 플레이어에게만 보이도록 설정
PartMesh->SetOwnerNoSee(true);

// 캐릭터의 1인칭 부품 메시 활성화
CarryingCharacter->ShowFirstPersonPart(PartMesh->GetStaticMesh());
```

현재 코드 (bIsCarried == false 분기):
```cpp
DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
```

수정 후 (else 분기 앞에 추가):
```cpp
// 이전 캐릭터의 1인칭 부품 메시 숨기기
// (CarryingCharacter는 이미 nullptr일 수 있으므로 이전 값 캐시 필요)
// → 3단계에서 OnRep_CarriedPart로 처리
```

**주의**: `OnRep_bIsCarried`에서 `CarryingCharacter`가 이미 nullptr일 수 있으므로, 1P 메시 숨기기는 `ADRCharacter::OnRep_CarriedPart()`에서 처리하는 것이 더 안전함 (3단계 참조).

#### DRCleanserPart.cpp `DropFromCarrier()` 수정

```cpp
void ADRCleanserPart::DropFromCarrier()
{
    if (!HasAuthority()) return;

    // 1인칭 부품 메시 숨기기 (드롭 전에 캐릭터 참조가 유효한 시점)
    if (CarryingCharacter)
    {
        CarryingCharacter->HideFirstPersonPart();
    }

    // 기존 로직
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    bIsCarried = false;
    CarryingCharacter = nullptr;

    // OwnerNoSee 복원 (바닥에 떨어진 부품은 모두에게 보여야 함)
    PartMesh->SetOwnerNoSee(false);

    // ... (기존 콜리전 복원 로직)
}
```

#### DRCleanserPart.cpp `InstallPart()` 수정

```cpp
void ADRCleanserPart::InstallPart()
{
    if (!HasAuthority()) return;

    // 1인칭 부품 메시 숨기기
    if (CarryingCharacter)
    {
        CarryingCharacter->HideFirstPersonPart();
    }

    // 기존 태그 제거 로직
    UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(CarryingCharacter->GetAbilitySystemComponent());
    if (DRASC)
    {
        DRASC->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_Carrying);
    }

    Destroy();
}
```

---

### 3단계: ADRCharacter에 1인칭 부품 표시/숨기기 함수 추가

#### DRCharacter.h에 함수 선언 추가

```cpp
// 1인칭 부품 메시 표시 (부품 픽업 시 호출)
void ShowFirstPersonPart(UStaticMesh* InPartMesh);

// 1인칭 부품 메시 숨기기 (부품 드롭/설치 시 호출)
void HideFirstPersonPart();
```

#### DRCharacter.cpp에 함수 구현 추가

```cpp
void ADRCharacter::ShowFirstPersonPart(UStaticMesh* InPartMesh)
{
    if (!FirstPersonPartMesh || !InPartMesh) return;

    FirstPersonPartMesh->SetStaticMesh(InPartMesh);
    FirstPersonPartMesh->SetVisibility(true);
}

void ADRCharacter::HideFirstPersonPart()
{
    if (!FirstPersonPartMesh) return;

    FirstPersonPartMesh->SetVisibility(false);
    FirstPersonPartMesh->SetStaticMesh(nullptr);
}
```

#### DRCharacter.cpp `OnRep_CarriedPart()` 수정

현재 코드:
```cpp
void ADRCharacter::OnRep_CarriedPart()
{
    // 클라이언트 시각적 효과
}
```

수정 후:
```cpp
void ADRCharacter::OnRep_CarriedPart()
{
    if (CarriedPart)
    {
        // 부품을 들고 있으면 1인칭 부품 메시 표시
        if (UStaticMeshComponent* PMesh = CarriedPart->GetPartMesh())
        {
            ShowFirstPersonPart(PMesh->GetStaticMesh());
        }
    }
    else
    {
        // 부품이 없으면 1인칭 부품 메시 숨기기
        HideFirstPersonPart();
    }
}
```

**이 방식의 장점**: 서버에서 `CarriedPart`가 변경되면 `OnRep_CarriedPart`가 클라이언트에서 자동 호출되어 1P 메시가 올바르게 동기화됨.

#### DRCleanserPart.h에 Getter 추가

```cpp
// 부품 메시 Getter (캐릭터에서 1P 메시 복제에 사용)
UFUNCTION(BlueprintCallable, Category = "CleanserPart")
UStaticMeshComponent* GetPartMesh() const { return PartMesh; }
```

---

### 4단계: 드롭/설치 시 3P 부품의 OwnerNoSee 복원

#### OnRep_bIsCarried() else 분기 수정

```cpp
void ADRCleanserPart::OnRep_bIsCarried()
{
    if (bIsCarried && CarryingCharacter)
    {
        // ... (기존 부착 로직)

        // 3P 부품은 주인에게 안 보이게
        PartMesh->SetOwnerNoSee(true);

        // 1P 부품 표시
        CarryingCharacter->ShowFirstPersonPart(PartMesh->GetStaticMesh());
    }
    else
    {
        // ... (기존 분리 로직)

        // 바닥에 떨어진 부품은 모두에게 보이도록 복원
        PartMesh->SetOwnerNoSee(false);

        // Note: 1P 메시 숨기기는 OnRep_CarriedPart에서 처리됨
    }
}
```

---

### 5단계: 대기실 가시성 처리

#### DRCharacter.cpp `SetWaitingRoomVisibility()` 수정

```cpp
void ADRCharacter::SetWaitingRoomVisibility(bool bInWaitingRoom)
{
    if (bInWaitingRoom)
    {
        // ... (기존 로직)

        // 1인칭 부품 메시도 숨기기 (대기실에서는 불필요)
        if (FirstPersonPartMesh)
        {
            FirstPersonPartMesh->SetVisibility(false);
        }
    }
    else
    {
        // 일반 FPS 모드 복원
        UpdateMeshVisibility();
        // ... (기존 로직)

        // 부품을 들고 있으면 1인칭 부품 메시 복원
        if (bIsCarryingPart && CarriedPart && FirstPersonPartMesh)
        {
            if (UStaticMeshComponent* PMesh = CarriedPart->GetPartMesh())
            {
                ShowFirstPersonPart(PMesh->GetStaticMesh());
            }
        }
    }
}
```

---

### 6단계: 사망 시 부품 드롭 처리 확인

현재 `DropCarriedPart()`가 사망 시 호출되는지 확인 필요. 사망 시:
- `HideFirstPersonPart()` 호출 → 1P 부품 메시 숨김
- `PartMesh->SetOwnerNoSee(false)` 복원 → 바닥에 떨어진 부품 모두에게 보임

이미 `DropCarriedPart()` → `DropFromCarrier()` 흐름에서 처리되므로 추가 작업 불필요.
단, 사망 로직에서 `DropCarriedPart()`가 호출되는지 `DRCharacterBase` 또는 `DRAttributeSet`의 사망 처리 코드를 확인해야 함.

---

## 수정 파일 요약

| 파일 | 수정 내용 |
|------|----------|
| `DRCharacter.h` | `FirstPersonPartMesh` 컴포넌트 추가, `ShowFirstPersonPart()`, `HideFirstPersonPart()` 함수 선언 |
| `DRCharacter.cpp` | 생성자에서 1P 부품 메시 생성, Show/Hide 함수 구현, `OnRep_CarriedPart()` 구현, `SetWaitingRoomVisibility()` 수정 |
| `DRCleanserPart.h` | `GetPartMesh()` Getter 추가 |
| `DRCleanserPart.cpp` | `PickupPart()`에서 OwnerNoSee + 1P 메시 활성화, `DropFromCarrier()`에서 1P 메시 비활성화 + OwnerNoSee 복원, `InstallPart()`에서 1P 메시 비활성화, `OnRep_bIsCarried()`에서 OwnerNoSee + 1P 메시 동기화 |

---

## 소켓 전제 조건

1P 메시(`FirstPersonMesh`)와 3P 메시(`GetMesh()`) 모두 `TestPartHand` 소켓이 존재해야 함.
- **3P 메시**: 이미 존재 (현재 부품 부착에 사용 중)
- **1P 메시**: 소켓 존재 여부 확인 필요 → 없으면 1P 스켈레톤에 `TestPartHand` 소켓 추가 필요 (언리얼 에디터에서 작업)

---

## 실행 흐름 요약

### 픽업 시 (서버)
```
ADRCharacter::PickupPart()
  → ADRCleanserPart::PickupPart(Character)
    → Actor를 3P 메시의 TestPartHand에 부착
    → PartMesh->SetOwnerNoSee(true)         ← 3P 부품: 다른 플레이어에게만 보임
    → Character->ShowFirstPersonPart(mesh)   ← 1P 부품: 주인에게만 보임
  → bIsCarryingPart = true, CarriedPart = Part (리플리케이트)
```

### 픽업 시 (클라이언트 - 리플리케이션)
```
OnRep_bIsCarried() [ADRCleanserPart]
  → 3P 메시에 부착 + OwnerNoSee(true)
  → CarryingCharacter->ShowFirstPersonPart(mesh)

OnRep_CarriedPart() [ADRCharacter]
  → ShowFirstPersonPart(CarriedPart의 StaticMesh)
```

### 드롭 시 (서버)
```
ADRCharacter::DropCarriedPart()
  → Character->HideFirstPersonPart()        ← 1P 부품 숨김
  → ADRCleanserPart::DropFromCarrier()
    → DetachFromActor
    → PartMesh->SetOwnerNoSee(false)         ← 바닥 부품: 모두에게 보임
  → CarriedPart = nullptr (리플리케이트 → OnRep_CarriedPart → HideFirstPersonPart)
```

### 설치 시 (서버)
```
ADRCharacter::InstallCarriedPart()
  → ADRCleanserPart::InstallPart()
    → HideFirstPersonPart()                  ← 1P 부품 숨김
    → Destroy()
  → CarriedPart = nullptr (리플리케이트 → OnRep_CarriedPart → HideFirstPersonPart)
```

---

## 주의 사항

1. **OwnerNoSee의 Owner**: `AttachToComponent`로 부착하면 부품 액터의 Owner가 자동으로 설정되지 않음. `PartMesh->SetOwnerNoSee(true)`가 올바르게 동작하려면 부품 액터의 Owner를 캐릭터(또는 캐릭터의 Owner인 PlayerController)로 설정해야 할 수 있음. 필요 시 `PickupPart()`에서 `SetOwner(Character)` 또는 `SetOwner(Character->GetOwner())` 호출 추가.

2. **리플리케이션 타이밍**: `OnRep_bIsCarried`와 `OnRep_CarriedPart`는 독립적으로 도착할 수 있음. 두 콜백 모두에서 1P 메시를 처리하여 어느 것이 먼저 도착하든 정상 동작하도록 함.

3. **1P 메시 소켓**: 1P 스켈레톤에 `TestPartHand` 소켓이 없으면 부품이 보이지 않음. 반드시 언리얼 에디터에서 1P 스켈레톤에 해당 소켓을 추가해야 함.

4. **StaticMesh 참조**: `FirstPersonPartMesh`의 StaticMesh는 `ADRCleanserPart`의 `PartMesh`에서 동적으로 복사하므로, 부품마다 다른 메시를 사용해도 자동으로 대응됨.

5. **부품 액터 Owner 설정**: `SetOwnerNoSee`가 동작하려면 해당 컴포넌트가 속한 액터(또는 부모 액터)에 올바른 Owner가 설정되어 있어야 함. `ADRCleanserPart::PickupPart()`에서 `SetOwner(Character->GetOwner())`를 호출하고, `DropFromCarrier()`에서 `SetOwner(nullptr)`로 복원해야 함. 이것이 가장 중요한 포인트.
