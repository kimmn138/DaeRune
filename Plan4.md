# Plan4 — 언어 설정 정리(중국어 제거) 및 실제 UI 다국어화 구현 계획

## 0. 목표 요약

1. **설정 화면의 언어 옵션에서 중국어(Chinese)를 제거**하고 **한국어 / English** 두 가지만 남긴다.
2. 설정에서 언어를 바꾸면 **현재 UI에 표시되는 모든 텍스트가 실제로 해당 언어로 즉시 갱신**되도록 다국어 시스템을 구축한다.
3. 선택한 언어는 **재실행 후에도 유지**되어야 한다(로컬 저장).
4. **현재 게임 빌드/패키징을 깨뜨리지 않고** 점진적으로 적용 가능한 단계별 구조로 진행한다.

---

## 1. 현재 상태 진단 (조사 결과 요약)

| 항목 | 현재 상태 | 비고 |
|------|----------|------|
| 언어 옵션 정의 | C++ 하드코딩 | `DRSettingsManager.cpp:482-513`, Korean / Chinese / English 3종 |
| 언어 적용 로직 | **미구현** | `DRSettingsManager.cpp:919-922`에 `// TODO (localization system integration pending)` 주석만 존재 |
| 언어 영속화 | 없음 | `UDRGameUserSettings`, `UDRSaveGame` 어디에도 언어 필드 없음 |
| `Config/DefaultGame.ini` | `CulturesToStage=en` 만 설정 | 한국어/중국어 컬처가 stage 대상에 없음 |
| `Config/DefaultEngine.ini` | `[Internationalization]` 섹션 자체가 없음 | |
| Localization Dashboard | **사용 안 함** | `Content/Localization/` 폴더 없음, `.archive/.manifest/.locres/.locmeta` 파일 전무 |
| C++ 텍스트 처리 | `FText::FromString(TEXT("..."))` 사용 | LOCTEXT/NSLOCTEXT 매크로 미사용 → **번역 추출 대상이 아님** |
| 위젯 텍스트 | 위젯 내부에 직접 입력된 한국어 문자열 다수 | 디자인 시점에 입력된 텍스트는 자동 수집되지만, 코드에서 동적으로 들어가는 텍스트는 수집되지 않음 |

> **핵심 결론**: 단순히 옵션에서 "Chinese"만 지운다고 끝이 아니다. 현재 시스템은 **언어를 바꿔도 아무 일도 일어나지 않는 UI 더미 상태**다. 따라서 (a) UI 옵션 정리, (b) 영속화, (c) `FInternationalization` 적용, (d) 모든 텍스트의 LOCTEXT 변환, (e) Localization Dashboard 셋업 및 번역 워크플로우 정착, 다섯 단계가 모두 필요하다.

---

## 2. 전체 작업 단계 (High-Level Roadmap)

| 단계 | 이름 | 목적 | 예상 난이도 |
|-----|-----|-----|-----------|
| **STEP 1** | 옵션 목록에서 중국어 제거 | "Chinese" 옵션 삭제, 기본값 정합성 확보 | ⭐ |
| **STEP 2** | 언어 영속화 인프라 구축 | `UDRGameUserSettings`에 언어 필드 추가, 부팅 시 적용 | ⭐⭐ |
| **STEP 3** | `FInternationalization` 연동 | `ApplySingleSetting`에서 실제 컬처 변경 + UI 즉시 갱신 | ⭐⭐ |
| **STEP 4** | Config 컬처 등록 | `DefaultGame.ini` / `DefaultEngine.ini`에 ko, en 등록 | ⭐ |
| **STEP 5** | 모든 텍스트 LOCTEXT화 | C++ `FText::FromString` → `LOCTEXT`, 위젯 텍스트 한국어/영어 번역 가능하게 | ⭐⭐⭐⭐ (분량 多) |
| **STEP 6** | Localization Dashboard 셋업 | 로컬리제이션 타깃 생성, Gather/Compile 파이프라인 구축 | ⭐⭐⭐ |
| **STEP 7** | DataTable의 텍스트 컬럼 다국어화 | `DT_PhaseObjective`, `DT_WaveLevelData` 등 FText 컬럼 검토 | ⭐⭐ |
| **STEP 8** | 동적 텍스트(런타임 포맷) 점검 | "%s 스킬 획득" 같은 포맷 문자열 → `FText::Format` + LOCTEXT | ⭐⭐⭐ |
| **STEP 9** | 런타임 갱신 처리 | 언어 변경 즉시 재생성/Refresh 필요한 위젯들 일괄 갱신 브로드캐스트 | ⭐⭐ |
| **STEP 10** | QA / 회귀 테스트 시나리오 작성 | 한↔영 토글, 재실행 유지, 멀티플레이 영향 없음 검증 | ⭐⭐ |

---

## 3. STEP별 상세 구현 계획

### STEP 1 — 옵션 목록에서 중국어 제거

**대상 파일**: `Source/DaeRune/Private/Game/DRSettingsManager.cpp` (라인 482~513 근방)

**현재 코드 형태 (요약)**
```cpp
// Gameplay.Language
Def.SettingId = FName("Gameplay.Language");
...
FDRSettingsOption KoreanOpt;  KoreanOpt.OptionId  = FName("Korean");  KoreanOpt.DisplayText  = FText::FromString(TEXT("KOREAN"));
FDRSettingsOption ChineseOpt; ChineseOpt.OptionId = FName("Chinese"); ChineseOpt.DisplayText = FText::FromString(TEXT("CHINESE"));
FDRSettingsOption EnglishOpt; EnglishOpt.OptionId = FName("English"); EnglishOpt.DisplayText = FText::FromString(TEXT("ENGLISH"));
Def.DefaultValue = UDRSettingsFunctionLibrary::MakeNameValue(FName("Korean"), 0);
```

**수정 내용**
1. `ChineseOpt`를 추가하는 3줄을 **완전히 삭제**한다(주석 처리 X).
2. `DefaultValue`의 인덱스 부분(두 번째 인자 `0`)이 Korean을 가리키는지 재확인한다. Korean이 첫 번째 옵션이므로 `0`이 그대로 유효하다.
3. `DisplayText`도 STEP 5에서 LOCTEXT로 바꿀 예정이지만, STEP 1에서는 우선 옵션 제거에만 집중한다.
4. **OptionId 값 재검토**: 옵션 ID(`Korean`, `English`)는 영속화 키와 직결되므로 변경하지 않는다. (한국어로 바꾸지 말 것)

**기존 저장값 호환성**
- 만약 사용자가 이전 빌드에서 "Chinese"를 골라 저장했다면, 다음 부팅 때 `CurrentValues`에 "Chinese"가 들어 있을 수 있다.
- `ApplySettings` / `LoadSettings` 단계에서 옵션 목록에 없는 OptionId는 **DefaultValue로 폴백**하도록 방어 코드 추가:
  ```cpp
  // (LoadSettings 직후 또는 ApplySingleSetting 진입부)
  if (SettingId == FName("Gameplay.Language"))
  {
      const FName SelectedId = Value.NameValue;
      const bool bExists = Def.Options.ContainsByPredicate(
          [&](const FDRSettingsOption& O){ return O.OptionId == SelectedId; });
      if (!bExists)
      {
          // 알 수 없는 언어값 → 기본값으로 강제
          SetValue(SettingId, Def.DefaultValue);
          Value = Def.DefaultValue;
      }
  }
  ```

**검증 방법**
- 에디터 실행 → 설정 → Gameplay 탭의 LANGUAGE 드롭다운에 **KOREAN / ENGLISH 두 항목만** 보이는지 확인.
- 이전 저장에서 Chinese를 선택했던 사용자도 정상 부팅되는지(폴백 동작) 확인.

---

### STEP 2 — 언어 영속화 인프라 구축

**대상 파일**
- `Source/DaeRune/Public/Game/DRGameUserSettings.h`
- `Source/DaeRune/Private/Game/DRGameUserSettings.cpp`
- `Source/DaeRune/Private/Game/DRSettingsManager.cpp` (Save/Load 통합 지점)

**작업 내용**

1. `UDRGameUserSettings`에 언어 필드 추가:
   ```cpp
   /** Stored as culture code: "ko", "en". Korean is project default. */
   UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Localization")
   FString PreferredCulture = TEXT("ko");
   ```
   - `Config` 키워드로 `GameUserSettings.ini`에 자동 저장된다(별도 SaveGame 불필요).
   - 게임 인스턴스/세이브 게임에 추가하지 않는 이유: 언어는 **런타임 유저 환경 설정**이며, 멀티플레이 캐릭터 진행도와 무관하기 때문.

2. 옵션 ID(`Korean`, `English`) ↔ 컬처 코드(`ko`, `en`) 매핑 테이블을 한 곳에 정의:
   - `DRSettingsManager.cpp` 상단 또는 별도 헬퍼에:
     ```cpp
     static const TMap<FName, FString>& GetLanguageOptionToCultureMap()
     {
         static const TMap<FName, FString> Map = {
             { FName("Korean"),  TEXT("ko") },
             { FName("English"), TEXT("en") }
         };
         return Map;
     }
     ```
   - 역방향(컬처 → 옵션 ID) 헬퍼도 함께 만든다.
   - 향후 언어 추가 시 **이 한 곳만 수정**하면 되도록 단일 진실 소스(SSOT)를 유지.

3. **저장**: 사용자가 LANGUAGE 드롭다운을 바꾸고 [Apply]를 누르면
   - `ApplySingleSetting("Gameplay.Language", value)` 내부에서
   - 옵션 ID → 컬처 코드 변환 → `GameUserSettings->PreferredCulture` 갱신 → `SaveSettings()` 호출.

