// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Phase/DRPhaseBase.h"
#include "Phase/DRPhase3DataTypes.h"
#include "DRPhase3.generated.h"

class ADRPoisonGasActor;
class ADREnemy;
class ADRCharacter;

// 독가스 스폰 포인트 타입
UENUM(BlueprintType)
enum class EPoisonGasSpawnPointType : uint8
{
	Normal			UMETA(DisplayName = "Normal (Green)"),		// 일반 스폰 지점 (초록색, 36개)
	CleanserLinked	UMETA(DisplayName = "Cleanser Linked (Blue)")	// 클렌저 연결 지점 (파란색, 12개)
};

// 독가스 스폰 포인트 데이터
USTRUCT(BlueprintType)
struct FPoisonGasSpawnPointData
{
	GENERATED_BODY()

	// 스폰 위치
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	// 스폰 포인트 타입
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPoisonGasSpawnPointType SpawnType = EPoisonGasSpawnPointType::Normal;

	// 연결된 클렌저 사이트 (파란색인 경우)
	// 예: "Cleanser_TopLeft", "Cleanser_TopRight", "Cleanser_BottomCenter"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "SpawnType == EPoisonGasSpawnPointType::CleanserLinked"))
	FName LinkedCleanserTag = NAME_None;
};

// 웨이브 상태 열거형
UENUM(BlueprintType)
enum class EWaveState : uint8
{
	Waiting			UMETA(DisplayName = "Waiting"),		// 웨이브 시작 대기
	InProgress		UMETA(DisplayName = "In Progress"),	// 웨이브 진행 중
	Rest			UMETA(DisplayName = "Rest"),			// 휴식 시간
	Completed		UMETA(DisplayName = "Completed")		// 웨이브 완료
};

// 웨이브 데이터 구조체 (웨이브 넘버별 고정 데이터)
USTRUCT(BlueprintType)
struct FWaveData
{
	GENERATED_BODY()

	// 웨이브 플레이 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PlayDuration = 50.0f;

	// 웨이브 휴식 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RestDuration = 10.0f;

	// 기본 스폰 주기 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float BaseSpawnInterval = 5.0f;

	// 기본 플레이어당 몬스터 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 BaseMonstersPerPlayer = 8;

	FWaveData()
	{
		PlayDuration = 50.0f;
		RestDuration = 10.0f;
		BaseSpawnInterval = 5.0f;
		BaseMonstersPerPlayer = 8;
	}
};

// 웨이브 레벨별 수정자 구조체
USTRUCT(BlueprintType)
struct FWaveLevelModifier
{
	GENERATED_BODY()

	// 몬스터 수 증가율 (1.0 = 100%, 1.2 = 120%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MonsterCountMultiplier = 1.0f;

	// 스폰 간격 감소율 (1.0 = 100%, 0.8 = 80% = 20% 감소)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float SpawnIntervalMultiplier = 1.0f;

	// 돌진형 몬스터 스폰 확률 (0.0 ~ 1.0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float RushMonsterSpawnChance = 0.0f;

	// 은신형 몬스터 스폰 확률 (0.0 ~ 1.0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float StealthMonsterSpawnChance = 0.0f;

	// 유독 가스 생성 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bSpawnToxicGas = false;

	// 엘리트 보스 스폰 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bSpawnEliteBoss = false;

	FWaveLevelModifier()
	{
		MonsterCountMultiplier = 1.0f;
		SpawnIntervalMultiplier = 1.0f;
		RushMonsterSpawnChance = 0.0f;
		StealthMonsterSpawnChance = 0.0f;
		bSpawnToxicGas = false;
		bSpawnEliteBoss = false;
	}
};

// 클렌저 사이트 UI 준비 완료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCleanserSiteReadySignature, ADRCleanserSite*, FirstCleanserSite, ADRCleanserSite*, SecondCleanserSite);

/**
 * Phase 3: 클렌저 사이트 방어
 * 웨이브 형태로 공격하는 적들로부터 클렌저 사이트를 방어
 */
UCLASS()
class DAERUNE_API UDRPhase3 : public UDRPhaseBase
{
	GENERATED_BODY()

public:
	UDRPhase3();

	// ========== 페이즈 생명주기 ==========
	virtual void OnPhaseStart() override;
	virtual void OnPhaseEnd() override;

	// ========== 적 관리 ==========
	UFUNCTION()
	void OnEliteEnemyDeath(AActor* DeadEnemy);

	// 클렌저 사이트 초기화 완료 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnCleanserSiteReadySignature OnCleanserSiteReadyDelegate;

protected:
	virtual void BeginDestroy() override;

	// ========== 웨이브 시스템 ==========
	
