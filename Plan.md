# Plan.md — 스팀 도전과제 기반 캐릭터/꾸밈 요소 해금 시스템

> 작성일: 2026-07-01
> 대상: DaeRune (UE 5.5, GAS, OnlineSubsystemSteam)
> 목적: 스팀 도전과제(Achievement) 달성 → 플레이어블 캐릭터 / 캐릭터 꾸밈 요소(코스메틱) 해금

---

## 0. 한눈에 보기 (TL;DR)

- 스팀 백엔드의 **Achievement(도전과제)** 를 "진실의 원천(source of truth)"으로 사용한다.
- 게임은 인게임 통계(`UDRStatTracker`)를 누적 → 조건 충족 시 **스팀 Achievement Unlock 호출** → 그 결과를 다시 읽어 **해금 상태(`Unlockable`) 평가** → **SaveGame + 스팀 클라우드**에 캐싱.
- 해금 결과는 **캐릭터 선택 UI / 코스메틱 선택 UI**에서 잠금/해제 표시로 반영.
- 멀티플레이에서 캐릭터/외형은 **이미 PlayerState·CharacterBP·SaveGame 경로로 복제**되므로, 해금은 **로컬(클라이언트) 판정 + 서버 검증** 구조로 붙인다.
- 스팀이 오프라인/비가용일 때를 대비해 **로컬 SaveGame 폴백**을 항상 유지한다.

---

## 1. 현재 코드베이스 기반 (이미 존재하는 것)

설계는 새로 만드는 게 아니라 **기존 자산 위에 얹는다**. 관련 파일:

| 영역 | 파일 | 현재 역할 | 확장 포인트 |
|---|---|---|---|
| 저장 | `Source/DaeRune/Public/Game/DRSaveGame.h` | `bHasCompletedTutorial` 하나만 저장. 슬롯명 `DaeRunePlayerProgress` | **해금/통계 필드 추가** |
| 게임 인스턴스 | `Source/DaeRune/Public/Game/DRGameInstance.h` | `SaveProgress/LoadProgress`, 캐릭터 선택 보존(`PlayerClassSelections`) | **도전과제·해금 매니저 소유** |
| 캐릭터 클래스 | `Source/.../Data/CharacterClassInfo.h` | `EPlayerCharacterClass { Gardener, VendingMachine }`, `CharacterBPClasses` 맵 | **신규 플레이어블 enum 추가 + 해금 게이팅** |
| 스팀 설정 | `Config/DefaultEngine.ini` | `[OnlineSubsystemSteam] bEnabled=true`, `SteamDevAppId=480` (테스트용) | **실제 AppId 교체, Achievement 매핑** |

> ⚠️ **AppId 480 은 Spacewar(밸브 SDK 샘플)** 이다. 실제 스팀 도전과제는 **본인 앱 AppId** 가 발급되고 Steamworks 파트너 페이지에 도전과제를 등록해야만 동작한다. 그전까지는 로컬 폴백으로만 개발/검증 가능.

---

## 2. 목표 및 비목표

### 목표
1. 특정 도전과제 달성 시 **새 플레이어블 캐릭터 해금** (예: `Gardener`, `VendingMachine` 외 신규).
2. 특정 도전과제 달성 시 **캐릭터 꾸밈 요소(코스메틱) 해금** — 스킨/색상/액세서리/이펙트 등.
3. 해금 상태가 **재접속·재설치 후에도 유지**(스팀 클라우드 + 로컬 폴백).
4. 캐릭터 선택 / 꾸미기 UI에서 잠금·해제 상태와 **해금 조건 안내** 표시.
5. 멀티플레이에서 각 플레이어가 **자기 해금분 내에서만** 선택 가능하도록 서버 검증.

### 비목표 (이번 범위 밖)
- 유료 DLC / 마이크로트랜잭션.
- 거래·인벤토리(Steam Inventory Service)·드롭 시스템.
- 시즌패스/배틀패스류 진행도.
- 서버 권위형 통계 저장(전적 DB). 통계는 로컬+스팀 stat으로 충분.

---

## 3. 아키텍처 개요