4. **로드/부팅 적용**: `UDRGameInstance::Init()` (또는 `OnStart()`)에서
   - `GameUserSettings = UGameUserSettings::GetGameUserSettings()` 후
   - `FInternationalization::Get().SetCurrentLanguageAndLocale(GameUserSettings->PreferredCulture)` 호출.
   - **주의**: 이 호출은 GameInstance가 만들어진 매우 이른 시점이어야 한다. 위젯이 캐싱되기 전에.

**기존 SaveGame 클래스(`UDRSaveGame`)는 건드리지 않는다.** 거기에는 튜토리얼 진행 등 캐릭터 진행도만 두고, 언어는 GameUserSettings에 두는 것이 Unreal 관용에 맞다.

---

### STEP 3 — `FInternationalization` 연동 (실제 언어 전환)

**대상 파일**: `Source/DaeRune/Private/Game/DRSettingsManager.cpp` 라인 919~922

**현재 코드**
```cpp
else if (SettingId == FName("Gameplay.Language"))
{
    // TODO (localization system integration pending)
}
```

**교체 코드 (의사 코드)**
```cpp
else if (SettingId == FName("Gameplay.Language"))
{
    const FName OptionId = Value.NameValue;
    const TMap<FName, FString>& Map = GetLanguageOptionToCultureMap();
    if (const FString* CulturePtr = Map.Find(OptionId))
    {
        const FString CultureCode = *CulturePtr;

        // 1) 엔진 컬처 변경 (UI에 보이는 모든 LOCTEXT 갱신 대상)
        FInternationalization::Get().SetCurrentLanguageAndLocale(CultureCode);

        // 2) 영속화
        if (UDRGameUserSettings* Settings = Cast<UDRGameUserSettings>(UGameUserSettings::GetGameUserSettings()))
        {
            Settings->PreferredCulture = CultureCode;
            Settings->SaveSettings();
        }

        // 3) UI 갱신: 변경 즉시 반영을 위한 브로드캐스트
        OnLanguageChanged.Broadcast(CultureCode);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Language] Unknown option id: %s, falling back to default."), *OptionId.ToString());
    }
}
```

**필요한 신규 멤버**
- `DRSettingsManager.h`에 다음 델리게이트 선언:
  ```cpp
  DECLARE_MULTICAST_DELEGATE_OneParam(FOnLanguageChanged, const FString& /*CultureCode*/);
  FOnLanguageChanged OnLanguageChanged;
  ```
- 이 델리게이트는 STEP 9에서 위젯 갱신 트리거로 활용.

**API 선택 근거**
- `SetCurrentLanguage()`만 호출하면 되지만, 날짜/숫자 포맷도 함께 영향을 받는 것이 자연스러우므로 `SetCurrentLanguageAndLocale()`이 더 적절.
- 향후 영어 사용자가 로케일은 시스템 그대로 두고 언어만 바꾸길 원한다면 분리 고려.

---

### STEP 4 — Config 컬처 등록

**대상 파일**: `Config/DefaultGame.ini`, `Config/DefaultEngine.ini`

#### `DefaultGame.ini` 수정
현재(라인 59~62 근처):
```ini
InternationalizationPreset=English
-CulturesToStage=en
+CulturesToStage=en
LocalizationTargetCatchAllChunkId=0
```

변경 후:
```ini
InternationalizationPreset=EnglishKorean   ; 임의 명, Editor의 Project Settings 에서도 확인
-CulturesToStage=zh-Hans
-CulturesToStage=zh-Hant
+CulturesToStage=en
+CulturesToStage=ko
LocalizationTargetCatchAllChunkId=0
```

> `+CulturesToStage` 항목에 패키징 시 함께 빌드될 컬처를 모두 등록한다. 한국어가 들어오는 게 핵심.
> `InternationalizationPreset`은 Project Settings → Packaging → Internationalization Support 에서 GUI로도 변경 가능. 영어+한국어만 패키징하는 프리셋을 선택하면 자동으로 갱신된다.

#### `DefaultEngine.ini` 추가 (선택적이지만 권장)
```ini
[Internationalization]
+LocalizationPaths=%GAMEDIR%Content/Localization/Game
```
Localization Dashboard에서 `Game` 타깃을 만들면 자동으로 등록되지만, 명시해 두면 안전.

#### 주의
- 빌드 머신에서 패키징 옵션을 통해 언어가 누락되는 경우가 많으므로, **`Project Settings → Packaging → Internationalization Support`** 도 GUI에서 확인.
- 현재 `Config/DefaultEngine.ini` 변경 사항이 git status에 이미 떠있으니, 이번 작업과 충돌하지 않도록 커밋 분리.

---

### STEP 5 — 모든 텍스트 LOCTEXT화 (가장 큰 작업)

> 이 단계는 **분량이 매우 크므로** 추가 하위 단계(5-A ~ 5-D)로 나눈다.

#### 5-A. C++ 코드의 `FText::FromString` 일괄 점검

**대상 파일** (조사 결과):
- `OverlayWidgetController.cpp`
- `DRTutorialManager.cpp`
- `DRStageGameState.cpp`
- `DRSettingsManager.cpp` (가장 많음)
- `DROverheadWidget.cpp`

**작업**

1. 각 cpp 파일 상단에 LOCTEXT 네임스페이스 선언:
   ```cpp
   #define LOCTEXT_NAMESPACE "DRSettings"   // 파일/모듈별로 다른 이름
   ```
   파일 마지막에:
   ```cpp
   #undef LOCTEXT_NAMESPACE
   ```
2. `FText::FromString(TEXT("LANGUAGE"))` → `LOCTEXT("Settings_Language_Label", "LANGUAGE")` 형태로 모두 치환.
3. **OptionId 와 DisplayText 분리 원칙**: OptionId는 절대 번역하지 않는다(코드 키). DisplayText만 LOCTEXT로 한다.
4. 변환 시 키(첫 번째 인자)는 **고유성 + 안정성**이 중요. 의미 기반으로 명명:
   - `Settings_Display_Mode`, `Settings_Resolution`, `Settings_Volume_Master`, `Settings_Lang_Korean`, `Settings_Lang_English` 등.
5. 동일한 영문 문자열이라도 **맥락이 다르면 키를 분리**해야 한국어 번역에서 자연스러운 표현이 가능(예: "Apply" 버튼과 "Apply" 표제어).

**팁**
- 각 파일을 한 번에 모두 변환하면 diff가 너무 커져 리뷰가 어렵다. 화면 단위(Settings 화면, Tutorial, Overlay 등) 별로 PR을 쪼개는 것을 권장.
- 변환 누락을 막기 위해 작업 후 `Grep`으로 `FText::FromString(TEXT(` 패턴이 0건인지 확인.

#### 5-B. 위젯 블루프린트(WBP) 텍스트 점검

블루프린트 위젯의 TextBlock에 직접 입력된 한국어 문자열(예: "옵션", "이전 화면으로", "스킬 획득" 등)은 Localization Gather 시 자동 수집된다. 단, 다음을 점검해야 한다:

1. **하드코딩 한국어 → 정확한 마스터 텍스트**: 현재 위젯의 마스터 언어가 한국어인지 영어인지 정해야 한다.
   - **권장 방향**: 마스터(원문)는 **한국어**로 둔다(현재 작성 상태가 한국어이고, 이미 콘텐츠가 다 한국어로 되어 있어 영어로 옮기는 비용이 큼).
   - 영어가 번역 대상이 된다.
2. 각 위젯의 `TextBlock`을 열어서 텍스트가 비어 있지 않고, **`Localizable` 체크박스가 켜져 있는지** 확인. 비활성화된 텍스트는 수집 대상에서 제외된다.
3. **이미지로 된 텍스트**(예: hp1.uasset, water.uasset 등 텍스처)는 다국어화 대상에서 제외하거나, 영어 버전 텍스처를 추가로 만들어야 한다(STEP 5-D 참고).

#### 5-C. 위젯 카탈로그 (1차 변환 대상)

조사로 식별된 다국어 우선 적용 위젯 (Content/Blueprints/UI/):
- **Settings**: `WBP_SettingsScreen`, `WBP_SettingsPage_Graphics/Gameplay/Audio/Controls`, `WBP_SettingRow_*`
- **Overlay**: `WBP_LobbyOverlay`, `WBP_StageOverlay`, `WBP_TutorialOverlay`, `WBP_WaitingRoomOverlay`
- **Skill UI**: `WBP_SkillIcon_Tutorial`, `WBP_SkillSlot_Tutorial1/2/3`
- **Container**: `WBP_HealthWaterBar`, `WBP_PlayerSlot`
- **TextUI**: `WBP_PhaseObjective`

각 위젯에 대해 다음 체크리스트를 수행:
- [ ] 모든 `TextBlock`/`RichTextBlock`의 텍스트가 `Localizable=true`
- [ ] 위젯 내 `Format` 노드의 매개변수가 텍스트 매개변수면 `FText::Format`을 사용하도록 변경
- [ ] 영어 버전에서 한국어보다 길어질 위험이 있는 텍스트(`KOREAN`/`ENGLISH` 같은 헤더 vs "한국어"/"영어")는 폰트 자동 줄바꿈/말줄임 옵션 확인

#### 5-D. 텍스처 형태로 박힌 텍스트 처리(선택)

현재 `Content/DaeRuneAssets/UI/PlayerAttributeBar/hp1.uasset`, `water.uasset` 등은 텍스트가 그래픽으로 합쳐진 이미지일 가능성. 다음 중 선택:

- **Option A (권장 - 최소 비용)**: 해당 텍스처가 **언어 비의존적인 아이콘**임을 확인하고 그대로 둔다.
- **Option B**: 컬처별 `localized` 폴더(`/Content/Localization/.../{culture}/`)에 텍스처 변형을 두는 Unreal Asset Localization 사용. 작업 비용이 큼 → 후순위.

---

### STEP 6 — Localization Dashboard 셋업

**한 번만 하면 되는 셋업 작업**:

1. **Editor → Tools → Localization Dashboard** 열기.
2. 새 Target 생성: 이름 `Game` (관례).
3. **Cultures** 탭에서:
   - Native Culture: `ko` (Korean). 우리 마스터 텍스트가 한국어이므로.
   - Add Culture: `en` (English).
4. **Gather Settings → Gather from Text Files**:
   - Search Path: `Source/DaeRune`
   - Wildcards: `*.cpp`, `*.h`
5. **Gather from Packages**:
   - Search Path: `Content/Blueprints/UI`, `Content/Blueprints/Phase`, 그리고 위젯/데이터테이블이 있는 디렉토리
6. **Gather Text** 실행 → `.archive` 생성.
7. `en` 컬처에 대한 `.archive` 파일을 외부 번역가에게 넘기거나, 우선 개발 중에는 임시로 `[en]` prefix를 채워 테스트.
8. **Compile Text** → `.locres` 생성.
9. 생성물 위치 확인: `Content/Localization/Game/{ko,en}/Game.archive`, `Game.locres`, `Game.manifest`, `Game.locmeta`.
10. 패키징 시 누락되지 않도록 `Project Settings → Packaging → Additional Asset Directories to Cook` 에 `Content/Localization` 자동 포함 여부 확인.

**번역 워크플로우 정리** (문서/팀 공유용):
- 새 텍스트 추가 → C++/위젯에서 작성(한국어 마스터) → Localization Dashboard에서 Gather → en .archive 갱신 → 번역 입력 → Compile → 게임 실행 시 자동 적용.

---

### STEP 7 — DataTable의 텍스트 컬럼 다국어화

**대상 후보**:
- `Content/Blueprints/Phase/Data/DT_PhaseObjective` (예상: 페이즈 목표 텍스트)
- `Content/Blueprints/Phase/Data/DT_WaveLevelData`
- `Content/Blueprints/AbilitySystem/Data/DA_EnemyCharacterClassInfo` 등

**작업**
1. 각 DataTable을 열어 컬럼 타입이 `FText`인지 `FString`인지 확인.
2. `FString`이면 → 다국어 불가. **`FText`로 마이그레이션** 필요. (행 데이터 재입력 또는 변환 스크립트)
3. `FText`이면 자동으로 Gather 대상이 되며, `.archive`에 노출됨.
4. 데이터 에셋(`UPrimaryDataAsset` 등)도 동일하게 `FText` 사용 권장.

---

### STEP 8 — 동적 텍스트(런타임 포맷) 점검

위험한 패턴:
```cpp
// 잘못된 예 — 한국어 어순이 영어와 달라 번역 시 깨짐
FText::FromString(FString::Printf(TEXT("%s 님이 %d개의 파츠를 획득했습니다"), *PlayerName, Count));
```

수정 패턴:
```cpp
// 올바른 예 — Format 인자를 키로 묶어 어순 자유도 부여
FText Msg = FText::Format(
    LOCTEXT("Pickup_Notify", "{Player}님이 {Count}개의 파츠를 획득했습니다"),
    FText::FromString(PlayerName), FText::AsNumber(Count));
```
- 영어 번역에선 `"{Player} picked up {Count} part(s)."` 식으로 Player와 Count 위치를 자유롭게 둘 수 있다.
- 단/복수, 성별 처리는 `FText::Format`의 plural form 기능 활용.

**점검 대상**:
- `OverlayWidgetController.cpp`에서 알림성 메시지 생성하는 부분
- `DRTutorialManager.cpp`의 튜토리얼 가이드 메시지
- `DROverheadWidget.cpp`의 머리 위 표시 텍스트

---

### STEP 9 — 런타임 갱신 처리 (즉시 반영)

`FInternationalization::SetCurrentLanguageAndLocale`은 컬처를 바꾸기만 한다. **이미 위젯에 캐싱된 `FText`** 는 자동 갱신되지 않을 수 있으므로 다음 두 가지 방식 중 선택:

**Option A (권장): 활성 위젯에 SetText 재호출**
- STEP 3에서 만든 `OnLanguageChanged` 델리게이트를 다음 위치에서 구독:
  - `WBP_SettingsScreen` → 자기 페이지 재구성
  - `WBP_StageOverlay`/`WBP_LobbyOverlay` 등 항상 떠있는 오버레이 → 모든 TextBlock의 SetText 재호출 또는 RebuildWidget
  - HUD 매니저(`ADRHUD`) → `OverlayWidgetController`에 BroadcastInitialValues 재호출

**Option B**: 설정 화면을 닫고 새로 열 때만 새 언어로 그려지도록 한다.
- 구현 비용은 작지만, **설정 화면 자기 자신의 텍스트는 즉시 안 바뀐다**. 사용자 경험 저하 → 비권장.

**구현 가이드**
1. `UDRSettingsManager`에 `OnLanguageChanged` (멀티캐스트) 추가.
2. `ADRHUD::BeginPlay`에서 SettingsManager의 델리게이트에 `RefreshAllOverlayTexts()` 바인딩.
3. `RefreshAllOverlayTexts()`는:
   - 활성 위젯들에 `SynchronizeProperties()` 호출하거나,
   - `OverlayWidgetController::BroadcastInitialValues()` 재호출.
4. **설정 화면**은 자체적으로 옵션 텍스트를 다시 만들어야 하므로 `BuildDefinitions()` 재실행 후 드롭다운 위젯 재구성.

---

### STEP 10 — QA 시나리오

| # | 시나리오 | 기대 결과 |
|---|---------|----------|
| 1 | 첫 부팅(설정 파일 없음) | 한국어로 시작, LANGUAGE 드롭다운에 KOREAN/ENGLISH 두 개 |
| 2 | 한국어 → 영어 변경 → Apply | 설정 화면 자체와 다른 화면(로비/스테이지 오버레이 등) 모두 즉시 영어로 전환 |
| 3 | 영어 상태에서 게임 종료 → 재실행 | 영어로 부팅 (`GameUserSettings.ini`의 `PreferredCulture=en` 유지 확인) |
| 4 | 이전 빌드의 잔존 "Chinese" 값 | 안전하게 한국어로 폴백, 크래시/빈 드롭다운 없음 |
| 5 | 인게임 중 언어 변경 | 페이즈 목표/체력바 라벨 등도 즉시 새 언어로 표시 |
| 6 | 멀티플레이어 호스트가 영어, 클라이언트가 한국어 | 각 클라이언트가 자기 로컬 언어로 표시 (서버 강제 X 확인) |
| 7 | 패키징 빌드 | 한국어/영어 .locres가 `Saved/StagedBuilds/.../Content/Localization/Game/{ko,en}/`에 존재 |
| 8 | 새 LOCTEXT 추가 후 Gather 미실행 빌드 | 신규 텍스트는 마스터(한국어)로 표시, 영어는 한국어로 폴백되어도 크래시 없음 |
| 9 | 폰트 글리프 누락 | 영어 폰트가 한국어 글리프를 가지지 않으면 `□` 표시 위험 → Composite Font에 한국어 폰트 등록 확인 |

---

## 4. 일정 권장 (예상치)

| 단계 | 예상 소요 |
|-----|---------|
| STEP 1 (옵션 제거) | 30분 |
| STEP 2 (영속화) | 1~2시간 |
| STEP 3 (i18n 연동) | 1시간 |
| STEP 4 (Config) | 30분 |
| STEP 5 (LOCTEXT화) | **2~5일** (위젯/코드 분량에 따라 변동, 가장 큰 비용) |
| STEP 6 (Loc Dashboard) | 1일 (셋업 + 첫 번역 입력) |
| STEP 7 (DataTable) | 0.5~1일 |
| STEP 8 (동적 텍스트) | 0.5~1일 |
| STEP 9 (런타임 갱신) | 0.5일 |
| STEP 10 (QA) | 0.5일 |

> STEP 1~4만 먼저 구현하면 **"중국어 제거 + 부팅 시 언어 적용"** 까지는 빠르게 끝난다. STEP 5~9가 본 작업의 절대다수 비용이며, 점진적으로 화면 단위로 PR을 쪼개 진행하는 것이 안전하다.

---

## 5. 진행 순서 제안 (PR 단위 분할)

1. **PR-1**: STEP 1 + STEP 2 + STEP 4
   - Chinese 제거 + 영속화 + Config 컬처 등록.
   - 이 시점에 빌드는 정상, UI는 여전히 마스터 언어로만 보임.
2. **PR-2**: STEP 3 + STEP 6 (Loc Dashboard 빈 셋업)
   - `FInternationalization` 호출, .archive/.locres 빈 상태로 생성.
3. **PR-3**: STEP 5-A (C++ LOCTEXT 변환, 화면 단위로 더 쪼개기)
4. **PR-4**: STEP 5-B/C (위젯 점검 및 마스터 일치)
5. **PR-5**: STEP 7 (DataTable)
6. **PR-6**: STEP 8 (동적 텍스트)
7. **PR-7**: STEP 9 (런타임 갱신)
8. **PR-8**: STEP 10 (QA 확정 및 폰트/Composite Font 정리)

---

## 6. 리스크 및 주의사항