	// 다음 웨이브 시작
	UFUNCTION()
	void StartNextWave();

	// 웨이브 종료 처리
	UFUNCTION()
	void EndCurrentWave();

	// 휴식 시간 시작
	UFUNCTION()
	void StartRestTime();

	// 휴식 시간 종료
	UFUNCTION()
	void EndRestTime();

	// 웨이브 스폰 처리
	UFUNCTION()
	void ProcessWaveSpawn();

	// ========== 스폰 시스템 ==========

	// 몬스터 스폰 (플레이어 위치 기반)
	void SpawnMonstersAroundPlayers(const TArray<ADRCharacter*>& PlayerCharacters, int32 RequiredSpawnCount);

	// 스폰 위치 계산 (플레이어 주변)
	FVector CalculateSpawnLocation(const FVector& PlayerLocation) const;

	// 웨이브 레벨에 따라 스폰할 몬스터 클래스 선택
	TSubclassOf<ADREnemy> SelectMonsterClass(int32 WaveLevel) const;

	// 특수 몬스터 스폰
	void SpawnEliteMonster(const FVector& SpawnLocation);

	// ========== 웨이브 레벨 시스템 ==========

	// 웨이브 레벨에 따른 수정자 가져오기
	FWaveLevelModifier GetWaveLevelModifier(int32 WaveLevel) const;

	// DataTable에서 웨이브 데이터 가져오기 (없으면 기본값)
	FWaveData GetWaveData(int32 WaveNumber) const;

	// GameBalanceConfig에서 Phase3 설정 로드
	void LoadPhase3ConfigFromBalanceConfig();

	// ========== 클렌저 사이트 ==========

	// 클렌저 사이트 초기화
	void InitializeCleanserSite();

	// 활성화된 스폰 포인트 계산
	void InitializeActiveSpawnPoints();

	// 클렌저 사이트 파괴 처리
	UFUNCTION()
	void OnCleanserSiteDestroyed(ADRCleanserSite* DestroyedSite) const;

	// 클렌저 사이트 체력 50% 이하 핸들러
	UFUNCTION()
	void OnCleanserSiteHealthBelowHalf();

	// 클렌저 사이트 체력 0 핸들러
	UFUNCTION()
	void OnCleanserSiteHealthZero();

	// ========== 웨이브 레벨 관리 ==========

	// 웨이브 레벨 증가
	void IncreaseWaveLevel(int32 Amount = 1);

	// ========== 게임 오버/승리 조건 ==========

	// 게임 오버 조건 체크
	void CheckGameOverConditions() const;

	// 게임 승리 조건 체크
	void CheckVictoryConditions();

	// 몬스터 수 체크 (100마리 이상 시 게임 오버)
	bool IsMonsterCountExceeded() const;

	// ========== 환경 위협 ==========

	// 8개의 랜덤 스폰 위치 선택
	TArray<int32> SelectRandomSpawnPointIndices() const;

	// 유독 가스 생성 (레벨 4)
	void SpawnToxicGas();

	// 독가스 액터 스폰
	void SpawnPoisonGasActor();

	// 유독 가스 제거
	void RemoveToxicGas();

	// ========== 독가스 그리드 설정 (새 방식) ==========

	// 그리드 기반 자동 생성 사용 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase3|PoisonGas")
	bool bUseGridBasedSpawnPoints = true;

	// 그리드 설정 (bUseGridBasedSpawnPoints = true일 때 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase3|PoisonGas", meta = (EditCondition = "bUseGridBasedSpawnPoints"))
	FPoisonGasGridConfig PoisonGasGridConfig;

	// ========== 수동 설정 (레거시 방식) ==========

	// 수동 스폰 포인트 (bUseGridBasedSpawnPoints = false일 때 사용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase3|PoisonGas|Legacy", meta = (EditCondition = "!bUseGridBasedSpawnPoints"))
	TArray<FPoisonGasSpawnPointData> ManualSpawnPoints;

	// ========== 런타임 데이터 ==========

	// 자동 생성된 스폰 포인트 (런타임)
	TArray<FPoisonGasSpawnPointData> AllPoisonGasSpawnPoints;

	// 독가스 스폰 주기 (GameBalanceConfig에서 로드)
	float PoisonGasSpawnInterval = 10.0f;

	// 스폰 포인트를 찾을 태그 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase3|Config")
	FName SpawnPointTag = "Phase3SpawnPoint";

	// ========== 그리드 헬퍼 함수 ==========

	// 그리드 기반 스폰 포인트 생성
	void GenerateGridSpawnPoints();

	// 클렌저에서 가장 가까운 N개 포인트를 CleanserLinked로 설정
	void AssignCleanserLinkedPoints();

private:
	// 엘리트 보스 태그 부여
	void GrantEliteBossTag();

