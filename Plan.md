# DaeRune 페이즈 구조 개편 계획 (Plan.md)

## 1. 목표 요약

| 항목 | 현재 (As-Is) | 변경 후 (To-Be) |
|---|---|---|
| 페이즈 수 | **3개** (Phase1: 클렌저 확보 / Phase2: 부품 회수 / Phase3: 방어) | **2개** (New Phase1: 클렌저 확보 + 부품 회수 통합 / New Phase2: 기존 Phase3 방어) |
| 클렌저 사이트 주변 스폰 적 | `NormalEnemyClass` 1종(개)만 N마리 고정 | **`TArray<TSubclassOf<ADREnemy>> EnemiesToSpawn`** 배열로 명시. 배열 크기 = 스폰 수, 각 원소가 개·아르마딜로·잠자리 중 하나의 클래스 |
| DREnemySpawnGroup(통로 적) 구성 | 개 1종으로 레벨에 직접 배치 | **개·아르마딜로·잠자리를 레벨 디자이너가 자유롭게 배치** (코드 변경 없이 BP 인스턴스를 다양화) |
| Phase1 적 대미지 | 100% | **플레이어에게 주는 대미지 -30%** (즉 0.7배) — 사이트 주변 적과 통로 적 모두 |
| 부품 운반 적 | Phase2 전용 `PartCarryingEnemyClass`가 별도 `Phase2SpawnPoint`에 스폰 | **각 DREnemySpawnGroup(통로)에서 아르마딜로를 제외한 적 중 무작위 1마리**가 부품 휴대 |
| 부품 설치 후 흐름 | 사이트 2곳 × 부품 2개 = 4개 설치 → Phase3로 진행 | 사이트 1곳에 **부품 2개 고정** 설치 → New Phase2(방어)로 진행 |
| 필요 부품 수 | `RequiredPartsCount = 2` (사이트당) | **`RequiredPartsCount = 2` 고정** (변경 없음). DREnemySpawnGroup도 **정확히 2개**로 운영하여 1:1 매칭 |
| 클렌저 사이트 수 | 맵 배치 3개 + 활성화 2개 (현재 임시 코드로 1개 고정 중) | **맵 배치 1개 + 활성화 1개로 영구 고정** (정식 코드화) |

핵심 방향:
- 기존 `UDRPhase2` (부품 회수)를 `UDRPhase1` (클렌저 확보)에 통합 흡수하여 페이즈 클래스 1개 제거.
- 통로 적(DREnemySpawnGroup)이 부품 운반 책임을 가짐. 사이트 주변 적은 부품 없음.
- 사이트 주변 적 구성은 **배열 기반**으로 명시 (가중치 무작위 X). 통로 적 구성은 **레벨 배치 기반** (코드 무작위 X).
- 임시로 들어가 있던 `IndexToKeep = 0` 류의 "1개 고정" 코드를 정식 코드로 승격.

---

## 2. 영향받는 파일 매트릭스

| 영역 | 파일 | 변경 유형 |
|---|---|---|
| 페이즈 통합 | `Source/DaeRune/Public/Phase/DRPhase1.h` | `EnemiesToSpawn` 배열, 부품/사이트 완료 로직, DREnemySpawnGroup 부품 운반자 선정 헬퍼 추가 |
| 페이즈 통합 | `Source/DaeRune/Private/Phase/DRPhase1.cpp` | `SpawnEnemyGroupAtCleanserSite` 배열 기반 재구성, 통로 그룹 자동 수집·부품 운반자 선정, `OnPartInstalled` 흡수 |
| 페이즈 삭제 | `Source/DaeRune/Public/Phase/DRPhase2.h` | 삭제 (한 사이클 dead-code로 둘 경우 deprecate 주석만) |
| 페이즈 삭제 | `Source/DaeRune/Private/Phase/DRPhase2.cpp` | 삭제 |
| 페이즈 인덱스 표기 | `Source/DaeRune/Public/Phase/DRPhase3.h` / `.cpp` | `SetupPhaseObjective(3)` → `SetupPhaseObjective(2)` 한 줄 변경. 로직 무변경 |
| 게임 모드 흐름 | `Source/DaeRune/Public/Game/DRStageGameMode.h` | 변경 없음 (필요 시 헬퍼 추가) |
| 게임 모드 흐름 | `Source/DaeRune/Private/Game/DRStageGameMode.cpp` | `InitializePhaseSystem`의 `CleanserSites.Num() < 3` → `< 1` / `ValidatePhaseCompletion` switch 케이스 재정렬 (Phase2 케이스 삭제, 기존 Phase3 케이스를 인덱스 1로 이동) |
| 게임 스테이트 | `Source/DaeRune/Public/Game/DRStageGameState.h` / `.cpp` | 변경 없음. 기존 `CollectedParts`, `bCleanserActivated`, `bCleanserAreaSecured` 필드 재사용 |
| 클렌저 사이트 | `Source/DaeRune/Public/Actor/DRCleanserSite.h` / `.cpp` | **변경 없음**. `RequiredPartsCount = 2` 기본값 그대로 사용 (런타임 동적 변경 X). `Phase1EnemySpawnOffsets`는 그대로 사용 (배열 인덱스가 `EnemiesToSpawn` 인덱스와 1:1 매칭) |
| 스폰 그룹 | `Source/DaeRune/Public/Actor/DREnemySpawnGroup.h` / `.cpp` | 부품 운반자 선정 결과 노출 (`GetPartCarrierEnemy`), 그룹 내 등록된 적 조회 API 추가 |
| 적 | `Source/DaeRune/Public/Character/DREnemy.h` / `.cpp` | `bCarriesPart` 복제 보장 (`ReplicatedUsing` 점검), 부품 휴대 setter 정비. **BP_Dog/Armadillo/Dragonfly의 PartMeshComponent에 부품 StaticMesh 에셋 사전 지정** (BP 작업) |
| GAS 대미지 계산 | `Source/DaeRune/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp` | "Phase1 적 → 플레이어" 케이스에 0.7배 곱 |
| 게임플레이 태그 | `Source/DaeRune/Public/DRGameplayTags.h` / `.cpp` | `State.Enemy.Phase1` 신규 태그 추가 |
| BP 자산 | `BP_DRStageGameMode` (PhaseClasses 배열) | 인덱스 1의 `BP_DRPhase2` 제거, 인덱스 1 = `BP_DRPhase3`로 이동 |
| BP 자산 | `BP_DRPhase1` 자식 BP | `EnemiesToSpawn` 배열 입력, Dog/Armadillo/Dragonfly 클래스 참조 설정 |
| 데이터 자산 | `DT_PhaseObjective` | PhaseNumber=1 문구 통합, PhaseNumber=2 행 신규(또는 기존 3 → 2 이동) |
| 레벨 데이터 | `Content/Maps/TestMap1.umap` (및 본 맵) | CleanserSite 1개만 남기고 제거 / 통로의 DREnemySpawnGroup에 개·아르마딜로·잠자리 BP 인스턴스 다양화 배치 / 사이트의 `Phase1EnemySpawnOffsets`(스폰 위치) 배열 수 = `EnemiesToSpawn` 배열 수와 일치하도록 정리 |

