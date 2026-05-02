# 설정 UI WBP Designer 구성 가이드

이 문서는 **Graph 로직 없이**, Unreal Engine UMG의 **WBP Designer 탭에서 실제로 배치할 Hierarchy와 Details 패널 설정값**만 기준으로 정리한 가이드입니다.

기준 해상도는 레퍼런스 이미지에 맞춰 **2048 × 1152**로 잡습니다.

> 용어 주의  
> Unreal Engine 버전이나 한글/영문 UI 설정에 따라 `Visibility` 항목 이름이 다르게 보일 수 있습니다.  
> 이 문서에서는 다음 표기를 함께 사용합니다.

| 기존 영문 표기 | 최신/다른 버전에서 보일 수 있는 표기 | 의미 |
|---|---|---|
| Visible | Visible / 보임 | 보이고 입력도 받을 수 있음 |
| Hidden | Hidden / 숨김 | 안 보이지만 레이아웃 공간은 유지 |
| Collapsed | Collapsed / 접힘 | 안 보이고 레이아웃 공간도 제거 |
| Hit Test Invisible | Not Hit-Testable (Self & All Children) / 히트 테스트 불가 | 보이지만 자기 자신과 자식 모두 입력을 받지 않음 |
| Self Hit Test Invisible | Not Hit-Testable (Self Only) / 자신만 히트 테스트 불가 | 자기 자신은 입력을 받지 않지만 자식 버튼은 입력 가능 |

장식용 이미지는 보통 **Not Hit-Testable (Self & All Children)** 로 둡니다.  
클릭 가능한 버튼이나 슬라이더는 **Visible** 로 둡니다.

---

# 0. Designer 기준에서 헷갈리기 쉬운 부분

## 0.1 `Position: Fill`이라는 설정은 없다

이전 설명에서 `Position: Fill`처럼 표현한 부분은 Designer에 실제로 있는 항목명이 아닙니다.

부모가 `Overlay`라면 다음처럼 설정합니다.

```text
Overlay Slot
├─ Horizontal Alignment: Fill
├─ Vertical Alignment: Fill
└─ Padding: 0, 0, 0, 0
```

부모가 `Canvas Panel`이라면 다음처럼 설정합니다.

```text
Canvas Panel Slot
├─ Anchors
├─ Position X / Position Y
├─ Size X / Size Y
├─ Alignment X / Alignment Y
└─ ZOrder
```

---

## 0.2 TextBlock 자체에는 일반적인 세로 정렬이 없다

`TextBlock`을 세로 가운데에 두고 싶다면 `TextBlock` 자체 옵션이 아니라 **부모 Slot**에서 맞춥니다.

예를 들어 부모가 `Overlay`라면:

```text
Overlay Slot
└─ Vertical Alignment: Center
```

부모가 `Horizontal Box`라면:

```text
Horizontal Box Slot
└─ Vertical Alignment: Center
```

---

## 0.3 Text Padding은 보통 부모 Slot Padding으로 처리한다

예를 들어 좌측 메뉴 텍스트를 왼쪽에서 42px 띄우려면:

```text
Text_Label
└─ Overlay Slot
   └─ Padding Left: 42
```

하단 키 힌트의 키 아이콘과 텍스트 사이 간격은:

```text
Text_Label
└─ Horizontal Box Slot
   └─ Padding Left: 10
```

처럼 처리합니다.

---

## 0.4 ZOrder는 Canvas Panel Slot에서만 주로 보인다

`Canvas Panel`의 자식은 `ZOrder`가 있습니다.

하지만 `Overlay` 안에서는 `ZOrder`를 찾지 말고, **Hierarchy에서 아래에 있는 자식이 위에 그려진다**고 보면 됩니다.

예:

```text
Overlay_Root
├─ Image_Background
├─ Image_Frame
├─ Text_Label
└─ Button_Hit
```

이 경우 `Button_Hit`이 가장 위에 있습니다.

---

## 0.5 Button을 투명하게 만드는 방법

Button Style에서 `No Draw` 또는 `Draw As: No Draw Type`이 보이는 버전도 있지만, 안 보일 수 있습니다.

가장 안전한 방법은 Button의 Style에서 각 상태의 Tint Alpha를 0으로 만드는 것입니다.

```text
Button_Hit
└─ Appearance > Style
   ├─ Normal Tint Alpha: 0
   ├─ Hovered Tint Alpha: 0
   ├─ Pressed Tint Alpha: 0
   └─ Disabled Tint Alpha: 0
```

---

# 1. 사용할 WBP 목록

```text
WBP_SettingsScreen
WBP_SettingsTabButton

WBP_SettingsPage_Gameplay
WBP_SettingsPage_Graphics
WBP_SettingsPage_Audio
WBP_SettingsPage_Controls

WBP_SettingRow_Dropdown
WBP_SettingRow_Toggle
WBP_SettingRow_Slider

WBP_SciFiDropdown
WBP_SciFiDropdownOptionList
WBP_SciFiDropdownOption
WBP_SciFiToggle
WBP_SciFiSlider
WBP_SciFiScrollBar

WBP_KeyHint
WBP_KeyHintBar
```

이번 Designer 구성에서는 `NamedSlot`을 쓰는 `RowBase` 방식은 생략합니다.  
초반 제작에서는 Designer에서 명확하게 보이는 개별 Row 위젯을 만드는 편이 더 안정적입니다.

---

# 2. 이미지 에셋 용도 정리

| 번호 | 권장 에셋명 | 사용 위치 |
|---:|---|---|
| 1 | `T_Settings_BG` | 전체 배경 |
| 2 | `T_Settings_FolderPanel` | 중앙 폴더 패널 |
| 3 | `T_Settings_Tab_Selected` | 좌측 선택 메뉴 배경 |
| 4 | `T_Settings_Row_Line` | 설정 항목 사이 가로선 |
| 5 | `T_Dropdown_Normal` | 드롭다운 기본 상태 |
| 6 | `T_Dropdown_Hover` | 드롭다운 Hover 상태 |
| 7 | `T_Dropdown_Selected` | 드롭다운 선택/열림 상태 |
| 8 | `T_Icon_Arrow_Down` | 드롭다운 아래 화살표 |
| 9 | `T_Control_Frame` | 드롭다운/토글 외곽 프레임 |
| 10 | `T_Toggle_SelectedFill` | 토글 선택 영역 채움 |
| 11 | `T_Slider_Frame` | 슬라이더 외곽 프레임 |
| 12 | `T_Slider_Fill` | 슬라이더 채움 바 |
| 13 | `T_Slider_ValueBox` | 슬라이더 숫자 박스 |
| 14 | `T_Scroll_Rail` | 세로 스크롤 레일 |
| 15 | `T_Scroll_Handle` | 세로 스크롤 손잡이 |
| 16 | `T_Key_R` | 하단 R 키 아이콘 |
| 17 | `T_Key_ESC` | 하단 ESC 키 아이콘 |
| 18 | `T_Key_A` | 하단 A 키 아이콘 |

