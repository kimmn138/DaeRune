# Plan.md — 스팀 도전과제 연동 및 코스메틱/캐릭터 해금 시스템

> **작성일**: 2026-07-01
> **전면 개정: 2026-07-27** — 원안이 전제한 코드베이스가 Plan2(재화·업그레이드 M1 구현 완료), Plan3(로봇청소기 추가),
> Plan4(현지화 STEP1~4), Plan6(스테이지2 설계)로 크게 달라져 재작성. 원안 대비 변경점은 §0.1 참조.
> **대상**: DaeRune (UE 5.5, GAS, OnlineSubsystemSteam) / 브랜치 `feat/PlayExpo`
> **선행 문서**: Plan2 §5(업적·마일스톤 보상), Plan4 STEP5~7(LOCTEXT/Dashboard), Plan5 §1(FP·TP 메시 분리), Plan6 §5(스테이지2 페이즈 구조)
> **목적**: 스팀 도전과제(Achievement) 달성 → 캐릭터 꾸밈 요소(코스메틱) 및 일부 플레이어블 캐릭터 해금

---

## 0. 한눈에 보기 (TL;DR)

- **업적 판정은 새로 만들지 않는다.** Plan2가 이미 구현한 `FDRStageAchievementDef`(FName 키) + 서버 판정 → `ApplyStageReward` 파이프라인을 **유일한 진실의 원천**으로 쓴다.
- 이 문서가 새로 추가하는 것은 딱 세 가지다:
  1. **스팀 미러링** — Plan2의 `AchievementId`(FName) ↔ 스팀 Achievement API Name 매핑, 달성 시 스팀에 write.
  2. **해금 규칙** — "업적 N개 달성 → `Unlockable.*` 해금" 규칙을 DataAsset으로 정의하고 평가.
  3. **코스메틱 적용** — 해금된 외형을 캐릭터에 입히고 멀티에서 복제.
- 해금 상태는 **로컬 SaveGame(v3)** 에 캐싱하고, 스팀은 **계정 간 이식성**을 담당한다. 충돌 시 **항상 합집합, 절대 회수 없음.**
- 스팀이 오프라인/미실행이어도 **모든 기능이 로컬만으로 동작**해야 한다(개발 중에는 이쪽이 기본 경로).

---

## 0.1 원안(2026-07-01) 대비 변경점 ★

| 원안 | 개정 | 이유 |
|---|---|---|
| `UDRStatTracker` 신설(태그 기반 통계 누적) | **전면 삭제** | Plan2의 `TotalKills` / `ClaimedKillMilestones` / `FDRStageRewardReport`가 같은 일을 이미 함 (§1.2) |
| `FDRAchievementDef` 신설 | **삭제** → `FDRStageAchievementDef`에 `SteamApiName` 컬럼 1개 추가 | 업적 정의가 두 벌이 되는 것을 방지 |
| `Achievement.*` GameplayTag 신설 | **삭제** → Plan2의 `FName AchievementId` 사용 | 세이브에 들어가는 식별자는 이름 변경에 취약한 태그보다 FName이 안전 (§4.1) |
| 통계 누적은 **클라이언트 로컬**(`IsLocallyControlled` 가드) | **서버 판정 → 클라 반영** | 클라는 relevancy 밖에서 죽은 적의 사망 이벤트를 못 받아 집계가 누락됨 (§3.1) |
| SaveGame에 `TSet<FGameplayTag> UnlockedAchievements` 추가 | **추가하지 않음** (동명 필드가 이미 존재) | `DRSaveGame.h:48`에 `TArray<FName> UnlockedAchievements`가 이미 있음 — 컴파일 충돌 |
| `SaveVersion = 1` 신규 필드 | **이미 존재. v2 → v3 비파괴 마이그레이션** | 현행 마이그레이션은 "버전 불일치 = 초기화" 패턴이라 그대로 답습하면 유저 재화가 소실됨 (§4.2) |
| enum은 "반드시 **끝에** 추가" | "**`Count` 직전에** 추가" | `EPlayerCharacterClass`에 `Count` 센티널이 생김 (§1.3) |
| 캐릭터 선택: 카드 UI + 옵션 A(서버가 클라 주장 신뢰) | **순환 선택에서 잠긴 인덱스 스킵** + 클라 해금 보고 RPC | 선택이 이미 서버 권위 순환 방식(`% Count`)이라 옵션 A가 성립하지 않음 (§6.1) |
| `FDRCosmeticDef`에 단일 `OverrideMesh` / `OverrideMaterial` | **FP/TP 각각** 머티리얼 배열 | `ADRCharacter`는 TP `Mesh` + `FirstPersonMesh` 이중 구조이고 청소기는 FP 스켈레톤이 별개 (§4.4) |
| 캐릭터 해금 = 신규 플레이어블 제작 | **기존 3종 중 일부를 해금형으로 출시** | 청소기 1종에 Plan3+Plan5 두 문서 분량이 들어감 — 신규 제작은 비현실적 (§11.1) |
| Steam Cloud "Auto-Cloud 또는 ISteamRemoteStorage 검토" | **Auto-Cloud로 확정** | UE SaveGame은 `Saved/SaveGames/*.sav` — 경로 매핑만으로 코드 변경 0 (§8) |
| 문구 현지화는 M5에서 | **M1 데이터 정의 시점부터** Plan4 STEP7과 묶음 | DataAsset의 `FText`는 Gather 경로 등록이 선행돼야 수집됨 (§7.5) |

---

## 1. 현재 코드베이스 실측 (2026-07-27 기준)

설계 근거가 되는 사실들. **원안 §1의 표는 전부 낡았으므로 이 절이 대체한다.**

### 1.1 SaveGame — 이미 v2, 필드 8개

`Source/DaeRune/Public/DRSaveGame.h`

| 필드 | 타입 | 용도 |
|---|---|---|
| `bHasCompletedTutorial` | `bool` | 튜토리얼 완료 |
| `Currency` / `LifetimeCurrency` | `int32` | 계정 공용 지갑 (Plan2) |
| `TotalKills` | `int32` | **계정 누적 처치 수 — 이미 존재** |
| `ClaimedKillMilestones` | `TArray<int32>` | 마일스톤 중복 지급 방지 |
| **`UnlockedAchievements`** | **`TArray<FName>`** | **Plan2 업적 달성 기록 — 원안의 동명 필드와 충돌** |
| `ClassUpgrades` | `TMap<EPlayerCharacterClass, FDRClassUpgradeState>` | 로봇별 업그레이드 |
| `SaveVersion` | `int32` | 현재 `CurrentSaveVersion = 2` |

