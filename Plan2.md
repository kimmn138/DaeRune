# Plan2: 클렌저 부품 설치 시 메시 시각화 구현 계획

## 목표

클렌저 부품을 클렌저 사이트에 설치할 때, 부품이 단순히 `Destroy()`되어 사라지는 대신 클렌저 사이트의 **지정된 위치에 부품 메시가 실제로 부착**되어 시각적으로 표시되도록 한다.

- 1번째 부품 → 클렌저 사이트의 **PartSlot1** 위치에 부착
- 2번째 부품 → 클렌저 사이트의 **PartSlot2** 위치에 부착

---

## 현재 시스템 분석

### 현재 설치 흐름
```
Player가 부품 들고 InteractionBox 진입
→ Interact 키 입력
→ ServerRequestInstallPartToSite(Site) [Server RPC]
→ ADRCleanserSite::InstallPart(Character)
  → Character->InstallCarriedPart()
    → CarriedPart->InstallPart()
      → State_Carrying 태그 제거
      → Destroy()  ← 부품 액터 완전 제거 (시각적 피드백 없음)
  → InstalledPartsCount++
  → OnPartInstalled 브로드캐스트
```

### 핵심 문제
- `ADRCleanserPart::InstallPart()`에서 `Destroy()`가 호출되어 부품 액터가 완전히 사라짐
- `ADRCleanserSite`에 부품 메시를 표시할 컴포넌트나 소켓이 없음
- 설치된 부품의 메시 정보가 `CleanserSite`에 전달되지 않음

### 관련 파일
| 파일 | 역할 |
|------|------|
| `Source/DaeRune/Public/Actor/DRCleanserSite.h` | 클렌저 사이트 헤더 |
| `Source/DaeRune/Private/Actor/DRCleanserSite.cpp` | 클렌저 사이트 구현 |
| `Source/DaeRune/Public/Actor/DRCleanserPart.h` | 클렌저 부품 헤더 |
| `Source/DaeRune/Private/Actor/DRCleanserPart.cpp` | 클렌저 부품 구현 |
| `Source/DaeRune/Public/Character/DRCharacter.h` | 플레이어 캐릭터 (InstallCarriedPart) |
| `Source/DaeRune/Private/Character/DRCharacter.cpp` | 플레이어 캐릭터 구현 |

---

## 구현 계획

### Step 1: ADRCleanserSite에 부품 메시 슬롯 컴포넌트 추가

**파일**: `DRCleanserSite.h`

클렌저 사이트에 부품이 설치될 위치를 나타내는 `UStaticMeshComponent`를 2개 추가한다.

```cpp
// ========== 설치된 부품 메시 슬롯 ==========

// 1번째 부품이 표시될 메시 컴포넌트
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|InstalledParts")
TObjectPtr<UStaticMeshComponent> InstalledPartMesh1;

// 2번째 부품이 표시될 메시 컴포넌트
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|InstalledParts")
TObjectPtr<UStaticMeshComponent> InstalledPartMesh2;
```

**설계 근거**:
- `USceneComponent`(소켓) 방식 대신 `UStaticMeshComponent`를 직접 사용하는 이유: 블루프린트 에디터에서 위치/회전/스케일을 시각적으로 조절 가능하며, 설치 전에는 `SetVisibility(false)`로 숨기고 설치 시 `SetVisibility(true)`로 표시하는 간단한 방식
- `RequiredPartsCount`가 기본 2로 고정되어 있으므로 2개의 고정 슬롯으로 충분

### Step 2: ADRCleanserSite 생성자에서 슬롯 컴포넌트 초기화

**파일**: `DRCleanserSite.cpp` - 생성자

```cpp
// 1번 부품 슬롯 생성
InstalledPartMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InstalledPartMesh1"));
InstalledPartMesh1->SetupAttachment(CleanserMesh);  // CleanserMesh에 부착
InstalledPartMesh1->SetVisibility(false);             // 초기에는 숨김
InstalledPartMesh1->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // 충돌 불필요
InstalledPartMesh1->SetIsReplicated(true);            // 클라이언트에서도 보이도록

// 2번 부품 슬롯 생성
InstalledPartMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InstalledPartMesh2"));
InstalledPartMesh2->SetupAttachment(CleanserMesh);
InstalledPartMesh2->SetVisibility(false);
InstalledPartMesh2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
InstalledPartMesh2->SetIsReplicated(true);
```

