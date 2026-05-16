# DaeRune 사운드 시스템 분석 (Research)

작성일: 2026-05-12
대상: 차후 사운드 추가/리팩토링 작업 전 점검용

---

## 1. 개요 (한눈에 보는 사운드 시스템)

DaeRune의 사운드 시스템은 **5개의 서로 다른 경로**로 사운드를 재생하고 있으며, 각 경로가 일관성 없이 혼재되어 있다. 그래서 "어떤 사운드는 재생되고 어떤 사운드는 재생되지 않는" 현상이 발생할 가능성이 매우 높다.

| # | 경로 | 호출 위치 | 멀티플레이 동기화 | 비고 |
|---|------|-----------|-------------------|------|
| 1 | `UDRSoundManager` (GameInstanceSubsystem) | **호출자 없음 (Dead Code)** | 로컬 World | 정의만 되어 있고 아무도 안 부름 |
| 2 | `UGameplayStatics::PlaySoundAtLocation/2D` 직접 호출 + `UDRAssetManager::GetSoundDataAsset()` | 액터/GameState Multicast RPC | 서버가 Multicast | 실질적 메인 경로 |
| 3 | `UDRGameplayCue_Sound` / `_LocalOnly` (UGameplayCueNotify_Static 파생) | GAS `ExecuteGameplayCue` | ASC 복제 | 데미지/사망/스킬 큐 |
| 4 | `ADRBGMActor` (레벨 배치 액터의 AudioComponent + Fade-in) | 레벨 배치, BeginPlay 자동 재생 | 서버 시간 동기화 시도 | BGM 전용 |
| 5 | 액터 자체 `USoundBase` 프로퍼티 + 직접 호출 | Projectile, Door 등 | 일부는 RepNotify, 일부는 일반 함수 | 가장 산발적 |

핵심 결론:
- **싱글 진실 소스(SSOT)가 없다.** `DRSoundDataAsset`이 모든 사운드를 모으는 것 같지만 실제로는 일부만 모았고, 나머지는 액터의 개별 `USoundBase*` 프로퍼티에 흩어져 있다.
- **`UDRSoundManager`는 죽은 코드다.** 메서드는 다 만들어져 있지만 코드 전역에서 단 한 번도 호출되지 않는다 (`GetSubsystem<UDRSoundManager>` 검색 결과 0건). 호출자 입장에서 SoundManager를 우회해 `UGameplayStatics`를 직접 부르는 패턴이 굳어졌다.
- **GameplayCue → 실제 사운드** 매핑은 Native 태그만 정의되어 있고, 사운드 본체는 블루프린트 GameplayCueNotify가 `/Game/Blueprints/AbilitySystem/GameplayCueNotifies` 경로에 있다 (`Config/DefaultGame.ini` 14행). 즉 사운드는 BP 자산을 통해서만 연결되며, 자산이 비어 있거나 `Sound` 프로퍼티가 미할당이면 조용히 무음 처리된다.

---

## 2. 핵심 코드 모듈

### 2.1 `UDRSoundManager` (`Source/DaeRune/Public/Sound/DRSoundManager.h:18`)

- 종류: `UGameInstanceSubsystem`
- 의도: 모든 효과음 재생을 한 곳에서 처리
- 보유 API: `PlaySound2D`, `PlayUIClickSound`, `PlayPhaseStartSound`, `PlayWaveStartSound`, `PlayGameClearSound`, `PlayGameOverSound`, `PlaySoundAtLocation`, `PlayPartPickupSound`, `PlayPartInstallSound`, `PlaySeedExplosionSound`, `PlayWaterGainSound`, `StartCleanserOperatingSound`
- 데이터: `TObjectPtr<UDRSoundDataAsset> SoundData` (서브시스템 초기화 시 `UDRAssetManager` → `UDRGameInstance` 순으로 fallback 로드)
- World 결정: `GEngine->GetCurrentPlayWorld()` → `GetGameInstance()->GetWorld()` 순 (`DRSoundManager.cpp:54-63`)
- **치명적 문제**: 외부에서 `GetSubsystem<UDRSoundManager>` 또는 `SoundManager->Play...` 호출이 0건. 즉 실제 게임에서는 한 번도 동작하지 않는다.

### 2.2 `UDRSoundDataAsset` (`Source/DaeRune/Public/Sound/DRSoundDataAsset.h:14`)

- 종류: `UPrimaryDataAsset`
- Primary Asset ID: `"SoundData"` 타입 (`DRAssetManager.cpp:10`)
- 인스턴스: `/Game/Blueprints/Sound/Data/DA_SoundData.DA_SoundData`
- 필드 (모든 필드가 `TObjectPtr<USoundBase>`):
  - **Game**: PhaseStartSound, WaveStartSound, GameClearSound, GameOverSound
  - **UI**: UIButtonClickSound
  - **Actor**: CleanserOperatingSound, PartPickupSound, PartInstallSound, PartInstallCompleteSound, SeedExplosionSound, WaterGainSound
  - **BGM**: BGM_MainMenu, BGM_Lobby, BGM_Stage, BGM_Boss  ← **코드에서 한 번도 참조하지 않음 (dead field)**
- 헤더 주석은 `cp949`로 깨져 있다(저장 인코딩 불일치).

