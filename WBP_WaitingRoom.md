4.7 WBP_WaitingRoom 블루프린트 위젯 상세 제작 가이드
UMG 프로퍼티 용어 정리

위젯 자체의 프로퍼티: Details 패널에서 위젯을 선택했을 때 보이는 것 (예: TextBlock의 Text, Font)
슬롯(Slot) 프로퍼티: 부모 컨테이너가 자식에게 부여하는 레이아웃 속성. 위젯을 선택하면 Details 패널 최상단의 Slot 섹션에 표시됨.
Canvas Panel의 자식 → Anchors, Offset Left/Top/Right/Bottom, Alignment, Size To Content
Vertical/Horizontal Box의 자식 → Padding, Size (Auto/Fill), Horizontal/Vertical Alignment
Overlay의 자식 → Padding, Horizontal/Vertical Alignment
위젯에 고정 크기를 지정하려면 → SizeBox로 감싸서 Width Override, Height Override 사용

중요: 이 가이드는 대기방의 배경, 슬롯 구조물, 플레이어 캐릭터가 모두 월드에 실제 스태틱 메시 / 스켈레탈 메시로 배치되어 있다는 전제로 작성한다.
즉, UMG는 화면 위에 얹히는 정보 UI만 담당한다.
플레이어 이름은 캐릭터 머리 위, Kick 버튼은 그 위, 캐릭터 선택 UI(◀ 클래스명 ▶)는 캐릭터 아래에 보이도록 만든다.
사진에 보이는 P1 / P2 / P3 / P4 같은 위치 표시는 위젯이 아니라 배경 메시 또는 월드 데칼/텍스트로 처리하는 것을 권장한다.

4.7.1 위젯 생성: WBP_PlayerSlot (서브위젯, 먼저 생성)
에디터에서 Content/Blueprints/UI/ 폴더 열기
우클릭 → User Interface → Widget Blueprint
부모 클래스: UserWidget (기본값 그대로)
이름: WBP_PlayerSlot

4.7.2 WBP_PlayerSlot 디자이너 탭: 위젯 트리
아래 순서대로 위젯을 배치한다. 들여쓰기는 부모-자식 관계를 나타냄.

