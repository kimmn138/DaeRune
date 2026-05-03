# Plan2: 캐릭터별 스킬 아이콘 UI 시스템

## 결론: "단일 오버레이 + 캐릭터별 스킬아이콘 서브위젯 교체" 방식 채택

### 왜 이 방식인가?

| 방식 | 장점 | 단점 |
|------|------|------|
| **A) 단일 오버레이에서 Visibility 토글** | 위젯 하나만 관리 | 4캐릭 이상 시 BP 스파게티, 레이아웃 자유도 낮음, 모든 아이콘이 메모리에 상주 |
| **B) 캐릭터마다 오버레이 전체 복제** | 캐릭별 완전 커스텀 가능 | 체력바/물/목표/웨이브타이머 등 공통 UI 4벌 중복, 유지보수 지옥 |
| **C) 단일 오버레이 + 스킬아이콘 영역만 교체 (채택)** | 공통 UI 한 벌, 스킬아이콘만 캐릭별 자유 레이아웃, 확장 용이 | 약간의 초기 구조 작업 필요 |

**C 방식**은 `WBP_StageOverlay`를 하나만 유지하면서, 스킬 아이콘 영역만 캐릭터 클래스에 따라 다른 위젯(WBP_SkillIcon_GardenRobot, WBP_SkillIcon_VendingMachine 등)을 동적으로 생성하여 배치하는 구조이다.

---

## 현재 시스템 분석

### 이미 갖춰진 것들
1. **`DA_AbilityInfo`** - 어빌리티 태그별 아이콘(UTexture2D), 배경 머티리얼, 쿨다운 태그 등 메타데이터 보유
2. **`BroadcastAbilityInfo()`** - ASC에 부여된 어빌리티만 순회하며 `AbilityInfoDelegate` 브로드캐스트 (캐릭터별로 다른 StartupAbilities → 자동으로 다른 아이콘 전달)
3. **`EPlayerCharacterClass`** - GardenRobot, VendingMachineRobot (추후 4개까지 확장)
4. **`ADRPlayerState::SelectedPlayerClass`** - Replicated, `OnPlayerClassChanged` 델리게이트 보유
5. **`WBP_SkillIcon_GardenRobot`** - 이미 가든로봇 전용 스킬아이콘 위젯 존재
6. **`WBP_SkillSlot`** - 범용 스킬 슬롯 컴포넌트 존재

### 핵심 데이터 흐름 (현재)
```
DA_PlayerCharacterClassInfo → StartupAbilities (캐릭별 다른 어빌리티)
    ↓
ASC에 부여됨
    ↓
BroadcastAbilityInfo() → DA_AbilityInfo에서 해당 어빌리티의 Icon 조회
    ↓
AbilityInfoDelegate.Broadcast(FDRAbilityInfo)
    ↓
WBP_SkillIcon 위젯이 수신 → 아이콘 텍스처 표시
```

---

## 상세 구현 계획

### Phase 1: C++ 인프라 수정

#### 1-1. `UPlayerCharacterClassInfo`에 스킬아이콘 위젯 클래스 맵 추가

**파일**: `Source/DaeRune/Public/AbilitySystem/Data/CharacterClassInfo.h`

```cpp
// FCharacterClassDefaultInfo 구조체에 추가
USTRUCT(BlueprintType)
struct FCharacterClassDefaultInfo
{
    GENERATED_BODY()

    // ... 기존 필드 유지 ...

    // 해당 캐릭터 전용 스킬아이콘 위젯 클래스
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UDRUserWidget> SkillIconWidgetClass;
};
```

**목적**: 데이터 에셋(DA_PlayerCharacterClassInfo)에서 캐릭터별로 어떤 스킬아이콘 위젯을 사용할지 지정할 수 있게 함.

#### 1-2. `UOverlayWidgetController`에 캐릭터 클래스 정보 브로드캐스트 추가

**파일**: `Source/DaeRune/Public/UI/WidgetController/OverlayWidgetController.h`

```cpp
// 새 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillIconClassChangedSignature, TSubclassOf<UDRUserWidget>, SkillIconWidgetClass);

// 클래스 내부에 추가
UPROPERTY(BlueprintAssignable, Category = "GAS|SkillIcon")
FOnSkillIconClassChangedSignature OnSkillIconClassChanged;

// 스킬아이콘 위젯 클래스를 결정하고 브로드캐스트하는 함수
void BroadcastSkillIconWidgetClass();
```

**파일**: `Source/DaeRune/Private/UI/WidgetController/OverlayWidgetController.cpp`