- 슬롯: `SaveSlotName = "DaeRunePlayerProgress"`, `UserIndex = 0` **고정** (`DRSaveGame.cpp:6-7`)
  → **해금 캐시는 계정 단위가 아니라 PC 단위다.** §12-2 결정 필요.
- 마이그레이션: `UDRGameInstance::EnsureProgressInitialized()` (`DRGameInstance.cpp:101-172`).
  현재 v1 분기는 재화/업그레이드를 **전부 `Reset()`** 한다 — v3 추가 시 이 패턴을 따라가면 안 된다.

### 1.2 업적·통계 파이프라인이 이미 존재한다 ★

Plan2 M1이 코드·빌드 검증 완료 상태로 다음을 제공한다.

- `FDRStageAchievementDef` — `DRProgressionConfig.h:37-57`
  `{ FName AchievementId (변경 금지), FText DisplayName, FText Description, int32 CurrencyReward, bool bOncePerAccount }`
- `FDRKillMilestone` — `DRProgressionConfig.h:15-29` (누적 처치 임계값 보상, 계정당 1회)
- `FDRStageRewardReport` — `DRProgressionTypes.h:94-117`
  `{ PlayedClass, ClearedPhaseCount, bGameClear, KillCount, TArray<FName> AchievementIds }` ← **서버 판정 결과**
- `UDRGameInstance::ApplyStageReward()` — `DRGameInstance.cpp:454-520`
  마일스톤 멱등 처리(484-488), 업적 멱등 처리(497-506), 지갑 반영·저장·브로드캐스트.

> ⚠️ **호출자가 아직 0개다.** `ApplyStageReward`를 부르는 코드가 없다(= Plan2 M4 미착수).
> 따라서 **이 문서의 M2는 Plan2 M4에 의존**한다. 순서를 어기면 검증할 데이터 소스가 없다.

### 1.3 캐릭터 클래스 — 3종 + `Count` 센티널

`AbilitySystem/Data/CharacterClassInfo.h:29-36`
```cpp
enum class EPlayerCharacterClass : uint8
{ Gardener, VendingMachine, RobotVacuum, Count UMETA(Hidden) };
```
`Count`에 의존하는 코드:
- `DRGameInstance.cpp:126-130` — 업그레이드 상태 lazy 초기화 루프
- `DRPlayerController.cpp:1460` — 클래스 순환 모듈로 연산

→ **신규 enum 값은 반드시 `Count` 직전에 추가**한다. 뒤에 붙이면 두 곳 모두에서 누락된다.

### 1.4 캐릭터 선택은 "카드 UI"가 아니라 서버 권위 순환이다 ★

`ADRPlayerController::ServerRequestChangeClass_Implementation` (`DRPlayerController.cpp:1449-1474`)
```cpp
// 대기실(WaitingRoom) 상태에서만 동작
NewIndex = bNext ? (Cur + 1) % ClassCount : (Cur - 1 + ClassCount) % ClassCount;
PS->SetSelectedPlayerClass(NewClass);
CachedSelectedClass = NewClass;   // Seamless Travel 대비
```
- 클라는 방향(`bNext`)만 보내고 **클래스는 서버가 결정**한다. 클라가 "이 클래스로 해줘"라고 주장하는 경로 자체가 없다.
- 따라서 **서버가 해금 정보를 모르면 잠금을 걸 방법이 없다.** → §6.1.

클래스가 복원되는 경로 3곳(잠금 검사가 함께 필요):
`DRLobbyGameMode.cpp:147-149`, `DRStageGameMode.cpp:37-40`, `DRTutorialGameMode.cpp:29`
그리고 맵 전환 보존: `UDRGameInstance::LoadPlayerClassSelection` (`DRGameInstance.cpp:529-533`, 기본값 `Gardener`).

### 1.5 캐릭터 메시는 FP/TP 이중 구조

`Source/DaeRune/Public/Character/DRCharacter.h:177`
```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;   // + 상속받은 TP Mesh
```
- `UpdateMeshVisibility()`가 소유자/타인 기준으로 가시성을 가른다.
- Plan5 §1.1 실측: **로봇청소기의 FP 스켈레톤은 TP와 완전 별개**(TP 몽타주를 FP에서 재생 불가).
- → 코스메틱을 한쪽에만 적용하면 "**내 스킨이 나한테만 안 보이는**" 버그가 된다.

### 1.6 스팀 설정 현황

`Config/DefaultEngine.ini:110-119`
```ini
[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480          ; ← Spacewar(밸브 SDK 샘플). 실 AppId로 교체 필수
bInitServerOnClient=true
```
- `DaeRune.Build.cs`에 `OnlineSubsystem`, `OnlineSubsystemUtils`가 **이미 Public 의존성**으로 있음 → 모듈 추가 작업 불필요.
- `DefaultPlatformService=Steam`이라 **PIE에서도 Steam이 기본 서비스**다. 스팀 미실행 시 인터페이스가 null인 경로가 상시 발생하므로,
  **`IsBackendAvailable()==false`는 예외가 아니라 개발 중 기본 경로**로 취급한다.

### 1.7 현지화 현황 (Plan4)

- ✅ STEP1~4 완료: `CulturesToStage=ko/en` (`DefaultGame.ini:60-61`), `LocalizationPaths` 등록 (`DefaultEngine.ini:217`),
  부팅 시 컬처 적용 (`DRGameInstance.cpp:21-38`).
- ⬜ STEP5(C++ LOCTEXT화) / STEP6(Dashboard 셋업) / STEP7(DataTable·DataAsset 텍스트) **미완**.
- → 이 문서가 만드는 DataAsset의 `FText`는 **Gather 경로에 에셋이 포함돼야** 수집된다. §7.5.

### 1.8 기타

- `UDRAssetManager` (`Public/DRAssetManager.h`)는 실재하고 `AssetManagerClassName`으로 등록됨(`DefaultEngine.ini:103`).
  단 현재 기능은 **GameplayTags/GAS 초기화 + 사운드 사전 로딩뿐**이며 **범용 비동기 로드 API가 없다.**
  → 코스메틱 비동기 로드는 신규 헬퍼를 추가하거나 `GetStreamableManager().RequestAsyncLoad`를 직접 쓴다.
- 기존 파일 다수가 한글 주석 모지바케 상태(예: `DRGameplayTags.h`, `DRPlayerController.cpp`).
  **신규 파일은 UTF-8(BOM) 고정**, 기존 파일 수정 시 주변 인코딩 보존 (Plan6 §0 관례 계승).

---

## 2. 목표 및 비목표

