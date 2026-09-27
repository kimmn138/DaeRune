// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2CctvBoard.generated.h"

class UMaterialInstanceDynamic;
class UTexture;
class UTexture2D;

/** CCTV 2초 스텝 1개 (Plan6 §4.10.4) */
USTRUCT()
struct FS2CctvStep
{
	GENERATED_BODY()

	// 이 스텝에 사진을 띄울 두 화면의 인덱스. 나머지 화면은 오류 화면이 된다.
	UPROPERTY() uint8 NormalScreenA = 0;
	UPROPERTY() uint8 NormalScreenB = 1;
};

/**
 * 타깃 후보 1개 — "이 캐릭터를 찾아라" 한 건 (Plan6 §14.2.4)
 *
 * ★개수는 서버가 정하는 값이 아니라 **그림에 이미 그려져 있는 사실**이다.
 *   CCTV 사진 6장을 직접 보고 그 캐릭터가 몇 명 나오는지 세어 여기에 적어야 한다.
 *   여기 적은 값이 금고 마지막 자리가 되므로, 틀리면 금고가 열리지 않는다.
 */
USTRUCT(BlueprintType)
struct FS2CctvTarget
{
	GENERATED_BODY()

	// 찾아야 할 캐릭터 1명의 그림. CCTV 화면에 쓰이는 장면 사진과는 별개 에셋이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV")
	TObjectPtr<UTexture2D> TargetImage;

	// 이 캐릭터가 CCTV 사진 전체에 총 몇 명 나오는지 (= 금고 마지막 자리).
	// 0도 유효한 값이다 — "아무리 봐도 없는데?"가 답인 함정 케이스 (§14.2.4).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV", meta = (ClampMin = "0", ClampMax = "9"))
	int32 AppearCount = 0;
};

/**
 * CCTV 기믹 (Plan6 §14.2.4)
 *
 * 화면 6개에는 **여러 캐릭터가 등장하는 장면 사진**이 한 장씩 붙어 있다.
 * 2초마다 그중 2개만 사진을 보여주고 나머지는 오류 화면을 띄운다 —
 * 6장을 한눈에 볼 수 없게 만들어서, 여러 주기에 걸쳐 조각조각 확인하게 만드는 장치다.
 *
 * 힌트 표시판(`HintMesh`)에는 **찾아야 할 캐릭터 1명의 그림**이 뜬다 (CCTV 사진과 별개 에셋).
 * 플레이어는 그 캐릭터가 CCTV 사진들 안에 **몇 명 나오는지** 세고, 그 개수(0~9)를 금고에 입력한다.
 *
 * ★★데이터 흐름이 다른 두 퍼즐과 반대다 (2026-09-16 확정).
 *   8퍼즐·스위치는 서버가 난수로 자릿수를 정해 주입하지만, CCTV의 개수는 **그림에 이미 그려져 있다.**
 *   그래서 이 보드가 후보 중 하나를 뽑아 자릿수를 **결정하고**, 페이즈가 그것을 읽어 금고에 넣는다.
 *   (`ChooseTarget()` → `GetSecretDigit()` → `ADRS2Safe::SetSecretCode()`)
 *
 * ★"해결" 상태가 없다. 스크린에 답을 표시하지 않으며, 금고 입력으로만 검증된다.
 *
 * 복제는 시퀀스 + 시작 서버시각 + 뽑힌 후보 인덱스뿐이고, 각 클라가 서버시각으로 현재 스텝을 계산한다.
 */
UCLASS()
class DAERUNE_API ADRS2CctvBoard : public AActor
{
	GENERATED_BODY()

public:
	ADRS2CctvBoard();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// [서버] 후보 중 하나를 무작위로 뽑아 이번 판의 타깃으로 삼고, 화면 시퀀스 재생을 시작한다.
	// 페이즈가 금고 코드를 만들기 전에 불러야 한다 (자릿수가 여기서 정해지므로).
	void ChooseTarget();

	// 이번 판의 금고 마지막 자리 (= 뽑힌 캐릭터가 CCTV 사진에 나오는 수).
	// ChooseTarget() 전이거나 후보가 없으면 INDEX_NONE 을 반환한다.
	UFUNCTION(BlueprintCallable, Category = "S2|CCTV")
	int32 GetSecretDigit() const;

	UFUNCTION(BlueprintCallable, Category = "S2|CCTV")
	int32 GetScreenCount() const { return ScreenCount; }

protected:
	virtual void BeginPlay() override;

	// ========== 화면 ==========

	// ★화면 6개는 **하나의 스태틱 메시**에 있는 **머티리얼 슬롯 6개**다 (2026-08-18 변경).
	//   메시를 6개 두는 대신 슬롯을 나눠 쓰므로 드로우콜과 배치 작업이 줄어든다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|CCTV")
	TObjectPtr<UStaticMeshComponent> ScreenMesh;

