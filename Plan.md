# 튜토리얼 맵 구현 계획 (Plan.md)

Research.md의 설계를 기반으로, 각 단계별 구현 방법을 상세히 기술한다.

---

## Phase 1: 핵심 인프라 구축

### Step 1: ADRTutorialManager 액터 생성

튜토리얼 전체 진행을 관리하는 중앙 액터. 레벨에 1개 배치하며, 구간/목표 상태를 추적하고, 문/발판/적/UI를 제어한다.

#### 1.1 새 파일 생성

**`Source/DaeRune/Public/Tutorial/DRTutorialManager.h`**
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTutorialManager.generated.h"

class ADRTutorialGate;
class ADRTutorialPressurePlate;
class ADRTutorialStartTile;
class ADREnemy;
class ADRCleanserSite;
class UOverlayWidgetController;
class UDRAbilitySystemComponent;

// 튜토리얼 구간 열거형
UENUM(BlueprintType)
enum class ETutorialSection : uint8
{
    Section1_Parkour     UMETA(DisplayName = "Section1 Parkour"),
    Section2_Combat      UMETA(DisplayName = "Section2 Combat"),
    Section3_PartCollect UMETA(DisplayName = "Section3 Part Collection"),
    Completed            UMETA(DisplayName = "Tutorial Completed")
};

// 구간 2 전투 목표 열거형
UENUM(BlueprintType)
enum class ECombatObjective : uint8
{
    None,
    Objective1_MeleeAttack,    // 기본 공격 5회
    Objective2_WaterPump,      // 물대포 3초
    Objective3_SeedCannon,     // SeedCannon 동시 적중
    AllComplete
};

UCLASS(Blueprintable)
class DAERUNE_API ADRTutorialManager : public AActor
{
    GENERATED_BODY()

public:
    ADRTutorialManager();

    // ========== 구간 진행 ==========

    // 현재 구간 조회
    UFUNCTION(BlueprintPure, Category = "Tutorial")
    ETutorialSection GetCurrentSection() const { return CurrentSection; }

    // 현재 전투 목표 조회
    UFUNCTION(BlueprintPure, Category = "Tutorial")
    ECombatObjective GetCurrentObjective() const { return CurrentObjective; }

    // 구간 클리어 처리 (발판이 호출)
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void OnSectionCleared(int32 SectionIndex);

    // 발판이 밟혔을 때 (발판이 호출)
    UFUNCTION(BlueprintCallable, Category = "Tutorial")
    void OnPressurePlateActivated(int32 SectionIndex);

    // ========== 구간 2 전투 훈련 ==========

    // 기본 공격 적중 보고 (샌드백 적이 호출)
    UFUNCTION(BlueprintCallable, Category = "Tutorial|Combat")
    void ReportMeleeHit();

    // WaterPump 활성 시간 보고 (Tick에서 호출)
    UFUNCTION(BlueprintCallable, Category = "Tutorial|Combat")
    void ReportWaterPumpTick(float DeltaTime);

    // SeedCannon 폭발 적중 수 보고 (SeedProjectile이 호출)
    UFUNCTION(BlueprintCallable, Category = "Tutorial|Combat")
    void ReportSeedCannonHits(int32 HitCount);

    // ========== 구간 3 부품 수집 ==========

    // 시작 타일 밟힘 보고
    UFUNCTION(BlueprintCallable, Category = "Tutorial|Parts")
    void OnStartTileActivated();

    // 부품 설치 보고 (CleanserSite의 OnPartInstalled에 바인딩)
    UFUNCTION()
    void OnPartInstalledToSite(ADRCleanserSite* Site);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ========== 에디터 설정 (레벨에서 할당) ==========

    // 문 액터 참조 (레벨에 배치 후 할당)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
    TObjectPtr<ADRTutorialGate> Gate1;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
    TObjectPtr<ADRTutorialGate> Gate2;

    // 발판 액터 참조
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
    TObjectPtr<ADRTutorialPressurePlate> PressurePlate1;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
    TObjectPtr<ADRTutorialPressurePlate> PressurePlate2;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
    TObjectPtr<ADRTutorialPressurePlate> PressurePlate3;

    // 시작 타일 (구간 3)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
    TObjectPtr<ADRTutorialStartTile> StartTile;

    // 샌드백 적 (구간 2, 레벨에 미리 배치)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
    TObjectPtr<ADREnemy> DummyEnemy_Center;

    // SeedCannon 목표용 적 스폰 포인트 (4방향)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
    TArray<TObjectPtr<AActor>> SeedCannonSpawnPoints;

    // SeedCannon 목표용 적 클래스
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Config")
    TSubclassOf<ADREnemy> DummyEnemyClass;

    // 부품 적 (구간 3, 레벨에 미리 배치)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
    TArray<TObjectPtr<ADREnemy>> PartEnemies;

    // 클렌저사이트 (구간 3)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
    TObjectPtr<ADRCleanserSite> TutorialCleanserSite;

    // ========== 어빌리티 부여 설정 ==========

    // 단계별로 부여할 어빌리티 클래스
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Abilities")
    TSubclassOf<UGameplayAbility> ClawSwipeAbilityClass;   // 목표 1 시작 시

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Abilities")
    TSubclassOf<UGameplayAbility> WaterPumpAbilityClass;   // 목표 2 시작 시

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Abilities")
    TSubclassOf<UGameplayAbility> SeedCannonAbilityClass;  // 목표 3 시작 시

    // ========== 목표 설정 ==========

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Config")
    int32 RequiredMeleeHits = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Config")
    float RequiredWaterPumpSeconds = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Config")
    int32 RequiredSimultaneousHits = 5;

private:
    // ========== 상태 ==========

    ETutorialSection CurrentSection = ETutorialSection::Section1_Parkour;
    ECombatObjective CurrentObjective = ECombatObjective::None;

    // 구간별 클리어 플래그
    bool bSection1Cleared = false;
    bool bSection2Cleared = false;
    bool bSection3Cleared = false;

    // 목표 1: 기본 공격 카운트
    int32 MeleeHitCount = 0;

    // 목표 2: 물대포 누적 시간
    float WaterPumpAccumulatedTime = 0.f;

    // SeedCannon 목표용으로 스폰된 적 배열
    UPROPERTY()
    TArray<TObjectPtr<ADREnemy>> SpawnedSeedCannonDummies;

    // ========== 내부 함수 ==========

    // 구간 전환
    void TransitionToSection(ETutorialSection NewSection);

    // 전투 목표 전환
    void TransitionToObjective(ECombatObjective NewObjective);

    // 어빌리티 부여 + UI 갱신
    void GrantAbilityToPlayer(TSubclassOf<UGameplayAbility> AbilityClass);

    // SeedCannon 목표 적 4마리 스폰
    void SpawnSeedCannonDummies();

    // SeedCannon 목표 적 정리
    void CleanupSeedCannonDummies();

    // UI 목표 텍스트 갱신
    void UpdateObjectiveUI(const FText& Title, const FText& ProgressFormat, int32 Current, int32 Max);

    // UI 조작 안내 갱신
    void UpdateControlGuideUI(const FText& GuideText);

    // OverlayWidgetController 캐시 가져오기
    UOverlayWidgetController* GetOverlayWidgetController() const;

    // 플레이어 ASC 가져오기
    UDRAbilitySystemComponent* GetPlayerASC() const;
};
```

**`Source/DaeRune/Private/Tutorial/DRTutorialManager.cpp`**

#### 1.2 핵심 로직 구현

**BeginPlay**:
```
1. 구간 1은 조건 없이 클리어 가능 → bSection1Cleared = true 설정
2. 발판1에 자신(TutorialManager) 참조 설정
3. DummyEnemy_Center를 샌드백 모드로 설정 (bIsTutorialDummy = true)
4. 부품 적들의 AI를 비활성화 (시작 타일 밟기 전까지)
5. 클렌저사이트의 OnPartInstalled 델리게이트에 바인딩
6. 목표 UI 초기화: "장애물을 넘어 이동하세요"
```

**TransitionToSection(NewSection)**:
```
switch(NewSection):
  Section2_Combat:
    CurrentSection = Section2_Combat
    TransitionToObjective(Objective1_MeleeAttack)
  Section3_PartCollect:
    CurrentSection = Section3_PartCollect
    CurrentObjective = None
    UpdateObjectiveUI("부품을 클렌저사이트에 설치하세요", "설치 완료", 0, 2)
  Completed:
    CurrentSection = Completed
    // TriggerTutorialComplete는 발판3에서 직접 GameMode 호출