### 목표
1. Plan2 업적 달성 시 **스팀 Achievement에 미러링** (스팀 오버레이 토스트).
2. 업적 달성 조합으로 **캐릭터 꾸밈 요소(코스메틱) 해금** — 1차 범위는 **색상/머티리얼 스킨**.
3. 업적 달성으로 **기존 플레이어블 캐릭터 일부를 해금형으로 게이팅** (신규 캐릭터 제작 아님).
4. 해금 상태가 **재접속·재설치 후에도 유지** (로컬 SaveGame v3 + Steam Cloud Auto-Cloud).
5. 대기실에서 **잠긴 캐릭터는 순환에서 제외**, 코스메틱 화면에서 잠금 항목은 조건 안내 표시.
6. **스팀 없이도 전 기능이 로컬로 동작**한다.

### 비목표 (이번 범위 밖)
- 유료 DLC / 마이크로트랜잭션, Steam Inventory Service, 드롭·거래.
- 시즌패스/배틀패스.
- 서버 권위형 전적 DB.
- **신규 플레이어블 캐릭터 제작** (§11.1 — 비용상 별도 계획으로 분리).
- **메시 교체형 스킨 / 부착물(모자) / 트레일 이펙트** — 2차 범위. 1차는 머티리얼만 (§4.4).

---

## 3. 아키텍처

```
[서버: 게임플레이]
  ADRStageGameMode / 페이즈 / ADREnemy / ADRCleanserSite
        │  스테이지 종료 시 성과 집계 (Plan2 M4 담당)
        ▼
  FDRStageRewardReport { PlayedClass, ClearedPhaseCount, bGameClear, KillCount, AchievementIds[] }
        │  Client_GrantStageReward (소유 클라에게만)
        ▼
─────────────────────────────── 클라이언트 경계 ───────────────────────────────
[UDRGameInstance::ApplyStageReward]   ← 이미 구현됨 (DRGameInstance.cpp:454)
   - 재화 환산 / 마일스톤·업적 멱등 처리 / SaveGame 기록
   - 반환: FDRStageRewardResult { ..., NewlyUnlockedAchievements[] }
        │
        ├──▶ [UDRAchievementSubsystem]  (신규, UGameInstanceSubsystem)
        │       AchievementId(FName) → SteamApiName 매핑 → 스팀 write
        │       (백엔드 없으면 no-op, 온라인 복귀 시 backfill)
        │
        └──▶ [UDRUnlockManager]  (신규, GameInstance 소유)
                UDRUnlockData 규칙 평가: 업적 집합 → Unlockable.* 해금
                SaveGame.UnlockedContent 기록 + 저장
                        │  OnUnlockableUnlocked(FGameplayTag)
                        ├──▶ 해금 토스트 UI
                        ├──▶ 대기실: 잠긴 클래스 스킵 (ServerReportUnlockedClasses)
                        └──▶ 코스메틱 UI / ADRCharacter 외형 적용
```

### 3.1 권위 모델 (원안에서 뒤집힌 부분)

| 대상 | 권위 | 근거 |
|---|---|---|
| 업적 **달성 판정** | **서버** | 적 처치·페이즈 클리어·무피해 여부는 전부 서버만 정확히 안다. 클라는 relevancy 밖 사망 이벤트를 못 받아 누락됨 |
| 업적 **보상 지급/중복 방지** | 클라이언트(SaveGame) | Plan2 §7.7 확정 사항. 계정 진행도는 로컬이 소유 |
| 해금 **판정** | 클라이언트 | 계정 단위 상태. 서버가 알 필요 없음 |
| 캐릭터 **선택 권한** | 서버(클라 보고 기반) | 선택 자체가 이미 서버 권위 (§1.4) |
| 코스메틱 **외형** | 서버 복제(단순 전달) | 게임플레이 영향 0 — 검증 불필요, 동기화만 필요 |

> 원안의 "`IsLocallyControlled` 가드로 로컬 집계" 원칙(§3, §5.1, §6.3)은 **폐기**한다.

---

## 4. 데이터 모델

### 4.1 식별자 전략 — 2단 구조

| 대상 | 식별자 | 이유 |
|---|---|---|
| 업적 | **`FName AchievementId`** (Plan2 기존) | 세이브에 저장됨. 태그는 이름 변경 시 리다이렉터가 없으면 저장 데이터가 깨짐. `DRProgressionConfig.h:41`에 "변경 금지" 명시 |
| 해금 콘텐츠 | **`FGameplayTag Unlockable.*`** | 저장되지만 개수가 적고 계층 조회(`Unlockable.Cosmetic.Gardener.*`)의 이점이 큼. 태그 rename 시 `Config/DefaultGameplayTags.ini` 리다이렉터를 반드시 등록한다 |

`DRGameplayTags.h/.cpp`에 추가할 태그 (원안의 `Achievement.*` / `Stat.*`는 **추가하지 않음**):
```
Unlockable.Character.VendingMachine
Unlockable.Character.RobotVacuum

Unlockable.Cosmetic.Gardener.Blue
Unlockable.Cosmetic.Gardener.White
Unlockable.Cosmetic.VendingMachine.<...>
Unlockable.Cosmetic.RobotVacuum.<...>

Cosmetic.Slot.Skin        // 슬롯 식별 (2차 범위에서 Hat/Trail 추가)
```

### 4.2 `UDRSaveGame` — v2 → v3 (**비파괴**)

```cpp
// 신규 필드만 추가. 기존 필드는 전부 그대로 둔다.

// 해금된 콘텐츠 (캐릭터 + 코스메틱 통합)
UPROPERTY(VisibleAnywhere, Category = "Progress|Unlock")
TSet<FGameplayTag> UnlockedContent;

// 캐릭터별 장착 중인 외형
UPROPERTY(VisibleAnywhere, Category = "Progress|Unlock")
TMap<EPlayerCharacterClass, FDRLoadout> EquippedLoadouts;

// 스팀에 아직 write하지 못한 업적 (오프라인 달성분 backfill 큐)
UPROPERTY(VisibleAnywhere, Category = "Progress|Unlock")
TArray<FName> PendingSteamAchievements;

static constexpr int32 CurrentSaveVersion = 3;   // 2 → 3
```

**마이그레이션 (`EnsureProgressInitialized`) 필수 규칙:**
```cpp
if (SaveVersion == 1) { /* 기존 파괴적 초기화 — 그대로 유지 */ }

// ★ v2 → v3 는 비파괴. 신규 필드는 기본값이면 충분하므로 초기화 코드 자체가 불필요하다.
//   Currency / TotalKills / ClassUpgrades / UnlockedAchievements 를 절대 Reset() 하지 말 것.
SaveVersion = CurrentSaveVersion;
```

> ⚠️ 현행 코드(`DRGameInstance.cpp:109-123`)는 "버전 불일치 → 초기화" 형태로 읽히기 쉽다.
> v3 분기를 추가할 때 v1 분기를 복사·붙여넣기 하면 **유저 재화와 업그레이드가 전부 소실**된다.