```
[게임플레이 이벤트]
   (적 처치, 페이즈 클리어, 파트 설치, 보스 격파, 무피해 클리어 …)
        │  BroadcastStatEvent(Tag, Value)
        ▼
[UDRStatTracker]  ← GameInstance가 소유 (세션 넘어 유지)
   - 인게임 통계 누적 (TMap<FGameplayTag,int64>)
   - 임계값 도달 시 → UDRAchievementSubsystem 통지
        │
        ▼
[UDRAchievementSubsystem]  (UGameInstanceSubsystem)
   - 스팀 IOnlineAchievements 래핑
   - WriteAchievement / QueryAchievements
   - 스팀 stat 동기화 (incremental 도전과제용)
        │  (해금됨 콜백)
        ▼
[UDRUnlockManager]  (GameInstance 소유 or Subsystem)
   - DataAsset(UDRUnlockData) 규칙 평가:
       "Achievement X 달성 → Unlockable Y 해금"
   - 결과를 UDRSaveGame에 기록 + 스팀 클라우드 저장
        │  OnUnlockableUnlocked(FGameplayTag) 델리게이트
        ▼
[UI: 캐릭터 선택 / 코스메틱 선택 위젯]
   - IsUnlocked(Tag) 조회 → 잠금/해제 표시, 신규 해금 토스트
        │  플레이어가 선택
        ▼
[서버 검증]  ADRPlayerController / GameMode
   - ServerRequestSelectCharacter(Class) / ServerRequestSetCosmetic(Id)
   - 서버가 "해당 클라가 정말 해금했는가" 신뢰 검증 → 적용 → 복제
```

핵심 원칙:
- **통계 누적은 클라이언트 로컬**(자기 캐릭터 기준)에서. 멀티에서 서버가 전수 집계할 필요 없음 — 도전과제는 "내 계정"의 진행도다.
- **해금 판정도 클라이언트 로컬**. 단, 멀티 세션에서 **선택 권한**만 서버가 가볍게 검증.

---

## 4. 데이터 모델

### 4.1 식별자 전략: GameplayTag 사용
프로젝트가 이미 `FDRGameplayTags` 중심으로 태그를 관리하므로 일관성 있게 태그로 식별한다.

`DRGameplayTags.h/.cpp` 에 신규 카테고리 추가:

```
Achievement.FirstBlood            // 첫 적 처치
Achievement.PhaseMaster           // 페이즈3까지 클리어
Achievement.Untouchable           // 무피해 클리어
Achievement.WaterLord             // 누적 워터 N 획득
Achievement.PartsCollector        // 파트 N개 설치
Achievement.BossSlayer            // 엘리트 보스 격파
...

Unlockable.Character.Ranger       // 캐릭터 해금
Unlockable.Character.Bear
Unlockable.Cosmetic.Gardener.SkinGold
Unlockable.Cosmetic.Gardener.HatLeaf
Unlockable.Cosmetic.VendingMachine.SkinNeon
...

Stat.Kills
Stat.WaterCollected
Stat.PartsInstalled
Stat.PhasesCleared
Stat.DamagelessRuns
```

### 4.2 스팀 측 매핑 (`DefaultEngine.ini`)
OnlineSubsystemSteam은 **스팀 파트너 페이지에 등록한 Achievement API Name** 으로 동작한다. 게임 태그 ↔ 스팀 API Name 매핑이 필요.

```ini
[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=<실제_AppId>     ; 480(Spacewar)에서 교체 필수
; Achievements는 코드에서 API Name 문자열로 직접 참조
```

매핑은 ini가 아니라 **DataAsset(`UDRUnlockData`)** 에서 관리(아래 4.4).

### 4.3 `UDRSaveGame` 확장
`Source/DaeRune/Public/Game/DRSaveGame.h` 에 필드 추가:

