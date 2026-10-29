// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Tuning/DRTuningTypes.h"
#include "DRTuningSubsystem.generated.h"

class ADRPlayerController;
struct FActorsInitializedParams;

/**
 * 외부 튜닝 파일 — 빌드를 다시 뽑지 않고 스킬 · 업그레이드 수치를 바꿔 보는 개발용 장치.
 *
 *   패키지 빌드:  <빌드 폴더>/DaeRune/Tuning/튜닝.csv
 *   에디터 · -game: <프로젝트>/Tuning/튜닝.csv
 *
 * 기획자가 엑셀로 [변경값] 칸을 채워 저장하면, 게임을 시작할 때(그리고 켜져 있는 동안 저장할 때마다)
 * 그 값을 블루프린트 기본값(CDO) · 데이터 에셋 · 데이터/커브 테이블에 덮어쓴다. 에셋 파일은 바뀌지 않는다.
 *
 *  - 목록([현재값] · [설명] · [범위])은 쿡을 시작할 때 에디터가 Content/Tuning/TuningCatalog.csv 로 만들어 pak 에 넣는다
 *    (DaeRuneEditor 의 쿡 훅). 게임은 그 목록에 기존 튜닝.csv 의 [변경값] · [메모] 를 옮겨 담아 다시 쓴다.
 *  - 멀티플레이는 ★방장의 값★ 을 쓴다 — 참가자는 접속하면 방장 값을 받아 자기 파일 대신 적용한다.
 *  - 이미 부여된 어빌리티 인스턴스도 같이 고친다(발동 중이면 끝난 뒤). 액터 · 투사체는 새로 생기는 것부터,
 *    시작할 때 한 번 읽는 값(캐릭터 기본 스탯 GE · 웨이브 표)은 다음 스테이지부터 바뀐다.
 *  - 쉬핑 빌드 · -NoTuning · bEnabled=False 면 만들어지지 않는다.
 *  - 에디터 PIE 에서는 dr.Tuning.ApplyInPIE 1 일 때만 적용하고 PIE 가 끝나면 되돌린다
 *    (CDO 를 바꾼 채 BP 를 저장하면 그 값이 에셋에 박히기 때문).
 */
UCLASS(Config = Game)
class DAERUNE_API UDRTuningSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UDRTuningSubsystem* Get(const UObject* WorldContextObject);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 튜닝.csv 를 다시 읽어 적용한다 (콘솔 DRTuningReload · 저장 감지). 방장 값을 따르는 참가자면 하지 않는다 */
	void ReloadLocalSheet(const FString& Reason);

	/** 덮어쓴 값을 모두 기준값으로 되돌린다 (콘솔 DRTuningRevert — 비교해 볼 때). 파일을 다시 읽으면 다시 적용된다 */
	void RevertAllOverrides(const FString& Reason);

	/**
	 * 목록 기준으로 튜닝.csv 를 다시 쓴다. [변경값] · [메모] 는 그대로 둔다.
	 * bRebuildCatalog — 에디터 · -game 에서 목록을 에셋에서 새로 만든다 (콘솔 DRTuningExport). 아니면 저장해 둔 목록 파일을 쓴다
	 */
	bool RefreshSheet(FString& OutMessage, bool bRebuildCatalog = false);

	/** 서버: 이 참가자에게 지금 적용 중인 값을 보낸다 (ADRPlayerController::ServerRequestTuning) */
	void SendTuningToClient(ADRPlayerController* PlayerController) const;

	/** 참가자: 방장 값 조각을 받는다. 다 모이면 자기 파일 대신 그 값을 적용한다 (ADRPlayerController::ClientReceiveTuningChunk) */
	void ReceiveHostChunk(int32 InRevision, int32 ChunkIndex, int32 ChunkCount, const TArray<FDRTuningOverride>& Chunk);

	/** 지금 상태 한 덩어리 (콘솔 DRTuningStatus) */
	FString GetStatusText() const;

	/** 기획자가 고치는 파일 (전체 경로) */
	static FString GetSheetPath();

	/** 쿡 때 만든 목록 파일 — 패키지 빌드에서는 pak 안에 있다 */
	static FString GetCatalogTemplatePath();

#if WITH_EDITOR
	/**
	 * 쿡이 끝날 때 에디터 모듈이 부른다 — Content/Tuning/TuningCatalog.csv(현재값 · 설명 · 범위 목록)를 만든다.
	 * PackageFilter 는 "이번 쿡에 들어간 패키지인가" (비우면 에디터 기준 — 참조 없는 BP 만 뺀다)
	 */
	static bool WriteCatalogTemplate(FString& OutPath, FString& OutError, int32& OutRowCount, const TFunction<bool(FName)>& PackageFilter = nullptr);