```cpp
void UOverlayWidgetController::BroadcastSkillIconWidgetClass()
{
    ADRPlayerState* PS = GetDRPS();
    if (!PS) return;

    EPlayerCharacterClass CharClass = PS->GetSelectedPlayerClass();

    // DRAbilitySystemLibrary를 통해 PlayerCharacterClassInfo 가져오기
    UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(GetWorld());
    if (!ClassInfo) return;

    FCharacterClassDefaultInfo DefaultInfo = ClassInfo->GetClassDefaultInfo(CharClass);
    if (DefaultInfo.SkillIconWidgetClass)
    {
        OnSkillIconClassChanged.Broadcast(DefaultInfo.SkillIconWidgetClass);
    }
}
```

#### 1-3. `ADRHUD::InitOverlay()`에서 스킬아이콘 브로드캐스트 호출

**파일**: `Source/DaeRune/Private/UI/HUD/DRHUD.cpp`

`InitOverlay()` 함수의 기존 `BroadcastAbilityInfo()` 호출 바로 앞에 추가:

```cpp
// 캐릭터 클래스에 맞는 스킬아이콘 위젯 클래스 브로드캐스트
WidgetController->BroadcastSkillIconWidgetClass();
// 어빌리티 아이콘 갱신
WidgetController->BroadcastAbilityInfo();
```

`UpdateOverlayForSpectating()`에도 동일하게 추가하여 관전 시에도 해당 캐릭터의 스킬아이콘이 표시되도록 함.

---

### Phase 2: 블루프린트 작업 - WBP_StageOverlay 수정

#### 2-1. WBP_StageOverlay 내 스킬아이콘 영역 구조 변경

**현재 구조 (추정)**:
```
WBP_StageOverlay
├── HealthBar / WaterBar / ObjectiveUI (공통)
└── WBP_SkillIcon_GardenRobot (하드코딩)
```

**변경 후 구조**:
```
WBP_StageOverlay
├── HealthBar / WaterBar / ObjectiveUI (공통)
└── SkillIconContainer (NamedSlot 또는 Overlay/CanvasPanel)
    └── [동적 생성된 WBP_SkillIcon_XXX]
```

**구체적 작업**:
1. `WBP_StageOverlay`에서 기존 스킬아이콘 위젯(WBP_SkillIcon_GardenRobot) 직접 배치를 **제거**
2. 대신 빈 `Overlay` 또는 `SizeBox` 위젯을 "SkillIconContainer"라는 이름으로 배치 (변수로 노출)
3. 이 컨테이너의 위치/앵커는 현재 스킬아이콘이 있던 자리 그대로 유지

#### 2-2. WBP_StageOverlay 이벤트 그래프에서 동적 생성 로직

`WidgetControllerSet` 이벤트(또는 `Event Construct`) 시:

```
[WidgetController의 OnSkillIconClassChanged에 바인딩]
    ↓
OnSkillIconClassChanged 수신 시:
    1. SkillIconContainer 내부의 기존 자식 위젯 제거 (Clear Children)
    2. 전달받은 SkillIconWidgetClass로 CreateWidget
    3. 생성된 위젯을 SkillIconContainer에 Add Child
    4. 생성된 위젯에 WidgetController 레퍼런스 전달 (SetWidgetController)
    5. 스킬아이콘 위젯 내부에서 AbilityInfoDelegate 바인딩
```

**블루프린트 노드 순서**:
```
Bind Event to OnSkillIconClassChanged
    → [Event] OnSkillIconClassChanged (SkillIconWidgetClass)
        → SkillIconContainer → Clear Children
        → Create Widget (Class = SkillIconWidgetClass, Owning Player = Get Owning Player)
        → Cast to UDRUserWidget
        → Set Widget Controller (WidgetController 참조)
        → SkillIconContainer → Add Child (Created Widget)
        → (변수에 참조 저장 - 나중에 AbilityInfo 리브로드 시 필요)
```

---

### Phase 3: 캐릭터별 스킬아이콘 위젯 제작

#### 3-1. 기본 구조 (모든 캐릭터 공통 패턴)

각 `WBP_SkillIcon_[캐릭터명]`은 `UDRUserWidget`을 상속받고 다음 구조를 가짐:

```
WBP_SkillIcon_[캐릭터명] (extends UDRUserWidget)
├── 캐릭터 고유 배경/프레임 이미지
├── SkillSlot_LMB (WBP_SkillSlot 또는 커스텀)
├── SkillSlot_RMB
├── SkillSlot_Q
├── SkillSlot_E
└── (필요시 추가 슬롯, 패시브 표시 등)
```

#### 3-2. `WidgetControllerSet` 이벤트에서 AbilityInfoDelegate 바인딩