### 2.3 `UDRAssetManager` (`Source/DaeRune/Public/DRAssetManager.h:18`)

- 종류: 커스텀 `UAssetManager`
- 역할:
  - `StartInitialLoading()`에서 `FDRGameplayTags::InitializeNativeGameplayTags()` 호출 (`DRAssetManager.cpp:25`)
  - `UAbilitySystemGlobals::InitGlobalData()` 호출
  - `LoadSoundAssets()` 호출 → `SoundData` Primary Asset을 동기 로드 (`WaitUntilComplete()`)
  - Primary Asset 시스템에서 못 찾으면 `LoadObject<UDRSoundDataAsset>(... DA_SoundData ...)`로 직접 경로 로드 fallback
  - 모든 호출 사이트에서 `Cast<UDRAssetManager>(UAssetManager::GetIfInitialized())->GetSoundDataAsset()` 패턴으로 사운드 데이터를 가져옴.

### 2.4 GameplayCue (사운드용)

#### `UDRGameplayCue_Sound` (`Source/DaeRune/Public/Sound/DRGameplayCue_Sound.h:22`)

- `UGameplayCueNotify_Static` 파생, `IsOverride = true`로 인스턴스 생성 방지
- 프로퍼티: `USoundBase* Sound`, `bool bIs3DSound = true`
- `OnExecute_Implementation`: `Parameters.Location`이 zero가 아니면 그 위치에서, 아니면 `MyTarget` 위치에서, 3D면 `PlaySoundAtLocation`, 2D면 `PlaySound2D` (`DRGameplayCue_Sound.cpp:14-44`).
- 사용 방식: BP에서 이 클래스를 상속한 Static Cue 노티파이를 만들고, `GameplayCue.*` 태그와 매칭되도록 클래스 이름을 짓는 것. (UE의 GameplayCueManager가 이름 컨벤션으로 자동 매핑)

#### `UDRGameplayCue_Sound_LocalOnly` (`Source/DaeRune/Public/Sound/DRGameplayCue_Sound_LocalOnly.h:13`)

- 부모 클래스를 상속, `OnExecute_Implementation`만 오버라이드.
- 차이: `Cast<APawn>(MyTarget)`이고 `IsLocallyControlled()`인 경우에만 재생 (즉 로컬 플레이어의 1인칭 사운드 전용).

#### Native GameplayCue 태그 (`DRGameplayTags.cpp:471-509`)

| 태그 | 발동 위치 | 설명 |
|------|-----------|------|
| `GameplayCue.Player.Damage` | `DRPlayerAttributeSet::HandleIncomingDamage` (`DRPlayerAttributeSet.cpp:102`) | 플레이어 피격 |
| `GameplayCue.Player.Death` | `ADRCharacterBase::MulticastHandleDeath_Implementation` (`DRCharacterBase.cpp:179`) | 플레이어 사망 |
| `GameplayCue.Player.LowHealth` | **없음** | 정의됐지만 코드에서 한 번도 ExecuteGameplayCue 안 됨 → 무음 |
| `GameplayCue.Player.WaterDepleted` | `DRAttributeSet.cpp:205` (Water가 0이 될 때) | 물 고갈 |
| `GameplayCue.Enemy.Damage` | `DREnemyAttributeSet::HandleIncomingDamage` (`DREnemyAttributeSet.cpp:27`) | 적 피격 |
| `GameplayCue.Cleanser.Damage` | `DRCleanserSiteAttributeSet.cpp:79` (Elite 디버프 상태에서만!) | 클렌저 피격 |
| `GameplayCue.Skill.WaterPump` | `DRWaterPump.cpp:202`(Add) / `:293`(Remove) | Add/Remove 형태 루프 큐 |
| `GameplayCue.Skill.ClawSwipe` | **없음** | 정의됐지만 코드에서 한 번도 발동 안 됨 → 무음 |

블루프린트 클래스가 존재해야 사운드가 들린다. 노티파이 BP는 `Config/DefaultGame.ini:14` 의 `+GameplayCueNotifyPaths=/Game/Blueprints/AbilitySystem/GameplayCueNotifies` 경로에 있다.

### 2.5 `ADRBGMActor` (`Source/DaeRune/Public/Actor/DRBGMActor.h:18`)

- 종류: 레벨 배치형 `AActor` (`bReplicates=true, bAlwaysRelevant=true`)
- 컴포넌트:
  - `USceneComponent` Root
  - `UAudioComponent` (`bAutoActivate=false`, `bIsUISound=true`, `bAllowSpatialization=false`)
  - SoundClass Override: `/Game/Blueprints/Audio/SoundClasses/SC_BGM.SC_BGM`을 ConstructorHelpers로 강제 부착 (`DRBGMActor.cpp:30-35`)
- 프로퍼티:
  - `BGMSound`, `bAutoPlay=true`, `FadeInDuration=2.0`, `Volume=1.0`
  - `BGMStartServerTime` (Replicated, RepNotify) - 클라이언트가 늦게 들어왔을 때 시간 동기화 의도였으나, 실제 코드는 단순 `FadeIn`만 함 (시간 보정 미사용, 주석에 "복잡한 시간 동기화 제거"라고 적혀 있음)