**주의사항**:
- `CleanserMesh`에 부착하여 클렌저 사이트가 움직이거나 회전해도 부품 위치가 함께 따라감
- 실제 위치(RelativeLocation, RelativeRotation)는 **블루프린트에서 조절**함 (코드에서 하드코딩하지 않음)
- 블루프린트 서브클래스(`BP_DRCleanserSite` 등)에서 각 사이트별로 다른 위치에 배치 가능

### Step 3: 부품 메시를 CleanserSite에 전달하는 구조 변경

현재 `InstallPart(ADRCharacter*)` → `Character->InstallCarriedPart()` → `CarriedPart->InstallPart()` → `Destroy()` 흐름에서, 부품의 메시 정보를 CleanserSite가 받아야 한다.

#### 3-1. ADRCleanserSite::InstallPart() 수정

**파일**: `DRCleanserSite.cpp` 175~208줄

현재 코드:
```cpp
void ADRCleanserSite::InstallPart(ADRCharacter* Character)
{
    if (!HasAuthority() || !Character) return;
    if (CurrentState != ECleanserSiteState::Active) return;
    if (InstalledPartsCount >= RequiredPartsCount) return;
    if (!Character->IsCarryingPart()) return;

    // 부품 설치 처리
    Character->InstallCarriedPart();

    // 설치 카운트 증가
    InstalledPartsCount++;
    // ...
}
```

수정 후:
```cpp
void ADRCleanserSite::InstallPart(ADRCharacter* Character)
{
    if (!HasAuthority() || !Character) return;
    if (CurrentState != ECleanserSiteState::Active) return;
    if (InstalledPartsCount >= RequiredPartsCount) return;
    if (!Character->IsCarryingPart()) return;

    // ★ 부품의 메시 정보를 먼저 가져온다 (Destroy 전에!)
    ADRCleanserPart* Part = Character->GetCarriedPart();
    UStaticMesh* PartStaticMesh = nullptr;
    if (Part)
    {
        UStaticMeshComponent* PartMeshComp = Part->GetPartMesh();
        if (PartMeshComp)
        {
            PartStaticMesh = PartMeshComp->GetStaticMesh();
        }
    }

    // 부품 설치 처리 (여기서 Part가 Destroy됨)
    Character->InstallCarriedPart();

    // ★ 설치된 슬롯에 메시 표시
    AttachPartMeshToSlot(InstalledPartsCount, PartStaticMesh);

    // 설치 카운트 증가
    InstalledPartsCount++;

    bool bIsComplete = (InstalledPartsCount >= RequiredPartsCount);
    MulticastPlayInstallSound(bIsComplete);

    UpdateInteractionUI();
    OnPartInstalled.Broadcast(this);

    if (InstalledPartsCount >= RequiredPartsCount)
    {
        SetPartsCollected();
    }
}
```

**핵심 포인트**: `InstalledPartsCount`가 증가하기 **전에** `AttachPartMeshToSlot`을 호출하므로, 현재 카운트 값이 슬롯 인덱스로 사용됨 (0 → 1번 슬롯, 1 → 2번 슬롯).

#### 3-2. ADRCleanserPart에 메시 접근자 추가

**파일**: `DRCleanserPart.h`

```cpp
public:
    // 부품의 StaticMeshComponent 접근자
    UFUNCTION(BlueprintCallable, Category = "CleanserPart")
    UStaticMeshComponent* GetPartMesh() const { return PartMesh; }
```

#### 3-3. ADRCharacter에 CarriedPart 접근자 확인/추가

**파일**: `DRCharacter.h`

이미 `IsCarryingPart()`는 있으므로, `GetCarriedPart()` 접근자가 필요하다:

```cpp
public:
    UFUNCTION(BlueprintCallable, Category = "Character|Part")
    ADRCleanserPart* GetCarriedPart() const { return CarriedPart; }
```

(이미 존재하면 이 단계는 생략)

