# Plan.md — 로비 옷장(코스메틱) 시스템 + 스팀 도전과제 연동 해금

> **작성일**: 2026-07-01
> **1차 전면 개정**: 2026-07-27
> **2차 전면 개정: 2026-08-25** — ① 요구사항이 **"로비의 옷장에서 옷을 골라 입고 그대로 플레이"** 로 구체화됨.
> ② 1차 개정이 실측했던 코드가 **Plan2 3차 개정(슬롯+칩)** 으로 전부 교체되어 §1 표가 통째로 무효.
> ③ **Plan2 M4가 완료**되어 1차 개정의 최대 선행 조건이 해소됨. 무효화된 전제는 §0.2 참조.
> ④ **결정 4건 확정** — 캐릭터 해금 제외 / **세이브 슬롯 스팀 계정 키잉** / 스팀 등록은 M6으로 지연 /
> **옷장 UI 안 실시간 3D 프리뷰**. 상세는 §0.1, 설계는 §4.6·§5.8.
> **⑤ 아트 시안 반영 (2026-08-25) — §15 신설.** PNG 12종 실측 기반 `WBP_Wardrobe` 제작 사양
> (위젯 트리·1080p 좌표·텍스처 설정·PS 블렌드 모드 변환). **시안의 4개 카테고리가 데이터 모델을
> 바꾼다 — §15.11 이 §4.2·§4.4·§5.1·§5.2·§5.4 를 개정한다.**
> **대상**: DaeRune (UE 5.5, GAS, OnlineSubsystemSteam) / 브랜치 `feat/PlayExpo`
> **선행 문서**: Plan2 §4~7(세이브·재화·칩·보고 파이프라인), Plan4(현지화), Plan5 §1(FP·TP 메시 분리), Plan6(스테이지2)
> **목적**: 스팀 도전과제 달성 → 로비 **옷장**에서 코스메틱(스킨) 해금·장착 → 스테이지까지 그 외형 유지

---

## 0. 한눈에 보기 (TL;DR)

**요구사항 해석**

| 요구 | 구현 대응 |
|---|---|
| "로비에 옷장이 있다" | `ADRWardrobe` 신규 액터. **`ADRUpgradeStation`의 완전한 복제판** — 로컬 오버랩 + `OnInteractPressed` + 로컬 UI (§5.5) |
| "옷을 선택해서 입는다" | 로컬 세이브 `EquippedSkins[클래스]` 에 기록. 로컬 UI라 서버 왕복 없음 (§5.6) |
| **"UI 창 안 3D 메시에 실시간으로 입혀진다"** | `ADRCosmeticPreviewStage` — 맵 밖 촬영 스튜디오 + `SceneCapture2D` → RenderTarget → UMG. **로컬 전용이라 왕복 0** (§5.8) |
| "그대로 입은 채로 플레이" | `ADRPlayerState::EquippedSkinId` 복제 + `CopyProperties` 보존 — **칩(`EquippedChips`)과 완전히 같은 경로** (§6.1) |
| "스팀 도전과제로 열린다" | `FDRAchievementDef` 에 `SteamApiName` 1컬럼 추가 + `UDRAchievementSubsystem` 미러링 (§5.7, §8) |
| (결정 2) 계정별 진행도 분리 | 세이브 슬롯을 `DaeRunePlayerProgress_<SteamId>` 로 키잉 + 레거시 1회 이관 (§4.6) |

**설계 5원칙**

1. **새 판정 로직을 만들지 않는다.** 업적 판정(`ADRStageGameMode::GrantStageAchievement`)과 지급 파이프라인
   (`Client_GrantStageReward` → `ApplyStageReward`)은 **이미 전 구간 완성**돼 있다. 이 문서는 그 위에 얹기만 한다.
2. **새 패턴을 만들지 않는다.** 옷장 액터는 `ADRUpgradeStation`, 로드아웃 동기화는 `EquippedChips`,
   화면 위젯은 `UDRUpgradeScreenWidget` 을 **구조째 복사**한다. 검증된 경로가 이미 있는데 두 번째 방식을 만들면 관리 비용만 는다.
3. **식별자는 전부 `FName`.** `ChipId` / `AchievementId` / `RewardId` 가 이미 FName이다. 코스메틱만 GameplayTag를
   쓰면 저장 데이터에 rename 리스크를 새로 들여오게 된다 (§4.1). — *1차 개정의 `Unlockable.*` 태그 안은 폐기.*
4. **해금 상태를 저장하지 않는다.** "달성 업적 목록"만 저장하고 해금 여부는 **매번 파생**한다.
   파생값을 저장하면 규칙 변경 시 두 값이 어긋난다 (§4.5).
5. **스팀 없이도 전 기능이 로컬로 동작한다.** 개발 중에는 이쪽이 기본 경로다 (§1.8).
   세이브 슬롯도 `SteamID → LastAccountKey → 레거시` 폴백 체인을 둬서, 스팀이 꺼진 채 실행돼도
   **같은 진행도를 잇는다** (§4.6.2).

---

## 0.1 이번 요구사항에서 새로 확정된 것

- 옷장이 놓이는 곳은 로비의 **FreeRoam 구간**이다. 대기실(WaitingRoom)에는 **플레이어 폰 자체가 없어서**
  걸어가서 상호작용하는 장치를 놓을 수 없다 (§1.4). → 업그레이드 장치 옆이 자연스럽다.
- 따라서 흐름은 **대기실(로봇 선택) → PowerOn → FreeRoam(옷장·업그레이드·스테이지 선택) → 스테이지** 다.
  옷장은 **이미 고른 로봇의 옷**을 바꾼다. 업그레이드 화면이 `GetViewedClass()` 로 같은 판단을 한다 (`DRUpgradeScreenWidget.h:34-35`).

### 결정 완료 (2026-08-25) ★

| # | 항목 | 결정 | 반영 절 |
|---|---|---|---|
| 1 | 캐릭터(로봇) 해금 | **이번 범위에서 제외.** 요구는 "옷"이고, 캐릭터 게이팅은 대기실 순환 + 복원 경로 3곳을 건드리는 별개 작업이다. **최종 확정은 아니므로** 필요 작업 목록은 §11.3에 보존한다 | §11.3 |
| 2 | 세이브 슬롯 계정 키잉 | **한다.** 슬롯명을 `DaeRunePlayerProgress_<SteamId>` 로 키잉하고 기존 세이브를 1회 이관한다. **Plan2 도 같은 결론을 써야 한다** | **§4.6 (신설)** |
| 3 | 스팀 미러링 범위 | **개수는 제약이 아니다.** 게임 쪽 업적 정의는 자유롭게 늘리되, **파트너 사이트 등록만 M5로 미룬다**(출시 후 삭제가 비가역). 오타는 에디터 검증기로 로컬에서 잡는다 | **§8 재작성** |
| 4 | 옷장 3D 프리뷰 | **한다.** UI 창 안에 캐릭터 3D 메시가 뜨고 **옷이 실시간으로 갈아입혀진다** | **§5.8 (신설)**, §7.2 |

---

## 0.2 1차 개정(2026-07-27) 대비 무효화된 전제 ★

**아래는 전부 "1차 개정이 사실이라고 적었으나 지금은 코드에 없는 것"이다.** 그대로 착수하면 컴파일되지 않는다.