- 흐름:
  - 서버 `BeginPlay`: `PlayBGM()` → 서버 시간 저장 + 로컬 재생
  - 클라이언트 `BeginPlay`: 0.5초 타이머 후 `FadeIn` (백업으로 `OnRep_BGMStartTime`에서도 `PlayBGMLocal()`)
- 정지: `StopBGM(FadeOutDuration=1.0)`은 클라이언트에서도 호출 가능. 단, **`StopBGM`을 BGMStartServerTime으로 트리거하지는 않음** → 게임 종료 시 `ADRPlayerController::ClientStopAllAudio_Implementation`에서 직접 모든 `ADRBGMActor`를 찾아 `StopBGM(0.0f)` 호출 (`DRPlayerController.cpp:308-321`).
- 인스턴스: `Content/Blueprints/Actor/Test/BP_DRBGMActor.uasset` (이름이 `Test` 폴더에 있다는 점이 임시 작업의 흔적).

### 2.6 `UDRSettingsManager` (`Source/DaeRune/Public/Game/DRSettingsManager.h:37`)

- 종류: `UGameInstanceSubsystem`
- 오디오 리소스 (Initialize에서 LoadObject):
  - `/Game/Blueprints/Audio/SoundMix/SM_GameMix.SM_GameMix` → `GameSoundMix`
  - `/Game/Blueprints/Audio/SoundClasses/SC_Master.SC_Master` → `MasterSoundClass`
  - `/Game/Blueprints/Audio/SoundClasses/SC_BGM.SC_BGM` → `BGMSoundClass`
  - `/Game/Blueprints/Audio/SoundClasses/SC_SFX.SC_SFX` → `SFXSoundClass`
- 볼륨 적용 흐름 (`ApplySoundMixToWorld`, `:165-190`):
  1. `PushSoundMixModifier(World, GameSoundMix)`
  2. 각 SoundClass에 대해 `SetSoundMixClassOverride(..., Volume=Settings->MasterVolume/BGMVolume/SFXVolume, Pitch=1.0, FadeIn=0.0, bApplyToChildren=true/false)`
- API: `SetMasterVolume`, `SetBGMVolume`, `SetSFXVolume` (Clamp 0~1)
- 데이터 주도 레이어:
  - `Audio.MasterVolume`, `Audio.MusicVolume`, `Audio.SFXVolume` 세 가지 `FDRSettingDefinition`을 정의 (`DRSettingsManager.cpp:530-593`)
  - `EDRSettingsApplyMode::Instant` 모드 → 슬라이더 조작 즉시 `ApplySingleSetting` → `ApplyAudioSettings` 호출
  - 저장 값: 0~100 슬라이더, 저장 시 `/100`으로 0~1 변환 (`:801-813`)
- 유의점:
  - SoundClass 자산이 4개 존재해야 함 (`SC_Master`, `SC_BGM`, `SC_SFX`, **`SC_Voice` (사용 X, 미사용 자산)**)
  - `SC_Voice`는 컨텐츠에는 있지만 SettingsManager에서 로드/적용하지 않음.

### 2.7 `UDRGameUserSettings` (`Source/DaeRune/Public/Game/DRGameUserSettings.h:13`)

- `UGameUserSettings` 파생, `[Config]` 속성으로 INI 영속화
- 오디오: `MasterVolume`, `BGMVolume`, `SFXVolume` (모두 0.0~1.0, 기본 1.0)
- `ApplyCustomSettings()`에서 Clamp만 함 (실제 사운드 적용은 `UDRSettingsManager`가 담당)

---

## 3. 실제 사운드 재생 경로 (호출 사이트별)

다음 표는 코드베이스에서 사운드가 재생되는 모든 지점을 정리한 것이다. **이 표가 곧 "현재 동작 중인 사운드 시스템의 전체 그림"**이다.

### 3.1 게임 전역 (Phase / 게임 종료)

| 이벤트 | 발동 위치 | 데이터 소스 | 멀티플레이 |
|--------|-----------|-------------|------------|
| Phase 시작 | `UDRPhaseBase::OnPhaseStart` → `GameState->Multicast_PlayPhaseStartSound` | `DA_SoundData.PhaseStartSound` | Multicast |
| Wave 시작 | `UDRPhase3::StartNextWave` → `GameState->Multicast_PlayWaveStartSound` (`DRPhase3.cpp:199`) | `DA_SoundData.WaveStartSound` | Multicast |
| 게임 클리어 | `ADRStageGameMode::TriggerGameClear` → `GameState->Multicast_PlayGameClearSound` | `DA_SoundData.GameClearSound` | Multicast |
| 게임 오버 | `ADRStageGameMode::TriggerGameOver` → `GameState->Multicast_PlayGameOverSound` | `DA_SoundData.GameOverSound` | Multicast |

구현은 모두 `ADRStageGameState::Multicast_Play..._Implementation` 안에서 `Cast<UDRAssetManager>(UAssetManager::GetIfInitialized())->GetSoundDataAsset()` → `UGameplayStatics::PlaySound2D(this, ...)` 패턴 (`DRStageGameState.cpp:271-329`).

### 3.2 클렌저 사이트 (`ADRCleanserSite`)

