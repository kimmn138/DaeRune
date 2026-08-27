// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2CctvBoard.generated.h"

class UMaterialInstanceDynamic;

/** CCTV 2초 스텝 1개 (Plan6 §4.10.4) */
USTRUCT()
struct FS2CctvStep
{
	GENERATED_BODY()

	// 이 스텝에 정상 화면을 띄울 두 화면의 인덱스
	UPROPERTY() uint8 NormalScreenA = 0;
	UPROPERTY() uint8 NormalScreenB = 1;

	// 각 화면에 띄울 이미지 인덱스. 0 = 타깃 이미지, 1 이상 = 더미 이미지
	UPROPERTY() uint8 ImageA = 0;
	UPROPERTY() uint8 ImageB = 0;
};

/**
 * CCTV 기믹 (Plan6 §14.2.4)
 *
 * 화면 6개 중 2초마다 2개만 정상 이미지를 띄우고 나머지는 오류 화면을 띄운다.
 * 플레이어는 깜빡이는 화면들 속에서 특정 이미지가 몇 번 등장하는지 세고,
 * 그 개수(0~9)가 금고의 마지막 자리가 된다.
 *
 * ★다른 두 퍼즐과 달리 "해결" 상태가 없다. 스크린에 답을 표시하지 않으며,
 *   플레이어의 관찰 결과를 금고에 입력해야만 검증된다.
 *
 * ★노출 시퀀스는 유한 길이 + 순환 반복이어야 한다.
 *   매 스텝을 즉석 랜덤으로 뽑으면 "총 몇 개"라는 값 자체가 정의되지 않아 퍼즐이 성립하지 않는다.
 *
 * 복제는 시퀀스 + 시작 서버시각 1회뿐이고, 각 클라가 서버시각으로 현재 스텝을 계산한다.
 */
UCLASS()
class DAERUNE_API ADRS2CctvBoard : public AActor
{
	GENERATED_BODY()

public:
	ADRS2CctvBoard();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 페이즈가 타깃 이미지 등장 횟수(= 금고 마지막 자리)를 주입한다 (서버 전용).
	// 주입 시점에 시퀀스를 생성하고 재생을 시작한다.
	void SetTargetImageCount(int32 InCount);

	UFUNCTION(BlueprintCallable, Category = "S2|CCTV")
	int32 GetScreenCount() const { return ScreenCount; }

protected:
	virtual void BeginPlay() override;

	// ========== 설정 ==========

	// ★화면 6개는 **하나의 스태틱 메시**에 있는 **머티리얼 슬롯 6개**다 (2026-08-18 변경).
	//   메시를 6개 두는 대신 슬롯을 나눠 쓰므로 드로우콜과 배치 작업이 줄어든다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|CCTV")
	TObjectPtr<UStaticMeshComponent> ScreenMesh;

	// 화면 개수 (= 사용할 머티리얼 슬롯 수)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV", meta = (ClampMin = "2"))
	int32 ScreenCount = 6;

	// 화면 인덱스 -> 머티리얼 슬롯(엘리먼트) 번호.
	// 비워두면 0, 1, 2 ... 순서를 그대로 쓴다. 메시의 슬롯 순서가 화면 배치 순서와
	// 다를 때만 채운다 (예: 3, 0, 5, 1, 4, 2).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV")
	TArray<int32> ScreenMaterialSlots;

	// 머티리얼의 텍스처 파라미터 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV")
	FName ScreenTextureParameterName = TEXT("ScreenTex");

	// 세어야 할 대상 이미지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV")
	TObjectPtr<UTexture2D> TargetImage;

	// 타깃이 아닌 이미지 풀
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV")
	TArray<TObjectPtr<UTexture2D>> DummyImages;

	// 오류 화면
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV")
	TObjectPtr<UTexture2D> ErrorImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV", meta = (ClampMin = "0.1"))
	float StepDuration = 2.f;

	// 시퀀스 길이. 총 노출 슬롯 = StepCount * 2 이며 여기에 타깃을 N개 배치한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV", meta = (ClampMin = "1"))
	int32 StepCount = 12;

	// ========== 복제 상태 ==========

	UPROPERTY(ReplicatedUsing = OnRep_Sequence)
	TArray<FS2CctvStep> Sequence;

	UPROPERTY(Replicated)
	float StartServerTime = 0.f;

	UFUNCTION()
	void OnRep_Sequence();

private:
	// 타깃 N개가 정확히 포함된 유한 시퀀스를 만든다 (서버)
	void BuildSequence();

	// 현재 스텝을 화면에 반영
	void ApplyStep(int32 StepIndex);

	// 이미지 인덱스 -> 텍스처 (0 = 타깃)
	UTexture2D* IndexToTexture(uint8 ImageIndex) const;

	// 머티리얼 인스턴스 생성 (텍스처 파라미터 교체용)
	void EnsureScreenMIDs();

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ScreenMIDs;

	// 서버 전용: 금고 마지막 자리
	int32 TargetImageCount = 0;

	// 마지막으로 적용한 스텝 (같은 스텝을 매 틱 다시 그리지 않기 위함)
	int32 LastAppliedStep = INDEX_NONE;
};