```cpp
UCLASS()
class DAERUNE_API UDRSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UDRSaveGame();

    // 기존
    UPROPERTY(VisibleAnywhere, Category="Progress")
    bool bHasCompletedTutorial = false;

    // ── 신규: 누적 통계 (스팀 비가용 시에도 진행되도록 로컬 미러) ──
    UPROPERTY(VisibleAnywhere, Category="Stats")
    TMap<FGameplayTag, int64> StatCounters;

    // ── 신규: 해금된 도전과제 (스팀 동기화 실패 대비 폴백 캐시) ──
    UPROPERTY(VisibleAnywhere, Category="Achievements")
    TSet<FGameplayTag> UnlockedAchievements;

    // ── 신규: 해금된 콘텐츠 (캐릭터 + 코스메틱 통합) ──
    UPROPERTY(VisibleAnywhere, Category="Unlocks")
    TSet<FGameplayTag> UnlockedContent;

    // ── 신규: 플레이어가 마지막으로 장착한 코스메틱 (캐릭터별) ──
    UPROPERTY(VisibleAnywhere, Category="Cosmetics")
    TMap<EPlayerCharacterClass, FDRLoadout> EquippedLoadouts;

    // 버전 마이그레이션용
    UPROPERTY(VisibleAnywhere, Category="Meta")
    int32 SaveVersion = 1;

    static const FString SaveSlotName;   // "DaeRunePlayerProgress"
    static const int32 UserIndex;
};
```

`FDRLoadout` 구조체(슬롯형 코스메틱):
```cpp
USTRUCT(BlueprintType)
struct FDRLoadout
{
    GENERATED_BODY()
    UPROPERTY() FGameplayTag SkinTag;   // Unlockable.Cosmetic.*.Skin*
    UPROPERTY() FGameplayTag HatTag;
    UPROPERTY() FGameplayTag TrailTag;  // 이펙트 등
    // 슬롯은 필요에 따라 확장
};
```

### 4.4 `UDRUnlockData` (DataAsset) — 규칙 정의
모든 "조건 → 보상" 규칙을 한 곳에서 관리. 코드 수정 없이 디자이너가 편집 가능.

```cpp
USTRUCT(BlueprintType)
struct FDRAchievementDef
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly) FGameplayTag AchievementTag;       // Achievement.*
    UPROPERTY(EditDefaultsOnly) FString SteamApiName;              // 스팀 파트너 등록명

    // 진행형(incremental) 도전과제일 때
    UPROPERTY(EditDefaultsOnly) FGameplayTag DrivingStat;          // Stat.* (없으면 즉시형)
    UPROPERTY(EditDefaultsOnly) int64 RequiredCount = 1;

    UPROPERTY(EditDefaultsOnly) FText DisplayName;
    UPROPERTY(EditDefaultsOnly) FText Description;                 // 해금 조건 안내문
    UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UTexture2D> Icon;
};

USTRUCT(BlueprintType)
struct FDRUnlockRule
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly) FGameplayTag UnlockableTag;        // Unlockable.*
    // 이 도전과제들이 모두(또는 하나) 달성되면 해금
    UPROPERTY(EditDefaultsOnly) TArray<FGameplayTag> RequiredAchievements;
    UPROPERTY(EditDefaultsOnly) bool bRequireAll = true;           // AND/OR

    UPROPERTY(EditDefaultsOnly) FText DisplayName;
    UPROPERTY(EditDefaultsOnly) FText HowToUnlock;                 // UI 안내문
};

UCLASS()
class DAERUNE_API UDRUnlockData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) TArray<FDRAchievementDef> Achievements;
    UPROPERTY(EditDefaultsOnly) TArray<FDRUnlockRule> UnlockRules;

    // 캐릭터 해금 태그 → EPlayerCharacterClass 매핑
    UPROPERTY(EditDefaultsOnly) TMap<FGameplayTag, EPlayerCharacterClass> CharacterUnlockMap;

    // 코스메틱 해금 태그 → 적용 데이터(메시/머티리얼/니아가라 등) 매핑
    UPROPERTY(EditDefaultsOnly) TMap<FGameplayTag, FDRCosmeticDef> CosmeticMap;
};
```

`FDRCosmeticDef` — 외형 적용 데이터:
```cpp
USTRUCT(BlueprintType)
struct FDRCosmeticDef
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly) EPlayerCharacterClass OwnerClass;
    UPROPERTY(EditDefaultsOnly) FGameplayTag SlotTag;             // Skin/Hat/Trail
    UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<USkeletalMesh> OverrideMesh;     // 선택
    UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UMaterialInterface> OverrideMaterial;
    UPROPERTY(EditDefaultsOnly) TSoftClassPtr<AActor> AttachmentActor;          // 모자/액세서리
    UPROPERTY(EditDefaultsOnly) FName AttachSocket;
    UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UNiagaraSystem> TrailEffect;
};
```