---

# 3. 텍스처 Import 권장 설정

각 Texture를 열고 다음처럼 설정합니다.

```text
Compression Settings: UserInterface2D (RGBA)
Texture Group: UI
Mip Gen Settings: NoMipmaps
sRGB: true
Never Stream: true
```

반투명 홀로그램 UI 이미지가 많기 때문에 `UserInterface2D (RGBA)`를 사용하는 것이 좋습니다.

---

# 4. WBP_SettingsScreen

메인 설정 화면입니다.

## 4.1 Hierarchy

```text
WBP_SettingsScreen
└─ CanvasPanel_Root
   ├─ Image_Background
   └─ ScaleBox_UI
      └─ SizeBox_Reference
         └─ CanvasPanel_UI
            ├─ Image_FolderPanel
            ├─ Text_SettingsTitle
            ├─ VerticalBox_LeftTabs
            │  ├─ WBP_SettingsTabButton_Gameplay
            │  ├─ WBP_SettingsTabButton_Graphics
            │  ├─ WBP_SettingsTabButton_Audio
            │  └─ WBP_SettingsTabButton_Controls
            ├─ WidgetSwitcher_SettingsPages
            │  ├─ WBP_SettingsPage_Gameplay
            │  ├─ WBP_SettingsPage_Graphics
            │  ├─ WBP_SettingsPage_Audio
            │  └─ WBP_SettingsPage_Controls
            ├─ WBP_SciFiScrollBar
            ├─ WBP_KeyHintBar
            └─ CanvasPanel_DropdownPopupLayer
```

## 4.2 WBP_SettingsScreen Class Defaults

Designer 우측 상단의 **Class Defaults**에서:

```text
Is Focusable: true
```

키보드 입력을 받을 예정이면 켜둡니다.

---

## 4.3 CanvasPanel_Root

```text
Root Widget: Canvas Panel
Visibility: Visible
```

---

## 4.4 Image_Background

1번 배경 이미지입니다.

부모:

```text
CanvasPanel_Root
```

Canvas Panel Slot:

```text
Anchors:
  Minimum X: 0
  Minimum Y: 0
  Maximum X: 1
  Maximum Y: 1

Offsets:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0

Alignment:
  X: 0
  Y: 0

ZOrder: 0
```

Appearance:

```text
Brush > Image: T_Settings_BG
Brush > Draw As: Image
Color and Opacity: White
Render Opacity: 1.0
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

배경은 클릭을 막으면 안 되므로 `Not Hit-Testable (Self & All Children)`로 둡니다.

---

## 4.5 ScaleBox_UI

부모:

```text
CanvasPanel_Root
```

Canvas Panel Slot:

```text
Anchors:
  Minimum X: 0
  Minimum Y: 0
  Maximum X: 1
  Maximum Y: 1

Offsets:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0

Alignment:
  X: 0
  Y: 0

ZOrder: 10
```

ScaleBox 설정:

```text
Stretch: Scale To Fit
Stretch Direction: Both
User Specified Scale: 1.0
Ignore Inherited Scale: false
```

Behavior:

```text
Visibility: Visible
Is Variable: false
```

---

## 4.6 SizeBox_Reference

부모:

```text
ScaleBox_UI
```

SizeBox 설정:

```text
Width Override: 체크, 2048
Height Override: 체크, 1152
```

Behavior:

```text
Visibility: Visible
Is Variable: false
```

---

## 4.7 CanvasPanel_UI

부모:

```text
SizeBox_Reference
```

설정:

```text
Visibility: Visible
Is Variable: true
```

이 CanvasPanel 안의 좌표는 전부 **2048 × 1152 기준**입니다.

---

## 4.8 Image_FolderPanel

2번 폴더 모양 이미지입니다.

부모:

```text
CanvasPanel_UI
```

Canvas Panel Slot:

```text
Anchors:
  Minimum X: 0
  Minimum Y: 0
  Maximum X: 0
  Maximum Y: 0

Position X: 0
Position Y: 18
Size X: 2048
Size Y: 1068
Alignment X: 0
Alignment Y: 0
ZOrder: 1
```

Appearance:

```text
Brush > Image: T_Settings_FolderPanel
Brush > Draw As: Image
Color and Opacity: White
Render Opacity: 1.0
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

2번 이미지는 큰 패널 전체 이미지이므로 `Draw As: Image`로 그대로 사용합니다.

---

## 4.9 Text_SettingsTitle

부모:

```text
CanvasPanel_UI
```

Canvas Panel Slot:

```text
Position X: 438
Position Y: 238
Size X: 480
Size Y: 80
Alignment X: 0
Alignment Y: 0
ZOrder: 5
```

Content:

```text
Text: SETTINGS
```

Appearance:

```text
Font:
  Family: 프로젝트 SF 폰트
  Typeface: Regular 또는 Bold
  Size: 60

Color and Opacity: #DDF7FFFF
Justification: Left
Auto Wrap Text: false
```

Font에 Outline Settings가 보이면:

```text
Outline Settings > Outline Size: 1
Outline Settings > Outline Color: #7CCFFF66
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 4.10 VerticalBox_LeftTabs

부모:

```text
CanvasPanel_UI
```

Canvas Panel Slot:

```text
Position X: 438
Position Y: 355
Size X: 330
Size Y: 380
Alignment X: 0
Alignment Y: 0
ZOrder: 10
```

Behavior:

```text
Visibility: Visible
Is Variable: true
```

---

## 4.11 WBP_SettingsTabButton 네 개

각각 `VerticalBox_LeftTabs`의 자식입니다.

Vertical Box Slot:

```text
Size: Auto
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

