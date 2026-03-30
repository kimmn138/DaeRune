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
Phase 7: 캐릭터 메시 교체 (클래스 선택 → 외형 변경) ─── [비주얼 피드백]
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

##### 2.0.5.6 FCharacterClassDefaultInfo 구조체 확장

`CharacterClassInfo.h`의 `FCharacterClassDefaultInfo`에 메시/애님 필드 추가 (플레이어 데이터 에셋 `UPlayerCharacterClassInfo`에서만 설정, 적 에셋에서는 비워둠):

```cpp
USTRUCT(BlueprintType)
struct FCharacterClassDefaultInfo
{
    GENERATED_BODY()

    // 기존 필드 (적/플레이어 공통)
    UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
    TSubclassOf<UGameplayEffect> PrimaryAttributes;

    UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
    TSubclassOf<UGameplayEffect> VitalAttributes;

    UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
    TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

    UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
    TArray<TSubclassOf<UGameplayAbility>> DeathAbilities;

    // ★ 새 필드 (플레이어 에셋에서만 설정, 적 에셋에서는 비워둠)
    UPROPERTY(EditDefaultsOnly, Category = "Visual")
    TSoftObjectPtr<USkeletalMesh> CharacterMesh;

    UPROPERTY(EditDefaultsOnly, Category = "Visual")
    TSubclassOf<UAnimInstance> AnimClass;
};
```

##### 2.0.5.7 블루프린트/에디터 작업

| 작업 | 설명 |
|------|------|
| `DA_CharacterClassInfo` → `DA_EnemyCharacterClassInfo` 이름 변경 | 기존 적 데이터 유지, 이름만 변경. 타입: `UCharacterClassInfo` |
| `DA_PlayerCharacterClassInfo` 신규 생성 | **`UPlayerCharacterClassInfo` 타입**의 새 데이터 에셋 |
| `BP_DRStageGameMode`에서 두 에셋 설정 | `EnemyCharacterClassInfo` = DA_EnemyCharacterClassInfo, `PlayerCharacterClassInfo` = DA_PlayerCharacterClassInfo |
| `BP_DRLobbyGameMode`에서 두 에셋 설정 | 동일하게 양쪽 모두 설정 (로비에서도 대기실 메시 교체에 PlayerCharacterClassInfo 필요) |
| `DA_PlayerCharacterClassInfo` 내용 채우기 | **GardenRobot**, **VendingMachineRobot** 엔트리별 PrimaryAttributes GE, VitalAttributes GE, StartupAbilities, CharacterMesh, AnimClass 설정 |

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
│  FCharacterClassDefaultInfo (구조체 확장)                               │
│  └── + CharacterMesh, + AnimClass (플레이어 에셋에서만 사용)            │
└──────────────────────────────────────────────────────────────────────┘
```

##### 2.0.5.9 구현 시점

- **Phase 2 (현재):** PlayerState에 `SelectedPlayerClass` (`EPlayerCharacterClass`) 추가 (데이터 레이어만)
- **Phase 7 (캐릭터 메시 교체 시):** 위의 CharacterClassInfo 분리 + 플레이어 초기화 경로 변경을 함께 진행
  - CharacterClassInfo.h에 `EPlayerCharacterClass` 열거형 + `UPlayerCharacterClassInfo` 클래스 추가
  - DRGameModeBase.h 수정 (EnemyCharacterClassInfo + PlayerCharacterClassInfo)
  - DRAbilitySystemLibrary 확장 (새 함수 3개, `EPlayerCharacterClass` + `UPlayerCharacterClassInfo*` 사용)
  - FCharacterClassDefaultInfo 확장 (CharacterMesh, AnimClass)
  - ADRCharacter에 `PlayerCharacterClass` 멤버 추가, 오버라이드 (InitializeDefaultAttributes, PossessedBy)
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
4. `UPlayerCharacterClassInfo` 데이터 에셋에 GardenRobot/VendingMachineRobot별 CharacterMesh, AnimClass 추가
5. 기존 `BP_DRCharacter`의 `DefaultPrimaryAttributes`, `DefaultVitalAttributes`, `StartupAbilities`는 폴백(fallback)으로 유지하되, `UPlayerCharacterClassInfo` 경로가 우선하도록 변경

### 검증 방법
PIE 2인 플레이. 콘솔 또는 블루프린트에서 `RequestChangeClass(true)` 호출. 두 클라이언트 모두에서 PlayerState의 `SelectedCharacterClass` 값이 변경되었는지 확인.

---

## Phase 3: 대기실 카메라 + 플레이어 배치

### 목표
로비 맵에 대기실 전용 고정 카메라를 배치하고, 플레이어 캐릭터들을 일렬로 정렬하는 시스템 구축.

### 3.1 새 파일 생성: `ADRWaitingRoomCameraActor`

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

    // 최대 4명의 플레이어 슬롯 위치 (호스트=0, 이후 참가순)
    // LobbyMap 에디터에서 직접 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waiting Room")
    TArray<FTransform> PlayerSlotTransforms;

    // 슬롯 인덱스에 해당하는 Transform 반환
    UFUNCTION(BlueprintCallable, Category = "Waiting Room")
    FTransform GetSlotTransform(int32 SlotIndex) const;

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

    // 기본 4슬롯 초기화 (에디터에서 조정)
    PlayerSlotTransforms.SetNum(4);
}

FTransform ADRWaitingRoomCameraActor::GetSlotTransform(int32 SlotIndex) const
{
    if (PlayerSlotTransforms.IsValidIndex(SlotIndex))
    {
        return PlayerSlotTransforms[SlotIndex];
    }

    // 범위 초과 시 마지막 유효 슬롯 반환
    if (PlayerSlotTransforms.Num() > 0)
    {
        return PlayerSlotTransforms.Last();
    }

    return FTransform::Identity;
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

### 3.2 ADRLobbyGameMode에 카메라 참조 + 슬롯 관리 추가

**파일:** `Source/DaeRune/Public/Game/DRLobbyGameMode.h`

**추가할 내용:**
```cpp
class ADRWaitingRoomCameraActor;