| 리스크 | 대응 |
|-------|-----|
| 영어 폰트가 한국어 글리프 미포함 → 깨진 글자 | `Composite Font` 사용. 한국어 fallback 폰트(예: Noto Sans KR) 등록 필수 |
| `FText::FromString`과 `LOCTEXT` 혼용으로 일부 텍스트만 번역됨 | STEP 5 후 정규식 grep으로 `FText::FromString` 잔존 0건 확인 |
| 멀티플레이어 환경에서 서버가 클라이언트 언어 강제 | `FInternationalization`은 클라이언트 로컬에만 영향. **RPC로 보내는 텍스트는 키만 전송하고 클라이언트에서 LOCTEXT 룩업** 하는 형태로 설계 권장 |
| 사용자가 OS 시스템 언어를 한/영 외로 두면 첫 부팅 컬처 결정 | `UDRGameInstance::Init`에서 `PreferredCulture`가 비어 있을 때만 OS 언어를 보고, 한국어/영어가 아니면 한국어로 강제 |
| 패키징 시 .locres 누락 | `CulturesToStage` + `Project Settings → Packaging → Internationalization Support` 둘 다 확인 |
| 이미 만들어진 위젯의 빈 텍스트가 Localizable=false로 남아 있음 | STEP 5-B에서 위젯별로 일괄 점검. Localization Dashboard의 Gather 결과에서 누락된 텍스트가 있다면 위젯 설정 검토 |
| 동일 영문 문자열을 여러 곳에서 다른 의미로 사용 | LOCTEXT 키를 명확히 분리(맥락 prefix 사용) |

---

## 7. 즉시 시작 가능한 첫 커밋 (Step 1만 분리)

가장 위험이 작고 즉시 머지 가능한 변경:

1. `DRSettingsManager.cpp` 에서 `ChineseOpt` 추가 코드 삭제.
2. 폴백 코드(STEP 1의 "기존 저장값 호환성") 삽입.
3. 빌드/PIE에서 LANGUAGE 드롭다운에 두 항목만 보이는지 확인.
4. 커밋 메시지 예: `feat(settings): drop Chinese option from language dropdown (placeholder only)`

이 PR을 먼저 머지하고, 이후 STEP 2~10을 별도 PR로 진행하면 게임 빌드 안정성에 영향 없이 점진 적용 가능.

---

## 8. 위젯 직접 입력 텍스트 / DataTable 텍스트 마이그레이션 상세

> 사용자 질문 — "지금 다른 UI들에 그냥 텍스트로 직접 적어놓거나 DataTable로 띄울 텍스트를 저장해놓은 것들이 있는데, 이런 건 이후에 어떻게 바꿔야 하는가?"
> 이 섹션은 **두 가지 텍스트 소스(위젯 직접 입력 / DataTable 저장)** 각각에 대한 마이그레이션 절차를 STEP 5·STEP 7보다 한 단계 더 깊게 다룬다.

### 8.0 텍스트 소스 인벤토리 (조사 결과 확정본)

DaeRune 프로젝트의 UI 텍스트는 현재 **5가지 소스**에 분산되어 있다:

| # | 소스 분류 | 예시 위치 | 다국어 가능 여부 (현재) | 우선순위 |
|---|----------|----------|----------------------|---------|
| (A) | **위젯에 직접 입력된 정적 텍스트** | `WBP_LobbyOverlay`, `WBP_StageOverlay`, `WBP_SettingsScreen` 등 모든 WBP의 TextBlock | 부분적(Localizable 체크 필요) | 高 |
| (B) | **DataTable의 FText 컬럼** | `DT_PhaseObjective` (`FPhaseObjectiveData::ObjectiveTitle`, `ProgressFormat`) | 가능(Gather 대상) | 高 |
| (C) | **C++ 코드의 `FText::FromString` 하드코딩** | `OverlayWidgetController.cpp` Phase 알람, `DRStageGameState.cpp:43-44` placeholder | **불가능** (수집 안 됨) | 高 |
| (D) | **C++ 코드의 NSLOCTEXT 하드코딩** | `DRTutorialManager.cpp` 라인 62~296 (14건, 대부분 한국어) | 가능(Gather 대상) | 中 |
| (E) | **Data Asset(`UPrimaryDataAsset` 등)의 FText 필드** | `FEffectInfo::EffectName` (status effect) | 가능 | 中 |

**전략 핵심**: (A)와 (B)는 데이터 자체에 한국어/영어 변형을 별도로 두기보다, **마스터(원문) 텍스트만 한국어로 두고 Localization Dashboard의 Gather 결과인 `.archive`에서 영어 번역을 채워넣는 방식**으로 통일한다. 이렇게 해야:
- 데이터 구조를 두 번(컬처별로) 만들 필요가 없다
- 새 언어를 추가할 때 데이터를 복제하지 않아도 된다
- 번역가에게 `.archive` 한 묶음만 넘기면 된다

---

### 8.1 (A) 위젯에 직접 입력한 텍스트 — 마이그레이션 절차

#### 8.1.1 전제: 위젯 텍스트의 마스터 언어를 "한국어"로 고정

현재 대부분의 위젯이 한국어 텍스트로 만들어져 있다. 이를 영어로 옮기지 않고, **한국어를 마스터(원문, source)** 로 두고 영어 번역만 추가한다.

> 이유: ① 콘텐츠 재입력 비용 절감 ② 디자이너가 친숙한 언어 유지 ③ 한국 출시가 우선순위가 높을 가능성

#### 8.1.2 위젯 1개당 처리 체크리스트 (반복 절차)

각 WBP_*.uasset 파일에 대해 동일하게 수행:

1. **에디터에서 위젯을 연다** (UMG Designer)
2. **Hierarchy 패널**에서 모든 `TextBlock` / `RichTextBlock`을 순회.
3. 각 텍스트 위젯 선택 → Details 패널에서:
   - **Content → Text** 항목의 우측 화살표(▼) 클릭 → **"Make Localizable"** 또는 *(이미 Localizable이면 그대로)*
   - **Localization Source**가 `Inline` (위젯에 박힌 텍스트)인지 확인
   - **Key**(자동 생성된 GUID 또는 수동 키)를 **의미 있는 이름으로 수동 지정**할 것을 권장
     - 예: `WBP_LobbyOverlay::TitleText` → 키 `Lobby_Title`
     - GUID 키는 위젯 복사/붙여넣기 시 충돌하거나 추적이 어려움
   - **Namespace**는 위젯 단위로 통일 (예: `WBP_LobbyOverlay`)
4. **체크박스 점검**: 텍스트가 `Localizable` 상태가 아니면 Gather에서 누락된다. UMG의 "Text" 옆 메뉴에서 "Make Localizable" 미확인 시 비활성 상태일 수 있음.
5. **빈 텍스트 / 런타임에 SetText로 채우는 텍스트**는 그대로 둔다 (코드에서 SetText 시 LOCTEXT 사용 여부가 중요).
6. 저장 → 다음 위젯으로 이동.

#### 8.1.3 위젯 단위 처리 우선순위 목록

조사 결과를 기반으로 **반드시 점검해야 할 위젯**:

**(1순위 — 게임 중 항상 보이는 오버레이)**
- `Content/Blueprints/UI/Overlay/WBP_LobbyOverlay.uasset`
- `Content/Blueprints/UI/Overlay/WBP_StageOverlay.uasset`
- `Content/Blueprints/UI/Overlay/WBP_TutorialOverlay.uasset`
- `Content/Blueprints/UI/Overlay/WBP_WaitingRoomOverlay.uasset`
- `Content/Blueprints/UI/Overlay/WBP_GameClear.uasset`
- `Content/Blueprints/UI/Overlay/WBP_GameOver.uasset`

**(2순위 — 메뉴/설정 화면)**
- `Content/Blueprints/UI/Settings/WBP_SettingsScreen.uasset`
- `Content/Blueprints/UI/Settings/SettingsPage/WBP_SettingsPage_Graphics.uasset`
- `Content/Blueprints/UI/Settings/SettingsPage/WBP_SettingsPage_Gameplay.uasset`
- `Content/Blueprints/UI/Settings/SettingsPage/WBP_SettingsPage_Audio.uasset`
- `Content/Blueprints/UI/Settings/SettingsPage/WBP_SettingsPage_Controls.uasset`
- `Content/Blueprints/UI/Settings/Contents/SettingRow/WBP_SettingRow_*.uasset`

**(3순위 — 인게임 컴포넌트 위젯)**
- `Content/Blueprints/UI/Container/WBP_HealthWaterBar.uasset`
- `Content/Blueprints/UI/Slot/WBP_PlayerSlot.uasset`
- `Content/Blueprints/UI/TextUI/WBP_PhaseObjective.uasset`
- `Content/Blueprints/UI/SkillIcon/WBP_SkillIcon_Tutorial.uasset`
- `Content/Blueprints/UI/SkillIcon/WBP_SkillSlot_Tutorial1/2/3.uasset`

#### 8.1.4 위젯 직접 입력 텍스트의 "흔한 함정" 모음