각 버튼 텍스트:

```text
GAMEPLAY
GRAPHICS
AUDIO
CONTROLS
```

기본 화면을 Graphics로 시작한다면 Designer에서는 `Graphics` 버튼의 선택 배경만 Visible로 켜두고, 나머지는 Hidden으로 둬도 됩니다.

---

## 4.12 WidgetSwitcher_SettingsPages

부모:

```text
CanvasPanel_UI
```

Canvas Panel Slot:

```text
Position X: 865
Position Y: 280
Size X: 760
Size Y: 620
Alignment X: 0
Alignment Y: 0
ZOrder: 10
```

Details:

```text
Active Widget Index: 1
```

자식 순서:

```text
0: WBP_SettingsPage_Gameplay
1: WBP_SettingsPage_Graphics
2: WBP_SettingsPage_Audio
3: WBP_SettingsPage_Controls
```

Behavior:

```text
Visibility: Visible
Is Variable: true
```

---

## 4.13 WBP_SciFiScrollBar

부모:

```text
CanvasPanel_UI
```

Canvas Panel Slot:

```text
Position X: 1660
Position Y: 245
Size X: 44
Size Y: 710
Alignment X: 0
Alignment Y: 0
ZOrder: 15
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

장식용으로 항상 보이게 할 경우 이 설정이 적합합니다.

---

## 4.14 WBP_KeyHintBar

부모:

```text
CanvasPanel_UI
```

Canvas Panel Slot:

```text
Position X: 1575
Position Y: 1028
Size X: 460
Size Y: 60
Alignment X: 0
Alignment Y: 0
ZOrder: 20
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

## 4.15 CanvasPanel_DropdownPopupLayer

드롭다운 목록을 화면 위에 띄울 때 사용할 레이어입니다.

부모:

```text
CanvasPanel_UI
```

Canvas Panel Slot:

```text
Anchors:
  Minimum X: 0
  Minimum Y: 0
  Maximum X: 1
  Maximum Y: 1

Offsets:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0

Alignment:
  X: 0
  Y: 0

ZOrder: 80
```

Behavior:

```text
Visibility: Not Hit-Testable (Self Only)
구버전 표기: Self Hit Test Invisible
Is Variable: true
```

이 레이어는 자기 자신은 클릭을 막지 않으면서, 나중에 올라오는 드롭다운 옵션 버튼은 클릭 가능하게 둬야 합니다. 그래서 **Self Only**를 씁니다.

---

# 5. WBP_SettingsTabButton

좌측 메뉴 버튼 하나입니다.

## 5.1 Hierarchy

```text
WBP_SettingsTabButton
└─ SizeBox_Root
   └─ Overlay_Root
      ├─ Image_SelectedBG
      ├─ Text_Label
      └─ Button_Hit
```

---

## 5.2 SizeBox_Root

Root Widget:

```text
SizeBox
```

SizeBox 설정:

```text
Width Override: 체크, 330
Height Override: 체크, 92
```

Behavior:

```text
Visibility: Visible
Is Variable: false
```

---

## 5.3 Overlay_Root

부모:

```text
SizeBox_Root
```

Behavior:

```text
Visibility: Visible
Is Variable: false
```

---

## 5.4 Image_SelectedBG

3번 이미지입니다. 선택된 메뉴에 깔리는 파란 반투명 사각형입니다.

부모:

```text
Overlay_Root
```

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Appearance:

```text
Brush > Image: T_Settings_Tab_Selected
Brush > Draw As: Image
Color and Opacity: White
Render Opacity: 1.0
```

Behavior:

```text
Visibility:
  선택 상태 기본 버튼: Not Hit-Testable (Self & All Children)
  비선택 상태 기본 버튼: Hidden

구버전 선택 상태 표기: Hit Test Invisible
Is Variable: true
```

---

## 5.5 Text_Label

부모:

```text
Overlay_Root
```

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Center
Padding:
  Left: 42
  Top: 0
  Right: 0
  Bottom: 0
```

Content 예시:

```text
Text: GRAPHICS
```

Appearance:

```text
Font:
  Family: 프로젝트 SF 폰트
  Size: 34

Color and Opacity: #DDF7FFFF
Justification: Left
Auto Wrap Text: false
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

## 5.6 Button_Hit

부모:

```text
Overlay_Root
```

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Behavior:

```text
Visibility: Visible
Is Focusable: true
Is Variable: true
```

Appearance > Style:

```text
Normal Tint Alpha: 0
Hovered Tint Alpha: 0
Pressed Tint Alpha: 0
Disabled Tint Alpha: 0
```

Button은 입력을 받아야 하므로 `Visible`이어야 합니다.

---

# 6. WBP_SettingsPage_Graphics

Graphics 페이지 예시입니다. Gameplay, Audio, Controls도 같은 구조로 만듭니다.

## 6.1 Hierarchy

```text
WBP_SettingsPage_Graphics
└─ SizeBox_Root
   └─ CanvasPanel_Page
      ├─ Text_PageTitle
      └─ VerticalBox_Rows
         ├─ WBP_SettingRow_Dropdown_DisplayMode
         ├─ WBP_SettingRow_Dropdown_Resolution
         ├─ WBP_SettingRow_Toggle_VSync
         ├─ WBP_SettingRow_Dropdown_FPS
         └─ WBP_SettingRow_Slider_Gamma
```

옵션이 많아질 예정이면 `VerticalBox_Rows` 대신 `ScrollBox_Rows`를 써도 됩니다.

---

## 6.2 SizeBox_Root

```text
Width Override: 체크, 760
Height Override: 체크, 620
Visibility: Visible
```

---

## 6.3 CanvasPanel_Page

```text
Visibility: Visible
Is Variable: false
```

---

## 6.4 Text_PageTitle

부모:

```text
CanvasPanel_Page
```

Canvas Panel Slot:

```text
Position X: 0
Position Y: 0
Size X: 760
Size Y: 60
Alignment X: 0
Alignment Y: 0
ZOrder: 0
```

Content:

```text
Text: GRAPHICS
```

Appearance:

```text
Font Size: 34
Color and Opacity: #8EA4FFFF
Justification: Center
Auto Wrap Text: false
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 6.5 VerticalBox_Rows

부모:

```text
CanvasPanel_Page
```

Canvas Panel Slot:

```text
Position X: 0
Position Y: 75
Size X: 760
Size Y: 520
Alignment X: 0
Alignment Y: 0
ZOrder: 1
```

Behavior:

```text
Visibility: Visible
Is Variable: true
```

각 Row 자식의 Vertical Box Slot:

```text
Size: Auto
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

---

# 7. WBP_SettingsPage_Gameplay

## 7.1 Hierarchy

```text
WBP_SettingsPage_Gameplay
└─ SizeBox_Root
   └─ CanvasPanel_Page
      ├─ Text_PageTitle
      └─ VerticalBox_Rows
         ├─ WBP_SettingRow_Slider_CameraSensitivity
         └─ WBP_SettingRow_Dropdown_Language
```

## 7.2 Text_PageTitle

```text
Text: GAMEPLAY
```

나머지 값은 Graphics 페이지와 동일합니다.

---

# 8. WBP_SettingsPage_Audio

## 8.1 Hierarchy

```text
WBP_SettingsPage_Audio
└─ SizeBox_Root
   └─ CanvasPanel_Page
      ├─ Text_PageTitle
      └─ VerticalBox_Rows
         ├─ WBP_SettingRow_Slider_MasterVolume
         ├─ WBP_SettingRow_Slider_MusicVolume
         ├─ WBP_SettingRow_Slider_SFXVolume
         └─ WBP_SettingRow_Slider_VoiceVolume
```

## 8.2 Text_PageTitle

```text
Text: AUDIO
```

나머지 값은 Graphics 페이지와 동일합니다.

---

# 9. WBP_SettingsPage_Controls

## 9.1 Hierarchy

```text
WBP_SettingsPage_Controls
└─ SizeBox_Root
   └─ CanvasPanel_Page
      ├─ Text_PageTitle
      └─ VerticalBox_Rows
         ├─ WBP_SettingRow_Dropdown_MoveForward
         ├─ WBP_SettingRow_Dropdown_MoveBackward
         ├─ WBP_SettingRow_Dropdown_Jump
         ├─ WBP_SettingRow_Dropdown_Interact
         └─ WBP_SettingRow_Dropdown_Sprint
```

## 9.2 Text_PageTitle

```text
Text: CONTROLS
```

키 리바인딩 전용 UI를 나중에 만들 경우 `WBP_SettingRow_KeyBind`를 따로 추가합니다.

---

# 10. WBP_SettingRow_Dropdown

드롭다운이 들어가는 한 줄입니다.

## 10.1 Hierarchy

```text
WBP_SettingRow_Dropdown
└─ SizeBox_Root
   └─ CanvasPanel_Row
      ├─ Text_Label
      ├─ Image_SeparatorLine
      └─ WBP_SciFiDropdown
```

---

## 10.2 SizeBox_Root

```text
Width Override: 체크, 760
Height Override: 체크, 92
Visibility: Visible
```

---

## 10.3 CanvasPanel_Row

```text
Visibility: Visible
Is Variable: false
```

---

## 10.4 Text_Label

부모:

```text
CanvasPanel_Row
```

Canvas Panel Slot:

```text
Position X: 0
Position Y: 14
Size X: 390
Size Y: 55
Alignment X: 0
Alignment Y: 0
ZOrder: 1
```

Content 예시:

```text
Text: DISPLAY MODE
```

Appearance:

```text
Font Size: 34
Color and Opacity: #DDF7FFFF
Justification: Left
Auto Wrap Text: false
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

## 10.5 Image_SeparatorLine

4번 얇은 가로선입니다.

부모:

```text
CanvasPanel_Row
```

Canvas Panel Slot:

```text
Position X: 0
Position Y: 72
Size X: 735
Size Y: 18
Alignment X: 0
Alignment Y: 0
ZOrder: 0
```

Appearance:

```text
Brush > Image: T_Settings_Row_Line
Brush > Draw As: Image
Color and Opacity: White
Render Opacity: 0.55
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 10.6 WBP_SciFiDropdown

부모:

```text
CanvasPanel_Row
```

Canvas Panel Slot:

```text
Position X: 465
Position Y: 14
Size X: 280
Size Y: 48
Alignment X: 0
Alignment Y: 0
ZOrder: 2
```

Behavior:

```text
Visibility: Visible
Is Variable: true
```

---

# 11. WBP_SettingRow_Toggle

토글이 들어가는 한 줄입니다.

## 11.1 Hierarchy

```text
WBP_SettingRow_Toggle
└─ SizeBox_Root
   └─ CanvasPanel_Row
      ├─ Text_Label
      ├─ Image_SeparatorLine
      └─ WBP_SciFiToggle
```

`SizeBox_Root`, `Text_Label`, `Image_SeparatorLine`은 Dropdown Row와 동일합니다.

---

## 11.2 WBP_SciFiToggle

부모:

```text
CanvasPanel_Row
```

Canvas Panel Slot:

```text
Position X: 465
Position Y: 14
Size X: 280
Size Y: 48
Alignment X: 0
Alignment Y: 0
ZOrder: 2
```

Behavior:

```text
Visibility: Visible
Is Variable: true
```

---

# 12. WBP_SettingRow_Slider

슬라이더가 들어가는 한 줄입니다.

## 12.1 Hierarchy

```text
WBP_SettingRow_Slider
└─ SizeBox_Root
   └─ CanvasPanel_Row
      ├─ Text_Label
      ├─ Image_SeparatorLine
      └─ WBP_SciFiSlider
```

`SizeBox_Root`, `Text_Label`, `Image_SeparatorLine`은 Dropdown Row와 동일합니다.

---

## 12.2 WBP_SciFiSlider

부모:

```text
CanvasPanel_Row
```

Canvas Panel Slot:

```text
Position X: 390
Position Y: 14
Size X: 380
Size Y: 48
Alignment X: 0
Alignment Y: 0
ZOrder: 2
```

Behavior:

```text
Visibility: Visible
Is Variable: true
```

슬라이더는 값 박스까지 포함해서 드롭다운보다 가로가 길기 때문에 X를 390 정도로 둡니다.

---

# 13. WBP_SciFiDropdown

5, 6, 7, 8, 9번 이미지를 사용하는 커스텀 드롭다운입니다.

## 13.1 Hierarchy

