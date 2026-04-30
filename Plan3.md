# Plan3: 설정 UI — C++ 코드 ↔ WBP Graph 연결 상세 설계

> **구현 순서**: 작은 위젯부터 큰 위젯 순서 (Bottom-Up)로 배치.
> 작은 요소의 함수/디스패처를 먼저 만든 뒤, 큰 요소에서 바인딩하도록 한다.

---

## 0. 전체 아키텍처 요약

```
┌────────────────────────────────────────────────────────────┐
│                   BP_DRPlayerController                     │
│  OpenSettingsMenu_Implementation → WBP_SettingsScreen 생성  │
│  CloseSettingsMenu_Implementation → WBP_SettingsScreen 제거 │
└──────────────┬─────────────────────────────────────────────┘
               │ CreateWidget / RemoveFromParent
               ▼
┌────────────────────────────────────────────────────────────┐
│                  WBP_SettingsScreen                         │
│  ┌─────────────┐  ┌──────────────────────────────────┐     │
│  │ TabButtons   │  │  WidgetSwitcher_SettingsPages     │     │
│  │  (4 tabs)    │  │  ┌─ Page_Gameplay ──────────┐    │     │
│  │              │  │  │  Row_Slider_CameraSens   │    │     │
│  │              │  │  │  Row_Dropdown_Language    │    │     │
│  │              │  │  └──────────────────────────┘    │     │
│  │              │  │  ┌─ Page_Graphics ──────────┐    │     │
│  │              │  │  │  Row_Dropdown_DisplayMode│    │     │
│  │              │  │  │  Row_Dropdown_Resolution │    │     │
│  │              │  │  │  Row_Toggle_VSync        │    │     │
│  │              │  │  │  Row_Dropdown_FPS        │    │     │
│  │              │  │  │  Row_Dropdown_Scalability│    │     │
│  │              │  │  │  Row_Slider_Gamma        │    │     │
│  │              │  │  └──────────────────────────┘    │     │
│  │              │  │  ┌─ Page_Audio ─────────────┐    │     │
│  │              │  │  │  Row_Slider_MasterVol    │    │     │
│  │              │  │  │  Row_Slider_MusicVol     │    │     │
│  │              │  │  │  Row_Slider_SFXVol       │    │     │
│  │              │  │  └──────────────────────────┘    │     │
│  │              │  │  ┌─ Page_Controls ──────────┐    │     │
│  │              │  │  │  (비어있음)              │    │     │
│  │              │  │  └──────────────────────────┘    │     │
│  └─────────────┘  └──────────────────────────────────┘     │
│  ┌─────────────┐  ┌─────────────┐                          │
│  │ KeyHintBar   │  │ ScrollBar    │                          │
│  │ R:RESET      │  │ (장식용)     │                          │
│  │ ESC:BACK     │  └─────────────┘                          │
│  │ A:APPLY      │                                           │
│  └─────────────┘                                            │
└──────────────┬─────────────────────────────────────────────┘
               │ GetGameInstance → GetSubsystem
               ▼
┌────────────────────────────────────────────────────────────┐
│               UDRSettingsManager (C++ Subsystem)            │
│                                                             │
│  InitSettings()  → BuildDefinitions + Load                  │
│  GetDefinitionsByTab(Tab) → 페이지별 설정 정의 목록           │
│  GetPendingValue(SettingId) → 현재 대기 값                   │
│  SetPendingValue(SettingId, Value) → 값 변경                 │
│  ApplyPendingSettings() → 모든 대기 변경 적용                │
│  ResetTabToDefault(Tab) → 탭별 기본값 리셋                   │
│  DiscardPendingChanges() → 변경 취소                         │
│                                                             │
│  Delegates:                                                 │
│    OnPendingValueChanged(SettingId, NewValue)                │
│    OnHasPendingChangesChanged(bHasPending)                   │
│    OnSettingsReset(Tab)                                      │
│    OnSettingsLoaded()                                        │
│    OnSettingsApplied()                                       │
└──────────────┬─────────────────────────────────────────────┘
               │ ApplySingleSetting → GameUserSettings 반영
               ▼
┌────────────────────────────────────────────────────────────┐
│              UDRGameUserSettings (C++ Config)                │
│  MasterVolume, BGMVolume, SFXVolume                         │
│  MouseSensitivity, Gamma                                    │
│  + UGameUserSettings 내장: Resolution, WindowMode, VSync 등  │
└────────────────────────────────────────────────────────────┘
```

---

## 1. C++ 코드 수정사항 (모두 적용 완료)

| 번호 | 파일 | 변경 내용 | 상태 |
|---:|---|---|---|
| 1 | DRSettingsManager.cpp | BuildDefinitions: FPS 옵션 30,60,120,UNLIMITED (4개), DefaultValue SelectedIndex=2 | Done |
| 2 | DRSettingsManager.cpp | BuildDefinitions: Language — Japanese 제거, Korean→Chinese→English, 기본값 Korean | Done |
| 3 | DRSettingsManager.cpp | BuildDefinitions: Scalability 추가 (LOW~CINEMATIC, 기본 EPIC SelectedIndex=3) | Done |
| 4 | DRSettingsManager.cpp | BuildDefinitions/Load/Save/Apply: VoiceVolume 전부 삭제 | Done |
| 5 | DRSettingsManager.cpp | LoadFromGameUserSettings: FPSLimit Options 배열 순회로 정확한 SelectedIndex 계산 | Done |
| 6 | DRSettingsManager.cpp | LoadFromGameUserSettings: Scalability 로드 (GetOverallScalabilityLevel, Mixed=-1→Epic) | Done |
| 7 | DRSettingsManager.cpp | SaveToGameUserSettings: Scalability 저장 (SetOverallScalabilityLevel) | Done |
| 8 | DRSettingsManager.cpp | ApplySingleSetting: Scalability 적용 (SetGraphicsQuality + ApplyGraphicsQualitySettings) | Done |
| 9 | DRSettingsManager.cpp/h | SetVoiceVolume 함수/선언 삭제, VoiceSoundClass 멤버 삭제 | Done |
| 10 | DRGameUserSettings.h/cpp | VoiceVolume 프로퍼티/클램프/기본값 삭제 | Done |
| 11 | DRVOIPTalker.cpp | VoiceVolume 참조 → 고정값 1.0f 대체 | Done |

---

## 2. Event Dispatcher / 이벤트 이름 규칙

혼동을 막기 위해, 이 문서에서 사용하는 이벤트 이름을 정리한다.

### 2.1 UMG 내장 이벤트 (직접 만들지 않아도 존재)

| 위젯 | 내장 이벤트 | 시그니처 | 설명 |
|---|---|---|---|
| USlider (`Slider_Input`) | `OnValueChanged` | `(float Value)` | 0~1 정규화된 값. Slider Details 패널에서 + 버튼으로 바인딩 |
| UButton (`Button_Hit`) | `OnClicked` | `()` | 버튼 클릭. Details 패널에서 바인딩 |
| UButton (`Button_Hit`) | `OnHovered` | `()` | 마우스 올림 |
| UButton (`Button_Hit`) | `OnUnhovered` | `()` | 마우스 벗어남 |

### 2.2 직접 만드는 Custom Event Dispatcher (My Blueprint 패널에서 생성)

| WBP | 디스패처 이름 | 시그니처 | 바인딩 주체 |
|---|---|---|---|
| WBP_SciFiSlider | **OnSliderValueChanged** | `(float AbsoluteValue)` | WBP_SettingRow_Slider |
| WBP_SciFiToggle | **OnToggleChanged** | `(bool bNewState)` | WBP_SettingRow_Toggle |
| WBP_SciFiDropdownOption | **OnOptionClicked** | `(int32 OptionIndex)` | WBP_SciFiDropdownOptionList |
| WBP_SciFiDropdown | **OnSelectionChanged** | `(int32 NewIndex)` | WBP_SettingRow_Dropdown |

> **핵심 구분**: `Slider_Input`의 내장 `OnValueChanged`(0~1)와 WBP_SciFiSlider의 커스텀 `OnSliderValueChanged`(절대값)는 **완전히 다른 이벤트**이다.
> - `OnValueChanged` = USlider가 자동으로 가진 것. Slider Details에서 바인딩.
> - `OnSliderValueChanged` = My Blueprint 패널에서 직접 만드는 것. 부모 Row가 바인딩.

### 2.3 UDRSettingsManager 참조 방법

`UDRSettingsManager`는 `UGameInstanceSubsystem`이므로, Blueprint에서 변수 타입으로 직접 검색이 어려울 수 있다.

**권장 패턴**: 필요할 때마다 인라인으로 가져오기

```
Get Game Instance → Get Subsystem (UDRSettingsManager) → 결과 사용
```

또는 Blueprint 함수 `GetSettingsManager`를 만들어 재사용:

```
Function: GetSettingsManager
Return: UDRSettingsManager*
│
├─ Get Game Instance
│  → Get Subsystem (UDRSettingsManager)
│  → Return
```