| 함정 | 증상 | 해결 |
|------|------|------|
| `FString → FText` 노드로 강제 변환한 위치 | 변환된 텍스트는 Localizable이 아님 | 블루프린트에서 `FString to Text` 노드 제거, 직접 `FText` 변수 사용 |
| `Append` 노드로 문자열 합성 | "Phase " + 1 + " 시작" 같은 합성은 어순 변경 불가 | `Format Text` 노드(Format Text)로 교체. {Phase} 토큰 사용 |
| BindWidget으로 텍스트를 받는데 외부에서 `FString::Printf`로 만든 값 전달 | 코드에서 만든 시점에 이미 한국어로 박힘 | 코드 측에서 `FText::Format(LOCTEXT(...))` 사용 (STEP 8 참조) |
| 디자이너가 임시 텍스트 ("test", "asdf")를 그대로 두고 SetText로 덮어씀 | Gather 결과물에 의미없는 키가 누적 | 디자인 시점에 원문(한국어) 마스터 텍스트로 채워두기. SetText는 변수만 |
| 동일 텍스트가 여러 위젯에 중복 입력 | 영어 번역 시 일관성 깨짐 (한 곳은 "Apply", 다른 곳은 "Confirm") | **공유 텍스트 라이브러리** (8.1.5) 도입 |

#### 8.1.5 공유 텍스트 라이브러리 (선택, 中규모 이상 권장)

여러 위젯에서 반복되는 텍스트("확인", "취소", "닫기", "적용" 등)는 **C++ 헬퍼 클래스 `UDRCommonTexts`** 에 LOCTEXT로 모아두는 패턴을 추천.

```cpp
// Source/DaeRune/Public/UI/DRCommonTexts.h
#pragma once
#include "CoreMinimal.h"
#include "Internationalization/Text.h"

class DAERUNE_API UDRCommonTexts
{
public:
    static FText Confirm();   // LOCTEXT("Common_Confirm", "확인")
    static FText Cancel();    // LOCTEXT("Common_Cancel", "취소")
    static FText Apply();     // LOCTEXT("Common_Apply", "적용")
    static FText Close();     // LOCTEXT("Common_Close", "닫기")
    static FText Back();      // LOCTEXT("Common_Back", "뒤로")
    static FText Yes();
    static FText No();
};
```
- 위젯에서 직접 입력 대신 `BlueprintCallable` 함수로 노출 → "Get Common Confirm Text" 같은 노드를 위젯에서 호출.
- 한국어/영어 번역 일관성 자동 확보.

---

### 8.2 (B) DataTable에 저장된 텍스트 — 마이그레이션 절차

#### 8.2.1 현재 DaeRune의 DataTable 텍스트 인벤토리 (확정)

| DataTable | 경로 | Row 구조체 | 텍스트 필드 | 현재 입력 언어 | 마이그레이션 필요 |
|-----------|------|-----------|------------|---------------|----------------|
| **DT_PhaseObjective** | `Content/Blueprints/Phase/Data/DT_PhaseObjective.uasset` | `FPhaseObjectiveData` | `FText ObjectiveTitle`, `FText ProgressFormat` | 한국어 | **YES** |
| DT_WaveData | `Content/Blueprints/Phase/Data/DT_WaveData.uasset` | `FWaveDataRow` | (없음, float만) | — | NO |
| DT_WaveLevelData | `Content/Blueprints/Phase/Data/DT_WaveLevelData.uasset` | `FWaveLevelModifierRow` | (없음, float/bool만) | — | NO |
| DA_EnemyCharacterClassInfo | `Content/Blueprints/AbilitySystem/Data/DA_EnemyCharacterClassInfo.uasset` | `UCharacterClassInfo` (DataAsset) | 표시 이름이 있다면 | 검사 필요 | 검사 후 결정 |
| Status Effect Info | `FEffectInfo` 구조체 | `FText EffectName` | 한국어 추정 | DataAsset | YES (해당 시) |

> **핵심**: 현재 DaeRune의 DataTable 중 **실제로 UI에 노출되는 텍스트를 가진 것은 사실상 `DT_PhaseObjective` 하나**다. 그 외 텍스트는 코드 또는 위젯에 박혀있다.

#### 8.2.2 `DT_PhaseObjective` 마이그레이션 절차 (모범 사례)

**(Step 1) Row 구조체 점검** — 이미 `FText`이므로 OK
```cpp
// Source/DaeRune/Public/Phase/DRPhaseBase.h
USTRUCT(BlueprintType)
struct FPhaseObjectiveData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText ObjectiveTitle;        // ← 이미 FText. 그대로 사용 가능

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText ProgressFormat;        // ← 이미 FText

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PhaseNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RequiredCount = 0;
};
```

**(Step 2) DataTable 행의 FText 키 안정화**
- 에디터에서 `DT_PhaseObjective` 열기 → 각 행(Phase1, Phase2, Phase3)의 `ObjectiveTitle`/`ProgressFormat` 셀을 클릭
- **FText의 Inspect 옵션**에서 *Key* / *Namespace* 확인. 자동 생성된 GUID 키가 있을 것.
- **Namespace**를 통일된 이름으로 수동 지정 (예: `DT_PhaseObjective`)
- **Key**를 의미 기반으로 지정:
  - Phase1.ObjectiveTitle → 키 `Phase1_Title`
  - Phase1.ProgressFormat → 키 `Phase1_Progress`
  - Phase2.ObjectiveTitle → 키 `Phase2_Title`
  - …
- 키를 안정화하지 않으면 **셀 텍스트를 수정할 때마다 새 키가 생성**되어 영어 번역과 자동 매칭이 끊긴다. 이게 DataTable 다국어화의 가장 큰 함정.

**(Step 3) Format 토큰 사용 확인**
- `ProgressFormat`이 `"클렌저 {Current}/{Required}개 확보"` 같은 토큰 형태인지 확인.
- C++ 측 사용 코드(`OverlayWidgetController::HandlePhaseObjectiveChanged`)에서 `FText::FormatNamed` 또는 `FText::Format`으로 토큰 치환하는지 확인.
- 만약 `FString::Printf("%s %d/%d", ...)` 형태로 합성한다면 → **(Step 4)** 에서 수정.

**(Step 4) 코드 측 포맷 처리 정리**
```cpp
// Before (위험: 어순/포맷 고정)
FString::Printf(TEXT("%s %d/%d"),
    *ObjectiveData.ProgressFormat.ToString(), Current, Required);

// After (안전)
FFormatNamedArguments Args;
Args.Add(TEXT("Current"), Current);
Args.Add(TEXT("Required"), Required);
FText FinalText = FText::Format(ObjectiveData.ProgressFormat, Args);
```
- 이렇게 하면 영어 번역에서 `"Secured {Current} of {Required} cleansers"`처럼 어순을 바꿀 수 있다.

**(Step 5) Localization Dashboard에서 Gather**
- `Content/Blueprints/Phase/Data` 경로를 Gather Settings의 **Gather from Packages** 대상에 포함.
- Gather 실행 → `.archive`에 `DT_PhaseObjective::Phase1_Title` 등의 키가 노출됨.
- 영어 번역을 `.archive`에 입력 → Compile → `.locres` 갱신.

**(Step 6) 검증**
- 게임 실행 → 한국어로 Phase 1 진입 → 정상 표시 확인.
- 설정에서 영어로 변경 → 동일 Phase 진입 → 영어 텍스트로 자동 표시되는지 확인.

#### 8.2.3 새 텍스트 DataTable을 만들 때의 가이드라인

향후 **대화/팁/툴팁/설명** 등의 텍스트를 DataTable에 저장하고 싶을 때:

1. Row 구조체는 항상 `FText` 사용 (절대 `FString` 금지)
2. Row 이름(RowName)은 **영문 식별자**로 (`Tutorial_Step01`, `Tip_Cleanser`). 한글 RowName은 키 안정성 떨어짐.
3. Row 구조체에 텍스트가 여러 개면 각 필드의 의미를 명확히 (`Title`, `Description`, `Hint` 식)
4. DataTable Asset 자체에 **언어 버전을 두지 말 것**. 한국어 마스터 1개만 두고 Localization으로 처리.
5. DataTable의 각 행을 추가/수정 후, Localization Dashboard에서 **Gather → Compile** 워크플로우를 반드시 다시 실행.

#### 8.2.4 DataAsset(`UPrimaryDataAsset` / `UDataAsset`)의 텍스트 처리

`DA_EnemyCharacterClassInfo` 같은 DataAsset의 `FText` 필드도 동일 원리:
- DataAsset에 직접 입력된 `FText`는 **에셋 단위로 Localization Gather 대상**
- Gather 시 `Content/Blueprints/AbilitySystem/Data` 경로 포함
- 단, DataAsset은 DataTable과 달리 행이 없으므로 키 안정성 문제는 덜 함

#### 8.2.5 DataTable 마이그레이션의 체크리스트

- [ ] 모든 텍스트 컬럼이 `FText` 타입인지 확인 (`FString`이면 변환)
- [ ] 각 셀의 FText에 **Namespace/Key 수동 지정** (자동 GUID 키 회피)
- [ ] 포맷 토큰을 사용한다면 `{TokenName}` 형식으로 통일
- [ ] 코드 측 사용처가 `FText::Format` 또는 `FText::FormatNamed`을 쓰는지 확인
- [ ] DataTable 경로가 Localization Dashboard의 Gather Path에 포함됨
- [ ] Gather 결과물(`.archive`)에 해당 행의 키가 표시되는지 확인
- [ ] 영어 번역 입력 후 Compile → 게임에서 영어 표시 확인

---

### 8.3 (C) C++ `FText::FromString` 잔존분 — 별도 정리

8.1·8.2와는 별개로, **`FText::FromString(TEXT("..."))` 패턴은 다국어 처리에서 가장 위험**하다 (Gather 대상이 아니므로 영어 번역으로 절대 갈아끼울 수 없다).

#### 8.3.1 확인된 최우선 수정 위치