---

## 3. 상세 설계

### 3.1. 페이즈 인덱스 및 클래스 매핑

**As-Is**:
```
PhaseInstances[0] = UDRPhase1 (확보)
PhaseInstances[1] = UDRPhase2 (부품)
PhaseInstances[2] = UDRPhase3 (방어)
```

**To-Be**:
```
PhaseInstances[0] = UDRPhase1 (확보 + 부품 통합)
PhaseInstances[1] = UDRPhase3 (방어, UI상 "페이즈 2"로 표기)
```

- C++ 클래스 이름인 `UDRPhase3`는 그대로 둔다 (Wave/엘리트/독가스/BGM 로직 코드량이 많고 리네임 이득 < 리스크).
- BP 측 `BP_DRStageGameMode::PhaseClasses` 배열에서 `BP_DRPhase2`를 제거하고 `BP_DRPhase3`를 인덱스 1로 이동.
- UI에 표기되는 "페이즈 X" 번호는 `CurrentPhaseIndex`(0-based) + 1을 사용하므로 인덱스 재정렬만으로 자동 갱신.
- `UDRPhase3::OnPhaseStart`에서 `SetupPhaseObjective(3)` 호출 인자를 `SetupPhaseObjective(2)`로 변경 → `DT_PhaseObjective`의 PhaseNumber=2 행을 참조.

### 3.2. New Phase1 (UDRPhase1) 상세

#### 3.2.1. 책임
1. 맵 배치 클렌저 사이트 1개를 활성화한다 (맵에 1개만 있다는 전제, 안전망으로 ≥2개일 경우 첫 번째만 유지).
2. **사이트 주변 적**을 `EnemiesToSpawn` 배열에 따라 정확히 배열 길이만큼 스폰한다 (각 인덱스의 클래스를 `Phase1EnemySpawnOffsets[i]` 위치에 스폰).
3. **통로 적**은 레벨 디자이너가 이미 DREnemySpawnGroup 액터의 `PrePlacedEnemies`/하위 ADREnemy 인스턴스로 다양하게 배치한 상태. 페이즈가 별도로 스폰하지 않는다.
4. 사이트 주변 적과 모든 통로 적의 ASC에 `State.Enemy.Phase1` 루즈 태그를 부여 → `ExecCalc_Damage`에서 0.7배 곱.
5. 각 DREnemySpawnGroup에서 `ArmadilloEnemyClass`가 아닌 적 중 1마리를 무작위로 선정하여 부품 휴대(`bCarriesPart = true`, `PartActorClass` 세팅, 부품 메시 가시화).
6. 클렌저 사이트의 `RequiredPartsCount = 2` 고정. 본 페이즈는 사이트 값을 변경하지 않는다. 대신 **레벨에 정확히 2개의 DREnemySpawnGroup이 배치되어 있어야 한다는 전제**를 유지하여 1:1 매칭. (그룹 수가 2를 초과할 경우 §3.2.5의 fallback 정책 적용)
7. `OnPartInstalled` 델리게이트를 직접 구독하여 설치 진행률 갱신 및 완료 시 `ValidatePhaseCompletion()` 호출.

#### 3.2.2. 헤더 필드 변경 (`DRPhase1.h`)

기존 `EliteEnemyClass`, `NormalEnemyClass`, `NormalEnemyCount`, `EliteSpawnOffset` 제거.

```cpp
// ========== 사이트 주변 스폰 ==========
// 클렌저 사이트 주변에 스폰할 적 클래스 배열.
// 배열의 i번째 원소가 사이트의 Phase1EnemySpawnOffsets[i] 위치에 스폰된다.
// 배열 크기와 사이트의 스폰 위치 수가 일치해야 한다.
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Spawn")
TArray<TSubclassOf<ADREnemy>> EnemiesToSpawn;

// ========== 통로(DREnemySpawnGroup) 부품 운반자 선정 ==========
// 부품 휴대 대상에서 제외할 적 클래스(아르마딜로). 정확히 이 클래스(자식 포함) 인스턴스는 운반자 후보에서 제외된다.
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Parts")
TSubclassOf<ADREnemy> ArmadilloEnemyClass;

// 부품 운반자에게 세팅할 부품 액터 클래스 (DREnemy::PartActorClass와 동일하게 세팅)
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Parts")
TSubclassOf<AActor> PartActorClass;

// ========== 대미지 감소 태그 ==========
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase1|Combat")
FGameplayTag Phase1EnemyTag;  // 기본값 = State.Enemy.Phase1

// ========== 런타임 캐시 ==========
// 월드에서 자동 수집한 통로 스폰 그룹들
UPROPERTY()
TArray<TObjectPtr<ADREnemySpawnGroup>> SpawnGroups;

// 그룹별 부품 운반자
UPROPERTY()
TMap<TObjectPtr<ADREnemySpawnGroup>, TWeakObjectPtr<ADREnemy>> PartCarrierByGroup;

// 활성화된 단일 클렌저 사이트 (편의 캐시)
UPROPERTY()
TWeakObjectPtr<ADRCleanserSite> ActiveSite;
```