| 이벤트 | 발동 위치 | 데이터 소스 | 비고 |
|--------|-----------|-------------|------|
| 부품 설치 (진행) | `MulticastPlayInstallSound(false)` | `PartInstallSound` | `InstallPart`에서 멀티캐스트 |
| 부품 설치 (완료) | `MulticastPlayInstallSound(true)` | `PartInstallCompleteSound` | 2개 다 설치 시 |
| 클렌저 작동 (루프) | `MulticastStartOperatingSound` / `MulticastStopOperatingSound` | `CleanserOperatingSound` | Phase3 시작/종료. `OperatingSoundComponent`를 캐시했다가 Stop |

작동 사운드 캐시(`OperatingSoundComponent`)는 **Multicast로 받은 클라이언트에서도 별도 컴포넌트로 생성**되므로 모든 클라이언트에서 들린다. 단 Stop은 동일 클라이언트에서 자기 컴포넌트만 멈출 수 있고, **late-join 클라이언트는 시작 사운드 RPC를 못 받았기 때문에 영원히 안 들린다** (재현 가능한 버그 후보).

### 3.3 클렌저 부품 (`ADRCleanserPart`)

| 이벤트 | 발동 위치 | 데이터 소스 |
|--------|-----------|-------------|
| 부품 줍기 | `MulticastPlayPickupSound` (`DRCleanserPart.cpp:207`) | `PartPickupSound` |

### 3.4 수원지 (`ADRWaterSource`)

| 이벤트 | 발동 위치 | 데이터 소스 |
|--------|-----------|-------------|
| 물 채우기 성공 | `MulticastPlayWaterGainSound` (`DRWaterSource.cpp:35`) | `WaterGainSound` |

### 3.5 발사체

| 액터 | 이벤트 | 데이터 소스 | 비고 |
|------|--------|-------------|------|
| `ADRProjectile` | BeginPlay에서 루프 사운드 재생 (`DRProjectile.cpp:50`) | 자체 `LoopingSound` UPROPERTY | `SpawnSoundAttached` 사용 |
| `ADRProjectile` | OnHit 시 임팩트 사운드 (`DRProjectile.cpp:58`) | 자체 `ImpactSound` UPROPERTY | 클라이언트에서 `Destroyed()`로도 재호출되어 양쪽에서 동작 |
| `ADRSeedProjectile` | 폭발 사운드 (`DRSeedProjectile.cpp:314`) | `DA_SoundData.SeedExplosionSound` (Multicast) | |

> 주의: `LoopingSound`/`ImpactSound`는 **자체 인스턴스 프로퍼티**이므로 BP에서 직접 설정해야 한다. 즉 `DA_SoundData`에 들어 있지 않다. 발사체 BP에서 누락되면 무음이다.

### 3.6 문(Door)

| 액터 | 이벤트 | 데이터 소스 |
|------|--------|-------------|
| `ADRAutoSlidingDoor` | 열림(`Opening`) → `DoorOpenSound` (`DRAutoSlidingDoor.cpp:158, 194`) | 자체 `DoorOpenSound` |
| `ADRAutoSlidingDoor` | 닫힘(`Closing`) → `DoorCloseSound` (`:166, 202`) | 자체 `DoorCloseSound` |
| `ADRBreakableDoor` | 파괴 시 (`DRBreakableDoor.cpp:224`) | 자체 `BreakSound` |

오토 슬라이딩 도어는 RepNotify(`OnRep_DoorState`)에서 한 번, 권한측 `SetDoorState`에서 한 번 ⇒ 리슨 서버에서는 같은 사운드가 두 번 재생될 가능성이 있다(서버측 인스턴스가 `SetDoorState` 분기 + 자기 자신의 RepNotify를 둘 다 타지는 않으나, 코드만 보면 동일 분기를 두 곳에서 호출하고 있어 잠재적 중복 후보).

### 3.7 GAS GameplayCue 기반 (블루프린트 의존)

| 큐 태그 | 발동 시점 | 위치 | 노티 BP 필요 여부 |
|---------|-----------|------|-------------------|
| `GameplayCue.Player.Damage` | 플레이어 피격 시 | `DRPlayerAttributeSet.cpp:102` | 필요 |
| `GameplayCue.Player.Death` | 플레이어 사망 시 | `DRCharacterBase.cpp:179` | 필요 |
| `GameplayCue.Player.WaterDepleted` | Water 어트리뷰트가 0으로 떨어지는 순간 | `DRAttributeSet.cpp:205` | 필요 |
| `GameplayCue.Enemy.Damage` | 적 피격 시 | `DREnemyAttributeSet.cpp:27` | 필요 |
| `GameplayCue.Cleanser.Damage` | 클렌저가 `Debuff.Elite` 상태에서 피격 | `DRCleanserSiteAttributeSet.cpp:79` | 필요 |
| `GameplayCue.Skill.WaterPump` | 워터펌프 어빌리티 활성/종료 (Add/Remove) | `DRWaterPump.cpp:202/293` | 필요 (WhileActive 형태) |

⚠ **재생 안 되는 사운드 후보 1**: `GameplayCue.Player.LowHealth`와 `GameplayCue.Skill.ClawSwipe`는 태그는 정의되어 있으나, 코드 어디에서도 `ExecuteGameplayCue/AddGameplayCue`되지 않는다. 따라서 BP가 있어도 안 울린다.

### 3.8 BGM