[SizeBox_Root]  ◀ 루트 위젯: SizeBox
│   Details → Child Layout:
│     Width Override: 220  (체크박스 ON, 값 220)
│     Height Override: 320 (체크박스 ON, 값 320)
│   ※ 이 위젯의 가운데 영역은 비워 둔다. 실제 캐릭터 메시가 이 영역에 보이게 된다.
│
└── [Canvas_SlotRoot] Canvas Panel
    │
    ├── [VBox_HeadUI] VerticalBox
    │   │ Slot (Canvas Panel Slot):
    │   │   Anchors: Top Center
    │   │   Alignment: (0.5, 0.0)
    │   │   Position X: 110, Y: 0
    │   │   Size To Content: true
    │   │
    │   ├── [Btn_Kick] Button
    │   │   │ Slot (Vertical Box Slot):
    │   │   │   Size: Auto
    │   │   │   Horizontal Alignment: Center
    │   │   │   Padding: (0, 0, 0, 2)
    │   │   │ Details:
    │   │   │   Visibility: Collapsed
    │   │   │   Style → Normal:  Image = None, Tint = (0.60, 0.10, 0.10, 0.95)
    │   │   │   Style → Hovered: Image = None, Tint = (0.85, 0.18, 0.18, 1.00)
    │   │   │   Style → Pressed: Image = None, Tint = (0.40, 0.05, 0.05, 1.00)
    │   │   │
    │   │   └── [Txt_Kick] TextBlock
    │   │         Details:
    │   │           Text: "Kick"
    │   │           Font: Font Family = Roboto, Typeface = Bold, Size = 12
    │   │           Color and Opacity: (1, 1, 1, 1) 흰색
    │   │           Justification: Center
    │   │
    │   └── [Txt_PlayerName] TextBlock
    │         Slot (VBox Slot): Size=Auto, H-Align=Center
    │         Details:
    │           Text: "Player"
    │           Font: Roboto, Bold, Size=16
    │           Color and Opacity: (1, 1, 1, 1)
    │           Shadow Offset: (1, 1)
    │           Shadow Color and Opacity: (0, 0, 0, 0.8)
    │           Justification: Center
    │
    └── [HBox_ClassSelect] HorizontalBox
        │ Slot (Canvas Panel Slot):
        │   Anchors: Bottom Center
        │   Alignment: (0.5, 1.0)
        │   Position X: 110, Y: 320
        │   Size To Content: true
        │
        ├── [Btn_PrevClass] Button
        │   │ Slot (Horizontal Box Slot): Size=Auto, V-Align=Center
        │   │ Details:
        │   │   Visibility: Collapsed
        │   │   Style → Normal:  Tint=(0.12, 0.12, 0.12, 0.85)
        │   │   Style → Hovered: Tint=(0.22, 0.22, 0.22, 1.00)
        │   │   Style → Pressed: Tint=(0.08, 0.08, 0.08, 1.00)
        │   │
        │   └── [Txt_PrevArrow] TextBlock
        │         Details:
        │           Text: "◀"
        │           Font: Roboto, Bold, Size=18
        │           Color and Opacity: (1, 1, 1, 1)
        │
        ├── [SizeBox_ClassName] SizeBox
        │   │ Slot (Horizontal Box Slot): Size=Auto, V-Align=Center, Padding=(10,0,10,0)
        │   │ Details:
        │   │   Min Desired Width: 체크박스 ON, 값 120
        │   │
        │   └── [Txt_ClassName] TextBlock
        │         Details:
        │           Text: "Elementalist"
        │           Font: Roboto, Bold, Size=15
        │           Color and Opacity: (1, 1, 1, 1)
        │           Shadow Offset: (1, 1)
        │           Shadow Color and Opacity: (0, 0, 0, 0.8)
        │           Justification: Center
        │
        └── [Btn_NextClass] Button
            │ Slot (Horizontal Box Slot): Size=Auto, V-Align=Center
            │ Details:
            │   Visibility: Collapsed
            │   Style: (Btn_PrevClass와 동일)
            │
            └── [Txt_NextArrow] TextBlock
                  Details:
                    Text: "▶"
                    Font: Roboto, Bold, Size=18
                    Color and Opacity: (1, 1, 1, 1)

IsVariable 체크할 위젯들 (Details 패널에서 위젯 이름 옆 눈 아이콘 클릭):

Btn_Kick, Txt_PlayerName, Txt_ClassName, Btn_PrevClass, Btn_NextClass

4.7.3 WBP_PlayerSlot 변수
이벤트 그래프 좌측 My Blueprint 패널 → Variables 섹션에서 추가:

변수명	타입	Instance Editable	기본값	용도
SlotIndex	Integer	✓	0	이 슬롯의 인덱스 (0~3)
bIsOccupied	Boolean	✗	false	플레이어가 배정되었는지
CachedPlayerState	Player State (Object Reference)	✗	None	Kick 시 식별용 캐시

4.7.4 WBP_PlayerSlot 이벤트 그래프
(A) Custom Event: UpdateSlot
이벤트 그래프에서 우클릭 → Add Custom Event → 이름: UpdateSlot

입력 파라미터 추가 (Details 패널 → Inputs):

Info : 타입 = Waiting Room Player Info (구조체)
bShowKick : 타입 = Boolean
bIsMySlot : 타입 = Boolean

