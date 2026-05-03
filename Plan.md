# 로비 HUD/스테이지 HUD 분리 + 캐릭터별 Overlay 적용 구현 상세 계획

작성일: 2026-05-02  
프로젝트: `DaeRune`  
문서 목적: 기존 Overlay 계획을 `HUD 컨텍스트 분리(로비/스테이지)` 요구사항까지 반영하여 실행 가능한 수준으로 전면 수정

---

## 1) 요구사항 재정의

최종 요구사항은 아래 2개를 동시에 만족해야 한다.

1. 로비에서는 로비 HUD 체계로 로비 Overlay를 사용해야 한다.
2. 스테이지에서는 스테이지 HUD 체계로 스테이지 Overlay를 사용해야 한다.

추가로 기존 요청(캐릭터별 Overlay 분리)을 유지하므로, 실제 구조는 다음의 2단 분기여야 한다.

1. 1차 분기: `어느 HUD 컨텍스트인가` (Lobby HUD / Stage HUD)
2. 2차 분기: `해당 HUD 안에서 어떤 캐릭터 클래스인가` (GardenRobot / VendingMachineRobot)

---

## 2) 현재 확인된 프로젝트 사실(로컬 코드/에셋 기준)

아래는 이미 프로젝트에 존재하는 자산과 코드 기반 사실이다.

1. HUD 블루프린트 존재
- `Content/Blueprints/UI/HUD/BP_DRLobbyHUD.uasset`
- `Content/Blueprints/UI/HUD/BP_DRStageHUD.uasset`
- `Content/Blueprints/UI/HUD/BP_DRTutorialHUD.uasset`

2. Overlay 블루프린트 존재
- `Content/Blueprints/UI/Overlay/WBP_LobbyOverlay.uasset`
- `Content/Blueprints/UI/Overlay/WBP_StageOverlay.uasset`
- `Content/Blueprints/UI/Overlay/WBP_TutorialOverlay.uasset`
- `Content/Blueprints/UI/Overlay/WBP_WaitingRoomOverlay.uasset`

3. C++ HUD 베이스는 단일 클래스
- `ADRHUD`만 존재 (`Source/DaeRune/Public/UI/HUD/DRHUD.h`)
- 현재 `OverlayWidgetClass` 단일 프로퍼티로 `InitOverlay()`에서 바로 생성

4. Overlay 초기화 호출 경로
- `ADRCharacter::InitAbilityActorInfo()` -> `DRHUD->InitOverlay(...)`
- `ADRPlayerController::InitOverlayForFreeRoam()` -> `DRHUD->InitOverlay(...)`

5. 대기실(WaitingRoom) 전환 시 Overlay 제거 로직 존재
- `ADRPlayerController::ClientSetWaitingRoomView_Implementation()`에서 `DRHUD->RemoveOverlay()` 호출

즉, 자산 레벨에서는 로비/스테이지 분리 기반이 이미 있으나, C++ 초기화 로직은 “현재 HUD 인스턴스가 어떤 Overlay를 써야 하는지”를 더 정교하게 해석하지 않는다.

---

## 3) 목표 아키텍처 (결정 완료)

## 3.1 기본 원칙

1. HUD 컨텍스트 분리는 `HUD 클래스/블루프린트`가 책임진다.
2. 캐릭터별 분기는 `각 HUD 내부의 캐릭터별 Overlay 매핑`으로 처리한다.
3. 폴백은 “HUD별 기본 Overlay”를 사용한다.

## 3.2 목표 분기 구조

실행 시 Overlay 선택은 아래 순서로 결정된다.

1. 현재 플레이어가 가진 HUD 인스턴스 확인
- Lobby 맵이면 `BP_DRLobbyHUD` 인스턴스
- Stage 맵이면 `BP_DRStageHUD` 인스턴스

2. HUD 내부에서 캐릭터 클래스 확인
- `ADRPlayerState::SelectedPlayerClass` 우선
- 실패 시 `Pawn(ADRCharacter)::PlayerCharacterClass` 보조

3. HUD별 캐릭터 매핑에서 Overlay 클래스 조회
- 성공 시 해당 Overlay 사용
- 실패 시 HUD별 기본 `OverlayWidgetClass` 사용

이 구조면 “로비 Overlay와 스테이지 Overlay가 섞이지 않음”이 보장되고, 동시에 캐릭터별 변형도 수용 가능하다.

