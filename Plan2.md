# Plan2 — 캐릭터별 경험치/레벨 진행 시스템 (Per-Character Progression)

> 작성일: 2026-07-01
> 대상 브랜치: `feat/PlayExpo` (작업은 별도 `feat/CharacterProgression` 브랜치 권장)
> 작성 목적: 스팀 계정마다, 플레이어 캐릭터(정원로봇 / 자판기 / 청소기)마다 독립적인 경험치·레벨을 누적하고,
> 레벨업 시 해당 캐릭터의 능력치(AttributeSet)와 스킬 데미지가 상승하도록 하는 기능의 상세 구현 계획.

---

## 0. 구현 현황 (Implementation Status)

> 마지막 갱신: 2026-07-22 — **M1(데이터/저장 토대) 코드까지 구현됨.** 기존 게임플레이 경로는 무변경(순수 추가).

| 마일스톤 | 상태 | 비고 |
|---|---|---|
| **M1. 데이터/저장 토대** | ✅ **코드 완료** | 신규 타입/Config/세이브 확장/GameInstance API. 아래 상세 참조 |
| M2. 레벨→능력치/스킬 스케일 | ⬜ 미착수 | 코드(Level 파라미터화) + 콘텐츠(커브) 모두 남음 |
| M3. 서버↔클라 레벨 운반 | ⬜ 미착수 | PlayerState/PlayerController/스폰 SetLevel |
| M4. 스테이지 종료 지급 파이프라인 | ⬜ 미착수 | GameMode ClearedCount + Client_GrantStageExp |
| M5. UI | ⬜ 미착수 | 로비 레벨 표시, 결과창 연출 |
| M6. 스팀 키잉(선택) | ⬜ 미착수 | UniqueNetId 슬롯명, 클램프/로깅 |

### M1에서 실제로 구현된 것 (틀만 — 아직 게임플레이에 연결 안 됨)
- ✅ 신규 `Source/DaeRune/Public/Game/DRProgressionTypes.h` — `FDRCharacterProgress`, `FDRStageProgressResult`.
- ✅ 신규 `Source/DaeRune/Public/Game/DRProgressionConfig.h` + `Private/Game/DRProgressionConfig.cpp`
  — `UDRProgressionConfig` DataAsset. `GetXpToNextLevel()`(커브 없으면 폴백), `CalcStageXp()` 구현.
- ✅ `DRSaveGame.h/.cpp` 확장 — `TMap<EPlayerCharacterClass, FDRCharacterProgress> CharacterProgress`,
  `int32 SaveVersion` + `static constexpr int32 CurrentSaveVersion` 추가. (`.cpp` 로직 변경 없음, 기본 초기화만.)
- ✅ `DRGameInstance.h/.cpp` 확장 — `ProgressionConfig` 프로퍼티 + 진행도 API:
  `GetCharacterProgress / GetCharacterLevel / GetAllClassLevels / ApplyStageResult`,
  `LoadProgress`에서 `EnsureProgressInitialized()`(누락 클래스 키 lazy 초기화 + SaveVersion 마이그레이션 스텁) 호출.

### M1에서 아직 안 한 것 (연결/콘텐츠는 다음 마일스톤)
- ⬜ 콘텐츠: `DA_ProgressionConfig` 인스턴스 생성, `XpToNextLevelCurve` 연결, GameInstance BP에 `ProgressionConfig` 지정.
- ⬜ `ApplyStageResult`를 실제 호출하는 경로(M4에서 `Client_GrantStageExp`가 호출) — 지금은 API만 존재.
- ⬜ 레벨→능력치/스킬 반영(M2~M3) — `SetLevel`/`GivePlayerStartupAbilities(Level)` 등 스폰 경로 연결 전혀 안 됨.
- ✅ **컴파일 검증 완료**: `DaeRuneEditor` Development 빌드 통과(2026-07-22, exit 0). 신규/수정 3파일 무경고 컴파일.
- ⚠️ **런타임 검증 대기**: 세이브 라운드트립(`ApplyStageResult` → 재시작 → 레벨 유지) 런타임 테스트 미실시. API가 아직 호출되는 경로가 없어 M4 연결 후 검증 예정.

---

## 1. 기능 요약 (Requirements)

사용자가 요청한 기능을 분해하면 다음과 같다.

1. **스팀 계정 단위 저장**: 진행도(경험치/레벨)는 스팀 계정(= 해당 PC의 로컬 세이브)에 귀속된다.
2. **캐릭터(클래스) 단위 분리**: `EPlayerCharacterClass`(현재 `Gardener` 정원로봇, `VendingMachine` 자판기, `RobotVacuum` 청소기)마다
   서로 독립된 경험치/레벨을 가진다. 정원로봇만 플레이하면 정원로봇 경험치만 오르고 자판기·청소기는 그대로.