Page/Row의 Init 함수에서는 **파라미터로 전달받는 것**이 가장 편리하다.
WBP_SettingsScreen의 Construct에서 한 번 가져온 뒤, 각 Page → Row로 전달한다.

---

## 3. WBP_KeyHint — Graph 로직

Graph 로직 불필요. Designer에서 이미지와 텍스트만 설정하면 끝.
동적으로 키/텍스트를 바꿀 필요가 있으면 `Image_KeyIcon`과 `Text_Label`을 Is Variable로 노출.

---

## 4. WBP_SciFiSlider — Graph 로직

### 4.1 구조 복습

```
WBP_SciFiSlider
└─ SizeBox_Root (380 x 48)
   └─ CanvasPanel_Slider
      ├─ Overlay_Track
      │  ├─ Image_SliderFrame
      │  ├─ SizeBox_FillClip (Clipping: Clip to Bounds)
      │  │  └─ Image_SliderFill
      │  └─ Slider_Input (Opacity: 0)
      └─ Overlay_ValueBox
         ├─ Image_ValueBox
         └─ Text_Value
```

### 4.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Slider_Input` | USlider* | Designer 바인딩. 투명 입력 슬라이더 |
| `SizeBox_FillClip` | USizeBox* | Designer 바인딩. Fill 바 클리핑 영역 |
| `Text_Value` | UTextBlock* | Designer 바인딩. "80%" 같은 표시 텍스트 |
| `TrackWidth` | float | 280.0 (Overlay_Track의 가로 크기) |
| `MinValue` | float | 설정 최소값 (예: 0) |
| `MaxValue` | float | 설정 최대값 (예: 100) |
| `StepValue` | float | 스냅 단위 (예: 1) |
| `SuffixText` | FText | "%" 같은 접미사 |
| `bSuppressCallback` | bool | 프로그래밍 방식 SetValue 시 무한 루프 방지 플래그 |

### 4.3 Custom Event Dispatcher 생성

**My Blueprint 패널 → Event Dispatchers → + 버튼** → 이름: `OnSliderValueChanged`

시그니처 (Inputs에서 추가):

```
OnSliderValueChanged (float NewAbsoluteValue)
```

이것은 부모 WBP_SettingRow_Slider가 바인딩할 디스패처이다.
`Slider_Input`의 내장 `OnValueChanged`(0~1)와 **완전히 별개**이다.

### 4.4 InitSlider 함수

```
Function: InitSlider
Input: float InMin, float InMax, float InStep, FText InSuffix
│
├─ MinValue = InMin
├─ MaxValue = InMax
├─ StepValue = InStep
├─ SuffixText = InSuffix
│
├─ Slider_Input → Set Min Value: 0.0
├─ Slider_Input → Set Max Value: 1.0
│   (내부적으로 0~1 정규화 사용, Min~Max 변환은 이 위젯이 처리)
│
├─ ★ Slider_Input의 내장 OnValueChanged 바인딩:
│   Slider_Input → Details 패널의 OnValueChanged 옆 + 버튼
│   → HandleNativeSliderChanged 함수에 바인딩
│
│   또는 Graph에서:
│   Slider_Input → Bind Event to OnValueChanged
│   → Create Event → HandleNativeSliderChanged
│
└─ (끝)
```

### 4.5 SetValue 함수 (외부에서 호출 — Row가 호출)

```
Function: SetValue
Input: float AbsoluteValue
│
├─ bSuppressCallback = true
│
├─ NormalizedValue = (AbsoluteValue - MinValue) / (MaxValue - MinValue)
│
├─ Slider_Input → Set Value (NormalizedValue)
│   ※ 이때 USlider 내장 OnValueChanged가 발생하지만
│     bSuppressCallback=true이므로 HandleNativeSliderChanged에서 무시됨
│
├─ Fill 바 업데이트:
│   FillWidth = TrackWidth * NormalizedValue
│   SizeBox_FillClip → Set Width Override (FillWidth)
│
├─ 텍스트 업데이트:
│   DisplayValue = Round(AbsoluteValue)
│   Text_Value → SetText (DisplayValue + SuffixText)
│   예: "80%"
│
├─ bSuppressCallback = false
│
└─ (끝)
```

### 4.6 HandleNativeSliderChanged 콜백

이 함수는 `Slider_Input`의 **내장** `OnValueChanged`에 바인딩된다.
사용자가 슬라이더를 드래그할 때 0~1 값이 들어온다.

```
Function: HandleNativeSliderChanged
Input: float NormalizedValue (0~1, USlider 내장 OnValueChanged에서 전달)
│
├─ Branch: bSuppressCallback == true?
│   └─ True → return (프로그래밍 방식 SetValue 호출 중이므로 무시)
│
├─ AbsoluteValue = MinValue + (MaxValue - MinValue) * NormalizedValue
│
├─ AbsoluteValue = ClampAndSnapFloat(AbsoluteValue, MinValue, MaxValue, StepValue)
│   ※ UDRSettingsFunctionLibrary::ClampAndSnapFloat BP 호출
│
├─ Fill 바 업데이트:
│   FillWidth = TrackWidth * ((AbsoluteValue - MinValue) / (MaxValue - MinValue))
│   SizeBox_FillClip → Set Width Override (FillWidth)
│
├─ 텍스트 업데이트:
│   Text_Value → SetText (Round(AbsoluteValue) + SuffixText)
│
├─ ★ OnSliderValueChanged 디스패처 → Broadcast (AbsoluteValue)
│   (커스텀 디스패처를 통해 부모 Row에 절대값 전달)
│
└─ (끝)
```

**흐름 정리**:
```
사용자 드래그 → Slider_Input 내장 OnValueChanged(0.8)
  → HandleNativeSliderChanged(0.8)
    → AbsoluteValue 계산 (예: 80)
    → Fill/Text 업데이트
    → 커스텀 OnSliderValueChanged 디스패처 Broadcast(80.0)
      → 부모 Row::HandleSliderValueChanged(80.0) 호출됨
```

---

## 5. WBP_SciFiToggle — Graph 로직

### 5.1 구조 복습

```
WBP_SciFiToggle
└─ SizeBox_Root (280 x 48)
   └─ CanvasPanel_Toggle
      ├─ Image_SelectedFill
      ├─ Image_Frame
      ├─ Text_Off
      ├─ Text_On
      └─ Button_Hit
```

### 5.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Image_SelectedFill` | UImage* | Designer 바인딩 |
| `Text_Off` | UTextBlock* | Designer 바인딩 |
| `Text_On` | UTextBlock* | Designer 바인딩 |
| `Button_Hit` | UButton* | Designer 바인딩 |
| `bIsOn` | bool | 현재 토글 상태 |
| `bSuppressCallback` | bool | 프로그래밍 방식 SetToggleState 시 무한 루프 방지 |

### 5.3 Custom Event Dispatcher 생성

**My Blueprint 패널 → Event Dispatchers → + 버튼** → 이름: `OnToggleChanged`

```
OnToggleChanged (bool bNewState)
```

### 5.4 Event Construct

```
Event Construct:
│
├─ Button_Hit → OnClicked 바인딩 → HandleClicked
│
└─ (끝)
```

### 5.5 SetToggleState 함수 (외부에서 호출 — Row가 호출)

```
Function: SetToggleState
Input: bool bNewOn
│
├─ bSuppressCallback = true
├─ bIsOn = bNewOn
│
├─ UpdateVisuals()
│
├─ bSuppressCallback = false
│
└─ (끝)
```

### 5.6 UpdateVisuals 함수

```
Function: UpdateVisuals
│
├─ Branch: bIsOn?
│   ├─ True (ON 상태):
│   │   Image_SelectedFill → Slot as Canvas Panel Slot → Set Position ((142, 4))
│   │   Text_Off → Set Color and Opacity: #DDF7FFFF (밝은 — Fill이 없는 쪽)
│   │   Text_On → Set Color and Opacity: #6677B8FF (어두운 — Fill 위에 있는 쪽)
│   │
│   └─ False (OFF 상태):
│       Image_SelectedFill → Slot as Canvas Panel Slot → Set Position ((4, 4))
│       Text_Off → Set Color and Opacity: #6677B8FF (어두운 — Fill 위에 있는 쪽)
│       Text_On → Set Color and Opacity: #DDF7FFFF (밝은 — Fill이 없는 쪽)
│
└─ (끝)
```

**Canvas Panel Slot Position 변경 (Blueprint 노드)**:
```
Image_SelectedFill → Slot as Canvas Panel Slot → Set Position (FVector2D)
```

### 5.7 HandleClicked

```
Function: HandleClicked
│
├─ bIsOn = NOT bIsOn (토글)
│
├─ UpdateVisuals()
│
├─ Branch: bSuppressCallback == false?
│   └─ True → OnToggleChanged 디스패처 → Broadcast (bIsOn)
│
└─ (끝)
```

---

## 6. WBP_SciFiDropdownOption — Graph 로직

### 6.1 구조 복습