- `ADRBGMActor`의 인스턴스를 레벨에 배치 (`BP_DRBGMActor`).
- `BGMSound`에 `SoundCue` 또는 `SoundWave` 할당. SoundClass는 생성자에서 `SC_BGM`으로 고정.
- `bAutoPlay=true`이면 `BeginPlay`에서 자동 재생.
- 게임 종료(Wipeout/Clear) 시 `NotifyAllPlayersGameEnd` → `PC->ClientStopAllAudio()` → 모든 `ADRBGMActor`에 `StopBGM(0.0f)` (`DRPlayerController.cpp:308-321`).

⚠ **MainMenu / Lobby BGM은 코드 흐름상 존재하지 않는다.** `DA_SoundData`에 `BGM_MainMenu/Lobby/Stage/Boss` 필드가 있지만 어디에서도 참조되지 않으며, `BGM_Boss` 또한 Phase4 진입 시 교체되는 로직이 없다. BGM은 오로지 "레벨에 `BP_DRBGMActor`를 배치했는가"에만 의존한다. MainMenu/LobbyMap에 BGM 액터가 없으면 무음이다.

---

## 4. 데이터 자산 인벤토리 (실제 컨텐츠 폴더 구조)

### 4.1 SoundClass / SoundMix (`Content/Blueprints/Audio/`)

- `SoundClasses/SC_Master.uasset`
- `SoundClasses/SC_BGM.uasset`
- `SoundClasses/SC_SFX.uasset`
- `SoundClasses/SC_Voice.uasset` ← **사용처 없음**
- `SoundMix/SM_GameMix.uasset`

### 4.2 SoundCue (`Content/Blueprints/Sound/SoundCue/`)

(`SC_*`는 SoundCue 접두사, SoundClass의 `SC_`와 이름이 겹쳐 혼란스러움)

- **BGM**: `BGM/Stage1/SC_Stage1BGM.uasset` (BP_DRBGMActor에 할당된 것으로 추정)
- **Cleanser**: `SC_CleanserActivate`, `SC_CleanserDamage`, `SC_CleanserFix`, `SC_CleanserPartCollect`, `SC_CleanserPartInsert`
- **Enemy_Dog**: `SC_DogAttack`, `SC_DogHit`
- **GardenerRobot** (플레이어 전용 스킬/이동음): `SC_GardenBasickAttack`, `SC_GardenHoseStart`, `SC_GardenJumpLand`, `SC_GardenJumpStart`, `SC_GardenRobotWalk`, `SC_GardenSeedCannonExplosion`, `SC_GardenSeedCannonLaunch`, `SC_HoseContinuous`
- **Phase**: `SC_PhaseStart`, `SC_WaveStart`
- **PlayerUniversal**: `SC_PlayerCollectWater`, `SC_PlayerDamage`, `SC_PlayerDeath`, `SC_PlayerLowHealth`, `SC_PlayerLowWater`
- **UI**: `SC_ButtonClick`, `SC_GameClear`, `SC_GameOver`

### 4.3 SoundAttenuation

- `SoundAttenuation/SA_Medium.uasset`
- `SoundAttenuation/SA_Short.uasset`

(코드에는 `SoundAttenuation*`이 직접 등장하지 않음. SoundCue 내부에서 사용되는 것으로 추정.)

### 4.4 Primary Data Asset

- `Content/Blueprints/Sound/Data/DA_SoundData.uasset`

### 4.5 SoundWave (raw assets, `Content/Blueprints/Sound/SoundWave/`)

생략 가능 — SoundCue가 묶음 단위이므로 보통 SoundCue를 참조하면 된다.

---

## 5. 사운드 자산 ↔ 데이터/코드 매핑 (커버리지 매트릭스)

다음 표는 **컨텐츠에 존재하는 SoundCue가 어디에서 소비되는지**를 추적해, 누락/고아 자산을 식별한 것이다.

