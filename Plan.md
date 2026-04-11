# Plan: 메인 메뉴 리뉴얼 + 대기실 시스템 - 구현 계획

---

## 구현 개요

총 9개의 구현 페이즈로 나뉘며, 각 페이즈는 이전 페이즈 위에 쌓이고 독립적으로 테스트 가능하다.

```
Phase 1: 로비 상태 머신 (ELobbyState) ──────────────── [기반]
    ↓
Phase 2: 캐릭터 클래스 선택 (PlayerState 확장) ─────── [데이터 레이어]
    ↓
Phase 3: 대기실 카메라 + 플레이어 배치 ────────────── [비주얼]
    ↓
Phase 4: 대기실 UI (위젯 + RPC 바인딩) ──────────── [인터랙션]
    ↓
Phase 5: 킥(Kick) 시스템 ──────────────────────── [기능]
    ↓
Phase 6: Power On 전환 (카메라 연출 + 상태 전환) ──── [핵심 전환]
    ↓
Phase 7: 캐릭터 BP 교체 (클래스 선택 → 외형/능력 변경) ── [비주얼 피드백]
    ↓
Phase 8: 메인 메뉴 리뉴얼 ──────────────────────── [독립 - Phase 1 이후 언제든]
    ↓
Phase 9: 스테이지 복귀 → 대기실 ──────────────────── [통합 테스트]
```

---

## Phase 1: 로비 상태 머신 (ELobbyState)

### 목표
로비 맵이 WaitingRoom / FreeRoam 두 가지 상태를 가지도록 하는 **기반 인프라** 구축.

### 1.1 새 파일 생성: `DRLobbyTypes.h`

**경로:** `Source/DaeRune/Public/Game/DRLobbyTypes.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "DRLobbyTypes.generated.h"

// 로비의 상태를 나타내는 열거형
UENUM(BlueprintType)
enum class ELobbyState : uint8
{
    WaitingRoom,     // 대기실 상태 (고정 카메라, UI Only, 캐릭터 조작 불가)
    Transitioning,   // Power On 후 카메라 전환 중
    FreeRoam         // 자유 조작 상태 (기존 로비와 동일)
};
```

**이유:** 별도 헤더로 분리하여 여러 클래스에서 include 시 순환 의존성 방지. 향후 `FWaitingRoomPlayerInfo` 등 로비 관련 타입도 여기에 추가.

### 1.2 ADRLobbyGameState 수정

**파일:** `Source/DaeRune/Public/Game/DRLobbyGameState.h`

**추가할 내용:**
```cpp
#include "Game/DRLobbyTypes.h"

// 기존 FOnRoomCodeGenerated 선언 아래에 추가
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyStateChanged, ELobbyState, NewState);

// 클래스 내부에 추가:

public:
    // 로비 상태 조회
    UFUNCTION(BlueprintCallable, Category = "Lobby|State")
    ELobbyState GetLobbyState() const { return LobbyState; }

    // 로비 상태 설정 (서버 전용)
    void SetLobbyState(ELobbyState NewState);

    // 로비 상태 변경 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Lobby")
    FOnLobbyStateChanged OnLobbyStateChanged;

protected:
    // 리플리케이트되는 로비 상태
    UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Lobby")
    ELobbyState LobbyState = ELobbyState::WaitingRoom;

    UFUNCTION()
    void OnRep_LobbyState();
```

**파일:** `Source/DaeRune/Private/Game/DRLobbyGameState.cpp`

**추가할 내용:**
```cpp
// GetLifetimeReplicatedProps에 추가:
DOREPLIFETIME(ADRLobbyGameState, LobbyState);

// 새 메서드:
void ADRLobbyGameState::SetLobbyState(ELobbyState NewState)
{
    if (!HasAuthority()) return;
    if (LobbyState == NewState) return;

    LobbyState = NewState;
    // 서버에서 직접 브로드캐스트 (OnRep은 클라이언트에서만 호출됨)
    OnLobbyStateChanged.Broadcast(LobbyState);
}

void ADRLobbyGameState::OnRep_LobbyState()
{
    OnLobbyStateChanged.Broadcast(LobbyState);
}
```

### 1.3 ADRLobbyGameMode에 BlockJoinInProgress 추가

**파일:** `Source/DaeRune/Public/Game/DRLobbyGameMode.h`

**추가할 내용:**
```cpp
public:
    // Power On 처리 (호스트만 호출 가능)
    UFUNCTION(BlueprintCallable, Category = "Lobby")
    void PowerOn(ADRPlayerController* Requester);

    // 플레이어 킥 (호스트만 호출 가능)
    UFUNCTION(BlueprintCallable, Category = "Lobby")
    void KickPlayer(ADRPlayerController* Requester, ADRPlayerController* TargetPlayer);

protected:
    // 세션 참가 차단
    void BlockJoinInProgress();
```

**근거:** 기존 `ADRStageGameMode`에 이미 동일한 `BlockJoinInProgress()` 패턴이 있으므로 그 구현을 그대로 따른다:
```cpp
void ADRLobbyGameMode::BlockJoinInProgress()
{
    if (!HasAuthority()) return;
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance) return;
    UMultiplayerSessionsSubsystem* SessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
    if (SessionsSubsystem)
    {
        SessionsSubsystem->UpdateSessionJoinability(false);
    }
}
```

### 1.4 기존 코드 변경 영향

- `ADRLobbyGameMode::BeginPlay()`는 기존대로 `AllowJoinInProgress()` 호출 → 대기실 시작 시 참가 허용 상태 유지
- `ADRLobbyGameState`의 기본값이 `WaitingRoom`이므로 로비 로드 시 자동으로 대기실 상태

### 검증 방법
PIE 2인 플레이, `LobbyState`를 화면에 출력하여 WaitingRoom 상태 확인. GameState에서 값이 리플리케이트되는지 확인.

---

## Phase 2: 캐릭터 클래스 선택 (PlayerState 확장)

### 목표
각 플레이어가 대기실에서 캐릭터 클래스를 선택하고, 그 선택이 모든 클라이언트에 리플리케이트되는 데이터 레이어 구축.

---

### ⚠️ 2.0 CharacterClass / CharacterClassInfo 현재 사용처 상세 분석

이 분석은 캐릭터 클래스 선택 시스템 구현 전에 **반드시** 이해해야 하는 핵심 내용이다.

#### 2.0.1 데이터 구조

| 요소 | 위치 | 설명 |
|------|------|------|
| `ECharacterClass` 열거형 (적 전용) | `CharacterClassInfo.h` | Elementalist(0), Warrior(1), Ranger(2) |
| `EPlayerCharacterClass` 열거형 (플레이어 전용) | `CharacterClassInfo.h` | GardenRobot(0), VendingMachineRobot(1) |
| `FCharacterClassDefaultInfo` 구조체 | `CharacterClassInfo.h` | PrimaryAttributes (GE), VitalAttributes (GE), StartupAbilities (배열), DeathAbilities (배열) |
| `UCharacterClassInfo` 데이터 에셋 (적 전용) | `CharacterClassInfo.h` | TMap<ECharacterClass, FCharacterClassDefaultInfo> + CommonAbilities 배열 |
| `UPlayerCharacterClassInfo` 데이터 에셋 (플레이어 전용) | `CharacterClassInfo.h` | TMap<EPlayerCharacterClass, FCharacterClassDefaultInfo> + CommonAbilities 배열 |
| 데이터 에셋 참조 | `ADRGameModeBase` | `EnemyCharacterClassInfo` (UCharacterClassInfo*) + `PlayerCharacterClassInfo` (UPlayerCharacterClassInfo*) |

```
UCharacterClassInfo (적 전용 데이터 에셋)
├── CharacterClassInformation (TMap<ECharacterClass, ...>)
│   ├── Elementalist → { PrimaryAttributes GE, VitalAttributes GE, StartupAbilities[], DeathAbilities[] }
│   ├── Warrior      → { PrimaryAttributes GE, VitalAttributes GE, StartupAbilities[], DeathAbilities[] }
│   └── Ranger       → { PrimaryAttributes GE, VitalAttributes GE, StartupAbilities[], DeathAbilities[] }
└── CommonAbilities[] (적 공통 어빌리티)

UPlayerCharacterClassInfo (플레이어 전용 데이터 에셋)
├── CharacterClassInformation (TMap<EPlayerCharacterClass, ...>)
│   ├── GardenRobot         → { PrimaryAttributes GE, VitalAttributes GE, StartupAbilities[], [] }
│   └── VendingMachineRobot → { PrimaryAttributes GE, VitalAttributes GE, StartupAbilities[], [] }
└── CommonAbilities[] (플레이어 공통 어빌리티)
```

#### 2.0.2 CharacterClass 멤버 변수 위치

- `ADRCharacterBase::CharacterClass` (protected, 기본값: `ECharacterClass::Warrior`) → **적 전용으로 유지**
- `ADRCharacter` 생성자에서 `ECharacterClass::Elementalist`로 덮어쓰던 코드 → **삭제** (플레이어에게 `ECharacterClass`는 무관)
- `ADRCharacter`에 새 멤버 추가: `EPlayerCharacterClass PlayerCharacterClass = EPlayerCharacterClass::GardenRobot` (플레이어 전용)
- `ICombatInterface::GetCharacterClass()`를 통해 외부에서 읽기 가능 (적 전용, 플레이어에서는 기본값 반환)

#### 2.0.3 🔴 핵심 발견: 플레이어와 적의 초기화 경로가 완전히 다름

**적 (ADREnemy) - CharacterClassInfo 데이터 에셋을 사용:**

| 사용 위치 | 코드 | 용도 |
|-----------|------|------|
| `ADREnemy::InitializeDefaultAttributes()` | `UDRAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, ASC)` | CharacterClassInfo에서 CharacterClass에 해당하는 PrimaryAttributes + VitalAttributes GE를 찾아 적용 → **적의 MaxHealth, MaxWater, MoveSpeed 등 결정** |
| `ADREnemy::BeginPlay()` | `UDRAbilitySystemLibrary::GiveStartupAbilities(this, ASC, CharacterClass)` | CharacterClassInfo에서 CommonAbilities + 클래스별 StartupAbilities를 찾아 부여 → **적의 전투 어빌리티 결정** |
| `ADREnemy::BeginPlay()` | `CharacterClassInfo->GetClassDefaultInfo(CharacterClass).DeathAbilities` | 클래스별 DeathAbilities를 가져옴 → **적의 사망 시 발동 어빌리티 결정** |
| `ADREnemy::PossessedBy()` | `RangedAttacker = CharacterClass != ECharacterClass::Warrior` | CharacterClass로 AI 행동 패턴 결정 → **Warrior = 근접공격, 나머지 = 원거리공격** |

**적의 초기화 흐름:**
```
ADREnemy::BeginPlay()
  → InitAbilityActorInfo()
      → ASC->InitAbilityActorInfo(this, this)  // 적은 자신이 ASC 소유
      → InitializeDefaultAttributes()  ← ★ 오버라이드됨
          → UDRAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, ASC)
              → CharacterClassInfo->GetClassDefaultInfo(CharacterClass)
              → PrimaryAttributes GE 적용 (MaxHealth, MoveSpeed 등)
              → VitalAttributes GE 적용 (Health, Water 초기값)
  → GiveStartupAbilities(this, ASC, CharacterClass)
      → CharacterClassInfo->CommonAbilities + ClassDefault.StartupAbilities 부여
  → CharacterClassInfo->GetClassDefaultInfo(CharacterClass).DeathAbilities 저장
```

---

**플레이어 (ADRCharacter) - ❌ CharacterClassInfo를 사용하지 않음 (Phase 7에서 `UPlayerCharacterClassInfo` + `EPlayerCharacterClass` 사용으로 변경):**

| 사용 위치 | 코드 | 용도 |
|-----------|------|------|
| `ADRCharacterBase::InitializeDefaultAttributes()` | `ApplyEffectToSelf(DefaultPrimaryAttributes); ApplyEffectToSelf(DefaultVitalAttributes);` | **블루프린트(BP_DRCharacter)에 직접 설정된** DefaultPrimaryAttributes / DefaultVitalAttributes GE를 사용 |
| `ADRCharacterBase::AddCharacterAbilities()` | `DRASC->AddCharacterAbilities(StartupAbilities); DRASC->AddCharacterPassiveAbilities(StartupPassiveAbilities);` | **블루프린트(BP_DRCharacter)에 직접 설정된** StartupAbilities / StartupPassiveAbilities 배열 사용 |

**플레이어의 초기화 흐름:**
```
ADRCharacter::PossessedBy(NewController)
  → InitAbilityActorInfo()
      → PlayerState에서 ASC 가져옴: ASC->InitAbilityActorInfo(DRPlayerState, this)
      → PlayerAttributeSet에 컨테이너 정보 설정
      → HUD 초기화
      → InitializeDefaultAttributes()  ← ★ 베이스 클래스 그대로 사용 (오버라이드 안 됨)
          → ApplyEffectToSelf(DefaultPrimaryAttributes)  // BP_DRCharacter에 설정된 GE
          → ApplyEffectToSelf(DefaultVitalAttributes)    // BP_DRCharacter에 설정된 GE
  → AddCharacterAbilities()
      → StartupAbilities 배열 부여           // BP_DRCharacter에 설정된 어빌리티들
      → StartupPassiveAbilities 배열 부여    // BP_DRCharacter에 설정된 패시브들
```

#### 2.0.4 결론: 현재 시스템에서의 CharacterClass 역할

```
┌─────────────────────────────────────────────────────────────────┐
│              UCharacterClassInfo (적 전용 데이터 에셋)              │
│                                                                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │ Elementalist  │  │   Warrior    │  │    Ranger    │           │
│  │ - Attributes │  │ - Attributes │  │ - Attributes │           │
│  │ - Abilities  │  │ - Abilities  │  │ - Abilities  │           │
│  │ - DeathAbil  │  │ - DeathAbil  │  │ - DeathAbil  │           │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘           │
│         │                 │                 │                     │
│         └────────┬────────┘─────────────────┘                     │
│                  │                                                 │
│         ┌────────▼────────┐                                       │
│         │   ADREnemy만    │  ← ECharacterClass 사용               │
│         │   사용 중       │                                       │
│         └─────────────────┘                                       │
│                                                                    │
│         UPlayerCharacterClassInfo (플레이어 전용 데이터 에셋)       │
│  ┌───────────────────┐  ┌────────────────────────┐               │
│  │   GardenRobot     │  │ VendingMachineRobot    │               │
│  │   - Attributes    │  │ - Attributes           │               │
│  │   - Abilities     │  │ - Abilities            │               │
│  └────────┬──────────┘  └──────────┬─────────────┘               │
│           └────────────┬───────────┘                              │
│                        │                                          │
│         ┌──────────────▼──────────────┐                           │
│         │  ADRCharacter (플레이어)     │  ← EPlayerCharacterClass │
│         │  PlayerCharacterClass 사용  │     사용                  │
│         └─────────────────────────────┘                           │
└─────────────────────────────────────────────────────────────────┘
```

**핵심:** 플레이어는 `EPlayerCharacterClass PlayerCharacterClass` 멤버를 사용 (기본값: GardenRobot). 상속받은 `ECharacterClass CharacterClass`는 플레이어에게 무관하며, `ICombatInterface::GetCharacterClass()`는 적 AI에서만 실제 사용된다.

#### 2.0.5 CharacterClassInfo를 Player용 / Enemy용으로 분리

현재 `UCharacterClassInfo` 데이터 에셋은 사실상 **적 전용**으로만 사용되고 있다 (DA_CharacterClassInfo.uasset). 플레이어에게도 클래스 선택 시스템을 도입하려면, 플레이어와 적은 **서로 다른 열거형**을 사용해야 한다 — 적은 `ECharacterClass` (Elementalist, Warrior, Ranger), 플레이어는 `EPlayerCharacterClass` (GardenRobot, VendingMachineRobot).

**이를 위해 `UPlayerCharacterClassInfo`라는 새 C++ 클래스를 만들고, Player용과 Enemy용 2개의 데이터 에셋으로 분리한다.**

---

##### 2.0.5.1 분리가 필요한 이유

적과 플레이어의 열거형 값 자체가 완전히 다르다:
- **적:** Elementalist, Warrior, Ranger (`ECharacterClass`)
- **플레이어:** GardenRobot, VendingMachineRobot (`EPlayerCharacterClass`)

따라서 같은 `TMap` 키 타입을 공유할 수 없으며, `UPlayerCharacterClassInfo`라는 별도의 C++ 클래스가 필요하다.