```
WBP_SciFiDropdownOption
└─ SizeBox_Root (280 x 44)
   └─ Overlay_Root
      ├─ Image_BG_Normal
      ├─ Image_BG_Hover (Hidden)
      ├─ Image_BG_Selected (Hidden)
      ├─ Text_Option
      └─ Button_Hit
```

### 6.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Text_Option` | UTextBlock* | Designer 바인딩 |
| `Image_BG_Normal` | UImage* | Designer 바인딩 |
| `Image_BG_Hover` | UImage* | Designer 바인딩 |
| `Image_BG_Selected` | UImage* | Designer 바인딩 |
| `Button_Hit` | UButton* | Designer 바인딩 |
| `OptionIndex` | int32 | 이 옵션의 배열 인덱스 |
| `bIsSelected` | bool | 현재 선택된 옵션인지 |

### 6.3 Custom Event Dispatcher 생성

**My Blueprint 패널 → Event Dispatchers → + 버튼** → 이름: `OnOptionClicked`

```
OnOptionClicked (int32 OptionIndex)
```

### 6.4 InitOption 함수

```
Function: InitOption
Input: FText InText, int32 InIndex, bool bInSelected
│
├─ OptionIndex = InIndex
├─ bIsSelected = bInSelected
├─ Text_Option → SetText (InText)
│
├─ Branch: bInSelected?
│   ├─ True:
│   │   Image_BG_Normal → Set Visibility: Hidden
│   │   Image_BG_Selected → Set Visibility: Not Hit-Testable (Self & All Children)
│   └─ False:
│       (기본 Normal 상태 유지)
│
├─ Button_Hit → OnClicked 바인딩 → HandleClicked
├─ Button_Hit → OnHovered 바인딩 → HandleHovered
├─ Button_Hit → OnUnhovered 바인딩 → HandleUnhovered
│
└─ (끝)
```

### 6.5 HandleClicked

```
Function: HandleClicked
│
├─ OnOptionClicked 디스패처 → Broadcast (OptionIndex)
│
└─ (끝)
```

### 6.6 HandleHovered / HandleUnhovered

```
Function: HandleHovered
│
├─ Branch: bIsSelected == false?
│   └─ True:
│       Image_BG_Normal → Set Visibility: Hidden
│       Image_BG_Hover → Set Visibility: Not Hit-Testable (Self & All Children)
│
└─ (끝)
```

```
Function: HandleUnhovered
│
├─ Branch: bIsSelected == false?
│   └─ True:
│       Image_BG_Hover → Set Visibility: Hidden
│       Image_BG_Normal → Set Visibility: Not Hit-Testable (Self & All Children)
│
└─ (끝)
```

---

## 7. WBP_SciFiDropdownOptionList — Graph 로직

### 7.1 구조 복습

```
WBP_SciFiDropdownOptionList
└─ SizeBox_Root (280 x Auto)
   └─ VerticalBox_Options
      ├─ WBP_SciFiDropdownOption (동적 생성)
      ├─ WBP_SciFiDropdownOption
      └─ ...
```

### 7.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `VerticalBox_Options` | UVerticalBox* | Designer 바인딩 |
| `OwnerDropdown` | WBP_SciFiDropdown* | 이 리스트를 소유한 드롭다운 참조 |

### 7.3 InitOptionList 함수

```
Function: InitOptionList
Input: TArray<FText> InOptions, int32 CurrentSelectedIndex, WBP_SciFiDropdown* InOwner
│
├─ OwnerDropdown = InOwner
│
├─ VerticalBox_Options → Clear Children
│
├─ For Each (InOptions, Index i):
│   │
│   ├─ Create Widget (WBP_SciFiDropdownOption) → OptionWidget
│   │
│   ├─ OptionWidget → InitOption (InOptions[i], i, i == CurrentSelectedIndex)
│   │
│   ├─ ★ OptionWidget.OnOptionClicked 바인딩 → HandleOptionClicked
│   │   (WBP_SciFiDropdownOption의 커스텀 디스패처에 바인딩)
│   │
│   ├─ VerticalBox_Options → Add Child (OptionWidget)
│   │   Slot: Size = Auto, HAlign = Fill, VAlign = Fill
│
└─ (끝)
```

### 7.4 HandleOptionClicked

```
Function: HandleOptionClicked
Input: int32 OptionIndex
│
├─ OwnerDropdown → OnOptionSelected (OptionIndex)
│   (WBP_SciFiDropdown의 함수를 직접 호출)
│
└─ (끝)
```

---

## 8. WBP_SciFiDropdown — Graph 로직

### 8.1 구조 복습

```
WBP_SciFiDropdown
└─ SizeBox_Root (280 x 48)
   └─ Overlay_Root
      ├─ Image_BG_Normal
      ├─ Image_BG_Hover (Hidden)
      ├─ Image_BG_Selected (Hidden)
      ├─ Image_Frame
      ├─ Text_SelectedValue
      ├─ SizeBox_Arrow
      │  └─ Image_Arrow
      └─ Button_Hit
```

### 8.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Image_BG_Normal` | UImage* | Designer 바인딩 |
| `Image_BG_Hover` | UImage* | Designer 바인딩 |
| `Image_BG_Selected` | UImage* | Designer 바인딩 |
| `Text_SelectedValue` | UTextBlock* | Designer 바인딩 |
| `Button_Hit` | UButton* | Designer 바인딩 |
| `Options` | TArray\<FText\> | 옵션 텍스트 배열 |
| `SelectedIndex` | int32 | 현재 선택된 인덱스 |
| `bIsOpen` | bool | 드롭다운 열림 상태 |
| `ParentScreen` | WBP_SettingsScreen* | 팝업 레이어를 가진 부모 화면 |
| `OptionListWidget` | WBP_SciFiDropdownOptionList* | 현재 열린 옵션 리스트 인스턴스 |

### 8.3 Custom Event Dispatcher 생성

**My Blueprint 패널 → Event Dispatchers → + 버튼** → 이름: `OnSelectionChanged`

```
OnSelectionChanged (int32 NewIndex)
```

### 8.4 InitDropdown 함수

```
Function: InitDropdown
Input: TArray<FText> InOptions, WBP_SettingsScreen* InScreen
│
├─ Options = InOptions
├─ ParentScreen = InScreen
├─ bIsOpen = false
├─ SelectedIndex = 0
│
├─ Button_Hit → OnClicked 바인딩 → HandleClicked
├─ Button_Hit → OnHovered 바인딩 → HandleHovered
├─ Button_Hit → OnUnhovered 바인딩 → HandleUnhovered
│
└─ (끝)
```

### 8.5 SetSelectedIndex 함수 (외부에서 호출)

```
Function: SetSelectedIndex
Input: int32 InIndex
│
├─ SelectedIndex = InIndex
│
├─ Branch: InIndex가 Options 배열 범위 내?
│   ├─ True → Text_SelectedValue → SetText (Options[InIndex])
│   └─ False → (skip)
│
└─ (끝)
```

### 8.6 HandleClicked

```
Function: HandleClicked
│
├─ Branch: bIsOpen?
│   ├─ True → CloseDropdown()
│   └─ False → OpenDropdown()
│
└─ (끝)
```

### 8.7 OpenDropdown

```
Function: OpenDropdown
│
├─ bIsOpen = true
│
├─ 비주얼 업데이트:
│   Image_BG_Normal → Set Visibility: Hidden
│   Image_BG_Hover → Set Visibility: Hidden
│   Image_BG_Selected → Set Visibility: Not Hit-Testable (Self & All Children)
│
├─ OptionListWidget 생성:
│   Create Widget (WBP_SciFiDropdownOptionList) → OptionListWidget
│   OptionListWidget → InitOptionList (Options, SelectedIndex, self)
│
├─ 위치 계산 (LocalToAbsolute → AbsoluteToLocal 변환):
│   (1) Button_Hit → Get Cached Geometry → MyGeo
│   (2) AbsolutePos = MyGeo → Local To Absolute (LocalCoord: (0, 48))
│       // (0, 48) = 드롭다운 박스 바로 아래 위치
│   (3) CanvasPanel_DropdownPopupLayer → Get Cached Geometry → PopupGeo
│       // ParentScreen에서 CanvasPanel_DropdownPopupLayer 참조를 가져와야 함
│   (4) LocalPos = PopupGeo → Absolute To Local (AbsolutePos)
│
├─ ParentScreen → ShowDropdownPopup (OptionListWidget, LocalPos)
│
└─ (끝)
```

> **참고**: `CanvasPanel_DropdownPopupLayer`의 참조는 `ParentScreen`의 public 변수나 함수를 통해 접근한다.
> 예: `ParentScreen → GetDropdownPopupLayer()` 또는 `ParentScreen.CanvasPanel_DropdownPopupLayer` 직접 접근.

### 8.8 CloseDropdown

