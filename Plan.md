# Plan — 스킬 차단(Block) 상태 시각화 (스킬아이콘 빨간색 처리)

## 0. 개요

### 목적
1. 특정 스킬이 활성화되어 있는 동안, 그 스킬이 차단하기로 지정한 다른 스킬들을 "차단 상태"로 표시(스킬 아이콘 빨간색)
2. `State.Carrying` 태그가 owner에게 붙어 있는 동안에는 **모든 스킬 아이콘**을 빨간색으로 표시
3. 차단이 풀리면 즉시 원래 색으로 복귀

### 예시 시나리오
- `Abilities.GardenRobot.ClawSwipe` GA가 활성화 중일 때:
  - 같은 캐릭터의 SkillSlot 중 다음 AbilityTag를 가진 슬롯이 빨갛게 변함
    - `Abilities.GardenRobot.SeedCannon`
    - `Abilities.GardenRobot.WaterPump`
    - `State.Carrying`
- 플레이어가 부품(Part)을 들어 `State.Carrying`이 ASC에 추가되면:
  - GardenRobot의 모든 스킬 슬롯(ClawSwipe, WaterPump, SeedCannon 등)이 빨갛게 변함

### 핵심 설계 결정
- **GAS의 표준 메커니즘을 그대로 사용**한다. 즉:
  - 각 GA의 `AbilityTags` 에 자기 식별 태그를 넣고 (이미 그렇게 되어 있음)
  - 각 GA의 `ActivationOwnedTags` 에도 자기 식별 태그를 넣어, 활성화 동안 owner ASC에 태그가 추가되도록 한다.
  - **"내가 활성화되면 X, Y, Z 태그를 가진 GA를 차단"** 은 GA의 `BlockAbilitiesWithTag` 에 그 태그들을 등록해 처리한다.
- UI(WBP_SkillSlot)는 **owner ASC의 차단 카운터(BlockedAbilityTags) 변화를 구독**해서, "내 AbilityTag가 차단되어 있는가?"를 매번 다시 평가해 색을 갱신한다.
- `State.Carrying` 은 GE 가 아닌 LooseGameplayTag로 붙고 있으므로, 동일한 UI 흐름이 자연스럽게 동작하도록 **태그 카운트 이벤트(`RegisterGameplayTagEvent`)** 도 함께 구독한다.

### 작업 산출물
1. C++ 변경
   - `UDRAbilitySystemComponent`: 차단 상태 변경을 알리는 델리게이트 + 등록/해제 헬퍼
   - `UOverlayWidgetController`: ASC 차단 델리게이트와 `State.Carrying` 태그 이벤트를 바인딩하고, UI로 재방송
   - (선택) `UDRAbilitySystemLibrary`: "이 AbilityTag가 지금 차단 상태인가?" 정적 헬퍼
2. Blueprint 변경
   - 각 GA 블루프린트의 `ActivationOwnedTags`, `BlockAbilitiesWithTag` 설정
   - `WBP_SkillSlot`: 차단 상태 바인딩 + 빨간색 색 전환 로직
3. Gameplay Tag
   - 기존 `Abilities.GardenRobot.*`, `State.Carrying` 만 사용. 신규 태그 추가 없음.

---

## 1. GAS 측 동작 정리 (왜 이 방식인가)

`UGameplayAbility`는 활성화될 때 owner ASC 에 다음을 적용한다:
- `ActivationOwnedTags` → owner의 GameplayTagContainer에 추가
- `BlockAbilitiesWithTag` → owner의 **BlockedAbilityTags 카운터**에 추가
- `CancelAbilitiesWithTag` → 매칭되는 다른 active ability를 즉시 취소

활성화가 끝나면(EndAbility) 둘 다 자동으로 빠진다. 즉 우리는 **추가 코드 없이** GAS 만으로 "ClawSwipe 활성 중에는 SeedCannon/WaterPump 가 활성화 시도 시 즉시 거부" 까지 보장된다. 우리가 해야 할 일은 두 가지:

1. **차단되고 있다는 사실을 UI 쪽에서 알 수 있게 노출** (`AreAbilityTagsBlocked` 호출 + 변경 시점 알림 델리게이트)
2. `State.Carrying` 의 경우는 LooseGameplayTag이므로 GE 가 안 끼어들지만, **모든 스킬 슬롯이 본인이 "차단" 상태인지 다시 평가하도록 트리거**만 쳐 주면 된다.

---

## 2. C++ 작업

### 2.1 `UDRAbilitySystemComponent` 확장

#### Public 추가
```cpp
// AbilitySystem/DRAbilitySystemComponent.h

// 차단된 AbilityTag 집합이 바뀔 때마다 호출 (인자 없음 — UI는 자기 AbilityTag 기준 재평가)
DECLARE_MULTICAST_DELEGATE(FOnBlockedAbilityTagsChanged);

UCLASS()
class DAERUNE_API UDRAbilitySystemComponent : public UAbilitySystemComponent
{
    ...
public:
    /** BlockAbilitiesWithTag 카운터가 변경될 때 브로드캐스트. UI 슬롯이 자기 태그 기준으로 재평가. */
    FOnBlockedAbilityTagsChanged OnBlockedAbilityTagsChanged;

    /** ASC 외부에서 "지금 이 AbilityTag가 차단되어 있는가?" 를 묻기 위한 헬퍼 (단일 태그). */
    bool IsAbilityTagBlocked(const FGameplayTag& AbilityTag) const;
```

#### Protected/내부
- `UAbilitySystemComponent::BlockAbilitiesWithTags(const FGameplayTagContainer& Tags)` / `UnBlockAbilitiesWithTags(...)` 는 **virtual이 아니다**. 따라서 override가 안 된다.
  대신 차단 카운터 변화를 감지하기 위해 **AbilityActorInfoSet 시점에 owner ASC의 모든 관심 태그들에 `RegisterGameplayTagEvent`** 를 건다. 단, 차단 카운터(BlockedAbilityTags)는 별도 컨테이너이므로 일반 GameplayTagEvent가 안 잡힌다.

  실용적 해결: **ActivationOwnedTags 가 추가/제거되는 시점을 트리거로 삼는다.**
  우리가 만드는 모든 "차단을 유발하는 GA" 는 자기 AbilityTag를 `ActivationOwnedTags`에도 넣는 규약을 강제하므로, owner ASC 의 그 태그 카운트가 바뀌는 순간 = BlockAbilitiesWithTag 도 같이 바뀌는 순간이다.

  이 규약을 코드에서도 강제하기 위해, `UDRGameplayAbility` 의 `PostInitProperties` 또는 에디터 검증(`#if WITH_EDITOR` `IsDataValid`)에서:
  - `AbilityTags`에 있는 태그가 `ActivationOwnedTags`에도 있는지 확인하고, 없으면 에디터 경고를 띄움.
  - 권장: 런타임에서도 `AbilityTags`를 `ActivationOwnedTags`에 머지(머지 시점은 `OnGiveAbility` 가 안전).

#### 등록 흐름 (의사 코드) — **캐릭터별 어빌리티에 자동 대응**

핵심: 정적 `WatchedTags` 배열을 두지 않는다. 대신
- **글로벌(캐릭터 무관) 태그** 만 정적으로 등록 (현재는 `State.Carrying` 만)
- **각 GA 의 AbilityTags** 는 `AbilitiesGivenDelegate` 가 발화한 직후 부여된 어빌리티에서 자동 수집해 등록

이 구조에서는 GardenRobot 이든 VendingMachine 이든 새 캐릭터든, **GA 만 잘 만들어 두면 C++ 수정 없이 모두 동작**한다.