3. **경험치 지급 시점**: 하나의 스테이지가 끝났을 때 1회 지급. **게임오버/게임클리어 무관**하게 지급한다.
4. **경험치 양 = 클리어한 페이즈 수에 비례**: 그 스테이지에서 도달/클리어한 페이즈 수가 많을수록 더 많은 경험치.
5. **레벨업 → 능력치 상승**: AttributeSet 스탯(MaxHealth, MaxWater, MoveSpeed 등)과 스킬 데미지가 레벨에 따라 상승.

---

## 2. 현재 코드베이스 분석 (이 기능과 직결되는 사실들)

설계의 근거가 되는 현재 구현 상태. (파일/라인은 작성 시점 기준)

### 2.1 레벨 → 능력치 스케일링은 "이미" 존재한다
- `ADRCharacterBase`는 `int32 Level = 1` 멤버를 가지며 `SetLevel(int32)` 제공.
  (`Source/DaeRune/Public/Character/DRCharacterBase.h:34, 90`)
- 플레이어 캐릭터 `ADRCharacter::InitializeDefaultAttributes()`가
  `UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(this, PlayerCharacterClass, Level, ASC)`를 호출.
  (`Source/DaeRune/Private/Character/DRCharacter.cpp:768-772`)
- `InitializePlayerDefaultAttributes`는 클래스별 `PrimaryAttributes`/`VitalAttributes` GE를
  `CreateAndApplyEffectSpec(ASC, GE, Avatar, Level)`로 **해당 Level에 적용**한다.
  (`Source/DaeRune/Private/AbilitySystem/DRAbilitySystemLibrary.cpp:120-156`)
- 즉, PrimaryAttributes/VitalAttributes GE의 Modifier가 **ScalableFloat + CurveTable(레벨 키)**로 구성되어 있으면
  `Level` 값만 바꿔서 재적용하면 능력치가 자동으로 스케일된다.
  → **전제 확인 필요**: 해당 GE들이 실제로 레벨 커브를 사용하는지 점검. (7.9 항목 참조)

### 2.2 스킬 데미지는 "아직" 레벨 스케일이 안 된다 (반드시 손봐야 함)
- 데미지 어빌리티는 `Damage.GetValueAtLevel(GetAbilityLevel())`로 데미지를 계산한다.
  (`Source/DaeRune/Private/AbilitySystem/Abilities/DRDamageGameplayAbility.cpp:12, 24, 47`)
  여기서 `Damage`는 `FScalableFloat`(커브테이블 가능).
- 그런데 플레이어 어빌리티 부여 경로
  `GivePlayerStartupAbilities` → `UDRAbilitySystemComponent::AddCharacterAbilities`는
  **어빌리티 레벨을 `1`로 하드코딩**한다.
  (`Source/DaeRune/Private/AbilitySystem/DRAbilitySystemComponent.cpp:41, 62`:
  `FGameplayAbilitySpec(AbilityClass, 1)`)
- 따라서 `GetAbilityLevel()`은 항상 1 → 스킬 데미지가 레벨로 안 오른다.
  → **AddCharacterAbilities / GivePlayerStartupAbilities에 Level 파라미터를 추가**해야 한다. (7.7 항목)

### 2.3 저장 시스템은 로컬 단일 슬롯, 진행도 거의 없음
- `UDRSaveGame`은 현재 `bHasCompletedTutorial` 하나만 가진다.
  (`Source/DaeRune/Public/Game/DRSaveGame.h`)
- `SaveSlotName` 고정, `UserIndex` 고정 단일 슬롯.
- `UDRGameInstance`가 `LoadProgress()/SaveProgress()`로 관리하며 `Init()`에서 로드.
  (`Source/DaeRune/Private/Game/DRGameInstance.cpp:13-118`)
- 스팀: `OnlineSubsystemSteam` 활성화됨. 한 PC에는 보통 한 스팀 유저가 로그인되어 있으므로
  **"로컬 세이브 = 그 PC의 스팀 계정 진행도"**가 자연스럽게 성립.
  (필요 시 세이브 슬롯명을 스팀 UniqueNetId로 키잉하여 한 PC 다계정도 지원 가능 — 6.3 참조)