> 에셋은 `TSoftObjectPtr/TSoftClassPtr` 로 두어 **선택 UI 미리보기에서만 로드**(메모리 절약). 기존 `DRAssetManager` 와 연계하여 비동기 로드.

---

## 5. 신규 C++ 클래스 명세

### 5.1 `UDRStatTracker` (UObject, GameInstance 소유)
역할: 인게임 이벤트 → 통계 누적 → 임계값 도달 통지.

```cpp
UCLASS()
class DAERUNE_API UDRStatTracker : public UObject
{
    GENERATED_BODY()
public:
    void Init(UDRGameInstance* InGI, UDRUnlockData* InData);

    // 게임플레이 코드에서 호출 (로컬 플레이어 기준)
    UFUNCTION(BlueprintCallable)
    void AddStat(FGameplayTag StatTag, int64 Delta = 1);

    UFUNCTION(BlueprintCallable)
    void SetStatIfGreater(FGameplayTag StatTag, int64 Value); // 최고기록형

    int64 GetStat(FGameplayTag StatTag) const;

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStatChanged, FGameplayTag, int64);
    FOnStatChanged OnStatChanged;

private:
    void EvaluateAchievements(FGameplayTag ChangedStat); // 관련 도전과제 평가
    UPROPERTY() TObjectPtr<UDRGameInstance> GI;
    UPROPERTY() TObjectPtr<UDRUnlockData> UnlockData;
};
```

**호출 지점 (게임플레이 후크)** — 기존 델리게이트에 바인딩:
- 적 처치: `ADREnemy` 사망 멀티캐스트 / 페이즈의 enemy death 델리게이트 → `Stat.Kills++`, `Stat.WaterCollected += reward`.
- 파트 설치: `ADRCleanserSite` 설치 완료 델리게이트 → `Stat.PartsInstalled++`.
- 페이즈 클리어: `ADRStageGameMode::TransitionToNextPhase()` → `Stat.PhasesCleared` set-max.
- 보스 격파: 엘리트 보스 사망 이벤트 → `Achievement.BossSlayer` 즉시형.
- 무피해 클리어: 페이즈 완료 시 플레이어가 한 번도 컨테이너 피해 안 받음 검사 → `Stat.DamagelessRuns++`.

> **로컬 플레이어만 카운트:** 클라이언트에서 `IsLocallyControlled()` 인 캐릭터 이벤트만 StatTracker로 보낸다. 서버/리슨서버에서 다른 플레이어 이벤트가 섞이지 않도록 주의.

### 5.2 `UDRAchievementSubsystem` (UGameInstanceSubsystem)
역할: 스팀 도전과제 R/W 캡슐화. OnlineSubsystem 추상화로 스팀 외 플랫폼/널 대응.

```cpp
UCLASS()
class DAERUNE_API UDRAchievementSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase&) override;
    virtual void Deinitialize() override;

    // 로그인 후 호출: 스팀에서 현재 도전과제/통계 상태 읽어와 캐시
    void QueryFromBackend();

    // 도전과제 해금 (idempotent — 이미 해금이면 무시)
    void UnlockAchievement(const FString& SteamApiName);

    // 진행형 도전과제 진척도 기록 (스팀 stat write + StoreStats)
    void WriteStat(const FString& StatApiName, int32 Value);

    bool IsAchievementUnlocked(const FString& SteamApiName) const;

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnAchievementUnlocked, FString /*ApiName*/);
    FOnAchievementUnlocked OnAchievementUnlocked;

    bool IsBackendAvailable() const; // 스팀 온라인 여부

private:
    IOnlineAchievementsPtr AchievementsInterface; // OnlineSubsystem
    IOnlineIdentityPtr IdentityInterface;
    void OnQueryComplete(const FUniqueNetId&, const bool bWasSuccessful);
    TMap<FString,bool> CachedAchievements;
};
```