| 1차 개정의 서술 | 2026-08-25 실측 | 영향 |
|---|---|---|
| SaveGame은 **v2**, 필드 8개 | **v4**. 필드 7개 (§1.1) | §4.2 마이그레이션 절 전체 무효 |
| `SaveGame.UnlockedAchievements : TArray<FName>` 존재 | **필드 없음**. `ClaimedRewards` 로 대체 | 해금 판정의 입력이 바뀜 (§4.5) |
| `SaveGame.TotalKills` / `ClaimedKillMilestones` 존재 | **둘 다 삭제됨** | `RequiredTotalKills` 해금 조건 **성립 불가** (§4.4) |
| `FDRStageAchievementDef` (`bOncePerAccount` 포함) | **`FDRAchievementDef`** 로 개명 + 필드 교체 (§1.2) | §4.3의 확장 대상이 바뀜 |
| `FDRKillMilestone` (누적 처치 보상) | **삭제됨** | 마일스톤 기반 해금 규칙 삭제 |
| `FDRStageRewardResult.NewlyUnlockedAchievements` | **없음**. `Lines : TArray<FDRRewardLineItem>` | 1차 개정 §5.3의 훅 2줄이 **컴파일 실패** (§5.1) |
| `FDRStageRewardReport` 에 스테이지 식별자 없음 (리스크 #5) | **`FName StageId` 추가됨** | 리스크 #5 **종결** |
| "`ApplyStageReward` 호출자가 0개 — Plan2 M4 미착수" | **완료.** `DRStageGameMode.cpp:237` → `DRPlayerController.cpp:1408` | **M2 선행 조건 해소 ★** |
| "v2 → v3 **비파괴** 마이그레이션을 하라" | 현행 마이그레이션은 **의도적으로 파괴적**(미출시 전제, `DRGameInstance.cpp:112-127`) | **버전을 올리면 재화·업그레이드가 전부 소실.** 결론이 정반대가 됨 (§4.2) |
| 코스메틱 동기화 = `ADRCharacter::ServerSetLoadout` 신설 | `ADRPlayerState::EquippedChips` 라는 **검증된 선례가 생김** | 신규 RPC 설계 폐기, 선례 복제 (§6.1) |
| 식별자로 `Unlockable.*` GameplayTag 도입 | 코드베이스 전체가 FName 규약으로 수렴 | 태그 안 폐기 (§4.1) |
| `EPlayerCharacterClass` 에 값 추가 시 주의 | 여전히 3종 + `Count` (변경 없음) | 유효 — §12-6 유지 |
| Plan4 STEP6(Dashboard) 미완 | **완료.** `Config/Localization/Game_Gather.ini` 존재 (§1.9) | 남은 건 **Gather 경로에 폴더 추가 1줄** |

---

## 1. 현재 코드베이스 실측 (2026-08-25 기준)

### 1.1 SaveGame — v4, 필드 7개

`Source/DaeRune/Public/Game/DRSaveGame.h`

| 필드 | 타입 | 줄 | 용도 |
|---|---|---|---|
| `bHasCompletedTutorial` | `bool` | 27 | 튜토리얼 완료 |
| `Currency` | `int32` | 33 | 계정 공용 지갑 |
| `LifetimeCurrency` | `int32` | 37 | 통계용 누적 획득량 |
| **`ClaimedRewards`** | **`TArray<FName>`** | 43 | **1회성 보상 원장.** `"StageClear.<StageId>"` + 업적 Id 를 함께 담는다 |
| `bUpgradeSystemUnlocked` | `bool` | 49 | 스테이지1 최초 클리어로 해금 |
| `ClassUpgrades` | `TMap<EPlayerCharacterClass, FDRClassUpgradeState>` | 53 | 로봇별 슬롯/장착 칩 |
| `SaveVersion` | `int32` | 59 | **`CurrentSaveVersion = 4`** (70줄) |

- 슬롯: `SaveSlotName = "DaeRunePlayerProgress"`, `UserIndex = 0` **고정** → 해금은 **PC 단위**다. §12-2.
- **마이그레이션은 의도적으로 파괴적이다** (`DRGameInstance.cpp:112-127`):
  ```cpp
  if (CurrentSaveGame->SaveVersion != UDRSaveGame::CurrentSaveVersion)
  {
      CurrentSaveGame->Currency = 0;
      CurrentSaveGame->LifetimeCurrency = 0;
      CurrentSaveGame->ClaimedRewards.Reset();
      CurrentSaveGame->bUpgradeSystemUnlocked = false;
      CurrentSaveGame->ClassUpgrades.Reset();
      CurrentSaveGame->SaveVersion = UDRSaveGame::CurrentSaveVersion;
  }
  ```
  주석에 "미출시 단계라 구 진행도 환산은 하지 않는다"고 명시돼 있다. → **§4.2의 결론이 1차 개정과 정반대다.**

### 1.2 업적·보상 파이프라인 — 전 구간 완성 ★

1차 개정이 "호출자 0개"라고 적었던 구간이 **지금은 끝에서 끝까지 연결돼 있다.**

```
[서버] 페이즈/BP 판정
  └─ ADRStageGameMode::GrantStageAchievement(FName, PC=nullptr)      DRStageGameMode.cpp:429-442
       PC != nullptr → PendingAchievements[PC]     (개인 업적)
       PC == nullptr → GlobalPendingAchievements   (전원 업적)
                │
                ▼  스테이지 종료
     ADRStageGameMode::NotifyAllPlayersGameEnd()                      DRStageGameMode.cpp:197-253
       FDRStageRewardReport { StageId, PlayedClass, bGameClear,
                              ClearedPhaseCount, KillCount, AchievementIds[] }
       ※ 업적은 bGameClear 를 전제로만 실린다 (225줄)
                │
                ▼  PC->Client_GrantStageReward(Report)                DRStageGameMode.cpp:237
─────────────────────────── 클라이언트 경계 ───────────────────────────
     ADRPlayerController::Client_GrantStageReward_Implementation      DRPlayerController.cpp:1402-1414
                │
                ▼
     UDRGameInstance::ApplyStageReward(Report) → FDRStageRewardResult DRGameInstance.cpp:1224-1320
       - StageClear 원장 대조 → 재화 지급 + bUpgradeSystemUnlocked
       - 업적 루프(1276-1300): ClaimedRewards 에 없으면 추가 + 재화, 이미 있으면 continue(1287)
       - SaveProgress() / OnCurrencyChanged / OnUpgradeSystemUnlocked
```

**이 문서가 붙일 지점은 딱 하나다** — `ApplyStageReward` 의 업적 루프(§5.1).

- `FDRAchievementDef` (`DRProgressionConfig.h:43-68`)
  `{ FName AchievementId(변경 금지), EDRRewardKind Kind, FName StageId, FText DisplayName, FText Description, int32 Currency }`
- `FDRStageRewardResult` (`DRProgressionTypes.h:142-171`)
  `{ FirstClearCurrency, AchievementCurrency, TotalGained, CurrencyBefore, CurrencyAfter, bUpgradeSystemNewlyUnlocked, Lines[] }`
  → **`NewlyUnlockedAchievements` 는 없다.** 이번에 새로 달성된 업적은 `Lines` 중 `Kind != StageFirstClear` 인 항목의 `RewardId` 다.

> ⚠️ 현재 `GrantStageAchievement()` 를 호출하는 **C++ 코드는 0개**다(BlueprintCallable이라 BP에서 부르도록 열려 있음).
> 즉 **파이프라인은 완성이지만 실제로 흐르는 업적 데이터는 아직 없다.** §10 M0에서 이걸 먼저 채운다.

### 1.3 캐릭터 클래스 — 3종 + `Count` 센티널 (변경 없음)

`AbilitySystem/Data/CharacterClassInfo.h:32-40`
```cpp
enum class EPlayerCharacterClass : uint8
{ Gardener, VendingMachine, RobotVacuum, Count UMETA(Hidden) };
```
`Count` 의존 코드: `DRGameInstance.cpp:130-134`(lazy 초기화), `DRPlayerController.cpp:1841`(순환 모듈로).
→ **신규 값은 반드시 `Count` 직전에.** (적 enum `ECharacterClass` 는 `MoleBoss` 가 맨 뒤에 추가됨 — 같은 규약.)

캐릭터 BP 매핑: `UPlayerCharacterClassInfo::CharacterBPClasses` (`CharacterClassInfo.h:115`).

### 1.4 로비는 2단계 상태다 — 옷장이 놓일 곳 ★

`ELobbyState` (`DRLobbyTypes.h:9-15`): `WaitingRoom` → `Transitioning` → `FreeRoam`

| | WaitingRoom | FreeRoam |
|---|---|---|
| 플레이어 폰 | **없음.** `GetDefaultPawnClassForController` 가 `nullptr` 반환 (`DRLobbyGameMode.cpp:658-663`) | 선택 클래스 BP로 Possess |
| 화면 | 고정 카메라 (`ADRWaitingRoomCameraActor`) + UI | 자유 조작 |
| 표시용 캐릭터 | **`SpawnDisplayCharacter()`** — 서버가 스폰한 **비점유** `ADRCharacter` (`DRLobbyGameMode.cpp:686-720`) | 없음 |
| 클래스 변경 | 가능 (`ServerRequestChangeClass`, WaitingRoom 한정) | 불가 |
| 걸어가서 쓰는 장치 | **불가능** (폰이 없음) | `ADRUpgradeStation`, `ADRStageSelectActor` |

→ **옷장은 FreeRoam 배치물이다.** 그리고 대기실 디스플레이 캐릭터에 스킨을 입히려면
**PlayerState 를 못 읽는다**(비점유 = `GetPlayerState()` 없음). 별도 전달 경로가 필요하다 (§6.3).

### 1.5 상호작용 장치의 표준형 — `ADRUpgradeStation`

`Public/Actor/DRUpgradeStation.h` / `Private/Actor/DRUpgradeStation.cpp` (119줄). **옷장은 이걸 그대로 베낀다.**

```cpp
bReplicates = false;                               // 상호작용이 전부 로컬이라 복제할 상태가 없다
StationMesh(Root) + InteractionBox + InteractionWidget(Screen space)

BeginPlay:   InteractionBox->OnComponentBeginOverlap/EndOverlap 바인딩  (각 머신 로컬 판정)
BeginOverlap: Cast<ACharacter> → GetController() → IsLocalController() 확인
              OverlappingLocalController->OnInteractPressed.AddDynamic(...)
              InteractionWidget->SetVisibility(true); OnLocalPlayerEnteredRange(bUnlocked);
OnLocalInteract: 해금 검사 실패 → OnInteractBlocked(BP 이벤트)
                 성공 → OverlappingLocalController->OpenUpgradeScreen();
EndPlay/EndOverlap: ClearLocalController() — ★델리게이트 해제 필수★
```

- `OnInteractPressed` 는 `ADRPlayerController` 의 `DECLARE_DYNAMIC_MULTICAST_DELEGATE` (`DRPlayerController.h:15, 80`).
- BP 훅 3종(`OnLocalPlayerEnteredRange` / `OnLocalPlayerLeftRange` / `OnInteractBlocked`)이 프롬프트 문구를 담당.

### 1.6 "클라 보고 → 서버 정화 → 복제" 의 표준형 — `EquippedChips` ★

**코스메틱 동기화가 그대로 따라야 할 선례.** 1차 개정이 설계했던 신규 RPC는 필요 없다.

```
[클라] UDRGameInstance (로컬 세이브)  ── GetEquippedChips(SelectedClass) ──┐
                                                                          ▼
     ADRPlayerController::ReportUpgradeLoadout()              DRPlayerController.cpp:1358-1370
       → ServerReportUpgradeLoadout(ForClass, Chips)   [Server, Reliable]
                                                                          │
[서버] ServerReportUpgradeLoadout_Implementation          DRPlayerController.cpp:1372-1400
       ① ForClass != PS->GetSelectedPlayerClass() → 폐기 (경합/스푸핑 방어, 1379줄)
       ② Catalog->SanitizeLoadout(...) 로 정화 (없으면 빈 배열)
       ③ PS->SetEquippedChips(Sanitized)
                                                                          │
     ADRPlayerState::SetEquippedChips  (서버)                 DRPlayerState.cpp:114-120
       → EquippedChips 대입 + RebuildUpgradeRuntime()
     ADRPlayerState::OnRep_EquippedChips (클라)               DRPlayerState.cpp:122-125
       → RebuildUpgradeRuntime()
                                                                          │
     RebuildUpgradeRuntime()                                  DRPlayerState.cpp:127-145
       → 캐시 재구축 후 Cast<ADRCharacter>(GetPawn())->RefreshUpgradeEffects()
         ★"보고가 스폰보다 늦게 도착해도 자동으로 치유되는 경로"★
```

**보고를 촉발하는 4개 지점** — 코스메틱도 정확히 같은 곳에 붙인다:

| 지점 | 파일:줄 | 이유 |
|---|---|---|
| 레벨 진입 | `DRPlayerController.cpp:935` (`OnLevelEntered`) | 호스트/이미 PS가 준비된 경로 |
| PlayerState 도착 | `DRPlayerController.cpp:2055` (`OnRep_PlayerState` 계열) | PS가 늦게 오는 경로 |
| 화면 닫기 | `DRPlayerController.cpp:1556` (`CloseUpgradeScreen`) | 변경 즉시 반영 |
| 클래스 변경 | `DRPlayerState.cpp:499` (`OnRep_SelectedPlayerClass`) | 새 클래스의 목록으로 교체 |

그리고 서버 측 `SetSelectedPlayerClass` 는 **이전 클래스의 장착 목록을 먼저 비운다** (`DRPlayerState.cpp:476`).
스킨도 같은 처리가 필요하다(자판기 스킨을 청소기가 입는 사고 방지).

**맵 전환 보존**: `ADRPlayerState::CopyProperties` (`DRPlayerState.cpp:60-76`) 가
`SelectedPlayerClass` / `WaitingRoomSlotIndex` / `bIsHost` / `EquippedChips` 를 복사한다.
→ **"입은 채로 스테이지에 간다"는 요구는 여기에 한 줄 추가로 해결된다.**

### 1.7 메시 구조와 사망 Dissolve 충돌 ★

`ADRCharacter` 가 가진 스킨 적용 대상:

| 컴포넌트 | 선언 | 비고 |
|---|---|---|
| `GetMesh()` (TP) | `ACharacter` 상속 | 타인에게 보이는 몸체 |
| `FirstPersonMesh` | `DRCharacter.h:206` | **본인에게만 보임.** 청소기는 FP 스켈레톤이 TP와 완전 별개 (Plan5 §1.1) |
| `Weapon` | `DRCharacterBase.h:110` | 별도 `USkeletalMeshComponent` — **1차 개정이 누락한 대상** |

`UpdateMeshVisibility()` (`DRCharacter.cpp:467-497`)가 소유자 기준으로 FP/TP를 가른다.
→ **한쪽에만 적용하면 "내 스킨이 나한테만 안 보인다"** 가 된다.

**★새로 발견된 충돌 — 사망 Dissolve가 머티리얼 슬롯 0을 덮어쓴다★**
```cpp
// ADRCharacterBase::Dissolve()                    DRCharacterBase.cpp:505-520
GetMesh()->SetMaterial(0, DynamicMatInst);   // ← 스킨 머티리얼이 여기서 날아간다
Weapon ->SetMaterial(0, DynamicMatInst);
```
부활(`MulticastHandleRevive_Implementation`, `DRCharacterBase.cpp:321-322`)은
`K2_OnCharacterRevived()` **BP 이벤트에 원복을 위임**한다. BP는 "원래 머티리얼"이 아니라
"Dissolve 파라미터"만 되돌리므로, **부활 후 스킨을 재적용하지 않으면 기본 외형으로 돌아간다.**
→ §5.4에서 부활 경로에 재적용 훅을 넣는다.

`SetWaitingRoomVisibility()` (`DRCharacter.cpp:557-605`) 도 FP/TP 가시성을 뒤집으므로 대기실 표시와 함께 확인.

### 1.8 스팀 현황

`Config/DefaultEngine.ini:110-119`
```ini
[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480          ; ← Spacewar(밸브 샘플). 실 AppId 교체 필수
bInitServerOnClient=true
```
- `DaeRune.uproject` 에 `OnlineSubsystemSteam` **활성**, `DaeRune.Build.cs` 에 `OnlineSubsystem`, `OnlineSubsystemUtils`
  **이미 Public 의존성** → 모듈 추가 작업 없음.
- **`steam_appid.txt` 없음** (프로젝트 전체 검색 결과 0건).
- `DefaultPlatformService=Steam` 이라 PIE에서도 스팀이 기본 서비스다. 스팀 미실행 시 인터페이스가 null인 경로가
  상시 발생하므로 **`IsBackendAvailable()==false` 는 예외가 아니라 개발 중 기본 경로**다.

### 1.9 현지화 현황 (Plan4)

- ✅ STEP1~4 완료 + **STEP6(Dashboard) 완료** — `Config/Localization/Game_Gather.ini` 가 실재하며
  `NativeCulture=ko-KR`, `CulturesToGenerate=en, ko-KR` 로 구성돼 있다.
- ⚠️ **`GatherTextStep1`(에셋 수집)의 `IncludePathFilters` 에 진행도 폴더가 없다:**
  ```ini
  IncludePathFilters=Content/Blueprints/UI/*
  IncludePathFilters=Content/Blueprints/Phase/Data/*
  IncludePathFilters=Content/Blueprints/AbilitySystem/Data/*
  IncludePathFilters=Plugins/MultiplayerSessions/Content/*
  ```
  DataAsset 인스턴스는 **`Content/Blueprints/Progression/`** 에 있다
  (`DA_ProgressionConfig`, `DA_ChipCatalog`, `DA_UpgradeUIStyle`).
  → **현재 `FDRAchievementDef::DisplayName` / `Description` 은 수집되지 않고 있다.**
  신규 `DA_CosmeticCatalog` 도 같은 폴더에 놓을 것이므로 **한 줄 추가로 둘 다 해결**된다 (§7.5).
- 신규 C++ 문자열은 처음부터 `LOCTEXT`/`NSLOCTEXT` (Plan4 STEP5 관례).

### 1.10 기타 관례

- `UDRAssetManager` (`Public/DRAssetManager.h`, 45줄)는 **GameplayTags/GAS 초기화 + 사운드 사전 로딩뿐**이며
  **범용 비동기 로드 API가 없다.** → 코스메틱 비동기 로드는 `UAssetManager::GetStreamableManager().RequestAsyncLoad` 직접 사용.
- 기존 파일 다수가 한글 주석 모지바케 상태(`DRPlayerController.cpp`, `DRCharacter.h`, `DRPlayerState.h` 등).
  **신규 파일은 UTF-8 고정**, 기존 파일 수정 시 **주변 인코딩을 보존**한다 (Plan6 §0 관례 계승).
- 치트 exec 함수 관례: `DRUnlockUpgrade` / `DRAddCurrency` / `DRDumpUpgrade` (`DRPlayerController.cpp:1447-1542`).

---

## 2. 목표 및 비목표

### 목표

1. 로비 FreeRoam에 **옷장(`ADRWardrobe`)** 을 배치하고, 상호작용하면 **옷장 화면**이 열린다.
2. 화면에서 **현재 로봇의 스킨**을 고르면 로컬 세이브에 즉시 저장된다.
3. 화면을 닫으면 **로비의 내 캐릭터에 즉시 반영**되고, **다른 플레이어에게도 보인다.**
4. 스테이지로 이동해도 **그 외형이 유지**된다 (Seamless Travel 보존).
5. 스킨은 **스팀 도전과제 달성**으로 해금된다. 잠긴 항목은 조건 안내와 함께 표시된다.
6. 달성한 업적은 **스팀 Achievement에 미러링**된다 (오버레이 토스트).
7. **스팀이 꺼져 있어도 전 기능이 로컬로 동작**한다.

### 비목표 (이번 범위 밖)

- **플레이어블 캐릭터(로봇) 해금** — 요구는 "옷"이다. §12-1에서 결정 항목으로만 유지.
- ~~**메시 교체형 스킨 / 부착물(모자) / 트레일 이펙트** — 2차 범위~~
  → **2026-08-25 해제.** 아트 시안의 4개 카테고리(HEAD·FACE·BODY·TAIL)가 **부착물로 확정**되어
  **부착물 메시가 1차 범위에 포함**된다 (§15.11). 다만 **트레일 이펙트는 여전히 비목표**다.
- **캐릭터 본체 메시 교체(Skeletal Mesh 자체 스왑)** — 부착물로 대체 가능하므로 하지 않는다.
- 유료 DLC / 마이크로트랜잭션 / Steam Inventory Service / 드롭·거래.
- 시즌패스, 서버 권위형 전적 DB.
- 신규 플레이어블 캐릭터 제작.

---

## 3. 아키텍처

```
[서버: 게임플레이]
  페이즈 / BP ──▶ ADRStageGameMode::GrantStageAchievement(FName, PC)      ★이미 존재★
        │           스테이지 종료 시 FDRStageRewardReport 로 집계
        ▼           PC->Client_GrantStageReward(Report)
────────────────────────── 클라이언트 경계 ──────────────────────────
[UDRGameInstance::ApplyStageReward]                                       ★이미 존재★
   ├─ (기존) ClaimedRewards 원장 대조 → 재화 지급 → SaveProgress
   └─ (신규 3줄) EarnedAchievements.AddUnique(Id)                          §5.1
        │
        ├──▶ [UDRAchievementSubsystem] (신규, UGameInstanceSubsystem)      §5.7
        │       AchievementId(FName) → SteamApiName(FString) → 스팀 write
        │       백엔드 없으면 PendingSteamAchievements 큐잉 → 복귀 시 flush
        │       QueryFromBackend() 로 타 PC 달성분 역매핑 → EarnedAchievements 합집합
        │
        └──▶ [UDRGameInstance 코스메틱 API]  (신규)                         §5.1
                IsSkinUnlocked(SkinId)  ← EarnedAchievements ∪ ClaimedRewards 로 ★파생★
                EquipSkin / GetEquippedSkin  → SaveGame.EquippedSkins
                        │  OnCosmeticsChanged
                        ├──▶ 옷장 화면 갱신 (UDRWardrobeScreenWidget)       §5.6
                        └──▶ 해금 토스트 (결과창 연동)                       §7.4

[동기화]  ADRPlayerController::ReportCosmeticLoadout()                      §5.3
            → ServerReportCosmeticLoadout(ForClass, SkinId)   [Server, Reliable]
            → ADRPlayerState::SetEquippedSkinId (정화 후)                   §5.2
            → 복제 → OnRep_EquippedSkinId → RebuildCosmeticRuntime()
            → ADRCharacter::RefreshSkinVisuals()  (FP + TP + Weapon)        §5.4
```

### 3.1 권위 모델

| 대상 | 권위 | 근거 |
|---|---|---|
| 업적 **달성 판정** | **서버** | 처치·페이즈 클리어·무피해는 서버만 정확히 안다. 클라는 relevancy 밖 사망 이벤트를 못 받는다 |
| 업적 **기록/중복 방지** | 클라이언트 (SaveGame) | Plan2 확정 사항. 계정 진행도는 로컬이 소유 |
| 스킨 **해금 판정** | 클라이언트 (파생) | 계정 단위 상태. 서버가 알 필요 없음 |
| 스킨 **선택** | 클라이언트 | 순수 로컬 UI. 서버 왕복 없음 |
| 스킨 **외형 동기화** | 서버 복제 (단순 전달 + 카탈로그 정화) | 게임플레이 영향 0 — 강력한 검증 불필요, 존재 여부만 확인 |

**치팅 관점**: 협동 PvE다. 잠긴 스킨을 보고해도 남에게 피해가 없다. 다만 **카탈로그에 없는 Id 는 서버가 버린다**
(`SanitizeLoadout` 선례와 동일) — 존재하지 않는 에셋 참조로 클라가 크래시하는 것을 막기 위함이다.

---

## 4. 데이터 모델

### 4.1 식별자 전략 — `FName` 단일 ★

1차 개정의 2단 구조(업적=FName / 해금=GameplayTag)를 **폐기**한다.

| 대상 | 식별자 | 이유 |
|---|---|---|
| 업적 | `FName AchievementId` | 기존. `DRProgressionConfig.h:49-50` 에 "변경 금지" 명시 |
| 스킨 | **`FName SkinId`** | 세이브에 저장된다. `ChipId`·`RewardId` 가 전부 FName이라 규약이 일치하고, GameplayTag rename 시의 리다이렉터 누락 사고가 원천 차단된다 |
| 스팀 API Name | `FString SteamApiName` | 스팀 파트너 페이지가 문자열 키를 쓴다 |

명명 규약: `Skin.<Class>.<Name>` — 예 `Skin.Gardener.Blue`, `Skin.VendingMachine.Retro`.
`ClaimedRewards` 의 `StageClear.<StageId>` 와 같은 접두어 방식이라 로그에서 바로 구분된다.

> **`Unlockable.*` / `Cosmetic.Slot.*` GameplayTag 는 추가하지 않는다.** `DRGameplayTags.h/.cpp` 무수정.

### 4.2 `UDRSaveGame` 확장 — **버전을 올리지 말 것** ★★

```cpp
// DRSaveGame.h — 기존 필드는 전부 그대로 두고 아래만 추가

// ========== 코스메틱 ==========

// 달성한 업적 Id (로컬 달성 + 스팀 역매핑의 ★합집합★).
// ClaimedRewards 와 분리하는 이유는 §4.5 를 볼 것 — 재화 원장과 절대 섞지 않는다.
UPROPERTY(VisibleAnywhere, Category = "Progress|Cosmetic")
TArray<FName> EarnedAchievements;

// 로봇별 장착 스킨. 키가 없거나 NAME_None 이면 BP 기본 외형.
UPROPERTY(VisibleAnywhere, Category = "Progress|Cosmetic")
TMap<EPlayerCharacterClass, FName> EquippedSkins;

// 스팀에 아직 write 하지 못한 업적 (오프라인 달성분 backfill 큐)
UPROPERTY(VisibleAnywhere, Category = "Progress|Cosmetic")
TArray<FName> PendingSteamAchievements;

// ★ CurrentSaveVersion 은 4 그대로 둔다 ★
```

**`CurrentSaveVersion` 을 5로 올리면 안 되는 이유:**

`EnsureProgressInitialized()` (`DRGameInstance.cpp:112-127`)는 버전이 다르면
`Currency` / `LifetimeCurrency` / `ClaimedRewards` / `bUpgradeSystemUnlocked` / `ClassUpgrades` 를
**전부 초기화한다.** 이건 버그가 아니라 "미출시 단계"를 전제한 의도적 설계다.

**순수 필드 추가에는 버전 상승이 필요 없다.** UE SaveGame 역직렬화는 구 세이브에 없는 프로퍼티를
**기본값(빈 배열/빈 맵)으로 채운다.** 즉:

| 버전 처리 | 결과 |
|---|---|
| **4 유지 (권장)** | 기존 세이브 그대로 로드 + 신규 필드는 기본값. 재화·업그레이드 **보존** |
| 5로 상승 | 마이그레이션 분기 발동 → **테스터 전원의 재화·슬롯·칩 소실** |

> 1차 개정은 "v2→v3 비파괴 마이그레이션을 추가하라"고 적었다. **지금 그 지시를 따르면 정확히 반대 결과가 난다.**
> 버전은 **저장 필드의 의미가 바뀔 때만** 올린다.

`EnsureProgressInitialized()` 에 추가할 lazy 초기화 (기존 `ClassUpgrades` 루프 옆, `DRGameInstance.cpp:129-134`):
```cpp
// 누락된 로봇 키를 빈 스킨으로 lazy 초기화 (enum 추가 시 자동 대응)
for (int32 Index = 0; Index < ClassCount; ++Index)
{
    CurrentSaveGame->EquippedSkins.FindOrAdd(static_cast<EPlayerCharacterClass>(Index));
}
```

**정화(Sanitize)**: 카탈로그에서 삭제된 스킨 Id 가 장착돼 있으면 `NAME_None` 으로 되돌린다.
`SanitizeClassState()` 가 고아 칩에 하는 처리와 같은 취지 — 다만 재화 환급은 없다(스킨은 무료).

### 4.3 `FDRAchievementDef` 확장 — 컬럼 1개

```cpp
// DRProgressionConfig.h — 기존 구조체(43-68줄) 말미에 추가

// 스팀 파트너 페이지에 등록한 Achievement API Name.
// 비우면 스팀 미러링 대상이 아니다(로컬 전용 업적).
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Achievement|Steam")
FString SteamApiName;
```
→ 1차 개정의 `FDRAchievementDef` **신설 안은 필요 없다** — 그 이름의 구조체가 이미 실재하며
`DisplayName` / `Description` / `Kind` / `StageId` 를 전부 갖고 있다.

### 4.4 `UDRCosmeticCatalog` (신규 DataAsset)

`UDRChipCatalog` 의 자매 에셋. `UDRProgressionConfig` 가 `ChipCatalog` 를 매달고 있는 것과 같은 방식으로 매단다.

```cpp
// Public/Game/DRCosmeticTypes.h (신규)

/** 스킨 1종의 정의. 1차 범위는 머티리얼 오버라이드만 다룬다. */
USTRUCT(BlueprintType)
struct FDRSkinDefinition
{
    GENERATED_BODY()

    // "Skin.Gardener.Blue". 변경 금지 — 세이브에 저장된다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
    FName SkinId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
    EPlayerCharacterClass OwnerClass = EPlayerCharacterClass::Gardener;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
    FText Description;

    // ===== 해금 조건 =====
    // 필요한 업적/보상 Id. 비어 있으면 ★기본 제공★(항상 해금).
    // SaveGame 의 EarnedAchievements ∪ ClaimedRewards 와 대조한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Unlock")
    TArray<FName> RequiredAchievements;

    // true = 전부 달성해야 해금(AND), false = 하나만 달성해도 해금(OR)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Unlock")
    bool bRequireAll = true;

    // 잠금 툴팁 문구. 비면 RequiredAchievements 의 DisplayName 을 조합해 자동 생성.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Unlock")
    FText HowToUnlock;

    // ===== 외형 =====
    // 3인칭 메시 머티리얼. Index = 머티리얼 슬롯 인덱스. 비면 그 슬롯은 건드리지 않는다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Visual")
    TArray<TSoftObjectPtr<UMaterialInterface>> ThirdPersonMaterials;

    // 1인칭 메시 머티리얼. 청소기처럼 FP 스켈레톤이 별개인 경우 필수 (§1.7).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Visual")
    TArray<TSoftObjectPtr<UMaterialInterface>> FirstPersonMaterials;

    // 무기(Weapon) 컴포넌트 머티리얼. ADRCharacterBase::Weapon 은 별도 스켈레탈 메시다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Visual")
    TArray<TSoftObjectPtr<UMaterialInterface>> WeaponMaterials;

    // ===== UI =====
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|UI")
    TSoftObjectPtr<UTexture2D> PreviewIcon;

    // 목록 정렬 순서 (작을수록 앞)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|UI")
    int32 SortOrder = 0;

    // 2차 범위: OverrideMesh(FP/TP), AttachmentActor + Socket, TrailEffect
};

/** 옷장 화면의 카드 1장을 그리는 데 필요한 전부. (FDRChipViewModel 의 스킨 버전) */
USTRUCT(BlueprintType)
struct FDRSkinViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Skin") FName SkinId;
    UPROPERTY(BlueprintReadOnly, Category = "Skin") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category = "Skin") FText Description;
    UPROPERTY(BlueprintReadOnly, Category = "Skin") TObjectPtr<UTexture2D> PreviewIcon;
    UPROPERTY(BlueprintReadOnly, Category = "Skin") bool bUnlocked = false;
    UPROPERTY(BlueprintReadOnly, Category = "Skin") bool bEquipped = false;
    // bUnlocked == false 일 때 표시할 조건 문구 (HowToUnlock 또는 자동 생성)
    UPROPERTY(BlueprintReadOnly, Category = "Skin") FText UnlockHint;
};
```

```cpp
// Public/Game/DRCosmeticCatalog.h (신규)
UCLASS(BlueprintType)
class DAERUNE_API UDRCosmeticCatalog : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic",
              meta = (TitleProperty = "SkinId"))
    TArray<FDRSkinDefinition> Skins;

    const FDRSkinDefinition* FindSkin(FName SkinId) const;

    // 이 로봇의 스킨 정의 (SortOrder 순)
    void GetSkinsForClass(EPlayerCharacterClass CharacterClass,
                          TArray<const FDRSkinDefinition*>& OutSkins) const;

    // 서버 정화: 카탈로그에 없거나 클래스가 다른 Id 면 NAME_None 을 돌려준다.
    // ★UDRChipCatalog::SanitizeLoadout 과 같은 역할★
    FName SanitizeSkin(FName SkinId, EPlayerCharacterClass ForClass) const;

#if WITH_EDITOR
    // SkinId 중복 / RequiredAchievements 가 ProgressionConfig 에 실재하는지 검증
    void ValidateAgainst(const UDRProgressionConfig* Config, TArray<FString>& OutErrors) const;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
};
```

`UDRProgressionConfig` 에 참조 추가 (`ChipCatalog` 바로 아래, `DRProgressionConfig.h:82-83`):
```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
TObjectPtr<UDRCosmeticCatalog> CosmeticCatalog;
```
> GameInstance BP에 이미 `ProgressionConfig` 슬롯이 있으므로(`DRGameInstance.h:44`) **새 슬롯을 늘리지 않는다.**
> `GetChipCatalog()` 가 `ProgressionConfig->ChipCatalog` 를 반환하는 것과 대칭으로 `GetCosmeticCatalog()` 를 둔다.

> ⚠️ **`RequiredTotalKills` 는 만들 수 없다.** 1차 개정이 전제한 `SaveGame.TotalKills` 가 삭제됐다(§0.2).
> 누적 처치 기반 해금이 필요하면 **먼저 서버 측 누적 통계를 Plan2에 되살려야 한다** — 이번 범위 밖이다.
> `ADRPlayerState::StageKillCount` 는 **스테이지 한정 값**이고 `CopyProperties` 에서 의도적으로 복사되지 않는다
> (`DRPlayerState.cpp:74`). 대신 **"N킬 달성" 업적을 서버가 판정해 `GrantStageAchievement` 로 넘기면**
> 같은 결과를 얻는다 — 이쪽이 기존 파이프라인과 일치한다.

### 4.5 해금 판정 = 저장하지 않고 파생 ★

```cpp
bool UDRGameInstance::IsSkinUnlocked(FName SkinId) const
{
    const FDRSkinDefinition* Def = FindSkinDef(SkinId);
    if (!Def) return false;

    // 조건이 없으면 기본 제공
    if (Def->RequiredAchievements.Num() == 0) return true;

    // 판정 입력 = EarnedAchievements ∪ ClaimedRewards
    //   EarnedAchievements : 업적 달성 기록 (로컬 + 스팀 역매핑)
    //   ClaimedRewards     : "StageClear.<StageId>" 류도 조건으로 쓸 수 있게 함께 본다
    auto Has = [this](FName Id)
    {
        return CurrentSaveGame->EarnedAchievements.Contains(Id)
            || CurrentSaveGame->ClaimedRewards.Contains(Id);
    };

    if (Def->bRequireAll)
    {
        for (const FName& Id : Def->RequiredAchievements) { if (!Has(Id)) return false; }
        return true;
    }
    for (const FName& Id : Def->RequiredAchievements) { if (Has(Id)) return true; }
    return false;
}
```

**왜 두 원장을 분리하는가 — 이 설계의 핵심**

| 원장 | 의미 | 쓰는 곳 | 스팀이 건드리는가 |
|---|---|---|---|
| `ClaimedRewards` (기존) | **"이 보상의 재화를 이미 지급했다"** | 재화 멱등 | **절대 아니다** |
| `EarnedAchievements` (신규) | **"이 업적을 달성했다 (어느 PC에서든)"** | 해금 판정 | **합집합으로 추가** |

하나로 합치면 이런 사고가 난다: 새 PC에서 스팀 역매핑으로 업적 Id 를 `ClaimedRewards` 에 넣는 순간,
`ApplyStageReward` 의 `if (Save->ClaimedRewards.Contains(AchievementId)) continue;` (`DRGameInstance.cpp:1287`)
가 걸려 **그 업적의 재화를 영원히 못 받는다.** 재화도 로컬 값이라 이관되지 않으므로 순손실이다.
→ **분리는 선택이 아니라 필수다.**

**해금 상태를 별도 `TSet` 으로 저장하지 않는 이유**: 카탈로그의 해금 조건을 나중에 조정하면
저장된 파생값과 규칙이 어긋난다. 매번 계산하면 규칙이 곧 진실이다. 스킨 수십 개 수준에서 비용은 무시할 만하다.

**콘텐츠 규칙**: 재설치·PC 이전 후에도 살아남아야 하는 스킨은 **반드시 `SteamApiName` 이 있는 업적**에 걸어라.
`StageClear.*` 는 로컬 전용이라 스팀에서 복구되지 않는다 (§9).

### 4.6 세이브 슬롯 스팀 계정 키잉 ★ (결정 2)

**현재 문제**: `SaveSlotName = "DaeRunePlayerProgress"` / `UserIndex = 0` 이 **컴파일 타임 상수**다
(`DRSaveGame.cpp:6-7`). 한 PC에서 스팀 계정을 바꾸면 서로의 재화·업그레이드·해금을 그대로 본다.

> **Plan2 도 같은 항목을 갖고 있다. 두 문서가 같은 결론을 써야 하므로, 이 절이 양쪽의 SSOT다.**

**호출부는 3곳뿐이다** (`DRGameInstance.cpp:71, 74, 90`) — 변경 범위가 작다.

#### 4.6.1 슬롯명 해석

```cpp
// DRSaveGame.h — SaveSlotName 을 BaseSlotName 으로 개명한다.
//   ("SaveSlotName" 이라는 이름이 "고정 슬롯"을 암시해 오해를 낳는다. 호출부 3곳뿐이라 비용이 없다)

// 레거시/폴백 슬롯명. 값은 "DaeRunePlayerProgress" 그대로 유지한다.
static const FString BaseSlotName;
static const int32 UserIndex;

// "DaeRunePlayerProgress_76561198…". AccountKey 가 비면 BaseSlotName 을 그대로 돌려준다.
static FString MakeSlotName(const FString& AccountKey);
```

```cpp
// DRGameInstance.h (private)

// 이번 실행에서 쓸 슬롯명. Init 에서 1회 확정하고 ★이후 절대 바꾸지 않는다★.
// 실행 도중 바뀌면 저장이 두 파일로 갈라진다.
FString ActiveSlotName;

static FString ResolveSteamAccountKey();   // 스팀 미실행/미로그인이면 빈 문자열
void ResolveActiveSaveSlot();              // ★LoadProgress 보다 먼저 호출★
```

```cpp
FString UDRGameInstance::ResolveSteamAccountKey()
{
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return FString();

    IOnlineIdentityPtr Identity = OSS->GetIdentityInterface();
    if (!Identity.IsValid()) return FString();

    // ★GameInstance::Init 시점에는 ULocalPlayer 가 아직 없다★ — LocalPlayer 경유 API는 못 쓴다.
    // FOnlineIdentitySteam 은 LocalUserNum 만으로 로그인된 SteamID 를 돌려주므로 이 경로가 성립한다.
    FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(0);
    if (!UserId.IsValid() || !UserId->IsValid()) return FString();

    // 파일명에 쓸 수 없는 문자 제거 (SteamID 는 숫자열이지만 다른 백엔드 대비 방어)
    return UserId->ToString()
        .Replace(TEXT(":"), TEXT("_"))
        .Replace(TEXT("/"), TEXT("_"))
        .Replace(TEXT("\\"), TEXT("_"));
}
```

> ⚠️ **구현 첫 커밋에서 반드시 로그로 확인할 것**: `Init()` 시점에 SteamID 해석이 되는지.
> `OnlineSubsystemSteam` 모듈은 엔진 기동 중 초기화되므로 **스팀 클라이언트가 켜져 있으면** 이 시점에
> 이미 유효하다. 되지 않으면 §4.6.3 의 지연 확정 대안으로 전환한다.

#### 4.6.2 확정 + 레거시 이관

```cpp
void UDRGameInstance::ResolveActiveSaveSlot()
{
    UDRGameUserSettings* Settings = UDRGameUserSettings::GetDRGameUserSettings();
    FString Key = ResolveSteamAccountKey();

    if (Key.IsEmpty())
    {
        // ★스팀이 꺼져 있어도 마지막으로 확인된 계정의 세이브를 이어서 쓴다★
        // 이게 없으면 오프라인 실행마다 레거시 슬롯으로 떨어져 진행도가 갈린다 (§9).
        if (Settings) Key = Settings->LastAccountKey;
    }
    else if (Settings && Settings->LastAccountKey != Key)
    {
        Settings->LastAccountKey = Key;
        Settings->SaveSettings();
    }

    ActiveSlotName = UDRSaveGame::MakeSlotName(Key);

    // ===== 레거시 슬롯 → 계정 슬롯 1회 이관 =====
    if (Key.IsEmpty()) return;                                                        // 이미 레거시 슬롯이다
    if (UGameplayStatics::DoesSaveGameExist(ActiveSlotName, UDRSaveGame::UserIndex)) return;
    if (!UGameplayStatics::DoesSaveGameExist(UDRSaveGame::BaseSlotName, UDRSaveGame::UserIndex)) return;

    if (USaveGame* Legacy = UGameplayStatics::LoadGameFromSlot(UDRSaveGame::BaseSlotName, UDRSaveGame::UserIndex))
    {
        UGameplayStatics::SaveGameToSlot(Legacy, ActiveSlotName, UDRSaveGame::UserIndex);
        UE_LOG(LogDR, Log, TEXT("[Progression] 레거시 세이브를 계정 슬롯 '%s' 로 이관했습니다."), *ActiveSlotName);
    }
    // ★레거시 파일은 지우지 않는다★ — 이관이 잘못됐을 때 되돌릴 길을 남긴다.
}
```

`UDRGameUserSettings` 에 필드 1개 추가 (`PreferredCulture` 옆, `DRGameUserSettings.h:44-45` 패턴):
```cpp
/** 마지막으로 확인된 온라인 계정 키. 스팀이 꺼진 채 실행해도 같은 세이브를 잇기 위한 값이다. */
UPROPERTY(Config, BlueprintReadOnly, Category = "Settings|Account")
FString LastAccountKey;
```

`LoadProgress()` / `SaveProgress()` 는 `UDRSaveGame::BaseSlotName` 대신 `ActiveSlotName` 을 쓴다.
`ActiveSlotName` 이 비어 있으면 먼저 `ResolveActiveSaveSlot()` 을 호출하는 가드를 둔다
(`GetOrLoadSaveGame()` 이 `LoadProgress()` 를 늦게 부르는 경로 방어).

**`Init()` 최종 순서**
```
1. 컬처 적용                                    (기존, DRGameInstance.cpp:25-40)
2. ResolveActiveSaveSlot()                      ★신규 — LoadProgress 보다 반드시 먼저★
3. LoadProgress()                               (기존 42줄)
4. 코스메틱 정화 (카탈로그에 없는 스킨 → NAME_None)
   ★여기까지로 오프라인에서도 완전 동작★
5. GetAchievements() 유효하면 QueryFromBackend() + FlushPending()
6. OnBackendSynced → EarnedAchievements 합집합 → SaveProgress → OnCosmeticsChanged
```

#### 4.6.3 대안 (§4.6.1 검증이 실패했을 때만)

`Init()` 시점에 SteamID 가 안 나오면 **지연 확정**으로 바꾼다:
`Init()` 은 슬롯을 확정하지 않고 진행도 접근을 미루다가, 첫 `ULocalPlayer` 생성
(`OnLocalPlayerAdded` / `Identity->AddOnLoginCompleteDelegate`) 이후에 확정한다.
**비용이 크다** — 그 전에 진행도를 읽는 코드(튜토리얼 완료 여부 등)가 전부 대기해야 한다.
그래서 §4.6.1 이 성립하는지를 **가장 먼저** 확인한다.

#### 4.6.4 Steam Cloud 연동 주의 ★

Auto-Cloud 규칙에는 **키 슬롯만 등록한다**:
```
DaeRunePlayerProgress_*.sav      ← 등록
DaeRunePlayerProgress.sav        ← ★등록하지 않는다★
```
레거시 파일까지 동기화하면, 같은 PC의 계정 B가 자기 클라우드 사본을 내려받아
**계정 A의 레거시 파일을 덮어쓴 뒤 A가 이관**하는 순서에서 A의 진행도가 B의 것으로 바뀐다.
레거시 파일을 순수 로컬로 두면 이 경로가 원천 차단된다.

---

## 5. C++ 명세

### 5.1 `UDRGameInstance` 확장

```cpp
// DRGameInstance.h — 기존 "Progression|Chip" 섹션 아래에 대칭으로 추가

// ========== 코스메틱 (계정 단위, 클라 권위) ==========

UFUNCTION(BlueprintPure, Category = "Cosmetic")
UDRCosmeticCatalog* GetCosmeticCatalog() const;      // ProgressionConfig->CosmeticCatalog

UFUNCTION(BlueprintPure, Category = "Cosmetic")
bool IsSkinUnlocked(FName SkinId) const;             // §4.5 — 파생

// 장착 (SaveGame 기록 + 즉시 저장 + OnCosmeticsChanged).
// 잠긴 스킨/다른 클래스 소속이면 false. NAME_None 은 "기본 외형으로 되돌리기"로 허용한다.
UFUNCTION(BlueprintCallable, Category = "Cosmetic")
bool EquipSkin(EPlayerCharacterClass CharacterClass, FName SkinId);

UFUNCTION(BlueprintPure, Category = "Cosmetic")
FName GetEquippedSkin(EPlayerCharacterClass CharacterClass) const;

// 옷장 화면 그리기의 ★단일 진입점★ (GetChipViewModels 와 같은 규약).
// UI 가 카탈로그와 세이브를 직접 조합하지 않게 한다.
UFUNCTION(BlueprintCallable, Category = "Cosmetic")
void GetSkinViewModels(EPlayerCharacterClass CharacterClass,
                       TArray<FDRSkinViewModel>& OutViewModels) const;

// 이 로봇에 해금된 스킨이 하나라도 있는가 (옷장 프롬프트의 잠김 표시용)
UFUNCTION(BlueprintPure, Category = "Cosmetic")
bool HasAnyUnlockedSkin(EPlayerCharacterClass CharacterClass) const;

// ★치트/디버그 전용★ — 업적을 임의로 달성 처리 (DebugSetUpgradeSystemUnlocked 선례)
UFUNCTION(BlueprintCallable, Category = "Cosmetic")
void DebugGrantAchievement(FName AchievementId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCosmeticsChangedSignature, EPlayerCharacterClass, CharacterClass);
UPROPERTY(BlueprintAssignable, Category = "Cosmetic")
FOnCosmeticsChangedSignature OnCosmeticsChanged;

// 신규 스킨이 해금됐을 때 (토스트용). ApplyStageReward 직후 계산해 브로드캐스트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkinUnlockedSignature, FName, SkinId);
UPROPERTY(BlueprintAssignable, Category = "Cosmetic")
FOnSkinUnlockedSignature OnSkinUnlocked;

UFUNCTION(BlueprintPure, Category = "Achievement")
UDRAchievementSubsystem* GetAchievements() const;    // GetSubsystem 래퍼
```

**`ApplyStageReward()` 개정 — 기존 로직 무변경, 삽입만**

```cpp
// DRGameInstance.cpp:1276-1300 의 업적 루프

// (해금 판정용 스냅샷 — 루프 전에 찍어 둔다)
TSet<FName> BeforeUnlocked = SnapshotUnlockedSkins();

for (const FName& AchievementId : Report.AchievementIds)
{
    const FDRAchievementDef* Def = ProgressionConfig->FindAchievement(AchievementId);
    if (!Def) { /* 기존 경고 */ continue; }

    // ★삽입 1★ 해금 원장은 재화 지급 여부와 무관하게 항상 기록한다.
    //   1287줄의 continue 보다 ★반드시 앞★ 이어야 한다 — 뒤에 두면
    //   이미 재화를 받은 업적이 해금 판정에서 누락된다.
    Save->EarnedAchievements.AddUnique(AchievementId);

    if (Save->ClaimedRewards.Contains(AchievementId)) continue;   // (기존)
    /* ... 기존 재화 지급 로직 그대로 ... */
}

/* ... 기존 지갑 반영 / SaveProgress() / 브로드캐스트 ... */

// ★삽입 2★ 스팀 미러링 + 신규 해금 통지 (SaveProgress 이후)
if (Report.AchievementIds.Num() > 0)
{
    if (UDRAchievementSubsystem* Ach = GetAchievements())
    {
        Ach->ReportAchievements(Report.AchievementIds);   // 백엔드 없으면 큐잉
    }
    BroadcastNewlyUnlockedSkins(BeforeUnlocked);          // OnSkinUnlocked 발사
}
```

> 1차 개정이 제시한 `Result.NewlyUnlockedAchievements` 는 **존재하지 않는 필드**다(§0.2).
> 위 스냅샷 비교 방식이 현재 구조에서 "이번에 새로 해금된 스킨"을 알아내는 유일하게 정확한 방법이다.

**`Init()` 개정 순서** → **§4.6.2 참조.** 슬롯 확정(`ResolveActiveSaveSlot`)이 `LoadProgress` 보다 앞선다는 것이 핵심이다.

**세이브 슬롯 관련 추가 멤버** (§4.6)
```cpp
private:
    FString ActiveSlotName;                    // Init 에서 1회 확정, 이후 불변
    static FString ResolveSteamAccountKey();
    void ResolveActiveSaveSlot();
public:
    // 디버그/QA 표시용 — 지금 어느 계정 슬롯을 쓰고 있는지
    UFUNCTION(BlueprintPure, Category = "Save")
    FString GetActiveSaveSlotName() const { return ActiveSlotName; }
```

### 5.2 `ADRPlayerState` 확장

```cpp
// DRPlayerState.h — "업그레이드 칩" 섹션(99-118줄) 아래에 대칭으로 추가

// ========== 코스메틱 ==========

// 이 플레이어가 ★현재 선택 클래스에★ 장착한 스킨 (클라 보고 → 서버 정화 → 복제).
// 클래스별 전체 맵은 로컬 세이브에만 있다 — 남에게 필요한 건 지금 입은 옷 하나뿐이다.
UFUNCTION(BlueprintPure, Category = "Cosmetic")
FName GetEquippedSkinId() const { return EquippedSkinId; }

// 서버 전용: 정화된 스킨 Id 를 싣는다.
void SetEquippedSkinId(FName InSkinId);

protected:
UPROPERTY(ReplicatedUsing = OnRep_EquippedSkinId, BlueprintReadOnly, Category = "Cosmetic")
FName EquippedSkinId;

UFUNCTION()
void OnRep_EquippedSkinId();

// 폰에 외형을 반영한다. RebuildUpgradeRuntime 과 같은 "늦게 도착해도 치유되는" 경로.
void RefreshCosmeticVisuals();

// 소유 클라이언트에서만: 현재 선택 클래스의 스킨을 서버에 보고
void ReportCosmeticIfLocal();
```

**수정할 기존 함수 4곳**

| 함수 | 파일:줄 | 추가 내용 |
|---|---|---|
| `GetLifetimeReplicatedProps` | `DRPlayerState.cpp` | `DOREPLIFETIME(ADRPlayerState, EquippedSkinId)` |
| `CopyProperties` | `:60-76` | `DRPS->EquippedSkinId = EquippedSkinId;` ← **"입은 채로 스테이지" 요구의 핵심 한 줄** |
| `SetSelectedPlayerClass` (서버) | `:468-491` | `EquippedSkins` 도 리셋 — `EquippedSkinId = NAME_None;` (476줄의 `EquippedChips.Reset()` 옆) |
| `OnRep_SelectedPlayerClass` (클라) | `:493-502` | `ReportCosmeticIfLocal();` (499줄의 `ReportUpgradeLoadoutIfLocal()` 옆) |
| `BeginPlay` | `:105-109` | `RefreshCosmeticVisuals();` (106줄 `RebuildUpgradeRuntime()` 옆 — OnRep 이 BeginPlay 보다 앞설 수 있음) |

```cpp
void ADRPlayerState::RefreshCosmeticVisuals()
{
    if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
    {
        DRCharacter->RefreshSkinVisuals();
    }
}
```

### 5.3 `ADRPlayerController` 확장

```cpp
// DRPlayerController.h — 업그레이드 칩 RPC(317-321줄) 옆에 대칭으로

// 로컬 세이브의 스킨을 서버에 보고 (IsLocalController 가드)
void ReportCosmeticLoadout();

UFUNCTION(Server, Reliable)
void ServerReportCosmeticLoadout(EPlayerCharacterClass ForClass, FName SkinId);

// ===== 옷장 화면 (OpenUpgradeScreen 계열과 동일 구조) =====
UFUNCTION(BlueprintCallable, Category = "Cosmetic")
void OpenWardrobeScreen();

UFUNCTION(BlueprintCallable, Category = "Cosmetic")
void CloseWardrobeScreen();

UFUNCTION(BlueprintPure, Category = "Cosmetic")
bool IsWardrobeScreenOpen() const { return bIsWardrobeScreenOpen; }

UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
void OnWardrobeScreenOpened();

UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
void OnWardrobeScreenClosed();

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
TSubclassOf<class UDRWardrobeScreenWidget> WardrobeScreenWidgetClass;

private:
bool bIsWardrobeScreenOpen = false;
```

```cpp
void ADRPlayerController::ReportCosmeticLoadout()
{
    if (!IsLocalController()) return;

    const ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
    if (!DRPS) return;

    const UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
    if (!GI) return;

    const EPlayerCharacterClass SelectedClass = DRPS->GetSelectedPlayerClass();
    ServerReportCosmeticLoadout(SelectedClass, GI->GetEquippedSkin(SelectedClass));
}

void ADRPlayerController::ServerReportCosmeticLoadout_Implementation(
    EPlayerCharacterClass ForClass, FName SkinId)
{
    ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
    if (!DRPS) return;

    // 경합/스푸핑 방어 — ServerReportUpgradeLoadout:1379 와 같은 가드
    if (ForClass != DRPS->GetSelectedPlayerClass()) return;

    const UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
    const UDRCosmeticCatalog* Catalog = GI ? GI->GetCosmeticCatalog() : nullptr;

    // 카탈로그가 없으면 검증할 수 없다 — 기본 외형으로 (칩 경로:1383-1388 과 같은 판단)
    DRPS->SetEquippedSkinId(Catalog ? Catalog->SanitizeSkin(SkinId, ForClass) : NAME_None);
}
```

**보고 호출 지점 3곳** (칩과 정확히 같은 자리):

| 위치 | 파일:줄 | 추가 |
|---|---|---|
| `OnLevelEntered()` | `:935` | `ReportUpgradeLoadout();` 바로 아래 `ReportCosmeticLoadout();` |
| `OnRep_PlayerState` 계열 | `:2055` | 동일 |
| `CloseWardrobeScreen()` | 신규 | `ReportCosmeticLoadout();` (`CloseUpgradeScreen:1556` 과 동형) |

`OpenWardrobeScreen()` 은 `OpenUpgradeScreen()`(`:1418-1445`)을 그대로 따른다 —
`IsLocalController()` / 중복 열기 / **`IsInLobby()` 가드** / 설정창 먼저 닫기 / `FInputModeUIOnly` + 커서 / BP 이벤트.
`HandleToggleSettings()`(`:940-950`)에 **ESC 우선순위 분기 추가** — 옷장이 열려 있으면 옷장을 먼저 닫는다.

**치트 exec 추가** (기존 관례 `:1447-1542`):
`DRUnlockSkins`(전 스킨 해금) / `DREquipSkin(FName)` / `DRDumpCosmetic`.

### 5.4 `ADRCharacter` — 외형 적용

```cpp
// DRCharacter.h — RefreshUpgradeEffects(84줄) 옆에 대칭으로

// 장착 스킨을 FP/TP/Weapon 메시에 반영한다. ★멱등★ — 몇 번 호출해도 안전하다.
// 스킨 소스 우선순위: DisplaySkinOverride(대기실 전용) → PlayerState::EquippedSkinId
void RefreshSkinVisuals();

// 대기실 디스플레이 캐릭터 전용 (비점유라 PlayerState 가 없다 — §6.3)
UPROPERTY(ReplicatedUsing = OnRep_DisplaySkinOverride)
FName DisplaySkinOverride;

UFUNCTION()
void OnRep_DisplaySkinOverride();

// 서버 전용. ADRLobbyGameMode::SpawnDisplayCharacter 가 호출한다.
void SetDisplaySkinOverride(FName SkinId);

private:
// 비동기 로드 완료 후 실제 적용. 콜백 유효성 재확인 포함.
void ApplySkinMaterials(const FDRSkinDefinition& Def);

// 로드 요청 시점의 스킨 (콜백에서 "아직 이 스킨이 맞는가" 확인용)
FName PendingSkinId;
TSharedPtr<FStreamableHandle> SkinLoadHandle;
```

**구현 규칙**

```cpp
void ADRCharacter::ApplySkinMaterials(const FDRSkinDefinition& Def)
{
    // TP — 타인 시점
    for (int32 i = 0; i < Def.ThirdPersonMaterials.Num(); ++i)
        if (UMaterialInterface* M = Def.ThirdPersonMaterials[i].Get())
            GetMesh()->SetMaterial(i, M);

    // FP — 본인 시점. 청소기는 FP 스켈레톤이 TP와 별개다 (§1.7)
    if (FirstPersonMesh)
        for (int32 i = 0; i < Def.FirstPersonMaterials.Num(); ++i)
            if (UMaterialInterface* M = Def.FirstPersonMaterials[i].Get())
                FirstPersonMesh->SetMaterial(i, M);

    // Weapon — 별도 스켈레탈 메시 (DRCharacterBase.h:110)
    if (Weapon)
        for (int32 i = 0; i < Def.WeaponMaterials.Num(); ++i)
            if (UMaterialInterface* M = Def.WeaponMaterials[i].Get())
                Weapon->SetMaterial(i, M);
}
```

- **비동기 로드**: `TSoftObjectPtr` → `UAssetManager::GetStreamableManager().RequestAsyncLoad`.
  콜백에서 **`IsValid(this)` 와 `PendingSkinId == 현재 스킨` 을 재확인**한 뒤에만 적용한다.
  (로드 도중 사망/재스폰/재선택 시 엉뚱한 외형이 입혀지는 것을 방지)
- 이전 요청이 살아 있으면 `SkinLoadHandle->CancelHandle()` 후 재요청.
- `SkinId == NAME_None` → BP 기본 외형 유지 (아무것도 하지 않는다). 이미 다른 스킨이 적용돼 있었다면
  **BP CDO 의 머티리얼로 복구**해야 한다 — `GetClass()->GetDefaultObject<ADRCharacter>()` 의
  메시 머티리얼을 슬롯별로 되돌린다.

**호출 지점 6곳 ★**

| 지점 | 파일 | 이유 |
|---|---|---|
| `ADRPlayerState::RefreshCosmeticVisuals()` | `DRPlayerState.cpp` | 서버 set + 클라 OnRep 공용 (칩의 `RebuildUpgradeRuntime` 과 동형) |
| `ADRCharacter::BeginPlay()` | `DRCharacter.cpp` | 늦게 들어온 클라 — 복제가 이미 끝나 있는 경우 |
| `ADRCharacter::OnRep_PlayerState()` | `DRCharacter.h:41` | 폰이 PS보다 먼저 도착한 경우 |
| `ADRCharacter::PossessedBy()` | `DRCharacter.h:39` | 서버(리슨 호스트) 경로 |
| **`MulticastHandleRevive_Implementation()`** | `DRCharacter.h:69` | **★Dissolve 가 슬롯 0을 덮어썼다 (§1.7). 재적용하지 않으면 부활 후 기본 외형★** |
| `OnRep_DisplaySkinOverride()` | 신규 | 대기실 디스플레이 캐릭터 |

### 5.5 `ADRWardrobe` (신규 액터) — 옷장

`ADRUpgradeStation` 의 구조를 그대로 따른다. **차이는 세 곳뿐이다.**

```cpp
// Public/Actor/DRWardrobe.h
UCLASS()
class DAERUNE_API ADRWardrobe : public AActor
{
    GENERATED_BODY()
public:
    ADRWardrobe();

    // 이 로봇에 고를 수 있는 옷이 하나라도 있는가 (프롬프트 잠김 표시용)
    UFUNCTION(BlueprintPure, Category = "Cosmetic")
    bool HasAnySkinAvailable() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> WardrobeMesh;      // 루트

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> InteractionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UWidgetComponent> InteractionWidget;     // Screen space, 기본 숨김

    // BP 훅 (프롬프트 문구/문 열림 애니메이션)
    UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
    void OnLocalPlayerEnteredRange(bool bHasAnySkin);

    UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
    void OnLocalPlayerLeftRange();

    UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
    void OnInteractBlocked();      // 고를 옷이 하나도 없을 때 안내

private:
    UFUNCTION() void OnBoxBeginOverlap(/* ... UpgradeStation 과 동일 시그니처 ... */);
    UFUNCTION() void OnBoxEndOverlap(/* ... */);
    UFUNCTION() void OnLocalInteract();
    void ClearLocalController();

    UPROPERTY()
    TObjectPtr<ADRPlayerController> OverlappingLocalController;
};
```

**`ADRUpgradeStation` 과의 차이 3가지**

| | 업그레이드 장치 | 옷장 |
|---|---|---|
| 잠금 조건 | `GI->IsUpgradeSystemUnlocked()` (스테이지1 최초 클리어) | `GI->HasAnyUnlockedSkin(ViewedClass)` — **기본 스킨이 항상 있으므로 사실상 항상 열림** |
| 여는 화면 | `PC->OpenUpgradeScreen()` | `PC->OpenWardrobeScreen()` |
| 프롬프트 | "업그레이드" | "옷 갈아입기" |

**그대로 가져가는 것** (변경 금지):
- `bReplicates = false` — 상호작용이 전부 로컬이라 복제할 상태가 없다.
- `InteractionBox` 를 `ECC_Pawn` 만 Overlap 으로 설정, `QueryOnly`.
- BeginOverlap 에서 **`IsLocalController()` 확인 + 중복 방지**(분할 화면 대비).
- **`EndPlay` / `EndOverlap` 에서 `OnInteractPressed.RemoveDynamic` 필수** — 빼먹으면 죽은 액터가
  델리게이트에 남는다.

**배치**: `LobbyMap.umap` 의 FreeRoam 구역, `BP_UpgradeStation` 근처.
BP 래퍼 `BP_Wardrobe` 를 `Content/Blueprints/Actor/Lobby/` 에 만들어 메시·프롬프트 위젯을 지정한다.

### 5.6 `UDRWardrobeScreenWidget` (신규)

`UDRUpgradeScreenWidget` (`Public/UI/Widget/DRUpgradeScreenWidget.h`, 89줄)의 구조를 그대로 따른다.

> ★**BP 측 사양은 §15 에 있다.**★ 아래 C++ API 는 아트 시안 반영 후 **카테고리 인자가 추가**된다 —
> 최종 시그니처는 **§15.11** 을 따를 것. (`TryEquip` / `RefreshAll` 은 그대로, 조회 계열만 바뀐다)

```cpp
UCLASS(Abstract)
class DAERUNE_API UDRWardrobeScreenWidget : public UDRUserWidget
{
    GENERATED_BODY()
public:
    // 이 화면이 편집 중인 로봇 (= PlayerState 의 선택 클래스)
    UFUNCTION(BlueprintPure, Category = "Cosmetic")
    EPlayerCharacterClass GetViewedClass() const;

    UFUNCTION(BlueprintPure, Category = "Cosmetic")
    UDRGameInstance* GetProgression() const;

    // 닫기 버튼 / ESC 공통 경로 → PC->CloseWardrobeScreen()
    // ★여기를 우회하면 서버로 재보고되지 않아 외형이 반영되지 않는다★
    UFUNCTION(BlueprintCallable, Category = "Cosmetic")
    void RequestClose();

    // 카드 목록을 다시 그린다 (GetSkinViewModels 사용)
    UFUNCTION(BlueprintCallable, Category = "Cosmetic")
    void RefreshAll();

    // 카드 클릭 → GI->EquipSkin(). 잠긴 항목이면 false 를 돌려 BP 가 흔들림 연출을 한다.
    UFUNCTION(BlueprintCallable, Category = "Cosmetic")
    bool TryEquip(FName SkinId);

protected:
    virtual void NativeConstruct() override;    // OnCosmeticsChanged 구독
    virtual void NativeDestruct() override;     // ★구독 해제 필수★

    UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
    void OnRefreshSkins();
};
```

> **`NativeDestruct` 의 구독 해제가 특히 중요하다.** `UDRGameInstance` 는 레벨 전환에도 파괴되지 않으므로,
> 해제를 빼먹으면 죽은 위젯이 델리게이트에 남아 다음 진입 때 유령 갱신이나 크래시를 일으킨다.
> (`DRUpgradeScreenWidget.h:23-25` 의 경고를 그대로 계승)

### 5.7 `UDRAchievementSubsystem` (신규, `UGameInstanceSubsystem`)

역할: 스팀 도전과제 R/W 캡슐화 + 백엔드 부재 시 안전한 no-op.

```cpp
UCLASS()
class DAERUNE_API UDRAchievementSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 스팀 로그인 + Achievements 인터페이스 유효성. 미실행/오프라인이면 false.
    UFUNCTION(BlueprintPure, Category = "Achievement")
    bool IsBackendAvailable() const;

    // 로그인 후 1회: 스팀에서 달성 상태를 읽어와 캐시 → EarnedAchievements 에 합집합
    void QueryFromBackend();

    // AchievementId 목록을 스팀에 반영. 백엔드 없으면 PendingSteamAchievements 에 큐잉.
    void ReportAchievements(const TArray<FName>& AchievementIds);

    // 온라인 복귀 시 큐 비우기
    void FlushPending();

    DECLARE_MULTICAST_DELEGATE(FOnBackendSynced);
    FOnBackendSynced OnBackendSynced;

private:
    void OnQueryComplete(const FUniqueNetId& PlayerId, const bool bWasSuccessful);

    // AchievementId(FName) ↔ SteamApiName(FString). ProgressionConfig 에서 부팅 시 1회 구축.
    TMap<FName, FString> IdToApiName;
    TMap<FString, FName> ApiNameToId;

    // 캐시: SteamApiName → 달성 여부
    TMap<FString, bool> CachedAchievements;
};
```

**실제 UE API 형태** — 1차 개정이 적었던 `UnlockAchievement(FString)` 같은 함수는 **없다.**
검증 기준 헤더: `Engine/Plugins/Online/OnlineSubsystem/Source/Public/Interfaces/OnlineAchievementsInterface.h`

```cpp
IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
IOnlineAchievementsPtr Achievements = OSS ? OSS->GetAchievementsInterface() : nullptr;
IOnlineIdentityPtr     Identity     = OSS ? OSS->GetIdentityInterface()     : nullptr;
FUniqueNetIdPtr        UserId       = Identity ? Identity->GetUniquePlayerId(0) : nullptr;
// ★모든 호출에 유효한 FUniqueNetId 가 필요하다★ — null 이면 즉시 no-op 처리

// 쓰기
FOnlineAchievementsWritePtr WriteObject = MakeShareable(new FOnlineAchievementsWrite());
WriteObject->SetFloatStat(*ApiName, 100.0f);          // 100 = 달성
FOnlineAchievementsWriteRef WriteRef = WriteObject.ToSharedRef();
Achievements->WriteAchievements(*UserId, WriteRef, Delegate);

// 읽기
Achievements->QueryAchievements(*UserId, FOnQueryAchievementsCompleteDelegate::CreateUObject(...));
// 완료 후:
FOnlineAchievement Out;
Achievements->GetCachedAchievement(*UserId, ApiName, Out);   // Out.Progress >= 100 → 달성
```

**주의사항**
- 도전과제는 **스팀 파트너 페이지에 미리 정의된 것만** 해금할 수 있다. 코드로 생성 불가.
- **AppId 480(Spacewar) 환경에서는 실제 해금이 되지 않거나 Spacewar 도전과제로 뜬다.**
  개발 중에는 `IsBackendAvailable()==false` 취급하고 로컬 경로로 검증한다.
- 스팀 write 는 멱등이므로 중복 호출이 안전하다 → `FlushPending()` 에서 재시도해도 무방.

### 5.8 `ADRCosmeticPreviewStage` (신규) — UI 안의 실시간 3D 프리뷰 ★ (결정 4)

**요구**: 옷장 UI 창 안에 캐릭터 3D 메시가 보이고, 옷을 고르면 **그 자리에서 갈아입는다.**

**방식**: `SceneCaptureComponent2D` → `UTextureRenderTarget2D` → UMG `Image` 의 머티리얼.
UMG 는 3D 메시를 직접 그릴 수 없으므로 "화면 밖 촬영 스튜디오"를 만들어 찍은 그림을 UI에 붙인다.

#### 5.8.1 프리뷰 캐릭터는 로비 디스플레이 캐릭터의 재탕이다

새 렌더링 경로를 만들지 않는다. `ADRLobbyGameMode::SpawnDisplayCharacter`(`DRLobbyGameMode.cpp:686-720`)가
**이미 "비점유 `ADRCharacter` 를 세워두고 보여주기"를 검증**해 두었다. 차이는 **로컬 전용**이라는 것뿐이다.

| | 로비 디스플레이 캐릭터 | 옷장 프리뷰 캐릭터 |
|---|---|---|
| 스폰 주체 | **서버** (복제됨) | **로컬 머신** (복제 안 함) |
| 스킨 전달 | `DisplaySkinOverride` 복제 | `DisplaySkinOverride` **직접 대입** (RPC 없음) |
| 수명 | 대기실 동안 | 옷장 화면이 열려 있는 동안 |

→ **§6.3에서 이미 만들기로 한 `DisplaySkinOverride` 가 그대로 재사용된다.** 신규 필드가 없다.

```cpp
UCLASS()
class DAERUNE_API ADRCosmeticPreviewStage : public AActor
{
    GENERATED_BODY()
public:
    ADRCosmeticPreviewStage();

    // 레벨에서 찾기. 없으면 nullptr — ★위젯은 2D 아이콘 그리드로 폴백한다★
    // (레벨에 액터를 안 놓았다고 옷장 기능 전체가 죽으면 안 된다)
    static ADRCosmeticPreviewStage* Find(const UObject* WorldContext);

    // 이 로봇을 프리뷰에 세운다. 같은 클래스면 재스폰하지 않는다.
    void SetPreviewClass(EPlayerCharacterClass CharacterClass);

    // ★실시간 옷 갈아입히기★ — 순수 로컬이라 서버 왕복이 0이다
    void SetPreviewSkin(FName SkinId);

    // 드래그 회전 (턴테이블)
    void AddPreviewYaw(float DeltaYaw);

    // 화면 열림/닫힘에 맞춰 캡처를 켜고 끈다 (성능)
    void SetCaptureActive(bool bActive);

    UFUNCTION(BlueprintPure, Category = "Cosmetic")
    UTextureRenderTarget2D* GetRenderTarget() const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> PreviewSpawnPoint;      // 캐릭터가 설 자리

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

    // BP(BP_CosmeticPreviewStage)에서 RT_CosmeticPreview 지정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    TObjectPtr<UTextureRenderTarget2D> RenderTarget;

    // 배경판 — ShowOnlyList 에 함께 넣어 투명 합성 없이 불투명으로 처리한다
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    TArray<TObjectPtr<AActor>> BackdropActors;

private:
    void DestroyPreviewCharacter();

    UPROPERTY()
    TObjectPtr<ADRCharacter> PreviewCharacter;

    EPlayerCharacterClass CurrentPreviewClass = EPlayerCharacterClass::Count;
};
```

#### 5.8.2 구현 규칙 (전부 사고 방지용)

```cpp
void ADRCosmeticPreviewStage::SetPreviewClass(EPlayerCharacterClass CharacterClass)
{
    // ★데디케이티드 서버에서는 절대 스폰하지 않는다★ (프리뷰는 순수 로컬 UI다)
    if (GetNetMode() == NM_DedicatedServer) return;
    if (PreviewCharacter && CurrentPreviewClass == CharacterClass) return;

    DestroyPreviewCharacter();

    UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this);
    if (!ClassInfo) return;
    TSubclassOf<ADRCharacter>* BPClassPtr = ClassInfo->CharacterBPClasses.Find(CharacterClass);
    if (!BPClassPtr || !*BPClassPtr) return;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    PreviewCharacter = GetWorld()->SpawnActor<ADRCharacter>(
        *BPClassPtr, PreviewSpawnPoint->GetComponentTransform(), Params);
    if (!PreviewCharacter) return;

    // ★로컬 전용★ — 켜두면 모든 클라가 서로의 프리뷰를 스폰하게 된다
    PreviewCharacter->SetReplicates(false);
    PreviewCharacter->SetReplicateMovement(false);

    if (UCharacterMovementComponent* CMC =
        Cast<UCharacterMovementComponent>(PreviewCharacter->GetMovementComponent()))
    {
        CMC->StopMovementImmediately();
        CMC->DisableMovement();
    }

    // ★3인칭 메시를 보여주고 1인칭 메시/오버헤드 닉네임을 숨긴다★
    //   기본 UpdateMeshVisibility() 는 "비로컬 = TP 표시" 라 대체로 맞지만,
    //   오버헤드 위젯까지 한 번에 처리해 주는 이 함수를 쓰는 편이 안전하다 (DRCharacter.cpp:557-605)
    PreviewCharacter->SetWaitingRoomVisibility(true);

    // 캡처 격리 — 이 액터들만 렌더한다
    CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    CaptureComponent->ShowOnlyActors = BackdropActors;
    CaptureComponent->ShowOnlyActors.Add(PreviewCharacter);

    CurrentPreviewClass = CharacterClass;
}

void ADRCosmeticPreviewStage::SetPreviewSkin(FName SkinId)
{
    if (!PreviewCharacter) return;

    // §6.3 에서 만든 필드를 그대로 쓴다. 로컬이라 복제가 관여하지 않는다.
    PreviewCharacter->SetDisplaySkinOverride(SkinId);
    PreviewCharacter->RefreshSkinVisuals();
}
```

**설계 제약과 이유**

| 규칙 | 이유 |
|---|---|
| **스테이지 액터는 맵 밖에 배치** (예: 로비 아래 `Z = -20000`) | `PRM_UseShowOnlyList` 는 **캡처만** 격리한다. 캐릭터는 일반 뷰에도 그대로 보이므로 물리적으로 치워야 한다. (`bHiddenInGame` 을 쓰면 캡처에서도 사라진다) |
| **배경판을 두고 불투명으로 처리** | 알파 합성(`SCS_SceneColorHDR` + Translucent)은 조명·톤매핑에 따라 결과가 갈린다. 배경판 + 불투명이 예측 가능하고 조명 제어도 쉽다 |
| **`bCaptureEveryFrame` 은 화면이 열려 있을 때만 true** | 상시 캡처는 로비 프레임을 그냥 깎아먹는다. `SetCaptureActive()` 를 Open/Close 에 배선한다 |
| **`EndPlay` / `CloseWardrobeScreen` 에서 프리뷰 캐릭터 파괴** | 안 지우면 레벨에 캐릭터가 쌓인다 |
| **프리뷰는 TP 메시만 보여준다** | 본인 1인칭 외형(FP)은 프리뷰로 확인할 수 없다. 팔만 보이는 화면이라 실익이 없고, TP가 "남에게 보이는 내 모습"이라 프리뷰의 목적에 맞다 |
| **스테이지 액터가 없으면 2D 폴백** | 레벨 배치를 잊었다고 옷을 못 갈아입으면 안 된다 |

**FreeRoam 에서는 로봇을 바꿀 수 없으므로**(§1.4) 화면이 열려 있는 동안 프리뷰 클래스는 고정이다.
`SetPreviewClass` 는 화면 열 때 1회만 호출된다 — 프레임마다 클래스 비교를 돌릴 필요가 없다.

#### 5.8.3 위젯 배선 — `UDRCosmeticPreviewWidget` (구현 완료)

프리뷰는 **전용 위젯**이 담당한다. 드래그 회전·MID 생성·스테이지 탐색이 한 곳에 모여 있고,
옷장 화면은 "장착이 바뀌었다"만 알려주면 된다.

```
WBP_CosmeticPreview  (Parent: UDRCosmeticPreviewWidget)
└── Img_Preview  [Image]   Brush.Material ← OnPreviewReady(Material) 에서 대입
```

| 항목 | 값 |
|---|---|
| `PreviewMaterial` | `M_CosmeticPreview` (BP 에서 지정) |
| `PreviewTextureParameter` | 기본 `"Tex"` — 머티리얼의 텍스처 파라미터 이름과 맞출 것 |
| `YawPerPixel` | 기본 `0.5` — **부호를 뒤집으면 회전 방향이 반대가 된다** |

**연결**: `WBP_Wardrobe` 안에 이 위젯을 배치하고 **변수 이름을 정확히 `Preview_Character`** 로 지으면
`UDRWardrobeScreenWidget` 이 `BindWidgetOptional` 로 자동 연결한다. 이름이 다르거나 없으면
**프리뷰만 빠지고 나머지는 정상 동작**한다.

> ⚠️ **`Img_Preview` 와 루트의 Visibility 를 `Visible` 로 둘 것.**
> `SelfHitTestInvisible` 이면 드래그 입력이 위젯에 도달하지 않는다.
> (`NativeConstruct` 에서 루트는 강제로 `Visible` 로 맞춰 두었지만, 자식 Image 는 디자이너 몫이다)

**흐름**
```
화면 열림  → NativeConstruct
             → Preview_Character->InitializePreview(ViewedClass, GetEquippedSkins(ViewedClass))
                  Stage::Find → SetPreviewClass → SetPreviewSkins → SetCaptureActive(true)
                  MID 생성 → OnPreviewReady(Material) → BP 가 Img_Preview 에 물린다

칸 클릭    → TryEquip → GI->EquipSkin → OnCosmeticsChanged
             → HandleCosmeticsChanged → SyncPreview() → Stage->SetPreviewSkins(...)
             ★서버 왕복 0 — 클릭한 프레임에 반영된다 (메시 비동기 로드 한두 프레임 뒤 실제 부착)★

드래그     → NativeOnMouseButtonDown(캡처) → OnMouseMove → Stage->AddPreviewYaw(-DeltaX * YawPerPixel)
             마우스를 캡처하므로 커서가 위젯 밖으로 나가도 회전이 이어진다

화면 닫힘  → NativeDestruct → SetCaptureActive(false)   (상시 캡처는 로비 프레임을 깎아먹는다)
```

> **갱신 지점이 `HandleCosmeticsChanged` 한 곳뿐인 이유**: 칸 클릭·Clear All·치트가 전부
> `OnCosmeticsChanged` 를 지나므로, 프리뷰 갱신을 거기 한 번만 걸면 경로를 빠뜨릴 수가 없다.

#### 5.8.4 투명 배경 — `SCS_SceneColorHDR` ★

시안의 레이어 순서상 **캐릭터 뒤에 청색 글로우(`Img_CharBack`)가 비쳐 보여야 한다**(§15.3).
따라서 RenderTarget 의 배경이 **투명**해야 한다.

`ADRCosmeticPreviewStage` 는 `CaptureSource = SCS_SceneColorHDR` 로 고정돼 있다.
엔진 정의상 이 모드의 알파는 **역불투명도(Inv Opacity)** 다:

| 픽셀 | A | 머티리얼 Opacity (`1 - A`) |
|---|---|---|
| 아무 것도 없음 | 1 | 0 (투명) |
| 캐릭터 | 0 | 1 (불투명) |

**`M_CosmeticPreview` 배선**
```
Material Domain : User Interface
Blend Mode      : Translucent
Shading Model   : Unlit

  Tex (TextureSampleParameter2D, 이름 "Tex")
    ├─ RGB ──────────────────▶ Emissive Color
    └─ A ──▶ OneMinus(1-x) ──▶ Opacity
```

**트레이드오프**: `SCS_SceneColorHDR` 은 **톤매퍼를 거치지 않아** 색이 인게임과 다를 수 있다.
스튜디오 조명을 눈으로 맞춰 흡수한다. 그래도 안 맞으면 대안은
Project Settings 의 알파 채널 전파를 켜고 `SCS_FinalColorLDR` 로 바꾸는 것인데,
**렌더링 전역 설정을 바꾸는 비용이 커서 기본값으로 택하지 않았다.**

#### 5.8.5 필요 에셋

| 에셋 | 설정 |
|---|---|
| `RT_CosmeticPreview` | `UTextureRenderTarget2D`. 권장 **512×1024** (프리뷰 영역 781×1080 의 대략 절반 — 세로형) |
| `M_CosmeticPreview` | §5.8.4 배선 |
| `WBP_CosmeticPreview` | 부모 `UDRCosmeticPreviewWidget` |
| `BP_CosmeticPreviewStage` | 부모 `ADRCosmeticPreviewStage`. **라이트를 컴포넌트로 포함**시켜야 맵 밖에서도 캐릭터가 보인다. `RenderTarget` 지정, `PreviewSpawnPoint` 와 `CaptureComponent` 로 프레이밍, `BaseYaw` 로 정면 각도 |

**배치**: `LobbyMap` 의 **맵 밖** (예: `Z = -20000`). `PRM_UseShowOnlyList` 는 캡처만 격리하지
일반 뷰를 가리지 않으므로, 맵 안에 두면 로비를 돌아다니다 프리뷰 캐릭터를 만난다.

---

## 6. 멀티플레이 처리

### 6.1 동기화 시퀀스 (정상 경로)

```
[클라 A]  옷장 상호작용 → 화면에서 Skin.Gardener.Blue 선택
             GI->EquipSkin(Gardener, "Skin.Gardener.Blue")   → 로컬 세이브 즉시 저장
          화면 닫기 → CloseWardrobeScreen() → ReportCosmeticLoadout()
             ServerReportCosmeticLoadout(Gardener, "Skin.Gardener.Blue")   [Server, Reliable]
                    │
[서버]      ForClass == PS->GetSelectedPlayerClass() 확인
            Catalog->SanitizeSkin(...)  → PS->SetEquippedSkinId(정화값)
                    │  (복제)
[전 클라]   OnRep_EquippedSkinId → RefreshCosmeticVisuals()
                    → Cast<ADRCharacter>(GetPawn())->RefreshSkinVisuals()
                    → 비동기 로드 → TP/FP/Weapon 머티리얼 적용
[리슨 호스트] SetEquippedSkinId 안에서 직접 RefreshCosmeticVisuals() 호출
              (서버에서는 RepNotify 가 자동 호출되지 않는다 — StunTagChanged:331-334 의 선례)
```

**스테이지 이동**
```
로비 FreeRoam → ADRStageSelectActor → ServerTravel(Seamless)
   ADRPlayerState::CopyProperties  → EquippedSkinId 복사          ★한 줄★
   ADRStageGameMode::BeginPlay     → 선택 클래스 BP 로 폰 스폰
   ADRPlayerController::OnLevelEntered → ReportCosmeticLoadout()  (재확인/치유)
   ADRCharacter::PossessedBy / OnRep_PlayerState → RefreshSkinVisuals()
```
→ **CopyProperties 한 줄 + OnLevelEntered 한 줄로 "입은 채로 플레이" 요구가 충족된다.**

### 6.2 늦게 들어온 클라이언트 / 순서 뒤바뀜

`EquippedSkinId` 는 복제 프로퍼티라 **join-in-progress 클라에 초기 복제로 자동 도착**한다.
문제는 "폰과 PlayerState 중 무엇이 먼저 오는가"인데, §5.4의 호출 지점 6곳이 **두 순서를 모두 흡수**한다:

| 도착 순서 | 치유 경로 |
|---|---|
| PS 먼저 → 폰 나중 | `ADRCharacter::BeginPlay` / `OnRep_PlayerState` |
| 폰 먼저 → PS 나중 | `ADRPlayerState::OnRep_EquippedSkinId` → `GetPawn()` |
| 둘 다 있는데 스킨만 나중 | `OnRep_EquippedSkinId` |

칩 시스템이 `RebuildUpgradeRuntime()` 주석(`DRPlayerState.cpp:139-140`)에서
"보고가 스폰보다 늦게 도착해도 자동으로 치유되는 경로"라고 부른 것과 동일한 구조다.

### 6.3 대기실 디스플레이 캐릭터 ★ (가장 까다로운 경로)

**문제**: `SpawnDisplayCharacter()` (`DRLobbyGameMode.cpp:686-720`)가 만드는 `ADRCharacter` 는
**비점유(unpossessed)** 다. `GetPlayerState()` 가 `nullptr` 이라 §6.1 경로를 쓸 수 없다.

**해결**: 액터 자체에 복제 필드를 둔다.

```cpp
// ADRLobbyGameMode::SpawnDisplayCharacter — 719줄(DisplayCharacterMap.Add) 직전에 추가
if (const ADRPlayerState* PS = Player->GetPlayerState<ADRPlayerState>())
{
    Display->SetDisplaySkinOverride(PS->GetEquippedSkinId());
}
```

**타이밍 위험 ★**: 서버는 `PostLogin` 후 **0.5초 타이머**로 디스플레이 캐릭터를 스폰한다
(`DRLobbyGameMode.cpp:40-63`). 클라의 최초 보고는 `ReceivedPlayer → OnLevelEntered` 에서 나간다.
0.5초면 대개 충분하지만 **보장되지 않는다.**

→ **재푸시 경로를 반드시 둔다.** `ADRPlayerState::SetEquippedSkinId()`(서버)가 끝에서:
```cpp
if (ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>())
{
    LobbyGM->RefreshDisplayCharacterSkin(Cast<AController>(GetOwner()));
}
```
`ADRLobbyGameMode::RefreshDisplayCharacterSkin(AController*)` 신규 —
`DisplayCharacterMap` 에서 찾아 `SetDisplaySkinOverride()` 를 다시 호출한다.
(`SetSelectedPlayerClass` 가 `RespawnPlayerWithClass` 를 부르는 것과 같은 방향의 의존이라 새로운 결합이 아니다.)

클래스 변경 시에는 `UpdateDisplayCharacter()`(`:722-731`)가 캐릭터를 재스폰하므로
그 안의 `SpawnDisplayCharacter` 가 새 스킨을 다시 싣는다 — 별도 처리 불필요.

### 6.4 리슨 서버

- 호스트의 `SetEquippedSkinId` 는 **RepNotify 가 자동 호출되지 않는다.** 서버 경로에서 직접
  `RefreshCosmeticVisuals()` 를 부른다 (`StunTagChanged` 가 `HasAuthority()` 분기로 `OnRep_Stunned()` 를
  직접 부르는 것과 같은 처리 — `DRCharacterBase.cpp:330-334`).
- 호스트도 로컬 세이브를 갖는다. `ReportCosmeticLoadout` 은 호스트에서도 그대로 동작한다
  (서버 RPC가 로컬 호출로 처리됨).

---

## 7. UI 설계

### 7.1 옷장 프롬프트 (월드)

- `InteractionWidget` (Screen space, 기본 숨김) — 범위 진입 시 표시.
- `OnLocalPlayerEnteredRange(bHasAnySkin)` 으로 BP가 문구를 고른다: "[F] 옷 갈아입기".
- 옷장 문이 열리는 애니메이션 등 연출은 전부 BP.

### 7.2 옷장 화면 (`WBP_Wardrobe`)

- **현재 로봇 1종의 스킨만** 보여준다 (`GetViewedClass()`). 로봇 전환은 대기실에서만 가능하므로
  탭 UI가 필요 없다 — 업그레이드 화면과 같은 판단이다.
- 그리드 카드 = `FDRSkinViewModel` 1개. `GetSkinViewModels()` **단일 진입점**으로 받는다.
- 잠긴 항목: 흑백 + 자물쇠 + 호버 시 `UnlockHint` 툴팁.
- "기본" 카드(`SkinId = NAME_None`)를 항상 첫 칸에 둬서 되돌리기가 가능하게 한다.
- ESC / 닫기 버튼 → `RequestClose()` → `PC->CloseWardrobeScreen()` → 서버 반영.

> ★**아트 시안 반영(2026-08-25) — 이 절의 스케치는 §15 로 대체되었다.**★
> 실제 배치·좌표·에셋·위젯 트리는 **§15 `WBP_Wardrobe` 제작 사양**이 확정판이다.
> 시안이 **좌 패널(탭+그리드) / 우 캐릭터** 구성이고 **카테고리가 4개(HEAD·FACE·BODY·TAIL)** 라는 점이
> 데이터 모델까지 바꾼다 — **§15.11 을 반드시 함께 읽을 것.**

**레이아웃**: 좌측 패널(탭 + 스킨 그리드), 우측 **3D 프리뷰**. (아래는 초기 스케치 — 확정판은 §15.5)

```
┌──────────────────────────────┬──────────────────────────────┐
│ WARDROBE                     │                              │
│ ┌HEAD┬FACE┬BODY┬TAIL┐        │                              │
│ ├────┴────┴────┴────┴──────┐ │        [3D 프리뷰]           │
│ │ ┌────┐┌────┐┌────┐       │ │        드래그로 회전         │
│ │ │기본││ ✔  ││ 🔒 │       │ │                              │
│ │ └────┘└────┘└────┘       │ │      RT_CosmeticPreview      │
│ │ ┌────┐┌────┐┌────┐  ▲    │ │      (뒤에 캐릭터 백글로우)  │
│ │ │    ││    ││    │  ║스크│ │                              │
│ │ └────┘└────┘└────┘  ▼롤  │ │                              │
│ └──────────────────────────┘ │                              │
│                        [ C ] │                              │
└──────────────────────────────┴──────────────────────────────┘
   좌: 패널 x151..971            우: 캐릭터 x938..1719  (1080p 기준)
```

- **3D 프리뷰는 필수다** (결정 4). 구현은 §5.8. 본인 시점은 FP 메시만 보이므로(§1.7)
  프리뷰가 없으면 **자기가 고른 옷을 확인할 방법이 사실상 없다.**
- 카드를 고르는 순간 프리뷰가 **서버 왕복 없이 그 프레임에** 갈아입는다 (§5.8.3).
- 스테이지 액터가 레벨에 없으면 프리뷰 영역만 숨기고 그리드는 그대로 동작한다 (§5.8.2 폴백).

### 7.3 업적 목록 화면 (선택)

`UDRProgressionConfig::Achievements` (`DRProgressionConfig.h:112-113`) 순회 →
`SaveGame.EarnedAchievements.Contains()` 로 달성 표시. 스킨 해금 조건과 함께 보여주면 동기가 명확해진다.

### 7.4 해금 토스트

- `UDRGameInstance::OnSkinUnlocked` 구독 → "새 옷 해금!" 토스트.
- **스테이지 결과창과 묶는 것이 자연스럽다** — `LastStageRewardResult.Lines` 를 그리는 자리에서
  같은 프레임에 처리된다 (`Client_GrantStageReward` 가 `ApplyStageReward` 를 먼저 끝내고 결과창이
  그 결과를 읽는 순서가 이미 보장돼 있다 — `DRPlayerController.cpp:1407`).

### 7.5 현지화 (Plan4 연동) ★

**Gather 경로에 진행도 폴더를 추가한다.** `Config/Localization/Game_Gather.ini` 의 `[GatherTextStep1]`:
```ini
IncludePathFilters=Content/Blueprints/UI/*
IncludePathFilters=Content/Blueprints/Phase/Data/*
IncludePathFilters=Content/Blueprints/AbilitySystem/Data/*
+IncludePathFilters=Content/Blueprints/Progression/*     ; ← 추가
IncludePathFilters=Plugins/MultiplayerSessions/Content/*
```
- 이 한 줄로 **`DA_CosmeticCatalog` 의 `DisplayName`/`Description`/`HowToUnlock` 과
  기존 `DA_ProgressionConfig` 의 업적 문구가 동시에 수집된다** (§1.9 — 지금은 둘 다 누락 상태).
- 신규 C++ 문자열은 처음부터 `LOCTEXT`/`NSLOCTEXT`.
- **M1에서 DataAsset을 만들 때 이 설정을 먼저 넣어라.** 나중에 넣으면 문구를 전부 다시 입력해야 한다.

---

## 8. 스팀 백엔드 준비 (게임 외 작업)

### 8.1 도전과제 개수의 실제 비용 ★ (결정 3)

**개수는 제약이 아니다.** 개당 엔지니어링 비용은 사실상 0 — `DA_ProgressionConfig` 에 데이터 한 줄과
`GrantStageAchievement(Id, PC)` 호출 한 줄이 전부이고, 한 번 등록하면 이후 손댈 일이 없다.
개수에 비례해 실제로 늘어나는 것은 아래 넷뿐이다.

| 항목 | 성격 | 완화책 |
|---|---|---|
| **아이콘 2장/개** (잠금·해제, 256×256) | **아트**. 유일하게 확실히 선형으로 느는 비용. 20개 = 40장 | 스킨 아이콘과 톤을 맞춰 템플릿화 |
| **문구가 두 곳에 존재** | 게임 안 `DA_ProgressionConfig` 의 FText(한/영, §7.5 파이프라인) ↔ **스팀 파트너 사이트의 언어별 표시명·설명**. 자동 동기화 수단이 없어 **개수만큼 어긋날 여지가 는다** | 파트너 사이트 입력을 M6 에 **한 번에 몰아서** 한다. 게임 쪽 FText 를 원본으로 삼고 복사해 넣는다 |
| **출시 후 삭제가 비가역** | 이미 달성한 플레이어가 있으면 삭제 시 그들 프로필에서 사라지고 전역 달성률 통계가 깨진다. Valve 도 권장하지 않는다 | **출시 전에는 자유롭게 편집·삭제 가능.** → 등록을 늦추는 것으로 완전히 회피된다 |
| **API Name 오타** | AppId 480 환경에서는 검증이 불가능해 **실 AppId 빌드에서만 드러난다** | §8.2 의 에디터 검증기로 로컬에서 잡는다 |

> **실행 규칙: "적게 만들어라"가 아니라 "ID와 문구가 확정되기 전에는 파트너 사이트에 등록하지 마라".**
> 게임 쪽 업적 정의(`DA_ProgressionConfig`)는 **지금부터 마음껏 늘려도 된다.**
> `SteamApiName` 을 비워 두면 미러링 대상에서 빠지므로(§4.3), 로컬 전용으로 운용하다가
> **M6 에서 최종본만 골라 한 번에 등록**하면 위 비용 대부분이 사라진다.

### 8.2 에디터 검증기 (오타를 실 AppId 없이 잡는다)

`UDRProgressionConfig::PostEditChangeProperty`(`DRProgressionConfig.h:156`)가 이미 예산 검증을 돌리고 있다.
같은 자리에 추가한다:

- `SteamApiName` **중복** 검사 (두 업적이 같은 스팀 키를 가리키면 하나가 조용히 덮인다)
- 스팀 API Name 문자 규칙 위반 검사 — 영숫자와 `_` 만, 공백·한글 불가
- `AchievementId` 중복 검사
- `UDRCosmeticCatalog::ValidateAgainst` — 스킨의 `RequiredAchievements` 가 실재하는 업적인지 (§4.4)

### 8.3 파트너 사이트 작업 (M6)

1. **실 AppId 발급** → `Config/DefaultEngine.ini:115` 의 `SteamDevAppId=480` 교체
   + **빌드 디렉터리에 `steam_appid.txt` 배치** (현재 프로젝트에 없음 — §1.8).
2. **Achievements 정의**: API Name(영문 키), 언어별 표시명·설명, 아이콘 2종 등록.
   API Name 은 `FDRAchievementDef::SteamApiName` 과 **문자 단위로 일치**해야 한다.
3. **Stats**: 1차 범위는 즉시형만 쓰므로 불필요. 진행형("N킬")을 쓰려면 누적 stat(INT) 등록 + 연결이 필요한데,
   **현재 로컬 누적 통계가 없다**(§4.4) — 서버 판정형 업적으로 대체하는 편이 훨씬 싸다.
4. **Steam Cloud = Auto-Cloud 로 확정.** UE SaveGame 경로(`Saved/SaveGames/`)를 등록하면 **코드 변경 0**.
   **단, `DaeRunePlayerProgress_*.sav` 만 등록하고 레거시 `DaeRunePlayerProgress.sav` 는 제외한다 — §4.6.4.**
   `ISteamRemoteStorage` 직접 사용은 검토하지 않는다.
5. Steamworks SDK 포함 확인 — `OnlineSubsystemSteam` 플러그인이 래핑하며 `.uproject` 에 이미 활성.

> 개발 중에는 Spacewar(480)로 **인터페이스 호출 경로까지만** 확인 가능하다.
> 토스트/영속 검증은 실 AppId 가 필요하다. → **M1~M5 를 로컬만으로 완주할 수 있게 설계했다.**

---

## 9. 폴백 & 엣지 케이스

**대원칙: 해금은 항상 합집합(union). 어떤 경로로도 회수하지 않는다.**

| 상황 | 처리 |
|---|---|
| 스팀 미실행/오프라인 (개발 중 기본) | `IsBackendAvailable()==false` → 스팀 호출 전부 no-op. 업적·해금·옷장은 SaveGame만으로 완전 동작. 달성분은 `PendingSteamAchievements` 큐잉 |
| 온라인 복귀 | `FlushPending()` → 큐를 스팀에 write 후 비움 (스팀 write 는 멱등이라 중복 안전) |
| 재설치 — 스팀엔 있고 로컬엔 없음 | `QueryFromBackend` → `ApiNameToId` 역매핑 → `EarnedAchievements` 에 **추가** → 해금 재파생. **`ClaimedRewards` 는 건드리지 않는다**(재화 이중지급/영구차단 방지 — §4.5) |
| 오프라인 달성 — 로컬엔 있고 스팀엔 없음 | backfill write. **로컬 기록은 지우지 않는다** |
| `StageClear.*` 조건 스킨 + 재설치 | **복구되지 않는다.** 스팀 미러링 대상이 아니다 → 콘텐츠 규칙으로 회피(§4.5 말미) |
| **스팀 계정 전환 (같은 PC)** | **슬롯이 `_<SteamId>` 로 갈리므로 진행도가 섞이지 않는다** (§4.6) |
| **스팀 미실행 상태로 실행** | `LastAccountKey`(GameUserSettings.ini)로 **마지막 계정의 슬롯을 이어서 쓴다.** 이게 없으면 오프라인 실행마다 진행도가 갈린다 (§4.6.2) |
| **스팀 미실행 + `LastAccountKey` 도 없음** (완전 초회) | 레거시 슬롯 `DaeRunePlayerProgress` 로 동작. 이후 스팀이 켜진 채 실행되면 그 시점에 계정 슬롯으로 1회 이관된다 |
| **한 PC·여러 계정 + 스팀 미실행** | 전부 `LastAccountKey` 를 공유해 **섞인다.** 스팀이 꺼진 상태에서는 계정을 구분할 수단이 없다 — 알려진 한계로 수용한다 |
| **레거시 이관 후 롤백 필요** | 레거시 `.sav` 를 **지우지 않으므로** 파일을 되돌리면 복구된다 (§4.6.2) |
| `Init()` 에서 SteamID 해석 실패 | 폴백 체인(`SteamID → LastAccountKey → 레거시`)이 항상 유효한 슬롯을 준다. 게임이 멈추지 않는다. 상시 실패하면 §4.6.3 지연 확정으로 전환 |
| 카탈로그에서 스킨 삭제 | 부팅 시 정화 → `NAME_None`(기본 외형). 서버도 `SanitizeSkin` 으로 한 번 더 거른다 |
| 프리뷰 스테이지 액터가 레벨에 없음 | 프리뷰 영역만 숨기고 **그리드는 정상 동작** (§5.8.2) |
| 프리뷰 캐릭터가 월드에 보임 | 스테이지를 **맵 밖에 배치**한다. `PRM_UseShowOnlyList` 는 캡처만 격리하지 일반 뷰를 가리지 않는다 (§5.8.2) |
| 프리뷰 캐릭터가 다른 클라에 스폰됨 | `SetReplicates(false)` + 데디 서버 가드. **로컬 전용이 아니면 인원수만큼 캐릭터가 생긴다** (§5.8.2) |
| 카탈로그 미지정 | 스킨 목록이 비고 옷장은 "기본"만 표시. **게임이 잠기지 않는다** |
| 비동기 로드 중 사망/재스폰/재선택 | 콜백에서 `IsValid(this)` + `PendingSkinId` 일치 재확인 후 적용 (§5.4) |
| **사망 → 부활** | Dissolve 가 슬롯 0 을 덮어씀 → `MulticastHandleRevive_Implementation` 에서 **재적용 필수** (§1.7) |
| 대기실 디스플레이 캐릭터 | 비점유라 PS 없음 → `DisplaySkinOverride` 복제 + 서버 재푸시 (§6.3) |
| 클래스 변경 (대기실) | 서버가 `EquippedSkinId` 리셋 → 클라가 `OnRep_SelectedPlayerClass` 에서 새 클래스 스킨 재보고 (§5.2) |
| 스킨 Id rename | **금지.** `SkinId` 는 세이브에 저장된다. 부득이하면 카탈로그에 구 Id 를 별칭으로 남기고 정화 시 매핑 |
| `EPlayerCharacterClass` 값 추가 | **`Count` 직전에만.** `EquippedSkins` 의 TMap 키로 직렬화된다 (§1.3) |
| **SaveGame 필드 추가** | **`CurrentSaveVersion` 을 올리지 말 것.** 올리면 재화·업그레이드 전소 (§4.2) |

---

## 10. 구현 단계 (마일스톤)

> **1차 개정의 "Plan2 M4 선행" 제약은 해소됐다** (§0.2). 다만 아래 M0 이 실질적 선행 조건이다.

### M0 — 업적 데이터 채우기 (선행, 소규모)

파이프라인은 완성이지만 **흐르는 데이터가 없다** (§1.2 말미). 검증할 소스부터 만든다.

- [ ] `DA_ProgressionConfig` 의 `Achievements` 배열에 스킨 해금용 업적 3~5개 정의
      (예: `Stage1_Clear`, `Stage1_NoDamage`, `Stage2_Clear`)
- [ ] 각 판정 지점에서 `ADRStageGameMode::GrantStageAchievement(Id, PC)` 호출
      (C++ 페이즈 또는 `BP_DRStageGameMode` — BlueprintCallable 로 이미 열려 있음)
- ✅ 검증: 스테이지 클리어 후 로그에 `[Progression] 스테이지 보상 반영: +N` 이 뜨고
      `ClaimedRewards` 에 Id 가 남는다

### M1 — 세이브 슬롯 계정 키잉 (결정 2) ★단독 착지★

**다른 작업과 섞지 말 것.** 모든 테스터의 진행도 파일을 건드리는 변경이라,
회귀가 나면 원인이 명확해야 한다. 커밋 하나로 끝나는 크기다 (호출부 3곳).

- [ ] `UDRSaveGame`: `SaveSlotName` → `BaseSlotName` 개명 + `MakeSlotName()` 추가 (§4.6.1)
- [ ] `UDRGameUserSettings::LastAccountKey` 필드 추가
- [ ] `UDRGameInstance::ResolveSteamAccountKey()` / `ResolveActiveSaveSlot()` + `ActiveSlotName`
- [ ] `LoadProgress` / `SaveProgress` 를 `ActiveSlotName` 기준으로 교체 (3곳)
- [ ] `Init()` 에서 `LoadProgress()` **앞에** `ResolveActiveSaveSlot()` 배치
- [ ] 레거시 → 계정 슬롯 1회 이관 (**레거시 파일 삭제 금지**)
- ✅ 검증 4종:
      ① **스팀 켠 채 실행** → 로그에 계정 슬롯명이 찍히고 `Saved/SaveGames/` 에 `_<SteamId>.sav` 생성.
         **기존 재화·슬롯·칩이 전부 살아 있어야 한다**
      ② **스팀 끈 채 실행** → 같은 슬롯을 이어서 씀 (`LastAccountKey` 경로)
      ③ 다른 스팀 계정으로 실행 → 새 슬롯, 진행도가 0에서 시작
      ④ `Init()` 시점 SteamID 해석 성공 여부를 **로그로 명시 확인** (§4.6.1 경고)

### M2 — 코스메틱 데이터 토대 (게임플레이 무관, 순수 추가)

> ⚠️ **착수 전 §15.11 결정 필요**: 아트 시안의 4개 카테고리(HEAD·FACE·BODY·TAIL)가
> **(A) 부위별 머티리얼 리컬러**인지 **(B) 부착물 메시**인지에 따라 아래 자료구조가 갈린다.
> 어느 쪽이든 **로봇당 스킨 1개 → 4개**로 바뀌므로, §4.2·§4.4·§5.1·§5.2·§5.4 는 **§15.11 개정안**을 따른다.

- [ ] `EDRCosmeticCategory` enum + `FDRSkinDefinition::Category` (§15.11)
- [ ] `UDRSaveGame` 에 필드 3개 추가 (`Cosmetics` 는 카테고리별) — **`CurrentSaveVersion` 은 4 그대로** (§4.2)
- [ ] `EnsureProgressInitialized()` 에 `Cosmetics` lazy 초기화 + 스킨 정화
- [ ] `FDRAchievementDef` 에 `SteamApiName` 컬럼 추가
- [ ] `DRCosmeticTypes.h` — `FDRSkinDefinition`, `FDRSkinViewModel`
- [ ] `UDRCosmeticCatalog` + `UDRProgressionConfig::CosmeticCatalog` 참조
- [ ] `UDRGameInstance` 코스메틱 API (`IsSkinUnlocked` / `EquipSkin` / `GetEquippedSkin` /
      `GetSkinViewModels` / `HasAnyUnlockedSkin` / `DebugGrantAchievement` + 델리게이트 2종)
- [ ] `ApplyStageReward` 훅 2곳 (`EarnedAchievements.AddUnique` + 신규 해금 브로드캐스트) (§5.1)
- [ ] **에디터 검증기** — `SteamApiName` 중복·문자규칙, `AchievementId` 중복, 스킨 조건 실재 여부 (§8.2)
- [ ] **`Game_Gather.ini` 에 `Content/Blueprints/Progression/*` 추가** (§7.5) ← 반드시 이 시점에
- [ ] `DA_CosmeticCatalog` 인스턴스 생성 (문구는 처음부터 한/영 대상으로)
- [ ] 치트 exec 3종
- ✅ 검증: `DRUnlockSkins` 후 `IsSkinUnlocked()` 가 참, 재시작 후에도 유지.
      **M1 에서 확인한 재화·슬롯·칩이 여전히 살아 있는지 재확인** (버전 사고 조기 발견)

### M3 — 외형 적용 + 멀티 동기화

- [ ] `ADRPlayerState`: `EquippedSkinId` 복제 + `SetEquippedSkinId` / `OnRep` /
      `RefreshCosmeticVisuals` / `ReportCosmeticIfLocal`
- [ ] 기존 함수 5곳 수정 (`GetLifetimeReplicatedProps` / `CopyProperties` /
      `SetSelectedPlayerClass` / `OnRep_SelectedPlayerClass` / `BeginPlay`) (§5.2)
- [ ] `ADRPlayerController`: `ReportCosmeticLoadout` + `ServerReportCosmeticLoadout`
      + 호출 지점 2곳(`OnLevelEntered`, `OnRep_PlayerState`)
- [ ] `ADRCharacter::RefreshSkinVisuals()` — FP/TP/**Weapon** 3개 대상 + 비동기 로드 + 유효성 재확인
- [ ] `ADRCharacter::DisplaySkinOverride` 복제 + `SetDisplaySkinOverride()`
      (§6.3 대기실과 §5.8 프리뷰가 **둘 다 이 필드를 쓴다** — 여기서 함께 만든다)
- [ ] 호출 지점 6곳 배선 — **부활 경로 포함** (§5.4)
- ✅ 검증: 2인 PIE에서 치트로 스킨을 바꿨을 때 **서로 다른 스킨이 양쪽 모두 정확히** 보이고,
      **사망→부활 후에도 유지**되며, **스테이지 이동 후에도 유지**된다

### M4 — 옷장 액터 + 화면 + 3D 프리뷰 ← **요구사항 완결점**

- [ ] `ADRWardrobe` (`ADRUpgradeStation` 복제) + `BP_Wardrobe`
- [ ] `ADRPlayerController::OpenWardrobeScreen` / `CloseWardrobeScreen` + ESC 우선순위 + `C` 키
- [ ] **`UDRWardrobeScreenWidget` + `WBP_Wardrobe` 외 위젯 3종 — 전체 사양은 §15, 순서는 §15.13**
- [ ] **`ADRCosmeticPreviewStage`** + `BP_CosmeticPreviewStage` + `RT_CosmeticPreview` + `M_CosmeticPreview` (§5.8)
- [ ] 프리뷰 드래그 회전 + 캡처 On/Off 배선
- [ ] `LobbyMap` — FreeRoam 구역에 옷장 배치 + **맵 밖에 프리뷰 스테이지 배치**
- ✅ 검증: 옷장에 다가가 F → 화면 → **좌측 3D 캐릭터가 뜨고, 카드를 고르면 그 자리에서 갈아입고**,
      닫으면 **내 캐릭터 외형이 바뀌고 다른 플레이어에게도 보이며, 스테이지에 그대로 입고 간다**
- ✅ 폴백 검증: 프리뷰 스테이지를 레벨에서 지워도 그리드가 정상 동작한다

### M5 — 대기실 반영 + UI 폴리시

- [ ] `ADRLobbyGameMode::SpawnDisplayCharacter` 에 `SetDisplaySkinOverride` 배선
- [ ] `RefreshDisplayCharacterSkin()` 재푸시 경로 — **0.5초 타이머 경합 대비** (§6.3)
- [ ] 해금 토스트 (결과창 연동)
- [ ] (선택) 업적 목록/진행도 화면
- ✅ 검증: 대기실 슬롯의 내 로봇이 저장된 옷을 입고 있고, 다른 플레이어에게도 그렇게 보인다

### M6 — 스팀 연동

- [ ] `UDRAchievementSubsystem` 구현 (§5.7 의 실제 API 기준)
- [ ] `ReportAchievements` / `FlushPending` / `QueryFromBackend` + `OnBackendSynced` 배선
- [ ] `UDRGameInstance::Init()` 5·6단계 추가 (§4.6.2)
- [ ] 실 AppId + `steam_appid.txt` + **파트너 사이트 도전과제 등록 (여기서 처음으로 등록)** (§8.1)
- [ ] Steam Cloud Auto-Cloud — **`DaeRunePlayerProgress_*.sav` 만** 등록 (§4.6.4)
- ✅ 검증: 실 AppId 빌드에서 도전과제 토스트가 뜨고, **세이브 삭제 후 스팀 상태만으로 스킨 해금이 복원**된다

### M7 — QA & 콘텐츠

- [ ] §9 엣지 케이스 전수 테스트 (특히 슬롯 폴백 체인 / 부활 / 대기실 / 오프라인→온라인 backfill)
- [ ] 스킨 에셋·아이콘·한영 문구 채우기
- [ ] (2차 범위 착수 판단) 메시 교체 스킨 / 모자 / 트레일
- [ ] (재검토) 캐릭터(로봇) 해금 도입 여부 — §11.3 에 필요 작업이 보존돼 있다

---

## 11. 콘텐츠 정의

### 11.1 1차 범위는 **색상 스킨(머티리얼 오버라이드)** 만

이미 진행 중인 작업이 이 방향과 맞물린다:
```
Content/DaeRuneAssets/Characters/GardenRobot_v3/FP/
    Gardener_Blue.uasset  Gardener_BlueLight.uasset  Gardener_Gray_001.uasset
Content/Blueprints/Character/Material/
    M_ViewmodelBase.uasset  MF_ViewmodelDepthCompress.uasset
```
→ **FP/TP/Weapon 3개 세트로 정리**해 두면 `FDRSkinDefinition` 에 그대로 꽂힌다.

로봇별 2~3종으로 시작. **기본 외형은 항상 무료·기본 제공**(`SkinId = NAME_None`).

### 11.2 해금 조건 예시 (콘텐츠팀 튜닝용)

| SkinId | 조건 (`RequiredAchievements`) | 스팀 미러링 |
|---|---|---|
| `Skin.Gardener.Blue` | (없음) — 기본 제공 | — |
| `Skin.Gardener.White` | `Stage1_Clear` | ✅ 필요 |
| `Skin.VendingMachine.Retro` | `Stage1_NoDamage` | ✅ 필요 |
| `Skin.RobotVacuum.Gold` | `Stage2_Clear` | ✅ 필요 |

> **재설치 후에도 살아남아야 하는 스킨은 반드시 `SteamApiName` 이 있는 업적에 건다** (§9).

### 11.3 캐릭터(로봇) 해금 — **이번 범위 제외, 단 최종 결정은 아님** (결정 1)

이번 요구는 "옷"이므로 제외했다. 다만 **나중에 도입할 수 있도록 필요 작업을 여기 보존**한다.
아래 어느 항목도 §1~§10 의 설계와 충돌하지 않는다 — 순수 추가다.

**도입 시 필요한 작업 (실측 기준)**

| # | 작업 | 위치 |
|---|---|---|
| 1 | `FDRSkinDefinition` 과 같은 형태의 `FDRCharacterUnlockDef` 정의 (`RequiredAchievements` 재사용) | `DRCosmeticTypes.h` 확장 |
| 2 | `UDRGameInstance::IsCharacterUnlocked(EPlayerCharacterClass)` — §4.5 와 **같은 파생 방식** | `DRGameInstance` |
| 3 | 클라 → 서버 해금 목록 보고. **선택이 이미 서버 권위 순환이라 서버가 알아야만 게이팅이 된다** | `ADRPlayerState` 복제 필드 + RPC |
| 4 | 순환에서 잠긴 인덱스 스킵 — 아래 코드 | `DRPlayerController.cpp:1830-1855` |
| 5 | 복원 경로 3곳에 폴백 검사 (잠긴 클래스면 `Gardener`) | `DRLobbyGameMode.cpp:149`, `DRStageGameMode.cpp:36-40`, `DRTutorialGameMode.cpp:33` |
| 6 | 대기실 UI 에 잠금 안내 (선택) | `WBP_PlayerSlot` 계열 |

**4번의 형태** (현재 `DRPlayerController.cpp:1844-1850` 의 단순 모듈로를 대체):
```cpp
int32 Index = CurrentIndex;
for (int32 Step = 0; Step < ClassCount; ++Step)
{
    Index = bNext ? (Index + 1) % ClassCount
                  : (Index - 1 + ClassCount) % ClassCount;
    if (PS->IsClassUnlocked(static_cast<EPlayerCharacterClass>(Index))) break;
}
// 전부 잠긴 병리적 상황에서도 ClassCount 회로 종료돼 무한루프가 없다
```

**안전 기본값**: 해금 규칙이 없는 클래스는 **해금된 것으로 간주**한다. 서버가 보고를 못 받았거나
빈 배열이면 **전부 해금**으로 취급한다 — 데이터 미비로 게임에 못 들어가는 사고를 막는다.
**`Gardener` 는 폴백 클래스이므로 절대 잠그지 않는다.**

**도입 전 고려**: 대기실 순환에서 로봇이 아예 사라지는 것은 신규 유저 체감이 크다.
"전부 기본 제공 + 옷만 해금"도 완결된 선택지다.

---

## 12. 리스크 & 열린 항목

### 결정 완료 (2026-08-25)

§0.1 의 표 참조. 4건 모두 확정되어 **착수를 막는 결정 게이트는 없다.**

### 남은 열린 항목 (착수를 막지 않음)

- **캐릭터(로봇) 해금** — 이번엔 제외이나 최종 결정은 아니다. 필요 작업이 §11.3 에 보존돼 있고,
  전부 순수 추가라 **나중에 붙여도 §1~§10 설계가 바뀌지 않는다.** M7 에서 재검토.
- **Plan2 와의 정합** — §4.6(슬롯 키잉)이 두 문서 공통 사항이다. **Plan2 쪽 기술도 이 절을 가리키도록
  갱신해야 한다.** 방치하면 두 문서가 다른 슬롯 규칙을 적게 된다.

### 리스크

1. **세이브 버전 사고 ★최우선★**: `CurrentSaveVersion` 을 무심코 올리면 테스터 전원의
   재화·슬롯·칩이 사라진다 (§4.2). M1·M2 검증 항목에 "기존 진행도 보존 확인"을 두 번 넣은 이유다.

2. **슬롯 키잉이 진행도를 통째로 옮긴다 ★신규★**: 슬롯명이 바뀌는 변경이라 이관 로직이 틀리면
   **모두가 빈 세이브로 시작한 것처럼 보인다.** 그래서 M1 을 단독 착지로 분리하고 레거시 파일을
   지우지 않는다 (§4.6.2). 폴백 체인은 `SteamID → LastAccountKey → 레거시` 로 항상 유효한 슬롯을 준다.

3. **`Init()` 시점 SteamID 가용성 ★검증 필요★**: §4.6.1 이 성립하지 않으면 지연 확정(§4.6.3)으로
   전환해야 하고 그쪽은 비용이 크다. **M1 착수 첫 작업이 이 로그 확인이어야 한다.**

4. **원장 분리를 지키지 않으면 재화가 영구 차단된다**: 스팀 역매핑을 `ClaimedRewards` 에 넣으면
   `DRGameInstance.cpp:1287` 의 `continue` 에 걸려 해당 업적의 재화를 영원히 못 받는다 (§4.5).

5. **부활 후 스킨 소실**: `Dissolve()` 가 머티리얼 슬롯 0을 덮어쓰고 부활 원복은 BP에 위임돼 있다 (§1.7).
   §5.4 의 6번째 호출 지점을 빼먹으면 **부활할 때마다 기본 외형으로 돌아간다.**

6. **프리뷰 캐릭터 복제 사고 ★신규★**: `SetReplicates(false)` 를 빼먹으면 **인원수만큼 프리뷰 캐릭터가
   월드에 생긴다.** 로비는 최대 인원이 모이는 곳이라 눈에 띄게 무거워진다 (§5.8.2).

7. **프리뷰 스테이지 배치 실수 ★신규★**: 맵 안에 두면 **로비를 돌아다니다 프리뷰 캐릭터를 만난다.**
   `PRM_UseShowOnlyList` 는 캡처만 격리하지 일반 뷰를 가리지 않는다 (§5.8.2).

8. **AppId**: 실 발급 전까지 스팀 실동작 검증 불가. → **M1~M5 를 로컬만으로 완주 가능하도록 설계했다.**

9. **enum 확장 호환성**: `EPlayerCharacterClass` 는 `EquippedSkins` TMap 의 키로 직렬화된다.
   **중간 삽입 금지 / `Count` 직전 추가만 허용.**

10. **머티리얼 슬롯 인덱스 가정**: `FDRSkinDefinition` 이 인덱스로 슬롯을 지정하므로
    **메시의 슬롯 순서가 바뀌면 조용히 어긋난다.** 슬롯 이름 기반(`GetMaterialIndex(FName)`)으로
    바꾸는 것이 안전하지만 데이터 입력 비용이 는다 — 캐릭터 3종 규모에서는 인덱스로 시작하고,
    메시 재작업이 잦아지면 이름 기반으로 전환한다.

11. **1차 범위를 머티리얼로 좁힌 대가**: 메시 교체형 스킨을 나중에 넣을 때 `FDRSkinDefinition` 이 확장된다.
    → 확장은 **필드 추가만** 하고 기존 필드의 의미를 바꾸지 않으면 DataAsset 재작업이 없다.

---

## 13. 영향받는 파일 요약

**신규 (C++)**
- `Public/Game/DRCosmeticTypes.h` — `FDRSkinDefinition`, `FDRSkinViewModel`
- `Public/Game/DRCosmeticCatalog.h` / `Private/Game/DRCosmeticCatalog.cpp`
- `Public/Game/DRAchievementSubsystem.h` / `Private/Game/DRAchievementSubsystem.cpp`
- `Public/Actor/DRWardrobe.h` / `Private/Actor/DRWardrobe.cpp`
- `Public/Actor/DRCosmeticPreviewStage.h` / `Private/Actor/DRCosmeticPreviewStage.cpp`
- `Public/UI/Widget/DRWardrobeScreenWidget.h` / `Private/UI/Widget/DRWardrobeScreenWidget.cpp`

**신규 (에셋)**
- `Content/Blueprints/Progression/DA_CosmeticCatalog.uasset`
- `Content/Blueprints/Actor/Lobby/BP_Wardrobe.uasset`
- `Content/Blueprints/Actor/Lobby/BP_CosmeticPreviewStage.uasset`
- `Content/Blueprints/UI/Wardrobe/WBP_Wardrobe.uasset` — **트리 사양 §15.5**
- `Content/Blueprints/UI/Wardrobe/WBP_WardrobeTab.uasset` — §15.6
- `Content/Blueprints/UI/Wardrobe/WBP_WardrobeSlot.uasset` — §15.7
- `Content/Blueprints/UI/Wardrobe/WBP_WardrobeClearAll.uasset` — §15.9
- `Content/Blueprints/UI/Wardrobe/RT_CosmeticPreview.uasset` (512×1024)
- `Content/Blueprints/UI/Wardrobe/Material/M_CosmeticPreview.uasset` (Unlit)
- `Content/Blueprints/UI/Wardrobe/Material/M_UI_Additive.uasset` + 인스턴스 3종 — §15.4
- `Content/DaeRuneAssets/UI/Wardrobe/` — **텍스처 10종** (§15.1 실측표, 임포트 설정 §15.10)

**수정**
| 파일 | 내용 | 마일스톤 |
|---|---|---|
| `Public/Game/DRSaveGame.h` / `.cpp` | `SaveSlotName` → `BaseSlotName` 개명 + `MakeSlotName()` | **M1** |
| `Public/Game/DRGameUserSettings.h` | `LastAccountKey` 필드 | **M1** |
| `Public/Game/DRSaveGame.h` | 코스메틱 필드 3개 추가 (**버전 유지**) | M2 |
| `Public/Game/DRProgressionConfig.h` / `.cpp` | `SteamApiName` 컬럼, `CosmeticCatalog` 참조, 검증기 | M2 |
| `Public/Game/DRGameInstance.h` / `.cpp` | 슬롯 해석·이관(M1), 코스메틱 API·`ApplyStageReward` 훅(M2) | M1·M2 |
| `Public/Player/DRPlayerState.h` / `.cpp` | `EquippedSkinId` 복제 + 기존 함수 5곳 | M3 |
| `Public/Player/DRPlayerController.h` / `.cpp` | 보고 RPC, 옷장 화면, ESC 분기, 치트 3종 | M3·M4 |
| `Public/Character/DRCharacter.h` / `.cpp` | `RefreshSkinVisuals`, `DisplaySkinOverride`, 호출 지점 6곳 | M3 |
| `Private/Game/DRLobbyGameMode.cpp` | `SpawnDisplayCharacter` 배선 + `RefreshDisplayCharacterSkin` 신규 | M5 |
| `Config/Localization/Game_Gather.ini` | **`Content/Blueprints/Progression/*` 추가** | **M2** |
| `Content/Maps/LobbyMap.umap` | `BP_Wardrobe`(FreeRoam) + `BP_CosmeticPreviewStage`(**맵 밖**) 배치 | M4 |
| `Config/DefaultEngine.ini` | 실 AppId | M6 |
| **`Plan2.md`** | §4.6 슬롯 키잉 결론을 가리키도록 갱신 | M1 |

**수정하지 않는 것** (1차 개정 대비)
- `Public/DRGameplayTags.h` / `.cpp` — 태그를 추가하지 않는다 (§4.1)
- `Config/DefaultGameplayTags.ini` — 리다이렉터 불필요
- `ADRStageGameMode` / `ADRPlayerController::Client_GrantStageReward` — 판정·지급 파이프라인 무수정

---

## 14. 다음 행동

**결정 4건이 모두 확정되어 착수를 막는 게이트가 없다.** 순서는 다음과 같다.

1. **가장 먼저 — `Init()` 시점 SteamID 해석 확인** (리스크 3).
   `UDRGameInstance::Init()` 에 로그 한 줄을 넣고 스팀 켠 채 실행해 SteamID 가 찍히는지 본다.
   여기서 §4.6.1 이 성립하면 M1 전체가 예정대로 간다. 실패하면 §4.6.3 으로 설계를 틀어야 하므로
   **다른 코드를 쓰기 전에 이것부터 확인한다.**

2. **M1 (세이브 슬롯 키잉) 단독 착지** — 진행도 파일 전체를 건드리므로 다른 변경과 섞지 않는다.
   검증 4종을 통과한 커밋을 따로 남긴다. 동시에 **Plan2 의 같은 항목을 §4.6 을 가리키도록 갱신**한다.

3. **M0 (업적 데이터 채우기) 병행 착수** — M1 과 파일이 겹치지 않아 동시 진행이 가능하다.
   `GrantStageAchievement()` 호출이 실제로 흐르지 않으면 M2 이후를 검증할 데이터가 없다.

4. **M2 착수** — 첫 커밋에 **`Game_Gather.ini` 한 줄**과 **`CurrentSaveVersion` 유지 확인**을 같이 넣는다.
   스팀 등록은 M6 이므로, 업적 정의(`DA_ProgressionConfig`)는 이 시점부터 자유롭게 늘려도 된다 (§8.1).

5. **M3 완료 시점에 회귀 시나리오를 고정한다** — **2인 PIE + 사망/부활 + 스테이지 왕복 + 재접속**.
   이후 마일스톤마다 재실행한다. 동기화 버그는 늦게 발견될수록 원인 추적이 어렵다.

6. **M4 가 요구사항 완결점**이다. 여기까지가 "로비 옷장에서 옷을 골라 입고 그대로 플레이"의 전부이며,
   **실 AppId 없이 로컬만으로 완주할 수 있다.** M5~M6 는 그 위의 폴리시와 계정 이식성이다.

---

# 15. `WBP_Wardrobe` 제작 사양 (에셋 실측 기반)

> **작성일** 2026-08-25 · **근거** 아트팀 제공 PNG 12종 실측(§15.1) + 프로젝트 설정 실측
> **범위** 옷장 화면 UMG 위젯 트리 · 좌표 · 텍스처 설정 · 상태 머신 · 블렌드 모드 변환
> **비범위** C++ 로직(§5.6), 3D 프리뷰 스테이지(§5.8) — 이 절은 **그리는 쪽만** 다룬다
>
> §7.2 의 레이아웃 스케치를 제작 가능한 수준으로 확정한 문서다. 충돌하면 **이 절이 우선**한다.

## 15.0 이 절을 읽는 법

- **"실측"** 은 PNG 픽셀에서 직접 잰 값이다. 그대로 입력하면 된다.
- **"추정 ±N"** 은 소프트 엣지 때문에 오차가 있다. 에디터에서 조립 시안을 겹쳐 눈으로 맞춘다.
- **"미확정"** 은 시안에 정보가 없어 결정이 필요한 항목이다. 전부 §15.12 에 모아 뒀다.

## 15.1 에셋 실측표

전부 RGBA PNG. **알파 최대값이 낮은 에셋이 여럿 있다** — 텍스처 압축 설정을 잘못 잡으면 뭉개진다(§15.10).

| 원본 파일 | 크기 | 알파 범위 | PS 블렌드 | 용도 | 제안 에셋명 |
|---|---:|---|---|---|---|
| `1차검은배경` | 1920x1080 | **155 균일** | Normal | 전체 화면 딤 (**순수 #000000 @ a61%**) | **텍스처 불필요** — 컬러 브러시(§15.4) |
| `2차배경` | 1601x901 | 0-254 | **Soft Light** | 중앙 하늘색 라디얼 글로우 | `T_Wardrobe_BGGlow` |
| `캐릭터 back` | 868x1076 | 0-**136** | **Lighter Color 90%** | 캐릭터 뒤 달걀형 청색 글로우 | `T_Wardrobe_CharBack` |
| `옷장` | 820x839 | 0-255 | Normal | 좌측 패널 프레임 (**글로우 여백 50px**) | `T_Wardrobe_PanelFrame` |
| `카테고리바` | 496x44 | 0-**102** | Normal | 탭 바 배경 | `T_Wardrobe_TabBar` |
| `카테고리 선택박스` | 124x44 | 0-209 | Normal | 활성 탭 필 | `T_Wardrobe_TabPill` |
| `속성 1=기본` | 216x252 | 0-179 | Normal | 칸 — 기본 | `T_Wardrobe_Slot_Normal` |
| `속성 1=호버` | 216x252 | 0-250 | Normal | 칸 — 호버 | `T_Wardrobe_Slot_Hovered` |
| `속성 1=선택` | 216x252 | 0-246 | Normal | 칸 — 선택 (**체크 마크 각인됨**) | `T_Wardrobe_Slot_Selected` |
| `clear all` | 112x117 | 0-255 | Normal | Clear All 버튼 (`C` 키 힌트) | `T_Wardrobe_ClearAll` |
| `화면효과` | 1753x993 | 0-**48** | **Soft Light** | 최상단 스캔라인 비네트 | `T_Wardrobe_ScreenFX` |
| `조립(배경없음)` | 2128x1198 | — | — | **참고용 합성 시안** (임포트 안 함) | — |
| `image_1` | 1018x1005 | — | — | **참고용 칸 배치 시안** (임포트 안 함) | — |

**임포트 위치** (프로젝트 관례 실측):
- 텍스처 → `Content/DaeRuneAssets/UI/Wardrobe/`  (`DaeRuneAssets/UI/Lobby/` 선례)
- 위젯 → `Content/Blueprints/UI/Wardrobe/`  (`Blueprints/UI/Upgrade/` 선례)
- 머티리얼 → `Content/Blueprints/UI/Wardrobe/Material/`  (`UI/Lobby/Material/M_UI_radialGradient2` 선례)

## 15.2 디자인 해상도 — 1920x1080, 에셋 1:1 (실측 확인) ★

**프로젝트 설정이 이미 이 캔버스에 맞춰져 있다** (`Config/DefaultEngine.ini:83-98`):
```ini
[/Script/Engine.UserInterfaceSettings]
UIScaleRule=ShortestSide
UIScaleCurve=(... (Time=1080.000000,Value=1.000000) ...)
DesignScreenSize=(X=1920,Y=1080)
```
→ **1080p 세로에서 스케일이 정확히 1.0.** 따라서 아래 좌표를 **Canvas Slot 의 Position/Size 에 그대로**
넣으면 다른 해상도에서 자동으로 비례한다. 별도 스케일 계산이 필요 없다.

**에셋이 1080p 네이티브라는 근거 (독립적으로 3번 교차 검증)**

| 검증 대상 | 조립 시안(2128x1198) 실측 | 에셋 원본 | 비율 |
|---|---:|---:|---:|
| 패널 테두리 박스 | 796 x 817 | 720 x 739 (`옷장.png` 내부 bbox 50,50 - 769,788) | 1.1056 |
| 활성 탭 필 너비 | 138 | 124 | 1.1129 |
| 캔버스 자체 | 2128 x 1198 | 1920 x 1080 | **1.1083** |

세 값이 일치한다 → **조립 시안은 1920x1080 디자인을 약 1.108배 확대 출력한 것**이고,
**개별 에셋은 1080p 기준 1:1** 이다. 조립 좌표를 1.10833 으로 나눈 값이 §15.5 의 배치표다.

> 경고: **조립 시안(2128x1198)을 그대로 임포트해 배경으로 쓰지 말 것.** 크기가 다르고 알파가 없다.
> 시안은 에디터에서 위치를 눈으로 맞출 때 참고용으로만 쓴다.

## 15.3 레이어 → 위젯 z-order 매핑

아트팀 레이어 순서(`레이어순서.png`, 위가 앞):
```
화면효과
캐릭터, 글자 등 다른여러 요소들   ← 그룹(접힘): 캐릭터 프리뷰 + 타이틀 + 탭 + 칸 + Clear All
옷장                              ← 패널 프레임만
캐릭터 back
2차배경
1차검은배경
```

**UMG Canvas Panel 은 자식 순서가 곧 z-order 다 (뒤에 있을수록 앞).** 위 순서를 뒤집어 배치한다:

| z | 위젯 | 대응 레이어 | 비고 |
|---:|---|---|---|
| 1 | `Img_Dim` | 1차검은배경 | 컬러 브러시 |
| 2 | `Img_BGGlow` | 2차배경 | Additive |
| 3 | `Img_CharBack` | 캐릭터 back | Additive. **패널보다 뒤** |
| 4 | `Img_PanelFrame` | 옷장 | 패널 프레임 |
| 5 | `Canvas_Content` | 캐릭터/글자 그룹 | 프리뷰 + 타이틀 + 탭 + 칸 + Clear All |
| 6 | `Img_ScreenFX` | 화면효과 | Additive, **`Is Hit Test Visible = false` 필수** |

> ★3번이 4번보다 뒤라는 점을 놓치기 쉽다.★ 캐릭터 백글로우(폭 868, x 895-1763)는
> 패널 우측 경계(x=919)를 약 24px 침범한다. 순서가 바뀌면 글로우가 패널 위로 올라와 테두리를 흐린다.

## 15.4 Photoshop 블렌드 모드 → UMG 변환 ★★

**UMG 는 Soft Light / Lighter Color 를 지원하지 않는다.** UI 도메인 머티리얼이 제공하는 블렌드 모드는
`Opaque / Masked / Translucent / Additive / Modulate / AlphaComposite / AlphaHoldout` 뿐이고,
UMG 위젯은 아래 위젯의 픽셀을 샘플링할 수 없어 **진짜 Soft Light 는 구현 불가**다.

아래가 실용적 변환이다. **전부 근사이므로 에디터에서 Tint 알파를 눈으로 튜닝한다.**

| 레이어 | 원본 | UMG 변환 | 근거 |
|---|---|---|---|
| 1차검은배경 | Normal, #000 a155 | **컬러 브러시** `(0,0,0,0.608)` — 텍스처 임포트 안 함 | 전 픽셀 알파가 155 로 **균일**. 1920x1080 텍스처를 쓸 이유가 없다 |
| 2차배경 | **Soft Light** | `M_UI_Additive` + Tint a **0.25 시작** | 어두운 바탕(a61% 검정) 위에서 Soft Light 의 밝은 색은 **밝히는 방향으로만** 작용 → Additive 가 가장 가깝다 |
| 캐릭터 back | **Lighter Color 90%** | `M_UI_Additive` + Tint a **1.0 시작** | Lighter Color 는 두 색 중 밝은 쪽을 취한다. 소스 배경이 검정이라 어두운 부분은 사라지고 밝은 부분만 남는다 = 사실상 Screen/Additive. **알파가 이미 최대 136(53%)이라 90% 불투명도가 반영돼 있을 가능성이 높다** — 1.0 부터 시작해 과하면 낮춘다 |
| 화면효과 | **Soft Light** | `M_UI_Additive` + Tint a **1.0 시작** | 알파 최대가 48(19%)로 이미 매우 옅다. Additive 로 거의 동일한 결과가 난다 |

**대안 — 픽셀 단위로 정확해야 한다면**: `1차검은배경 + 2차배경` 두 장을 **포토샵에서 병합**해
알파를 보존한 채 한 장으로 내보낸다(`T_Wardrobe_BG` 1920x1080). 둘 다 정적 전체화면이라 손해가 없고
Soft Light 문제가 배경에서는 완전히 사라진다.
**화면효과는 캐릭터 위에 와야 하므로 병합 대상이 아니다.**

### `M_UI_Additive` (신규 머티리얼 1개, 인스턴스 3개)

```
Material Domain : User Interface
Blend Mode      : Additive
Shading Model   : Unlit

파라미터
  Tex  : TextureSampleParameter2D
  Tint : VectorParameter (기본 1,1,1,1)

배선
  Emissive Color = Tex.RGB * Tint.RGB
  Opacity        = Tex.A   * Tint.A
```
→ `MI_UI_Wardrobe_BGGlow` / `MI_UI_Wardrobe_CharBack` / `MI_UI_Wardrobe_ScreenFX` 3개를 만들고
각 Image 위젯의 Brush 에 물린다. Tint 알파만 다르게 잡으면 된다.

## 15.5 `WBP_Wardrobe` 위젯 트리 + 좌표

> ★**실제로 만들 때는 §16 을 보라.**★ 이 절은 **완성된 모습과 좌표의 근거**만 담는다.
> 어떤 순서로 만드는지, 각 변수/함수가 어디서 나와 어디로 연결되는지는 **§16 클릭 단위 가이드**에 있다.

**부모 클래스: `UDRWardrobeScreenWidget`** (§5.6). 루트는 Canvas Panel.

```
WBP_Wardrobe  (Parent: UDRWardrobeScreenWidget)
└── RootCanvas                          [Canvas Panel]
    │
    ├── Img_Dim                         Anchor 전체(0,0,1,1) / Offset 0,0,0,0
    │                                   Brush: Color (0,0,0,0.608)
    │
    ├── Img_BGGlow                      Anchor 중앙(0.5,0.5) / Alignment(0.5,0.5)
    │                                   Pos(0,0)  Size 1601x901
    │                                   Brush: MI_UI_Wardrobe_BGGlow
    │
    ├── Img_CharBack                    Anchor 좌상(0,0) / Pos(895, 2)  Size 868x1076
    │                                   Brush: MI_UI_Wardrobe_CharBack
    │
    ├── Img_PanelFrame                  Anchor 좌상(0,0) / Pos(151, 147) Size 820x839
    │                                   Brush: T_Wardrobe_PanelFrame (Draw As = Image)
    │
    ├── Canvas_Content                  Anchor 전체 / Offset 0,0,0,0   ← z-order 그룹 용도
    │   │
    │   ├── Preview_Character           [WBP_CosmeticPreview]  ★변수 이름 고정★
    │   │                               Anchor 좌상 / Pos(938, 0)  Size 781x1080
    │   │                               Visibility = Visible (SelfHitTestInvisible 이면 드래그 불가)
    │   │                               내부 Img_Preview 의 Brush.Material 은 런타임 대입 (§5.8.3)
    │   │                               ★스테이지가 없으면 스스로 접힌다 (§5.8.2 폴백)★
    │   │
    │   ├── Txt_Title                   Anchor 좌상 / Pos(207, 52)
    │   │                               "WARDROBE" / Font: BrunoAceSC-Regular_Font / Size 48 (추정)
    │   │                               Color: 거의 흰색 (#EAF2FF)
    │   │
    │   ├── Overlay_TabBar              Anchor 좌상 / Pos(202, 139)  Size 496x44
    │   │   ├── Img_TabBarBG            Fill / Brush: T_Wardrobe_TabBar
    │   │   └── HBox_Tabs               Fill
    │   │       ├── Tab_Head            [WBP_WardrobeTab]  124x44
    │   │       ├── Tab_Face            [WBP_WardrobeTab]  124x44
    │   │       ├── Tab_Body            [WBP_WardrobeTab]  124x44
    │   │       └── Tab_Tail            [WBP_WardrobeTab]  124x44
    │   │
    │   ├── Scroll_Slots                Anchor 좌상 / Pos(224, 227)  Size 672x677   (추정 +-5)
    │   │   └── Grid_Slots              [Uniform Grid Panel]  Slot Padding (6,6,6,6)
    │   │       └── (런타임 생성) WBP_WardrobeSlot x N
    │   │
    │   └── Btn_ClearAll                [WBP_WardrobeClearAll]
    │                                   Anchor(1,1) / Alignment(1,1) / Pos(-64,-64) Size 112x117
    │                                   ★화면 우하단 — 설정창·업그레이드 창과 같은 자리 (§15.9)★
    │
    └── Img_ScreenFX                    Anchor 중앙 / Alignment(0.5,0.5)
                                        Pos(0,0)  Size 1753x993
                                        Brush: MI_UI_Wardrobe_ScreenFX
                                        ★Is Hit Test Visible = false★
```

### 좌표 산출 근거 (조립 시안 실측 ÷ 1.10833)

| 요소 | 조립 시안(2128) 실측 | → 1080p | 최종 배치값 |
|---|---|---|---|
| 패널 **테두리** 박스 | x 223-1019, y 218-1035 | x 201-919, y 197-934 (718x737) | — |
| 패널 **이미지** | 위 + 글로우 여백 50px | — | **Pos(151,147) Size 820x839** |
| 탭 바 좌측 = 활성 필 좌측 | x 224 | x 202 | **X = 202** |
| 탭 바 상단 | y 154 | y 139 | **Y = 139** |
| 활성 필 너비 | 138 | 124.5 | 에셋 124 그대로 |
| 타이틀 글자 bbox | x 229-572, y 68-108 | x 207-516, y 61-97 | **Pos(207,52)**, 대문자 높이 36px |
| 캐릭터 영역 가이드선 | x 1040 / 1905 | x 938 / 1719 | **프리뷰 Pos(938,0) W=781** |
| 캐릭터 백글로우 | 가이드 영역 중앙 정렬 | 중심 x = 1328.5 | **Pos(895,2) 868x1076** |

**패널 이미지 검산**: `151 + 820 = 971`, 글로우 50 제외 → 우측 테두리 `921` (실측 919, 오차 2px).
`147 + 839 = 986` → 하단 테두리 `936` (실측 934, 오차 2px). **소프트 엣지 기준 허용 범위.**

### 그리드 영역 (추정 +-5px — 에디터에서 시안 겹쳐 확인)

- 좌우 여백: 패널 테두리 안쪽에서 **23px** → X = 201 + 23 = **224**
- 폭: `216x3 + 12x2 = 672` → **정확히 3열이 들어간다**
- 상단 여백: 패널 상단에서 **30px** → Y = 197 + 30 = **227**
- 높이: 패널 하단(934) - 하단 여백 30 = **677**
- 근거: `image_1.png`(1018x1005) 실측 — 칸 폭 285 / 간격 16 / 좌여백 35.
  이 시안의 스케일은 285 ÷ 216 = **1.319** 이므로 1080p 환산 시 칸 216 / 간격 12 / 여백 26.5.

## 15.6 `WBP_WardrobeTab` (탭 1개, 124x44)

```
WBP_WardrobeTab   (Root: SizeBox  W=124 H=44)
└── Overlay
    ├── Img_Pill        Brush: T_Wardrobe_TabPill (124x44)
    │                   ★Visibility 를 선택 상태로 토글★ (Visible / Collapsed)
    ├── Btn_Tab         Style 전부 투명 (Normal/Hovered/Pressed 알파 0)
    │                   ★Hovered = Normal 과 동일 — 호버 연출 없음 (확정 §15.12-5)★
    │                   배경은 Img_Pill 이 담당 — 버튼은 히트박스와 이벤트만
    └── Txt_Label       HAlign/VAlign Center
                        Font: BrunoAceSC-Regular_Font / Size 20 (추정)
```

**노출 변수 / 이벤트**
```
[변수]   Category : EDRCosmeticCategory  (Instance Editable, Expose on Spawn)
         Label    : Text                  (Instance Editable)  "HEAD" / "FACE" / "BODY" / "TAIL"
[이벤트] OnTabClicked(EDRCosmeticCategory)  → 부모 WBP_Wardrobe 가 구독
[함수]   SetSelected(bool bSelected)
           Img_Pill.Visibility        = bSelected ? Visible : Collapsed
           Txt_Label.ColorAndOpacity  = bSelected ? #EAF2FF : #8E9BC8   (시안 기준 흐린 청보라)
```

**필(Pill)을 탭마다 하나씩 두는 이유**: 필 1개를 활성 탭 위치로 이동시키는 방식은 좌표 계산과
애니메이션이 붙는다. 탭마다 소유하고 `Collapsed` 토글하면 계산이 0이 된다.
`496 ÷ 4 = 124` 로 **에셋 크기가 이미 정확히 4등분**이라 HBox 만으로 정렬이 끝난다.

**호버 연출 — 확정 (§15.12-5)**: **넣지 않는다.** `Btn_Tab` 의 Hovered/Pressed 스타일을
Normal 과 동일하게 둔다. 나중에 필요해지면 `Txt_Label` 색 보간만 추가하면 되므로 구조는 그대로다.

**폰트 — 확정 (§15.12-8)**: 프로젝트에 실재하는 3종 중 아래로 정한다.

| 용도 | 폰트 | 이유 |
|---|---|---|
| **라틴 표시용** (`WARDROBE`, `HEAD/FACE/BODY/TAIL`) | **`BrunoAceSC-Regular_Font`** | 시안의 각지고 넓은 테크노 서체와 형태가 가장 가깝다. 대문자 전용 표시라 한글 미지원이 문제되지 않는다 |
| **한글/본문** (툴팁, 해금 조건 문구, 스킨 이름) | **`Pretendard-SemiBold__1__Font`** | 한글 가독성이 좋고 이미 다른 UI 에서 쓰인다 |

> 스킨 이름·설명은 **한글이 들어올 수 있으므로 반드시 Pretendard** 로 둔다.
> BrunoAce 는 한글 글리프가 없어 네모(tofu)로 표시된다.

## 15.7 `WBP_WardrobeSlot` (칸 1개, 216x252)

> ⚠️ **아래 "버튼 스타일 스왑" 구조는 §16 STEP 4 가 대체했다.**
> UE 5.2 부터 `UButton::WidgetStyle` 직접 접근이 deprecated 라 BP 에서 하려면 `FButtonStyle` 을
> 통째로 조립해야 한다(노드 10개 이상). **§16 STEP 4 의 "투명 버튼 + 상태 이미지" 구조를 쓸 것** —
> `Set Brush from Texture` 노드 하나로 끝나고 C++ 은 하나도 바뀌지 않는다.

```
WBP_WardrobeSlot  (Parent: UDRWardrobeSlotWidget / Root: SizeBox  W=216 H=252)
└── Btn_Slot                      [Button]  ★상태 브러시를 이 버튼 스타일로 처리★
    └── Overlay
        ├── Img_Thumbnail         Padding (16,16,16,16) / Stretch: ScaleToFit
        │                         Brush 는 디자이너에 비워 둔다 — 런타임 바인딩 (§15.12-7)
        │                         ★아이콘이 아직 없으므로 자동으로 Collapsed 된다★
        ├── Img_Lock              HAlign/VAlign Center / 잠김일 때만 Visible
        │                         ★브러시 미지정 (§15.12-2) — 자물쇠 아트가 나오면 채운다★
        └── Txt_Debug             (개발 중 SkinId 표시, 배포 시 Collapsed)
```

> **아이콘·자물쇠 아트가 아직 없다(§15.12-2, §15.12-7).** 위젯은 **지금 만들어 두고 브러시만 비워 둔다.**
> `UDRWardrobeSlotWidget` 이 `PreviewIcon == nullptr` 이면 `Img_Thumbnail` 을 자동으로 숨기므로,
> 아트가 없는 동안에도 칸은 **상태 3종(기본/호버/선택)이 정상 동작**한다. 아트가 나오면 브러시만 채우면 된다.

### 상태 머신 — Button Style 스왑

칸에는 **입력 상태(Hover/Press)** 와 **지속 상태(Selected/Locked)** 가 섞여 있다.
`UButton` 스타일만으로 처리하되 **선택/잠김일 때 스타일 3칸을 전부 덮어쓴다.**

| 논리 상태 | Normal | Hovered | Pressed | 추가 처리 |
|---|---|---|---|---|
| 일반 (해금·미선택) | `Slot_Normal` | `Slot_Hovered` | `Slot_Hovered` | — |
| **선택됨** | `Slot_Selected` | `Slot_Selected` | `Slot_Selected` | 체크 마크는 **텍스처에 각인돼 있어 별도 위젯 불필요** |
| **잠김** | `Slot_Normal` | `Slot_Normal` | `Slot_Normal` | `Img_Lock` 표시 + 썸네일 회색조 Tint `(0.35,0.35,0.35,1)` |

```
[함수] Setup(const FDRSkinViewModel& VM)
    SkinId = VM.SkinId
    Img_Thumbnail.Brush.SetResourceObject(VM.PreviewIcon)
    Img_Thumbnail.ColorAndOpacity = VM.bUnlocked ? White : (0.35,0.35,0.35,1)
    Img_Lock.Visibility           = VM.bUnlocked ? Collapsed : Visible
    ApplyStateBrushes(VM.bUnlocked, VM.bEquipped)
    Btn_Slot.ToolTipText          = VM.bUnlocked ? VM.Description : VM.UnlockHint
    Btn_Slot.IsEnabled            = true          ← ★잠겨도 비활성화하지 않는다★
```

> **잠긴 칸을 `IsEnabled=false` 로 두지 말 것.** 비활성 버튼은 UMG 에서 툴팁과 호버 이벤트를 받지 못해
> **"왜 잠겼는지" 안내가 뜨지 않는다.** 활성으로 두고, 클릭 시 `TryEquip()` 이 `false` 를 돌려주면
> 흔들림 연출 + 조건 문구를 띄운다 (§5.6 `TryEquip` 의 반환값이 이 용도다).

**클릭 흐름** — §5.8.3 의 "실시간 갈아입기"가 여기서 시작된다:
```
Btn_Slot.OnClicked
  → 부모 WBP_Wardrobe 의 OnSlotClicked(SkinId)
  → UDRWardrobeScreenWidget::TryEquip(SkinId)
       ├─ 실패(잠김) → false → 흔들림 애니메이션 + UnlockHint 토스트
       └─ 성공       → GI->EquipSkin()                  (로컬 세이브 즉시 기록)
                     → Stage->SetPreviewSkin(SkinId)    ★서버 왕복 없이 그 프레임에 반영★
                     → RefreshAll()                     (이전 선택 해제 + 새 선택 표시)
```

## 15.8 그리드 / 스크롤

```
Scroll_Slots  [Scroll Box]
    Orientation           : Vertical
    Scroll Bar Visibility : Collapsed   ★확정 §15.12-6 — 스크롤바를 넣지 않는다★
    Consume Mouse Wheel   : WhenScrollingPossible
    Allow Overscroll      : false       ← 고정 프레임 UI 에서 튕김은 어색하다
└── Grid_Slots  [Uniform Grid Panel]
        Slot Padding : (6,6,6,6)        ← 상하좌우 6 → 인접 칸 간격 12 (실측값)
```

**런타임 채우기** (`OnRefreshSkins` BP 이벤트 안):
```
Grid_Slots.ClearChildren()
GetProgression()->GetSkinViewModels(GetViewedClass(), CurrentCategory, ViewModels)
for (i, VM) in ViewModels:
    W    = CreateWidget<WBP_WardrobeSlot>()
    W.Setup(VM)
    Slot = Grid_Slots.AddChildToUniformGrid(W)
    Slot.SetRow(i / 3)                          ← ★3열 고정★
    Slot.SetColumn(i % 3)
    Slot.SetHorizontalAlignment(HAlign_Left)    ← Fill 로 두면 칸이 늘어난다
    Slot.SetVerticalAlignment(VAlign_Top)
```

> **`WrapBox` 대신 `UniformGridPanel` + 명시적 행/열을 쓰는 이유**: WrapBox 는 가용 폭에 따라
> 열 수가 바뀐다. 패딩을 한 번 잘못 건드리면 조용히 2열이나 4열이 되고, 그 사실은 아이템 수가
> 늘어난 뒤에야 드러난다. 행/열을 직접 지정하면 3열이 구조적으로 보장된다.

> ℹ️ **스크롤바를 뺀 데 따르는 한 가지**: 그리드 높이 677 에는 **2행(252×2 + 간격 12 = 516)까지만**
> 온전히 들어가고 3행째는 잘린다. 즉 **아이템이 7개를 넘으면 스크롤이 필요한데 그 사실을 알리는
> 시각 요소가 없다.** 카테고리당 6개 이하로 유지되면 문제가 없다.
> 넘어가게 되면 스크롤바 대신 **하단 페이드 그라디언트** 한 장이 가장 싼 해결책이다(아트 1장).

**"기본" 칸**: `SkinId = NAME_None` 인 가상 항목을 **항상 인덱스 0** 에 넣어 기본 외형으로 되돌릴 수 있게
한다(§7.2). `GetSkinViewModels()` 가 이 항목을 포함해 반환하도록 §5.1 에서 처리한다.

## 15.9 Clear All 버튼

`clear all.png` (112x117) 는 모서리가 둥근 사각형 안에 **`C`** 가 들어 있다 — **키보드 힌트를 겸한 버튼**이다.

```
WBP_WardrobeClearAll  (Root: SizeBox 112x117)
└── Btn_Clear   Normal  : T_Wardrobe_ClearAll
                Hovered : 같은 텍스처 + Tint (1.15, 1.15, 1.15, 1)
                Pressed : 같은 텍스처 + Tint (0.85, 0.85, 0.85, 1)
```

**배치 — 확정 (§15.12-3)**: **화면 우하단**. 설정창·업그레이드 창과 같은 자리다.
```
Btn_ClearAll   Anchor (1,1)  Alignment (1,1)   Pos(-64, -64)   Size 112x117
```
> 앵커를 (1,1) 로 잡는 이유: 패널 기준이 아니라 **화면 기준**이라 다른 창과 같은 자리에 오고,
> 해상도가 바뀌어도 우하단에 붙어 있는다. `Pos` 는 다른 창의 여백에 맞춰 조정한다.

**동작 — 확정 (§15.12-4)**: **4개 카테고리 전부 해제.** 확인 창 없음
(되돌리기가 클릭 한 번이라 확인 창은 과하다). → `UDRGameInstance::ClearAllSkins(Class)`

**`C` 키 바인딩**: 옷장 화면이 열려 있는 동안만 유효해야 한다. 화면이 `FInputModeUIOnly` 이므로
(§5.3 `OpenWardrobeScreen`) Enhanced Input 액션이 아니라 **위젯의 `NativeOnKeyDown` 오버라이드**로
받는 편이 안전하다. `NativeConstruct` 에서 `SetKeyboardFocus()` 를 호출해 두어야 키 입력이 들어온다.

**닫기 — 확정 (§15.12-9)**: 마우스 전용 닫기 버튼은 만들지 않는다.
`NativeOnKeyDown` 이 **`Escape`** 를 받아 `RequestClose()` → `PC->CloseWardrobeScreen()` 으로 넘긴다.
(`ADRPlayerController::HandleToggleSettings` 에도 같은 분기를 넣어 두 경로 모두 동작하게 한다)

## 15.10 텍스처 임포트 설정 ★ (틀리면 티가 난다)

**전 텍스처 공통**
```
Texture Group        : UI
Compression Settings : UserInterface2D (RGBA)     ★기본 DXT5 금지★
sRGB                 : true
Mip Gen Settings     : NoMipmaps
Filter               : Default (Bilinear)
Never Stream         : true
```

> **`UserInterface2D` 가 필수인 이유**: 이 에셋들은 대부분 **부드러운 라디얼 그라디언트**다.
> 기본 `DXT5` 는 4x4 블록당 색을 2개만 저장해서 이런 그라디언트에 **밴딩(띠)** 을 만든다.
> 특히 알파 최대값이 낮은 3종 — `화면효과`(48) · `카테고리바`(102) · `캐릭터 back`(136) — 은
> 알파 정밀도가 그대로 뭉개져 **계단이 눈에 보인다.** UI 텍스처는 압축하지 않는 것이 정답이다.

**Draw As 설정**

| 위젯 | Draw As | 이유 |
|---|---|---|
| `Img_PanelFrame` | **Image** | 모서리 노치가 비대칭이라 9-slice(Box)로 자를 수 없다 |
| `Img_TabBarBG` / `Img_Pill` | **Image** | 크기 고정(496x44 / 124x44)이라 늘릴 일이 없다 |
| 칸 3종 | **Image** | 216x252 고정 |
| `Img_Dim` | **Image** + 색상만 | 단색 |

> **어떤 것도 `Box`(9-slice)로 설정하지 않는다.** 전부 고정 크기로 쓰는 에셋이고,
> 소프트 글로우가 있는 이미지를 9-slice 하면 가장자리가 늘어나며 글로우가 뭉개진다.

## 15.11 ★4개 카테고리가 데이터 모델을 바꾼다★

**이번 시안에서 드러난 가장 큰 설계 영향이다. §4.2 / §4.4 / §5.1 / §5.2 / §5.4 를 개정해야 한다.**

현재 계획서는 **로봇 1대당 스킨 1개**를 전제한다:
```cpp
TMap<EPlayerCharacterClass, FName> EquippedSkins;   // §4.2 — 로봇당 1개
FName EquippedSkinId;                               // §5.2 — 복제 1개
```
그런데 UI 는 **HEAD / FACE / BODY / TAIL 4개 카테고리**가 각각 자기 그리드와 자기 선택을 갖는다.
즉 **로봇 1대당 동시 장착 4개**다.

### 개정안

```cpp
// DRCosmeticTypes.h
UENUM(BlueprintType)
enum class EDRCosmeticCategory : uint8
{
    Head, Face, Body, Tail,
    Count UMETA(Hidden)      // ★EPlayerCharacterClass 와 같은 규약 — Count 직전에만 추가★
};

// FDRSkinDefinition 에 추가
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
EDRCosmeticCategory Category = EDRCosmeticCategory::Head;
```

```cpp
// DRSaveGame.h — §4.2 의 EquippedSkins 를 대체
USTRUCT()
struct FDRClassCosmeticState
{
    GENERATED_BODY()
    // 길이 = EDRCosmeticCategory::Count. 빈 칸은 NAME_None.
    // FDRClassUpgradeState::SlotChips 와 ★같은 관용구★ (고정 길이 배열 + NAME_None)
    UPROPERTY() TArray<FName> EquippedByCategory;
};

UPROPERTY(VisibleAnywhere, Category = "Progress|Cosmetic")
TMap<EPlayerCharacterClass, FDRClassCosmeticState> Cosmetics;
```

```cpp
// DRPlayerState.h — §5.2 의 EquippedSkinId 를 대체
// 길이 4 고정, 인덱스 = EDRCosmeticCategory
UPROPERTY(ReplicatedUsing = OnRep_EquippedSkinIds, BlueprintReadOnly, Category = "Cosmetic")
TArray<FName> EquippedSkinIds;
```

### ★확정 (2026-08-25): 4개 카테고리는 **부착물 메시**다★

`HEAD / FACE / BODY / TAIL` = 부위별 **부착물**. 따라서 **§2 비목표의 "부착물(모자)" 제외가 해제**된다.
(트레일 이펙트와 본체 메시 스왑은 여전히 비목표)

```cpp
// DRCosmeticTypes.h

/** 메시 1개를 캐릭터에 붙이는 명세. TP/FP 각각 따로 지정한다. */
USTRUCT(BlueprintType)
struct FDRSkinAttachSpec
{
    GENERATED_BODY()

    // 둘 다 지정하면 Skeletal 이 우선. 둘 다 비면 "이쪽에는 안 붙인다".
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
    TSoftObjectPtr<UStaticMesh> StaticMesh;

    // 붙을 소켓/본. None 이면 부모 컴포넌트 원점.
    // ★bUseLeaderPose 가 true 면 무시된다★ (스켈레톤을 공유하므로 소켓 개념이 없다)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
    FName SocketName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
    FTransform RelativeTransform;

    // 부모 메시의 포즈를 그대로 따라갈지 (SetLeaderPoseComponent).
    //   몸통 의상처럼 ★같은 스켈레톤을 공유하는 메시★ → true (전용 ABP 불필요, 성능도 유리)
    //   모자처럼 소켓에 매달리는 소품                → false
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
    bool bUseLeaderPose = false;

    // 부착물 자체의 머티리얼 오버라이드 (같은 메시의 색상 변형)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
    TArray<FDRSkinMaterialOverride> MaterialOverrides;

    bool IsValid() const { return !SkeletalMesh.IsNull() || !StaticMesh.IsNull(); }
};

/** 캐릭터 ★본체★ 메시의 일부 머티리얼 슬롯만 덮어쓴다. (부착물 없이 리컬러만 하는 스킨용) */
USTRUCT(BlueprintType)
struct FDRSkinMaterialOverride
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 MaterialSlot = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TSoftObjectPtr<UMaterialInterface> Material;
};
```

`FDRSkinDefinition` 은 **부착물 + 본체 리컬러를 모두** 담는다. 둘 다 선택이므로
"부착물만" / "리컬러만" / "둘 다" 스킨이 전부 표현된다.

```cpp
// 외형 — 부착물 (주 경로)
UPROPERTY(...) FDRSkinAttachSpec ThirdPerson;   // 타인 시점
UPROPERTY(...) FDRSkinAttachSpec FirstPerson;   // 본인 시점. ★비워두면 FP 에는 안 붙는다★