```
Function: CloseDropdown
│
├─ bIsOpen = false
│
├─ 비주얼 업데이트:
│   Image_BG_Selected → Set Visibility: Hidden
│   Image_BG_Normal → Set Visibility: Not Hit-Testable (Self & All Children)
│
├─ ParentScreen → HideDropdownPopup()
│
├─ OptionListWidget = null
│
└─ (끝)
```

### 8.9 HandleHovered / HandleUnhovered

```
Function: HandleHovered
│
├─ Branch: bIsOpen == false?
│   └─ True:
│       Image_BG_Normal → Set Visibility: Hidden
│       Image_BG_Hover → Set Visibility: Not Hit-Testable (Self & All Children)
│
└─ (끝)
```

```
Function: HandleUnhovered
│
├─ Branch: bIsOpen == false?
│   └─ True:
│       Image_BG_Hover → Set Visibility: Hidden
│       Image_BG_Normal → Set Visibility: Not Hit-Testable (Self & All Children)
│
└─ (끝)
```

### 8.10 OnOptionSelected 콜백 (OptionList에서 호출)

```
Function: OnOptionSelected
Input: int32 NewIndex
│
├─ SetSelectedIndex (NewIndex)
├─ CloseDropdown()
├─ ★ OnSelectionChanged 디스패처 → Broadcast (NewIndex)
│   (커스텀 디스패처를 통해 부모 Row에 인덱스 전달)
│
└─ (끝)
```

---

## 9. WBP_SettingRow_Slider — Graph 로직

### 9.1 구조 복습

```
WBP_SettingRow_Slider
└─ SizeBox_Root (760 x 92)
   └─ CanvasPanel_Row
      ├─ Text_Label
      ├─ Image_SeparatorLine
      └─ WBP_SciFiSlider
```

### 9.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Text_Label` | UTextBlock* | Designer 바인딩 |
| `SciFiSlider` | WBP_SciFiSlider* | Designer 바인딩 |
| `SettingId` | FName | 이 Row가 담당하는 설정 ID (예: "Audio.MasterVolume") |
| `Definition` | FDRSettingDefinition | 설정 정의 사본 |

### 9.3 InitRow 함수

```
Function: InitRow
Input: FDRSettingDefinition InDefinition, UDRSettingsManager* InManager
│
├─ Definition = InDefinition
├─ SettingId = InDefinition.SettingId
│
├─ Text_Label → SetText (InDefinition.LabelText)
│
├─ SciFiSlider → InitSlider (
│     MinValue = InDefinition.MinValue,       (예: 0)
│     MaxValue = InDefinition.MaxValue,       (예: 100)
│     StepValue = InDefinition.StepValue,     (예: 1)
│     SuffixText = InDefinition.SuffixText    (예: "%")
│   )
│
├─ 현재값 로드:
│   InManager → GetPendingValue (SettingId) → CurrentValue
│   SciFiSlider → SetValue (CurrentValue.FloatValue)
│
├─ ★ SciFiSlider.OnSliderValueChanged 바인딩 → HandleSliderValueChanged
│   (WBP_SciFiSlider의 커스텀 디스패처에 바인딩)
│
│   바인딩 방법:
│   SciFiSlider → Bind Event to OnSliderValueChanged
│   → Create Event → HandleSliderValueChanged
│
└─ (끝)
```

### 9.4 HandleSliderValueChanged 콜백

이 함수는 WBP_SciFiSlider의 **커스텀** `OnSliderValueChanged` 디스패처에 바인딩된다.
절대값(예: 80.0)이 전달된다.

```
Function: HandleSliderValueChanged
Input: float NewAbsoluteValue (OnSliderValueChanged에서 전달된 절대값)
│
├─ FDRSettingsValue SettingsValue = MakeFloatValue (NewAbsoluteValue)
│
├─ Get Game Instance → Get Subsystem (UDRSettingsManager) → SettingsManager
│   SettingsManager → SetPendingValue (SettingId, SettingsValue)
│   (Instant 모드 설정은 C++ 내부에서 즉시 ApplySingleSetting + CommitSingleSetting 호출됨)
│
└─ (끝)
```

### 9.5 UpdateDisplayValue 함수 (Page에서 호출)

```
Function: UpdateDisplayValue
Input: FDRSettingsValue NewValue
│
├─ SciFiSlider → SetValue (NewValue.FloatValue)
│   (SetValue 내부에서 bSuppressCallback=true이므로
│    OnSliderValueChanged 디스패처가 발생하지 않음 → 무한 루프 없음)
│
└─ (끝)
```

### 9.6 RefreshFromManager 함수 (Reset 시 호출)

```
Function: RefreshFromManager
│
├─ Get Game Instance → Get Subsystem (UDRSettingsManager) → SettingsManager
│   SettingsManager → GetPendingValue (SettingId) → Value
├─ UpdateDisplayValue (Value)
│
└─ (끝)
```

---

## 10. WBP_SettingRow_Toggle — Graph 로직

### 10.1 구조 복습

```
WBP_SettingRow_Toggle
└─ SizeBox_Root (760 x 92)
   └─ CanvasPanel_Row
      ├─ Text_Label
      ├─ Image_SeparatorLine
      └─ WBP_SciFiToggle
```

### 10.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Text_Label` | UTextBlock* | Designer 바인딩 |
| `SciFiToggle` | WBP_SciFiToggle* | Designer 바인딩 |
| `SettingId` | FName | 이 Row가 담당하는 설정 ID |
| `Definition` | FDRSettingDefinition | 설정 정의 사본 |

### 10.3 InitRow 함수

```
Function: InitRow
Input: FDRSettingDefinition InDef, UDRSettingsManager* InMgr
│
├─ Definition = InDef
├─ SettingId = InDef.SettingId
│
├─ Text_Label → SetText (InDef.LabelText)
│
├─ 현재값 로드:
│   InMgr → GetPendingValue (SettingId) → PendingValue
│   SciFiToggle → SetToggleState (PendingValue.BoolValue)
│
├─ ★ SciFiToggle.OnToggleChanged 바인딩 → HandleToggleChanged
│   (WBP_SciFiToggle의 커스텀 디스패처에 바인딩)
│
└─ (끝)
```

### 10.4 HandleToggleChanged

```
Function: HandleToggleChanged
Input: bool bNewState (OnToggleChanged에서 전달)
│
├─ Value = MakeBoolValue (bNewState)
│
├─ Get Game Instance → Get Subsystem (UDRSettingsManager) → SettingsManager
│   SettingsManager → SetPendingValue (SettingId, Value)
│
└─ (끝)
```

### 10.5 UpdateDisplayValue / RefreshFromManager

```
Function: UpdateDisplayValue
Input: FDRSettingsValue NewValue
│
├─ SciFiToggle → SetToggleState (NewValue.BoolValue)
│   (SetToggleState 내부에서 bSuppressCallback=true이므로
│    OnToggleChanged가 발생하지 않음 → 무한 루프 없음)
│
└─ (끝)
```

```
Function: RefreshFromManager
│
├─ Get Game Instance → Get Subsystem (UDRSettingsManager) → SettingsManager
│   SettingsManager → GetPendingValue (SettingId) → Value
├─ UpdateDisplayValue (Value)
│
└─ (끝)
```

---

## 11. WBP_SettingRow_Dropdown — Graph 로직

### 11.1 구조 복습

```
WBP_SettingRow_Dropdown
└─ SizeBox_Root (760 x 92)
   └─ CanvasPanel_Row
      ├─ Text_Label
      ├─ Image_SeparatorLine
      └─ WBP_SciFiDropdown
```

### 11.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Text_Label` | UTextBlock* | Designer 바인딩 |
| `SciFiDropdown` | WBP_SciFiDropdown* | Designer 바인딩 |
| `SettingId` | FName | 이 Row가 담당하는 설정 ID |
| `Definition` | FDRSettingDefinition | 설정 정의 사본 |

### 11.3 InitRow 함수

```
Function: InitRow
Input: FDRSettingDefinition InDef, UDRSettingsManager* InMgr, WBP_SettingsScreen* InScreen
│
├─ Definition = InDef
├─ SettingId = InDef.SettingId
│
├─ Text_Label → SetText (InDef.LabelText)
│
├─ 옵션 텍스트 배열 구성:
│   OptionTexts = []
│   For Each (InDef.Options):
│     OptionTexts.Add (Option.DisplayText)
│
├─ SciFiDropdown → InitDropdown (OptionTexts, InScreen)
│
├─ 현재값 로드:
│   InMgr → GetPendingValue (SettingId) → PendingValue
│   SciFiDropdown → SetSelectedIndex (PendingValue.SelectedIndex)
│
├─ ★ SciFiDropdown.OnSelectionChanged 바인딩 → HandleSelectionChanged
│   (WBP_SciFiDropdown의 커스텀 디스패처에 바인딩)
│
└─ (끝)
```

### 11.4 HandleSelectionChanged 콜백