```

**TransitionToObjective(NewObjective)**:
```
switch(NewObjective):
  Objective1_MeleeAttack:
    GrantAbilityToPlayer(ClawSwipeAbilityClass)
    MeleeHitCount = 0
    UpdateObjectiveUI("기본 공격으로 적을 공격하세요", "적중", 0, 5)
  Objective2_WaterPump:
    GrantAbilityToPlayer(WaterPumpAbilityClass)
    WaterPumpAccumulatedTime = 0
    UpdateObjectiveUI("물대포를 발사하세요", "초", 0, 3)
  Objective3_SeedCannon:
    GrantAbilityToPlayer(SeedCannonAbilityClass)
    SpawnSeedCannonDummies()
    UpdateObjectiveUI("시드캐논으로 모든 적을 한 번에 맞추세요", "동시 적중", 0, 5)
  AllComplete:
    bSection2Cleared = true
    UpdateObjectiveUI("전투 훈련 완료!", "", 0, 0)
```

**GrantAbilityToPlayer(AbilityClass)**:
```cpp
// 1. 플레이어 캐릭터의 ASC 가져오기
UDRAbilitySystemComponent* ASC = GetPlayerASC();
if (!ASC) return;

// 2. 어빌리티 Spec 생성 + InputTag 설정
FGameplayAbilitySpec AbilitySpec(AbilityClass, 1);
if (const UDRGameplayAbility* DRAbility = Cast<UDRGameplayAbility>(AbilitySpec.Ability))
{
    AbilitySpec.DynamicAbilityTags.AddTag(DRAbility->StartupInputTag);
    FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(AbilitySpec);

    // InputTag 캐시에도 추가
    if (DRAbility->StartupInputTag.IsValid())
    {
        ASC->AddToInputTagCache(AbilitySpec); // 이 함수를 public으로 변경 필요
    }
}

// 3. AbilitiesGivenDelegate 브로드캐스트 → UI 아이콘 갱신
ASC->AbilitiesGivenDelegate.Broadcast();

// 4. OverlayWidgetController를 통해 어빌리티 아이콘 브로드캐스트
if (UOverlayWidgetController* WC = GetOverlayWidgetController())
{
    WC->BroadcastAbilityInfo();
}
```

**UpdateObjectiveUI(Title, ProgressFormat, Current, Max)**:
```cpp
// OverlayWidgetController의 기존 델리게이트 활용
UOverlayWidgetController* WC = GetOverlayWidgetController();
if (!WC) return;

FText ProgressText;
if (Max > 0)
{
    ProgressText = FText::Format(
        NSLOCTEXT("Tutorial", "Progress", "{0} / {1} {2}"),
        FText::AsNumber(Current), FText::AsNumber(Max), ProgressFormat
    );
}
WC->OnObjectiveTextChanged.Broadcast(Title, ProgressText);
WC->OnObjectiveProgressChanged.Broadcast(Current, Max);
```

#### 1.3 기존 코드 수정 필요사항

**`DRAbilitySystemComponent.h`** — `AddToInputTagCache` 접근 제어 변경:
```cpp
// protected: → public:
public:
    void AddToInputTagCache(const FGameplayAbilitySpec& AbilitySpec);
    void RemoveFromInputTagCache(const FGameplayTag& InputTag);
```

---

### Step 2: ADRTutorialGate 액터 생성

문(Gate) 액터. 닫힌 상태에서 물리적으로 통행을 차단하고, `OpenGate()` 호출 시 위로 슬라이드하며 열린다.

#### 2.1 새 파일 생성

**`Source/DaeRune/Public/Tutorial/DRTutorialGate.h`**
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTutorialGate.generated.h"

UCLASS(Blueprintable)
class DAERUNE_API ADRTutorialGate : public AActor
{
    GENERATED_BODY()

public:
    ADRTutorialGate();

    // 문 열기
    UFUNCTION(BlueprintCallable, Category = "Tutorial|Gate")
    void OpenGate();

    // 문이 열려있는지 확인
    UFUNCTION(BlueprintPure, Category = "Tutorial|Gate")
    bool IsOpen() const { return bIsOpen; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 문 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> GateMesh;

    // 열림 높이 (위로 이동할 거리)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gate|Config")
    float OpenHeight = 300.f;

    // 열림 속도
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gate|Config")
    float OpenSpeed = 200.f;

private:
    bool bIsOpen = false;
    bool bIsOpening = false;
    FVector ClosedLocation;
    FVector OpenLocation;
};
```

#### 2.2 핵심 로직

**BeginPlay**:
```
ClosedLocation = GetActorLocation()
OpenLocation = ClosedLocation + FVector(0, 0, OpenHeight)
```

**OpenGate()**:
```
if (bIsOpen || bIsOpening) return;
bIsOpening = true;
```

**Tick(DeltaTime)**:
```
if (bIsOpening && !bIsOpen)
{
    FVector CurrentLocation = GetActorLocation();
    FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, OpenLocation, DeltaTime, OpenSpeed);
    SetActorLocation(NewLocation);

    if (FVector::Dist(NewLocation, OpenLocation) < 1.f)
    {
        SetActorLocation(OpenLocation);
        bIsOpen = true;
        bIsOpening = false;
        SetActorTickEnabled(false); // 더 이상 Tick 불필요
    }
}
```

> **참고**: Tick 기반 보간 대신 Blueprint의 Timeline을 사용해도 되지만, C++에서 직접 제어하는 것이 더 명확하다. BP 서브클래스에서 Timeline을 사용하고 싶다면 `OpenGate()`를 `BlueprintNativeEvent`로 변경할 수 있다.

---

### Step 3: ADRTutorialPressurePlate 액터 생성

발판 액터. 플레이어가 위에 올라서면 해당 구간의 클리어 여부를 체크하고, 클리어된 상태라면 문을 열거나 튜토리얼 완료를 트리거한다.

#### 3.1 새 파일 생성

**`Source/DaeRune/Public/Tutorial/DRTutorialPressurePlate.h`**
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTutorialPressurePlate.generated.h"

class UBoxComponent;
class ADRTutorialManager;

UCLASS(Blueprintable)
class DAERUNE_API ADRTutorialPressurePlate : public AActor
{
    GENERATED_BODY()

public:
    ADRTutorialPressurePlate();

    // 이 발판이 담당하는 구간 인덱스 (1, 2, 3)
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "PressurePlate")
    int32 SectionIndex = 1;

    // 튜토리얼 매니저 참조
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "PressurePlate")
    TObjectPtr<ADRTutorialManager> TutorialManager;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> PlateMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
    bool bActivated = false; // 1회성 방지
};
```

#### 3.2 핵심 로직

**OnTriggerBeginOverlap**:
```cpp
// 1회만 작동
if (bActivated) return;

// 플레이어인지 체크
ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(OtherActor);
if (!PlayerCharacter) return;

// 튜토리얼 매니저에 발판 활성화 보고
if (TutorialManager)
{
    TutorialManager->OnPressurePlateActivated(SectionIndex);
}
```

**TutorialManager::OnPressurePlateActivated(SectionIndex)** 내부 로직:
```
switch(SectionIndex):
  1: // 구간 1 발판
    if (bSection1Cleared)
      Gate1->OpenGate()
      TransitionToSection(Section2_Combat)
      bActivated = true
  2: // 구간 2 발판
    if (bSection2Cleared)
      Gate2->OpenGate()
      TransitionToSection(Section3_PartCollect)
      bActivated = true
  3: // 구간 3 발판 (마지막)
    if (bSection3Cleared)
      // GameMode의 TriggerTutorialComplete() 호출
      ADRTutorialGameMode* GM = Cast<ADRTutorialGameMode>(GetWorld()->GetAuthGameMode())
      GM->TriggerTutorialComplete()
      bActivated = true
```

---

### Step 4: ADRTutorialStartTile 액터 생성

구간 3 전용. 플레이어가 밟으면 부품 적의 AI를 활성화한다.

#### 4.1 새 파일 생성

**`Source/DaeRune/Public/Tutorial/DRTutorialStartTile.h`**
```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTutorialStartTile.generated.h"

class UBoxComponent;
class ADRTutorialManager;