`DRPhase1.h`의 protected 함수:
```cpp
void KeepSingleCleanserSite();
void SpawnEnemiesAroundCleanserSite(ADRCleanserSite* Site);
void CollectSpawnGroups();
void AssignPartCarriersForAllGroups();
ADREnemy* PickPartCarrierFromGroup(ADREnemySpawnGroup* Group) const;
void ApplyPhase1Tag(ADREnemy* Enemy) const;
UFUNCTION() void OnPartInstalled(ADRCleanserSite* Site);
```

#### 3.2.3. OnPhaseStart 흐름

```text
OnPhaseStart()
├── SetupPhaseObjective(1)
├── GameState->SetCollectedParts(0)
├── GameState->SetCleanserActivated(false)
├── KeepSingleCleanserSite()
│       └── CleanserSites 배열 중 인덱스 0만 남기고 나머지 Destroy + 배열에서 제거
│       └── SelectedCleanserSites = CleanserSites
│       └── SetActiveCleanserSites(SelectedCleanserSites)
│       └── GameState->SetCleanserSites(SelectedCleanserSites)
│       └── ActiveSite = CleanserSites[0]
│       └── ActiveSite->ActivateSite()
│       └── ActiveSite->OnPartInstalled.AddDynamic(this, &UDRPhase1::OnPartInstalled)
├── SpawnEnemiesAroundCleanserSite(ActiveSite.Get())
│       └── 사이트의 GetPhase1EnemySpawnLocations() 가져오기
│       └── EnemiesToSpawn 배열과 위치 배열 중 작은 쪽 길이만큼 루프
│       └── 각 인덱스 i: SpawnEnemy(EnemiesToSpawn[i], Locations[i])
│       └── 스폰된 적 → SpawnedEnemies 등록, OnDeath 델리게이트 바인딩, ApplyPhase1Tag
├── CollectSpawnGroups()
│       └── TActorIterator<ADREnemySpawnGroup>로 월드 탐색
│       └── 각 그룹 등록 + 그룹 내 PrePlacedEnemies(이미 BeginPlay에서 RegisterEnemy 완료)에 ApplyPhase1Tag
├── AssignPartCarriersForAllGroups()
│       └── for (Group : SpawnGroups) carrier = PickPartCarrierFromGroup(Group); PartCarrierByGroup.Add(Group, carrier)
│       └── 그룹 수가 2가 아닐 경우 경고 로그 (사이트 RequiredPartsCount=2 고정과 불일치)
└── GameState 진행률 초기화
    └── FPhaseObjectiveData Obj = GameState->GetCurrentPhaseObjective();
        Obj.RequiredCount = ActiveSite->RequiredPartsCount;   // = 2 (사이트 BP 기본값)
        GameState->SetPhaseObjective(Obj);
    └── GameState->UpdatePhaseObjectiveProgress(0)
```

#### 3.2.4. 사이트 주변 적 스폰 (배열 기반)

요구사항: "배열로 해서 어떤 적을 배열의 수만큼 스폰시킬지 정하고 싶어"

```cpp
void UDRPhase1::SpawnEnemiesAroundCleanserSite(ADRCleanserSite* Site)
{
    if (!Site) return;

    const TArray<FVector> Locations = Site->GetPhase1EnemySpawnLocations();
    const int32 SpawnCount = FMath::Min(EnemiesToSpawn.Num(), Locations.Num());

    if (EnemiesToSpawn.Num() != Locations.Num())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Phase1: EnemiesToSpawn (%d) and CleanserSite spawn locations (%d) mismatch. Spawning %d."),
            EnemiesToSpawn.Num(), Locations.Num(), SpawnCount);
    }

    for (int32 i = 0; i < SpawnCount; ++i)
    {
        TSubclassOf<ADREnemy> EnemyClass = EnemiesToSpawn[i];
        if (!EnemyClass) continue;

        AActor* SpawnedActor = SpawnEnemy(EnemyClass, Locations[i]);
        if (ADREnemy* Spawned = Cast<ADREnemy>(SpawnedActor))
        {
            ApplyPhase1Tag(Spawned);
        }
    }
}
```

- 배열 길이 ↔ 사이트 측 `Phase1EnemySpawnOffsets` 길이 미스매치 시 최소값만큼만 스폰하고 경고. (스폰 위치 부족 또는 배열 부족을 즉시 인지)
- 사이트 주변 적은 부품을 휴대하지 않는다 (요구사항: 부품은 DREnemySpawnGroup의 통로 적에게서만 회수).

#### 3.2.5. 통로 적 부품 운반자 선정

레벨 디자이너가 DREnemySpawnGroup에 `PrePlacedEnemies`로 개·아르마딜로·잠자리 BP 인스턴스를 다양하게 배치한다는 전제. 그룹의 `BeginPlay`에서 이미 `RegisterEnemy` 호출이 이루어진다.

