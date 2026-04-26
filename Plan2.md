# Plan2: 클렌저 부품 설치 시 메시 시각화 구현 계획

## 목표

클렌저 부품을 클렌저 사이트에 설치할 때, 부품이 단순히 `Destroy()`되어 사라지는 대신 클렌저 사이트의 **지정된 위치에 미리 배치해둔 부품 메시가 보이도록** 한다.

- 1번째 부품 설치 → `InstalledPartMesh1` **Visible**
- 2번째 부품 설치 → `InstalledPartMesh2` **Visible**

## 접근 방식: 사전 배치 + Visibility 토글

블루프린트에서 미리 부품 메시(StaticMeshComponent)를 원하는 위치에 배치해두고 `Visibility(false)`로 숨겨놓는다.
부품이 설치되면 해당 슬롯의 `SetVisibility(true)`만 호출한다.

**장점**:
- 런타임에 메시를 복사/전달할 필요 없음
- 블루프린트 에디터에서 위치/회전/스케일/메시를 자유롭게 설정 가능
- 리플리케이션이 단순함 (`InstalledPartsCount`의 OnRep에서 Visibility만 복원)
- `SetStaticMesh()` 런타임 호출 불필요

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
      → HideFirstPersonPart()
      → State_Carrying 태그 제거
      → Destroy()  ← 부품 액터 완전 제거 (시각적 피드백 없음)
  → InstalledPartsCount++
  → OnPartInstalled 브로드캐스트
```

### 핵심 문제
- `ADRCleanserPart::InstallPart()`에서 `Destroy()`가 호출되어 부품 액터가 완전히 사라짐
- `ADRCleanserSite`에 부품 메시를 표시할 컴포넌트가 없음
- 설치 후 시각적 피드백이 사운드뿐

### 관련 파일
| 파일 | 역할 |
|------|------|
| `Source/DaeRune/Public/Actor/DRCleanserSite.h` | 클렌저 사이트 헤더 |
| `Source/DaeRune/Private/Actor/DRCleanserSite.cpp` | 클렌저 사이트 구현 |

---

## 구현 계획

### Step 1: ADRCleanserSite에 부품 메시 슬롯 컴포넌트 추가

**파일**: `DRCleanserSite.h` - Components 섹션 (159줄 부근)

```cpp
// ========== 설치된 부품 메시 슬롯 ==========

// 1번째 부품이 표시될 메시 컴포넌트 (블루프린트에서 메시/위치 설정)
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|InstalledParts")
TObjectPtr<UStaticMeshComponent> InstalledPartMesh1;

// 2번째 부품이 표시될 메시 컴포넌트 (블루프린트에서 메시/위치 설정)
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|InstalledParts")
TObjectPtr<UStaticMeshComponent> InstalledPartMesh2;
```

**설계 근거**:
- 블루프린트 에디터에서 원하는 메시를 직접 설정하고, 위치/회전을 시각적으로 조절 가능
- 게임 시작 시 `SetVisibility(false)`, 설치 시 `SetVisibility(true)` — 이것이 전부
- `RequiredPartsCount`가 기본 2이므로 2개의 고정 슬롯으로 충분

### Step 2: ADRCleanserSite 생성자에서 슬롯 컴포넌트 초기화

**파일**: `DRCleanserSite.cpp` - 생성자 (22~80줄), HealthBar 생성 이후에 추가

```cpp
// 1번 부품 슬롯 생성 (메시와 위치는 블루프린트에서 설정)
InstalledPartMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InstalledPartMesh1"));
InstalledPartMesh1->SetupAttachment(CleanserMesh);
InstalledPartMesh1->SetVisibility(false);
InstalledPartMesh1->SetCollisionEnabled(ECollisionEnabled::NoCollision);