UCLASS(Blueprintable)
class DAERUNE_API ADRTutorialStartTile : public AActor
{
    GENERATED_BODY()

public:
    ADRTutorialStartTile();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StartTile")
    TObjectPtr<ADRTutorialManager> TutorialManager;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> TileMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> TriggerBox;

    UFUNCTION()
    void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
    bool bActivated = false;
};
```

#### 4.2 핵심 로직

**OnTriggerBeginOverlap**:
```cpp
if (bActivated) return;
if (!Cast<ADRCharacter>(OtherActor)) return;

bActivated = true;

if (TutorialManager)
{
    TutorialManager->OnStartTileActivated();
}
```

**TutorialManager::OnStartTileActivated()**:
```cpp
// 부품 적들의 AI 활성화 (도주 BT 실행)
for (ADREnemy* Enemy : PartEnemies)
{
    if (Enemy)
    {
        // Blackboard에서 도주 모드 설정
        if (UBlackboardComponent* BB = Enemy->GetBlackboardComponent())
        {
            // AI Controller의 BrainComponent(BT) 재시작
            AAIController* AIC = Cast<AAIController>(Enemy->GetController());
            if (AIC && AIC->GetBrainComponent())
            {
                AIC->GetBrainComponent()->RestartLogic();
            }
        }
    }
}

UpdateObjectiveUI("부품을 들고 있는 적을 잡아 부품을 수집하세요", "", 0, 0);
```

---

## Phase 2: 샌드백 적 시스템 구현

### Step 5: ADREnemy에 튜토리얼 더미 플래그 추가

기존 `ADREnemy` 클래스에 `bIsTutorialDummy` 플래그를 추가하여, 이동/공격 비활성화 + 무적 상태를 구현한다.

#### 5.1 기존 파일 수정

**`Source/DaeRune/Public/Character/DREnemy.h`** — 프로퍼티 추가:
```cpp
// ===== 튜토리얼 더미 시스템 =====

