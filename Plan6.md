# Plan6 — 스테이지2 (6개 방 진행형 스테이지) 설계 및 구현 계획

> **범위**: 방 1~6으로 구성된 스테이지2의 전체 게임플레이 흐름(방별 전투/퍼즐/방어/웨이브/열차 탈출)을 구현하기 위한 ① 현재 코드 상세 분석, ② "스테이지 1개 + 동일 패턴" 가정 제거 리팩토링, ③ 신규 페이즈/액터 시스템 설계, ④ 에셋·레벨 작업 목록, ⑤ 구현 순서와 테스트 계획.
> **선행 문서**: Plan3 §5 (로봇청소기 탑승/마운트 — 열차 좌석 시스템이 이 선례를 미러링), CLAUDE.md (페이즈/GAS/멀티플레이 아키텍처).
> **작성일**: 2026-07-23. 코드 실측 기반 (아래 §2). 대상 브랜치: `feat/PlayExpo`.
> **갱신 (2026-07-23)**: §11 미확정 사항에 대한 답변 반영 — 확정 5건(§11.A), 미정 유지 7건(§11.B). 미정 항목은 전부 BP/프레임워크로 격리되어 논블로킹이며, 구조에 영향을 주는 §11-9(설치대 체력)만 M4 착수 전 결정 게이트로 관리.
> **갱신 (2026-08-06)**: ① **방1 상세 사양 확정 → §14.1 신설 (방1의 SSOT)**. 인원별 스폰 룩업 테이블, 웨이브 2개(30초 간격), 시작지점 봉쇄 방식이 "물체 상승 차단물", 방1→방2 이동이 "문 모델 발광 + 근접 순간이동"으로 확정되어 §0/§1.1/§1.2/§4.1/§4.3/§4.5/§4.11/§4.12/§5/§6/§7.2/§8/§9/§10.1/§11/§12 동반 수정. ② 선행 답변 반영 — 부품 **1개**(§11-2 종결), 설치대 체력 **미도입**(§11-9 종결 → 적 AI 타겟팅 작업 전량 삭제), 선로 **ㄷ자 + 코너 곡선 일부·등속 주행**(§4.9.1), 방4 클리어 시 **사망자 50% 체력 부활 도입**(신규 요구 → §5.9, §11.B-13). ③ 방3~방6 상세 사양은 순차 설명 예정이며 확정될 때마다 §14.3~§14.6에 추가하고 관련 §4 설계를 갱신한다. **§14가 방별 요구사항의 최신 기준이며, §1·§4의 방별 서술이 §14와 충돌하면 §14가 우선한다.**
> **갱신 (2026-08-07, 4차)**: **§9 마일스톤 재분배 + §15 구현 상세 신설.** ① §9를 6개 방 확정 사양 기준으로 재작성 — 방2(M3)·방3+4(M4)가 단일 최대 작업이라 각각 3개 서브 단계로 분할하고, 의존 그래프·크리티컬 패스(M1→M3→M8)·규모(S~XL)·회귀 체크 3지점·별도 트랙 2개(아트, 보스 스킬)를 명시. ② **§15 "어떻게 만드는가" 신설** — 마일스톤 순서대로 실제 시그니처·알고리즘·검증 방법을 기록한다. 공통 규약(§15.0), 공유 코드 수정 4건의 before/after(§15.1), 기반 타입·베이스·레지스트리(§15.2), 통로 액터 2종(§15.3), BP·맵 배선 절차(§15.4), 방1~방6 구현(§15.5~§15.9), 신규 파일 21개 생성 순서(§15.10). 소프트락 유발 알고리즘 3종(8퍼즐 셔플, 스위치 마스크 생성, CCTV 시퀀스)과 부활 역연산 12항목 체크리스트를 코드 수준으로 확정.
> **갱신 (2026-08-07, 3차)**: **방6 상세 사양 확정 → §14.6 전면 작성. 이로써 6개 방 전체 사양이 확정되었다(§14).** 4칸 열차에 상호작용 키로 탑승해 생존자 전원 착석 시 등속 출발, 장애물 3구간에서 **두더지 보스가 장애물을 부수며 등장**하고, **단일 개체의 체력이 67% → 34% → 처치로 이어지며**(§14.6.4), **최종 처치가 곧 스테이지2 클리어**다. 이에 따라 ① **§4.9.7 열차 페이즈 재작성**(임계 배열 `RetreatHealthRatios`, **Destroy 대신 비활성 보관**으로 체력 유지, 완료 조건 `bBossDefeated`), ② §4.9.5 장애물의 해제 타이밍이 "임계 도달" → **"보스 등장과 동시"** 로 변경되고 `PawnWall` 제거, ③ §4.9.6 배리어를 **전·후방 한 쌍(3개 → 6개)** 으로 확장, ④ **원안의 종점 도착 판정 폐기** — `Trigger_Destination`·`S2P5_Arrive`·도착 플랫폼 배치 삭제(§14.6.6), ⑤ §3.1 페이즈 매핑의 완료 조건 5개를 전부 확정값으로 갱신, ⑥ §0·§1.2·§7.2·§8·§9·§10.1·§11·§12 동반 갱신. 보스의 스킬·패턴은 별도 작업으로 격리(§14.6.7).
> **갱신 (2026-08-07, 2차)**: **방5 상세 사양 확정 → §14.5 신설**. 전원 입장 시 **D4 구조물이 다시 올라와 봉쇄**(왕복 동작), 인원별 3웨이브(§14.5.2 — 총 9/11/16/20마리), 웨이브 전환은 **`min(30초 경과, 전원 전멸)` 하이브리드**(§14.5.3 — 방1의 시간 고정, 방3의 시간 고정 무한 반복과 구분), 웨이브3 전멸 시 **D5 구조물 하강 개방**. 이에 따라 ① **§4.8 방5 페이즈 재작성**(타이머 + 전멸 시 타이머 취소·즉시 스폰, 마지막 웨이브 타이머 미예약), ② §4.11 표에 D4 왕복·D5 추가 + 멱등 호출 규약, ③ **`ADRAutoSlidingDoor` 재사용이 0건으로 결론**(통로 8곳 전부 구조물/게이트) → §0·§1.1·§2.3·§6.2·§8에서 `BP_S2Door` 제거, ④ §7.2/§9/§10.1/§11 동반 갱신. **남은 미확정은 방6(열차) 상세뿐.**
> **갱신 (2026-08-07)**: **방3·방4 상세 사양 확정 → §14.3·§14.4 신설**. 방4는 부품 소지자 1명만 텔레포트로 입장(통과 즉시 잠김), **부품 설치가 홀로그램 두더지 게임과 방3 무한 웨이브(50초 주기)를 동시에 시작**, **두더지 20마리 클리어가 페이즈 완료 조건**(사망자 방3 부활 + D4 하강 개방 + 스폰 중단 + 잔적 즉시 사망). 이에 따라 ① **§4.7 방어 페이즈 전면 재작성**(완료 조건이 "잔적 전멸"→"두더지 클리어"), ② `ADRS2RisingBlocker` → **`ADRS2MovingBlocker`로 일반화**(D4가 하강 개방 구조라서 — §4.11), ③ 게이트에 `EntryRule`/`bDeactivateOnUse` 추가(§4.12), ④ **`ADRS2MoleGame`/`ADRS2Mole` 신설**(§4.7.1·§4.7.2) + **2m 근접 히트 게이트**를 `ApplyDamageEffect` 단일 관문에 추가(§5.11), ⑤ 부활 구체화(§5.9)·`Logout` 오버라이드 신규(§5.10)·`EjectInstalledPart` 신규(§5.12), ⑥ §1.1에 D2/D3/R4/D4 확정 반영, §6~§12 동반 갱신.
> **갱신 (2026-08-06, 2차)**: **방2 상세 사양 확정 → §14.2 신설**. 퍼즐 3종(8퍼즐 / 스위치 3라운드 / CCTV 카운팅)이 각각 금고 비밀번호 1자리를 담당하고, 금고 3자리 입력 → 부품 1개 → **부품 소지자 퇴장 시 생존자 전원 방1 회수 + D1 비활성화**로 페이즈 완료. 이에 따라 ① **§4.10 퍼즐 프레임워크 전면 교체**(`ADRS2PuzzleBase`/`PuzzleGroup` 폐기 → `ADRS2InteractProp` + 퍼즐 3종 + 금고 개별 클래스), ② `ADRS2TeleportGate`에 `TeamOnCarrier` 모드 추가(§4.12), ③ 방2 페이즈 완료 조건이 "부품 픽업"→"부품 소지자 퇴장"으로 변경(§4.6), ④ PlayerController 상호작용 프롭 배관 추가(§5.5-b), ⑤ §1.1에 출구 게이트 `E2` 행 추가, ⑥ §6/§7.2/§8/§9/§10.1/§11/§12 동반 갱신.

---

## 0. 결정사항 요약 ★

| 질문 | 결정 |
|---|---|
| 스테이지2를 어떤 단위로 쪼개나 | **페이즈 5개** — P0: 방1 전투, P1: 방2 퍼즐, P2: 방3+4 방어·설치(동시 진행이므로 한 페이즈), P3: 방5 3웨이브, P4: 방6 열차 탈출. 기존 `PhaseClasses` 배열 인프라(`DRStageGameMode.h:84-85`)에 그대로 플러그인 |
| GameMode를 새로 만드나 | **C++은 재사용, BP만 신규** (`BP_DRStage2GameMode`). `ADRStageGameMode`는 이미 페이즈 배열 데이터 주도 + `IsCompleted()` 위임 구조라서 클래스 분기 불필요. 단 `InitializePhaseSystem()`의 클렌저 사이트 개수 가드 1줄 완화 필요 (§5.1) |
| 방-액터 배선 방법 | **`ADRS2StageDirector` 단일 레지스트리 액터** (맵에 1개 배치, 문/트리거/퍼즐/설치대/열차를 `EditInstanceOnly` 참조로 보유). `ADRDoorManager`(스테이지1 전용 하드코딩, `DRDoorManager.h:32-53`)의 방식을 계승하되 스테이지2 전용으로 신설. 페이즈는 `TActorIterator`로 1회 탐색 후 캐시 |
| 방 사이 차단·이동 수단 | **신규 2종만 사용 — 통로 8곳 전부 확정** (§1.1): ① **`ADRS2MovingBlocker`**(신규, §4.11) 4곳 — 구조물이 올라와 막고/내려가 열린다 (D0·D2 상승 봉쇄, D4 하강 개방 후 재봉쇄, D5 하강 개방). ② **`ADRS2TeleportGate`**(신규, §4.12) 4곳 — 문 모델이 발광하고 근접 시 순간이동 (D1·E2·D3·R4). ★ 원안이 전제했던 **`ADRAutoSlidingDoor` 재사용은 결과적으로 0건**이며 `BP_S2Door` 제작도 불필요하다 (§14.5.4) |
| 인원 스케일링 방식 | **인원별 룩업 테이블** (배수 계산 아님) — 1~4인 각 구간의 웨이브 구성을 BP 배열로 직접 지정 (§4.1 `FS2WaveSet`). 기준 인원은 **방 시작 시점의 생존자 수로 1회 확정**하고 이후 사망해도 재계산하지 않는다 (§14.1.4). 방1 표는 §14.1.3 확정, 방3/방5는 대기 |
| "모든 플레이어 입장" 판정 | 신규 **`ADRS2RoomTrigger`** — 서버에서 박스 오버랩 인원 카운트를 `GameState->GetAlivePlayers()`(`DRGameStateBase.cpp:91-115`)와 비교, 사망/이탈 시 재평가. 델리게이트 `OnAllAlivePlayersInside` 발화 |
| 부품 획득/설치 | **`ADRCleanserPart` + `ADRCleanserSite` 전면 재사용**. 방4 설치대는 CleanserSite BP 자식 — 부품 개수는 §11-2 미정이지만 `RequiredPartsCount`가 EditDefaultsOnly(`DRCleanserSite.h:238-239`)라 **BP 값 변경만으로 대응** (개발 기본값 1). 방4 퍼즐 해결 전까지 `Inactive` 상태로 두면 `InstallPart()`의 상태 가드(`DRCleanserSite.cpp:214`)가 설치를 자연 차단 → 퍼즐 해결 시 `ActivateSite()` 호출로 개방 |
| 방3 무한 웨이브 | **신규 경량 스포너** (페이즈 내 타이머 + 순환 템플릿 + 동시 생존 상한). `UDRPhase3`은 시간 기반 웨이브 + DataTable + 독가스/엘리트 결합이 과해서 재사용하지 않음 (§4.6) |
| 방5 3웨이브 | 인원별 웨이브 배열(`FS2WaveSet` — §4.1, 웨이브 3개, 구성은 §14.5.2 확정). 전환 트리거는 **`min(30초 경과, 전원 전멸)` 하이브리드**(§14.5.3) — 방1(시간 고정)·방3(시간 고정 무한)과 다르다. `GameState`의 기존 `CurrentWaveNumber/TotalWaves` 복제 필드 재활용 (`DRStageGameState.h:303-309`) |
| 열차 이동 방식 | **스플라인 트랙 + "복제 1회, 로컬 시뮬" 이동** — `{상태, 시작거리, 목표거리, 시작서버시각, 속도}` 구조체 1회 복제 후 서버/클라 각자 동일 계산 (기존 `WaveTimerEndServerTime` 관례 미러, `DRStageGameState.h:314-316`). 탑승자는 좌석에 attach되어 자동 추종 |
| 열차 탑승 | **마운트 시스템 선례 미러** (`DRCharacter.h:108-138`의 `MountedOn` RepNotify attach 패턴) — 신규 `ADRS2TrainSeat`(IDRInteractable) + `ADRCharacter`에 좌석 상태 추가. F키 탑승 / 점프키 하차 (마운트와 동일 관례, `DRPlayerController.cpp:1102-1103`) |
| 열차 구간 보스 | **두더지 보스 1개체가 3구간에 반복 등장하며 체력이 이어진다** (§14.6.4) — 67% → 34% → 처치. `ADREnemy::OnHealthChanged / OnMaxHealthChanged`(BlueprintAssignable, `DREnemy.h:74-78`)에 페이즈가 바인딩해 비율을 추적하고, 임계 도달 시 **Destroy가 아니라 비활성 보관**(숨김 + 콜리전/AI 정지)으로 체력·디버프를 유지한 뒤 다음 구간에서 되살린다. 3구간에는 임계가 없고 **처치 = 스테이지2 클리어**. 보스의 스킬·패턴은 별도 작업(§14.6.7)이며 `MoleBossClass` BP 프로퍼티로 격리 |
| 하차 후 이동 제한 | 장애물 자체(전방 차단) + 신규 **`ADRS2Barrier`**(후방 차단, Pawn 전용 콜리전, 복제 토글) 조합. 정지 중에만 하차 가능하므로 "움직이는 발판 위 캐릭터" 네트워크 문제 자체가 발생하지 않음 ★핵심 단순화 |
| UI의 페이즈 인덱스 하드코딩 | **제거 필수** — `OverlayWidgetController.cpp:34, 459-475, 481-488`의 `PhaseIndex == 2` 비교는 스테이지2에서 방3 페이즈(인덱스 2)에 스테이지1 방어 UI(웨이브 타이머/클렌저 HP)를 띄우는 **실제 버그**를 유발. `GameState`에 복제 플래그 `bWaveDefenseUIActive` 신설로 대체 (§5.4). M1에서 선처리 |
| 페이즈 알람 텍스트 | `OverlayWidgetController.cpp:493-507`의 하드코딩 switch를 `FPhaseObjectiveData`에 `PhaseAlarmText` 컬럼 추가로 데이터 주도화 (기존 switch는 2페이즈 구조에서 인덱스1에 "수집"을 표시하는 **기존 버그**도 있음 — 데이터화로 함께 해소) |
| 신규 파일 인코딩 | **UTF-8 (BOM)** — 기존 파일 다수가 한글 주석 모지바케 상태. 신규 파일은 처음부터 UTF-8(BOM) 고정, 기존 파일 수정 시 주변 인코딩 보존 주의 |
| 미정 사양 처리 원칙 ★ | **6개 방 사양 확정 완료(2026-08-07, §14)** — 원안의 구조 영향 게이트(§11-9 설치대 체력)는 미도입으로 해소되어 결정 대기 항목이 없다. 남은 것은 값·에셋·연출뿐이며(잔여 확인 56건, §9.3 트랙 C) 전부 **BP 프로퍼티 / BP 이벤트 훅**으로 격리되어 논블로킹이다. 별도 작업으로 분리한 항목은 **두더지 보스의 스킬·패턴**(§14.6.7) 하나 |

---

## 1. 요구사항 명세 (사용자 설명의 정형화)

### 1.1 방 구조와 문

```
[시작지점] --D0(상승 차단물)--> [방1] --D1(텔레포트 게이트)--> [방2(퍼즐)]
                                 |
                                D2
                                 v
                               [방3] --D3--> [방4(퍼즐+설치대)]
                                 |
                                D4
                                 v
                               [방5] --D5--> [방6(열차역)] ==선로(ㄷ자)==> 장애물1 → 장애물2 → 장애물3 → [도착지점]
```

| ID | 통로 | 종류 | 초기 상태 | 차단/봉쇄 트리거 | 개방/활성 트리거 |
|---|---|---|---|---|---|
| D0 | 시작지점 → 방1 | **상승 차단물** `ADRS2MovingBlocker` ★확정 | 내려감 (통행 가능) | **전원 방1 입장 → 물체 상승, 영구 봉쇄** | (재개방 없음 — 시작지점 복귀 불가) |
| D1 | 방1 → 방2 | **텔레포트 게이트** `ADRS2TeleportGate` (Mode=`Individual`) ★확정 | 비활성 (발광 없음) | **부품 소지자의 방2 퇴장 → 비활성화**(재입장 불가, §14.2.7) | **방1 적 전멸 → 문 모델 발광, 근접 시 순간이동** (문이 열리지는 않음) |
| E2 | 방2 → 방1 (출구) | **텔레포트 게이트** `ADRS2TeleportGate` (Mode=`TeamOnCarrier`) ★확정 | 상시 활성 | — | 부품 미소지자는 개별 이동, **부품 소지자 통과 시 생존자 전원을 방1로 회수**(§14.2.7) |
| D2 | 방1 → 방3 | **상승 구조물** `ADRS2MovingBlocker` ★확정 | 열림 (통행 가능) | **전원 방3 입장 → 구조물 상승, 영구 봉쇄** | (재개방 없음 — 방1 복귀 불가) |
| D3 | 방3 → 방4 | **텔레포트 게이트** (`CarrierOnly` + `bDeactivateOnUse`) ★확정 | 비활성 | **부품 소지자 1명 통과 즉시 비활성** | 전원 방3 입장 시 발광. 두더지 클리어 시 재활성(복귀용). 방4 플레이어 사망 시 규칙을 `Anyone`으로 바꿔 재활성 (§14.3.5) |
| R4 | 방4 → 방3 (복귀) | **텔레포트 게이트** (`Individual`) ★확정 | 비활성 | — | 두더지 클리어 시 활성 (§14.3.6-5) |
| D4 | 방3 ↔ 방5 | **구조물** `ADRS2MovingBlocker` ★확정 | **막힘** | **전원 방5 입장 → 다시 상승 봉쇄**(방3 복귀 불가) | **두더지 20마리 클리어 → 구조물 하강, 개방** |
| D5 | 방5 → 방6 | **하강 개방 구조물** `ADRS2MovingBlocker` ★확정 | **막힘** | (재봉쇄 없음) | **방5 3웨이브 클리어 → 구조물 하강, 개방** |

> **통로 8곳 전부 확정**: 구조물 4곳(D0·D2·D4·D5) + 텔레포트 게이트 4곳(D1·E2·D3·R4). 원안이 재사용을 전제했던 `ADRAutoSlidingDoor`는 **결과적으로 스테이지2에서 쓰이지 않는다** (§14.5.4, §6.2).

### 1.2 방별 규칙

| 방 | 유형 | 시작 조건 | 진행 | 완료 조건 |
|---|---|---|---|---|
| 방1 ★확정 | 전투 (웨이브 2개) | 모든 생존 플레이어 입장 → **D0 차단물 상승·영구 봉쇄** | **웨이브1 즉시 → 30초 후 웨이브2** (전멸 여부 무관, 시간 기반). 구성은 **인원별 룩업 테이블**(§14.1.3) | **전 웨이브 전멸 → D1 게이트 발광** → 근접한 플레이어가 방2로 순간이동 (상세: **§14.1**) |
| 방2 ★확정 | 퍼즐 3종 + 금고 | **D1 게이트 발광 → 근접 순간이동으로 진입** (전원 대기 없음) | 8퍼즐(1번째 자리) / 스위치 3라운드(2번째 자리) / CCTV 이미지 개수 세기(3번째 자리) — **순서 무관·동시 진행** → 금고 물리 버튼으로 3자리 입력 → 개방 시 내부에 부품 1개 | **부품 소지자가 방2 출구 통과 → 생존자 전원 방1로 회수 + D1 비활성화** (상세: **§14.2**) |
| 방3 ★확정 | 무한 방어 | 모든 생존 플레이어(+부품) 입장 → **D2 구조물 상승 봉쇄** | 방4 문 발광(부품 소지자만 입장) → **부품 설치 시점부터 50초 주기 무한 웨이브**(인원별 구성 §14.3.2) | **두더지 클리어** → 사망자 방3 부활(체력 50%) + D4 하강 개방 + **스폰 중단 + 잔적 즉시 사망** (상세: **§14.3**) |
| 방4 ★확정 | 두더지 잡기 | **부품 소지자 1명만** D3로 입장(통과 즉시 잠김) | 중앙 설치대에 부품 설치 → **홀로그램 두더지 잡기** 시작. 2m 이내 근접 공격만 유효, 잡은 수에 따라 난이도 상승(§14.4.3) | **20마리 처치** (방3 완료 조건과 동일). 사망/접속종료 시 재시도 규칙 §14.3.5 (상세: **§14.4**) |
| 방5 ★확정 | 3웨이브 | 모든 생존 플레이어 입장 → **D4 구조물 재상승 봉쇄** | 웨이브1→2→3. 전환 트리거 = **30초 경과 또는 전원 전멸 중 먼저 오는 쪽**(하이브리드). 구성은 인원별 표 §14.5.2 | **웨이브3까지 전멸 → D5 구조물 하강 개방** (상세: **§14.5**) |
| 방6 ★확정 | 열차 탈출 | 4칸 열차에 각 칸 1명씩 상호작용 키로 탑승 → **생존자 전원 착석 시 출발** | 장애물 3회: 정지 → **두더지 보스가 장애물을 부수며 등장** → 전투 → 체력 **67% / 34%** 도달 시 도망 → 전원 재탑승 → 출발 | **3번째 구간에서 보스 처치 = 스테이지2 클리어** (도착지점 이동 없음 — 상세: **§14.6**) |

### 1.3 열차 구간 세부 규칙

- 열차 칸 수 = **4 고정** (§11-6 확정). 한 칸에 1명씩, 생존 인원 < 4면 **빈 칸 허용**. **생존자 전원 탑승 시 출발**.
- 장애물 도달 → 열차 정지 → 엘리트 몬스터 스폰.
- **엘리트 HP가 임계값(%) 이하** → 길이 열리고(장애물 해제) **전원(생존자) 재탑승 후** 열차 재출발 (§11-5 확정).
- **하차는 엘리트 전투 구간(정지 중)에만 가능**. 하차 후 이동 가능 범위 = 직전 장애물 ~ 현재 장애물 사이 구간.
- 3번째 장애물 통과 후 종점 도착 → 하차 → 전원 도착지점 진입 시 클리어.

---

## 2. 현재 코드 분석 (실측)

### 2.1 페이즈 시스템 골격 — 재사용 가능한 확장 포인트

| 요소 | 위치 | 스테이지2 관점 평가 |
|---|---|---|
| `PhaseClasses` 배열 (BP에서 페이즈 구성) | `DRStageGameMode.h:84-85` | ★ 그대로 사용. `BP_DRStage2GameMode`에 스테이지2 페이즈 5개를 배열로 지정하면 끝 |
| `UDRPhaseBase::IsCompleted()` 가상 함수 위임 | `DRPhaseBase.h:55-57`, `DRStageGameMode.cpp:354-370` | ★ 인덱스 switch가 이미 제거되어 있어(주석 명시) 새 페이즈가 스스로 완료 판정 가능 |
| `StartPhase → OnPhaseEnd → OnPhaseStart` 전환 체인 | `DRStageGameMode.cpp:265-333` | ★ 재사용. `EndCurrentPhase()`가 `Multicast_ObjectiveCompleted`(클리어 연출)까지 자동 발화 |
| 마지막 페이즈 완료 → `TriggerGameClear()` | `DRStageGameMode.cpp:343-348` | ★ 재사용. 5번째 페이즈 완료 시 자동 게임 클리어 → 로비 복귀 |
| `SpawnedEnemies` 추적 + `OnPhaseEnd` 일괄 정리 | `DRPhaseBase.cpp:30-52` | ★ 재사용. 전투/방어/웨이브 페이즈 공통 |
| `SetupPhaseObjective(int32)` — "Phase%d" 행 조회 | `DRPhaseBase.cpp:85-96` | △ 재사용하되, 페이즈 중간 서브 목표 교체용 `SetupPhaseObjectiveByRow(FName)` 추가 필요 (§5.2) |
| `PhaseObjectiveDataTable` (페이즈 BP별 EditDefaultsOnly) | `DRPhaseBase.h:116-117` | ★ 스테이지2 페이즈 BP들이 신규 `DT_S2PhaseObjective`를 지정하면 스테이지1과 완전 분리 |
| 목표 진행도 복제 (`UpdatePhaseObjectiveProgress`) | `DRStageGameState.h:164-176` | ★ 재사용 (입장 인원 n/N, 처치 수, 웨이브 i/3, 탑승 n/N 전부 이 채널로 표시) |
| 치트 `ServerCheatSkipToNextPhase` | `DRPlayerController.cpp:1416-1432` | ★ 재사용. Phase3 캐스트 분기 외에는 `TransitionToNextPhase()` 호출이라 스테이지2에서도 페이즈 스킵 동작 |
| 페이즈 시작/클리어/게임오버 사운드 Multicast | `DRPhaseBase.cpp:26-27`, `DRStageGameState.h:184-197` | ★ 자동 재사용 |
| 전멸(Wipeout)/게임오버/로비 복귀 | `DRStageGameMode.cpp:57-121, 178-194` | ★ 자동 재사용 |
| 진행 중 참가 차단 | `DRStageGameMode.cpp:163-176` | ★ 자동 재사용 (레이트 조인 걱정 없음) |
| 레벨 컨텍스트 판별 | `DRPlayerController.cpp:1245-1246` | ★ 이미 "Stage2가 생겨도 동작"하도록 GameState 타입 기반으로 수정되어 있음 |
| 로비 → 스테이지 이동 | `DRStageSelectActor.cpp:167` → `DRLobbyGameMode.cpp:213` | ★ 로비에 `DestinationMapName = "Stage2"` 포털 액터 1개 추가 배치로 연결 완료 |

### 2.2 "스테이지 1개 + 동일 패턴" 고정 가정 목록 (수정 대상) ★

| # | 가정/하드코딩 | 위치 | 영향 | 조치 |
|---|---|---|---|---|
| A1 | 클렌저 사이트가 1개 이상 없으면 페이즈 시스템 자체를 초기화하지 않음 | `DRStageGameMode.cpp:226` `if (CleanserSites.Num() < 1) return;` | 방4 설치대를 CleanserSite로 재사용하면 통과는 하지만, 구조적으로 "사이트 = 스테이지 필수"인 가정이 남음 | 가드를 경고 로그로 완화 (§5.1). 스테이지2는 방4 설치대에 `CleanserSite` 태그를 달아 어차피 통과 |
| A2 | 웨이브 타이머/클렌저HP UI를 **페이즈 인덱스==2**로 판단 | `OverlayWidgetController.cpp:34, 459-475, 481-488` | **스테이지2 방3 페이즈(인덱스2)에서 스테이지1 방어 UI가 뜨는 버그**. 또한 현 2페이즈 스테이지1에선 인덱스 2가 없어 이 경로 자체가 죽은 코드(잠재 버그) | `bWaveDefenseUIActive` 복제 플래그로 대체 (§5.4). `UDRPhase3`만 이 플래그를 켬 |
| A3 | 페이즈 알람 텍스트 switch (0=확보/1=수집/2=방어) | `OverlayWidgetController.cpp:493-507` | 스테이지2의 페이즈 이름과 불일치. 스테이지1에서도 인덱스1(방어)이 "수집"으로 표기되는 기존 버그 | `FPhaseObjectiveData::PhaseAlarmText` 컬럼 추가로 데이터 주도화 (§5.3) |
| A4 | `ADRDoorManager`가 스테이지1 맵의 방 구조를 멤버 이름 수준으로 하드코딩 | `DRDoorManager.h:32-53`, `DRPhase1.cpp:93-97` (`OnPhase1Ended`) | 스테이지2에서 재사용 불가 | 건드리지 않음(스테이지1 전용으로 유지). 스테이지2는 `ADRS2StageDirector` 신설 (§4.3) |
| A5 | `UDRPhase1`의 사이트 1개 강제 유지·나머지 파괴 | `DRPhase1.cpp:121-162` | 스테이지2 페이즈 목록에 Phase1이 없으므로 실행 안 됨 — 영향 없음 | 조치 불요 (확인만) |
| A6 | `UDRPhase3` 웨이브 = 시간 기반(PlayDuration/RestDuration) + 최대 100마리 초과 시 게임오버 | `DRPhase3.cpp:241-377`, `:1117-1126` | 방3(무한)·방5(전멸 연쇄) 요구와 모델이 다름 | 재사용 포기, 신규 경량 스포너 (§4.6, §4.7) |
| A7 | 관전/입력모드 등은 이미 레벨 컨텍스트 기반 | `DRPlayerController.cpp:1233-1249` | 없음 | 조치 불요 |
| A8 | `ServerRequestInteract`가 부품/사이트/마운트 3종만 디스패치 | `DRPlayerController.cpp:371-413` | 열차 좌석 상호작용 추가 필요 | 좌석 분기 추가 (§5.5) |

### 2.3 재사용 자산 인벤토리 (스테이지2 관점)

| 자산 | 위치 | 재사용 방식 |
|---|---|---|
| ~~자동문 (복제/잠금/수동 개폐)~~ | `DRAutoSlidingDoor.h` | ~~스테이지2 문 6개 전부~~ → **재사용 없음으로 결론** (2026-08-07). 통로 8곳이 모두 구조물(§4.11) 또는 텔레포트 게이트(§4.12)로 확정되었다 (§14.5.4). 단 **상태 복제 + 수동 제어 API 패턴 자체는 두 신규 클래스의 설계 참고 자료**로 유효 |
| 부품 픽업/운반/드롭 파이프라인 | `DRCleanserPart.h`, `DRCharacter.cpp:373-430` (`PickupPart/InstallCarriedPart/ForceDropCarriedPart`), `DRPlayerController.cpp:1095-1135` | 방2 부품에 그대로 사용. 사망 시 강제 드롭(`ForceDropCarriedPart`)도 이미 존재 → 방3 운반 중 사망 엣지 자동 처리 |
| 설치대 (상태 가드 + 설치 연출 + 델리게이트) | `DRCleanserSite.cpp:209-232`, `OnPartInstalled` (`DRCleanserSite.h:139-140`) | 방4 설치대. `Inactive→Active` 전환을 퍼즐 게이트로 활용 |
| 운반 중 스킬 차단 (`State.Carrying`) | `DRPlayerController.cpp:1397-1401`, CLAUDE.md | 방2→방3 부품 운반 시 자동 적용 |
| 적 스폰 + 사망 델리게이트 바인딩 패턴 | `DRPhase1.cpp:191-212` (`SpawnEnemy`) | 신규 페이즈 공통 베이스로 승격 (§4.4) |
| 적 사망 감지 | `ICombatInterface::GetOnDeathDelegate()` (사용례 `DRPhase1.cpp:205-208`) | 전 전투 페이즈 |
| 적 체력 변화 델리게이트 | `DREnemy.h:74-78` | 열차 엘리트 임계 감지 |
| 캐릭터 간 attach 탑승 (복제 검증 완료 선례) | `DRCharacter.h:108-138`, `DRRobotVacuumCharacter.h:49-55` | 열차 좌석 시스템의 설계 템플릿 (attach + RepNotify + 서버 검증 + 점프 하차) |
| "복제 1회 + 로컬 시뮬" 타이머 관례 | `DRStageGameState.h:119, 314-316` (`WaveTimerEndServerTime`) | 열차 이동 복제 모델의 근거 관례 |
| 서버시각 동기화 | `GetServerWorldTimeSeconds()` (`DRStageGameState.h:119` 사용례) | 열차 로컬 시뮬 기준 시각 |
| 압력판/발판 상호작용 선례 | `DRTutorialPressurePlate.h`, `DRTutorialStartTile.h` | 방2/방4 퍼즐 구현체 참고 (BP 콘텐츠) |
| 목표 UI 채널 (제목/진행도/클리어 연출) | `OverlayWidgetController.h:128-141` | 전 페이즈 목표 표시 |
| 생존 플레이어 조회 | `DRGameStateBase.cpp:91-115` | 전원 입장/전원 탑승 판정 |
| 사망 통지 | `ADRGameModeBase::OnPlayerDied` (`DRGameModeBase.h:37`) | 입장/탑승 조건 재평가 트리거 |
| 치트 (페이즈 스킵) | `DRPlayerController.cpp:1416-1432` | 개발 중 방 단위 점프 테스트 |

### 2.4 이미 존재하는 스테이지2 에셋 (git 미추적 신규)

- `Content/Maps/Stage2.umap` — 맵 파일 존재 (509KB)
- `Content/DaeRuneAssets/Map/Stage2_v1/` — 맵 전용 아트 에셋
- `Content/Blueprints/Character/Material/` — 신규 머티리얼 폴더

→ 레벨 지오메트리는 준비 중. 본 계획의 §8 배치 체크리스트가 이 맵 위에서 수행됨.

---

## 3. 전체 아키텍처

### 3.1 페이즈 매핑

**6개 방 전체 사양이 확정되었다 (§14).** 완료 조건은 §14의 확정 사양을 반영한 최신 값이다.

| 인덱스 | 클래스 | 담당 | 완료 조건 (`IsCompleted`) | SSOT |
|---|---|---|---|---|
| 0 | `UDRS2CombatPhase` | 방1 전투 | 전원 입장 후 **2웨이브 전멸** (`Alive==0 && Pending==0`) | §14.1 |
| 1 | `UDRS2PuzzlePhase` | 방2 퍼즐 3종 + 금고 | **부품 소지자가 방2 출구 통과 + 전원 방1 회수** (`bCarrierExited`) | §14.2 |
| 2 | `UDRS2DefensePhase` | 방3 무한 방어 + 방4 두더지 | **두더지 20마리 클리어** (`bMoleGameCleared`) — 잔적은 즉시 사망 처리 | §14.3·§14.4 |
| 3 | `UDRS2WavePhase` | 방5 3웨이브 | **마지막(3) 웨이브 전멸** | §14.5 |
| 4 | `UDRS2TrainPhase` | 방6 열차 탈출 | **3구간 두더지 보스 최종 처치** (`bBossDefeated`) — 도착지점 이동 없음 | §14.6 |

- 페이즈 5개 모두 `UDRS2PhaseBase`(신규, `UDRPhaseBase` 상속)를 공통 부모로 사용.
- 각 페이즈의 **입장 문 열기는 자기 `OnPhaseStart`에서** 수행한다 (예: 방2 페이즈 시작 시 D1 열기). 완료 측 `OnPhaseEnd`에서 열지 않는 이유: `TriggerGameOver()`도 `OnPhaseEnd`를 호출하므로 (`DRStageGameMode.cpp:66-69`) 게임오버 시 부수효과가 실행되는 경로가 생긴다.

### 3.2 페이즈 전환 시퀀스 (기존 인프라 위)

```
[페이즈 내부 이벤트: 적 전멸/설치/도착 등]
        │ GameMode->ValidatePhaseCompletion()      (DRPhase1.cpp:347 관례)
        ▼
IsCompleted() == true
        ▼
EndCurrentPhase()                                  (DRStageGameMode.cpp:310)
  ├─ SetCurrentPhaseState(Completed)
  ├─ Multicast_ObjectiveCompleted()  → 목표 클리어 연출 (전 클라)
  ├─ CurrentPhase->OnPhaseEnd()      → 타이머/적/바인딩 정리
  └─ TransitionToNextPhase()
        ├─ 다음 페이즈 있음 → StartPhase(N) → OnPhaseStart() → 입장 문 열기/목표 설정
        └─ 없음(P4 완료)   → TriggerGameClear() → 5초 후 로비 복귀
```

### 3.3 액터 배선 — `ADRS2StageDirector`

페이즈는 GameMode가 `NewObject`로 만드는 UObject라 레벨 액터를 `EditInstanceOnly`로 직접 참조할 수 없다 (스테이지1은 태그 문자열 스캔으로 해결 — `DRPhase3.h:203-207`). 스테이지2는 참조가 10개+로 많아 **타입 안전한 중앙 레지스트리**를 둔다:

- 맵에 1개 배치, `HasAuthority()`에서만 유효 (서버 전용 데이터, 복제 불필요 — 문/트리거 등 참조 대상이 각자 복제됨).
- 페이즈가 `UDRS2PhaseBase::GetDirector()`(TActorIterator 1회 탐색 + 캐시)로 접근.
- 배치 검증: `BeginPlay`에서 미할당 참조를 `UE_LOG` Error로 전부 나열 (레벨 배선 실수 조기 발견).

### 3.4 "전원 입장" 판정 설계

- 서버 전용 로직. `BeginOverlap/EndOverlap`에서 `ADRCharacter`만 카운트(Set 보관), 이벤트마다 `GetAlivePlayers()`와 대조.
- **사망 재평가**: 방 밖에서 1명이 죽으면 오버랩 이벤트가 안 옴 → `ADRGameModeBase::OnPlayerDied` 경로에서 GameMode가 현재 페이즈에 `NotifyPlayerDied()`를 전달하거나(§5.1 선택 변경), 트리거가 활성 상태 동안 0.5초 폴링 타이머로 재평가 (구현 단순성 우선 — **폴링 채택**, 트리거 활성 구간에만 돌므로 비용 무시 가능).
- 발화는 1회 래치 (`bFired`), 페이즈가 `Arm()/Disarm()`으로 수명 제어.
- 진행도 UI: 트리거가 `OnInsideCountChanged(n, total)`도 브로드캐스트 → 페이즈가 `UpdatePhaseObjectiveProgress(n)`로 "입장 n/N" 표시.

---

## 4. 신규 클래스 상세 설계

파일 배치: `Source/DaeRune/Public|Private/Phase/Stage2/`, `Source/DaeRune/Public|Private/Actor/Stage2/`.

### 4.1 공용 타입 — `DRS2Types.h`

> **2026-08-06 재설계**: 방1 사양이 "인원별로 웨이브 구성을 표로 직접 지정"(§14.1.3)으로 확정되어, 구 설계의 `FS2SpawnEntry`(스폰 지점 인덱스 + 개별 지연) / `FS2WaveDefinition`(엔트리 나열)을 폐기하고 **"적 종류 × 마리 수" 기반 3단 구조**로 교체한다. 스폰 지점은 엔트리마다 지정하지 않고 방별 `SpawnPoints` 배열에서 랜덤 배정한다(§14.1.5).

```cpp
// 적 1종 × 마리 수
USTRUCT(BlueprintType)
struct FS2EnemyCount
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TSubclassOf<ADREnemy> EnemyClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0"))
    int32 Count = 0;
};

// 웨이브 1개 구성
USTRUCT(BlueprintType)
struct FS2WaveComposition
{
    GENERATED_BODY()

    // 이 웨이브에 스폰할 적 (종류별 마리 수)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (TitleProperty = "EnemyClass"))
    TArray<FS2EnemyCount> Enemies;

    // 방 전투 시작 시점 기준 스폰 지연. 방1 = [0s, 30s] (§14.1.3)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float StartDelaySeconds = 0.f;

    // 같은 웨이브 안에서 마리 단위 스폰 간격 — "막 나오는" 연출용 (0 = 동시 스폰)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float PerEnemySpawnInterval = 0.f;
};

// 인원 1구간(1인/2인/3인/4인)의 웨이브 목록
USTRUCT(BlueprintType)
struct FS2WaveSet
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TArray<FS2WaveComposition> Waves;
};
```

**페이즈 BP에서의 사용 형태**와 조회 규칙:

```cpp
// 페이즈 프로퍼티: [0]=1인 … [3]=4인 (§14.1.3 표를 그대로 입력)
UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Spawn")
TArray<FS2WaveSet> WaveSetsByPlayerCount;

// 조회 (방 시작 시 1회):
const int32 Index = FMath::Clamp(BasePlayerCount - 1, 0, WaveSetsByPlayerCount.Num() - 1);
const FS2WaveSet& ActiveSet = WaveSetsByPlayerCount[Index];
// ※ 배열이 비었거나 인원 구간이 부족하면 Error 로그 + 마지막 항목 폴백 (§14.1.4)
```

- 이 3단 구조는 방1(§14.1) 확정 사양이고, 방3 무한 스포너(§4.7)와 방5 3웨이브(§4.8)도 인원별 구성이 확정되면 같은 타입을 재사용한다 (§14.3·§14.5 대기).

### 4.2 `ADRS2RoomTrigger` (Actor)

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllAlivePlayersInside);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInsideCountChanged, int32, InsideCount, int32, AliveTotal);

UCLASS()
class ADRS2RoomTrigger : public AActor
{
    // UBoxComponent* TriggerBox (루트, Pawn 오버랩만)
    // FName RoomID;                          // 배선 검증/로그용
    // void Arm(); void Disarm();             // 페이즈가 수명 제어 (서버)
    // bool AreAllAlivePlayersInside() const; // 즉시 질의 (부품 동반 검증 등에 사용)
    // bool IsActorInside(AActor*) const;     // 부품 액터 포함 판정용
    // FOnAllAlivePlayersInside OnAllInside;  // 1회 래치
    // FOnInsideCountChanged OnCountChanged;
private:
    // TSet<TWeakObjectPtr<ADRCharacter>> PlayersInside;
    // FTimerHandle ReevaluateTimer;          // Arm 동안 0.5s 재평가 (사망/리스폰 엣지)
    // bool bArmed, bFired;
};
```

- 오버랩 등록/해제 시 `PlayersInside` 갱신 → `Evaluate()`: `GetAlivePlayers()` 전원이 Set에 포함되고 수가 일치하면 발화.
- 도착지점 트리거도 이 클래스 재사용 (`RoomID = "Destination"`).

### 4.3 `ADRS2StageDirector` (Actor)

```cpp
UCLASS()
class ADRS2StageDirector : public AActor
{
public:
    // ===== 통로 (EditInstanceOnly) — 종류가 통로마다 다름 (§1.1) =====
    // ADRS2MovingBlocker* Blocker_StartToRoom1;  // D0 ★확정: 상승 봉쇄 (§4.11, §14.1.2)
    // ADRS2TeleportGate*  Gate_Room1ToRoom2;     // D1 ★확정: 게이트 Individual (§4.12, §14.1.6)
    // ADRS2TeleportGate*  Gate_Room2Exit;        // E2 ★확정: 게이트 TeamOnCarrier → 방1 회수 (§14.2.7)
    // ADRS2MovingBlocker* Blocker_Room1ToRoom3;  // D2 ★확정: 상승 봉쇄 (§14.3.3)
    // ADRS2TeleportGate*  Gate_Room3ToRoom4;     // D3 ★확정: CarrierOnly + bDeactivateOnUse (§14.3.3)
    // ADRS2TeleportGate*  Gate_Room4Return;      // 방4 → 방3 복귀 (클리어 시 활성 — §14.3.6-5)
    // ADRS2MovingBlocker* Blocker_Room3ToRoom5;  // D4 ★확정: bStartBlocked=true, 하강 개방 후 방5 입장 시 재봉쇄 (§14.5.4)
    // ADRS2MovingBlocker* Blocker_Room5ToRoom6;  // D5 ★확정: bStartBlocked=true, 3웨이브 클리어 시 하강 개방 (§14.5.4)

    // ===== 트리거 =====
    // ADRS2RoomTrigger* Trigger_Room1; Trigger_Room3; Trigger_Room5;
    //   ※ Trigger_Destination 폐기 — 방6은 보스 최종 처치가 곧 클리어라 도착지점 판정이 없다 (§14.6.6)

    // ===== 스폰 포인트 (빈 액터/TargetPoint 참조 배열) =====
    // TArray<AActor*> Room1SpawnPoints; Room3SpawnPoints; Room5SpawnPoints;
    //
    // ===== 방3/방4 부속 (§14.3·§14.4) =====
    // TArray<AActor*> Room3RevivePoints;       // 부활 지점 (§14.3.4)
    // AActor* Room4EntranceDropPoint;          // 접속 종료 시 부품을 놓을 방3 쪽 문 앞 (§14.3.5-B)
    // ADRS2MoleGame* Room4MoleGame;            // 두더지 게임 관리 액터 (§4.7.1)

    // ===== 방2 퍼즐 (§14.2 확정) =====
    // ADRS2SlidePuzzle*  Room2SlidePuzzle;     // 8퍼즐 (§4.10.2)
    // ADRS2SwitchPuzzle* Room2SwitchPuzzle;    // 스위치 퍼즐 (§4.10.3)
    // ADRS2CctvBoard*    Room2CctvBoard;       // CCTV 기믹 (§4.10.4)
    // ADRS2Safe*         Room2Safe;            // 금고 — 부품은 금고 내부에서 스폰 (§4.10.5)
    // ADRS2TeleportGate* Gate_Room2Exit;       // 방2 출구 (Mode = TeamOnCarrier, 목적지 = 방1) (§14.2.7)
    //
    // ===== 방4 퍼즐 (두더지 — §14.4 확정 대기) =====
    // (두더지 스포너/판정 액터 참조 예정)

    // ===== 설치대 =====
    // ADRCleanserSite* Room4InstallSite;       // BP_S2InstallStation (RequiredPartsCount=1)

    // ===== 열차 (§14.6) =====
    // ADRS2Train* Train;
    // ADRS2TrainTrack* Track;
    // TArray<ADRS2TrainObstacle*> Obstacles;      // 순서 = 진행 순서 (3개), 각자 BossSpawnPoint 보유
    // TArray<ADRS2Barrier*> ForwardBarriers;      // ★신규: 전방 차단 (장애물이 부서지므로 필요 — §14.6.5)
    // TArray<ADRS2Barrier*> RearBarriers;         // 후방 차단 (Obstacles와 동일 인덱스)

    // BeginPlay(HasAuthority): 널 참조 전수 검사 + Error 로그
};
```

### 4.4 `UDRS2PhaseBase` (UObject, `UDRPhaseBase` 상속)

```cpp
UCLASS(Abstract, Blueprintable)
class UDRS2PhaseBase : public UDRPhaseBase
{
protected:
    // ADRS2StageDirector* GetDirector();   // TActorIterator 1회 + 캐시 (DRPhase1::GetDoorManager 관례)
    // AActor* SpawnEnemyAt(TSubclassOf<ADREnemy>, const FTransform&);
    //   → DRPhase1.cpp:191-212 미러: 스폰 + GetOnDeathDelegate 바인딩 + SpawnedEnemies 추가
    //
    // ===== 인원별 웨이브 스폰 (§4.1 타입 사용) =====
    // int32 ResolveBasePlayerCount();      // 방 시작 시 1회: Clamp(GetAlivePlayers().Num(), 1, 4) → BasePlayerCount 확정 (§14.1.4)
    // const FS2WaveSet* ResolveWaveSet(const TArray<FS2WaveSet>&, int32 BasePlayerCount);   // clamp 폴백 + Error 로그
    // int32 CountTotalSpawns(const FS2WaveSet&);   // 목표 UI 분모 + PendingSpawnCount 초기값
    // void ScheduleWaveSet(const FS2WaveSet&, const TArray<AActor*>& Points);
    //   → 각 웨이브의 StartDelaySeconds로 타이머 예약, 웨이브 안에서는 PerEnemySpawnInterval 간격 순차 스폰
    // void SpawnComposition(const FS2WaveComposition&, const TArray<AActor*>& Points);
    //   → 종류별 Count를 마리 단위로 펼쳐 셔플 → 스폰 지점 랜덤 배정 → 1마리마다 --PendingSpawnCount
    //
    // ===== 통로 제어 래퍼 (널 가드 포함) =====
    // void OpenDoor(ADRAutoSlidingDoor*); void CloseAndLockDoor(ADRAutoSlidingDoor*);
    // void SetBlocked(ADRS2MovingBlocker*, bool);      // §4.11
    // void SetGateActive(ADRS2TeleportGate*, bool);    // §4.12
    //
    // int32 BasePlayerCount = 0;           // 방 시작 시점 생존자 수 (확정 후 불변 — §14.1.4)
    // int32 PendingSpawnCount = 0;         // 아직 스폰되지 않은 마리 수 (예약된 다음 웨이브 포함)
    //   ★ 전멸 오판 방지: Alive==0 && Pending==0일 때만 전멸로 인정 → 웨이브1을 30초 안에 전멸시켜도 페이즈가 끝나지 않는다
    // FTimerHandle 관리 배열 + ClearAllTimers() (DRPhase3::ClearAllPhaseTimers 관례, OnPhaseEnd/BeginDestroy 공용)
};
```

**전멸 판정 공통 규칙**: `GetAliveEnemyCount() == 0 && PendingSpawnCount == 0`. `OnEnemyDeath` 오버라이드에서 검사 후 조건 충족 시 다음 단계/`ValidatePhaseCompletion()`.

### 4.5 `UDRS2CombatPhase` (방1) — ★사양 확정 (상세: §14.1)

```
상태: WaitingEntry → (전원 입장) → BlockerRaised/Combat[Wave1 → +30s Wave2] → (전멸 = 완료)
```

```cpp
UCLASS(Blueprintable)
class UDRS2CombatPhase : public UDRS2PhaseBase
{
    // ===== 설정 (BP) — §14.1.3 표를 그대로 입력 =====
    // TArray<FS2WaveSet> WaveSetsByPlayerCount;   // [0]=1인 … [3]=4인, 각 2웨이브(0s / 30s)
    //
    // ===== 런타임 상태 =====
    // int32 TotalSpawnCount = 0;   // 확정 인원의 전 웨이브 마리 수 합 (목표 UI 분모)
    // int32 KilledCount = 0;
    // bool bCombatStarted = false; // 전원 입장 래치
    //
    // virtual void OnPhaseStart() override;
    //   1) SetupPhaseObjectiveByRow("S2P1_Enter") — "1번방으로 이동하라 (n/N)"
    //   2) Director->Trigger_Room1->Arm(); OnAllInside/OnCountChanged 바인딩
    //   3) Director->Blocker_StartToRoom1 널 검사만 (초기 상태 = 내려감 = 통행 가능)
    //
    // OnAllInsideRoom1():                                  // 1회 래치
    //   1) BasePlayerCount = ResolveBasePlayerCount()       // 이후 사망해도 재계산 없음 (§14.1.4)
    //   2) SetBlocked(Director->Blocker_StartToRoom1, true)     // 물체 상승 → 통로 영구 봉쇄 (복제)
    //   3) const FS2WaveSet* Set = ResolveWaveSet(WaveSetsByPlayerCount, BasePlayerCount)
    //      TotalSpawnCount = CountTotalSpawns(*Set); PendingSpawnCount = TotalSpawnCount
    //   4) SetupPhaseObjectiveByRow("S2P1_Combat") + RequiredCount를 TotalSpawnCount로 런타임 재설정
    //      (DRPhase1.cpp:72-75 관례) — "적을 모두 처치하라 (0/N)"
    //   5) bCombatStarted = true; ScheduleWaveSet(*Set, Director->Room1SpawnPoints)
    //      → 웨이브1 즉시(0s), 웨이브2는 30s 예약. ★웨이브1 전멸 여부와 무관하게 시간 기반 진행
    //
    // virtual void OnEnemyDeath(AActor*) override:
    //   ++KilledCount → UpdatePhaseObjectiveProgress(KilledCount)
    //   Alive == 0 && PendingSpawnCount == 0 → GameMode->ValidatePhaseCompletion()
    //
    // virtual bool IsCompleted() const override:
    //   bCombatStarted && GetAliveEnemyCount() == 0 && PendingSpawnCount == 0
    //
    // virtual void OnPhaseEnd() override: Trigger Disarm, 웨이브 타이머 전부 해제, Super(잔적 파괴)
    //   ※ D1 게이트 발광은 여기서 하지 않는다 — 다음(방2) 페이즈 OnPhaseStart 담당 (§3.1 규약, §14.1.6)
};
```

### 4.6 `UDRS2PuzzlePhase` (방2) — ★사양 확정 (상세: §14.2)

```
상태: Entering(게이트 발광) → PuzzleActive(3종 병행) → SafeOpened → PartCarried → (부품 소지자 퇴장 + 전원 회수 = 완료)
```

```cpp
UCLASS(Blueprintable)
class UDRS2PuzzlePhase : public UDRS2PhaseBase
{
    // ===== 런타임 상태 =====
    // TArray<uint8> SecretCode;        // 서버 전용 3자리 (매 판 랜덤)
    // int32 DigitsRevealed = 0;        // 8퍼즐/스위치 해결 수 (0~2) — 목표 진행도
    // bool bSafeOpened = false; bool bPartPickedUp = false; bool bCarrierExited = false;
    //
    // OnPhaseStart():
    //   1) SetGateActive(Director->Gate_Room1ToRoom2, true)      // D1 발광 — 방1 클리어 보상 (§14.1.6, §3.1 규약)
    //   2) SetupPhaseObjectiveByRow("S2P2_Move") — "빛나는 문으로 이동하라"
    //   3) SecretCode = {Rand(0,9), Rand(0,9), Rand(0,9)} 생성 후 주입 (§14.2.6):
    //        SlidePuzzle->SetRevealDigit(SecretCode[0])
    //        SwitchPuzzle->SetRevealDigit(SecretCode[1])
    //        CctvBoard->SetTargetImageCount(SecretCode[2])
    //        Safe->SetSecretCode(SecretCode)
    //   4) 델리게이트 바인딩: SlidePuzzle/SwitchPuzzle->OnPuzzleSolved, Safe->OnSafeOpened,
    //                        Gate_Room2Exit->OnTeamRecalled
    //   5) SetupPhaseObjectiveByRow("S2P2_Puzzle") — "금고의 비밀번호를 알아내라 (0/2)"
    //      ※ 분모 2 = 스크린에 숫자가 뜨는 퍼즐 수(8퍼즐·스위치). CCTV는 해결 개념이 없어 분모에 포함하지 않는다
    //
    // OnPuzzleSolved(AActor* Puzzle): ++DigitsRevealed → UpdatePhaseObjectiveProgress(DigitsRevealed)
    //
    // OnSafeOpened():
    //   1) bSafeOpened = true
    //   2) SetupPhaseObjectiveByRow("S2P2_Part") — "부품을 획득하라"
    //   3) 금고가 스폰한 부품의 OnPartPickedUp 바인딩 (§5.6)
    //
    // OnPartPickedUp(): bPartPickedUp = true
    //   → SetupPhaseObjectiveByRow("S2P2_Return") — "부품을 들고 1번방으로 돌아가라"
    //   ※ 픽업만으로는 완료가 아니다 (원안과의 차이). 픽업 후 드롭해도 목표는 유지되고, 누구든 다시 들고 나가면 된다
    //
    // OnTeamRecalled():   // 부품 소지자가 방2 출구 통과 → 전원 방1 회수 완료 (§14.2.7)
    //   1) SetGateActive(Director->Gate_Room1ToRoom2, false)     // D1 비활성 → 방2 재입장 불가
    //   2) bCarrierExited = true → GameMode->ValidatePhaseCompletion()
    //
    // IsCompleted(): bCarrierExited
    // OnPhaseEnd(): 퍼즐 3종 비활성(프롭 상호작용 차단) + 바인딩 해제
    //   ※ 부품은 SpawnedEnemies가 아니므로 Super의 정리 대상이 아님 — 파괴하지 말 것 (방3~4에서 계속 사용)
};
```

### 4.7 `UDRS2DefensePhase` (방3 + 방4) — ★사양 확정 (상세: §14.3·§14.4)

```
상태: WaitingEntry → CarrierDispatch(방4 문 발광, 부품 소지자 대기)
      → MoleGame ‖ InfiniteWaves (병행)   ──(방4 사망/접속종료)──▶ Retry로 복귀
      → Cleared(부활 + 스폰 중단 + 잔적 즉시 사망) → (완료)
```

```cpp
UCLASS(Blueprintable)
class UDRS2DefensePhase : public UDRS2PhaseBase
{
    // ===== 설정 (BP) =====
    // TArray<FS2WaveSet> WaveSetsByPlayerCount;  // §14.3.2 표 (각 인원 구간에 웨이브 구성 1개)
    // float WaveIntervalSeconds = 50.f;          // ★확정: 50초 주기 무한 반복
    // int32 MaxAliveEnemies = 30;                // 안전장치 (§14.3.6-4)
    // float ReviveHealthRatio = 0.5f;            // ★확정: 최대 체력의 50% (§14.3.4)
    //
    // ===== 런타임 상태 =====
    // TWeakObjectPtr<ADRCharacter> Room4Player;  // 현재 방4에 들어가 있는 플레이어
    // bool bMoleGameCleared = false;
    //
    // OnPhaseStart():
    //   1) SetupPhaseObjectiveByRow("S2P3_Enter") — "부품을 들고 3번방으로 이동하라 (n/N)"
    //   2) Trigger_Room3->Arm(); OnAllInside 바인딩   ※ D2는 이 시점 통행 가능(구조물 내려간 상태)
    //
    // OnAllInsideRoom3():                        // 1회 래치
    //   0) 부품 동반 검증: 부품이 (운반 중 || 방3 트리거 안)이 아니면 발화 무시 + 트리거 재무장
    //      (부품을 방1에 두고 전원 입장하는 소프트락 방지)
    //   1) BasePlayerCount = ResolveBasePlayerCount()            // 웨이브 구성 확정 (§14.3.6-2)
    //   2) SetBlocked(Director->Blocker_Room1ToRoom3, true)      // ★구조물 상승 → 방1 봉쇄 (§14.3.3)
    //   3) Gate_Room3ToRoom4->SetEntryRule(CarrierOnly) + SetGateActive(true)   // 방4 문 발광
    //      Gate_Room3ToRoom4->OnGateUsed 바인딩
    //   4) Room4InstallSite->OnPartInstalled 바인딩 (DRPhase1.cpp:44 관례)
    //   5) SetupPhaseObjectiveByRow("S2P3_Enter4") — "부품을 들고 4번방으로 들어가라"
    //
    // OnGateUsed(ADRCharacter* Who):              // 부품 소지자가 방4 입장
    //   1) Room4Player = Who
    //   2) SetGateActive(Gate_Room3ToRoom4, false)               // ★즉시 비활성 (1명만 입장)
    //   3) SetupPhaseObjectiveByRow("S2P3_Hold") — "4번방 작업이 끝날 때까지 버텨라"
    //
    // OnPartInstalled(ADRCleanserSite*):          // 방4 중앙 설치 → 게임 시작
    //   1) MoleGame->StartGame()                                 // §4.7.1
    //   2) StartWaveLoop()                                       // ★설치 순간부터 50초 주기 무한 웨이브
    //   ※ 설치대는 처음부터 Active — 원안의 "퍼즐 선행 → ActivateSite" 게이트는 폐기 (§14.4.1)
    //
    // OnMoleProgress(int32 Killed, int32 Goal): UpdatePhaseObjectiveProgress(Killed)   // n/20 (§14.4.5-8)
    //
    // OnMoleGameCleared():                        // 20마리 달성 — §14.3.1의 클리어 처리 순서
    //   1) SetGateActive(Gate_Room3ToRoom4, true)                // 방4 복귀 가능 (§14.3.6-5)
    //   2) ReviveAllDeadPlayers()                                // 방3 부활 지점 + 최대 체력 50% (§5.9)
    //   3) SetBlocked(Director->Blocker_Room3ToRoom5, false)     // ★구조물 하강 → 방5 개방
    //   4) StopWaveLoop(); KillAllSpawnedEnemies()               // ★스폰 중단 + 잔적 즉시 사망
    //   5) bMoleGameCleared = true → GameMode->ValidatePhaseCompletion()
    //
    // ===== 예외 처리 (§14.3.5) =====
    // NotifyPlayerDied(APlayerState*):            // A: 방4 플레이어 사망
    //   if (사망자 == Room4Player):
    //     1) MoleGame->AbortAndReset()                           // 두더지 전부 소멸 + KillCount = 0
    //     2) Room4InstallSite->EjectInstalledPart()               // 설치 해제 + 설치대 옆에 부품 드롭 (§5.12)
    //     3) Gate_Room3ToRoom4->SetEntryRule(Anyone) + SetGateActive(true)
    //        ★부품이 방4 안에 있으므로 "소지자만" 규칙을 유지하면 아무도 못 들어간다
    //     4) Room4Player = nullptr;  방3 웨이브는 그대로 유지 (중단하지 않음)
    //
    // NotifyPlayerLeft(APlayerState*):            // B: 방4 플레이어 접속 종료 (§5.10 Logout 훅)
    //   if (이탈자 == Room4Player):
    //     1) MoleGame->AbortAndReset()
    //     2) StopWaveLoop(); DestroyAllSpawnedEnemies()          // ★몬스터 전부 소멸 + 스폰 중단
    //     3) 부품을 Director->Room4EntranceDropPoint(방3 쪽 문 앞)로 이동
    //     4) Gate_Room3ToRoom4->SetEntryRule(CarrierOnly) + SetGateActive(true)   // 원래 흐름 복원
    //     5) BasePlayerCount 재계산 (영구 이탈 반영 — §14.3.6-7)
    //   ※ 재설치 시 OnPartInstalled 경로를 다시 타므로 두더지와 웨이브가 함께 재개된다
    //
    // IsCompleted(): bMoleGameCleared
    // OnPhaseEnd(): 웨이브 타이머/바인딩/트리거/MoleGame 정리 + Super
};
```

- **완료 조건이 "잔적 전멸 대기" → "두더지 클리어"로 변경**되었다 (§14.3.1-④에서 남은 적을 즉시 사망 처리). 원안의 `S2P3_Cleanup`(남은 적 처치) 단계는 삭제.
- **적 AI가 방4로 추격하는 문제 해소**: 방4는 텔레포트 게이트로만 들어가는 분리 공간이라 NavMesh가 이어지지 않는다 → 원안이 검토했던 방4 입구 NavModifier가 불필요.
- **설치대 체력**: 미도입 확정(§11.A-9) — 적 AI 타겟팅 작업 없음.
- **부품을 방3에 두고 아무도 방4에 안 가는 경우**: 웨이브가 아직 시작되지 않았고(설치 시점 시작) 게이트는 계속 발광하므로 소프트락은 아니다 — 진행만 정지한다.

#### 4.7.1 `ADRS2MoleGame` (Actor, bReplicates) — 두더지 게임 관리 (§14.4)

```cpp
USTRUCT(BlueprintType)
struct FS2MoleTier            // 난이도 티어 1구간 (§14.4.3)
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly) int32 KillThreshold = 0;    // 구간 시작 누적 처치 수 (0 / 5 / 12)
    UPROPERTY(EditDefaultsOnly) float MoleLifetime = 2.0f;  // 등장 유지 시간
    UPROPERTY(EditDefaultsOnly) float SpawnInterval = 0.8f; // 스폰 간격
};

UCLASS()
class ADRS2MoleGame : public AActor
{
    // 설정 (BP) — §14.4.3 표 그대로:
    //   TArray<FS2MoleTier> Tiers = { {0, 2.0f, 0.8f}, {5, 1.3f, 0.5f}, {12, 0.8f, 0.3f} };
    //   int32 GoalKills = 20;
    //   int32 MaxConcurrentMoles = 3;                       // §14.4.5-3
    //   TSubclassOf<ADRS2Mole> MoleClass;
    //   TArray<TObjectPtr<AActor>> SpawnPoints;             // EditInstanceOnly: 바닥 등장 지점 (§14.4.5-4)
    //
    // UPROPERTY(ReplicatedUsing = OnRep_KillCount) int32 KillCount = 0;   // 진행도 UI용
    // UPROPERTY(Replicated) bool bActive = false;
    //
    // [서버] void StartGame();       // KillCount = 0, 스폰 타이머 시작
    // [서버] void AbortAndReset();   // 두더지 전부 Destroy + KillCount = 0 + 타이머 정지 (§14.3.5)
    // [서버] void SpawnMole();       // 현재 티어의 Lifetime 주입, 직전 지점 회피 랜덤 배치,
    //                                // 동시 존재 수 < MaxConcurrentMoles일 때만 스폰
    // [서버] void OnMoleKilled(ADRS2Mole*);   // ++KillCount → 티어 갱신(다음 스폰부터 적용)
    //                                         // KillCount >= GoalKills → OnCleared 발화
    // [서버] void OnMoleExpired(ADRS2Mole*);  // 유지 시간 초과 소멸 (패널티 없음)
    //
    // const FS2MoleTier& GetCurrentTier() const;   // KillThreshold 역순 탐색
    // FOnMoleProgress OnProgress;   FOnMoleGameCleared OnCleared;   // 서버 델리게이트
};
```

#### 4.7.2 `ADRS2Mole` (Actor + 최소 ASC) — 홀로그램 두더지 (§14.4.4)

```cpp
UCLASS()
class ADRS2Mole : public AActor, public IAbilitySystemInterface, public IDRProximityHitOnly
{
    // 컴포넌트: UStaticMeshComponent/USkeletalMeshComponent* MoleMesh (홀로그램 머티리얼)
    //           UCapsuleComponent* HitBox                 // 투사체/트레이스에 걸리는 콜리전
    //           UAbilitySystemComponent* ASC + UDREnemyAttributeSet (MaxHealth = 1)
    //           ※ 선례: ADRCleanserSite = AActor + IAbilitySystemInterface (DRCleanserSite.h:48, :221)
    //
    // BeginPlay: Tags.Add("Enemy")
    //   ★ IsNotFriend가 액터 태그 기반이라(DRAbilitySystemLibrary.cpp:415-422) 이 한 줄로 기존
    //     플레이어 공격 전부(투사체/대시/제트점프/근접)의 유효 대상이 된다
    //
    // 설정: float ProximityRadius = 200.f;   // 2m (§14.4.5-5)
    //       float Lifetime;                  // MoleGame이 스폰 시 티어값을 주입
    //
    // virtual bool AcceptsHitFrom(const AActor* Attacker) const override;
    //   → 거리 <= ProximityRadius 면 true, 초과면 false (2m 밖 공격은 투과 — §14.4.4, §5.11)
    //
    // [서버] 체력 0 → MoleGame->OnMoleKilled(this) → 소멸 연출 후 Destroy
    // [서버] Lifetime 만료 → MoleGame->OnMoleExpired(this) → 소멸 연출 후 Destroy
    //
    // UPROPERTY(ReplicatedUsing = OnRep_bVanishing) bool bVanishing = false;
    // UFUNCTION(BlueprintImplementableEvent) void OnEmergeVisual();   // 바닥에서 튀어나오는 연출
    // UFUNCTION(BlueprintImplementableEvent) void OnVanishVisual();   // 홀로그램 소멸 연출
    //
    // ※ 이동/AI 없음 (AIController·BT 미사용) — 순수 표적 (§14.4.5-9)
    // ※ 물(Water) 보상 미지급 — ADREnemy의 보상 경로를 타지 않는다 (§14.4.5-7)
};
```

### 4.8 `UDRS2WavePhase` (방5) — ★사양 확정 (상세: §14.5)

```
상태: WaitingEntry → Wave(0) → Wave(1) → Wave(2) → (마지막 웨이브 전멸 = 완료)
       각 웨이브 전환 트리거 = min(30초 경과, 전체 전멸)   ★하이브리드 (§14.5.3)
```

```cpp
UCLASS(Blueprintable)
class UDRS2WavePhase : public UDRS2PhaseBase
{
    // ===== 설정 (BP) — §14.5.2 표를 그대로 입력 =====
    // TArray<FS2WaveSet> WaveSetsByPlayerCount;  // [0]=1인 … [3]=4인, 각 3웨이브
    // float WaveIntervalSeconds = 30.f;          // ★확정: 30초 (전멸 시 앞당겨짐)
    //   ※ FS2WaveComposition::StartDelaySeconds는 사용하지 않는다 — 전환 시점을 페이즈가 직접 제어
    //
    // ===== 런타임 상태 =====
    // int32 CurrentWaveIndex = -1;
    // FTimerHandle NextWaveTimer;
    // bool bWavesStarted = false;
    //
    // OnPhaseStart():
    //   1) SetBlocked(Director->Blocker_Room3ToRoom5, false)  // D4 하강 개방 (§3.1 규약: 입장 통로는 다음 페이즈가 연다)
    //      ※ 방3 페이즈의 클리어 처리에서 이미 열렸다면 no-op — 멱등 호출
    //   2) SetupPhaseObjectiveByRow("S2P4_Enter") — "5번방으로 이동하라 (n/N)"
    //   3) Trigger_Room5->Arm(); OnAllInside/OnCountChanged 바인딩
    //
    // OnAllInsideRoom5():                        // 1회 래치
    //   1) BasePlayerCount = ResolveBasePlayerCount()
    //   2) SetBlocked(Director->Blocker_Room3ToRoom5, true)   // ★D4 재봉쇄 — 방3 복귀 불가 (§14.5.4)
    //   3) GameState->SetTotalWaves(3)                        // 기존 복제 필드 재활용 (DRStageGameState.h:309)
    //   4) bWavesStarted = true; StartWave(0)
    //
    // StartWave(int32 i):
    //   1) CurrentWaveIndex = i; GameState->SetCurrentWaveNumber(i + 1)
    //   2) SetupPhaseObjectiveByRow("S2P4_Wave") + 진행도 (i+1)/3 — "웨이브를 막아내라"
    //   3) GameState->Multicast_PlayWaveStartSound()          // 기존 사운드 재활용 (§14.5.5-4)
    //   4) SpawnComposition(ActiveSet.Waves[i], Director->Room5SpawnPoints)
    //   5) if (i < 마지막) 30초 후 StartWave(i+1) 예약 → NextWaveTimer
    //      ★마지막 웨이브에는 타이머를 걸지 않는다 (4번째 웨이브 방지)
    //
    // OnEnemyDeath(AActor*) override:            // ★하이브리드 전환 (§14.5.3)
    //   if (GetAliveEnemyCount() > 0 || PendingSpawnCount > 0) return;
    //   if (CurrentWaveIndex < 마지막):
    //       ClearTimer(NextWaveTimer);           // 예약된 30초를 취소하고
    //       StartWave(CurrentWaveIndex + 1);     // 즉시 다음 웨이브 (새 30초 타이머 재예약)
    //   else:
    //       GameMode->ValidatePhaseCompletion();
    //   ※ 판정 기준은 웨이브 단위가 아니라 **전체 생존 수 0** — 30초로 겹쳐 스폰된 잔존분까지 포함
    //
    // IsCompleted():
    //   bWavesStarted && CurrentWaveIndex == 마지막 && GetAliveEnemyCount() == 0 && PendingSpawnCount == 0
    //
    // OnPhaseEnd(): NextWaveTimer 해제, 트리거 Disarm, Super(잔적 파괴)
    //   ※ D5(방5→방6) 하강 개방은 방6 페이즈 OnPhaseStart 담당 (§3.1 규약)
};
```

- **방1·방3과의 차이**: 방1은 시간 고정(전멸해도 안 당김), 방3은 시간 고정 무한 반복, **방5만 "먼저 오는 쪽"** 이다 (§14.5.3 비교표).

### 4.9 열차 시스템 (방6) ★ 신규 규모 최대

#### 4.9.1 `ADRS2TrainTrack` (Actor)

- `USplineComponent* Spline` 하나를 보유하는 순수 데이터 액터. 레벨에서 선로를 따라 스플라인 편집.
- 장애물 정지 위치는 **장애물 액터의 트랙 거리값**으로 정의: `ADRS2TrainObstacle::StopDistance` (float, 에디터 입력) — 또는 에디터 유틸로 장애물 위치를 스플라인에 투영해 자동 계산(`FindInputKeyClosestToWorldLocation`). 초기 구현은 수동 입력 + `BeginPlay` 검증(정렬 오름차순 확인).

#### 4.9.2 `ADRS2Train` (Actor, bReplicates)

```cpp
UENUM(BlueprintType)
enum class ES2TrainState : uint8 { WaitingForBoarding, Moving, StoppedAtObstacle, Arrived };

USTRUCT()
struct FS2TrainMovement            // "복제 1회 + 로컬 시뮬" (WaveTimerEndServerTime 관례 미러)
{
    GENERATED_BODY()
    UPROPERTY() ES2TrainState State = ES2TrainState::WaitingForBoarding;
    UPROPERTY() float StartDistance = 0.f;
    UPROPERTY() float TargetDistance = 0.f;
    UPROPERTY() float StartServerTime = 0.f;
    UPROPERTY() float Speed = 0.f;          // uu/s
};

UCLASS()
class ADRS2Train : public AActor
{
    // 컴포넌트: RootScene + 칸별 StaticMesh (BP 구성) + 칸별 ADRS2TrainSeat ChildActorComponent(4개)
    //
    // UPROPERTY(ReplicatedUsing = OnRep_Movement) FS2TrainMovement Movement;
    // ADRS2TrainTrack* Track (EditInstanceOnly);
    //
    // Tick (서버+클라 공통):
    //   if (Movement.State == Moving)
    //     Now  = GameState->GetServerWorldTimeSeconds();
    //     Dist = FMath::Min(Movement.TargetDistance,
    //                       Movement.StartDistance + Movement.Speed * (Now - Movement.StartServerTime));
    //     SetActorTransform(Track->Spline->GetTransformAtDistanceAlongSpline(Dist, World));
    //     [서버만] Dist >= TargetDistance → ArriveAtTarget()  (OnStoppedAtObstacle / OnArrivedAtEnd 발화)
    //   ※ 좌석·탑승자는 attach 계층이라 자동 추종. bReplicateMovement는 사용하지 않음(이중 소스 방지).
    //
    // 서버 API (페이즈가 호출):
    //   void DepartTo(float TargetDistance, float Speed);   // 상태 Moving 전환 + Movement 복제
    //   void SetWaitingForBoarding(); void SetArrived();
    //
    // 좌석/탑승:
    //   TArray<ADRS2TrainSeat*> GetSeats() const;           // ChildActor 수집 (BeginPlay 캐시)
    //   bool AreAllAlivePlayersSeated() const;              // GetAlivePlayers 전원이 좌석 점유자와 일치
    //   FOnTrainBoardingChanged OnBoardingChanged;          // 좌석 변동 시 (탑승 n/N UI + 출발 판정 재평가)
    //   FOnTrainStopped OnStopped;   FOnTrainArrived OnArrived;   // 서버 전용 델리게이트
    //
    // 하차 허용 질의: bool CanDeboardNow() const
    //   → State == StoppedAtObstacle || State == Arrived  (요구사항 "엘리트 구간에서만 하차")
};
```

- **이동 정밀도**: 서버-클라 시각차(GetServerWorldTimeSeconds 동기 오차) 수준의 위치 편차만 발생하고, 게임플레이 판정(정지/전투)은 전부 서버 기준이므로 문제 없음. 열차 콜리전은 `BlockAll`-계열이되 탑승자 캡슐과의 충돌은 attach 상태(MOVE_None)라 스윕이 없어 간섭하지 않음.
- **출발 연출**: `OnRep_Movement`에서 상태 전환별 사운드/진동 재생 훅 (BP 이벤트).

#### 4.9.3 `ADRS2TrainSeat` (Actor, IDRInteractable, bReplicates)

```cpp
UCLASS()
class ADRS2TrainSeat : public AActor, public IDRInteractable
{
    // USceneComponent* SeatPoint;   // 착석 attach 지점
    // USceneComponent* ExitPoint;   // 하차 배치 지점 (칸 옆)
    // UWidgetComponent* InteractionWidget;   // "F 탑승" (CleanserSite 관례)
    //
    // UPROPERTY(ReplicatedUsing = OnRep_SeatedCharacter) ADRCharacter* SeatedCharacter;
    //
    // 서버 API:
    //   bool CanBeBoardedBy(ADRCharacter*) const;
    //     → 미점유 && 후보 생존 && !IsCarryingPart && !IsMounted && 열차 상태가 탑승 허용
    //       (DRRobotVacuumCharacter::CanBeMountedBy 검증 항목 미러, DRRobotVacuumCharacter.h:51-52)
    //   void Board(ADRCharacter*);    // SeatedCharacter 세팅 → 캐릭터 측 SetSeatedOn(this)
    //   void Deboard();               // ExitPoint로 배치 + 해제
    //   사망/파괴 대응: 탑승자 OnDeath 바인딩 → 좌석 자동 해제
    //
    // virtual void SetInteractionUIVisible(bool) override;   // 로컬 UI (IDRInteractable 관례)
};
```

#### 4.9.4 `ADRCharacter` 좌석 상태 (기존 파일 추가 — §5.5와 세트)

마운트 구현(`MountedOn` RepNotify + `AttachToMountSocket`, `DRCharacter.h:108-133`)을 미러링한 **병렬 상태**로 추가한다 (마운트 타입이 `ADRRobotVacuumCharacter`로 고정되어 있어 일반화 대신 병렬 추가가 안전):

```cpp
// DRCharacter.h 추가
UPROPERTY(ReplicatedUsing = OnRep_SeatedOn, BlueprintReadOnly, Category = "Train")
TObjectPtr<ADRS2TrainSeat> SeatedOn;

UFUNCTION() void OnRep_SeatedOn();          // attach/detach + 로컬 연출
bool IsSeatedOnTrain() const { return SeatedOn != nullptr; }
void SetSeatedOn(ADRS2TrainSeat* Seat);     // 서버: attach(SeatPoint) + CMC MOVE_None / 해제 시 원복
```

- 착석 중 이동 입력 무시: `MOVE_None`으로 CMC가 무시 (마운트 라이더와 동일 접근).
- 착석 중 스킬: **미정 (§11-10)** — 기본 구현은 허용(이동만 잠금, 열차 위 사격 가능). 차단으로 결정되면 `State.Seated` 태그 부여 + `IsAbilityInputBlocked` 확장(§5.8)으로 전환 — 착석/하차 시 태그 1개 넣고 빼는 수준이라 교체 비용 낮음. M6.2(좌석 구현) 전까지 결정 권장.
- 사망 시: `Die()` 경로에서 `SeatedOn` 해제 (좌석 측 OnDeath 바인딩과 이중 안전).

#### 4.9.5 `ADRS2TrainObstacle` (Actor, bReplicates) — ★사양 변경 (§14.6.3)

**두더지 보스가 장애물을 부수며 등장**하므로(사용자 확정), 장애물은 "임계 도달 시 해제되는 잠금"이 아니라 **열차 정지 지점 정의 + 보스 등장 연출용 파괴 오브젝트**가 된다. 전방 차단 역할은 `ADRS2Barrier`로 이관한다.

```cpp
UCLASS()
class ADRS2TrainObstacle : public AActor
{
    // UStaticMeshComponent* ObstacleMesh;    // 선로 차단물 (보스가 부수는 대상)
    // USceneComponent* BossSpawnPoint;       // 두더지 보스 등장 트랜스폼 (장애물 뒤/아래)
    // float StopDistance;                    // 트랙 상 정지 거리 (§4.9.1)
    //
    // UPROPERTY(ReplicatedUsing = OnRep_bBroken) bool bBroken = false;
    // void BreakByBoss();   // 서버: bBroken=true → OnRep: 파괴 연출(BP 이벤트) + ObstacleMesh 콜리전/가시성 해제
    //   ★타이밍 변경: 원안은 "보스 체력 임계 도달 시 Clear()"였으나, 확정 사양은 **보스 등장과 동시**에 부서진다
    //
    // UFUNCTION(BlueprintImplementableEvent) void OnBrokenVisual();   // 파편/먼지/사운드
    // ※ 원안의 PawnWall(전방 차단)은 제거 — 부서진 뒤에는 막을 수 없으므로 §4.9.6 배리어가 전담
};
```

#### 4.9.6 `ADRS2Barrier` (Actor, bReplicates) — 전투 구간 전·후방 차단 (§14.6.5)

장애물이 등장 즉시 부서지므로 **전방·후방 양쪽**을 배리어로 막아 전투 구간을 한정한다 (원안은 후방만 담당).

```cpp
// 전투 구간 차단벽: Pawn 전용 Block 박스, 기본 비활성
UPROPERTY(ReplicatedUsing = OnRep_bEnabled) bool bEnabled = false;
void SetBarrierEnabled(bool);   // OnRep: 콜리전 토글 + (선택) 역장 시각화

// Director 배선: 장애물 i마다 전방(ForwardBarriers[i]) / 후방(RearBarriers[i]) 한 쌍
```

- 콜리전 채널: **Pawn만 Block**, WorldDynamic(열차)·Projectile 등은 Ignore → 열차 통과와 사격을 방해하지 않는다.
- 전투 시작 시 두 배리어를 켜고, 보스가 도망친 뒤(재탑승 국면) **전방 배리어만 해제**해 열차가 지나갈 수 있게 한다. 후방 배리어는 열차 출발과 함께 해제한다.

#### 4.9.7 `UDRS2TrainPhase` (방6 페이즈) — ★사양 확정 (상세: §14.6)

```
상태: Boarding → Traveling(0) → BossFight(0, 67%) → ReBoarding → Traveling(1)
      → BossFight(1, 34%) → ReBoarding → Traveling(2) → BossFight(2, 처치) → (스테이지2 클리어)
```

```cpp
UCLASS(Blueprintable)
class UDRS2TrainPhase : public UDRS2PhaseBase
{
    // ===== 설정 (BP) =====
    // TSubclassOf<ADREnemy> MoleBossClass;                 // 두더지 보스 (스킬은 별도 작업 — §14.6.7)
    // TArray<float> RetreatHealthRatios = { 0.67f, 0.34f }; // ★확정: 1번 67%, 2번 34%, 3번은 사망까지
    // float TrainSpeed = 600.f;                            // 등속 (§14.6.2)
    //
    // ===== 런타임 상태 =====
    // TWeakObjectPtr<ADREnemy> MoleBoss;   // ★단일 개체를 3구간에 재사용 (체력 이어짐 — §14.6.4)
    // int32 CurrentObstacleIndex = -1;
    // bool bBossDefeated = false;
    // TArray<bool> bSegmentResolved;       // 구간별 1회 래치
    //
    // OnPhaseStart():
    //   1) SetBlocked(Director->Blocker_Room5ToRoom6, false)   // D5 하강 개방 (§3.1 규약, 멱등)
    //   2) Train->SetWaitingForBoarding(); OnBoardingChanged/OnStopped 바인딩
    //   3) SetupPhaseObjectiveByRow("S2P5_Board") — "열차에 탑승하라 (n/N)"
    //
    // OnBoardingChanged(): 진행도 n/N 갱신 →   [§11.A-6: 칸 4 고정, 빈 칸 허용, 조건은 "생존자 전원 착석"]
    //   Train->AreAllAlivePlayersSeated() && 대기 상태이면:
    //     - 후방 배리어(직전 구간) 해제
    //     - Train->DepartTo(Obstacles[NextIdx].StopDistance, TrainSpeed)
    //     - SetupPhaseObjectiveByRow("S2P5_Ride")
    //
    // OnTrainStopped(int32 i):     // 장애물 앞 도착 → 전투 시작
    //   1) CurrentObstacleIndex = i
    //   2) Obstacles[i]->BreakByBoss()                       // ★보스가 장애물을 부수는 연출 (§14.6.3)
    //   3) ForwardBarriers[i]->SetBarrierEnabled(true); RearBarriers[i]->SetBarrierEnabled(true)
    //      // 장애물이 부서지므로 전·후방 모두 배리어로 구간 한정 (§4.9.6)
    //   4) if (i == 0) MoleBoss = SpawnEnemyAt(MoleBossClass, Obstacles[0]->BossSpawnPoint)
    //      else        ReappearBoss(Obstacles[i]->BossSpawnPoint)   // ★같은 개체 재등장 (체력 유지)
    //   5) MoleBoss->OnHealthChanged/OnMaxHealthChanged 바인딩 → 비율 추적 (DREnemy.h:74-78)
    //   6) SetupPhaseObjectiveByRow("S2P5_Boss") + 진행도 (i+1)/3
    //
    // OnBossHealthChanged(float NewValue):
    //   const float Ratio = NewValue / CachedMaxHealth;
    //   if (i < RetreatHealthRatios.Num() && Ratio <= RetreatHealthRatios[i]) → ResolveSegment(i);
    //   ※ 3번째 구간(i == 2)에는 임계가 없다 — 사망만이 해제 조건 (§14.6.4)
    //
    // OnEnemyDeath(보스 사망):
    //   if (CurrentObstacleIndex == 마지막) { bBossDefeated = true; ValidatePhaseCompletion(); }  // ★스테이지2 클리어
    //   else ResolveSegment(CurrentObstacleIndex);   // 임계 전에 죽인 예외 케이스도 진행으로 인정
    //
    // ResolveSegment(int32 i):   [bSegmentResolved[i] 래치]
    //   1) RetreatBoss()  — 체력/사망 바인딩 해제 → 도망 연출(BP `OnBossRetreat`) →
    //      ★Destroy가 아니라 **비활성 보관**: SetActorHiddenInGame(true) + 콜리전/AI 정지
    //      (체력·디버프 상태를 그대로 유지해 다음 구간에서 이어 싸우기 위함 — §14.6.4)
    //   2) ForwardBarriers[i]->SetBarrierEnabled(false)       // 전방 개방 (열차 통과용)
    //   3) SetupPhaseObjectiveByRow("S2P5_Board") — "다시 열차에 탑승하라 (n/N)"
    //      → 전원 재탑승 시 OnBoardingChanged 경로로 재출발 (§11.A-5)
    //
    // ReappearBoss(const FTransform&):
    //   보관 중인 MoleBoss를 스폰 지점으로 이동 → 가시성/콜리전/AI 복구 → OnBossReappear BP 훅
    //   ★체력은 손대지 않는다 (67% → 34% → 0% 로 이어지는 것이 사양)
    //
    // IsCompleted(): bBossDefeated
    //   ※ 원안의 "종점 도착 + 전원 도착지점 진입" 조건은 폐기 — **최종 처치가 곧 스테이지 클리어** (§14.6.6)
    // OnPhaseEnd(): 전 바인딩/타이머/배리어 정리 + 보스 정리 + Super
};
```

- **하차 검증 경로**: 점프 입력 → `ADRCharacter` 점프 처리에서 `IsSeatedOnTrain()`이면 `ServerRequestTrainDeboard()` → 서버가 `Train->CanDeboardNow()` 확인 후 `Seat->Deboard()` (이동 중 하차 거부). 마운트 하차(점프키) 분기와 같은 위치에 병렬 추가 (§5.5).
- **보스 물 보상**: 1·2구간 도망은 `Die()`를 타지 않으므로 보상이 없고, **3구간 최종 처치에서만** 기존 규칙대로 지급된다(`bIsBoss` 설정에 따라 전역 지급).
- **보관 방식을 쓰는 이유**: 매 구간 새로 스폰하고 체력을 비율로 복원하는 방식은 어트리뷰트 초기화 순서·디버프 상태·AI 블랙보드가 리셋되어 "이어 싸우는" 느낌이 깨진다. 같은 액터를 숨겨두면 체력·디버프가 자동으로 유지된다.

### 4.10 방2 퍼즐 시스템 — ★사양 확정 (상세: §14.2)

퍼즐 3종이 **구조가 전혀 다른 독립 미니게임**(슬라이드 타일 / 라이트아웃 / 관찰 카운팅)으로 확정되어, 원안의 "`ADRS2PuzzleBase` + `PuzzleGroup`으로 일반화" 방식을 폐기한다. 공유하는 것은 **상호작용 프롭 베이스**와 **숫자 공개 계약** 두 가지뿐이다.

> 폐기 사유: ① CCTV는 "해결" 상태가 없어 `bSolved`/`OnAllSolved` 계약에 맞지 않는다. ② 세 퍼즐의 복제 상태 형태(9칸 배열 / 비트마스크 / 시퀀스+시각)가 달라 공통 베이스에 담을 게 없다. ③ 방2 완료 판정은 퍼즐이 아니라 **금고 개방 → 부품 → 퇴장**이 결정한다.

#### 4.10.1 `ADRS2InteractProp` (Actor, IDRInteractable) — 공용 상호작용 프롭

방2의 조작 대상(타일 8 / 레버 5 / 버튼 12+)을 전부 이 베이스의 자식 액터로 만든다 (§14.2.8).

```cpp
UCLASS(Abstract)
class ADRS2InteractProp : public AActor, public IDRInteractable
{
    // UStaticMeshComponent* PropMesh;             // 루트 (ECC_Visibility 트레이스 대상)
    // UWidgetComponent* InteractionWidget;        // "F" 프롬프트 (CleanserSite 관례)
    //
    // UPROPERTY(ReplicatedUsing = OnRep_bPropEnabled) bool bPropEnabled = true;
    // UPROPERTY(EditInstanceOnly) TObjectPtr<AActor> OwnerPuzzle;   // 소속 퍼즐 액터
    // UPROPERTY(EditInstanceOnly) int32 PropIndex = INDEX_NONE;     // 타일/레버/버튼 번호
    //
    // virtual bool CanInteract(const ADRCharacter*) const;   // 기본: bPropEnabled && !bDead && !IsCarryingPart()
    // virtual void ServerHandleInteract(ADRCharacter*) PURE_VIRTUAL;  // 실제 로직은 OwnerPuzzle에 위임
    // virtual void SetInteractionUIVisible(bool) override;   // 로컬 UI (IDRInteractable)
    // UFUNCTION(BlueprintImplementableEvent) void OnInteractedVisual();      // 누름 연출
    // UFUNCTION(BlueprintImplementableEvent) void OnPropEnabledChanged(bool);
};
```

- 자식: `ADRS2SlideTile`, `ADRS2Lever`, `ADRS2PuzzleButton`(되돌리기/초기화), `ADRS2SafeButton`(숫자 0~9).
- **상태는 프롭이 아니라 퍼즐 액터가 보유·복제**한다 (프롭 27개를 각각 복제하지 않기 위함).

#### 4.10.2 `ADRS2SlidePuzzle` (Actor, bReplicates) — 8퍼즐 (§14.2.2)

```cpp
UCLASS()
class ADRS2SlidePuzzle : public AActor
{
    // 구성: BoardRoot + TArray<ADRS2SlideTile*> Tiles(8) + 완성 사진 플레인
    //       + ADRS2CodeScreen* Screen + ADRS2PuzzleButton* UndoButton/ResetButton
    // UPROPERTY(EditDefaultsOnly) float CellSize;  int32 ShuffleMoves = 80;  float SlideDuration = 0.15f;
    //
    // UPROPERTY(ReplicatedUsing = OnRep_BoardState) TArray<uint8> BoardState;  // 9칸: 0=빈칸, 1~8=타일ID
    // UPROPERTY(ReplicatedUsing = OnRep_RevealedDigit) int32 RevealedDigit = -1;  // 해결 전 -1 (§14.2.6)
    // UPROPERTY(Replicated) bool bSolved = false;
    // int32 SecretDigit = -1;          // 서버 전용 (페이즈가 주입)
    // TOptional<TPair<uint8,uint8>> LastMove;   // 되돌리기용 1수
    // TArray<uint8> InitialBoardState;          // 초기화 버튼용
    //
    // [서버] void GenerateShuffledBoard();      // 빈칸 무작위 이동 ShuffleMoves회 → 항상 해결 가능
    // [서버] bool TryMoveTile(int32 TileId);    // 빈칸 인접 검증 → 스왑 → LastMove 기록 → CheckSolved()
    // [서버] void UndoLastMove(); void ResetBoard();
    // [서버] void CheckSolved();                // 목표 배열 일치 → bSolved 래치 + RevealedDigit = SecretDigit + OnPuzzleSolved 발화
    // void SetRevealDigit(int32);               // 페이즈 주입 (§14.2.6)
    //
    // OnRep_BoardState(): 각 타일의 목표 로컬 위치 산출 → SlideDuration 로컬 보간 (이동 복제 없음)
    // FOnS2PuzzleSolved OnPuzzleSolved;         // 서버 델리게이트 (페이즈 진행도 표시용)
};
```

#### 4.10.3 `ADRS2SwitchPuzzle` (Actor, bReplicates) — 스위치 퍼즐 (§14.2.3)

```cpp
UCLASS()
class ADRS2SwitchPuzzle : public AActor
{
    // 구성: TArray<ADRS2Lever*> Levers(5) + TArray<UStaticMeshComponent*> Bulbs(9)
    //       + TArray<UStaticMeshComponent*> RoundMarkers(3) + ADRS2CodeScreen* Screen
    // UPROPERTY(EditDefaultsOnly) int32 RequiredRounds = 3;  int32 MinBitsPerLever = 3, MaxBitsPerLever = 5;
    //
    // ===== 서버 전용 (복제 금지 — 정답 정보) =====
    // TArray<uint16> LeverMasks;       // 레버별 9비트 전구 마스크
    // int32 SecretDigit = -1;
    //
    // ===== 복제 =====
    // UPROPERTY(ReplicatedUsing = OnRep_Bulbs)  uint16 BulbBits = 0;      // 전구 점등 상태
    // UPROPERTY(ReplicatedUsing = OnRep_Levers) uint8  LeverBits = 0;     // 레버 시각 상태
    // UPROPERTY(ReplicatedUsing = OnRep_Rounds) int32  RoundsCleared = 0;
    // UPROPERTY(ReplicatedUsing = OnRep_RevealedDigit) int32 RevealedDigit = -1;
    //
    // [서버] void GenerateRound();
    //   1) 정답 부분집합 S 랜덤 선택 (공집합 아님)
    //   2) S의 마지막 원소 마스크 = 0x1FF XOR (S 나머지 마스크들의 XOR)   ← 해 존재 보장 (§14.2.3)
    //   3) S 밖 레버는 자유 랜덤 마스크 (0 / 0x1FF 제외)
    //   4) 32조합 완전탐색으로 해 존재 검증 — 실패 시 재생성
    //   5) LeverBits = 0, BulbBits = 0
    // [서버] void ToggleLever(int32 Index);     // LeverBits 토글 → BulbBits = XOR(ON 레버 마스크들) → CheckRound()
    // [서버] void CheckRound();                 // BulbBits == 0x1FF → ++RoundsCleared → 마커 발광
    //        RoundsCleared >= RequiredRounds ? (RevealedDigit = SecretDigit + OnPuzzleSolved) : GenerateRound()
    // void SetRevealDigit(int32);
    //
    // FOnS2PuzzleSolved OnPuzzleSolved;
};
```

#### 4.10.4 `ADRS2CctvBoard` (Actor, bReplicates) — CCTV 기믹 (§14.2.4)

```cpp
USTRUCT()
struct FS2CctvStep      // 2초 스텝 1개
{
    GENERATED_BODY()
    UPROPERTY() uint8 NormalScreenA = 0;   // 정상 표시 화면 인덱스 (0~5)
    UPROPERTY() uint8 NormalScreenB = 1;
    UPROPERTY() uint8 ImageA = 0;          // 그 화면에 띄울 이미지 인덱스 (0 = 타깃 이미지)
    UPROPERTY() uint8 ImageB = 0;
};

UCLASS()
class ADRS2CctvBoard : public AActor
{
    // 구성: TArray<UStaticMeshComponent*> Screens(6) — 머티리얼 인스턴스 텍스처 파라미터 교체
    // UPROPERTY(EditDefaultsOnly) UTexture2D* TargetImage;  TArray<UTexture2D*> DummyImages;  UTexture2D* ErrorImage;
    // UPROPERTY(EditDefaultsOnly) float StepDuration = 2.f;  int32 StepCount = 12;
    //
    // UPROPERTY(ReplicatedUsing = OnRep_Sequence) TArray<FS2CctvStep> Sequence;   // 1회 복제
    // UPROPERTY(Replicated) float StartServerTime = 0.f;                          // 1회 복제
    // int32 TargetImageCount = 0;       // 서버 전용 (= 금고 3번째 자리)
    //
    // [서버] void SetTargetImageCount(int32 N);   // 페이즈 주입 → BuildSequence()
    // [서버] void BuildSequence();
    //   전체 노출 슬롯(StepCount × 2) 중 타깃 이미지를 정확히 N개 배치, 나머지는 DummyImages에서 채움
    //   각 스텝의 정상 화면 2개는 6개 중 랜덤 (연속 스텝 중복 완화)
    //
    // Tick (서버+클라 공통 — 로컬 시뮬):
    //   Step = FMath::FloorToInt((GetServerWorldTimeSeconds() - StartServerTime) / StepDuration) % Sequence.Num()
    //   → 정상 화면 2개엔 해당 이미지, 나머지 4개엔 ErrorImage 세팅 (스텝이 바뀔 때만 갱신)
    //   ※ 시퀀스가 끝나면 처음부터 순환 → 놓쳐도 다시 셀 수 있다 (§14.2.4)
    //
    // ※ "해결" 개념·스크린 표시 없음. 답은 플레이어가 세어 금고에 입력한다.
};
```

#### 4.10.5 `ADRS2Safe` (Actor, bReplicates) — 금고 (§14.2.5)

```cpp
UCLASS()
class ADRS2Safe : public AActor
{
    // 구성: SafeBodyMesh + DoorMesh(개방 연출) + TArray<ADRS2SafeButton*> DigitButtons(0~9)
    //       + ADRS2PuzzleButton* ClearButton + ADRS2CodeScreen* InputDisplay + USceneComponent* PartSpawnPoint
    // UPROPERTY(EditDefaultsOnly) TSubclassOf<ADRCleanserPart> PartClass;   // BP_S2Part
    //
    // TArray<uint8> SecretCode;                                     // 서버 전용 (3자리)
    // UPROPERTY(ReplicatedUsing = OnRep_Input)  TArray<uint8> InputDigits;   // 입력 진행 (팀 공유 표시)
    // UPROPERTY(ReplicatedUsing = OnRep_bOpened) bool bOpened = false;
    //
    // void SetSecretCode(const TArray<uint8>&);     // 페이즈 주입 (§14.2.6)
    // [서버] void PushDigit(uint8 Digit);            // append → 3자리 도달 시 Validate()
    // [서버] void ClearInput();
    // [서버] void Validate();                        // 일치 → Open() / 불일치 → ClearInput() + Multicast 오답 사운드
    // [서버] void Open();                            // bOpened 래치 → 문 개방 연출 + PartSpawnPoint에 PartClass 스폰
    // OnRep_bOpened(): 문 개방 연출 + 사운드 (BP 훅 OnSafeOpened)
    // FOnSafeOpened OnSafeOpened;   // 서버 델리게이트 (페이즈 목표 전환용)
};
```

#### 4.10.6 `ADRS2CodeScreen` (Actor 또는 컴포넌트) — 숫자 표시판

- 8퍼즐/스위치의 "비밀번호 스크린" + 금고 입력 표시창 공용. `SetDigits(const TArray<int32>&)` 하나만 노출하고, 표시 방식(머티리얼 숫자 아틀라스 / 3D 텍스트)은 BP에서 결정 (§14.2.9-12).
- 자체 복제 없음 — 상위 퍼즐/금고의 복제 상태를 `OnRep`에서 받아 갱신한다.

### 4.11 `ADRS2MovingBlocker` (Actor, bReplicates) — ★신규 확정 (D0/D2/D4)

"문이 열리고 닫히는" 방식이 아니라 **구조물이 올라오거나 내려가서 통로를 막고/여는** 연출 액터. 방3 사양에서 **D4가 "아래로 내려가서 열리는" 구조**로 확정되었으므로(§14.3.3), 원래 이름 `ADRS2RisingBlocker`를 **`ADRS2MovingBlocker`로 일반화**해 세 통로를 한 클래스로 처리한다.

| 통로 | 초기 상태 | 동작 | 방향 |
|---|---|---|---|
| D0 (시작지점→방1) | 열림 | 전원 방1 입장 시 **막힘** | 상승 (§14.1.2) |
| D2 (방1→방3) | 열림 | 전원 방3 입장 시 **막힘** | 상승 (§14.3.3) |
| D4 (방3↔방5) | **막힘** | 두더지 클리어 시 **열림** → **전원 방5 입장 시 다시 막힘** ★왕복 | 하강 → 상승 (§14.5.4) |
| D5 (방5→방6) | **막힘** | 방5 3웨이브 클리어 시 **열림** | 하강 (§14.5.4) |

```cpp
UCLASS()
class ADRS2MovingBlocker : public AActor
{
    // 컴포넌트:
    //   USceneComponent* Root
    //   UStaticMeshComponent* BlockerMesh;   // 이동하는 구조물 (여러 개면 BP에서 추가 — 각 메시가 Root 자식)
    //   UBoxComponent* PawnBlock;            // 통로 차단 콜리전 (Pawn만 Block)
    //
    // 설정 (EditDefaultsOnly):
    //   FVector BlockedOffset = (0,0,300);   // "막힘" 포즈의 로컬 오프셋 (상승형은 +Z, 하강형은 0)
    //   FVector OpenOffset    = (0,0,0);     // "열림" 포즈의 로컬 오프셋 (하강형은 -Z)
    //   float MoveDuration = 1.5f;           // 이동 시간 — §14.1.7-7 미정, 플레이스홀더
    //   bool bStartBlocked = false;          // D0/D2 = false, D4 = true
    //   TObjectPtr<AActor> PushOutPoint;     // EditInstanceOnly: 막힘 완료 시 통로에 남은 Pawn을 옮길 지점
    //
    // UPROPERTY(ReplicatedUsing = OnRep_bBlocked) bool bBlocked = false;
    //
    // void SetBlocked(bool);   // 서버: bBlocked 세팅 + OnRep 수동 호출 (리슨 서버 관례)
    // OnRep_bBlocked():        // 서버·클라 공통: 두 포즈 사이 로컬 보간 (이동 복제 아님 — "1회 복제 + 로컬 시뮬")
    //   → OnMoveStarted(bBlocked) BP 훅 (사운드/카메라 셰이크/먼지 VFX)
    // [서버] 막힘 완료: PawnBlock 콜리전 활성 + 박스 안에 남은 Pawn을 PushOutPoint로 이동 (끼임 방어)
    // [서버] 열림 완료: PawnBlock 콜리전 해제
    //   → OnMoveFinished(bBlocked) BP 훅
    //
    // UFUNCTION(BlueprintImplementableEvent) void OnMoveStarted(bool bNowBlocking);
    // UFUNCTION(BlueprintImplementableEvent) void OnMoveFinished(bool bNowBlocking);
};
```

- **끼임 방어**: D0/D2의 발화 조건이 "전원 다음 방 입장"이므로 통로에는 원칙적으로 아무도 없다. 경계에 걸친 케이스만 `PushOutPoint` 이동으로 처리한다 (§10.1 테스트 포함).
- **콜리전 전환 시점**: 막힐 때는 **이동 완료 후** 활성화(상승 중 캐릭터를 천장으로 밀어 올리는 사고 방지), 열릴 때는 **이동 시작 시** 즉시 해제(하강 중 벽에 갇히는 것 방지).
- D0·D2는 재개방 경로를 사용하지 않으므로(영구 봉쇄) 페이즈가 `SetBlocked(false)`를 호출하지 않는다 — API는 공용이지만 호출 규약으로 보장한다.
- **D4만 왕복 동작**(막힘 → 하강 개방 → 상승 재봉쇄)을 하며, `SetBlocked(bool)` 양방향 API가 이를 그대로 커버한다. 방5 페이즈의 `OnPhaseStart`에서 여는 호출은 **멱등**이어야 한다(방3 페이즈가 이미 열었을 수 있음) — 같은 값으로 재호출 시 no-op 처리.
- 이 클래스가 **스테이지2 통로 4곳(D0·D2·D4·D5)** 을 전담하고, 나머지 4곳은 텔레포트 게이트(§4.12)다. `ADRAutoSlidingDoor`는 스테이지2에서 사용하지 않는다 (§14.5.4).

### 4.12 `ADRS2TeleportGate` (Actor, bReplicates) — ★신규 확정 (D1, §14.1.6)

문 모델이 **열리지 않고 발광만** 하며, 근접한 플레이어를 목적지로 **순간이동**시킨다. 자동문(`ADRAutoSlidingDoor`)과 성격이 완전히 달라 별개 클래스로 둔다.

```cpp
UCLASS()
class ADRS2TeleportGate : public AActor
{
    // 컴포넌트:
    //   UStaticMeshComponent* GateMesh;      // 문 모델 (개폐 애니메이션 없음)
    //   UBoxComponent* TriggerBox;           // 근접 판정 (Pawn 오버랩 — 판정은 서버 전용)
    //   USceneComponent* DefaultDestination; // 기본 도착 지점
    //
    // 설정:
    //   TObjectPtr<AActor> DestinationOverride;      // EditInstanceOnly: 도착 TargetPoint (있으면 우선)
    //   float DestinationSpreadRadius = 150.f;       // 동시 진입 시 겹침 분산 반경
    //   bool bFaceDestinationRotation = true;        // 도착 시 목적지 방향으로 시선 정렬
    //   ES2GateMode Mode = ES2GateMode::Individual;  // ★방2 출구용 모드 (§14.2.7)
    //
    // UPROPERTY(ReplicatedUsing = OnRep_bActive) bool bActive = false;
    //
    // void SetGateActive(bool);      // 서버 (페이즈가 호출) — 방1 전멸 후 방2 페이즈 OnPhaseStart에서 true
    // OnRep_bActive():               // 문 모델 발광 전환 (머티리얼 스칼라/Emissive 파라미터) + 루프 사운드
    //   → OnGateActiveChanged(bool) BP 훅
    //
    // [서버] OnBeginOverlap: bActive && ADRCharacter && 생존 확인 →
    //   1) 목적지 주변 분산 위치 산출 (캡슐 여유 검사 실패 시 반경 내 재시도)
    //   2) SetActorLocationAndRotation(sweep=false, teleport=true) + Controller->SetControlRotation
    //   3) Multicast_PlayTeleportFX(Character)   // 출발/도착 VFX·사운드 (로컬 화면 페이드는 소유 클라 훅)
    //   4) OnGateUsed 델리게이트 발화 (페이즈가 필요 시 이동 인원 진행도 표시)
    //   ※ attach된 액터(부품 등)는 attach 계층이라 함께 이동 — 부품을 든 채 통과해도 부품이 따라온다
    //
    // UFUNCTION(BlueprintImplementableEvent) void OnGateActiveChanged(bool bNowActive);
    // FOnGateUsed OnGateUsed;         // DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(..., ADRCharacter*)
    // FOnTeamRecalled OnTeamRecalled; // TeamOnCarrier 모드에서 전원 회수 완료 시 (§14.2.7)
};
```

**게이트 모드 / 진입 규칙** (§14.2.7·§14.3.3 확정):

```cpp
UENUM(BlueprintType)
enum class ES2GateMode : uint8
{
    Individual,      // 진입한 캐릭터만 이동 — D1(방1→방2), 방4 복귀 게이트
    TeamOnCarrier    // 부품 소지자가 진입하면 생존자 전원을 목적지로 회수 — 방2 출구
};

UENUM(BlueprintType)
enum class ES2GateEntryRule : uint8
{
    Anyone,          // 누구나 통과 (기본)
    CarrierOnly      // 부품 소지자만 통과 — D3(방3→방4)
};

// 추가 설정:
//   ES2GateEntryRule EntryRule = Anyone;     // 서버 API SetEntryRule()로 런타임 변경 (§14.3.5-A)
//   bool bDeactivateOnUse = false;           // 1명 통과 후 자동 비활성 — D3에서 true (§14.3.3)
```

- `TeamOnCarrier` 동작: 진입자가 `IsCarryingPart()`이면 → `GetAlivePlayers()` 전원을 목적지 주변에 분산 배치 → `OnTeamRecalled` 발화(페이즈가 D1 비활성화 + 완료 처리). 진입자가 부품을 들지 않았으면 **개별 이동만** 수행하고 회수/비활성화는 하지 않는다 (§14.2.9-10).
- `CarrierOnly` + `bDeactivateOnUse` 조합이 **D3(방4 입장)** 을 구성한다: 부품 소지자 1명만 들어가고 즉시 잠긴다. 방4 플레이어가 죽으면 부품이 방4 안에 남으므로 페이즈가 `SetEntryRule(Anyone)`으로 바꿔 재시도를 허용한다 (§14.3.5-A). 접속 종료 시엔 부품을 방3 쪽으로 되돌리고 `CarrierOnly`로 복원한다 (§14.3.5-B).
- 진입 규칙에 걸려 거부될 때는 텔레포트하지 않고 **로컬 안내 연출만** 재생한다 (`OnEntryDenied` BP 훅).
- **왜 서버 판정인가**: 위치 이동은 서버 권한이어야 클라 예측과 어긋나지 않는다. 오버랩 자체는 양쪽에서 발생하지만 텔레포트 실행은 `HasAuthority()`에서만 한다.
- **비활성 상태**에서는 오버랩을 무시하므로, 전투 중 문에 붙어 있어도 이동하지 않는다.

---

## 5. 기존 코드 수정 목록 (공유 파일 — 스테이지1 회귀 주의) ★

| # | 파일 | 변경 내용 | 성격 | 스테이지1 회귀 위험 |
|---|---|---|---|---|
| 5.1 | `DRStageGameMode.cpp` | `InitializePhaseSystem()`의 `if (CleanserSites.Num() < 1) return;`(:226) → 경고 로그 후 계속 진행으로 완화 | 1줄 | **낮음** — 스테이지1 맵은 항상 사이트 ≥1 |
| 5.2 | `DRPhaseBase.h/.cpp` | `void SetupPhaseObjectiveByRow(FName RowName)` 추가 (기존 `SetupPhaseObjective(int32)`는 내부적으로 이를 호출하도록 정리 가능) | 추가 | **없음** (순수 추가) |
| 5.3 | `DRPhaseBase.h` (`FPhaseObjectiveData`) | `FText PhaseAlarmText` 필드 추가 (+ 선택 `bool bIsSubObjective` — 서브 목표 전환 시 알람/배너 생략 판단용) | 구조체 확장 | **낮음** — 기존 DataTable 행은 기본값(빈 텍스트)으로 로드, §5.4의 폴백 유지 |
| 5.4 | `DRStageGameState.h/.cpp` + `OverlayWidgetController.h/.cpp` | ① GameState에 `UPROPERTY(ReplicatedUsing=OnRep_WaveDefenseUIActive) bool bWaveDefenseUIActive` + setter + 로컬 델리게이트 추가. ② `UDRPhase3::OnPhaseStart/OnPhaseEnd`에서 true/false 설정. ③ OverlayWidgetController의 `== 2` 비교 3곳(:34, :459-475, :481-488)을 플래그 구독으로 교체 (`BindCallbacksCleanserSiteToDependencies` 게이트 포함). ④ 알람 텍스트: `OnRep_CurrentPhaseObjective` 경로에서 `PhaseAlarmText`가 비어있지 않으면 브로드캐스트, 비어있으면 기존 switch 폴백 (스테이지1 데이터 갱신 전까지 동작 보존) | 리팩토링 | **중간** — §10 회귀 체크리스트로 검증 (M1에 포함해 조기 안정화) |
| 5.5 | `DRPlayerController.h/.cpp` | ① 감지: 좌석 라인트레이스 슬롯 추가 — `CurrentDetectedSeat` + `FindSeatByLineTrace()` (마운트 감지 :241, :671 미러). ② `HandleInteract()`(:1095)에 좌석 분기 추가 (부품 미소지 && 좌석 감지 → `ServerRequestInteract(Seat)`). ③ `ServerRequestInteract_Implementation`(:371-413)에 `ADRS2TrainSeat` 분기 (CanBeBoardedBy + 거리 검증). ④ 점프 입력 경로의 마운트 하차 분기 옆에 좌석 하차(`ServerRequestTrainDeboard`) 추가 | 분기 추가 | **낮음** — 기존 분기 뒤에 병렬 추가 |
| 5.5-b ★신규 | `DRPlayerController.h/.cpp` | **방2 상호작용 프롭 배관** (§14.2.8) — ① 감지 슬롯 `CurrentDetectedProp` + `FindPropByLineTrace()` (`FindSiteByLineTrace` :326-362 미러, 오버랩 집합 없이 트레이스 거리 + `CanInteract()` 검사). ② `HandleInteract()`에 프롭 분기 추가. ③ `ServerRequestInteract_Implementation`에 `ADRS2InteractProp` 분기 1개 — 거리 재검증(`MaxInteractDistance`) 후 `Prop->ServerHandleInteract(Char)`. ④ 감지 틱(:662-674)에 프롭 슬롯 갱신 추가 | 분기 추가 | **낮음** — 프롭 타입은 스테이지2 전용이라 기존 경로 무영향. **조작 대상 27개를 이 분기 1개로 처리**(타일·레버·버튼 전부 `ADRS2InteractProp` 자식) |
| 5.6 | `DRCleanserPart.h/.cpp` | ① `FOnPartPickedUp OnPartPickedUp` 델리게이트 추가, `PickupPart()`에서 브로드캐스트. ② **`bool IsCarriedNow() const` 공개 접근자 추가** — `bIsCarried`(`:94`)에 공개 접근자가 없어 방3 부품 동반 검증(§15.7.1)에서 필요 | 추가 | **없음** (순수 추가) |
| 5.7 | `DRCharacter.h/.cpp` | §4.9.4 좌석 상태 (`SeatedOn` RepNotify + attach/detach + `Die()` 해제 + 점프 하차 훅) | 추가 | **낮음** — 마운트 경로와 독립 |
| 5.8 | `DRGameplayTags` (선택) | `State.Seated` 태그 (착석 중 스킬 차단이 필요해질 경우에만) | 추가 | 없음 |
| 5.9 ★신규 | `DRCharacterBase.h/.cpp`, `DRCharacter.h/.cpp` | **부활(`Revive`) 신규** — 두더지 클리어 시 사망자를 **방3에서 최대 체력 50%로** 되살린다(§14.3.4). 현재 사망 처리가 폰을 파괴하지 않고 `bDead` 래치 + 콜리전/이동/물리 차단 + Dissolve + 1인칭↔3인칭 메시 스왑 + 사망 카메라 애님으로 구성돼 있어(`DRCharacterBase.cpp:165-204`, `DRCharacter.cpp:1125-1150`) **같은 폰을 되살리는 역연산**으로 구현 가능. 필요 항목: `bDead=false` + `OnRep_Dead` 역경로, 캡슐/메시 콜리전 복원, CMC 복원, Dissolve 머티리얼 원복, 사망 몽타주 정지, 1인칭 메시 재표시 + 3인칭 `SetOwnerNoSee(true)` 복원, GAS 상태(사망 태그/디버프) 정리, **부활 지점으로 이동**, Health = MaxHealth × 0.5, Water 처리(§14.3.6-6), 관전 해제, `Multicast_HandleRevive` 연출 | 신규 시스템 | **중간** — 스테이지1에도 노출되는 공유 경로. 호출부는 `UDRS2DefensePhase` 한 곳뿐이라 실질 회귀 위험은 낮음 |
| 5.10 ★신규 | `DRGameModeBase.h/.cpp` 또는 `DRStageGameMode.h/.cpp` | **`Logout(AController*)` 오버라이드 신규** — 현재 프로젝트에 `Logout`/`PostLogin` 오버라이드가 없다(확인 완료). 방4 플레이어의 접속 종료를 감지해 현재 페이즈에 `NotifyPlayerLeft(PlayerState)`를 전달해야 한다(§14.3.5-B). 겸사겸사 전멸 판정 재평가에도 사용 가능 | 추가 | **낮음** — 신규 오버라이드, 기존 경로 무영향 |
| 5.11 ★신규 | `DRAbilitySystemLibrary.h/.cpp` + 신규 인터페이스 `IDRProximityHitOnly` | **근접 전용 히트 게이트** — `ApplyDamageEffect()`(`:424`)가 프로젝트 모든 데미지의 단일 관문이므로, 진입부에 "대상이 `IDRProximityHitOnly`이면 `AcceptsHitFrom(SourceAvatar)` 확인 후 실패 시 조기 return" 분기 4줄 추가. 두더지의 **2m 밖 공격 투과** 규칙(§14.4.4)을 공격 종류마다 손대지 않고 한 곳에서 구현 | 분기 추가 | **없음** — 인터페이스를 구현한 대상(두더지)에만 적용 |
| 5.12 ★신규 | `DRCleanserSite.h/.cpp` | **`EjectInstalledPart()` 신규** — 방4 플레이어 사망 시 **설치된 부품을 설치대 옆에 다시 떨어뜨리고 설치 상태를 해제**해야 한다(§14.3.5-A). 기존에는 설치가 되돌려지는 경로가 없다. `InstalledPartCount` 감소 + 표시 메시 숨김 + 부품 액터 재활성/스폰 | 추가 | **낮음** — 스테이지1은 이 함수를 호출하지 않음 |

**수정하지 않는 것**: `ADRDoorManager`(스테이지1 전용 유지), `UDRPhase1/3`(§5.4의 플래그 설정 2줄 외), `UDRPhase2`(deprecated — 별도 정리 PR 권장 유지).

---

## 6. 신규 파일 목록

### 6.1 C++ (h/cpp 쌍)

| 파일 | 클래스 | 비고 |
|---|---|---|
| `Phase/Stage2/DRS2Types.h` | `FS2EnemyCount`, `FS2WaveComposition`, `FS2WaveSet` | 헤더 단독 (§4.1 — 인원별 룩업 테이블용으로 재설계됨) |
| `Phase/Stage2/DRS2PhaseBase.h/.cpp` | `UDRS2PhaseBase` | 공통 베이스 (§4.4) |
| `Phase/Stage2/DRS2CombatPhase.h/.cpp` | `UDRS2CombatPhase` | 방1 |
| `Phase/Stage2/DRS2PuzzlePhase.h/.cpp` | `UDRS2PuzzlePhase` | 방2 |
| `Phase/Stage2/DRS2DefensePhase.h/.cpp` | `UDRS2DefensePhase` | 방3+4 |
| `Phase/Stage2/DRS2WavePhase.h/.cpp` | `UDRS2WavePhase` | 방5 |
| `Phase/Stage2/DRS2TrainPhase.h/.cpp` | `UDRS2TrainPhase` | 방6 |
| `Actor/Stage2/DRS2StageDirector.h/.cpp` | `ADRS2StageDirector` | 배선 레지스트리 |
| `Actor/Stage2/DRS2RoomTrigger.h/.cpp` | `ADRS2RoomTrigger` | 전원 입장 판정 |
| `Actor/Stage2/DRS2InteractProp.h/.cpp` ★ | `ADRS2InteractProp`, `ADRS2SlideTile`, `ADRS2Lever`, `ADRS2PuzzleButton`, `ADRS2SafeButton` | 상호작용 프롭 베이스 + 얇은 자식 4종 (§4.10.1) |
| `Actor/Stage2/DRS2SlidePuzzle.h/.cpp` ★ | `ADRS2SlidePuzzle` | 8퍼즐 — 9칸 상태 복제 + 셔플/Undo/Reset (§4.10.2) |
| `Actor/Stage2/DRS2SwitchPuzzle.h/.cpp` ★ | `ADRS2SwitchPuzzle` | 스위치 퍼즐 — 마스크 생성(해 보장) + 3라운드 (§4.10.3) |
| `Actor/Stage2/DRS2CctvBoard.h/.cpp` ★ | `ADRS2CctvBoard`, `FS2CctvStep` | CCTV — 시퀀스 1회 복제 + 로컬 시뮬 (§4.10.4) |
| `Actor/Stage2/DRS2Safe.h/.cpp` ★ | `ADRS2Safe` | 금고 — 코드 검증/개방/부품 스폰 (§4.10.5) |
| `Actor/Stage2/DRS2CodeScreen.h/.cpp` ★ | `ADRS2CodeScreen` | 숫자 표시판 공용 (§4.10.6) |
| ~~`Actor/Stage2/DRS2PuzzleBase.h/.cpp`~~ | ~~`ADRS2PuzzleBase`, `ADRS2PuzzleGroup`~~ | **폐기** — 퍼즐 3종이 구조가 달라 일반화 불가 (§4.10 서두) |
| `Actor/Stage2/DRS2MovingBlocker.h/.cpp` ★ | `ADRS2MovingBlocker` | 상승/하강 구조물 — D0·D2·D4 (§4.11) |
| `Actor/Stage2/DRS2TeleportGate.h/.cpp` ★ | `ADRS2TeleportGate`, `ES2GateMode`, `ES2GateEntryRule` | 발광 + 순간이동 게이트 — D1·E2·D3·R4 (§4.12) |
| `Actor/Stage2/DRS2MoleGame.h/.cpp` ★ | `ADRS2MoleGame`, `FS2MoleTier` | 두더지 게임 관리 (§4.7.1) |
| `Actor/Stage2/DRS2Mole.h/.cpp` ★ | `ADRS2Mole` | 홀로그램 두더지 (Actor + 최소 ASC) (§4.7.2) |
| `Interaction/DRProximityHitOnly.h` ★ | `IDRProximityHitOnly` | 근접 전용 히트 인터페이스 (§5.11) |
| `Actor/Stage2/DRS2Barrier.h/.cpp` | `ADRS2Barrier` | 구간 차단벽 |
| `Actor/Stage2/DRS2TrainTrack.h/.cpp` | `ADRS2TrainTrack` | 스플라인 보유 |
| `Actor/Stage2/DRS2Train.h/.cpp` | `ADRS2Train`, `ES2TrainState`, `FS2TrainMovement` | 이동/좌석 관리 |
| `Actor/Stage2/DRS2TrainSeat.h/.cpp` | `ADRS2TrainSeat` | IDRInteractable |
| `Actor/Stage2/DRS2TrainObstacle.h/.cpp` | `ADRS2TrainObstacle` | 장애물 + 엘리트 스폰점 |

### 6.2 블루프린트 / 데이터 에셋

| 에셋 | 부모 | 핵심 설정 |
|---|---|---|
| `Content/Blueprints/Game/BP_DRStage2GameMode` | `ADRStageGameMode` | `PhaseClasses = [BP_S2CombatPhase, BP_S2PuzzlePhase, BP_S2DefensePhase, BP_S2WavePhase, BP_S2TrainPhase]`, GameStateClass/EnemyCharacterClassInfo/AbilityInfo/GameBalanceConfig 등은 `BP_DRStageGameMode` 값 복사 |
| `Content/Blueprints/Phase/Stage2/BP_S2CombatPhase` 외 4종 | 각 C++ 페이즈 | `PhaseObjectiveDataTable = DT_S2PhaseObjective`, 스폰/웨이브/엘리트/속도 등 수치. **`BP_S2CombatPhase`는 `WaveSetsByPlayerCount`에 §14.1.3 표 4구간(각 2웨이브: 0s / 30s)을 입력** |
| `Content/Blueprints/Data/DT_S2PhaseObjective` | DataTable(`FPhaseObjectiveData`) | §7.2 행 구성 |
| ~~`BP_S2Door`~~ | ~~`ADRAutoSlidingDoor`~~ | **제작 불필요** — 통로 8곳이 전부 구조물/게이트로 확정 (§14.5.4) |
| `BP_S2MovingBlocker` ★ | `ADRS2MovingBlocker` | **상승형**(D0·D2: `bStartBlocked=false`) + **하강형 자식 BP**(D4·D5: `bStartBlocked=true`, `OpenOffset` -Z). 구조물 메시 + 이동 사운드·먼지 VFX + `PushOutPoint` 배선 |
| `BP_S2TeleportGate` ★ | `ADRS2TeleportGate` | 문 모델 + **발광 전환 머티리얼 파라미터**(`OnGateActiveChanged`), 근접 트리거 박스, 텔레포트 VFX/사운드, 목적지 배선. 인스턴스별로 `Mode`/`EntryRule`/`bDeactivateOnUse` 설정 (D1·E2·D3·R4) |
| `BP_S2MoleGame` ★ | `ADRS2MoleGame` | `Tiers` 3구간(§14.4.3 표), `GoalKills=20`, `MaxConcurrentMoles`, `SpawnPoints` 배선 |
| `BP_S2Mole` ★ | `ADRS2Mole` | 홀로그램 메시/머티리얼 + 등장·소멸 VFX·사운드, `ProximityRadius=200`, 최소 어트리뷰트(MaxHealth=1) GE |
| `BP_S2StageDirector`, `BP_S2RoomTrigger`, `BP_S2Barrier` | 각 C++ | — |
| `BP_S2InstallStation` | `ADRCleanserSite` | `RequiredPartsCount = 1`(§11.A-2), 태그 `CleanserSite`, 스테이지2 외형 |
| `BP_S2Part` | `ADRCleanserPart` (기존 BP 복제) | 외형만 교체 가능. 금고 내부에서 스폰됨 |
| `BP_S2SlidePuzzle` ★ | `ADRS2SlidePuzzle` | 3×3 보드 + 타일 8(ChildActor) + 완성 사진 플레인 + 스크린 + 되돌리기/초기화 버튼. `CellSize`/`ShuffleMoves`/`SlideDuration` |
| `BP_S2SwitchPuzzle` ★ | `ADRS2SwitchPuzzle` | 레버 5(ChildActor) + 전구 9 + 성공 표시 3 + 스크린. 전구 점등 머티리얼 파라미터 |
| `BP_S2CctvBoard` ★ | `ADRS2CctvBoard` | 화면 6개 메시 + 머티리얼 인스턴스(텍스처 파라미터), `TargetImage`/`DummyImages`/`ErrorImage`, `StepDuration=2`, `StepCount` |
| `BP_S2Safe` ★ | `ADRS2Safe` | 금고 본체 + 문(개방 연출) + 숫자 버튼 0~9(ChildActor) + 초기화 버튼 + 입력 표시창 + 부품 스폰 지점 |
| `BP_S2SlideTile` / `BP_S2Lever` / `BP_S2PuzzleButton` / `BP_S2SafeButton` ★ | 각 `ADRS2InteractProp` 자식 | 메시 + "F" 프롬프트 위젯 + 누름/당김 연출 |
| `BP_S2CodeScreen` ★ | `ADRS2CodeScreen` | 숫자 아틀라스 머티리얼 (0~9), 자리별 표시 |
| `BP_S2Train`, `BP_S2TrainSeat`, `BP_S2TrainObstacle`, `BP_S2TrainTrack` | 각 C++ | 칸 메시 4개 + 좌석 ChildActor 4개 구성 |
| 엘리트 | 기존 `EliteBossClass` BP 재사용 | 필요시 스탯 조정 자식 BP |

---

## 7. UI / 목표 데이터 설계

### 7.1 표시 채널 재사용 맵

| 정보 | 채널 | 근거 |
|---|---|---|
| 목표 제목/진행도 | `SetPhaseObjective` + `UpdatePhaseObjectiveProgress` (복제) → `OnObjectiveTextChanged/OnObjectiveProgressChanged` | `OverlayWidgetController.h:128-137` |
| 페이즈 클리어 연출 | `Multicast_ObjectiveCompleted` (자동) | `DRStageGameMode.cpp:321` |
| 방5 웨이브 번호 | `SetCurrentWaveNumber/SetTotalWaves` (복제 재활용) | `DRStageGameState.h:303-309` |
| 열차 엘리트 HP | 엘리트 머리 위 빌보드 HP바 (기존 `DREnemy::HealthBar`) — 별도 UI 불요 | `DREnemy.h:239-240` |
| 웨이브 타이머/클렌저 HP UI | 스테이지2에서는 **항상 숨김** (`bWaveDefenseUIActive=false` 유지) | §5.4 |

### 7.2 `DT_S2PhaseObjective` 행 설계 (RowName / PhaseNumber / 제목 / RequiredCount / AlarmText)

| RowName | 제목 (ObjectiveTitle) | RequiredCount | PhaseAlarmText | 비고 |
|---|---|---|---|---|
| `S2P1_Enter` | 1번방으로 이동하라 | 런타임(생존 인원) | "구역 1: 돌입" | 서브 |
| `S2P1_Combat` | 적을 모두 처치하라 | **런타임 = `TotalSpawnCount`** (§14.1.3 총합: 1인 6 / 2인 8 / 3인 11 / 4인 13) | (빈 값 = 알람 생략) | 진행도 = 누적 처치 수. 웨이브 번호 표시 여부는 §14.1.7-8 미정 |
| `S2P2_Move` ★ | 빛나는 문으로 이동하라 | 0 | "구역 2: 수수께끼" | 방2 페이즈 `OnPhaseStart`에서 D1 게이트 발광과 함께 표시 |
| `S2P2_Puzzle` | 금고의 비밀번호를 알아내라 | **2** | (빈 값) | 진행도 = 스크린에 숫자가 뜬 퍼즐 수(8퍼즐·스위치). **CCTV는 해결 개념이 없어 분모에 포함하지 않음** (§14.2.4) |
| `S2P2_Part` | 부품을 획득하라 | 0 | (빈 값) | 부품 1개이므로 0으로 세팅해 숫자 숨김 (Max==0 규칙, `OverlayWidgetController.h:135-137`) |
| `S2P2_Return` ★ | 부품을 들고 1번방으로 돌아가라 | 0 | (빈 값) | 부품 픽업 후 표시. 출구 통과 시 전원 회수 → 페이즈 완료 (§14.2.7) |
| `S2P3_Enter` | 부품을 들고 3번방으로 이동하라 | 런타임(인원) | "구역 3: 농성" | |
| `S2P3_Enter4` ★ | 부품을 들고 4번방으로 들어가라 | 0 | (빈 값) | 방3 전원 입장 후. D3 게이트 발광과 함께 표시 |
| `S2P3_Hold` ★ | 4번방 작업이 끝날 때까지 버텨라 | **20** | (빈 값) | 진행도 = 두더지 처치 수(n/20) — 방3 인원에게도 공유 (§14.4.5-8) |
| ~~`S2P3_Install`~~ / ~~`S2P3_Cleanup`~~ | — | — | — | **폐기** — 퍼즐 선행 게이트와 잔적 처치 단계가 §14.3·§14.4 확정으로 사라짐 |
| `S2P4_Enter` | 5번방으로 이동하라 | 런타임(생존 인원) | "구역 5: 항전" | |
| `S2P4_Wave` | 웨이브를 막아내라 | 3 | (빈 값) | 진행도 = 현재 웨이브 번호(i+1). 웨이브 시작 사운드는 매 웨이브 재생 (§14.5.5-4~5) |
| `S2P5_Board` | 열차에 탑승하라 | 런타임(생존 인원) | "구역 6: 탈출" | 재탑승 시에도 재사용 |
| `S2P5_Ride` | 이동 중… | 0 | (빈 값) | |
| `S2P5_Boss` ★ | 두더지를 몰아내라 | 3 | (빈 값) | 진행도 = 현재 구간 번호(i+1). 3구간에서는 "쓰러뜨려라" 계열 문구로 교체 검토 (§14.6.8-9) |
| ~~`S2P5_Arrive`~~ | — | — | — | **폐기** — 최종 처치가 곧 클리어라 도착지점 단계가 없다 (§14.6.6) |

- 스테이지1의 `DT_PhaseObjective`에도 `PhaseAlarmText` 채우기 (기존 잘못된 "수집" 표기 교정 기회 — 선택).

---

## 8. 레벨(에디터) 작업 체크리스트 — `Stage2.umap`

1. **WorldSettings**: GameMode Override = `BP_DRStage2GameMode` (GameState는 `BP_DRStageGameState` 유지 — DoorManager 등록 등 기존 기능 필요).
2. **PlayerStart** ×4 — 시작지점 구역.
3. **통로 8곳** (§1.1 표대로):
   - **D0 = `BP_S2MovingBlocker`** — 시작지점↔방1 통로. 내려간(통행 가능) 상태 + `BlockedOffset`을 통로 높이 이상으로. `PushOutPoint`를 방1 쪽 안전 지점에 배선.
   - **D1 = `BP_S2TeleportGate`** (Mode=`Individual`) — 방1의 방2행 문 위치, 비활성 시작. **목적지 TargetPoint를 방2 안에** 배치.
   - **E2 = `BP_S2TeleportGate`** (Mode=`TeamOnCarrier`) — 방2 출구. 목적지 = 방1 회수 지점 TargetPoint.
   - **D2 = `BP_S2MovingBlocker`** — 방1↔방3 통로. 통행 가능 상태 시작 + `PushOutPoint`를 방3 쪽에 배선.
   - **D3 = `BP_S2TeleportGate`** (Mode=`Individual`, EntryRule=`CarrierOnly`, `bDeactivateOnUse=true`) — 방3의 방4행 문. 목적지 = 방4 입구 TargetPoint.
   - **R4 = `BP_S2TeleportGate`** (Mode=`Individual`) — 방4 안 복귀 게이트, 비활성 시작. 목적지 = 방3 안 TargetPoint.
   - **D4 = `BP_S2MovingBlocker`** (하강형: `bStartBlocked=true`, `OpenOffset` = -Z) — 방3↔방5 통로를 **막고 있는 상태로** 배치. 왕복 동작이므로 `PushOutPoint`를 **방5 쪽**에 배선(재봉쇄 시 끼임 방어).
   - **D5 = `BP_S2MovingBlocker`** (하강형: `bStartBlocked=true`) — 방5↔방6 통로를 **막고 있는 상태로** 배치.
4. **`BP_S2StageDirector` 1개** — 모든 참조 배선 (누락 시 BeginPlay Error 로그로 검출).
5. **룸 트리거 4개**: 방1/방3/방5 전체를 덮는 박스 + 도착지점. 방 경계(문 안쪽)까지 충분히 크게. **방1 트리거는 D0 통로 입구 안쪽까지 덮어** 전원 입장 발화 시 통로에 사람이 남지 않게 한다 (차단물 끼임 방어와 세트).
6. **스폰 포인트** (TargetPoint, Director 배열에 등록): 방1 **최소 6개**(4인 웨이브1이 9마리 동시 스폰 — §14.1.3) / 방3 4~6개(4인 8마리) / 방5 4~6개 + **잠자리용 공중 지점 2개**(4인 웨이브3에 잠자리 3마리 — §14.5.2, §14.5.5-3). 방5는 잠자리 비중이 높으므로 **천장 높이 확보 확인** 필요(§14.5.5-6).
7. **방2** (§14.2): ① `BP_S2SlidePuzzle` — 타일 8개 격자 정렬 확인 + 옆에 완성 사진판 + 스크린 + 되돌리기/초기화 버튼. ② `BP_S2SwitchPuzzle` — 레버 5 / 전구 9 / 성공 표시 3 / 스크린. ③ `BP_S2CctvBoard` — 화면 6개(6개 CCTV 아트 에셋 활용 가능: `Map/Stage2_v1/cctv1~6`). ④ `BP_S2Safe` — 숫자 버튼 0~9 위치 확인 + 내부 부품 스폰 지점. ⑤ **방2 출구 게이트** `BP_S2TeleportGate`(Mode=`TeamOnCarrier`, 목적지 = 방1 회수 지점 TargetPoint). ⑥ D1 게이트의 목적지 TargetPoint를 방2 안에 배치. ※ 모든 프롭이 `ECC_Visibility` 트레이스에 걸리도록 콜리전 확인 필요.
8. **방3/방4** (§14.3·§14.4):
   - 방3: 스폰 지점 4~6개(4인 8마리 동시 수용), **부활 지점 4개**(`Room3RevivePoints`), **`Room4EntranceDropPoint`**(D3 게이트 앞, 접속 종료 시 부품 위치).
   - 방4: `BP_S2InstallStation`을 **중앙**에 배치 (태그 `CleanserSite` 확인 — GameMode 사이트 스캔 `DRStageGameMode.cpp:140-147` 통과 목적), `BP_S2MoleGame` 1개 + **두더지 등장 지점 9~12개**를 바닥에 격자 배치, R4 복귀 게이트.
   - ※ 방4는 텔레포트로만 진입하는 분리 공간이라 방3과 NavMesh가 이어지지 않아도 된다 (적 추격 차단용 NavModifier 불필요).
9. **방6/선로** (§14.6): `BP_S2TrainTrack` 스플라인을 **ㄷ자 + 코너 곡선**으로 편집 → `BP_S2Train`(좌석 4개) 시점 배치 → `BP_S2TrainObstacle` ×3 (`StopDistance` 입력 + **`BossSpawnPoint`** 배치) → **`BP_S2Barrier` ×6 (전방 3 + 후방 3)** — 통로 폭 전체를 덮는지 확인 (§14.6.5). ※ 도착 플랫폼/`Trigger_Destination`은 **불필요**(§14.6.6).
10. **NavMesh**: 방1~5 + **선로변 전투 구간 3곳**(전방~후방 배리어 사이) 커버 — 두더지 보스가 하차한 플레이어를 추적해야 함.
11. **로비 연결**: `LobbyMap`에 `ADRStageSelectActor` 추가 배치, `DestinationMapName = "Stage2"`.
12. **BGM/사운드**: `ADRBGMActor` 배치 (Phase3의 엘리트 BGM 전환 로직은 스테이지1 전용이므로, 열차 엘리트 BGM 전환이 필요하면 `UDRS2TrainPhase`에서 동일 패턴 호출 — `DRPhase3.cpp:1314-1328` 참고).

---

## 9. 구현 순서 (마일스톤) — 6개 방 확정 사양 기준 재분배 (2026-08-07)

6개 방 사양이 모두 확정되면서 방별 실제 작업량이 드러났다. **방2(퍼즐 3종 + 금고)와 방3+4(무한 방어 + 두더지 + 부활)가 원안 가정보다 훨씬 크고, 방1·방5는 작다.** 반대로 설치대 체력 미도입(§11.A-9), 자동문 미사용(§14.5.4), 도착지점 판정 폐기(§14.6.6)로 사라진 작업도 있다. 이를 반영해 큰 마일스톤을 **검증 가능한 서브 단계로 분할**했다.

- 상위 번호(M0~M8)는 문서 곳곳의 기존 참조를 깨뜨리지 않도록 유지하고, 서브 단계를 추가했다.
- **규모**는 상대 크기다 (S < M < L < XL). 절대 일정은 팀 속도에 따라 결정한다.
- ★ **각 마일스톤의 실제 구현 방법(시그니처·알고리즘·검증)은 §15에 마일스톤 번호별로 정리되어 있다.**

### 9.1 의존 관계

```
                      ┌─ M2 방1 (S) ─┬─ M4 방3+4 (XL) ─┐
M0 사양확정 ─ M1 인프라 (L) ─┤              └─ M5 방5 (S) ────┤
                      └─ M3 방2 (XL) ──────────────────┼─ M7 폴리시 ─ M8 통합
                      └─ M6 방6 열차 (L) ──────────────┘
      [별도 트랙 A: 아트/에셋]  ─────────────────────────┘
      [별도 트랙 B: 두더지 보스 스킬 §14.6.7] ────────────┘
```

- **M1이 전체의 선행 조건**이며 병렬화가 불가능하다 → 최우선.
- M1 이후 **M2·M3·M6은 서로 독립**이라 인원이 있으면 병행 가능하다.
- **M4는 M2 이후**가 자연스럽다 (인원별 웨이브 스폰 헬퍼를 M2에서 처음 만들고 M4·M5가 재사용).
- **M5는 M4 이후**가 자연스럽다 (D4 왕복 동작이 M4에서 열리고 M5에서 다시 닫힌다).
- **크리티컬 패스: M1 → M3 → M8.** M3(방2)가 단일 최대 작업이므로 여기에 인원을 먼저 배치한다.

### 9.2 마일스톤 상세

| M | 규모 | 내용 | 완료 기준 (게이트) |
|---|---|---|---|
| **M0** | — | 사양 확정 — **완료 (2026-08-07)**. 6개 방 전부 §14에 SSOT로 기록. 잔여는 값·에셋뿐(§14 서두) | — |
| **M1.1** | M | **공유 코드 리팩토링**: §5.1 사이트 가드 완화, §5.2 `SetupPhaseObjectiveByRow`, §5.3 `PhaseAlarmText` 컬럼, §5.4 `bWaveDefenseUIActive` 플래그 전환(하드코딩 `PhaseIndex == 2` 제거) | **스테이지1 회귀 체크(§10.2) 통과 ★1회차** — 스테이지2 코드가 붙기 전에 공유 코드 변경만 먼저 검증 |
| **M1.2** | M | **기반 타입·액터**: `DRS2Types`(§4.1), `UDRS2PhaseBase`(인원별 조회·웨이브 스폰·타이머 관리), `ADRS2StageDirector`(배선 검증 로그), `ADRS2RoomTrigger` | Director 미배선 시 BeginPlay Error 로그로 전부 검출 |
| **M1.3** | M | **통로 액터 2종**: `ADRS2MovingBlocker`(§4.11 — D0·D2·D4·D5 4곳 전부 커버, 왕복/멱등 포함), `ADRS2TeleportGate`(§4.12 — `Mode`·`EntryRule`·`bDeactivateOnUse` 전부 구현) | 치트로 열고 닫아 상승/하강 연출·콜리전 전환·텔레포트 분산 배치가 서버/클라 동일 |
| **M1.4** | M | **페이즈 5개 골격 + 배선**: 목표 텍스트만 있고 트리거 입장 시 즉시 완료되는 골격, `BP_DRStage2GameMode`/`DT_S2PhaseObjective`/WorldSettings, **통로 8곳 배치**, `StageId` 설정, 로비 포털 연결 | PIE 2인: 방 이동만으로 5페이즈 순차 전환 → 게임 클리어 → 로비 복귀 |
| **M2** | **S** | **방1 (§14.1)**: 인원별 룩업 조회(`ResolveBasePlayerCount`/`ResolveWaveSet`), 웨이브 2개 시간 예약(0s/30s), `PendingSpawnCount` 전멸 판정, D0 봉쇄 + D1 게이트 연동 | 1·2·4인 PIE에서 §14.1.3 표대로 스폰 / 웨이브1을 30초 전에 전멸시켜도 페이즈가 끝나지 않음 / 전멸 → D1 발광 → 순간이동 |
| **M3.1** | L | **방2 흐름 골조**: `ADRS2InteractProp`(§4.10.1) + PlayerController 배관(§5.5-b), `ADRS2Safe`(§4.10.5) + `ADRS2CodeScreen`, 페이즈의 코드 생성·배분(§14.2.6), 출구 게이트 `TeamOnCarrier` + D1 비활성(§14.2.7) | ★**퍼즐 없이도 방2 전체 흐름이 도는 상태**: 치트로 코드 확인 → 금고 물리 버튼 입력 → 개방 → 부품 픽업 → 출구 통과 → 전원 방1 회수 → D2 개방 |
| **M3.2** | L | **퍼즐 2종** (서로 독립, 병렬 가능): `ADRS2SlidePuzzle`(보드 1개 복제 + 타일 로컬 보간, **빈칸 이동 셔플**, Undo 1수, Reset), `ADRS2SwitchPuzzle`(**해 보장 마스크 생성 + 32조합 검증**, 3라운드) | 셔플 100회 생성 전부 해결 가능 / 매 라운드 정답 조합 존재 / 2인 동시 조작 시 상태 일관 / 해결 시 스크린에 자리 숫자 공개 |
| **M3.3** | M | **CCTV**: `ADRS2CctvBoard` — 유한 시퀀스 생성(타깃 N개 배치) + 순환 반복 + 서버시각 로컬 시뮬 이미지 교체 | 1주기 관찰 시 타깃 등장 횟수 == 금고 3번째 자리 / N=0 케이스 동작 / 서버·클라 화면 동일 |
| **M4.1** | M | **방3 방어 루프 (§14.3)**: D2 상승 봉쇄 + 부품 동반 검증, D3 `CarrierOnly` 게이트(1인 입장 후 잠김), 설치 트리거 → **50초 주기 무한 웨이브**(인원별 구성) | 부품 없이 전원 입장 시 봉쇄 거부 / 부품 미소지자 D3 진입 거부 / 설치 순간 두더지·웨이브 동시 시작(두더지는 스텁) |
| **M4.2** | L | **두더지 게임 (§14.4)**: `ADRS2Mole`(Actor + 최소 ASC + `Enemy` 태그) / `ADRS2MoleGame`(티어 3구간·동시 상한), **2m 근접 히트 게이트**(§5.11 `IDRProximityHitOnly` + `ApplyDamageEffect` 분기) | 2m 밖 원거리 공격 무반응(투과) / 2m 이내 1히트 소멸 / 5·12킬에서 티어 전환 / 20킬 달성 |
| **M4.3** | L | **클리어·부활·예외 처리**: 클리어 4단 처리(D3 재활성 / **부활 §5.9** / D4 하강 개방 / 스폰 중단 + 잔적 즉시 사망), `Logout` 오버라이드(§5.10), `EjectInstalledPart`(§5.12), **예외 A·B**(§14.3.5) | 20킬 시 4단 처리 동시 확인 / **부활 후 조작·관전 해제 정상** / 방4 사망 시 D3가 `Anyone`으로 재활성(★소프트락 방지) / 강제 종료 후 재시도 성공. **스테이지1 회귀 체크 ★2회차**(부활이 공유 경로) |
| **M5** | **S** | **방5 (§14.5)**: D4 재봉쇄, 인원별 3웨이브, **하이브리드 전환**(30초 타이머 + 전멸 시 취소·즉시 스폰), 마지막 웨이브 타이머 미예약, D5 하강 개방 | 표대로 스폰 / 30초 전 전멸 시 즉시 다음 / 방치 시 30초에 겹쳐 스폰 / **웨이브3 뒤 4번째가 안 나오는지** / 웨이브3 전멸 시에만 완료 |
| **M6.1** | M | **선로·열차 이동**: `ADRS2TrainTrack`(ㄷ자 스플라인), `ADRS2Train`(§4.9.2 — `FS2TrainMovement` 1회 복제 + 로컬 시뮬) | 치트 출발로 등속 주행 / 코너 곡선 통과 / 클라 화면에서 위치 부드러움 |
| **M6.2** | L | **좌석·탑승**: `ADRS2TrainSeat`(§4.9.3), `ADRCharacter` 좌석 상태(§5.7), PlayerController 좌석 분기(§5.5), 생존자 전원 착석 시 출발 | 중복 탑승 거부 / 빈 칸 허용 출발 / 이동 중 하차 거부 / 탑승자 사망 시 좌석 해제. **마운트 경로 회귀 확인**(§10.2) |
| **M6.3** | L | **장애물·보스 루프 (§14.6)**: `ADRS2TrainObstacle`(등장과 동시 파괴), `ADRS2Barrier` **전·후방 6개**, 보스 3구간 루프(**임계 67%/34% → 비활성 보관 → 재등장**), 최종 처치 → 클리어 | 정지 → 보스가 장애물 부수며 등장 → 임계 도달 → 도망 → 재탑승 → 재출발 3회 / ★**구간2·3 재등장 시 체력이 이어지는지** / 3구간 처치 시 즉시 스테이지 클리어 |
| **M7** | M | **폴리시**: 목표 문구·알람 텍스트, 사운드/BGM(보스 BGM 전환), 연출 훅 채우기(구조물 이동, 게이트 발광, 금고 개방, 두더지 등장/소멸, 장애물 파괴, 보스 도망/재등장), **아트 에셋 교체**(임시 프리미티브 → 실제 메시), 영문 텍스트(§C2) | 데모 시연 가능 품질 |
| **M8** | M | **통합·밸런스**: §10.1 테스트 매트릭스 전체, 1~4인 각 인원 완주, 밸런스 수치가 BP만으로 튜닝 가능한지 확인, `StageId` 보상 정의 | §10.1 전 항목 통과. **스테이지1 회귀 체크 ★3회차** |

### 9.3 별도 트랙 (마일스톤과 병렬)

| 트랙 | 내용 | 차단 여부 |
|---|---|---|
| **A. 아트/에셋** | 열차 4칸·선로·장애물·이동 구조물·홀로그램 두더지·금고·퍼즐 프롭(타일/레버/버튼)·CCTV 화면 메시가 **현재 전부 미제작**(§2.4 확인 — `Stage2_v1`에 방 지오메트리만 존재, `LEVER` 프롭 1종). M1~M6는 **임시 프리미티브로 진행**하고 M7에서 교체 | **논블로킹** — 단 M7 전에 확보 필요 |
| **B. 두더지 보스 스킬·패턴** | §14.6.7 — 별도 작업으로 분리 확정. M6.3은 `EliteBear` 계열 임시 클래스로 루프 전체를 검증하며, 페이즈는 `MoleBossClass`와 체력 델리게이트만 참조하므로 보스 교체가 페이즈 코드에 영향 없음 | **논블로킹** |
| **C. 잔여 값 결정** | 각 방 "잔여 확인 항목" 총 56건(§14.1.7 / §14.2.9 / §14.3.6 / §14.4.5 / §14.5.5 / §14.6.8). 전부 기본 진행값이 지정되어 있음 | **논블로킹** — M8 밸런스에서 확정 |

### 9.4 스테이지1 회귀 체크 배치 (§10.2)

공유 코드를 건드리는 시점마다 배치해 원인 추적을 쉽게 한다.

| 회차 | 시점 | 대상 변경 |
|---|---|---|
| 1 | **M1.1 직후** | §5.1~5.4 (UI 플래그·목표 데이터 리팩토링) |
| 2 | **M4.3** | §5.9 부활, §5.10 `Logout`, §5.11 데미지 관문, §5.12 `EjectInstalledPart` |
| 3 | **M8** | §5.5 PlayerController(좌석·프롭 분기), §5.7 캐릭터 좌석 상태 포함 전체 |

### 9.5 규모 요약

| 마일스톤 | 규모 | 비고 |
|---|---|---|
| M1 (4단계) | **L** | 크리티컬 패스 선두, 병렬 불가 |
| M2 방1 | S | 인원별 웨이브 헬퍼의 첫 사용처 |
| M3 방2 (3단계) | **XL** | 단일 최대 — 독립 미니게임 3종 + 금고 |
| M4 방3+4 (3단계) | **XL** | 두 번째로 큼 — 부활·예외 처리 포함 |
| M5 방5 | S | M2 인프라 재사용 |
| M6 방6 (3단계) | **L** | M2~M5와 병렬 가능 |
| M7 / M8 | M / M | 아트 트랙 합류 지점 |

---

## 10. 테스트 계획

### 10.1 스테이지2 시나리오 (PIE: 호스트 단독 / 호스트+클라 2인 각각)

| 분류 | 케이스 | 기대 |
|---|---|---|
| 입장 판정 | 1명만 방1 입장 | 미발동, 진행도 1/2 표시 |
| | 전원 입장 | **D0 차단물 상승 → 봉쇄** + 웨이브1 스폰 |
| | 대기 중 방 밖 1명 사망 | 0.5s 내 재평가 → 생존자 기준 충족 시 발동 |
| 방1 (§14.1) | 1인 / 2인 / 3인 / 4인 각각 플레이 | §14.1.3 표와 스폰 마리 수·종류가 정확히 일치 (총 6/8/11/13) |
| | 웨이브1을 30초 전에 전멸 | **페이즈 완료되지 않음**(`PendingSpawnCount > 0`) → 30초에 웨이브2 스폰 |
| | 웨이브1 진행 중 1명 사망 | 웨이브2 구성 변경 없음 (`BasePlayerCount` 고정 — §14.1.4) |
| | 전원 입장 순간 통로 경계에 걸친 플레이어 | 차단물 상승 완료 시 `PushOutPoint`로 이동, 끼임/천장 관통 없음 |
| | 차단물 상승 후 시작지점 복귀 시도 | 차단됨 (영구 봉쇄) |
| | 전멸 전 D1 게이트에 접근 | 발광 없음 + 텔레포트 안 됨 |
| | 전멸 후 D1 게이트 접근 | 발광 확인(전 클라) → 근접 시 방2로 순간이동 |
| | 2명이 동시에 게이트 진입 | 각자 분산 위치로 도착, 겹침/바닥 관통 없음 |
| | 클라이언트에서 차단물 상승·게이트 발광 | 서버와 동일하게 보임 (복제 1회 + 로컬 시뮬) |
| 방2 (§14.2) | 8퍼즐: 빈칸에 인접하지 않은 타일 조작 | 무시(이동 없음), 페널티 없음 |
| | 8퍼즐: 2명이 동시에 서로 다른 타일 조작 | 서버 직렬 처리로 보드 상태 일관, 클라 슬라이드 연출 동일 |
| | 8퍼즐: 되돌리기 연속 2회 | 1회만 동작(직전 1수만 보관) |
| | 8퍼즐: 초기화 | 최초 셔플 배치로 복귀(재셔플 아님) |
| | 8퍼즐: 셔플 배치의 해결 가능성 | 100회 반복 생성 검증 — 항상 해결 가능 (빈칸 이동 방식) |
| | 스위치: 32조합 완전탐색 | 각 라운드에 **정답 조합이 반드시 존재** |
| | 스위치: 라운드 성공 | 성공 표시 1개 발광 + 마스크 재생성 + 레버 전부 OFF |
| | 스위치: 3라운드 완료 | 스크린에 2번째 자리 표시 |
| | CCTV: 시퀀스 1주기 관찰 | 타깃 이미지 등장 횟수 == 금고 3번째 자리 |
| | CCTV: 타깃 개수가 0인 판 | 타깃 이미지 미등장 → 3번째 자리 0으로 금고 개방 |
| | CCTV: 클라이언트/서버 화면 비교 | 같은 시각에 같은 화면 (시퀀스+서버시각 로컬 시뮬) |
| | 금고: 오답 3자리 입력 | 입력만 초기화, 페널티·리셋 없음, 재입력 가능 |
| | 금고: 정답 입력 | 문 개방 + 내부 부품 노출 → 픽업 가능 |
| | 금고: 퍼즐 해결 전 정답 자리 정보가 클라에 있는지 | `RevealedDigit = -1`, 마스크·정답 코드 미복제 (§14.2.6) |
| | 출구: 부품 미소지자 통과 | 개별 이동만, D1 유지(방2 재입장 가능) |
| | 출구: 부품 소지자 통과 | **생존자 전원 방1로 회수** + D1 비활성 + 페이즈 완료 |
| | 부품 픽업 후 드롭하고 빈손으로 출구 통과 | 회수 없음 — 부품을 다시 들고 나가야 완료 (소프트락 아님) |
| | 부품 운반자 사망 | `ForceDropCarriedPart`로 부품 드롭 → 다른 플레이어가 재획득 후 퇴장 가능 |
| 방3 (§14.3) | **부품을 방1에 두고 전원 방3 입장** | 봉쇄 거부(구조물 안 올라옴), 트리거 재무장 — 소프트락 없음 |
| | 전원 방3 입장 | D2 구조물 상승·봉쇄 + 방1 복귀 불가 + D3 게이트 발광 |
| | **부품 미소지자가 D3 진입** | 거부(텔레포트 없음) + 안내 연출 |
| | 부품 소지자가 D3 진입 | 방4로 이동 + **게이트 즉시 비활성**(2번째 사람 진입 불가) |
| | 설치 전 방3 대기 | 웨이브 미시작(설치가 트리거) |
| | 부품 설치 | 두더지 시작 + **같은 순간 방3 첫 웨이브 스폰** |
| | 1/2/3/4인 각각 웨이브 구성 | §14.3.2 표와 일치(4/5/6/8마리), 50초 주기 반복 |
| | 두더지 클리어 | ① D3 재활성 ② 사망자 방3에서 체력 50% 부활 ③ D4 하강 개방 ④ 스폰 중단 + **잔적 즉시 전멸** 이 동시에 발생 |
| | 부활한 플레이어 조작 | 이동/공격/스킬 정상, 관전 해제, 1인칭 메시 복원 (§5.9 복원 항목 누락 검사) |
| 방4 (§14.4) | **2m 밖에서 원거리 공격** | 두더지 무반응(투과) — 데미지 이벤트 자체가 무시됨 |
| | 2m 이내 공격 | 1히트에 소멸 + 처치 수 증가 |
| | 유지 시간 내 미공격 | 소멸 후 다른 위치에 재등장, 처치 수 변화 없음 |
| | 처치 수 5 / 12 도달 | 다음 스폰부터 티어 2 / 3 수치 적용 (§14.4.3) |
| | 동시 등장 마리수 | `MaxConcurrentMoles` 이하 유지 |
| | 20마리 달성 | 클리어 처리 5단계 실행 |
| | **방4 플레이어 사망** | 두더지 전부 소멸 + 처치 수 0 + **부품이 설치대 옆에 드롭** + D3가 `Anyone`으로 재활성 → 다른 1명이 입장해 재시도 가능. **방3 웨이브는 유지** |
| | **방4 플레이어 접속 종료** | 방3 몬스터 전부 소멸 + 스폰 중단 + **부품이 D3 문 앞(방3)에 배치** + D3가 `CarrierOnly`로 재활성 → 재입장·재설치 시 두더지·웨이브 동시 재개 |
| | 재시도 시 진행도 | 처치 수 0부터 (누적되지 않음) |
| 방5 (§14.5) | 1인 / 2인 / 3인 / 4인 각각 3웨이브 | §14.5.2 표와 정확히 일치 (총 9/11/16/20마리) |
| | 전원 방5 입장 | **D4 구조물 재상승 봉쇄** → 방3 복귀 불가 |
| | 웨이브를 30초 전에 전멸 | 예약 타이머 취소 + **즉시 다음 웨이브** + 새 30초 타이머 |
| | 웨이브를 방치(30초 경과) | 잔존 적이 있어도 다음 웨이브 스폰(겹침 허용) |
| | 겹친 상태에서 전체 전멸 | 다음 웨이브 즉시 스폰 (웨이브 단위가 아니라 전체 생존 0 기준) |
| | **웨이브3 전멸** | 페이즈 완료 → D5 하강 개방. **4번째 웨이브가 스폰되지 않는지** 확인(마지막 타이머 미예약) |
| | 웨이브1·2 전멸 시점 | 페이즈가 완료되지 않음 (마지막 웨이브 아님) |
| | 웨이브 시작 UI/사운드 | 웨이브 번호 1/3 → 2/3 → 3/3 갱신 + 매 웨이브 사운드 |
| 열차 (§14.6) | 한 좌석 중복 탑승 시도 | 거부 |
| | 부품 소지 탑승 시도 | 거부 (이론상 불가 상태지만 방어) |
| | 생존 4명 중 3명만 탑승 | 출발 안 함, n/N 표시 |
| | 생존 3명이 3칸만 채움 (1칸 빈 칸) | **출발** (§11.A-6: 빈 칸 허용 — 조건은 생존자 전원 착석) |
| | 이동 중 점프(하차 시도) | 무시 |
| | 장애물 앞 정지 | **보스가 장애물을 부수며 등장** + 전·후방 배리어 활성 |
| | 정지 중 하차 → 전방/후방 이탈 시도 | 양쪽 배리어로 차단 (장애물이 부서져도 전방 유지) |
| | **구간1에서 체력 67% 도달** | 보스 도망(숨김 보관) + 전방 배리어 해제 + "재탑승" 목표 |
| | **구간2에서 보스 재등장 시 체력** | **67%에서 이어짐** (100% 리셋되지 않는지 ★핵심 검증) |
| | **구간2에서 34% 도달 → 구간3 재등장** | **34%에서 이어짐** |
| | 구간3에서 임계 없이 계속 딜 | 도망 없음 — 처치까지 전투 지속 |
| | **구간3 보스 처치** | **즉시 스테이지2 클리어** → 게임 클리어 사운드/UI → 로비 (도착지점 이동 단계 없음) |
| | 구간1·2에서 임계를 지나쳐 즉사시킴(과도한 폭딜) | 해당 구간이 해제 처리되어 진행 (소프트락 없음) |
| | 보스 도망 중 남은 체력바 UI | 다음 구간에서 이어진 값으로 표시 |
| | 탑승자 사망 | 좌석 해제, 출발 조건 재계산 |
| | 전투 중 전원 사망 | 게임오버 → 로비 복귀 |
| 치트 | `ServerCheatSkipToNextPhase` 각 페이즈에서 | 다음 페이즈로 전환 + 정리 누수 없음 (문 상태는 스킵 특성상 수동 보정 허용) |

### 10.2 스테이지1 회귀 체크 (M1 직후 + M8)

- 페이즈1→방어 페이즈 전환, 목표/알람 배너 텍스트 정상 (§5.4 폴백 경로).
- 방어 페이즈에서 웨이브 타이머 UI + 클렌저 HP UI 표시(`bWaveDefenseUIActive=true`), 페이즈1/게임오버 후 숨김.
- 부품 픽업/설치/드롭 전 과정 (델리게이트 추가 §5.6 영향 없음 확인).
- 마운트 탑승/하차 (좌석 분기 추가 §5.5 이후에도 기존 경로 무결).

---

## 11. 미확정 사항 현황 ★ (2026-07-23 답변 반영)

미정 항목은 전부 **BP 프로퍼티 / 프레임워크 / BP 이벤트 훅으로 격리**되어 있어 결정이 늦어져도 C++ 재작업이 발생하지 않는다 (§0 "미정 사양 처리 원칙"). 결정 기한이 필요한 항목만 마일스톤 게이트로 관리한다.

### 11.A 확정 (설계에 반영 완료)

| # | 항목 | 확정 내용 | 반영 위치 |
|---|---|---|---|
| 3 | 보스 해제 방식 | **HP %-임계 방식** → 값도 확정: **67% / 34% / 처치** (§14.6.4). 원안의 단일 값 `EliteRetreatHealthRatio`는 배열 `RetreatHealthRatios`로 대체 | §14.6.4, §4.9.7 |
| 4 | 임계 도달 시 처리 | **도망(퇴장) 연출 후 비활성 보관** — `Destroy`하지 않고 숨겨서 **체력·디버프를 유지**하고 다음 구간에서 되살린다(§14.6.4). 연출은 `OnBossRetreat`/`OnBossReappear` BP 훅으로 격리 | §14.6.4, §4.9.7 |
| 5 | 재출발 조건 | **전원(생존자) 재탑승 후 출발** | §4.9.7 |
| 6 | 열차 칸 | **4칸 고정, 빈 칸 허용** — 출발 조건은 "생존자 전원 착석" (좌석 만석 아님) | §4.9.2, §10.1 |
| 7-a | 방3 스폰 상한 | **상한 도입 확정** (값은 11.B-7에서 계속 미정). 상한 도달 시 스폰 일시 정지 방식(게임오버 아님)으로 설계 | §4.7 |
| 8 | "모든 플레이어" 기준 | **생존자 기준** (`GetAlivePlayers` 관례) | §3.4 |
| 2 | 부품 개수 | **1개 확정** — 설치대 슬롯 1개, 운반자 1명, 방3 동반 검증도 1개 기준 (슬롯 추가 작업 불필요) | §4.6, §4.7 |
| 9 | 방4 설치대 체력 | **미도입 확정** — 설치 전용. 어트리뷰트 초기화·체력 0 게임오버·**적 AI 타겟팅 작업 전량 삭제** (최대 리스크 해소) | §4.7 |
| 15 | 선로 형태 | **ㄷ자 + 코너에 약간의 곡선**, 열차는 **등속 주행**, 장애물 도달 시 엘리트 등장 → §4.9 스플라인 설계 그대로 유효 | §4.9.1~2 |
| 16 | 방1 전체 사양 | **확정** — 인원별 스폰 룩업 테이블(§14.1.3), 웨이브 2개·30초 간격 시간 기반, 시작지점 = 상승 차단물 영구 봉쇄, 방1→방2 = 문 발광 + 근접 순간이동 | **§14.1**, §4.5, §4.11, §4.12 |
| 18 | 방2 전체 사양 | **확정** — 퍼즐 3종(8퍼즐/스위치 3라운드/CCTV 카운팅, 순서 무관·동시 진행) → 금고 3자리 입력 → 부품 1개 → **부품 소지자 퇴장 시 전원 방1 회수 + D1 비활성**. 완료 조건이 "부품 픽업"이 아니라 "퇴장"인 점이 원안과 다름 | **§14.2**, §4.6, §4.10, §4.12 |
| 19 | 방2 CCTV의 성격 | **"해결" 상태 없음** — 관찰 카운팅 결과가 곧 금고 마지막 자리. 완료 판정·목표 진행도 분모에 포함하지 않음 | §14.2.4, §7.2 |
| 20 | 방2→방1 복귀 수단 | **방2 출구 텔레포트 게이트**(§14.1.7-10 해소) | §14.2.7 |
| 21 | 방3+방4 전체 사양 | **확정** — 방4는 부품 소지자 1명만 텔레포트 입장(통과 즉시 잠김), **설치가 두더지 게임과 방3 웨이브를 동시에 시작**, 방3은 50초 주기 무한 웨이브(인원별 구성), **두더지 20마리 클리어가 페이즈 완료 조건**(잔적은 즉시 사망 처리) | **§14.3·§14.4**, §4.7, §4.7.1, §4.7.2 |
| 22 | 두더지 규칙 | 2m 이내 근접 공격만 유효(밖은 투과), 유지 시간 경과 시 다른 위치로 재등장, 제한 시간 없음, 티어 3단(2.0/0.8 → 1.3/0.5 → 0.8/0.3초) | §14.4.2~4 |
| 23 | 부활 사양 (§11.B-13 일부 해소) | **두더지 클리어 시 / 방3에서 / 최대 체력 50%**. 물·부식 처리만 미정 | §14.3.4, §5.9 |
| 24 | 방4 실패·예외 처리 | **사망**: 두더지 리셋 + 부품이 설치대 옆 드롭 + D3를 `Anyone`으로 재활성(방3 웨이브 유지). **접속 종료**: 방3 몬스터 전부 소멸 + 스폰 중단 + 부품을 문 앞으로 + `CarrierOnly` 복원 | §14.3.5, §5.10, §5.12 |
| 25 | 통로 방식 (D2·D3·D4) | D2 상승 봉쇄 / D3 텔레포트 게이트(CarrierOnly, 1회용) / **D4 하강 개방 구조물** → `ADRS2RisingBlocker`(원안)를 `ADRS2MovingBlocker`로 일반화 | §4.11, §14.3.3 |
| 26 | 방5 전체 사양 | **확정** — 전원 입장 시 **D4 재봉쇄**, 인원별 3웨이브(§14.5.2, 총 9/11/16/20마리), 전환 트리거는 **`min(30초, 전원 전멸)` 하이브리드**, 웨이브3 전멸 시 **D5 하강 개방** | **§14.5**, §4.8 |
| 27 | 웨이브 진행 방식이 방마다 다름 | 방1 = 시간 고정(30초, 전멸 무관) / 방3 = 시간 고정 무한 반복(50초) / **방5 = 하이브리드** — 세 방식을 한 표로 정리 | §14.5.3 |
| 28 | 통로 8곳 전부 확정 + **자동문 미사용** | 구조물 4곳(D0·D2·D4·D5) + 텔레포트 게이트 4곳(D1·E2·D3·R4). `ADRAutoSlidingDoor` 재사용 0건 → `BP_S2Door` 제작 불필요. §2.3 재사용 인벤토리에서도 제외 | §1.1, §14.5.4, §6.2 |
| 29 | 방6 전체 사양 | **확정** — 4칸 열차 상호작용 탑승 → 생존자 전원 착석 시 등속 출발, 장애물 3구간에서 **두더지 보스가 장애물을 부수며 등장**, **체력이 67% → 34% → 처치로 이어지는 단일 개체**, **최종 처치가 곧 스테이지2 클리어** | **§14.6**, §4.9 |
| 30 | 장애물 해제 타이밍 변경 | 원안 "임계 도달 시 해제" → **"보스 등장과 동시에 부서짐"**. 전방 차단을 장애물에 의존할 수 없어 **전·후방 배리어 한 쌍**으로 구간을 한정 (배리어 3개 → 6개) | §14.6.3·§14.6.5, §4.9.5~6 |
| 31 | 스테이지 종료 방식 변경 | 원안 "종점 주행 → 전원 도착지점 진입" → **폐기**. `Trigger_Destination`·`S2P5_Arrive` 목표 행·도착 플랫폼 배치가 모두 불필요해졌다 | §14.6.6, §4.3, §7.2, §8 |
| 32 | 두더지 보스 구현 범위 | **스킬·공격 패턴은 이번 범위 밖** (두더지 몬스터 제작 시 별도 확정). 페이즈는 `MoleBossClass` BP 프로퍼티 + 체력 델리게이트만 사용하므로 보스 구현이 바뀌어도 페이즈 코드 수정 없음. 임시로 `EliteBear` 계열 사용 | §14.6.7 |
| 17 | 인원 스케일링 방식 | **인원별 룩업 테이블** (배수 아님), 기준 인원은 방 시작 시 1회 확정 후 불변 | §4.1, §14.1.4 |

### 11.B 미정 — 결정 대기 (격리 방법과 결정 기한)

| # | 항목 | 미정 상태에서의 진행 방법 (격리) | 결정 기한 (권장) |
|---|---|---|---|
| 1 | 방2 잔여 확인 13건 | **사양 확정 완료(§14.2)**. 남은 것은 CCTV 타깃 이미지 종류(사용자 미정 명시), 시퀀스 길이, 8퍼즐 그림 텍스처, 스위치 난이도 등 — 전부 BP 값/에셋이라 논블로킹. 목록은 **§14.2.9** | M3 중 (에셋은 M7) |
| 1-b | 방3·방4 잔여 확인 17건 | **사양 확정 완료(§14.3·§14.4)**. 남은 것은 티어 경계 해석 확인, 스폰 간격 의미(동시 등장 여부), 두더지 에셋, 부활 시 물 처리 등 — 전부 BP 값/에셋이라 논블로킹. 목록은 **§14.3.6 · §14.4.5** | M4 중 (에셋은 M7) |
| ~~3~~ | ~~엘리트 임계 % 값~~ | **해소** — 67% / 34% / 처치로 확정 (§14.6.4). BP 배열 `RetreatHealthRatios = {0.67, 0.34}` | 완료 |
| 7-b | 방3 스폰 주기·상한 값 / 인원별 구성 | BP `SpawnInterval` / `SpawnCountPerTick` / `MaxAliveEnemies` + 인원별 룩업 테이블(§4.1) | §14.3 확정 시 / M8 밸런스 |
| 10 | 착석 중 스킬 허용 | 기본 구현 = 허용(이동만 잠금). 차단 결정 시 `State.Seated` 태그 + `IsAbilityInputBlocked` 확장(§5.8)으로 전환 — 교체 비용 낮음 | M6.2 전 |
| 11 | 방5 잔여 확인 6건 | **사양 확정 완료(§14.5)**. 남은 것은 웨이브1 스폰 타이밍, 공중 스폰 지점, 방5 천장 높이 등 — 전부 레벨/BP 값이라 논블로킹. 목록은 **§14.5.5** | M5 중 |
| 12 | 연출 (열차 주행/급정거, 장애물 파괴, 보스 도망/재등장 등) | 연출 훅(BP 이벤트)만 코드에 선노출, 내용은 M7에서 확정 | M7 |
| 15 ★신규 | **두더지 보스의 스킬·공격 패턴·스탯** | 이번 범위 밖으로 확정(§14.6.7). 페이즈는 `MoleBossClass` + 체력 델리게이트만 참조하므로 논블로킹. M6는 `EliteBear` 계열 임시 클래스로 열차 루프 전체를 검증 | 보스 제작 착수 시 |
| 16 ★신규 | 방6 잔여 확인 10건 | 자동/수동 하차, 주행 속도·구간 거리, 재탑승 좌석 자유도, 최종 처치 후 연출 등 — 전부 BP 값/연출 훅. 목록은 **§14.6.8** | M6 중 |
| 13 | **부활 사양** | 위치(방3)·시점(두더지 클리어)·체력(최대 체력의 50% = 200) **확정**(§14.3.4). 남은 미정: **물(Water) 지급량과 부식 상태 처리**(물 0으로 부활하면 즉시 부식) — 기본값 "물 50% 지급 + 부식 해제"로 진행. 스테이지1 적용 여부는 비목표(호출부가 방3 페이즈뿐) | M4 중 |
| 14 ★신규 | 방1 잔여 확인 10건 | 적 BP 변종 선택, 웨이브2 무조건 스폰 여부, 스폰 연출, 차단물 상승 시간 등 — 전부 BP 값/연출 훅이라 논블로킹. 목록은 **§14.1.7** | M2 중 |

---

## 12. 리스크 & 완화

| 리스크 | 완화 |
|---|---|
| 열차 위치의 서버/클라 미세 편차 | 판정은 전부 서버, 시각은 로컬 시뮬 통일(§4.9.2). 편차가 눈에 띄면 `OnRep_Movement` 시 스냅 보정 1회 |
| attach 상태 캐릭터의 콜리전 부작용 (배리어/장애물 통과 시) | 탑승 중 `MOVE_None`(스윕 없음) + 이동 중 배리어 비활성 이중 안전 (§4.9.6) |
| 전원 입장 판정의 엣지 (경계에 걸친 캐릭터, 리스폰) | 트리거 박스를 방 안쪽으로 넉넉히 + 0.5s 재평가 폴링 (§3.4) |
| 페이즈 스킵 치트 사용 시 문/트리거 상태 불일치 | 각 페이즈 `OnPhaseStart`가 자기 입장 문을 여는 규약(§3.1) 덕에 진행은 가능. 완전 정합은 비목표(개발 편의 기능) |
| `SpawnedEnemies`가 `TWeakObjectPtr`라 전멸 판정 타이밍 이슈 (Destroy 지연) | 사망 델리게이트 기반 카운트(`OnEnemyDeath`에서 Remove — `DRPhaseBase.cpp:72-78`)를 그대로 신뢰. 스테이지1과 동일 특성 |
| UI 리팩토링(§5.4) 회귀 | **M1.1**에 단독 배치해 스테이지2 코드가 붙기 전에 회귀 체크 1회차 수행 (§9.4) |
| 한글 주석 인코딩 손상 재발 | 신규 파일 UTF-8(BOM) 고정, 공유 파일 수정은 최소 diff |
| 상승 차단물이 캐릭터를 끼우거나 천장으로 밀어 올림 | 콜리전을 **상승 완료 시점에** 활성 + 완료 시 통로 내 Pawn을 `PushOutPoint`로 이동 + 방1 트리거가 통로 입구까지 덮음 (§4.11, §8-5) |
| 텔레포트 동시 진입 시 캐릭터 겹침/바닥 관통 | 목적지 주변 `DestinationSpreadRadius` 분산 + 캡슐 여유 검사 재시도, 서버 권한 텔레포트(§4.12) |
| 부활(§5.9) 상태 복원 누락으로 "살아있지만 조작 불가" 폰 발생 | 사망 처리 함수(`DRCharacterBase.cpp:165-204`)의 항목을 1:1 대응 체크리스트로 관리 + 부활 직후 PIE 검증 케이스 고정. 호출부를 방4 페이즈로 한정해 스테이지1 영향 차단 |
| 인원별 룩업 테이블의 BP 입력 실수(구간 누락) | `ResolveWaveSet`에서 배열 길이 검사 + Error 로그 + 마지막 구간 폴백 (§4.1) |
| 스위치 퍼즐 마스크를 랜덤 생성하다 **정답이 없는 라운드**가 나옴 (소프트락) | 정답 부분집합에서 역산 생성 + 32조합 완전탐색 검증 + 실패 시 재생성 (§14.2.3). 테스트에 100회 생성 검증 포함 |
| 8퍼즐 셔플이 **해결 불가능한 순열**로 생성됨 (소프트락) | 무작위 순열 금지, **빈칸 무작위 이동**으로만 생성 (§14.2.2) |
| CCTV를 매 스텝 즉석 랜덤으로 만들면 "개수" 자체가 정의되지 않음 | 판 시작 시 유한 시퀀스 1회 생성 + 순환 반복, 시퀀스·시작시각만 복제 (§14.2.4) |
| 퍼즐 정답 정보가 클라에 복제되어 치트로 금고 즉시 개방 | 자리 숫자는 공개 시점에만 복제(`RevealedDigit = -1` 초기값), 스위치 마스크·금고 코드는 서버 전용 (§14.2.6) |
| 방2 프롭 27개를 각각 복제하면 트래픽/코드 폭증 | 상태는 퍼즐 액터 1개가 보유·복제, 프롭은 상호작용 입력 창구만 담당 (§4.10.1) |
| 부품을 드롭한 채 전원이 방2에 남아 진행 정지 | 출구 회수는 **부품 소지 시에만** 발동하고 부품은 월드에 남으므로 재획득 가능 — 소프트락 아님 (§14.2.7) |
| **방4 플레이어 사망 시 부품이 방4에 갇혀 아무도 못 들어감 (치명적 소프트락)** | D3 진입 규칙을 `Anyone`으로 **전환**해 재시도를 허용 (§14.3.5-A). 규칙 전환을 빠뜨리면 즉시 진행 불가이므로 §10.1 테스트 필수 항목 |
| 방4 플레이어 접속 종료로 부품이 소실 | `Logout` 훅에서 부품을 방3 쪽 문 앞으로 **재배치**(§14.3.5-B). `Logout` 오버라이드가 없으면 감지 자체가 불가하므로 §5.10이 선행 |
| 두더지가 기존 데미지 파이프라인에 안 걸림(공격해도 무반응) | 액터 태그 `Enemy` + ASC + AttributeSet 3종이 모두 있어야 히트한다 — `IsNotFriend`가 태그 기반(`DRAbilitySystemLibrary.cpp:415-422`)이라는 점이 근거 (§14.4.4) |
| 2m 게이트가 전체 데미지 경로에 부작용 | 인터페이스 구현 대상에만 적용되는 조기 return 1개 (§5.11). 스테이지1 대상은 인터페이스를 구현하지 않아 무영향 |
| 부활 시 복원 항목 누락으로 "살아있지만 조작 불가" | 사망 처리(`DRCharacterBase.cpp:165-204`) 항목의 1:1 역연산 체크리스트 + §10.1 부활 조작 테스트 |
| 무한 웨이브가 두더지 재시도 반복으로 누적 | 사망 재시도 시엔 웨이브 유지(설계), 접속 종료 시엔 전부 소멸 후 재개 — 두 경로 모두 `MaxAliveEnemies` 상한이 최종 안전장치 |
| 방5 마지막 웨이브 뒤에 **4번째 웨이브가 스폰**되어 클리어 불가 | `StartWave()`에서 마지막 인덱스면 다음 웨이브 타이머를 **예약하지 않는다**(§4.8). §10.1 필수 테스트 항목 |
| 방5 전멸 판정이 웨이브 단위라 겹침 상황에서 오작동 | 판정 기준을 **전체 생존 수 0 && 대기 스폰 0**으로 통일 (§14.5.3) |
| D4 왕복(개방→재봉쇄)에서 통로 안 캐릭터 끼임 | 재봉쇄 시 `PushOutPoint`를 **방5 쪽**으로 배선 + 콜리전은 이동 완료 후 활성 (§4.11, §8-3) |
| 방5 페이즈가 D4를 여는 호출과 방3 페이즈의 개방이 중복 | `SetBlocked()` 멱등 처리 — 같은 값 재호출 시 no-op (§4.11) |
| **보스를 매 구간 새로 스폰하면 체력이 100%로 리셋**되어 사양(67%→34%→처치)이 깨짐 | `Destroy` 금지, **비활성 보관 후 재등장**(§14.6.4). §10.1에 "구간2/3 재등장 시 체력이 이어지는지"를 핵심 검증 항목으로 고정 |
| 보스 보관 중 델리게이트가 남아 오작동(중복 임계 발화) | 도망 시 체력/사망 바인딩을 **해제**하고 재등장 시 다시 연결 + 구간별 `bSegmentResolved` 래치 (§4.9.7) |
| 장애물이 부서진 뒤 플레이어가 선로를 따라 앞으로 이탈 | **전방 배리어**를 전투 시작과 함께 활성화(원안에는 없던 요소 — §14.6.5) |
| 보스 스킬 미구현 상태로 M6를 진행하다 나중에 페이즈를 고쳐야 함 | 페이즈는 `MoleBossClass`와 체력 델리게이트만 참조 — 보스 구현체 교체가 페이즈 코드에 영향을 주지 않는다 (§14.6.7) |
| 미정 사양의 결정 지연 | **해소** — 6개 방 사양이 모두 확정(§14)되었고, 원안에서 구조 영향 항목이던 §11-9(설치대 체력)는 미도입, §11-2(부품 개수)는 1개로 확정되어 게이트가 사라졌다. 남은 §11.B 항목은 값·에셋·연출뿐이라 전부 논블로킹(§9.3 트랙 C) |
| **M3(방2)·M4(방3+4)가 단일 최대 작업이라 일정이 몰림** | §9.2에서 각각 3개 서브 단계로 분할하고, **M3.1만으로 방2 전체 흐름이 도는 순서**(퍼즐 없이 금고→부품→퇴장까지)로 배치해 조기에 리스크를 걷어낸다. M3.2의 두 퍼즐과 M6은 서로 독립이라 인원이 있으면 병렬 진행 가능 (§9.1) |
| 아트 에셋 미제작으로 M6(열차)이 대기 | 전 마일스톤을 **임시 프리미티브로 진행**하고 M7에서 교체하는 전제 (§9.3 트랙 A). 열차/선로/장애물/구조물/두더지 메시가 현재 전부 없음(§2.4) |

---

## 13. 부록 — 방3+방4 페이즈 시퀀스 다이어그램 (§14.3·§14.4 확정 반영)

```
[서버]                                                      [클라이언트]
OnPhaseStart
 ├ SetPhaseObjective("S2P3_Enter") ────────복제──────→ 목표 UI "부품을 들고 3번방으로 (0/2)"
 └ Trigger_Room3.Arm()
      │  (오버랩/폴링)
OnAllInside + 부품 동반 검증 OK
 ├ BasePlayerCount 확정 (웨이브 구성 고정)
 ├ Blocker_Room1ToRoom3.SetBlocked(true) ──복제(bBlocked)──→ 구조물 상승 연출 → 방1 봉쇄
 ├ Gate_Room3ToRoom4: EntryRule=CarrierOnly, Active=true ─복제─→ 방4 문 발광
 └ SetPhaseObjective("S2P3_Enter4") ───────복제──────→ "부품을 들고 4번방으로"
      │
[부품 소지자가 게이트 근접] ── 서버 오버랩 판정 ──
 ├ 텔레포트(방4) + Gate.Active=false ──복제──→ 발광 해제 (추가 진입 불가)
 └ SetPhaseObjective("S2P3_Hold") ─복제──→ "4번방 작업이 끝날 때까지 버텨라 (0/20)"
      │
[방4] F키 → ServerRequestInteract(Site) ──RPC──→
InstallPart 실행 ── OnPartInstalled
 ├ MoleGame.StartGame() ── SpawnActor(Mole) ──복제──→ 홀로그램 두더지 등장
 └ StartWaveLoop()      ── SpawnActor(Enemy) ─복제──→ 방3 웨이브 스폰 (이후 50초마다)
      │
      │  [방4] 2m 이내 공격 → ApplyDamageEffect
      │        └ IDRProximityHitOnly.AcceptsHitFrom() 통과 시에만 데미지 (밖이면 투과)
      │           └ Mole 사망 → KillCount++ ──복제──→ 진행도 UI n/20 (방3 인원도 공유)
      │  ※ 병행: 방3은 50초마다 웨이브 계속 (전멸 여부 무관)
      ▼
KillCount == 20 → OnMoleGameCleared
 ├ Gate_Room3ToRoom4.Active=true ───복제──→ 방4 복귀 가능
 ├ ReviveAllDeadPlayers() ──Multicast_HandleRevive──→ 방3에서 체력 50%로 부활 연출
 ├ Blocker_Room3ToRoom5.SetBlocked(false) ─복제──→ 구조물 하강 → 방5 개방
 ├ StopWaveLoop() + KillAllSpawnedEnemies() ─복제──→ 잔적 즉시 사망 연출
 └ ValidatePhaseCompletion
      └ EndCurrentPhase → Multicast_ObjectiveCompleted ──→ 클리어 연출
         └ StartPhase(3) = 방5 페이즈 ...

■ 예외 A — 방4 플레이어 사망 (OnPlayerDied)
   MoleGame.AbortAndReset() + Site.EjectInstalledPart()
   + Gate: EntryRule=Anyone, Active=true    → 다른 1명이 들어가 재시도 (방3 웨이브는 유지)

■ 예외 B — 방4 플레이어 접속 종료 (Logout, §5.10)
   MoleGame.AbortAndReset() + StopWaveLoop() + DestroyAllSpawnedEnemies()
   + 부품을 Room4EntranceDropPoint(방3 쪽 문 앞)로 이동
   + Gate: EntryRule=CarrierOnly, Active=true  → 재입장·재설치 시 두더지·웨이브 동시 재개
```

---

## 14. 방별 상세 사양 (확정 스펙 — SSOT) ★

이 장은 사용자가 확정한 방별 요구사항을 그대로 기록하는 **단일 기준(SSOT)**이다. §1·§4의 방별 서술과 충돌하면 **이 장이 우선**한다.

**6개 방 전체 사양 확정 완료 (2026-08-07).**

| 방 | 상태 | 절 | 핵심 |
|---|---|---|---|
| 방1 (전투) | **확정 (2026-08-06)** | §14.1 | 2웨이브 30초 간격(시간 고정), 인원별 룩업 |
| 방2 (퍼즐 3종 + 금고) | **확정 (2026-08-06)** | §14.2 | 8퍼즐/스위치/CCTV → 금고 3자리 → 부품 → 퇴장 시 전원 회수 |
| 방3 (무한 방어) | **확정 (2026-08-07)** | §14.3 | 50초 주기 무한 웨이브, 설치가 시작 트리거 |
| 방4 (두더지 + 설치 + 부활) | **확정 (2026-08-07)** | §14.4 | 2m 근접 전용 홀로그램 두더지 20마리, 클리어 시 부활 |
| 방5 (3웨이브) | **확정 (2026-08-07)** | §14.5 | `min(30초, 전멸)` 하이브리드 전환 |
| 방6 (열차 탈출) | **확정 (2026-08-07)** | §14.6 | 보스 체력이 67%→34%→처치로 이어짐, 최종 처치 = 클리어 |

**남은 미확정은 사양이 아니라 값·에셋뿐이다** — 각 방의 "잔여 확인 항목"(§14.1.7 / §14.2.9 / §14.3.6 / §14.4.5 / §14.5.5 / §14.6.8, 총 56건)은 전부 기본 진행값이 지정되어 있어 논블로킹이며, 별도 작업으로 분리된 항목은 **두더지 보스의 스킬·패턴**(§14.6.7) 하나다.

---

### 14.1 방1 — 전투 ★확정 (2026-08-06)

#### 14.1.1 진행 시퀀스 (서버 기준)

```
스테이지2 시작
 └ 전원 시작지점 스폰 — 시작지점↔방1 통로는 열려 있음(차단물 내려간 상태)
      │ 자유 이동, 목표 UI "1번방으로 이동하라 (n/N)"
      ▼
[모든 생존 플레이어가 방1 진입]        ← ADRS2RoomTrigger::OnAllAlivePlayersInside (1회 래치)
 ├ ① BasePlayerCount 확정 (= 이 순간 생존자 수, 1~4 clamp, 이후 불변)
 ├ ② D0 상승 차단물 Raise()  ─복제─→ 전 클라에서 물체 상승 연출 → 통로 영구 봉쇄
 ├ ③ 목표 교체 "적을 모두 처치하라 (0/N)"   (N = 확정 인원의 총 스폰 수)
 └ ④ 웨이브1 스폰 (지연 0초)
      │
      ├ ── +30초 ──▶ 웨이브2 스폰   ★웨이브1 전멸 여부와 무관 (시간 기반)
      │
      ▼
[전 웨이브 전멸]  (Alive == 0 && PendingSpawnCount == 0)
 ├ ValidatePhaseCompletion → EndCurrentPhase → Multicast_ObjectiveCompleted (클리어 연출)
 └ 방2 페이즈 OnPhaseStart
      ├ D1 텔레포트 게이트 SetGateActive(true) ─복제─→ 문 모델 발광
      └ 목표 "빛나는 문으로 이동하라"
           │ 플레이어가 게이트에 근접
           ▼
      개별 순간이동 → 방2 진입 (전원 대기 없음)
```

#### 14.1.2 시작지점 봉쇄 (D0 = 상승 차단물)

- **방식**: 문이 열리고 닫히는 것이 아니라 **물체(들)가 올라와 시작지점과 방1 사이를 가로막는다**. 한 번 올라오면 내려오지 않으며 시작지점으로 되돌아갈 수 없다.
- **트리거**: 모든 생존 플레이어의 방1 진입 (`ADRS2RoomTrigger`).
- **구현**: `ADRS2MovingBlocker` (§4.11). 서버가 `Raise()` → `bRaised` 1회 복제 → 서버·클라가 각자 `RiseDuration` 동안 로컬 보간(이동 복제 없음, 열차/타이머와 동일한 "복제 1회 + 로컬 시뮬" 관례).
- **콜리전**: 상승 **완료 시점**에 Pawn 차단 활성화. 완료 시 통로에 남은 Pawn은 `PushOutPoint`(방1 쪽)로 이동시켜 끼임을 방어한다.
- **연출**: 상승 사운드·먼지 VFX·카메라 셰이크는 `OnRiseStarted()` / `OnRiseFinished()` BP 훅에서 처리 (구체 연출 미정 — §14.1.7-5, 7).

#### 14.1.3 인원별 스폰 테이블 ★확정

**웨이브 2개, 간격 30초.** 아래 표를 `BP_S2CombatPhase`의 `WaveSetsByPlayerCount`에 그대로 입력한다.

| 기준 인원 | 웨이브 1 (0초) | 웨이브 2 (+30초) | 총 마리 |
|---|---|---|---|
| **1인** | 개 2, 아르마딜로 1 → 3마리 | 개 2, 잠자리 1 → 3마리 | **6** |
| **2인** | 개 3, 아르마딜로 2 → 5마리 | 개 2, 잠자리 1 → 3마리 | **8** |
| **3인** | 개 4, 아르마딜로 2, 잠자리 1 → 7마리 | 개 3, 잠자리 1 → 4마리 | **11** |
| **4인** | 개 5, 아르마딜로 3, 잠자리 1 → 9마리 | 개 3, 잠자리 1 → 4마리 | **13** |

**적 클래스 매핑** (변종 선택은 §14.1.7-1 확인 대기):

| 표기 | 블루프린트 | 비고 |
|---|---|---|
| 개 | `Content/Blueprints/Character/Enemy/Dog/BP_Dog` | 변종: `BP_Dog_Phase3`, `BP_Dog_WithPart` |
| 아르마딜로 | `Content/Blueprints/Character/Enemy/Armadillo/BP_DRArmadillo` | 변종: `BP_DRArmadillo_Phase1` (전용 BT 존재) |
| 잠자리 | `Content/Blueprints/Character/Enemy/DragonFly/BP_DRDragonFly` | 변종: `BP_DRDragonFly_Part` |

**표 특성 메모** (구현에 영향 없음, 의도 확인용 — §14.1.7-2): 웨이브2는 인원에 따라 완만하게 증가한다 — 2인 웨이브2가 1인과 동일(개2+잠자리1), 4인 웨이브2가 3인과 동일(개3+잠자리1). 웨이브1이 난이도 스케일링의 주축.

#### 14.1.4 기준 인원 확정 규칙

- `BasePlayerCount` = **전원 입장 발화 시점**의 `GetAlivePlayers().Num()`을 1~`WaveSetsByPlayerCount.Num()` 범위로 clamp.
- **확정 후 재계산하지 않는다.** 웨이브2 도중 누가 죽어도 구성은 그대로 유지한다.
  - 이유 ①: 죽으면 쉬워지는 역인센티브 방지. ②: 이미 스폰된 적 수와 목표 UI 분모(`TotalSpawnCount`)가 어긋나지 않게 하기 위해.
- 인원 구간을 초과하는 경우(5인 이상 확장 시): 마지막 구간(4인) 사용 + `UE_LOG` Warning.
- BP 배열이 비어 있거나 구간이 부족하면 Error 로그 후 마지막 유효 구간으로 폴백 (§4.1).

#### 14.1.5 스폰 규칙

- **스폰 지점**: `Director->Room1SpawnPoints` 중 랜덤 배정. 4인 웨이브1이 9마리이므로 지점은 최소 6개 권장 (동일 지점 연속 선택은 1회 재추첨으로 완화).
- **스폰 순서**: 웨이브 구성의 종류별 마리 수를 1마리 단위 리스트로 펼친 뒤 셔플 → `PerEnemySpawnInterval` 간격으로 순차 스폰. 값이 0이면 동시 스폰. "적이 막 나오는" 느낌은 이 간격 + 스폰 VFX로 조절한다.
- **웨이브 진행은 시간 기반**: 웨이브2는 웨이브1의 전멸 여부와 무관하게 전투 시작 후 30초에 스폰된다 (전멸 시 앞당기지 않음 — §14.1.7-3 확인 대기).
- **전멸 판정** ★: `GetAliveEnemyCount() == 0 && PendingSpawnCount == 0`.
  - `PendingSpawnCount`는 전투 시작 시 `TotalSpawnCount`로 초기화하고 1마리 스폰마다 감소시킨다.
  - 따라서 **웨이브1을 30초 안에 전멸시켜도 페이즈가 끝나지 않는다** (예약된 웨이브2가 Pending에 포함되어 있음). 원안 설계의 "지연 스폰 잔여" 문제를 이 방식으로 해소.
- **목표 진행도**: 누적 처치 수 / `TotalSpawnCount` (§7.2 `S2P1_Combat`).

#### 14.1.6 방2로의 이동 (D1 = 텔레포트 게이트)

- 방1 전멸 → **문이 열리는 것이 아니라 문 모델이 발광**하고, 플레이어가 다가가면 방2로 **순간이동**한다.
- **활성화 주체**: 방1 페이즈의 `OnPhaseEnd`가 아니라 **방2 페이즈의 `OnPhaseStart`**에서 `SetGateActive(true)`. §3.1 규약(입장 수단은 다음 페이즈가 켠다)을 따르는 이유는 `TriggerGameOver()`도 `OnPhaseEnd`를 호출하기 때문(`DRStageGameMode.cpp:66-69`) — 게임오버 시 게이트가 켜지는 부수효과를 원천 차단한다.
- **발광 표현**: `bActive` 복제 → `OnGateActiveChanged(true)` BP 훅에서 머티리얼 Emissive/스칼라 파라미터 전환 + 루프 사운드 (+ 선택 Niagara).
- **이동 처리** (서버 권한): 게이트 근접 박스 오버랩 → 목적지(방2 안 TargetPoint) 주변으로 분산 배치 → `SetActorLocationAndRotation(teleport=true)` + `SetControlRotation` → `Multicast_PlayTeleportFX`.
- **개별 이동**: 전원이 모일 필요 없이 도착한 사람부터 각자 이동한다 (방2는 자유 진입 구조).
- **되돌아오기**: D0가 영구 봉쇄이므로 시작지점 복귀는 불가. 방2 → 방1 복귀 수단(부품을 들고 방3으로 가야 하므로 필요)은 §14.2에서 확정한다.

#### 14.1.7 방1 잔여 확인 항목 (전부 논블로킹 — BP 값/연출 훅)

| # | 확인 항목 | 기본 진행값(미정 시) |
|---|---|---|
| 1 | 적 BP 변종: `BP_Dog` vs `BP_Dog_Phase3`, `BP_DRArmadillo` vs `BP_DRArmadillo_Phase1` (BT·스탯이 다름) | 기본형(`BP_Dog`/`BP_DRArmadillo`/`BP_DRDragonFly`) |
| 2 | 2인 웨이브2 = 1인 웨이브2, 4인 웨이브2 = 3인 웨이브2가 의도인지 | 표 그대로 입력 |
| 3 | 웨이브1 전멸 시 웨이브2를 앞당기는지 | 앞당기지 않음(30초 고정) |
| 4 | 웨이브1 스폰 시점: 차단물 상승 **시작**과 동시 / 상승 **완료** 후 | 상승 시작과 동시 |
| 5 | 적 등장 연출: 스폰 지점에 즉시 등장 / 환기구·구멍 등 특정 모델에서 등장 / 스폰 VFX | 즉시 등장 + `PerEnemySpawnInterval` 0.2s |
| 6 | 방1 스폰 지점 개수·위치 (4인 웨이브1 9마리 동시 수용) | 6개, 방 외곽 분산 |
| 7 | 차단물 상승 시간(초)과 상승 중 통로 캐릭터 처리 | 1.5초, 완료 시 `PushOutPoint`로 밀어냄 |
| 8 | 목표 UI에 웨이브 번호(1/2)도 표시할지 | 처치 수만 표시 |
| 9 | 발광 외 추가 안내(화살표/미니맵/거리 표시) 필요 여부 | 발광 + 목표 텍스트만 |
| 10 | 방2 → 방1 복귀 수단 | **해소** — 방2 출구 텔레포트 게이트 (§14.2.7) |

---

### 14.2 방2 — 퍼즐 3종 + 금고 ★확정 (2026-08-06)

#### 14.2.1 전체 흐름

```
[방1에서 D1 게이트로 순간이동 → 방2 진입]   (개별 이동, 전원 대기 없음 — §14.1.6)
      │  목표 "금고의 비밀번호를 알아내라 (0/2)"
      ├─ 8퍼즐    → 해결 시 스크린에 **1번째 자리** 숫자 표시
      ├─ 스위치    → 3라운드 전부 성공 시 스크린에 **2번째 자리** 숫자 표시
      └─ CCTV      → 깜빡이는 화면에서 특정 이미지 **개수(0~9)** 를 직접 세어 **3번째 자리** 파악
         ※ 세 퍼즐은 순서 무관, 여러 명이 동시에 서로 다른/같은 퍼즐 조작 가능
      ▼
[금고에서 3자리 버튼 입력]  → 일치하면 금고 문 개방 → 내부 부품 1개 노출
      ▼
[부품 픽업]  (기존 `ADRCleanserPart` 파이프라인)
      ▼
[부품 소지자가 방2 출구 통과]
 ├ 모든 생존 플레이어를 **방1로 강제 순간이동** (분산 배치)
 ├ D1 (방1→방2) 게이트 **비활성화** → 방2 재입장 불가
 └ 방2 페이즈 완료 → 방3 페이즈 시작 (D2 개방)
```

- **완료 조건이 "부품 픽업"이 아니라 "부품 소지자의 방2 퇴장 + 전원 방1 회수"** 로 확정 (Plan6 원안 §4.6과 다름).
- 이로써 §14.1.7-10(방2→방1 복귀 수단)은 **해소**: 방2 출구 게이트가 복귀 수단이다.

#### 14.2.2 퍼즐 1 — 8퍼즐 (슬라이드 타일)

**규칙**: 9칸 격자에 타일 8개 + 빈칸 1개. **빈칸에 인접한 타일을 빈칸 쪽으로 밀어** 위치를 바꾸고, 타일들이 **하나의 이어지는 그림**이 되면 성공.

**구성물** (레벨에 실제 모델로 배치):

| 요소 | 수량 | 역할 |
|---|---|---|
| 슬라이드 타일 | 8 | 조작 대상. 각 타일이 상호작용 단위 |
| 완성 사진 (작은 참조판) | 1 | 정답 그림을 옆에 상시 표시 (조작 불가) |
| 비밀번호 스크린 | 1 | 성공 시 **1번째 자리** 숫자 표시 |
| 되돌리기 버튼 | 1 | **직전 1수만** 되돌림 |
| 초기화 버튼 | 1 | 처음 배치로 되돌림 |

**구현 요점**:
- 보드 상태는 `TArray<uint8> BoardState`(9칸, 0=빈칸, 1~8=타일 ID) **1개만 복제**. 타일은 보드에 attach되어 있고 각 클라가 상태에서 목표 로컬 위치를 계산해 **로컬 보간(슬라이드 연출)** → 타일마다 이동 복제 불필요 (열차·차단물과 동일한 "복제 1회 + 로컬 시뮬" 관례).
- 조작: 타일에 시선을 두고 상호작용 → 서버가 "빈칸에 인접한가" 검증 후 스왑. 인접하지 않으면 무시(오답 페널티 없음).
- **셔플은 빈칸을 무작위로 K회 이동**시켜 생성한다 (무작위 순열은 절반이 해결 불가능한 배치가 되므로 금지). K 기본값 80.
- 되돌리기: 마지막 이동 1건만 보관(`LastMove`), 사용 후 히스토리 비움 → 연속 되돌리기 불가.
- 초기화: **최초 셔플 배치로 복귀**(재셔플이 아님 — §14.2.9-4 확인).
- 해결 판정: `BoardState`가 목표 배열과 일치 → `bSolved` 1회 래치 → 스크린에 1번째 자리 공개.
- 여러 명이 동시에 타일을 밀면 서버 RPC가 순차 처리되어 자연 직렬화된다. 슬라이드 연출 중(0.15초) 같은 타일 재입력은 무시.

#### 14.2.3 퍼즐 2 — 스위치 (전구 9개 / 레버 5개 / 3라운드)

**규칙**: 레버 5개, 전구 9개. **레버마다 켜고 끌 수 있는 전구 조합이 다르다.** 모든 전구를 켜는 레버 조합을 찾으면 1라운드 성공. **3라운드 성공해야 클리어**이며, 성공할 때마다 ① 옆의 성공 횟수 표시 1개가 발광하고 ② **레버-전구 조합이 새로 바뀐다**. 3라운드 완료 시 스크린에 **2번째 자리** 숫자가 표시된다.

**구성물**: 레버 5 / 전구 9 / 성공 횟수 표시 3 / 비밀번호 스크린 1.

**구현 요점**:
- 서버 내부 상태: `TArray<uint16> LeverMasks`(레버 5개 각각의 9비트 전구 마스크), `uint8 LeverOnBits`(레버 ON/OFF), `int32 RoundsCleared`.
- 전구 표시값 계산: **ON 상태 레버들의 마스크 XOR** → 목표는 `0b111111111`(9개 전부 점등). XOR이므로 두 레버가 공유하는 전구는 서로 상쇄된다 (라이트아웃 계열 — OR 누적 방식이면 아무 레버나 다 켜면 끝나므로 퍼즐이 성립하지 않음. §14.2.9-6 확인).
- **★해가 존재하는 마스크 생성이 필수**: 마스크를 완전 랜덤으로 뽑으면 32개 조합 중 정답이 없을 수 있다. 생성 절차 —
  1. 정답이 될 레버 부분집합 `S`(공집합 아님)를 랜덤 선택
  2. `S`의 마지막 원소 마스크 = `0b111111111 XOR (S의 나머지 원소 마스크들의 XOR)` 로 **역산**
  3. `S` 밖의 레버는 자유 랜덤 마스크(전구 0개/9개 전부는 제외)
  4. 생성 후 **32조합 완전탐색으로 해 존재를 검증**(5개뿐이라 비용 무시) — 실패 시 재생성
- 복제: **전구 표시 상태(`BulbBits`)·레버 시각 상태(`LeverBits`)·성공 횟수(`RoundsCleared`)만 복제**하고 마스크 자체는 복제하지 않는다 (클라에 정답 정보를 내려보내지 않음).
- 라운드 성공 시: `RoundsCleared++` → 성공 표시 발광(복제) → 마스크 재생성 + 레버 전부 OFF로 초기화 → 3회 도달 시 2번째 자리 공개.
- 동시 조작: 레버별 서버 RPC 직렬 처리.

#### 14.2.4 퍼즐 3 — CCTV 기믹 (6화면 / 2초 주기 / 이미지 교체)

**규칙**: 화면 6개. **2초마다 2개 화면만 정상 이미지**를 보여주고 나머지 4개는 **오류 화면**을 띄운다. 화면 전환은 전부 **이미지 교체 방식**(실시간 카메라 캡처가 아님). 플레이어들은 깜빡이는 사진들 속에서 **특정 이미지가 몇 개 등장하는지** 세고, 그 개수(**0~9**)가 금고의 **마지막 자리**가 된다.

**★ 다른 두 퍼즐과의 결정적 차이**: CCTV는 "해결(solved)" 상태가 없고 **스크린에 답을 표시하지 않는다.** 플레이어의 관찰 결과가 곧 답이며, 금고 입력으로만 검증된다. 따라서 방2 완료 판정에 CCTV의 상태는 포함되지 않는다.

**구현 요점**:
- **노출 시퀀스는 유한 길이 + 순환 반복**이어야 한다. 매 스텝을 즉석 랜덤으로 뽑으면 "총 몇 개"라는 값 자체가 정의되지 않아 퍼즐이 성립하지 않는다. 따라서 —
  1. 판 시작 시 서버가 시퀀스를 1회 생성: `TArray<FS2CctvStep>`, 각 스텝 = `{정상 화면 인덱스 2개, 그 두 화면에 띄울 이미지 인덱스 2개}`
  2. 전체 노출 슬롯 수 = `스텝 수 × 2`. 이 중 **타깃 이미지를 정확히 N개**(N = 마지막 자리 값) 배치하고 나머지는 더미 이미지 풀에서 채운다
  3. 시퀀스가 끝나면 **처음부터 다시 반복** → 플레이어가 놓쳐도 다시 셀 수 있다
- 복제: 시퀀스 배열 + `StartServerTime`을 **1회 복제**하고, 각 클라가 `GetServerWorldTimeSeconds()`로 현재 스텝을 계산해 텍스처를 교체한다 (매 스텝 RPC 불필요, 자동 동기 — 열차·타이머 관례 미러).
- 이미지 교체 = 화면 메시 머티리얼 인스턴스의 텍스처 파라미터 세팅 (오류 화면은 전용 텍스처 + 노이즈 머티리얼).
- N = 0인 경우도 유효하다 (타깃 이미지가 한 번도 안 나옴 → 마지막 자리 0). 이 케이스를 테스트에 포함한다.
- **타깃 이미지가 무엇인지, 더미 풀 크기, 시퀀스 길이는 미정** (§14.2.9-1·2).

#### 14.2.5 금고 (3자리 코드 → 부품)

- 실제 모델의 **숫자 버튼을 눌러** 3자리를 입력한다. 자리 순서는 **① 8퍼즐 → ② 스위치 → ③ CCTV 개수**.
- 입력 진행 상황은 금고 표시창에 복제되어 팀 전원에게 보인다.
- 3자리 입력 완료 시 서버가 검증 → **일치**: 금고 문 개방 연출 + 내부 부품(`BP_S2Part`) 노출/스폰 → 기존 픽업 파이프라인 / **불일치**: 입력만 초기화 + 오답 사운드 (페널티·리셋 없음 — 요구사항 "실패 시 리셋 없음"과 일관).
- 부품은 1개(§11.A-2)이며, 픽업 시 `State.Carrying` 태그로 이동속도 감소·스킬 차단이 자동 적용된다.
- 금고 입력을 퍼즐 완료 전에도 허용할지는 §14.2.9-9 확인 항목 (기본: 허용 — 1000조합 브루트포스는 비현실적이라 게이트가 불필요).

#### 14.2.6 비밀번호 생성·배분·복제 규칙 ★

- **생성 주체**: `UDRS2PuzzlePhase::OnPhaseStart()`가 매 판 `{d1, d2, d3}`(각 0~9)를 생성하고 각 액터에 주입한다.
  - `SlidePuzzle->SetRevealDigit(d1)` / `SwitchPuzzle->SetRevealDigit(d2)` / `CctvBoard->SetTargetImageCount(d3)` / `Safe->SetSecretCode({d1,d2,d3})`
  - 이렇게 하면 세 퍼즐과 금고의 값이 어긋날 수 없다 (각 액터가 독립적으로 랜덤을 뽑는 구조를 금지).
- **복제 시점 규칙 (치트 방지)**: 자리 숫자는 **공개되는 순간에만** 복제한다.
  - 8퍼즐/스위치: 해결 전에는 `RevealedDigit = -1`로 복제되고, 해결 시점에 실제 값으로 갱신 → 해결 전 클라에는 정답 정보가 존재하지 않는다.
  - 스위치 마스크·금고 정답 코드는 **끝까지 복제하지 않는다** (서버 전용).
  - CCTV는 N을 직접 복제하지 않고 "타깃 이미지가 N개 포함된 시퀀스"만 복제한다 — 시퀀스를 분석하면 셀 수 있지만, 그건 화면을 보고 세는 것과 동일한 정보라 문제없다.

#### 14.2.7 방2 퇴장 — 전원 방1 회수 + D1 비활성화 ★

- **부품을 든 플레이어가 방2 출구를 통과**하면:
  1. 모든 생존 플레이어를 방1의 지정 지점으로 **강제 순간이동**(겹침 방지 분산 배치). 방2에 남아 있던 팀원도 함께 회수된다.
  2. D1(방1→방2) 게이트를 **비활성화** → 발광 해제 + 재입장 불가.
  3. 방2 페이즈 완료 → 방3 페이즈 시작(D2 개방).
- 구현: `ADRS2TeleportGate`에 **모드**를 추가한다 (§4.12) — `Individual`(방1→방2, 개별 이동) / `TeamOnCarrier`(방2 출구, 부품 소지자 통과 시 전원 회수).
- 부품을 들지 않은 플레이어가 출구를 통과하는 경우는 **개별 이동만** 수행한다(방1↔방2 왕복 자유). 이때 D1은 비활성화하지 않는다 (§14.2.9-10 확인).
- 부품 운반 중 사망 시엔 기존 `ForceDropCarriedPart` 경로로 부품이 떨어지므로, 다른 플레이어가 다시 들고 나오면 된다 — 소프트락 없음.

#### 14.2.8 상호작용 구현 방식 (조작 대상이 27개 규모)

방2의 조작 대상은 **타일 8 + 레버 5 + 되돌리기·초기화 2 + 금고 숫자 버튼 10 + (선택)확인·초기화 2 ≒ 27개**다. 현재 상호작용 배관은 "타입별 감지 슬롯 + `ServerRequestInteract(AActor*)` 타입 분기"(`DRPlayerController.cpp:326-413`) 구조이므로, **조작 대상 1개 = 액터 1개**로 만들고 **공통 베이스 `ADRS2InteractProp`(IDRInteractable)** 하나만 분기에 추가하는 방식이 가장 적은 변경으로 끝난다 (§4.10.1).

- 감지: `FindPropByLineTrace()` — 카메라 라인트레이스(`ECC_Visibility`, `LineTraceDistance`) + `CanInteract()` 검사 (`FindSiteByLineTrace` 관례 미러).
- 실행: `ServerRequestInteract`에 `ADRS2InteractProp` 분기 1개 추가 → 서버에서 거리 재검증(`MaxInteractDistance`) 후 `Prop->ServerHandleInteract(Char)`.
- 각 프롭은 자기 소유 퍼즐 액터와 인덱스를 알고 있어, 실제 로직은 퍼즐 액터가 수행한다(상태를 한 곳에 모아 복제하기 위함).
- 컴포넌트 단위 히트 판정(액터 1개 + 컴포넌트 27개) 방식은 PlayerController가 히트 컴포넌트를 전달하도록 배관을 고쳐야 해서 채택하지 않는다.

#### 14.2.9 방2 잔여 확인 항목

| # | 확인 항목 | 기본 진행값(미정 시) |
|---|---|---|
| 1 | **CCTV 타깃 이미지가 무엇인지** (사용자 미정 명시) | 임시 텍스처 1종으로 개발 |
| 2 | CCTV 시퀀스 길이 / 더미 이미지 풀 크기 (총 노출 슬롯 = 스텝×2) | 스텝 12개(24슬롯, 24초 주기) + 더미 6종 |
| 3 | 8퍼즐 완성 그림 텍스처와 3×3 분할 방식 | 임시 텍스처 1장 UV 분할 |
| 4 | 8퍼즐 초기화 버튼: 최초 배치 복귀 vs 새로 셔플 | 최초 배치 복귀 |
| 5 | 8퍼즐 셔플 강도(빈칸 이동 횟수) | 80회 |
| 6 | 스위치 전구 계산: **XOR(상쇄)** vs OR(누적) | XOR (OR이면 퍼즐 성립 불가) |
| 7 | 스위치 정답 조합의 유일성 요구 여부 / 레버당 전구 수 범위 | 복수해 허용, 레버당 3~5개 |
| 8 | 금고 버튼 구성: 0~9 10키만 / 확인·초기화 버튼 추가 여부 | 10키 + 초기화 1개, 3자리 입력 시 자동 판정 |
| 9 | 퍼즐 완료 전 금고 입력 허용 여부 | 허용 |
| 10 | 부품 미소지자의 출구 통과 허용 여부 | 허용(개별 이동, D1 유지) |
| 11 | 조작 방식: 전부 F키 상호작용인지 (사격으로 조작하는 요소가 있는지) | 전부 F키 |
| 12 | 스크린 숫자 표시 방식 | 머티리얼 숫자 텍스처(0~9 아틀라스) |
| 13 | 방1 회수 지점 위치 (방1 내 어디) | D1 게이트 앞 |

### 14.3 방3 — 무한 방어 ★확정 (2026-08-07)

방3과 방4는 **하나의 페이즈(P2 = `UDRS2DefensePhase`)** 가 동시에 관리한다. 방4에 들어간 1명이 두더지 게임을 하는 동안 나머지는 방3에서 무한 웨이브를 막는 **분업 구조**다.

#### 14.3.1 진행 시퀀스

```
[방2 출구 → 전원 방1로 회수됨]  (§14.2.7)
      │  D2 개방, 목표 "부품을 들고 3번방으로 이동하라 (n/N)"
      ▼
[모든 생존 플레이어가 방3에 진입]
 ├ ① 방1↔방3 사이 **구조물이 올라와** 통로 봉쇄 (D2 = 상승 차단물, §4.11)
 ├ ② **방4 문이 발광** + 진입 규칙 = **부품 소지자만** (D3 = 텔레포트 게이트, CarrierOnly)
 └ ③ 목표 "부품을 들고 4번방으로 들어가라"
      ▼
[부품 소지자가 D3 통과 → 방4 입장]
 ├ D3 게이트 **즉시 비활성화** (1명만 입장, 추가 진입 차단)
 └ 목표(방3) "4번방 작업이 끝날 때까지 버텨라"
      ▼
[방4 중앙 설치대에 상호작용 키로 부품 설치]
 ├ 홀로그램 두더지 잡기 게임 시작 (§14.4)
 └ ★같은 순간부터 방3 무한 웨이브 시작 — 50초 주기 (§14.3.2)
      ▼
[두더지 20마리 클리어]
 ├ ① D3(방3↔방4) 게이트 재활성화 → 방4 플레이어 복귀 가능
 ├ ② **사망한 모든 플레이어를 방3에서 체력 50%로 부활** (§14.3.4)
 ├ ③ 방3↔방5 사이 **구조물이 아래로 내려가 개방** (D4 = 하강 개방 구조물)
 └ ④ 방3 적 스폰 **즉시 중단** + **남은 적 전부 즉시 사망**
      ▼
페이즈 완료 → 방5 페이즈 시작
```

#### 14.3.2 방3 무한 웨이브 ★확정

- **웨이브 주기: 50초.** 시작 시점은 **방4에서 부품이 설치된 순간**이며, 첫 웨이브는 설치 즉시 스폰된다 (§14.3.6-3 확인).
- 전멸 여부와 무관하게 50초마다 계속 스폰되는 **무한 웨이브**이고, 종료 조건은 두더지 클리어뿐이다.

| 기준 인원 | 웨이브 1회 구성 | 합계 |
|---|---|---|
| **1인** | 개 2, 아르마딜로 1, 잠자리 1 | 4마리 |
| **2인** | 개 2, 아르마딜로 2, 잠자리 1 | 5마리 |
| **3인** | 개 2, 아르마딜로 3, 잠자리 1 | 6마리 |
| **4인** | 개 3, 아르마딜로 3, 잠자리 2 | 8마리 |

- 적 클래스 매핑은 §14.1.3과 동일(`BP_Dog` / `BP_DRArmadillo` / `BP_DRDragonFly`).
- 기준 인원은 **방3 전원 입장 시점의 생존자 수**로 1회 확정한다(§14.1.4 규칙 일관). 방4로 1명이 빠져도 구성을 줄이지 않는다 — 방3 인원이 (생존자−1)인 것을 전제로 설계된 수치로 본다 (§14.3.6-2 확인).
- 스폰 지점은 `Director->Room3SpawnPoints` 랜덤 배정. 데이터 구조는 §4.1 `FS2WaveSet`(웨이브 구성 1개) + `WaveIntervalSeconds = 50`을 재사용한다.
- 안전장치로 동시 생존 상한(`MaxAliveEnemies`)을 두되(§11.A-7a), 두더지 게임이 통상 1~2웨이브 안에 끝나므로 실전에서는 거의 걸리지 않는다.

#### 14.3.3 통로 3곳의 방식 ★확정

| ID | 통로 | 방식 | 동작 |
|---|---|---|---|
| D2 | 방1 → 방3 | **상승 차단물** (`ADRS2MovingBlocker`) | 전원 방3 진입 시 올라와 봉쇄 (방1 복귀 불가) |
| D3 | 방3 ↔ 방4 | **텔레포트 게이트** (`CarrierOnly`) | 방3 진입 시 발광 + 부품 소지자만 통과 → **통과 즉시 비활성**. 두더지 클리어 시 재활성(복귀용) |
| D4 | 방3 → 방5 | **하강 개방 구조물** (`ADRS2MovingBlocker`) | 초기에 막고 있고, 두더지 클리어 시 **아래로 내려가 개방** |

- D4가 "내려가서 열리는" 구조라서 §4.11의 `ADRS2RisingBlocker`를 **`ADRS2MovingBlocker`로 일반화**한다 (막힘/열림 두 포즈 + `SetBlocked(bool)`). D0·D2는 열림→막힘, D4는 막힘→열림으로 같은 클래스가 커버한다.

#### 14.3.4 부활 처리 ★확정 (위치·체력 확정, 나머지는 §14.3.6-6)

- **시점**: 두더지 20마리 클리어.
- **대상**: 그 시점까지 사망한 **모든** 플레이어 (방4에서 죽은 사람 포함).
- **위치**: **방3** (레벨에 배치한 부활 지점, 겹침 방지 분산).
- **체력**: **최대 체력의 50%** — 컨테이너 4개 × 100 = 총 400 기준 **200**(= 컨테이너 2개 만충)으로 해석한다. 물(Water)·부식 상태 처리는 미정(§14.3.6-6).
- 구현은 §5.9 `Revive()` (사망 처리의 역연산). 호출부는 이 페이즈 한 곳뿐이다.

#### 14.3.5 실패·예외 처리 ★확정

**A. 방4 플레이어가 사망한 경우**

| 순서 | 처리 |
|---|---|
| 1 | 홀로그램 두더지 전부 소멸, **`KillCount`를 0으로 초기화** (퍼즐 리셋) |
| 2 | **설치되어 있던 부품을 설치 장치 옆에 드롭** (설치 상태 해제) |
| 3 | D3 게이트 **재활성** — 이때 진입 규칙을 **`Anyone`(1명)** 으로 전환한다. 부품이 방4 안에 있으므로 "부품 소지자만" 규칙을 그대로 두면 아무도 들어갈 수 없다 ★ |
| 4 | 새 플레이어가 입장하면 게이트 **다시 비활성**, 방4 안의 부품을 주워 재설치 → 두더지 재시작 |
| 5 | **방3 웨이브는 중단하지 않는다** (계속 진행) |
| 6 | 방4에서 죽은 플레이어도 최종 클리어 시 부활 대상에 포함된다 |

**B. 방4 플레이어가 게임을 종료(접속 끊김)한 경우**

| 순서 | 처리 |
|---|---|
| 1 | 방3에 있던 **몬스터 전부 소멸 + 스폰 중단** |
| 2 | 두더지 전부 소멸, `KillCount` 0 초기화 |
| 3 | **부품을 방4로 가는 문 앞(방3 쪽)에 배치** — A와 달리 방4 안이 아니다 |
| 4 | D3 게이트 재활성, 진입 규칙 = **`CarrierOnly`**(원래 흐름 복원) |
| 5 | 새 플레이어가 부품을 들고 입장 → 재설치 시 **두더지 재시작 + 방3 웨이브 재개** |
| 6 | 접속 종료는 영구 이탈이므로 **웨이브 기준 인원을 재계산**한다 (§14.3.6-7 확인) |

- B를 감지하려면 `Logout` 오버라이드가 필요하다 — 현재 `ADRStageGameMode`/`ADRGameModeBase`에 없으므로 신규 추가(§5.10).
- A는 기존 사망 통지(`ADRGameModeBase::OnPlayerDied`) 경로로 감지한다.

#### 14.3.6 방3 잔여 확인 항목

| # | 확인 항목 | 기본 진행값(미정 시) |
|---|---|---|
| 1 | 방3 스폰 지점 개수·위치 (4인 8마리 동시 수용) | 4~6개, 방 외곽 |
| 2 | 웨이브 기준 인원이 **전체 생존자**인지 **방3에 남은 인원(생존자−1)** 인지 | 전체 생존자 |
| 3 | 첫 웨이브 타이밍: 설치 즉시 vs 설치 후 50초 | 설치 즉시 |
| 4 | 동시 생존 상한값 | 30 (안전장치) |
| 5 | 방4 복귀 수단: D3 게이트를 양방향으로 쓸지, 방4 쪽에 별도 복귀 게이트를 둘지 | 방4 쪽 복귀 게이트(클리어 시 활성) |
| 6 | 부활 시 **물(Water) 지급량**과 부식 상태 처리 (물 0이면 즉시 부식) | 물 50% 지급, 부식 해제 |
| 7 | 접속 종료 시 웨이브 기준 인원 재계산 여부 | 재계산 |
| 8 | D2 봉쇄 후 방1에 남은 부품/아이템 접근 불가 문제 없는지 | 문제 없음(부품은 소지 상태로 이동) |

---

### 14.4 방4 — 홀로그램 두더지 잡기 ★확정 (2026-08-07)

#### 14.4.1 입장·설치

- **부품 소지자 1명만** D3 게이트로 입장하며, 통과 즉시 게이트가 비활성화된다 (§14.3.3).
- 방4 **중앙 설치대에 접근 → 상호작용 키로 부품 설치** → 그 순간 두더지 게임이 시작된다.
- 원안(§4.7)의 "퍼즐 해결 → `ActivateSite()` → 설치" **순서가 역전**되므로, 설치대는 처음부터 `Active` 상태로 두고 퍼즐 게이트 로직은 폐기한다. `RequiredPartsCount = 1`.

#### 14.4.2 두더지 규칙 ★확정

- 방4 **바닥 곳곳(레벨 배치 지점)에서 홀로그램 두더지 모델이 튀어나온다.**
- **2m 이내로 근접해서 공격**해야 두더지가 사라진다. **2m 밖에서의 공격은 무시(투과)** 된다 — 원거리 딜로 클리어할 수 없다.
- 일정 시간(유지 시간) 동안 공격받지 않으면 스스로 사라지고 **다음 무작위 위치에 나타난다**. 놓쳐도 페널티는 없다.
- **제한 시간 없음.** 실패 조건은 (사망/접속 종료 외에) 존재하지 않는다.
- **20마리를 잡으면 성공.**

#### 14.4.3 난이도 티어 ★확정

잡은 마릿수에 따라 **등장 유지 시간**과 **스폰 간격**이 단계적으로 짧아진다.

| 티어 | 잡은 마릿수(`KillCount`) | 사냥 대상 | 등장 유지 시간 | 스폰 간격 |
|---|---|---|---|---|
| 1 | 0 ~ 4 | 1~5마리째 | **2.0초** | **0.8초** |
| 2 | 5 ~ 11 | 6~12마리째 | **1.3초** | **0.5초** |
| 3 | 12 ~ 19 | 13~20마리째 | **0.8초** | **0.3초** |

- 표는 "1~5마리까지는 …" 서술을 **잡고 있는 순번 기준**으로 해석한 것이다 (§14.4.5-1 확인).
- **스폰 간격 < 유지 시간**이므로 두더지는 **동시에 여러 마리 존재**한다 (티어별 평균 동시 2~3마리). 한 마리씩 순차 등장이 의도라면 스폰 간격의 의미를 "직전 두더지 소멸 후 대기"로 바꿔야 한다 (§14.4.5-2 확인).
- 티어는 `KillCount`가 바뀌는 즉시 적용되며, 이미 등장해 있는 두더지의 잔여 유지 시간은 갱신하지 않는다.

#### 14.4.4 구현 방식 (2m 판정을 기존 데미지 파이프라인에 얹는 방법) ★

- 프로젝트의 모든 플레이어 공격은 **`UDRAbilitySystemLibrary::ApplyDamageEffect()` 단일 관문**을 지난다 (투사체 `DRProjectile.cpp:99-120`, 대시·제트점프·엘리트 스윕 전부 동일). 아군 판정은 **액터 태그 기반**(`"Player"`/`"Enemy"`, `DRAbilitySystemLibrary.cpp:415-422`)이다.
- 따라서 두더지를 **`Enemy` 태그 + 최소 ASC를 가진 액터**로 만들면 (선례: `ADRCleanserSite`가 `AActor + IAbilitySystemInterface`, `DRCleanserSite.h:48`) **기존 모든 공격이 그대로 히트**한다. 체력은 1로 두어 어떤 공격이든 1히트에 사라진다.
- **2m 게이트**는 `ApplyDamageEffect()` 진입부에 **인터페이스 기반 분기 1개**로 넣는다 (§5.11):
  ```cpp
  // 근접 전용 대상: 소스 아바타가 허용 반경 밖이면 데미지 자체를 무시(투과)
  if (const IDRProximityHitOnly* Prox = Cast<IDRProximityHitOnly>(TargetAvatarActor))
  {
      if (!Prox->AcceptsHitFrom(SourceAvatarActor)) return FGameplayEffectContextHandle();
  }
  ```
  단일 관문이라 공격 종류마다 손댈 필요가 없고, 인터페이스를 구현한 대상(두더지)에만 적용되므로 스테이지1 회귀 위험이 없다.

#### 14.4.5 방4 잔여 확인 항목

| # | 확인 항목 | 기본 진행값(미정 시) |
|---|---|---|
| 1 | 티어 경계 해석: "1~5마리까지"가 **잡는 순번**(0~4 kills) 기준인지 | 순번 기준 (표 그대로) |
| 2 | 스폰 간격의 의미: **동시 등장 허용**인지 "직전 소멸 후 대기"인지 | 동시 등장 허용 |
| 3 | 동시 등장 상한 (동시 존재 최대 마리수) | 3 |
| 4 | 두더지 등장 지점 개수·배치 | 9~12개, 바닥 격자 |
| 5 | 2m의 정확한 기준: 캐릭터 중심 간 거리 vs 캡슐 표면 거리 | 액터 위치 간 거리 200uu |
| 6 | 두더지 홀로그램 모델·등장/소멸 VFX·사운드 에셋 | 임시 메시 + 홀로그램 머티리얼 |
| 7 | 두더지 공격 시 물(Water) 보상 지급 여부 | 미지급(홀로그램) |
| 8 | 진행도 UI 표시: "두더지 처치 n/20"을 방3 플레이어에게도 보여줄지 | 전원에게 표시 |
| 9 | 두더지가 방4 플레이어를 공격하거나 방해하는지 | 없음(순수 표적) |

### 14.5 방5 — 3웨이브 ★확정 (2026-08-07)

#### 14.5.1 진행 시퀀스

```
[두더지 클리어로 D4 구조물이 하강 개방됨]  (§14.3.1)
      │  목표 "5번방으로 이동하라 (n/N)"
      ▼
[모든 생존 플레이어가 방5에 진입]
 ├ ① BasePlayerCount 확정 (§14.1.4 규칙 일관)
 ├ ② 방3↔방5 구조물이 **다시 위로 올라와 봉쇄** (D4 재봉쇄 — 방3 복귀 불가)
 ├ ③ GameState->SetTotalWaves(3) (기존 웨이브 번호 UI 재활용)
 └ ④ 웨이브1 스폰
      │
      ├─ 30초 경과 ──┐
      ├─ 전원 전멸  ──┼──▶ 둘 중 **먼저 오는 쪽**에 웨이브2 스폰  ★§14.5.3
      │              │
      ├─ 30초 경과 ──┤
      ├─ 전원 전멸  ──┴──▶ 웨이브3 스폰
      ▼
[웨이브3까지 전멸 = 모든 웨이브의 적 처치 완료]
 └ 방5↔방6 **구조물이 아래로 내려가 개방** (D5) → 페이즈 완료 → 방6 페이즈 시작
```

#### 14.5.2 인원별 3웨이브 구성 ★확정

| 기준 인원 | 웨이브 1 | 웨이브 2 | 웨이브 3 | 총 마리 |
|---|---|---|---|---|
| **1인** | 개 2, 아르마딜로 1 (3) | 개 1, 아르마딜로 1, 잠자리 1 (3) | 개 1, 아르마딜로 1, 잠자리 1 (3) | **9** |
| **2인** | 개 2, 아르마딜로 2 (4) | 개 1, 아르마딜로 1, 잠자리 1 (3) | 개 1, 아르마딜로 1, 잠자리 2 (4) | **11** |
| **3인** | 개 2, 아르마딜로 2, 잠자리 1 (5) | 개 2, 아르마딜로 2, 잠자리 1 (5) | 개 1, 아르마딜로 2, 잠자리 3 (6) | **16** |
| **4인** | 개 2, 아르마딜로 3, 잠자리 1 (6) | 개 2, 아르마딜로 3, 잠자리 2 (7) | 개 2, 아르마딜로 2, 잠자리 3 (7) | **20** |

- 적 클래스 매핑은 §14.1.3과 동일(`BP_Dog` / `BP_DRArmadillo` / `BP_DRDragonFly`).
- 인원이 늘수록 **잠자리(공중) 비중이 커지는** 구성이다 (1인 2마리 → 4인 6마리). 3인/4인 웨이브3은 잠자리가 절반을 차지하므로 대공 대응이 필요한 구간으로 읽힌다.
- 기준 인원은 **방5 전원 입장 시점의 생존자 수**로 1회 확정하고 이후 사망해도 재계산하지 않는다 (§14.1.4).
- 스폰 지점은 `Director->Room5SpawnPoints` 랜덤 배정. 데이터 구조는 §4.1 `FS2WaveSet`(웨이브 3개)을 그대로 사용한다.

#### 14.5.3 웨이브 진행 규칙 ★ (방1·방3과 다른 하이브리드)

**"30초 간격" 과 "전멸 시 즉시" 를 동시에 만족**해야 한다 — 둘 중 먼저 도달한 조건으로 다음 웨이브가 스폰된다.

| 방 | 웨이브 진행 방식 |
|---|---|
| 방1 (§14.1.5) | **시간 고정** — 전멸해도 앞당기지 않음 (30초) |
| 방3 (§14.3.2) | **시간 고정 무한 반복** — 전멸 무관 (50초) |
| **방5** | **하이브리드** — `min(30초 경과, 전원 전멸)` ★ |

구현 규칙:
- 웨이브 스폰 시 30초 타이머를 예약한다.
- 적 사망 콜백에서 **전체 생존 수가 0이 되면** 예약 타이머를 **취소**하고 즉시 다음 웨이브를 스폰한 뒤 새 30초 타이머를 건다.
- "이전 웨이브 전멸"의 판정 기준은 **웨이브 단위가 아니라 전체 생존 수 0**이다. 30초 타이머로 웨이브가 겹쳐 스폰된 뒤라면 이전·현재 웨이브 적이 혼재하므로, 혼재분까지 모두 전멸했을 때 다음 웨이브를 당긴다.
- **마지막(3) 웨이브 전멸 시**에만 페이즈 완료로 판정한다. 판정식은 `CurrentWaveIndex == 마지막 && 전체 생존 0 && 대기 스폰 0`.
- 마지막 웨이브 뒤에는 30초 타이머를 걸지 않는다 (4번째 웨이브가 생기지 않도록).

#### 14.5.4 통로 방식 ★확정 — 자동문은 스테이지2에서 사용하지 않음

| ID | 통로 | 방식 | 동작 |
|---|---|---|---|
| D4 | 방3 ↔ 방5 | `ADRS2MovingBlocker` | 초기 막힘 → 두더지 클리어 시 **하강 개방** → **방5 전원 입장 시 다시 상승 봉쇄** (왕복) |
| D5 | 방5 → 방6 | `ADRS2MovingBlocker` | 초기 막힘 → 3웨이브 클리어 시 **하강 개방** |

- **D4가 "열렸다가 다시 막히는" 왕복 동작**을 하게 되어, `ADRS2MovingBlocker`의 `SetBlocked(bool)` 양방향 API가 그대로 쓰인다 (§4.11).
- D5까지 구조물로 확정되면서 **스테이지2의 모든 통로가 "구조물(4곳) + 텔레포트 게이트(4곳)"** 로 구성된다. 원안이 재사용을 전제했던 `ADRAutoSlidingDoor`는 **결과적으로 한 번도 쓰이지 않는다** → `BP_S2Door` 제작 불필요 (§6.2).

#### 14.5.5 방5 잔여 확인 항목

| # | 확인 항목 | 기본 진행값(미정 시) |
|---|---|---|
| 1 | 웨이브1 스폰 타이밍: 전원 입장 즉시 vs 구조물 상승 완료 후 | 입장 즉시(구조물 상승과 동시) |
| 2 | 웨이브 겹침 허용 여부 (30초가 지났는데 이전 웨이브가 남아 있을 때) | 허용(겹쳐 스폰) |
| 3 | 방5 스폰 지점 개수·위치 (4인 웨이브3 7마리 + 공중 유닛 3마리) | 4~6개 + **잠자리용 공중 지점 2개** |
| 4 | 웨이브 시작 사운드/배너를 웨이브마다 재생할지 | 매 웨이브 `Multicast_PlayWaveStartSound` 재생 |
| 5 | 목표 UI 표기: 웨이브 번호(i/3) vs 남은 적 수 | 웨이브 번호(기존 복제 필드 재활용) |
| 6 | 잠자리 다수 구성에 맞춘 방5 지형 높이(천장) 확보 여부 | 레벨 확인 필요 |

### 14.6 방6 — 열차 탈출 ★확정 (2026-08-07)

#### 14.6.1 진행 시퀀스

```
[방5 3웨이브 클리어로 D5 구조물 하강 개방]
      │  목표 "열차에 탑승하라 (n/N)"
      ▼
[4칸 열차 — 각 칸에 1명씩 상호작용 키로 탑승]
 └ 생존자 전원 착석 → 열차 출발 (빈 칸 허용, §11.A-6)
      ▼
[장애물 1 앞 정지]
 ├ **두더지 보스가 장애물을 부수며 등장** → 전투 시작
 └ 보스 체력 **67% 도달 → 도망**  → 전원 재탑승 → 출발
      ▼
[장애물 2 앞 정지]
 ├ **같은 보스가 다시 등장** (체력 67%에서 이어짐)
 └ 보스 체력 **34% 도달 → 도망**  → 전원 재탑승 → 출발
      ▼
[장애물 3 앞 정지]
 ├ **같은 보스 최종 등장** (체력 34%에서 이어짐)
 └ **보스 처치 → 스테이지2 클리어** ★도착지점 이동 없음
```

#### 14.6.2 열차와 탑승

- **4칸 열차**, **각 칸에 플레이어 1명**씩 **상호작용 키(F)로 탑승** (`ADRS2TrainSeat`, §4.9.3).
- **생존자 전원이 착석하면 출발.** 4명 미만이면 빈 칸을 허용한다 (§11.A-6).
- 열차는 **등속 주행**하며, 선로는 **ㄷ자 형태에 코너 곡선이 약간 포함**된다 (§4.9.1 스플라인).
- 이동 복제는 "1회 복제 + 로컬 시뮬" 모델(§4.9.2)이고, 탑승자는 좌석에 attach되어 자동 추종한다.

#### 14.6.3 장애물과 보스 등장 ★원안과 다른 지점

- 선로에 **장애물 구간 3개**가 있고, 열차가 장애물 앞에 도달하면 정지한다.
- **두더지 보스가 장애물을 부수며 나타나** 전투가 시작된다.
- ★ 즉 **장애물 해제는 "보스 등장 시점"** 이다. 원안(§4.9.7)은 "보스 체력 임계 도달 → 장애물 해제"였는데, 확정 사양에서는 등장과 동시에 부서지고, **열차의 진행을 막는 것은 보스 자체**(전투 미종료)가 된다.
- 그래서 전방 차단을 장애물 메시에 의존할 수 없으므로 **전·후방 배리어 한 쌍**으로 전투 구간을 한정한다 (§4.9.6, §14.6.5).

#### 14.6.4 보스 체력이 3구간에 걸쳐 이어짐 ★핵심 설계

**같은 두더지 보스 1개체**가 세 구간에 반복 등장하며 **체력이 누적적으로 이어진다.**

| 구간 | 시작 체력 | 해제 조건 | 이후 |
|---|---|---|---|
| 장애물 1 | 100% | **67% 도달** | 도망 → 재탑승 → 출발 |
| 장애물 2 | 67% | **34% 도달** | 도망 → 재탑승 → 출발 |
| 장애물 3 | 34% | **처치(0%)** | **스테이지2 클리어** |

- 구현은 **`Destroy` 대신 비활성 보관**: 도망 시 `SetActorHiddenInGame(true)` + 콜리전/AI 정지로 숨기고, 다음 구간 스폰 지점으로 이동시켜 되살린다. **체력·디버프·어트리뷰트를 그대로 유지**하는 것이 목적이다 (매번 새로 스폰해 비율로 복원하면 상태가 리셋되어 "이어 싸우는" 감각이 깨진다).
- BP 설정은 `RetreatHealthRatios = { 0.67, 0.34 }` 배열이며, **3번째 구간에는 임계가 없다**(사망만이 조건). 구간 수와 임계 개수의 관계는 `구간 수 = 임계 수 + 1`이다.
- 원안의 단일 값 `EliteRetreatHealthRatio`(플레이스홀더 30%)는 **폐기**되고, §11.B-3(임계 % 미정)도 **해소**된다.
- 임계 도달 전에 한 번에 죽여버리는 예외(과도한 폭딜)도 진행으로 인정한다 — 사망 델리게이트에서 해당 구간을 해제 처리한다.

#### 14.6.5 하차와 전투 구간 제한

- 하차는 **정지 중(전투 구간)에만** 가능하다 (점프키, `Train->CanDeboardNow()` 검증 — §4.9.7).
- 이동 가능 범위는 **전방 배리어 ~ 후방 배리어 사이**로 한정한다. 장애물이 부서져 전방이 열리기 때문에 전방 배리어가 필수다.
- 보스가 도망친 뒤 **전방 배리어만 해제**해 열차가 통과할 수 있게 하고, 후방 배리어는 열차 출발과 함께 해제한다.
- 이동 중에는 하차가 거부되므로 "움직이는 발판 위 캐릭터" 네트워크 문제가 발생하지 않는다 (원안의 핵심 단순화 유지).

#### 14.6.6 스테이지 종료 ★원안과 다른 지점

- **3번째 구간에서 보스를 처치하면 그 즉시 스테이지2 클리어**다.
- 원안의 "종점까지 주행 → 전원 하차 → **도착지점 트리거 진입**" 단계는 **폐기**한다. 이에 따라 `Trigger_Destination`(§4.2 재사용 계획)과 목표 행 `S2P5_Arrive`(§7.2)도 불필요해진다.
- 페이즈 완료 → 마지막 페이즈이므로 `ADRStageGameMode::TriggerGameClear()`가 자동 호출되어 클리어 연출 후 로비로 복귀한다 (`DRStageGameMode.cpp:343-348`).

#### 14.6.7 두더지 보스 자체는 별도 작업

- **보스의 스킬·공격 패턴은 이번 범위 밖**이며, 두더지 몬스터를 실제로 제작할 때 별도로 확정한다 (사용자 명시).
- 그때까지는 기존 `EliteBear` 계열 BP를 임시 클래스로 지정해 열차 루프 전체를 검증한다. 페이즈는 `MoleBossClass`(BP 프로퍼티)와 체력 델리게이트만 사용하므로, **보스 구현이 바뀌어도 페이즈 코드는 수정되지 않는다.**
- 방4의 **홀로그램 두더지(`ADRS2Mole`, §4.7.2)와는 완전히 다른 존재**다 — 전자는 1히트 표적, 후자는 체력·스킬을 가진 보스. 클래스명을 `ADRS2Mole`(표적) / 보스용 별도 BP로 구분한다.

#### 14.6.8 방6 잔여 확인 항목

| # | 확인 항목 | 기본 진행값(미정 시) |
|---|---|---|
| 1 | 보스 등장 시 **자동 하차**인지 수동 하차인지 | 수동(점프키) — 착석 상태로도 사격 가능하면 안 내려도 됨 |
| 2 | 착석 중 스킬 사용 허용 여부 (§11.B-10) | 허용(이동만 잠금) |
| 3 | 열차 주행 속도·구간 간 거리(체감 이동 시간) | 600uu/s, 구간당 20~30초 |
| 4 | 보스 최대 체력·스킬·공격 패턴 | 별도 작업(§14.6.7), 임시로 `EliteBear` 스탯 |
| 5 | 도망/재등장 연출(땅속으로 파고들기 등) | `OnBossRetreat`/`OnBossReappear` BP 훅 |
| 6 | 전투 중 잡몹 동반 여부 | 없음(보스 단독) |
| 7 | 재탑승 시 좌석 자유 선택인지 원래 칸 고정인지 | 자유 선택 |
| 8 | 전투 중 전원 사망 시 처리 | 기존 전멸 → 게임오버 경로 |
| 9 | 최종 처치 후 클리어 연출(열차 주행 컷 등) 필요 여부 | 즉시 클리어(§14.6.6) |
| 10 | 보스 BGM 전환 필요 여부 | Phase3 BGM 전환 패턴 재사용(`DRPhase3.cpp:1314-1328`) |

---

## 15. 구현 상세 — 마일스톤별 작업 지시 ★ (2026-08-07)

§4는 "무엇을 만드는가"(설계), 이 장은 **"어떻게 만드는가"**(구현)다. 마일스톤(§9.2) 순서대로 파일 단위 작업 지시·실제 시그니처·알고리즘·검증 방법을 적는다. 코드 앵커는 모두 현재 저장소 실측이다.

### 15.0 공통 구현 규약

| 규약 | 내용 |
|---|---|
| **파일 배치** | C++: `Source/DaeRune/Public·Private/Phase/Stage2/`, `Actor/Stage2/`. BP: `Content/Blueprints/Phase/Stage2/`, `Actor/Stage2/` |
| **인코딩** | 신규 파일은 **UTF-8 (BOM)** 고정. 기존 파일 수정 시 **수정 라인 외에는 손대지 않는다** — 현재 다수 파일의 한글 주석이 모지바케 상태라 전체 재저장 시 손상이 확산된다 |
| **서버 권한** | 모든 게임플레이 판정은 `HasAuthority()` 안에서만. 클라는 연출·UI만 |
| **복제 모델** | "**1회 복제 + 로컬 시뮬**"이 기본 — 상태 스냅샷 1개(+서버시각)를 복제하고 각 클라가 동일 계산으로 보간/재생한다. 구조물 이동·열차 주행·CCTV 시퀀스·타일 슬라이드 전부 이 모델. `bReplicateMovement`는 쓰지 않는다(이중 소스 방지) |
| **RepNotify 수동 호출** | 리슨 서버에서도 연출이 돌아야 하므로, 서버에서 값을 바꾼 뒤 `OnRep_Xxx()`를 **직접 호출**한다 (프로젝트 기존 관례) |
| **`GetLifetimeReplicatedProps`** | 복제 프로퍼티 추가 시 반드시 등록. 빠뜨리면 조용히 클라에서만 동작하지 않는다 |
| **타이머 정리** | 페이즈/액터마다 핸들을 보관하고 `OnPhaseEnd()`/`EndPlay()`에서 전부 `ClearTimer`. 페이즈는 `UObject`라 GC가 늦어 누수가 잘 드러나지 않는다 |
| **로그** | `UE_LOG(LogDR, ...)`. 배선 누락 = **Error**, 밸런스 폴백 = **Warning**, 상태 전이 = **Verbose** |
| **널 가드** | Director 참조는 레벨 배선이라 언제든 null일 수 있다. 페이즈의 모든 진입점에서 검사 + Error 로그 1회 + 조기 return |
| **사망 판정 호출법** ★ | `IsDead`는 `ICombatInterface`의 **BlueprintNativeEvent**다(`DRCharacterBase.h:41` = `IsDead_Implementation`). 따라서 `Char->IsDead()`가 아니라 **`ICombatInterface::Execute_IsDead(Char)`** 로 호출해야 한다. `const` 포인터에는 `const_cast`가 필요하다 |
| **부품 운반 여부 조회** | `ADRCleanserPart::bIsCarried`(`:94`)는 공개 접근자가 없다. 방3 동반 검증(§15.7.1)을 위해 **`bool IsCarriedNow() const` 접근자를 추가**한다 — §5.6(픽업 델리게이트)과 같은 파일이라 함께 작업 |

**페이즈 작성 공통 골격** (5개 페이즈 전부 동일):

```cpp
UCLASS(Blueprintable)
class DAERUNE_API UDRS2XxxPhase : public UDRS2PhaseBase
{
    GENERATED_BODY()
public:
    virtual void OnPhaseStart() override;
    virtual void OnPhaseEnd() override;
    virtual bool IsCompleted() const override;
    virtual void OnEnemyDeath(AActor* DeadEnemy) override;   // 전투가 있는 페이즈만
};
```

```cpp
void UDRS2XxxPhase::OnPhaseStart()
{
    Super::OnPhaseStart();          // bIsPhaseActive = true + Multicast_PlayPhaseStartSound
    if (!GameMode || !GameState) return;
    ADRS2StageDirector* Dir = GetDirector();
    if (!Dir) return;               // Error 로그는 GetDirector 내부에서 1회만
    // ... 페이즈 고유 로직
}

void UDRS2XxxPhase::OnPhaseEnd()
{
    ClearAllTimers();               // 자기 타이머 먼저
    // 델리게이트 언바인드 (대상 Destroy 전에!)
    Super::OnPhaseEnd();            // SpawnedEnemies 언바인드 + 파괴
}
```

- `OnPhaseEnd()`는 **게임오버 경로에서도 호출**된다(`DRStageGameMode.cpp:66-69`). 따라서 "다음 방 열기" 같은 진행성 부수효과를 넣으면 안 된다 — 입장 통로는 **다음 페이즈의 `OnPhaseStart`**가 연다(§3.1).
- 완료 신호는 항상 `GameMode->ValidatePhaseCompletion()`을 호출한다. 내부에서 `CurrentPhase->IsCompleted()`를 물어보고 참이면 `EndCurrentPhase()`가 실행된다(`DRStageGameMode.cpp:387-401`). **페이즈가 직접 `EndCurrentPhase()`를 부르지 않는다.**

---

### 15.1 M1.1 — 공유 코드 수정 4건 (스테이지1 회귀 주의)

이 단계는 **스테이지2 코드를 한 줄도 추가하지 않고** 공유 코드만 고친다. 끝나면 즉시 §10.2 회귀 체크를 돌려 원인 추적 범위를 좁힌다.

#### 15.1.1 사이트 개수 가드 완화 (§5.1)

**위치**: `Private/Game/DRStageGameMode.cpp:259` (`InitializePhaseSystem()` 내부, 함수 시작 `:254`)

```cpp
// AS-IS
if (CleanserSites.Num() < 1) return;

// TO-BE
if (CleanserSites.Num() < 1)
{
    // 스테이지2는 방4 설치대 1개만 쓰며, 맵 구성에 따라 사이트가 없을 수도 있다.
    // 페이즈 시스템 자체를 막지 않고 경고만 남긴다.
    UE_LOG(LogDR, Warning, TEXT("[Phase] CleanserSite 가 없습니다. 사이트 의존 로직은 동작하지 않습니다."));
}
```

- 스테이지1 맵은 항상 사이트 ≥ 1이라 실동작 변화 없음. 회귀 위험 **낮음**.

#### 15.1.2 `SetupPhaseObjectiveByRow(FName)` 추가 (§5.2)

**위치**: `Public/Phase/DRPhaseBase.h:119` / `Private/Phase/DRPhaseBase.cpp:84-96`

기존 `SetupPhaseObjective(int32)`는 `"Phase%d"` 문자열을 조립해 행을 찾는다. 스테이지2는 한 페이즈 안에서 목표를 여러 번 교체하므로(`S2P3_Enter` → `S2P3_Enter4` → `S2P3_Hold`) **행 이름 직접 지정**이 필요하다.

```cpp
// DRPhaseBase.h — protected 에 추가
void SetupPhaseObjectiveByRow(FName RowName);
// 런타임에 RequiredCount 를 바꿔야 하는 경우를 위한 오버로드
void SetupPhaseObjectiveByRow(FName RowName, int32 OverrideRequiredCount);
```

```cpp
// DRPhaseBase.cpp — 기존 함수는 위임 형태로 정리
void UDRPhaseBase::SetupPhaseObjectiveByRow(FName RowName)
{
    if (!PhaseObjectiveDataTable || !GameState || RowName.IsNone()) return;

    if (const FPhaseObjectiveData* Row =
            PhaseObjectiveDataTable->FindRow<FPhaseObjectiveData>(RowName, TEXT("SetupPhaseObjectiveByRow")))
    {
        GameState->SetPhaseObjective(*Row);
    }
    else
    {
        UE_LOG(LogDR, Error, TEXT("[Phase] DT 행을 찾을 수 없습니다: %s"), *RowName.ToString());
    }
}

void UDRPhaseBase::SetupPhaseObjectiveByRow(FName RowName, int32 OverrideRequiredCount)
{
    if (!PhaseObjectiveDataTable || !GameState || RowName.IsNone()) return;

    if (const FPhaseObjectiveData* Row =
            PhaseObjectiveDataTable->FindRow<FPhaseObjectiveData>(RowName, TEXT("SetupPhaseObjectiveByRow")))
    {
        FPhaseObjectiveData Copy = *Row;          // 값 복사 후 분모만 교체
        Copy.RequiredCount = OverrideRequiredCount;
        GameState->SetPhaseObjective(Copy);
    }
}

void UDRPhaseBase::SetupPhaseObjective(int32 PhaseNumber)
{
    SetupPhaseObjectiveByRow(FName(*FString::Printf(TEXT("Phase%d"), PhaseNumber)));
}
```

- 순수 추가 + 기존 함수의 내부 위임이라 스테이지1 동작은 그대로다.
- 오버로드는 방1의 `TotalSpawnCount`(인원별로 6/8/11/13), 방3의 두더지 20, 방5의 웨이브 3처럼 **런타임에 분모가 정해지는** 목표에 쓴다.

#### 15.1.3 `FPhaseObjectiveData::PhaseAlarmText` 컬럼 추가 (§5.3)

**위치**: `Public/Phase/DRPhaseBase.h:13-29`

```cpp
USTRUCT(BlueprintType)
struct FPhaseObjectiveData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PhaseNumber = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText ObjectiveTitle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText ProgressFormat;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RequiredCount = 0;

    // ★추가: 페이즈 진입 배너 문구. 비어 있으면 배너를 띄우지 않는다(서브 목표 전환용).
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText PhaseAlarmText;
};
```

- 기존 DataTable 행들은 이 필드가 **빈 값으로 로드**되므로 §15.1.4의 폴백이 스테이지1 동작을 보존한다.

#### 15.1.4 웨이브 UI 플래그 전환 + 알람 데이터화 (§5.4) ★회귀 위험 중간

현재 `UOverlayWidgetController`가 **페이즈 인덱스 2**를 스테이지1 방어 페이즈로 하드코딩한다. 스테이지2에서는 인덱스 2가 방3(무한 방어)이라 **스테이지1 전용 UI(웨이브 타이머·클렌저 HP)가 잘못 뜬다.**

**수정 대상** (`Private/UI/WidgetController/OverlayWidgetController.cpp` 실측 앵커):

| 앵커 | 현재 코드 | 변경 |
|---|---|---|
| `:37` | `OnPhase3TimerVisibilityChanged.Broadcast(DRGameState->GetCurrentPhaseIndex() == 2);` | 플래그 구독으로 교체 |
| `:495` | `CheckAndBindWaveTimer()`의 `if (CurrentPhase == 2)` | `if (DRGameState->IsWaveDefenseUIActive())` |
| `:517` | `OnPhaseChanged()`의 `if (NewPhaseIndex == 2) BindCallbacksCleanserSiteToDependencies();` | 플래그 켜짐 콜백으로 이동 |
| `:524` | `OnPhase3TimerVisibilityChanged.Broadcast(NewPhaseIndex == 2);` | 플래그 값 브로드캐스트 |
| `:527-545` | 알람 `switch (NewPhaseIndex)` (LOCTEXT `Phase_Alarm_1/2/3`) | `PhaseAlarmText` 우선 + 기존 switch 폴백 |

**① GameState에 복제 플래그** (`Public/Game/DRStageGameState.h`):

```cpp
UPROPERTY(ReplicatedUsing = OnRep_WaveDefenseUIActive)
bool bWaveDefenseUIActive = false;

UFUNCTION() void OnRep_WaveDefenseUIActive();

UPROPERTY(BlueprintAssignable) FOnBoolChangedSignature OnWaveDefenseUIActiveChanged;

UFUNCTION(BlueprintCallable) void SetWaveDefenseUIActive(bool bActive);   // 서버 전용
bool IsWaveDefenseUIActive() const { return bWaveDefenseUIActive; }
```

```cpp
void ADRStageGameState::SetWaveDefenseUIActive(bool bActive)
{
    if (!HasAuthority() || bWaveDefenseUIActive == bActive) return;
    bWaveDefenseUIActive = bActive;
    OnRep_WaveDefenseUIActive();                 // 리슨 서버 수동 호출
}

void ADRStageGameState::OnRep_WaveDefenseUIActive()
{
    OnWaveDefenseUIActiveChanged.Broadcast(bWaveDefenseUIActive);
}

// GetLifetimeReplicatedProps 에 반드시 추가
DOREPLIFETIME(ADRStageGameState, bWaveDefenseUIActive);
```

**② `UDRPhase3`에서 플래그 설정** (스테이지1 방어 페이즈 — 2줄):

```cpp
void UDRPhase3::OnPhaseStart() { Super::OnPhaseStart(); /*...*/ GameState->SetWaveDefenseUIActive(true); }
void UDRPhase3::OnPhaseEnd()   { GameState->SetWaveDefenseUIActive(false); /*...*/ Super::OnPhaseEnd(); }
```

**③ 컨트롤러가 인덱스 대신 플래그를 구독**:

```cpp
DRGameState->OnWaveDefenseUIActiveChanged.AddDynamic(this, &UOverlayWidgetController::HandleWaveDefenseUIActiveChanged);
HandleWaveDefenseUIActiveChanged(DRGameState->IsWaveDefenseUIActive());   // late joiner 대응: 현재 상태 즉시 반영

void UOverlayWidgetController::HandleWaveDefenseUIActiveChanged(bool bActive)
{
    OnPhase3TimerVisibilityChanged.Broadcast(bActive);
    if (bActive)
    {
        BindCallbacksCleanserSiteToDependencies();
        if (CachedWavePhaseNumber != 2) { CachedWavePhaseNumber = 2; BindWaveTimerDelegate(); }
    }
    else
    {
        CachedWavePhaseNumber = -1;
    }
}
```

**④ 알람 텍스트 데이터화** (`OnPhaseChanged` 내부 switch 대체):

```cpp
FText PhaseText = GameState->GetCurrentPhaseObjective().PhaseAlarmText;   // 복제된 목표 데이터에서
if (PhaseText.IsEmpty())
{
    // 폴백: 스테이지1 DT에 PhaseAlarmText를 채우기 전까지 기존 switch 유지
    switch (NewPhaseIndex) { /* 기존 LOCTEXT 3종 */ }
}
if (!PhaseText.IsEmpty()) { OnPhaseAlarm.Broadcast(PhaseText); bPhaseAlarmShown = true; }
```

- **핵심 규칙**: 알람은 원래 "페이즈 진입 시 1회"였는데 스테이지2는 한 페이즈 안에서 목표를 여러 번 교체한다. `PhaseAlarmText`가 **비어 있으면 배너를 띄우지 않는** 규칙이 서브 목표 전환의 배너 스팸을 막는 장치다(§7.2에서 서브 목표 행의 AlarmText를 빈 값으로 둔 이유).
- 기존 버그도 함께 고쳐진다: 2페이즈 구조인 스테이지1에서 인덱스 1(방어)에 "페이즈 2: 수집"이 표시된다(`:534`). DT에 올바른 문구를 넣으면 해소.

**게이트**: 스테이지1 PIE 2인 — 페이즈 전환, 목표/알람 배너, 웨이브 타이머 UI, 클렌저 HP UI, 게임오버 후 숨김이 모두 이전과 동일한지(§10.2).

---

### 15.2 M1.2 — 기반 타입·베이스·레지스트리

#### 15.2.1 `Phase/Stage2/DRS2Types.h` (헤더 단독)

§4.1의 3단 구조를 그대로 만든다. **`FS2WaveSet`이 인원 1구간**이고, 페이즈는 `TArray<FS2WaveSet>`(1~4인)을 BP 프로퍼티로 노출한다.

```cpp
#pragma once
#include "CoreMinimal.h"
#include "DRS2Types.generated.h"

class ADREnemy;

USTRUCT(BlueprintType)
struct FS2EnemyCount
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite) TSubclassOf<ADREnemy> EnemyClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0")) int32 Count = 0;
};

USTRUCT(BlueprintType)
struct FS2WaveComposition
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (TitleProperty = "EnemyClass"))
    TArray<FS2EnemyCount> Enemies;

    // 방 전투 시작 기준 스폰 지연. 방1 = [0, 30] (§14.1.3)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0")) float StartDelaySeconds = 0.f;

    // 같은 웨이브 안 마리 단위 간격 ("막 나오는" 연출, 0 = 동시)
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (ClampMin = "0.0")) float PerEnemySpawnInterval = 0.f;
};

USTRUCT(BlueprintType)
struct FS2WaveSet     // 인원 1구간(1인/2인/3인/4인)의 웨이브 목록
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite) TArray<FS2WaveComposition> Waves;
};
```

#### 15.2.2 `Phase/Stage2/DRS2PhaseBase.h/.cpp` — 5개 페이즈 공통 베이스

**전 페이즈가 쓰는 공용 기능 5가지**: Director 접근, 인원 확정, 웨이브 세트 조회, 스폰, 타이머 정리.

```cpp
UCLASS(Abstract, Blueprintable)
class DAERUNE_API UDRS2PhaseBase : public UDRPhaseBase
{
    GENERATED_BODY()
public:
    virtual void OnPhaseEnd() override;      // ClearAllTimers 후 Super

protected:
    // ===== Director =====
    ADRS2StageDirector* GetDirector();       // TActorIterator 1회 + 캐시, 실패 시 Error 1회

    // ===== 인원/웨이브 =====
    int32 ResolveBasePlayerCount();                                   // 방 시작 시 1회 (§14.1.4)
    const FS2WaveSet* ResolveWaveSet(const TArray<FS2WaveSet>& Sets) const;
    static int32 CountTotalSpawns(const FS2WaveSet& Set);

    void ScheduleWaveSet(const FS2WaveSet& Set, const TArray<TObjectPtr<AActor>>& Points);  // StartDelay별 예약
    void SpawnComposition(const FS2WaveComposition& Comp, const TArray<TObjectPtr<AActor>>& Points);
    AActor* SpawnEnemyAt(TSubclassOf<ADREnemy> Class, const FTransform& Xf);   // 스폰 + 사망 바인딩 + 추적

    // ===== 통로 래퍼 (널 가드 포함) =====
    void SetBlocked(ADRS2MovingBlocker* Blocker, bool bBlocked);
    void SetGateActive(ADRS2TeleportGate* Gate, bool bActive);

    // ===== 타이머 =====
    FTimerHandle& AddTimerHandle();          // 관리 배열에 슬롯 추가
    void ClearAllTimers();

    UPROPERTY() TObjectPtr<ADRS2StageDirector> CachedDirector;
    int32 BasePlayerCount = 0;               // 확정 후 불변
    int32 PendingSpawnCount = 0;             // 아직 스폰되지 않은 마리 수 (예약분 포함)
    TArray<FTimerHandle> ManagedTimers;
};
```

**핵심 구현**:

```cpp
ADRS2StageDirector* UDRS2PhaseBase::GetDirector()
{
    if (IsValid(CachedDirector)) return CachedDirector;
    if (!GameMode) return nullptr;

    for (TActorIterator<ADRS2StageDirector> It(GameMode->GetWorld()); It; ++It)
    {
        CachedDirector = *It;
        return CachedDirector;
    }
    UE_LOG(LogDR, Error, TEXT("[S2] StageDirector 가 레벨에 없습니다. 스테이지2 진행 불가."));
    return nullptr;
}

int32 UDRS2PhaseBase::ResolveBasePlayerCount()
{
    int32 Alive = 0;
    if (ADRGameStateBase* GS = Cast<ADRGameStateBase>(GameState))
    {
        Alive = GS->GetAlivePlayers().Num();      // DRGameStateBase.cpp:91-115
    }
    BasePlayerCount = FMath::Clamp(Alive, 1, 4);
    UE_LOG(LogDR, Verbose, TEXT("[S2] BasePlayerCount 확정: %d"), BasePlayerCount);
    return BasePlayerCount;
}

const FS2WaveSet* UDRS2PhaseBase::ResolveWaveSet(const TArray<FS2WaveSet>& Sets) const
{
    if (Sets.Num() == 0)
    {
        UE_LOG(LogDR, Error, TEXT("[S2] WaveSetsByPlayerCount 가 비어 있습니다. BP 설정 확인."));
        return nullptr;
    }
    const int32 Index = FMath::Clamp(BasePlayerCount - 1, 0, Sets.Num() - 1);
    if (BasePlayerCount - 1 > Sets.Num() - 1)
    {
        UE_LOG(LogDR, Warning, TEXT("[S2] 인원 %d 구간이 없어 마지막 구간으로 폴백"), BasePlayerCount);
    }
    return &Sets[Index];
}
```

**스폰 (★전멸 오판 방지의 핵심)**:

```cpp
void UDRS2PhaseBase::ScheduleWaveSet(const FS2WaveSet& Set, const TArray<TObjectPtr<AActor>>& Points)
{
    // PendingSpawnCount 는 호출 측에서 CountTotalSpawns 로 미리 세팅해 둔다.
    for (const FS2WaveComposition& Comp : Set.Waves)
    {
        if (Comp.StartDelaySeconds <= 0.f)
        {
            SpawnComposition(Comp, Points);
        }
        else
        {
            FTimerDelegate D = FTimerDelegate::CreateUObject(this, &UDRS2PhaseBase::SpawnComposition, Comp, Points);
            GameMode->GetWorldTimerManager().SetTimer(AddTimerHandle(), D, Comp.StartDelaySeconds, false);
        }
    }
}

void UDRS2PhaseBase::SpawnComposition(const FS2WaveComposition& Comp, const TArray<TObjectPtr<AActor>>& Points)
{
    // 1) 종류별 Count 를 1마리 단위 리스트로 펼친 뒤 셔플 (한 종류가 몰려 나오지 않게)
    TArray<TSubclassOf<ADREnemy>> Flat;
    for (const FS2EnemyCount& E : Comp.Enemies)
    {
        for (int32 i = 0; i < E.Count; ++i) Flat.Add(E.EnemyClass);
    }
    for (int32 i = Flat.Num() - 1; i > 0; --i) Flat.Swap(i, FMath::RandRange(0, i));

    // 2) 간격 0이면 즉시, 아니면 인덱스 * 간격으로 예약
    for (int32 i = 0; i < Flat.Num(); ++i)
    {
        const float Delay = Comp.PerEnemySpawnInterval * i;
        if (Delay <= 0.f) { SpawnOne(Flat[i], Points); }
        else
        {
            FTimerDelegate D = FTimerDelegate::CreateUObject(this, &UDRS2PhaseBase::SpawnOne, Flat[i], Points);
            GameMode->GetWorldTimerManager().SetTimer(AddTimerHandle(), D, Delay, false);
        }
    }
}

void UDRS2PhaseBase::SpawnOne(TSubclassOf<ADREnemy> Class, const TArray<TObjectPtr<AActor>>& Points)
{
    if (Points.Num() == 0 || !Class) { --PendingSpawnCount; return; }

    // 직전 지점 회피 1회 재추첨
    int32 Idx = FMath::RandRange(0, Points.Num() - 1);
    if (Points.Num() > 1 && Idx == LastSpawnPointIndex) Idx = (Idx + 1) % Points.Num();
    LastSpawnPointIndex = Idx;

    SpawnEnemyAt(Class, Points[Idx]->GetActorTransform());
    --PendingSpawnCount;                       // ★스폰이 실제로 일어난 뒤에 감소
}

AActor* UDRS2PhaseBase::SpawnEnemyAt(TSubclassOf<ADREnemy> Class, const FTransform& Xf)
{
    // DRPhase1.cpp:191-212 미러
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AActor* Enemy = GameMode->GetWorld()->SpawnActor<ADREnemy>(Class, Xf, Params);
    if (!Enemy) return nullptr;

    if (ICombatInterface* Combat = Cast<ICombatInterface>(Enemy))
    {
        Combat->GetOnDeathDelegate().AddDynamic(this, &UDRPhaseBase::OnEnemyDeath);
    }
    SpawnedEnemies.Add(Enemy);                 // 베이스의 추적 배열 (TWeakObjectPtr)
    return Enemy;
}
```

> **`PendingSpawnCount` 규약** — 방 전투 시작 시 `PendingSpawnCount = CountTotalSpawns(Set)`으로 초기화하고 **1마리가 실제로 스폰될 때마다 1 감소**시킨다. 전멸 판정은 항상 `GetAliveEnemyCount() == 0 && PendingSpawnCount == 0`이다. 이 규약 덕에 **방1에서 웨이브1을 30초 안에 전멸시켜도 페이즈가 끝나지 않는다**(웨이브2가 아직 Pending). 방5는 여기에 더해 "전멸 시 예약 타이머를 취소하고 즉시 다음 웨이브"를 얹는다(§15.8).

#### 15.2.3 `Actor/Stage2/DRS2StageDirector.h/.cpp` — 배선 레지스트리

페이즈는 `NewObject`로 생성되는 `UObject`라 레벨 액터를 `EditInstanceOnly`로 직접 참조할 수 없다. 그래서 **레벨에 1개 배치하는 서버 전용 레지스트리**를 둔다(복제 불필요 — 참조 대상이 각자 복제된다).

```cpp
UCLASS()
class DAERUNE_API ADRS2StageDirector : public AActor
{
    GENERATED_BODY()
public:
    ADRS2StageDirector();
    virtual void BeginPlay() override;

    // 통로 8곳 (§1.1)
    UPROPERTY(EditInstanceOnly, Category = "S2|통로") TObjectPtr<ADRS2MovingBlocker> Blocker_StartToRoom1;
    UPROPERTY(EditInstanceOnly, Category = "S2|통로") TObjectPtr<ADRS2TeleportGate>  Gate_Room1ToRoom2;
    UPROPERTY(EditInstanceOnly, Category = "S2|통로") TObjectPtr<ADRS2TeleportGate>  Gate_Room2Exit;
    UPROPERTY(EditInstanceOnly, Category = "S2|통로") TObjectPtr<ADRS2MovingBlocker> Blocker_Room1ToRoom3;
    UPROPERTY(EditInstanceOnly, Category = "S2|통로") TObjectPtr<ADRS2TeleportGate>  Gate_Room3ToRoom4;
    UPROPERTY(EditInstanceOnly, Category = "S2|통로") TObjectPtr<ADRS2TeleportGate>  Gate_Room4Return;
    UPROPERTY(EditInstanceOnly, Category = "S2|통로") TObjectPtr<ADRS2MovingBlocker> Blocker_Room3ToRoom5;
    UPROPERTY(EditInstanceOnly, Category = "S2|통로") TObjectPtr<ADRS2MovingBlocker> Blocker_Room5ToRoom6;

    // 트리거 / 스폰
    UPROPERTY(EditInstanceOnly, Category = "S2|트리거") TObjectPtr<ADRS2RoomTrigger> Trigger_Room1;
    UPROPERTY(EditInstanceOnly, Category = "S2|트리거") TObjectPtr<ADRS2RoomTrigger> Trigger_Room3;
    UPROPERTY(EditInstanceOnly, Category = "S2|트리거") TObjectPtr<ADRS2RoomTrigger> Trigger_Room5;
    UPROPERTY(EditInstanceOnly, Category = "S2|스폰")   TArray<TObjectPtr<AActor>> Room1SpawnPoints;
    UPROPERTY(EditInstanceOnly, Category = "S2|스폰")   TArray<TObjectPtr<AActor>> Room3SpawnPoints;
    UPROPERTY(EditInstanceOnly, Category = "S2|스폰")   TArray<TObjectPtr<AActor>> Room5SpawnPoints;

    // 방2 / 방3·4 / 방6 (각 절에서 상세)
    UPROPERTY(EditInstanceOnly, Category = "S2|방2") TObjectPtr<ADRS2SlidePuzzle>  Room2SlidePuzzle;
    UPROPERTY(EditInstanceOnly, Category = "S2|방2") TObjectPtr<ADRS2SwitchPuzzle> Room2SwitchPuzzle;
    UPROPERTY(EditInstanceOnly, Category = "S2|방2") TObjectPtr<ADRS2CctvBoard>    Room2CctvBoard;
    UPROPERTY(EditInstanceOnly, Category = "S2|방2") TObjectPtr<ADRS2Safe>         Room2Safe;
    UPROPERTY(EditInstanceOnly, Category = "S2|방3") TArray<TObjectPtr<AActor>>    Room3RevivePoints;
    UPROPERTY(EditInstanceOnly, Category = "S2|방3") TObjectPtr<AActor>            Room4EntranceDropPoint;
    UPROPERTY(EditInstanceOnly, Category = "S2|방4") TObjectPtr<ADRCleanserSite>   Room4InstallSite;
    UPROPERTY(EditInstanceOnly, Category = "S2|방4") TObjectPtr<ADRS2MoleGame>     Room4MoleGame;
    UPROPERTY(EditInstanceOnly, Category = "S2|방6") TObjectPtr<ADRS2Train>        Train;
    UPROPERTY(EditInstanceOnly, Category = "S2|방6") TObjectPtr<ADRS2TrainTrack>   Track;
    UPROPERTY(EditInstanceOnly, Category = "S2|방6") TArray<TObjectPtr<ADRS2TrainObstacle>> Obstacles;
    UPROPERTY(EditInstanceOnly, Category = "S2|방6") TArray<TObjectPtr<ADRS2Barrier>> ForwardBarriers;
    UPROPERTY(EditInstanceOnly, Category = "S2|방6") TArray<TObjectPtr<ADRS2Barrier>> RearBarriers;
};
```

**배선 검증** — 레벨 작업 실수를 조기에 잡는다. 이게 없으면 "왜 문이 안 열리지"를 런타임에 디버깅하게 된다.

```cpp
void ADRS2StageDirector::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority()) return;

    int32 MissingCount = 0;
    auto Check = [&](const UObject* Ptr, const TCHAR* Name)
    {
        if (!IsValid(Ptr)) { UE_LOG(LogDR, Error, TEXT("[S2Director] 미배선: %s"), Name); ++MissingCount; }
    };

    Check(Blocker_StartToRoom1, TEXT("Blocker_StartToRoom1 (D0)"));
    Check(Gate_Room1ToRoom2,    TEXT("Gate_Room1ToRoom2 (D1)"));
    /* ... 전 항목 ... */

    if (Room1SpawnPoints.Num() < 6)
        UE_LOG(LogDR, Warning, TEXT("[S2Director] Room1SpawnPoints %d개 — 4인 웨이브1 9마리 동시 스폰에 부족할 수 있음"), Room1SpawnPoints.Num());
    if (Obstacles.Num() != 3 || ForwardBarriers.Num() != 3 || RearBarriers.Num() != 3)
        UE_LOG(LogDR, Error, TEXT("[S2Director] 방6 장애물/배리어는 각 3개여야 합니다."));

    UE_LOG(LogDR, Log, TEXT("[S2Director] 배선 검증 완료 — 누락 %d건"), MissingCount);
}
```

#### 15.2.4 `Actor/Stage2/DRS2RoomTrigger.h/.cpp` — 전원 입장 판정

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllAlivePlayersInside);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInsideCountChanged, int32, InsideCount, int32, AliveTotal);

UCLASS()
class DAERUNE_API ADRS2RoomTrigger : public AActor
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) void Arm();       // 서버: 감시 시작 + 폴링 타이머
    UFUNCTION(BlueprintCallable) void Disarm();    // 서버: 감시 중지 + 타이머 해제
    void ReArm();                                  // 발화 래치만 해제(부품 검증 실패 시 재무장)

    bool AreAllAlivePlayersInside() const;
    bool IsActorInside(const AActor* Actor) const; // 부품 동반 검증용

    FOnAllAlivePlayersInside OnAllInside;          // 1회 래치
    FOnInsideCountChanged    OnCountChanged;

protected:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> TriggerBox;
    UPROPERTY(EditAnywhere) FName RoomID;          // 로그용

private:
    void Evaluate();
    TSet<TWeakObjectPtr<ADRCharacter>> PlayersInside;
    FTimerHandle ReevaluateTimer;
    bool bArmed = false;
    bool bFired = false;
};
```

```cpp
void ADRS2RoomTrigger::Evaluate()
{
    if (!bArmed || bFired || !HasAuthority()) return;

    ADRGameStateBase* GS = GetWorld()->GetGameState<ADRGameStateBase>();
    if (!GS) return;

    const TArray<ADRCharacter*> Alive = GS->GetAlivePlayers();   // DRGameStateBase.cpp:91-115
    if (Alive.Num() == 0) return;                                // 전멸 처리는 GameMode 담당

    int32 InsideAlive = 0;
    for (ADRCharacter* C : Alive) { if (PlayersInside.Contains(C)) ++InsideAlive; }

    OnCountChanged.Broadcast(InsideAlive, Alive.Num());

    if (InsideAlive == Alive.Num())
    {
        bFired = true;
        OnAllInside.Broadcast();
    }
}
```

- **오버랩만으로는 부족한 이유**: 방 밖에서 누군가 죽으면 오버랩 이벤트가 발생하지 않아 조건이 영원히 성립하지 않는다. 그래서 `Arm()` 동안 **0.5초 폴링**으로 재평가한다(활성 구간에만 도므로 비용 무시 가능).
- `bFired` 래치는 `ReArm()`으로만 풀린다 — 방3의 부품 동반 검증 실패 시 사용한다(§15.7).
- 트리거 박스는 방 전체 + **통로 입구 안쪽까지** 덮어야 구조물 상승 시 통로에 사람이 남지 않는다(§8-5).

---

### 15.3 M1.3 — 통로 액터 2종 (스테이지2 통로 8곳 전부)

#### 15.3.1 `ADRS2MovingBlocker` — 구조물 4곳 (D0·D2·D4·D5)

```cpp
UCLASS()
class DAERUNE_API ADRS2MovingBlocker : public AActor
{
    GENERATED_BODY()
public:
    ADRS2MovingBlocker();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    UFUNCTION(BlueprintCallable) void SetBlocked(bool bNewBlocked);   // 서버 전용, 멱등

protected:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BlockerMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> PawnBlock;

    UPROPERTY(EditDefaultsOnly, Category = "S2|Blocker") FVector BlockedOffset = FVector(0, 0, 300.f);
    UPROPERTY(EditDefaultsOnly, Category = "S2|Blocker") FVector OpenOffset    = FVector::ZeroVector;
    UPROPERTY(EditDefaultsOnly, Category = "S2|Blocker") float   MoveDuration  = 1.5f;
    UPROPERTY(EditAnywhere,     Category = "S2|Blocker") bool    bStartBlocked = false;   // D4·D5 = true
    UPROPERTY(EditInstanceOnly, Category = "S2|Blocker") TObjectPtr<AActor> PushOutPoint;

    UPROPERTY(ReplicatedUsing = OnRep_bBlocked) bool bBlocked = false;
    UFUNCTION() void OnRep_bBlocked();

    UFUNCTION(BlueprintImplementableEvent) void OnMoveStarted(bool bNowBlocking);
    UFUNCTION(BlueprintImplementableEvent) void OnMoveFinished(bool bNowBlocking);

private:
    void FinishMove();
    float MoveElapsed = 0.f;
    bool  bMoving = false;
    FVector FromOffset, ToOffset;
};
```

**구현 요점**:

```cpp
void ADRS2MovingBlocker::SetBlocked(bool bNewBlocked)
{
    if (!HasAuthority() || bBlocked == bNewBlocked) return;   // ★멱등 — 방3/방5가 D4를 중복 호출할 수 있음
    bBlocked = bNewBlocked;
    OnRep_bBlocked();                                          // 리슨 서버 수동 호출
}

void ADRS2MovingBlocker::OnRep_bBlocked()
{
    FromOffset  = Root->GetRelativeLocation();
    ToOffset    = bBlocked ? BlockedOffset : OpenOffset;
    MoveElapsed = 0.f;
    bMoving     = true;
    SetActorTickEnabled(true);

    // ★열릴 때는 이동 "시작" 시 콜리전 해제 — 하강 중 벽에 갇히는 것 방지
    if (!bBlocked) { PawnBlock->SetCollisionEnabled(ECollisionEnabled::NoCollision); }

    OnMoveStarted(bBlocked);
}

void ADRS2MovingBlocker::Tick(float DeltaSeconds)
{
    if (!bMoving) return;
    MoveElapsed += DeltaSeconds;
    const float A = FMath::Clamp(MoveElapsed / FMath::Max(MoveDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
    Root->SetRelativeLocation(FMath::Lerp(FromOffset, ToOffset, A));
    if (A >= 1.f) FinishMove();
}

void ADRS2MovingBlocker::FinishMove()
{
    bMoving = false;
    SetActorTickEnabled(false);
    Root->SetRelativeLocation(ToOffset);      // 스냅

    if (bBlocked && HasAuthority())
    {
        // ★막힐 때는 이동 "완료" 후 콜리전 활성 — 상승 중 캐릭터를 천장으로 밀어 올리는 사고 방지
        PawnBlock->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

        // 통로에 남은 Pawn 밀어내기 (경계에 걸친 케이스 방어)
        if (PushOutPoint)
        {
            TArray<AActor*> Overlaps;
            PawnBlock->GetOverlappingActors(Overlaps, ADRCharacter::StaticClass());
            for (AActor* A : Overlaps)
            {
                A->SetActorLocation(PushOutPoint->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
            }
        }
    }
    OnMoveFinished(bBlocked);
}
```

- `BeginPlay`에서 `bStartBlocked`를 초기 상태로 적용한다(서버에서 `bBlocked = bStartBlocked` + 오프셋 즉시 스냅 + 콜리전 설정, 이동 연출 없음).
- **D4만 왕복**한다: 방3 페이즈가 `SetBlocked(false)`, 방5 페이즈가 `OnPhaseStart`에서 다시 `SetBlocked(false)`(멱등 no-op), 전원 입장 후 `SetBlocked(true)`.

#### 15.3.2 `ADRS2TeleportGate` — 게이트 4곳 (D1·E2·D3·R4)

```cpp
UENUM(BlueprintType)
enum class ES2GateMode : uint8 { Individual, TeamOnCarrier };

UENUM(BlueprintType)
enum class ES2GateEntryRule : uint8 { Anyone, CarrierOnly };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGateUsed, ADRCharacter*, Who);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamRecalled);

UCLASS()
class DAERUNE_API ADRS2TeleportGate : public AActor
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) void SetGateActive(bool bNewActive);          // 서버
    UFUNCTION(BlueprintCallable) void SetEntryRule(ES2GateEntryRule NewRule);  // 서버 (§14.3.5-A)

    FOnGateUsed      OnGateUsed;
    FOnTeamRecalled  OnTeamRecalled;

protected:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GateMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> TriggerBox;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> DefaultDestination;

    UPROPERTY(EditInstanceOnly) TObjectPtr<AActor> DestinationOverride;
    UPROPERTY(EditAnywhere) ES2GateMode Mode = ES2GateMode::Individual;
    UPROPERTY(EditAnywhere) ES2GateEntryRule EntryRule = ES2GateEntryRule::Anyone;
    UPROPERTY(EditAnywhere) bool bDeactivateOnUse = false;                      // D3 = true
    UPROPERTY(EditAnywhere) float DestinationSpreadRadius = 150.f;

    UPROPERTY(ReplicatedUsing = OnRep_bActive) bool bActive = false;
    UFUNCTION() void OnRep_bActive();

    UFUNCTION() void OnBeginOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&);

    UFUNCTION(BlueprintImplementableEvent) void OnGateActiveChanged(bool bNowActive);
    UFUNCTION(BlueprintImplementableEvent) void OnEntryDenied(ADRCharacter* Who);
    UFUNCTION(NetMulticast, Unreliable) void Multicast_PlayTeleportFX(ADRCharacter* Who);

private:
    void TeleportOne(ADRCharacter* C, int32 SlotIndex, int32 SlotTotal);
};
```

**오버랩 처리 (서버 전용)**:

```cpp
void ADRS2TeleportGate::OnBeginOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!HasAuthority() || !bActive) return;

    ADRCharacter* C = Cast<ADRCharacter>(Other);
    if (!C || ICombatInterface::Execute_IsDead(C)) return;   // ★IsDead 는 인터페이스 BlueprintNativeEvent

    // 진입 규칙 검사
    if (EntryRule == ES2GateEntryRule::CarrierOnly && !C->IsCarryingPart())
    {
        OnEntryDenied(C);      // 로컬 안내 연출 훅 (텔레포트 없음)
        return;
    }

    if (Mode == ES2GateMode::TeamOnCarrier && C->IsCarryingPart())
    {
        // ★부품 소지자 통과 → 생존자 전원 회수 (§14.2.7)
        ADRGameStateBase* GS = GetWorld()->GetGameState<ADRGameStateBase>();
        const TArray<ADRCharacter*> Alive = GS ? GS->GetAlivePlayers() : TArray<ADRCharacter*>();
        for (int32 i = 0; i < Alive.Num(); ++i) { TeleportOne(Alive[i], i, Alive.Num()); }
        OnTeamRecalled.Broadcast();
    }
    else
    {
        TeleportOne(C, 0, 1);
        OnGateUsed.Broadcast(C);
    }

    if (bDeactivateOnUse) { SetGateActive(false); }   // D3: 1명 통과 후 잠김
}

void ADRS2TeleportGate::TeleportOne(ADRCharacter* C, int32 SlotIndex, int32 SlotTotal)
{
    AActor* Dest = DestinationOverride ? DestinationOverride : nullptr;
    const FVector Base = Dest ? Dest->GetActorLocation() : DefaultDestination->GetComponentLocation();
    const FRotator Rot = Dest ? Dest->GetActorRotation() : DefaultDestination->GetComponentRotation();

    // 동시 진입 겹침 방지: 목적지 주변에 원형 분산
    FVector Offset = FVector::ZeroVector;
    if (SlotTotal > 1)
    {
        const float Angle = (2.f * PI * SlotIndex) / SlotTotal;
        Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * DestinationSpreadRadius;
    }

    C->SetActorLocationAndRotation(Base + Offset, Rot, false, nullptr, ETeleportType::TeleportPhysics);
    if (AController* PC = C->GetController()) { PC->SetControlRotation(Rot); }
    Multicast_PlayTeleportFX(C);
}
```

- **왜 서버 판정인가**: 위치 이동은 서버 권한이어야 클라 예측과 어긋나지 않는다. 오버랩 자체는 양쪽에서 발생하지만 실행은 `HasAuthority()`에서만.
- `bActive == false`면 오버랩을 무시하므로 전투 중 문에 붙어 있어도 이동하지 않는다.
- **부품은 attach 계층**이라 소지자가 텔레포트하면 함께 따라온다(별도 처리 불필요).

**인스턴스별 설정표** (레벨 배치 시):

| 게이트 | Mode | EntryRule | bDeactivateOnUse | 초기 Active | 목적지 |
|---|---|---|---|---|---|
| D1 (방1→방2) | Individual | Anyone | false | false | 방2 안 |
| E2 (방2 출구) | **TeamOnCarrier** | Anyone | false | **true** | 방1 회수 지점 |
| D3 (방3→방4) | Individual | **CarrierOnly** | **true** | false | 방4 입구 |
| R4 (방4→방3) | Individual | Anyone | false | false | 방3 안 |

---

### 15.4 M1.4 — 페이즈 5개 골격 + BP·맵 배선

**골격 페이즈의 목적**: 콘텐츠 없이 **페이즈 전환 체인 전체를 먼저 검증**한다. 각 페이즈는 목표 텍스트만 띄우고 트리거 입장(또는 치트) 즉시 완료된다.

```cpp
// 골격 단계의 최소 구현 예 (방1)
void UDRS2CombatPhase::OnPhaseStart()
{
    Super::OnPhaseStart();
    if (ADRS2StageDirector* Dir = GetDirector())
    {
        SetupPhaseObjectiveByRow(TEXT("S2P1_Enter"));
        if (Dir->Trigger_Room1)
        {
            Dir->Trigger_Room1->OnAllInside.AddDynamic(this, &UDRS2CombatPhase::HandleAllInside);
            Dir->Trigger_Room1->Arm();
        }
    }
}
void UDRS2CombatPhase::HandleAllInside() { bStub = true; GameMode->ValidatePhaseCompletion(); }
bool UDRS2CombatPhase::IsCompleted() const { return bStub; }
```

**BP·데이터 작업 순서**:

1. `BP_DRStage2GameMode` (부모 `ADRStageGameMode`) 생성 → `PhaseClasses = [BP_S2CombatPhase, BP_S2PuzzlePhase, BP_S2DefensePhase, BP_S2WavePhase, BP_S2TrainPhase]`
   - `GameStateClass`/`EnemyCharacterClassInfo`/`AbilityInfo`/`GameBalanceConfig` 등은 **`BP_DRStageGameMode` 값을 그대로 복사**한다(누락 시 GAS 초기화가 조용히 실패한다).
   - **`StageId`를 `"Stage2"`로 설정** — 진행/보상이 이 키로 조회된다(`DRStageGameMode.cpp:213` → `DRGameInstance.cpp:752`). 정의가 없으면 클리어해도 보상 0 + Warning(`:779`).
2. `DT_S2PhaseObjective` (행 구조 `FPhaseObjectiveData`) 생성 → §7.2 표대로 행 입력. **서브 목표 행은 `PhaseAlarmText`를 빈 값으로.**
3. 페이즈 BP 5종 생성 → 각각 `PhaseObjectiveDataTable = DT_S2PhaseObjective`
4. `Stage2.umap` WorldSettings → `GameMode Override = BP_DRStage2GameMode` (GameState는 기존 `BP_DRStageGameState` 유지)
5. 통로 8곳·트리거 3개·`BP_S2StageDirector` 1개 배치 후 **Director에 전부 배선**
6. 로비: `ADRStageSelectActor` 배치 + `DestinationMapName = "Stage2"` (`DRStageSelectActor.cpp:167` → `DRLobbyGameMode.cpp:213`)

**게이트**: PIE 2인에서 방을 순서대로 이동하기만 해도 5페이즈가 순차 전환되고 → `TriggerGameClear()` → 로비 복귀. 치트 `ServerCheatSkipToNextPhase`(`DRPlayerController.cpp:1416-1432`)로도 각 페이즈를 건너뛸 수 있어야 한다.

---

### 15.5 M2 — 방1 전투 (§14.1)

**BP 입력 (`BP_S2CombatPhase`)** — §14.1.3 표를 `WaveSetsByPlayerCount`에 그대로:

| 배열 인덱스 | 웨이브 0 (StartDelay 0) | 웨이브 1 (StartDelay **30**) |
|---|---|---|
| [0] 1인 | Dog×2, Armadillo×1 | Dog×2, DragonFly×1 |
| [1] 2인 | Dog×3, Armadillo×2 | Dog×2, DragonFly×1 |
| [2] 3인 | Dog×4, Armadillo×2, DragonFly×1 | Dog×3, DragonFly×1 |
| [3] 4인 | Dog×5, Armadillo×3, DragonFly×1 | Dog×3, DragonFly×1 |

- 클래스: `BP_Dog` / `BP_DRArmadillo` / `BP_DRDragonFly` (변종 선택은 §14.1.7-1)
- `PerEnemySpawnInterval`은 두 웨이브 모두 0.2 정도로 시작한다("막 나오는" 연출).

**구현**:

```cpp
void UDRS2CombatPhase::HandleAllInsideRoom1()
{
    ADRS2StageDirector* Dir = GetDirector();
    if (!Dir) return;

    // ① 인원 확정 (이후 사망해도 재계산 없음 — §14.1.4)
    ResolveBasePlayerCount();

    // ② D0 구조물 상승 → 시작지점 영구 봉쇄
    SetBlocked(Dir->Blocker_StartToRoom1, true);

    // ③ 웨이브 세트 조회 + 분모 확정
    const FS2WaveSet* Set = ResolveWaveSet(WaveSetsByPlayerCount);
    if (!Set) return;
    TotalSpawnCount   = CountTotalSpawns(*Set);     // 1인 6 / 2인 8 / 3인 11 / 4인 13
    PendingSpawnCount = TotalSpawnCount;

    // ④ 목표 교체 (분모를 런타임 값으로)
    SetupPhaseObjectiveByRow(TEXT("S2P1_Combat"), TotalSpawnCount);

    // ⑤ 웨이브 예약 (0초 / 30초) — 전멸 여부와 무관한 시간 기반
    bCombatStarted = true;
    ScheduleWaveSet(*Set, Dir->Room1SpawnPoints);
}

void UDRS2CombatPhase::OnEnemyDeath(AActor* DeadEnemy)
{
    Super::OnEnemyDeath(DeadEnemy);          // SpawnedEnemies 에서 제거
    if (!bCombatStarted) return;

    ++KilledCount;
    GameState->UpdatePhaseObjectiveProgress(KilledCount);

    if (GetAliveEnemyCount() == 0 && PendingSpawnCount == 0)
    {
        GameMode->ValidatePhaseCompletion();
    }
}

bool UDRS2CombatPhase::IsCompleted() const
{
    return bCombatStarted && GetAliveEnemyCount() == 0 && PendingSpawnCount == 0;
}
```

- **D1 게이트 발광은 여기서 하지 않는다.** 방2 페이즈의 `OnPhaseStart`가 켠다(§3.1 규약 — 게임오버 시 `OnPhaseEnd`에서 켜지는 부수효과 방지).
- **검증 포인트**: 웨이브1을 30초 전에 전멸시켜도 `PendingSpawnCount > 0`이라 페이즈가 끝나지 않아야 한다. 이게 깨지면 방1이 절반만 진행되고 넘어간다.

---

### 15.6 M3 — 방2 퍼즐 3종 + 금고 (§14.2)

#### 15.6.1 M3.1-① 상호작용 프롭 배관 (조작 대상 27개를 분기 1개로)

현재 상호작용 배관은 **타입별 감지 슬롯 + `ServerRequestInteract(AActor*)` 타입 분기** 구조다(`DRPlayerController.cpp:326-413`). 조작 대상 하나당 액터 하나로 만들면 **분기 1개만 추가**하면 된다.

```cpp
UCLASS(Abstract)
class DAERUNE_API ADRS2InteractProp : public AActor, public IDRInteractable
{
    GENERATED_BODY()
public:
    virtual bool CanInteract(const ADRCharacter* C) const;
    virtual void ServerHandleInteract(ADRCharacter* C) PURE_VIRTUAL(ADRS2InteractProp::ServerHandleInteract, );
    virtual void SetInteractionUIVisible(bool bShow) override;   // IDRInteractable

    UFUNCTION(BlueprintCallable) void SetPropEnabled(bool bNewEnabled);   // 서버

protected:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PropMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> InteractionWidget;   // "F" 프롬프트

    UPROPERTY(ReplicatedUsing = OnRep_bPropEnabled) bool bPropEnabled = true;
    UFUNCTION() void OnRep_bPropEnabled();

    UPROPERTY(EditInstanceOnly) TObjectPtr<AActor> OwnerPuzzle;   // 소속 퍼즐/금고
    UPROPERTY(EditInstanceOnly) int32 PropIndex = INDEX_NONE;     // 타일/레버/버튼 번호

    UFUNCTION(BlueprintImplementableEvent) void OnInteractedVisual();
    UFUNCTION(BlueprintImplementableEvent) void OnPropEnabledChanged(bool bNowEnabled);
};

bool ADRS2InteractProp::CanInteract(const ADRCharacter* C) const
{
    return bPropEnabled && IsValid(C) && !ICombatInterface::Execute_IsDead(const_cast<ADRCharacter*>(C)) && !C->IsCarryingPart();
}
```

**자식 4종은 전부 얇다** — 로직은 소유 퍼즐에 위임한다:

```cpp
void ADRS2SlideTile::ServerHandleInteract(ADRCharacter* C)
{
    if (ADRS2SlidePuzzle* P = Cast<ADRS2SlidePuzzle>(OwnerPuzzle)) { P->TryMoveTile(PropIndex); }
}
void ADRS2Lever::ServerHandleInteract(ADRCharacter* C)
{
    if (ADRS2SwitchPuzzle* P = Cast<ADRS2SwitchPuzzle>(OwnerPuzzle)) { P->ToggleLever(PropIndex); }
}
void ADRS2SafeButton::ServerHandleInteract(ADRCharacter* C)
{
    if (ADRS2Safe* S = Cast<ADRS2Safe>(OwnerPuzzle)) { S->PushDigit((uint8)PropIndex); }   // PropIndex = 0~9
}
```

**PlayerController 수정 3곳** (§5.5-b):

```cpp
// 1) 헤더에 감지 슬롯 추가 (CurrentDetectedPart :629 / CurrentDetectedMount :601 옆)
UPROPERTY() TObjectPtr<AActor> CurrentDetectedProp;

// 2) 감지 함수 (FindSiteByLineTrace :326-362 미러, 오버랩 집합 없이 거리 검사)
ADRS2InteractProp* ADRPlayerController::FindPropByLineTrace()
{
    ADRCharacter* C = GetPawn<ADRCharacter>();
    if (!C || C->IsCarryingPart()) return nullptr;      // 운반 중엔 퍼즐 조작 불가

    UCameraComponent* Cam = C->GetFollowCamera();
    if (!Cam) return nullptr;

    const FVector Start = Cam->GetComponentLocation();
    const FVector End   = Start + Cam->GetForwardVector() * LineTraceDistance;   // :442 = 250.f

    FHitResult Hit;
    FCollisionQueryParams Q; Q.AddIgnoredActor(C);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Q))
    {
        ADRS2InteractProp* Prop = Cast<ADRS2InteractProp>(Hit.GetActor());
        if (Prop && Prop->CanInteract(C)) return Prop;
    }
    return nullptr;
}

// 3) 감지 틱(:662-674)에 슬롯 갱신 + HandleInteract(:529)에 분기 + ServerRequestInteract(:371)에 분기
void ADRPlayerController::ServerRequestInteract_Implementation(AActor* Interactable)
{
    // ... 기존 Part / Site / Mount 분기 뒤에 병렬 추가 ...
    if (ADRS2InteractProp* Prop = Cast<ADRS2InteractProp>(Interactable))
    {
        if (!Prop->CanInteract(DRCharacter)) return;
        const float DistSq = FVector::DistSquared(DRCharacter->GetActorLocation(), Prop->GetActorLocation());
        if (DistSq > FMath::Square(MaxInteractDistance)) return;    // :106 = 800.f
        Prop->ServerHandleInteract(DRCharacter);
        return;
    }
}
```

- **상태를 프롭이 갖지 않는 이유**: 프롭 27개가 각자 복제하면 트래픽과 코드가 폭증한다. 상태는 퍼즐 액터 1개가 모아 복제하고, 프롭은 **입력 창구**만 한다.
- 동시 조작 경합은 **서버 RPC가 순차 처리**되므로 자연 해소된다(별도 락 불필요).

#### 15.6.2 M3.1-② 금고 + 코드 생성·배분

**★가장 먼저 만드는 이유**: 퍼즐 없이도 방2 전체 흐름(코드 → 금고 → 부품 → 퇴장 → 전원 회수)이 돌아 리스크를 앞당겨 걷어낸다.

```cpp
void UDRS2PuzzlePhase::OnPhaseStart()
{
    Super::OnPhaseStart();
    ADRS2StageDirector* Dir = GetDirector();
    if (!Dir) return;

    SetGateActive(Dir->Gate_Room1ToRoom2, true);          // D1 발광 (방1 클리어 보상)
    SetupPhaseObjectiveByRow(TEXT("S2P2_Move"));

    // ★코드는 페이즈가 한 번만 만들어 각 액터에 주입한다 (§14.2.6)
    SecretCode = { (uint8)FMath::RandRange(0,9), (uint8)FMath::RandRange(0,9), (uint8)FMath::RandRange(0,9) };
    if (Dir->Room2SlidePuzzle)  Dir->Room2SlidePuzzle ->SetRevealDigit(SecretCode[0]);
    if (Dir->Room2SwitchPuzzle) Dir->Room2SwitchPuzzle->SetRevealDigit(SecretCode[1]);
    if (Dir->Room2CctvBoard)    Dir->Room2CctvBoard   ->SetTargetImageCount(SecretCode[2]);
    if (Dir->Room2Safe)         Dir->Room2Safe        ->SetSecretCode(SecretCode);
    UE_LOG(LogDR, Verbose, TEXT("[S2] 금고 코드 %d%d%d"), SecretCode[0], SecretCode[1], SecretCode[2]);

    // 바인딩
    Dir->Room2SlidePuzzle->OnPuzzleSolved.AddDynamic(this, &UDRS2PuzzlePhase::HandlePuzzleSolved);
    Dir->Room2SwitchPuzzle->OnPuzzleSolved.AddDynamic(this, &UDRS2PuzzlePhase::HandlePuzzleSolved);
    Dir->Room2Safe->OnSafeOpened.AddDynamic(this, &UDRS2PuzzlePhase::HandleSafeOpened);
    Dir->Gate_Room2Exit->OnTeamRecalled.AddDynamic(this, &UDRS2PuzzlePhase::HandleTeamRecalled);

    SetupPhaseObjectiveByRow(TEXT("S2P2_Puzzle"), 2);     // 분모 2 = 스크린 퍼즐만 (CCTV 제외)
}
```

**금고 입력 검증**:

```cpp
void ADRS2Safe::PushDigit(uint8 Digit)
{
    if (!HasAuthority() || bOpened) return;
    InputDigits.Add(Digit);
    OnRep_Input();                               // 입력 표시창 갱신 (팀 공유)

    if (InputDigits.Num() >= SecretCode.Num()) { Validate(); }
}

void ADRS2Safe::Validate()
{
    const bool bMatch = (InputDigits == SecretCode);
    if (bMatch) { Open(); }
    else
    {
        InputDigits.Reset();                     // 페널티 없음 — 재입력 자유 (§14.2.5)
        OnRep_Input();
        Multicast_PlayWrongCodeFX();
    }
}

void ADRS2Safe::Open()
{
    bOpened = true;
    OnRep_bOpened();                             // 문 개방 연출

    // 내부 부품 스폰 → 기존 픽업 파이프라인 그대로
    if (PartClass && PartSpawnPoint)
    {
        ADRCleanserPart* Part = GetWorld()->SpawnActor<ADRCleanserPart>(PartClass, PartSpawnPoint->GetComponentTransform());
        OnSafeOpened.Broadcast(Part);
    }
}
```

- **정답 코드는 끝까지 복제하지 않는다**(서버 전용 `SecretCode`). 복제되는 건 `InputDigits`(입력 진행)와 `bOpened`뿐이다(§14.2.6).

#### 15.6.3 M3.1-③ 퇴장 처리

```cpp
void UDRS2PuzzlePhase::HandleTeamRecalled()      // E2 게이트가 전원 회수 완료 시 발화
{
    ADRS2StageDirector* Dir = GetDirector();
    SetGateActive(Dir->Gate_Room1ToRoom2, false);   // ★D1 비활성 → 방2 재입장 불가
    bCarrierExited = true;
    GameMode->ValidatePhaseCompletion();
}
bool UDRS2PuzzlePhase::IsCompleted() const { return bCarrierExited; }
```

#### 15.6.4 M3.2-① 8퍼즐 — 셔플·이동·해결 판정

**★셔플은 반드시 "빈칸 무작위 이동"으로 만든다.** 무작위 순열은 절반이 해결 불가능한 배치(패리티 홀수)라 소프트락이 된다.

```cpp
void ADRS2SlidePuzzle::GenerateShuffledBoard()
{
    // 목표 상태: [1,2,3,4,5,6,7,8,0] (0 = 빈칸)
    BoardState.Reset();
    for (uint8 i = 1; i <= 8; ++i) BoardState.Add(i);
    BoardState.Add(0);

    int32 Empty = 8;
    int32 LastEmpty = -1;
    for (int32 n = 0; n < ShuffleMoves; ++n)          // 기본 80회
    {
        TArray<int32> Neighbors = GetAdjacentIndices(Empty);   // 상하좌우 (격자 경계 처리)
        Neighbors.Remove(LastEmpty);                            // 직전 위치로 되돌아가지 않게
        if (Neighbors.Num() == 0) continue;

        const int32 Pick = Neighbors[FMath::RandRange(0, Neighbors.Num() - 1)];
        BoardState.Swap(Empty, Pick);
        LastEmpty = Empty;
        Empty = Pick;
    }
    InitialBoardState = BoardState;                   // 초기화 버튼용 (§14.2.9-4)
    OnRep_BoardState();
}

bool ADRS2SlidePuzzle::TryMoveTile(int32 TileId)
{
    if (!HasAuthority() || bSolved) return false;

    const int32 From  = BoardState.IndexOfByKey((uint8)TileId);
    const int32 Empty = BoardState.IndexOfByKey((uint8)0);
    if (From == INDEX_NONE || Empty == INDEX_NONE) return false;
    if (!AreAdjacent(From, Empty)) return false;      // 빈칸 인접이 아니면 무시 (페널티 없음)

    BoardState.Swap(From, Empty);
    LastMove = TPair<uint8,uint8>((uint8)TileId, (uint8)From);   // 되돌리기 1수만 보관
    OnRep_BoardState();
    CheckSolved();
    return true;
}

void ADRS2SlidePuzzle::UndoLastMove()
{
    if (!HasAuthority() || !LastMove.IsSet() || bSolved) return;
    TryMoveTileInternal(LastMove->Key);      // 인접 검사를 다시 타고 원위치로
    LastMove.Reset();                        // ★연속 되돌리기 불가 ("1개 전"만)
}

void ADRS2SlidePuzzle::CheckSolved()
{
    for (int32 i = 0; i < 8; ++i) { if (BoardState[i] != (uint8)(i + 1)) return; }
    bSolved = true;
    RevealedDigit = SecretDigit;             // ★이 순간에만 복제됨 (그전엔 -1)
    OnRep_RevealedDigit();
    OnPuzzleSolved.Broadcast(this);
}
```

**타일 시각화 (이동 복제 없음)**:

```cpp
void ADRS2SlidePuzzle::OnRep_BoardState()
{
    for (int32 Cell = 0; Cell < 9; ++Cell)
    {
        const uint8 TileId = BoardState[Cell];
        if (TileId == 0) continue;
        if (ADRS2SlideTile* Tile = FindTileById(TileId))
        {
            const FVector Target = CellToLocalLocation(Cell);      // (Cell%3, Cell/3) * CellSize
            Tile->StartSlideTo(Target, SlideDuration);             // 로컬 보간 0.15s
        }
    }
}
```

#### 15.6.5 M3.2-② 스위치 퍼즐 — 해가 존재하는 마스크 생성 ★

**마스크를 완전 랜덤으로 뽑으면 32개 조합 중 정답이 없는 라운드가 나와 소프트락이 된다.** 정답 부분집합에서 역산해야 한다.

```cpp
void ADRS2SwitchPuzzle::GenerateRound()
{
    constexpr uint16 ALL_ON = 0x1FF;      // 9비트 전부 1
    const int32 LeverCount = 5;

    for (int32 Attempt = 0; Attempt < 32; ++Attempt)
    {
        LeverMasks.SetNum(LeverCount);

        // ① 정답이 될 부분집합 S 선택 (공집합 아님)
        TArray<int32> S;
        while (S.Num() == 0)
        {
            for (int32 i = 0; i < LeverCount; ++i) { if (FMath::RandBool()) S.Add(i); }
        }

        // ② S 밖 레버 + S의 마지막을 제외한 레버는 자유 랜덤 (0 / ALL_ON 제외)
        for (int32 i = 0; i < LeverCount; ++i)
        {
            LeverMasks[i] = MakeRandomMask(MinBitsPerLever, MaxBitsPerLever);   // 3~5비트
        }

        // ③ S의 마지막 원소를 역산 → S 전체 XOR == ALL_ON 보장
        uint16 Acc = 0;
        for (int32 k = 0; k < S.Num() - 1; ++k) { Acc ^= LeverMasks[S[k]]; }
        LeverMasks[S.Last()] = ALL_ON ^ Acc;

        // ④ 검증: 32조합 완전탐색 (5개뿐이라 비용 무시)
        bool bSolvable = false;
        for (uint8 Combo = 1; Combo < 32; ++Combo)
        {
            uint16 Bits = 0;
            for (int32 i = 0; i < LeverCount; ++i) { if (Combo & (1 << i)) Bits ^= LeverMasks[i]; }
            if (Bits == ALL_ON) { bSolvable = true; break; }
        }
        // 마스크가 0이거나 중복이면 재시도
        if (bSolvable && !HasDegenerateMask()) break;
    }

    LeverBits = 0;
    BulbBits  = 0;
    OnRep_Levers();
    OnRep_Bulbs();
}

void ADRS2SwitchPuzzle::ToggleLever(int32 Index)
{
    if (!HasAuthority() || Index < 0 || Index >= LeverMasks.Num()) return;

    LeverBits ^= (1 << Index);

    // 전구 = ON 레버들의 마스크 XOR (라이트아웃 방식 — OR이면 다 켜기만 하면 끝나 퍼즐이 성립 안 함)
    BulbBits = 0;
    for (int32 i = 0; i < LeverMasks.Num(); ++i) { if (LeverBits & (1 << i)) BulbBits ^= LeverMasks[i]; }

    OnRep_Levers();
    OnRep_Bulbs();
    CheckRound();
}

void ADRS2SwitchPuzzle::CheckRound()
{
    if (BulbBits != 0x1FF) return;

    ++RoundsCleared;
    OnRep_Rounds();                                  // 성공 표시 1개 발광

    if (RoundsCleared >= RequiredRounds)             // 3회
    {
        RevealedDigit = SecretDigit;
        OnRep_RevealedDigit();
        OnPuzzleSolved.Broadcast(this);
    }
    else
    {
        GenerateRound();                             // 마스크 재생성 + 레버 전부 OFF
    }
}
```

- **마스크는 복제하지 않는다** — 클라에 정답 정보를 내려보내지 않기 위해 `BulbBits`/`LeverBits`/`RoundsCleared`만 복제한다.

#### 15.6.6 M3.3 CCTV — 유한 시퀀스 + 로컬 시뮬

**★매 스텝을 즉석 랜덤으로 뽑으면 "총 몇 개"라는 값 자체가 정의되지 않는다.** 판 시작 시 유한 시퀀스를 만들고 순환 재생해야 셀 수 있다.

```cpp
void ADRS2CctvBoard::BuildSequence()
{
    const int32 SlotTotal = StepCount * 2;              // 스텝당 정상 화면 2개
    if (TargetImageCount > SlotTotal)
    {
        UE_LOG(LogDR, Warning, TEXT("[S2CCTV] 타깃 %d개 > 슬롯 %d개 — StepCount를 늘리세요"), TargetImageCount, SlotTotal);
    }

    // ① 슬롯 배열 구성: 타깃 N개 + 나머지는 더미
    TArray<uint8> Slots;
    for (int32 i = 0; i < SlotTotal; ++i)
    {
        Slots.Add(i < TargetImageCount ? 0 : (uint8)(1 + FMath::RandRange(0, DummyImages.Num() - 1)));
    }
    for (int32 i = Slots.Num() - 1; i > 0; --i) Slots.Swap(i, FMath::RandRange(0, i));   // 셔플

    // ② 스텝별로 정상 화면 2개 선택 + 이미지 할당
    Sequence.Reset();
    for (int32 s = 0; s < StepCount; ++s)
    {
        FS2CctvStep Step;
        Step.NormalScreenA = (uint8)FMath::RandRange(0, 5);
        do { Step.NormalScreenB = (uint8)FMath::RandRange(0, 5); } while (Step.NormalScreenB == Step.NormalScreenA);
        Step.ImageA = Slots[s * 2];
        Step.ImageB = Slots[s * 2 + 1];
        Sequence.Add(Step);
    }

    StartServerTime = GetWorld()->GetGameState<ADRStageGameState>()->GetServerWorldTimeSeconds();
    OnRep_Sequence();
}

void ADRS2CctvBoard::Tick(float DeltaSeconds)      // 서버·클라 공통 로컬 시뮬
{
    if (Sequence.Num() == 0) return;

    const float Now = GetWorld()->GetGameState<ADRStageGameState>()->GetServerWorldTimeSeconds();
    const int32 Step = FMath::FloorToInt((Now - StartServerTime) / StepDuration) % Sequence.Num();
    if (Step == LastAppliedStep) return;            // 스텝이 바뀔 때만 갱신
    LastAppliedStep = Step;

    const FS2CctvStep& S = Sequence[Step];
    for (int32 i = 0; i < 6; ++i)
    {
        UTexture2D* Tex = ErrorImage;
        if (i == S.NormalScreenA) Tex = IndexToTexture(S.ImageA);
        else if (i == S.NormalScreenB) Tex = IndexToTexture(S.ImageB);
        ScreenMIDs[i]->SetTextureParameterValue(TEXT("ScreenTex"), Tex);
    }
}
```

- `IndexToTexture(0)` = 타깃 이미지, `1..N` = 더미. 시퀀스가 끝나면 `% Sequence.Num()`으로 **처음부터 반복**되어 놓쳐도 다시 셀 수 있다.
- **N = 0인 판**도 유효하다(타깃 미등장 → 마지막 자리 0). 테스트에 포함한다.
- 이미지 교체는 **머티리얼 인스턴스 다이내믹의 텍스처 파라미터**로 한다(SceneCapture 불필요 — 성능·멀티 이슈 없음).

---

### 15.7 M4 — 방3 방어 + 방4 두더지 (§14.3·§14.4)

#### 15.7.1 M4.1 방3 진입과 무한 웨이브

```cpp
void UDRS2DefensePhase::HandleAllInsideRoom3()
{
    ADRS2StageDirector* Dir = GetDirector();
    if (!Dir) return;

    // ★① 부품 동반 검증 — 실패 시 봉쇄하지 않고 트리거 재무장 (부품이 방1에 남는 소프트락 방지)
    if (!IsPartPresentInRoom3(Dir))
    {
        UE_LOG(LogDR, Verbose, TEXT("[S2] 부품 없이 전원 입장 — 봉쇄 보류"));
        Dir->Trigger_Room3->ReArm();
        return;
    }

    ResolveBasePlayerCount();
    SetBlocked(Dir->Blocker_Room1ToRoom3, true);                     // ② D2 상승 봉쇄

    Dir->Gate_Room3ToRoom4->SetEntryRule(ES2GateEntryRule::CarrierOnly);   // ③ 방4 문 발광
    Dir->Gate_Room3ToRoom4->SetGateActive(true);
    Dir->Gate_Room3ToRoom4->OnGateUsed.AddDynamic(this, &UDRS2DefensePhase::HandleRoom4Entered);

    Dir->Room4InstallSite->OnPartInstalled.AddDynamic(this, &UDRS2DefensePhase::HandlePartInstalled);
    Dir->Room4MoleGame->OnProgress.AddDynamic(this, &UDRS2DefensePhase::HandleMoleProgress);
    Dir->Room4MoleGame->OnCleared.AddDynamic(this, &UDRS2DefensePhase::HandleMoleGameCleared);

    SetupPhaseObjectiveByRow(TEXT("S2P3_Enter4"));
}

bool UDRS2DefensePhase::IsPartPresentInRoom3(ADRS2StageDirector* Dir) const
{
    for (TActorIterator<ADRCleanserPart> It(GetWorld()); It; ++It)
    {
        ADRCleanserPart* Part = *It;
        if (Part->IsCarriedNow() || Dir->Trigger_Room3->IsActorInside(Part)) return true;   // ★IsCarriedNow 접근자 신규 필요(§5.6과 함께)
    }
    return false;
}
```

**웨이브 루프 — 설치 시점에 시작, 50초 반복**:

```cpp
void UDRS2DefensePhase::HandlePartInstalled(ADRCleanserSite* /*Site*/)
{
    ADRS2StageDirector* Dir = GetDirector();
    Dir->Room4MoleGame->StartGame();          // 두더지 시작
    StartWaveLoop();                          // ★같은 순간 방3 웨이브 시작 (§14.3.2)
    SetupPhaseObjectiveByRow(TEXT("S2P3_Hold"), 20);
}

void UDRS2DefensePhase::StartWaveLoop()
{
    SpawnOneWave();                           // 첫 웨이브 즉시 (§14.3.6-3)
    GameMode->GetWorldTimerManager().SetTimer(
        WaveLoopTimer, this, &UDRS2DefensePhase::SpawnOneWave, WaveIntervalSeconds, /*bLoop=*/true);
}

void UDRS2DefensePhase::SpawnOneWave()
{
    if (GetAliveEnemyCount() >= MaxAliveEnemies)     // 안전장치 — 스킵만 하고 게임오버 아님
    {
        UE_LOG(LogDR, Warning, TEXT("[S2] 동시 생존 상한 도달 — 이번 웨이브 스킵"));
        return;
    }
    const FS2WaveSet* Set = ResolveWaveSet(WaveSetsByPlayerCount);
    if (Set && Set->Waves.Num() > 0)
    {
        SpawnComposition(Set->Waves[0], GetDirector()->Room3SpawnPoints);   // 구성 1개를 반복 사용
    }
}
```

- 방3은 **전멸 판정을 쓰지 않는다**(무한 웨이브). `OnEnemyDeath`는 진행도 표시에만 쓰고 완료 판정에 관여하지 않는다.

#### 15.7.2 M4.2 두더지 — 기존 데미지 파이프라인에 얹기 ★

**핵심 사실 2가지** (실측):
1. 프로젝트의 모든 플레이어 공격은 `UDRAbilitySystemLibrary::ApplyDamageEffect()`(`:424`) 단일 관문을 지난다.
2. 아군 판정 `IsNotFriend()`(`:415-422`)는 **액터 태그 기반**(`"Player"` / `"Enemy"`)이다.

→ 두더지에 **`Enemy` 태그 + 최소 ASC + AttributeSet**만 주면 기존 공격이 전부 통한다. 2m 규칙만 관문에 얹으면 된다.

```cpp
// ① 인터페이스 (Interaction/DRProximityHitOnly.h)
UINTERFACE(MinimalAPI) class UDRProximityHitOnly : public UInterface { GENERATED_BODY() };
class DAERUNE_API IDRProximityHitOnly
{
    GENERATED_BODY()
public:
    virtual bool AcceptsHitFrom(const AActor* Attacker) const = 0;
};

// ② ApplyDamageEffect 진입부에 분기 4줄 (§5.11)
FGameplayEffectContextHandle UDRAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectParams& P)
{
    const AActor* SourceAvatar = P.SourceAbilitySystemComponent->GetAvatarActor();
    const AActor* TargetAvatar = P.TargetAbilitySystemComponent ? P.TargetAbilitySystemComponent->GetAvatarActor() : nullptr;

    // ★근접 전용 대상: 허용 반경 밖이면 데미지 자체를 무시 (투과)
    if (const IDRProximityHitOnly* Prox = Cast<IDRProximityHitOnly>(TargetAvatar))
    {
        if (!Prox->AcceptsHitFrom(SourceAvatar)) return FGameplayEffectContextHandle();
    }
    // ... 기존 로직 그대로 ...
}

// ③ 두더지
void ADRS2Mole::BeginPlay()
{
    Super::BeginPlay();
    Tags.AddUnique(FName("Enemy"));       // ★이 한 줄로 IsNotFriend 를 통과해 공격 대상이 된다
    if (HasAuthority())
    {
        InitializeAttributes();           // MaxHealth = 1 → 어떤 공격이든 1히트 소멸
        GetWorldTimerManager().SetTimer(LifetimeTimer, this, &ADRS2Mole::HandleExpired, Lifetime, false);
        OnEmergeVisual();
    }
}

bool ADRS2Mole::AcceptsHitFrom(const AActor* Attacker) const
{
    if (!IsValid(Attacker)) return false;
    return FVector::DistSquared(GetActorLocation(), Attacker->GetActorLocation())
           <= FMath::Square(ProximityRadius);      // 200uu = 2m
}
```

**게임 관리 — 티어 전환**:

```cpp
const FS2MoleTier& ADRS2MoleGame::GetCurrentTier() const
{
    // KillThreshold 내림차순으로 첫 매치 (0 / 5 / 12)
    for (int32 i = Tiers.Num() - 1; i >= 0; --i)
    {
        if (KillCount >= Tiers[i].KillThreshold) return Tiers[i];
    }
    return Tiers[0];
}

void ADRS2MoleGame::SpawnMole()
{
    if (!bActive || ActiveMoles.Num() >= MaxConcurrentMoles) return;

    const FS2MoleTier& Tier = GetCurrentTier();
    int32 Idx = FMath::RandRange(0, SpawnPoints.Num() - 1);
    if (SpawnPoints.Num() > 1 && Idx == LastPointIndex) Idx = (Idx + 1) % SpawnPoints.Num();
    LastPointIndex = Idx;

    ADRS2Mole* Mole = GetWorld()->SpawnActor<ADRS2Mole>(MoleClass, SpawnPoints[Idx]->GetActorTransform());
    Mole->InitFromGame(this, Tier.MoleLifetime);    // 유지 시간 주입
    ActiveMoles.Add(Mole);
}

void ADRS2MoleGame::OnMoleKilled(ADRS2Mole* Mole)
{
    ActiveMoles.Remove(Mole);
    ++KillCount;
    OnRep_KillCount();
    OnProgress.Broadcast(KillCount, GoalKills);

    if (KillCount >= GoalKills) { StopSpawning(); OnCleared.Broadcast(); return; }
    RestartSpawnTimerWithCurrentTier();             // ★티어가 바뀌었으면 다음 스폰부터 새 간격 적용
}
```

- **스폰 간격 < 유지 시간**이라 두더지가 동시에 2~3마리 존재한다(§14.4.5-2). `MaxConcurrentMoles`로 상한을 둔다.
- 이미 나와 있는 두더지의 잔여 유지 시간은 티어가 바뀌어도 **갱신하지 않는다**.

#### 15.7.3 M4.3 클리어 처리·부활·예외

```cpp
void UDRS2DefensePhase::HandleMoleGameCleared()
{
    ADRS2StageDirector* Dir = GetDirector();

    Dir->Gate_Room4Return->SetGateActive(true);           // ① 방4 복귀 게이트
    ReviveAllDeadPlayers(Dir);                            // ② 부활
    SetBlocked(Dir->Blocker_Room3ToRoom5, false);         // ③ D4 하강 개방
    StopWaveLoop();
    KillAllSpawnedEnemies();                              // ④ 잔적 즉시 사망

    bMoleGameCleared = true;
    GameMode->ValidatePhaseCompletion();
}

void UDRS2DefensePhase::ReviveAllDeadPlayers(ADRS2StageDirector* Dir)
{
    int32 Slot = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ADRCharacter* C = Cast<ADRCharacter>(It->Get()->GetPawn());
        if (!C || !ICombatInterface::Execute_IsDead(C)) continue;

        const FVector Where = Dir->Room3RevivePoints.IsValidIndex(Slot)
                            ? Dir->Room3RevivePoints[Slot]->GetActorLocation()
                            : Dir->Room3RevivePoints[0]->GetActorLocation();
        C->Revive(Where, ReviveHealthRatio);              // 최대 체력의 50% (§5.9)
        ++Slot;
    }
}

void UDRS2DefensePhase::KillAllSpawnedEnemies()
{
    // 역순 순회 — OnEnemyDeath 콜백이 SpawnedEnemies 를 수정한다
    for (int32 i = SpawnedEnemies.Num() - 1; i >= 0; --i)
    {
        if (SpawnedEnemies[i].IsValid())
        {
            if (ICombatInterface* Combat = Cast<ICombatInterface>(SpawnedEnemies[i].Get())) { Combat->Die(FVector::ZeroVector); }
        }
    }
}
```

**`ADRCharacterBase::Revive()` — 사망 처리의 역연산 체크리스트** (§5.9):

`MulticastHandleDeath_Implementation`(`DRCharacterBase.cpp:165-204`)이 한 일을 **1:1로 되돌린다**. 하나라도 빠지면 "살아있지만 조작 불가"가 된다.

| # | 사망 시 처리 | 부활 시 역연산 |
|---|---|---|
| 1 | `bDead = true` | `bDead = false` + `OnRep_Dead()` 역경로 |
| 2 | 캡슐/메시 콜리전 해제 | `SetCollisionEnabled` 복원 |
| 3 | CMC 정지 (`MOVE_None` 등) | `SetMovementMode(MOVE_Walking)` |
| 4 | 래그돌/물리 시뮬 | 물리 해제 + 메시 트랜스폼 원복 |
| 5 | Dissolve 머티리얼 진행 | 머티리얼 파라미터 원복 |
| 6 | 사망 몽타주 재생 | `StopAnimMontage()` |
| 7 | 1인칭 메시 숨김 + 3인칭 `SetOwnerNoSee(false)` (`:189-203`) | 1인칭 재표시 + 3인칭 `SetOwnerNoSee(true)` |
| 8 | 사망 카메라 애니메이션 (`PlayDeathCameraAnimation`) | 카메라 원복 |
| 9 | GameplayCue `GameplayCue_Player_Death` | 부활 Cue 재생 |
| 10 | 사망 태그/디버프 잔존 | 태그 정리 + 디버프 GE 제거 |
| 11 | (관전 전환) | 관전 해제 + 폰 재빙의 확인 |
| 12 | — | **위치 이동** (부활 지점) + `Health = MaxHealth * 0.5` + Water 처리(§14.3.6-6) |

```cpp
void ADRCharacterBase::Revive(const FVector& Where, float HealthRatio)
{
    if (!HasAuthority() || !bDead) return;
    SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
    // 어트리뷰트 복원
    UDRAttributeSet* AS = /* ... */;
    AS->SetHealth(AS->GetMaxHealth() * HealthRatio);
    bDead = false;
    Multicast_HandleRevive();       // 위 표의 2~11을 전 클라에서 되돌림
}
```

**예외 A — 방4 플레이어 사망** (`NotifyPlayerDied` 경로):

```cpp
void UDRS2DefensePhase::NotifyPlayerDied(APlayerState* Dead)
{
    if (!Room4Player.IsValid() || Room4Player->GetPlayerState<APlayerState>() != Dead) return;

    ADRS2StageDirector* Dir = GetDirector();
    Dir->Room4MoleGame->AbortAndReset();                          // ① 두더지 전멸 + KillCount = 0
    Dir->Room4InstallSite->EjectInstalledPart();                  // ② 설치 해제 + 옆에 드롭 (§5.12)
    Dir->Gate_Room3ToRoom4->SetEntryRule(ES2GateEntryRule::Anyone);   // ★③ 소지 조건 해제 — 없으면 소프트락!
    Dir->Gate_Room3ToRoom4->SetGateActive(true);
    Room4Player = nullptr;
    // ④ 방3 웨이브는 그대로 유지 (중단하지 않음)
}
```

**예외 B — 접속 종료** (`Logout` 신규 오버라이드 §5.10):

```cpp
// DRGameModeBase.cpp — 신규 오버라이드 (현재 프로젝트에 Logout 오버라이드가 없음)
void ADRGameModeBase::Logout(AController* Exiting)
{
    if (APlayerState* PS = Exiting ? Exiting->PlayerState : nullptr)
    {
        if (ADRStageGameMode* SGM = Cast<ADRStageGameMode>(this))
        {
            if (UDRPhaseBase* Phase = SGM->GetCurrentPhase()) { Phase->NotifyPlayerLeft(PS); }
        }
    }
    Super::Logout(Exiting);
}

// 페이즈 측
void UDRS2DefensePhase::NotifyPlayerLeft(APlayerState* Left)
{
    if (!Room4Player.IsValid() || Room4Player->GetPlayerState<APlayerState>() != Left) return;

    ADRS2StageDirector* Dir = GetDirector();
    Dir->Room4MoleGame->AbortAndReset();
    StopWaveLoop();
    DestroyAllSpawnedEnemies();                                   // ★방3 몬스터 전부 소멸
    MovePartTo(Dir->Room4EntranceDropPoint->GetActorLocation());  // 부품을 방3 쪽 문 앞으로
    Dir->Gate_Room3ToRoom4->SetEntryRule(ES2GateEntryRule::CarrierOnly);
    Dir->Gate_Room3ToRoom4->SetGateActive(true);
    ResolveBasePlayerCount();                                     // 영구 이탈 반영 (§14.3.6-7)
    Room4Player = nullptr;
}
```

---

### 15.8 M5 — 방5 3웨이브 하이브리드 (§14.5)

**방1·방3과 다른 유일한 지점**: 웨이브 전환이 `min(30초 경과, 전원 전멸)`이다.

```cpp
void UDRS2WavePhase::StartWave(int32 i)
{
    CurrentWaveIndex = i;
    GameState->SetCurrentWaveNumber(i + 1);
    SetupPhaseObjectiveByRow(TEXT("S2P4_Wave"), 3);
    GameState->UpdatePhaseObjectiveProgress(i + 1);
    GameState->Multicast_PlayWaveStartSound();

    const FS2WaveSet* Set = ResolveWaveSet(WaveSetsByPlayerCount);
    PendingSpawnCount += CountComposition(Set->Waves[i]);
    SpawnComposition(Set->Waves[i], GetDirector()->Room5SpawnPoints);

    // ★마지막 웨이브에는 타이머를 걸지 않는다 — 4번째 웨이브가 생기면 클리어 불가
    if (i < Set->Waves.Num() - 1)
    {
        GameMode->GetWorldTimerManager().SetTimer(
            NextWaveTimer, FTimerDelegate::CreateUObject(this, &UDRS2WavePhase::StartWave, i + 1),
            WaveIntervalSeconds, false);
    }
}

void UDRS2WavePhase::OnEnemyDeath(AActor* DeadEnemy)
{
    Super::OnEnemyDeath(DeadEnemy);
    if (!bWavesStarted) return;

    // ★판정 기준은 웨이브 단위가 아니라 전체 생존 0 — 30초로 겹쳐 스폰된 잔존분까지 포함
    if (GetAliveEnemyCount() > 0 || PendingSpawnCount > 0) return;

    const FS2WaveSet* Set = ResolveWaveSet(WaveSetsByPlayerCount);
    if (CurrentWaveIndex < Set->Waves.Num() - 1)
    {
        GameMode->GetWorldTimerManager().ClearTimer(NextWaveTimer);   // 예약된 30초 취소
        StartWave(CurrentWaveIndex + 1);                              // 즉시 다음 (새 30초 재예약)
    }
    else
    {
        GameMode->ValidatePhaseCompletion();
    }
}
```

**BP 입력** — §14.5.2 표 (1인 총 9 / 2인 11 / 3인 16 / 4인 20마리). `StartDelaySeconds`는 **쓰지 않는다**(전환을 페이즈가 직접 제어).

---

### 15.9 M6 — 방6 열차 (§14.6)

#### 15.9.1 M6.1 열차 이동 — 1회 복제 + 로컬 시뮬

```cpp
void ADRS2Train::DepartTo(float TargetDistance, float Speed)
{
    if (!HasAuthority()) return;
    Movement.State           = ES2TrainState::Moving;
    Movement.StartDistance   = CurrentDistance;
    Movement.TargetDistance  = TargetDistance;
    Movement.Speed           = Speed;
    Movement.StartServerTime = GetWorld()->GetGameState<ADRStageGameState>()->GetServerWorldTimeSeconds();
    OnRep_Movement();
}

void ADRS2Train::Tick(float DeltaSeconds)     // 서버·클라 공통
{
    if (Movement.State != ES2TrainState::Moving || !Track) return;

    const float Now  = GetWorld()->GetGameState<ADRStageGameState>()->GetServerWorldTimeSeconds();
    const float Dist = FMath::Min(Movement.TargetDistance,
                                  Movement.StartDistance + Movement.Speed * (Now - Movement.StartServerTime));
    CurrentDistance = Dist;

    // ㄷ자 코너의 회전까지 스플라인이 처리해 준다
    const FTransform Xf = Track->Spline->GetTransformAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
    SetActorTransform(Xf);

    if (HasAuthority() && Dist >= Movement.TargetDistance) { ArriveAtTarget(); }
}
```

- `bReplicateMovement`는 **끈다**. 좌석·탑승자는 attach 계층이라 자동 추종한다.
- 탑승 중 캐릭터는 `MOVE_None`이라 스윕이 없어 열차 콜리전과 간섭하지 않는다.

#### 15.9.2 M6.2 좌석 — 마운트 패턴 미러

기존 마운트 구현(`DRCharacter.h:118-137`의 `MountedOn` RepNotify + `AttachToMountSocket`)을 **병렬 상태로 미러링**한다. 마운트 타입이 `ADRRobotVacuumCharacter`로 고정돼 있어 일반화보다 병렬 추가가 안전하다.

```cpp
// DRCharacter.h 추가
UPROPERTY(ReplicatedUsing = OnRep_SeatedOn, BlueprintReadOnly, Category = "Train")
TObjectPtr<ADRS2TrainSeat> SeatedOn;

UFUNCTION() void OnRep_SeatedOn();
bool IsSeatedOnTrain() const { return SeatedOn != nullptr; }
void SetSeatedOn(ADRS2TrainSeat* Seat);       // 서버: attach + MOVE_None / 해제 시 원복
```

```cpp
bool ADRS2TrainSeat::CanBeBoardedBy(const ADRCharacter* C) const
{
    // DRRobotVacuumCharacter::CanBeMountedBy(:51-52) 검증 항목 미러
    return !SeatedCharacter && IsValid(C) && !ICombatInterface::Execute_IsDead(const_cast<ADRCharacter*>(C))
        && !C->IsCarryingPart() && !C->IsMounted()
        && OwningTrain && OwningTrain->IsWaitingForBoarding();
}
```

- 하차는 **점프 입력** 경로에서 처리한다: `IsSeatedOnTrain()`이면 `ServerRequestTrainDeboard()` → 서버가 `Train->CanDeboardNow()`(정지 중에만 true) 확인 후 `Seat->Deboard()`. 마운트 하차 분기(`DRPlayerController.cpp:1150`) 옆에 병렬 추가한다.

#### 15.9.3 M6.3 보스 3구간 루프 ★체력이 이어지게

**절대 `Destroy`하지 않는다.** 새로 스폰하면 체력이 100%로 리셋되어 사양(67%→34%→처치)이 깨진다.

```cpp
void UDRS2TrainPhase::HandleTrainStopped(int32 i)
{
    ADRS2StageDirector* Dir = GetDirector();
    CurrentObstacleIndex = i;

    Dir->Obstacles[i]->BreakByBoss();                       // ★장애물은 보스 등장과 동시에 부서진다
    Dir->ForwardBarriers[i]->SetBarrierEnabled(true);       // 전방 (장애물이 부서지므로 필수)
    Dir->RearBarriers[i]->SetBarrierEnabled(true);          // 후방

    if (i == 0)
    {
        MoleBoss = Cast<ADREnemy>(SpawnEnemyAt(MoleBossClass, Dir->Obstacles[0]->GetBossSpawnTransform()));
        CachedMaxHealth = MoleBoss->GetMaxHealth();
    }
    else
    {
        ReappearBoss(Dir->Obstacles[i]->GetBossSpawnTransform());   // ★같은 개체 재등장 (체력 유지)
    }

    MoleBoss->OnHealthChanged.AddDynamic(this, &UDRS2TrainPhase::HandleBossHealthChanged);
    SetupPhaseObjectiveByRow(TEXT("S2P5_Boss"), 3);
    GameState->UpdatePhaseObjectiveProgress(i + 1);
}

void UDRS2TrainPhase::HandleBossHealthChanged(float NewValue)
{
    const int32 i = CurrentObstacleIndex;
    if (!RetreatHealthRatios.IsValidIndex(i)) return;       // ★3구간(i==2)은 임계 없음 — 사망만이 조건
    if (bSegmentResolved.IsValidIndex(i) && bSegmentResolved[i]) return;

    if (CachedMaxHealth > 0.f && (NewValue / CachedMaxHealth) <= RetreatHealthRatios[i])
    {
        ResolveSegment(i);
    }
}

void UDRS2TrainPhase::ResolveSegment(int32 i)
{
    if (bSegmentResolved[i]) return;
    bSegmentResolved[i] = true;

    // ① 바인딩 해제 먼저 (숨기는 동안 중복 발화 방지)
    MoleBoss->OnHealthChanged.RemoveDynamic(this, &UDRS2TrainPhase::HandleBossHealthChanged);

    // ② 도망 = 비활성 보관 (Destroy 아님!)
    MoleBoss->SetActorHiddenInGame(true);
    MoleBoss->SetActorEnableCollision(false);
    MoleBoss->SetActorTickEnabled(false);
    if (AAIController* AI = Cast<AAIController>(MoleBoss->GetController())) { AI->BrainComponent->StopLogic(TEXT("Retreat")); }
    OnBossRetreat();                                        // BP 연출 훅

    GetDirector()->ForwardBarriers[i]->SetBarrierEnabled(false);   // ③ 전방 개방 (열차 통과용)
    SetupPhaseObjectiveByRow(TEXT("S2P5_Board"));                  // ④ 재탑승 목표
}

void UDRS2TrainPhase::ReappearBoss(const FTransform& Xf)
{
    MoleBoss->SetActorTransform(Xf);
    MoleBoss->SetActorHiddenInGame(false);
    MoleBoss->SetActorEnableCollision(true);
    MoleBoss->SetActorTickEnabled(true);
    if (AAIController* AI = Cast<AAIController>(MoleBoss->GetController())) { AI->BrainComponent->RestartLogic(); }
    OnBossReappear();
    // ★체력은 손대지 않는다 — 67% → 34% → 0% 로 이어지는 것이 사양
}

void UDRS2TrainPhase::OnEnemyDeath(AActor* DeadEnemy)
{
    Super::OnEnemyDeath(DeadEnemy);
    if (DeadEnemy != MoleBoss.Get()) return;

    if (CurrentObstacleIndex == GetDirector()->Obstacles.Num() - 1)
    {
        bBossDefeated = true;
        GameMode->ValidatePhaseCompletion();      // ★최종 처치 = 스테이지2 클리어 (도착지점 없음)
    }
    else
    {
        ResolveSegment(CurrentObstacleIndex);     // 임계 전에 즉사시킨 예외도 진행으로 인정
    }
}
```

- 마지막 페이즈이므로 완료 시 `TriggerGameClear()`가 자동 호출된다(`DRStageGameMode.cpp:343-348`).
- **보스 스킬은 미구현 상태로 진행**한다. 페이즈는 `MoleBossClass`와 체력 델리게이트만 참조하므로 나중에 실제 두더지 보스로 교체해도 이 코드는 그대로다(§14.6.7).

---

### 15.10 신규 파일 체크리스트 (생성 순서)

| # | 파일 | 마일스톤 |
|---|---|---|
| 1 | `Phase/Stage2/DRS2Types.h` | M1.2 |
| 2 | `Phase/Stage2/DRS2PhaseBase.h/.cpp` | M1.2 |
| 3 | `Actor/Stage2/DRS2StageDirector.h/.cpp` | M1.2 |
| 4 | `Actor/Stage2/DRS2RoomTrigger.h/.cpp` | M1.2 |
| 5 | `Actor/Stage2/DRS2MovingBlocker.h/.cpp` | M1.3 |
| 6 | `Actor/Stage2/DRS2TeleportGate.h/.cpp` | M1.3 |
| 7 | `Phase/Stage2/DRS2CombatPhase.h/.cpp` | M1.4 골격 → M2 |
| 8 | `Phase/Stage2/DRS2PuzzlePhase.h/.cpp` | M1.4 골격 → M3 |
| 9 | `Phase/Stage2/DRS2DefensePhase.h/.cpp` | M1.4 골격 → M4 |
| 10 | `Phase/Stage2/DRS2WavePhase.h/.cpp` | M1.4 골격 → M5 |
| 11 | `Phase/Stage2/DRS2TrainPhase.h/.cpp` | M1.4 골격 → M6 |
| 12 | `Actor/Stage2/DRS2InteractProp.h/.cpp` (+자식 4종) | M3.1 |
| 13 | `Actor/Stage2/DRS2Safe.h/.cpp`, `DRS2CodeScreen.h/.cpp` | M3.1 |
| 14 | `Actor/Stage2/DRS2SlidePuzzle.h/.cpp` | M3.2 |
| 15 | `Actor/Stage2/DRS2SwitchPuzzle.h/.cpp` | M3.2 |
| 16 | `Actor/Stage2/DRS2CctvBoard.h/.cpp` | M3.3 |
| 17 | `Interaction/DRProximityHitOnly.h` | M4.2 |
| 18 | `Actor/Stage2/DRS2Mole.h/.cpp`, `DRS2MoleGame.h/.cpp` | M4.2 |
| 19 | `Actor/Stage2/DRS2TrainTrack.h/.cpp`, `DRS2Train.h/.cpp` | M6.1 |
| 20 | `Actor/Stage2/DRS2TrainSeat.h/.cpp` | M6.2 |
| 21 | `Actor/Stage2/DRS2TrainObstacle.h/.cpp`, `DRS2Barrier.h/.cpp` | M6.3 |

**기존 파일 수정** (§5 요약): `DRStageGameMode.cpp`(가드) · `DRPhaseBase.h/.cpp`(행 조회·알람 컬럼) · `DRStageGameState.h/.cpp`(UI 플래그) · `OverlayWidgetController.h/.cpp`(플래그 구독) · `DRPhase3.cpp`(플래그 2줄) · `DRPlayerController.h/.cpp`(프롭·좌석 분기) · `DRCharacter.h/.cpp`(좌석 상태) · `DRCharacterBase.h/.cpp`(부활) · `DRGameModeBase.h/.cpp`(Logout) · `DRAbilitySystemLibrary.h/.cpp`(근접 게이트) · `DRCleanserSite.h/.cpp`(부품 배출) · `DRCleanserPart.h/.cpp`(픽업 델리게이트)