| 파일 | 라인 | 현재 코드 (요약) | 변환 후 |
|------|------|----------------|---------|
| `OverlayWidgetController.cpp` | `OnPhaseChanged()` switch | `FText::FromString(TEXT("Phase 1: Secure"))` 등 4건 | `LOCTEXT("Phase_1_Alarm", "Phase 1: 확보")` 등 |
| `DRStageGameState.cpp` | 43~44 | `FText::FromString("Preparing...")`, `FText::FromString("Progress")` | `LOCTEXT("Phase_PreparingTitle", "준비 중...")`, `LOCTEXT("Phase_DefaultProgress", "진행도")` |
| `DRSettingsManager.cpp` | 295~507 다수 | `FText::FromString(TEXT("LANGUAGE"))` 등 모든 라벨/옵션 | 각 라벨/옵션 → 개별 LOCTEXT 키 |
| `DROverheadWidget.cpp` | 해당 부분 | 머리 위 표시 텍스트 | LOCTEXT |

#### 8.3.2 변환 패턴

```cpp
// 파일 상단
#define LOCTEXT_NAMESPACE "DRPhase"

// 본문
case 0: PhaseText = LOCTEXT("Phase_Alarm_1", "Phase 1: 확보"); break;
case 1: PhaseText = LOCTEXT("Phase_Alarm_2", "Phase 2: 수집"); break;
case 2: PhaseText = LOCTEXT("Phase_Alarm_3", "Phase 3: 방어"); break;
default: PhaseText = LOCTEXT("Phase_Alarm_Unknown", "알 수 없는 페이즈"); break;

// 파일 하단
#undef LOCTEXT_NAMESPACE
```

#### 8.3.3 검증 그렙

작업 완료 후 다음 검색이 **0건**이어야 한다 (UI에 보이는 텍스트 한정):
```
FText::FromString(TEXT(
FText::FromString(FString::Printf
```
> 단, 디버그 로그/플레이어 이름 등 **번역 대상이 아닌 동적 문자열**(예: 사용자 입력, 네트워크에서 받은 닉네임)은 예외이므로 grep 결과를 사람이 한 번 검토해야 한다.

---

### 8.4 (D) `DRTutorialManager.cpp`의 NSLOCTEXT 정리

이미 NSLOCTEXT 매크로를 쓰고 있어 Gather 대상이지만:

1. **네임스페이스 일관성 확인** — 모든 NSLOCTEXT가 동일 네임스페이스(`"DRTutorial"`)를 사용하는지 검토
2. **키 명명 규칙** — `Tutorial_Parkour_Objective`, `Tutorial_WaterPump_Objective` 식으로 의미 기반 키 부여
3. **하드코딩 라벨 분리** — `"초"`, `"적중"`, `"동시 적중"` 같은 단위 라벨은 `UDRCommonTexts::Seconds()` 등으로 추출 후 재사용
4. **Format 토큰 사용** — 현재 코드가 `"%d초"` 같은 패턴이라면 `LOCTEXT("Tutorial_TimeLeft", "{Seconds}초")` + `FText::Format`으로 교체

---

### 8.5 텍스트 마이그레이션 진행 순서 (작업 흐름 요약)

이 섹션의 작업을 실제로 진행할 때의 권장 순서:

1. **인벤토리 PR**: 8.0 표를 그대로 코드 주석 또는 별도 `LocalizationInventory.md` 파일로 정착 (트래킹용)
2. **위젯 1순위 일괄 점검 PR**: 8.1.3의 1순위 위젯 6개에 대해 모든 TextBlock의 Localizable + Namespace 통일 작업
3. **DataTable PR**: `DT_PhaseObjective` 키 안정화 + 코드 측 `FText::Format` 정리 (8.2.2)
4. **C++ FText::FromString → LOCTEXT 일괄 PR**: 8.3.1 표의 4파일 처리
5. **NSLOCTEXT 정리 PR**: 8.4
6. **위젯 2~3순위 일괄 점검 PR**: 8.1.3의 나머지 위젯
7. **공유 텍스트 라이브러리 도입 PR**: 8.1.5 (선택)
8. **Localization Dashboard Gather/Compile 정착**: STEP 6의 워크플로우 자동화 (가능하면 빌드 파이프라인에 commandlet 추가)

각 PR은 가능한 한 작은 단위로 쪼개어 리뷰 부담을 낮춘다 — 위젯 점검 PR은 1~2개 위젯씩, 코드 LOCTEXT 변환은 파일 1개씩.

---

### 8.6 텍스트 마이그레이션 체크리스트 (이 섹션 전용)

- [ ] 텍스트 소스 인벤토리(8.0) 문서화 완료
- [ ] 위젯 직접 입력 텍스트 모두 Localizable=true + 의미 기반 Key
- [ ] 동일 표현이 다중 위젯에서 등장 시 공유 텍스트로 통합 (선택)
- [ ] `DT_PhaseObjective`의 모든 셀에 Namespace/Key 수동 지정
- [ ] DataTable의 Format 사용처가 `FText::Format` 으로 변환됨
- [ ] `FText::FromString` 잔존분 0건 (UI 텍스트 한정)
- [ ] NSLOCTEXT 네임스페이스/키 명명 규칙 일관화
- [ ] 새 텍스트 DataTable 추가 시 가이드라인(8.2.3) 준수
- [ ] 위젯 1·2·3순위 모두 처리 완료
- [ ] 텍스처화된 텍스트(이미지 안에 그려진 한국어)가 있는지 최종 점검 → 있으면 별도 처리 또는 명시적으로 보류 결정

---

## 9. 요약 체크리스트

- [ ] STEP 1: Chinese 옵션 코드 제거 + 폴백 처리
- [ ] STEP 2: `UDRGameUserSettings::PreferredCulture` 추가, OptionId↔Culture 매핑 함수 정리
- [ ] STEP 3: `ApplySingleSetting`의 Language 분기 구현, `OnLanguageChanged` 델리게이트 추가
- [ ] STEP 4: `DefaultGame.ini`에 `+CulturesToStage=ko` 추가
- [ ] STEP 5: 모든 `FText::FromString` → `LOCTEXT`, 위젯 텍스트 Localizable 점검
- [ ] STEP 6: Localization Dashboard에 `Game` 타깃 + ko/en 컬처 + Gather/Compile
- [ ] STEP 7: 텍스트성 DataTable 컬럼 `FText`로 정리
- [ ] STEP 8: 동적 포맷 문자열 → `FText::Format`
- [ ] STEP 9: `OnLanguageChanged` 구독 → 활성 위젯 텍스트 갱신
- [ ] STEP 10: QA 시나리오 9건 통과

---

## 10. 에디터 작업 완전 가이드 — C++ 이후, 실제 번역을 화면에 띄우기

> **작성일 2026-07-24. 코드 실측 기반.** 이 섹션은 §1~§9 작성 이후 **실제로 구현이 진행된 상태**를 전제로 하며, 앞 섹션과 충돌하면 이 섹션이 우선한다.
> §8이 "무엇을·왜"(개념적 마이그레이션)를 다뤘다면, 이 섹션은 **"에디터에서 실제로 어떤 버튼을 누르는가"**(구체 액션) + **"C++로는 못 쓰고 에디터에서만 작성되는 UI 텍스트를 어떻게 처리하는가"**를 다룬다.

### 10.0 전제 — 지금까지 코드 측에서 완료된 것

| 항목 | 상태 | 위치 |
|---|---|---|
| 언어 옵션(한/영) 복구 + 확장 SSOT | ✅ 완료 | `UDRSettingsManager::GetSupportedLanguages()` — 언어 추가는 이 배열 1줄 |
| 영속화(`PreferredCulture`) + 부팅 적용 | ✅ 완료 | `DRGameUserSettings`, `DRGameInstance::Init` |
| 컬처 전환 + `OnLanguageChanged` 발행 | ✅ 완료 | `UDRSettingsManager::ApplySingleSetting` |
| Config (ko/en 스테이징 + `LocalizationPaths`) | ✅ 완료 | `DefaultGame.ini`, `DefaultEngine.ini` |
| C++ 텍스트 `LOCTEXT`화 | ✅ 완료 | `OverlayWidgetController`(ns `DROverlay`), `DRStageGameState`(ns `DRPhase`), `DRSettingsManager`(ns `DRSettings`) |
| 소스 인코딩 정규화(UTF-8 BOM) | ✅ 완료 | 위 파일들 |
| 언어 변경 라이브 갱신 배선 | ✅ 완료 | `UDRUserWidget`이 `OnLanguageChanged` 구독 → 자식 Text 재동기화 + BP 이벤트 |

→ **코드에 있는 텍스트는 이미 "번역 준비 완료".** 그런데도 화면엔 한국어만 나온다. 남은 두 가지 때문이며, **둘 다 C++이 아니라 에디터에서만** 처리된다:

1. **번역 데이터(`.locres`)가 없다** — Gather/Compile을 한 번도 안 했다(`Content/Localization` 폴더 부재).
2. **에디터에 박힌 위젯/DataTable 텍스트가 수집 대상인지 미확인** — 디자이너가 직접 입력한 텍스트는 코드로 못 바꾸고 에디터에서 처리해야 한다.

이 섹션은 위 1·2를 끝내는 방법이다.

---

### 10.1 큰 그림 — 텍스트는 "두 세계"에서 나와 한 곳으로 모인다

DaeRune의 UI 텍스트는 원천이 둘이다. **둘 다 최종적으로는 같은 `.archive`(번역본) → `.locres`(컴파일 결과)로 모인다.** 언어별로 에셋을 복제하지 않는다.