```
Function: HandleSelectionChanged
Input: int32 NewIndex (OnSelectionChanged에서 전달)
│
├─ SelectedOption = Definition.Options[NewIndex]
│
├─ Switch (Definition.ValueType):
│   │
│   ├─ Name (DisplayMode, Language 등):
│   │   Value = MakeNameValue (SelectedOption.OptionId, NewIndex)
│   │
│   ├─ Int (FPSLimit, Scalability 등):
│   │   Value = MakeIntValue (SelectedOption.IntValue)
│   │   Value.SelectedIndex = NewIndex
│   │   ※ MakeIntValue는 SelectedIndex를 0으로 초기화하므로 반드시 수동 설정
│   │
│   ├─ Resolution:
│   │   Value = MakeResolutionValue (SelectedOption.ResolutionX, SelectedOption.ResolutionY, NewIndex)
│   │
│   └─ (기타 타입)
│
├─ Get Game Instance → Get Subsystem (UDRSettingsManager) → SettingsManager
│   SettingsManager → SetPendingValue (SettingId, Value)
│
└─ (끝)
```

### 11.5 UpdateDisplayValue / RefreshFromManager

```
Function: UpdateDisplayValue
Input: FDRSettingsValue NewValue
│
├─ SciFiDropdown → SetSelectedIndex (NewValue.SelectedIndex)
│
└─ (끝)
```

```
Function: RefreshFromManager
│
├─ Get Game Instance → Get Subsystem (UDRSettingsManager) → SettingsManager
│   SettingsManager → GetPendingValue (SettingId) → PendingValue
├─ UpdateDisplayValue (PendingValue)
│
└─ (끝)
```

---

## 12. WBP_SettingsTabButton — Graph 로직

### 12.1 구조 복습

```
WBP_SettingsTabButton
└─ SizeBox_Root (330 x 92)
   └─ Overlay_Root
      ├─ Image_SelectedBG
      ├─ Text_Label
      └─ Button_Hit
```

### 12.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Image_SelectedBG` | UImage* | Designer 바인딩 |
| `Text_Label` | UTextBlock* | Designer 바인딩 |
| `Button_Hit` | UButton* | Designer 바인딩 |
| `TabIndex` | int32 | 이 버튼의 탭 인덱스 (0~3) |
| `ParentScreen` | WBP_SettingsScreen* | 부모 화면 참조 |
| `bIsSelected` | bool | 현재 선택 상태 |

### 12.3 InitTab 함수

```
Function: InitTab
Input: int32 InTabIndex, WBP_SettingsScreen* InParentScreen
│
├─ TabIndex = InTabIndex
├─ ParentScreen = InParentScreen
│
├─ Switch (TabIndex):
│   0 → Text_Label → SetText ("GAMEPLAY")
│   1 → Text_Label → SetText ("GRAPHICS")
│   2 → Text_Label → SetText ("AUDIO")
│   3 → Text_Label → SetText ("CONTROLS")
│
├─ Button_Hit → OnClicked 바인딩 → HandleClicked
│
└─ (끝)
```

### 12.4 HandleClicked

```
Function: HandleClicked
│
├─ ParentScreen → SelectTab (TabIndex)
│
└─ (끝)
```

### 12.5 SetSelected 함수

```
Function: SetSelected
Input: bool bSelected
│
├─ bIsSelected = bSelected
│
├─ Branch: bSelected?
│   ├─ True:
│   │   Image_SelectedBG → Set Visibility: Not Hit-Testable (Self & All Children)
│   │   Image_SelectedBG → Set Render Opacity: 1.0
│   │   Text_Label → Set Color and Opacity: #DDF7FFFF (밝은 색)
│   └─ False:
│       Image_SelectedBG → Set Visibility: Hidden
│       Text_Label → Set Color and Opacity: #6677B8FF (어두운 색)
│
└─ (끝)
```

### 12.6 Hover 효과 (선택 사항)

```
Button_Hit → OnHovered:
  Branch: bIsSelected == false?
    True → Image_SelectedBG → Set Visibility: Not Hit-Testable (Self & All Children)
            Image_SelectedBG → Set Render Opacity: 0.4

Button_Hit → OnUnhovered:
  Branch: bIsSelected == false?
    True → Image_SelectedBG → Set Visibility: Hidden
```

---

## 13. WBP_KeyHintBar — Graph 로직

### 13.1 구조 복습

```
WBP_KeyHintBar
└─ SizeBox_Root (460 x 60)
   └─ HorizontalBox_Hints
      ├─ WBP_KeyHint_R ("RESET")
      ├─ Spacer
      ├─ WBP_KeyHint_ESC ("BACK")
      ├─ Spacer
      └─ WBP_KeyHint_A ("APPLY")
```

### 13.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `KeyHint_A` | WBP_KeyHint* | Designer 바인딩. A 키 힌트 위젯 |

### 13.3 SetApplyVisible 함수

```
Function: SetApplyVisible
Input: bool bVisible
│
├─ Branch: bVisible?
│   ├─ True → KeyHint_A → Set Visibility: Not Hit-Testable (Self & All Children)
│   └─ False → KeyHint_A → Set Visibility: Collapsed
│
└─ (끝)
```

WBP_SettingsScreen에서 `OnHasPendingChangesChanged`를 받으면 이 함수를 호출한다.

---

## 14. WBP_SettingsPage_Gameplay — Graph 로직

### 14.1 구조 복습

```
WBP_SettingsPage_Gameplay
└─ SizeBox_Root
   └─ CanvasPanel_Page
      ├─ Text_PageTitle ("GAMEPLAY")
      └─ VerticalBox_Rows
         ├─ WBP_SettingRow_Slider_CameraSensitivity
         └─ WBP_SettingRow_Dropdown_Language
```

### 14.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Row_CameraSensitivity` | WBP_SettingRow_Slider* | Designer 바인딩 |
| `Row_Language` | WBP_SettingRow_Dropdown* | Designer 바인딩 |

### 14.3 InitializePage 함수

```
Function: InitializePage
Input: UDRSettingsManager* InManager, WBP_SettingsScreen* InParentScreen
│
├─ ① Camera Sensitivity Row 초기화
│   InManager → GetDefinitionById ("Gameplay.CameraSensitivity") → Definition
│   Row_CameraSensitivity → InitRow (Definition, InManager)
│
├─ ② Language Row 초기화
│   InManager → GetDefinitionById ("Gameplay.Language") → Definition
│   Row_Language → InitRow (Definition, InManager, InParentScreen)
│
├─ ③ 델리게이트 바인딩
│   InManager.OnPendingValueChanged → OnPendingValueChanged
│   InManager.OnSettingsReset → OnSettingsReset
│
└─ (끝)
```

### 14.4 OnPendingValueChanged 콜백

```
Function: OnPendingValueChanged
Input: FName SettingId, FDRSettingsValue NewValue
│
├─ Branch: SettingId == "Gameplay.CameraSensitivity"?
│   └─ True → Row_CameraSensitivity → UpdateDisplayValue (NewValue)
│
├─ Branch: SettingId == "Gameplay.Language"?
│   └─ True → Row_Language → UpdateDisplayValue (NewValue)
│
└─ (끝)
```

### 14.5 OnSettingsReset 콜백

```
Function: OnSettingsReset
Input: EDRSettingsTab Tab
│
├─ Branch: Tab == Gameplay?
│   └─ True:
│       Row_CameraSensitivity → RefreshFromManager()
│       Row_Language → RefreshFromManager()
│
└─ (끝)
```

---

## 15. WBP_SettingsPage_Graphics — Graph 로직

### 15.1 구조 복습

```
WBP_SettingsPage_Graphics
└─ SizeBox_Root
   └─ CanvasPanel_Page
      ├─ Text_PageTitle ("GRAPHICS")
      └─ VerticalBox_Rows
         ├─ WBP_SettingRow_Dropdown_DisplayMode
         ├─ WBP_SettingRow_Dropdown_Resolution
         ├─ WBP_SettingRow_Toggle_VSync
         ├─ WBP_SettingRow_Dropdown_FPS
         ├─ WBP_SettingRow_Dropdown_Scalability
         └─ WBP_SettingRow_Slider_Gamma
```

### 15.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Row_DisplayMode` | WBP_SettingRow_Dropdown* | Designer 바인딩 |
| `Row_Resolution` | WBP_SettingRow_Dropdown* | Designer 바인딩 |
| `Row_VSync` | WBP_SettingRow_Toggle* | Designer 바인딩 |
| `Row_FPS` | WBP_SettingRow_Dropdown* | Designer 바인딩 |
| `Row_Scalability` | WBP_SettingRow_Dropdown* | Designer 바인딩 |
| `Row_Gamma` | WBP_SettingRow_Slider* | Designer 바인딩 |

### 15.3 InitializePage 함수