```cpp
ADREnemy* UDRPhase1::PickPartCarrierFromGroup(ADREnemySpawnGroup* Group) const
{
    if (!Group) return nullptr;

    // 그룹 내 살아있는 적 목록 (DREnemySpawnGroup이 노출해야 함, §3.4 참조)
    TArray<ADREnemy*> AliveEnemies = Group->GetRegisteredEnemies();

    // 아르마딜로 제외 후보 추리기
    TArray<ADREnemy*> Candidates;
    Candidates.Reserve(AliveEnemies.Num());
    for (ADREnemy* Enemy : AliveEnemies)
    {
        if (!Enemy || !IsValid(Enemy)) continue;
        if (ArmadilloEnemyClass && Enemy->IsA(ArmadilloEnemyClass)) continue;
        Candidates.Add(Enemy);
    }

    if (Candidates.Num() == 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Phase1: SpawnGroup %s has no non-Armadillo carrier candidates. Skipping part assignment."),
            *Group->GetName());
        return nullptr;
    }

    const int32 PickedIndex = FMath::RandRange(0, Candidates.Num() - 1);
    ADREnemy* Carrier = Candidates[PickedIndex];

    if (Carrier)
    {
        Carrier->PartActorClass = PartActorClass;
        Carrier->bCarriesPart = true;
        if (Carrier->PartMeshComponent)
        {
            Carrier->PartMeshComponent->SetVisibility(true);
        }
        // Replication: bCarriesPart가 ReplicatedUsing이면 OnRep에서 클라이언트에서도 가시화
    }
    return Carrier;
}
```

**부품 운반자 초기화 흐름 — 기존 Phase2와의 비교**:

| 항목 | 기존 Phase2 (As-Is) | 새 Phase1 (To-Be) |
|---|---|---|
| 운반자 결정 시점 | `PartCarryingEnemyClass` BP를 spawn하는 순간 (스폰=운반자 1:1) | DREnemySpawnGroup 등록 적 중 런타임 무작위 선정 |
| `bCarriesPart` 세팅 | BP 기본값에서 `true` 사전 설정 | C++ 런타임 setter (`Carrier->bCarriesPart = true`) |
| `PartActorClass` 세팅 | BP 기본값에서 사전 설정 | C++ 런타임 setter (`Carrier->PartActorClass = PartActorClass`) |
| `PartMeshComponent` StaticMesh 에셋 | BP에서 사전 지정 | **개·아르마딜로·잠자리 모든 BP의 PartMeshComponent에 동일한 부품 StaticMesh 에셋을 미리 지정** (BP 사전 설정) ★ |
| 메시 가시화 | `BeginPlay`의 `if (bCarriesPart) SetVisibility(true)` 자동 처리 | Phase1 OnPhaseStart는 BeginPlay 이후에 실행되므로 자동 처리가 안 됨 → 운반자 선정 시 C++ 런타임에서 직접 `SetVisibility(true)` 호출. 클라이언트는 `OnRep_bCarriesPart`에서 동일하게 처리 |
| 드롭 시 부품 액터 생성 | `DropPart()` → `PartActorClass`로 SpawnActor (기존 흐름) | **변경 없음** — 동일 흐름 유지. UDRPhase1이 `PartActorClass`만 정확히 주입하면 됨 |
| 픽업/설치 | `ADRCleanserPart::PickupPart`/`InstallPart` (기존 흐름) | **변경 없음** |

★ **PartMesh 에셋 정책 (결정됨)**: BP_Dog, BP_Armadillo, BP_Dragonfly 각각의 `PartMeshComponent`에 부품 StaticMesh 에셋(예: SM_CleanserPart)을 미리 지정해둔다. 가시성만 false로 유지하면 됨. 런타임에 `SetVisibility(true)`만 호출하면 메시가 즉시 보임.

→ 이렇게 하면 기존 Phase2와 **시각적·기능적으로 동일한 결과**(부품 메시 표시 + DropPart 정상 동작 + Pickup/Install 정상 동작)를 보장한다. 차이는 "BP가 가지고 태어났느냐 vs 런타임에 부여받느냐"뿐이며, 부품 시스템(드롭, 픽업, 설치)은 기존 코드 경로를 그대로 사용한다.

**중요 제약**:
- 그룹 후보가 0이면 부품 미할당 → 레벨 디자이너 경고. 페이즈 완료 불가 상태가 되므로 레벨 검수 단계에서 차단해야 함.
- 후보가 1마리라도 있으면 그 1마리에 부품 부여 (요구사항 충족).
- BP_Dog/Armadillo/Dragonfly의 `PartMeshComponent`에 StaticMesh 에셋이 누락되면, `SetVisibility(true)` 호출에도 메시가 보이지 않음 → BP 검수 단계에서 차단해야 함.
- **그룹 수 vs RequiredPartsCount 정합성**:
  - 사이트 `RequiredPartsCount = 2` 고정이므로 그룹 수가 **정확히 2**일 때 가장 안전.
  - 그룹 수 < 2: 부품 부족으로 페이즈 완료 불가 → 레벨 단계에서 차단.
  - 그룹 수 > 2: 처음 2개 그룹만 부품 운반자를 가지도록 제한 (fallback 정책). 나머지 그룹은 일반 통로 적으로만 동작.
  - 즉 `AssignPartCarriersForAllGroups` 내에서 `MaxCarrierGroups = ActiveSite->RequiredPartsCount` 만큼만 운반자 선정 루프를 돌린다.

#### 3.2.6. Phase1 대미지 감소 태그 부여

```cpp
void UDRPhase1::ApplyPhase1Tag(ADREnemy* Enemy) const
{
    if (!Enemy) return;
    if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
    {
        ASC->AddLooseGameplayTag(Phase1EnemyTag);
        // 서버 권위 GAS이므로 ExecCalc는 서버에서만 실행. 단, 안전을 위해 Replicated Loose 사용 권장:
        // ASC->AddReplicatedLooseGameplayTag(Phase1EnemyTag);
    }
}
```

- 사이트 주변 적: 스폰 직후 `ApplyPhase1Tag` 호출.
- 통로 적: `CollectSpawnGroups` 단계에서 각 그룹의 등록된 모든 적에 호출 (아르마딜로 포함, 부품 운반자 여부 무관).

#### 3.2.7. 사이트 1개 강제