---

## 4) 설계 변경 상세

## 4.1 ADRHUD C++ 확장

대상 파일:
- `Source/DaeRune/Public/UI/HUD/DRHUD.h`
- `Source/DaeRune/Private/UI/HUD/DRHUD.cpp`

### 4.1.1 신규 프로퍼티

`ADRHUD`에 아래 프로퍼티를 추가한다.

1. 캐릭터별 Overlay 맵
```cpp
UPROPERTY(EditAnywhere, Category = "Overlay")
TMap<EPlayerCharacterClass, TSubclassOf<UDRUserWidget>> CharacterOverlayWidgetClasses;
```

2. 캐릭터 분기 활성화 스위치
```cpp
UPROPERTY(EditAnywhere, Category = "Overlay")
bool bUseCharacterSpecificOverlay = true;
```

설계 의도:
- 같은 `ADRHUD` 기반이라도 `BP_DRLobbyHUD`와 `BP_DRStageHUD`가 서로 다른 맵 값을 가질 수 있다.
- 결과적으로 “HUD 컨텍스트 분리 + 캐릭터 분기”가 동시에 가능하다.

### 4.1.2 신규 함수

1. 캐릭터 클래스 해석
```cpp
bool ResolvePlayerCharacterClass(APlayerState* PS, APlayerController* PC, EPlayerCharacterClass& OutClass) const;
```

2. 최종 Overlay 클래스 해석
```cpp
TSubclassOf<UDRUserWidget> ResolveOverlayWidgetClass(APlayerState* PS, APlayerController* PC) const;
```

### 4.1.3 해석 규칙(고정)

`ResolveOverlayWidgetClass()`는 반드시 아래 순서로 동작한다.

1. `bUseCharacterSpecificOverlay == true`일 때 캐릭터 클래스 해석 시도
2. 해석 성공 + 맵 키 존재 + 값 유효 -> 해당 클래스 반환
3. 실패 시 HUD 기본값 `OverlayWidgetClass` 반환
4. 기본값도 null이면 `checkf`로 즉시 오류 노출

### 4.1.4 InitOverlay 변경

현재 `InitOverlay()`의 다음 부분:
- `CreateWidget(..., OverlayWidgetClass)`

변경 후:
- `CreateWidget(..., ResolveOverlayWidgetClass(PS, PC))`

유지할 기존 동작:

1. Overlay 중복 생성 방지 (`if (OverlayWidget) return;`)
2. `OverlayWidgetControllerClass` check
3. `SetWidgetController` -> `BroadcastInitialValues` -> `BroadcastAbilityInfo` -> `AddToViewport` 순서

### 4.1.5 로그 정책

경고 로그는 아래 상황에 남긴다.

1. 캐릭터 클래스 해석 실패
2. 캐릭터 클래스는 찾았지만 맵 엔트리 없음
3. 맵 엔트리는 있으나 클래스 값 null
4. fallback으로 전환됨

로그 키 필드:
- HUD 이름
- PlayerState 이름
- 캐릭터 enum 값(int)
- 사용된 최종 Overlay 클래스명

---

## 4.2 HUD 블루프린트 세팅 전략

## 4.2.1 BP_DRLobbyHUD

1. 기본 `OverlayWidgetClass` = `WBP_LobbyOverlay`
2. `CharacterOverlayWidgetClasses`:
- GardenRobot -> `WBP_LobbyOverlay_GardenRobot` (있으면 지정)
- VendingMachineRobot -> `WBP_LobbyOverlay_VendingMachineRobot` (있으면 지정)
3. 캐릭터별 UI가 아직 없으면 맵을 비우고 기본값만 사용 (로비 공통 Overlay 유지)

## 4.2.2 BP_DRStageHUD

1. 기본 `OverlayWidgetClass` = `WBP_StageOverlay`
2. `CharacterOverlayWidgetClasses`:
- GardenRobot -> `WBP_StageOverlay_GardenRobot`
- VendingMachineRobot -> `WBP_StageOverlay_VendingMachineRobot`
3. 스테이지는 능력/자원/Phase 정보 비중이 크므로 캐릭터별 변형 우선 적용

## 4.2.3 BP_DRTutorialHUD

1. 이번 요구사항 범위 외
2. 기존 `WBP_TutorialOverlay` 유지
3. 단, C++ 공통 변경 영향 없는지 회귀 테스트는 수행