```
Function: InitializePage
Input: UDRSettingsManager* InManager, WBP_SettingsScreen* InParentScreen
│
├─ ① DisplayMode
│   InManager → GetDefinitionById ("Graphics.DisplayMode") → Def
│   Row_DisplayMode → InitRow (Def, InManager, InParentScreen)
│
├─ ② Resolution
│   InManager → GetDefinitionById ("Graphics.Resolution") → Def
│   Row_Resolution → InitRow (Def, InManager, InParentScreen)
│
├─ ③ VSync
│   InManager → GetDefinitionById ("Graphics.VSync") → Def
│   Row_VSync → InitRow (Def, InManager)
│
├─ ④ FPS
│   InManager → GetDefinitionById ("Graphics.FPSLimit") → Def
│   Row_FPS → InitRow (Def, InManager, InParentScreen)
│
├─ ⑤ Scalability
│   InManager → GetDefinitionById ("Graphics.Scalability") → Def
│   Row_Scalability → InitRow (Def, InManager, InParentScreen)
│
├─ ⑥ Gamma
│   InManager → GetDefinitionById ("Graphics.Gamma") → Def
│   Row_Gamma → InitRow (Def, InManager)
│
├─ 델리게이트 바인딩:
│   InManager.OnPendingValueChanged → OnPendingValueChanged
│   InManager.OnSettingsReset → OnSettingsReset
│
└─ (끝)
```

### 15.4 OnPendingValueChanged 콜백

```
Function: OnPendingValueChanged
Input: FName SettingId, FDRSettingsValue NewValue
│
├─ Switch (SettingId):
│   "Graphics.DisplayMode"  → Row_DisplayMode  → UpdateDisplayValue (NewValue)
│   "Graphics.Resolution"   → Row_Resolution   → UpdateDisplayValue (NewValue)
│   "Graphics.VSync"        → Row_VSync        → UpdateDisplayValue (NewValue)
│   "Graphics.FPSLimit"     → Row_FPS          → UpdateDisplayValue (NewValue)
│   "Graphics.Scalability"  → Row_Scalability  → UpdateDisplayValue (NewValue)
│   "Graphics.Gamma"        → Row_Gamma        → UpdateDisplayValue (NewValue)
│
└─ (끝)
```

### 15.5 OnSettingsReset 콜백

```
Function: OnSettingsReset
Input: EDRSettingsTab Tab
│
├─ Branch: Tab == Graphics?
│   └─ True:
│       Row_DisplayMode → RefreshFromManager()
│       Row_Resolution → RefreshFromManager()
│       Row_VSync → RefreshFromManager()
│       Row_FPS → RefreshFromManager()
│       Row_Scalability → RefreshFromManager()
│       Row_Gamma → RefreshFromManager()
│
└─ (끝)
```

---

## 16. WBP_SettingsPage_Audio — Graph 로직

### 16.1 구조

```
WBP_SettingsPage_Audio
└─ SizeBox_Root
   └─ CanvasPanel_Page
      ├─ Text_PageTitle ("AUDIO")
      └─ VerticalBox_Rows
         ├─ WBP_SettingRow_Slider_MasterVolume
         ├─ WBP_SettingRow_Slider_MusicVolume
         └─ WBP_SettingRow_Slider_SFXVolume
```

### 16.2 필요한 변수

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `Row_MasterVolume` | WBP_SettingRow_Slider* | Designer 바인딩 |
| `Row_MusicVolume` | WBP_SettingRow_Slider* | Designer 바인딩 |
| `Row_SFXVolume` | WBP_SettingRow_Slider* | Designer 바인딩 |

### 16.3 InitializePage

```
Function: InitializePage
Input: UDRSettingsManager* InManager, WBP_SettingsScreen* InParentScreen
│
├─ InManager → GetDefinitionById ("Audio.MasterVolume") → Def
│  Row_MasterVolume → InitRow (Def, InManager)
│
├─ InManager → GetDefinitionById ("Audio.MusicVolume") → Def
│  Row_MusicVolume → InitRow (Def, InManager)
│
├─ InManager → GetDefinitionById ("Audio.SFXVolume") → Def
│  Row_SFXVolume → InitRow (Def, InManager)
│
├─ 델리게이트 바인딩:
│   InManager.OnPendingValueChanged → OnPendingValueChanged
│   InManager.OnSettingsReset → OnSettingsReset
│
└─ (끝)
```

### 16.4 OnPendingValueChanged / OnSettingsReset

```
OnPendingValueChanged (FName SettingId, FDRSettingsValue NewValue):
  "Audio.MasterVolume" → Row_MasterVolume → UpdateDisplayValue (NewValue)
  "Audio.MusicVolume"  → Row_MusicVolume  → UpdateDisplayValue (NewValue)
  "Audio.SFXVolume"    → Row_SFXVolume    → UpdateDisplayValue (NewValue)
```

```
OnSettingsReset (EDRSettingsTab Tab):
  Tab == Audio? → 모든 Row → RefreshFromManager()
```

---

## 17. WBP_SettingsPage_Controls — Graph 로직

현재 비어있으므로 InitializePage에서 아무것도 초기화하지 않는다.

```
Function: InitializePage
Input: UDRSettingsManager* InManager, WBP_SettingsScreen* InParentScreen
│
└─ (아무것도 안 함 — 추후 키 리바인딩 추가 시 여기에 작성)
```

---

## 18. WBP_SettingsScreen — Graph 로직

### 18.1 필요한 변수

| 변수 이름 | 타입 | 카테고리 | 설명 |
|---|---|---|---|
| `WidgetSwitcher_SettingsPages` | UWidgetSwitcher* | Designer 바인딩 | 페이지 전환기 |
| `CanvasPanel_DropdownPopupLayer` | UCanvasPanel* | Designer 바인딩 | 드롭다운 팝업 레이어 |
| `CurrentTabIndex` | int32 | State | 현재 선택된 탭 (0~3) |
| `TabButtons` | TArray\<WBP_SettingsTabButton*\> | State | 탭 버튼 4개 배열 |
| `PageGameplay` | WBP_SettingsPage_Gameplay* | Designer 바인딩 | |
| `PageGraphics` | WBP_SettingsPage_Graphics* | Designer 바인딩 | |
| `PageAudio` | WBP_SettingsPage_Audio* | Designer 바인딩 | |
| `PageControls` | WBP_SettingsPage_Controls* | Designer 바인딩 | |
| `KeyHintBar` | WBP_KeyHintBar* | Designer 바인딩 | 하단 키 힌트 바 |
| `ActiveDropdown` | WBP_SciFiDropdown* | State | 현재 열린 드롭다운 (닫기용) |

### 18.2 Event Construct

```
Event Construct
│
├─ ① SettingsManager 참조 획득
│   Get Game Instance → Get Subsystem (UDRSettingsManager) → SettingsManager (로컬 변수)
│
├─ ② 설정 시스템 초기화
│   SettingsManager → InitSettings()
│   (이미 로드된 경우 bLoaded=true로 조기 리턴)
│
├─ ③ 각 페이지 초기화
│   PageGameplay → InitializePage (SettingsManager, self)
│   PageGraphics → InitializePage (SettingsManager, self)
│   PageAudio → InitializePage (SettingsManager, self)
│   PageControls → InitializePage (SettingsManager, self)
│
├─ ④ 탭 버튼 초기화
│   TabButtons 배열에 4개의 WBP_SettingsTabButton 참조 추가
│   (Designer에서 VerticalBox_LeftTabs의 자식들을 Is Variable로 설정해 두었으므로 직접 참조)
│   각 TabButton → InitTab (TabIndex, self)
│
├─ ⑤ 기본 탭 선택 (Graphics = 인덱스 1)
│   SelectTab (1)
│
├─ ⑥ 델리게이트 바인딩
│   SettingsManager.OnHasPendingChangesChanged → OnPendingChangesChanged
│   SettingsManager.OnSettingsApplied → OnSettingsApplied
│
├─ ⑦ 키보드 포커스 설정
│   self → Set Keyboard Focus
│   (OnKeyDown을 받기 위해 필요)
│
└─ (끝)
```

### 18.3 SelectTab 함수

```
Function: SelectTab
Input: int32 TabIndex
│
├─ CurrentTabIndex = TabIndex
│
├─ WidgetSwitcher_SettingsPages → Set Active Widget Index (TabIndex)
│
├─ For Each (TabButtons 배열):
│   │
│   ├─ Branch: 현재 Index == TabIndex?
│   │   ├─ True → TabButton → SetSelected (true)
│   │   └─ False → TabButton → SetSelected (false)
│
└─ (끝)
```

### 18.4 키보드 입력 처리 (OnKeyDown 오버라이드)

WBP_SettingsScreen은 `Is Focusable: true`로 설정되어 있으므로, `OnKeyDown`을 오버라이드하여 키 입력을 처리한다.

> **왜 OnKeyDown인가?**: 설정 화면이 열리면 `SetInputMode(FInputModeUIOnly)`가 호출되어 Enhanced Input이 작동하지 않는다.
> 따라서 UMG의 `OnKeyDown`이 키 입력을 받는 유일한 방법이다.