`FDRLoadout` (1차 범위 — 슬롯 1개):
```cpp
USTRUCT(BlueprintType)
struct FDRLoadout
{
    GENERATED_BODY()

    // Unlockable.Cosmetic.<Class>.<Name>. 비어 있으면 BP 기본 외형.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic")
    FGameplayTag SkinTag;

    // 2차 범위: HatTag / TrailTag

    bool operator==(const FDRLoadout& O) const { return SkinTag == O.SkinTag; }
};
```

### 4.3 `FDRStageAchievementDef` 확장 (Plan2 구조체에 컬럼 1개)

```cpp
// DRProgressionConfig.h — 기존 구조체에 추가
// 스팀 파트너 페이지에 등록한 Achievement API Name. 비우면 스팀 미러링 대상 아님.
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Achievement|Steam")
FString SteamApiName;
```
→ 원안의 `FDRAchievementDef`는 만들지 않는다. `DisplayName` / `Description` / `bOncePerAccount`는 이미 있다.

### 4.4 `UDRUnlockData` (신규 DataAsset) — 해금 규칙

```cpp
USTRUCT(BlueprintType)
struct FDRUnlockRule
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Unlock")
    FGameplayTag UnlockableTag;                     // Unlockable.*

    // Plan2 업적 ID (FName). 비어 있으면 "기본 제공"(항상 해금).
    UPROPERTY(EditDefaultsOnly, Category = "Unlock")
    TArray<FName> RequiredAchievements;

    UPROPERTY(EditDefaultsOnly, Category = "Unlock")
    bool bRequireAll = true;                        // AND / OR

    // 누적 처치 수 조건 (SaveGame.TotalKills 와 비교, 0 = 미사용)
    UPROPERTY(EditDefaultsOnly, Category = "Unlock")
    int32 RequiredTotalKills = 0;

    UPROPERTY(EditDefaultsOnly, Category = "Unlock")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, Category = "Unlock")
    FText HowToUnlock;                              // 잠금 툴팁 문구
};
```

`FDRCosmeticDef` — **FP/TP 양쪽 적용** (원안 대비 핵심 수정):
```cpp
USTRUCT(BlueprintType)
struct FDRCosmeticDef
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, Category = "Cosmetic")
    EPlayerCharacterClass OwnerClass = EPlayerCharacterClass::Gardener;

    UPROPERTY(EditDefaultsOnly, Category = "Cosmetic")
    FGameplayTag SlotTag;                           // Cosmetic.Slot.Skin

    // 3인칭 메시 머티리얼 오버라이드. Index = 머티리얼 슬롯 인덱스.
    UPROPERTY(EditDefaultsOnly, Category = "Cosmetic|TP")
    TArray<TSoftObjectPtr<UMaterialInterface>> ThirdPersonMaterials;

    // 1인칭 메시 머티리얼 오버라이드. 청소기처럼 FP 스켈레톤이 별개인 경우 필수.
    UPROPERTY(EditDefaultsOnly, Category = "Cosmetic|FP")
    TArray<TSoftObjectPtr<UMaterialInterface>> FirstPersonMaterials;

    UPROPERTY(EditDefaultsOnly, Category = "Cosmetic|UI")
    TSoftObjectPtr<UTexture2D> PreviewIcon;

    // 2차 범위: OverrideMesh(FP/TP), AttachmentActor + AttachSocket(FP/TP), TrailEffect
};
```

```cpp
UCLASS()
class DAERUNE_API UDRUnlockData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category = "Unlock", meta = (TitleProperty = "UnlockableTag"))
    TArray<FDRUnlockRule> UnlockRules;

    // 캐릭터 해금 태그 → 클래스
    UPROPERTY(EditDefaultsOnly, Category = "Unlock")
    TMap<FGameplayTag, EPlayerCharacterClass> CharacterUnlockMap;

    // 코스메틱 해금 태그 → 적용 데이터
    UPROPERTY(EditDefaultsOnly, Category = "Unlock")
    TMap<FGameplayTag, FDRCosmeticDef> CosmeticMap;

    const FDRUnlockRule* FindRule(FGameplayTag UnlockableTag) const;
    const FDRCosmeticDef* FindCosmetic(FGameplayTag CosmeticTag) const;

#if WITH_EDITOR
    // 규칙이 참조하는 AchievementId 가 ProgressionConfig 에 실재하는지 검증
    void ValidateAgainst(const UDRProgressionConfig* Config) const;
#endif
};
```
`UDRProgressionConfig`에 참조 추가:
```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unlock")
TObjectPtr<UDRUnlockData> UnlockData;
```
> GameInstance BP에 이미 `ProgressionConfig` 슬롯이 있으므로(`DRGameInstance.h:42`) 새 슬롯을 늘리지 않고 여기에 매단다.

---

## 5. 신규 C++ 클래스 명세

> **원안 §5.1 `UDRStatTracker`는 삭제되었다.** 통계는 Plan2가 이미 누적한다.

### 5.1 `UDRUnlockManager` (UObject, GameInstance 소유)

```cpp
UCLASS()
class DAERUNE_API UDRUnlockManager : public UObject
{
    GENERATED_BODY()
public:
    void Init(UDRGameInstance* InGI);

    // 업적/통계 변동 시, 그리고 부팅·로그인 직후 전체 재평가.
    // 새로 충족된 규칙만 CommitUnlock 한다(멱등).
    void ReevaluateAll();

    UFUNCTION(BlueprintPure, Category = "Unlock")
    bool IsUnlocked(FGameplayTag UnlockableTag) const;

    UFUNCTION(BlueprintPure, Category = "Unlock")
    bool IsCharacterUnlocked(EPlayerCharacterClass CharacterClass) const;

    // 해금 규칙이 아예 없는 클래스는 "기본 제공"으로 true 를 반환한다(안전 기본값).
    UFUNCTION(BlueprintPure, Category = "Unlock")
    TArray<EPlayerCharacterClass> GetUnlockedCharacters() const;

    UFUNCTION(BlueprintPure, Category = "Unlock")
    TArray<FGameplayTag> GetCosmeticsForClass(EPlayerCharacterClass CharacterClass, bool bUnlockedOnly) const;

    UFUNCTION(BlueprintPure, Category = "Unlock")
    FText GetUnlockHint(FGameplayTag UnlockableTag) const;   // 잠금 툴팁

    // 장착 (SaveGame 기록 + 즉시 저장). 잠긴 항목이면 false.
    UFUNCTION(BlueprintCallable, Category = "Unlock")
    bool EquipCosmetic(EPlayerCharacterClass CharacterClass, FGameplayTag CosmeticTag);

    UFUNCTION(BlueprintPure, Category = "Unlock")
    FDRLoadout GetEquippedLoadout(EPlayerCharacterClass CharacterClass) const;

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnlocked, FGameplayTag);
    FOnUnlocked OnUnlockableUnlocked;   // 신규 해금 토스트용

private:
    bool EvaluateRule(const FDRUnlockRule& Rule) const;   // SaveGame 의 UnlockedAchievements / TotalKills 참조
    void CommitUnlock(FGameplayTag Tag);

    UPROPERTY() TObjectPtr<UDRGameInstance> GI;
};
```