```cpp
// 헤더에 추가
protected:
    void HandleWatchedTagChanged(const FGameplayTag Tag, int32 NewCount);
    void RegisterAbilityTagEvents(); // AbilitiesGivenDelegate 콜백
    TSet<FGameplayTag> RegisteredWatchedTags; // 중복 등록 방지

// 구현
void UDRAbilitySystemComponent::AbilityActorInfoSet()
{
    Super::AbilityActorInfoSet();

    // (1) 글로벌 태그: 캐릭터 무관하게 항상 감시 대상
    static const TArray<FGameplayTag> GlobalWatchedTags = {
        FDRGameplayTags::Get().State_Carrying,
        // (필요 시 캐릭터 무관 글로벌 태그를 추가)
    };
    for (const FGameplayTag& Tag : GlobalWatchedTags)
    {
        if (RegisteredWatchedTags.Contains(Tag)) continue;
        RegisteredWatchedTags.Add(Tag);
        RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &UDRAbilitySystemComponent::HandleWatchedTagChanged);
    }

    // (2) 어빌리티 부여 직후 자동 등록
    //     AddCharacterAbilities 마지막에 AbilitiesGivenDelegate.Broadcast() 가 호출됨
    AbilitiesGivenDelegate.AddUObject(this,
        &UDRAbilitySystemComponent::RegisterAbilityTagEvents);
}

void UDRAbilitySystemComponent::RegisterAbilityTagEvents()
{
    // 현재 부여된 모든 어빌리티 스펙을 순회하며 AbilityTags 수집
    for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
    {
        if (!Spec.Ability) continue;
        for (const FGameplayTag& Tag : Spec.Ability->AbilityTags)
        {
            if (RegisteredWatchedTags.Contains(Tag)) continue;
            RegisteredWatchedTags.Add(Tag);

            RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
                .AddUObject(this, &UDRAbilitySystemComponent::HandleWatchedTagChanged);
        }
    }
}

void UDRAbilitySystemComponent::HandleWatchedTagChanged(const FGameplayTag, int32 NewCount)
{
    OnBlockedAbilityTagsChanged.Broadcast();
}

bool UDRAbilitySystemComponent::IsAbilityTagBlocked(const FGameplayTag& AbilityTag) const
{
    FGameplayTagContainer Single;
    Single.AddTag(AbilityTag);
    return AreAbilityTagsBlocked(Single);
}
```

> **메모**: `AreAbilityTagsBlocked`는 `UAbilitySystemComponent`의 public 멤버다. 그 내부적으로 `BlockedAbilityTags.GetExplicitGameplayTags().HasAny(Tags)` 를 검사한다. 우리가 알림 트리거를 ActivationOwnedTags 변화로 묶었기 때문에, 같은 GA가 그 두 컨테이너에 동일 태그를 넣어 두면 `IsAbilityTagBlocked` 의 결과가 항상 최신 상태로 노출된다.

> **튜토리얼/동적 부여 케이스**: 튜토리얼 매니저처럼 `AbilitiesGivenDelegate` 와 별도로 어빌리티를 부여하는 경로가 있다면, 해당 경로에서도 `RegisterAbilityTagEvents()` 를 명시적으로 한 번 호출해 주거나, 부여 직후 직접 같은 등록 로직을 태워야 한다. (`UDRAbilitySystemComponent` 에 `AddToInputTagCache` 가 이미 있는 것처럼, `RegisterAbilityTagEvents` 도 외부에서 호출 가능한 public 헬퍼로 두는 것을 권장.)

#### 왜 AbilityTag 인가 (InputTag 안 쓰는 이유)
- **GAS 의 차단 메커니즘 자체가 AbilityTag 기반**: `BlockAbilitiesWithTag` / `AreAbilityTagsBlocked` 모두 AbilityTag만 본다. InputTag 로는 GAS 차단을 표현 못 한다.
- **InputTag 는 너무 거칠다**: LMB/RMB/Q/E 4종뿐이라 캐릭터마다 같은 키에 다른 스킬이 들어간다. "ClawSwipe 가 SeedCannon 을 차단" 같은 정밀 정책이 표현되지 않고 "LMB가 RMB 차단" 으로 뭉뚱그려진다.
- **차후 확장에 막힌다**: 같은 InputTag 에 두 스킬이 매핑되거나, 새 입력 패턴이 들어오면 매핑이 깨진다.

UI(WBP_SkillSlot) 가 본인을 식별하는 키는 `FDRAbilityInfo.AbilityTag` 이므로, 캐릭터가 바뀌어 스킬아이콘 위젯 클래스가 교체돼도(`OnSkillIconClassChanged` 경로) 본인 AbilityTag 로 `IsAbilityBlockedNow` 만 물어보면 동일하게 동작한다.