현재 데이터 에셋 구조:
```
DA_CharacterClassInfo (단일 데이터 에셋, 적 전용)
├── Elementalist → { 적의 PrimaryAttributes GE, 적의 StartupAbilities, 적의 DeathAbilities }
├── Warrior      → { 적의 PrimaryAttributes GE, 적의 StartupAbilities, 적의 DeathAbilities }
└── Ranger       → { 적의 PrimaryAttributes GE, 적의 StartupAbilities, 적의 DeathAbilities }
```

플레이어는 GardenRobot, VendingMachineRobot이라는 전혀 다른 클래스 체계를 사용하므로, 기존 `UCharacterClassInfo` (TMap<ECharacterClass, ...>)에는 키를 추가할 수조차 없다.

##### 2.0.5.2 분리 후 목표 구조

```
DA_EnemyCharacterClassInfo (기존 DA_CharacterClassInfo 이름 변경, 타입: UCharacterClassInfo)
├── Elementalist → { 적 PrimaryAttributes, 적 VitalAttributes, 적 StartupAbilities, 적 DeathAbilities }
├── Warrior      → { 적 PrimaryAttributes, 적 VitalAttributes, 적 StartupAbilities, 적 DeathAbilities }
└── Ranger       → { 적 PrimaryAttributes, 적 VitalAttributes, 적 StartupAbilities, 적 DeathAbilities }
└── CommonAbilities[] (적 공통 어빌리티)

DA_PlayerCharacterClassInfo (신규 생성, 타입: UPlayerCharacterClassInfo)
├── GardenRobot         → { 플레이어 PrimaryAttributes, 플레이어 VitalAttributes, 플레이어 StartupAbilities, [] }
└── VendingMachineRobot → { 플레이어 PrimaryAttributes, 플레이어 VitalAttributes, 플레이어 StartupAbilities, [] }
└── CommonAbilities[] (플레이어 공통 어빌리티)
```

**핵심:** 적과 플레이어가 서로 다른 열거형을 사용하므로 C++ 클래스도 분리한다. `UCharacterClassInfo`는 적 전용 (TMap<ECharacterClass, ...>), `UPlayerCharacterClassInfo`는 플레이어 전용 (TMap<EPlayerCharacterClass, ...>).

##### 2.0.5.3 C++ 코드 변경사항

**파일: `Source/DaeRune/Public/AbilitySystem/Data/CharacterClassInfo.h`**

`EPlayerCharacterClass` 열거형 추가:
```cpp
// 기존 ECharacterClass 아래에 추가
UENUM(BlueprintType)
enum class EPlayerCharacterClass : uint8
{
    GardenRobot,
    VendingMachineRobot
};
```

`UPlayerCharacterClassInfo` 클래스 추가:
```cpp
// 기존 UCharacterClassInfo 아래에 추가
UCLASS()
class DAERUNE_API UPlayerCharacterClassInfo : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
    TMap<EPlayerCharacterClass, FCharacterClassDefaultInfo> CharacterClassInformation;

    UPROPERTY(EditDefaultsOnly, Category = "Common Class Defaults")
    TArray<TSubclassOf<UGameplayAbility>> CommonAbilities;

    FCharacterClassDefaultInfo GetClassDefaultInfo(EPlayerCharacterClass CharacterClass);
};
```

---

**파일: `Source/DaeRune/Public/Game/DRGameModeBase.h`**

기존:
```cpp
// 단일 CharacterClassInfo
UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
TObjectPtr<UCharacterClassInfo> CharacterClassInfo;
```

변경:
```cpp
// 적 전용 CharacterClassInfo
UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
TObjectPtr<UCharacterClassInfo> EnemyCharacterClassInfo;

// 플레이어 전용 CharacterClassInfo
UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
TObjectPtr<UPlayerCharacterClassInfo> PlayerCharacterClassInfo;
```

---

**파일: `Source/DaeRune/Public/AbilitySystem/DRAbilitySystemLibrary.h`**

기존 함수 유지 + 분리된 getter 추가:
```cpp
// 기존 함수 (하위호환, 적용 → EnemyCharacterClassInfo 반환)
UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
static UCharacterClassInfo* GetCharacterClassInfo(const UObject* WorldContextObject);

// 새 함수: 플레이어 전용 CharacterClassInfo 반환 (타입: UPlayerCharacterClassInfo*)
UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
static UPlayerCharacterClassInfo* GetPlayerCharacterClassInfo(const UObject* WorldContextObject);

// 새 함수: 플레이어용 어트리뷰트 초기화 (EPlayerCharacterClass 사용)
UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
static void InitializePlayerDefaultAttributes(const UObject* WorldContextObject, EPlayerCharacterClass PlayerClass, float Level, UAbilitySystemComponent* ASC);

// 새 함수: 플레이어용 어빌리티 부여 (EPlayerCharacterClass 사용)
UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
static void GivePlayerStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, EPlayerCharacterClass PlayerClass);
```

---

**파일: `Source/DaeRune/Private/AbilitySystem/DRAbilitySystemLibrary.cpp`**

기존 `GetCharacterClassInfo()` 수정 + 새 함수 구현:
```cpp
// 기존 함수 → 적용으로 리다이렉트
UCharacterClassInfo* UDRAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
    const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
    if (DRGameMode == nullptr) return nullptr;
    return DRGameMode->EnemyCharacterClassInfo;  // ← 기존 CharacterClassInfo → EnemyCharacterClassInfo
}

// 새 함수 (반환 타입: UPlayerCharacterClassInfo*)
UPlayerCharacterClassInfo* UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(const UObject* WorldContextObject)
{
    const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
    if (DRGameMode == nullptr) return nullptr;
    return DRGameMode->PlayerCharacterClassInfo;
}

// 플레이어용 어트리뷰트 초기화 (UPlayerCharacterClassInfo 사용)
void UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(
    const UObject* WorldContextObject, EPlayerCharacterClass PlayerClass, float Level, UAbilitySystemComponent* ASC)
{
    AActor* AvatarActor = ASC->GetAvatarActor();

    UPlayerCharacterClassInfo* ClassInfo = GetPlayerCharacterClassInfo(WorldContextObject);
    if (!ClassInfo) return;

    FCharacterClassDefaultInfo ClassDefaultInfo = ClassInfo->GetClassDefaultInfo(PlayerClass);

    CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.PrimaryAttributes, AvatarActor, Level);
    CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.VitalAttributes, AvatarActor, Level);
}

// 플레이어용 어빌리티 부여 (UPlayerCharacterClassInfo 사용)
void UDRAbilitySystemLibrary::GivePlayerStartupAbilities(
    const UObject* WorldContextObject, UAbilitySystemComponent* ASC, EPlayerCharacterClass PlayerClass)
{
    UPlayerCharacterClassInfo* ClassInfo = GetPlayerCharacterClassInfo(WorldContextObject);
    if (!ClassInfo) return;

    int32 CharacterLevel = 1;
    if (ASC->GetAvatarActor()->Implements<UCombatInterface>())
    {
        CharacterLevel = ICombatInterface::Execute_GetPlayerLevel(ASC->GetAvatarActor());
    }

    // 플레이어 공통 어빌리티
    for (TSubclassOf<UGameplayAbility> AbilityClass : ClassInfo->CommonAbilities)
    {
        FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CharacterLevel);
        ASC->GiveAbility(AbilitySpec);
    }

    // 클래스별 어빌리티
    const FCharacterClassDefaultInfo& DefaultInfo = ClassInfo->GetClassDefaultInfo(PlayerClass);
    for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.StartupAbilities)
    {
        FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CharacterLevel);
        ASC->GiveAbility(AbilitySpec);
    }
}
```

##### 2.0.5.4 적(ADREnemy) 코드 - 변경 없음

적의 코드는 기존 함수들을 그대로 사용하므로 변경이 필요 없다:

| 적의 호출 | 내부 동작 | 변경 여부 |
|-----------|----------|----------|
| `UDRAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, ASC)` | `GetCharacterClassInfo()` → `DRGameMode->EnemyCharacterClassInfo` | ❌ 변경 없음 (기존 함수가 EnemyCharacterClassInfo를 반환하도록 리다이렉트) |
| `UDRAbilitySystemLibrary::GiveStartupAbilities(this, ASC, CharacterClass)` | `GetCharacterClassInfo()` → `DRGameMode->EnemyCharacterClassInfo` | ❌ 변경 없음 |
| `UDRAbilitySystemLibrary::GetCharacterClassInfo(this)` → DeathAbilities | `DRGameMode->EnemyCharacterClassInfo` | ❌ 변경 없음 |

##### 2.0.5.5 플레이어(ADRCharacter) 코드 - Phase 7에서 변경

Phase 7에서 플레이어가 `UPlayerCharacterClassInfo`를 사용하도록 전환. `CharacterClass` 대신 `PlayerCharacterClass` (타입: `EPlayerCharacterClass`) 사용:

```cpp
// ADRCharacter::InitializeDefaultAttributes() 오버라이드 (Phase 7에서 추가)
void ADRCharacter::InitializeDefaultAttributes() const
{
    // 플레이어 전용 CharacterClassInfo 사용 (EPlayerCharacterClass)
    UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(this, PlayerCharacterClass, Level, AbilitySystemComponent);
}

// ADRCharacter::PossessedBy() 수정 (Phase 7에서 변경)
void ADRCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitAbilityActorInfo();

    // 기존: AddCharacterAbilities();  // BP 직접 설정된 어빌리티
    // 변경: UPlayerCharacterClassInfo 기반 어빌리티 부여
    if (HasAuthority())
    {
        UDRAbilitySystemLibrary::GivePlayerStartupAbilities(this, AbilitySystemComponent, PlayerCharacterClass);
    }

    InitializeMoveSpeedBinding();
}
```

##### 2.0.5.6 FCharacterClassDefaultInfo 구조체 — 변경 없음 (런타임 메시 교체 대신 BP 교체 방식 채택)

`FCharacterClassDefaultInfo`에 `CharacterMesh`/`AnimClass` 필드를 추가하지 **않는다.**

**이유: 런타임 `SetSkeletalMesh()` 방식의 한계**

현재 `ADRCharacter`의 컴포넌트 구조를 분석한 결과:

```
CapsuleComponent (Root)
├── CameraBoom (SetupAttachment: CapsuleComponent)
│   ├── SetRelativeLocation(30, 0, 50)
│   └── FollowCamera (SetupAttachment: CameraBoom Socket)
│       └── FirstPersonMesh (SetupAttachment: FollowCamera) ← 1인칭 전용, OnlyOwnerSee
└── GetMesh() ← 3인칭 전용, OwnerNoSee
    └── Weapon (WeaponHandSocket에 부착)
```

`SetSkeletalMesh()`으로 런타임 메시 교체 시 다음 문제가 발생한다:

| 문제 | 설명 |
|------|------|
| **1인칭 메시 미교체** | `FirstPersonMesh`는 `GetMesh()`와 별도 컴포넌트. `GetMesh()->SetSkeletalMesh()`을 호출해도 **1인칭 메시는 그대로** |
| **카메라 위치 부정합** | `CameraBoom` 오프셋(30, 0, 50)이 캐릭터별로 다를 수 있음. 메시 비율이 다르면 카메라가 머리를 관통하거나 공중에 뜸 |
| **소켓 호환성** | 새 스켈레톤에 `WeaponHandSocket` 등 기존 소켓이 없으면 무기 부착이 깨짐 |
| **AnimClass 재초기화** | `SetAnimInstanceClass()` 후 재초기화 타이밍 이슈 가능 |

**따라서 `FCharacterClassDefaultInfo`에는 메시/애님 필드를 추가하지 않고, 대신 `UPlayerCharacterClassInfo`에 캐릭터 BP 클래스 참조를 둔다.**

`UPlayerCharacterClassInfo` 클래스에 추가할 필드:
```cpp
UCLASS()
class DAERUNE_API UPlayerCharacterClassInfo : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
    TMap<EPlayerCharacterClass, FCharacterClassDefaultInfo> CharacterClassInformation;

    UPROPERTY(EditDefaultsOnly, Category = "Common Class Defaults")
    TArray<TSubclassOf<UGameplayAbility>> CommonAbilities;

    // ★ 클래스별 캐릭터 BP 매핑 (외형은 BP에서 전부 설정)
    UPROPERTY(EditDefaultsOnly, Category = "Character Blueprint")
    TMap<EPlayerCharacterClass, TSubclassOf<ADRCharacter>> CharacterBPClasses;

    FCharacterClassDefaultInfo GetClassDefaultInfo(EPlayerCharacterClass CharacterClass);
};
```

**BP 교체 방식 (Phase 7에서 구현):**
- `BP_DRCharacter_GardenRobot`, `BP_DRCharacter_VendingMachineRobot` 각각의 BP에서 **1인칭 메시, 3인칭 메시, 카메라 오프셋, 애니메이션을 개별 설정**
- 클래스 변경 시 **기존 폰을 Destroy하고 새 BP의 폰을 Spawn → Possess**
- 각 BP는 `ADRCharacter`를 상속하므로 C++ 코드는 동일하게 동작

```
┌─────────────────────────────────────────────────┐
│  UPlayerCharacterClassInfo (데이터 에셋)          │
│  ├── CharacterClassInformation (TMap)            │
│  │   ├── GardenRobot → { GE, Abilities }        │
│  │   └── VendingMachineRobot → { GE, Abilities }│
│  └── CharacterBPClasses (TMap)                   │
│      ├── GardenRobot → BP_DRCharacter_Garden     │
│      └── VendingMachineRobot → BP_DRCharacter_VM │
└─────────────────────────────────────────────────┘

BP_DRCharacter_GardenRobot (ADRCharacter 상속)
├── GetMesh() → GardenRobot 3인칭 SkeletalMesh
├── FirstPersonMesh → GardenRobot 1인칭 SkeletalMesh
├── CameraBoom → GardenRobot 전용 오프셋
├── AnimClass → GardenRobot 전용 AnimBP
└── Weapon 소켓 → GardenRobot 스켈레톤 기준

BP_DRCharacter_VendingMachineRobot (ADRCharacter 상속)
├── GetMesh() → VM 3인칭 SkeletalMesh
├── FirstPersonMesh → VM 1인칭 SkeletalMesh
├── CameraBoom → VM 전용 오프셋
├── AnimClass → VM 전용 AnimBP
└── Weapon 소켓 → VM 스켈레톤 기준
```

##### 2.0.5.7 블루프린트/에디터 작업

| 작업 | 설명 |
|------|------|
| `DA_CharacterClassInfo` → `DA_EnemyCharacterClassInfo` 이름 변경 | 기존 적 데이터 유지, 이름만 변경. 타입: `UCharacterClassInfo` |
| `DA_PlayerCharacterClassInfo` 신규 생성 | **`UPlayerCharacterClassInfo` 타입**의 새 데이터 에셋 |
| `BP_DRCharacter_GardenRobot` 신규 생성 | `ADRCharacter` 상속. GardenRobot용 1인칭/3인칭 메시, 카메라 오프셋, AnimBP 설정 |
| `BP_DRCharacter_VendingMachineRobot` 신규 생성 | `ADRCharacter` 상속. VendingMachineRobot용 1인칭/3인칭 메시, 카메라 오프셋, AnimBP 설정 |
| `BP_DRStageGameMode`에서 두 에셋 설정 | `EnemyCharacterClassInfo` = DA_EnemyCharacterClassInfo, `PlayerCharacterClassInfo` = DA_PlayerCharacterClassInfo |
| `BP_DRLobbyGameMode`에서 두 에셋 설정 | 동일하게 양쪽 모두 설정 (로비에서도 대기실 BP 교체에 PlayerCharacterClassInfo 필요) |
| `DA_PlayerCharacterClassInfo` 내용 채우기 | **GardenRobot**, **VendingMachineRobot** 엔트리별 PrimaryAttributes GE, VitalAttributes GE, StartupAbilities 설정 + `CharacterBPClasses`에 각 클래스별 BP 매핑 |

##### 2.0.5.8 영향 범위 요약

