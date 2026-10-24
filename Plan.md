# Plan.md — 로비 옷장(코스메틱) + 스팀 도전과제 해금

> **상태 (2026-10-20 — 2인 PIE 실측 확인)**
> - **요구사항 완결점(M3)까지 끝** — 옷장에서 고른 옷이 ★다른 플레이어 화면에도 보이고★, 죽었다 살아나도 · 스테이지로 넘어가도 그대로다.
>   대기실 캐릭터도 입는다(M5). 리슨 서버 + 클라이언트 2인 PIE 로 확인 (8.0).
> - **실제 옷 8벌 — ★로봇 4종 공용★** 이 카탈로그에 들어갔다 — 토끼 · 신사 · 악마 · 천사 4세트 (9.0). 해금도 공용이다(D-31).
>   로봇마다 부위 소켓(`Cosmetic_*`)을 달아 같은 옷이 체형에 맞게 붙고, 안 맞는 옷만 로봇별로 덮어쓴다(D-32). 테스트 고깔은 전부 지웠다.
> - **남은 것** — ① B3 업적 데이터 ★기획 결정이 먼저★ ② B4 세이브 계정 키잉 ★단독 커밋 · 스팀 켜고 확인★ ③ B7 스팀(실 AppId 이후)
>   ④ 콘텐츠 — 다른 로봇 옷 · 아이콘 · 자물쇠 아트 · 해금 토스트 UI (9장).
>
> 최종 갱신: 2026-10-20 / 브랜치 `feat/closet` (커밋 전)
> 선행 문서: Plan2.md(재화·슬롯·칩 — 이 문서의 "보고 → 정화 → 복제" 선례), Plan4.md(현지화). 로봇은 벨보이가 추가돼 **4종**이다.
>
> 📌 **문서 구성** — **1부는 이미 된 것의 요약**(다시 만들거나 고칠 때 필요한 결정 · 함정 · 코드 위치만).
> **2부는 남은 일을 순서대로**, 처음 보는 사람도 따라 할 수 있게 적었다. 위에서부터 차례로 하면 된다.
>
> ⚠️ **코드 주석 표기 규칙** — 이 문서는 코드에서 **`Plan(옷장)`** 접두 + **결정 번호 D-n / 함정 번호 T-n / 마일스톤 M-n** 으로 가리킨다.
> 절 번호(§ · A6-1 같은 단계 번호)는 문서가 바뀔 때마다 조용히 틀어지므로 코드에 쓰지 않는다.
> D/T 번호는 영구 ID 다 — 빠진 번호는 결번이며 재사용하지 않는다. **다음 번호: D-33 · T-42.**

---

## 0. 한눈에 보기

### 0.1 무엇을 만드나

| 요구 | 구현 |
|---|---|
| 로비에 옷장이 있다 | `ADRWardrobe` — 로비 **FreeRoam** 에 놓는 상호작용 장치 (D-9) |
| 옷을 골라 입는다 | 옷장 화면에서 HEAD · FACE · BODY · TAIL **4부위** 중 부위마다 1개씩 고른다. 로컬 세이브에 즉시 저장 (D-5) |
| UI 안에서 3D 로 갈아입혀 보인다 | 맵 밖 촬영 스튜디오(`ADRCosmeticPreviewStage`) → 렌더 타깃 → 화면 (D-11) |
| 그대로 입고 플레이한다 / 남에게도 보인다 | PlayerState 복제 + 맵 이동 보존 — ✅ M3 (8.0) |
| 스팀 도전과제로 해금된다 | 업적 달성 → 해금은 매번 파생 (D-2). 스팀 미러링은 **M6 (미구현)** |

```
[옷장 F] → OpenWardrobeScreen ─▶ (C++) CreateWidget(WBP_Wardrobe) → AddToViewport          ★BP 배선 없음 (D-27)★
                                   탭 클릭 → SetCurrentCategory → RefreshAll → RebuildSlotGrid(칸 생성) → OnRefreshSkins(BP 연출용)
                                   칸 클릭 → TryEquip → GI.EquipSkin → 세이브 → OnCosmeticsChanged
                                                                         ├─▶ 칸 목록 다시 그림
                                                                         └─▶ 3D 프리뷰 갱신 (서버 왕복 0)
[ESC]  → CloseWardrobeScreen ─▶ RemoveFromParent ─▶ 서버 보고 → PlayerState 복제 → 모든 머신의 캐릭터 · 대기실 디스플레이에 ApplyCosmeticSkins
```

### 0.2 진행 현황

| 마일스톤 | 내용 | 상태 |
|---|---|---|
| M0 | 업적 데이터 (정의 + 판정 호출) | ⬜ 정의 0개 · 호출 0곳 — ★어떤 업적이 어떤 옷을 푸는지 결정 필요★ (B3) |
| M1 | 세이브 슬롯 스팀 계정 키잉 | ⬜ (B4 — 단독 커밋) |
| M2 | 데이터 토대 — 타입 · 카탈로그 · 세이브 필드 · GI API · 보상 훅 | ✅ C++ · ✅ 카탈로그 (공용 실제 옷 8벌 — 4종 모두 입는다, 9.0) |
| M3 | 멀티 동기화 — PlayerState 복제 + 보고 RPC + 인게임 캐릭터 적용 | ✅ 2026-10-20 (8.0) |
| M4 | 옷장 액터 · 화면 · 3D 프리뷰 | ✅ C++ · ✅ 에디터 · ✅ 사람 손 확인 (7.2 — 2026-10-19 사용자 확인) |
| M5 | 대기실 디스플레이 캐릭터 + 해금 토스트 | ✅ 디스플레이 · 이름 헬퍼 (8.0) / ⬜ 토스트 UI (9장 C7) |
| M6 | 스팀 연동 | ⬜ (B7) |
| M7 | QA · 콘텐츠 | 🔶 (9장 · 10장) |
| — | 외형 적용 (부착물 · 리컬러 · CDO 리셋 · 비동기 로드 · 부활 재적용 · 1/3인칭 가시성) | ✅ |
| — | 사망 시 부착물 처리 | ✅ (8.0) |
| — | 에디터 검증기 (스팀 키 · 해금 조건) | ✅ (8.0 · D-29) |

### 0.3 남은 작업 순서

```
✅ A 에디터 (7장)   ✅ B1 M3 · B2 사망 · B5 M5 · B6 검증기 (8.0)   ✅ 공용 실제 옷 8벌 + 부위 소켓 (9.0)
남은 것:
   B3 M0 업적 데이터  ★기획 결정 먼저★ (8.1)
   → 지금까지(feat/closet)를 커밋
   → B4 M1 세이브 키잉  ★단독 커밋 · 스팀 켜고 확인★ (8.2)
   → B7 M6 스팀  (실 AppId 발급 후, 8.3)
C  콘텐츠 (9장) — 소켓 · 위치 눈 다듬기(디자이너) · FACE 옷 · 아이콘 · 자물쇠 · 토스트 UI. B 와 나란히 해도 된다
10 QA
```

- **B3 가 결정부터인 이유**: 코드는 다 있다(판정 → 보고 → `ApplyStageReward` → 해금). 남은 건 "어떤 옷을 무엇으로 잠글지"와 "업적을 무엇으로 판정할지"라는 기획이다.
  `StageClear.Stage1` 처럼 이미 있는 원장 키로 잠그는 건 카탈로그 데이터만 바꾸면 된다.
- **B4 를 따로 커밋하는 이유**: 모든 테스터의 진행도 파일 위치가 바뀐다. 다른 변경과 섞이면 회귀 원인을 못 가린다 (T-25 · T-26).
  지금 브랜치에 커밋 안 된 변경이 많으니 ★B4 전에 먼저 커밋★ 한다.

---

# 1부 — 이미 된 것 (참조용 요약)

## 1. 확정 사양

### 1.1 범위
- 4부위(HEAD · FACE · BODY · TAIL) = **부위별 부착물 메시**. 본체 머티리얼 리컬러는 보조 경로. 둘 다 한 스킨 정의에 담긴다.
- 로봇 4종(정원로봇 · 자판기 · 청소기 · 벨보이) 각각 따로 장착한다. 옷장은 **지금 고른 로봇**의 옷만 바꾼다.
- **범위 밖**: 캐릭터(로봇) 해금(부록 C) · 트레일 이펙트 · 본체 메시 통째 교체 · 유료/거래.

### 1.2 데이터 모델 (`Public/Game/DRCosmeticTypes.h`)

| 타입 | 핵심 |
|---|---|
| `EDRCosmeticCategory` | `Head, Face, Body, Tail, Count` — **값이 세이브 배열 인덱스로 쓰인다** (T-11) |
| `FDRSkinDefinition` | `SkinId`(변경 금지, 규약 `Skin.<Class>.<Category>.<Name>` · 공용은 `Skin.Shared.<Category>.<Name>`) · **`bCommonSkin` + `ExcludedClasses`**(공용 — D-31) · `OwnerClass`(전용일 때) · `Category` · `DisplayName/Description` · `RequiredAchievements` + `bRequireAll` + `HowToUnlock` · `ThirdPerson/FirstPerson`(부착 명세) · `BodyMaterialsTP/FP` · `WeaponMaterials` · `PreviewIcon` · `SortOrder` · **`ThirdPersonByClass`**(로봇별 덮어쓰기 — D-32). 소속 판정은 전부 **`IsUsableBy(로봇)`**, 3인칭 명세는 **`GetThirdPersonFor(로봇)`** 로만 |
| `FDRSkinAttachOverride` | `SocketName`(비우면 기본 소켓) · `RelativeTransform`(★통째로 바꾼다★) — D-32 |
| `FDRSkinAttachSpec` | `SkeletalMesh` 또는 `StaticMesh`(Skeletal 우선) · `SocketName` · `RelativeTransform` · `bUseLeaderPose`(같은 스켈레톤 의상) · `MaterialOverrides` |
| `FDRSkinMaterialOverride` | `MaterialSlot`(인덱스) + `Material` |
| `FDRClassCosmeticState` | `EquippedByCategory` — 길이 4 고정, 빈 칸 `NAME_None` (칩의 `SlotChips` 와 같은 관용구) |
| `FDRSkinViewModel` | 칸 1개를 그릴 정보 — `SkinId · Category · DisplayName · Description · PreviewIcon(없으면 null) · bUnlocked · bEquipped · UnlockHint · bIsNoneSlot` |

**세이브 (`UDRSaveGame`) 추가 필드 3개 — 버전은 4 그대로** (D-4 · T-1)

| 필드 | 뜻 |
|---|---|
| `EarnedAchievements : TArray<FName>` | 달성한 업적 Id (로컬 + 나중에 스팀 역매핑의 합집합). **해금 판정용** (D-3) |
| `Cosmetics : TMap<EPlayerCharacterClass, FDRClassCosmeticState>` | 로봇별 · 부위별 장착 |
| `PendingSteamAchievements : TArray<FName>` | 스팀에 아직 못 쓴 업적 큐. `SteamApiName` 이 있는 업적만 쌓인다. M6 전까지 쌓기만 한다 |

`FDRAchievementDef` 에 `SteamApiName`(비면 로컬 전용) · `UDRProgressionConfig` 에 `CosmeticCatalog` 참조가 추가돼 있다.

### 1.3 해금 판정
`IsSkinUnlocked(SkinId)` = `RequiredAchievements` 가 비면 기본 제공, 아니면 각 Id 가 **`EarnedAchievements ∪ ClaimedRewards`** 에 있는지(AND/OR).
→ 이미 있는 원장 키 **`StageClear.Stage1`**(스테이지1 최초 클리어 시 기록)을 조건으로 쓰면 **업적 정의 없이도 지금 바로** 해금 조건을 걸 수 있다.
`ApplyStageReward` 는 업적마다 `EarnedAchievements.AddUnique` → (스팀 키가 있으면) `PendingSteamAchievements` → 끝에서 새로 풀린 스킨마다 `OnSkinUnlocked` 를 발행한다.

---

## 2. 설계 결정 (D-n)