protected:
    // 대기실 카메라 (BeginPlay에서 레벨에서 탐색)
    UPROPERTY()
    TObjectPtr<ADRWaitingRoomCameraActor> WaitingRoomCamera;

    // 플레이어 → 슬롯 인덱스 매핑
    TMap<AController*, int32> PlayerSlotMap;

    // 다음 사용 가능한 슬롯 인덱스
    int32 NextAvailableSlot = 0;

    // 플레이어를 슬롯에 배치
    void AssignPlayerToSlot(AController* Player);

    // 모든 플레이어 재배치 (킥 후 빈 자리 정리)
    void RepositionAllPlayers();

    // 플레이어 폰을 슬롯 위치로 텔레포트 + 이동 비활성화
    void PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex);

    // 대기실 카메라 탐색
    void FindWaitingRoomCamera();
```

**파일:** `Source/DaeRune/Private/Game/DRLobbyGameMode.cpp`

```cpp
#include "Actor/DRWaitingRoomCameraActor.h"
#include "EngineUtils.h"  // TActorIterator

void ADRLobbyGameMode::BeginPlay()
{
    Super::BeginPlay();

    FindWaitingRoomCamera();
    AllowJoinInProgress();
}

void ADRLobbyGameMode::FindWaitingRoomCamera()
{
    // CleanserSite 탐색과 동일한 패턴 (DRStageGameMode::BeginPlay에서 사용)
    for (TActorIterator<ADRWaitingRoomCameraActor> It(GetWorld()); It; ++It)
    {
        WaitingRoomCamera = *It;
        break; // 하나만 필요
    }
}

void ADRLobbyGameMode::AssignPlayerToSlot(AController* Player)
{
    if (!WaitingRoomCamera) return;

    int32 SlotIndex = NextAvailableSlot++;
    PlayerSlotMap.Add(Player, SlotIndex);

    if (APawn* Pawn = Player->GetPawn())
    {
        PositionPawnAtSlot(Pawn, SlotIndex);
    }
}