```
┌──────────────────────────────────────────────────────────────────────┐
│                         변경 후 전체 구조                              │
│                                                                       │
│  CharacterClassInfo.h                                                 │
│  ├── ECharacterClass (적 전용, 기존 유지)                              │
│  ├── EPlayerCharacterClass (플레이어 전용, 신규)                       │
│  │   └── GardenRobot(0), VendingMachineRobot(1)                      │
│  ├── UCharacterClassInfo (적 전용, 기존 유지)                          │
│  │   └── TMap<ECharacterClass, FCharacterClassDefaultInfo>            │
│  └── UPlayerCharacterClassInfo (플레이어 전용, 신규)                   │
│      └── TMap<EPlayerCharacterClass, FCharacterClassDefaultInfo>      │
│                                                                       │
│  ADRGameModeBase                                                      │
│  ├── EnemyCharacterClassInfo (UCharacterClassInfo*)                   │
│  │   → DA_EnemyCharacterClassInfo.uasset                             │
│  └── PlayerCharacterClassInfo (UPlayerCharacterClassInfo*)            │
│      → DA_PlayerCharacterClassInfo.uasset                            │
│                                                                       │
│  DRAbilitySystemLibrary                                               │
│  ├── GetCharacterClassInfo()               → UCharacterClassInfo*    │
│  ├── GetPlayerCharacterClassInfo()   [NEW] → UPlayerCharacterClassInfo*│
│  ├── InitializeDefaultAttributes()         → 적 전용 (ECharacterClass)│
│  ├── InitializePlayerDefaultAttributes() [NEW] → EPlayerCharacterClass│
│  ├── GiveStartupAbilities()                → 적 전용 (ECharacterClass)│
│  └── GivePlayerStartupAbilities()    [NEW] → EPlayerCharacterClass   │
│                                                                       │
│  ADREnemy (변경 없음)                                                  │
│  └── ECharacterClass CharacterClass 사용, 기존 코드 그대로 동작        │
│                                                                       │
│  ADRCharacter (Phase 7에서 변경)                                       │
│  ├── EPlayerCharacterClass PlayerCharacterClass (신규 멤버)            │
│  ├── InitializeDefaultAttributes() → InitializePlayerDefaultAttributes│
│  └── PossessedBy() → GivePlayerStartupAbilities(PlayerCharacterClass)│
│                                                                       │
│  FCharacterClassDefaultInfo (구조체 변경 없음)                           │
│  └── GE + 어빌리티만 관리, 외형은 BP에서 설정                          │
│                                                                       │
│  UPlayerCharacterClassInfo (신규 클래스)                                │
│  ├── TMap<EPlayerCharacterClass, FCharacterClassDefaultInfo> (GE/어빌) │
│  └── TMap<EPlayerCharacterClass, TSubclassOf<ADRCharacter>> (BP 매핑) │
└──────────────────────────────────────────────────────────────────────┘
```

##### 2.0.5.9 구현 시점

- **Phase 2 (현재):** PlayerState에 `SelectedPlayerClass` (`EPlayerCharacterClass`) 추가 (데이터 레이어만)
- **Phase 7 (캐릭터 BP 교체 시):** 위의 CharacterClassInfo 분리 + 플레이어 초기화 경로 변경 + BP 교체 시스템을 함께 진행
  - CharacterClassInfo.h에 `EPlayerCharacterClass` 열거형 + `UPlayerCharacterClassInfo` 클래스 추가 (`CharacterBPClasses` TMap 포함)
  - DRGameModeBase.h 수정 (EnemyCharacterClassInfo + PlayerCharacterClassInfo)
  - DRAbilitySystemLibrary 확장 (새 함수 3개, `EPlayerCharacterClass` + `UPlayerCharacterClassInfo*` 사용)
  - ADRCharacter에 `PlayerCharacterClass` 멤버 추가, 오버라이드 (InitializeDefaultAttributes, PossessedBy)
  - 클래스별 캐릭터 BP 생성 (BP_DRCharacter_GardenRobot, BP_DRCharacter_VendingMachineRobot)
  - 클래스 변경 시 폰 Destroy → 새 BP 폰 Spawn → Possess 로직 구현
  - 블루프린트/에디터 작업 (데이터 에셋 2개 설정: DA_EnemyCharacterClassInfo + DA_PlayerCharacterClassInfo)

---

### 2.1 ADRPlayerState에 SelectedPlayerClass 추가

**파일:** `Source/DaeRune/Public/Player/DRPlayerState.h`

**추가할 내용:**
```cpp
#include "AbilitySystem/Data/CharacterClassInfo.h"  // EPlayerCharacterClass

// 기존 델리게이트 선언 아래에 추가
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerClassChanged, ADRPlayerState*, PlayerState, EPlayerCharacterClass, NewClass);

public:
    // 캐릭터 클래스 선택
    UFUNCTION(BlueprintCallable, Category = "Character Selection")
    EPlayerCharacterClass GetSelectedPlayerClass() const { return SelectedPlayerClass; }

    void SetSelectedPlayerClass(EPlayerCharacterClass NewClass);

    UPROPERTY(BlueprintAssignable, Category = "Character Selection")
    FOnPlayerClassChanged OnPlayerClassChanged;

protected:
    UPROPERTY(ReplicatedUsing = OnRep_SelectedPlayerClass, BlueprintReadOnly, Category = "Character Selection")
    EPlayerCharacterClass SelectedPlayerClass = EPlayerCharacterClass::GardenRobot;

    UFUNCTION()
    void OnRep_SelectedPlayerClass();
```

**파일:** `Source/DaeRune/Private/Player/DRPlayerState.cpp`

**추가할 내용:**
```cpp
// GetLifetimeReplicatedProps에 추가:
DOREPLIFETIME(ADRPlayerState, SelectedPlayerClass);

void ADRPlayerState::SetSelectedPlayerClass(EPlayerCharacterClass NewClass)
{
    if (!HasAuthority()) return;
    if (SelectedPlayerClass == NewClass) return;

    SelectedPlayerClass = NewClass;
    OnPlayerClassChanged.Broadcast(this, SelectedPlayerClass);
}

void ADRPlayerState::OnRep_SelectedPlayerClass()
{
    OnPlayerClassChanged.Broadcast(this, SelectedPlayerClass);
}
```

### 2.2 ADRPlayerController에 클래스 변경 Server RPC 추가

**파일:** `Source/DaeRune/Public/Player/DRPlayerController.h`

**추가할 내용:**
```cpp
public:
    // 클라이언트에서 호출 → 서버에서 실행
    UFUNCTION(Server, Reliable, Category = "Character Selection")
    void ServerRequestChangeClass(bool bNext);

    // 블루프린트에서도 호출 가능한 래퍼
    UFUNCTION(BlueprintCallable, Category = "Character Selection")
    void RequestChangeClass(bool bNext);
```

**파일:** `Source/DaeRune/Private/Player/DRPlayerController.cpp`

```cpp
void ADRPlayerController::RequestChangeClass(bool bNext)
{
    ServerRequestChangeClass(bNext);
}

void ADRPlayerController::ServerRequestChangeClass_Implementation(bool bNext)
{
    // 대기실 상태에서만 가능
    ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
    if (!LGS || LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

    ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
    if (!PS) return;

    // 현재 클래스 가져오기
    int32 CurrentIndex = static_cast<int32>(PS->GetSelectedPlayerClass());
    constexpr int32 ClassCount = 2; // GardenRobot, VendingMachineRobot

    // 순환
    int32 NewIndex;
    if (bNext)
        NewIndex = (CurrentIndex + 1) % ClassCount;
    else
        NewIndex = (CurrentIndex - 1 + ClassCount) % ClassCount;

    PS->SetSelectedPlayerClass(static_cast<EPlayerCharacterClass>(NewIndex));
}
```

**순환 순서:**
```
bNext=true:  GardenRobot(0) → VendingMachineRobot(1) → GardenRobot(0)
bNext=false: GardenRobot(0) → VendingMachineRobot(1) → GardenRobot(0)
```

### 2.3 기존 ADRCharacterBase::CharacterClass와의 연결 - 상세 분석

#### 2.3.1 현재 상태 (변경 전)

`ADRCharacterBase`에 `ECharacterClass CharacterClass = ECharacterClass::Warrior`가 있고,
`ADRCharacter` 생성자에서 `ECharacterClass::Elementalist`로 덮어씀.

**이제 `ADRCharacter`는 상속받은 `ECharacterClass CharacterClass`를 더 이상 사용하지 않는다.** 대신 `EPlayerCharacterClass PlayerCharacterClass` 멤버를 사용한다. 생성자의 `CharacterClass = ECharacterClass::Elementalist` 덮어쓰기는 삭제한다.

**그러나 기존 `CharacterClass` 값은 적의 어트리뷰트 초기화에만 사용되며, 플레이어의 어트리뷰트 초기화에는 사용되지 않는다:**

```cpp
// ADRCharacterBase::InitializeDefaultAttributes() - 플레이어가 사용하는 경로
void ADRCharacterBase::InitializeDefaultAttributes() const
{
    ApplyEffectToSelf(DefaultPrimaryAttributes);  // ← BP에 설정된 GE 직접 사용
    ApplyEffectToSelf(DefaultVitalAttributes);    // ← BP에 설정된 GE 직접 사용
    // ❌ CharacterClass 미사용, CharacterClassInfo 미참조
}

// ADREnemy::InitializeDefaultAttributes() - 적이 사용하는 오버라이드 경로
void ADREnemy::InitializeDefaultAttributes() const
{
    UDRAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, ASC);
    // ✅ CharacterClass 사용 → CharacterClassInfo 데이터 에셋 참조
}
```

**어빌리티도 마찬가지:**

```cpp
// ADRCharacterBase::AddCharacterAbilities() - 플레이어가 사용
void ADRCharacterBase::AddCharacterAbilities()
{
    DRASC->AddCharacterAbilities(StartupAbilities);         // ← BP에 설정된 배열
    DRASC->AddCharacterPassiveAbilities(StartupPassiveAbilities); // ← BP에 설정된 배열
    // ❌ CharacterClass 미사용, CharacterClassInfo 미참조
}

// ADREnemy::BeginPlay() 에서 - 적이 사용
UDRAbilitySystemLibrary::GiveStartupAbilities(this, ASC, CharacterClass);
// ✅ CharacterClass 사용 → CharacterClassInfo의 CommonAbilities + 클래스별 StartupAbilities
```

#### 2.3.2 Phase 2에서는 데이터 레이어만 구축

Phase 2에서는 `SelectedCharacterClass`를 PlayerState에 저장하고 리플리케이트하는 것만 다룬다.
실제 게임플레이(어트리뷰트, 어빌리티, 메시)에 연결하는 작업은 Phase 7에서 수행.

**Phase 7에서 해야 할 작업 (미리보기):**

1. `ADRCharacter::InitializeDefaultAttributes()`를 오버라이드하여 `UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(this, PlayerCharacterClass, Level, ASC)`를 사용하도록 변경 (`EPlayerCharacterClass` 타입)
2. `ADRCharacter::PossessedBy()`에서 `AddCharacterAbilities()` 대신 `UDRAbilitySystemLibrary::GivePlayerStartupAbilities(this, ASC, PlayerCharacterClass)`를 사용하도록 변경 (`EPlayerCharacterClass` 타입)
3. `ADRCharacter::InitAbilityActorInfo()` 시작 시 `PlayerCharacterClass = DRPlayerState->GetSelectedPlayerClass()`로 설정
4. `UPlayerCharacterClassInfo`의 `CharacterBPClasses` TMap에 GardenRobot/VendingMachineRobot별 캐릭터 BP 클래스 매핑
5. 클래스 변경 시 **기존 폰 Destroy → 새 클래스 BP 폰 Spawn → Possess** 로직 구현 (외형은 BP에서 완전 관리)
6. 클래스별 캐릭터 BP 생성: `BP_DRCharacter_GardenRobot`, `BP_DRCharacter_VendingMachineRobot` (각각 1인칭/3인칭 메시, 카메라 오프셋, AnimBP 개별 설정)

### 검증 방법
PIE 2인 플레이. 콘솔 또는 블루프린트에서 `RequestChangeClass(true)` 호출. 두 클라이언트 모두에서 PlayerState의 `SelectedCharacterClass` 값이 변경되었는지 확인.

---

## Phase 3: 대기실 카메라 + 플레이어 배치

### 목표
로비 맵에 대기실 전용 고정 카메라를 배치하고, 플레이어 캐릭터들을 지정된 슬롯 위치에 정렬하는 시스템 구축.

---

### 3.0 설계 결정: 플레이어 슬롯 위치를 어디에 정의할 것인가

#### 3.0.1 후보 비교

| 방식 | 설명 | 장점 | 단점 |
|------|------|------|------|
| **A. 카메라 액터에 슬롯 포함** (기존 Plan) | `ADRWaitingRoomCameraActor`에 `TArray<FTransform> PlayerSlotTransforms` | 하나의 액터로 관리 간단 | **카메라와 플레이어 배치가 결합** — 카메라 책임이 아닌 데이터를 들고 있음. 카메라 교체/추가 시 슬롯 데이터도 따라감 |
| **B. GameMode에 슬롯 배열** (유저 제안) | `ADRLobbyGameMode`에 `TArray<FTransform>` EditDefaultsOnly | 개념적으로 가장 정확 (GameMode가 플레이어 관리 담당) | GameMode는 레벨에 배치되지 않으므로 **에디터에서 슬롯 위치를 시각적으로 확인/조정 불가** — 좌표를 수동 입력해야 함 |
| **C. 개별 슬롯 마커 액터** (CleanserSite 패턴) | `ATargetPoint`에 "WaitingRoomSlot" 태그, 레벨에 4개 배치. GameMode가 태그로 탐색 후 이름순 정렬 | **기존 코드베이스 패턴과 동일** (CleanserSite 탐색 = `TActorIterator` + `ActorHasTag`). 레벨에서 드래그로 위치 조정 가능. 카메라와 완전 분리 | 레벨에 액터 4개 추가 (사소함) |

#### 3.0.2 결정: **방식 C (개별 슬롯 마커 액터)** 채택

**이유:**
1. **기존 패턴 일관성** — `DRStageGameMode`가 `TActorIterator<ADRCleanserSite>` + `ActorHasTag("CleanserSite")`로 사이트를 탐색하는 것과 동일한 패턴. 코드베이스 전반에서 일관된 "레벨에 액터 배치 → GameMode가 탐색" 방식.
2. **관심사 분리** — 카메라 액터는 카메라 전용, 슬롯 위치는 GameMode가 관리. 플레이어 배치 로직은 오롯이 GameMode의 책임.
3. **시각적 편집** — 에디터에서 각 슬롯 마커를 드래그하여 위치 조정 가능. 에디터 뷰포트에서 플레이어가 서 있을 위치를 직접 눈으로 확인 가능.
4. **유연성** — 슬롯 개수를 늘리거나 줄이려면 액터를 추가/삭제하면 됨. 코드 변경 불필요.

#### 3.0.3 기존 PlayerStart 처리

현재 로비 맵에 배치된 `APlayerStart` 액터의 처리:

- **기존 PlayerStart 유지** — UE5 GameMode의 기본 스폰 시스템이 `PlayerStart`를 필요로 함. 플레이어는 먼저 `PlayerStart` 위치에 스폰된 후, 0.5초 딜레이로 슬롯 위치로 텔레포트됨.
- **PlayerStart 위치 조정** — 슬롯 마커 근처 (카메라에 안 보이는 곳)에 하나 배치하면 충분. 여러 개 둘 필요 없음.
- **FreeRoam 전환 시** — `PowerOn()` 후에는 슬롯 텔레포트 없이 자유 이동이므로 PlayerStart 위치는 무관.

**정리:** 로비 맵의 기존 PlayerStart를 **1개만 남기고** (스폰용), 나머지는 삭제. 슬롯 마커로 플레이어 위치를 제어.

---

### 3.1 카메라 액터: `ADRWaitingRoomCameraActor` (카메라 전용)

카메라 액터는 **고정 카메라 시점만 담당**. 슬롯 위치 데이터를 포함하지 않음.

**경로:** `Source/DaeRune/Public/Actor/DRWaitingRoomCameraActor.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRWaitingRoomCameraActor.generated.h"

class UCameraComponent;

/**
 * 대기실의 고정 카메라. LobbyMap에 하나 배치한다.
 * 캐릭터들이 서 있는 위치를 한눈에 볼 수 있는 시점을 제공.
 */
UCLASS()
class DAERUNE_API ADRWaitingRoomCameraActor : public AActor
{
    GENERATED_BODY()

public:
    ADRWaitingRoomCameraActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCameraComponent> CameraComponent;

    // 카메라가 바라보는 방향으로 캐릭터가 회전해야 할 Rotation 반환
    UFUNCTION(BlueprintCallable, Category = "Waiting Room")
    FRotator GetCharacterFacingRotation() const;
};
```

**경로:** `Source/DaeRune/Private/Actor/DRWaitingRoomCameraActor.cpp`

```cpp
#include "Actor/DRWaitingRoomCameraActor.h"
#include "Camera/CameraComponent.h"

ADRWaitingRoomCameraActor::ADRWaitingRoomCameraActor()
{
    PrimaryActorTick.bCanEverTick = false;

    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("WaitingRoomCamera"));
    RootComponent = CameraComponent;
}

FRotator ADRWaitingRoomCameraActor::GetCharacterFacingRotation() const
{
    // 카메라를 향하는 방향 = 카메라 전방 벡터의 반대
    FVector CameraForward = CameraComponent->GetForwardVector();
    FRotator FacingRotation = (-CameraForward).Rotation();
    FacingRotation.Pitch = 0.f; // 수평 유지
    FacingRotation.Roll = 0.f;
    return FacingRotation;
}
```