기존 임시 코드 (DRPhase1.cpp 83~94행):
```cpp
const int32 IndexToKeep = 0;
for (int32 i = CleanserSites.Num() - 1; i >= 0; --i)
{
    if (i == IndexToKeep) continue;
    /* Destroy + RemoveAt */
}
```

→ `KeepSingleCleanserSite()` 함수로 분리하여 정식 함수로 승격. 주석/[임시] 표기 제거.

**레벨 정책**:
- TestMap1을 비롯한 모든 본 맵에서 사이트 액터는 1개만 배치한다.
- 코드는 안전망으로만 동작 (2개 이상이면 첫 번째만 유지, 0개면 경고).

**InitializePhaseSystem 가드** (`DRStageGameMode.cpp`):
- 현재: `if (CleanserSites.Num() < 3) return;`
- 변경: `if (CleanserSites.Num() < 1) return;`

#### 3.2.8. 부품 설치 및 완료 처리

기존 `UDRPhase2::OnPartInstalled` 로직을 그대로 흡수:

```cpp
void UDRPhase1::OnPartInstalled(ADRCleanserSite* Site)
{
    if (!Site || !GameState) return;

    const int32 Installed = Site->GetInstalledPartsCount();
    GameState->SetCollectedParts(Installed);
    GameState->UpdatePhaseObjectiveProgress(Installed);

    if (Site->IsPartInstallationComplete())
    {
        GameState->SetCleanserActivated(true);
        if (GameMode) GameMode->ValidatePhaseCompletion();
    }
}
```

`OnPhaseEnd`:
```cpp
void UDRPhase1::OnPhaseEnd()
{
    if (ActiveSite.IsValid())
    {
        ActiveSite->OnPartInstalled.RemoveDynamic(this, &UDRPhase1::OnPartInstalled);
    }
    SpawnGroups.Empty();
    PartCarrierByGroup.Empty();
    if (ADRDoorManager* DoorMgr = GetDoorManager())
    {
        DoorMgr->OnPhase1Ended();
    }
    Super::OnPhaseEnd();
}
```

`OnEnemyDeath`:
- 기존 "모든 적 처치 = 페이즈 완료" 의미는 **삭제**. 페이즈 완료는 부품 설치로만 판정.
- 다만 통로 그룹 적이 모두 죽으면 문이 부서지는 기존 동작은 DREnemySpawnGroup의 `OnAllEnemiesDead` 델리게이트가 처리하므로 이 페이즈 코드에서는 신경 쓰지 않는다.
- 사이트 주변 적의 진행률은 더 이상 ObjectiveProgress로 표시하지 않는다 (ObjectiveProgress는 `CollectedParts`로 갱신).

### 3.3. 대미지 -30% 적용 (`ExecCalc_Damage`)

`ExecCalc_Damage::Execute_Implementation` 내, Damage 합산 직후·OutputModifier 직전에 추가:

```cpp
// New Phase1 적 → 플레이어 대미지 0.7배
if (SourceASC && SourceASC->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Enemy_Phase1))
{
    if (TargetASC && TargetASC->HasAttributeSetForAttribute(UDRAttributeSet::GetIncomingDamageAttribute()))
    {
        Damage *= 0.7f;
    }
}
```

- 엘리트 룬 버프(`Buff.Elite.Roar` → ×1.2)와 곱셈 순서는 결과적으로 동일(모두 곱).
- 타겟이 `UDRCleanserSiteAttributeSet` 보유(클렌저 사이트)인 경우에는 적용하지 않는다. → 요구사항 "플레이어에게 주는 대미지 30%감소"에 부합.
- 만약 향후 "모든 타겟에 적용"으로 해석이 바뀌면 타겟 조건을 제거.

**태그 정의** (`DRGameplayTags`):
```cpp
// .h
FGameplayTag State_Enemy_Phase1;

// .cpp InitializeNativeGameplayTags()
GameplayTags.State_Enemy_Phase1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("State.Enemy.Phase1"),
    FString("Enemy belongs to New Phase1; deals 30% reduced damage to players."));
```

### 3.4. DREnemySpawnGroup 보강

요구사항: "다 죽으면 문이 부서지게 하던 그 적들" = 기존 PrePlacedEnemies 기반 통로 시스템 그대로 유지. 코드 신규 스폰 없음.

추가 노출 API (`DREnemySpawnGroup.h`):
```cpp
public:
    // UDRPhase1이 부품 운반자 후보를 추리기 위해 사용
    UFUNCTION(BlueprintCallable, Category = "SpawnGroup")
    TArray<ADREnemy*> GetRegisteredEnemies() const;

    // (선택) 운반자 캐시 — 디버깅/UI 연동용
    UFUNCTION(BlueprintPure, Category = "SpawnGroup")
    ADREnemy* GetPartCarrierEnemy() const { return PartCarrierEnemy.Get(); }

    void SetPartCarrierEnemy(ADREnemy* Carrier) { PartCarrierEnemy = Carrier; }

private:
    UPROPERTY()
    TWeakObjectPtr<ADREnemy> PartCarrierEnemy;
```

`GetRegisteredEnemies` 구현:
```cpp
TArray<ADREnemy*> ADREnemySpawnGroup::GetRegisteredEnemies() const
{
    TArray<ADREnemy*> Out;
    Out.Reserve(RegisteredEnemies.Num());
    for (const TWeakObjectPtr<ADREnemy>& Weak : RegisteredEnemies)
    {
        if (ADREnemy* E = Weak.Get())
        {
            Out.Add(E);
        }
    }
    return Out;
}
```

- 기존 `PrePlacedEnemies`, `RegisterEnemy`, `OnAllEnemiesDead` 델리게이트, 문 부서지는 연동 로직은 **변경 없음**.
- UDRPhase1이 `CollectSpawnGroups` 단계에서 그룹별 `GetRegisteredEnemies`를 호출하여 후보 선정에만 사용.