| # | 결정 | 이유 |
|---|---|---|
| D-1 | 식별자는 전부 `FName` (`SkinId` · `AchievementId`). GameplayTag 를 쓰지 않는다 | 세이브에 저장된다. 칩·보상이 이미 FName 이고, 태그 이름 변경 시 리다이렉터 누락 사고를 원천 차단 |
| D-2 | **해금 여부는 저장하지 않고 매번 파생**한다 | 파생값을 저장하면 카탈로그의 조건을 고쳤을 때 둘이 어긋난다. 규칙이 곧 진실 |
| D-3 | **원장 분리** — `EarnedAchievements`(해금) ≠ `ClaimedRewards`(재화 지급) | 합치면 T-2 사고. 스팀 역매핑은 `EarnedAchievements` 에만 넣는다 |
| D-4 | 세이브 버전은 **4 그대로**. 필드 추가만 했다 | UE 역직렬화가 없는 필드를 기본값으로 채운다. 버전을 올리면 T-1 |
| D-5 | 4부위 = 부착물, **부위당 1개** → 로봇당 최대 4개 동시 장착. 고정 길이 배열로 저장 | 시안의 탭 4개. 배열 구조만으로 "부위당 1개"가 보장된다 |
| D-6 | **부착물 컴포넌트는 복제하지 않는다.** 각 머신이 장착 Id 4개를 받아 로컬로 만든다. `ADRCharacter::ApplyCosmeticSkins(Ids)` 가 인게임 · 대기실 · 프리뷰 **공용 단일 경로** | 컴포넌트 복제는 스폰 순서 · 소유권 문제만 생긴다 |
| D-7 | 적용 순서: 본체 머티리얼을 **BP CDO 값으로 리셋** → Head→Face→Body→Tail 순 적용(같은 슬롯은 **나중이 이김**) → 메시는 비동기 로드 | 리셋이 없으면 옷을 벗어도 이전 머티리얼이 남는다. 순서를 고정해 결과를 결정적으로 |
| D-8 | 1인칭(FP) 부착은 **기본 비움** | 1인칭에서는 자기 모자·꼬리가 안 보인다. 청소기는 FP 스켈레톤이 별개라 소켓도 다르다 |
| D-9 | 옷장은 **로비 FreeRoam 배치물**이고 `ADRUpgradeStation` 구조를 그대로 따른다. 상호작용 · 화면 전부 로컬, `bReplicates=false` | 대기실(WaitingRoom)에는 플레이어 폰이 없어 걸어가는 장치를 둘 수 없다 |
| D-10 | 옷장 화면은 **지금 고른 로봇**의 옷만 보여준다 (`GetViewedClass`) | 로봇 변경은 대기실에서만 된다 — 업그레이드 화면과 같은 판단 |
| D-11 | 3D 프리뷰 = **맵 밖 촬영 스튜디오** + SceneCapture → RenderTarget → UMG. **로컬 전용**. `SCS_SceneColorHDR` 의 알파(= 역불투명도)로 **배경을 투명하게** → 뒤의 캐릭터 글로우가 비친다. 화면이 열린 동안만 캡처 | UMG 는 3D 를 직접 못 그린다. 상시 캡처는 로비 프레임을 깎는다. 톤매퍼를 안 거쳐 색이 인게임과 조금 다를 수 있다(조명으로 흡수) |
| D-12 | **프리뷰 폴백** — 스테이지 · 머티리얼 · RT 중 하나라도 없으면 프리뷰만 접히고(`OnPreviewUnavailable`) 칸 목록은 정상 동작 | 레벨 배치를 잊었다고 옷을 못 갈아입으면 안 된다 |
| D-13 | 칸 목록 **0번 = "기본"(장착 해제) 칸** — `GetSkinViewModels` 가 항상 앞에 붙인다 | 되돌리기 수단 |
| D-14 | **잠긴 칸도 버튼을 활성으로 둔다.** 클릭은 `TryEquip` 이 거절하고 `OnEquipRejected(SkinId, UnlockHint)` 를 발행 | 비활성 버튼은 툴팁 · 호버를 못 받아 "왜 잠겼는지"를 못 보여준다 |
| D-15 | 탭 선택 표시는 **화면이 WidgetTree 를 돌며 일괄로** 넣는다 (`UpdateTabSelection` → 탭 `SetSelected` → 필 · 라벨 색은 탭 C++ 이 그린다, `OnSelectionChanged` 는 연출용) | 탭을 추가하고 배선을 잊으면 그 탭만 조용히 고장 난다 |
| D-16 | 화면이 `FInputModeUIOnly` 라 **ESC · C 는 위젯 `NativeOnKeyDown` 이 받는다**(+ 컨트롤러 `HandleToggleSettings` 에도 ESC 분기). **마우스 닫기 버튼 없음** | 다른 창들과 같은 조작. 포커스를 잃은 경우도 대비 |
| D-17 | Clear All = **4부위 전부 해제, 확인 창 없음**, 위치는 **화면 우하단**(업그레이드 화면 키 힌트와 같은 자리) | 되돌리기가 클릭 한 번이라 확인 창은 과하다 |
| D-18 | 아트 미정 — 탭 호버 연출 없음 · 스크롤바 없음 · 썸네일/자물쇠 아트는 추후. **위젯만 두고 브러시는 비운다, 없으면 숨긴다** | 아트가 나오면 브러시만 채우면 된다 |
| D-19 | 그 로봇의 옷이 **하나도 정의되지 않았으면 옷장이 열리지 않는다** (`OnInteractBlocked`). 치트 `DROpenWardrobe` 는 우회 | "기본" 칸만 있는 화면은 버그처럼 보인다 |
| D-20 | 스팀 파트너 사이트 등록은 **M6 에서 한 번에**. `SteamApiName` 이 비면 로컬 전용 업적 | 출시 후 도전과제 삭제는 비가역. 게임 쪽 업적 정의는 지금부터 자유롭게 늘려도 된다 |
| D-21 | 세이브 슬롯은 **스팀 계정별** `DaeRunePlayerProgress_<SteamId>`. 폴백 `SteamID → LastAccountKey → 레거시` (M1) | 한 PC 에서 계정을 바꾸면 서로의 진행도를 본다 |
| D-22 | 캐릭터(로봇) 해금은 **이번 범위 제외** — 필요 작업은 부록 C 에 보존 | 요구는 "옷"이다. 대기실 순환 + 복원 경로 3곳을 건드리는 별개 작업 |
| D-23 | 시안의 포토샵/피그마 블렌드는 UMG 로 이렇게 바꾼다: **2차배경(Soft Light) → Additive**, **캐릭터 back(Lighter Color) → Additive + 청색 틴트**, **화면효과(Soft Light) → Modulate** (부록 B) | UMG 는 아래 픽셀을 읽을 수 없어 진짜 Soft Light 가 불가능 |
| D-24 | 화면 좌표는 **2026-10-19 실측값**이 확정판이다 (7.1 A6 표 · 부록 A). 같은 날 1080p PIE 로 대조해 확인 | 2026-08-25 값은 시안 배율 보정 누락으로 1px 씩 어긋났고, 제목 폰트 · 라벨 크기/색 · 화면효과 크기가 틀렸다 |
| D-25 | 화면을 닫아도 **프리뷰 캐릭터는 지우지 않고 캡처만 끈다**. 레벨이 끝날 때(EndPlay) 지운다 | FreeRoam 에서는 로봇이 안 바뀌므로 다음 열기 때 그대로 쓴다 |
| D-26 | 서버 동기화는 칩(`EquippedChips`)과 **똑같은 "클라 보고 → 서버 정화 → 복제"** 경로로 한다 (M3 설계 — B1) | 검증된 선례가 있는데 두 번째 방식을 만들지 않는다 |
| D-27 | **BP 그래프 배선을 두지 않는다** — 클릭 · 호버 · 선택 그림 · 칸 목록 생성 · 화면 생성/제거를 C++ 이 한다. WBP 는 **위젯 이름**(`BindWidgetOptional` — 6.1 표)만 맞으면 동작하고, BP 이벤트(`OnSelectionChanged` · `OnViewModelUpdated` · `OnRefreshSkins` · `OnPreviewReady` · `OnWardrobeScreenOpened/Closed` …)는 **연출 전용**. WBP 트리는 에디터 도우미 `UDRWardrobeEditorTools`(에디터 전용 · 멱등)가 D-24 사양대로 만든다 (6.2) | BP 함수를 C++ 이벤트로 착각하는 사고(T-13)가 탭 · 칸 2곳에서 이미 났다. 이름만 맞추면 되게 해서 실수 지점을 없앤다. 좌표를 손으로 옮기지 않으니 재실측 값을 그대로 다시 넣을 수 있다 |
| D-28 | 3D 프리뷰 구도는 **로봇별 카메라 위치** (`BP_DRCosmeticPreviewStage` 의 `CaptureLocationByClass`, 표에 없으면 컴포넌트 기본 (-420,0,0)). 기준: ① 15° 간격 24각도 어디로 돌려도 좌우가 안 잘린다(최소 여백 23px) ② 정면에서 발이 캐릭터 back 글로우의 바닥선(화면 y≈880)에 선다 ③ 확대는 1배까지(자판기는 거리 그대로) | 구도 하나(-420)로는 정원로봇 삽 · 벨보이 가방이 프리뷰 왼쪽 끝(x=938 — 그 왼쪽은 패널이라 넓힐 수 없다)에서 잘렸고, 로봇마다 발 높이가 최대 107px 달랐다. 값은 7.1 A9 |
| D-29 | 업적 · 스팀 키 · 옷 해금 조건 정합성은 ★에디터에서 저장할 때★ 검사한다 — `UDRProgressionConfig::ValidateAchievements` (스팀 키는 영문 · 숫자 · `_` 만 + 중복 금지, 옷의 `RequiredAchievements` 는 업적 Id 이거나 `StageClear.<StageId>`). 설정을 고칠 때와 ★카탈로그를 고칠 때★ 둘 다 돈다 (카탈로그는 프로젝트 GameInstance 기본값에서 설정을 찾는다) | 스팀 키 오타는 실 AppId 빌드에서야 드러나고, 조건 오타는 그 옷을 영원히 잠근다 — 둘 다 조용한 실패라 저장 시점에 잡는다 |
| D-30 | ~~(D-32 로 대체 — 정원로봇 값은 소켓으로 옮겼다)~~ 실제 옷(의상 GLB 에서 온 스태틱 메시)은 ★`Body` 뼈★ 에 붙이고 `RelativeTransform` = 배율 1/1.5 · 이동 (−2.49, −18, −16.43) 을 공통으로 쓴다 (부록 E). 소켓을 새로 만들지 않는다 | 의상 GLB 는 정원로봇을 인게임 크기(1.5배) · 엉덩이 높이 원점에 놓고 만든 좌표를 그대로 담고 있어 변환 하나로 전부 제자리에 온다. 머리 겸 몸통인 Body 뼈를 따라가므로 애니메이션에도 붙어 다닌다 |
| D-31 | 옷은 ★로봇 공용★ 일 수 있다 — `bCommonSkin` 이면 `OwnerClass` 를 무시하고 `ExcludedClasses` 를 뺀 모든 로봇이 입는다. ★해금도 공용★(해금은 SkinId 하나에 매이므로 한 번 풀면 4종 모두). 소속 판정(목록 · 장착 · 정화 · 세이브 · 서버 보고)은 전부 `FDRSkinDefinition::IsUsableBy` 한 곳을 지난다. 장착은 여전히 로봇별(세이브 `Cosmetics[로봇]`). 공용 옷에는 본체/무기 리컬러 · 리더 포즈를 쓰지 않는다(검증기 경고) | 아트가 옷을 4종 공용으로 만들었다. 같은 옷을 로봇마다 4번 등록하면 해금 · 문구 · 아이콘이 4벌로 갈라진다. 판정을 한 함수로 모은 건 한 곳만 빠져도 "입혀지는데 다음 실행에 사라지는" 늦은 버그가 나기 때문(공용칩 교훈) |
| D-32 | 공용 옷의 위치 = **로봇의 부위 소켓** × **옷마다의 상대 변환** (+ 필요한 로봇만 **덮어쓰기**). 소켓 5개 `Cosmetic_Head · Face · Chest · Back · Tail` 을 4종 TP 메시에 ★메시 전용 소켓★ 으로 단다. 소켓 배율 = (1/1.5) × 그 로봇 머리 폭 / 70 이라 옷 크기가 로봇 체형을 따라간다. 옷의 상대 변환은 "GLB 원점 → 그 옷이 붙는 점" 순수 이동(GLB 단위)이고, 같은 부위라도 옷마다 소켓을 고른다(날개는 Back, 나비넥타이는 Chest). 덮어쓰기 `ThirdPersonByClass[로봇]` 는 소켓(선택) + 변환 통째 — 붙는 점 기준으로 k 배 하려면 이동도 k 배 | "같은 몸 부위라도 등 · 앞" 이 옷마다 다르고, 체형은 로봇마다 다르다. 소켓이 체형을, 옷 변환이 옷의 생김새를 맡으면 대부분 덮어쓰기 없이 맞고, 디자이너는 스켈레톤 편집기에서 소켓만 옮기면 그 로봇의 모든 옷이 같이 움직인다 |

---

## 3. 함정 (T-n) — 다시 밟지 말 것