**평가 시점**
1. `UDRGameInstance::Init()` — `LoadProgress()` 직후 (오프라인에서도 즉시 정상 동작)
2. `ApplyStageReward()` 반환 직후 — `NewlyUnlockedAchievements`가 비지 않았을 때
3. 스팀 `QueryAchievements` 완료 콜백 — 다른 PC에서 딴 업적 반영

**안전 기본값**: 규칙에 등장하지 않는 `Unlockable` / 클래스는 **해금된 것으로 간주**한다.
DataAsset 미지정 상태에서 모든 캐릭터가 잠겨 게임을 못 하는 사고를 막는다.

### 5.2 `UDRAchievementSubsystem` (UGameInstanceSubsystem)

역할: 스팀 도전과제 R/W 캡슐화 + 백엔드 부재 시 안전한 no-op.

```cpp
UCLASS()
class DAERUNE_API UDRAchievementSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 스팀 로그인 상태 + Achievements 인터페이스 유효성. 미실행/오프라인이면 false.
    UFUNCTION(BlueprintPure, Category = "Achievement")
    bool IsBackendAvailable() const;

    // 로그인 후 1회: 스팀에서 도전과제 상태/설명 읽어와 캐시.
    void QueryFromBackend();

    // Plan2 AchievementId 목록을 스팀에 반영. 백엔드 없으면 PendingSteamAchievements 에 큐잉.
    void ReportAchievements(const TArray<FName>& AchievementIds);

    // 온라인 복귀 시 큐 비우기
    void FlushPending();

    DECLARE_MULTICAST_DELEGATE(FOnBackendSynced);
    FOnBackendSynced OnBackendSynced;   // → UnlockManager::ReevaluateAll

private:
    void OnQueryComplete(const FUniqueNetId& PlayerId, const bool bWasSuccessful);

    // 캐시: SteamApiName → 달성 여부
    TMap<FString, bool> CachedAchievements;
};
```

**구현 주의 — 실제 UE API 형태 (원안 §5.2의 시그니처는 존재하지 않는다)**
- 인터페이스 획득: `IOnlineSubsystem::Get()->GetAchievementsInterface()` → `IOnlineAchievementsPtr`,
  `GetIdentityInterface()` → 로그인된 `FUniqueNetIdPtr`. **모든 호출에 유효한 `FUniqueNetId`가 필요**하다.
- 쓰기: `UnlockAchievement(FString)` 같은 함수는 **없다**.
  `FOnlineAchievementsWriteRef`를 만들어 API Name에 진행도(100 = 달성)를 넣고
  `WriteAchievements(PlayerId, WriteObject, Delegate)`를 호출한다.
- 읽기: `QueryAchievements` / `QueryAchievementDescriptions` 완료 후 `GetCachedAchievement(...)`로 조회.
- 도전과제는 **스팀 파트너 페이지에 미리 정의된 것만** 해금 가능하다. 코드로 생성 불가.
- **AppId 480(Spacewar) 환경에서는 실제 해금이 되지 않거나 Spacewar 도전과제로 뜬다.**
  개발 중에는 `IsBackendAvailable()==false` 취급하고 로컬 경로로 검증한다.

### 5.3 `UDRGameInstance` 확장

```cpp
// 추가 멤버
UPROPERTY() TObjectPtr<UDRUnlockManager> UnlockManager;

UFUNCTION(BlueprintPure, Category = "Unlock")
UDRUnlockManager* GetUnlockManager() const;

UFUNCTION(BlueprintPure, Category = "Achievement")
UDRAchievementSubsystem* GetAchievements() const;   // GetSubsystem 래퍼
```

`Init()` 개정 순서:
```
1. 컬처 적용 (기존, DRGameInstance.cpp:21-38)
2. LoadProgress()  ─ v2→v3 비파괴 마이그레이션 포함
3. UnlockManager 생성 + Init()
4. UnlockManager->ReevaluateAll()          ← 오프라인에서도 여기까지로 완전 동작
5. Achievements(Subsystem)가 사용 가능하면 QueryFromBackend() + FlushPending()
6. OnBackendSynced → ReevaluateAll() 재호출
```

`ApplyStageReward()` 말미에 2줄 추가 (기존 로직 무변경):
```cpp
if (Result.NewlyUnlockedAchievements.Num() > 0)
{
    GetAchievements()->ReportAchievements(Result.NewlyUnlockedAchievements);
    UnlockManager->ReevaluateAll();
}
```

---

## 6. 멀티플레이 처리

해금은 **계정 단위 로컬 상태**다. 멀티에서 필요한 것은 (a) 캐릭터 선택 게이팅, (b) 코스메틱 외형 동기화 둘뿐이다.

### 6.1 캐릭터 선택 — 순환에서 잠긴 인덱스 스킵 ★ (원안 §6.1 전면 교체)

원안의 옵션 A(서버가 클라 주장을 신뢰)는 **선택이 이미 서버 권위 순환**이라 성립하지 않는다(§1.4).
서버가 잠금을 알아야만 게이팅이 가능하므로, **클라의 1회 보고가 필수**다.

**흐름**
1. 대기실 진입 시(`ADRPlayerController::BeginPlay` 또는 로비 상태 진입) 클라가 1회 보고:
   ```cpp
   UFUNCTION(Server, Reliable)
   void ServerReportUnlockedClasses(const TArray<EPlayerCharacterClass>& Unlocked);
   ```
2. 서버는 `ADRPlayerState`에 캐시:
   ```cpp
   UPROPERTY(Replicated)  // 대기실 UI가 타인 카드 표시에 쓸 수 있으므로 복제
   TArray<EPlayerCharacterClass> UnlockedClasses;
   ```
   **보고를 못 받았거나 빈 배열이면 "전부 해금"으로 간주**한다(구버전 클라/타이밍 사고 방지).
3. `ServerRequestChangeClass`가 잠긴 인덱스를 건너뛰며 순환:
   ```cpp
   int32 Index = CurrentIndex;
   for (int32 Step = 0; Step < ClassCount; ++Step)
   {
       Index = bNext ? (Index + 1) % ClassCount
                     : (Index - 1 + ClassCount) % ClassCount;
       if (PS->IsClassUnlocked(static_cast<EPlayerCharacterClass>(Index))) break;
   }
   ```
   (전부 잠긴 병리적 상황에서도 루프가 `ClassCount`회로 종료돼 무한루프가 없다)