void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
    if (!WaitingRoomCamera || !Pawn) return;

    FTransform SlotTransform = WaitingRoomCamera->GetSlotTransform(SlotIndex);
    FRotator FacingRotation = WaitingRoomCamera->GetCharacterFacingRotation();

    // 텔레포트
    Pawn->SetActorLocation(SlotTransform.GetLocation());
    Pawn->SetActorRotation(FacingRotation);

    // 이동 비활성화
    if (UCharacterMovementComponent* MovementComp =
        Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
    {
        MovementComp->DisableMovement(); // MOVE_None으로 설정
    }
}

void ADRLobbyGameMode::RepositionAllPlayers()
{
    if (!WaitingRoomCamera) return;

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

### 3.3 PostLogin / HandleSeamlessTravelPlayer에서 슬롯 배치

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
            // ... (기존 코드 유지)
        }
    }
    // ... (기존 디버그 메시지 코드 유지)
}
```

**기존 `HandleSeamlessTravelPlayer` 수정:** 동일 패턴으로 `AssignPlayerToSlot` + `ClientSetWaitingRoomView` 호출.

### 3.4 ADRPlayerController에 카메라 설정 Client RPC 추가

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
```

### 3.5 RestoreDefaultInputMode 수정

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

### 3.6 레벨에 배치할 블루프린트

**블루프린트 에셋:** `Content/Blueprints/Actor/BP_WaitingRoomCamera.uasset`
- `ADRWaitingRoomCameraActor` 기반
- `PlayerSlotTransforms` 4개를 에디터에서 설정 (호스트 왼쪽, 나머지 오른쪽 일렬)
- `LobbyMap.umap`에 하나 배치

### 검증 방법
PIE 2인 플레이. 로비 진입 시 두 플레이어 모두 고정 카메라 시점으로 전환. 캐릭터들이 지정된 슬롯 위치에 일렬 배치. 마우스 커서 표시, 캐릭터 이동 불가.

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

**레이아웃:**
```
┌─────────────────────────────────────┐
│  Room Code: A3K9BX7M               │  ← 상단
│                                     │
│  [Kick]    [Kick]    [Kick]         │  ← 호스트에게만 보임
│  Player1  Player2   Player3         │
│  ◀ GardenRobot ▶  ◀ VendingMachine ▶│  ← 자기것만 인터랙티브
│                                     │
│         [ Power On ]                │  ← 호스트에게만 보임
└─────────────────────────────────────┘
```

- `RefreshPlayerSlots` 이벤트 구현: `FWaitingRoomPlayerInfo` 배열 받아 슬롯 UI 업데이트
- `SetIsHost` 이벤트 구현: Kick 버튼, Power On 버튼 가시성 토글
- 화살표 버튼 클릭 → `GetOwningPlayer()→RequestChangeClass(true/false)`
- Kick 버튼 클릭 → `GetOwningPlayer()→ServerRequestKickPlayer(SlotInfo.OwningPlayerState)`
- Power On 클릭 → `GetOwningPlayer()→ServerRequestPowerOn()`

### 검증 방법
PIE 2인 플레이. 대기실 UI 표시. 호스트에게만 Kick/PowerOn 버튼 보임. 화살표로 클래스 변경 시 UI 텍스트 갱신.

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

## Phase 7: 캐릭터 메시 교체 (클래스 선택 → 외형 변경)

### 목표
대기실에서 클래스를 변경하면 캐릭터의 외형(SkeletalMesh)이 즉시 바뀌고, 게임 시작 시 선택한 클래스의 어트리뷰트/어빌리티가 적용된다.

### 7.1 CharacterClassInfo에 메시 참조 추가

**파일:** `Source/DaeRune/Public/AbilitySystem/Data/CharacterClassInfo.h`

`FCharacterClassDefaultInfo`에 추가:
```cpp
    // 캐릭터 클래스별 SkeletalMesh
    UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
    TSoftObjectPtr<USkeletalMesh> CharacterMesh;

    // 캐릭터 클래스별 AnimBlueprint (옵션)
    UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
    TSubclassOf<UAnimInstance> AnimClass;
```

**에디터 작업:** `UPlayerCharacterClassInfo` 데이터 에셋 (`DA_PlayerCharacterClassInfo`)에서 GardenRobot/VendingMachineRobot별 메시/애님 설정.

### 7.2 ADRCharacter에 메시 교체 함수 추가