	// 엘리트 보스 태그 삭제
	void RemoveEliteBossTag();
	
	// ========== 설정 데이터 (DataTable 기반) ==========

	// 웨이브 데이터 DataTable (RowName: "Wave1" ~ "Wave5")
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Config|DataTable")
	TObjectPtr<UDataTable> WaveDataTable;

	// 웨이브 레벨 수정자 DataTable (RowName: "Level1" ~ "Level5")
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Config|DataTable")
	TObjectPtr<UDataTable> WaveLevelModifierTable;

	// ========== Fallback 설정 (DataTable 미설정 시 사용) ==========

	// 웨이브 데이터 배열 (DataTable 미사용 시)
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Config|Fallback")
	TArray<FWaveData> WaveDataArray;

	// 웨이브 레벨별 수정자 (DataTable 미사용 시)
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Config|Fallback")
	TMap<int32, FWaveLevelModifier> WaveLevelModifiers;

	// ========== 런타임 설정 (GameBalanceConfig에서 로드) ==========

	// 최대 몬스터 수 (GameBalanceConfig.Phase3에서 로드)
	int32 MaxMonsterCount = 100;

	// 방어 시간 (GameBalanceConfig.Phase3에서 로드)
	float DefenseDuration = 300.0f;

	// 몬스터 스폰 거리 설정 (GameBalanceConfig.Phase3에서 로드)
	float SpawnDistanceMin = 500.0f;
	float SpawnDistanceMax = 2000.0f;

	// 레벨 4+ 광폭화 체력 비율 (GameBalanceConfig.Phase3에서 로드)
	float HighLevelEnrageThreshold = 0.25f;

	// ========== 몬스터 클래스 ==========

	// 일반 몬스터 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Enemies")
	TSubclassOf<ADREnemy> NormalMonsterClass;

	// 돌진형 몬스터 클래스 (레벨 2)
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Enemies")
	TSubclassOf<ADREnemy> RushMonsterClass;

	// 은신형 몬스터 클래스 (레벨 3)
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Enemies")
	TSubclassOf<ADREnemy> StealthMonsterClass;

	// 엘리트 보스 클래스 (레벨 5)
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Enemies")
	TSubclassOf<ADREnemy> EliteBossClass;

	// 독가스 액터 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Phase3|Enemies")
	TSubclassOf<ADRPoisonGasActor> PoisonGasActorClass;

	// ========== 런타임 데이터 ==========

	// 현재 웨이브 번호 (1~5, 진행도)
	UPROPERTY()
	int32 CurrentWaveNumber;

	// 현재 웨이브 레벨 (1~5, 난이도)
	UPROPERTY()
	int32 CurrentWaveLevel;

	// 현재 웨이브 상태
	UPROPERTY()
	EWaveState CurrentWaveState;

	// 엘리트 보스 참조 (레벨 5)
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> EliteBosses;

	// 유독 가스 액터들 (레벨 4)
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ToxicGasActors;

	// 현재 활성화된 스폰 포인트 인덱스 (44개, 런타임 계산)
	TArray<int32> ActiveSpawnPointIndices;

	// 현재 활성화된 파란색 포인트 인덱스 (8개, 런타임 계산)
	TArray<int32> ActiveBlueSpawnPointIndices;

	// ========== 타이머 핸들 ==========

	// 웨이브 타이머
	FTimerHandle WaveTimerHandle;

	// 스폰 타이머
	FTimerHandle SpawnTimerHandle;

	// 방어 시간 타이머
	FTimerHandle DefenseTimerHandle;

	// 독가스 스폰 타이머
	FTimerHandle PoisonGasSpawnTimerHandle;

	// ========== 추적 변수 ==========

	// 현재 웨이브 스폰 틱
	int32 CurrentSpawnTick;

	// 현재 웨이브 최대 스폰 틱
	int32 TotalSpawnTick;
	
	// 현재 웨이브 스폰 횟수
	int32 CurrentSpawnCount;

	// 현재 웨이브 목표 스폰 횟수
	int32 TotalSpawnCount;

	// 엘리트 몬스터 스폰 상태
	bool bEliteBossSpawned;

	// 클렌저 사이트별 50% 이하 체력 트리거 여부 (중복 레벨 증가 방지)
	TMap<TObjectPtr<ADRCleanserSite>, bool> CleanserSiteHalfHealthTriggered;

	// 방어 시작 시간
	float DefenseStartTime;

	// 웨이브 타이머 업데이트
	FTimerHandle WaveTimerUpdateHandle;

	// 웨이브 남은 시간 추적
	float WaveTimeRemaining;
	float RestTimeRemaining;
};