구현 메모:
- `IOnlineSubsystem::Get()->GetAchievementsInterface()` 사용. 스팀이면 자동으로 SteamUserStats로 매핑.
- 도전과제는 **스팀 파트너 페이지에 미리 정의된 것만** 해금 가능. 코드에서 새로 만들 수 없음.
- `WriteAchievements()` 호출 후 스팀이 내부적으로 `StoreStats` → 오버레이 토스트 표시.
- **AppId 480 환경에서는 실제 해금이 안 되거나 Spacewar 도전과제로 뜬다** → 개발 중엔 `IsBackendAvailable()==false` 취급하고 로컬 폴백 경로로 테스트.

### 5.3 `UDRUnlockManager` (UObject, GameInstance 소유)
역할: 도전과제 상태 → 해금 규칙 평가 → SaveGame 반영 → UI 통지.

```cpp
UCLASS()
class DAERUNE_API UDRUnlockManager : public UObject
{
    GENERATED_BODY()
public:
    void Init(UDRGameInstance* GI, UDRUnlockData* Data,
              UDRAchievementSubsystem* Ach, UDRSaveGame* Save);

    // 도전과제 상태 변동/로그인 시 전체 재평가
    void ReevaluateAll();

    UFUNCTION(BlueprintPure)
    bool IsUnlocked(FGameplayTag UnlockableTag) const;

    UFUNCTION(BlueprintPure)
    bool IsCharacterUnlocked(EPlayerCharacterClass Class) const;

    UFUNCTION(BlueprintPure)
    TArray<FGameplayTag> GetUnlockedCosmetics(EPlayerCharacterClass Class) const;

    // UI 안내: 아직 못 깬 해금 + 조건문
    UFUNCTION(BlueprintPure)
    FText GetUnlockHint(FGameplayTag UnlockableTag) const;

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnlocked, FGameplayTag);
    FOnUnlocked OnUnlockableUnlocked; // 신규 해금 시 토스트용

private:
    bool EvaluateRule(const FDRUnlockRule& Rule) const;
    void CommitUnlock(FGameplayTag Tag); // SaveGame 기록 + 저장 + 델리게이트
    /* 참조들 ... */
};
```

평가 흐름:
1. `ReevaluateAll()` 은 **로그인 직후**(스팀 query 완료 콜백) + **도전과제 해금 콜백** 시 호출.
2. 각 `FDRUnlockRule` 에 대해 `RequiredAchievements` 를 (AND/OR)로 검사.
3. 새로 충족된 규칙이 있으면 `CommitUnlock` → `SaveGame.UnlockedContent.Add` → `SaveProgress()` → `OnUnlockableUnlocked` 브로드캐스트.
4. 이미 해금된 건 멱등 처리.

### 5.4 `UDRGameInstance` 확장
```cpp
// 추가 멤버
UPROPERTY() TObjectPtr<UDRStatTracker> StatTracker;
UPROPERTY() TObjectPtr<UDRUnlockManager> UnlockManager;
UPROPERTY(EditDefaultsOnly, Category="Unlock") TObjectPtr<UDRUnlockData> UnlockData;

// 접근자
UDRStatTracker* GetStatTracker() const;
UDRUnlockManager* GetUnlockManager() const;
UDRAchievementSubsystem* GetAchievements() const; // GetSubsystem 래퍼
```

`Init()` 순서:
1. `LoadProgress()` (SaveGame 로드 — 기존)
2. `AchievementSubsystem` 은 GameInstanceSubsystem 이라 자동 생성됨
3. `StatTracker->Init()`, `UnlockManager->Init()`
4. 스팀 로그인 완료 델리게이트에 `Achievements->QueryFromBackend()` 바인딩
5. query 완료 → `UnlockManager->ReevaluateAll()`

---

## 6. 멀티플레이 처리

도전과제/해금은 **계정 단위 로컬 상태**다. 멀티에서 핵심은 "선택 시 검증".