| # | 함정 | 대처 |
|---|---|---|
| T-1 | `CurrentSaveVersion` 을 올리면 `EnsureProgressInitialized` 가 **재화 · 슬롯 · 칩 · 코스메틱을 전부 초기화**한다 (미출시 전제의 의도적 설계) | 버전은 기존 필드의 **의미가 바뀔 때만** 올린다 |
| T-2 | 스팀 역매핑 Id 를 `ClaimedRewards` 에 넣으면 `ApplyStageReward` 의 중복 방지 `continue` 에 걸려 **그 업적 재화를 영원히 못 받는다** | `EarnedAchievements` 에만. `AddUnique` 는 그 `continue` **보다 앞**이어야 한다(구현됨) |
| T-3 | `Dissolve()` 가 본체 · 무기 **머티리얼 슬롯 0 을 덮어쓴다**. 부활 복원은 BP 가 Dissolve 파라미터만 되돌린다 | 부활에서 `RefreshSkinVisuals()` 재적용(구현됨). **사망 시 부착물은 Dissolve 되지 않고 그대로 남는다** → B2 |
| T-4 | `UpdateMeshVisibility()` · `SetWaitingRoomVisibility()` 가 부모 메시 가시성을 뒤집는다 | 두 함수에서 `SyncCosmeticVisibility()` (구현됨) |
| T-5 | 비동기 로드 도중 다른 옷을 고르면 늦게 온 콜백이 엉뚱한 옷을 입힌다 | 세대 번호(`CosmeticApplyGeneration`)로 늦은 콜백 폐기 (구현됨) |
| T-6 | 프리뷰 캐릭터에 `SetReplicates(false)` 가 없으면 **인원수만큼 캐릭터가 생긴다** | 구현됨 + 데디 서버 가드 |
| T-7 | `PRM_UseShowOnlyList` 는 **캡처만** 격리한다. 맵 안에 스튜디오를 두면 로비를 돌다 프리뷰 캐릭터를 만난다 | 스튜디오는 **맵 밖**(Z = -20000) (A9 — 됨) |
| T-8 | 프리뷰 이미지가 `Not Hit-Testable` 이면 **드래그 회전이 안 된다** | `Img_Preview` Visibility = **Visible** (A5 — 도우미가 넣는다) |
| T-9 | RT 포맷이 RGBA8 이면 HDR 이 잘리고, 머티리얼에서 `1 - A` 반전을 빼면 **배경이 검게 채워진다** | RT = **RGBA16f**, Opacity = **OneMinus(A)** (A5 — 됨) |
| T-10 | GameInstance 는 레벨 전환에도 살아 있다 — 위젯이 델리게이트를 안 풀면 죽은 위젯이 남는다 | 화면 `NativeDestruct` 에서 해제 (구현됨) |
| T-11 | `EPlayerCharacterClass` · `EDRCosmeticCategory` 는 값 자체가 저장된다 | **`Count` 직전에만** 추가. 중간 삽입 금지 |
| T-12 | `SkinId` 를 바꾸면 유저의 장착 상태가 사라진다 | rename 금지. 부득이하면 구 Id 를 남겨 매핑 |
| T-13 | BP 에서 C++ 이벤트(BlueprintImplementableEvent)를 **오버라이드하지 않고 비슷한 이름의 BP 함수를 새로 만들면 C++ 이 부르지 않는다** | 반드시 My Blueprint → Functions → **Override ▼** 에서 고른다. (2026-10-19: 탭 `SetSelected`, 칸 `Setup` 이 이 상태였다 → D-27 로 배선 자체를 없앴다) |
| T-14 | BP 의 색 칸(Make Slate Color · Linear Color)은 **선형값**이다. sRGB hex 를 255 로 나눠 넣으면 훨씬 밝게 나온다 (#8E9BC8 → 실제 #C5CDE5) | 색 선택기의 **Hex sRGB** 칸에 hex 를 입력한다 |
| T-15 | UMG 는 **감마 공간에서 섞는다** — UI 머티리얼 출력에 pow(1/2.2) 를 건 뒤 더한다. Additive 세기 0.2 가 실제로는 약 0.48 처럼 보인다 | `Opacity = Power(알파 × 세기, 2.2)` 로 보정 (A2 · 부록 B) |
| T-16 | Image 의 브러시가 비어 있으면 **흰 사각형**이 그려진다 | 브러시 없는 Image 는 Collapsed 로 두거나 지운다 (칸의 썸네일 · 자물쇠는 C++ 이 알아서 숨긴다) |
| T-17 | Dim 을 "텍스처(알파 0.608 내장) + Color and Opacity 알파 0.608" 로 하면 **실효 0.37** — 배경이 시안보다 밝다 | 둘 중 한 번만 (도우미는 컬러 브러시 0.608 한 번) |
| T-18 | 조립 시안 PNG 는 **P3 레티나 스크린샷**(가장자리 잘림, 배율 1.2014/1.1073)이다. 그대로 1920×1080 에 늘려 겹치면 2~4px · 색이 어긋난다 | 정렬본 `조립_1080p정렬_sRGB.png` 를 쓴다 (6.4) |
| T-19 | Bruno Ace SC 는 **소문자를 작은 대문자로** 그린다 — `Head` 는 시안의 `HEAD` 와 다르다 | 라벨은 대문자로 입력 (도우미가 `HEAD` · `FACE` … 를 넣는다) |
| T-20 | 칸에 데이터를 넣을 때 BP 함수에 직접 넘기면 C++ 쪽 `ViewModel` 이 비어 **어느 칸을 눌러도 "기본"이 장착된다** | C++ `RebuildSlotGrid` 가 `SetSkinViewModel` 로 넣는다 (D-27). BP 에서 칸을 따로 만들지 않는다 |
| T-21 | 탭은 C++ 변수 **`Category`** 로 동작한다. 같은 뜻의 BP 변수를 따로 만들면 무시되어 4개 탭이 전부 Head 가 된다 | 탭 인스턴스마다 C++ `Category` (도우미가 넣는다) |
| T-22 | UI 텍스처 압축이 기본값(BC)이면 부드러운 그라디언트에 **띠(밴딩)** 가 생긴다 (특히 알파가 낮은 캐릭터 back) | `UserInterface2D` (A1 — 됨) |
| T-23 | (M3) 리슨 서버 호스트에서는 서버가 값을 바꿔도 **RepNotify 가 자동 호출되지 않는다** | Set 함수에서 직접 갱신 (B1) |
| T-24 | (M5) 대기실 디스플레이 캐릭터는 **비점유라 PlayerState 가 없고**, 서버의 0.5초 스폰 타이머와 클라 보고가 경합한다 | 캐릭터 액터에 복제 필드 + 서버 재푸시 (B5) |
| T-25 | (M1) `GameInstance::Init()` 시점에 SteamID 가 해석되는지 **확인되지 않았다** | 첫 커밋에서 로그로 확인. 안 되면 지연 확정(비용 큼) (B4) |
| T-26 | (M1) 레거시 세이브를 지우면 이관이 잘못됐을 때 되돌릴 길이 없다 | 레거시 파일은 **지우지 않는다** (B4) |
| T-27 | (M6) Steam Cloud 에 레거시 `DaeRunePlayerProgress.sav` 까지 올리면 같은 PC 계정끼리 진행도가 섞인다 | **`DaeRunePlayerProgress_*.sav` 만** 등록 (B7) |
| T-28 | (M6) AppId 480(Spacewar)에서는 실제 해금이 안 되거나 Spacewar 도전과제로 뜬다 | 개발 중에는 백엔드 없음으로 취급하고 로컬 경로로 검증 |
| T-29 | 옷장 액터가 `EndPlay/EndOverlap` 에서 `OnInteractPressed` 구독을 안 풀면 죽은 액터가 델리게이트에 남는다 | 구현됨 (`ClearLocalController`) |
| T-30 | 렌더 타깃 비율과 프리뷰 위젯 비율이 다르면 **캐릭터가 옆으로 늘어난다** (예전 계획의 512×1024 RT ↔ 781×1080 위젯 = 1.45배) | RT 를 **782×1080** 으로 (A5 — 됨) |
| T-31 | 카탈로그가 비어 있으면 옷장 앞에서 F 를 눌러도 **아무 반응이 없다** (D-19 — `OnInteractBlocked` 만 발생) | 지금은 테스트 6벌로 4종 모두 열린다. 테스트 옷을 지울 때 실제 옷이 없는 로봇은 다시 이 상태가 된다 (9장) |
| T-32 | 위젯 BP 의 CDO 기본값(예: `SlotWidgetClass` · `PreviewMaterial`)을 **컴파일한 뒤에** 바꾸면 새 인스턴스에 안 들어간다 — 생성 클래스의 "생성 후 복사 목록"이 컴파일 때 만들어진다. 그 사이에 만든 템플릿 인스턴스(`WBP_Wardrobe` 안의 `Preview_Character`)는 None 을 **따로 저장**해서 CDO 를 고쳐도 따라가지 않는다 | CDO 를 고친 뒤 **다시 컴파일** + 템플릿 인스턴스에도 직접 넣는다 (도우미에 반영됨). 증상: PIE 에서 칸 0개 · 로그 "프리뷰 머티리얼(X)" |
| T-33 | Python 에서 `bool F(FString& Out)` · `bool F(TArray<FString>& Out)` 꼴 BlueprintCallable 은 **Out 만 돌려준다** (bool 은 사라지고, ★false 면 None★ 이 온다) | 성공 여부는 돌려받은 로그 문자열(`컴파일 성공`)이나 None 여부로 본다 |
| T-34 | Python `set_editor_property` 는 **EditDefaultsOnly** 속성을 에셋 인스턴스 · 구조체 안에서 못 바꾼다 ("cannot be edited on instances") — 카탈로그 `Skins` 배열 · `DA_ProgressionConfig.CosmeticCatalog` 가 여기 걸린다 | `unreal.DRWardrobeEditorTools.import_property_text(대상, '속성이름', 'T3D 텍스트')` — 복사/붙여넣기 형식, PostEditChange 까지 돈다 |
| T-35 | Slate 자간은 **정수 px 로 잘린다** (`LetterSpacing × Size / 1000` → int) — 41.25pt 제목에 8 을 넣어도 0 | 1px 미만 자간은 못 넣는다. 제목이 시안보다 끝 글자 3~4px 왼쪽인 것은 그대로 둔다 (글자 폭은 같고, 글자 진행 폭이 정수로 잘려 생기는 차이) |
| T-36 | 레벨 뷰포트 PIE 는 **비율이 에디터 배치를 따른다**(이 PC 2.7:1) — 1080p 시안과 맞대 볼 수 없다. 새 창 PIE 는 요청보다 **높이가 3px 크다** | `unreal.DRWardrobeEditorTools.start_play_in_new_window(1920, 1077)` → 뷰포트 1920×1080. `Shot showui` 는 창 제목줄까지 찍힌다 — 뷰포트는 (5, 53) 부터 (6.4) |
| T-37 | Python 에서 부른 UFunction 은 전부 `FEditorScriptExecutionGuard` 안에서 돈다 → 그 안에서 나가는 ★Server RPC 가 네트워크로 안 가고 그 자리(클라이언트)에서 실행된다★ (`GAllowActorScriptExecutionInEditor` — 슬레이트 틱 콜백도 같다). 그래서 클라이언트 PC 에 `server_toggle_ready()` 를 불러도 서버는 모른다 | `unreal.DRWardrobeEditorTools.call_on_next_tick(대상, '함수 인자')` — C++ 코어 티커에서 불러 정상 경로를 탄다. 서버 쪽 객체의 BlueprintCallable(예: `GameMode.power_on`)은 그냥 불러도 된다 (6.5) |
| T-38 | `ADRPlayerController::OnLevelEntered` 는 ★대기실이면 일찍 return★ 하고, 리슨 호스트는 `OnRep_PlayerState` 도 안 불린다 → 함수 끝에 둔 보고는 호스트에게 영영 안 간다. (칩 보고가 지금 이 상태다 — 로비 FreeRoam 에서 호스트 칩이 비어 있다가 스테이지 진입 때 채워진다) | 옷 보고는 로비 분기 ★앞★ 에 둔다. 호스트가 대기실에서 로봇을 바꿀 때는 `SetSelectedPlayerClass` 끝의 `ReportCosmeticIfLocal` 이 바로 보고한다 |
| T-39 | 옷장 화면의 `TryEquip` 은 ★지금 보고 있는 탭의 부위만★ 받는다 (다른 부위 Id 는 false) | 스크립트로 세트를 입힐 때는 `set_current_category` 로 탭을 바꾼 뒤 `try_equip` 하거나 GI `equip_skin(로봇, 부위, Id)` 를 직접 부른다 |
| T-40 | ★PIE 중에는 `EditorAssetLibrary` 가 막힌다★ — `load_asset` 은 None, `does_asset_exist` 는 False, 저장도 안 된다 (오류 없이) | PIE 중 에셋을 만질 때는 `unreal.load_asset('/Game/…/X.X')`(객체 경로) 로 잡고, 저장은 PIE 를 끝낸 뒤. 또 옷장 프리뷰는 소켓을 고쳐도 다시 그리지 않을 수 있다 — 위치 확인은 대기실 디스플레이(실제 화면)로 |
| T-41 | 탭 라벨 `Text_Label` 이 ★Visible★ 이면 탭 가운데(글자 위)를 눌러도 안 바뀐다 — 오버레이에서 라벨이 `Button_Tab` 위에 있어 클릭을 라벨이 받고, 형제인 버튼으로는 안 간다. 글자 밖 가장자리만 눌려 "잘 안 먹히는" 것처럼 보인다 (TextBlock 기본값이 Visible) | 버튼 위에 얹는 그림 · 글자는 전부 `HitTestInvisible`. 탭은 `ApplySelectionVisuals` 가 매번 넣고 도우미도 넣는다(칸의 `Txt_Name` 은 처음부터 그랬다) |

---

## 4. 코드 지도 (전부 존재 — 빌드 통과)

| 파일 | 역할 · 핵심 |
|---|---|
| `Public/Game/DRCosmeticTypes.h` | 1.2 의 타입 전부. `DRGetCosmeticCategoryCount()` · `DRIsValidCosmeticCategory()` · **`IsUsableBy` · `GetThirdPersonFor`**(D-31 · D-32) |
| `Game/DRCosmeticCatalog.h/.cpp` | 스킨 DataAsset. `FindSkin` · `GetSkinsForCategory`(SortOrder→SkinId 정렬) · `SanitizeSkin` · **`SanitizeLoadout`(길이 4 맞춤 + 정화 — M3 서버가 쓴다)** · `ValidateCatalog`(편집 시 자동 실행 — 이어서 설정의 `ValidateAchievements` 도 돈다, D-29). **공용 옷 규칙**: 리컬러 · 리더 포즈 경고, 못 입는 로봇의 덮어쓰기 경고, ★입을 수 있는 로봇 메시마다 소켓이 있는지★(D-32) |
| `Game/DRSaveGame.h` | 필드 3개 (1.2) |
| `Game/DRProgressionConfig.h/.cpp` | `FDRAchievementDef::SteamApiName` · `UDRProgressionConfig::CosmeticCatalog` · **`ValidateAchievements`**(D-29 — 저장 시 자동) |
| `Game/DRGameInstance.h/.cpp` | `GetCosmeticCatalog` · `IsSkinUnlocked` · `GetEquippedSkin(s)` · `EquipSkin` · `ClearAllSkins` · `GetSkinViewModels` · `HasAnySkinAvailable` · **`GetSkinDisplayName`**(토스트용) · `DebugGrantAchievement` · `DebugUnlockAllSkins` · 델리게이트 `OnCosmeticsChanged`/`OnSkinUnlocked`. `EnsureProgressInitialized` 의 lazy 초기화 + 카탈로그 정화, `ApplyStageReward` 훅 |
| `Character/DRCharacter.h/.cpp` | `ApplyCosmeticSkins(Ids)`(단일 진입점) · `RefreshSkinVisuals()`(재적용) · **`ResolveCosmeticClass()`**(이 몸이 어느 로봇인가 — BP 클래스를 클래스 정보에서 역조회, 못 찾으면 `PlayerCharacterClass`. 못 입는 옷은 건너뛰고 `GetThirdPersonFor` 로 붙인다, D-32) · `SyncCosmeticVisibility()` · 내부 `BuildCosmeticVisuals`/`SpawnCosmeticAttachment`/`ApplyCosmeticMaterials`/`ClearCosmeticAttachments`. 부활(`MulticastHandleRevive`)에서 재적용. **M3**: `ApplyCosmeticsFromPlayerState`(`PossessedBy` · `OnRep_PlayerState` 끝 — 폰이 늦게 와도 치유). **M5**: `DisplaySkinIds`(복제) · `SetDisplaySkinIds`(서버, 대기실 디스플레이 전용). **사망**: `bCosmeticHiddenForDeath` — 사망 멀티캐스트에서 켜고 부활에서 끄며 `OnRep_Dead` 로도 양방향으로 맞춘다 |
| `Actor/DRWardrobe.h/.cpp` | 옷장 장치. `WardrobeMesh`(루트) · `InteractionBox`(Pawn 만 Overlap) · `InteractionWidget`(Screen, 기본 숨김). BP 이벤트 `OnLocalPlayerEnteredRange(bHasAnySkin)` · `OnLocalPlayerLeftRange` · `OnInteractBlocked` |
| `Actor/DRCosmeticPreviewStage.h/.cpp` | 촬영 스튜디오. `SceneRoot` · `PreviewSpawnPoint` · `CaptureComponent`(생성자에서 `SCS_SceneColorHDR` · `PRM_UseShowOnlyList` · 캡처 꺼 둠). 속성 `RenderTarget` · `BackdropActors` · `BaseYaw` · **`CaptureLocationByClass`(로봇별 카메라 — D-28, `SetPreviewClass` 가 적용)**. `Find` · `SetPreviewClass` · `SetPreviewSkins` · `AddPreviewYaw` · `SetCaptureActive` |
| `UI/Widget/DRWardrobeScreenWidget.h/.cpp` | 화면 베이스. `GetViewedClass` · `Get/SetCurrentCategory` · `RefreshAll` · `TryEquip` · `ClearAll` · `RequestClose` · `GetCurrentSkinViewModels` · `FindOwnerScreen`. **칸 목록을 C++ 이 만든다**(`RebuildSlotGrid` — `SlotWidgetClass` · `SlotColumns` 3) · `Btn_ClearAll` 연결. BindWidgetOptional `Preview_Character` · `UniformGridPanel_Slots` · `Btn_ClearAll`. BP 이벤트(연출용) `OnRefreshSkins` · `OnCategoryChanged` · `OnEquipRejected` · `OnSkinEquipped` · `OnSkinNewlyUnlocked`. 설정 `ClearAllKey`(C) · `DefaultCategory`(Head) |
| `UI/Widget/DRWardrobeTabWidget.h/.cpp` | 탭. `Category` · `LabelText`(인스턴스마다) · 라벨 색 `SelectedLabelColor`(DDF7FF) / `UnselectedLabelColor`(6676B8). BindWidgetOptional `Image_Pill` · `Button_Tab` · `Text_Label` — 클릭 → 카테고리 전환, 필 표시 · 라벨 글자/색을 C++ 이 그린다. BP 이벤트 `OnSelectionChanged`(연출용) |
| `UI/Widget/DRWardrobeSlotWidget.h/.cpp` | 칸. `SetSkinViewModel`(★그리기까지★) · `HandleClicked`(→ bool) · `HasThumbnail` · `GetThumbnailTint` · `GetTooltipText` · 설정 `StateNormal/Hovered/SelectedTexture` · `LockedTint` · `LockedNameOpacity`(0.4). BindWidgetOptional `Image_State` · `Image_Thumbnail` · `Txt_Name` · `Image_Lock` · `Button_Slot` — 상태 이미지(선택 > 호버 > 기본) · 썸네일 또는 이름 · 자물쇠(브러시가 있을 때만) · 툴팁. BP 이벤트 `OnViewModelUpdated`(연출용) |
| `UI/Widget/DRCosmeticPreviewWidget.h/.cpp` | 프리뷰 위젯. `InitializePreview`(`Img_Preview` 에 MID 를 직접 물린다) · `UpdatePreviewSkins` · 드래그 회전(마우스 캡처) · `MarkPreviewUnavailable`(접기 — D-12). BindWidgetOptional `Img_Preview`. 설정 `PreviewMaterial` · `PreviewTextureParameter`("Tex") · `YawPerPixel`(0.5). BP 이벤트 `OnPreviewReady(Material)` · `OnPreviewUnavailable` |
| `Player/DRPlayerState.h/.cpp` | **M3**: `EquippedSkinIds`(복제, 인덱스 = 부위) · `SetEquippedSkinIds`(서버 — 폰에 바로 입히고 대기실 디스플레이에 재푸시) · `OnRep_EquippedSkinIds` · `RefreshCosmeticVisuals` · `ReportCosmeticIfLocal`. `CopyProperties` 로 스테이지에 가져간다. `SetSelectedPlayerClass` 에서 리셋 + (호스트) 즉시 보고, `OnRep_SelectedPlayerClass` 에서 보고 |
| `Game/DRLobbyGameMode.h/.cpp` | **M5**: `SpawnDisplayCharacter` 가 그 플레이어의 옷을 싣고, `RefreshDisplayCharacterSkin`(PlayerState 가 부른다)이 보고가 늦게 와도 다시 싣는다 (T-24) |
| `Player/DRPlayerController.h/.cpp` | `OpenWardrobeScreen`(로비 가드 · 다른 창 닫기 · UIOnly · 커서 · **위젯 생성 + AddToViewport**) / `CloseWardrobeScreen`(RemoveFromParent → **옷 보고**) · BP 이벤트 `OnWardrobeScreenOpened/Closed`(연출용) · `WardrobeScreenWidgetClass` · `WardrobeScreenWidget` · ESC 우선 분기 · 레벨 이동 시 리셋. **M3**: `ReportCosmeticLoadout` → `ServerReportCosmeticLoadout`(선택 클래스 가드 + `SanitizeLoadout`, 해금은 안 본다) · 보고 지점 = `OnLevelEntered`(★로비 분기 앞★ T-38) · `OnRep_PlayerState` · `CloseWardrobeScreen`. 치트 `DRUnlockSkins` / `DROpenWardrobe` / `DRDumpCosmetic` / **`DRDie` · `DRRevive`**(개발 빌드 전용, 서버 RPC) |
| `Editor/DRWardrobeEditorTools.h/.cpp` (신규) | **에디터 전용 도우미** (게임 빌드에서는 빈 함수). `ImportPropertyText`(T-34) · `BuildWardrobeTabWidget` · `BuildWardrobeSlotWidget` · `BuildCosmeticPreviewWidget` · `BuildWardrobeScreenWidget` (D-24 좌표 · 브러시 · 폰트 · 탭 4개 · 프리뷰 · Clear All · `SlotWidgetClass` — 멱등, 저장은 부른 쪽) · `StartPlayInNewWindow(폭, 높이, 인원)`(T-36 — 2명 이상이면 리슨 서버) · `CallOnNextTick`(T-37) · **`SetMeshSocket(메시, 소켓, 뼈, 위치, 회전, 배율)`**(D-32 — 메시 전용 소켓 생성/수정, 소켓 맵 재구성). `DaeRune.Build.cs` 에 에디터 타깃 한정 `UnrealEd` · `UMGEditor` · `EngineSettings` |

---

## 5. 에셋 현황 (2026-10-20 실측)

| 에셋 | 상태 | 내용 |
|---|---|---|
| 텍스처 11장 `DaeRuneAssets/UI/Closet/**` · `UI/Settimgs/clear_all` | ✅ | UserInterface2D · Texture Group UI (A1). 원본 PNG: `C:\Users\OWNER\Documents\카카오톡 받은 파일\옷장\` |
| `Blueprints/UI/Closet/Material/` — `M_UI_Additive` · `M_UI_ScanlineModulate` · `M_CosmeticPreview` + MI 3개 | ✅ | A2 · A5 |
| `Blueprints/UI/Closet/RT_CosmeticPreview` | ✅ | 782×1080 · RGBA16f · Clamp |
| `WBP_Wardrobe` · `WBP_WardrobeTab` · `WBP_WardrobeSlot` · `WBP_CosmeticPreview` | ✅ | 도우미가 만든 트리, **그래프 없음** (D-27) |
| `BP_DRPlayerController` | ✅ | `Wardrobe Screen Widget Class = WBP_Wardrobe` (화면 생성은 C++) |
| `Blueprints/Progression/DA_CosmeticCatalog` | ✅ | **공용 실제 옷 8벌**(9.0). 테스트 고깔은 전부 지웠다 |
| `DaeRuneAssets/Characters/Closet/{Head,Body,Tail}` | ✅ | 의상 스태틱 메시 8 + 머티리얼 7 — 의상 GLB 4개(`카카오톡 받은 파일\gardener_*.glb`)를 Interchange 로 임포트한 것. 부모 머티리얼은 엔진 `InterchangeAssets` 플러그인(콘텐츠 전용 · 기본 켜짐 — 패키징에 들어간다). `Face` 폴더는 비어 있다 |
| `DA_ProgressionConfig` | ✅ | `CosmeticCatalog` 연결. 업적 0개 (B3) |
| `Config/Localization/Game_Gather.ini` | ✅ | `Content/Blueprints/Progression/*` 수집 경로 추가 |
| `Blueprints/Actor/Test/BP_DRCosmeticPreviewStage` · `BP_DRWardrobe` | ✅ | A9 |
| `Maps/LobbyMap` | ✅ | 옷장 (-430, 736, 6) · 스튜디오 (0, 700, -20000) |
| `Cosmetic_Head/Face/Chest/Back/Tail` 소켓 | 🔶 | 4종 TP 메시 전부(메시 전용 소켓, 9.0 표). 값은 ★1차 실측★ — 디자이너가 눈으로 다듬는다 (C2) |
| 아이콘 · 자물쇠 아트 · 옷장 메시 · 프롬프트 위젯 | ⬜ | 9장 |

---

# 2부 — 남은 작업 (이 순서대로)

## 6. 시작 전에 알아 둘 것 (처음 보는 사람용)

### 6.1 위젯 이름 규칙 ★D-27★

C++ 이 `BindWidgetOptional` 로 **이름을 보고** 연결한다. 이름을 한 글자라도 바꾸면 컴파일은 되는데 **그 기능만 조용히 멈춘다**(Optional 이라서).

| WBP | C++ 이 찾는 이름 | 하는 일 |
|---|---|---|
| `WBP_Wardrobe` | `Preview_Character` [WBP_CosmeticPreview] · `UniformGridPanel_Slots` · `Btn_ClearAll` | 3D 프리뷰 · 칸 목록 · 전부 벗기 |
| `WBP_WardrobeTab` | `Image_Pill` · `Button_Tab` · `Text_Label` | 선택 필 · 클릭 · 라벨 |
| `WBP_WardrobeSlot` | `Image_State` · `Image_Thumbnail` · `Txt_Name` · `Image_Lock` · `Button_Slot` | 상태 이미지 · 썸네일 · 이름 · 자물쇠 · 클릭/호버/툴팁 |
| `WBP_CosmeticPreview` | `Img_Preview` | 렌더 타깃 표시 + 드래그 |

`WBP_Wardrobe` 의 나머지 이름(`CanvasPanel_Root` · `CanvasPanel_Content` · `Image_Dim` · `Image_BGGlow` · `Image_CharBack` · `Image_PanelFrame` · `Text_Title` · `Overlay_TabBar` · `Image_TabBarBG` · `HorizontalBox_Tabs` · `Tab_Head/Face/Body/Tail` · `ScrollBox_Slots` · `Image_ScreenFX`)은 C++ 은 안 쓰지만 **도우미가 이름으로 찾는다** — 바꾸면 도우미 로그에 `위젯 없음: 이름` 이 뜬다.

**연출을 더하고 싶으면** My Blueprint → Functions → **`Override ▼`** 에서 BP 이벤트를 고른다 (같은 이름 함수를 새로 만들면 안 된다 — T-13). 그림 · 클릭은 이미 C++ 이 하므로 여기서 또 하지 않는다.

### 6.2 에디터 도우미 다시 돌리기

좌표 · 브러시 · 폰트 같은 수치는 **도우미 코드(`Private/Editor/DRWardrobeEditorTools.cpp`)가 정본**이다.
★WBP 를 손으로 고친 값은 도우미를 다시 돌리면 덮어써진다★ — 수치를 바꾸려면 코드를 고치고 빌드한 뒤 다시 돌린다.

에디터 하단 **Output Log** → 입력줄 왼쪽을 `Cmd` → **`Python`** 으로 바꾸고 한 줄씩, 또는 Tools → Execute Python Script 로 파일을 실행:
```python
import unreal
T = unreal.DRWardrobeEditorTools
for build in [T.build_wardrobe_tab_widget, T.build_wardrobe_slot_widget,     # ★이 순서★ — 화면이 탭 · 칸 · 프리뷰 클래스를 쓴다
              T.build_cosmetic_preview_widget, T.build_wardrobe_screen_widget]:
    print(build())                                   # 로그 문자열이 돌아온다 (T-33). "컴파일 성공" 확인
for name in ['WBP_WardrobeTab', 'WBP_WardrobeSlot', 'WBP_CosmeticPreview', 'WBP_Wardrobe']:
    unreal.EditorAssetLibrary.save_asset('/Game/Blueprints/UI/Closet/' + name, False)
```
- PIE 중에는 돌리지 않는다 (컴파일 · 저장이 미뤄진다).
- 몇 번을 다시 돌려도 결과가 같다(멱등). 탭 4개는 매번 지우고 다시 만든다.

### 6.3 텍스처 이름 (실제 에셋 이름)
| 짧게 부르는 이름 | 에셋 |
|---|---|
| 패널 | `UI/Closet/옷장` (820×839) |
| 탭바 | `UI/Closet/카테고리바` (496×44) |
| 탭 필 | `UI/Closet/카테고리_선택박스` (124×44) |
| 1차 배경 | `UI/Closet/Background/1차검은배경` (1920×1080, 검정 알파 0.608 균일) — 화면은 이 텍스처 대신 컬러 브러시 (T-17) |
| 2차 배경 | `UI/Closet/Background/2차배경_블렌드모드_소프트라이트_` (1601×901) |
| 캐릭터 back | `UI/Closet/Background/캐릭터_back_블렌드_모드_Lighter_Color_불투명도_90으로_조정_` (868×1076 — 위에서 비추는 빛기둥, 바닥선은 그림의 y≈870) |
| 화면효과 | `UI/Closet/Background/화면효과_블렌드모드_소프트라이트_` (1753×993) |
| 칸 기본/호버/선택 | `UI/Closet/Slot/속성_1_기본` · `속성_1_호버` · `속성_1_선택` (216×252, 선택에 체크 표시 각인) |
| Clear All | `UI/Settimgs/clear_all` (112×117, `C` 키캡 그림) |

### 6.4 시안과 맞대 보기
1. 정렬본: `C:\Users\OWNER\Documents\카카오톡 받은 파일\옷장\레이어순서_가이드\` 의 **`조립_1080p정렬_sRGB.png`**(로비 배경 포함) · **`조립(배경없음)_1080p정렬_sRGB.png`** — 배율 · 잘린 가장자리 · P3 색을 보정해 1920×1080 디자인 좌표에 맞춘 사본 (원본 `조립.png` 는 T-18).
2. 1080p PIE: Python `unreal.DRWardrobeEditorTools.start_play_in_new_window(1920, 1077)` → FreeRoam → 콘솔 `DROpenWardrobe` → `Shot showui` → `Saved/Screenshots/WindowsEditor/` 의 그림에서 **(5, 53) 부터 1920×1080** 을 잘라 정렬본과 비교 (T-36).
   FreeRoam 으로 바로 가려면 Python `unreal.GameplayStatics.get_game_mode(월드).power_on(플레이어컨트롤러)` (혼자일 때).
- 색을 넣을 일이 있으면 색 선택기의 **`Hex sRGB`** 칸에 hex 를 넣는다 (T-14). Visibility 이름(UE 5.5): `Visible` / `Collapsed` / `Hidden` / `Not Hit-Testable (Self & All Children)` / `Not Hit-Testable (Self Only)`.

### 6.5 멀티플레이 확인 방법 (Python, 한 프로세스 2인 PIE)
1. `unreal.DRWardrobeEditorTools.start_play_in_new_window(1280, 717, 2)` — 리슨 서버 창 + 클라이언트 창. ★먼저 LobbyMap 을 연다★ (다른 맵에서 켜면 맵 쪽 문제로 크래시가 날 수 있다 — 7.3).
2. 월드: `unreal.EditorLevelLibrary.get_pie_worlds(False)` — GameMode 가 있는 쪽이 서버. 각 월드의 로컬 PC = `GameplayStatics.get_player_controller(월드, 0)`.
3. ★클라이언트 쪽 행동은 `call_on_next_tick` 으로★ (T-37): `call_on_next_tick(클라PC, 'RequestChangeClass 1')` · `'ServerToggleReady'` · `'CloseWardrobeScreen'`(먼저 `open_wardrobe_screen()`) · `'DRDie'` · `'DRRevive'`.
   서버 쪽은 그냥 불러도 된다: `get_game_mode(서버).power_on(호스트PC)` · 스테이지 이동은 `call_on_next_tick(서버GM, 'TravelToStage Stage1 <호스트PC 경로>')`.
4. 옷은 각 월드의 GI `equip_skin(로봇, 부위, Id)` 로 입히고 옷장을 열었다 닫아 보고시킨다. 확인은 PlayerState `EquippedSkinIds` 와 캐릭터의 `StaticMeshComponent`(부모 = Mesh) 목록.
5. ★끝나면 두 월드 모두 `clear_all_skins`★ — 두 인스턴스가 같은 세이브 파일을 쓴다.

---

## 7. A — 에디터 작업 ✅ (2026-10-19 완료)

### 7.1 한 일

| 단계 | 결과 |
|---|---|
| A1 텍스처 | 11장 — Compression **UserInterface2D (RGBA)** · Texture Group **UI** · NoMipmaps · sRGB · Never Stream |
| A2 머티리얼 | `M_UI_Additive`(UI · Additive — `Final Color = Tex.RGB × Tint`, `Opacity = Power(Tex.A × Strength, 2.2)`) · `M_UI_ScanlineModulate`(UI · Modulate — 부록 B) → MI `MI_UI_Wardrobe_BGGlow`(Strength 0.2) · `MI_UI_Wardrobe_CharBack`(0.55, Tint sRGB `00A3FF`) · `MI_UI_Wardrobe_ScreenFX`(1.0) |
| A3 탭 | 도우미 — 필 `카테고리_선택박스` 124×44(평소 Collapsed) · 투명 버튼 · 라벨 Bruno Ace SC 14.4 대문자 `6676B8`, 위 여백 2(시안은 필 중앙보다 1px 아래). BP 잔재(`SetSelected` · `CosmeticCategory` · `Label`) 삭제 |
| A4 칸 | 도우미 — `SizeBox_Root`(216×252) → `Overlay_Root` → `Image_State` · `Image_Thumbnail`(여백 16) · `Txt_Name`(Pretendard 13.5 · `DDF7FF` · 줄바꿈) · `Image_Lock` · `Button_Slot`(투명, 맨 위). 상태 텍스처 3장 = Class Defaults. BP 함수 `Setup` 삭제 |
| A5 프리뷰 재료 | `RT_CosmeticPreview` 782×1080 RGBA16f Clamp · `M_CosmeticPreview`(UI · Translucent, `Opacity = 1 − Tex.A`, Tex 기본값 = RT) · `WBP_CosmeticPreview`(루트 `Img_Preview` Visible, `PreviewMaterial` = `M_CosmeticPreview`) |
| A6 화면 | 도우미 — 아래 표 그대로 |
| A7 컨트롤러 | BP 배선 대신 C++ `OpenWardrobeScreen` 이 위젯을 만든다 (D-27). `BP_DRPlayerController` 는 클래스 지정만 |
| A8 카탈로그 | `Game_Gather.ini` 경로 한 줄 · `DA_CosmeticCatalog` 테스트 6벌(아래 표) · `DA_ProgressionConfig.CosmeticCatalog` 연결 |
| A9 배치 | `BP_DRCosmeticPreviewStage` — 조명 `KeyLight` · `FillLight`(Rect, 30 · 12 cd) · `RimLight`(Spot, 40 cd), 그림자 없음 · `RenderTarget` = RT · `BaseYaw` 180 · FOV 35 · **`CaptureLocationByClass`**(D-28): 정원로봇 (-632, 0, 50.8) · 벨보이 (-553, 0, 58.9) · 청소기 (-568, 0, 53.5) · 자판기 (-449, 0, 34.1). `BP_DRWardrobe` — 메시 없음(옆 수납장 `Cube_058` 앞의 보이지 않는 상호작용 상자, 회전 -90). LobbyMap 에 둘 다 배치 |
| A10 확인 | 7.1-3 (자동) + 7.2 (사람) |

#### 7.1-1 `WBP_Wardrobe` 최종 계층 + 좌표 (★D-24 — 도우미가 넣는 값★)

Canvas Panel 은 **아래 줄일수록 위에 그려진다**.

```
CanvasPanel_Root
 ├─ Image_Dim            Anchor 전체 · Offset 0 · 컬러 브러시 (0,0,0, A 0.608) — 텍스처를 쓰지 않는다 (T-17)
 ├─ Image_BGGlow         Anchor 중앙 · Alignment (0.5,0.5) · Size 1601×901 · MI_UI_Wardrobe_BGGlow
 ├─ Image_CharBack       왼쪽 위 · (895, 2) · 868×1076 · MI_UI_Wardrobe_CharBack
 ├─ Image_PanelFrame     왼쪽 위 · (150, 146) · 820×839 · 옷장
 ├─ CanvasPanel_Content  Anchor 전체 · Offset 0   (z-order 묶음)
 │   ├─ Preview_Character  [WBP_CosmeticPreview] · 왼쪽 위 · (938, 0) · 781×1080
 │   ├─ Text_Title         왼쪽 위 · (202, 54) · Size To Content · "WARDROBE" · HakgyoansimUndongjangL_Font 41.25(=55px) · DDF7FF · 자간 0 (T-35)
 │   ├─ Overlay_TabBar     왼쪽 위 · (201, 139) · 496×44
 │   │   ├─ Image_TabBarBG     Fill · 카테고리바
 │   │   └─ HorizontalBox_Tabs Fill · Tab_Head / Tab_Face / Tab_Body / Tab_Tail [WBP_WardrobeTab] (각 124, Auto)
 │   │                         Category = Head/Face/Body/Tail · LabelText = HEAD/FACE/BODY/TAIL
 │   ├─ ScrollBox_Slots    왼쪽 위 · ★(218, 221) · 684×689★ · Vertical · 스크롤바 Collapsed · Overscroll ✖ · 휠 = When Scrolling Possible
 │   │   └─ UniformGridPanel_Slots   Slot Padding 6 (사방) → 칸 왼쪽 위가 (224, 227), 3 × (216 + 12) = 684
 │   └─ Btn_ClearAll       Anchor 오른쪽 아래 · Alignment (1,1) · (-100, -60) · 112×117 · clear_all (눌림 Tint 0.8) · 툴팁 "전부 벗기 (C)"
 └─ Image_ScreenFX       Anchor 중앙 · Alignment (0.5,0.5) · 1613×914 (원본의 92%) · MI_UI_Wardrobe_ScreenFX · Not Hit-Testable
```

| 항목 | 근거 (부록 A) |
|---|---|
| 패널 (150,146) | 패널 테두리선 4변을 두 시안에서 서브픽셀로 측정, 서로 0.5px 이내. **1080p PIE 대조 0.05px** |
| 탭바 (201,139) | 선택박스의 1px 밝은 테두리 4변 + 탭바 끝. **1080p PIE 대조 일치** |
| 제목 | 글자 8개의 좌우 끝을 폰트 메트릭으로 맞춤 — 학교안심 운동장 L 55px. PIE 에서 글자 폭은 같고 끝 글자가 3~4px 왼쪽 (T-35) |
| 라벨 | Bruno Ace SC 19.2px, 셀(124) 중앙 정렬, 대문자. PIE 대조 0.5px 안 |
| 색 | 시안을 P3 → sRGB 변환 후 측정. 제목 · 선택 라벨 `DDF7FF`, 비선택 라벨 `6676B8` |
| 2차 배경 · 캐릭터 back | 중앙 1:1 / (895,2) |
| 화면효과 92% | 스캔라인 줄 간격 상관 0.95~0.98 (두 시안 · 여러 구역 일치). 원본 크기면 줄 간격이 9% 넓어진다 |
| 프리뷰 (938,0) 781×1080 | 시안의 피그마 선택 상자 |
| 그리드 | **시안에 칸이 없어 검증 못 함.** 가로 224 는 패널 안쪽 중앙과 정확히 일치. 칸 배치 시안 `image_1.png` 가 원본 폴더에 없다 — 받으면 같은 방법으로 잰다 |
| Clear All (-100,-60) | `WBP_UpgradeScreen` 의 키 힌트 바와 같은 앵커 · 오프셋 (D-17) |

#### 7.1-2 테스트 카탈로그 6벌 (`DA_CosmeticCatalog` — ★2026-10-20 전부 지웠다★, 기록용)

설명은 전부 "테스트용 — 실제 옷 메시가 나오면 지울 항목", 문구 네임스페이스 `DRCosmetic`.

| SkinId | 로봇 · 부위 | 메시(엔진 기본) · 소켓 · 크기 | 해금 |
|---|---|---|---|
| `Skin.Gardener.Head.TestCone` | 정원로봇 · HEAD "테스트 고깔" | Cone · `HatSocket` · 0.3 | 기본 제공 |
| `Skin.Gardener.Head.TestCubeLocked` | 정원로봇 · HEAD "테스트 큐브(잠김)" | Cube · `HatSocket` · 0.25 · SortOrder 1 | `StageClear.Stage1` |
| `Skin.Gardener.Tail.TestSphere` | 정원로봇 · TAIL "테스트 구슬" | Sphere · `TestTail` · 0.25 | 기본 제공 |
| `Skin.VendingMachine.Head.TestCone` · `Skin.RobotVacuum.Head.TestCone` · `Skin.BellBoy.Head.TestCone` | 각 로봇 · HEAD | Cone · `HatSocket` · 0.3 | 기본 제공 |

#### 7.1-3 자동으로 확인한 것 (1920×1080 PIE 창 · FreeRoam · 치트 `DROpenWardrobe`)
- 칸 3개(기본 · 테스트 고깔 · 테스트 큐브(잠김)), 3열, (224, 227) 부터. 잠긴 칸은 이름이 흐리다.
- `TryEquip(고깔)` → 성공, 프리뷰에 바로 씌워지고 칸에 체크 이미지 / 잠긴 큐브 → 거절.
- 세이브 유지: 장착 → PIE 종료 → **에디터 재시작** → 다시 열기 → 그대로.
- `ClearAll()` → 4부위 해제, 프리뷰에서 벗겨지고 "기본" 칸에 체크. (테스트로 입힌 옷은 이걸로 벗겨 두었다 — 세이브는 원래대로)
- 1280×720 창: 비례 축소 (패널 테두리가 1080p 값 × 0.666 자리에서 1.2px 안).
- 로봇 4종 × 24각도 촬영: 잘림 없음, 발이 글로우 바닥선에 선다 (D-28).
- 시안 대조: 부록 A.

### 7.2 ★사람이 손으로 확인할 것★ (실제 마우스 · 키 입력 경로 — 10분)

`LobbyMap` PIE → 대기실에서 로봇 선택 → FreeRoam:

| # | 동작 | 기대 결과 | 안 되면 |
|---|---|---|---|
| 1 | 옷장(수납장 앞, (-430, 736))에 다가가 **F** | 화면이 뜬다 | 반응 없음 → 그 로봇 옷이 카탈로그에 없다(T-31). 프롬프트가 안 뜨는 건 정상 (`InteractionWidget` 비어 있음 — 9장) |
| 2 | 탭 4개 클릭 | 필이 옮겨 가고 글자색이 바뀐다. FACE · BODY 는 "기본"만, TAIL 은 "테스트 구슬"(정원로봇) | 위젯 이름 (6.1) |
| 3 | 칸 위에 마우스 | 호버 이미지 | `Button_Slot` 이 맨 위인지 |
| 4 | 칸 클릭 | 그 자리에서 갈아입혀지고 칸에 체크 | — |
| 5 | 잠긴 큐브 위에 마우스 | 해금 조건 툴팁 | — |
| 6 | 캐릭터 드래그 | 돈다 — 어느 각도에서도 안 잘린다 | `Img_Preview` Visible (T-8) |
| 7 | **C** 키 / 오른쪽 아래 C 버튼 | 4부위 전부 해제 | — |
| 8 | **ESC** | 닫히고 조작이 돌아온다 | — |
| 9 | 콘솔 `DRUnlockSkins` 후 다시 열기 | 큐브가 열린다 | `Required Achievements` |

→ 끝나면 **Clear All 로 테스트 옷을 벗겨 둔다** (로컬 세이브에 남는다).

### 7.3 눈으로 다듬을 것 (선택) · 열린 이슈

- **세기**: MI Strength (BGGlow 0.2 · CharBack 0.55 · ScreenFX 1.0) · CharBack Tint `00A3FF` · 스튜디오 조명(30 / 12 / 40 cd) — 수식 근사의 시작값이다 (부록 B). 캐릭터 back 은 내보낸 PNG 로는 정확히 못 맞춘다 → 디자이너에게 **"보이는 최종 모습 그대로" 내보낸 PNG** 를 받으면 다시 잰다.
- **구도**: 큰 옷(높은 모자 · 긴 꼬리)이 오면 `CaptureLocationByClass` 를 다시 맞춘다 — X 를 더 음수로 하면 작아지고, Z 를 올리면 캐릭터가 화면 아래로 내려간다. 기준은 D-28.
- **열린 이슈 — 로비 HUD 가 비친다**: 옷장 화면 아래로 `Room Code :` 와 상호작용 안내("스테이지1로 가려면 F를 누르세요.")가 흐리게 보인다. **업그레이드 화면도 같다.** 숨기기로 하면 `OpenWardrobeScreen` / `CloseWardrobeScreen` 에서 HUD 오버레이를 Collapsed ↔ 복원 (업그레이드 화면과 같이 정한다).
- **알려진 차이 — 제목 끝 3~4px** (T-35). 고치려면 글자를 따로 놓는 수밖에 없어 두기로 했다.
- **열린 이슈 — 프리뷰에서 검정 옷이 회색빛**: 신사 모자 · 나비넥타이(`costume_black`, 거칠기 0.5)가 스튜디오 조명 + 톤매핑 없는 렌더 타깃(D-11)에서 회색으로 보인다. 원본 GLB 와 값은 같다. 손보려면 MI 의 `RoughnessFactor` 를 올리거나 스튜디오 조명을 낮춘다.
- **옷장과 무관한 발견 (기록만)**: ① `StartupMap` 에서 PIE 를 켜면 `ADREnemy` → `UDRAbilitySystemLibrary::InitializeDefaultAttributes` 가 null `CharacterClassInfo` 를 읽어 크래시한다 (null 검사 없음). ② `BP_VendingMachine` CDO 의 `PlayerCharacterClass` 가 범위 밖 값(5)이다 — 빙의 캐릭터는 PlayerState 값으로 덮여 문제없지만 프리뷰 · 대기실 캐릭터에는 남는다. ③ 호스트 칩 보고 누락 (T-38).

---

## 8. B — C++ 작업

> 빌드: 에디터를 닫고 PowerShell 에서 `Build.bat DaeRuneEditor Win64 Development "-Project=…\DaeRune.uproject"` (헤더 · UPROPERTY 변경은 Live Coding 불가).
> 새 주석은 `Plan(옷장) D-n / T-n / M-n` 로 이 문서를 가리킨다.

### 8.0 완료 — B1 M3 · B2 사망 · B5 M5 · B6 검증기 (2026-10-20)

| 단계 | 한 것 (코드 위치는 4장) | 확인 — 2인 PIE · 리슨 서버 (6.5) |
|---|---|---|
| B1 M3 | 칩과 같은 "보고 → 정화 → 복제" (D-26). PlayerState `EquippedSkinIds` 복제 · `ServerReportCosmeticLoadout`(선택 클래스 가드 + `SanitizeLoadout`) · 보고 지점 3곳(레벨 진입 — ★로비 분기 앞★ T-38 · `OnRep_PlayerState` · 옷장 닫기) · 폰 치유(`PossessedBy` · `OnRep_PlayerState`) · `CopyProperties` 보존 · 로봇 변경 시 리셋 + 호스트 즉시 보고 | ① 서로 다른 옷(호스트 정원로봇 고깔 + 구슬 / 클라 자판기 고깔)이 ★양쪽 화면★ 에서 ② 옷장을 닫는 즉시 상대 화면에 반영 ③ 사망 → 부활 뒤 유지 ④ Stage1 seamless travel 뒤 새 폰이 그대로 입음 ⑤ 대기실에서 로봇을 바꾸면 이전 옷이 빠짐 — 전부 ✅. ⑥ 늦은 참가는 최초 복제라는 같은 경로라 따로 안 돌렸다 |
| B2 | `bCosmeticHiddenForDeath` — 사망 멀티캐스트에서 켜고 부활에서 끈다. `OnRep_Dead` 로도 ★양방향★ 맞춘다(늦은 참가 · 부활보다 늦게 온 옛 값). Sync · Spawn 두 곳이 플래그를 본다 | 치트 `DRDie` → 양쪽 화면에서 몸은 보이고 옷만 숨음 · `DRRevive` → 다시 보임 ✅ |
| B5 M5 | 디스플레이 캐릭터 `DisplaySkinIds`(복제) · `SpawnDisplayCharacter` 가 싣고 `RefreshDisplayCharacterSkin` 이 재푸시(T-24) · GI `GetSkinDisplayName` | 대기실 2명이 양쪽 화면에서 세이브의 옷(토끼 세트)을 입음 · 클라/호스트가 로봇을 바꾸면 옷이 바뀌고 다시 입으면 재푸시 ✅. ★토스트 UI 는 없다★ (9장 C7) |
| B6 | `ValidateAchievements` (D-29) | 일부러 넣은 스팀 키 공백 · 중복 · 조건 오타가 저장 즉시 Output Log 에 뜸 ✅ |

> 이 단계에서 추가한 확인용 도구: 치트 `DRDie` / `DRRevive`(개발 빌드 전용 서버 RPC) · `StartPlayInNewWindow` 의 인원 인자 · `CallOnNextTick`(T-37).

### 8.1 B3. M0 — 업적 데이터 (해금이 실제로 흐르게) ★기획 결정이 먼저★

> **결정할 것** — ① 정원로봇 옷 8벌 중 무엇을 잠글지 (지금은 전부 기본 제공) ② 무엇으로 풀지:
> `StageClear.Stage1` 같은 클리어 조건이면 **카탈로그의 `Required Achievements` · `How To Unlock` 만 고치면 끝**(코드 · 업적 정의 불필요),
> 업적(무피해 클리어 등)이면 아래 2 · 3 이 필요하다. 저장하면 D-29 검증이 오타를 잡는다.

**지금**: 파이프라인(판정 → 보고 → `ApplyStageReward` → `EarnedAchievements`)은 완성인데 **업적 정의 0개 · `GrantStageAchievement` 호출 0곳**이다.

1. **바로 쓸 수 있는 조건**: `StageClear.Stage1` (스테이지1 최초 클리어 원장 키 — 1.3). 클리어형 해금은 업적 없이 이걸로 충분하다.
   단, **재설치 후에도 살아남아야 하는 옷은 스팀 키가 있는 업적에 건다** — `StageClear.*` 는 로컬 원장이라 스팀에서 복구되지 않는다.
2. **업적 정의**: `DA_ProgressionConfig` → `Achievements` 배열 `+`:

| 필드 | 예 | 비고 |
|---|---|---|
| `Achievement Id` | `Stage1_NoDamage` | ★변경 금지★ (세이브 키) |
| `Kind` | Achievement / ClearCondition | |
| `Stage Id` | `Stage1` | |
| `Display Name` · `Description` | 한글 | Gather 경로는 이미 들어가 있다 |
| `Currency` | 400 | 최초 1회 지급 |
| `Steam Api Name` | **비워 둔다** | M6 에서 한 번에 채운다 (D-20) |

3. **판정 지점에서 호출**: `ADRStageGameMode::GrantStageAchievement(AchievementId, PC)` — BlueprintCallable.
   `PC` 를 주면 그 플레이어만, `nullptr` 이면 전원. 업적은 **클리어했을 때만** 결과 보고에 실린다.
   - 예: 무피해 클리어 → 스테이지 종료 판정에서 피해 0 인 플레이어마다 `GrantStageAchievement("Stage1_NoDamage", PC)`.
   - 누적 N킬 같은 "진행형"은 만들지 않는다 — 세이브에 누적 통계가 없다. 서버가 판정하는 업적으로 대체한다.
- ✅ 검증: 클리어 후 로그 `[Progression] 스테이지 보상 반영` + 해당 옷이 옷장에서 열려 있다. 재시작 후에도 유지.

### 8.2 B4. M1 — 세이브 슬롯 스팀 계정 키잉 (★단독 커밋★)

> ★시작 전에 지금까지(feat/closet)를 먼저 커밋한다★ — 이 단계만 한 커밋에 담아야 회귀 원인을 가릴 수 있다.
> 0번 확인은 ★스팀을 켠 채 단독 실행(Standalone · 패키지)★ 으로 해야 한다 — 에디터 PIE 는 스팀 OSS 를 쓰지 않는다.

**지금**: 슬롯명 `DaeRunePlayerProgress` · UserIndex 0 고정 → 한 PC 에서 스팀 계정을 바꾸면 서로의 재화 · 업그레이드 · 옷을 본다 (D-21).
**Plan2 와 공통 사항이다 — 이 절이 두 문서의 기준이다.**

0. **먼저 확인 (T-25)**: `UDRGameInstance::Init()` 에 로그 한 줄을 넣고 스팀을 켠 채 실행해 SteamID 가 찍히는지 본다.
   `IOnlineSubsystem::Get()->GetIdentityInterface()->GetUniquePlayerId(0)` — Init 시점엔 LocalPlayer 가 없으므로 LocalUserNum 0 경로만 쓴다.
   **찍히지 않으면 여기서 멈추고** 지연 확정(첫 LocalPlayer 생성/로그인 완료 후 확정 — 그 전에 진행도를 읽는 코드가 전부 기다려야 해서 비용이 크다)으로 설계를 바꾼다.
1. `UDRSaveGame`: `SaveSlotName` → **`BaseSlotName`** 로 개명(값은 `"DaeRunePlayerProgress"` 그대로) + `static FString MakeSlotName(const FString& AccountKey)` — 키가 비면 BaseSlotName, 아니면 `BaseSlotName + "_" + 키`.
2. `UDRGameUserSettings`: `UPROPERTY(Config) FString LastAccountKey;` (`PreferredCulture` 옆) — 스팀이 꺼진 채 실행해도 같은 세이브를 잇는 값.
3. `UDRGameInstance`:
   - `FString ActiveSlotName;` — Init 에서 1회 확정, **이후 절대 바꾸지 않는다** (실행 중 바뀌면 저장이 두 파일로 갈라진다)
   - `static FString ResolveSteamAccountKey();` — 스팀 미실행/미로그인이면 빈 문자열. `:` `/` `\` 는 `_` 로 치환.
   - `void ResolveActiveSaveSlot();` — 키 = 스팀 → 없으면 `LastAccountKey` → 없으면 빈 값(레거시). 키가 바뀌었으면 `LastAccountKey` 저장.
     **계정 슬롯이 없고 레거시 슬롯이 있으면 1회 복사(이관). 레거시 파일은 지우지 않는다 (T-26).**
   - `FString GetActiveSaveSlotName() const` (BlueprintPure — QA 표시용)
   - 세이브 호출부 3곳(Load/Save/Exists)을 `UDRSaveGame::SaveSlotName` → `ActiveSlotName` 으로. 비어 있으면 먼저 `ResolveActiveSaveSlot()` 하는 가드.
4. `Init()` 순서: 컬처 적용 → **`ResolveActiveSaveSlot()`** → `LoadProgress()` → (나머지 그대로).
5. **Plan2.md 의 같은 항목을 이 절을 가리키도록 고친다.**
- ✅ 검증 4종: ① 스팀 켠 채 실행 → `Saved/SaveGames/DaeRunePlayerProgress_<SteamId>.sav` 생성, **기존 재화 · 슬롯 · 칩 · 옷이 전부 살아 있다** ② 스팀 끈 채 실행 → 같은 슬롯을 잇는다 ③ 다른 스팀 계정 → 새 슬롯, 0 부터 ④ 0번 로그.
- 알려진 한계: 한 PC · 여러 계정 · 스팀을 끈 채 실행하면 `LastAccountKey` 를 공유해 섞인다 — 스팀 없이는 계정을 구분할 수단이 없다.

### 8.3 B7. M6 — 스팀 연동 (마지막)

**전제**: 실 AppId 발급. 그전까지는 AppId 480 이라 실제 해금 검증이 불가능하다 (T-28) — **나머지 단계는 스팀 없이 완주할 수 있게 설계돼 있다.**

1. **`UDRAchievementSubsystem`** (`UGameInstanceSubsystem`, 신규 `Public/Game/DRAchievementSubsystem.h/.cpp`)

| 함수 | 하는 일 |
|---|---|
| `IsBackendAvailable()` | OSS · Achievements 인터페이스 · `GetUniquePlayerId(0)` 가 전부 유효한가. 아니면 모든 호출이 no-op |
| `ReportAchievements(Ids)` | `SteamApiName` 이 있는 것만 쓰기. 백엔드 없으면 `PendingSteamAchievements` 에 그대로 둔다 |
| `FlushPending()` | 큐를 쓰고 비운다 (스팀 쓰기는 멱등 — 재시도 안전) |
| `QueryFromBackend()` | 스팀 달성 상태 읽기 → API 이름 → AchievementId 역매핑 → **`EarnedAchievements` 에만 합집합** (T-2) → 저장 → `OnCosmeticsChanged` |

   실제 UE API (`OnlineAchievementsInterface.h`):
   ```cpp
   IOnlineAchievementsPtr Ach = IOnlineSubsystem::Get()->GetAchievementsInterface();
   FUniqueNetIdPtr UserId = IOnlineSubsystem::Get()->GetIdentityInterface()->GetUniquePlayerId(0);
   // 쓰기
   FOnlineAchievementsWritePtr Write = MakeShareable(new FOnlineAchievementsWrite());
   Write->SetFloatStat(*ApiName, 100.0f);                       // 100 = 달성
   FOnlineAchievementsWriteRef WriteRef = Write.ToSharedRef();
   Ach->WriteAchievements(*UserId, WriteRef, FOnAchievementsWrittenDelegate::CreateUObject(...));
   // 읽기
   Ach->QueryAchievements(*UserId, FOnQueryAchievementsCompleteDelegate::CreateUObject(...));
   FOnlineAchievement Out; Ach->GetCachedAchievement(*UserId, ApiName, Out);   // Out.Progress >= 100 → 달성
   ```
2. 배선: `ApplyStageReward` 끝(지금 큐에 쌓는 자리)에서 `ReportAchievements` · `Init()` 의 `LoadProgress` 뒤에 `QueryFromBackend` + `FlushPending`.
3. 파트너 사이트 · 설정:
   - `Config/DefaultEngine.ini` 의 `SteamDevAppId=480` → 실 AppId, 빌드 폴더에 `steam_appid.txt` (지금 없음).
   - 도전과제 등록 — API Name 은 `SteamApiName` 과 **글자 단위로 같게**, 언어별 이름 · 설명(게임 쪽 FText 를 원본으로 복사), 아이콘 2장(256×256 잠김/해제).
   - Steam Cloud = Auto-Cloud, **`DaeRunePlayerProgress_*.sav` 만** 등록 (T-27).
- ✅ 검증: 실 AppId 빌드에서 토스트가 뜨고, **세이브를 지운 뒤 스팀 상태만으로 옷 해금이 복원**된다. 오프라인에서 달성 → 온라인 복귀 시 스팀에 반영(backfill).

---

## 9. C — 콘텐츠 (아트가 오는 대로)

> 코드 수정 없이 에셋만으로 된다. 새 옷은 **C1(카탈로그)**, 위치가 어긋나면 **C2(소켓 → 덮어쓰기 순)**. 아이콘 · 자물쇠 · 토스트는 아트가 오는 대로.

### 9.0 완료 — 공용 실제 옷 8벌 + 부위 소켓 (2026-10-20)

의상 GLB 4개(테마별, 각 2부위)를 Interchange 로 임포트한 `DaeRuneAssets/Characters/Closet/` 의 메시를 쓴다. 전부 **`bCommonSkin`**(4종 모두 입는다 · 해금 공용 — D-31), 위치는 **부위 소켓 + 옷마다 상대 이동**(D-32 · 부록 E).

| SkinId | 부위 | 표시 이름(★임시★) | 메시 | 소켓 | 상대 이동(GLB 단위) | 덮어쓰기 | 세트 · SortOrder |
|---|---|---|---|---|---|---|---|
| `Skin.Shared.Head.RabbitEars` | HEAD | 토끼 귀 | `Head/Cube_027` | `Cosmetic_Head` | (−3.73, −27, −100.23) | | 토끼 · 10 |
| `Skin.Shared.Tail.RabbitTail` | TAIL | 토끼 꼬리 | `Tail/Icosphere` | `Cosmetic_Tail` | (−3.73, 25.5, 3.27) | | 토끼 · 10 |
| `Skin.Shared.Head.TopHat` | HEAD | 신사 모자 | `Head/Cylinder` | `Cosmetic_Head` | (−3.73, −27, −100.23) | 벨보이: 1.15배(이동도 1.15배) — 원래 모자에 파묻혀서 | 신사 · 20 |
| `Skin.Shared.Body.BowTie` | BODY | 나비넥타이 | `Body/Cube_001` | `Cosmetic_Chest` | (−3.73, −75.9, −0.78) | | 신사 · 20 |
| `Skin.Shared.Head.DevilHorns` | HEAD | 악마 뿔 | `Head/Cylinder_006` | `Cosmetic_Head` | (−3.73, −27, −100.23) | | 악마 · 30 |
| `Skin.Shared.Tail.DevilTail` | TAIL | 악마 꼬리 | `Tail/Cube_008` | `Cosmetic_Tail` | (−3.73, 25.5, 3.27) | | 악마 · 30 |
| `Skin.Shared.Head.Halo` | HEAD | 천사 고리 | `Head/Torus` | `Cosmetic_Head` | (−3.73, −27, −100.23) | 자판기 z −53.73(손잡이 위) · 벨보이 z −62.43(원래 모자 위) | 천사 · 40 |
| `Skin.Shared.Body.AngelWings` | BODY | 천사 날개 | `Body/Plane` | `Cosmetic_Back` | (−3.73, 25.26, −36.1) | | 천사 · 40 |

**부위 소켓** (스켈레톤 편집기에 보이는 값 = 뼈 기준. 회전 0, 배율 = (1/1.5) × 머리 폭 / 70)

| 로봇 (메시) | 배율 | Head | Face | Chest | Back | Tail |
|---|---|---|---|---|---|---|
| 정원로봇 `Gardener_v0_1_1` | 0.667 | Body (0, 0, 50.39) | Body (0, 35, 15.39) | Body (0, 32.6, −15.91) | Body (0, −34.84, 7.64) | Body (0, −35, −18.61) |
| 자판기 `VendingMachine_v0_1_1` | 0.495 | Body (0, 0, 66.05) | Body (0, 24, 5.05) | Body (0, 24.5, −6.9) | Body (0, −27, 33.05) | Body (0, −24, −3.95) |
| 청소기 `VecuumCleaner_v0_2_0` | 0.752 | Head (0, 0, 28.88) | Head (0, 30, 15.88) | Body (0, 35, 28.99) | Body (0, −34, 18.99) | Body (0, −31, 6.99) |
| 벨보이 `Bellboy_v0_1_1` | 0.952 | Body_002 (0, 1.26, 75.97) | Body_002 (0, 59.26, 44.97) | Body_002 (0, 77, −21) | Body_002 (0, −64.74, 27.97) | Body_001 (0, −77.74, 1.39) |

- 전부 **기본 제공**(해금 조건 없음) — 조건은 8.1(B3). 이름 · 설명(`DRCosmetic` 네임스페이스, 키 `<SkinId>.Name/.Desc`)은 임시 문구. ★SkinId 는 바꾸지 않는다★ (T-12) — `Skin.Gardener.*` 에서 `Skin.Shared.*` 로 바꾼 건 출시 전이라서다(옛 Id 는 부팅 정화가 벗긴다).
- 정원로봇 소켓 값은 D-30 의 공통 변환을 그대로 옮긴 것이다(옛 배치와 실루엣 차이 ~1%). 다른 3종은 ★1차 실측★ — 4방향 정사영으로 머리 꼭대기 · 화면 앞면 · 등 · 엉덩이를 잡았다.
  벨보이 가슴은 몸에 그려진 나비넥타이 위로(처음엔 화면 위에 떴다), 자판기 가슴은 받침에 묻히지 않게 올렸다. 벨보이 나비넥타이는 원래 나비넥타이보다 작아 가장자리가 보인다 — 원하면 덮어쓰기로 키운다(C2).
- 테스트 고깔 · 큐브 · 구슬은 전부 지웠다. ★FACE 부위는 옷이 없다★ (Face 폴더가 비어 있다 — 소켓은 있다).
- 확인: 4종 × 4세트 정사영 촬영, 1080p PIE 프리뷰, PIE 대기실(벨보이 신사 세트 — 모자 덮어쓰기 배율 1.15 × 소켓 0.952 = 1.10 이 실제로 붙음).
- 벨보이는 캡슐 배율이 1.0 이다(다른 로봇은 1.5) — 그래서 소켓 배율이 크다. 메시 피벗이 GLB 엉덩이 원점이라 ★스켈레톤 편집기의 Preview Asset 으로 옷을 걸어 보면 상대 이동이 빠져 엉뚱한 곳에 뜬다★ — 위치 확인은 PIE 대기실로.

### C1. 새 옷 넣기 — `DA_CosmeticCatalog` → `Skins` 배열 `+`

| 필드 | 값 | 비고 |
|---|---|---|
| `Skin Id` | 공용 `Skin.Shared.<부위>.<이름>` · 전용 `Skin.<로봇>.<부위>.<이름>` | ★변경 금지 (T-12)★ |
| `b Common Skin` · `Excluded Classes` / `Owner Class` | 공용이면 ✔ (+ 안 맞는 로봇 빼기) / 전용이면 로봇 | D-31 |
| `Category` · `Display Name` · `Description` | 부위 · 한글 가능 | 한/영 수집 대상 |
| `Required Achievements` · `b Require All` · `How To Unlock` | 비우면 기본 제공 | 8.1 · D-29 |
| `Third Person` → `Static Mesh` 또는 `Skeletal Mesh` | 붙일 메시 | 전용 옷이 같은 스켈레톤 의상이면 Skeletal + `b Use Leader Pose` ✔ |
| `Third Person` → `Socket Name` · `Relative Transform` | 공용: `Cosmetic_*` 중 하나 + GLB 원점 → 붙는 점 이동 | ★없는 소켓이면 발밑에 붙는다★ — 저장 시 로봇별로 검사해 경고 |
| `Third Person By Class` | 특정 로봇만 다르게 | C2 |
| `First Person` | **비워 둔다** (D-8) | |
| `Preview Icon` · `Sort Order` | C3 · 작을수록 앞 | |

- 같은 작가의 GLB(정원로봇 1.5배 · 엉덩이 원점 기준)면 상대 이동 = −(그 옷이 붙는 점의 GLB 좌표) — 부록 E 의 방법으로 붙는 점(모자 밑면 중심 등)만 재면 된다.
- Python 으로 넣을 때는 `Skins` 가 EditDefaultsOnly 라 `import_property_text` (T-34) — 기존 항목은 `export_text()` 로 뽑아 이어 붙인다. 빈 TMap 은 `ThirdPersonByClass=()` 로 나온다.

### C2. 위치 다듬기 — ① 소켓 ② 덮어쓰기
1. **한 로봇의 그 부위 옷이 다 같이 어긋나면 → 소켓**: 스켈레톤 편집기(그 로봇 TP 메시)에서 `Cosmetic_*` 를 옮긴다. 그 로봇의 그 부위 옷 전부가 따라온다. Python 이면 `DRWardrobeEditorTools.set_mesh_socket` (소켓 · 뼈 이름은 Python 에서 못 바꾼다).
2. **한 옷만 한 로봇에서 어긋나면 → 덮어쓰기**: 그 옷의 `Third Person By Class` 에 로봇 추가 → `Relative Transform` 을 ★통째로★ 넣는다(기본값을 복사해 고친다). 붙는 점 기준으로 k 배 = 이동도 k 배 + 배율 k.
3. 확인은 PIE 대기실에서 (T-40). 옷장 프리뷰는 로봇별 카메라(D-28)라 큰 옷이면 `CaptureLocationByClass` 도 본다.

기존 소켓(옷이 안 쓴다): 4종 모두 `HatSocket` · `MountSocket` · `TestPartHand`, 정원로봇 `TestTail` · `TestLeftHand` · `TestRightHand`, 벨보이 `BagSocket` · `CapSocket`, 청소기 `RideSocket`, 자판기 `TestLeftHand`.

### C3. 아이콘
스킨의 `Preview Icon` 에 텍스처를 넣으면 칸의 `Image_Thumbnail`(여백 16)이 켜지고 이름 글자가 사라진다. 잠긴 칸은 `LockedTint`(0.35 회색)로 어둡게.
아이콘 텍스처도 A1 설정(UserInterface2D · UI)으로.

### C4. 자물쇠 아트
`WBP_WardrobeSlot` 의 `Image_Lock` 에 브러시가 있으면 C++ 이 **잠긴 칸에만** 켠다 (D-18 — 비어 있으면 계속 숨김).
★손으로 브러시를 넣으면 도우미를 다시 돌릴 때 지워진다★ (칸 트리를 새로 만든다) → 도우미 `BuildWardrobeSlotWidget` 의 `Image_Lock` 에 텍스처 경로를 넣고 다시 돌리는 쪽이 맞다 (6.2).

### C5. 문구 수집
1. 옷 이름 · 설명을 확정한 뒤 Localization Dashboard 로 Gather → 한/영 번역. (테스트 고깔 정리는 끝났다 — 9.0)
2. 세이브에 지운 옷(옛 `Skin.Gardener.*` · 테스트 고깔)이 장착돼 있었다면 부팅 정화(카탈로그에 없는 Id → None)로 자동으로 벗겨진다.

### C6. 옷장 장치 · 구도 마무리
- `BP_DRWardrobe` 에 옷장 메시(`WardrobeMesh`)와 "[F] 옷 갈아입기" 프롬프트(`InteractionWidget` 또는 `On Local Player Entered Range` 이벤트) — 지금은 수납장 큐브 앞의 보이지 않는 상자다.
- 큰 옷이 들어오면 `CaptureLocationByClass` 재확인 (7.3). 공용 8벌은 4종 모두 프레임 안이다(가장 높은 토끼 귀 포함).

### C7. 해금 토스트 UI (M5 의 남은 절반)
- 재료는 다 있다: GI 델리게이트 `On Skin Unlocked(SkinId)` (스테이지 보상 반영 때 새로 풀린 옷마다) + `Get Skin Display Name(SkinId)`.
- 결과창(스테이지 결과 위젯)에서 구독해 "새 옷 해금! — <이름>" 을 띄운다. `Client_GrantStageReward` 가 보상 반영을 끝낸 뒤 결과창이 열리므로 순서는 보장돼 있다.
- ★시안이 없다★ — 모양을 정한 뒤 만든다. D-27 처럼 그리기는 C++ 로 두고 WBP 는 위젯 이름만 맞추는 방식을 권한다.

---

## 10. 마무리 검증 (M7)

| 상황 | 기대 | 상태 |
|---|---|---|
| 2인 PIE: 서로 다른 옷 · 옷장 닫기 반영 · 사망/부활 · 스테이지 이동 · 대기실 로봇 변경 | 옷이 계속 맞다 | ✅ 2026-10-20 (8.0) |
| 2인 PIE: 재접속 · 늦은 참가 · 스테이지 → 로비 복귀 | 옷이 계속 맞다 | ⬜ (마일스톤마다 다시 돌린다 — 6.5) |
| 대기실 디스플레이 캐릭터 | 양쪽 화면에서 세이브의 옷 | ✅ (8.0) |
| 카탈로그에서 스킨 삭제 후 실행 | 부팅 정화로 기본 외형, 서버도 한 번 더 거른다 | ⬜ |
| 프리뷰 스튜디오를 레벨에서 지움 | 프리뷰만 접히고 칸 목록 정상 (D-12) | ⬜ |
| 부위당 7개 이상 | 스크롤로 3열 유지 (스크롤바 없음 — D-18) | ⬜ |
| 1280×720 · 2560×1440 | 배치 비례 | ✅ 1280×720 / ⬜ 2560×1440 (모니터보다 큰 창은 못 띄운다) |
| 화면효과가 클릭을 먹지 않는다 | `Image_ScreenFX` Not Hit-Testable | 🔶 설정됨 · 손 확인 7.2 |
| 알파 낮은 3장(화면효과 · 탭바 · 캐릭터 back)에 띠 없음 | A1 | ✅ |
| 프리뷰를 어느 각도로 돌려도 안 잘림 | D-28 | ✅ 4종 × 24각도 촬영 |
| 스팀 오프라인 → 온라인 | backfill (B7) | ⬜ |
| 콘텐츠 | 로봇 4종 × 부위별 옷 · 아이콘 · 한/영 문구 · 자물쇠 아트 · 해금 토스트 | 🔶 정원로봇 8벌 (9.0) / ⬜ 나머지 (9장) |

---

# 부록

## 부록 A. 시안 실측 근거 (2026-10-19)

- **재료**: 원본 PNG 13장 — `C:\Users\OWNER\Documents\카카오톡 받은 파일\옷장\` (텍스처 에셋의 임포트 정보에 경로가 남아 있다). 개별 에셋은 피그마 sRGB 1080p 1:1 내보내기.
- **조립 시안 2장은 macOS 레티나 스크린샷**(Display P3, 피그마 약 55% 확대)이다:

| 파일 | 크기 | 디자인 1px 당 | 원점 오프셋 |
|---|---|---|---|
| `조립.png` (로비 배경 포함) | 2306×1294 | 1.20135 | (-1.1, -2.2) — 위 · 왼쪽이 1~2px 잘림 |
| `조립(배경없음).png` | 2128×1198 | 1.10730 | (1.37, 0.75) |

  가로 배율은 프레임 경계(2126.0px)와 패널 테두리 간격(795.05px ÷ 718)이 1.5×10⁻⁵ 까지 일치 → **패널이 1:1 로 쓰였고 프레임이 1920px 임**을 교차 확인. 세로 배율 차이 0.02%.
- **측정 방법**: 패널 테두리선(2px `#6FC8FF`) 4변 · 선택박스 1px 테두리 4변의 서브픽셀 중심 / 제목 · 라벨은 글자별 좌우 끝을 폰트 파일 메트릭(fontTools)으로 최소제곱 / 화면효과는 스캔라인 수직 프로파일 상관 / 2차 배경 · 캐릭터 back 은 합성 시뮬레이션 대조.
- **결과**: 7.1-1 표. 2026-08-25 값과 달라진 것 — 패널 (151,147)→(150,146), 탭바 x 202→201, 제목 BrunoAce 48 @ (207,52) → 학교안심 운동장 L 55px @ (202,54), 라벨 크기 · 색 · 대소문자, 화면효과 1753×993 → 1613×914, Dim 이중 알파.
- **1080p PIE 대조 (같은 날 저녁, `start_play_in_new_window(1920, 1077)` · FreeRoam · `Shot showui`)**:

| 항목 | PIE − 정렬본 | 비고 |
|---|---|---|
| 패널 테두리 왼 · 오 · 위 · 아래 | −0.01 · −0.02 · −0.02 · −0.04 px | 테두리선 서브픽셀 중심 (위는 x 620~900 구간) |
| 탭바 · 필 | 0 px (필 201~324 · 139~182) | |
| 선택 라벨 HEAD | (−0.01, −0.50) px | 글자 무게중심 |
| 제목 | 글자 폭 같음, 진행 −0.5px/글자 → 끝 글자 −3~−4px | T-35 |
| 1280×720 | 패널 테두리가 1080p × 0.666 자리에서 1.2px 안 | DPI 0.666 |

- **에셋 알파 최대값** (텍스처 에셋 LayerColorInfo): 1차 배경 0.608 균일 · 2차 배경 0.996 · 캐릭터 back 0.533 · 화면효과 0.188 · 탭바 0.4 · 선택박스 0.82 · 칸 기본 0.70 / 호버 0.98 / 선택 0.97.
- **프리뷰 구도 (D-28)**: 스튜디오 카메라 자리에서 화각을 넓혀(1.6배) 로봇 4종 × 8각도를 찍어 실루엣 폭을 쟀다 — 기본 구도(-420)에서 화면 중심 기준 최대 반폭: 정원로봇 570px(삽, 정면 498) · 청소기 509 · 벨보이 482(가방, 정면) · 자판기 209. 쓸 수 있는 반폭은 391(= 782/2). 거리 · 높이를 정한 뒤 24각도로 다시 찍어 확인 (최소 여백 23~24px, 자판기 197px).

## 부록 B. 블렌드 변환 근거

- **UMG 는 감마 공간에서 섞는다** (UE 5.5 `SlateElementPixelShader.usf` · `SlateRHIRenderingPolicy.cpp`): 텍스처는 선형으로 읽히고, 최종 색에 pow(1/2.2)(`GammaCorrect`) 를 건 뒤 블렌드한다.
  Additive 머티리얼 출력 = `Final Color × Opacity`(× 위젯 색 · 알파), 블렌드 One/One. Modulate = 아래 색 × 출력(1 을 넘을 수 없다). Translucent = 일반 알파 → **일반 이미지는 피그마와 같게 보인다.**
  → Additive 의 세기는 ^0.45 로 커진다(T-15). `Opacity = Power(알파 × 세기, 2.2)` 로 넣으면 출력이 `텍스처 × 알파 × 세기` 그대로가 된다.
- **2차 배경 (Soft Light, 하늘색 212/238/252)**: Soft Light 는 밝은 색일 때 `Cb + (2Cs−1)(D(Cb)−Cb)` — 아래가 어두울수록 적게 밝힌다.
  인게임 바탕(어두운 로비 × 0.39 ≈ 0.15)에서 중심부 증가량 ≈ R +40 · B +58 레벨 → 가산으로 **세기 약 0.2**. 순검정 위에서는 Soft Light 가 아무것도 안 하는데 가산은 밝히므로 아주 어두운 곳은 조금 더 밝아진다(허용).
- **화면효과 (Soft Light, 회색 평균 0.39 · 알파 최대 0.19)**: 회색 < 0.5 이면 Soft Light 는 **어둡게** 한다. 어두운 바탕에서 ≈ `바탕 × 2·회색` → Modulate 가 맞다. 밝은 줄(> 0.5)의 미세한 밝힘은 Modulate 로 못 낸다(무시).
  `M_UI_ScanlineModulate`: `Lerp(1, Saturate(Power(Tex.RGB, 0.4545) × 2), Tex.A × Strength)` → `Power(…, 2.2)` → Final Color. 앞의 0.4545 는 텍스처를 감마 값으로 되돌리고, 뒤의 2.2 는 UMG 출력 감마를 상쇄한다. 효과는 ±4 레벨로 아주 옅다.
- **캐릭터 back (Lighter Color 90%)**: 내보낸 PNG 를 W3C/포토샵 블렌드 17종 · 두 레이어 순서로 시뮬레이션해 봐도 시안 오차가 5~7 레벨 남는다. 바닥 띠에서 R 은 거의 그대로이고 G · B 만 밝아진다.
  가산 근사 최적 = 채널별 (-0.06, 0.35, 0.55) → **Tint `00A3FF` × 세기 0.55**. 정확히 맞추려면 "보이는 최종 모습"으로 다시 내보낸 PNG 가 필요하다.
- 2차 배경 위치(중앙 1:1)와 화면효과 92% · 상단 y=84 는 합성 시뮬레이션에서 오차 1.8 레벨로 맞았다.

## 부록 C. 캐릭터(로봇) 해금 — 이번 범위 제외 (D-22), 필요 작업 보존

도입하게 되면(전부 순수 추가 — 1부 설계와 충돌 없음):
1. `FDRCharacterUnlockDef` (`RequiredAchievements` 재사용) — `DRCosmeticTypes.h` 확장
2. `UDRGameInstance::IsCharacterUnlocked(Class)` — 해금 판정과 같은 파생 방식 (D-2)
3. 클라 → 서버 해금 목록 보고 — 선택이 서버 권위 순환이라 서버가 알아야 게이팅된다 (`ADRPlayerState` 복제 필드 + RPC)
4. 대기실 순환에서 잠긴 인덱스 건너뛰기 — 단순 모듈로를 "ClassCount 번 안에서 다음 해금 로봇 찾기" 루프로 (전부 잠긴 병리적 상황도 ClassCount 회로 끝난다)
5. 복원 경로 3곳 폴백(잠긴 클래스면 Gardener) — `DRLobbyGameMode` · `DRStageGameMode` · `DRTutorialGameMode` 의 클래스 복원부
6. (선택) 대기실 UI 잠금 안내

안전 기본값: 규칙이 없는 로봇은 해금으로 본다. 서버가 보고를 못 받았으면 전부 해금. **Gardener 는 폴백이라 절대 잠그지 않는다.**
고려: 대기실에서 로봇이 사라지는 건 신규 유저 체감이 크다 — "전부 기본 제공 + 옷만 해금"도 완결된 선택지다.

## 부록 D. 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-07-01 | 작성 (스팀 업적 + 스킨 해금 초안) |
| 2026-07-27 | 1차 전면 개정 |
| 2026-08-25 | 2차 전면 개정 — "로비 옷장에서 골라 입고 그대로 플레이"로 구체화, 결정 4건(로봇 해금 제외 · 계정 키잉 · 스팀 등록 지연 · 3D 프리뷰), 아트 시안 반영(4부위 = 부착물), WBP 사양 · 클릭 가이드 |
| 2026-09-02 | C++ 토대 구현 커밋 (`9f71431f`) |
| 2026-10-19 | **3차 — 압축 + 재실측.** 완료분을 1부로 압축(결정 D-1~26 · 함정 T-1~31 신설, 코드 주석 38곳을 `Plan(옷장)` + ID 로 교체), 남은 일을 2부로 재작성. 시안 재실측으로 좌표 · 폰트 · 색 · 화면효과 크기 · 블렌드 변환(감마 보정 · Modulate) 정정. 이전 판(3619줄)은 git `9f71431f:Plan.md` |
| 2026-10-19 (저녁) | **에디터 A1~A10 완료 + 배선 C++ 이전.** 그래프 배선을 전부 C++ 로(D-27 — 탭 · 칸 · 프리뷰 · 화면 · 컨트롤러), 에디터 도우미 `UDRWardrobeEditorTools` 신설(WBP 트리 생성 · `ImportPropertyText` · 고정 크기 PIE 창), 테스트 카탈로그 6벌 · 스튜디오 · 옷장 배치. 1080p PIE 로 시안 대조(부록 A), 로봇별 프리뷰 구도(D-28), 함정 T-32~36. 2부 A 의 클릭 안내서(약 440줄)를 7.1 요약으로 압축하고 콘텐츠 작업을 9장(C)으로 분리. 직전 판은 scratchpad 백업 |
| 2026-10-20 | **M3 · M5 · 사망 · 검증기 + 실제 옷.** B1(복제 — 호스트 보고 위치 수정 T-38) · B2(사망 숨김 양방향) · B5(대기실 디스플레이 + 이름 헬퍼) · B6(D-29) 구현 · 2인 PIE 로 확인(8.0). 정원로봇 실제 옷 8벌 등록(D-30 · 9.0 · 부록 E). 확인 도구(T-37 `CallOnNextTick`, 2인 PIE 창, 치트 `DRDie`/`DRRevive`). B1 · B2 · B5 · B6 본문은 8.0 요약으로 압축. 직전 판은 scratchpad 백업 |
| 2026-10-20 (오후) | **공용 옷.** 8벌을 4종 공용으로(`bCommonSkin` · `IsUsableBy` · 해금 공용 — D-31), 부위 소켓 5개 × 4종 + 옷마다 상대 이동 + 로봇별 덮어쓰기(D-32 — `ThirdPersonByClass` · `ResolveCosmeticClass` · `SetMeshSocket`), 검증기에 공용 규칙 · 로봇별 소켓 검사. SkinId `Skin.Shared.*` 로 변경, 테스트 고깔 전부 삭제, D-30 은 D-32 로 대체. 함정 T-40. 직전 판은 scratchpad 백업 |

## 부록 E. 의상 GLB → 로봇 좌표 맞춤 (D-30 → D-32, 2026-10-20)

- **재료**: `C:\Users\OWNER\Documents\카카오톡 받은 파일\gardener_{rabbit,gentleman,devil,angel} hat.glb` — 각각 부품 2개(머리 + 꼬리 또는 몸통)만 들어 있고 뼈에 붙어 있지 않다.
  Interchange 가 노드 변환을 정점에 구워 넣어, 메시 원점 = GLB 원점이다. 축은 정원로봇 메시와 같다 (X 좌우 · Y 앞 · Z 위).
- **측정**: 스튜디오 자리에 기준 자세 정원로봇(`SkeletalMeshActor`)과 부품을 띄우고 정사영(옆 · 앞 · 뒤 · 위)으로 찍어 실루엣을 맞댔다.
  변환 없이 놓으면 부품이 아래 · 앞으로 밀려 있다(나비넥타이가 무릎 앞에 뜬다).
- **결과**: 메시 좌표 = GLB × **1/1.5** + (−2.49, −18, **64.18**).
  - 1/1.5 — 의상이 인게임 크기(캡슐 1.5배)의 정원로봇에 맞춰 만들어졌다. 붙이면 부모 1.5배와 곱해져 원래 크기가 된다.
  - 64.18 — GLB 원점 높이 = 엉덩이 관절(`Leg_001_L/R`, z 64.18).
  - −18 — 모자 · 고리의 중심이 모니터 중심(Y=0)에 오고, 나비넥타이 뒷면이 화면 앞면(Y≈+35)에 닿는 값. −2.49 = GLB 의 x 0.0373m × 1/1.5 (좌우 중심 보정).
  - `Body` 뼈(메시 (0, 0, 80.61), 회전 없음) 기준 상대값 = **(−2.49, −18, −16.43) · 배율 0.6667** — 카탈로그 8벌 공통.
- **확인**: 정사영 4방향 겹쳐보기(모자는 머리 위 가운데 · 나비넥타이는 화면 아래 앞면 · 날개는 등 · 꼬리는 등 아래) + 1080p PIE 세트별 프리뷰.
- **공용화(D-32)**: 위 변환을 "소켓 × 옷 상대 이동" 으로 나눴다. 정원로봇 소켓 = Body 뼈 기준 (메시 기준점 − 뼈 위치), 배율 1/1.5. 옷 상대 이동 = 소켓⁻¹ × 옛 변환 = 순수 이동(GLB 단위) — 예) 머리 옷 (−3.73, −27, −100.23) 은 모자 밑면 중심을 소켓에 놓는다.
  다른 로봇은 같은 기준점(머리 꼭대기 · 화면 앞면 · 등 · 엉덩이 뒤)을 정사영으로 찾아 소켓을 놓고, 배율은 머리 폭 비(정원로봇 70 기준)로 잡았다. 실제 값은 9.0 표.