[UpdateSlot] (Info, bShowKick, bIsMySlot)
│
├── Set Visibility (Self) = Visible   ← 이 슬롯에 플레이어가 들어오면 다시 표시
├── SET bIsOccupied = true
├── SET CachedPlayerState = [Break WaitingRoomPlayerInfo → OwningPlayerState]
│
├── Txt_PlayerName → Set Text
│     Text = [Break WaitingRoomPlayerInfo → PlayerName]
│
├── Txt_ClassName → Set Text
│     Text = [SELECT on SelectedClass]
│            Elementalist → "Elementalist"
│            Warrior      → "Warrior"
│            Ranger       → "Ranger"
│     ※ 방법: Break WaitingRoomPlayerInfo → SelectedClass 핀을
│       Select 노드 (Enum)의 Index에 연결.
│       각 옵션에 문자열을 직접 입력한다.
│
├── Btn_Kick → Set Visibility
│     분기: [bShowKick] AND [NOT bIsMySlot]
│     → True: ESlateVisibility::Visible
│     → False: ESlateVisibility::Collapsed
│     ※ 호스트 화면에서만 다른 플레이어 머리 위 Kick 버튼이 보이게 된다.
│
├── Btn_PrevClass → Set Visibility
│     [bIsMySlot] ? Visible : Collapsed
│
├── Btn_NextClass → Set Visibility
│     [bIsMySlot] ? Visible : Collapsed
│
├── Txt_PlayerName → Set Color and Opacity
│     [bIsMySlot]
│     → True:  (1.0, 1.0, 1.0, 1.0)
│     → False: (0.86, 0.86, 0.86, 1.0)
│
└── Txt_ClassName → Set Color and Opacity
      [bIsMySlot]
      → True:  (1.0, 0.95, 0.75, 1.0)  ← 자기 슬롯은 살짝 강조
      → False: (1.0, 1.0, 1.0, 1.0)
      ※ 기존처럼 큰 프레임을 두르지 않고, 텍스트 강조만 준다.

(B) Custom Event: ClearSlot
이벤트 그래프에서 우클릭 → Add Custom Event → 이름: ClearSlot (입력 파라미터 없음)

[ClearSlot]
│
├── SET bIsOccupied = false
├── SET CachedPlayerState = None  ← 빈 Object Reference 할당 (핀 우클릭 → Clear)
│
├── Txt_PlayerName → Set Text("")
├── Txt_ClassName → Set Text("")
│
├── Btn_Kick → Set Visibility(Collapsed)
├── Btn_PrevClass → Set Visibility(Collapsed)
├── Btn_NextClass → Set Visibility(Collapsed)
│
└── Set Visibility (Self) = Collapsed
      ※ 빈 슬롯은 아예 숨긴다. 사진처럼 빈 자리는 월드의 구조물만 남고,
        플레이어 UI는 표시되지 않는다.

(C) Btn_Kick → OnClicked 이벤트
디자이너에서 Btn_Kick 선택 → Details → Events → On Clicked 옆 + 클릭

[On Clicked (Btn_Kick)]
│
├── Is Valid (CachedPlayerState) → Is Not Valid 핀 → Return
│
├── Get Owning Player → Get Player Controller → Cast to DRPlayerController
│
└── [Cast 성공] → Server Request Kick Player (Target=CastResult, TargetPlayerState=CachedPlayerState)

(D) Btn_PrevClass → OnClicked 이벤트
디자이너에서 Btn_PrevClass 선택 → Details → Events → On Clicked 옆 + 클릭

[On Clicked (Btn_PrevClass)]
│
└── Get Owning Player → Get Player Controller → Cast to DRPlayerController
    └── [Cast 성공] → Request Change Class (Target=CastResult, bNext=false)

(E) Btn_NextClass → OnClicked 이벤트
[On Clicked (Btn_NextClass)]
│
└── Get Owning Player → Get Player Controller → Cast to DRPlayerController
    └── [Cast 성공] → Request Change Class (Target=CastResult, bNext=true)

주의: RequestChangeClass는 내부적으로 ServerRequestChangeClass Server RPC를 호출하므로 블루프린트에서 직접 ServerRequestChangeClass를 호출하지 않아도 됨.
캐릭터 순환 순서는 다음과 같다.
→ Elementalist → Warrior → Ranger → Elementalist → ...
← Elementalist ← Warrior ← Ranger ← Elementalist ← ...

4.7.5 위젯 생성: WBP_WaitingRoom (메인 위젯)
Content/Blueprints/UI/ 폴더에서 우클릭 → User Interface → Widget Blueprint
부모 클래스: DRWaitingRoomWidget 검색 후 선택
이름: WBP_WaitingRoom