### 6.1 캐릭터 선택
- 기존 캐릭터 선택은 `UDRGameInstance::SavePlayerClassSelection` + `ADRPlayerController`(추정)로 흐름.
- 신규 게이팅:
  - 선택 UI는 `UnlockManager->IsCharacterUnlocked()` 가 false면 잠금 표시.
  - 클라가 `ServerRequestSelectCharacter(EPlayerCharacterClass)` 호출.
  - **서버 검증 옵션 A (신뢰):** 협동 PvE이고 치팅 영향이 미미하므로, 서버는 클라가 보낸 "해금했다"는 주장을 신뢰하고 적용. 가장 단순.
  - **서버 검증 옵션 B (검증):** 클라가 접속 시 자기 해금 목록을 서버로 1회 전송(`ServerReportUnlocks`), 서버가 캐싱해 선택 시 대조. 무결성↑, 구현↑.
  - **권장: 옵션 A**. 협동 PvE + 해금은 "내 계정 자랑"이라 타인 피해 없음. 치팅은 자기 손해. 단, **잠긴 캐릭터로 선택 시도하면 서버가 기본 캐릭터로 폴백**하는 최소 가드만 둔다.

### 6.2 코스메틱 동기화
- 코스메틱은 **순수 외형** → 게임플레이 영향 0.
- `ADRCharacter` 에 복제 프로퍼티 추가:
  ```cpp
  UPROPERTY(ReplicatedUsing=OnRep_Loadout) FDRLoadout ActiveLoadout;
  UFUNCTION() void OnRep_Loadout(); // 메시/머티리얼/어태치/니아가라 적용
  ```
- 흐름: 클라 선택 → `ServerRequestSetLoadout(FDRLoadout)` → 서버가 `ActiveLoadout` 설정 → RepNotify로 전 클라가 외형 적용.
- `OnRep_Loadout()` 에서 `DRAssetManager` 통해 소프트 레퍼런스 **비동기 로드 후 적용** (히치 방지).
- 폰 소유 시점 타이밍: PossessedBy/OnRep_PlayerState 이후 SaveGame의 `EquippedLoadouts` 기본값을 서버로 전송.

### 6.3 리슨 서버 주의
- 리슨 서버 호스트는 자기 자신이 클라이언트이기도 함 → StatTracker는 **`IsLocallyControlled` 기준**으로만 카운트해서 중복/타인 집계 방지.

---

## 7. UI 설계

### 7.1 캐릭터 선택 화면 (대기실/로비)
- 기존 `DRWaitingRoomCameraActor` / 캐릭터 선택 위젯 확장.
- 각 캐릭터 카드:
  - 해금됨: 정상 선택 가능.
  - 잠김: 흑백/자물쇠 아이콘 + 호버 시 `GetUnlockHint()` 툴팁("Phase 3를 클리어하세요").
- `UDRUserWidget` 패턴(컨트롤러→위젯 브로드캐스트) 유지. 신규 `UCharacterSelectWidgetController` 또는 기존 컨트롤러 확장.

### 7.2 꾸미기(코스메틱) 화면
- 캐릭터별 탭 → 슬롯(스킨/모자/트레일) → 항목 그리드.
- 잠긴 항목은 조건 안내. 선택 시 `ServerRequestSetLoadout`.
- 미리보기: 대기실 카메라 앞 프리뷰 폰에 즉시 반영(로컬 프리뷰는 서버 왕복 없이 표시 후, 확정 시 서버 전송).

### 7.3 도전과제 목록 화면 (선택)
- `UDRUnlockData.Achievements` 순회 → 달성/미달성 + 진행도 바(`Stat / RequiredCount`).
- 스팀 오버레이가 토스트를 띄우지만, 인게임 진행도 표시는 별도 제공이 UX상 유리.

### 7.4 해금 토스트
- `OnUnlockableUnlocked` 구독 → 화면 토스트("새 캐릭터 해금: Ranger!"). 오버레이 위젯 컨트롤러에 델리게이트 추가(기존 상태효과 UI 패턴 재사용).

---

## 8. 스팀 백엔드 준비 (게임 외 작업)

이 작업들은 코드와 별개로 **Steamworks 파트너 사이트**에서 선행되어야 실제 동작.