4. **복원 경로 3곳에도 동일 검사**를 넣어, 잠긴 클래스가 복원되면 `Gardener`로 폴백:
   `DRLobbyGameMode.cpp:147-149`, `DRStageGameMode.cpp:37-40`, `DRTutorialGameMode.cpp:29`

**치팅 관점**: 협동 PvE라 조작된 보고는 자기 손해뿐이다. 검증은 하지 않는다(원안의 판단은 유지).
바뀐 것은 "신뢰하되 **보고 자체는 반드시 받아야 한다**"는 점이다.

### 6.2 코스메틱 동기화

- 코스메틱은 순수 외형 → **검증 불필요, 동기화만 필요.**
- `ADRCharacter`에 복제 프로퍼티 추가:
  ```cpp
  UPROPERTY(ReplicatedUsing = OnRep_Loadout)
  FDRLoadout ActiveLoadout;

  UFUNCTION()
  void OnRep_Loadout();

  UFUNCTION(Server, Reliable)
  void ServerSetLoadout(FDRLoadout NewLoadout);

  void ApplyLoadoutVisuals();   // FP/TP 양쪽에 머티리얼 적용
  ```
- **적용 시 FP/TP를 모두 처리**한다(§1.5):
  ```cpp
  // TP
  for (int32 i = 0; i < Def->ThirdPersonMaterials.Num(); ++i)
      GetMesh()->SetMaterial(i, LoadedTP[i]);
  // FP
  for (int32 i = 0; i < Def->FirstPersonMaterials.Num(); ++i)
      FirstPersonMesh->SetMaterial(i, LoadedFP[i]);
  ```
- **비동기 로드**: `TSoftObjectPtr` → `UAssetManager::GetStreamableManager().RequestAsyncLoad`.
  로드 완료 콜백에서 적용하며, 콜백 안에서 `IsValid(this)`와 "요청 시점 로드아웃 == 현재 로드아웃"을 재확인한다
  (로드 도중 사망/재스폰/재선택 시 엉뚱한 외형이 입혀지는 것을 방지).
- **초기 전송 타이밍**: `ADRCharacter::PossessedBy`(서버) 이후 소유 클라가 `OnRep_PlayerState` 시점에
  SaveGame의 `EquippedLoadouts[내 클래스]`를 `ServerSetLoadout`으로 보낸다. 늦게 들어온 클라는 RepNotify로 자동 수신.
- **로비 프리뷰 캐릭터**(`DRLobbyGameMode.cpp:319` `SpawnDisplayCharacter`)에도 동일 적용이 필요하다.

### 6.3 리슨 서버

원안의 "`IsLocallyControlled` 가드로 중복 집계 방지"는 통계를 서버가 집계하도록 바뀌면서 **불필요해졌다**.
남는 주의점은 하나뿐: 리슨 서버 호스트는 `ApplyStageReward`를 자기 클라이언트 경로로 **정확히 1회만** 타야 한다(Plan2 M4 범위).

---

## 7. UI 설계

### 7.1 대기실 캐릭터 선택
- 기존 순환 UI 유지. 잠긴 클래스는 **순환에서 아예 제외**되므로 별도 잠금 카드가 없어도 동작한다.
- (선택) 잠금 안내를 보여주려면 별도 목록 패널에서 `GetUnlockHint()` 출력.

### 7.2 코스메틱 선택 화면
- 캐릭터별 탭 → 스킨 그리드(1차 범위는 슬롯 1개).
- 잠긴 항목: 흑백 + 자물쇠 + 호버 시 `HowToUnlock` 툴팁.
- 선택 시 `UnlockManager->EquipCosmetic()` (로컬 즉시 저장) → 인게임 진입 시 `ServerSetLoadout`으로 반영.
- **로비 프리뷰에 즉시 반영**은 로컬 적용으로 처리(서버 왕복 불필요).

### 7.3 업적 목록 화면 (선택)
- `UDRProgressionConfig::Achievements` 순회 → `SaveGame.UnlockedAchievements.Contains()`로 달성 표시.
- 누적 처치 진행도는 `GetTotalKills()` / `FDRKillMilestone.TotalKills` 비율로 바 표시.

### 7.4 해금 토스트
- `OnUnlockableUnlocked` 구독 → "새 스킨 해금!" 토스트.
- 기존 `UOverlayWidgetController`의 상태효과 브로드캐스트 패턴 재사용.
- 스테이지 결과창(Plan2 M4)에서 `NewlyUnlockedAchievements`와 함께 묶어 보여주는 편이 자연스럽다.

### 7.5 현지화 (Plan4 연동) ★
- 신규 C++ 문자열은 **처음부터 `LOCTEXT`/`NSLOCTEXT`** 로 작성 (Plan4 STEP5 관례).
- `DA_UnlockData` / `DA_ProgressionConfig`의 `FText` 컬럼(`DisplayName`, `Description`, `HowToUnlock`)은
  **Localization Dashboard의 Gather 경로에 해당 에셋 폴더가 포함돼야** 수집된다 → Plan4 **STEP7과 동시에 처리**.
- M1에서 DataAsset을 만들 때 이 점을 무시하면 M4에서 문구를 전부 다시 넣어야 한다.

---

## 8. 스팀 백엔드 준비 (게임 외 작업)

코드와 별개로 **Steamworks 파트너 사이트**에서 선행되어야 실제 동작한다.

1. **실 AppId 발급** → `Config/DefaultEngine.ini:115`의 `SteamDevAppId=480` 교체 + 빌드 디렉터리에 `steam_appid.txt` 배치.
2. **Achievements 정의**: API Name(영문 키), 표시명/설명, 아이콘(잠금·해제 2종) 등록.
   API Name은 `FDRStageAchievementDef.SteamApiName`과 **문자 단위로 일치**해야 한다.
3. **Stats 정의**: 진행형 도전과제를 쓸 경우 누적 stat(INT) 등록 + 도전과제와 연결.
   (1차 범위는 즉시형만 써도 무방 — 진행도는 로컬 `TotalKills`가 이미 갖고 있다.)
4. **Steam Cloud = Auto-Cloud로 확정.**
   UE SaveGame 경로(`%USERDIR%` 하위 `Saved/SaveGames/*.sav`)를 Auto-Cloud 규칙에 등록하면 **코드 변경이 0**이다.
   `ISteamRemoteStorage` 직접 사용은 검토하지 않는다.
5. Steamworks SDK 포함 확인(OnlineSubsystemSteam 플러그인이 래핑, 이미 활성).