// 2번 부품 슬롯 생성
InstalledPartMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InstalledPartMesh2"));
InstalledPartMesh2->SetupAttachment(CleanserMesh);
InstalledPartMesh2->SetVisibility(false);
InstalledPartMesh2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
```

**주의사항**:
- `CleanserMesh`에 부착 → 클렌저 사이트 전체 이동/회전 시 부품도 따라감
- **메시(StaticMesh)는 코드에서 설정하지 않음** → 블루프린트 에디터에서 직접 지정
- **위치(RelativeLocation/Rotation)도 블루프린트에서 설정** → 사이트마다 다른 위치 가능

### Step 3: ADRCleanserSite::InstallPart()에 Visibility 토글 추가

**파일**: `DRCleanserSite.cpp` - `InstallPart()` (175~208줄)

현재 코드:
```cpp
void ADRCleanserSite::InstallPart(ADRCharacter* Character)
{
    if (!HasAuthority() || !Character) return;
    if (CurrentState != ECleanserSiteState::Active) return;
    if (InstalledPartsCount >= RequiredPartsCount) return;
    if (!Character->IsCarryingPart()) return;

    Character->InstallCarriedPart();
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

수정 후:
```cpp
void ADRCleanserSite::InstallPart(ADRCharacter* Character)
{
    if (!HasAuthority() || !Character) return;
    if (CurrentState != ECleanserSiteState::Active) return;
    if (InstalledPartsCount >= RequiredPartsCount) return;
    if (!Character->IsCarryingPart()) return;

    Character->InstallCarriedPart();

    // ★ 설치된 슬롯의 부품 메시 표시 (Multicast로 모든 클라이언트 동기화)
    MulticastShowInstalledPart(InstalledPartsCount);

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

**핵심**: `InstalledPartsCount++` **전에** `MulticastShowInstalledPart`를 호출하므로:
- 카운트 0일 때 → `InstalledPartMesh1` Visible
- 카운트 1일 때 → `InstalledPartMesh2` Visible

### Step 4: 멀티캐스트 RPC 선언 및 구현

**파일**: `DRCleanserSite.h` - public 섹션 (MulticastPlayInstallSound 근처)

```cpp
// 부품 설치 시 해당 슬롯 메시를 모든 클라이언트에서 표시
UFUNCTION(NetMulticast, Reliable)
void MulticastShowInstalledPart(int32 SlotIndex);
```

**파일**: `DRCleanserSite.cpp`

```cpp
void ADRCleanserSite::MulticastShowInstalledPart_Implementation(int32 SlotIndex)
{
    UStaticMeshComponent* TargetSlot = nullptr;

    switch (SlotIndex)
    {
    case 0:
        TargetSlot = InstalledPartMesh1;
        break;
    case 1:
        TargetSlot = InstalledPartMesh2;
        break;
    default:
        return;
    }

    if (TargetSlot)
    {
        TargetSlot->SetVisibility(true);
    }
}
```

**설계 포인트**:
- `NetMulticast, Reliable` → 서버 + 모든 클라이언트에서 실행됨 (서버도 포함)
- 서버 자신도 Multicast RPC를 받으므로, 서버에서 별도로 Visibility 설정할 필요 없음
- 메시는 블루프린트에서 미리 설정되어 있으므로 `SetStaticMesh()` 호출 불필요

### Step 5: 레이트 조인 대응 - OnRep_InstalledPartsCount 수정

늦게 접속한 클라이언트는 Multicast RPC를 받지 못했으므로, `InstalledPartsCount`의 리플리케이션 콜백에서 복원한다.

**파일**: `DRCleanserSite.h` - 218줄

현재: `static void OnRep_InstalledPartsCount();` → **static 제거 필수**

```cpp
UFUNCTION()
void OnRep_InstalledPartsCount();
```

**파일**: `DRCleanserSite.cpp` - 370줄

현재 비어있는 함수를 수정:

```cpp
void ADRCleanserSite::OnRep_InstalledPartsCount()
{
    // 레이트 조인 클라이언트를 위한 부품 메시 Visibility 복원
    if (InstalledPartMesh1)
    {
        InstalledPartMesh1->SetVisibility(InstalledPartsCount >= 1);
    }
    if (InstalledPartMesh2)
    {
        InstalledPartMesh2->SetVisibility(InstalledPartsCount >= 2);
    }
}
```

**핵심**: `InstalledPartsCount`는 이미 `ReplicatedUsing = OnRep_InstalledPartsCount`로 리플리케이트되고 있으므로, 추가 리플리케이트 변수가 필요 없다. 카운트 값만으로 어떤 슬롯이 Visible이어야 하는지 결정 가능.

### Step 6: UpdateMeshByState()에서 부품 메시 처리

**파일**: `DRCleanserSite.cpp` - `UpdateMeshByState()` (397~422줄)

Inactive 상태에서는 부품 메시도 숨기고, 다른 상태에서는 카운트 기반으로 복원:

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
        // 이미 설치된 부품 복원 (레이트 조인 대비)
        if (InstalledPartMesh1) InstalledPartMesh1->SetVisibility(InstalledPartsCount >= 1);
        if (InstalledPartMesh2) InstalledPartMesh2->SetVisibility(InstalledPartsCount >= 2);
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
        // 모든 부품 슬롯 표시 (이 상태면 2개 모두 설치 완료)
        if (InstalledPartMesh1) InstalledPartMesh1->SetVisibility(true);
        if (InstalledPartMesh2) InstalledPartMesh2->SetVisibility(true);
        break;
    }
}
```

---

## 변경 파일 요약

| 파일 | 변경 내용 |
|------|----------|
| `DRCleanserSite.h` | `InstalledPartMesh1/2` 컴포넌트 선언 추가, `MulticastShowInstalledPart()` 선언 추가, `OnRep_InstalledPartsCount`에서 `static` 제거 |
| `DRCleanserSite.cpp` | 생성자에 슬롯 컴포넌트 초기화, `InstallPart()`에 `MulticastShowInstalledPart()` 호출 추가, `MulticastShowInstalledPart_Implementation()` 구현, `OnRep_InstalledPartsCount()` 구현 (Visibility 복원), `UpdateMeshByState()` 수정 (부품 슬롯 Visibility 처리) |

**이전 계획 대비 제거된 것들** (불필요해짐):
- ~~`InstalledPartMeshes` 리플리케이트 배열~~ → `InstalledPartsCount`로 충분
- ~~`OnRep_InstalledPartMeshes()`~~ → 불필요
- ~~`AttachPartMeshToSlot()` 함수~~ → Multicast에서 직접 처리
- ~~`GetPartMesh()` 접근자~~ → 이미 추가되어 있지만 이 기능에서는 사용 안 함
- ~~`GetCarriedPart()` 접근자~~ → 부품 메시를 런타임에 복사하지 않으므로 불필요
- ~~`SetStaticMesh()` 런타임 호출~~ → 블루프린트에서 미리 설정

---

## 멀티플레이어 동기화 전략

```
[서버]
InstallPart() 호출
  → MulticastShowInstalledPart(SlotIndex)  ← 서버+모든 클라이언트에서 실행
  → InstalledPartsCount++ (리플리케이트됨)

[기존 연결 클라이언트]
  → MulticastShowInstalledPart RPC 수신 → 해당 슬롯 SetVisibility(true)
  (+ OnRep_InstalledPartsCount도 이후 도착 → 동일 결과)

[레이트 조인 클라이언트]
  → Multicast RPC 수신 못함
  → OnRep_InstalledPartsCount() 호출됨 → 카운트 기반으로 Visibility 복원
  → OnRep_CurrentState() → UpdateMeshByState()에서도 복원
```

**이중 안전장치**: Multicast RPC (즉시 반영) + OnRep (레이트 조인/재접속 대응)

---

## 데이터 흐름 다이어그램

```
ADRCleanserSite::InstallPart(Character)
  ├── Character->InstallCarriedPart()
  │     → Part->InstallPart() → HideFirstPersonPart() → Destroy()
  ├── MulticastShowInstalledPart(InstalledPartsCount)
  │     → [서버+클라이언트] InstalledPartMesh1 or 2 → SetVisibility(true)
  ├── InstalledPartsCount++
  │     → [리플리케이트] OnRep_InstalledPartsCount()
  │           → InstalledPartMesh1->SetVisibility(Count >= 1)
  │           → InstalledPartMesh2->SetVisibility(Count >= 2)
  └── OnPartInstalled 브로드캐스트
```

---

## 블루프린트 작업 (코드 완료 후)

### 부품 슬롯 설정 순서
1. 각 CleanserSite 블루프린트 (`BP_DRCleanserSite` 등)를 연다
2. Components 패널에서 `InstalledPartMesh1` 선택
3. **Static Mesh** 속성에서 원하는 부품 메시 에셋 설정
4. **Transform** (Relative Location / Rotation / Scale) 조절하여 1번 부품 위치 설정
5. `InstalledPartMesh2`도 동일하게 설정
6. 에디터에서 미리보기 시 두 메시가 원하는 위치에 보이는지 확인
7. **게임 시작 시 자동으로 숨겨짐** (코드에서 `SetVisibility(false)` 처리)

### 사이트별 다른 배치
- 각 사이트 블루프린트 인스턴스마다 다른 위치/메시를 설정할 수 있음
- 필요하면 동일 메시를 사용하되 위치만 다르게 설정

---

## 주의사항

1. **`OnRep_InstalledPartsCount`의 `static` 키워드**: 현재 `static`으로 선언되어 있어 멤버 변수 접근 불가. **반드시 `static` 제거 필요**
2. **Multicast 호출 타이밍**: `InstalledPartsCount++` **이전에** 호출해야 SlotIndex가 올바름 (0→1번, 1→2번)
3. **Visibility 중복 설정 안전**: `SetVisibility(true)`를 여러 번 호출해도 문제 없음 (Multicast + OnRep 이중 호출 가능)
4. **블루프린트에서 메시 미설정 시**: `SetVisibility(true)` 호출되어도 메시가 없으면 아무것도 안 보임 — 정상 동작, 에러 아님