1. 실제 **AppId 발급** (앱 등록). `Config/DefaultEngine.ini` 의 `SteamDevAppId=480` → 실제 ID 교체. `steam_appid.txt` 도 빌드 디렉터리에 배치.
2. **Achievements 정의:** 각 도전과제의 API Name(영문 키), 표시명/설명/아이콘(잠금/해제 2종) 등록.
3. **Stats 정의:** 진행형 도전과제용 누적 stat(INT) 등록 (예: `STAT_KILLS`).
4. 도전과제 ↔ stat 연결(진행형) 설정.
5. Steam Cloud(Auto-Cloud or Remote Storage) 활성화 — SaveGame 동기화. (UE SaveGame은 기본 로컬; 스팀 클라우드 경로 매핑 또는 ISteamRemoteStorage 사용 검토.)
6. 빌드에 **Steamworks SDK** 포함 확인(OnlineSubsystemSteam 플러그인이 래핑).

> 개발 중 검증은 **Spacewar(480)** 환경에서 OnlineSubsystem 인터페이스 호출까지만 확인 가능하고, 실제 도전과제 토스트/영속은 실 AppId 필요.

---

## 9. 폴백 & 엣지 케이스

| 상황 | 처리 |
|---|---|
| 스팀 오프라인/미실행 | `IsBackendAvailable()==false` → StatTracker가 **SaveGame 로컬 통계로만** 누적, UnlockManager가 로컬 통계로 해금 판정. 온라인 복귀 시 스팀에 backfill write. |
| 스팀에는 해금됐는데 로컬 SaveGame엔 없음 (재설치) | 로그인 query 후 `ReevaluateAll`로 SaveGame 재구성 (스팀이 진실의 원천). |
| 로컬엔 해금됐는데 스팀엔 없음 (오프라인 달성 후) | 온라인 시 `UnlockAchievement` backfill. |
| 진행형 도전과제 중복 카운트 | stat은 절대값 write(누적값) 또는 set-max로 멱등 처리. |
| 멀티에서 타인 이벤트 집계 | `IsLocallyControlled` 가드. |
| 소프트 레퍼런스 코스메틱 로드 히치 | `DRAssetManager` 비동기 로드 + 로드 완료 콜백에서 적용. |
| SaveGame 버전 변경 | `SaveVersion` 필드로 마이그레이션 분기. |
| 잠긴 캐릭터 선택 시도(치팅/버그) | 서버 최소 가드로 기본 캐릭터 폴백. |

---

## 10. 구현 단계 (마일스톤)

### M1 — 토대 (백엔드 무관, 로컬만)
- [ ] `DRGameplayTags` 에 `Achievement.*`, `Unlockable.*`, `Stat.*` 추가.
- [ ] `UDRSaveGame` 필드 확장 + 마이그레이션.
- [ ] `FDRLoadout`, `FDRCosmeticDef`, `FDRAchievementDef`, `FDRUnlockRule`, `UDRUnlockData` 정의.
- [ ] `UDRStatTracker` 구현 + GameInstance 통합.
- [ ] `UDRUnlockManager` 구현(로컬 통계 기반 평가).
- [ ] 게임플레이 후크 바인딩(킬/파트/페이즈/보스/무피해).
- ✅ 검증: PIE에서 적 처치 → 통계 누적 → 로컬 해금 → 로그 확인.

### M2 — UI
- [ ] 캐릭터 선택 잠금/해제 표시 + 조건 툴팁.
- [ ] 코스메틱 선택 화면 + 프리뷰.
- [ ] 해금 토스트.
- [ ] (선택) 도전과제 진행도 화면.

### M3 — 멀티플레이
- [ ] `ADRCharacter` `ActiveLoadout` 복제 + `OnRep_Loadout` 외형 적용.
- [ ] `ServerRequestSetLoadout` / `ServerRequestSelectCharacter` + 최소 가드.
- [ ] `DRAssetManager` 비동기 코스메틱 로드.
- ✅ 검증: 2인 PIE에서 서로 다른 캐릭터/스킨이 정확히 보임.

### M4 — 스팀 연동
- [ ] `UDRAchievementSubsystem` 구현(OnlineSubsystem 래핑).
- [ ] StatTracker/UnlockManager ↔ Subsystem 연결.
- [ ] 실제 AppId + Steamworks 도전과제/stat 등록.
- [ ] Steam Cloud SaveGame 동기화.
- ✅ 검증: 실 AppId 빌드에서 도전과제 토스트, 재설치 후 해금 복원.