```
Event OnKeyDown (MyGeometry, InKeyEvent)
│
├─ Key = InKeyEvent → Get Key
│
├─ Branch: Key == Escape?
│   └─ True:
│       ├─ Get Game Instance → Get Subsystem (UDRSettingsManager) → Mgr
│       │   Branch: Mgr.bHasPendingChanges?
│       │   ├─ True → Mgr → DiscardPendingChanges()
│       │   └─ False → (skip)
│       ├─ Get Owning Player → Cast to DRPlayerController
│       │   → CloseSettingsMenu()
│       └─ Return: Handled
│
├─ Branch: Key == R?
│   └─ True:
│       ├─ CurrentTabIndex → EDRSettingsTab 변환 (아래 참고)
│       ├─ Get Game Instance → Get Subsystem (UDRSettingsManager) → Mgr
│       │   Mgr → ResetTabToDefault (변환된 EDRSettingsTab)
│       └─ Return: Handled
│
├─ Branch: Key == A?
│   └─ True:
│       ├─ Get Game Instance → Get Subsystem (UDRSettingsManager) → Mgr
│       │   Branch: Mgr.bHasPendingChanges?
│       │   ├─ True → Mgr → ApplyPendingSettings()
│       │   └─ False → (skip)
│       └─ Return: Handled
│
└─ Return: Unhandled
```

**CurrentTabIndex → EDRSettingsTab 변환**:
- 0 → `EDRSettingsTab::Gameplay`
- 1 → `EDRSettingsTab::Graphics`
- 2 → `EDRSettingsTab::Audio`
- 3 → `EDRSettingsTab::Controls`

Switch 노드 또는 `static_cast` (Make Literal Enum)로 처리 가능.

### 18.5 OnPendingChangesChanged 콜백

```
Function: OnPendingChangesChanged
Input: bool bHasPendingChanges
│
├─ KeyHintBar → SetApplyVisible (bHasPendingChanges)
│
└─ (끝)
```

### 18.6 OnSettingsApplied 콜백

```
Function: OnSettingsApplied
│
├─ (현재는 특별한 처리 불필요)
│  (Apply 후 bHasPendingChanges가 false가 되면
│   OnPendingChangesChanged에서 A 키 힌트가 자동으로 사라짐)
│
└─ (끝)
```

### 18.7 드롭다운 팝업 레이어 관련 함수

WBP_SciFiDropdown이 열릴 때 옵션 목록을 `CanvasPanel_DropdownPopupLayer`에 띄워야 한다.

```
Function: ShowDropdownPopup
Input: WBP_SciFiDropdownOptionList* OptionListWidget, FVector2D ScreenPosition
│
├─ CanvasPanel_DropdownPopupLayer → Clear Children
│
├─ ── 외부 클릭으로 닫기 위한 투명 배경 버튼 추가 ──
│   Create Widget (UButton) → BackgroundButton
│   BackgroundButton의 Style: 모든 상태 Tint Alpha = 0
│   CanvasPanel_DropdownPopupLayer → Add Child (BackgroundButton)
│   Canvas Panel Slot:
│     Anchors: (0,0) ~ (1,1)  (전체 화면)
│     Offsets: 0,0,0,0
│     ZOrder: 0
│   BackgroundButton.OnClicked → HandleBackgroundClicked
│
├─ ── 옵션 리스트 위젯 추가 ──
│   CanvasPanel_DropdownPopupLayer → Add Child (OptionListWidget)
│   Canvas Panel Slot:
│     Position = ScreenPosition
│     Size = (280, Auto)
│     ZOrder: 1
│
├─ CanvasPanel_DropdownPopupLayer → Set Visibility: Visible
│
├─ ActiveDropdown = OptionListWidget의 OwnerDropdown
│   (닫기 처리를 위해 현재 열린 드롭다운 추적)
│
└─ (끝)
```

```
Function: HideDropdownPopup
│
├─ CanvasPanel_DropdownPopupLayer → Clear Children
│
├─ CanvasPanel_DropdownPopupLayer → Set Visibility: Not Hit-Testable (Self Only)
│
├─ ActiveDropdown = null
│
├─ self → Set Keyboard Focus
│   (포커스를 다시 SettingsScreen으로 돌려 OnKeyDown 작동 보장)
│
└─ (끝)
```

```
Function: HandleBackgroundClicked
│
├─ Branch: ActiveDropdown이 Valid?
│   └─ True → ActiveDropdown → CloseDropdown()
│
└─ (끝)
```

---

## 19. BP_DRPlayerController — 설정 화면 열기/닫기

`ADRPlayerController`의 `OpenSettingsMenu`/`CloseSettingsMenu`는 `BlueprintNativeEvent`로 선언되어 있으므로, 블루프린트 자식 클래스(`BP_DRPlayerController`)에서 오버라이드하여 WBP 생성/제거 로직을 구현한다.

### 19.1 변수 추가 (BP_DRPlayerController)

| 변수 이름 | 타입 | 설명 |
|---|---|---|
| `SettingsScreenClass` | TSubclassOf\<UUserWidget\> | WBP_SettingsScreen 클래스 참조 (Details에서 설정) |
| `SettingsScreenInstance` | UUserWidget* | 현재 생성된 설정 화면 인스턴스 |

### 19.2 OpenSettingsMenu 오버라이드

```
Event OpenSettingsMenu
│
├─ (부모 호출: Parent: OpenSettingsMenu)
│   → C++ 내부: bIsSettingsMenuOpen = true
│   → C++ 내부: InputMode = UI Only
│   → C++ 내부: ShowMouseCursor = true
│
├─ Branch: SettingsScreenInstance가 Valid한가?
│   ├─ True → 이미 존재하면 아무것도 안 함 (return)
│   └─ False → 아래로 진행
│
├─ Create Widget (SettingsScreenClass, Owning Player = self)
│   → 결과를 SettingsScreenInstance에 저장
│
├─ SettingsScreenInstance → Add to Viewport (ZOrder: 50)
│
└─ (끝)
```

### 19.3 CloseSettingsMenu 오버라이드

```
Event CloseSettingsMenu
│
├─ Branch: SettingsScreenInstance가 Valid한가?
│   ├─ True:
│   │   ├─ SettingsScreenInstance → Remove from Parent
│   │   └─ SettingsScreenInstance = null
│   └─ False → skip
│
└─ (부모 호출: Parent: CloseSettingsMenu)
    → C++ 내부: bIsSettingsMenuOpen = false
    → C++ 내부: RestoreDefaultInputMode()
```

---

## 20. 데이터 흐름 요약

### 20.1 설정 화면 열기 흐름

```
(1) 플레이어가 ESC(또는 설정 키) 누름
(2) DRPlayerController::HandleToggleSettings() → ToggleSettingsMenu()
(3) OpenSettingsMenu_Implementation() (C++: 입력 모드 변경)
(4) BP_DRPlayerController Override: WBP_SettingsScreen 생성 + Viewport에 추가
(5) WBP_SettingsScreen::Event Construct:
    - GetGameInstance → GetSubsystem(UDRSettingsManager) → SettingsManager
    - SettingsManager → InitSettings()
    - 각 Page → InitializePage(SettingsManager, self)
    - 각 Row → InitRow(Definition, SettingsManager, ...)
    - 현재값 로드 및 표시
```

### 20.2 값 변경 흐름 (슬라이더 예시: Gamma)

```
(1) 플레이어가 Gamma 슬라이더 드래그
(2) Slider_Input 내장 OnValueChanged(0.8) 발생
(3) WBP_SciFiSlider::HandleNativeSliderChanged(0.8)
    → AbsoluteValue = 80.0 계산
    → Fill 바/텍스트 업데이트
    → ★ 커스텀 OnSliderValueChanged 디스패처 Broadcast(80.0)
(4) WBP_SettingRow_Slider::HandleSliderValueChanged(80.0)
    → MakeFloatValue(80.0)
    → SettingsManager::SetPendingValue("Graphics.Gamma", Value)
(5) SettingsManager 내부:
    - PendingValues에 저장
    - DirtySettingIds에 추가
    - OnPendingValueChanged 브로드캐스트
    - ApplyMode == Instant이므로:
      → ApplySingleSetting() → Gamma 즉시 적용
      → CommitSingleSetting() → CurrentValues에 반영
(6) WBP_SettingsPage_Graphics::OnPendingValueChanged
    → Row_Gamma → UpdateDisplayValue
    → SciFiSlider → SetValue(80.0)
    → bSuppressCallback=true이므로 디스패처 발생 안 함 (무한 루프 방지)
```

### 20.3 값 변경 흐름 (드롭다운 예시: DisplayMode)