4.7.6 WBP_WaitingRoom 디자이너 탭: 위젯 트리
[CanvasPanel] (루트, 자동 생성됨)
│
├── [PlayerSlot_0] WBP_PlayerSlot     ◀ IsVariable 체크
│   │ Slot (Canvas Panel Slot):
│   │   Anchors: Center
│   │   Alignment: (0.5, 0.5)
│   │   Position: (-360, -40)
│   │   Auto Size: true
│   │   ZOrder: 5
│   │   ※ 1920x1080 기준 예시. 왼쪽 첫 번째 캐릭터 위/아래에 오도록 조정.
│
├── [PlayerSlot_1] WBP_PlayerSlot     ◀ IsVariable 체크
│   │ Slot (Canvas Panel Slot):
│   │   Anchors: Center
│   │   Alignment: (0.5, 0.5)
│   │   Position: (-120, -30)
│   │   Auto Size: true
│   │   ZOrder: 5
│
├── [PlayerSlot_2] WBP_PlayerSlot     ◀ IsVariable 체크
│   │ Slot (Canvas Panel Slot):
│   │   Anchors: Center
│   │   Alignment: (0.5, 0.5)
│   │   Position: (120, -30)
│   │   Auto Size: true
│   │   ZOrder: 5
│
├── [PlayerSlot_3] WBP_PlayerSlot     ◀ IsVariable 체크
│   │ Slot (Canvas Panel Slot):
│   │   Anchors: Center
│   │   Alignment: (0.5, 0.5)
│   │   Position: (360, -40)
│   │   Auto Size: true
│   │   ZOrder: 5
│
└── [SizeBox_PowerOn] SizeBox
    │ Slot (Canvas Panel Slot):
    │   Anchors: Bottom Center
    │   Alignment: (0.5, 1.0)
    │   Position: (0, -28)
    │   Auto Size: false
    │   Size X: 260
    │   Size Y: 56
    │   ZOrder: 10
    │
    └── [Btn_PowerOn] Button          ◀ IsVariable 체크
        │ Details:
        │   Visibility: Collapsed  ← 기본. 호스트만 Visible로 변경됨
        │   Style → Normal:  Image = None, Tint = (0.10, 0.10, 0.10, 0.85)
        │   Style → Hovered: Image = None, Tint = (0.18, 0.18, 0.18, 0.95)
        │   Style → Pressed: Image = None, Tint = (0.05, 0.05, 0.05, 1.00)
        │
        └── [Txt_PowerOn] TextBlock
              Details:
                Text: "Power On"
                Font: Roboto, Bold, Size=28
                Color and Opacity: (1, 1, 1, 1)
                Shadow Offset: (1, 1)
                Shadow Color and Opacity: (0, 0, 0, 0.35)
                Justification: Center

IsVariable 체크 요약 (이벤트 그래프에서 참조해야 하는 위젯들):

위젯 이름	타입	용도
Btn_PowerOn	Button	가시성 제어 + OnClicked 이벤트
PlayerSlot_0	WBP_PlayerSlot	슬롯 0 참조
PlayerSlot_1	WBP_PlayerSlot	슬롯 1 참조
PlayerSlot_2	WBP_PlayerSlot	슬롯 2 참조
PlayerSlot_3	WBP_PlayerSlot	슬롯 3 참조

4.7.7 WBP_WaitingRoom 변수
이벤트 그래프 → My Blueprint → Variables에서 추가:

변수명	타입	기본값	용도
bCachedIsHost	Boolean	false	SetIsHost에서 저장, RefreshPlayerSlots에서 사용
CachedPlayerCount	Integer	0	플레이어 수 변경 감지용 (폴링)
PlayerSlots	Array of WBP_PlayerSlot	—	4개 슬롯 참조 배열 (Construct에서 초기화)

4.7.8 WBP_WaitingRoom 이벤트 그래프
(A) Event Construct
[Event Construct]
│
│ ── 1단계: PlayerSlots 배열 초기화 ──
│
├── Make Array (WBP_PlayerSlot)
│     [0]=PlayerSlot_0, [1]=PlayerSlot_1, [2]=PlayerSlot_2, [3]=PlayerSlot_3
│     → SET PlayerSlots
│
│ ── 2단계: 시작 시 모든 슬롯 숨김 ──
│
├── For Each Loop (PlayerSlots)
│     └── Clear Slot
│
│ ── 3단계: 클래스 변경 델리게이트 구독 ──
│
├── Get World → Get Game State → Get Player Array
│     → For Each Loop
│       → Cast to DRPlayerState
│       → [Cast 성공] → 핀 드래그 → "Assign On Player Class Changed"
│           → [OnAnyPlayerClassChanged] (Custom Event 자동 생성됨)
│               └── Get Owning Player → Get Player Controller
│                   → Cast to DRPlayerController
│                   → Refresh Waiting Room UI
│
│ ── 4단계: 플레이어 수 변경 감지 타이머 ──
│
└── Set Timer by Function Name
      Function Name: "PollPlayerCount"
      Time: 1.0
      Looping: true