// 튜토리얼 샌드백 모드 (움직이지 않음, 공격 안 함, 무적)
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
bool bIsTutorialDummy = false;
```

#### 5.2 동작 변경 위치

**`DREnemy.cpp` — PossessedBy()**:
```cpp
// BehaviorTree 실행 부분에 조건 추가:
if (bIsTutorialDummy)
{
    // AI BT를 실행하지 않음 → 이동/공격 비활성화
    // (PossessedBy에서 BT RunBehaviorTree 호출을 스킵)
    return;  // 또는 조건부로 BT 스킵
}
```

**`DRAttributeSet.cpp` (또는 `DREnemyAttributeSet`) — PostGameplayEffectExecute()**:
```cpp
// 데미지 처리 시 튜토리얼 더미 체크:
if (ADREnemy* Enemy = Cast<ADREnemy>(TargetActor))
{
    if (Enemy->bIsTutorialDummy)
    {
        // 체력을 최소 1로 유지 (죽지 않음)
        // SetHealth(FMath::Max(1.f, GetHealth()));

        // 히트 리액션은 정상 재생 (피격 느낌)
        // 데미지 적용 후 즉시 체력 복구 대신, 체력을 0 이하로 내려가지 않게 클램핑
    }
}
```

> **구체적 구현**: `PostGameplayEffectExecute`에서 데미지 적용 후, `bIsTutorialDummy == true`이면 체력을 최소값(예: MaxHealth의 50%)으로 클램핑한다. 이렇게 하면 히트 리액션은 정상적으로 발생하면서도 적이 죽지 않는다.

#### 5.3 튜토리얼 매니저에서 데미지 감지

샌드백 적이 데미지를 받았을 때 튜토리얼 매니저에 보고하는 방법:

**방법**: `PostGameplayEffectExecute`에서 `bIsTutorialDummy`인 경우, 월드에서 `ADRTutorialManager`를 찾아 `ReportMeleeHit()` 호출.

```cpp
// PostGameplayEffectExecute에서:
if (Enemy->bIsTutorialDummy && Data.EvaluatedData.Attribute == GetHealthAttribute())
{
    // 튜토리얼 매니저에 적중 보고
    if (ADRTutorialManager* TM = Cast<ADRTutorialManager>(
        UGameplayStatics::GetActorOfClass(Enemy->GetWorld(), ADRTutorialManager::StaticClass())))
    {
        TM->ReportMeleeHit();
    }

    // 체력 복구 (무적)
    SetHealth(GetMaxHealth());
}
```

> **대안**: 매 틱마다 `GetActorOfClass`를 호출하는 것은 비효율적이므로, `ADRTutorialManager`를 적에게 참조로 캐싱하거나, `BeginPlay` 시 한 번만 찾아서 저장하는 것이 좋다. 튜토리얼 매니저가 BeginPlay에서 DummyEnemy에 자신의 참조를 설정할 수 있다.

```cpp
// DREnemy.h에 추가:
UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
TWeakObjectPtr<AActor> TutorialManagerRef;
```

---

## Phase 3: 구간 2 전투 훈련 시스템

### Step 6: 목표 1 — 기본 공격 카운트 (ClawSwipe 5회 적중)

#### 6.1 ReportMeleeHit() 구현

**`DRTutorialManager.cpp`**:
```cpp
void ADRTutorialManager::ReportMeleeHit()
{
    if (CurrentObjective != ECombatObjective::Objective1_MeleeAttack) return;

    MeleeHitCount++;
    UpdateObjectiveUI(
        NSLOCTEXT("Tutorial", "Obj1", "기본 공격으로 적을 공격하세요"),
        NSLOCTEXT("Tutorial", "Obj1Fmt", "적중"),
        MeleeHitCount, RequiredMeleeHits
    );

    if (MeleeHitCount >= RequiredMeleeHits)
    {
        // 목표 1 완료 → 목표 2로 전환
        TransitionToObjective(ECombatObjective::Objective2_WaterPump);
    }
}
```

#### 6.2 데미지 이벤트 감지 흐름

```
플레이어 LMB → GA_ClawSwipe 활성화 → 몽타주 → EventMontage에서 데미지 GE 적용
→ ExecCalc_Damage 실행 → 타겟 AttributeSet::PostGameplayEffectExecute
→ bIsTutorialDummy 체크 → TutorialManager->ReportMeleeHit()
→ 체력 복구 (무적)
```

---

### Step 7: 목표 2 — 물대포(WaterPump) 3초간 발사

#### 7.1 ReportWaterPumpTick() 구현

WaterPump 어빌리티가 활성화 중인지 감지하여 시간을 누적한다.

**방법 A: TutorialManager의 Tick에서 체크**

```cpp
void ADRTutorialManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CurrentObjective != ECombatObjective::Objective2_WaterPump) return;

    // WaterPump 어빌리티가 활성 중인지 체크
    UDRAbilitySystemComponent* ASC = GetPlayerASC();
    if (!ASC) return;

    // InputTag.RMB에 해당하는 어빌리티가 활성 상태인지 확인
    static const FGameplayTag RMBTag = FGameplayTag::RequestGameplayTag(FName("InputTag.RMB"));
    FGameplayAbilitySpec* WaterPumpSpec = ASC->FindAbilitySpecByInputTag(RMBTag);

    if (WaterPumpSpec && WaterPumpSpec->IsActive())
    {
        WaterPumpAccumulatedTime += DeltaTime;

        // 소수점 첫째자리까지 표시 (0.1초 단위)
        int32 DisplaySeconds = FMath::FloorToInt(WaterPumpAccumulatedTime * 10) ; // 1/10초 단위
        int32 DisplayMax = FMath::FloorToInt(RequiredWaterPumpSeconds * 10);

        UpdateObjectiveUI(
            NSLOCTEXT("Tutorial", "Obj2", "물대포를 발사하세요"),
            NSLOCTEXT("Tutorial", "Obj2Fmt", "초"),
            FMath::FloorToInt(WaterPumpAccumulatedTime), // 정수 초 표시
            FMath::FloorToInt(RequiredWaterPumpSeconds)
        );

        if (WaterPumpAccumulatedTime >= RequiredWaterPumpSeconds)
        {
            // 목표 2 완료 → 목표 3로 전환
            TransitionToObjective(ECombatObjective::Objective3_SeedCannon);
        }
    }
}
```

> **주의**: `FindAbilitySpecByInputTag`는 `public`이므로 외부에서 호출 가능. `IsActive()`로 활성 여부를 판단한다.

#### 7.2 Water Cost 처리

튜토리얼에서 WaterPump 사용 시 물(Water) 자원이 소모되면 안 될 수 있다. 두 가지 선택지:
1. **튜토리얼용 WaterCost = 0으로 설정**: `DA_TutorialCharacterClassInfo`에서 WaterPump의 WaterCost를 0으로 오버라이드
2. **물 자원을 충분히 부여**: 초기 Water를 매우 높게 설정

> 권장: 방법 2 — `DA_TutorialCharacterClassInfo`의 Primary Attributes GE에서 MaxWater를 10000 등 매우 높은 값으로 설정하면 별도 처리 불필요.

---

### Step 8: 목표 3 — SeedCannon 동시 적중 감지

#### 8.1 SeedProjectile 수정 — 폭발 적중 수 보고

기존 `ADRSeedProjectile::ExplodeAtLocation()`에서 적중한 적의 수를 세고, 튜토리얼 매니저에 보고하는 로직을 추가한다.

**`Source/DaeRune/Private/Actor/DRSeedProjectile.cpp` — ExplodeAtLocation() 수정**:

```cpp
void ADRSeedProjectile::ExplodeAtLocation(const FVector& ImpactLocation)
{
    // ... 기존 로직 유지 ...

    // 적중 카운트 (기존 AffectedCount 변수 활용)
    int32 EnemyHitCount = 0;  // 적만 따로 카운트

    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* Target = Result.GetActor();
        if (!Target) continue;

        // ... 기존 필터링 로직 유지 ...

        if (bCanApply)
        {
            ApplyEffectToActor(Target, Distance);
            AffectedCount++;

            // 적(Enemy)인 경우만 카운트
            if (Cast<ADREnemy>(Target))
            {
                EnemyHitCount++;
            }
        }
    }

    // === 튜토리얼 적중 수 보고 (신규 추가) ===
    if (EnemyHitCount > 0)
    {
        if (ADRTutorialManager* TM = Cast<ADRTutorialManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), ADRTutorialManager::StaticClass())))
        {
            TM->ReportSeedCannonHits(EnemyHitCount);
        }
    }

    Destroy();
}
```

> **성능 고려**: `GetActorOfClass`는 매 폭발마다 호출되지만, SeedCannon 발사 빈도가 낮으므로 문제 없다. 스테이지 맵에서는 TutorialManager가 없으므로 `nullptr`이 반환되어 아무 동작도 하지 않는다.

#### 8.2 ReportSeedCannonHits() 구현

```cpp
void ADRTutorialManager::ReportSeedCannonHits(int32 HitCount)
{
    if (CurrentObjective != ECombatObjective::Objective3_SeedCannon) return;

    if (HitCount >= RequiredSimultaneousHits)
    {
        // 5마리 동시 적중 성공!
        TransitionToObjective(ECombatObjective::AllComplete);
    }
    else
    {
        // 실패: UI에 현재 적중 수 표시 (재시도 유도)
        UpdateObjectiveUI(
            NSLOCTEXT("Tutorial", "Obj3", "시드캐논으로 모든 적을 한 번에 맞추세요"),
            NSLOCTEXT("Tutorial", "Obj3Fmt", "동시 적중"),
            HitCount, RequiredSimultaneousHits
        );
    }
}
```

#### 8.3 SeedCannon 적 스폰 로직

```cpp
void ADRTutorialManager::SpawnSeedCannonDummies()
{
    if (!DummyEnemyClass) return;

    for (AActor* SpawnPoint : SeedCannonSpawnPoints)
    {
        if (!SpawnPoint) continue;

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        ADREnemy* NewDummy = GetWorld()->SpawnActor<ADREnemy>(
            DummyEnemyClass,
            SpawnPoint->GetActorLocation(),
            SpawnPoint->GetActorRotation(),
            SpawnParams
        );

        if (NewDummy)
        {
            NewDummy->bIsTutorialDummy = true;
            NewDummy->TutorialManagerRef = this;
            SpawnedSeedCannonDummies.Add(NewDummy);
        }
    }
}
```

**적 배치 거리**: 중앙 샌드백으로부터 동서남북 각 300 유닛 거리에 SpawnPoint 배치 (SeedCannon 외부 반경 400 유닛 이내).

---

## Phase 4: 구간 3 부품 수집 시스템

### Step 9: 부품 적 + 클렌저사이트 연동

#### 9.1 부품 적 설정

레벨에 미리 배치된 `ADREnemy` 2마리에 다음 설정:
- `bCarriesPart = true`
- `PartActorClass = BP_CleanserPart` (기존 블루프린트 사용)
- AI BehaviorTree: 도주 전용 BT (도망만 하고 공격 안 함)
- 시작 시 AI 비활성화 (StartTile 밟기 전까지)

#### 9.2 부품 적 AI 비활성화/활성화

**TutorialManager::BeginPlay() 내**:
```cpp
// 부품 적들의 BrainComponent 정지
for (ADREnemy* Enemy : PartEnemies)
{
    if (Enemy)
    {
        if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
        {
            if (UBrainComponent* Brain = AIC->GetBrainComponent())
            {
                Brain->StopLogic(TEXT("Tutorial: Waiting for StartTile"));
            }
        }
    }
}
```

**OnStartTileActivated() 내**:
```cpp
// 부품 적들의 BrainComponent 재시작
for (ADREnemy* Enemy : PartEnemies)
{
    if (Enemy)
    {
        if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
        {
            if (UBrainComponent* Brain = AIC->GetBrainComponent())
            {
                Brain->RestartLogic();
            }
        }
    }
}
```

#### 9.3 부품 설치 감지 → 구간 3 클리어

**TutorialManager::BeginPlay() 내**:
```cpp
// CleanserSite의 OnPartInstalled 델리게이트에 바인딩
if (TutorialCleanserSite)
{
    TutorialCleanserSite->ActivateSite(); // Active 상태로 설정
    TutorialCleanserSite->OnPartInstalled.AddDynamic(this, &ADRTutorialManager::OnPartInstalledToSite);
}
```

**OnPartInstalledToSite(Site)**:
```cpp
void ADRTutorialManager::OnPartInstalledToSite(ADRCleanserSite* Site)
{
    if (!Site) return;

    int32 Installed = Site->GetInstalledPartsCount();
    UpdateObjectiveUI(
        NSLOCTEXT("Tutorial", "Obj_Parts", "부품을 클렌저사이트에 설치하세요"),
        NSLOCTEXT("Tutorial", "Obj_PartsFmt", "설치 완료"),
        Installed, 2
    );

    if (Site->IsPartInstallationComplete())
    {
        // 구간 3 클리어
        bSection3Cleared = true;
        UpdateObjectiveUI(
            NSLOCTEXT("Tutorial", "Complete", "튜토리얼 완료! 발판을 밟으세요"),
            FText::GetEmpty(), 0, 0
        );
    }
}
```

---

## Phase 5: 튜토리얼 전용 UI

### Step 10: WBP_TutorialOverlay 블루프린트 생성

기존 `WBP_StageOverlay`를 기반으로 한 튜토리얼 전용 오버레이.
**핵심: 기존 `WBP_PhaseObjective` 위젯을 그대로 재사용한다.**

#### 10.1 WBP_PhaseObjective 재사용 근거

`DRTutorialManager::UpdateObjectiveUI()`는 이미 `OverlayWidgetController`의 기존 델리게이트를 직접 브로드캐스트한다:

```cpp
// DRTutorialManager.cpp (기존 구현)
void ADRTutorialManager::UpdateObjectiveUI(...)
{
    WC->OnObjectiveTextChanged.Broadcast(Title, ProgressText);
    WC->OnObjectiveProgressChanged.Broadcast(Current, Max);
}
```

`WBP_PhaseObjective`는 이미 이 델리게이트에 바인딩되어 목표 텍스트와 진행도를 표시한다.
따라서 **WBP_TutorialOverlay 안에 WBP_PhaseObjective를 배치하기만 하면** 목표 UI가 자동으로 동작한다.

**장점:**
- 새로운 목표 텍스트/진행도 위젯을 만들 필요 없음
- 기존 바인딩 로직이 그대로 재활용됨
- 스테이지와 튜토리얼의 목표 UI 스타일이 일관됨
- DT_PhaseObjective DataTable과 동일한 패턴으로 동작

#### 10.2 블루프린트 위젯 생성 (에디터 작업)

1. `Content/Blueprints/UI/Overlay/` 폴더에 `WBP_TutorialOverlay` 생성
2. Parent Class: `UDRUserWidget`
3. 포함 요소:
   - **체력/물 바**: 기존 `WBP_StageOverlay`에서 복사
   - **스킬 아이콘 영역**: `WBP_SkillSlot` 3개 (LMB, RMB, Q)
   - **WBP_PhaseObjective**: 기존 위젯 그대로 배치 (목표 텍스트 + 진행도)
   - **조작 안내 텍스트**: Text Block

4. 스킬 아이콘 초기 상태: 모두 **Hidden**
5. `AbilityInfoDelegate` 바인딩: 어빌리티가 부여될 때마다 해당 슬롯을 **Visible**로 전환 + 페이드인 애니메이션

> **WBP_PhaseObjective 관련 추가 코드 불필요**: 이 위젯은 자체적으로 WidgetController에 바인딩하므로, WBP_TutorialOverlay에서는 Designer 탭에서 배치만 하면 된다. TutorialManager가 UpdateObjectiveUI()를 호출하면 WBP_PhaseObjective가 자동으로 텍스트와 진행도를 업데이트한다.

#### 10.3 HUD 설정

**방법**: `BP_DRTutorialGameMode`의 HUD 클래스에서 `OverlayWidgetClass`를 `WBP_TutorialOverlay`로 설정.

두 가지 접근법:
1. **전용 HUD 블루프린트 `BP_DRTutorialHUD`** 생성 → `OverlayWidgetClass = WBP_TutorialOverlay` 설정
2. 기존 `BP_DRHUD`를 사용하되, `BP_DRTutorialGameMode`에서 `HUDClass = BP_DRTutorialHUD`로 오버라이드

> 권장: 방법 1 — `BP_DRTutorialHUD` 블루프린트를 생성하고, `BP_DRTutorialGameMode`의 `HUDClass`에 할당.

#### 10.4 스킬 아이콘 점진적 표시 구현

**WBP_TutorialOverlay Blueprint**에서:
1. `AbilityInfoDelegate` 수신 시, `FDRAbilityInfo.InputTag`를 확인
2. `InputTag.LMB` → `SkillSlot_LMB.SetVisibility(Visible)` + 페이드인 애니메이션
3. `InputTag.RMB` → `SkillSlot_RMB.SetVisibility(Visible)` + 페이드인 애니메이션
4. `InputTag.Q` → `SkillSlot_Q.SetVisibility(Visible)` + 페이드인 애니메이션

**애니메이션**: UMG의 `Play Animation`으로 Opacity 0→1 (0.5초) + Scale 1.2→1.0 (0.3초)

---

### Step 11: 튜토리얼 전용 CharacterClassInfo

#### 11.1 DA_TutorialCharacterClassInfo 생성 (에디터 작업)

1. `Content/Blueprints/AbilitySystem/Data/` 폴더에 `DA_TutorialCharacterClassInfo` 생성
2. Parent: `UPlayerCharacterClassInfo`
3. 설정:
   - `StartupAbilities`: **비어있음** (어빌리티 없이 시작)
   - `CommonAbilities`: **비어있음**
   - `PrimaryAttributes GE`: MaxWater를 매우 높게 (10000) 설정하여 물 부족 방지
   - `VitalAttributes GE`: Health/Water 초기화
   - `CharacterBPClasses`: 기존과 동일 (GardenRobot 등)

4. `BP_DRTutorialGameMode`의 `PlayerCharacterClassInfo`에 할당

#### 11.2 GameMode 확장

**`DRTutorialGameMode.h`** — TutorialManager 스폰 추가:
```cpp
// 튜토리얼 매니저 클래스 (BP에서 할당)
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial")
TSubclassOf<ADRTutorialManager> TutorialManagerClass;
```

> **참고**: TutorialManager는 레벨에 직접 배치하는 것이 더 적합하다. GameMode에서 스폰하는 것보다, 레벨 디자이너가 에디터에서 Gate/PressurePlate/적 참조를 직접 연결할 수 있기 때문이다. 따라서 TutorialManager는 **레벨에 배치**하는 방식을 권장한다.

---

## Phase 6: 에디터 작업 목록

### Step 12: 블루프린트 + 레벨 구성

#### 12.1 블루프린트 생성 목록

| 블루프린트 | 부모 클래스 | 설명 |
|---|---|---|
| `BP_TutorialManager` | `ADRTutorialManager` | 튜토리얼 매니저 |
| `BP_TutorialGate` | `ADRTutorialGate` | 문 (메시 할당) |
| `BP_TutorialPressurePlate` | `ADRTutorialPressurePlate` | 발판 (메시 할당) |
| `BP_TutorialStartTile` | `ADRTutorialStartTile` | 시작 타일 (메시 할당) |
| `BP_TutorialDummy` | `ADREnemy` | 샌드백 적 (bIsTutorialDummy=true) |
| `BP_TutorialPartEnemy` | `ADREnemy` | 부품 적 (bCarriesPart=true, 도주 BT) |
| `WBP_TutorialOverlay` | `UDRUserWidget` | 튜토리얼 오버레이 |
| `BP_DRTutorialHUD` | `ADRHUD` | 튜토리얼 HUD |
| `DA_TutorialCharacterClassInfo` | `UPlayerCharacterClassInfo` | 어빌리티 없는 클래스 정보 |

#### 12.2 TutorialMap 레벨 배치

```
[PlayerStart]
    ↓