// 외형 — 본체 머티리얼 오버라이드 (보조 경로, 선택)
UPROPERTY(...) TArray<FDRSkinMaterialOverride> BodyMaterialsTP;
UPROPERTY(...) TArray<FDRSkinMaterialOverride> BodyMaterialsFP;
UPROPERTY(...) TArray<FDRSkinMaterialOverride> WeaponMaterials;
```

> **FP 를 비워두는 것이 기본값이다.** 1인칭에서는 자기 머리 위의 모자도, 꼬리도 보이지 않는다.
> 청소기는 FP 스켈레톤이 TP 와 완전히 별개라(Plan5 §1.1) **소켓 이름이 아예 다르거나 없을 수 있다.**
> FP 스펙은 "1인칭에서도 실제로 보이는 부위"에만 채운다.

### 적용 규칙 (§5.4 `RefreshSkinVisuals` 개정)

**부착물 컴포넌트는 복제하지 않는다.** 각 머신이 복제된 `EquippedSkinIds`(길이 4)를 보고
**로컬에서 컴포넌트를 만든다.** 컴포넌트를 복제하면 스폰 순서·소유권 문제가 붙는데 얻는 게 없다.

```
1. 기존 부착물 컴포넌트 4×2(TP/FP) 를 전부 DestroyComponent
2. 본체 머티리얼을 BP CDO 값으로 ★전 슬롯 리셋★
     (리셋이 없으면 아이템을 벗어도 이전 머티리얼이 남는다)