	// 화면 개수 (= 사용할 머티리얼 슬롯 수). 스텝마다 서로 다른 화면 2개가 켜져야 하므로 최소 2개.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV", meta = (ClampMin = "2"))
	int32 ScreenCount = 6;

	// 화면 인덱스 -> 머티리얼 슬롯(엘리먼트) 번호.
	// 비워두면 0, 1, 2 ... 순서를 그대로 쓴다. 메시에 화면이 아닌 슬롯(프레임 등)이 섞여 있거나
	// 슬롯 순서가 화면 배치 순서와 다를 때 채운다 (예: 1, 2, 3, 4, 5, 6).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV")
	TArray<int32> ScreenMaterialSlots;

	// 머티리얼의 텍스처 파라미터 이름.
	// glTF 임포트 머티리얼(MI_Unlit_Opaque_DS 계열)은 BaseColorTexture 를 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV")
	FName ScreenTextureParameterName = TEXT("BaseColorTexture");

	// 오류 화면. 화면이 꺼져 있는 동안 표시된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV")
	TObjectPtr<UTexture2D> ErrorImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV", meta = (ClampMin = "0.1"))
	float StepDuration = 2.f;

	// 시퀀스 길이 (= 1주기의 스텝 수).
	// 1주기 안에 모든 화면이 골고루 나와야 하므로 ScreenCount 의 절반 이상이어야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV", meta = (ClampMin = "1"))
	int32 StepCount = 12;

	// ========== 타깃 후보 ==========

	// ★찾아야 할 캐릭터 후보들. 판이 시작될 때 서버가 이 중 하나를 무작위로 뽑는다 (보통 4개).
	//   각 후보의 AppearCount 가 그대로 금고 마지막 자리가 되므로 실제 그림과 맞게 적어야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV|Target")
	TArray<FS2CctvTarget> TargetCandidates;

	// ========== 힌트 표시판 ==========

	// 찾아야 할 캐릭터 그림을 보여주는 표시판. 화면(ScreenMesh)과는 별개 메시다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|CCTV|Hint")
	TObjectPtr<UStaticMeshComponent> HintMesh;

	// 힌트 표시판에서 텍스처를 갈아끼울 머티리얼 슬롯
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV|Hint", meta = (ClampMin = "0"))
	int32 HintMaterialSlot = 0;

	// 힌트 표시판의 텍스처 파라미터 이름.
	// 비워두면 ScreenTextureParameterName 을 그대로 쓴다 (보통 같은 계열 머티리얼이므로).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|CCTV|Hint")
	FName HintTextureParameterName = NAME_None;

	// ========== 복제 상태 ==========

	UPROPERTY(ReplicatedUsing = OnRep_Sequence)
	TArray<FS2CctvStep> Sequence;

	UPROPERTY(Replicated)
	float StartServerTime = 0.f;

	// 이번 판에 뽑힌 후보의 인덱스. INDEX_NONE = 아직 안 정해짐 (페이즈가 ChooseTarget 을 부르기 전).
	UPROPERTY(ReplicatedUsing = OnRep_ChosenTargetIndex)
	int32 ChosenTargetIndex = INDEX_NONE;

	UFUNCTION()
	void OnRep_Sequence();

	UFUNCTION()
	void OnRep_ChosenTargetIndex();

private:
	// 1주기 안에 모든 화면이 골고루 나오는 유한 시퀀스를 만든다 (서버).
	// ★타깃 개수는 그림이 결정하므로, 여기서 조절할 것은 "어느 화면을 언제 보여줄까"뿐이다.
	void BuildSequence();

	// 현재 스텝을 화면에 반영
	void ApplyStep(int32 StepIndex);

	// 머티리얼 인스턴스 생성 + 각 화면의 원본 사진 캐시 (텍스처 파라미터 교체용)
	void EnsureScreenMIDs();

	// 힌트 표시판의 MID 를 만든다 (없으면 nullptr)
	UMaterialInstanceDynamic* EnsureHintMID();

	// 힌트 표시판에 뽑힌 캐릭터 그림을 띄운다.
	// BeginPlay 와 OnRep_ChosenTargetIndex 양쪽에서 불리며, 준비가 덜 됐으면 조용히 넘어간다.
	void ApplyHintTexture();

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ScreenMIDs;

	// 화면별 원래 사진. MID 를 만드는 시점에 머티리얼에서 읽어 두고, 화면이 켜질 때 이 값으로 되돌린다.
	UPROPERTY()
	TArray<TObjectPtr<UTexture>> ScreenBaseTextures;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HintMID;

	// 마지막으로 적용한 스텝 (같은 스텝을 매 틱 다시 그리지 않기 위함)
	int32 LastAppliedStep = INDEX_NONE;
};
