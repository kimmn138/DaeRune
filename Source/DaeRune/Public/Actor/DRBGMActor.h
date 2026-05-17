// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRBGMActor.generated.h"

class UAudioComponent;
class USoundBase;
class USoundClass;

UENUM(BlueprintType)
enum class EDRBGMSlot : uint8
{
	None     UMETA(DisplayName = "None"),
	MainMenu UMETA(DisplayName = "Main Menu"),
	Lobby    UMETA(DisplayName = "Lobby"),
	Tutorial UMETA(DisplayName = "Tutorial"),
	Stage    UMETA(DisplayName = "Stage (Playlist + Boss)")
};

/**
 * 맵 전체에서 동기화된 BGM을 재생하는 액터
 * - 단일 트랙 슬롯(MainMenu/Lobby/Tutorial): Sound Cue Looping으로 무한 재생
 * - Stage 슬롯: DataAsset의 BGM_StagePlaylist를 순차 재생 + 엘리트 보스 등장 시 BGM_Boss로 크로스페이드
 */
UCLASS()
class DAERUNE_API ADRBGMActor : public AActor
{
	GENERATED_BODY()

public:
	ADRBGMActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// BGM 재생 (서버에서 호출)
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void PlayBGM();

	// BGM 정지 (페이드아웃)
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void StopBGM(float FadeOutDuration = 2.0f);

	// 보스 BGM으로 전환 (서버에서 호출)
	UFUNCTION(BlueprintCallable, Category = "BGM|Boss")
	void StartBossBGM();

	// 보스 BGM 종료 후 스테이지 플레이리스트로 복귀 (서버에서 호출)
	UFUNCTION(BlueprintCallable, Category = "BGM|Boss")
	void EndBossBGM();

protected:
	virtual void BeginPlay() override;

	// DataAsset의 어떤 BGM을 재생할지 선택
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	EDRBGMSlot BGMSlot = EDRBGMSlot::None;

	// 시작 시 자동 재생
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	bool bAutoPlay = true;

	// 첫 페이드 인 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	float FadeInDuration = 2.0f;

	// 스테이지 플레이리스트 트랙 사이 크로스페이드 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM|Stage")
	float TrackCrossfadeDuration = 0.8f;

	// 보스 BGM 전환 크로스페이드 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM|Boss")
	float BossCrossfadeDuration = 1.5f;

	// 볼륨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Volume = 1.0f;

	// 현재 플레이리스트 인덱스 (Stage 슬롯 전용, 복제됨)
	UPROPERTY(ReplicatedUsing = OnRep_CurrentTrackIndex)
	int32 CurrentTrackIndex = -1;

	// 보스 BGM 활성 상태 (복제됨)
	UPROPERTY(ReplicatedUsing = OnRep_BossBGMActive)
	bool bBossBGMActive = false;

	UFUNCTION()
	void OnRep_CurrentTrackIndex();

	UFUNCTION()
	void OnRep_BossBGMActive();

private:
	// 메인 BGM 오디오 컴포넌트 (단일 트랙 또는 플레이리스트 재생용)
	UPROPERTY()
	TObjectPtr<UAudioComponent> AudioComponent;

	// 보스 BGM 전용 오디오 컴포넌트 (크로스페이드용 분리)
	UPROPERTY()
	TObjectPtr<UAudioComponent> BossAudioComponent;

	// DataAsset에서 로드한 단일 BGM (Stage가 아닌 슬롯용)
	UPROPERTY()
	TObjectPtr<USoundBase> SingleTrackSound;

	// DataAsset에서 로드한 스테이지 플레이리스트
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> StagePlaylist;

	// DataAsset에서 로드한 보스 BGM
	UPROPERTY()
	TObjectPtr<USoundBase> BossSound;

	// DataAsset에서 사운드 로드
	void LoadSoundsFromDataAsset();

	// 단일 트랙 재생 (MainMenu/Lobby/Tutorial)
	void PlaySingleTrackLocal();

	// 플레이리스트 트랙 재생 (Stage)
	void PlayPlaylistTrackLocal(int32 Index, float FadeIn);

	// 보스 BGM 로컬 재생
	void PlayBossBGMLocal();

	// 보스 BGM 중지 및 플레이리스트 복귀 로컬 처리
	void StopBossBGMLocal();

	// 오디오 종료 콜백 (서버에서 다음 트랙 진행)
	UFUNCTION()
	void OnAudioComponentFinished();
};