3. Head → Face → Body → Tail 순으로
     a. BodyMaterials* 오버라이드 적용   (같은 슬롯이 겹치면 ★나중이 이긴다★)
     b. AttachSpec 이 유효하면 컴포넌트 생성 → 소켓에 Attach → MaterialOverrides 적용
4. 4개 모두 NAME_None 이면 1~2 만 수행 = 기본 외형
```

`ADRCharacter` 추가 멤버:
```cpp
// 카테고리별 부착물 컴포넌트. 런타임 생성이며 ★복제하지 않는다★.
// 길이 = EDRCosmeticCategory::Count. 비어 있는 칸은 nullptr.
UPROPERTY(Transient) TArray<TObjectPtr<UMeshComponent>> CosmeticAttachTP;
UPROPERTY(Transient) TArray<TObjectPtr<UMeshComponent>> CosmeticAttachFP;
```

**가시성**: 생성 직후 TP 부착물은 `SetOwnerNoSee(true)`, FP 부착물은 `SetOnlyOwnerSee(true)` 로 맞춘다.
`UpdateMeshVisibility()`(`DRCharacter.cpp:467-497`)와 `SetWaitingRoomVisibility()`(`:557-605`)가
부모 메시의 가시성을 뒤집으므로, **두 함수에서 부착물도 함께 갱신**해야 한다.

### 함수 시그니처 변경

| 기존 (§5.1 / §5.3) | 개정 |
|---|---|
| `EquipSkin(Class, SkinId)` | `EquipSkin(Class, Category, SkinId)` |
| `GetEquippedSkin(Class)` | `GetEquippedSkin(Class, Category)` |
| `GetSkinViewModels(Class, Out)` | `GetSkinViewModels(Class, Category, Out)` |
| — | `ClearAllSkins(Class)` ← Clear All 버튼(§15.9) |
| — | `GetEquippedSkins(Class)` → `TArray<FName>` 길이 4 (서버 보고용) |
| `ServerReportCosmeticLoadout(ForClass, SkinId)` | `ServerReportCosmeticLoadout(ForClass, const TArray<FName>& SkinIds)` |

> 복제량은 FName 1개 → 4개로 는다. **무시할 수준이다** (`EquippedChips` 가 이미 최대 6개를 복제한다).

### 부착물로 확정되면서 새로 생기는 작업

| 항목 | 내용 |
|---|---|
| **소켓 규약** | 로봇 3종의 TP 스켈레톤에 `Cosmetic_Head` / `Cosmetic_Face` / `Cosmetic_Body` / `Cosmetic_Tail` 소켓을 추가한다. 없으면 부착물이 원점에 붙어 바닥에 박힌다 |
| **비동기 로드** | 대상이 텍스처가 아니라 **메시**라 용량이 크다. §5.4 의 `RequestAsyncLoad` + 유효성 재확인이 더 중요해진다 |
| **프리뷰 스테이지** | §5.8 의 프리뷰 캐릭터도 같은 `RefreshSkinVisuals()` 를 타므로 **추가 작업 없음** |
| **사망 Dissolve** | 본체만 Dissolve 되고 부착물은 그대로 남는다 → `MulticastHandleDeath` 에서 부착물도 숨기거나 함께 Dissolve 해야 한다 (§15.12-11 신규) |

## 15.12 결정 완료 (2026-08-25) · 남은 항목

**10건 전부 회신됨.** 아래가 확정 사양이다.

| # | 항목 | **확정** | 반영 |
|---|---|---|---|
| 1 | 4 카테고리의 의미 | **부착물 메시** | §15.11 재작성 · §2 비목표 해제 |
| 2 | 잠긴 칸 아트 | **추후 추가.** 지금은 **현재 이미지만으로** 처리 | §15.7 — `Slot_Normal` + 썸네일 회색조 Tint. **자물쇠 아이콘 위젯은 만들되 브러시 미지정 상태로 둔다** |
| 3 | Clear All 위치 | **화면 우하단** (설정창·업그레이드 창과 동일) | §15.9 — 앵커 (1,1) |
| 4 | Clear All 범위 | **4개 카테고리 전부 해제** | §15.9 · `ClearAllSkins(Class)` |
| 5 | 탭 호버 연출 | **미정 — 넣지 않는다** | §15.6 — Hovered 스타일 = Normal 과 동일 |
| 6 | 스크롤바 | **넣지 않는다** (내릴 일이 없을 수 있음) | §15.8 — `Scroll Bar Visibility = Collapsed` |
| 7 | 칸 썸네일 | **추후 추가.** 지금은 **칸에 맞춰 이미지만 배치** | §15.7 — `Img_Thumbnail` 배치만 하고 브러시는 런타임 바인딩. 아이콘이 없으면 자동으로 숨긴다 |
| 8 | 폰트 | **`BrunoAceSC-Regular_Font`** (라틴 표시용) + **`Pretendard-SemiBold__1__Font`** (한글/본문) | §15.5 · §15.6 |
| 9 | 닫기 버튼 | **불필요.** 다른 창과 동일하게 **키 입력으로 닫는다** | §15.9 — `NativeOnKeyDown` |
| 10 | 화면효과 배치 | **중앙 정렬** | §15.5 — Anchor 중앙 / 네이티브 1753x993 |

### 남은 항목 (부착물 확정으로 새로 생김)

| # | 항목 | 필요 조치 |
|---|---|---|
| 11 | **사망 Dissolve 와 부착물** | 본체만 Dissolve 되고 부착물은 남는다. `MulticastHandleDeath` 에서 부착물도 숨기거나 함께 Dissolve (§15.11) |
| 12 | **소켓 규약** | 로봇 3종 TP 스켈레톤에 `Cosmetic_Head/Face/Body/Tail` 소켓 추가. **없으면 부착물이 원점에 박힌다** |
| 13 | **부착물 메시 에셋** | 카테고리별 실제 메시. 없으면 옷장은 "기본" 칸 1개만 표시된다(동작에는 문제 없음) |

## 15.13 제작 순서 체크리스트

**A. 에셋 준비**
- [ ] PNG 10종을 `Content/DaeRuneAssets/UI/Wardrobe/` 로 임포트 (`1차검은배경`·시안 2종 제외)
- [ ] 전 텍스처에 §15.10 설정 적용 — **`UserInterface2D` 확인이 핵심**
- [ ] `M_UI_Additive` 제작 + 인스턴스 3종 (§15.4)

**B. 하위 위젯**
- [ ] `WBP_WardrobeTab` (§15.6)
- [ ] `WBP_WardrobeSlot` (§15.7) — 상태 브러시 스왑 + 잠김 처리
- [ ] `WBP_WardrobeClearAll` (§15.9)

**C. 메인 위젯**
- [ ] `WBP_Wardrobe` 트리 구성 (§15.5) — **z-order 6단 순서 주의(§15.3)**
- [ ] 조립 시안을 임시 Image 로 얹고 알파 30% 로 겹쳐 **좌표 눈맞춤** → 확인 후 제거
- [ ] `UDRWardrobeScreenWidget` 를 부모로 지정하고 `OnRefreshSkins` 구현

**D. 배선**
- [ ] 탭 4개 → `CurrentCategory` 전환 → `RefreshAll()`
- [ ] 칸 클릭 → `TryEquip()` → 프리뷰 즉시 반영 (§15.7)
- [ ] `C` 키 → `NativeOnKeyDown` → Clear All
- [ ] ESC → `RequestClose()` (§5.6)

**E. 검증**
- [ ] 1920x1080 / 2560x1440 / 1280x720 에서 배치가 비례하는지 (ShortestSide 스케일 확인)
- [ ] `Img_ScreenFX` 가 클릭을 먹지 않는지 (`Is Hit Test Visible = false`)
- [ ] 알파 낮은 3종(화면효과·카테고리바·캐릭터 back)에 밴딩이 없는지
- [ ] 칸이 10개 이상일 때 스크롤이 3열을 유지하는지
- [ ] 잠긴 칸 호버 시 조건 툴팁이 뜨는지 (**`IsEnabled=false` 로 두면 안 뜬다**)

## 15.14 §15 가 기존 절에 미치는 영향 요약

| 절 | 변경 |
|---|---|
| §2 비목표 | **(B) 해석 채택 시** "부착물" 제외 항목 해제 필요 (§15.11) |
| §4.2 SaveGame | `EquippedSkins` → `Cosmetics : TMap<Class, FDRClassCosmeticState>` |
| §4.4 스킨 정의 | `Category` 필드 추가, 머티리얼 배열 → `FDRSkinMaterialOverride` 배열 |
| §5.1 GameInstance | 4개 API 시그니처에 `Category` 추가 + `ClearAllSkins()` 신설 |
| §5.2 PlayerState | `EquippedSkinId : FName` → `EquippedSkinIds : TArray<FName>` (길이 4) |
| §5.3 Controller | `ServerReportCosmeticLoadout` 인자 변경 + `C` 키 처리 |
| §5.4 Character | **CDO 리셋 후 Head→Face→Body→Tail 순차 적용** 규칙 신설 |
| §5.6 위젯 | `CurrentCategory` 상태 + `TryEquip` 에 카테고리 반영 |
| §7.2 레이아웃 | **§15.5 가 확정판** — §7.2 의 ASCII 스케치를 대체 |
| §13 파일 목록 | 신규 위젯 3종 + 텍스처 10종 + 머티리얼 1+3 추가 |
| §10 마일스톤 | M4 에 §15.13 체크리스트 반영. **M2 착수 전 §15.11 결정 필요** |

## 15.15 구현 현황 (2026-08-25)

**UI 제작을 막고 있던 C++ 을 전부 착지시켰다.** WBP 작업은 지금 바로 시작할 수 있다.
UHT(리플렉션) 통과 확인 — 에디터가 열려 있어 C++ 컴파일은 미검증(§15.15 말미).

### 완료 — 신규 파일 8개

| 파일 | 내용 |
|---|---|
| `Public/Game/DRCosmeticTypes.h` | `EDRCosmeticCategory`(Head/Face/Body/Tail/Count), `FDRSkinMaterialOverride`, `FDRSkinAttachSpec`, `FDRSkinDefinition`, `FDRClassCosmeticState`, `FDRSkinViewModel` |
| `Public/Game/DRCosmeticCatalog.h` / `.cpp` | 스킨 카탈로그 DataAsset. Id 인덱스 캐시 · `GetSkinsForCategory`(정렬 포함) · `SanitizeSkin`/`SanitizeLoadout` · 에디터 자동 검증 |
| `Public/UI/Widget/DRWardrobeScreenWidget.h` / `.cpp` | 옷장 화면 베이스. 카테고리 상태 · `TryEquip` · `ClearAll` · ESC/C 키 · 탭 선택 일괄 갱신 · 델리게이트 구독/해제 |
| `Public/UI/Widget/DRWardrobeTabWidget.h` / `.cpp` | 탭 1개. `Category` 지정 + `SetSelected` + 클릭 중계 |
| `Public/UI/Widget/DRWardrobeSlotWidget.h` / `.cpp` | 칸 1개. 뷰모델 수신 + 클릭 중계 + 썸네일 유무/틴트/툴팁 헬퍼 |
| `Public/Actor/DRWardrobe.h` / `.cpp` | 로비 옷장 액터 (`ADRUpgradeStation` 구조 복제) |
| `Public/Actor/DRCosmeticPreviewStage.h` / `.cpp` | 맵 밖 촬영 스튜디오. 프리뷰 캐릭터 스폰·회전·캡처 On/Off·ShowOnlyList (§5.8) |
| `Public/UI/Widget/DRCosmeticPreviewWidget.h` / `.cpp` | RT→MID 배선 + 드래그 회전(마우스 캡처) + 스테이지 부재 시 폴백 |

### 완료 — 기존 파일 수정

| 파일 | 내용 |
|---|---|
| `DRSaveGame.h` | `EarnedAchievements` / `Cosmetics` / `PendingSteamAchievements` 추가. **`CurrentSaveVersion` 은 4 유지** |
| `DRProgressionConfig.h` | `FDRAchievementDef::SteamApiName` + `UDRProgressionConfig::CosmeticCatalog` |
| `DRGameInstance.h` / `.cpp` | 코스메틱 API 11종 + 델리게이트 2종 + 내부 헬퍼 8종. `EnsureProgressInitialized` lazy 초기화·정화, `ApplyStageReward` 훅 2곳 |
| `DRCharacter.h` / `.cpp` | `ApplyCosmeticSkins`/`RefreshSkinVisuals`/`SyncCosmeticVisibility` + 내부 5종. 부활 경로·가시성 2곳 배선 |
| `DRPlayerController.h` / `.cpp` | `OpenWardrobeScreen`/`CloseWardrobeScreen` + BP 훅 2종 + 위젯 클래스 슬롯, ESC 우선순위, 레벨 이동 리셋, 치트 3종(`DRUnlockSkins`/`DROpenWardrobe`/`DRDumpCosmetic`) |

### 지금 되는 것 / 안 되는 것

| | 상태 |
|---|---|
| 옷장 화면 열기·닫기 (ESC / C) | ✅ |
| 탭 4개 전환 + 칸 목록 생성 + 잠금 판정 + 툴팁 | ✅ |
| 선택 → 로컬 세이브 기록 → 재시작 후 유지 | ✅ |
| 업적 달성 → 스킨 해금 + 해금 토스트 이벤트 | ✅ |
| **부착물이 캐릭터에 실제로 붙는 것** | ✅ `ADRCharacter::ApplyCosmeticSkins` — 부착물 생성 + 본체 머티리얼 오버라이드 + CDO 리셋 + 비동기 로드 + 세대 검사 |
| **3D 프리뷰 (실시간 갈아입기 + 드래그 회전)** | ✅ `ADRCosmeticPreviewStage` + `UDRCosmeticPreviewWidget` (§5.8) |
| 부활 후 옷 유지 (Dissolve 원복) | ✅ `MulticastHandleRevive_Implementation` 에서 `RefreshSkinVisuals()` |
| 대기실/1인칭 전환 시 부착물 가시성 | ✅ `SyncCosmeticVisibility()` — `UpdateMeshVisibility`/`SetWaitingRoomVisibility` 두 곳에 배선 |
| **다른 플레이어에게 내 옷이 보이는 것** | ⬜ **M3** — `ADRPlayerState::EquippedSkinIds` 복제 + 보고 RPC (§5.2/§5.3). 지금은 ★내 화면의 프리뷰에서만★ 보인다 |
| 대기실 디스플레이 캐릭터에 반영 | ⬜ **M5** — `DisplaySkinOverride` 배선 (§6.3) |
| 세이브 슬롯 계정 키잉 | ⬜ **M1** — 진행도 파일 전체를 건드리므로 ★단독 착지★ (§4.6) |

> **컴파일 미검증**: UHT 는 통과했으나(헤더·리플렉션 정상) 언리얼 에디터가 실행 중이라
> Live Coding 이 빌드를 막았다. **신규 `UCLASS` 파일은 Live Coding 으로 못 올라가므로 에디터를 닫고
> 풀 빌드해야 한다.** 에디터 종료 후 재빌드 필요.

### WBP 작업 착수 순서

1. 텍스처 10종 임포트 + §15.10 설정 (**`UserInterface2D` 확인**)
2. `M_UI_Additive` + 인스턴스 3종 (§15.4)
3. `WBP_WardrobeTab` → 부모 `UDRWardrobeTabWidget`, `Category` 지정, 버튼 OnClicked → `HandleClicked`
4. `WBP_WardrobeSlot` → 부모 `UDRWardrobeSlotWidget`, 버튼 OnClicked → `HandleClicked`,
   `OnViewModelUpdated` 에서 상태 브러시 스왑 + `HasThumbnail`/`GetThumbnailTint`/`GetTooltipText` 반영
5. `WBP_Wardrobe` → 부모 `UDRWardrobeScreenWidget`, §15.5 트리 구성,
   `OnRefreshSkins` 에서 `GetCurrentSkinViewModels` → 칸 생성(§15.8)
6. `BP_DRPlayerController` 에 `WardrobeScreenWidgetClass` 지정 +
   `OnWardrobeScreenOpened/Closed` 에서 CreateWidget/RemoveFromParent
7. `DA_CosmeticCatalog` 생성 → `DA_ProgressionConfig.CosmeticCatalog` 에 연결
8. 콘솔에서 `DROpenWardrobe` / `DRUnlockSkins` / `DRDumpCosmetic` 으로 확인

---

# 16. WBP 제작 클릭 단위 가이드

> **작성일** 2026-08-25 · **§15 와의 관계**: §15 는 **"무엇을 왜"**(좌표·에셋·근거), 이 절은 **"어떤 순서로 어떻게"**다.
> 수치가 필요하면 §15 를 보고, 만드는 동안에는 이 절만 위에서 아래로 따라가면 된다.
> **충돌 시 이 절이 우선**한다 (§16 STEP 4 가 §15.7 의 칸 구조를 대체했다 — 이유는 STEP 4-0).

## 16.0 먼저 알아 둘 것 — "이 변수/함수는 어디서 나왔나"

이 문서에 나오는 모든 이름은 **출처가 셋 중 하나**다. 각 STEP 의 표에 항상 표기한다.

| 표기 | 뜻 | 에디터에서 꺼내는 법 |
|---|---|---|
| **[C++]** | C++ 부모 클래스가 준 것 | 아래 4가지 방법 참조 |
| **[BP변수]** | 이 문서에서 **직접 만들라고 지시하는** 블루프린트 변수 | My Blueprint 패널 → Variables → `+` |
| **[위젯]** | 디자이너에 배치한 위젯. 이름이 곧 변수 | Designer 에서 배치 후 이름 변경 |

### [C++] 을 꺼내는 4가지 방법

**① C++ 이벤트를 구현한다 (BlueprintImplementableEvent)**
> My Blueprint 패널 → **Functions** 항목 오른쪽의 **`Override ▼`** 드롭다운 클릭 → 목록에서 선택
> → **Event Graph 에 `Event <이름>` 노드가 생긴다.**
> 예) `OnRefreshSkins` → `Event On Refresh Skins`

**② C++ 함수를 호출한다 (BlueprintCallable / BlueprintPure)**
> Event Graph 빈 곳 **우클릭 → 이름 검색**.
> 자기 자신(self)에 대한 호출이면 Context Sensitive 가 켜진 채로 그냥 이름만 쳐도 나온다.
> 예) `TryEquip` → `Try Equip` 노드 (에디터는 띄어쓰기를 넣어 표시한다)

**③ C++ 변수를 읽는다 (UPROPERTY BlueprintReadOnly/ReadWrite)**
> ★상속 변수는 My Blueprint 패널에 기본으로 안 보인다★ —
> My Blueprint 패널 우상단 **눈 아이콘(Settings) → `Show Inherited Variables` 체크**.
> 또는 그래프에서 우클릭 → `Get <이름>` 검색.
> 예) `ViewModel` → `Get View Model`

**④ C++ 기본값을 설정한다 (EditDefaultsOnly / EditAnywhere)**
> 블루프린트 에디터 상단 툴바의 **`Class Defaults`** 버튼 → 우측 Details 패널에 나타난다.
> 예) `ClearAllKey`, `LockedTint`, `PreviewMaterial`

### BindWidget 규칙

C++ 이 `meta = (BindWidget)` 또는 `(BindWidgetOptional)` 로 선언한 프로퍼티는
**디자이너의 위젯 이름을 프로퍼티 이름과 글자 단위로 똑같이** 지으면 자동 연결된다.

이 프로젝트에서 해당하는 것은 **딱 하나**다:

| WBP | 위젯 이름 | 타입 | 필수? |
|---|---|---|---|
| `WBP_Wardrobe` | **`Preview_Character`** | `WBP_CosmeticPreview` | Optional — 없거나 이름이 달라도 컴파일된다 (프리뷰만 빠짐) |

나머지 위젯 이름은 **전부 자유**다. 다만 이 문서의 이름을 그대로 쓰면 아래 배선 설명과 1:1로 맞는다.

## 16.1 제작 순서 (의존 관계)

**아래에서 위로 만든다.** 뒤 단계가 앞 단계의 결과물을 참조하므로 순서를 지켜야 한다.

```
STEP 1  텍스처 임포트            (의존 없음)
STEP 2  M_UI_Additive + 인스턴스 3종        ← STEP 1
STEP 3  WBP_WardrobeTab                     ← STEP 1
STEP 4  WBP_WardrobeSlot                    ← STEP 1
STEP 5  RT_CosmeticPreview + M_CosmeticPreview  (의존 없음)
STEP 6  WBP_CosmeticPreview                 ← STEP 5
STEP 7  BP_CosmeticPreviewStage             ← STEP 5
STEP 8  WBP_Wardrobe  ★본체★               ← STEP 2,3,4,6
STEP 9  BP_DRPlayerController 배선           ← STEP 8
STEP 10 DA_CosmeticCatalog                  (의존 없음)
STEP 11 BP_Wardrobe 액터 + 레벨 배치         ← STEP 7
STEP 12 동작 확인
```

> **STEP 8 까지 가야 화면이 뜬다.** 중간에 확인하고 싶으면 STEP 3·4 를 만든 직후
> 각 WBP 에디터의 Designer 탭에서 눈으로만 확인하고 넘어간다.

---

## STEP 1 — 텍스처 임포트

**위치**: `Content/DaeRuneAssets/UI/Wardrobe/`

PNG 10 장을 드래그해 임포트한다 (`1차검은배경`·`조립`·`image_1` 3장은 **임포트하지 않는다** — §15.1).

임포트 후 **10장 전부 선택 → 우클릭 → Asset Actions → Bulk Edit via Property Matrix** 로 한 번에 설정:

| 항목 | 값 |
|---|---|
| Texture Group | **UI** |
| Compression Settings | **UserInterface2D (RGBA)** ★가장 중요★ |
| Mip Gen Settings | **NoMipmaps** |
| sRGB | 체크 |

> 기본 `DXT5` 로 두면 부드러운 그라디언트에 **밴딩(띠)** 이 생긴다.
> 특히 `화면효과`(알파 최대 48)·`카테고리바`(102)·`캐릭터 back`(136)은 알파 정밀도가 낮아 계단이 눈에 보인다. (§15.10)

**이름 변경** (§15.1 의 "제안 에셋명" 열대로):
`T_Wardrobe_BGGlow` / `T_Wardrobe_CharBack` / `T_Wardrobe_PanelFrame` / `T_Wardrobe_TabBar` /
`T_Wardrobe_TabPill` / `T_Wardrobe_Slot_Normal` / `T_Wardrobe_Slot_Hovered` / `T_Wardrobe_Slot_Selected` /
`T_Wardrobe_ClearAll` / `T_Wardrobe_ScreenFX`

---

## STEP 2 — `M_UI_Additive` + 머티리얼 인스턴스 3종

**위치**: `Content/Blueprints/UI/Wardrobe/Material/`

포토샵의 **Soft Light / Lighter Color 를 UMG 가 지원하지 않기 때문에** Additive 로 근사한다 (§15.4).

### 2-1. 머티리얼 만들기

우클릭 → Material → 이름 `M_UI_Additive` → 더블클릭

**Details 패널 (그래프 빈 곳 클릭 시 나오는 머티리얼 자체 설정)**

| 항목 | 값 |
|---|---|
| Material Domain | **User Interface** |
| Blend Mode | **Additive** |
| Shading Model | **Unlit** |

**노드 배선**

```
[TextureSampleParameter2D]  이름: Tex        [VectorParameter]  이름: Tint
        │ RGB ─────────┐                            │ RGB ──┐
        │              └──▶ [Multiply] ◀────────────┘       │
        │                        └──────────────▶ Emissive Color
        │ A ───────────┐                                    │
                       └──▶ [Multiply] ◀───────────── A ────┘
                                └──────────────▶ Opacity