```
[세계 A] C++ 코드의 LOCTEXT  ──┐
                              ├──►  Localization Dashboard "Gather"  ──►  Game.archive(ko/en)
[세계 B] 에셋 내부 FText     ──┘        (ko=원문, en=번역 입력)
   - WBP 디자이너에 입력한 TextBlock                                          │
   - DataTable / DataAsset의 FText 컬럼                                     "Compile"
                                                                             ▼
                                                       Content/Localization/Game/{ko,en}/Game.locres
                                                                             │
                                              런타임: SetCurrentLanguageAndLocale("en") → .locres 조회
```

- **세계 A(코드)** 는 §5·§8.3에서 이미 처리됨(LOCTEXT). Gather 시 소스 파일(`*.cpp/*.h`)에서 자동 추출.
- **세계 B(에셋)** 가 이 섹션의 핵심. **에디터에서 "Localizable"로 표시된 FText만** Gather 시 패키지에서 추출된다.

즉 C++을 아무리 LOCTEXT화해도, **WBP에 직접 입력한 한국어는 에디터에서 별도로 "수집 가능" 상태로 만들지 않으면 영원히 번역되지 않는다.**

---

### 10.2 [에디터 작업 1] Localization Dashboard 타깃 생성 (최초 1회, 되돌아올 필요 없음)

**목적**: `Game` 로컬리제이션 타깃을 만들어 Gather/Compile 파이프라인을 성립시키고 `Content/Localization/Game/...` 산출물을 생성.

1. 에디터 상단 **Tools → Localization Dashboard** 실행. (메뉴에 안 보이면 Tools 메뉴에서 "Localization" 검색)
2. **New Target** 클릭 → 이름 `Game` (관례. `+LocalizationTargets=Game`, `LocalizationPaths=.../Game` 와 반드시 일치).
3. **Cultures 섹션**:
   - **Native Culture = Korean (`ko`)** 로 지정. ← 우리 마스터(원문)가 한국어이기 때문. 이게 핵심 결정이며, 이후 모든 LOCTEXT/위젯의 소스 문자열이 "ko 원문"으로 취급된다.
   - **Add New Culture → English (`en`)** 추가. (번역 대상)
4. **Gather Text 설정 — Gather from Text Files** (세계 A: C++):
   - Search Directories: `Source/DaeRune`
   - File Extensions(Wildcards): `*.cpp`, `*.h`
5. **Gather Text 설정 — Gather from Packages** (세계 B: 에셋):
   - Include Paths(예): `Content/Blueprints/UI`, `Content/Blueprints/Phase/Data`, `Content/Blueprints/AbilitySystem/Data`
   - (위젯·DataTable·DataAsset이 있는 디렉토리를 모두 포함. 넓게 `Content/Blueprints` 로 잡아도 됨 — 텍스트 없는 에셋은 그냥 안 걸림)
6. 상단 **Gather Text** 실행 → 각 컬처의 `Game.archive` 와 `Game.manifest` 생성.
7. `en` 행의 **연필(Edit translations)** 아이콘 → 수집된 각 원문(한국어)에 대응하는 **영어 번역 입력**. (개발 초기엔 임시로 `[en] 원문` 식으로 채워 파이프라인만 검증해도 됨)
8. 상단 **Compile Text** → `Game.locres` 생성.
9. **산출물 확인**: `Content/Localization/Game/ko/Game.archive`, `Content/Localization/Game/en/Game.archive`, 각 컬처 `Game.locres`, 루트 `Game.manifest`, `Game.locmeta`.
10. 에디터 재생성 없이 바로 **PIE에서 설정 → 언어 → English** 선택 시, LOCTEXT/수집된 위젯 텍스트가 영어로 바뀌어야 한다.

> **주의**: Dashboard가 최초 Gather 시 `Config/Localization/Game*.ini`(gather/import/export/compile 스텝 설정)를 자동 생성한다. 이 파일들이 있어야 §10.10의 커맨드릿 자동화가 가능하다. → **최초 1회는 Dashboard로 만드는 것이 가장 안전.**

---

### 10.3 [에디터 작업 2] 위젯(WBP) 텍스트를 "수집 가능"하게 만들기 — 이 계획의 실질 최대 작업

이게 **"C++로는 못 쓰고 에디터에서만 되는 UI 텍스트"** 처리의 핵심이다.

#### 10.3.1 원리 — 왜 그냥은 안 걸리나

- UMG 디자이너에 직접 타이핑한 TextBlock 문자열은 **기본적으로 Localizable**이라 Gather 대상이다. 다만 다음이면 **누락**된다:
  - 텍스트가 **Culture Invariant(문화 불변)** 로 표시됨 → 수집 제외.
  - 텍스트를 **Blueprint의 `ToText(FString)` / `Append` 노드로 런타임 합성** → 그 시점엔 이미 문자열이라 수집 불가(세계 B가 아니라 "런타임 동적"이 됨. §8.1.4 함정 표 참조).
  - 텍스트를 **C++/BP에서 `SetText`로 덮어씀** → 디자이너 값은 안 걸리고, 넣는 쪽(코드)의 LOCTEXT 여부가 관건.
- 또한 자동 생성 **Key가 GUID** 라, 위젯 복붙/텍스트 수정 시 키가 바뀌어 **영어 번역 매칭이 끊긴다**(DataTable과 동일한 함정).

#### 10.3.2 위젯 1개당 반복 절차

각 `WBP_*` 에 대해:
1. UMG 디자이너로 연다.
2. **Hierarchy**에서 모든 `TextBlock` / `RichTextBlock`을 순회.
3. 각 텍스트 위젯 선택 → **Details → Content → Text** 행의 우측 **아래 화살표(▼)** 클릭:
   - **"Make Localizable"** 이 보이면 누른다(현재 Culture Invariant 상태라는 뜻).
   - 이미 Localizable이면 **Namespace / Key / Source string** 이 보인다.
4. **Key를 의미 기반으로 수동 지정** 권장(GUID 회피). 예: `WBP_LobbyOverlay`의 제목 → Namespace `WBP_LobbyOverlay`, Key `Title`.
5. **런타임에 `SetText`로 채우는 빈/임시 텍스트**는 디자이너 값은 그냥 두고(또는 한국어 원문으로 채워두고), **넣는 코드/BP 쪽**에서 LOCTEXT를 쓰는지 확인(§10.7 (D) 참조).
6. 저장 → 다음 위젯.

#### 10.3.3 ★ D(라이브 갱신)와의 연결 — 위젯 부모 클래스가 관건

이번에 `UDRUserWidget`에 **언어 변경 자동 갱신**을 심었다. 이 혜택을 받으려면:

| 위젯의 부모 클래스 | 언어 변경 시 동작 | 작업 |
|---|---|---|
| **`UDRUserWidget` 파생** (대부분의 오버레이) | 디자이너에 박힌 정적 텍스트는 **자동 갱신**(내부에서 자식 TextBlock `SynchronizeProperties` 재호출) | 없음 — 단, 동적 `SetText` 텍스트는 아래 |
| `UDRUserWidget` 파생인데 **동적 SetText** 사용 | 자동 갱신 안 됨(코드가 만든 값이라) | WBP에서 **`On Language Changed` 이벤트**(BlueprintImplementableEvent) 구현 → 그 안에서 SetText 재실행 |
| **plain `UUserWidget` 파생**(DRUserWidget 미상속) | 아무 일도 안 일어남 | **부모를 `DRUserWidget`으로 Reparent**(File → Reparent Blueprint) 하거나, 개별로 SettingsManager의 `OnLanguageChanged`에 바인딩 |

> 실무 규칙: **텍스트가 있는 WBP는 되도록 `UDRUserWidget`을 상속**시킨다. 그러면 정적 텍스트는 공짜로 즉시 갱신되고, 동적 텍스트만 `On Language Changed` 이벤트에서 처리하면 된다.

#### 10.3.4 현재 프로젝트 위젯 전수 인벤토리 (실측 2026-07-24 — §8.1.3보다 완전판)

**우선 1순위 — 항상/자주 보이는 텍스트 위젯 (반드시 점검)**
- Overlay: `WBP_LobbyOverlay`, `WBP_StageOverlay`, `WBP_TutorialOverlay`, `WBP_WaitingRoomOverlay`, `WBP_GameClear`, `WBP_GameOver`, `WBP_TutorialClear`, `WBP_TutorialStartMenu`
- TextUI(상호작용/알람 — 다수 한국어 하드코딩 예상): `WBP_CleanserSiteInteraction`, `WBP_PartInteraction`, `WBP_PickupCleanserPart`, `WBP_PhaseAlarm`, `WBP_PhaseObjective`, `WBP_ElectricAlarm`, `WBP_Phase3Timer`, `WBP_MountInteraction`, `WBP_StageSelectInteraction`, `WBP_RoomCode`

**2순위 — 메뉴/설정 (일부는 C++ 라벨 사용 → 그건 이미 LOCTEXT됨)**
- Settings: `WBP_SettingsScreen`, `WBP_SettingsPage_Graphics/Gameplay/Audio/Controls`, `WBP_SettingRow_Dropdown/Slider/Toggle`, `WBP_SettingsTabButton`, `WBP_Tutorial`
- Settings 하위 컴포넌트: `WBP_SciFiDropdown/Option/OptionList`, `WBP_KeyHint / WBP_KeyHintBar / WBP_KeyHint_Wide`(키 힌트 라벨), `WBP_SciFiSlider/Toggle/ScrollBar`