**파일:** `Source/DaeRune/Public/Character/DRCharacter.h`

```cpp
public:
    // 캐릭터 클래스에 따른 메시 교체
    UFUNCTION(BlueprintCallable, Category = "Character Selection")
    void UpdateCharacterAppearance(EPlayerCharacterClass NewClass);
```

**파일:** `Source/DaeRune/Private/Character/DRCharacter.cpp`

```cpp
void ADRCharacter::UpdateCharacterAppearance(EPlayerCharacterClass NewClass)
{
    // PlayerCharacterClassInfo 데이터 에셋 가져오기 (반환 타입: UPlayerCharacterClassInfo*)
    UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this);
    if (!ClassInfo) return;

    FCharacterClassDefaultInfo ClassDefault = ClassInfo->GetClassDefaultInfo(NewClass);

    // 메시 교체
    if (!ClassDefault.CharacterMesh.IsNull())
    {
        USkeletalMesh* LoadedMesh = ClassDefault.CharacterMesh.LoadSynchronous();
        if (LoadedMesh)
        {
            GetMesh()->SetSkeletalMesh(LoadedMesh);
        }
    }

    // AnimBlueprint 교체
    if (ClassDefault.AnimClass)
    {
        GetMesh()->SetAnimInstanceClass(ClassDefault.AnimClass);
    }
}
```

### 7.3 OnRep_SelectedPlayerClass에서 메시 교체 트리거

**파일:** `Source/DaeRune/Private/Player/DRPlayerState.cpp`

`OnRep_SelectedPlayerClass` 수정:
```cpp
void ADRPlayerState::OnRep_SelectedPlayerClass()
{
    OnPlayerClassChanged.Broadcast(this, SelectedPlayerClass);

    // 폰의 외형 업데이트
    if (APawn* Pawn = GetPawn())
    {
        if (ADRCharacter* Character = Cast<ADRCharacter>(Pawn))
        {
            Character->UpdateCharacterAppearance(SelectedPlayerClass);
        }
    }
}
```

`SetSelectedPlayerClass`에도 동일 로직 추가 (서버에서 직접 호출 시):
```cpp
void ADRPlayerState::SetSelectedPlayerClass(EPlayerCharacterClass NewClass)
{
    if (!HasAuthority()) return;
    if (SelectedPlayerClass == NewClass) return;

    SelectedPlayerClass = NewClass;
    OnPlayerClassChanged.Broadcast(this, SelectedPlayerClass);

    // 서버에서도 폰 외형 업데이트
    if (APawn* Pawn = GetPawn())
    {
        if (ADRCharacter* Character = Cast<ADRCharacter>(Pawn))
        {
            Character->UpdateCharacterAppearance(SelectedPlayerClass);
        }
    }
}
```

### 7.4 플레이어 초기화 경로를 CharacterClassInfo 기반으로 리팩터링

**⚠️ Phase 2.0의 분석에 따라, 플레이어가 CharacterClassInfo를 사용하도록 변경해야 한다.**