```

- `TextureSampleParameter2D` 노드: 팔레트에서 `TextureSampleParameter2D` 검색 →
  노드 선택 후 Details 의 **Parameter Name 을 `Tex`** 로
- `VectorParameter` 노드: Details 의 **Parameter Name 을 `Tint`**, Default Value 를 `(1,1,1,1)` 로
- Multiply 두 개: 하나는 RGB 끼리, 하나는 A 끼리

### 2-2. 인스턴스 3개 만들기

`M_UI_Additive` 우클릭 → **Create Material Instance** → 3개 만들고 각각:

| 인스턴스 이름 | `Tex` | `Tint` 의 A (시작값) |
|---|---|---|
| `MI_UI_Wardrobe_BGGlow` | `T_Wardrobe_BGGlow` | **0.25** |
| `MI_UI_Wardrobe_CharBack` | `T_Wardrobe_CharBack` | **1.0** |
| `MI_UI_Wardrobe_ScreenFX` | `T_Wardrobe_ScreenFX` | **1.0** |

> 인스턴스 에디터에서 파라미터 왼쪽 **체크박스를 켜야** 값이 적용된다.
> Tint 알파는 **눈으로 보고 조정할 값**이다 — 위 숫자는 시작점일 뿐이다.

---

## STEP 3 — `WBP_WardrobeTab` (탭 1개)

**위치**: `Content/Blueprints/UI/Wardrobe/`
**만들기**: 우클릭 → User Interface → Widget Blueprint → **Parent Class 선택 창에서 `DRWardrobeTabWidget` 검색**

> ★Widget Blueprint 를 만들 때 부모를 고르는 창이 뜬다.★ "User Widget" 을 고르면 안 된다.
> 이미 만들었다면: 블루프린트 에디터 → File → Reparent Blueprint → `DRWardrobeTabWidget`

### 3-1. 출처표

| 이름 | 출처 | 타입 | 꺼내는 법 |
|---|---|---|---|
| `Category` | **[C++]** `UDRWardrobeTabWidget` | `EDRCosmeticCategory` | ④ Class Defaults, 또는 §16.9 에서 **인스턴스마다** 설정 |
| `HandleClicked` | **[C++]** BlueprintCallable | 함수 | ② 우클릭 검색 `Handle Clicked` |
| `OnSelectionChanged(bool)` | **[C++]** BlueprintImplementableEvent | 이벤트 | ① Override ▼ → `On Selection Changed` |
| `bSelected` | **[C++]** BlueprintReadOnly | bool | ③ (이 STEP 에서는 안 쓴다 — 이벤트 인자로 받는다) |
| `Img_Pill` | **[위젯]** | Image | 아래 3-2 |
| `Btn_Tab` | **[위젯]** | Button | 아래 3-2 |
| `Txt_Label` | **[위젯]** | Text | 아래 3-2 |

### 3-2. Designer 계층

```
[Root]
└── SizeBox                    ← Palette 에서 드래그. 루트로 놓는다
    │   Width Override  = 124   (Details → Child Layout)
    │   Height Override = 44
    └── Overlay                ← SizeBox 안에 드래그
        ├── Img_Pill    [Image]   ★Overlay 슬롯: Horizontal/Vertical Alignment = Fill★
        ├── Btn_Tab     [Button]  ★Fill★
        └── Txt_Label   [Text]    ★Alignment = Center / Center★