### M5 — 폴리시 & QA
- [ ] 오프라인/온라인 전환 backfill.
- [ ] 엣지 케이스 표 전수 테스트.
- [ ] 콘텐츠 채우기(실제 캐릭터/코스메틱 에셋, 아이콘, 문구 현지화).

---

## 11. 신규 콘텐츠 정의 (콘텐츠팀 작업 — 채워넣기)

### 11.1 해금 캐릭터 후보
`EPlayerCharacterClass` 에 추가하고 `UPlayerCharacterClassInfo.CharacterBPClasses` / `CharacterUnlockMap` 등록:

| 캐릭터 | 해금 도전과제 | 비고 |
|---|---|---|
| (예) Ranger | `Achievement.PhaseMaster` | 페이즈3 클리어 |
| (예) Bear | `Achievement.BossSlayer` | 엘리트 보스 격파 |
| ... | ... | ECharacterClass에 Bear/Ranger 이미 존재 — 플레이어블 전환 여부 결정 필요 |

> 참고: `ECharacterClass`에는 Ranger/Bear가 있으나 `EPlayerCharacterClass`에는 Gardener/VendingMachine만 있음. 신규 플레이어블은 **`EPlayerCharacterClass` 확장 + 전용 BP/어빌리티/스킬아이콘 위젯** 세트가 필요.

### 11.2 코스메틱 슬롯/항목 (예시)
- 슬롯: Skin(머티리얼/메시), Hat(어태치 액터), Trail(니아가라).
- 캐릭터별 최소 1~2개 해금 항목으로 시작.

---

## 12. 리스크 & 결정 필요 사항

1. **AppId**: 실제 발급 전까지 스팀 도전과제 실동작 불가. → M1~M3는 로컬 폴백으로 선행 개발 가능.
2. **신규 플레이어블 캐릭터 비용**: 캐릭터 1종 = BP + 어빌리티 세트 + 애니메이션 + UI. 코스메틱보다 비쌈. → 초기엔 **코스메틱 위주**로 해금을 채우고 캐릭터 해금은 1~2종으로 제한 권장.
3. **서버 검증 강도(옵션 A vs B)**: 협동 PvE이므로 옵션 A(신뢰) 권장. PvP 추가 시 재검토.
4. **Steam Cloud vs 로컬 SaveGame**: 스팀이 진실의 원천이되 SaveGame을 항상 미러로 유지하는 이중화 채택.
5. **enum 확장 호환성**: `EPlayerCharacterClass` 중간 삽입 금지(SaveGame의 enum 직렬화 깨짐). **반드시 끝에 추가**.

---

## 13. 영향받는 파일 요약

신규:
- `Public/Game/DRStatTracker.h` / `Private/.../DRStatTracker.cpp`
- `Public/Game/DRAchievementSubsystem.h` / `.cpp`
- `Public/Game/DRUnlockManager.h` / `.cpp`
- `Public/Data/DRUnlockData.h` / `.cpp` (+ DataAsset 인스턴스 `DA_UnlockData`)
- UI 위젯/컨트롤러 (캐릭터선택·코스메틱·도전과제·토스트)

수정:
- `DRGameplayTags.h/.cpp` (태그 추가)
- `DRSaveGame.h/.cpp` (필드 확장)
- `DRGameInstance.h/.cpp` (매니저 소유·초기화·로그인 후킹)
- `CharacterClassInfo.h` (`EPlayerCharacterClass` 확장, 신규 캐릭터)
- `ADRCharacter` (Loadout 복제 + 외형 적용)
- `ADRPlayerController` (Server RPC: 선택/로드아웃)
- `ADRStageGameMode` / 페이즈 / `ADREnemy` / `ADRCleanserSite` (StatTracker 후킹)
- `Config/DefaultEngine.ini` (실 AppId)

---

## 14. 다음 행동
1. 위 계획 중 **해금 대상(캐릭터 몇 종 / 코스메틱 슬롯 구성)** 확정.
2. **서버 검증 강도(옵션 A/B)** 확정.
3. 확정되면 **M1(토대)** 부터 착수 — 백엔드 없이도 로컬 폴백으로 전 기능 개발·검증 가능.