현재 플레이어의 `InitializeDefaultAttributes()`는 BP에 직접 설정된 GE를 사용하므로 `CharacterClass`를 바꿔도 어트리뷰트에 영향이 없다. 이를 적과 동일한 CharacterClassInfo 경로로 변경한다.

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
    // PlayerState에서 선택한 클래스 정보 사용
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

    // 기본 어트리뷰트 초기화 (이제 CharacterClassInfo 기반으로 동작)
    InitializeDefaultAttributes();
}
```

#### 7.4.3 ADRCharacter::PossessedBy() 수정 - 어빌리티도 CharacterClassInfo 사용

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

#### 7.4.4 UPlayerCharacterClassInfo 데이터 에셋 업데이트 (에디터 작업)

`DA_PlayerCharacterClassInfo` (`UPlayerCharacterClassInfo` 타입) 데이터 에셋에 플레이어용 데이터를 설정한다:

| EPlayerCharacterClass | 설정 내용 |
|----------------------|-----------|
| GardenRobot | 플레이어용 PrimaryAttributes GE, VitalAttributes GE, StartupAbilities, CharacterMesh, AnimClass |
| VendingMachineRobot | 플레이어용 PrimaryAttributes GE, VitalAttributes GE, StartupAbilities, CharacterMesh, AnimClass |

열거형 분리(`ECharacterClass` vs `EPlayerCharacterClass`)로 적/플레이어 간 키 충돌 문제는 완전히 해결되었다. 적 데이터 에셋(`DA_EnemyCharacterClassInfo`)과 플레이어 데이터 에셋(`DA_PlayerCharacterClassInfo`)은 서로 다른 C++ 클래스 타입이므로 독립적이다.

**중요:** `PlayerCharacterClass`는 `ADRCharacter`의 멤버이며, 기존 `ADRCharacterBase::CharacterClass`는 적 전용으로 유지된다.

### 검증 방법
PIE 2인 플레이. 대기실에서 화살표로 클래스 변경 → 캐릭터 메시가 즉시 변경됨 (양 클라이언트 모두). Power On 후 스테이지 진입 → 선택한 클래스의 어빌리티가 부여됨.

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

### 수정하는 파일 (11개)

| 파일 | 주요 변경 |
|------|-----------|
| `DRLobbyGameState.h/.cpp` | +ELobbyState, +OnLobbyStateChanged, +SetLobbyState/OnRep |
| `DRLobbyGameMode.h/.cpp` | +PowerOn, +KickPlayer, +BlockJoinInProgress, +슬롯관리, +카메라탐색, PostLogin/HandleSeamlessTravel 수정 |
| `DRPlayerState.h/.cpp` | +SelectedPlayerClass (EPlayerCharacterClass), +OnRep_SelectedPlayerClass, +FOnPlayerClassChanged 델리게이트 |
| `DRPlayerController.h/.cpp` | +ServerRPC(ChangeClass/PowerOn/Kick), +ClientRPC(WaitingRoomView/CameraTransition/Kicked), +UI관리, RestoreDefaultInputMode 수정, OnLevelEntered 수정 |
| `DRCharacter.h/.cpp` | +PlayerCharacterClass 멤버 (EPlayerCharacterClass), +UpdateCharacterAppearance(EPlayerCharacterClass), InitializeDefaultAttributes 오버라이드, PossessedBy 수정 |
| `CharacterClassInfo.h` | +EPlayerCharacterClass 열거형, +UPlayerCharacterClassInfo 클래스, +CharacterMesh/AnimClass in FCharacterClassDefaultInfo |
| `DRGameModeBase.h` | CharacterClassInfo → EnemyCharacterClassInfo (UCharacterClassInfo*) + PlayerCharacterClassInfo (UPlayerCharacterClassInfo*) |
| `DRAbilitySystemLibrary.h/.cpp` | GetCharacterClassInfo→EnemyCharacterClassInfo 리다이렉트, +GetPlayerCharacterClassInfo (UPlayerCharacterClassInfo*), +InitializePlayerDefaultAttributes (EPlayerCharacterClass), +GivePlayerStartupAbilities (EPlayerCharacterClass) |
| `DRMainMenuGameMode.h/.cpp` | +BeginPlay (카메라 설정), +HasCompletedTutorial |

### 블루프린트 에셋 (에디터 작업)

| 에셋 | 용도 |
|------|------|
| `BP_WaitingRoomCamera` | ADRWaitingRoomCameraActor 블루프린트, LobbyMap에 배치 |
| `WBP_WaitingRoom` | 대기실 UI 위젯 (UDRWaitingRoomWidget 상속) |

### 맵 변경 (에디터 작업)

| 맵 | 변경 |
|----|------|
| `LobbyMap.umap` | BP_WaitingRoomCamera 배치, 슬롯 위치 설정 |
| `MainMenu.umap` | CameraActor 배치, 배경 환경 구성 |

### 데이터 에셋 변경 (에디터 작업)

| 에셋 | 변경 |
|------|------|
| `DA_EnemyCharacterClassInfo` | 기존 DA_CharacterClassInfo 이름 변경, 타입 UCharacterClassInfo |
| `DA_PlayerCharacterClassInfo` | 신규, 타입 UPlayerCharacterClassInfo, GardenRobot + VendingMachineRobot 엔트리 |
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