```

> **Overlay 는 나중에 넣은 것이 위에 그려진다.** 순서를 반드시 위 그대로.
> `Txt_Label` 을 `Btn_Tab` 뒤에 두는 이유는 글자가 버튼 위에 보여야 하기 때문이다.
> 버튼이 글자에 가려도 클릭은 정상 동작한다 (Text 는 기본 `SelfHitTestInvisible`).

**Details 설정**

| 위젯 | 설정 |
|---|---|
| `Img_Pill` | Appearance → Brush → **Image = `T_Wardrobe_TabPill`** / **Visibility = `Collapsed`** (기본은 꺼진 상태) |
| `Btn_Tab` | Style → Normal/Hovered/Pressed 의 **Tint 알파를 전부 `0`** 으로 (배경은 `Img_Pill` 이 담당) |
| `Txt_Label` | Font = **`BrunoAceSC-Regular_Font`**, Size **20**, Color = `#8E9BC8` (비활성 기본색) |

> **호버 연출은 넣지 않는다** (§15.12-5 확정). Hovered/Pressed 를 Normal 과 똑같이 두면 된다.

### 3-3. Graph 배선

**(1) 클릭 → 화면에 카테고리 전환 요청**

`Btn_Tab` 선택 → Details 패널 맨 아래 **Events → `On Clicked` 의 `+`** 클릭
→ Event Graph 에 `On Clicked (Btn_Tab)` 노드가 생긴다.