---

## 4.3 GameMode <-> HUD 연결 검증 계획

블루프린트 자산:
- `BP_DRLobbyGameMode`
- `BP_DRStageGameMode`

검증 항목:

1. `BP_DRLobbyGameMode`의 `HUDClass == BP_DRLobbyHUD`
2. `BP_DRStageGameMode`의 `HUDClass == BP_DRStageHUD`
3. Stage/Lobby 맵 World Settings 또는 GameMode Override가 위 BP를 참조하는지 확인

중요:
- 코드 수정만으로는 적용되지 않으며, 맵/게임모드 세팅이 정확해야 컨텍스트 분리가 실제로 동작한다.

---

## 4.4 상태별 Overlay 정책(명시)

아래는 상태별로 Overlay가 무엇이어야 하는지 고정 정책이다.

1. Lobby WaitingRoom
- Gameplay Overlay 없음
- WaitingRoom UI(`UDRWaitingRoomWidget`) 중심
- 필요 시 `WBP_WaitingRoomOverlay`는 별도 계층으로 취급

2. Lobby FreeRoam
- `BP_DRLobbyHUD` 기반 Overlay 생성
- 캐릭터 분기 활성 시 로비용 캐릭터별 Overlay 사용

3. Stage InGame
- `BP_DRStageHUD` 기반 Overlay 생성
- 캐릭터 분기 활성 시 스테이지용 캐릭터별 Overlay 사용

4. Tutorial
- 기존 튜토리얼 HUD/Overlay 유지

---

## 5) 구현 절차 (세부 단계)

### Step 1. C++ HUD 확장

1. `DRHUD.h`에 `CharacterOverlayWidgetClasses`, `bUseCharacterSpecificOverlay` 추가
2. `ResolvePlayerCharacterClass`, `ResolveOverlayWidgetClass` 선언 추가
3. `DRHUD.cpp`에 구현
4. `InitOverlay()`를 해석 함수 기반으로 교체
5. 로그 추가

완료 기준:
- 컴파일 성공
- 기존 단일 Overlay 경로(fallback) 동작 유지

### Step 2. HUD BP별 값 세팅

1. `BP_DRLobbyHUD` 기본 Overlay = `WBP_LobbyOverlay`
2. `BP_DRStageHUD` 기본 Overlay = `WBP_StageOverlay`
3. 필요 시 캐릭터별 Overlay BP 생성 후 맵 입력

완료 기준:
- Lobby/Stage에서 서로 다른 기본 Overlay가 표시됨

### Step 3. GameMode/HUD 연결 검증

1. `BP_DRLobbyGameMode`의 HUDClass 점검
2. `BP_DRStageGameMode`의 HUDClass 점검
3. LobbyMap/Stage1 맵 오버라이드 점검

완료 기준:
- 맵 전환 시 HUD 인스턴스 타입이 의도대로 바뀜

### Step 4. 전환 시나리오 점검

1. WaitingRoom 진입 시 Overlay 제거 유지
2. FreeRoam 진입 시 LobbyOverlay 생성
3. Stage 이동 후 StageOverlay 생성
4. ReturnToLobby 후 다시 LobbyOverlay 생성

완료 기준:
- 전환 과정에서 Overlay 혼선/중복/누락 없음

### Step 5. 캐릭터별 Overlay 검증

1. GardenRobot 로비/스테이지 각각 UI 확인
2. VendingMachineRobot 로비/스테이지 각각 UI 확인
3. 누락 시 fallback + 로그 확인

완료 기준:
- 클래스별 UI 분기 확인 + 안전한 폴백 확인

---

## 6) 테스트 매트릭스 (상세)

## 6.1 컨텍스트 분리 테스트

1. LobbyMap 접속
- 기대 HUD: `BP_DRLobbyHUD`
- 기대 Overlay: 로비 계열(`WBP_LobbyOverlay*`)

2. Stage1 이동
- 기대 HUD: `BP_DRStageHUD`
- 기대 Overlay: 스테이지 계열(`WBP_StageOverlay*`)

3. Stage 종료 후 Lobby 복귀
- 기대 HUD: 다시 `BP_DRLobbyHUD`
- 기대 Overlay: 로비 계열 재생성

## 6.2 캐릭터 분기 테스트

1. GardenRobot
- LobbyFreeRoam: 로비 Garden Overlay
- Stage: 스테이지 Garden Overlay