각 스킬아이콘 위젯 내부:
```
Event WidgetControllerSet
    → Bind Event to WidgetController.AbilityInfoDelegate
    → [Event] OnAbilityInfo (FDRAbilityInfo)
        → Switch on InputTag:
            InputTag.LMB → SkillSlot_LMB.SetIcon(Info.Icon)
            InputTag.RMB → SkillSlot_RMB.SetIcon(Info.Icon)
            InputTag.Q   → SkillSlot_Q.SetIcon(Info.Icon)
            InputTag.E   → SkillSlot_E.SetIcon(Info.Icon)
```

#### 3-3. 제작해야 할 위젯 목록

| 캐릭터 | 위젯명 | 상태 |
|---------|--------|------|
| GardenRobot | `WBP_SkillIcon_GardenRobot` | 이미 존재 (수정 필요) |
| VendingMachineRobot | `WBP_SkillIcon_VendingMachine` | 신규 제작 |
| 캐릭터3 (추후) | `WBP_SkillIcon_Character3` | 추후 제작 |
| 캐릭터4 (추후) | `WBP_SkillIcon_Character4` | 추후 제작 |

#### 3-4. WBP_SkillSlot 재사용

`WBP_SkillSlot`은 범용 슬롯으로서 모든 캐릭터 스킬아이콘 위젯 내부에서 재사용:
- 아이콘 텍스처 세팅 함수
- 쿨다운 오버레이 표시
- 키 바인딩 텍스트 표시

각 `WBP_SkillIcon_[캐릭터명]`은 WBP_SkillSlot들의 **배치, 크기, 배경 프레임**만 다르게 설정.

---

### Phase 4: DA_PlayerCharacterClassInfo 데이터 에셋 설정

에디터에서 `DA_PlayerCharacterClassInfo` 열기:

```
CharacterClassInformation:
├── GardenRobot:
│   ├── PrimaryAttributes: GE_GardenRobot_PrimaryAttributes
│   ├── StartupAbilities: [GA_ClawSwipe, GA_WaterPump, ...]
│   └── SkillIconWidgetClass: WBP_SkillIcon_GardenRobot  ← 새로 추가
└── VendingMachineRobot:
    ├── PrimaryAttributes: GE_VendingMachine_PrimaryAttributes
    ├── StartupAbilities: [GA_VendingSkill1, ...]
    └── SkillIconWidgetClass: WBP_SkillIcon_VendingMachine  ← 새로 추가
```

---

### Phase 5: DA_AbilityInfo 에 캐릭터별 어빌리티 아이콘 등록

각 캐릭터의 고유 어빌리티에 대해 `DA_AbilityInfo`에 엔트리 추가:

```
AbilityInformation:
├── [0] AbilityTag: Abilities.GardenRobot.Skill1
│       Icon: T_Icon_ClawSwipe
│       CooldownTag: Cooldown.GardenRobot.Skill1
│       InputTag: (런타임 할당)
├── [1] AbilityTag: Abilities.GardenRobot.Skill2
│       Icon: T_Icon_WaterPump
│       ...
├── [2] AbilityTag: Abilities.VendingMachine.Skill1
│       Icon: T_Icon_VendingSkill1
│       ...
└── ... (각 캐릭터별 모든 어빌리티)
```

**아이콘 에셋 경로 규칙**:
```
Content/DaeRuneAssets/UI/SkillIcon/[캐릭터명]/Icon_[스킬명].png
```

---

### Phase 6: 관전 시스템 대응

`UpdateOverlayForSpectating()`이 호출될 때:
1. 새 대상의 `PlayerState`에서 `SelectedPlayerClass` 읽음
2. `BroadcastSkillIconWidgetClass()` 호출 → 스킬아이콘 위젯 교체
3. `BroadcastAbilityInfo()` 호출 → 새 아이콘 데이터 전달

이미 `UpdateOverlayForSpectating()`이 WidgetController를 재생성하므로, 위 흐름이 자연스럽게 동작함.

---

## 파일 수정 요약

### C++ 수정 (4개 파일)

| 파일 | 수정 내용 |
|------|-----------|
| `CharacterClassInfo.h` | `FCharacterClassDefaultInfo`에 `SkillIconWidgetClass` 필드 추가 |
| `OverlayWidgetController.h` | `FOnSkillIconClassChangedSignature` 델리게이트, `BroadcastSkillIconWidgetClass()` 함수 선언 |
| `OverlayWidgetController.cpp` | `BroadcastSkillIconWidgetClass()` 구현 |
| `DRHUD.cpp` | `InitOverlay()`와 `UpdateOverlayForSpectating()`에서 `BroadcastSkillIconWidgetClass()` 호출 추가 |