**변경 사항 (기존 Phase 3 구현과의 차이):**
- `PlayerSlotTransforms` TArray 제거
- `GetSlotTransform()` 제거
- `GetCharacterFacingRotation()`만 유지

---

### 3.2 슬롯 마커: `ATargetPoint` + "WaitingRoomSlot" 태그

**새 C++ 클래스 불필요.** UE5 기본 `ATargetPoint`를 그대로 사용하고, 에디터에서 액터 태그를 설정한다.

**레벨 배치 (LobbyMap):**
- `ATargetPoint` 4개를 레벨에 배치
- 각 액터에 태그 `"WaitingRoomSlot"` 추가 (Details → Tags)
- 각 액터 이름을 `WaitingRoomSlot_0`, `WaitingRoomSlot_1`, `WaitingRoomSlot_2`, `WaitingRoomSlot_3` 으로 지정 (정렬용)
- 위치: 카메라가 바라보는 영역에 일렬로 배치 (간격 약 150~200 유닛)
- 회전: 무관 (캐릭터 회전은 카메라 기준으로 자동 계산)

**이름 정렬 규칙:**
- `GetName()` 기준 알파벳 정렬 → `_0` < `_1` < `_2` < `_3`
- 호스트(슬롯 0) = 가장 왼쪽, 이후 참가순으로 오른쪽

---

### 3.3 ADRLobbyGameMode에 슬롯 탐색 + 배치 로직 추가

**파일:** `Source/DaeRune/Public/Game/DRLobbyGameMode.h`

**추가할 내용:**
```cpp
class ADRWaitingRoomCameraActor;

protected:
    // 대기실 카메라 (BeginPlay에서 레벨에서 탐색)
    UPROPERTY()
    TObjectPtr<ADRWaitingRoomCameraActor> WaitingRoomCamera;

    // 슬롯 마커 액터 배열 (이름순 정렬)
    UPROPERTY()
    TArray<TObjectPtr<AActor>> WaitingRoomSlots;

    // 플레이어 → 슬롯 인덱스 매핑
    TMap<AController*, int32> PlayerSlotMap;

    // 다음 사용 가능한 슬롯 인덱스
    int32 NextAvailableSlot = 0;

    // 대기실 카메라 및 슬롯 마커 탐색
    void FindWaitingRoomActors();

    // 플레이어를 슬롯에 배치
    void AssignPlayerToSlot(AController* Player);

    // 모든 플레이어 재배치 (킥/퇴장 후 빈 자리 정리)
    void RepositionAllPlayers();

    // 플레이어 폰을 슬롯 위치로 텔레포트 + 이동 비활성화
    void PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex);
```

**파일:** `Source/DaeRune/Private/Game/DRLobbyGameMode.cpp`

```cpp
#include "Actor/DRWaitingRoomCameraActor.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"  // TActorIterator

void ADRLobbyGameMode::BeginPlay()
{
    Super::BeginPlay();

    FindWaitingRoomActors();
    AllowJoinInProgress();
}

void ADRLobbyGameMode::FindWaitingRoomActors()
{
    // 카메라 탐색 (TActorIterator, 프로젝트 기존 패턴)
    for (TActorIterator<ADRWaitingRoomCameraActor> It(GetWorld()); It; ++It)
    {
        WaitingRoomCamera = *It;
        break; // 하나만 필요
    }

    // 슬롯 마커 탐색 (CleanserSite 탐색과 동일한 패턴: TActorIterator + ActorHasTag)
    static const FName WaitingRoomSlotTag = TEXT("WaitingRoomSlot");
    for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(WaitingRoomSlotTag))
        {
            WaitingRoomSlots.Add(*It);
        }
    }

    // 이름순 정렬 (WaitingRoomSlot_0 < WaitingRoomSlot_1 < ...)
    WaitingRoomSlots.Sort([](const TObjectPtr<AActor>& A, const TObjectPtr<AActor>& B)
    {
        return A->GetName() < B->GetName();
    });
}

void ADRLobbyGameMode::AssignPlayerToSlot(AController* Player)
{
    if (WaitingRoomSlots.Num() == 0) return;

    int32 SlotIndex = NextAvailableSlot++;
    PlayerSlotMap.Add(Player, SlotIndex);

    if (APawn* Pawn = Player->GetPawn())
    {
        PositionPawnAtSlot(Pawn, SlotIndex);
    }
}

void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
    if (WaitingRoomSlots.Num() == 0 || !Pawn) return;

    // 유효 범위 클램프
    int32 ClampedIndex = FMath::Clamp(SlotIndex, 0, WaitingRoomSlots.Num() - 1);
    AActor* SlotActor = WaitingRoomSlots[ClampedIndex];
    if (!SlotActor) return;

    // 텔레포트
    Pawn->SetActorLocation(SlotActor->GetActorLocation());

    // 카메라를 향하도록 회전
    if (WaitingRoomCamera)
    {
        Pawn->SetActorRotation(WaitingRoomCamera->GetCharacterFacingRotation());
    }

    // 이동 비활성화
    if (UCharacterMovementComponent* MovementComp =
        Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
    {
        MovementComp->DisableMovement(); // MOVE_None으로 설정
    }
}

void ADRLobbyGameMode::RepositionAllPlayers()
{
    if (WaitingRoomSlots.Num() == 0) return;

    // 기존 매핑 수집 (호스트 우선)
    TArray<AController*> OrderedPlayers;

    // 호스트를 먼저 찾기
    for (auto& Pair : PlayerSlotMap)
    {
        if (ADRPlayerController* PC = Cast<ADRPlayerController>(Pair.Key))
        {
            if (PC->IsLocalController() && HasAuthority())
            {
                OrderedPlayers.Insert(Pair.Key, 0); // 호스트를 맨 앞에
                continue;
            }
        }
        OrderedPlayers.Add(Pair.Key);
    }

    // 매핑 재구성
    PlayerSlotMap.Empty();
    NextAvailableSlot = 0;

    for (AController* Player : OrderedPlayers)
    {
        AssignPlayerToSlot(Player);
    }
}
```

---

### 3.4 PostLogin / HandleSeamlessTravelPlayer에서 슬롯 배치

**기존 `PostLogin` 수정:**
```cpp
void ADRLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(NewPlayer))
    {
        ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();

        // 대기실 상태일 때만 슬롯 배치
        if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
        {
            FTimerHandle SlotTimerHandle;
            GetWorldTimerManager().SetTimer(
                SlotTimerHandle,
                [this, DRPC]()
                {
                    if (!IsValid(DRPC)) return;
                    AssignPlayerToSlot(DRPC);

                    // 고정 카메라로 ViewTarget 설정
                    if (WaitingRoomCamera)
                    {
                        DRPC->ClientSetWaitingRoomView(WaitingRoomCamera);
                    }
                },
                0.5f, false
            );
        }
        else
        {
            // 기존 복원 로직 (FreeRoam 상태)
            // ... (기존 코드 유지: ClientStopSpectating, Movement/Capsule 복원)
        }
    }
    // ... (기존 디버그 메시지 코드 유지)
}
```

**기존 `HandleSeamlessTravelPlayer` 수정:** 동일 패턴으로 `AssignPlayerToSlot` + `ClientSetWaitingRoomView` 호출.

**기존 `Logout` 수정:**
```cpp
void ADRLobbyGameMode::Logout(AController* Exiting)
{
    // 슬롯 매핑에서 제거
    if (PlayerSlotMap.Contains(Exiting))
    {
        PlayerSlotMap.Remove(Exiting);

        // 대기실 상태면 남은 플레이어 재배치
        ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
        if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
        {
            RepositionAllPlayers();
        }
    }

    Super::Logout(Exiting);
    // ... (기존 디버그 메시지 코드 유지)
}
```

---

### 3.5 ADRPlayerController에 카메라 설정 Client RPC 추가

**파일:** `Source/DaeRune/Public/Player/DRPlayerController.h`

```cpp
class ADRWaitingRoomCameraActor;

public:
    // 서버 → 클라이언트: 대기실 카메라로 ViewTarget 설정
    UFUNCTION(Client, Reliable)
    void ClientSetWaitingRoomView(ADRWaitingRoomCameraActor* CameraActor);

    // 서버 → 클라이언트: 카메라를 캐릭터로 부드럽게 전환
    UFUNCTION(Client, Reliable)
    void ClientStartCameraTransitionToCharacter();
```

**파일:** `Source/DaeRune/Private/Player/DRPlayerController.cpp`

```cpp
#include "Actor/DRWaitingRoomCameraActor.h"

void ADRPlayerController::ClientSetWaitingRoomView_Implementation(
    ADRWaitingRoomCameraActor* CameraActor)
{
    if (!CameraActor) return;

    // 즉시 고정 카메라로 전환
    SetViewTargetWithBlend(CameraActor, 0.f);

    // UI Only 모드 (마우스 커서 ON)
    SetInputMode(FInputModeUIOnly());
    SetShowMouseCursor(true);
}

void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
    // 캐릭터로 부드럽게 전환
    if (APawn* MyPawn = GetPawn())
    {
        SetViewTargetWithBlend(MyPawn, 1.5f);
    }

    // 게임 입력 모드로 전환
    SetInputMode(FInputModeGameOnly());
    SetShowMouseCursor(false);
}
```

---

### 3.6 RestoreDefaultInputMode 수정

**기존 코드:**
```cpp
void ADRPlayerController::RestoreDefaultInputMode()
{
    if (IsInMainMenu()) { SetInputMode(UIOnly); SetShowMouseCursor(true); }
    else { SetInputMode(GameOnly); SetShowMouseCursor(false); }
}
```

**변경:**
```cpp
void ADRPlayerController::RestoreDefaultInputMode()
{
    if (IsInMainMenu())
    {
        SetInputMode(FInputModeUIOnly());
        SetShowMouseCursor(true);
    }
    else if (IsInLobby())
    {
        // 로비 상태에 따라 분기
        ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
        if (LGS && (LGS->GetLobbyState() == ELobbyState::WaitingRoom
                  || LGS->GetLobbyState() == ELobbyState::Transitioning))
        {
            SetInputMode(FInputModeUIOnly());
            SetShowMouseCursor(true);
        }
        else
        {
            SetInputMode(FInputModeGameOnly());
            SetShowMouseCursor(false);
        }
    }
    else // Stage
    {
        SetInputMode(FInputModeGameOnly());
        SetShowMouseCursor(false);
    }
}
```

---

### 3.7 에디터/블루프린트 작업

| 작업 | 설명 |
|------|------|
| **BP_WaitingRoomCamera 생성** | `ADRWaitingRoomCameraActor` 기반 블루프린트 (경로: `Content/Blueprints/Actor/`). LobbyMap에 하나 배치. 캐릭터들이 서 있는 영역을 정면에서 바라보는 위치/각도로 설정 |
| **WaitingRoomSlot 마커 4개 배치** | LobbyMap에 `ATargetPoint` 4개 배치. 각각 이름: `WaitingRoomSlot_0`, `WaitingRoomSlot_1`, `WaitingRoomSlot_2`, `WaitingRoomSlot_3`. 각각 태그: `WaitingRoomSlot`. 카메라가 바라보는 영역에 좌→우 일렬 배치 (간격 약 150~200 유닛) |
| **기존 PlayerStart 정리** | LobbyMap에서 기존 PlayerStart를 **1개만 남기고** 나머지 삭제. 남은 1개는 슬롯 마커 근처 (카메라에 안 보이는 위치)에 배치 |

---

### 3.8 코드 변경 영향 요약 (기존 Phase 3 구현과의 차이)

현재 코드에 이미 Phase 3가 부분적으로 구현되어 있다. 새 Plan으로 변경이 필요한 부분:

| 파일 | 현재 상태 | 변경 필요 사항 |
|------|----------|---------------|
| `DRWaitingRoomCameraActor.h` | `PlayerSlotTransforms` TArray + `GetSlotTransform()` 포함 | **제거**: `PlayerSlotTransforms`, `GetSlotTransform()`. 카메라 전용으로 축소 |
| `DRWaitingRoomCameraActor.cpp` | 생성자에서 `PlayerSlotTransforms.SetNum(4)`, `GetSlotTransform()` 구현 | **제거**: 위 코드 삭제 |
| `DRLobbyGameMode.h` | `WaitingRoomCamera` 참조만 있음. `FindWaitingRoomCamera()` | **추가**: `WaitingRoomSlots` TArray, `FindWaitingRoomActors()` 통합. **변경**: `FindWaitingRoomCamera()` → `FindWaitingRoomActors()` |
| `DRLobbyGameMode.cpp` | `PositionPawnAtSlot()`이 `WaitingRoomCamera->GetSlotTransform()` 사용 | **변경**: `WaitingRoomSlots[SlotIndex]->GetActorLocation()` 사용. `FindWaitingRoomCamera()` → `FindWaitingRoomActors()`로 통합 (카메라 + 슬롯 마커 동시 탐색) |
| `DRPlayerController.h/cpp` | `ClientSetWaitingRoomView`, `ClientStartCameraTransitionToCharacter`, `RestoreDefaultInputMode` | **변경 없음** — 이미 구현된 코드 그대로 유지 |

---

### 검증 방법
PIE 2인 플레이:
1. 로비 진입 시 두 플레이어 모두 고정 카메라 시점으로 전환되는지 확인
2. 캐릭터들이 `WaitingRoomSlot_0`, `WaitingRoomSlot_1` 위치에 배치되는지 확인
3. 마우스 커서가 표시되고 캐릭터 이동이 불가한지 확인
4. 한 플레이어가 나가면 남은 플레이어가 재배치되는지 확인
5. 에디터에서 슬롯 마커를 이동하면 다음 테스트에서 캐릭터 위치가 변경되는지 확인

---

## Phase 4: 대기실 UI (위젯 + RPC 바인딩)

### 목표
대기실에서 표시되는 UI 위젯 생성. 캐릭터 선택 화살표, Kick 버튼, Power On 버튼, 방 코드 표시.

### 4.1 FWaitingRoomPlayerInfo 구조체 추가

**파일:** `Source/DaeRune/Public/Game/DRLobbyTypes.h`에 추가

```cpp
USTRUCT(BlueprintType)
struct FWaitingRoomPlayerInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString PlayerName;
    UPROPERTY(BlueprintReadOnly) EPlayerCharacterClass SelectedClass = EPlayerCharacterClass::GardenRobot;
    UPROPERTY(BlueprintReadOnly) bool bIsHost = false;
    UPROPERTY(BlueprintReadOnly) bool bIsLocalPlayer = false;
    UPROPERTY(BlueprintReadOnly) int32 SlotIndex = 0;

    // PlayerState 참조 (킥 시 식별용)
    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<APlayerState> OwningPlayerState = nullptr;
};
```

### 4.2 C++ 위젯 베이스 클래스 생성

**경로:** `Source/DaeRune/Public/UI/Widget/DRWaitingRoomWidget.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRLobbyTypes.h"
#include "DRWaitingRoomWidget.generated.h"

/**
 * 대기실 UI의 C++ 베이스 클래스.
 * 블루프린트에서 상속받아 WBP_WaitingRoom으로 구현.
 */
UCLASS()
class DAERUNE_API UDRWaitingRoomWidget : public UDRUserWidget
{
    GENERATED_BODY()

public:
    // 플레이어 슬롯 정보 갱신 (블루프린트에서 UI 업데이트 구현)
    UFUNCTION(BlueprintImplementableEvent, Category = "WaitingRoom")
    void RefreshPlayerSlots(const TArray<FWaitingRoomPlayerInfo>& PlayerInfos);

    // 로비 상태 변경 시 호출
    UFUNCTION(BlueprintImplementableEvent, Category = "WaitingRoom")
    void OnLobbyStateChanged(ELobbyState NewState);

    // 호스트 여부 설정 (Kick/PowerOn 버튼 표시 제어)
    UFUNCTION(BlueprintImplementableEvent, Category = "WaitingRoom")
    void SetIsHost(bool bIsHost);
};
```

### 4.3 ADRPlayerController에 위젯 관리 로직 추가

**파일:** `Source/DaeRune/Public/Player/DRPlayerController.h`