| SoundCue 자산 | 매핑된 곳 | 상태 |
|---------------|-----------|------|
| `SC_Stage1BGM` | `BP_DRBGMActor`의 `BGMSound` (추정) | 동작 |
| `SC_PhaseStart` | `DA_SoundData.PhaseStartSound` | 동작 |
| `SC_WaveStart` | `DA_SoundData.WaveStartSound` | 동작 |
| `SC_ButtonClick` | `DA_SoundData.UIButtonClickSound` | `PlayUIClickSound()`는 `DRSoundManager` 안에만 있고 호출자 없음. 실제 버튼 SFX는 WBP에서 별도 처리하거나 무음 가능성. |
| `SC_GameClear` | `DA_SoundData.GameClearSound` | 동작 |
| `SC_GameOver` | `DA_SoundData.GameOverSound` | 동작 |
| `SC_CleanserActivate` | 미매핑 (코드/DataAsset 어디에도 직접 참조 없음) | 고아 자산 후보 |
| `SC_CleanserFix` | 미매핑 | 고아 자산 후보 |
| `SC_CleanserDamage` | `GameplayCue.Cleanser.Damage` 노티 BP에서 참조 (추정) | 발동 조건이 `Debuff.Elite`일 때만 — 일반 클렌저 피격은 무음 |
| `SC_CleanserPartCollect` | `DA_SoundData.PartPickupSound` 후보 | 동작 (할당 시) |
| `SC_CleanserPartInsert` | `DA_SoundData.PartInstallSound` 후보 | 동작 (할당 시) |
| (없음) | `DA_SoundData.PartInstallCompleteSound` | 별도 자산 없음 → DataAsset 필드 자체가 비어 있을 가능성 → 부품 2개 설치 완료 시 무음? |
| (없음) | `DA_SoundData.CleanserOperatingSound` | Loop용 자산이 폴더에 없어 보임 → Phase3 클렌저 작동 루프 무음 가능성 |
| `SC_PlayerCollectWater` | `DA_SoundData.WaterGainSound` | 동작 |
| `SC_PlayerDamage` | `GameplayCue.Player.Damage` 노티 BP | 동작 (BP만 만들어졌다면) |
| `SC_PlayerDeath` | `GameplayCue.Player.Death` 노티 BP | 동작 |
| `SC_PlayerLowHealth` | `GameplayCue.Player.LowHealth` 노티 BP | **태그 발동 호출이 코드 어디에도 없음** → 무음 확정 |
| `SC_PlayerLowWater` | `GameplayCue.Player.WaterDepleted` 노티 BP | 동작 |
| `SC_DogAttack` | (적 캐릭터/몽타주 AnimNotify에서 직접 재생 추정) | 코드에는 흔적 없음 |
| `SC_DogHit` | `GameplayCue.Enemy.Damage` 노티 BP에서 참조 가능 | 단일 자산만 GAS 큐로 연결 가능 (`dog_hit_A~E` 5종 SoundWave는 SoundCue가 랜덤 노드를 쓰는지 확인 필요) |
| `SC_GardenBasickAttack` | Player 어빌리티 몽타주 AnimNotify (Blueprint) | 미확인 |
| `SC_GardenHoseStart`, `SC_HoseContinuous` | `GameplayCue.Skill.WaterPump` (WhileActive) | 추정 동작 |
| `SC_GardenJumpStart/Land`, `SC_GardenRobotWalk` | Anim Notify | 미확인 |
| `SC_GardenSeedCannonLaunch` | (어빌리티 활성 시 BP에서 재생 추정) | 미확인 |
| `SC_GardenSeedCannonExplosion` | `DA_SoundData.SeedExplosionSound` | 동작 |

### 5.1 DataAsset 필드 ↔ 자산 매칭 누락 (반드시 점검 필요)

`DA_SoundData.uasset`을 에디터에서 열어 아래 필드가 실제로 SoundCue를 가리키는지 확인:

- 정상 동작이 확인된 필드라도 자산이 비어 있으면 무음 (코드 측은 `if (Sound)` 가드만 함, 로그 없음)
- 의심 필드: **`PartInstallCompleteSound`**, **`CleanserOperatingSound`**, **`UIButtonClickSound`**
- 절대 안 쓰는 필드 (제거 후보): `BGM_MainMenu`, `BGM_Lobby`, `BGM_Stage`, `BGM_Boss`

---

## 6. 알려진 / 강력 의심 버그 & 누락 사운드

이전 작업에서 "그냥 넘어갔던" 사운드 후보를 우선순위 순으로 정리.

### 확정 무음 (코드 레벨에서 호출이 없음)

1. **Container Low Sound** (`GameplayCue.Player.LowHealth`)
   - 태그/노티 BP는 만들 수 있지만 ExecuteGameplayCue 호출이 코드에 없음.
   - 컨테이너 체력이 임계치 이하일 때 발동시키는 로직 자체가 누락. `DRPlayerAttributeSet` 또는 컨테이너 헬스 차감 지점에서 추가 필요.

2. **ClawSwipe Effect Sound** (`GameplayCue.Skill.ClawSwipe`)
   - 동일 이유. 어빌리티(`DRClawSwipe`?) 활성 시 ASC에 큐를 추가하는 코드 없음.

3. **`UDRSoundManager`의 12개 메서드 전체**
   - 호출자 0건. 특히 `PlayUIClickSound`가 죽어 있으므로 UI 버튼 클릭 사운드는 WBP에서 자체적으로 처리하지 않으면 무음.

### 조건부 무음 / 자산 미할당 의심

4. **`PartInstallCompleteSound`** — 폴더에 명확히 매칭되는 SoundCue가 없음. DataAsset 필드가 비어 있으면 2번째 부품 설치 시 무음.
5. **`CleanserOperatingSound`** — Loop용 별도 자산이 폴더에 안 보임. 미할당이면 Phase3 클렌저 작동 루프가 무음.
6. **`UIButtonClickSound`** — SoundCue는 있으나 호출 경로가 죽은 SoundManager뿐. WBP에서 별도 처리하지 않으면 무음.

### 동기화/멀티플레이 문제

7. **`MulticastStartOperatingSound` Late-Join** — RPC를 못 받은 늦은 클라이언트는 Phase3 시작 후 클렌저 작동 사운드를 영원히 못 듣는다. RepNotify 기반 또는 GameState 플래그 기반으로 전환 필요.
8. **`ADRAutoSlidingDoor`의 사운드 중복 가능성** — `SetDoorState`와 `OnRep_DoorState` 양쪽에서 사운드 재생. 리슨 서버에서 호스트 본인은 한 번만 들릴 가능성이 높지만, 일반 클라이언트 동기화 타이밍에 따라 두 번 들릴 수 있다.
9. **`ADRBGMActor`의 시간 동기화 미구현** — 주석에 "복잡한 시간 동기화 제거"라고 적혀 있어 BGM이 클라이언트마다 다른 시점에서 시작될 수 있다. 협동 플레이에서 BGM 위상 차이 → 알아채기 어렵지만 거슬리는 이슈.