#### `State.Carrying` 의 특수성
- `State.Carrying`은 GE 가 아니라 `AddLooseGameplayTag` 로 추가된다 (`DRCleanserPart.cpp:99`, `DRCharacter.cpp:289`).
- `RegisterGameplayTagEvent` 는 LooseGameplayTag 변경도 동일하게 잡아 준다(`AddLooseGameplayTag`는 내부적으로 GameplayTagCountContainer 를 거치므로).
- 따라서 글로벌 WatchedTags 에 `State.Carrying` 만 포함해 두면 UI는 자동으로 갱신된다.
- 단, "모든 스킬을 빨갛게" 하기 위한 차단 로직은 다음 절(2.2)에서 처리한다.

### 2.2 "Carrying 중 전부 차단" 의 단일 진입점

두 가지 후보:

**(A) GA 측에서 처리 (권장)**
- 모든 플레이어 사용 GA의 `ActivationBlockedTags` 또는 `SourceBlockedTags` 에 `State.Carrying` 을 추가.
- 효과: Carrying 중에는 어떤 GA도 활성화 안 됨. (이미 게임 동작상 이렇게 되어 있다고 함.)
- UI 측에서는 **WBP_SkillSlot 이 본인 평가 시 "owner ASC 가 State.Carrying을 가지면 무조건 빨강"** 으로 처리.

**(B) 가상의 "MasterBlock" GE 또는 BlockAbilitiesWithTag 로 일괄 차단**
- 별도 GA(예: `GA_CarryingBlocker`)를 만들어 Carrying 시작 시 자동 활성화, 종료 시 자동 종료. 그 GA의 `BlockAbilitiesWithTag` 에 모든 스킬 AbilityTag 를 등록.
- 장점: UI가 일반 차단 로직만 보면 된다(특수 케이스 분기 불필요).
- 단점: 운용 GA 가 늘어남.

**채택**: 본 계획은 (A) 를 채택. UI 쪽 분기 한 줄이면 충분하고, 실제 게임 로직(activation block)은 이미 동작하는 상태이므로 변경 폭이 작다.

### 2.3 `UOverlayWidgetController` 연동

`OverlayWidgetController.h` 에 다음 추가:
```cpp
// 한 슬롯이 자기 차단 상태를 재평가해야 함을 알리는 신호 (인자 없음)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAbilityBlockStateDirtySignature);

UPROPERTY(BlueprintAssignable, Category = "GAS|SkillIcon")
FOnAbilityBlockStateDirtySignature OnAbilityBlockStateDirty;

UFUNCTION(BlueprintPure, Category = "GAS|SkillIcon")
bool IsAbilityBlockedNow(FGameplayTag AbilityTag) const;
```

`OverlayWidgetController.cpp`:
```cpp
void UOverlayWidgetController::BindCallbacksToDependencies()
{
    Super::BindCallbacksToDependencies();
    ...
    if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(AbilitySystemComponent))
    {
        DRASC->OnBlockedAbilityTagsChanged.AddUObject(this,
            &UOverlayWidgetController::HandleBlockedTagsChanged);
    }
}

void UOverlayWidgetController::HandleBlockedTagsChanged()
{
    OnAbilityBlockStateDirty.Broadcast(); // 모든 슬롯 위젯이 알아서 재평가
}

bool UOverlayWidgetController::IsAbilityBlockedNow(FGameplayTag AbilityTag) const
{
    if (!AbilitySystemComponent) return false;

    const FDRGameplayTags& Tags = FDRGameplayTags::Get();

    // Carrying 중에는 모든 슬롯 차단으로 간주
    if (AbilitySystemComponent->HasMatchingGameplayTag(Tags.State_Carrying))
    {
        return true;
    }

    // 일반 차단 카운터 검사
    FGameplayTagContainer Single;
    Single.AddTag(AbilityTag);
    return AbilitySystemComponent->AreAbilityTagsBlocked(Single);
}
```

`UnbindAllDelegates()` 에 `OnBlockedAbilityTagsChanged` 해제 추가.

---

## 3. Blueprint 작업