```
(1) 플레이어가 DisplayMode 드롭다운 클릭
(2) WBP_SciFiDropdown::HandleClicked → OpenDropdown()
(3) WBP_SettingsScreen::ShowDropdownPopup() → 팝업 레이어에 OptionList 표시
(4) 플레이어가 "WINDOWED" 옵션 클릭
(5) WBP_SciFiDropdownOption::HandleClicked
    → ★ 커스텀 OnOptionClicked 디스패처 Broadcast(2)
(6) WBP_SciFiDropdownOptionList::HandleOptionClicked(2)
    → WBP_SciFiDropdown::OnOptionSelected(2)
(7) WBP_SciFiDropdown:
    → SetSelectedIndex(2) — 텍스트 "WINDOWED"로 변경
    → CloseDropdown() — 팝업 닫기
    → ★ 커스텀 OnSelectionChanged 디스패처 Broadcast(2)
(8) WBP_SettingRow_Dropdown::HandleSelectionChanged(2)
    → Definition.Options[2] → OptionId="Windowed"
    → MakeNameValue("Windowed", 2)
    → SettingsManager::SetPendingValue("Graphics.DisplayMode", Value)
(9) ApplyMode == RequiresApply이므로 Pending에만 저장됨
(10) OnHasPendingChangesChanged → Apply 힌트(A 키) 표시
```

### 20.4 Apply 흐름

```
(1) 플레이어가 A 키 누름
(2) WBP_SettingsScreen::OnKeyDown 핸들러
(3) SettingsManager::ApplyPendingSettings()
    - 모든 Dirty 설정 ApplySingleSetting()
    - CurrentValues = PendingValues
    - SaveToGameUserSettings() → INI 파일 저장
    - ApplyResolutionSettings() + ApplyNonResolutionSettings()
    - OnSettingsApplied 브로드캐스트
(4) bHasPendingChanges = false → A 키 힌트 숨김
```

### 20.5 Reset 흐름

```
(1) 플레이어가 R 키 누름
(2) WBP_SettingsScreen::OnKeyDown 핸들러
(3) CurrentTabIndex → EDRSettingsTab 변환 (예: 1 → Graphics)
(4) SettingsManager::ResetTabToDefault(Graphics)
    - 해당 탭의 모든 설정을 DefaultValue로 PendingValues에 설정
    - 각 설정에 대해 OnPendingValueChanged 브로드캐스트
    - OnSettingsReset(Graphics) 브로드캐스트
(5) 각 Row의 UI가 기본값으로 업데이트됨
(6) Apply 필요 시 A 키 힌트 표시
```

### 20.6 닫기 흐름

```
(1) 플레이어가 ESC 키 누름
(2) WBP_SettingsScreen::OnKeyDown 핸들러
(3) bHasPendingChanges → DiscardPendingChanges() (변경 취소)
(4) PlayerController → CloseSettingsMenu()
    - BP Override: WBP_SettingsScreen → RemoveFromParent
    - C++ Parent: bIsSettingsMenuOpen = false, RestoreDefaultInputMode()
```

---

## 21. 주의사항 및 팁

### 21.1 슬라이더 무한 루프 방지 (bSuppressCallback)

슬라이더에서 무한 루프가 발생할 수 있는 경로:

```
사용자 드래그
→ Slider_Input 내장 OnValueChanged
→ HandleNativeSliderChanged
→ 커스텀 OnSliderValueChanged 디스패처
→ Row::HandleSliderValueChanged
→ SetPendingValue
→ OnPendingValueChanged (C++ 델리게이트)
→ Page::OnPendingValueChanged
→ Row::UpdateDisplayValue
→ SciFiSlider::SetValue
→ Slider_Input → Set Value
→ Slider_Input 내장 OnValueChanged 다시 발생! ← 무한 루프
```

**해결**: `WBP_SciFiSlider::SetValue()`에서 `bSuppressCallback = true`로 설정한 뒤 Slider 값 변경. `HandleNativeSliderChanged`가 호출되어도 `bSuppressCallback` 체크로 즉시 리턴.

**토글도 동일**: `WBP_SciFiToggle::SetToggleState()`에서 `bSuppressCallback = true`로 설정하여 `HandleClicked`에서 `OnToggleChanged`가 발생하지 않도록 처리.

### 21.2 드롭다운 외부 클릭으로 닫기

드롭다운이 열린 상태에서 다른 곳을 클릭하면 닫혀야 한다.

**구현 방법** (Section 18.7에 포함):
`ShowDropdownPopup`에서 `CanvasPanel_DropdownPopupLayer`에 먼저 전체 화면 투명 Button을 추가(ZOrder 0), 그 위에 OptionList를 추가(ZOrder 1). 투명 Button의 OnClicked → `ActiveDropdown → CloseDropdown()`.

### 21.3 Focus 관리

WBP_SettingsScreen이 키보드 입력(ESC, R, A)을 받으려면:
- `Is Focusable: true` (Designer에서 설정)
- Event Construct에서 `self → Set Keyboard Focus`
- 드롭다운/버튼 클릭 후 Focus가 해당 위젯으로 이동할 수 있으므로, `HideDropdownPopup`에서 Focus를 SettingsScreen으로 돌려야 함

### 21.4 해상도 드롭다운 동적 옵션

`Graphics.Resolution`의 Options는 `BuildDefinitions()`에서 `GetSupportedResolutions()`를 통해 동적으로 생성됨.
모니터에 따라 옵션 수가 다를 수 있으므로, OptionList의 높이가 유동적이어야 함.
`WBP_SciFiDropdownOptionList`의 `SizeBox_Root`에서 `Height Override: 체크 해제`가 이를 처리함.

### 21.5 Instant vs RequiresApply 설정

| 설정 | ApplyMode | 동작 |
|---|---|---|
| Gameplay.CameraSensitivity | **Instant** | 슬라이더 움직이면 즉시 감도 변경 |
| Gameplay.Language | RequiresApply | A 키로 Apply 필요 |
| Graphics.DisplayMode | RequiresApply | A 키로 Apply 필요 |
| Graphics.Resolution | RequiresApply | A 키로 Apply 필요 |
| Graphics.VSync | RequiresApply | A 키로 Apply 필요 |
| Graphics.FPSLimit | RequiresApply | A 키로 Apply 필요 |
| Graphics.Scalability | RequiresApply | A 키로 Apply 필요 |
| Graphics.Gamma | **Instant** | 슬라이더 움직이면 즉시 감마 변경 |
| Audio.MasterVolume | **Instant** | 슬라이더 움직이면 즉시 볼륨 변경 |
| Audio.MusicVolume | **Instant** | 슬라이더 움직이면 즉시 볼륨 변경 |
| Audio.SFXVolume | **Instant** | 슬라이더 움직이면 즉시 볼륨 변경 |

Instant 설정은 `SetPendingValue()` 내부에서 자동으로 `ApplySingleSetting()` + `CommitSingleSetting()`을 호출하므로, WBP에서 별도 처리가 필요 없음.

RequiresApply 설정은 A 키를 눌러 `ApplyPendingSettings()`를 호출해야만 실제 적용됨.

---

## 22. 구현 순서 요약

### Phase A: C++ 수정 (완료)

모든 코드 수정 적용 완료 (Section 1 참고).

### Phase B: WBP Designer 제작 (사용자가 직접)

`WBP_Designer_Hierarchy_Details.md` 문서대로 모든 WBP Designer 배치.

### Phase C: WBP Graph 로직 (Bottom-Up 순서)

```
 1. WBP_KeyHint ─────────────────── Graph 로직 없음
 2. WBP_SciFiSlider ─────────────── 커스텀 OnSliderValueChanged 디스패처 생성
 3. WBP_SciFiToggle ─────────────── 커스텀 OnToggleChanged 디스패처 생성
 4. WBP_SciFiDropdownOption ─────── 커스텀 OnOptionClicked 디스패처 생성
 5. WBP_SciFiDropdownOptionList ─── OnOptionClicked 바인딩
 6. WBP_SciFiDropdown ──────────── 커스텀 OnSelectionChanged 디스패처 생성
 7. WBP_SettingRow_Slider ──────── OnSliderValueChanged 바인딩
 8. WBP_SettingRow_Toggle ──────── OnToggleChanged 바인딩
 9. WBP_SettingRow_Dropdown ────── OnSelectionChanged 바인딩
10. WBP_SettingsTabButton ──────── Button_Hit.OnClicked 바인딩
11. WBP_KeyHintBar ─────────────── SetApplyVisible 함수
12. WBP_SettingsPage_Gameplay ──── Row 초기화 + 델리게이트 바인딩
13. WBP_SettingsPage_Graphics ──── Row 초기화 + 델리게이트 바인딩
14. WBP_SettingsPage_Audio ─────── Row 초기화 + 델리게이트 바인딩
15. WBP_SettingsPage_Controls ──── 빈 페이지
16. WBP_SettingsScreen ─────────── 전체 조립, OnKeyDown, 드롭다운 팝업
17. BP_DRPlayerController ─────── OpenSettings / CloseSettings 오버라이드
```

### Phase D: 통합 테스트

```
1. 설정 화면 열기/닫기 테스트
2. 탭 전환 테스트
3. 슬라이더 조작 → 즉시 적용 확인 (Gamma, Volume, Sensitivity)
4. 드롭다운 선택 → Pending 상태 확인
5. Apply (A 키) 테스트
6. Reset (R 키) 테스트
7. ESC로 Discard 후 닫기 테스트
8. 설정 저장 후 재시작 시 값 유지 확인
```