> 개발 중에는 Spacewar(480)로 **인터페이스 호출 경로까지만** 확인 가능하다. 토스트/영속 검증은 실 AppId 필요.

---

## 9. 폴백 & 엣지 케이스

**대원칙: 해금은 항상 합집합(union). 어떤 경로로도 회수하지 않는다.**
(원안 §9는 "스팀이 진실의 원천"과 "로컬 → 스팀 backfill"이 서로 모순이었다. 합집합 규칙으로 해소한다.)

| 상황 | 처리 |
|---|---|
| 스팀 미실행/오프라인 (개발 중 기본 상태) | `IsBackendAvailable()==false` → 스팀 호출 전부 no-op. 업적·해금은 SaveGame만으로 완전 동작. 달성분은 `PendingSteamAchievements`에 큐잉 |
| 온라인 복귀 | `FlushPending()` → 큐를 스팀에 write 후 비움 (스팀 write는 멱등이라 중복 안전) |
| 재설치 — 스팀엔 있고 로컬엔 없음 | `QueryFromBackend` 완료 → 달성된 SteamApiName 역매핑 → `SaveGame.UnlockedAchievements`에 **추가** → `ReevaluateAll` |
| 오프라인 달성 — 로컬엔 있고 스팀엔 없음 | backfill write. **로컬 기록은 지우지 않는다** |
| 스팀 계정 전환 (같은 PC) | 현재 슬롯이 고정 문자열이라 **해금이 섞인다.** §12-2 결정 사항 |
| DataAsset 미지정 / 규칙 없음 | 해당 콘텐츠는 **해금된 것으로 간주**(안전 기본값). 게임이 잠기는 사고 방지 |
| 코스메틱 비동기 로드 중 캐릭터 파괴·재선택 | 콜백에서 `IsValid(this)` + 로드아웃 일치 재확인 후 적용 |
| 잠긴 클래스가 세이브/맵전환으로 복원됨 | 3개 복원 경로에서 검사 → `Gardener` 폴백 (§6.1-4) |
| `Unlockable` 태그 이름 변경 | `Config/DefaultGameplayTags.ini`에 리다이렉터 필수 등록 |
| SaveGame v2 → v3 | **비파괴 마이그레이션.** 기존 필드 절대 Reset 금지 (§4.2) |
| `EPlayerCharacterClass` 값 추가 | **`Count` 직전에만** 추가 (§1.3) |

---

## 10. 구현 단계 (마일스톤)

> **선행 조건: Plan2 M4(재화 획득 파이프라인)가 먼저 끝나야 한다.**
> `ApplyStageReward`를 부르는 코드가 아직 없어(§1.2) 그 전에는 업적이 한 건도 기록되지 않는다.
> M1은 Plan2 M4와 **병행 가능**하지만, M2부터는 의존한다.

### M1 — 데이터 토대 (백엔드·게임플레이 무관, 순수 추가)
- [ ] `DRGameplayTags`에 `Unlockable.*` / `Cosmetic.Slot.Skin` 추가
- [ ] `UDRSaveGame` v3 필드 3개 추가 + **비파괴** 마이그레이션 (§4.2)
- [ ] `FDRStageAchievementDef`에 `SteamApiName` 컬럼 추가
- [ ] `FDRLoadout`, `FDRCosmeticDef`, `FDRUnlockRule`, `UDRUnlockData` 정의 (+ `UDRProgressionConfig.UnlockData` 참조)
- [ ] `UDRUnlockManager` 구현 + `UDRGameInstance` 통합 (`Init()` 4단계)
- [ ] `DA_UnlockData` 인스턴스 생성 — 문구는 **처음부터 LOCTEXT/현지화 대상 폴더에** (§7.5)
- ✅ 검증: 콘솔/치트로 `UnlockedAchievements`에 ID를 심었을 때 `IsUnlocked()`가 참이 되고 재시작 후에도 유지

### M2 — 해금 → 게임플레이 반영 (Plan2 M4 이후)
- [ ] `ApplyStageReward` 말미에 `ReevaluateAll()` 훅 2줄
- [ ] `ADRPlayerState.UnlockedClasses` + `ServerReportUnlockedClasses` RPC
- [ ] `ServerRequestChangeClass` 잠긴 인덱스 스킵 + 복원 경로 3곳 폴백 검사
- ✅ 검증: 잠긴 클래스가 대기실 순환에서 빠지고, 해금 후 다시 나타남

### M3 — 코스메틱 외형 + 멀티 동기화
- [ ] `ADRCharacter.ActiveLoadout` 복제 + `OnRep_Loadout` + `ApplyLoadoutVisuals`(FP/TP 양쪽)
- [ ] 비동기 로드 + 유효성 재확인 콜백
- [ ] `ServerSetLoadout` + 초기 전송 타이밍(`OnRep_PlayerState`)
- [ ] 로비 프리뷰 캐릭터에도 적용
- ✅ 검증: 2인 PIE에서 **서로 다른 스킨이 양쪽 모두 정확히** 보이고, **본인 1인칭 시점에도** 반영됨

### M4 — UI
- [ ] 코스메틱 선택 화면 + 잠금 툴팁 + 로컬 프리뷰
- [ ] 해금 토스트 (결과창 연동)
- [ ] (선택) 업적 목록/진행도 화면
- [ ] Plan4 STEP7과 함께 DataAsset 텍스트 Gather 경로 등록

### M5 — 스팀 연동
- [ ] `UDRAchievementSubsystem` 구현 (실제 `IOnlineAchievements` API 기준, §5.2)
- [ ] `ReportAchievements` / `FlushPending` / `QueryFromBackend` + `OnBackendSynced` 배선
- [ ] 실 AppId + Steamworks 도전과제 등록 + `steam_appid.txt`
- [ ] Steam Cloud Auto-Cloud 경로 등록
- ✅ 검증: 실 AppId 빌드에서 도전과제 토스트, 세이브 삭제 후 스팀 상태로 해금 복원

### M6 — 폴리시 & QA
- [ ] 오프라인 → 온라인 backfill 시나리오
- [ ] §9 엣지 케이스 전수 테스트
- [ ] 콘텐츠 채우기(스킨 에셋, 아이콘, 한/영 문구)
- [ ] (2차 범위 착수 판단) 메시 교체 스킨 / 부착물 / 트레일

---

## 11. 신규 콘텐츠 정의 (콘텐츠팀 작업)

### 11.1 캐릭터 해금 — **신규 제작이 아니라 기존 3종의 게이팅**

원안은 `Ranger` / `Bear`를 플레이어블로 전환하는 안을 담았으나, **비용 실측 결과 비현실적**이다:
로봇청소기 1종에 Plan3 + Plan5 **문서 두 개**, 어빌리티 5종, FP/TP 애니 14종, 전용 ABP, 신규 투사체 클래스가 들어갔다.