### 3.1 각 GA 설정 (Class Defaults → Tags)

대상 GA: `GA_ClawSwipe`, `GA_WaterPump`, `GA_SeedCannon`, `GA_VendingMachineBasicAttack`, `GA_VendingMachineAttackSpeedBuff`, 및 향후 추가되는 모든 캐릭터의 GA.

**공통 규칙 (캐릭터 무관)**
- 모든 GA 의 `ActivationOwnedTags` 에 자기 `AbilityTags` 와 동일한 태그를 포함시킨다. (이것이 UI 트리거의 핵심 — §2.1 참조)
- `UDRGameplayAbility::OnGiveAbility` 에서 자동 머지하므로 BP 에서 깜빡해도 안전하지만, 명시적으로 넣어 두는 것을 권장.

**GardenRobot 예시**
| GA | AbilityTags | ActivationOwnedTags | BlockAbilitiesWithTag |
|---|---|---|---|
| GA_ClawSwipe | `Abilities.GardenRobot.ClawSwipe` | 동일 | `Abilities.GardenRobot.SeedCannon`, `Abilities.GardenRobot.WaterPump`, `State.Carrying` |
| GA_WaterPump | `Abilities.GardenRobot.WaterPump` | 동일 | `Abilities.GardenRobot.ClawSwipe`, `Abilities.GardenRobot.SeedCannon`, `State.Carrying` |
| GA_SeedCannon | `Abilities.GardenRobot.SeedCannon` | 동일 | `Abilities.GardenRobot.ClawSwipe`, `Abilities.GardenRobot.WaterPump`, `State.Carrying` |

**VendingMachine 예시**
| GA | AbilityTags | ActivationOwnedTags | BlockAbilitiesWithTag |
|---|---|---|---|
| GA_VendingMachineBasicAttack | `Abilities.VendingMachine.BasicAttack` | 동일 | `Abilities.VendingMachine.AttackSpeedBuff`, `State.Carrying` |
| GA_VendingMachineAttackSpeedBuff | `Abilities.VendingMachine.AttackSpeedBuff` | 동일 | `Abilities.VendingMachine.BasicAttack`, `State.Carrying` |

> **정책의 본질**: "어떤 스킬이 어떤 스킬을 막는가" 는 캐릭터별로 다를 수 있으므로 GA 블루프린트의 `BlockAbilitiesWithTag` 가 단일 진실 소스(SSOT)가 된다. C++/UI 는 이 정책에 종속되지 않는다.

체크 포인트:
- **ActivationOwnedTags 누락 시 UI가 갱신 안 됨** (2.1 의 트리거가 이 컨테이너 변화에 의존). 누락 방지를 위해 `UDRGameplayAbility::OnGiveAbility` 에서 `AbilityTags`를 `ActivationOwnedTags`로 머지하는 보호 코드 권장.
- **CancelAbilitiesWithTag** 도 같은 태그 목록으로 함께 채우면, 이미 활성화된 다른 GA 가 새 GA 발동 시 즉시 취소된다(스킬 차단 정책에 맞게 선택).
- **State.Carrying 의 단일 진입점**: 모든 캐릭터의 모든 GA 의 BlockAbilitiesWithTag 에 `State.Carrying` 을 동일하게 포함시키는 것을 약속(또는 2.2 의 옵션 B 처럼 별도 BlockerGA 로 통합).

### 3.2 `WBP_SkillSlot` 변경
가정: 각 슬롯은 자기 `FDRAbilityInfo`를 가지고 있고, 그 안에 `AbilityTag` 가 있음(`AbilityInfo.h:18`).

추가 변수:
- `MyAbilityTag : FGameplayTag` (이미 AbilityInfo에서 노출되어 있다면 그것 사용)
- `IconImage : Image` (스킬 아이콘 이미지 위젯 참조)
- `NormalColor : LinearColor` (예: 1,1,1,1)
- `BlockedColor : LinearColor` (예: 1, 0.15, 0.15, 1)