#endif

	// ===== 설정 — DefaultGame.ini [/Script/DaeRune.DRTuningSubsystem] =====

	// false 면 만들지 않는다 (전시 빌드 등에서 끌 때)
	UPROPERTY(Config)
	bool bEnabled = true;

	// 게임이 켜져 있는 동안 튜닝.csv 를 저장하면 바로 다시 적용한다
	UPROPERTY(Config)
	bool bAutoReload = true;

	// 목록에 실을 블루프린트 — 이 클래스를 상속한 /Game 의 BP 전부 (CDO 값)
	UPROPERTY(Config)
	TArray<FSoftClassPath> ListedBlueprintBases;

	// 목록에 실을 에셋 종류 — 이 클래스의 /Game 에셋 전부 (데이터 에셋 · 데이터 테이블 · 커브 테이블 · 비헤이비어 트리)
	UPROPERTY(Config)
	TArray<FSoftClassPath> ListedAssetClasses;

	// 그 밖에 따로 실을 에셋
	UPROPERTY(Config)
	TArray<FSoftObjectPath> ListedAssets;

	// 배치 액터 값(맵에서 그 액터만 따로 정한 값)을 실을 맵 — 그 맵이 열릴 때 적용한다
	UPROPERTY(Config)
	TArray<FString> ListedMaps;

	// 경로에 이 글자가 들어 있으면 목록에서 뺀다 (UI · 템플릿 · 테스트 에셋)
	UPROPERTY(Config)
	TArray<FString> ExcludedPathContains;

	// [분류] 칸 규칙 (위에 있는 것이 먼저 · 시트도 이 순서로 정렬)
	UPROPERTY(Config)
	TArray<FDRTuningGroupRule> GroupRules;

private:
	bool IsApplyAllowed() const;

	/** 지금 덮어쓴 값을 모두 되돌린 뒤 Overrides 를 적용하고, 결과를 보고 · 방장이면 참가자에게 보낸다 */
	void ApplyOverrides(const TArray<FDRTuningOverride>& Overrides, const TArray<int32>& LineNumbers, bool bFromHost, const FString& SourceLabel);

	/** 1건 적용. 성공하면 OutNormalized(정규 경로 · 값)와 "기존 → 새 값" 설명 */
	bool ApplyOne(const FDRTuningOverride& Override, FDRTuningOverride& OutNormalized, FString& OutDetail, FString& OutWarning, FString& OutError);

	/** [대상] 글자(경로 · 짧은 이름) → 전체 경로. 오브젝트는 불러오지 않는다 (배치 액터는 맵을 불러오면 안 된다) */
	bool ResolveTargetPath(const FString& Target, FString& OutPath, FString& OutError) const;

	/** 배치 액터 값 1건 — 목록에 넣고, 그 맵이 지금 열려 있으면 바로 적용한다 */
	bool ApplyLevelOverride(const FDRTuningOverride& Override, FName MapPackage, FName ActorName, const FString& TargetPath,
		FDRTuningOverride& OutNormalized, FString& OutDetail, FString& OutError);

	struct FLevelOverride;

	/** 이 월드가 그 맵이면 배치 액터 값을 적용한다. 적용 수를 돌려주고 실패는 OutErrors 에 */
	int32 ApplyLevelOverridesToWorld(UWorld* World, TArrayView<const FLevelOverride> Overrides, TArray<FString>& OutErrors) const;

	bool LoadCatalog(TArray<FDRTuningRow>& OutRows, FString& OutMessage, bool bRebuildCatalog);
	void IndexCatalog(const TArray<FDRTuningRow>& Rows);

	/** 칩 캐시 · 업그레이드 GE 를 새 수치로 (모든 PlayerState) */
	void RefreshUpgradeCaches() const;

	/** 칩 카탈로그 · 진행 설정 검사 결과 (적용 전후를 비교해 새로 생긴 것만 알린다) */
	void CollectProgressionIssues(TArray<FString>& OutIssues) const;

	void BroadcastToClients() const;
	void WriteReport(const FString& Text) const;
	void EnsureReadme() const;
	void ShowScreenMessage(const FString& Message, const FColor& Color);
	void RememberSheetStamp();

	bool HandleTick(float DeltaTime);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);

	// 배치 액터 값 — 맵을 불러오지 않고 들고 있다가, 그 맵이 열릴 때(액터 BeginPlay 전) 적용한다
	struct FLevelOverride
	{
		FName MapPackage;
		FName ActorName;
		FString Property;
		FString Value;
	};
	TArray<FLevelOverride> LevelOverrides;
	FDelegateHandle WorldActorsInitializedHandle;

	// 참가자가 방장 값을 따르는 중 (자기 파일은 무시)
	bool bFollowingHost = false;

	// 적용할 때마다 +1 — 참가자에게 보내는 조각 묶음 번호
	int32 Revision = 0;

	// 지금 적용 중인 값 (성공한 것만 · 정규 경로)
	TArray<FDRTuningOverride> AppliedOverrides;

	// 받고 있는 방장 값 조각
	int32 PendingHostRevision = INDEX_NONE;
	int32 PendingHostChunkCount = 0;
	TMap<int32, TArray<FDRTuningOverride>> PendingHostChunks;

	// 목록에서 — 짧은 이름(소문자) → 경로, 경로|항목(소문자) → 범위
	TMap<FString, FString> AliasToPath;
	TMap<FString, FString> RangeByKey;

	// 파일 감시 — 바뀐 크기 · 시각이 한 번 더 같게 보이면(저장이 끝나면) 다시 읽는다
	FDateTime WatchedStamp;
	int64 WatchedSize = -1;
	FDateTime SeenStamp;
	int64 SeenSize = -1;
	bool bChangeSeen = false;

	// 화면이 아직 없을 때(게임 시작 직후) 띄울 메시지 — 첫 맵이 열리면 띄운다
	FString PendingScreenMessage;
	FColor PendingScreenColor = FColor::White;

	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle PostLoadMapHandle;

	// 덮어쓴 CDO 의 클래스 · 데이터 에셋 — GC 로 내려갔다 다시 올라오면 덮어쓴 값이 사라진다
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> PinnedObjects;
};