```cpp
class UDRWaitingRoomWidget;

protected:
    // 대기실 위젯 클래스 (블루프린트에서 설정)
    UPROPERTY(EditDefaultsOnly, Category = "UI|Lobby")
    TSubclassOf<UDRWaitingRoomWidget> WaitingRoomWidgetClass;

    // 현재 대기실 위젯 인스턴스
    UPROPERTY()
    TObjectPtr<UDRWaitingRoomWidget> WaitingRoomWidget;

public:
    void CreateWaitingRoomUI();
    void DestroyWaitingRoomUI();
    void RefreshWaitingRoomUI();
```

**파일:** `Source/DaeRune/Private/Player/DRPlayerController.cpp`

```cpp
void ADRPlayerController::CreateWaitingRoomUI()
{
    if (!IsLocalController()) return;
    if (WaitingRoomWidget) return; // 이미 존재
    if (!WaitingRoomWidgetClass) return;

    WaitingRoomWidget = CreateWidget<UDRWaitingRoomWidget>(this, WaitingRoomWidgetClass);
    if (WaitingRoomWidget)
    {
        WaitingRoomWidget->AddToViewport();

        // 호스트 여부 설정
        ADRGameStateBase* GS = GetWorld()->GetGameState<ADRGameStateBase>();
        bool bIsHost = GS && GS->IsPlayerHost(PlayerState);
        WaitingRoomWidget->SetIsHost(bIsHost);

        // LobbyState 변경 구독
        if (ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>())
        {
            LGS->OnLobbyStateChanged.AddDynamic(this, &ADRPlayerController::OnLobbyStateChangedForUI);
        }

        RefreshWaitingRoomUI();
    }
}

void ADRPlayerController::DestroyWaitingRoomUI()
{
    if (WaitingRoomWidget)
    {
        WaitingRoomWidget->RemoveFromParent();
        WaitingRoomWidget = nullptr;
    }
}

void ADRPlayerController::RefreshWaitingRoomUI()
{
    if (!WaitingRoomWidget) return;

    ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
    ADRGameStateBase* GS = GetWorld()->GetGameState<ADRGameStateBase>();
    if (!LGS || !GS) return;

    TArray<FWaitingRoomPlayerInfo> Infos;
    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS) continue;
        ADRPlayerState* DRPS = Cast<ADRPlayerState>(PS);
        if (!DRPS) continue;

        FWaitingRoomPlayerInfo Info;
        Info.PlayerName = PS->GetPlayerName();
        Info.SelectedClass = DRPS->GetSelectedPlayerClass();
        Info.bIsHost = GS->IsPlayerHost(PS);
        Info.bIsLocalPlayer = (PS == PlayerState);
        Info.OwningPlayerState = PS;
        // SlotIndex는 GameMode의 PlayerSlotMap에서 가져와야 하지만,
        // 클라이언트에서 접근 불가 → PlayerArray 순서로 대체
        Info.SlotIndex = Infos.Num();
        Infos.Add(Info);
    }

    WaitingRoomWidget->RefreshPlayerSlots(Infos);
}
```

### 4.4 OnLevelEntered에서 대기실 UI 생성 연동

**파일:** `Source/DaeRune/Private/Player/DRPlayerController.cpp`

기존 `OnLevelEntered()` 끝부분에 추가:
```cpp
// 기존 코드 유지 ...
RestoreDefaultInputMode();

// 로비에서 대기실 UI 생성
if (IsInLobby())
{
    ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
    if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
    {
        CreateWaitingRoomUI();
    }
}
```

### 4.5 Server RPC 추가: PowerOn, KickPlayer

**파일:** `Source/DaeRune/Public/Player/DRPlayerController.h`

```cpp
public:
    // 호스트가 Power On 클릭 시
    UFUNCTION(Server, Reliable)
    void ServerRequestPowerOn();

    // 호스트가 Kick 클릭 시
    UFUNCTION(Server, Reliable)
    void ServerRequestKickPlayer(APlayerState* TargetPlayerState);

protected:
    // LobbyState 변경 시 UI 처리
    UFUNCTION()
    void OnLobbyStateChangedForUI(ELobbyState NewState);
```

**파일:** `Source/DaeRune/Private/Player/DRPlayerController.cpp`

```cpp
void ADRPlayerController::ServerRequestPowerOn_Implementation()
{
    // 호스트 검증
    if (!IsLocalController() || !HasAuthority()) return;

    ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
    if (LobbyGM)
    {
        LobbyGM->PowerOn(this);
    }
}

void ADRPlayerController::ServerRequestKickPlayer_Implementation(APlayerState* TargetPlayerState)
{
    // 호스트 검증
    if (!IsLocalController() || !HasAuthority()) return;
    if (!TargetPlayerState) return;

    // TargetPlayerState → PlayerController 찾기
    APlayerController* TargetPC = Cast<APlayerController>(TargetPlayerState->GetOwner());
    ADRPlayerController* TargetDRPC = Cast<ADRPlayerController>(TargetPC);

    ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
    if (LobbyGM && TargetDRPC)
    {
        LobbyGM->KickPlayer(this, TargetDRPC);
    }
}

void ADRPlayerController::OnLobbyStateChangedForUI(ELobbyState NewState)
{
    if (NewState == ELobbyState::Transitioning || NewState == ELobbyState::FreeRoam)
    {
        DestroyWaitingRoomUI();
    }
}
```

### 4.6 블루프린트 위젯 (에디터 작업)

**에셋:** `Content/Blueprints/UI/WBP_WaitingRoom.uasset`

`UDRWaitingRoomWidget`을 상속받아 제작. 블루프린트에서 구현할 내용:

**설계 원칙:** UMG는 화면 위에 얹히는 정보 UI만 담당한다. 대기방의 배경, 슬롯 구조물, 플레이어 캐릭터는 월드에 실제 메시로 배치되어 있으며, UI를 통해 보인다.

**레이아웃 (투명 오버레이):**
```
┌─ 3D 월드 배경 (대기실 + 캐릭터들이 보임) ─┐
│                                            │
│  [Kick]           [Kick]                   │  ← 호스트에게만, 캐릭터 머리 위
│  Player1          Player2                  │  ← 캐릭터 머리 위 이름
│  ◀ Elementalist ▶ ◀ Warrior ▶              │  ← 캐릭터 발 아래, 자기것만 인터랙티브
│                                            │
│              [ Power On ]                  │  ← 하단 중앙, 호스트에게만
└────────────────────────────────────────────┘
```

- `RefreshPlayerSlots` 이벤트 구현: `FWaitingRoomPlayerInfo` 배열 받아 슬롯 UI 업데이트
- `SetIsHost` 이벤트 구현: Kick 버튼, Power On 버튼 가시성 토글
- 화살표 버튼 클릭 → `GetOwningPlayer()→RequestChangeClass(true/false)`
- Kick 버튼 클릭 → `GetOwningPlayer()→ServerRequestKickPlayer(SlotInfo.OwningPlayerState)`
- Power On 클릭 → `GetOwningPlayer()→ServerRequestPowerOn()`

### 4.7 WBP_WaitingRoom 블루프린트 위젯 상세 제작 가이드

> **UMG 프로퍼티 용어 정리**
> - 위젯 자체의 프로퍼티: Details 패널에서 위젯을 선택했을 때 보이는 것 (예: TextBlock의 `Text`, `Font`)
> - **슬롯(Slot) 프로퍼티**: 부모 컨테이너가 자식에게 부여하는 레이아웃 속성. 위젯을 선택하면 Details 패널 **최상단**의 `Slot` 섹션에 표시됨.
>   - Canvas Panel의 자식 → `Anchors`, `Offset Left/Top/Right/Bottom`, `Alignment`, `Size To Content`
>   - Vertical/Horizontal Box의 자식 → `Padding`, `Size (Auto/Fill)`, `Horizontal/Vertical Alignment`
>   - Overlay의 자식 → `Padding`, `Horizontal/Vertical Alignment`
> - 위젯에 고정 크기를 지정하려면 → **SizeBox**로 감싸서 `Width Override`, `Height Override` 사용

> **중요 전제**: 이 가이드는 대기방의 배경, 슬롯 구조물, 플레이어 캐릭터가 모두 월드에 실제 스태틱 메시 / 스켈레탈 메시로 배치되어 있다는 전제로 작성한다.
> 즉, **UMG는 화면 위에 얹히는 정보 UI만 담당**한다.
> 플레이어 이름은 캐릭터 머리 위, Kick 버튼은 그 위, 캐릭터 선택 UI(`◀ 클래스명 ▶`)는 캐릭터 아래에 보이도록 만든다.
> P1 / P2 / P3 / P4 같은 위치 표시는 위젯이 아니라 배경 메시 또는 월드 데칼/텍스트로 처리하는 것을 권장한다.

---

#### 4.7.1 위젯 생성: WBP_PlayerSlot (서브위젯, 먼저 생성)

1. 에디터에서 `Content/Blueprints/UI/` 폴더 열기
2. 우클릭 → **User Interface → Widget Blueprint**
3. 부모 클래스: **UserWidget** (기본값 그대로)
4. 이름: `WBP_PlayerSlot`

---

#### 4.7.2 WBP_PlayerSlot 디자이너 탭: 위젯 트리

아래 순서대로 위젯을 배치한다. 들여쓰기는 부모-자식 관계를 나타냄.
가운데 영역은 비워 둔다 — 실제 캐릭터 메시가 이 영역 뒤에서 보이게 된다.

```
[SizeBox_Root]  ◀ 루트 위젯: SizeBox
│   Details → Child Layout:
│     Width Override: 220  (체크박스 ON, 값 220)
│     Height Override: 320 (체크박스 ON, 값 320)
│   ※ 이 위젯의 가운데 영역은 비워 둔다. 실제 캐릭터 메시가 이 영역에 보이게 된다.
│
└── [Canvas_SlotRoot] Canvas Panel
    │
    ├── [VBox_HeadUI] VerticalBox
    │   │ Slot (Canvas Panel Slot):
    │   │   Anchors: Top Center (프리셋에서 상단 가운데 선택)
    │   │   Alignment: (0.5, 0.0)
    │   │   Position X: 110, Y: 0
    │   │   Size To Content: true
    │   │
    │   ├── [Btn_Kick] Button
    │   │   │ Slot (Vertical Box Slot):
    │   │   │   Size: Auto
    │   │   │   Horizontal Alignment: Center
    │   │   │   Padding: (0, 0, 0, 2)
    │   │   │ Details:
    │   │   │   Visibility: Collapsed
    │   │   │   Style → Normal:  Image = None, Tint = (0.60, 0.10, 0.10, 0.95)
    │   │   │   Style → Hovered: Image = None, Tint = (0.85, 0.18, 0.18, 1.00)
    │   │   │   Style → Pressed: Image = None, Tint = (0.40, 0.05, 0.05, 1.00)
    │   │   │
    │   │   └── [Txt_Kick] TextBlock
    │   │         Details:
    │   │           Text: "Kick"
    │   │           Font: Font Family = Roboto, Typeface = Bold, Size = 12
    │   │           Color and Opacity: (1, 1, 1, 1) 흰색
    │   │           Justification: Center
    │   │
    │   └── [Txt_PlayerName] TextBlock
    │         Slot (VBox Slot): Size=Auto, H-Align=Center
    │         Details:
    │           Text: "Player"
    │           Font: Roboto, Bold, Size=16
    │           Color and Opacity: (1, 1, 1, 1)
    │           Shadow Offset: (1, 1)
    │           Shadow Color and Opacity: (0, 0, 0, 0.8)
    │           Justification: Center
    │
    └── [HBox_ClassSelect] HorizontalBox
        │ Slot (Canvas Panel Slot):
        │   Anchors: Bottom Center (프리셋에서 하단 가운데 선택)
        │   Alignment: (0.5, 1.0)
        │   Position X: 110, Y: 320
        │   Size To Content: true
        │
        ├── [Btn_PrevClass] Button
        │   │ Slot (Horizontal Box Slot): Size=Auto, V-Align=Center
        │   │ Details:
        │   │   Visibility: Collapsed
        │   │   Style → Normal:  Tint=(0.12, 0.12, 0.12, 0.85)
        │   │   Style → Hovered: Tint=(0.22, 0.22, 0.22, 1.00)
        │   │   Style → Pressed: Tint=(0.08, 0.08, 0.08, 1.00)
        │   │
        │   └── [Txt_PrevArrow] TextBlock
        │         Details:
        │           Text: "◀"
        │           Font: Roboto, Bold, Size=18
        │           Color and Opacity: (1, 1, 1, 1)
        │
        ├── [SizeBox_ClassName] SizeBox
        │   │ Slot (Horizontal Box Slot): Size=Auto, V-Align=Center, Padding=(10,0,10,0)
        │   │ Details:
        │   │   Min Desired Width: 체크박스 ON, 값 120
        │   │
        │   └── [Txt_ClassName] TextBlock
        │         Details:
        │           Text: "Elementalist"
        │           Font: Roboto, Bold, Size=15
        │           Color and Opacity: (1, 1, 1, 1)
        │           Shadow Offset: (1, 1)
        │           Shadow Color and Opacity: (0, 0, 0, 0.8)
        │           Justification: Center
        │
        └── [Btn_NextClass] Button
            │ Slot (Horizontal Box Slot): Size=Auto, V-Align=Center
            │ Details:
            │   Visibility: Collapsed
            │   Style: (Btn_PrevClass와 동일)
            │
            └── [Txt_NextArrow] TextBlock
                  Details:
                    Text: "▶"
                    Font: Roboto, Bold, Size=18
                    Color and Opacity: (1, 1, 1, 1)
```

**IsVariable 체크할 위젯들** (Details 패널에서 위젯 이름 옆 눈 아이콘 클릭):
- `Btn_Kick`, `Txt_PlayerName`, `Txt_ClassName`, `Btn_PrevClass`, `Btn_NextClass`

---

#### 4.7.3 WBP_PlayerSlot 변수

이벤트 그래프 좌측 My Blueprint 패널 → Variables 섹션에서 추가:

| 변수명 | 타입 | Instance Editable | 기본값 | 용도 |
|--------|------|:-:|--------|------|
| `SlotIndex` | Integer | ✓ | 0 | 이 슬롯의 인덱스 (0~3) |
| `bIsOccupied` | Boolean | ✗ | false | 플레이어가 배정되었는지 |
| `CachedPlayerState` | Player State (Object Reference) | ✗ | None | Kick 시 식별용 캐시 |

---

#### 4.7.4 WBP_PlayerSlot 이벤트 그래프

##### (A) Custom Event: UpdateSlot

이벤트 그래프에서 우클릭 → **Add Custom Event** → 이름: `UpdateSlot`

**입력 파라미터 추가** (Details 패널 → Inputs):
- `Info` : 타입 = `Waiting Room Player Info` (구조체)
- `bShowKick` : 타입 = `Boolean`
- `bIsMySlot` : 타입 = `Boolean`

```
[UpdateSlot] (Info, bShowKick, bIsMySlot)
│
├── Set Visibility (Self) = Visible   ← 이 슬롯에 플레이어가 들어오면 다시 표시
├── SET bIsOccupied = true
├── SET CachedPlayerState = [Break WaitingRoomPlayerInfo → OwningPlayerState]
│
├── Txt_PlayerName → Set Text
│     Text = [Break WaitingRoomPlayerInfo → PlayerName]
│
├── Txt_ClassName → Set Text
│     Text = [SELECT on SelectedClass]
│            Elementalist → "Elementalist"
│            Warrior      → "Warrior"
│            Ranger       → "Ranger"
│     ※ 방법: Break WaitingRoomPlayerInfo → SelectedClass 핀을
│       Select 노드 (Enum)의 Index에 연결.
│       각 옵션에 문자열을 직접 입력한다.
│
├── Btn_Kick → Set Visibility
│     분기: [bShowKick] AND [NOT bIsMySlot]
│     → True: ESlateVisibility::Visible
│     → False: ESlateVisibility::Collapsed
│     ※ 호스트 화면에서만 다른 플레이어 머리 위 Kick 버튼이 보이게 된다.
│
├── Btn_PrevClass → Set Visibility
│     [bIsMySlot] ? Visible : Collapsed
│
├── Btn_NextClass → Set Visibility
│     [bIsMySlot] ? Visible : Collapsed
│
├── Txt_PlayerName → Set Color and Opacity
│     [bIsMySlot]
│     → True:  (1.0, 1.0, 1.0, 1.0)
│     → False: (0.86, 0.86, 0.86, 1.0)
│
└── Txt_ClassName → Set Color and Opacity
      [bIsMySlot]
      → True:  (1.0, 0.95, 0.75, 1.0)  ← 자기 슬롯은 살짝 강조
      → False: (1.0, 1.0, 1.0, 1.0)
      ※ 기존처럼 큰 프레임을 두르지 않고, 텍스트 강조만 준다.
```