추가 함수: `RefreshBlockedState`
```
function RefreshBlockedState()
    HUD = GetOwningPlayer().GetHUD() as ADRHUD
    WC  = HUD.GetOverlayWidgetController(...)
    bBlocked = WC.IsAbilityBlockedNow(MyAbilityTag)
    if bBlocked:
        IconImage.SetColorAndOpacity(BlockedColor)
        // 선택: 호버 비활성, 클릭 비활성 등
    else:
        IconImage.SetColorAndOpacity(NormalColor)
```

이벤트 바인딩 (Construct/Initialize 시):
```
WC.OnAbilityBlockStateDirty.AddDynamic(self, RefreshBlockedState)
RefreshBlockedState()   // 초기 1회
```

Destruct 시 `RemoveDynamic` 으로 정리.

쿨다운 색과 충돌 주의:
- 기존 쿨다운 오버레이(회색/반투명)와 빨강이 동시에 들어갈 수 있다. 정책 명확화:
  - 권장: **차단 빨강이 쿨다운보다 우선** (Z order 또는 색 합성). 차단 중에는 쿨다운 표시를 숨기거나, 빨강 틴트만 노출.
- 입력 비활성:
  - 클릭/누름으로 발동하는 슬롯이면 `IsAbilityBlockedNow == true` 일 때 OnClicked 무시.

---

## 4. 태그 정합성 점검 (필수)

- `State.Carrying` 은 이미 정의되어 있으며(`DRGameplayTags.h:36`) LooseGameplayTag 로 동작 중이다.
- `Abilities.GardenRobot.ClawSwipe/WaterPump/SeedCannon` 모두 이미 정의되어 있다(`DRGameplayTags.h:94-96`). 신규 태그 추가 없음.

---

## 5. 구현 순서 (체크리스트)

1. [ ] `UDRGameplayAbility::OnGiveAbility` 오버라이드: `AbilityTags` 의 모든 태그를 `ActivationOwnedTags` 에 머지 (방어 코드)
2. [ ] `UDRAbilitySystemComponent`
   - [ ] `OnBlockedAbilityTagsChanged` 델리게이트 추가
   - [ ] `IsAbilityTagBlocked` 헬퍼 추가
   - [ ] `AbilityActorInfoSet` 에서 `WatchedTags` 에 대해 `RegisterGameplayTagEvent` 바인딩
3. [ ] `UOverlayWidgetController`
   - [ ] `OnAbilityBlockStateDirty` 델리게이트 추가
   - [ ] `IsAbilityBlockedNow(AbilityTag)` BlueprintPure 추가 (Carrying 우선 분기 포함)
   - [ ] `BindCallbacksToDependencies` 에서 ASC 델리게이트 구독
   - [ ] `UnbindAllDelegates` 에서 해제
4. [ ] GA 블루프린트들 (`GA_ClawSwipe`, `GA_WaterPump`, `GA_SeedCannon`, ...)
   - [ ] `ActivationOwnedTags` 에 자기 AbilityTag 추가
   - [ ] `BlockAbilitiesWithTag` 에 정책에 맞는 차단 대상 태그 등록
5. [ ] `WBP_SkillSlot`
   - [ ] `IconImage`, `NormalColor`, `BlockedColor` 변수/디자이너 노출
   - [ ] `RefreshBlockedState` 함수 구현
   - [ ] `OnAbilityBlockStateDirty` 구독 + 초기 1회 호출
   - [ ] (선택) 차단 중 클릭/입력 무시
   - [ ] (선택) 쿨다운 시각화와의 우선순위 정리
6. [ ] 테스트
   - [ ] PIE: ClawSwipe 발동 → SeedCannon/WaterPump 슬롯이 즉시 빨강 → ClawSwipe 종료 시 원복
   - [ ] PIE: 부품 픽업으로 `State.Carrying` 추가 → 모든 슬롯 빨강 → 부품 설치/드롭으로 원복
   - [ ] 멀티플레이(리슨 서버 + 클라 1): 두 클라이언트 모두 정상 색 전환
   - [ ] 쿨다운과 동시에 차단 발생 시 색 충돌 없음

---

## 6. 멀티플레이/네트워크 고려사항