```
[On Clicked (Btn_Tab)] ──exec──▶ [Handle Clicked]        ← ② 우클릭 검색
                                  (Target = self, 자동 연결됨)
```

> `HandleClicked` 는 C++ 이 `FindOwnerScreen(this)` 로 소속 옷장 화면을 찾아
> `SetCurrentCategory(Category)` 를 부른다. **BP 는 화면을 몰라도 된다.**

**(2) 선택 상태 표시**

My Blueprint → Functions 옆 **`Override ▼` → `On Selection Changed`** 선택

```
[Event On Selection Changed] ──exec──▶ [Set Visibility]        (Target = Img_Pill)
       │ b In Selected ──────────────────▶ [Select]  ← 아래 참조
       └───────────────┐
                       └──▶ [Set Color and Opacity] (Target = Txt_Label)
```

- **`Set Visibility`** 의 `In Visibility` 핀: 우클릭 → Promote 하지 말고
  **`Select` 노드**(우클릭 → `Select` 검색)를 써서
  `bInSelected == true` → `Visible`, `false` → `Collapsed` 로 고른다.
  (또는 Branch 두 갈래로 `Set Visibility` 를 두 번 놓아도 된다 — 취향)
- **`Set Color and Opacity`** (Target = `Txt_Label`): 같은 방식으로
  `true` → `#EAF2FF`, `false` → `#8E9BC8`

> ★`SetSelected` 를 BP 에서 부르지 않는다★ — 옷장 화면이 WidgetTree 를 순회하며 대신 불러 준다(§15.6).
> BP 는 "바뀌었다는 통보"만 받아 그림을 바꾼다.

**컴파일 → 저장.**

---

## STEP 4 — `WBP_WardrobeSlot` (칸 1개)

**위치**: `Content/Blueprints/UI/Wardrobe/`
**부모 클래스**: `DRWardrobeSlotWidget`

### 4-0. ★§15.7 에서 구조가 바뀐 이유★

§15.7 은 "버튼 스타일 3칸을 상태에 따라 통째로 교체"하는 방식이었다.
그런데 UE 5.2 부터 `UButton::WidgetStyle` 직접 접근이 deprecated 라
BP 에서 하려면 **`FButtonStyle` 을 통째로 조립**해야 한다 —
Normal/Hovered/Pressed 각각 `Make SlateBrush` 를 만들어 끼우는 노드가 열 개 넘게 붙는다.

대신 **"투명 버튼 + 상태 이미지 1장"** 으로 바꾼다.
`Set Brush from Texture` **노드 하나**로 끝나고, 탭 위젯과도 같은 관용구가 된다.
**C++ 은 하나도 바뀌지 않는다.**

### 4-1. 출처표

| 이름 | 출처 | 타입 | 꺼내는 법 |
|---|---|---|---|
| `ViewModel` | **[C++]** `UDRWardrobeSlotWidget` | `FDRSkinViewModel` | ③ `Get View Model` |
| `OnViewModelUpdated` | **[C++]** BIE | 이벤트 | ① Override ▼ |
| `HandleClicked` | **[C++]** BlueprintCallable → bool | 함수 | ② 우클릭 검색 |
| `HasThumbnail` | **[C++]** BlueprintPure → bool | 함수 | ② |
| `GetThumbnailTint` | **[C++]** BlueprintPure → LinearColor | 함수 | ② |
| `GetTooltipText` | **[C++]** BlueprintPure → Text | 함수 | ② |
| `LockedTint` | **[C++]** EditDefaultsOnly | LinearColor | ④ Class Defaults (기본 0.35 회색 — 그대로 두면 된다) |
| **`Tex_Normal`** | **[BP변수]** | `Texture2D` | 직접 만든다 (4-3) |
| **`Tex_Hovered`** | **[BP변수]** | `Texture2D` | 직접 만든다 |
| **`Tex_Selected`** | **[BP변수]** | `Texture2D` | 직접 만든다 |
| **`bIsHovering`** | **[BP변수]** | `Boolean` | 직접 만든다 |
| **`UpdateStateImage`** | **[BP함수]** | 함수 | 직접 만든다 (4-5) |
| `Img_State` `Img_Thumbnail` `Img_Lock` `Btn_Slot` | **[위젯]** | | 4-2 |

### 4-2. Designer 계층

```
[Root]
└── SizeBox
    │   Width Override  = 216
    │   Height Override = 252
    └── Overlay
        ├── Img_State      [Image]  Fill / Fill      ← 상태 브러시(기본·호버·선택)
        ├── Img_Thumbnail  [Image]  Fill / Fill, Padding = 16 (상하좌우)
        ├── Img_Lock       [Image]  Center / Center  ← 브러시 ★비워 둔다★ (아트 미정 §15.12-2)
        └── Btn_Slot       [Button] Fill / Fill      ← 맨 위: 클릭/호버 히트박스
```

**Details 설정**

| 위젯 | 설정 |
|---|---|
| `Img_State` | Brush Image = `T_Wardrobe_Slot_Normal` (런타임에 교체된다) |
| `Img_Thumbnail` | Brush 비워 둠. **Visibility = `Collapsed`** (아이콘이 없으면 숨긴 채로 시작) |
| `Img_Lock` | Brush 비워 둠. **Visibility = `Collapsed`** |
| `Btn_Slot` | Style 의 Normal/Hovered/Pressed **Tint 알파 전부 `0`** |

### 4-3. BP 변수 4개 만들기

My Blueprint → Variables → `+` 를 4번:

| 이름 | 타입 | Instance Editable | 기본값 |
|---|---|---|---|
| `Tex_Normal` | Texture 2D **(Object Reference)** | ✔ | `T_Wardrobe_Slot_Normal` |
| `Tex_Hovered` | Texture 2D | ✔ | `T_Wardrobe_Slot_Hovered` |
| `Tex_Selected` | Texture 2D | ✔ | `T_Wardrobe_Slot_Selected` |
| `bIsHovering` | Boolean | ✖ | false |

> 기본값은 **컴파일을 한 번 해야** Details 패널에서 지정할 수 있다.

### 4-4. 그래프 — 뷰모델 반영

My Blueprint → **Override ▼ → `On View Model Updated`**