### 자산 정리 후보 (사용처 없음)

- `SC_CleanserActivate`, `SC_CleanserFix` 두 SoundCue가 어디서도 참조되지 않음. 의도된 자산인지 폐기 후보인지 결정 필요.
- `SC_Voice` SoundClass — Voice 채널을 설정창에서 노출하지 않으면서 정의만 되어 있음. 향후 보이스 채팅/내레이션 계획이 있는지 확인 필요.
- `DRSoundDataAsset.BGM_*` 4개 필드 — 모두 사용 안 됨.

---

## 7. 사운드 시스템에 영향을 주는 부수적 구조

### 7.1 게임 종료 시 오디오 정리

- `ADRStageGameMode::NotifyAllPlayersGameEnd` → 각 PC `ClientStopAllAudio()`
- `ADRPlayerController::ClientStopAllAudio_Implementation`은 **BGMActor만** 정지. 효과음 루프(클렌저 작동 등)는 안 멈춤.
- 결과: 게임 오버/클리어 후 ServerTravel 직전 짧은 순간에 클렌저 루프 사운드가 남아 있을 수 있음.

### 7.2 패키징 설정 (`Config/DefaultGame.ini`)

```
+DirectoriesToAlwaysCook=(Path="/Game/Blueprints/Sound")
+DirectoriesToAlwaysCook=(Path="/Game/Blueprints/Audio")
```

⇒ Sound/Audio 폴더 전체가 패키지에 포함되도록 명시되어 있어, Primary Asset 미등록 자산도 쿠킹된다.

### 7.3 GameplayCue 노티 경로

`Config/DefaultGame.ini:14`:
```
+GameplayCueNotifyPaths=/Game/Blueprints/AbilitySystem/GameplayCueNotifies
```

⇒ 모든 GameplayCueNotify Blueprint는 이 경로 안에 있어야 자동 매핑된다.

---

## 8. "어떤 사운드가 어떻게 동작하는가" — 한 페이지 정리

```
[BGM]
  레벨 배치된 BP_DRBGMActor → AudioComponent(FadeIn)
  └ SoundClass = SC_BGM ← SettingsManager가 BGMVolume 조정

[Phase / Wave / GameClear / GameOver]
  Server: Phase or GameMode 코드
  → ADRStageGameState::Multicast_Play...Sound (RPC)
  → 각 클라이언트: UAssetManager에서 DA_SoundData 가져옴
  → UGameplayStatics::PlaySound2D
  └ SoundClass = SoundCue 내부 설정 (대개 SC_SFX 또는 SC_Master)

[Cleanser Site / Cleanser Part / Water Source / Seed Explosion]
  Server: 액터 자체 권한 메서드
  → 액터의 NetMulticast RPC
  → 각 클라이언트: UAssetManager → DA_SoundData → PlaySoundAtLocation

[일반 발사체 / 문]
  자체 USoundBase* 프로퍼티 (BP에서 직접 할당)
  → PlaySoundAtLocation 또는 SpawnSoundAttached

[GAS Cue (피격/사망/물고갈/스킬)]
  Attribute 변경 또는 캐릭터 코드
  → ASC->ExecuteGameplayCue or AddGameplayCue
  → GameplayCueManager가 태그명으로 BP 노티 자동 매칭
  → BP 노티의 Sound 필드에서 SoundCue 재생
  └ Player.Damage / Player.Death / Player.WaterDepleted / Enemy.Damage
    / Cleanser.Damage(Elite only) / Skill.WaterPump (loop)
  └ X Player.LowHealth / Skill.ClawSwipe → 발동 코드 없음

[UI 버튼]
  WBP 자체 처리 (코드 경로 사용 안 됨, DRSoundManager 죽어 있음)

[볼륨]
  GameUserSettings.ini → UDRGameUserSettings (Master/BGM/SFX 0~1)
  → UDRSettingsManager::ApplyAudioSettings
  → PushSoundMixModifier + SetSoundMixClassOverride
  → SoundClass별 볼륨 (SC_Master, SC_BGM, SC_SFX)
```

---

## 9. 사운드 작업 진입 시 권장 순서 (TODO 백로그)

이건 의사결정이 필요한 영역이라 직접 결론은 내리지 않고 옵션만 정리.

1. **단일 진입점 정하기**:
   - 옵션 A: `UDRSoundManager`를 진짜로 살리고 모든 호출을 그쪽으로 모은다. 멀티플레이 경로(Multicast RPC)는 액터에 유지하지만 액터 내부에서는 `UDRSoundManager`를 호출하도록 한다.
   - 옵션 B: `UDRSoundManager`를 폐기하고, 현재의 "`Cast<UDRAssetManager>(...)->GetSoundDataAsset()`을 직접 호출"하는 패턴을 정식 표준으로 삼는다. (사실상 현재 동작에 가까움)