**3순위 — 인게임 컴포넌트 (텍스트 있는 것만)**
- CharacterInfo(설명문 가능성 큼): `WBP_GardenInfo`, `WBP_VendingInfo`
- Loading(팁/문구 가능성): `WBP_CreateRoomLoading`, `WBP_InRoomLoading`, `WBP_StageLoading`, `WBP_TutorialLoading`
- Slot: `WBP_PlayerSlot`
- SkillIcon(툴팁/이름 가능성): `WBP_SkillIcon_GardenRobot/VendingMachine/Tutorial`, `WBP_SkillSlot`, `WBP_SkillSlot_Tutorial1/2/3`

**점검 대상 아님(비주얼/동적) — 확인만 하고 스킵**
- 커서(`WBP_Cursor_*`, `WBP_CursorGrab`), 미니맵(`WBP_Minimap`, `WBP_MinimapIcon`), 체력/물 바·아이콘(`WBP_Container*`, `WBP_HealthWaterBar`, `WBP_ProgressBar`, `WBP_*HealthBar`), 힐 이펙트(`WBP_Heal*`, `WBP_DebuffBleed`)
- **동적 숫자/이름**: `WBP_DamageText`(대미지 숫자), `WBP_OverheadWidget`(머리 위 — 코드에서 `SetText(FromString)`, 번역 예외)

> **레벨업 UI 주의**: 최근 "레벨업 기능 틀 구현" 커밋 이후 생기는 XP/레벨 결과 표시 위젯이 있으면 이 인벤토리에 추가로 편입할 것. §8 작성 시점엔 없던 항목이다.

---

### 10.4 [에디터 작업 3] DataTable / DataAsset의 FText 텍스트

세계 B의 나머지. 상세 절차는 **§8.2를 그대로 따르되**, 에디터 액션만 요약:

- **`DT_PhaseObjective`** (`Content/Blueprints/Phase/Data`) — Row 구조체 `FPhaseObjectiveData`의 `ObjectiveTitle`/`ProgressFormat`는 **이미 `FText`**(코드 확인 완료). 에디터에서 각 행 셀의 FText에 **Namespace/Key 수동 지정**(자동 GUID 회피, §8.2.2) 후, Gather Path에 `Content/Blueprints/Phase/Data` 포함.
- **DataAsset**(예: `DA_EnemyCharacterClassInfo`, 상태이상 `FEffectInfo::EffectName`) — FText 필드면 자동 수집 대상. Gather Path에 해당 폴더 포함.
- **Format 토큰**: `"{Current}/{Required}"` 형태 + 코드 측 `FText::Format`/`FormatNamed` 사용 확인(§8.2.2 Step 3~4).

---

### 10.5 [에디터 작업 4] Gather → 번역 → Compile 워크플로우 (반복)

1. **Gather Text** (Dashboard 상단 버튼) — 코드 LOCTEXT + Localizable 위젯/DataTable을 긁어 `.archive` 갱신.
2. `en` **Edit translations** — 새로 걸린 원문(ko)에 영어 입력.
3. **Compile Text** — `.locres` 재생성.
4. PIE에서 언어 전환으로 확인.

> 새 텍스트를 추가할 때마다 **1→3을 다시** 돌려야 반영된다(§10.9).

---

### 10.6 [에디터 작업 5] 폰트 / Composite Font (글자 깨짐 방지)

- 마스터가 한국어이므로 **UI 기본 폰트는 한글 글리프를 포함**해야 한다(대개 이미 그럼).
- 영어로 전환 시엔 Latin 글리프만 있으면 되므로 대개 문제 없음. 반대로 **영문 위주 폰트를 한글에 쓰면 `□`(두부) 발생** → 해당 폰트를 **Composite Font**로 만들어 한글 fallback(예: Noto Sans KR)을 등록.
- 숫자/기호(`%`, 해상도 `1920 X 1080`)는 언어 무관이라 무시.

---

### 10.7 ★ 사용자 질문 직접 답변 — "에디터에서 작성되는 UI 텍스트"는 유형별로 이렇게

| 유형 | 예시 | 처리 방법 | 언어 변경 즉시 반영 |
|---|---|---|---|
| **(A) WBP 디자이너 정적 텍스트** | `WBP_StageOverlay`에 타이핑한 "웨이브 시작" | 에디터에서 **Localizable 확인 + Key 안정화**(§10.3) → Gather에 걸림 | `UDRUserWidget` 파생이면 **자동**(D) |
| **(B) DataTable/DataAsset FText** | `DT_PhaseObjective.ObjectiveTitle` | **FText 유지 + Key/Namespace 지정**(§10.4) → Gather에 걸림. 값 표시하는 위젯이 재조회하도록 | 위젯이 `On Language Changed`에서 재세팅 필요할 수 있음 |
| **(C) 위젯이 코드/BP에서 `SetText`로 채우는 동적 텍스트** | "%d초 남음", 플레이어가 넣은 값 | **넣는 쪽에서 `LOCTEXT`+`FText::Format`** 사용(세계 A로 편입). 순수 사용자 입력/닉네임은 번역 예외 | WBP `On Language Changed`에서 SetText 재실행 |
| **(D) 이미지(텍스처)에 그려진 글자** | 텍스트가 합쳐진 아이콘 | 언어 비의존 아이콘이면 그대로. 아니면 컬처별 텍스처(§5-D). **후순위** | 해당 없음 |

**핵심 원칙(3줄 요약)**
1. 에디터에서 작성한 텍스트는 **에셋에 그대로 두고(마스터=한국어)**, 번역은 `.archive`에만 넣는다 — **언어별 에셋 복제 금지.**
2. 걸리게 하려면 **"Localizable"** 여야 한다(동적 합성/culture-invariant면 안 걸림).
3. 즉시 갱신을 받으려면 위젯이 **`UDRUserWidget` 파생**이어야 하고, 동적 텍스트는 **`On Language Changed` 이벤트**에서 다시 세팅한다.

---

### 10.8 검증 시나리오 — 한→영 전환 시 무엇이 바뀌어야 하나

| # | 확인 지점 | 기대 |
|---|---|---|
| 1 | 설정 → 언어 드롭다운 | **한국어 / English** 두 항목 |
| 2 | English 선택 → 설정 화면 라벨 | "디스플레이 모드"→"Display Mode" 등 (C++ LOCTEXT, `.archive`에 en 입력 시) 즉시 변경 |
| 3 | 페이즈 진입 알람 | "페이즈 1: 확보" → 영어 (OverlayWidgetController LOCTEXT) |
| 4 | 페이즈 목표(HUD) | `DT_PhaseObjective` 번역본으로 |
| 5 | TextUI 상호작용 프롬프트 | 위젯 Localizable 처리한 것만 영어로 |
| 6 | 재시작 후 | English 유지(`PreferredCulture=en`) |
| 7 | 인게임 중 전환 | `UDRUserWidget` 파생 위젯 즉시 갱신, 비파생은 재오픈 시 |
| 8 | Gather 안 한 신규 텍스트 | 마스터(한국어)로 표시, 크래시 없음 |

---

### 10.9 새 텍스트 추가 시 반복 워크플로우 (팀 규칙)

1. **코드 텍스트**: `LOCTEXT("Ns_Key", "한국어 원문")` 로 작성(절대 `FText::FromString` 금지, 동적/닉네임 제외).
2. **위젯 텍스트**: 디자이너에 한국어로 입력 + **Localizable 확인** + 의미 기반 Key. 되도록 부모를 `UDRUserWidget`으로.
3. **DataTable 텍스트**: `FText` 컬럼 + Key/Namespace 지정, RowName은 영문 식별자.
4. Dashboard에서 **Gather → en 번역 → Compile**.
5. PIE 한/영 토글로 확인.

---

### 10.10 (선택) 커맨드릿으로 Gather/Compile 자동화

Dashboard로 최초 타깃을 만든 뒤 생성된 `Config/Localization/Game*.ini` 를 이용하면 CLI로 파이프라인을 돌릴 수 있다(빌드 자동화용):

```bat
"D:/UE_5.5/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
  "C:/Users/OWNER/Documents/Unreal Projects/DaeRune/DaeRune.uproject" ^
  -run=GatherText ^
  -config="Config/Localization/Game.ini" ^
  -SCCProvider=None -Unattended -LogLocalizationConflicts
```

- `Game.ini`는 Gather→(Import)→(Export)→Compile 스텝을 체이닝한다(Dashboard가 생성).
- CI에 넣으면 "소스/에셋 변경 → 자동 Gather/Compile"이 가능하나, **번역 입력(en .archive)** 은 여전히 사람이 채워야 한다.

---

### 10.11 에디터 작업 체크리스트

- [ ] Localization Dashboard에서 `Game` 타깃 생성 (Native=`ko`, +`en`)
- [ ] Gather from Text Files: `Source/DaeRune` (`*.cpp`,`*.h`)
- [ ] Gather from Packages: `Content/Blueprints/UI`, `.../Phase/Data`, `.../AbilitySystem/Data`
- [ ] 1순위 위젯(오버레이+TextUI)의 모든 TextBlock **Localizable + Key 안정화**
- [ ] 텍스트 있는 위젯을 되도록 `UDRUserWidget` 상속으로 통일(라이브 갱신 수혜)
- [ ] 동적 `SetText` 위젯은 `On Language Changed` 이벤트 구현
- [ ] `DT_PhaseObjective` 셀 FText Key/Namespace 수동 지정
- [ ] Gather → `en` 번역 입력 → Compile → `Content/Localization/Game/{ko,en}/*.locres` 확인
- [ ] PIE 한↔영 토글 검증(§10.8)
- [ ] 2·3순위 위젯 순차 처리
- [ ] (선택) Composite Font 한글 fallback 등록
- [ ] (선택) 커맨드릿 자동화 구성