(B) Custom Event: PollPlayerCount
[PollPlayerCount]
│
├── Get World → Get Game State → Get Player Array → Num
│
├── [결과] ≠ CachedPlayerCount ?
│     Branch:
│     │
│     ├── True:
│     │   ├── SET CachedPlayerCount = [Num 결과]
│     │   └── Get Owning Player → Get Player Controller
│     │       → Cast to DRPlayerController
│     │       → Refresh Waiting Room UI
│     │
│     └── False: (아무것도 안 함)

(C) Event RefreshPlayerSlots (C++에서 호출되는 BlueprintImplementableEvent)
이벤트 그래프에서 우클릭 → "RefreshPlayerSlots" 검색하면 오버라이드 가능한 이벤트로 나타남.

[Event Refresh Player Slots] (Input: PlayerInfos - Array of WaitingRoomPlayerInfo)
│
│ ── 1단계: 채워진 슬롯 갱신 ──
│
├── For Each Loop (PlayerInfos)
│   │  Array Index 핀 사용
│   │
│   ├── Array Index < PlayerSlots.Num (Length) ? Branch
│   │     False → 무시 (4명 초과 방지)
│   │
│   └── True →
│       PlayerSlots → Get (Array Index)
│       → Update Slot (
│             Info = Array Element,
│             bShowKick = bCachedIsHost,
│             bIsMySlot = [Break WaitingRoomPlayerInfo → bIsLocalPlayer]
│         )
│
│ ── 2단계: 비어있는 슬롯 정리 ──
│
├── PlayerInfos → Num (Length) → 저장 (Local Variable: FilledCount)
│
├── For Loop (Integer)
│     First Index = FilledCount
│     Last Index = 3
│     │
│     └── PlayerSlots → Get (Index)
│         → Clear Slot
│
│ ── 완료 ──
│
└── (실행 완료)
      ※ 중요: PlayerInfos 배열은 항상 왼쪽부터 빈자리 없이 정렬된 상태로 전달된다고 가정한다.
        따라서 Kick 이후 Array Index 기준으로 다시 뿌려 주기만 해도
        남은 플레이어들이 자동으로 왼쪽으로 당겨진 배치처럼 보이게 된다.

(D) Event SetIsHost (C++ BlueprintImplementableEvent)
[Event Set Is Host] (Input: bIsHost - Boolean)
│
├── SET bCachedIsHost = bIsHost
│
├── Btn_PowerOn → Set Visibility
│     [bIsHost] ?
│     → True: ESlateVisibility::Visible
│     → False: ESlateVisibility::Collapsed
│
└── Get Owning Player → Get Player Controller → Cast to DRPlayerController
    └── [Cast 성공] → Refresh Waiting Room UI
      ※ 호스트 여부가 바뀌면 Kick 버튼 표시 여부도 다시 계산해야 하므로 한 번 더 갱신한다.

(E) Event OnLobbyStateChanged (C++ BlueprintImplementableEvent)
[Event On Lobby State Changed] (Input: NewState - ELobbyState)
│
└── Switch on ELobbyState (NewState):
    │
    ├── WaitingRoom: (아무것도 안 함)
    │
    ├── Transitioning:
    │   └── Btn_PowerOn → Set Is Enabled (false)
    │
    └── FreeRoam: (C++ DestroyWaitingRoomUI()가 처리하므로 도달 안 함)

(F) Btn_PowerOn → OnClicked
디자이너에서 Btn_PowerOn 선택 → Details → Events → On Clicked 옆 + 클릭

[On Clicked (Btn_PowerOn)]
│
├── Btn_PowerOn → Set Is Enabled (false)   ← 중복 클릭 방지
│
└── Get Owning Player → Get Player Controller → Cast to DRPlayerController
    └── [Cast 성공] → Server Request Power On