##### (B) Custom Event: ClearSlot

이벤트 그래프에서 우클릭 → **Add Custom Event** → 이름: `ClearSlot` (입력 파라미터 없음)

```
[ClearSlot]
│
├── SET bIsOccupied = false
├── SET CachedPlayerState = None  ← 빈 Object Reference 할당 (핀 우클릭 → Clear)
│
├── Txt_PlayerName → Set Text("")
├── Txt_ClassName → Set Text("")
│
├── Btn_Kick → Set Visibility(Collapsed)
├── Btn_PrevClass → Set Visibility(Collapsed)
├── Btn_NextClass → Set Visibility(Collapsed)
│
└── Set Visibility (Self) = Collapsed
      ※ 빈 슬롯은 아예 숨긴다. 빈 자리는 월드의 구조물만 남고,
        플레이어 UI는 표시되지 않는다.
```

##### (C) Btn_Kick → OnClicked 이벤트

디자이너에서 `Btn_Kick` 선택 → Details → Events → On Clicked 옆 **+** 클릭

```
[On Clicked (Btn_Kick)]
│
├── Is Valid (CachedPlayerState) → Is Not Valid 핀 → Return
│
├── Get Owning Player → Cast to DRPlayerController
│
└── [Cast 성공] → Server Request Kick Player (Target=CastResult, TargetPlayerState=CachedPlayerState)
```

##### (D) Btn_PrevClass → OnClicked 이벤트

디자이너에서 `Btn_PrevClass` 선택 → Details → Events → On Clicked 옆 **+** 클릭

```
[On Clicked (Btn_PrevClass)]
│
└── Get Owning Player → Cast to DRPlayerController
    └── [Cast 성공] → Request Change Class (Target=CastResult, bNext=false)
```

##### (E) Btn_NextClass → OnClicked 이벤트

```
[On Clicked (Btn_NextClass)]
│
└── Get Owning Player → Cast to DRPlayerController
    └── [Cast 성공] → Request Change Class (Target=CastResult, bNext=true)
```

> **주의**: `RequestChangeClass`는 내부적으로 `ServerRequestChangeClass` Server RPC를 호출하므로
> 블루프린트에서 직접 `ServerRequestChangeClass`를 호출하지 않아도 됨.
> 캐릭터 순환 순서: `→ Elementalist → Warrior → Ranger → Elementalist → ...`

---

#### 4.7.5 위젯 생성: WBP_WaitingRoom (메인 위젯)

1. `Content/Blueprints/UI/` 폴더에서 우클릭 → **User Interface → Widget Blueprint**
2. 부모 클래스: **DRWaitingRoomWidget** 검색 후 선택
3. 이름: `WBP_WaitingRoom`

---

#### 4.7.6 WBP_WaitingRoom 디자이너 탭: 위젯 트리

Canvas Panel 루트에 PlayerSlot들을 **절대 좌표**로 배치한다.
각 슬롯의 Position은 대기실 카메라 구도에서 캐릭터가 서 있는 위치에 맞게 직접 조정한다.

```
[CanvasPanel] (루트, 자동 생성됨)
│
├── [PlayerSlot_0] WBP_PlayerSlot     ◀ IsVariable 체크
│   │ Slot (Canvas Panel Slot):
│   │   Anchors: Center (프리셋에서 정가운데 선택)
│   │   Alignment: (0.5, 0.5)
│   │   Position: (-360, -40)
│   │   Auto Size: true (Size To Content 체크)
│   │   ZOrder: 5
│   │   ※ 1920x1080 기준 예시. 왼쪽 첫 번째 캐릭터 위/아래에 오도록 조정.
│
├── [PlayerSlot_1] WBP_PlayerSlot     ◀ IsVariable 체크
│   │ Slot (Canvas Panel Slot):
│   │   Anchors: Center
│   │   Alignment: (0.5, 0.5)
│   │   Position: (-120, -30)
│   │   Auto Size: true
│   │   ZOrder: 5
│
├── [PlayerSlot_2] WBP_PlayerSlot     ◀ IsVariable 체크
│   │ Slot (Canvas Panel Slot):
│   │   Anchors: Center
│   │   Alignment: (0.5, 0.5)
│   │   Position: (120, -30)
│   │   Auto Size: true
│   │   ZOrder: 5
│
├── [PlayerSlot_3] WBP_PlayerSlot     ◀ IsVariable 체크
│   │ Slot (Canvas Panel Slot):
│   │   Anchors: Center
│   │   Alignment: (0.5, 0.5)
│   │   Position: (360, -40)
│   │   Auto Size: true
│   │   ZOrder: 5
│
└── [SizeBox_PowerOn] SizeBox
    │ Slot (Canvas Panel Slot):
    │   Anchors: Bottom Center (프리셋에서 하단 가운데 선택)
    │   Alignment: (0.5, 1.0)
    │   Position: (0, -28)
    │   Auto Size: false
    │   Size X: 260
    │   Size Y: 56
    │   ZOrder: 10
    │
    └── [Btn_PowerOn] Button          ◀ IsVariable 체크
        │ Details:
        │   Visibility: Collapsed  ← 기본. 호스트만 Visible로 변경됨
        │   Style → Normal:  Image = None, Tint = (0.10, 0.10, 0.10, 0.85)
        │   Style → Hovered: Image = None, Tint = (0.18, 0.18, 0.18, 0.95)
        │   Style → Pressed: Image = None, Tint = (0.05, 0.05, 0.05, 1.00)
        │
        └── [Txt_PowerOn] TextBlock
              Details:
                Text: "Power On"
                Font: Roboto, Bold, Size=28
                Color and Opacity: (1, 1, 1, 1)
                Shadow Offset: (1, 1)
                Shadow Color and Opacity: (0, 0, 0, 0.35)
                Justification: Center
```

**IsVariable 체크 요약** (이벤트 그래프에서 참조해야 하는 위젯들):

| 위젯 이름 | 타입 | 용도 |
|-----------|------|------|
| `Btn_PowerOn` | Button | 가시성 제어 + OnClicked 이벤트 |
| `PlayerSlot_0` | WBP_PlayerSlot | 슬롯 0 참조 |
| `PlayerSlot_1` | WBP_PlayerSlot | 슬롯 1 참조 |
| `PlayerSlot_2` | WBP_PlayerSlot | 슬롯 2 참조 |
| `PlayerSlot_3` | WBP_PlayerSlot | 슬롯 3 참조 |

---

#### 4.7.7 WBP_WaitingRoom 변수

이벤트 그래프 → My Blueprint → Variables에서 추가:

| 변수명 | 타입 | 기본값 | 용도 |
|--------|------|--------|------|
| `bCachedIsHost` | Boolean | false | SetIsHost에서 저장, RefreshPlayerSlots에서 사용 |
| `CachedPlayerCount` | Integer | 0 | 플레이어 수 변경 감지용 (폴링) |
| `PlayerSlots` | Array of WBP_PlayerSlot | — | 4개 슬롯 참조 배열 (Construct에서 초기화) |

---

#### 4.7.8 WBP_WaitingRoom 이벤트 그래프

##### (A) Event Construct

```
[Event Construct]
│
│ ── 1단계: PlayerSlots 배열 초기화 ──
│
├── Make Array (WBP_PlayerSlot)
│     [0]=PlayerSlot_0, [1]=PlayerSlot_1, [2]=PlayerSlot_2, [3]=PlayerSlot_3
│     → SET PlayerSlots
│
│ ── 2단계: 시작 시 모든 슬롯 숨김 ──
│
├── For Each Loop (PlayerSlots)
│     └── Clear Slot
│
│ ── 3단계: 클래스 변경 델리게이트 구독 ──
│
├── Get World → Get Game State → Get Player Array
│     → For Each Loop
│       → Cast to DRPlayerState
│       → [Cast 성공] → 핀 드래그 → "Assign On Player Class Changed"
│           → [OnAnyPlayerClassChanged] (Custom Event 자동 생성됨)
│               └── Get Owning Player → Get Player Controller
│                   → Cast to DRPlayerController
│                   → Refresh Waiting Room UI
│
│ ── 4단계: 플레이어 수 변경 감지 타이머 ──
│
└── Set Timer by Function Name
      Function Name: "PollPlayerCount"
      Time: 1.0
      Looping: true
```

##### (B) Custom Event: PollPlayerCount

```
[PollPlayerCount]
│
├── Get World → Get Game State → Get Player Array → Num
│
├── [결과] ≠ CachedPlayerCount ?
│     Branch:
│     │
│     ├── True:
│     │   ├── SET CachedPlayerCount = [Num 결과]
│     │   └── Get Owning Player → Get Player Controller
│     │       → Cast to DRPlayerController
│     │       → Refresh Waiting Room UI
│     │
│     └── False: (아무것도 안 함)
```

##### (C) Event RefreshPlayerSlots (C++에서 호출되는 BlueprintImplementableEvent)

이벤트 그래프에서 우클릭 → "RefreshPlayerSlots" 검색하면 오버라이드 가능한 이벤트로 나타남.

```
[Event Refresh Player Slots] (Input: PlayerInfos - Array of WaitingRoomPlayerInfo)
│
│ ── 1단계: 채워진 슬롯 갱신 ──
│
├── For Each Loop (PlayerInfos)
│   │  Array Index 핀 사용
│   │
│   ├── Array Index < PlayerSlots.Num (Length) ? Branch
│   │     False → 무시 (4명 초과 방지)
│   │
│   └── True →
│       PlayerSlots → Get (Array Index)
│       → Update Slot (
│             Info = Array Element,
│             bShowKick = bCachedIsHost,
│             bIsMySlot = [Break WaitingRoomPlayerInfo → bIsLocalPlayer]
│         )
│
│ ── 2단계: 비어있는 슬롯 정리 ──
│
├── PlayerInfos → Num (Length) → 저장 (Local Variable: FilledCount)
│
├── For Loop (Integer)
│     First Index = FilledCount
│     Last Index = 3
│     │
│     └── PlayerSlots → Get (Index)
│         → Clear Slot
│
│ ── 완료 ──
│
└── (실행 완료)
      ※ 중요: PlayerInfos 배열은 항상 왼쪽부터 빈자리 없이 정렬된 상태로 전달된다고 가정한다.
        따라서 Kick 이후 Array Index 기준으로 다시 뿌려 주기만 해도
        남은 플레이어들이 자동으로 왼쪽으로 당겨진 배치처럼 보이게 된다.
```

##### (D) Event SetIsHost (C++ BlueprintImplementableEvent)

```
[Event Set Is Host] (Input: bIsHost - Boolean)
│
├── SET bCachedIsHost = bIsHost
│
├── Btn_PowerOn → Set Visibility
│     [bIsHost] ?
│     → True: ESlateVisibility::Visible
│     → False: ESlateVisibility::Collapsed
│
└── Get Owning Player → Cast to DRPlayerController
    └── [Cast 성공] → Refresh Waiting Room UI
      ※ 호스트 여부가 바뀌면 Kick 버튼 표시 여부도 다시 계산해야 하므로 한 번 더 갱신한다.
```

##### (E) Event OnLobbyStateChanged (C++ BlueprintImplementableEvent)

```
[Event On Lobby State Changed] (Input: NewState - ELobbyState)
│
└── Switch on ELobbyState (NewState):
    │
    ├── WaitingRoom: (아무것도 안 함)
    │
    ├── Transitioning:
    │   └── Btn_PowerOn → Set Is Enabled (false)
    │
    └── FreeRoam: (C++ DestroyWaitingRoomUI()가 처리하므로 도달 안 함)
```

##### (F) Btn_PowerOn → OnClicked

디자이너에서 `Btn_PowerOn` 선택 → Details → Events → On Clicked 옆 **+** 클릭

```
[On Clicked (Btn_PowerOn)]
│
├── Btn_PowerOn → Set Is Enabled (false)   ← 중복 클릭 방지
│
└── Get Owning Player → Cast to DRPlayerController
    └── [Cast 성공] → Server Request Power On
```

---

#### 4.7.9 스타일/비주얼 참고

| 요소 | 에디터 설정 위치 | 값 |
|------|-----------------|-----|
| 전체 배경 | 없음 (3D 대기실 장면이 배경) | — |
| 플레이어 슬롯 프레임 | 사용하지 않음 | 투명 UI만 유지 |
| 빈 슬롯 표시 | ClearSlot 이벤트에서 `Self Collapsed` | 빈 자리는 구조물/배경만 보임 |
| 자기 슬롯 강조 | Txt_ClassName 색상 동적 변경 | (1.0, 0.95, 0.75, 1.0) |
| Kick 버튼 | Btn_Kick → Style → Normal/Hovered/Pressed → Tint | 작은 빨간 계열 버튼 |
| 플레이어 이름 | Txt_PlayerName → Font | Roboto Bold 16, 흰색 + 그림자 |
| 클래스 이름 | Txt_ClassName → Font | Roboto Bold 15, 흰색 |
| Power On 버튼 | Btn_PowerOn → Style | 짙은 회색 반투명 버튼 |
| Power On 텍스트 | Txt_PowerOn → Font | Roboto Bold 28 |
| 슬롯 위치 | 각 PlayerSlot의 Canvas Slot → Position | 카메라 구도에 맞게 직접 조정 |

---

#### 4.7.10 BP_DRPlayerController에 위젯 클래스 설정

1. Content Browser에서 `BP_DRPlayerController` 더블클릭하여 열기
2. 상단 툴바의 **Class Defaults** 버튼 클릭
3. Details 패널에서 **UI|Lobby** 카테고리 검색
4. `Waiting Room Widget Class` 드롭다운 → **WBP_WaitingRoom** 선택
5. **Compile** → **Save**

---

#### 4.7.11 체크리스트

**WBP_PlayerSlot:**
- [ ] SizeBox_Root 생성 (Width=220, Height=320)
- [ ] Canvas_SlotRoot 생성
- [ ] Btn_Kick: 기본 Collapsed, 작은 빨간 스타일, OnClicked 이벤트 연결
- [ ] Txt_PlayerName: 머리 위 표시용 텍스트 설정
- [ ] Btn_PrevClass, Btn_NextClass: 기본 Collapsed, OnClicked → RequestChangeClass
- [ ] Txt_ClassName: Elementalist / Warrior / Ranger 표시
- [ ] Custom Event `UpdateSlot` 구현 (3개 파라미터)
- [ ] Custom Event `ClearSlot` 구현

**WBP_WaitingRoom:**
- [ ] Canvas 루트에 4개 PlayerSlot 배치, 각각 IsVariable 체크
- [ ] 각 PlayerSlot Canvas Position을 대기실 카메라 구도에 맞게 조정
- [ ] Btn_PowerOn: 하단 중앙 배치, 기본 Collapsed
- [ ] Event Construct: PlayerSlots 배열 초기화 + 초기 Clear + 델리게이트 바인딩 + 타이머
- [ ] Event RefreshPlayerSlots: For Each로 슬롯 갱신 + 빈 슬롯 Clear
- [ ] Event SetIsHost: bCachedIsHost 저장 + PowerOn 가시성 + UI 재갱신
- [ ] Event OnLobbyStateChanged: Transitioning 시 PowerOn 비활성화
- [ ] Btn_PowerOn OnClicked: ServerRequestPowerOn 호출
- [ ] PollPlayerCount: 1초 타이머로 인원수 변경 감지

**에디터 설정:**
- [ ] BP_DRPlayerController → WaitingRoomWidgetClass = WBP_WaitingRoom
- [ ] PIE 2인 테스트: 호스트/클라이언트 양쪽 UI 확인
- [ ] 호스트 화면에서만 다른 플레이어 위 Kick 버튼 표시 확인
- [ ] 일반 플레이어 화면에서는 Kick 버튼이 전혀 보이지 않는지 확인
- [ ] 자신의 슬롯에서만 ◀ / ▶ 버튼이 표시되고 클릭 가능한지 확인
- [ ] 클래스 변경 시 Elementalist → Warrior → Ranger 순환 확인
- [ ] Kick 실행 후 남은 플레이어 UI가 왼쪽부터 재배치되는지 확인
- [ ] Power On 버튼이 호스트에게만 보이는지 확인

### 검증 방법
PIE 2인 또는 3인 플레이. 대기실 UI 표시. 호스트에게만 Kick / Power On 버튼 보임.
각 플레이어는 자기 슬롯에서만 캐릭터를 변경할 수 있어야 하며, 다른 플레이어 슬롯의 클래스명은 읽기 전용이어야 한다.
Kick 실행 시 대상 플레이어는 메인 메뉴로 이동하고, 남은 플레이어들의 슬롯 UI가 빈 자리 없이 왼쪽으로 정렬되어야 한다.