```text
WBP_SciFiDropdown
└─ SizeBox_Root
   └─ Overlay_Root
      ├─ Image_BG_Normal
      ├─ Image_BG_Hover
      ├─ Image_BG_Selected
      ├─ Image_Frame
      ├─ Text_SelectedValue
      ├─ SizeBox_Arrow
      │  └─ Image_Arrow
      └─ Button_Hit
```

---

## 13.2 SizeBox_Root

```text
Width Override: 체크, 280
Height Override: 체크, 48
Visibility: Visible
```

---

## 13.3 Overlay_Root

```text
Visibility: Visible
```

---

## 13.4 Image_BG_Normal

5번 드롭다운 기본 박스입니다.

부모:

```text
Overlay_Root
```

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Appearance:

```text
Brush > Image: T_Dropdown_Normal
Brush > Draw As: Box
Brush > Margin:
  Left: 0.08
  Top: 0.30
  Right: 0.08
  Bottom: 0.30

Color and Opacity: White
Render Opacity: 1.0
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

`Draw As: Box`에서 이미지가 깨지면 `Draw As: Image`로 바꿔도 됩니다.

---

## 13.5 Image_BG_Hover

6번 마우스 오버 상태입니다.

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Appearance:

```text
Brush > Image: T_Dropdown_Hover
Brush > Draw As: Box
Brush > Margin:
  Left: 0.08
  Top: 0.30
  Right: 0.08
  Bottom: 0.30
```

Behavior:

```text
Visibility: Hidden
Is Variable: true
```

---

## 13.6 Image_BG_Selected

7번 선택/열림 상태입니다.

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Appearance:

```text
Brush > Image: T_Dropdown_Selected
Brush > Draw As: Box
Brush > Margin:
  Left: 0.08
  Top: 0.30
  Right: 0.08
  Bottom: 0.30
```

Behavior:

```text
Visibility: Hidden
Is Variable: true
```

---

## 13.7 Image_Frame

9번 외곽 프레임입니다.

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Appearance:

```text
Brush > Image: T_Control_Frame
Brush > Draw As: Box
Brush > Margin:
  Left: 0.06
  Top: 0.30
  Right: 0.06
  Bottom: 0.30

Color and Opacity: White
Render Opacity: 1.0
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 13.8 Text_SelectedValue

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Center
Padding:
  Left: 22
  Top: 0
  Right: 58
  Bottom: 0
```

Content 예시:

```text
Text: FULLSCREEN
```

Appearance:

```text
Font Size: 24
Color and Opacity: #DDF7FFFF
Justification: Left
Auto Wrap Text: false
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

## 13.9 SizeBox_Arrow

부모:

```text
Overlay_Root
```

Overlay Slot:

```text
Horizontal Alignment: Right
Vertical Alignment: Center
Padding:
  Left: 0
  Top: 0
  Right: 20
  Bottom: 0
```

SizeBox 설정:

```text
Width Override: 체크, 22
Height Override: 체크, 12
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 13.10 Image_Arrow

8번 아래 방향 화살표입니다.

부모:

```text
SizeBox_Arrow
```

Appearance:

```text
Brush > Image: T_Icon_Arrow_Down
Brush > Draw As: Image
Color and Opacity: White
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 13.11 Button_Hit

부모:

```text
Overlay_Root
```

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Behavior:

```text
Visibility: Visible
Is Focusable: true
Is Variable: true
```

Appearance > Style:

```text
Normal Tint Alpha: 0
Hovered Tint Alpha: 0
Pressed Tint Alpha: 0
Disabled Tint Alpha: 0
```

---

# 14. WBP_SciFiDropdownOptionList

드롭다운을 열었을 때 나오는 목록입니다.

## 14.1 Hierarchy

```text
WBP_SciFiDropdownOptionList
└─ SizeBox_Root
   └─ VerticalBox_Options
      ├─ WBP_SciFiDropdownOption
      ├─ WBP_SciFiDropdownOption
      └─ WBP_SciFiDropdownOption
```

---

## 14.2 SizeBox_Root

```text
Width Override: 체크, 280
Height Override: 체크 해제
Visibility: Visible
```

높이는 옵션 개수에 따라 자식들이 만드는 Desired Size를 사용하게 둡니다.

---

## 14.3 VerticalBox_Options

```text
Visibility: Visible
Is Variable: true
```

각 Option 자식의 Vertical Box Slot:

```text
Size: Auto
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

---

# 15. WBP_SciFiDropdownOption

드롭다운 목록의 옵션 한 줄입니다.

## 15.1 Hierarchy

```text
WBP_SciFiDropdownOption
└─ SizeBox_Root
   └─ Overlay_Root
      ├─ Image_BG_Normal
      ├─ Image_BG_Hover
      ├─ Image_BG_Selected
      ├─ Text_Option
      └─ Button_Hit
```

---

## 15.2 SizeBox_Root

```text
Width Override: 체크, 280
Height Override: 체크, 44
```

---

## 15.3 Image_BG_Normal

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding: 0, 0, 0, 0
```

Appearance:

```text
Brush > Image: T_Dropdown_Normal
Brush > Draw As: Box
Brush > Margin:
  Left: 0.08
  Top: 0.30
  Right: 0.08
  Bottom: 0.30
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
```

---

## 15.4 Image_BG_Hover

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding: 0, 0, 0, 0
```

Appearance:

```text
Brush > Image: T_Dropdown_Hover
Brush > Draw As: Box
Brush > Margin:
  Left: 0.08
  Top: 0.30
  Right: 0.08
  Bottom: 0.30
```

Behavior:

```text
Visibility: Hidden
Is Variable: true
```

---

## 15.5 Image_BG_Selected

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding: 0, 0, 0, 0
```

Appearance:

```text
Brush > Image: T_Dropdown_Selected
Brush > Draw As: Box
Brush > Margin:
  Left: 0.08
  Top: 0.30
  Right: 0.08
  Bottom: 0.30
```

Behavior:

```text
Visibility: Hidden
Is Variable: true
```

---

## 15.6 Text_Option

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Center
Padding:
  Left: 22
  Top: 0
  Right: 22
  Bottom: 0
```

Content 예시:

```text
Text: FULLSCREEN
```

Appearance:

```text
Font Size: 23
Color and Opacity: #DDF7FFFF
Justification: Left
Auto Wrap Text: false
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

## 15.7 Button_Hit

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Behavior:

```text
Visibility: Visible
Is Focusable: true
Is Variable: true
```

Appearance > Style:

```text
Normal Tint Alpha: 0
Hovered Tint Alpha: 0
Pressed Tint Alpha: 0
Disabled Tint Alpha: 0
```

---

# 16. WBP_SciFiToggle

토글 스위치입니다.

## 16.1 Hierarchy

```text
WBP_SciFiToggle
└─ SizeBox_Root
   └─ CanvasPanel_Toggle
      ├─ Image_SelectedFill
      ├─ Image_Frame
      ├─ Text_Off
      ├─ Text_On
      └─ Button_Hit
```

---

## 16.2 SizeBox_Root

```text
Width Override: 체크, 280
Height Override: 체크, 48
Visibility: Visible
```

---

## 16.3 CanvasPanel_Toggle

```text
Visibility: Visible
```

---

## 16.4 Image_SelectedFill

10번 선택된 쪽 밝은 채움 이미지입니다.

부모:

```text
CanvasPanel_Toggle
```

Canvas Panel Slot:

```text
Position X: 142
Position Y: 4
Size X: 134
Size Y: 40
Alignment X: 0
Alignment Y: 0
ZOrder: 0
```

ON 기본 상태라면 X를 142로 둡니다.  
OFF 기본 상태라면 X를 4로 둡니다.

Appearance:

```text
Brush > Image: T_Toggle_SelectedFill
Brush > Draw As: Box
Brush > Margin:
  Left: 0.15
  Top: 0.25
  Right: 0.15
  Bottom: 0.25

Color and Opacity: White
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

## 16.5 Image_Frame

9번 외곽 프레임입니다.

Canvas Panel Slot:

```text
Position X: 0
Position Y: 0
Size X: 280
Size Y: 48
Alignment X: 0
Alignment Y: 0
ZOrder: 1
```

Appearance:

```text
Brush > Image: T_Control_Frame
Brush > Draw As: Box
Brush > Margin:
  Left: 0.06
  Top: 0.30
  Right: 0.06
  Bottom: 0.30
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

프레임은 선택 채움보다 위에 있어야 외곽선이 살아납니다.

---

## 16.6 Text_Off

Canvas Panel Slot:

```text
Position X: 0
Position Y: 7
Size X: 140
Size Y: 34
Alignment X: 0
Alignment Y: 0
ZOrder: 2
```

Content:

```text
Text: OFF
```

Appearance:

```text
Font Size: 24
Color and Opacity:
  기본 ON 상태일 때: #DDF7FFFF
  기본 OFF 상태일 때: #6677B8FF

Justification: Center
Auto Wrap Text: false
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

## 16.7 Text_On

Canvas Panel Slot:

```text
Position X: 140
Position Y: 7
Size X: 140
Size Y: 34
Alignment X: 0
Alignment Y: 0
ZOrder: 2
```

Content:

```text
Text: ON
```

Appearance:

```text
Font Size: 24
Color and Opacity:
  기본 ON 상태일 때: #6677B8FF
  기본 OFF 상태일 때: #DDF7FFFF

Justification: Center
Auto Wrap Text: false
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

밝은 채움 위의 글자는 어두운 보라/블루 계열이 더 잘 보입니다.

---

## 16.8 Button_Hit

Canvas Panel Slot:

```text
Position X: 0
Position Y: 0
Size X: 280
Size Y: 48
Alignment X: 0
Alignment Y: 0
ZOrder: 3
```

Behavior:

```text
Visibility: Visible
Is Focusable: true
Is Variable: true
```

Appearance > Style:

```text
Normal Tint Alpha: 0
Hovered Tint Alpha: 0
Pressed Tint Alpha: 0
Disabled Tint Alpha: 0
```

---

# 17. WBP_SciFiSlider

슬라이더입니다.  
실제 입력은 투명한 기본 `Slider`를 위에 얹고, 보이는 이미지는 제공한 이미지로 구성합니다.

## 17.1 Hierarchy

```text
WBP_SciFiSlider
└─ SizeBox_Root
   └─ CanvasPanel_Slider
      ├─ Overlay_Track
      │  ├─ Image_SliderFrame
      │  ├─ SizeBox_FillClip
      │  │  └─ Image_SliderFill
      │  └─ Slider_Input
      └─ Overlay_ValueBox
         ├─ Image_ValueBox
         └─ Text_Value
```

---

## 17.2 SizeBox_Root

```text
Width Override: 체크, 380
Height Override: 체크, 48
Visibility: Visible
```

---

## 17.3 CanvasPanel_Slider

```text
Visibility: Visible
```

---

## 17.4 Overlay_Track

부모:

```text
CanvasPanel_Slider
```

Canvas Panel Slot:

```text
Position X: 0
Position Y: 9
Size X: 280
Size Y: 32
Alignment X: 0
Alignment Y: 0
ZOrder: 0
```

---

## 17.5 Image_SliderFrame

11번 슬라이더 전체 외곽 프레임입니다.

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Appearance:

```text
Brush > Image: T_Slider_Frame
Brush > Draw As: Box
Brush > Margin:
  Left: 0.08
  Top: 0.35
  Right: 0.08
  Bottom: 0.35
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 17.6 SizeBox_FillClip

밝은 채움 바를 자르는 영역입니다.

Overlay Slot:

```text
Horizontal Alignment: Left
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

SizeBox 설정:

```text
Width Override: 체크, 224
Height Override: 체크, 32
```

80% 기본값이면:

```text
280 × 0.8 = 224
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Clipping: Clip to Bounds
Is Variable: true
```

Clipping은 이 영역에만 사용하는 것을 추천합니다.

---

## 17.7 Image_SliderFill

12번 채워진 밝은 바입니다.

부모:

```text
SizeBox_FillClip
```

Appearance:

```text
Brush > Image: T_Slider_Fill
Brush > Draw As: Box
Brush > Margin:
  Left: 0.08
  Top: 0.35
  Right: 0.08
  Bottom: 0.35

Color and Opacity: White
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 17.8 Slider_Input

투명한 실제 입력 슬라이더입니다.

부모:

```text
Overlay_Track
```

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Slider 설정:

```text
Value: 0.8
Min Value: 0.0
Max Value: 1.0
Step Size: 0.01
Orientation: Horizontal
Is Focusable: true
```

Appearance:

```text
Render Opacity: 0.0
```

Behavior:

```text
Visibility: Visible
Is Variable: true
```

주의:

```text
Visibility를 Hidden으로 하면 입력도 안 받습니다.
보이지 않게 하려면 Visibility는 Visible로 두고 Render Opacity만 0.0으로 둡니다.
```

---

## 17.9 Overlay_ValueBox

부모:

```text
CanvasPanel_Slider
```

Canvas Panel Slot:

```text
Position X: 300
Position Y: 8
Size X: 64
Size Y: 34
Alignment X: 0
Alignment Y: 0
ZOrder: 1
```

---

## 17.10 Image_ValueBox

13번 숫자 텍스트 박스입니다.

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Fill
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Appearance:

```text
Brush > Image: T_Slider_ValueBox
Brush > Draw As: Box
Brush > Margin:
  Left: 0.12
  Top: 0.30
  Right: 0.12
  Bottom: 0.30
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 17.11 Text_Value

Overlay Slot:

```text
Horizontal Alignment: Fill
Vertical Alignment: Center
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

Content:

```text
Text: 80%
```

Appearance:

```text
Font Size: 18
Color and Opacity: #DDF7FFFF
Justification: Center
Auto Wrap Text: false
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

# 18. WBP_SciFiScrollBar

우측 세로 스크롤바입니다.

## 18.1 Hierarchy

```text
WBP_SciFiScrollBar
└─ SizeBox_Root
   └─ CanvasPanel_Root
      ├─ Image_Rail
      └─ Image_Handle
```

---

## 18.2 SizeBox_Root

```text
Width Override: 체크, 44
Height Override: 체크, 710
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
```

---

## 18.3 CanvasPanel_Root

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
```

---

## 18.4 Image_Rail

14번 세로 레일입니다.

Canvas Panel Slot:

```text
Position X: 0
Position Y: 0
Size X: 44
Size Y: 710
Alignment X: 0
Alignment Y: 0
ZOrder: 0
```

Appearance:

```text
Brush > Image: T_Scroll_Rail
Brush > Draw As: Image
Color and Opacity: White
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 18.5 Image_Handle

15번 손잡이입니다.

Canvas Panel Slot:

```text
Position X: 5
Position Y: 90
Size X: 34
Size Y: 170
Alignment X: 0
Alignment Y: 0
ZOrder: 1
```

Appearance:

```text
Brush > Image: T_Scroll_Handle
Brush > Draw As: Image
Color and Opacity: White
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

# 19. WBP_KeyHint

하단 키 아이콘 하나와 텍스트 하나를 묶은 위젯입니다.

## 19.1 Hierarchy

```text
WBP_KeyHint
└─ HorizontalBox_Root
   ├─ SizeBox_KeyIcon
   │  └─ Image_KeyIcon
   └─ Text_Label
```

---

## 19.2 HorizontalBox_Root

Root Widget:

```text
Horizontal Box
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: false
```

---

## 19.3 SizeBox_KeyIcon

Horizontal Box Slot:

```text
Size: Auto
Horizontal Alignment: Fill
Vertical Alignment: Center
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

SizeBox 설정:

R 또는 A 키일 때:

```text
Width Override: 체크, 38
Height Override: 체크, 40
```

ESC 키일 때:

```text
Width Override: 체크, 76
Height Override: 체크, 40
```

---

## 19.4 Image_KeyIcon

Appearance:

```text
Brush > Image:
  R 키: T_Key_R
  ESC 키: T_Key_ESC
  A 키: T_Key_A

Brush > Draw As: Image
Color and Opacity: White
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

## 19.5 Text_Label

Horizontal Box Slot:

```text
Size: Auto
Horizontal Alignment: Fill
Vertical Alignment: Center
Padding:
  Left: 10
  Top: 0
  Right: 0
  Bottom: 0
```

Content 예시:

```text
Text: RESET
```

Appearance:

```text
Font Size: 32
Color and Opacity: #DDF7FFFF
Justification: Left
Auto Wrap Text: false
```

Behavior:

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
Is Variable: true
```

---

# 20. WBP_KeyHintBar

하단 키 안내 전체 묶음입니다.

## 20.1 Hierarchy

```text
WBP_KeyHintBar
└─ SizeBox_Root
   └─ HorizontalBox_Hints
      ├─ WBP_KeyHint_R
      ├─ Spacer_Between_01
      ├─ WBP_KeyHint_ESC
      ├─ Spacer_Between_02
      └─ WBP_KeyHint_A
```

---

## 20.2 SizeBox_Root

```text
Width Override: 체크, 460
Height Override: 체크, 60
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
```

---

## 20.3 HorizontalBox_Hints

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
```

---

## 20.4 WBP_KeyHint_R

Horizontal Box Slot:

```text
Size: Auto
Vertical Alignment: Center
Padding:
  Left: 0
  Top: 0
  Right: 0
  Bottom: 0
```

내부:

```text
Image: T_Key_R
Text: RESET
```

---

## 20.5 Spacer_Between_01

Spacer 설정:

```text
Size X: 42
Size Y: 1
```

---

## 20.6 WBP_KeyHint_ESC

내부:

```text
Image: T_Key_ESC
Text: BACK
```

---

## 20.7 Spacer_Between_02

A 키를 쓸 때만 필요합니다.

```text
Size X: 42
Size Y: 1
```

---

## 20.8 WBP_KeyHint_A

내부:

```text
Image: T_Key_A
Text: APPLY 또는 SELECT
```

Designer 기본 화면에서 A 키가 필요 없으면:

```text
Visibility: Collapsed
```

로 둬도 됩니다.

---

# 21. 기본 ComboBoxString을 사용할 경우

커스텀 `WBP_SciFiDropdown` 대신 기본 `ComboBoxString`을 사용할 수도 있습니다.  
하지만 네가 제공한 5, 6, 7, 8, 9번 이미지와 완전히 맞추기는 어렵습니다.

`WBP_SettingRow_Dropdown`에서 `WBP_SciFiDropdown` 대신 `ComboBoxString`을 넣을 경우:

Canvas Panel Slot:

```text
Position X: 465
Position Y: 14
Size X: 280
Size Y: 48
Alignment X: 0
Alignment Y: 0
ZOrder: 2
```

ComboBoxString Details:

```text
Default Options:
  FULLSCREEN
  BORDERLESS
  WINDOWED