2. **DataAsset vs 인스턴스 프로퍼티 정책 결정**:
   - 현재는 일부(Cleanser, Phase 등)는 DataAsset에서, 일부(Projectile, Door)는 액터별 BP 프로퍼티에서 가져온다. 일관성을 잡지 않으면 신규 사운드 추가 때마다 어디에 넣을지 헤맨다.

3. **GameplayCueNotify 점검**:
   - `/Game/Blueprints/AbilitySystem/GameplayCueNotifies` 폴더에서 위 6개 활성 태그에 대응하는 BP가 모두 존재하고 `Sound` 필드가 채워져 있는지 확인.
   - `Player.LowHealth`, `Skill.ClawSwipe` 두 큐는 코드 측 호출도 추가 필요.

4. **DA_SoundData 누락 필드 채우기**:
   - 특히 `PartInstallCompleteSound`, `CleanserOperatingSound`, `UIButtonClickSound`.

5. **Late-Join 대응**:
   - 클렌저 작동 루프 사운드는 RepNotify 또는 GameState 플래그 기반으로 변경.

6. **`SC_Voice` 활용 또는 제거 결정**, `BGM_*` DataAsset 필드 제거 또는 활용 결정.

---

## 10. 부록: 빠른 참조 표 (파일별 사운드 관련 라인 번호)

| 파일 | 주요 라인 | 내용 |
|------|----------|------|
| `Source/DaeRune/Public/Sound/DRSoundManager.h` | 18-69 | SoundManager 인터페이스 (Dead Code) |
| `Source/DaeRune/Private/Sound/DRSoundManager.cpp` | 13-202 | SoundManager 구현 (Dead Code) |
| `Source/DaeRune/Public/Sound/DRSoundDataAsset.h` | 14-72 | DataAsset 필드 정의 |
| `Source/DaeRune/Public/Sound/DRGameplayCue_Sound.h` | 22-39 | 사운드 Cue 베이스 |
| `Source/DaeRune/Private/Sound/DRGameplayCue_Sound.cpp` | 14-44 | OnExecute 구현 |
| `Source/DaeRune/Public/Sound/DRGameplayCue_Sound_LocalOnly.h` | 13-19 | 로컬 전용 Cue |
| `Source/DaeRune/Private/Actor/DRBGMActor.cpp` | 30-147 | BGM 액터 |
| `Source/DaeRune/Public/Actor/DRBGMActor.h` | 18-70 | BGM 헤더 |
| `Source/DaeRune/Private/Game/DRStageGameState.cpp` | 271-329 | Multicast Phase/Wave/Clear/Over 사운드 |
| `Source/DaeRune/Private/Actor/DRCleanserSite.cpp` | 107-153 | Install/Operating Multicast |
| `Source/DaeRune/Private/Actor/DRCleanserPart.cpp` | 207-220 | Pickup Multicast |
| `Source/DaeRune/Private/Actor/DRWaterSource.cpp` | 35-48 | WaterGain Multicast |
| `Source/DaeRune/Private/Actor/DRSeedProjectile.cpp` | 314-327 | Explosion Multicast |
| `Source/DaeRune/Private/Actor/DRProjectile.cpp` | 50, 58 | Looping/Impact 사운드 |
| `Source/DaeRune/Private/Actor/DRAutoSlidingDoor.cpp` | 158, 166, 194, 202 | Door Open/Close |
| `Source/DaeRune/Private/Actor/DRBreakableDoor.cpp` | 216-225 | Break 사운드 |
| `Source/DaeRune/Private/AbilitySystem/DRPlayerAttributeSet.cpp` | 95-105 | Player.Damage 큐 |
| `Source/DaeRune/Private/AbilitySystem/DRAttributeSet.cpp` | 194-210 | Player.WaterDepleted 큐 |
| `Source/DaeRune/Private/Character/DRCharacterBase.cpp` | 173-184 | Player.Death 큐 |
| `Source/DaeRune/Private/AbilitySystem/DREnemyAttributeSet.cpp` | 22-31 | Enemy.Damage 큐 |
| `Source/DaeRune/Private/AbilitySystem/DRCleanserSiteAttributeSet.cpp` | 70-83 | Cleanser.Damage 큐 (Elite only) |
| `Source/DaeRune/Private/AbilitySystem/Abilities/DRWaterPump.cpp` | 200-206, 290-295 | Skill.WaterPump Add/Remove |
| `Source/DaeRune/Private/Game/DRSettingsManager.cpp` | 41-69, 136-214, 530-593, 980-998 | 오디오 설정 적용 |
| `Source/DaeRune/Public/Game/DRGameUserSettings.h` | 28-35 | Master/BGM/SFX Volume Config |
| `Source/DaeRune/Private/DRAssetManager.cpp` | 20-66 | SoundData 사전 로드 |
| `Source/DaeRune/Private/Player/DRPlayerController.cpp` | 308-321 | ClientStopAllAudio (BGM만) |
| `Source/DaeRune/Private/DRGameplayTags.cpp` | 471-509 | GameplayCue 태그 정의 |
| `Config/DefaultGame.ini` | 14 | GameplayCueNotifyPaths |
| `Config/DefaultGame.ini` | 122-123 | Sound/Audio 폴더 always-cook |