권장안:

| 클래스 | 해금 조건 | 비고 |
|---|---|---|
| `Gardener` | **기본 제공** (규칙 없음) | 폴백 클래스이므로 절대 잠그지 않는다 |
| `VendingMachine` | (예) 스테이지1 클리어 업적 | 규칙 미정 시 자동으로 해금 취급 |
| `RobotVacuum` | (예) 누적 처치 N 또는 스테이지1 무피해 클리어 | `RequiredTotalKills` 활용 가능 |

> `ECharacterClass`(적)의 `Ranger`/`Bear`를 플레이어블로 만드는 건 **별도 계획 문서**로 분리한다.

### 11.2 코스메틱 — 1차는 색상 스킨

- 슬롯: **Skin(머티리얼 오버라이드)** 1개만.
- 캐릭터별 2~3종으로 시작. 기본 외형은 항상 무료·기본 제공.
- 정황상 진행 중인 작업(`Content/DaeRuneAssets/Characters/GardenRobot_v3/FP/Gardener_Blue|BlueLight|Gray|White`,
  신규 `Content/Blueprints/Character/Material/`)이 이 방향과 맞물린다 — **FP/TP 양쪽 머티리얼 세트**로 정리해 두면 그대로 쓸 수 있다.
- 2차 범위(메시 교체/모자/트레일)는 M6 이후 판단.

---

## 12. 리스크 & 결정 필요 사항

### 결정 필요 (착수 전)

1. **캐릭터 해금을 실제로 할 것인가?**
   대기실 순환에서 캐릭터가 빠지는 것은 신규 유저 체감이 크다. "전부 기본 제공 + 코스메틱만 해금"도 유효한 선택지다.
   → 이 답에 따라 §6.1(RPC + PlayerState 복제) 작업 전체가 필요/불필요로 갈린다.

2. **세이브 슬롯을 스팀 계정으로 키잉할 것인가?** ★
   현재 `"DaeRunePlayerProgress"` 고정이라 해금·재화가 **PC 단위**다. 한 PC에서 계정을 바꾸면 서로의 진행도를 본다.
   Plan2 M6에도 같은 항목("UniqueNetId 슬롯명")이 있으므로 **두 계획이 같은 결론을 써야 한다.**
   - 안 A: 그대로 둔다 (로컬 협동 위주, 단순)
   - 안 B: `SaveSlotName + "_" + UniqueNetId` — 계정 정확성↑, **기존 세이브 이관 로직 필요**

3. **스팀 미러링 범위**: 모든 Plan2 업적을 스팀에 올릴지, 대표 업적만 올릴지.
   (스팀 도전과제는 파트너 페이지에서 하나씩 등록해야 하므로 개수가 곧 운영 비용이다.)

### 리스크

4. **AppId**: 실 발급 전까지 스팀 실동작 검증 불가. → M1~M4는 로컬만으로 완주 가능하도록 설계했다.
5. **스테이지2 도입 시 업적 키 충돌** (Plan6): 스테이지가 늘면 "페이즈 N 클리어" 류 업적이 모호해진다.
   `FDRStageRewardReport`에 **스테이지 식별자 필드가 없다**(`DRProgressionTypes.h:94-117`).
   → 업적 ID를 `Stage1_Clear` / `Stage2_Clear`처럼 스테이지 접두어로 명명하고,
   Plan2 M4 착수 시 `FDRStageRewardReport`에 `FName StageId` 추가를 함께 검토한다.
6. **enum 확장 호환성**: `EPlayerCharacterClass`는 SaveGame의 `TMap` 키로 직렬화된다.
   **중간 삽입 금지 / `Count` 직전 추가만 허용.**
7. **태그 rename 리스크**: `Unlockable.*`는 SaveGame에 저장된다. 이름 변경 시 리다이렉터 등록을 잊으면 해금이 사라진다.
8. **1차 범위를 머티리얼로 좁힌 대가**: 메시 교체형 스킨을 나중에 넣을 때 `FDRCosmeticDef`가 확장된다.
   → 확장은 **필드 추가만** 하고 기존 필드 의미를 바꾸지 않으면 DataAsset 재작업이 없다.

---

## 13. 영향받는 파일 요약

**신규**
- `Public/Game/DRUnlockTypes.h` — `FDRLoadout`, `FDRCosmeticDef`, `FDRUnlockRule`
- `Public/Game/DRUnlockData.h` / `Private/Game/DRUnlockData.cpp` (+ `DA_UnlockData` 인스턴스)
- `Public/Game/DRUnlockManager.h` / `.cpp`
- `Public/Game/DRAchievementSubsystem.h` / `.cpp`
- UI 위젯/컨트롤러 (코스메틱 선택, 해금 토스트, 선택적 업적 목록)

**수정**
- `Public/DRGameplayTags.h` / `.cpp` — `Unlockable.*`, `Cosmetic.Slot.*`
- `Public/Game/DRSaveGame.h` — v3 필드 3개
- `Public/Game/DRGameInstance.h` / `.cpp` — `UnlockManager` 소유·`Init()` 순서·v3 마이그레이션·`ApplyStageReward` 훅
- `Public/Game/DRProgressionConfig.h` — `SteamApiName` 컬럼, `UnlockData` 참조
- `Public/Player/DRPlayerState.h` / `.cpp` — `UnlockedClasses` 복제
- `Private/Player/DRPlayerController.cpp:1449` — 잠긴 인덱스 스킵, `ServerReportUnlockedClasses`
- `Public/Character/DRCharacter.h` / `.cpp` — `ActiveLoadout` 복제 + FP/TP 외형 적용
- `Private/Game/DRLobbyGameMode.cpp:147,319` / `DRStageGameMode.cpp:37` / `DRTutorialGameMode.cpp:29` — 잠금 폴백 검사
- `Config/DefaultEngine.ini` — 실 AppId
- `Config/DefaultGameplayTags.ini` — 태그 리다이렉터(rename 발생 시)

---

## 14. 다음 행동

1. **§12의 결정 3건**(캐릭터 해금 여부 / 세이브 슬롯 계정 키잉 / 스팀 미러링 범위) 확정.
   특히 **세이브 슬롯 키잉은 Plan2 M6와 함께** 결정해야 한다.
2. **Plan2 M4 착수** — 이 문서의 M2 이후가 전부 여기에 의존한다.
   그 과정에서 `FDRStageRewardReport`에 `StageId` 추가 여부를 함께 판단(§12-5).
3. 병행하여 **M1(데이터 토대)** 착수 — 백엔드·게임플레이와 무관한 순수 추가라 지금 바로 가능하다.