---

## Phase 5: 킥(Kick) 시스템

### 목표
호스트가 대기실에서 다른 플레이어를 추방할 수 있는 기능 구현.

### 5.1 ADRLobbyGameMode::KickPlayer 구현

**파일:** `Source/DaeRune/Private/Game/DRLobbyGameMode.cpp`

```cpp
void ADRLobbyGameMode::KickPlayer(ADRPlayerController* Requester, ADRPlayerController* TargetPlayer)
{
    if (!HasAuthority()) return;

    // 호스트 검증
    if (!Requester || !Requester->IsLocalController()) return;

    // 대기실 상태에서만 가능
    ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
    if (!LGS || LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

    // 자기 자신은 킥 불가
    if (Requester == TargetPlayer) return;

    // 대상이 유효한지 확인
    if (!TargetPlayer) return;

    // 슬롯에서 제거
    PlayerSlotMap.Remove(TargetPlayer);

    // 클라이언트에게 킥 알림 → 메인 메뉴로 이동
    TargetPlayer->ClientKicked(TEXT("You have been kicked by the host."));

    // 나머지 플레이어 재배치
    RepositionAllPlayers();

    // 모든 클라이언트의 대기실 UI 갱신
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
        {
            PC->RefreshWaitingRoomUI(); // 서버에서 직접 호출 (로컬)
            // 클라이언트 UI 갱신은 PlayerState 리플리케이션으로 자동 처리
        }
    }
}
```

### 5.2 ADRPlayerController에 ClientKicked RPC 추가

**파일:** `Source/DaeRune/Public/Player/DRPlayerController.h`

```cpp
public:
    // 서버 → 클라이언트: 킥 당했음을 알림
    UFUNCTION(Client, Reliable)
    void ClientKicked(const FString& Reason);
```

**파일:** `Source/DaeRune/Private/Player/DRPlayerController.cpp`

```cpp
void ADRPlayerController::ClientKicked_Implementation(const FString& Reason)
{
    // 대기실 UI 제거
    DestroyWaitingRoomUI();

    // 세션 떠나기
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UMultiplayerSessionsSubsystem* Subsystem = GI->GetSubsystem<UMultiplayerSessionsSubsystem>())
        {
            Subsystem->LeaveServer();
            // LeaveServer()가 내부적으로 세션 파괴 + 메인 메뉴 이동 처리
        }
    }
}
```

**참고:** `UMultiplayerSessionsSubsystem::LeaveServer()` 기존 구현:
```cpp
void UMultiplayerSessionsSubsystem::LeaveServer()
{
    StopVoiceChat();
    DestroySession();
    // + ClientTravel to MainMenu
}
```
이미 메인 메뉴로 이동하는 로직이 포함되어 있으므로 그대로 활용.

### 5.3 RepositionAllPlayers에서 카메라 재설정 포함

킥 후 재배치 시 남은 플레이어들의 위치가 바뀌므로, 각 PC에게 `ClientSetWaitingRoomView`를 다시 호출할 필요는 없다 (카메라는 고정이므로). 하지만 **캐릭터 위치가 바뀌면** 월드스페이스 UI가 있다면 자동 갱신.

### 검증 방법
PIE 3인 플레이. 호스트가 Player2를 킥. Player2가 메인 메뉴로 이동. Player1, Player3의 배치가 빈 자리 없이 재정렬.

---

## Phase 6: Power On 전환 (카메라 연출 + 상태 전환)

### 목표
호스트가 Power On을 누르면 대기실 → 자유 조작으로 전환. 부드러운 카메라 연출 포함.

### 6.1 ADRLobbyGameMode::PowerOn 구현

**파일:** `Source/DaeRune/Private/Game/DRLobbyGameMode.cpp`

```cpp
void ADRLobbyGameMode::PowerOn(ADRPlayerController* Requester)
{
    if (!HasAuthority()) return;

    // 호스트 검증
    if (!Requester || !Requester->IsLocalController()) return;

    // 대기실 상태에서만 가능
    ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
    if (!LGS || LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

    // 1. 세션 참가 차단
    BlockJoinInProgress();

    // 2. 전환 상태로 변경
    LGS->SetLobbyState(ELobbyState::Transitioning);

    // 3. 모든 플레이어에게 카메라 전환 시작 명령
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
        {
            // 이동 재활성화
            if (APawn* Pawn = PC->GetPawn())
            {
                if (UCharacterMovementComponent* MovementComp =
                    Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
                {
                    MovementComp->SetMovementMode(MOVE_Walking);
                }
            }

            // 카메라 전환 명령
            PC->ClientStartCameraTransitionToCharacter();
        }
    }

    // 4. 전환 완료 후 FreeRoam으로 상태 변경 (카메라 연출 시간만큼 딜레이)
    FTimerHandle TransitionTimerHandle;
    GetWorldTimerManager().SetTimer(
        TransitionTimerHandle,
        [LGS]()
        {
            if (IsValid(LGS))
            {
                LGS->SetLobbyState(ELobbyState::FreeRoam);
            }
        },
        2.0f, // 카메라 전환 시간
        false
    );
}
```

### 6.2 ClientStartCameraTransitionToCharacter 구현

**파일:** `Source/DaeRune/Private/Player/DRPlayerController.cpp`

```cpp
void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
    APawn* MyPawn = GetPawn();
    if (!MyPawn) return;

    // UE5 내장 블렌드 기능으로 고정 카메라 → 캐릭터 카메라로 부드럽게 전환
    // BlendTime = 1.5초, 선형 보간
    SetViewTargetWithBlend(MyPawn, 1.5f, EViewTargetBlendFunction::VTBlend_EaseInOut);

    // 전환 완료 후 인풋 모드 변경
    FTimerHandle InputTimerHandle;
    GetWorldTimerManager().SetTimer(
        InputTimerHandle,
        [this]()
        {
            if (!IsValid(this)) return;
            SetInputMode(FInputModeGameOnly());
            SetShowMouseCursor(false);
        },
        1.5f,
        false
    );
}
```

**`SetViewTargetWithBlend` 사용 이유:**
- UE5에 내장된 카메라 블렌드 함수
- 현재 ViewTarget (고정 카메라) → 새 ViewTarget (캐릭터의 SpringArm 카메라)으로 자동 보간
- `EViewTargetBlendFunction::VTBlend_EaseInOut`으로 부드러운 가감속 효과
- 별도 Timeline이나 커스텀 보간 불필요

### 6.3 대기실 UI는 LobbyState 변경 시 자동 제거

Phase 4.5에서 구현한 `OnLobbyStateChangedForUI`가 `Transitioning` 또는 `FreeRoam` 시 `DestroyWaitingRoomUI()` 호출.

### 검증 방법
PIE 2인 플레이. 대기실에서 호스트가 Power On. 카메라가 1.5초간 부드럽게 자신의 캐릭터로 전환. 전환 완료 후 캐릭터 이동 가능. 세션 참가 차단 확인.

---

## Phase 7: 캐릭터 BP 교체 (클래스 선택 → 외형/능력 변경)

### 목표
대기실에서 클래스를 변경하면 **캐릭터 BP 자체가 교체**되어 외형이 즉시 바뀌고, 게임 시작 시 선택한 클래스의 어트리뷰트/어빌리티가 적용된다.

### ⚠️ 7.0 런타임 메시 교체 vs BP 교체 — 설계 결정

**런타임 `SetSkeletalMesh()` 방식을 채택하지 않는 이유:**

`ADRCharacter`의 컴포넌트 구조:
```
CapsuleComponent (Root)
├── CameraBoom (오프셋: 30, 0, 50)
│   └── FollowCamera
│       └── FirstPersonMesh (OnlyOwnerSee, 1인칭 전용)
└── GetMesh() (OwnerNoSee, 3인칭 전용)
    └── Weapon (소켓 부착)
```

| 문제 | 설명 |
|------|------|
| 1인칭 메시 미교체 | `FirstPersonMesh`는 별도 컴포넌트. `GetMesh()->SetSkeletalMesh()`으로 3인칭만 바뀌고 **1인칭은 그대로** |
| 카메라 위치 부정합 | CameraBoom 오프셋이 캐릭터 체형에 따라 달라야 함. 런타임에 조정하면 에디터 미리보기 불가 |
| 소켓 호환성 | 새 스켈레톤에 기존 소켓(WeaponHandSocket 등)이 없으면 무기 부착 깨짐 |

**채택 방식: 클래스별 별도 캐릭터 BP**

각 클래스마다 `ADRCharacter`를 상속하는 BP를 만들어 **1인칭/3인칭 메시, 카메라 오프셋, AnimBP, 소켓**을 에디터에서 개별 설정. 클래스 변경 시 기존 폰을 Destroy하고 새 BP 폰을 Spawn → Possess.

```
BP_DRCharacter_GardenRobot (ADRCharacter 상속)
├── GetMesh() → GardenRobot 3인칭 SkeletalMesh
├── FirstPersonMesh → GardenRobot 1인칭 SkeletalMesh
├── CameraBoom → GardenRobot 전용 오프셋
├── AnimClass → GardenRobot 전용 AnimBP
└── Weapon 소켓 → GardenRobot 스켈레톤 기준

BP_DRCharacter_VendingMachineRobot (ADRCharacter 상속)
├── GetMesh() → VM 3인칭 SkeletalMesh
├── FirstPersonMesh → VM 1인칭 SkeletalMesh
├── CameraBoom → VM 전용 오프셋
├── AnimClass → VM 전용 AnimBP
└── Weapon 소켓 → VM 스켈레톤 기준
```

### 7.1 ADRLobbyGameMode에 폰 교체 함수 추가

**파일:** `Source/DaeRune/Public/Game/DRLobbyGameMode.h`

```cpp
public:
    // 플레이어 클래스 변경 시 폰 교체 (서버 전용)
    void RespawnPlayerWithClass(ADRPlayerController* PC, EPlayerCharacterClass NewClass);
```

**파일:** `Source/DaeRune/Private/Game/DRLobbyGameMode.cpp`

```cpp
void ADRLobbyGameMode::RespawnPlayerWithClass(ADRPlayerController* PC, EPlayerCharacterClass NewClass)
{
    if (!HasAuthority() || !PC) return;

    // 1. PlayerCharacterClassInfo에서 해당 클래스의 BP 가져오기
    if (!PlayerCharacterClassInfo) return;
    TSubclassOf<ADRCharacter>* BPClassPtr = PlayerCharacterClassInfo->CharacterBPClasses.Find(NewClass);
    if (!BPClassPtr || !*BPClassPtr) return;

    // 2. 현재 폰의 위치/회전 저장
    APawn* OldPawn = PC->GetPawn();
    FTransform SpawnTransform = OldPawn ? OldPawn->GetActorTransform() : FTransform::Identity;

    // 3. 기존 폰 제거
    if (OldPawn)
    {
        PC->UnPossess();
        OldPawn->Destroy();
    }

    // 4. 새 BP 폰 스폰
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ADRCharacter* NewPawn = GetWorld()->SpawnActor<ADRCharacter>(*BPClassPtr, SpawnTransform, SpawnParams);
    if (!NewPawn) return;

    // 5. Possess (이 안에서 InitAbilityActorInfo → InitializeDefaultAttributes → PossessedBy 체인 실행)
    PC->Possess(NewPawn);

    // 6. 대기실 상태면 이동 비활성화 + 슬롯 위치 재설정
    ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
    if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
    {
        int32* SlotIndex = PlayerSlotMap.Find(PC);
        if (SlotIndex)
        {
            PositionPawnAtSlot(NewPawn, *SlotIndex);
        }

        // 고정 카메라 재설정
        if (WaitingRoomCamera)
        {
            PC->ClientSetWaitingRoomView(WaitingRoomCamera);
        }
    }
}
```

### 7.2 SetSelectedPlayerClass에서 폰 교체 트리거

**파일:** `Source/DaeRune/Private/Player/DRPlayerState.cpp`

```cpp
void ADRPlayerState::SetSelectedPlayerClass(EPlayerCharacterClass NewClass)
{
    if (!HasAuthority()) return;
    if (SelectedPlayerClass == NewClass) return;

    SelectedPlayerClass = NewClass;
    OnPlayerClassChanged.Broadcast(this, SelectedPlayerClass);

    // ★ 로비 대기실에서만 폰 교체 실행
    ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
    if (LobbyGM)
    {
        // PlayerState → 소유 PlayerController 찾기
        APlayerController* PC = Cast<APlayerController>(GetOwner());
        if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
        {
            LobbyGM->RespawnPlayerWithClass(DRPC, NewClass);
        }
    }
}
```

**`OnRep_SelectedPlayerClass`는 외형 교체를 트리거하지 않는다** — 폰 교체는 서버 권한 작업이므로 `SetSelectedPlayerClass`에서 직접 처리. 클라이언트에서는 새로운 폰이 리플리케이트되어 자동으로 보인다.

```cpp
void ADRPlayerState::OnRep_SelectedPlayerClass()
{
    // 클라이언트에서는 델리게이트 브로드캐스트만 (UI 갱신용)
    OnPlayerClassChanged.Broadcast(this, SelectedPlayerClass);
}
```

### 7.3 ADRCharacter에 PlayerCharacterClass 멤버 추가

**파일:** `Source/DaeRune/Public/Character/DRCharacter.h`

```cpp
public:
    // 플레이어 캐릭터 클래스 (EPlayerCharacterClass)
    // BP별로 에디터에서 설정하거나, InitAbilityActorInfo에서 PlayerState에서 읽어옴
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Character Class Defaults")
    EPlayerCharacterClass PlayerCharacterClass = EPlayerCharacterClass::GardenRobot;
```

### 7.4 플레이어 초기화 경로를 UPlayerCharacterClassInfo 기반으로 리팩터링

**⚠️ Phase 2.0의 분석에 따라, 플레이어가 UPlayerCharacterClassInfo를 사용하도록 변경해야 한다.**

현재 플레이어의 `InitializeDefaultAttributes()`는 BP에 직접 설정된 GE를 사용하므로 `CharacterClass`를 바꿔도 어트리뷰트에 영향이 없다. 이를 `UPlayerCharacterClassInfo` 경로로 변경한다.

#### 7.4.1 ADRCharacter::InitializeDefaultAttributes() 오버라이드 추가

**파일:** `Source/DaeRune/Public/Character/DRCharacter.h`

```cpp
protected:
    virtual void InitializeDefaultAttributes() const override;
```

**파일:** `Source/DaeRune/Private/Character/DRCharacter.cpp`

```cpp
void ADRCharacter::InitializeDefaultAttributes() const
{
    // UPlayerCharacterClassInfo 데이터 에셋 기반 초기화 (EPlayerCharacterClass 타입)
    UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(this, PlayerCharacterClass, Level, AbilitySystemComponent);
}
```

#### 7.4.2 ADRCharacter::InitAbilityActorInfo() 수정

```cpp
void ADRCharacter::InitAbilityActorInfo()
{
    ADRPlayerState* DRPlayerState = GetPlayerState<ADRPlayerState>();
    if (!DRPlayerState) return;

    // ... 기존 ASC 초기화 코드 유지 ...

    // ★ PlayerState의 선택된 클래스를 PlayerCharacterClass에 반영
    PlayerCharacterClass = DRPlayerState->GetSelectedPlayerClass();

    // ... 기존 HUD 초기화 코드 유지 ...

    // 기본 어트리뷰트 초기화 (UPlayerCharacterClassInfo 기반으로 동작)
    InitializeDefaultAttributes();
}
```

#### 7.4.3 ADRCharacter::PossessedBy() 수정 - 어빌리티도 UPlayerCharacterClassInfo 사용

```cpp
void ADRCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    InitAbilityActorInfo();

    // ★ 기존 AddCharacterAbilities() 대신 UPlayerCharacterClassInfo 기반으로 변경
    // AddCharacterAbilities();  ← 기존 코드 (BP 직접 설정된 어빌리티 사용)
    if (HasAuthority())
    {
        UDRAbilitySystemLibrary::GivePlayerStartupAbilities(this, AbilitySystemComponent, PlayerCharacterClass);
    }

    InitializeMoveSpeedBinding();
}
```

#### 7.4.4 ADRCharacter 생성자 수정

기존 `CharacterClass = ECharacterClass::Elementalist;` 라인을 **삭제**한다. 플레이어는 더 이상 `ECharacterClass`를 사용하지 않는다.

```cpp
ADRCharacter::ADRCharacter()
{
    // ... 기존 코드 유지 ...

    // CharacterClass = ECharacterClass::Elementalist;  ← ★ 삭제
    // 플레이어는 EPlayerCharacterClass PlayerCharacterClass를 사용
    // 기본값은 헤더에서 EPlayerCharacterClass::GardenRobot으로 설정됨

    // ... 기존 코드 유지 ...
}
```