### 3.5. CleanserSite 측 변경

- **변경 없음**. `RequiredPartsCount = 2` 기본값 그대로. UDRPhase1은 사이트의 부품 수 값을 절대 변경하지 않는다.
- `InstalledPartMesh1`, `InstalledPartMesh2` 두 슬롯 메시와 정확히 매칭되는 2개 부품 흐름이 변경 없이 유지된다.
- 운영 전제: **레벨에 DREnemySpawnGroup이 정확히 2개 배치되어 있어야 한다**. (각 그룹이 부품 1개를 공급 → 사이트 2개 슬롯에 1:1)
- `Phase1EnemySpawnOffsets` 필드는 그대로 사용. `EnemiesToSpawn` 배열 크기와 1:1 매칭.
- `OnPartInstalled` 델리게이트는 UDRPhase1이 직접 구독.

### 3.6. GameMode 변경

`DRStageGameMode.cpp`:

1. `InitializePhaseSystem`의 가드:
   ```cpp
   if (CleanserSites.Num() < 1) return;  // 기존: < 3
   ```

2. `ValidatePhaseCompletion` switch 케이스 재정렬:
   ```cpp
   switch (CurrentPhaseIndex)
   {
   case 0: // New Phase1: 부품 2개 설치 + 사이트 활성화
   {
       const int32 CollectedParts = CachedGameState->GetCollectedParts();
       const bool bActivated = CachedGameState->IsCleanserActivated();
       bIsCompleted = (CollectedParts >= 2) && bActivated;
   }
   break;

   case 1: // New Phase2 (= 기존 Phase3): 웨이브 종료
   {
       const int32 CurrentWaveNumber = CachedGameState->GetCurrentWaveNumber();
       const int32 TotalWaves = CachedGameState->GetTotalWaves();
       bIsCompleted = CurrentWaveNumber >= TotalWaves;
   }
   break;

   default:
       break;
   }
   ```
   - 기존 case 1 (부품) / case 2 (방어) / case 3 (보스) 모두 제거하고 위 두 케이스로 정리.
   - 페이즈 수가 2이므로 보스 케이스는 본 PR에서 다루지 않는다.

3. 부품 필요 수는 **2 고정**(`CleanserSite::RequiredPartsCount` 기본값과 일치). UDRPhase1은 사이트의 값을 변경하지 않으며, `FPhaseObjectiveData::RequiredCount`도 사이트의 `RequiredPartsCount` 값을 읽어 동일하게 세팅한다.

### 3.7. UI / DT_PhaseObjective

- `DT_PhaseObjective`:
  - PhaseNumber=1 행: 제목/문구를 통합 메시지로 수정. 예: "클렌저 부품을 회수해 사이트에 설치하라". `RequiredCount`는 런타임에서 그룹 수로 덮어쓰므로 기본값은 임의 값 가능.
  - PhaseNumber=2 행: 기존 PhaseNumber=3 행의 내용(예: "5분간 방어하라" / 웨이브 수 등)을 옮긴다.
  - PhaseNumber=3 행 삭제(또는 보존만).
- `UDRPhase3::SetupPhaseObjective(3)` → `SetupPhaseObjective(2)`.

### 3.8. BP / 데이터 작업

- `BP_DRStageGameMode::PhaseClasses`
  - 인덱스 0: `BP_DRPhase1` (그대로)
  - 인덱스 1: `BP_DRPhase3` (`BP_DRPhase2`를 제거하고 이동)
- `BP_DRPhase1` 자식 BP에서:
  - `EnemiesToSpawn` 배열에 `[BP_Dog, BP_Armadillo, BP_Dragonfly, ...]` 등 원하는 구성을 입력 (배열 길이 = 사이트 측 `Phase1EnemySpawnOffsets` 길이). 사이트 주변 적 수는 부품 수(2)와 무관 — 자유롭게 설정 가능.
  - `ArmadilloEnemyClass = BP_Armadillo`
  - `PartActorClass = BP_CleanserPart`
  - `Phase1EnemyTag = State.Enemy.Phase1`
- `BP_CleanserSite`의 `Phase1EnemySpawnOffsets` 배열 길이를 `EnemiesToSpawn`와 동일하게 맞춤.
- TestMap1:
  - CleanserSite 인스턴스 1개만 남기기
  - 통로 DREnemySpawnGroup을 **정확히 2개** 배치 (사이트 `RequiredPartsCount=2` 고정과 1:1 매칭)
  - 각 그룹의 `PrePlacedEnemies`에 개·아르마딜로·잠자리 BP 인스턴스 다양화 배치
  - 각 그룹에 비-아르마딜로 인스턴스가 최소 1개 이상 포함되도록 검수

---

## 4. 단계별 작업 순서 (Execution Plan)

### Step 1 — 게임플레이 태그 + 대미지 감소 (독립적, 우선)
1. `DRGameplayTags.h/.cpp`에 `State_Enemy_Phase1` 추가.
2. `ExecCalc_Damage.cpp`에 SourceASC 태그 체크 → 0.7배 곱 (타겟이 `UDRAttributeSet` 보유 시).
3. 빌드. 단일 적의 ASC에 콘솔로 태그 부여 후 PIE에서 대미지가 정확히 0.7배인지 확인.

### Step 2 — DREnemySpawnGroup API 추가
1. `GetRegisteredEnemies()`, `GetPartCarrierEnemy()`, `SetPartCarrierEnemy()` 추가.
2. 빌드 + 기존 PrePlacedEnemies 흐름 회귀 테스트 (그룹 적 사망 → 문 부서짐 동작 유지).