4.7.9 스타일/비주얼 참고
요소	에디터 설정 위치	값
전체 배경	없음 (3D 대기실 장면이 배경)	—
플레이어 슬롯 프레임	사용하지 않음	투명 UI만 유지
빈 슬롯 표시	ClearSlot 이벤트에서 Self Collapsed	빈 자리는 구조물/배경만 보임
자기 슬롯 강조	Txt_ClassName 색상 동적 변경	(1.0, 0.95, 0.75, 1.0)
Kick 버튼	Btn_Kick → Style → Normal/Hovered/Pressed → Tint	작은 빨간 계열 버튼
플레이어 이름	Txt_PlayerName → Font	Roboto Bold 16, 흰색 + 그림자
클래스 이름	Txt_ClassName → Font	Roboto Bold 15, 흰색
Power On 버튼	Btn_PowerOn → Style	짙은 회색 반투명 버튼
Power On 텍스트	Txt_PowerOn → Font	Roboto Bold 28
슬롯 위치	각 PlayerSlot의 Canvas Slot → Position	카메라 구도에 맞게 직접 조정

4.7.10 BP_DRPlayerController에 위젯 클래스 설정
Content Browser에서 BP_DRPlayerController 더블클릭하여 열기
상단 툴바의 Class Defaults 버튼 클릭
Details 패널에서 UI|Lobby 카테고리 검색
Waiting Room Widget Class 드롭다운 → WBP_WaitingRoom 선택
Compile → Save

4.7.11 체크리스트
WBP_PlayerSlot:

[ ] SizeBox_Root 생성 (Width=220, Height=320)
[ ] Canvas_SlotRoot 생성
[ ] Btn_Kick: 기본 Collapsed, 작은 빨간 스타일, OnClicked 이벤트 연결
[ ] Txt_PlayerName: 머리 위 표시용 텍스트 설정
[ ] Btn_PrevClass, Btn_NextClass: 기본 Collapsed, OnClicked → RequestChangeClass
[ ] Txt_ClassName: Elementalist / Warrior / Ranger 표시
[ ] Custom Event UpdateSlot 구현 (3개 파라미터)
[ ] Custom Event ClearSlot 구현

WBP_WaitingRoom:

[ ] Canvas 루트에 4개 PlayerSlot 배치, 각각 IsVariable 체크
[ ] 각 PlayerSlot Canvas Position을 대기실 카메라 구도에 맞게 조정
[ ] Btn_PowerOn: 하단 중앙 배치, 기본 Collapsed
[ ] Event Construct: PlayerSlots 배열 초기화 + 초기 Clear + 델리게이트 바인딩 + 타이머
[ ] Event RefreshPlayerSlots: For Each로 슬롯 갱신 + 빈 슬롯 Clear
[ ] Event SetIsHost: bCachedIsHost 저장 + PowerOn 가시성 + UI 재갱신
[ ] Event OnLobbyStateChanged: Transitioning 시 PowerOn 비활성화
[ ] Btn_PowerOn OnClicked: ServerRequestPowerOn 호출
[ ] PollPlayerCount: 1초 타이머로 인원수 변경 감지

에디터 설정:

[ ] BP_DRPlayerController → WaitingRoomWidgetClass = WBP_WaitingRoom
[ ] PIE 2인 테스트: 호스트/클라이언트 양쪽 UI 확인
[ ] 호스트 화면에서만 다른 플레이어 위 Kick 버튼 표시 확인
[ ] 일반 플레이어 화면에서는 Kick 버튼이 전혀 보이지 않는지 확인
[ ] 자신의 슬롯에서만 ◀ / ▶ 버튼이 표시되고 클릭 가능한지 확인
[ ] 클래스 변경 시 Elementalist → Warrior → Ranger 순환 확인
[ ] Kick 실행 후 남은 플레이어 UI가 왼쪽부터 재배치되는지 확인
[ ] Power On 버튼이 호스트에게만 보이는지 확인

검증 방법
PIE 2인 또는 3인 플레이. 대기실 UI 표시. 호스트에게만 Kick / Power On 버튼 보임.
각 플레이어는 자기 슬롯에서만 캐릭터를 변경할 수 있어야 하며, 다른 플레이어 슬롯의 클래스명은 읽기 전용이어야 한다.
Kick 실행 시 대상 플레이어는 메인 메뉴로 이동하고, 남은 플레이어들의 슬롯 UI가 빈 자리 없이 왼쪽으로 정렬되어야 한다.