### 2.4 클래스 선택 / 플레이어 식별
- 선택 클래스는 `ADRPlayerState::SelectedPlayerClass`(복제, RepNotify). (`DRPlayerState.h:118-120`)
- `EPlayerCharacterClass { Gardener, VendingMachine, RobotVacuum, Count UMETA(Hidden) }` (`CharacterClassInfo.h:29-36`).
  - **주의**: 현재 플레이 가능 클래스는 **3개**(정원로봇/자판기/청소기). 마지막 `Count`는 개수 계산용 센티넬(Hidden)이며 실제 클래스가 아니다.
  - 배열/맵 크기는 하드코딩하지 말고 `(int32)EPlayerCharacterClass::Count`로 산정하면 이후 클래스 추가에 자동 대응된다(10장 엣지케이스 #6이 이 센티넬로 자연 해결됨).
- GameMode가 스폰 시 선택 클래스에 맞는 BP 폰을 고른다.
  (`DRStageGameMode::GetDefaultPawnClassForController_Implementation`, `DRStageGameMode.cpp:24-55`)

### 2.5 스테이지 종료 흐름 (경험치 지급을 끼워넣을 위치)
- 서버 권한 `ADRStageGameMode::TriggerGameOver()` / `TriggerGameClear()`
  → `NotifyAllPlayersGameEnd(bIsGameClear)`
  → `WipeoutDelayTime` 타이머 후 `ReturnToLobby()`(`ServerTravel`).
  (`DRStageGameMode.cpp:57-219`)
- 도달 페이즈는 `ADRStageGameState::CurrentPhaseIndex`(0-base, 복제)로 알 수 있다.
  (`DRStageGameState.h:247-248`, `DRStageGameMode.cpp` 페이즈 전환부)

### 2.6 핵심 아키텍처 난점 (가장 중요)
- **권한 분리 문제**: "몇 페이즈까지 깼는가"는 **서버만** 안다.
  하지만 "각 플레이어의 경험치/레벨"은 **각 플레이어 본인의 클라이언트 머신 로컬 세이브**에 있다.
- 따라서 진행도는 **클라이언트 로컬에서 권위적으로 관리**하고,
  서버는 "이번 스테이지에서 너는 N 페이즈 클리어 → XP M 획득"이라는 **결과만 각 클라이언트에 통지**해야 한다.
- 마찬가지로 스폰 시점에 캐릭터 `Level`을 정하려면, 서버가 각 플레이어의 **저장된 레벨**을 알아야 한다
  → 클라이언트가 접속/로비 단계에서 자기 레벨을 서버로 올려 PlayerState에 싣는다 (Server RPC).

---

## 3. 설계 개요 (High-Level Design)

```
[클라이언트 로컬 세이브 (스팀 계정별, 권위적 진행도)]
        UDRSaveGame
        └─ TMap<EPlayerCharacterClass, FDRCharacterProgress>   // 클래스별 {Level, CurrentXP} (3클래스)
                │  (로비 진입/접속 시)
                ▼  ClientReportLevels → ServerRPC
[서버: ADRPlayerState]
        └─ ReportedClassLevels: TArray<int32>  // 스폰/스케일에 사용
                │  (캐릭터 스폰 시 SetLevel(저장레벨) → 능력치/스킬 GE를 그 레벨로 적용)
                ▼
[플레이 → 스테이지 종료 (서버)]
        ADRStageGameMode::TriggerGameOver/Clear
        └─ 도달 페이즈 수 계산 → 각 PC에 Client_GrantStageExp(클래스, 페이즈수, 클리어여부) RPC
                │
                ▼
[클라이언트: 로컬 세이브에 XP 가산 → 레벨업 판정 → 저장]
        UDRGameInstance::ApplyStageResult(...)
        └─ 레벨업 시 다음 로비/스테이지 진입부터 새 레벨로 스폰
```

설계 원칙:
- **진행도(XP/Level)의 단일 진실 원천(SSOT)은 각 클라이언트의 로컬 세이브.** 서버는 "지급 이벤트"만 발생.
- **PvE 협동**이므로 클라이언트 신고 레벨을 신뢰한다(치트 가능하나 영향은 자기 캐릭터 강화뿐). 6.4에서 위험/완화책 명시.
- 능력치/스킬 스케일은 **기존 Level 스케일링 메커니즘을 재사용** — 신규 스탯 적용 로직을 만들지 않는다.

---

## 4. 데이터 모델

### 4.1 캐릭터 진행도 구조체
신규: `Source/DaeRune/Public/Game/DRProgressionTypes.h`
```cpp
USTRUCT(BlueprintType)
struct FDRCharacterProgress
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 Level = 1;

    // 현재 레벨에서 누적된 경험치 (다음 레벨까지의 진행)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CurrentXP = 0;

    // (선택) 통계용 — 총 누적 경험치
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 TotalXP = 0;
};
```

### 4.2 세이브 확장
`UDRSaveGame`에 추가 (`DRSaveGame.h`):
```cpp
UPROPERTY(VisibleAnywhere, Category = "Progress")
TMap<EPlayerCharacterClass, FDRCharacterProgress> CharacterProgress;

// 세이브 포맷 버전 (마이그레이션 대비)
UPROPERTY(VisibleAnywhere, Category = "Progress")
int32 SaveVersion = 1;
```
- 기존 세이브 호환: 맵에 키가 없으면 `{Level=1, XP=0}`로 간주(로드 시 lazy 초기화).
- `SaveVersion`은 추후 XP 곡선/스탯 재조정 시 마이그레이션 분기점으로 사용.
- `DRSaveGame.h`는 이미 `CharacterClassInfo.h`의 `EPlayerCharacterClass`를 include 해야 함(헤더 의존성 추가).

### 4.3 레벨/경험치 곡선 데이터 (디자이너 튜닝 가능하게)
신규 DataAsset: `UDRProgressionConfig` (`Source/DaeRune/Public/Game/DRProgressionConfig.h`)
```cpp
UCLASS()
class DAERUNE_API UDRProgressionConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    // 최대 레벨
    UPROPERTY(EditDefaultsOnly) int32 MaxLevel = 20;

    // 레벨 L → L+1 에 필요한 경험치. CurveFloat(가로축 Level) 사용 권장.
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> XpToNextLevelCurve;

    // === 스테이지 종료 경험치 지급 규칙 (페이즈 수 기반) ===
    UPROPERTY(EditDefaultsOnly) int32 BaseStageXp = 10;        // 참가 기본 보상
    UPROPERTY(EditDefaultsOnly) int32 XpPerClearedPhase = 50;  // 클리어 페이즈 1개당
    UPROPERTY(EditDefaultsOnly) int32 FullClearBonusXp = 100;  // 전 페이즈 완수 보너스

    // 곡선 미설정 시 폴백: 다음 레벨 필요 XP = 선형 기본값
    UPROPERTY(EditDefaultsOnly) int32 FallbackXpPerLevel = 100;

    int32 GetXpToNextLevel(int32 CurrentLevel) const;          // 커브 없으면 폴백
    int32 CalcStageXp(int32 ClearedPhaseCount, bool bGameClear) const;
};
```
- 경험치 계산이 필요한 곳:
  1. **클라이언트**(레벨업 판정, GameInstance) — 메인메뉴/로비 등 서버 없는 곳에서도 필요.
  2. (선택) 서버 — 지급량까지 서버가 계산하려면 서버도 참조. **권장은 클라 권위(서버는 페이즈 수만 전달)**.
- 단순화를 위해 `ProgressionConfig`는 **`UDRGameInstance`에 보유**(서버/클라 공통 접근). 서버 계산이 필요하면 GameMode가 GameInstance 경유로 가져온다.

---

## 5. 경험치 지급 규칙 (구체값 예시)

> 모두 `UDRProgressionConfig`로 튜닝 가능. 아래는 초기 권장값.

스테이지 종료 시 한 플레이어가 받는 XP (`CalcStageXp`):
```
ClearedPhaseCount = (도달/완료한 페이즈 수)
XP = BaseStageXp(10)
   + XpPerClearedPhase(50) * ClearedPhaseCount
   + (게임클리어면 FullClearBonusXp(100) 추가)
```

"클리어한 페이즈 수" 정의 (게임오버/클리어 공통):
- 게임 진행은 `CurrentPhaseIndex`(0-base). Phase1=index0, Phase2=1, Phase3=2.
- **게임클리어**: 모든 페이즈 완수 → `ClearedPhaseCount = 총 페이즈 수`(`PhaseClasses.Num()`, 예: 3).
- **게임오버**: 진행 중이던 페이즈는 "미완"으로 보고, **그 이전까지 완료한 페이즈 수** = `CurrentPhaseIndex`.
  (예: Phase2 진행 중 전멸 → index 1 → 1페이즈 클리어 인정.)
  - 옵션: 진행 중 페이즈 부분 점수도 가능하나, 1차 구현은 "완료 페이즈만 인정"으로 단순화.

서버 → 클라 전달 페이로드: `{ uint8 PlayedClass, int32 ClearedPhaseCount, bool bGameClear }`
- **XP 환산은 클라이언트에서** `ProgressionConfig`로 수행(클라 권위 일관성). 서버는 페이즈 수/클리어 여부만 전달.

어떤 클래스에 지급? — 그 스테이지에서 **실제로 플레이한 클래스** = `PlayerState->GetSelectedPlayerClass()`.
- 1차 구현은 **스테이지 도중 클래스 변경 불가**를 전제로 한다.

---

## 6. 멀티플레이어 / 저장 위치 설계

### 6.1 레벨 정보의 서버 반영 (스폰 전에 필요)
1. 클라이언트가 게임/로비 접속 후 자기 로컬 세이브에서 클래스별 레벨을 읽음.
2. `ADRPlayerController::ServerReportCharacterLevels(const TArray<int32>&)`(Server, Reliable)로 서버에 전달.
3. 서버는 `ADRPlayerState`의 `ReportedClassLevels`(복제 배열)에 저장.
4. 캐릭터 스폰/`InitAbilityActorInfo` 시 `SetLevel(선택클래스 레벨)` 적용.

PlayerState 추가:
```cpp
// 클래스별 저장 레벨 (클라가 보고, 스폰 시 사용). index = (int)EPlayerCharacterClass
UPROPERTY(Replicated) TArray<int32> ReportedClassLevels;
int32 GetReportedLevel(EPlayerCharacterClass C) const; // 범위 밖이면 1 반환
```
- TMap 복제 회피 위해 enum 개수만큼 고정 크기 배열 사용 — 단순/안전.
- 크기는 `(int32)EPlayerCharacterClass::Count`(현재 3: 정원로봇/자판기/청소기)로 초기화(예: `ReportedClassLevels.Init(1, (int32)EPlayerCharacterClass::Count)`). 하드코딩 금지.
- `GetReportedLevel`은 `(int32)C`가 `[0, Count)` 범위이고 배열 크기 내일 때만 값 반환, 아니면 1.

### 6.2 스폰 시 레벨 적용 지점
- `ADRCharacter::InitAbilityActorInfo()`에서 이미 `PlayerCharacterClass = PS->GetSelectedPlayerClass()` 수행.
  (`DRCharacter.cpp:818-819`) 그 직후 서버 권한일 때:
  ```cpp
  if (HasAuthority())
      SetLevel(DRPlayerState->GetReportedLevel(PlayerCharacterClass));
  ```
- 그 뒤 기존 `InitializeDefaultAttributes()`(Level 사용)와 `GivePlayerStartupAbilities(..., Level)`가 새 레벨로 동작.
- 주의: `Level`은 현재 복제 안 됨. 능력치는 GE로 복제되어 외형 문제는 없으나,
  클라 UI에 레벨 텍스트가 필요하면 PlayerState의 `ReportedClassLevels`를 UI 소스로 사용(별도 복제 불필요).

### 6.3 스팀 계정 키잉 (선택적 강화)
- 1차: 현재처럼 머신당 단일 로컬 슬롯(그 PC 스팀 계정 진행도)로 충분.
- 강화: 세이브 슬롯명을 스팀 UniqueNetId로 구성 →
  `SlotName = FString::Printf(TEXT("Progress_%s"), *UniqueNetIdString)`.
  - `UGameInstance::GetFirstGamePlayer()->GetPreferredUniqueNetId()` 또는 OnlineSubsystem identity에서 획득.
  - Steam 미연결(에디터/오프라인) 폴백: 기존 고정 슬롯명.

### 6.4 신뢰/치트 고려
- 클라가 자기 레벨을 신고 → 위조 가능. **PvE 협동이라 영향은 자기 캐릭터 강화에 한정**(타 플레이어 피해 없음).
- 완화책(선택): 서버가 `ProgressionConfig.MaxLevel`로 상한 클램프. 비정상 값 로깅.
- 진짜 서버 권위가 필요하면 차후 온라인 백엔드/스팀 클라우드로 이전(향후 확장 13장).

---

## 7. C++ 구현 상세 (파일별)

### 7.1 신규: `DRProgressionTypes.h`
- `FDRCharacterProgress` 구조체(4.1), 결과 UI용 `FDRStageProgressResult` 구조체(7.4).

### 7.2 신규: `DRProgressionConfig.h/.cpp`
- `UDRProgressionConfig` DataAsset(4.3) + `GetXpToNextLevel()`, `CalcStageXp()` 구현(커브 없으면 폴백).
- 콘텐츠에 `DA_ProgressionConfig` 인스턴스 생성, `XpToNextLevelCurve`에 `Curve_*` 연결.

### 7.3 `DRSaveGame.h/.cpp`
- `CharacterProgress` 맵, `SaveVersion` 추가(4.2). `EPlayerCharacterClass` 헤더 의존성 추가.

### 7.4 `DRGameInstance.h/.cpp` — 진행도 API (클라 권위)
```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression")
TObjectPtr<UDRProgressionConfig> ProgressionConfig;

FDRCharacterProgress GetCharacterProgress(EPlayerCharacterClass C) const;
int32 GetCharacterLevel(EPlayerCharacterClass C) const;

// 스테이지 결과 반영 (다중 레벨업 포함), 저장까지 수행. 반환은 결과 UI용 요약.
FDRStageProgressResult ApplyStageResult(EPlayerCharacterClass C, int32 ClearedPhaseCount, bool bGameClear);

// 모든 클래스 레벨을 배열로 (PlayerController가 서버 보고용). index=(int)enum
TArray<int32> GetAllClassLevels() const;
```
- `ApplyStageResult`: `XpGained = ProgressionConfig->CalcStageXp(...)` → `CurrentXP += XpGained` →
  `while (Level < MaxLevel && CurrentXP >= GetXpToNextLevel(Level)) { CurrentXP -= ...; Level++; }` → `SaveProgress()`.
- `LoadProgress()`에서 `SaveVersion` 확인 후 필요한 마이그레이션 / 누락 클래스 키 lazy 초기화.

`FDRStageProgressResult`: `{int32 XpGained; int32 StartLevel; int32 EndLevel; int32 XpIntoCurrent; int32 XpForNext; bool bLeveledUp;}`

### 7.5 `DRPlayerState.h/.cpp` — 레벨 운반
- `TArray<int32> ReportedClassLevels;`(Replicated, 크기 = enum 개수) + `GetReportedLevel()`.
- `GetLifetimeReplicatedProps`에 등록, `CopyProperties`(Seamless Travel)에서 복사(로비↔스테이지 보존).
  (`DRPlayerState.cpp`의 기존 `CopyProperties`에 한 줄 추가.)

### 7.6 `DRPlayerController.h/.cpp` — 보고 / 지급 RPC
```cpp
UFUNCTION(Server, Reliable) void ServerReportCharacterLevels(const TArray<int32>& Levels);
UFUNCTION(Client, Reliable) void Client_GrantStageExp(uint8 PlayedClass, int32 ClearedPhaseCount, bool bGameClear);
```
- 보고 타이밍: 로컬 컨트롤러에서 `OnRep_PlayerState`/`AcknowledgePossession`/`BeginPlayingState` 중 1회.
  `GameInstance->GetAllClassLevels()` → `ServerReportCharacterLevels()`.
- `Client_GrantStageExp`: 로컬에서 `GameInstance->ApplyStageResult(...)` → 결과로 레벨업 결과 UI(9.3).
  - 호출 후 갱신된 레벨은 다음 로비 진입 시 재보고되므로 다음 스테이지에 자동 반영.

### 7.7 `DRAbilitySystemComponent.*` / `DRAbilitySystemLibrary.*` — 스킬 레벨 스케일
- `AddCharacterAbilities`에 `int32 Level = 1` 파라미터 추가, `FGameplayAbilitySpec(AbilityClass, Level)`로 부여.
  (현재 `:41, :62`의 하드코딩 `1` 대체)
- `GivePlayerStartupAbilities(WorldContextObject, ASC, PlayerClass, int32 Level)`로 시그니처 확장 →
  내부 `DRASC->AddCharacterAbilities(AllAbilities, Level)`.
- 호출부 `ADRCharacter::PossessedBy`(`DRCharacter.cpp:141`)에서 `Level` 전달.
- 효과: `GetAbilityLevel()`이 캐릭터 레벨 반환 → `Damage.GetValueAtLevel(...)`로 스킬 데미지 스케일.
  - **전제**: 각 데미지 어빌리티의 `Damage`(FScalableFloat)에 레벨 커브 연결 필요(7.9).

### 7.8 `DRStageGameMode.cpp` — 스테이지 종료 시 지급
- `NotifyAllPlayersGameEnd()` 루프를 확장(기존 단일 순회 재사용):
  ```cpp
  const int32 TotalPhases = PhaseClasses.Num();
  const int32 ClearedCount = bIsGameClear ? TotalPhases
                                          : FMath::Max(0, CachedGameState->GetCurrentPhaseIndex());
  for (PC : controllers) {
      EPlayerCharacterClass PlayedClass = PS->GetSelectedPlayerClass();
      PC->Client_GrantStageExp((uint8)PlayedClass, ClearedCount, bIsGameClear);
      // 기존 UI/오디오 처리 ...
  }
  ```
- 주의: 곧이어 `ReturnToLobby`의 `ServerTravel`이 발생. Reliable RPC는 트래블 전 큐잉되며,
  현재 `WipeoutDelayTime`(5초) 지연이 있어 전송 보장됨. 그래도 RPC는 트래블 직전에 보낸다.
- `PhaseClasses`는 현재 private — `TotalPhases` 산정을 위해 getter 추가 또는 GameMode 내부에서 직접 사용.

### 7.9 콘텐츠/에셋 작업 (커브 연결) — 코드 외 필수 작업
1. **PrimaryAttributes/VitalAttributes GE**(클래스별, `UPlayerCharacterClassInfo` 연결 GE)의
   Modifier Magnitude를 `ScalableFloat`로 두고 **CurveTable(레벨 키)**에 연결.
   - 정원로봇/자판기 각각 MaxHealth/MaxWater/MoveSpeed 성장 곡선 정의.
   - 이미 커브 사용 중이면 곡선 범위를 `MaxLevel`까지 확장만 하면 됨. (2.1 "전제 확인")
2. **데미지 어빌리티의 `Damage` FScalableFloat**에 레벨 커브 연결(스킬별 성장).
3. `DA_ProgressionConfig` 작성 및 `XpToNextLevelCurve` 연결.
4. GameInstance BP에 `ProgressionConfig` 지정.

> ⚠️ 검증 포인트: PrimaryAttributes GE가 레벨 커브를 쓰는지 먼저 확인.
> 안 쓰면 "레벨만 올려도 능력치 불변" → 이 콘텐츠 작업이 본 기능의 실제 체감 핵심이다.

---

## 8. 데이터 흐름 (시퀀스 정리)

### 8.1 접속 → 레벨 서버 반영
```
Client GameInstance.Init() → LoadProgress() (로컬 세이브 로드)
Client PC (로비/스테이지 Possess) → GetAllClassLevels() → ServerReportCharacterLevels()
Server PlayerState.ReportedClassLevels 채움 (복제)
```

### 8.2 스테이지 스폰 → 능력치/스킬 레벨 적용
```
Server GameMode 스폰 → ADRCharacter.InitAbilityActorInfo()
   PlayerCharacterClass = PS.SelectedPlayerClass
   SetLevel(PS.GetReportedLevel(class))
   InitializeDefaultAttributes()           // PrimaryAttributes/Vital GE @ Level
   GivePlayerStartupAbilities(... Level)    // 어빌리티 spec @ Level → 스킬 데미지 스케일
```

### 8.3 스테이지 종료 → XP 지급 → 저장 → 레벨업
```
Server TriggerGameOver/Clear
   ClearedCount 계산
   각 PC: Client_GrantStageExp(class, ClearedCount, bClear)
Client GameInstance.ApplyStageResult(...)   // XP 가산 + 레벨업 루프 + SaveProgress()
Client 결과/레벨업 UI 표시
Server → ReturnToLobby (ServerTravel)
다음 스폰부터 새 레벨 반영 (8.1 재보고)
```

---

## 9. UI 작업

### 9.1 로비/대기실 — 캐릭터별 레벨 표시
- `DRWaitingRoomWidget` 등에서 현재 선택 클래스의 `GameInstance.GetCharacterLevel(class)` 표시.
- 클래스 토글 시 해당 클래스 레벨/XP 바 갱신.

### 9.2 인게임 HUD (선택)
- 캐릭터 레벨 표시가 필요하면 `OverlayWidgetController`에 레벨 델리게이트 추가하거나
  PlayerState.ReportedLevel을 직접 바인딩.

### 9.3 스테이지 종료 결과 화면 — 경험치/레벨업 연출
- 기존 `Client_ShowGameOverUI` / `Client_ShowGameClearUI`(`DRStageGameMode.cpp:208-212`)와 연동.
- `Client_GrantStageExp` 처리 결과(`FDRStageProgressResult`)를 결과창에 전달:
  - 획득 XP, XP 바 채우기 애니메이션, 레벨업 시 "LEVEL UP!" 강조.
- 순서 주의: `ApplyStageResult`를 먼저 수행해 XP 적용을 끝낸 뒤 그 결과로 UI 갱신.

---

## 10. 엣지 케이스 & 결정 사항

1. **최대 레벨**: `MaxLevel` 도달 시 레벨업/스탯 상승 중단(XP는 캡 또는 누적만). 결과창 "MAX" 표기.
2. **0페이즈 게임오버**(Phase1 즉시 전멸): `ClearedCount = 0` → `BaseStageXp`만 지급. 0 정책이면 0.
3. **호스트(리슨 서버) 플레이어**: 호스트도 클라 컨트롤러를 가지므로 `Client_GrantStageExp`가 동일 동작(로컬 GameInstance 저장). 검증 필요.
4. **중도 접속/이탈**: Join-in-progress 차단됨(`BlockJoinInProgress`). 종료 시점 존재 PC에게만 지급.
5. **세이브 손상/마이그레이션**: `SaveVersion` 불일치 시 안전 초기화/변환. 로드 실패 시 기본 진행도.
6. **클래스 enum 추가 시**: `ReportedClassLevels` 크기/세이브 맵 키를 `EPlayerCharacterClass::Count` 기반으로 자동 확장(하드코딩 금지). 센티넬이 이미 존재하므로 새 클래스는 열거자만 추가하면 됨.
7. **스테이지 도중 클래스 변경 금지 전제**: 변경 허용 시 5장 "지급 대상 클래스" 규칙 재설계.
8. **튜토리얼/메인메뉴**: 진행도 지급 없음. 튜토리얼 게임모드는 XP 흐름에서 제외.

---

## 11. 구현 단계 (마일스톤 / 권장 순서)

> 각 단계는 독립 빌드/테스트 가능하도록 분리.

- **M1. 데이터/저장 토대**
  - `DRProgressionTypes.h`, `DRProgressionConfig.h/.cpp`, `DRSaveGame` 확장.
  - `DRGameInstance`에 `ProgressionConfig` + `Get/ApplyStageResult/GetAllClassLevels` 구현.
  - 검증: 임시 호출로 `ApplyStageResult` → 세이브 파일에 레벨/XP 반영 확인.

- **M2. 레벨 → 능력치/스킬 스케일 (콘텐츠 + 코드)**
  - `AddCharacterAbilities`/`GivePlayerStartupAbilities` Level 파라미터화(7.7).
  - PrimaryAttributes/Vital GE 및 데미지 어빌리티 커브 연결(7.9).
  - 검증: `Level`을 임시로 1/5/10 강제 세팅 → 체력/물/스킬 데미지 변화 확인.

- **M3. 서버↔클라 레벨 운반**
  - PlayerState `ReportedClassLevels` + 복제 + CopyProperties.
  - PlayerController `ServerReportCharacterLevels`.
  - 스폰 시 `SetLevel(ReportedLevel)` 적용(7.5, 6.2).
  - 검증: 세이브 레벨 변경 후 스테이지 진입 → 그 레벨 능력치로 스폰(멀티 인스턴스).

- **M4. 스테이지 종료 지급 파이프라인**
  - GameMode `ClearedCount` 계산 + `Client_GrantStageExp`(7.8, 7.6).
  - 클라 수신 → `ApplyStageResult` → 저장.
  - 검증: 게임오버/클리어 각각 도달 페이즈별 지급량/레벨업 정확성. 플레이한 클래스만 오르고 나머지 2클래스 불변 확인.

- **M5. UI**
  - 로비 레벨 표시(9.1), 결과창 XP/레벨업 연출(9.3).

- **M6. 스팀 키잉(선택) + 마무리**
  - UniqueNetId 슬롯명(6.3), 클램프/로깅(6.4), 마이그레이션(10.5).

---

## 12. 테스트 계획

### 12.1 단일/로컬
- 세이브 라운드트립: 지급 → 재시작 → 레벨 유지.
- 클래스 독립성: 한 클래스(예: 청소기) 플레이 후 나머지 2클래스(정원로봇/자판기) 레벨 0 변화 확인.
- 레벨업 경계: XP가 정확히 임계치/초과 시 다중 레벨업 처리.

### 12.2 멀티플레이어 (PIE 2+ 인스턴스, 리슨 서버)
- 각 클라이언트가 자기 로컬 세이브에만 기록(타 클라 세이브 미오염).
- 호스트 플레이어도 지급/저장 정상.
- 서로 다른 레벨의 두 플레이어가 각자 레벨에 맞는 능력치로 스폰.
- 게임오버/클리어 시 모든 클라가 1회씩만 지급(중복 없음).

### 12.3 회귀
- 기존 튜토리얼 완료 플래그 저장/로드 유지.
- 능력치 GE 커브 변경이 적/사이트 등 비플레이어 경로에 영향 없는지(플레이어 전용 GE만 수정).

---

## 13. 향후 확장 (Out of Scope, 메모)
- 스탯 포인트 수동 분배 / 스킬 트리.
- 서버 권위 진행도(온라인 백엔드, 스팀 클라우드 동기화)로 치트 방지.
- 시즌/프레스티지, 레벨별 해금 콘텐츠.
- 경험치 부스트 아이템/이벤트 배수.

---

## 14. 변경 파일 요약 체크리스트

신규:
- [x] `Source/DaeRune/Public/Game/DRProgressionTypes.h` — **M1 완료**
- [x] `Source/DaeRune/Public/Game/DRProgressionConfig.h` + `Private/Game/DRProgressionConfig.cpp` — **M1 완료**
- [ ] 콘텐츠: `DA_ProgressionConfig`, XP 커브, 능력치/스킬 레벨 커브 — (M2/콘텐츠)

수정:
- [x] `DRSaveGame.h/.cpp` — CharacterProgress 맵, SaveVersion — **M1 완료**
- [x] `DRGameInstance.h/.cpp` — ProgressionConfig, 진행도 API, 마이그레이션 스텁 — **M1 완료**
- [ ] `DRPlayerState.h/.cpp` — ReportedClassLevels(복제) + CopyProperties — (M3)
- [ ] `DRPlayerController.h/.cpp` — ServerReportCharacterLevels, Client_GrantStageExp — (M3/M4)
- [ ] `DRCharacter.cpp` — 스폰 시 SetLevel(ReportedLevel), GivePlayerStartupAbilities에 Level 전달 — (M2/M3)
- [ ] `DRAbilitySystemComponent.h/.cpp` — AddCharacterAbilities(Level) — (M2)
- [ ] `DRAbilitySystemLibrary.h/.cpp` — GivePlayerStartupAbilities(Level) — (M2)
- [ ] `DRStageGameMode.cpp` — ClearedCount 계산 + Client_GrantStageExp 전송 (+ PhaseClasses 접근) — (M4)
- [ ] UI 위젯들 — 로비 레벨 표시, 결과창 XP/레벨업 — (M5)