### Step 3 — DREnemy 복제 점검 + PartMesh BP 사전 설정
1. `bCarriesPart`가 `ReplicatedUsing=OnRep_bCarriesPart`인지 확인. 누락이면 추가.
2. `OnRep_bCarriesPart`에서 `PartMeshComponent->SetVisibility(true)` 갱신이 일관되게 일어나는지 확인.
3. **BP_Dog, BP_Armadillo, BP_Dragonfly 각각의 `PartMeshComponent`에 부품 StaticMesh 에셋을 미리 지정** (가시성 false 기본). 기존 PartCarrying BP에서 사용하던 동일 에셋을 재사용.
4. 멀티플레이 PIE에서 서버가 `bCarriesPart=true`로 셋한 적이 클라이언트에서도 부품 메시를 보이는지 검증.

### Step 4 — UDRPhase1 전면 개편
1. 헤더 변경: `EnemiesToSpawn`, `ArmadilloEnemyClass`, `PartActorClass`, `Phase1EnemyTag`, `SpawnGroups`, `PartCarrierByGroup`, `ActiveSite` 추가. 기존 Normal/Elite 관련 필드 제거.
2. `KeepSingleCleanserSite()`, `SpawnEnemiesAroundCleanserSite()`, `CollectSpawnGroups()`, `AssignPartCarriersForAllGroups()`, `PickPartCarrierFromGroup()`, `ApplyPhase1Tag()`, `OnPartInstalled()` 구현.
3. `OnPhaseStart` 흐름을 §3.2.3대로 재구성.
4. `OnPhaseEnd`에서 델리게이트 해제 + 컬렉션 비움.
5. `OnEnemyDeath`는 ObjectiveProgress와 무관하게 단순 호출(또는 빈 구현)로 변경.

### Step 5 — UDRPhase2 제거
1. `BP_DRStageGameMode::PhaseClasses` 인덱스 1에서 `BP_DRPhase2` 제거.
2. C++ `UDRPhase2` 클래스 파일은 다음 PR에서 삭제 (BP 참조 잔재 정리 후). 본 PR에서는 헤더에 `UE_DEPRECATED` 주석만 추가하고 컴파일 가능한 상태로 둔다.

### Step 6 — UDRPhase3 표기 정정 + BP 인덱스 이동
1. `UDRPhase3::OnPhaseStart`의 `SetupPhaseObjective(3)` → `SetupPhaseObjective(2)`.
2. `BP_DRStageGameMode::PhaseClasses` 인덱스 1 = `BP_DRPhase3` 세팅.

### Step 7 — GameMode 검증/완료 케이스 재정렬
1. `InitializePhaseSystem`의 `< 3` → `< 1`.
2. `ValidatePhaseCompletion` switch를 §3.6대로 case 0, case 1만 남기도록 정리.

### Step 8 — CleanserSite RequiredPartsCount 검증 (동적 설정 없음)
1. 사이트 BP의 `RequiredPartsCount = 2` 기본값을 그대로 사용하고, UDRPhase1이 이 값을 변경하지 않는지 코드 리뷰로 재확인.
2. 부품 2개 설치 시 `IsPartInstallationComplete()`가 true, 슬롯 메시 `InstalledPartMesh1/2` 두 개가 모두 시각적으로 채워지는지 확인.
3. 레벨에 DREnemySpawnGroup이 정확히 2개 배치되어 있는지 검수. (그룹 수 ≠ 2이면 §3.2.5의 fallback 정책에 의해 정상 진행은 되지만 경고 로그가 떠야 함)

### Step 9 — DT_PhaseObjective 및 BP 자산 정리
1. PhaseNumber=1 문구 통합 메시지로 수정 (`RequiredCount`는 어차피 런타임 덮어쓰기).
2. PhaseNumber=2 행 추가 (기존 PhaseNumber=3 내용 이동).
3. `BP_DRPhase1` 자식 BP에 `EnemiesToSpawn`, `ArmadilloEnemyClass`, `PartActorClass`, `Phase1EnemyTag` 입력.

### Step 10 — 레벨 작업 (TestMap1)
1. CleanserSite 액터 1개만 남기고 나머지 제거.
2. CleanserSite의 `Phase1EnemySpawnOffsets` 길이를 `EnemiesToSpawn` 길이와 일치시킴.
3. 통로 DREnemySpawnGroup의 `PrePlacedEnemies`에 개·아르마딜로·잠자리 BP 인스턴스를 적절히 섞어 배치. 각 그룹에 비-아르마딜로 인스턴스가 최소 1개 이상 포함되도록.
4. 그룹 수가 2가 되도록 정리 (사이트 슬롯 메시 2개와 매칭).

### Step 11 — 통합 테스트 (Listen Server + Client 1)
1. PIE 시작 → 페이즈 인덱스 0에서 UDRPhase1 동작 확인.
2. 사이트 주변에 `EnemiesToSpawn` 배열의 클래스대로 정확한 위치에 스폰되는가?
3. 사이트 주변 적과 통로 적 모두 플레이어 대상 대미지가 0.7배인가? (Health 로그 확인)
4. 각 DREnemySpawnGroup에서 비-아르마딜로 적 정확히 1마리가 부품 메시를 표시하는가?
5. 통로 적 전원 사망 시 문 부서짐이 여전히 동작하는가?
6. 부품 픽업/설치 → 그룹 수만큼 설치 완료 시 New Phase2(웨이브) 진입.
7. New Phase2 동안 웨이브/엘리트/독가스/사이트 체력 시스템이 종전대로 동작.
8. HUD 페이즈 라벨이 "페이즈 1", "페이즈 2"로 표시.

### Step 12 — 정리
1. `UDRPhase2` C++ 클래스 파일 삭제 (다음 PR).
2. 기존 임시 주석/`[임시]` 표기 제거.
3. CLAUDE.md "Phase System" 섹션을 새 2-페이즈 구조로 갱신.

---

## 5. 리스크 및 결정 필요 항목