[구간 1: 점프 파쿠르 영역]
  - 다양한 높이/간격의 장애물 (StaticMesh)
  - KillZ Volume (떨어지면 리스폰)
  - BP_TutorialPressurePlate (SectionIndex=1)
    ↓
[BP_TutorialGate #1]
    ↓
[구간 2: 전투 훈련 영역]
  - BP_TutorialDummy (중앙 배치)
  - SpawnPoint_N, S, E, W (Empty Actor 4개, 중앙에서 300 유닛 거리)
  - BP_TutorialPressurePlate (SectionIndex=2)
    ↓
[BP_TutorialGate #2]
    ↓
[구간 3: 부품 수집 영역]
  - ADRCleanserSite (가운데, Active 상태)
  - BP_TutorialPartEnemy x2 (초기 위치)
  - BP_TutorialStartTile (입구 근처)
  - BP_TutorialPressurePlate (SectionIndex=3)

[BP_TutorialManager] (레벨 아무데나 배치)
  - Gate1 → BP_TutorialGate #1
  - Gate2 → BP_TutorialGate #2
  - PressurePlate1/2/3 → 각 발판
  - DummyEnemy_Center → BP_TutorialDummy
  - SeedCannonSpawnPoints → SpawnPoint_N, S, E, W
  - PartEnemies → BP_TutorialPartEnemy x2
  - TutorialCleanserSite → ADRCleanserSite
  - StartTile → BP_TutorialStartTile
```

#### 12.3 BP_DRTutorialGameMode 설정

| 프로퍼티 | 값 |
|---|---|
| `HUDClass` | `BP_DRTutorialHUD` |
| `PlayerCharacterClassInfo` | `DA_TutorialCharacterClassInfo` |
| `DefaultPawnClass` | 기존 BP_GardenRobot 등 |
| `AbilityInfo` | 기존 `DA_AbilityInfo` (아이콘 정보 필요) |

---

## 수정 파일 요약

### 새로 생성할 C++ 파일 (8개)

| 파일 | 역할 |
|---|---|
| `Source/DaeRune/Public/Tutorial/DRTutorialManager.h` | 튜토리얼 매니저 헤더 |
| `Source/DaeRune/Private/Tutorial/DRTutorialManager.cpp` | 튜토리얼 매니저 구현 |
| `Source/DaeRune/Public/Tutorial/DRTutorialGate.h` | 문 액터 헤더 |
| `Source/DaeRune/Private/Tutorial/DRTutorialGate.cpp` | 문 액터 구현 |
| `Source/DaeRune/Public/Tutorial/DRTutorialPressurePlate.h` | 발판 액터 헤더 |
| `Source/DaeRune/Private/Tutorial/DRTutorialPressurePlate.cpp` | 발판 액터 구현 |
| `Source/DaeRune/Public/Tutorial/DRTutorialStartTile.h` | 시작 타일 헤더 |
| `Source/DaeRune/Private/Tutorial/DRTutorialStartTile.cpp` | 시작 타일 구현 |

### 수정할 기존 C++ 파일 (4개)

| 파일 | 수정 내용 |
|---|---|
| `DREnemy.h` | `bIsTutorialDummy`, `TutorialManagerRef` 프로퍼티 추가 |
| `DREnemy.cpp` | PossessedBy에서 bIsTutorialDummy 시 BT 스킵 |
| `DRAbilitySystemComponent.h` | `AddToInputTagCache`, `RemoveFromInputTagCache`를 public으로 변경 |
| `DRSeedProjectile.cpp` | ExplodeAtLocation에서 적중 수 → TutorialManager 보고 |

### 수정할 기존 AttributeSet 파일 (1개)

| 파일 | 수정 내용 |
|---|---|
| `DREnemyAttributeSet.cpp` (또는 관련 AttributeSet) | PostGameplayEffectExecute에서 bIsTutorialDummy 시 체력 복구 + 적중 보고 |

### 에디터에서 생성할 블루프린트 (9개)

위의 Step 12.1 블루프린트 생성 목록 참조.

---

## 구현 순서 (권장)

```
1단계: C++ 코어 클래스 생성 (Step 1~4)
  → DRTutorialManager, DRTutorialGate, DRTutorialPressurePlate, DRTutorialStartTile
  → 컴파일 확인

2단계: 기존 코드 수정 (Step 5)
  → ADREnemy에 bIsTutorialDummy 추가
  → DRAbilitySystemComponent 접근 제어 변경
  → AttributeSet에 무적 로직 추가
  → 컴파일 확인

3단계: 전투 목표 로직 구현 (Step 6~8)
  → ReportMeleeHit, WaterPump Tick, SeedCannon 적중 보고
  → DRSeedProjectile 수정
  → 컴파일 확인

4단계: 부품 수집 로직 구현 (Step 9)
  → AI 비활성화/활성화, CleanserSite 연동
  → 컴파일 확인

5단계: 에디터 작업 (Step 10~12)
  → 블루프린트 생성
  → WBP_TutorialOverlay 위젯 구성
  → TutorialMap 레벨 배치
  → BP_DRTutorialGameMode 설정

6단계: 통합 테스트
  → MainMenu → TutorialMap 진입
  → 구간 1: 파쿠르 → 발판1 → 문1 열림
  → 구간 2: 기본 공격 5회 → 물대포 3초 → SeedCannon 5마리 동시 적중
  → 구간 3: 시작 타일 → 적 2마리 도주 → 부품 수집 → 클렌저사이트 설치
  → 발판3 → 튜토리얼 완료 → SaveGame 저장 → MainMenu 복귀
  → MainMenu에서 로비 모드 확인
```

---

# 에디터 작업 상세 가이드

> **C++ 코드 구현은 완료됨.** 아래는 언리얼 에디터에서 수행해야 하는 모든 블루프린트/에셋/레벨 작업을 단계별로 정리한 것이다.

---

## E-1. 블루프린트 생성

### E-1.1 BP_TutorialManager (튜토리얼 매니저)

| 항목 | 값 |
|---|---|
| **위치** | `Content/Blueprints/Tutorial/BP_TutorialManager` |
| **부모 클래스** | `ADRTutorialManager` (C++) |

**Details 패널에서 설정할 프로퍼티:**

| 카테고리 | 프로퍼티 | 설명 |
|---|---|---|
| Tutorial - Config | `RequiredMeleeHits` | 기본 공격 목표 횟수 (기본값: 5) |
| Tutorial - Config | `RequiredWaterPumpSeconds` | 물대포 유지 시간 (기본값: 3.0) |
| Tutorial - Config | `RequiredSimultaneousHits` | SeedCannon 동시 적중 수 (기본값: 5) |
| Tutorial - Abilities | `ClawSwipeAbilityClass` | `GA_ClawSwipe` (기존 기본공격 어빌리티 클래스) |
| Tutorial - Abilities | `WaterPumpAbilityClass` | `GA_WaterPump` (기존 물대포 어빌리티 클래스) |
| Tutorial - Abilities | `SeedCannonAbilityClass` | `GA_SeedCannon` (기존 시드캐논 어빌리티 클래스) |
| Tutorial - Enemies | `DummyEnemyClass` | `BP_TutorialDummy` (E-1.5에서 생성) |

> **주의**: `Gate1`, `Gate2`, `PressurePlate1~3`, `StartTile`, `DummyEnemy_Center`, `SeedCannonSpawnPoints`, `PartEnemies`, `TutorialCleanserSite` 등 Instance-Only 프로퍼티들은 **레벨에 배치한 후** 해당 액터를 드래그하여 연결한다 (E-3 참조).

---

### E-1.2 BP_TutorialGate (문)

| 항목 | 값 |
|---|---|
| **위치** | `Content/Blueprints/Tutorial/BP_TutorialGate` |
| **부모 클래스** | `ADRTutorialGate` (C++) |

**설정:**
1. `GateMesh` 컴포넌트 선택 → Static Mesh에 적절한 문 메시 할당 (예: 기존 `SM_Gate` 또는 Cube 등)
2. 머티리얼 설정 (튜토리얼 맵 테마에 맞게)
3. Details 패널:
   - `OpenHeight`: 문이 위로 이동할 거리 (기본값: 300, 메시 높이에 맞게 조정)
   - `OpenSpeed`: 문 열리는 속도 (기본값: 200 units/sec)
4. `GateMesh`의 Collision → Collision Presets: **BlockAll** 확인

---

### E-1.3 BP_TutorialPressurePlate (발판)

| 항목 | 값 |
|---|---|
| **위치** | `Content/Blueprints/Tutorial/BP_TutorialPressurePlate` |
| **부모 클래스** | `ADRTutorialPressurePlate` (C++) |

**설정:**
1. `PlateMesh` 컴포넌트 → Static Mesh에 발판 메시 할당 (납작한 플레이트 형태)
2. `TriggerBox` 컴포넌트:
   - Box Extent: 발판 위 영역 (예: X=100, Y=100, Z=50)
   - Collision Presets: **OverlapOnlyPawn**
   - Generate Overlap Events: **True**
3. Details 패널:
   - `SectionIndex`: 레벨 배치 시 각각 1, 2, 3으로 설정
   - `TutorialManager`: 레벨 배치 후 BP_TutorialManager 인스턴스 연결

**참고**: 발판 3개를 레벨에 배치하되, 각각 `SectionIndex`를 1, 2, 3으로 다르게 설정.

---

### E-1.4 BP_TutorialStartTile (시작 타일)

| 항목 | 값 |
|---|---|
| **위치** | `Content/Blueprints/Tutorial/BP_TutorialStartTile` |
| **부모 클래스** | `ADRTutorialStartTile` (C++) |

**설정:**
1. `TileMesh` → 시작 타일 메시 (다른 발판과 구분되는 디자인, 예: 화살표 표시된 타일)
2. `TriggerBox`:
   - Box Extent: (100, 100, 50)
   - Collision: OverlapOnlyPawn
   - Generate Overlap Events: True
3. `TutorialManager`: 레벨 배치 후 연결

---

### E-1.5 BP_TutorialDummy (샌드백 적)

| 항목 | 값 |
|---|---|
| **위치** | `Content/Blueprints/Character/Enemy/BP_TutorialDummy` |
| **부모 클래스** | `ADREnemy` (기존 적 클래스) |

**Details 패널 설정:**

| 프로퍼티 | 값 | 설명 |
|---|---|---|
| `bIsTutorialDummy` | **True** | 무적 모드 활성화 |
| `BehaviorTree` | **None (비워두기)** | AI 행동 없음 (제자리 대기) |
| `CharacterClass` | 기존 적과 동일 | AttributeSet 초기화용 |
| `MaxHealth` (또는 관련 GE) | 1000 이상 | 넉넉한 체력 (어차피 무적) |

**Skeletal Mesh / Animation:**
- 기존 적과 동일한 스켈레탈 메시 사용
- AnimBP: 기존 적의 AnimBP 사용 (히트리액션 재생을 위해)
- Capsule 크기: 기존 적과 동일

**참고**: `TutorialManagerRef`는 C++ `SetupDummyEnemy()`에서 자동 설정되므로 에디터에서 설정 불필요.

---

### E-1.6 DA_TutorialCharacterClassInfo (튜토리얼 전용 캐릭터 클래스 정보)

| 항목 | 값 |
|---|---|
| **위치** | `Content/Blueprints/AbilitySystem/Data/DA_TutorialCharacterClassInfo` |
| **부모 클래스** | `UCharacterClassInfo` (기존 데이터에셋 타입) |

**설정 (기존 DA_CharacterClassInfo를 복제 후 수정):**

1. 콘텐츠 브라우저에서 기존 `DA_CharacterClassInfo` 우클릭 → **Duplicate**
2. 이름을 `DA_TutorialCharacterClassInfo`으로 변경
3. 열어서 수정:

| 항목 | 원본 | 튜토리얼용 |
|---|---|---|
| `CharacterClassInformation[GardenRobot].StartupAbilities` | [GA_ClawSwipe, GA_WaterPump, GA_SeedCannon, ...] | **빈 배열 []** |
| `CommonAbilities` | [GA_HitReact, GA_Death, ...] | **기존 유지** (히트리액션, 사망 등 기본 어빌리티 필요) |
| `SecondaryAttributes` | 기존 GE | **기존 유지** |
| `VitalAttributes` | 기존 GE | **기존 유지** |

> **핵심**: `StartupAbilities`만 비우면 된다. 캐릭터가 전투 스킬 없이 스폰되며, TutorialManager가 구간 2에서 순차적으로 부여한다.

---

### E-1.7 WBP_TutorialOverlay (튜토리얼 전용 오버레이 위젯)

| 항목 | 값 |
|---|---|
| **위치** | `Content/Blueprints/UI/Overlay/WBP_TutorialOverlay` |
| **부모 클래스** | `UDRUserWidget` |

**핵심: 기존 `WBP_PhaseObjective` 위젯을 그대로 배치하여 목표 UI를 재사용한다.**

`WBP_PhaseObjective`는 이미 `OverlayWidgetController`의 `OnObjectiveTextChanged`와 `OnObjectiveProgressChanged` 델리게이트에 바인딩되어 있으므로, `DRTutorialManager::UpdateObjectiveUI()`가 호출되면 자동으로 텍스트와 진행도가 업데이트된다. 새로운 Text Block이나 Progress Bar를 만들어 수동 바인딩할 필요가 없다.

**위젯 구조 (Designer 탭):**

```
[Canvas Panel] (Root)
├── [WBP_HealthWaterBar]          ← 기존 HP/Water 바 위젯 (좌측 상단)
│     Anchors: Top-Left
│     Position: (20, 20)
│
├── [WBP_PhaseObjective]          ← 기존 목표 위젯 재사용 (상단 중앙)
│     Anchors: Top-Center
│     Position: (0, 30)
│     Alignment: (0.5, 0)
│     ※ 이 위젯은 자체적으로 OnObjectiveTextChanged/
│       OnObjectiveProgressChanged에 바인딩됨.
│       TutorialManager가 UpdateObjectiveUI()를 호출하면
│       자동으로 목표 텍스트와 진행도가 표시된다.
│
├── [Horizontal Box] "SkillSlotBox"  ← 스킬 아이콘 (하단 중앙)
│     Anchors: Bottom-Center
│     Position: (0, -80)
│     Alignment: (0.5, 1.0)
│     │
│     ├── [WBP_SkillSlot] "SkillSlot_LMB"  ← 기본 공격 (InputTag.LMB)
│     ├── [WBP_SkillSlot] "SkillSlot_RMB"  ← 물대포 (InputTag.RMB)
│     └── [WBP_SkillSlot] "SkillSlot_Q"    ← 시드캐논 (InputTag.Q)
│
└── [Text Block] "ControlGuideText"  ← 조작 안내 (하단)
      Anchors: Bottom-Center
      Position: (0, -30)
      Alignment: (0.5, 1.0)
      Font Size: 12
      Color: Light Gray
      Text: ""
```

**Event Graph 설정:**

1. **Widget Controller 바인딩** (`Event Construct` 또는 `SetWidgetController` 오버라이드):

```
Event SetWidgetController
  → Cast to OverlayWidgetController
  → Bind Event to AbilityInfoDelegate
      → (기존 WBP_StageOverlay의 스킬 아이콘 갱신 로직 동일하게 적용)
```

> **WBP_PhaseObjective 관련 바인딩은 불필요**: WBP_PhaseObjective가 자체적으로 WidgetController에 바인딩하므로 WBP_TutorialOverlay에서는 배치만 하면 된다.

2. **스킬 아이콘 갱신** (AbilityInfoDelegate 콜백):
   - `AbilityInfoDelegate`에서 전달되는 `AbilityInfo` 배열을 순회
   - 각 `InputTag`에 해당하는 `WBP_SkillSlot`의 아이콘과 Visibility를 설정
   - 아직 부여되지 않은 어빌리티의 슬롯은 Hidden 유지

> **팁**: 기존 `WBP_StageOverlay`의 이벤트 그래프를 참고하되, 목표 UI는 WBP_PhaseObjective가 처리하므로 스킬 아이콘 갱신만 구현하면 된다.

---

### E-1.8 BP_DRTutorialHUD

| 항목 | 값 |
|---|---|
| **위치** | `Content/Blueprints/UI/HUD/BP_DRTutorialHUD` |
| **부모 클래스** | `ADRHUD` (기존 C++ HUD 클래스) |

**Details 패널 설정:**

| 프로퍼티 | 값 |
|---|---|
| `OverlayWidgetClass` | `WBP_TutorialOverlay` (E-1.7에서 생성) |

> 기존 `BP_DRHUD`와 동일하되, `OverlayWidgetClass`만 튜토리얼 전용 위젯으로 교체한 것.

---

### E-1.9 BP_DRTutorialGameMode 수정

| 항목 | 값 |
|---|---|
| **위치** | `Content/Blueprints/Game/BP_DRTutorialGameMode` (기존 파일) |
| **부모 클래스** | `ADRTutorialGameMode` (기존 C++) |

**Details 패널 설정 수정:**

| 프로퍼티 | 값 | 설명 |
|---|---|---|
| `HUDClass` | `BP_DRTutorialHUD` | 튜토리얼 전용 HUD |
| `PlayerCharacterClassInfo` | `DA_TutorialCharacterClassInfo` | 스킬 없이 시작 |
| `DefaultPawnClass` | `BP_GardenRobot` (기존) | 캐릭터는 동일 |
| `PlayerControllerClass` | `BP_DRPlayerController` (기존) | 기존 유지 |
| `GameStateClass` | 기존 값 유지 | |

---

## E-2. TutorialMap 레벨 배치

### E-2.1 맵 구조 개요

TutorialMap은 3개 구간으로 나뉘며, 각 구간 사이에 **문(Gate)**이 있고, 각 구간 끝에 **발판(PressurePlate)**이 있다.

```
[스폰 지점]
    │
[═══ 구간 1: 파쿠르 구간 ═══]
    │  - 장애물 (StaticMesh로 배치: 벽, 계단, 점프대 등)
    │  - PlayerStart 근처에 배치
    │
[발판 1] ← PressurePlate (SectionIndex=1)
[문 1]   ← Gate
    │
[═══ 구간 2: 전투 훈련 구간 ═══]
    │  - [중앙] DummyEnemy_Center (BP_TutorialDummy)
    │  - [동서남북 4곳] SeedCannonSpawnPoints (TargetPoint x4~5개)
    │  - 넓은 평지 (전투 공간)
    │
[발판 2] ← PressurePlate (SectionIndex=2)
[문 2]   ← Gate
    │
[═══ 구간 3: 부품 수집 구간 ═══]
    │  - [입구] StartTile (BP_TutorialStartTile)
    │  - [구간 내] PartEnemies (ADREnemy x2, bCarriesPart=true)
    │  - [중앙] TutorialCleanserSite (ADRCleanserSite)
    │
[발판 3] ← PressurePlate (SectionIndex=3)
    │
[튜토리얼 완료 → 메인메뉴 복귀]
```

---

### E-2.2 액터 배치 상세

#### 1) PlayerStart
- 구간 1 시작 지점에 배치
- 위치: 맵 시작점

#### 2) BP_TutorialManager (1개)
- 맵 아무 곳에 배치 (물리적 존재감 없음, 관리 액터)
- 배치 후 Details 패널에서 아래 인스턴스 참조를 모두 연결:

| 프로퍼티 | 연결 대상 |
|---|---|
| `Gate1` | 구간 1↔2 사이의 BP_TutorialGate |
| `Gate2` | 구간 2↔3 사이의 BP_TutorialGate |
| `PressurePlate1` | 구간 1 끝의 BP_TutorialPressurePlate (SectionIndex=1) |
| `PressurePlate2` | 구간 2 끝의 BP_TutorialPressurePlate (SectionIndex=2) |
| `PressurePlate3` | 구간 3 끝의 BP_TutorialPressurePlate (SectionIndex=3) |
| `StartTile` | 구간 3 입구의 BP_TutorialStartTile |
| `DummyEnemy_Center` | 구간 2 중앙의 BP_TutorialDummy |
| `SeedCannonSpawnPoints` | 구간 2의 TargetPoint 4~5개 (배열) |
| `PartEnemies` | 구간 3의 부품 적 ADREnemy 2마리 (배열) |
| `TutorialCleanserSite` | 구간 3의 ADRCleanserSite |

#### 3) BP_TutorialGate (2개)

- **Gate1**: 구간 1과 구간 2 사이
  - 통로를 완전히 막도록 크기 조정
  - Rotation: 통로에 수직으로

- **Gate2**: 구간 2와 구간 3 사이
  - 위와 동일

#### 4) BP_TutorialPressurePlate (3개)

- **PressurePlate1**: 구간 1 끝 (Gate1 바로 앞)
  - `SectionIndex` = **1**
  - `TutorialManager` = BP_TutorialManager 인스턴스

- **PressurePlate2**: 구간 2 끝 (Gate2 바로 앞)
  - `SectionIndex` = **2**
  - `TutorialManager` = BP_TutorialManager 인스턴스

- **PressurePlate3**: 구간 3 끝
  - `SectionIndex` = **3**
  - `TutorialManager` = BP_TutorialManager 인스턴스

#### 5) BP_TutorialDummy (1개 - 중앙 샌드백)

- 구간 2 중앙에 배치
- `bIsTutorialDummy` = True (BP 기본값으로 설정됨)
- BP_TutorialManager의 `DummyEnemy_Center`에 연결

#### 6) TargetPoint (4~5개 - SeedCannon 스폰 위치)

- 구간 2에서 중앙 샌드백 주변 동서남북 + 중앙에 배치
- 각 TargetPoint 간 간격: 약 200~300 유닛 (SeedCannon 외곽 반경 400 이내)
- BP_TutorialManager의 `SeedCannonSpawnPoints` 배열에 추가

#### 7) BP_TutorialStartTile (1개)

- 구간 3 입구에 배치
- `TutorialManager` = BP_TutorialManager 인스턴스

#### 8) 부품 적 (ADREnemy 2마리)

- 구간 3 내부에 직접 배치 (기존 적 BP 사용)
- 각 적의 Details:
  - `bCarriesPart` = **True**
  - `BehaviorTree` = 기존 도주형 BT (적이 부품을 들고 도망하는 AI)
- BP_TutorialManager의 `PartEnemies` 배열에 추가

#### 9) ADRCleanserSite (1개)

- 구간 3 중앙에 배치
- 기존 CleanserSite BP 사용
- Actor Tag: `"CleanserSite"` 확인
- BP_TutorialManager의 `TutorialCleanserSite`에 연결

#### 10) 구간 1 장애물

- StaticMesh 액터로 파쿠르 코스 구성:
  - 낮은 벽 (점프로 넘기)
  - 계단형 구조물
  - 갭 (점프 필요 구간)
- Collision: BlockAll
- 난이도는 쉽게 (튜토리얼이므로)

---

### E-2.3 World Settings 설정

TutorialMap의 World Settings에서:

| 프로퍼티 | 값 |
|---|---|
| `GameMode Override` | `BP_DRTutorialGameMode` |

---

## E-3. 레벨 스트리밍 프리뷰 (선택사항)

기존 `SL_TutorialPreview.umap`이 존재한다면, TutorialMap에 서브레벨로 추가하거나 메인메뉴의 레벨 전환에서 올바른 맵 이름을 사용하고 있는지 확인.

---

## E-4. 검증 체크리스트

### E-4.1 블루프린트 컴파일 확인

- [ ] `BP_TutorialManager` — 컴파일 성공, 경고 없음
- [ ] `BP_TutorialGate` — 컴파일 성공
- [ ] `BP_TutorialPressurePlate` — 컴파일 성공
- [ ] `BP_TutorialStartTile` — 컴파일 성공
- [ ] `BP_TutorialDummy` — 컴파일 성공
- [ ] `WBP_TutorialOverlay` — 컴파일 성공, 바인딩 확인
- [ ] `BP_DRTutorialHUD` — 컴파일 성공
- [ ] `BP_DRTutorialGameMode` — 컴파일 성공

### E-4.2 참조 연결 확인

- [ ] BP_TutorialManager의 모든 EditInstanceOnly 프로퍼티가 레벨 액터와 연결됨
- [ ] 각 PressurePlate의 `TutorialManager`와 `SectionIndex` 설정됨
- [ ] StartTile의 `TutorialManager` 설정됨
- [ ] BP_DRTutorialGameMode의 `HUDClass`, `PlayerCharacterClassInfo` 설정됨

### E-4.3 게임플레이 테스트

**구간 1 테스트:**
- [ ] 게임 시작 시 UI에 "장애물을 넘어 이동하세요" 표시
- [ ] 스킬 아이콘 없음 (빈 슬롯)
- [ ] 장애물 통과 → 발판1 밟기 → 문1 열림
- [ ] 문이 위로 슬라이드하며 열림

**구간 2 테스트 - 목표 1 (기본 공격):**
- [ ] 구간 2 진입 시 LMB 스킬 아이콘 표시
- [ ] UI: "기본 공격으로 적을 공격하세요 (좌클릭)" + "0 / 5 적중"
- [ ] 샌드백 적 좌클릭 공격 → 히트리액션 재생, 체력 1 이하로 안 떨어짐
- [ ] 5회 적중 시 목표 2로 자동 전환

**구간 2 테스트 - 목표 2 (물대포):**
- [ ] RMB 스킬 아이콘 추가 표시
- [ ] UI: "물대포를 발사하세요 (우클릭 유지)" + "0 / 3 초"
- [ ] 우클릭 유지 → 시간 카운트 증가
- [ ] 3초 달성 시 목표 3으로 자동 전환

**구간 2 테스트 - 목표 3 (SeedCannon):**
- [ ] Q 스킬 아이콘 추가 표시
- [ ] 동서남북에 추가 적 4마리 스폰됨
- [ ] UI: "시드캐논으로 모든 적을 한 번에 맞추세요 (Q)" + "0 / 5 동시 적중"
- [ ] SeedCannon으로 5마리 동시 적중 → "전투 훈련 완료! 발판을 밟으세요"
- [ ] 실패 시 현재 적중 수 표시 (재시도 가능)
- [ ] 발판2 밟기 → 문2 열림

**구간 3 테스트:**
- [ ] UI: "시작 타일을 밟아 부품 수집을 시작하세요"
- [ ] 시작 타일 밟기 전: 부품 적 2마리 정지 상태
- [ ] 시작 타일 밟기 → 적 AI 활성화, 도주 시작
- [ ] UI: "부품을 들고 있는 적을 잡아 부품을 수집하세요"
- [ ] 적 처치 → 부품 드롭 → 부품 줍기 → 클렌저사이트에 설치
- [ ] 부품 설치 시 UI: "부품을 클렌저사이트에 설치하세요" + "1 / 2 설치 완료"
- [ ] 2개 모두 설치 → "튜토리얼 완료! 발판을 밟으세요"
- [ ] 발판3 밟기 → 튜토리얼 완료

**완료 후 테스트:**
- [ ] SaveGame에 `bTutorialCompleted = true` 저장됨
- [ ] MainMenu로 복귀
- [ ] MainMenu에서 다시 "Start Game" → 로비 모드 (호스트/참가) 화면 표시 (튜토리얼 스킵)

### E-4.4 엣지 케이스

- [ ] 발판을 구간 클리어 전에 밟으면 아무 반응 없음
- [ ] 구간 2에서 샌드백 적은 죽지 않음 (체력 최소 1)
- [ ] SeedCannon 동시 적중 실패 시 재시도 가능 (적이 리스폰되어 있음)
- [ ] 구간 3 시작 타일은 1회만 활성화
- [ ] 문은 열린 후 다시 닫히지 않음