### Step 4: 부품 메시 슬롯 부착 함수 구현

**파일**: `DRCleanserSite.h`에 선언 추가

```cpp
protected:
    // 설치된 부품 메시를 해당 슬롯에 표시
    void AttachPartMeshToSlot(int32 SlotIndex, UStaticMesh* PartStaticMesh);

    // 멀티캐스트 RPC: 클라이언트에 부품 메시 표시 동기화
    UFUNCTION(NetMulticast, Reliable)
    void MulticastShowInstalledPart(int32 SlotIndex, UStaticMesh* PartStaticMesh);
```

**파일**: `DRCleanserSite.cpp`에 구현 추가

```cpp
void ADRCleanserSite::AttachPartMeshToSlot(int32 SlotIndex, UStaticMesh* PartStaticMesh)
{
    if (!HasAuthority()) return;

    // 슬롯에 해당하는 메시 컴포넌트 선택
    UStaticMeshComponent* TargetSlot = (SlotIndex == 0) ? InstalledPartMesh1 : InstalledPartMesh2;
    if (!TargetSlot) return;

    // 메시 설정 및 표시
    if (PartStaticMesh)
    {
        TargetSlot->SetStaticMesh(PartStaticMesh);
    }
    TargetSlot->SetVisibility(true);

    // 모든 클라이언트에 동기화
    MulticastShowInstalledPart(SlotIndex, PartStaticMesh);
}

void ADRCleanserSite::MulticastShowInstalledPart_Implementation(int32 SlotIndex, UStaticMesh* PartStaticMesh)
{
    // 서버는 이미 처리했으므로 클라이언트만
    if (HasAuthority()) return;

    UStaticMeshComponent* TargetSlot = (SlotIndex == 0) ? InstalledPartMesh1 : InstalledPartMesh2;
    if (!TargetSlot) return;

    if (PartStaticMesh)
    {
        TargetSlot->SetStaticMesh(PartStaticMesh);
    }
    TargetSlot->SetVisibility(true);
}
```

### Step 5: 상태 복원 처리 (레이트 조인 / OnRep)

클라이언트가 늦게 접속하거나 리플리케이션이 늦게 도착하는 경우를 위해, `OnRep_InstalledPartsCount`에서 슬롯의 가시성을 동기화한다.

**방법 A: 설치된 메시 정보를 리플리케이트** (권장)

설치된 부품의 StaticMesh를 리플리케이트 변수로 저장:

**파일**: `DRCleanserSite.h`

```cpp
protected:
    // 설치된 부품 메시 레퍼런스 (리플리케이션용)
    UPROPERTY(ReplicatedUsing = OnRep_InstalledPartMeshes)
    TArray<TObjectPtr<UStaticMesh>> InstalledPartMeshes;

    UFUNCTION()
    void OnRep_InstalledPartMeshes();
```

**파일**: `DRCleanserSite.cpp`

```cpp
// GetLifetimeReplicatedProps에 추가
DOREPLIFETIME(ADRCleanserSite, InstalledPartMeshes);
```

```cpp
void ADRCleanserSite::OnRep_InstalledPartMeshes()
{
    // 레이트 조인 클라이언트를 위해 설치된 부품 메시 복원
    for (int32 i = 0; i < InstalledPartMeshes.Num(); ++i)
    {
        UStaticMeshComponent* TargetSlot = (i == 0) ? InstalledPartMesh1 : InstalledPartMesh2;
        if (TargetSlot && InstalledPartMeshes[i])
        {
            TargetSlot->SetStaticMesh(InstalledPartMeshes[i]);
            TargetSlot->SetVisibility(true);
        }
    }
}
```

그리고 `AttachPartMeshToSlot()`에서 배열에도 저장:

```cpp
void ADRCleanserSite::AttachPartMeshToSlot(int32 SlotIndex, UStaticMesh* PartStaticMesh)
{
    if (!HasAuthority()) return;

    UStaticMeshComponent* TargetSlot = (SlotIndex == 0) ? InstalledPartMesh1 : InstalledPartMesh2;
    if (!TargetSlot) return;

    if (PartStaticMesh)
    {
        TargetSlot->SetStaticMesh(PartStaticMesh);
    }
    TargetSlot->SetVisibility(true);

    // 리플리케이션용 배열에 저장
    if (InstalledPartMeshes.Num() <= SlotIndex)
    {
        InstalledPartMeshes.SetNum(SlotIndex + 1);
    }
    InstalledPartMeshes[SlotIndex] = PartStaticMesh;

    // 즉시 동기화를 위한 멀티캐스트
    MulticastShowInstalledPart(SlotIndex, PartStaticMesh);
}
```

### Step 6: 기존 OnRep_InstalledPartsCount 수정

**파일**: `DRCleanserSite.h` 218줄

현재 `static void OnRep_InstalledPartsCount();`로 되어 있는데, `static`을 제거해야 멤버 변수에 접근 가능:

```cpp
UFUNCTION()
void OnRep_InstalledPartsCount();  // static 제거
```

**파일**: `DRCleanserSite.cpp` 370줄

현재 비어있는 `OnRep_InstalledPartsCount`를 업데이트:

```cpp
void ADRCleanserSite::OnRep_InstalledPartsCount()
{
    // InstalledPartMeshes의 OnRep에서 메시 복원을 처리하므로
    // 여기서는 UI 관련 업데이트만 수행
    UpdateInteractionUI();
}
```

### Step 7: UpdateMeshByState()에서 설치된 부품 메시 처리

**파일**: `DRCleanserSite.cpp` 397~422줄

Inactive 상태로 전환될 때 설치된 부품 메시도 숨기도록 수정:

```cpp
void ADRCleanserSite::UpdateMeshByState()
{
    switch (CurrentState)
    {
    case ECleanserSiteState::Inactive:
        if (CleanserMesh) CleanserMesh->SetVisibility(false);
        if (WaterMesh) WaterMesh->SetVisibility(false);
        // 설치된 부품도 숨김
        if (InstalledPartMesh1) InstalledPartMesh1->SetVisibility(false);
        if (InstalledPartMesh2) InstalledPartMesh2->SetVisibility(false);
        break;

    case ECleanserSiteState::Active:
        if (CleanserMesh) CleanserMesh->SetVisibility(true);
        if (WaterMesh) WaterMesh->SetVisibility(true);
        // Active 상태에서 이미 설치된 부품이 있으면 표시 (레이트 조인 대비)
        // InstalledPartMeshes 배열 기반으로 복원
        for (int32 i = 0; i < InstalledPartMeshes.Num(); ++i)
        {
            UStaticMeshComponent* Slot = (i == 0) ? InstalledPartMesh1 : InstalledPartMesh2;
            if (Slot && InstalledPartMeshes[i])
            {
                Slot->SetStaticMesh(InstalledPartMeshes[i]);
                Slot->SetVisibility(true);
            }
        }
        break;

    case ECleanserSiteState::PartsCollected:
    case ECleanserSiteState::Operational:
    case ECleanserSiteState::Completed:
        if (CleanserMesh) CleanserMesh->SetVisibility(true);
        if (WaterMesh && WaterMesh_AfterParts)
        {
            WaterMesh->SetStaticMesh(WaterMesh_AfterParts);
            WaterMesh->SetVisibility(true);
        }
        // 모든 부품 슬롯 표시
        if (InstalledPartMesh1) InstalledPartMesh1->SetVisibility(true);
        if (InstalledPartMesh2) InstalledPartMesh2->SetVisibility(true);
        break;
    }
}
```

---

## 블루프린트 작업 (코드 완료 후)

### 부품 슬롯 위치 설정
1. `BP_DRBreakableDoor_Room1to2` 등 각 CleanserSite 블루프린트를 열기
2. `InstalledPartMesh1` 컴포넌트의 **Relative Location/Rotation** 조절하여 1번 부품 위치 설정
3. `InstalledPartMesh2` 컴포넌트의 **Relative Location/Rotation** 조절하여 2번 부품 위치 설정
4. 필요 시 **기본 메시(Default Static Mesh)** 를 설정하여 에디터에서 미리보기 가능 (게임 시작 시 `SetVisibility(false)`로 숨겨짐)