| 항목 | 리스크 | 권장 결정 |
|---|---|---|
| `EnemiesToSpawn` 길이 ≠ `Phase1EnemySpawnOffsets` 길이 | 일부 적 미스폰 or 일부 위치 미사용 | 런타임 경고 로그 + min(N,M)만큼만 스폰. 레벨 검수 단계에서 길이 일치를 보장 |
| DREnemySpawnGroup이 전원 아르마딜로 | 부품 미할당 → 페이즈 완료 불가 | 레벨 디자이너에게 "각 그룹에 비-아르마딜로 ≥1마리" 룰 명시. 코드는 경고만 |
| `bCarriesPart` 복제 누락 | 클라이언트에 부품 메시 미표시 | Step 3에서 점검 후 `ReplicatedUsing=OnRep_bCarriesPart` 보장 |
| 그룹 수 ≠ 2 | 사이트 부품 수 2 고정과 불일치 | 레벨에 그룹 정확히 2개 배치. 3+ 경우 fallback으로 앞쪽 2개 그룹만 부품 운반자 선정. < 2면 경고 + 페이즈 완료 불가 |
| `RequiredPartsCount` 의도치 않은 변경 | 사이트 BP 값과 다른 값으로 덮어쓰면 부품 흐름 깨짐 | UDRPhase1은 사이트 값을 **읽기만** 한다. 쓰기 금지를 코드 리뷰에서 강제 |
| 루즈 태그 vs 복제 루즈 태그 | 서버만 동작하는 ExecCalc에서는 루즈 태그 OK. 그러나 클라이언트 UI에서 태그 조회 시 누락 가능 | `AddReplicatedLooseGameplayTag` 사용 권장 |
| 임시 1개 코드의 정식화 | 다른 페이즈 (UDRPhase3 등)가 `ActiveCleanserSites.Num() == 2`를 가정하는 코드가 남아있을 수 있음 | DRPhase3의 `InitializeCleanserSite` / `InitializeActiveSpawnPoints`에서 사이트 수 가정 검증 후 1개 기준으로 동작하는지 점검 |
| `ValidatePhaseCompletion` 인덱스 기반 switch | 페이즈 추가/제거 시 case 누락 위험 | 본 PR 범위에서는 그대로. 폴리모픽 디스패치 리팩터는 별도 PR |
| `Phase1EnemySpawnOffsets` 의미 변경 | 기존 레벨 입력값이 새 의미(=배열 i번째 위치)와 어긋날 수 있음 | 레벨 작업 시 사이트별로 재정렬·재입력 |

---

## 6. 테스트 체크리스트

- [ ] PIE 시작 시 클렌저 사이트가 정확히 1개만 활성화된다 (다른 사이트는 Destroy됨).
- [ ] 사이트 주변에 `EnemiesToSpawn` 배열의 각 원소 클래스가 `Phase1EnemySpawnOffsets[i]` 위치에 정확히 1마리씩 스폰된다.
- [ ] 사이트 주변 적은 부품을 휴대하지 않는다 (부품 메시 비표시).
- [ ] 각 DREnemySpawnGroup 내에서 아르마딜로가 아닌 적 정확히 1마리만 부품 메시를 표시한다.
- [ ] 운반자 선정 직후 서버·클라이언트 양쪽에서 부품 메시가 즉시 보인다 (BP의 PartMesh 에셋 사전 지정 + OnRep_bCarriesPart 동작 확인).
- [ ] 운반자가 사망하면 `DropPart()`가 `PartActorClass`로 부품 액터를 정확히 스폰한다 (기존 Phase2와 동일한 흐름 유지).
- [ ] DREnemySpawnGroup 적 전원 사망 시 기존 문 부서짐 / `OnAllEnemiesDead` 동작이 유지된다.
- [ ] 사이트 주변 적과 통로 적 모두 플레이어에게 주는 대미지가 0.7배다 (체력 로그 검증).
- [ ] 0.7배는 엘리트 룬 버프(×1.2)와 곱셈 순서가 결과에 영향 없음 (×1.2×0.7 = ×0.84).
- [ ] 0.7배는 클렌저 사이트(`UDRCleanserSiteAttributeSet`)에는 적용되지 않는다.
- [ ] 부품 운반자 처치 → 부품 드롭 → 픽업 → 사이트 설치 흐름이 그룹 단위로 정상 동작한다.
- [ ] 부품 정확히 2개 설치 시 `ValidatePhaseCompletion`이 즉시 true → New Phase2 진입.
- [ ] 사이트의 `RequiredPartsCount`가 런타임에 변경되지 않는다 (항상 2 유지).
- [ ] 레벨의 DREnemySpawnGroup 수가 정확히 2개일 때 정상 진행되며, 2개가 아니면 경고 로그가 출력된다.
- [ ] New Phase2 진입 후 웨이브/엘리트/독가스/사이트 체력/BGM이 종전대로 동작한다.
- [ ] HUD 페이즈 라벨이 "페이즈 1", "페이즈 2"로 표시되고 진행률(`CollectedParts / RequiredCount`)이 정확히 갱신된다.
- [ ] 멀티플레이 환경(서버 + 1~2 클라이언트)에서 부품 메시 가시화, 부품 픽업 권한, 사이트 슬롯 메시 표시가 모든 머신에서 일관된다.

---

## 7. 범위 밖 (Out of Scope)

- 페이즈 클래스 C++ 리네이밍(`UDRPhase3` → `UDRPhase2`)
- `ValidatePhaseCompletion` 인덱스 기반 switch → 폴리모픽 디스패치 리팩터링
- DREnemySpawnGroup 수를 3 이상으로 늘리기 위한 CleanserSite 슬롯 메시 컴포넌트 확장 (현재 2 고정 유지)
- 사이트 `RequiredPartsCount`를 2 이외 값으로 변경하는 런타임 로직
- 아르마딜로/잠자리 적 자체의 AI/Ability 신규 구현 (기존 BP 자산 사용 전제)
- 신규 페이즈(보스 등) 추가