```
[Event On View Model Updated]
  │
  ├─exec─▶ [Update State Image]                       ← 4-5 에서 만들 BP 함수
  │
  ├─exec─▶ [Set Visibility]  Target = Img_Thumbnail
  │            In Visibility ◀── [Select] ◀── [Has Thumbnail]        ← ② C++ Pure
  │                          (true → Visible / false → Collapsed)
  │
  ├─exec─▶ [Set Brush from Texture]  Target = Img_Thumbnail
  │            Texture ◀── [Get View Model] ▶ [Break] ▶ PreviewIcon   ← ③ C++ 변수
  │            (bMatchSize = false)
  │
  ├─exec─▶ [Set Color and Opacity]  Target = Img_Thumbnail
  │            In Color and Opacity ◀── [Get Thumbnail Tint]         ← ② C++ Pure
  │
  ├─exec─▶ [Set Visibility]  Target = Img_Lock
  │            In Visibility ◀── [Select] ◀── [NOT] ◀── ViewModel.bUnlocked
  │                          (잠김이면 Visible)
  │
  └─exec─▶ [Set Tool Tip Text]  Target = Btn_Slot
               In Tool Tip Text ◀── [Get Tooltip Text]               ← ② C++ Pure
```

**`Get View Model` 구조체 멤버 꺼내는 법**: `Get View Model` 노드의 출력 핀을 끌어
`Break DRSkin View Model` 을 놓으면 `SkinId / DisplayName / PreviewIcon / bUnlocked / bEquipped / UnlockHint / bIsNoneSlot` 이 모두 나온다.
> 핀이 너무 많으면 Break 노드 우클릭 → **Split Struct Pin** 대신
> `Get View Model` 핀에서 바로 드래그 → 멤버 이름 검색이 더 빠르다.

### 4-5. BP 함수 `UpdateStateImage` 만들기

My Blueprint → Functions → `+` → 이름 `UpdateStateImage`

```
[Function Entry]
   └─exec─▶ [Set Brush from Texture]   Target = Img_State, bMatchSize = false
                Texture ◀── [Select]
                              Option 0(=Index 0) : Tex_Normal
                              Option 1           : Tex_Hovered
                              Option 2           : Tex_Selected
                              Index ◀── (아래 계산)
```

Index 계산 — **Select (Int)** 대신 `Branch` 두 개가 더 읽기 쉽다:

```
[Branch]  Condition ◀── ViewModel.bEquipped
   True  ─▶ [Set Brush from Texture]  Texture = Tex_Selected
   False ─▶ [Branch]  Condition ◀── bIsHovering
                True  ─▶ [Set Brush from Texture]  Texture = Tex_Hovered
                False ─▶ [Set Brush from Texture]  Texture = Tex_Normal
```

> **잠긴 칸도 `Tex_Normal` 을 쓴다** — 전용 잠금 아트가 아직 없다(§15.12-2 확정).
> 잠김 표시는 `Img_Lock` + 썸네일 회색조(`GetThumbnailTint`)가 담당한다.

### 4-6. 그래프 — 호버 / 클릭

`Btn_Slot` Details → Events 에서 `On Hovered`, `On Unhovered`, `On Clicked` 를 각각 `+`:

```
[On Hovered (Btn_Slot)]   ─▶ [Set bIsHovering] = true   ─▶ [Update State Image]
[On Unhovered (Btn_Slot)] ─▶ [Set bIsHovering] = false  ─▶ [Update State Image]

[On Clicked (Btn_Slot)]   ─▶ [Handle Clicked]           ← ② C++ BlueprintCallable
                                 └─ Return Value(bool) ─▶ [Branch]
                                        False ─▶ (선택) 흔들림 애니메이션 재생
```

> `HandleClicked` 는 C++ 이 소속 화면을 찾아 `TryEquip(ViewModel.SkinId)` 를 부른다.
> **성공 시 목록이 통째로 다시 그려지므로 BP 가 선택 표시를 직접 바꿀 필요가 없다.**
> `false` 는 "잠겨서 거절됨" — 연출을 붙이고 싶을 때만 쓴다
> (조건 문구는 화면 쪽 `OnEquipRejected` 로도 온다).

**컴파일 → 저장.**

---

## STEP 5 — `RT_CosmeticPreview` + `M_CosmeticPreview`

**위치**: `Content/Blueprints/UI/Wardrobe/` (RT) / `.../Wardrobe/Material/` (머티리얼)

### 5-1. 렌더 타깃

우클릭 → Materials & Textures → **Render Target** → 이름 `RT_CosmeticPreview`

| 항목 | 값 |
|---|---|
| Size X / Y | **512 / 1024** (세로형 — 프리뷰 영역 781×1080 비율) |
| Render Target Format | **RTF RGBA16f** ★HDR + 알파가 필요하다★ |

> `RTF RGBA8` 로 두면 `SCS_SceneColorHDR` 의 HDR 값이 잘린다.

### 5-2. 프리뷰 머티리얼

우클릭 → Material → 이름 `M_CosmeticPreview`

**Details**

| 항목 | 값 |
|---|---|
| Material Domain | **User Interface** |
| Blend Mode | **Translucent** |
| Shading Model | **Unlit** |

**노드 배선** — ★알파가 "역불투명도"라 반전이 필요하다★ (§5.8.4)

```
[TextureSampleParameter2D]  Parameter Name: Tex
        │ RGB ────────────────────────────────▶ Emissive Color
        │ A ──▶ [OneMinus (1-x)] ─────────────▶ Opacity
```

- `Tex` 의 Default Texture 에 `RT_CosmeticPreview` 를 넣어 두면 미리보기가 편하다
  (런타임에는 C++ 이 같은 이름의 파라미터에 다시 넣는다)
- `OneMinus` 노드: 우클릭 → `OneMinus` 검색

> **왜 반전인가**: 엔진 정의상 `SCS_SceneColorHDR` 의 알파는 **Inv Opacity** 다.
> 아무 것도 없는 픽셀 A=1, 캐릭터 A=0 → `1 - A` 가 곧 불투명도가 된다.
> 이 덕분에 배경이 뚫려 **뒤에 깔린 청색 글로우(`Img_CharBack`)가 비쳐 보인다.**

---

## STEP 6 — `WBP_CosmeticPreview`

**위치**: `Content/Blueprints/UI/Wardrobe/`
**부모 클래스**: `DRCosmeticPreviewWidget`

### 6-1. 출처표

| 이름 | 출처 | 꺼내는 법 |
|---|---|---|
| `PreviewMaterial` | **[C++]** EditDefaultsOnly | ④ Class Defaults → `M_CosmeticPreview` 지정 |
| `PreviewTextureParameter` | **[C++]** EditDefaultsOnly | ④ 기본 `Tex` — STEP 5 의 파라미터 이름과 같으면 그대로 둔다 |
| `YawPerPixel` | **[C++]** EditDefaultsOnly | ④ 기본 0.5. **부호를 뒤집으면 회전 방향이 반대** |
| `OnPreviewReady(Material)` | **[C++]** BIE | ① Override ▼ |
| `OnPreviewUnavailable()` | **[C++]** BIE | ① Override ▼ |
| `Img_Preview` | **[위젯]** | 6-2 |

### 6-2. Designer 계층

```
[Root]
└── Img_Preview   [Image]
        Visibility = ★Visible★   (SelfHitTestInvisible 이면 드래그가 안 먹는다)
        Brush 는 비워 둔다 — 런타임에 머티리얼이 들어간다
```

루트 위젯(캔버스 없이 Image 하나)이면 된다. Image 를 루트로 끌어다 놓는다.

> ★가장 흔한 실수★: Image 의 Visibility 가 기본 `Self Hit Test Invisible` 이면
> 마우스 드래그 이벤트가 위젯에 도달하지 않아 **회전이 안 된다.**
> 루트는 C++ 이 강제로 `Visible` 로 맞추지만 **자식 Image 는 디자이너 몫이다.**

### 6-3. Class Defaults

Class Defaults → Details 에서:
- **Preview Material = `M_CosmeticPreview`**
- Preview Texture Parameter = `Tex`
- Yaw Per Pixel = `0.5`

### 6-4. Graph 배선

```
[Event On Preview Ready]              ← ① Override ▼
   │ Material (입력 핀)
   └─exec─▶ [Set Brush from Material]   Target = Img_Preview
                 Material ◀── Material 핀
```

```
[Event On Preview Unavailable]        ← ① Override ▼
   └─exec─▶ [Set Visibility]  Target = self,  In Visibility = Collapsed
```

> 두 번째가 **§15.12 의 폴백**이다 — 레벨에 프리뷰 스테이지를 안 놓았을 때
> 프리뷰 영역만 접히고 칸 목록은 정상 동작한다.

**드래그 회전은 BP 배선이 필요 없다.** C++ 의 `NativeOnMouseButtonDown/Move/Up` 이 처리한다.

**컴파일 → 저장.**

---

## STEP 7 — `BP_CosmeticPreviewStage`

**위치**: `Content/Blueprints/Actor/Lobby/`
**만들기**: 우클릭 → Blueprint Class → **All Classes 에서 `DRCosmeticPreviewStage` 검색**

### 7-1. 컴포넌트 추가

C++ 이 준 컴포넌트 3개는 이미 계층에 있다 (**[C++]** — Components 패널에 회색으로 보인다):
`SceneRoot` (루트) / `PreviewSpawnPoint` / `CaptureComponent`

여기에 **조명을 직접 추가한다** — 맵 밖에 두면 월드 조명이 닿지 않아 캐릭터가 새까맣게 나온다:

| 추가할 컴포넌트 | 위치(대략) | 설정 |
|---|---|---|
| `KeyLight` [Rect Light] | 캐릭터 앞 왼쪽 위 | Intensity 를 눈으로 조정 |
| `FillLight` [Rect Light] | 앞 오른쪽 | Key 보다 약하게 |
| `RimLight` [Spot Light] | 뒤쪽 | 실루엣 강조 (선택) |

> 조명은 `SceneRoot` 아래에 붙인다. 액터를 옮기면 통째로 따라간다.

### 7-2. 배치 조정 (Viewport)

- `PreviewSpawnPoint` → 캐릭터가 설 자리. 원점 근처에 둔다.
- `CaptureComponent` → 캐릭터를 바라보게 회전 + 거리 조절.
  **Details 에서 `FOV Angle` 을 30~40 정도로 낮추면** 왜곡이 줄어 인물 사진처럼 나온다.

### 7-3. Class Defaults

| 항목 | 값 |
|---|---|
| **Render Target** | `RT_CosmeticPreview` ★필수★ |
| Backdrop Actors | 비워 둔다 (배경을 투명하게 쓰므로) |
| Base Yaw | 캐릭터가 카메라를 정면으로 보는 각도. 보통 `180` 근처 — **STEP 12 에서 눈으로 맞춘다** |

> `CaptureComponent` 의 나머지 설정(`CaptureSource`, `PrimitiveRenderMode`, `bCaptureEveryFrame`)은
> **C++ 생성자가 이미 잡아 뒀다.** 건드리지 않는다.

### 7-4. 레벨 배치

`LobbyMap` 을 열고 `BP_CosmeticPreviewStage` 를 드래그 → **Location Z 를 `-20000`** 으로.

> ★맵 안에 두면 안 된다★ — `PRM_UseShowOnlyList` 는 **캡처만** 격리하지 일반 뷰를 가리지 않는다.
> 맵 안에 두면 로비를 돌아다니다 프리뷰 캐릭터를 만난다.

---

## STEP 8 — `WBP_Wardrobe` ★본체★

**위치**: `Content/Blueprints/UI/Wardrobe/`
**부모 클래스**: `DRWardrobeScreenWidget`

### 8-1. 출처표

| 이름 | 출처 | 꺼내는 법 |
|---|---|---|
| `OnRefreshSkins()` | **[C++]** BIE | ① Override ▼ ← **칸 목록을 그리는 핵심** |
| `OnCategoryChanged(Category)` | **[C++]** BIE | ① (선택 — 탭 표시는 C++ 이 알아서 한다) |
| `OnEquipRejected(SkinId, UnlockHint)` | **[C++]** BIE | ① (선택 — 잠금 안내 토스트) |
| `OnSkinEquipped(SkinId)` | **[C++]** BIE | ① (선택 — 사운드 등) |
| `OnSkinNewlyUnlocked(SkinId)` | **[C++]** BIE | ① (선택 — 해금 토스트) |
| `GetCurrentSkinViewModels(out)` | **[C++]** BlueprintCallable | ② 우클릭 검색 |
| `ClearAll()` | **[C++]** BlueprintCallable | ② (Clear All 버튼용) |
| `RequestClose()` | **[C++]** BlueprintCallable | ② (필요 시) |
| `ClearAllKey` / `DefaultCategory` | **[C++]** EditDefaultsOnly | ④ Class Defaults (기본값 그대로 두면 된다) |
| **`Preview_Character`** | **[C++]** BindWidgetOptional | ★위젯 이름을 정확히 이렇게★ |
| **`SlotWidgetClass`** | **[BP변수]** | 직접 만든다 (8-4) |

### 8-2. Designer 계층 — 위에서부터 순서대로 배치

루트 캔버스에 **아래 순서 그대로** 드래그한다. **Canvas Panel 은 나중에 넣은 것이 앞에 그려진다**(§15.3).

```
[Root] CanvasPanel  (이름: RootCanvas)
 │
 ├─1─ Img_Dim              [Image]
 │      Anchor: 전체(왼쪽 위/오른쪽 아래 늘리기 프리셋) · Offset L/T/R/B = 0
 │      Brush → Image 비움 / **Tint = (0, 0, 0, 0.608)**
 │
 ├─2─ Img_BGGlow           [Image]
 │      Anchor: 중앙 · Alignment (0.5, 0.5) · Position (0,0) · Size 1601 x 901
 │      Brush → **Image = MI_UI_Wardrobe_BGGlow** (머티리얼도 Image 슬롯에 넣는다)
 │
 ├─3─ Img_CharBack         [Image]
 │      Anchor: 왼쪽 위 · Position (895, 2) · Size 868 x 1076
 │      Brush → Image = MI_UI_Wardrobe_CharBack
 │
 ├─4─ Img_PanelFrame       [Image]
 │      Anchor: 왼쪽 위 · Position (151, 147) · Size 820 x 839
 │      Brush → Image = T_Wardrobe_PanelFrame
 │
 ├─5─ Preview_Character    [WBP_CosmeticPreview]  ★이름 고정★
 │      Anchor: 왼쪽 위 · Position (938, 0) · Size 781 x 1080
 │
 ├─6─ Txt_Title            [Text]
 │      Anchor: 왼쪽 위 · Position (207, 52)
 │      Text "WARDROBE" · Font BrunoAceSC-Regular_Font · Size 48 · Color #EAF2FF
 │
 ├─7─ Overlay_TabBar       [Overlay]
 │      Anchor: 왼쪽 위 · Position (202, 139) · Size 496 x 44
 │      ├── Img_TabBarBG   [Image]  Fill/Fill · Brush Image = T_Wardrobe_TabBar
 │      └── HBox_Tabs      [Horizontal Box]  Fill/Fill
 │            ├── Tab_Head  [WBP_WardrobeTab]   Slot: Size = Fill(1.0)
 │            ├── Tab_Face  [WBP_WardrobeTab]   Slot: Size = Fill(1.0)
 │            ├── Tab_Body  [WBP_WardrobeTab]   Slot: Size = Fill(1.0)
 │            └── Tab_Tail  [WBP_WardrobeTab]   Slot: Size = Fill(1.0)
 │
 ├─8─ Scroll_Slots         [Scroll Box]
 │      Anchor: 왼쪽 위 · Position (224, 227) · Size 672 x 677
 │      Orientation = Vertical
 │      **Scroll Bar Visibility = Collapsed**   (§15.12-6 확정)
 │      Allow Overscroll = ✖
 │      └── Grid_Slots     [Uniform Grid Panel]
 │             Slot Padding = (6, 6, 6, 6)
 │
 ├─9─ Btn_ClearAll         [Button]
 │      Anchor: 오른쪽 아래 · Alignment (1, 1) · Position (-64, -64) · Size 112 x 117
 │      Style Normal/Hovered/Pressed 의 Image = T_Wardrobe_ClearAll
 │      (Hovered Tint 1.15 / Pressed Tint 0.85 로 살짝 변화를 준다)
 │
 └─10─ Img_ScreenFX        [Image]
        Anchor: 중앙 · Alignment (0.5,0.5) · Position (0,0) · Size 1753 x 993
        Brush → Image = MI_UI_Wardrobe_ScreenFX
        ★Visibility = `Self Hit Test Invisible`★  ← 클릭을 먹지 않게
```

> **좌표 입력 위치**: 위젯 선택 → Details 패널 맨 위 **Slot (Canvas Panel Slot)** 섹션.
> Anchor 프리셋 버튼 → 원하는 정렬 → 그 아래 Position/Size 에 숫자를 넣는다.

**탭 4개의 Category 지정** — 각 탭을 선택하고 Details 패널의 **`Category`**(**[C++]** 상속 변수)를:

| 위젯 | Category |
|---|---|
| `Tab_Head` | Head |
| `Tab_Face` | Face |
| `Tab_Body` | Body |
| `Tab_Tail` | Tail |

**탭 라벨** — 각 탭의 `Txt_Label` 은 자식 WBP 안에 있어 부모에서 직접 못 고친다.
→ **가장 간단한 방법**: `WBP_WardrobeTab` 에 `Label` [BP변수, Text, Instance Editable] 을 하나 만들고
`Event Pre Construct` 에서 `Set Text (Txt_Label) = Label` 을 연결한 뒤,
부모에서 탭마다 `Label` 에 "HEAD"/"FACE"/"BODY"/"TAIL" 을 입력한다.

### 8-3. Class Defaults

Class Defaults → Details:
- `Default Category` = **Head**
- `Clear All Key` = **C** (기본값)

### 8-4. BP 변수 만들기

My Blueprint → Variables → `+`

| 이름 | 타입 | 기본값 |
|---|---|---|
| `SlotWidgetClass` | **Class Reference → `DRWardrobeSlotWidget`** | `WBP_WardrobeSlot` |

> 타입 고를 때: 검색창에 `DRWardrobeSlotWidget` → 오른쪽 화살표 → **Class Reference** 를 고른다.
> (Object Reference 를 고르면 Create Widget 의 Class 핀에 못 꽂는다)

### 8-5. ★핵심★ 칸 목록 그리기

My Blueprint → **Override ▼ → `On Refresh Skins`**

```
[Event On Refresh Skins]
   │
   ├─exec─▶ [Clear Children]              Target = Grid_Slots
   │
   ├─exec─▶ [Get Current Skin View Models]        ← ② C++ BlueprintCallable
   │             └─ Out View Models (배열 출력 핀)
   │                        │
   ├─exec─▶ [For Each Loop] ◀─ Array 핀에 위 배열 연결
   │             │
   │             │  Loop Body:
   │             ├─exec─▶ [Create Widget]
   │             │           Class         ◀── [Get SlotWidgetClass]
   │             │           Owning Player ◀── [Get Owning Player]
   │             │           └─ Return Value  (이 핀을 아래 두 곳에 재사용)
   │             │
   │             ├─exec─▶ [Set Skin View Model]
   │             │           Target        ◀── Create Widget 의 Return Value
   │             │           In View Model ◀── For Each 의 **Array Element**
   │             │
   │             └─exec─▶ [Add Child to Uniform Grid]
   │                         Target     ◀── Grid_Slots
   │                         Content    ◀── Create Widget 의 Return Value
   │                         In Row     ◀── [Divide (integer)]  A = Array Index, B = 3
   │                         In Column  ◀── [% (integer)]       A = Array Index, B = 3
   │                         └─ Return Value (Uniform Grid Slot)
   │                               │
   │                               ├─exec─▶ [Set Horizontal Alignment]  = Left
   │                               └─exec─▶ [Set Vertical Alignment]    = Top
   │
   └─ Completed: (아무 것도 안 함)
```

**노드 찾는 법**
- `Clear Children` : `Grid_Slots` 를 그래프로 드래그 → 핀에서 드래그 → `Clear Children` 검색
- `Divide (integer)` : 우클릭 → `/` 또는 `Divide` 검색 → **integer** 버전 선택
- `% (integer)` : 우클릭 → `%` 또는 `Percent` 검색 → integer 버전
- `Add Child to Uniform Grid` : `Grid_Slots` 핀에서 드래그 → 검색

> **왜 3 으로 나누고 나머지를 쓰나**: 그리드 폭 672 = `216×3 + 12×2` 로 **3열이 딱 맞는다**(§15.5).
> Row/Column 을 직접 넣으므로 패딩을 바꿔도 3열이 구조적으로 보장된다.

> **`Array Index` 가 안 보이면**: `For Each Loop` 노드를 쓴 게 맞는지 확인한다
> (`For Each Loop with Break` 에도 있다). 매크로를 접었다 펴면 나타난다.

### 8-6. Clear All 버튼

`Btn_ClearAll` Details → Events → `On Clicked` `+`

```
[On Clicked (Btn_ClearAll)] ──exec──▶ [Clear All]     ← ② C++ BlueprintCallable
```

> `C` 키는 **C++ 의 `NativeOnKeyDown` 이 이미 처리한다.** BP 배선이 필요 없다.
> ESC 닫기도 마찬가지다.

### 8-7. (선택) 잠금 안내 · 해금 토스트

```
[Event On Equip Rejected]      ← ① Override ▼
   │ Skin Id, Unlock Hint (Text)
   └─▶ 원하는 안내 위젯에 Unlock Hint 를 표시

[Event On Skin Newly Unlocked] ← ① Override ▼
   └─▶ "새 옷 해금!" 토스트
```

**컴파일 → 저장.**

---

## STEP 9 — `BP_DRPlayerController` 배선

`Content/Blueprints/` 아래의 플레이어 컨트롤러 BP 를 연다
(업그레이드 화면이 이미 배선돼 있는 그 BP 다 — `OnUpgradeScreenOpened` 를 찾으면 된다).

### 9-1. Class Defaults

Details 에서 **`Wardrobe Screen Widget Class` = `WBP_Wardrobe`** 지정 (**[C++]** EditDefaultsOnly)

### 9-2. Graph — 화면 생성/제거

**Override ▼ → `On Wardrobe Screen Opened`**

```
[Event On Wardrobe Screen Opened]
   ├─exec─▶ [Create Widget]
   │           Class         ◀── [Get Wardrobe Screen Widget Class]   ← ③ C++ 변수
   │           Owning Player ◀── [Self]
   │           └─ Return Value
   ├─exec─▶ [Set Wardrobe Screen Widget]  ◀── Return Value            ← ③ C++ 변수(ReadWrite)
   └─exec─▶ [Add to Viewport]  Target ◀── Return Value
```

**Override ▼ → `On Wardrobe Screen Closed`**

```
[Event On Wardrobe Screen Closed]
   ├─▶ [Get Wardrobe Screen Widget] ─▶ [Is Valid] ─exec─▶ [Remove from Parent]
   └─exec─▶ [Set Wardrobe Screen Widget] = None
```

> **입력 모드(`FInputModeUIOnly`)와 커서 표시는 C++ 이 이미 처리한다.** BP 에서 또 하지 않는다.
> 기존 `OnUpgradeScreenOpened` 배선을 그대로 흉내 내면 된다.

---

## STEP 10 — `DA_CosmeticCatalog`

**위치**: `Content/Blueprints/Progression/`

우클릭 → Miscellaneous → **Data Asset** → 클래스 목록에서 **`DRCosmeticCatalog`** 선택
→ 이름 `DA_CosmeticCatalog`

### 10-1. 스킨 추가

`Skins` 배열에 `+` 를 눌러 항목을 만들고:

| 필드 | 값 | 비고 |
|---|---|---|
| `Skin Id` | `Skin.Gardener.Head.Cap` 같은 형식 | ★변경 금지★ — 세이브에 저장된다 |
| `Owner Class` | Gardener / VendingMachine / RobotVacuum | |
| `Category` | Head / Face / Body / Tail | 어느 탭에 나올지 |
| `Display Name` | 표시명 | 한글 가능 |
| `Required Achievements` | 비워 두면 **기본 제공(항상 해금)** | 테스트 중엔 비워 두는 게 편하다 |
| `Third Person` → `Skeletal Mesh` 또는 `Static Mesh` | 부착할 메시 | |
| `Third Person` → `Socket Name` | 붙을 소켓 이름 | ★없으면 캐릭터 원점(발밑)에 붙는다★ |
| `First Person` | **비워 둔다** | 1인칭에선 자기 모자가 안 보인다 |
| `Preview Icon` | 아직 없으면 비워 둔다 | 칸이 비어 보일 뿐 동작엔 문제없다 |

> **메시가 아직 없다면**: 아무 스태틱 메시(큐브 등)로 항목 하나만 만들어도
> 옷장 UI 전체 흐름을 확인할 수 있다.

### 10-2. 연결 ★이걸 빼먹으면 목록이 안 나온다★

`Content/Blueprints/Progression/DA_ProgressionConfig` 를 열고
**`Cosmetic Catalog` 에 `DA_CosmeticCatalog`** 를 지정한다.

> `UDRGameInstance::GetCosmeticCatalog()` 가 `ProgressionConfig->CosmeticCatalog` 를 읽는다.
> GameInstance BP 에 새 슬롯을 늘리지 않고 여기에 매달아 두었다(§4.4).

### 10-3. 소켓 추가

부착물이 제 위치에 붙으려면 **캐릭터 스켈레톤에 소켓이 있어야 한다.**
로봇의 스켈레탈 메시 에셋을 열고 → Skeleton Tree → 원하는 본 우클릭 → **Add Socket** →
이름을 `Cosmetic_Head` / `Cosmetic_Face` / `Cosmetic_Body` / `Cosmetic_Tail` 로.

---

## STEP 11 — `BP_Wardrobe` 액터 + 레벨 배치

**위치**: `Content/Blueprints/Actor/Lobby/`
**만들기**: 우클릭 → Blueprint Class → All Classes → **`DRWardrobe`**

### 11-1. 컴포넌트 설정

C++ 이 준 3개(`WardrobeMesh` 루트 / `InteractionBox` / `InteractionWidget`)를 채운다:

| 컴포넌트 | 설정 |
|---|---|
| `WardrobeMesh` | 옷장 스태틱 메시 지정 |
| `InteractionBox` | Box Extent 를 옷장 크기에 맞게 |
| `InteractionWidget` | Widget Class = 프롬프트 위젯 (없으면 비워 둬도 동작) |

### 11-2. (선택) BP 이벤트

```
[Event On Local Player Entered Range]   ← ① Override ▼,  입력: b Has Any Skin
   └─▶ 프롬프트 문구 갱신 / 문 열림 애니메이션

[Event On Local Player Left Range]      ← ①
[Event On Interact Blocked]             ← ①  (고를 옷이 하나도 없을 때)
```

### 11-3. 레벨 배치

`LobbyMap` 의 **FreeRoam 구역** (업그레이드 장치 근처)에 드래그해 놓는다.

> ★대기실(WaitingRoom)에는 놓을 수 없다★ — 그 상태에서는 플레이어 폰 자체가 없다(§1.4).

---

## STEP 12 — 동작 확인 순서

에디터에서 `LobbyMap` PIE 실행 후, 콘솔(`~`)에 순서대로:

| # | 명령 / 동작 | 기대 결과 | 실패 시 확인 |
|---|---|---|---|
| 1 | `DRDumpCosmetic` | 로그에 `카탈로그=지정됨` | STEP 10-2 연결 |
| 2 | `DROpenWardrobe` | 옷장 화면이 뜬다 | STEP 9-1 위젯 클래스 지정 |
| 3 | 탭 4개 클릭 | 필이 옮겨 다닌다 | STEP 8-2 의 Category 지정 |
| 4 | 칸 목록 | 첫 칸이 "기본", 그 뒤로 카탈로그 항목 | STEP 8-5 배선 |
| 5 | 좌측 3D 캐릭터 | 캐릭터가 보인다 | 스테이지 배치(STEP 7-4), `RenderTarget` 지정, 조명 |
| 6 | 드래그 | 캐릭터가 돈다 | `Img_Preview` 의 Visibility = Visible |
| 7 | 칸 클릭 | 그 자리에서 옷이 바뀐다 | 소켓 이름(STEP 10-3), 메시 지정 |
| 8 | `C` 키 | 전부 벗는다 | — |
| 9 | `ESC` | 닫힌다 | — |
| 10 | `DRUnlockSkins` 후 재확인 | 잠긴 칸이 열린다 | `Required Achievements` 설정 |
| 11 | PIE 재시작 | **고른 옷이 유지된다** | 세이브 정상 |

### 자주 걸리는 것

| 증상 | 원인 |
|---|---|
| 칸이 하나도 안 나온다 (기본 칸만) | `DA_ProgressionConfig.CosmeticCatalog` 미지정 (STEP 10-2) |
| 프리뷰가 검은 사각형 | 스테이지에 **조명이 없다** (STEP 7-1) |
| 프리뷰가 아예 안 보인다 | 레벨에 `BP_CosmeticPreviewStage` 미배치 → 로그에 경고가 찍힌다 |
| 프리뷰 배경이 검게 채워진다 | `M_CosmeticPreview` 의 `1 - A` 반전 누락 (STEP 5-2) 또는 RT 포맷이 RGBA8 |
| 드래그가 안 먹는다 | `Img_Preview` Visibility 가 `Self Hit Test Invisible` |
| 부착물이 발밑에 박힌다 | 소켓 이름 오타/미생성 (STEP 10-3) — 로그에 경고가 찍힌다 |
| 클릭이 안 먹는다 | `Img_ScreenFX` 의 Visibility 가 `Visible` (→ `Self Hit Test Invisible` 로) |
| 탭 필이 안 옮겨진다 | 탭들의 `Category` 가 전부 같은 값 |