#### 7.4.5 UPlayerCharacterClassInfo 데이터 에셋 업데이트 (에디터 작업)

`DA_PlayerCharacterClassInfo` (`UPlayerCharacterClassInfo` 타입) 데이터 에셋에 설정할 내용:

**CharacterClassInformation (TMap<EPlayerCharacterClass, FCharacterClassDefaultInfo>):**

| EPlayerCharacterClass | 설정 내용 |
|----------------------|-----------|
| GardenRobot | PrimaryAttributes GE, VitalAttributes GE, StartupAbilities |
| VendingMachineRobot | PrimaryAttributes GE, VitalAttributes GE, StartupAbilities |

**CharacterBPClasses (TMap<EPlayerCharacterClass, TSubclassOf<ADRCharacter>>):**

| EPlayerCharacterClass | BP 클래스 |
|----------------------|-----------|
| GardenRobot | `BP_DRCharacter_GardenRobot` |
| VendingMachineRobot | `BP_DRCharacter_VendingMachineRobot` |

#### 7.4.6 캐릭터 BP 생성 (에디터 작업)

| BP 이름 | 설정 내용 |
|---------|-----------|
| `BP_DRCharacter_GardenRobot` | ADRCharacter 상속. GardenRobot 3인칭 메시, 1인칭 메시, CameraBoom 오프셋, AnimBP, 무기 소켓 위치 설정 |
| `BP_DRCharacter_VendingMachineRobot` | ADRCharacter 상속. VendingMachineRobot 3인칭 메시, 1인칭 메시, CameraBoom 오프셋, AnimBP, 무기 소켓 위치 설정 |

**각 BP에서 설정할 항목:**
- `GetMesh()` → 해당 클래스의 3인칭 SkeletalMesh + AnimBP
- `FirstPersonMesh` → 해당 클래스의 1인칭 SkeletalMesh + AnimBP
- `CameraBoom` → 해당 클래스 체형에 맞는 `SetRelativeLocation` 값
- 무기 부착 소켓 이름은 스켈레톤 기준

**중요:** `PlayerCharacterClass`는 `ADRCharacter`의 멤버이며, 기존 `ADRCharacterBase::CharacterClass`는 적 전용으로 유지된다. 각 BP의 `DefaultPrimaryAttributes`/`DefaultVitalAttributes`/`StartupAbilities` BP 프로퍼티는 더 이상 사용되지 않고 `UPlayerCharacterClassInfo` 경로가 우선한다.

### 7.5 ADRStageGameMode에서 DefaultPawnClass 동적 결정

스테이지 진입 시에도 선택한 클래스의 BP를 스폰해야 한다.

**파일:** `Source/DaeRune/Public/Game/DRStageGameMode.h`

```cpp
public:
    // 플레이어별 DefaultPawnClass 결정
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
```

**파일:** `Source/DaeRune/Private/Game/DRStageGameMode.cpp`

```cpp
UClass* ADRStageGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    if (APlayerController* PC = Cast<APlayerController>(InController))
    {
        if (ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
        {
            EPlayerCharacterClass SelectedClass = PS->GetSelectedPlayerClass();

            // PlayerCharacterClassInfo에서 BP 클래스 조회
            if (PlayerCharacterClassInfo)
            {
                TSubclassOf<ADRCharacter>* BPClassPtr = PlayerCharacterClassInfo->CharacterBPClasses.Find(SelectedClass);
                if (BPClassPtr && *BPClassPtr)
                {
                    return *BPClassPtr;
                }
            }
        }
    }

    // 폴백: 기존 DefaultPawnClass
    return Super::GetDefaultPawnClassForController_Implementation(InController);
}
```

**ADRLobbyGameMode에도 동일 오버라이드 추가** — 로비 첫 진입 시에도 선택된 클래스 BP를 스폰.

### 검증 방법
PIE 2인 플레이.
1. 대기실에서 화살표로 클래스 변경 → **캐릭터가 새 BP로 교체됨** (양 클라이언트 모두에서 외형 변경 확인)
2. 교체 후 카메라, 1인칭 메시, 3인칭 메시 모두 정상 동작 확인
3. Power On 후 스테이지 진입 → 선택한 클래스의 어빌리티가 부여됨
4. 스테이지에서도 선택한 클래스 BP로 스폰되는지 확인

---

## Phase 8: 메인 메뉴 리뉴얼

### 목표
메인 메뉴를 실제 맵을 배경으로 렌더링하는 시각적 화면으로 변경.

### 8.1 접근 방식

**선택한 방식:** MainMenu.umap을 유지하되, 로비 환경과 유사한 배경 + CameraActor를 배치. 메인 메뉴는 네트워크 세션이 없는 로컬 상태이므로 LobbyMap 자체를 재사용하는 것은 복잡해진다.

- MainMenu.umap에 로비와 동일한 환경 아트(서브레벨 또는 복사)를 배치
- `ACameraActor`를 배치하여 원하는 앵글로 배경 촬영
- 캐릭터 배치는 `ADRMainMenuGameMode`에서 처리

### 8.2 ADRMainMenuGameMode 수정

**현재:** `AGameModeBase`를 상속하는 빈 클래스.

**파일:** `Source/DaeRune/Public/Game/DRMainMenuGameMode.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRMainMenuGameMode.generated.h"

UCLASS()
class DAERUNE_API ADRMainMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADRMainMenuGameMode();

protected:
    virtual void BeginPlay() override;

    // 튜토리얼 완료 여부 확인 (향후 SaveGame 연동)
    bool HasCompletedTutorial() const;
};
```

**파일:** `Source/DaeRune/Private/Game/DRMainMenuGameMode.cpp`

```cpp
#include "Game/DRMainMenuGameMode.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

ADRMainMenuGameMode::ADRMainMenuGameMode()
{
    // 메인 메뉴에서는 기본 폰 사용하지 않음 (카메라만 사용)
    DefaultPawnClass = nullptr;
}

void ADRMainMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 레벨에 배치된 CameraActor를 찾아 ViewTarget으로 설정
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
        {
            PC->SetViewTargetWithBlend(*It, 0.f);
            break;
        }
    }
}

bool ADRMainMenuGameMode::HasCompletedTutorial() const
{
    // TODO: SaveGame에서 튜토리얼 완료 플래그 읽기
    // 현재는 항상 true (로비 배경 사용)
    return true;
}
```

### 8.3 메인 메뉴 맵 아트 작업 (에디터)

- MainMenu.umap에 `ACameraActor` 배치
- 로비 환경과 유사한 배경 구성 (서브레벨 참조 또는 별도 제작)
- `BP_DRMainMenuGameMode`의 DefaultPawnClass = None으로 설정

### 8.4 튜토리얼 분기 (향후 구현)

`GameInstance`에 `bTutorialCompleted` 추가하고 SaveGame으로 영속화. 메인 메뉴에서 이 값에 따라:
- 미완료: 튜토리얼 맵 배경 + StartGame 버튼
- 완료: 로비 맵 배경 + Host/Join 버튼

현재는 구조만 잡고 항상 "완료" 상태로 동작.

### 검증 방법
게임 실행 → MainMenu 로드 → 카메라가 배경을 비추고 Host/Join UI 오버레이 표시. 기능적으로는 기존과 동일.

---

## Phase 9: 스테이지 복귀 → 대기실

### 목표
스테이지에서 게임 클리어/오버 후 로비로 복귀 시 대기실 상태로 돌아가도록 전체 플로우 통합.

### 9.1 기존 플로우 확인

현재 `ADRStageGameMode::ReturnToLobby()`:
```cpp
void ADRStageGameMode::ReturnToLobby()
{
    PrepareForTravel();
    World->ServerTravel(LobbyMapName + TEXT("?listen"));
    bIsWipeoutInProgress = false;
}
```

→ LobbyMap 로드 → 새 `ADRLobbyGameState` 생성 (기본값 `WaitingRoom`) → `ADRLobbyGameMode::BeginPlay()` → `AllowJoinInProgress()`

**이미 대부분 자동으로 동작한다:**
- 새 LobbyGameState의 `LobbyState` = `WaitingRoom` (기본값)
- BeginPlay에서 `AllowJoinInProgress()` → 세션 참가 허용
- BeginPlay에서 `FindWaitingRoomCamera()` → 카메라 탐색

### 9.2 HandleSeamlessTravelPlayer에서 대기실 모드 적용

**기존 `HandleSeamlessTravelPlayer` 수정:**

```cpp
void ADRLobbyGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
    Super::HandleSeamlessTravelPlayer(C);

    if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(C))
    {
        ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();

        FTimerHandle RestoreTimerHandle;
        GetWorldTimerManager().SetTimer(
            RestoreTimerHandle,
            [this, DRPC, LGS]()
            {
                if (!IsValid(DRPC)) return;

                // 관전 모드 강제 종료
                DRPC->ClientStopSpectating();

                if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
                {
                    // 대기실 모드: 슬롯 배치 + 고정 카메라
                    AssignPlayerToSlot(DRPC);
                    if (WaitingRoomCamera)
                    {
                        DRPC->ClientSetWaitingRoomView(WaitingRoomCamera);
                    }
                }
                else
                {
                    // 자유 조작 모드: 기존 복원 로직
                    if (APawn* Pawn = DRPC->GetPawn())
                    {
                        if (auto* MC = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
                        {
                            MC->SetMovementMode(MOVE_Walking);
                            MC->SetComponentTickEnabled(true);
                        }
                        if (auto* CC = Cast<UCapsuleComponent>(Pawn->GetRootComponent()))
                        {
                            CC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                        }
                    }
                }
            },
            0.5f, false
        );
    }
}
```

### 9.3 OnLevelEntered에서 대기실 UI 자동 생성

Phase 4.4에서 이미 구현:
```cpp
// OnLevelEntered() 끝부분
if (IsInLobby())
{
    ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
    if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
    {
        CreateWaitingRoomUI();
    }
}
```

`PostSeamlessTravel()` → `OnLevelEntered()` → `CreateWaitingRoomUI()` 체인이 자동 실행.

### 9.4 캐릭터 선택 상태 보존

Seamless Travel에서 PlayerController와 PlayerState가 보존되므로, `SelectedPlayerClass` 값도 유지된다. 스테이지에서 복귀해도 이전에 선택한 클래스가 그대로 유지.

### 검증 방법
전체 플로우 테스트:
1. 메인 메뉴 → Host → 로비(대기실)
2. Player2 Join → 대기실에 추가
3. 캐릭터 선택
4. Power On → 자유 조작
5. 스테이지 진입 → 게임 진행
6. 게임 오버/클리어 → 로비 복귀
7. 다시 대기실 상태 확인 (고정 카메라, UI 표시, 캐릭터 배치)
8. 세션 참가 다시 허용 확인

---

## 전체 파일 변경 요약

### 새로 생성하는 파일 (4개)

| 파일 | 용도 |
|------|------|
| `Source/DaeRune/Public/Game/DRLobbyTypes.h` | ELobbyState, FWaitingRoomPlayerInfo |
| `Source/DaeRune/Public/Actor/DRWaitingRoomCameraActor.h` | 대기실 카메라 액터 |
| `Source/DaeRune/Private/Actor/DRWaitingRoomCameraActor.cpp` | 구현 |
| `Source/DaeRune/Public/UI/Widget/DRWaitingRoomWidget.h` | 대기실 UI 베이스 클래스 |

### 수정하는 파일 (12개)

| 파일 | 주요 변경 |
|------|-----------|
| `DRLobbyGameState.h/.cpp` | +ELobbyState, +OnLobbyStateChanged, +SetLobbyState/OnRep |
| `DRLobbyGameMode.h/.cpp` | +PowerOn, +KickPlayer, +BlockJoinInProgress, +슬롯관리, +카메라탐색, +RespawnPlayerWithClass (BP 교체), +GetDefaultPawnClassForController 오버라이드, PostLogin/HandleSeamlessTravel 수정 |
| `DRPlayerState.h/.cpp` | +SelectedPlayerClass (EPlayerCharacterClass), +OnRep_SelectedPlayerClass, +FOnPlayerClassChanged 델리게이트, SetSelectedPlayerClass에서 폰 교체 트리거 |
| `DRPlayerController.h/.cpp` | +ServerRPC(ChangeClass/PowerOn/Kick), +ClientRPC(WaitingRoomView/CameraTransition/Kicked), +UI관리, RestoreDefaultInputMode 수정, OnLevelEntered 수정 |
| `DRCharacter.h/.cpp` | +PlayerCharacterClass 멤버 (EPlayerCharacterClass), InitializeDefaultAttributes 오버라이드, PossessedBy 수정, 생성자에서 CharacterClass=Elementalist 삭제 |
| `CharacterClassInfo.h/.cpp` | +EPlayerCharacterClass 열거형, +UPlayerCharacterClassInfo 클래스 (CharacterBPClasses TMap 포함). FCharacterClassDefaultInfo 변경 없음 |
| `DRGameModeBase.h` | CharacterClassInfo → EnemyCharacterClassInfo (UCharacterClassInfo*) + PlayerCharacterClassInfo (UPlayerCharacterClassInfo*) |
| `DRAbilitySystemLibrary.h/.cpp` | GetCharacterClassInfo→EnemyCharacterClassInfo 리다이렉트, +GetPlayerCharacterClassInfo (UPlayerCharacterClassInfo*), +InitializePlayerDefaultAttributes (EPlayerCharacterClass), +GivePlayerStartupAbilities (EPlayerCharacterClass) |
| `DRStageGameMode.h/.cpp` | +GetDefaultPawnClassForController 오버라이드 (선택한 클래스 BP 스폰) |
| `DRMainMenuGameMode.h/.cpp` | +BeginPlay (카메라 설정), +HasCompletedTutorial |

### 블루프린트 에셋 (에디터 작업)

| 에셋 | 용도 |
|------|------|
| `BP_WaitingRoomCamera` | ADRWaitingRoomCameraActor 블루프린트, LobbyMap에 배치 |
| `WBP_WaitingRoom` | 대기실 UI 위젯 (UDRWaitingRoomWidget 상속) |
| `BP_DRCharacter_GardenRobot` | ADRCharacter 상속. GardenRobot 1인칭/3인칭 메시, 카메라 오프셋, AnimBP 설정 |
| `BP_DRCharacter_VendingMachineRobot` | ADRCharacter 상속. VendingMachineRobot 1인칭/3인칭 메시, 카메라 오프셋, AnimBP 설정 |

### 맵 변경 (에디터 작업)

| 맵 | 변경 |
|----|------|
| `LobbyMap.umap` | BP_WaitingRoomCamera 배치, 슬롯 위치 설정 |
| `MainMenu.umap` | CameraActor 배치, 배경 환경 구성 |

### 데이터 에셋 변경 (에디터 작업)

| 에셋 | 변경 |
|------|------|
| `DA_EnemyCharacterClassInfo` | 기존 DA_CharacterClassInfo 이름 변경, 타입 UCharacterClassInfo |
| `DA_PlayerCharacterClassInfo` | 신규, 타입 UPlayerCharacterClassInfo. CharacterClassInformation: GardenRobot/VendingMachineRobot별 GE/어빌리티. CharacterBPClasses: 각 클래스별 캐릭터 BP 매핑 |
| `BP_DRStageGameMode` | EnemyCharacterClassInfo + PlayerCharacterClassInfo 양쪽 설정 |
| `BP_DRLobbyGameMode` | EnemyCharacterClassInfo + PlayerCharacterClassInfo 양쪽 설정 |
| `BP_DRPlayerController` | WaitingRoomWidgetClass 설정 |

### CombatInterface 처리 방침

- `ICombatInterface::GetCharacterClass()` (반환: `ECharacterClass`)는 **적 전용으로 유지**
- 플레이어에서는 이 메서드가 기본값(`Warrior`)을 반환하지만 게임플레이에 영향 없음 (적 AI에서만 실제 사용)
- 플레이어는 `PlayerCharacterClass` 멤버를 직접 사용

---

## 구현 우선순위 의존성 그래프

```
Phase 1 ─── Phase 2 ─── Phase 3 ─── Phase 4 ─── Phase 5
  │                                     │           │
  │                                     └── Phase 6 ┘
  │                                           │
  │         Phase 2 ─── Phase 7               │
  │                                           │
  └── Phase 8 (독립)                     Phase 9 (최종 통합)
```

- Phase 1~6은 순차적 의존성
- Phase 7은 Phase 2 이후 독립 진행 가능 (Phase 4~6과 병렬)
- Phase 8은 Phase 1 이후 독립 진행 가능
- Phase 9는 모든 Phase 완료 후 통합 테스트