### 부품 메시 설정 확인
- `ADRCleanserPart`의 `PartMesh` 컴포넌트에 실제 StaticMesh가 설정되어 있는지 확인
- 적 캐릭터의 `PartMeshComponent`에 설정된 메시와 동일해야 시각적 일관성 유지

---

## 변경 파일 요약

| 파일 | 변경 내용 |
|------|----------|
| `DRCleanserSite.h` | `InstalledPartMesh1/2` 컴포넌트 추가, `InstalledPartMeshes` 리플리케이트 배열 추가, `AttachPartMeshToSlot()` 선언, `MulticastShowInstalledPart()` 선언, `OnRep_InstalledPartMeshes()` 선언, `OnRep_InstalledPartsCount` static 제거 |
| `DRCleanserSite.cpp` | 생성자에 슬롯 컴포넌트 초기화, `InstallPart()` 수정 (메시 추출 → 슬롯 부착), `AttachPartMeshToSlot()` 구현, `MulticastShowInstalledPart()` 구현, `OnRep_InstalledPartMeshes()` 구현, `OnRep_InstalledPartsCount()` 수정, `UpdateMeshByState()` 수정, `GetLifetimeReplicatedProps` 업데이트 |
| `DRCleanserPart.h` | `GetPartMesh()` 접근자 추가 |
| `DRCharacter.h` | `GetCarriedPart()` 접근자 추가 (없는 경우) |

---

## 멀티플레이어 동기화 전략

```
[서버]
InstallPart() 호출
  → Part에서 StaticMesh 포인터 추출
  → AttachPartMeshToSlot(): 슬롯 메시 설정 + Visible
  → InstalledPartMeshes 배열에 저장 (리플리케이트됨)
  → MulticastShowInstalledPart(): 연결된 모든 클라이언트에 즉시 전파

[기존 연결 클라이언트]
  → MulticastShowInstalledPart RPC 수신
  → 해당 슬롯의 메시 설정 + Visible

[레이트 조인 클라이언트]
  → OnRep_InstalledPartMeshes() 호출됨
  → InstalledPartMeshes 배열 기반으로 슬롯 메시 복원
  → OnRep_CurrentState() → UpdateMeshByState()에서도 슬롯 가시성 복원
```

---

## 데이터 흐름 다이어그램

```
ADRCleanserPart (Destroy 전)
  └── PartMesh (UStaticMeshComponent)
        └── GetStaticMesh() → UStaticMesh*  ─────────┐
                                                      │
ADRCleanserSite::InstallPart()                        │
  ├── Character->GetCarriedPart()->GetPartMesh()  ←───┘
  │     → PartStaticMesh 추출
  ├── Character->InstallCarriedPart()
  │     → Part->InstallPart() → Destroy()
  ├── AttachPartMeshToSlot(SlotIndex, PartStaticMesh)
  │     ├── InstalledPartMesh1 or 2에 SetStaticMesh()
  │     ├── SetVisibility(true)
  │     ├── InstalledPartMeshes 배열에 저장
  │     └── MulticastShowInstalledPart() → 클라이언트 동기화
  └── InstalledPartsCount++
```

---

## 주의사항 및 엣지 케이스

1. **메시 추출 타이밍**: 반드시 `Character->InstallCarriedPart()` (Destroy) 호출 **전에** 메시 포인터를 추출해야 함
2. **nullptr 안전성**: 부품에 StaticMesh가 설정되지 않은 경우에도 `SetVisibility(true)`는 호출하되, `SetStaticMesh(nullptr)` 호출은 건너뜀
3. **SlotIndex 범위**: `InstalledPartsCount`가 `RequiredPartsCount`(2) 미만일 때만 `InstallPart`가 실행되므로 SlotIndex는 항상 0 또는 1
4. **UStaticMesh 리플리케이션**: `UStaticMesh*`는 UObject이므로 네트워크 리플리케이션 시 자산 경로로 직렬화됨. 모든 클라이언트에 해당 에셋이 존재해야 함 (동일 빌드이므로 문제 없음)
5. **OnRep_InstalledPartsCount의 static 키워드**: 현재 `static`으로 선언되어 있어 멤버 변수 접근 불가. 반드시 `static` 제거 필요