2. VendingMachineRobot
- LobbyFreeRoam: 로비 Vending Overlay
- Stage: 스테이지 Vending Overlay

## 6.3 상태 전환 테스트

1. WaitingRoom -> Transitioning -> FreeRoam
- 기대: WaitingRoom에서 Overlay 없음, FreeRoam에서 로비 Overlay 생성

2. Spectating 시작/종료
- 기대: `UpdateOverlayForSpectating()` 호출 시 크래시/이벤트 누수 없음

3. Seamless Travel
- 기대: 캐릭터 선택값 유지 + 해당 컨텍스트 Overlay 생성

## 6.4 폴백/오류 테스트

1. 캐릭터 맵 엔트리 제거
- 기대: HUD 기본 Overlay로 폴백
- 기대: Warning 로그 출력

2. HUD 기본 Overlay도 null
- 기대: checkf로 즉시 실패(설정 오류 조기 발견)

## 6.5 멀티플레이 테스트

1. Listen Server Host(Garden), Client(Vending)
- Host 로컬: 본인 클래스 + 현재 컨텍스트 Overlay
- Client 로컬: 본인 클래스 + 현재 컨텍스트 Overlay
- 서로의 UI가 섞이지 않음

2. 로비에서 클래스 변경 후 FreeRoam/Stage 진입
- 기대: 최신 선택 클래스 기준 Overlay 반영

---

## 7) 리스크 및 대응

### 리스크 1: HUDClass 세팅 불일치

증상:
- Lobby인데 StageOverlay가 뜨거나 그 반대

대응:
1. GameMode BP HUDClass 점검 체크리스트 필수화
2. PIE 시작 로그에 현재 HUD 클래스명 출력

### 리스크 2: OverlayWidgetController의 Stage 의존 로직

증상:
- LobbyOverlay에서 Stage용 delegate/데이터 접근 시 노이즈 발생 가능

대응:
1. 현재 코드도 null-check 기반이라 치명도는 낮음
2. 필요 시 `OverlayWidgetController`를 Lobby/Stage 파생으로 분리하는 2차 개선 태스크 등록

### 리스크 3: 캐릭터별 Overlay 자산 미완성

증상:
- 일부 클래스만 분기되고 일부는 공통 UI 표시

대응:
1. 의도적으로 fallback 허용
2. 누락 로그로 빠르게 식별

---

## 8) 롤백 계획

문제 발생 시 즉시 복구 절차:

1. `BP_DRLobbyHUD`, `BP_DRStageHUD`의 `CharacterOverlayWidgetClasses`를 비움
2. 각 HUD의 기본 `OverlayWidgetClass`만 사용
3. 필요하면 `bUseCharacterSpecificOverlay = false`로 강제

이 롤백은 코드 재배포 없이 BP 값만으로 가능하게 설계한다.

---

## 9) 완료 체크리스트

1. `DRHUD` C++에 캐릭터별 Overlay 맵/해석 함수가 추가되었는가
2. `InitOverlay()`가 `ResolveOverlayWidgetClass()`를 사용하도록 변경되었는가
3. `BP_DRLobbyHUD` 기본 Overlay가 로비 계열로 설정되었는가
4. `BP_DRStageHUD` 기본 Overlay가 스테이지 계열로 설정되었는가
5. Lobby/Stage GameMode HUDClass 연결이 올바른가
6. WaitingRoom/FreeRoam/Stage/복귀 시 Overlay 정책이 모두 맞는가
7. 캐릭터별 분기 성공 + 누락 폴백 로그 확인이 되었는가
8. 멀티플레이 Host/Client에서 로컬 Overlay가 각각 올바른가

---

## 10) 최종 요약

수정된 계획의 핵심은 “캐릭터 분기” 이전에 반드시 “HUD 컨텍스트 분리(로비 HUD vs 스테이지 HUD)”를 먼저 고정하는 것이다.  
즉, `BP_DRLobbyHUD`는 로비 Overlay 계열만, `BP_DRStageHUD`는 스테이지 Overlay 계열만 책임지게 하고, 그 안에서 캐릭터별 Overlay 분기를 적용한다.  
이렇게 하면 요구사항인 “로비는 로비 오버레이, 스테이지는 스테이지 오버레이”가 구조적으로 보장되며, 동시에 캐릭터별 UI 확장도 안전하게 달성할 수 있다.