### 블루프린트 수정/생성 (4~5개)

| 에셋 | 작업 |
|------|------|
| `WBP_StageOverlay` | 하드코딩된 스킬아이콘 제거, SkillIconContainer 추가, 동적 생성 로직 추가 |
| `WBP_SkillIcon_GardenRobot` | 기존 위젯을 새 구조에 맞게 수정 (WidgetControllerSet에서 AbilityInfoDelegate 바인딩) |
| `WBP_SkillIcon_VendingMachine` | 신규 제작 (VendingMachine 전용 레이아웃) |
| `DA_PlayerCharacterClassInfo` | 각 캐릭터에 SkillIconWidgetClass 지정 |
| `DA_AbilityInfo` | VendingMachine 어빌리티 아이콘 엔트리 추가 |

### 아이콘 에셋 추가

| 경로 | 설명 |
|------|------|
| `Content/DaeRuneAssets/UI/SkillIcon/GardenRobot/` | 가든로봇 전용 아이콘 (기존 것 이동) |
| `Content/DaeRuneAssets/UI/SkillIcon/VendingMachine/` | 자판기로봇 전용 아이콘 (신규) |

---

## 확장성 분석 (캐릭터 4개까지)

### 새 캐릭터 추가 시 해야 할 일 (체크리스트)

1. `EPlayerCharacterClass` enum에 새 값 추가
2. `DA_PlayerCharacterClassInfo`에 새 캐릭터 엔트리 추가
   - PrimaryAttributes, StartupAbilities, DeathAbilities 설정
   - **SkillIconWidgetClass** 지정
3. `WBP_SkillIcon_[NewCharacter]` 위젯 제작
   - WBP_SkillSlot 배치 (캐릭터 고유 레이아웃)
   - WidgetControllerSet에서 AbilityInfoDelegate 바인딩
4. `DA_AbilityInfo`에 새 캐릭터 어빌리티 아이콘 등록
5. 아이콘 텍스처 에셋 임포트

**C++ 코드 수정 불필요** - 완전히 데이터 드리븐으로 확장됨.

---

## 쿨다운 표시 연동

스킬 아이콘에 쿨다운 표시가 필요한 경우 (추후 또는 현재):

`AbilityInfoDelegate`가 브로드캐스트하는 `FDRAbilityInfo`에 이미 `CooldownTag`가 포함되어 있으므로, 각 SkillSlot에서:

```
1. CooldownTag 저장
2. ASC의 OnGameplayEffectAppliedDelegateToSelf에 바인딩
3. 적용된 GE에 해당 CooldownTag가 있으면 Duration 읽어서 쿨다운 UI 시작
4. Tick 또는 Timer로 남은 시간 업데이트
```

이 로직은 `WBP_SkillSlot` 내부에 구현하면 모든 캐릭터에 자동 적용됨.

---

## 구현 순서 (권장)

```
1. CharacterClassInfo.h 수정 (SkillIconWidgetClass 필드 추가)
2. OverlayWidgetController에 델리게이트 & 함수 추가
3. DRHUD.cpp에서 호출 추가
4. 컴파일 확인
5. WBP_StageOverlay 블루프린트 수정 (동적 생성 로직)
6. WBP_SkillIcon_GardenRobot 수정 (새 구조 적용)
7. WBP_SkillIcon_VendingMachine 제작
8. DA_PlayerCharacterClassInfo에서 위젯 클래스 지정
9. 테스트: 캐릭터 선택 → 올바른 스킬아이콘 표시 확인
10. 관전 모드 테스트: 다른 캐릭터 관전 시 아이콘 교체 확인
```

---

## 대안 고려사항

### Widget Switcher vs 동적 생성

- **Widget Switcher**: 모든 캐릭터의 스킬아이콘 위젯을 미리 배치하고 인덱스로 전환
  - 장점: 즉시 전환, 간단한 BP 로직
  - 단점: 모든 위젯이 메모리 상주, 캐릭터 추가 시 오버레이 BP 수정 필요

- **동적 생성 (채택)**: `CreateWidget`으로 필요한 것만 생성
  - 장점: 메모리 효율적, 오버레이 BP 수정 불필요 (데이터 드리븐)
  - 단점: 위젯 생성에 미세한 딜레이 (실질적으로 무시 가능)

**결론**: 4캐릭터 이상 확장을 고려하면 동적 생성이 압도적으로 유리함. 오버레이 자체를 건드리지 않고 데이터 에셋과 새 위젯 BP만 추가하면 되기 때문.