Selected Option: FULLSCREEN
Has Down Arrow: false
Content Padding:
  Left: 22
  Top: 0
  Right: 22
  Bottom: 0

Max List Height: 240
Is Focusable: true
```

추천은 다음과 같습니다.

```text
빠른 기능 구현:
  ComboBoxString 사용

레퍼런스와 같은 외형 구현:
  WBP_SciFiDropdown 직접 제작
```

---

# 22. 이미지별 실제 적용 위치 요약

```text
1번 배경
→ WBP_SettingsScreen / Image_Background

2번 폴더 패널
→ WBP_SettingsScreen / Image_FolderPanel

3번 좌측 선택 메뉴 배경
→ WBP_SettingsTabButton / Image_SelectedBG

4번 얇은 가로선
→ WBP_SettingRow_Dropdown, Toggle, Slider / Image_SeparatorLine

5번 드롭다운 기본 상태
→ WBP_SciFiDropdown / Image_BG_Normal
→ WBP_SciFiDropdownOption / Image_BG_Normal

6번 드롭다운 Hover 상태
→ WBP_SciFiDropdown / Image_BG_Hover
→ WBP_SciFiDropdownOption / Image_BG_Hover

7번 드롭다운 Selected 상태
→ WBP_SciFiDropdown / Image_BG_Selected
→ WBP_SciFiDropdownOption / Image_BG_Selected

8번 아래 화살표
→ WBP_SciFiDropdown / Image_Arrow

9번 컨트롤 외곽 프레임
→ WBP_SciFiDropdown / Image_Frame
→ WBP_SciFiToggle / Image_Frame

10번 토글 선택 채움
→ WBP_SciFiToggle / Image_SelectedFill

11번 슬라이더 외곽 프레임
→ WBP_SciFiSlider / Image_SliderFrame

12번 슬라이더 채움 바
→ WBP_SciFiSlider / Image_SliderFill

13번 슬라이더 값 박스
→ WBP_SciFiSlider / Image_ValueBox

14번 스크롤 레일
→ WBP_SciFiScrollBar / Image_Rail

15번 스크롤 손잡이
→ WBP_SciFiScrollBar / Image_Handle

16번 R키
→ WBP_KeyHint / Image_KeyIcon

17번 ESC키
→ WBP_KeyHint / Image_KeyIcon

18번 A키
→ WBP_KeyHint / Image_KeyIcon
```

---

# 23. Designer에서 반드시 체크할 것

## 23.1 장식용 이미지는 클릭을 막지 않게 한다

아래 요소는 전부 다음 값으로 둡니다.

```text
Visibility: Not Hit-Testable (Self & All Children)
구버전 표기: Hit Test Invisible
```

대상:

```text
Image_Background
Image_FolderPanel
Image_SeparatorLine
Image_Rail
Image_Handle
Image_KeyIcon
클릭 대상이 아닌 TextBlock
드롭다운 배경 이미지
토글 프레임 이미지
슬라이더 프레임 이미지
```

---

## 23.2 클릭 가능한 것은 Visible로 둔다

아래 요소만 입력을 받습니다.

```text
WBP_SettingsTabButton / Button_Hit
WBP_SciFiDropdown / Button_Hit
WBP_SciFiDropdownOption / Button_Hit
WBP_SciFiToggle / Button_Hit
WBP_SciFiSlider / Slider_Input
```

이 요소들은 반드시:

```text
Visibility: Visible
```

이어야 합니다.

---

## 23.3 Overlay에서는 Hierarchy 순서가 중요하다

예를 들어 드롭다운은 아래 순서가 좋습니다.

```text
Image_BG_Normal
Image_BG_Hover
Image_BG_Selected
Image_Frame
Text_SelectedValue
SizeBox_Arrow
Button_Hit
```

Button이 제일 위에 있어야 클릭이 잘 됩니다.  
Button은 투명하게 만들어 아래 이미지와 텍스트가 보이게 합니다.

---

## 23.4 CanvasPanel에서만 ZOrder를 설정한다

`CanvasPanel_UI`의 자식 ZOrder 추천값:

```text
Background: 0
FolderPanel: 1
Title: 5
Tabs / Pages: 10
ScrollBar: 15
Footer: 20
DropdownPopupLayer: 80
```

`Overlay` 안에서는 ZOrder를 찾지 말고 Hierarchy 순서로 조절합니다.

---

## 23.5 SizeBox는 위치를 정하지 않는다

`SizeBox`는 크기를 정하는 위젯입니다.  
위치는 부모 Slot에서 정합니다.

예:

```text
WBP_SciFiDropdown 자체 위치
→ WBP_SettingRow_Dropdown 안의 Canvas Panel Slot에서 Position X/Y 설정

WBP_SciFiDropdown 내부 크기
→ WBP_SciFiDropdown의 SizeBox_Root에서 Width/Height Override 설정
```

이렇게 나눠서 생각하면 Details 패널에서 없는 값을 찾는 문제가 줄어듭니다.

---

# 24. 제작 순서 추천

Designer만 기준으로 제작한다면 다음 순서가 가장 안정적입니다.

```text
1. 모든 텍스처 Import 및 UI용 설정
2. WBP_SettingsScreen 생성
3. 배경, ScaleBox, 기준 SizeBox, 폴더 패널 배치
4. WBP_SettingsTabButton 제작
5. 좌측 메뉴 4개 배치
6. WBP_SciFiDropdown 제작
7. WBP_SciFiToggle 제작
8. WBP_SciFiSlider 제작
9. WBP_SettingRow_Dropdown 제작
10. WBP_SettingRow_Toggle 제작
11. WBP_SettingRow_Slider 제작
12. WBP_SettingsPage_Graphics 제작
13. WBP_SettingsPage_Gameplay 제작
14. WBP_SettingsPage_Audio 제작
15. WBP_SettingsPage_Controls 제작
16. WidgetSwitcher 안에 페이지 4개 삽입
17. WBP_SciFiScrollBar 제작 및 배치
18. WBP_KeyHint 제작
19. WBP_KeyHintBar 제작 및 배치
20. CanvasPanel_DropdownPopupLayer 배치
```

Designer가 완성된 뒤 Graph 로직을 붙이면 구조가 훨씬 덜 꼬입니다.