- `BlockAbilitiesWithTag` 는 활성화 시 owner ASC 양측(서버/소유 클라)에서 동일 카운터로 동작 — 별도 RPC 불필요.
- `ActivationOwnedTags` 도 양측에 적용 → `RegisterGameplayTagEvent` 가 양측에서 발화 → 양측 UI 모두 갱신.
- `State.Carrying` 의 LooseGameplayTag 적용 경로:
  - 서버: `DRCleanserPart.cpp:99` 에서 `AddLooseGameplayTag` (replicated 여부 확인 필요).
  - 클라이언트: LooseGameplayTag는 기본적으로 **복제되지 않는다**. 해결책:
    - (1) `AddReplicatedLooseGameplayTag` / `RemoveReplicatedLooseGameplayTag` 사용으로 전환 — UI가 정확히 동기되어 안전. **권장**.
    - (2) 클라이언트에서도 `OnRep_CarriedPart` / `OnRep_bIsCarryingPart` 에서 같은 LooseGameplayTag 를 직접 추가/제거 (현재 코드가 이미 비슷한 RepNotify를 가짐). 이 경로를 유지한다면 RepNotify 안에서 ASC 의 LooseGameplayTag 도 미러링되는지 점검 필요.
- UI 갱신은 모두 owning client 기준이므로, 위의 동기화만 보장되면 추가 RPC는 필요 없다.

---

## 7. 확장 시 주의 (새 캐릭터 / 새 GA 추가)

§2.1 의 동적 등록 구조 덕분에 **C++ 변경 없이** 새 캐릭터/스킬을 추가할 수 있다. 새 GA 를 만들 때 체크할 항목은 BP 측뿐이다:

- [ ] `AbilityTags` 설정 (예: `Abilities.<Character>.<Skill>`)
- [ ] `ActivationOwnedTags` 에 동일 태그 포함 (자동 머지 코드가 보호하지만 명시 권장)
- [ ] `BlockAbilitiesWithTag` 에 차단 정책 명시 (해당 캐릭터의 같이 못 쓰는 스킬 태그 + `State.Carrying`)
- [ ] (선택) `CancelAbilitiesWithTag` 에 동일 정책 적용해 활성 중 어빌리티도 강제 종료

새 캐릭터 클래스를 추가할 때:
- [ ] `FDRGameplayTags` 에 신규 AbilityTag 정의 (`InitializeNativeGameplayTags`)
- [ ] `CharacterClassInfo` 의 StartupAbilities 에 GA 등록
- [ ] `AbilityInfo` 데이터 에셋에 `FDRAbilityInfo` 항목 추가 (UI 슬롯이 AbilityTag → Icon 매핑 시 사용)

> 동적 부여 경로(예: 튜토리얼)는 `RegisterAbilityTagEvents()` 를 한 번 호출하거나, 부여 직후 자동 호출되도록 헬퍼 안에서 트리거를 박아 두면 자동 대응된다.

---

## 8. 리스크 & 대안

| 리스크 | 대응 |
|---|---|
| ActivationOwnedTags 누락으로 UI 가 갱신 안 됨 | `OnGiveAbility` 에서 AbilityTags → ActivationOwnedTags 자동 머지 |
| LooseGameplayTag 복제 누락으로 클라 UI 가 Carrying 인식 못함 | `AddReplicatedLooseGameplayTag` 로 전환 |
| 쿨다운/차단 색 충돌 | 슬롯에서 명시적 우선순위 정의 (차단 > 쿨다운 틴트) |
| 새 캐릭터/스킬 추가 시 등록 누락 | `AbilitiesGivenDelegate` 기반 자동 수집으로 해결됨 (§2.1). 동적 부여 경로만 `RegisterAbilityTagEvents()` 호출 보장 |
| `AreAbilityTagsBlocked` 호출 빈도 | 슬롯 수 × 이벤트 빈도 = 매우 낮음. 폴링 아님. 성능 영향 무시 가능 |
| 캐릭터 전환 시 이전 캐릭터의 등록 태그가 남음 | `RegisteredWatchedTags` 는 ASC 단위로 유지되며, 캐릭터(GA) 가 빠지면 해당 태그는 이벤트가 더 발화하지 않아 무해. PlayerState 가 새로 만들어지는 흐름이라면 자연히 초기화됨 |
