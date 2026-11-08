// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2TeleportGate.generated.h"

class UBoxComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ADRCharacter;

/** 게이트 통과 방식 (Plan6 §4.12) */
UENUM(BlueprintType)
enum class ES2GateMode : uint8
{
	// 진입한 캐릭터만 이동 — D1(방1->방2), R4(방4->방3)
	Individual		UMETA(DisplayName = "Individual"),

	// 부품 소지자가 진입하면 생존자 전원을 목적지로 회수 — 방2 출구(E2)
	TeamOnCarrier	UMETA(DisplayName = "Team On Carrier")
};

/** 게이트 진입 자격 */
UENUM(BlueprintType)
enum class ES2GateEntryRule : uint8
{
	Anyone			UMETA(DisplayName = "Anyone"),

	// 부품 소지자만 통과 — D3(방3->방4)
	CarrierOnly		UMETA(DisplayName = "Carrier Only")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGateUsed, ADRCharacter*, Who);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamRecalled);

/**
 * 스테이지2 텔레포트 게이트 (Plan6 §4.12)
 *
 * 문이 실제로 열리지 않고 발광만 하며, 근접한 플레이어를 목적지로 순간이동시킨다.
 * 자동문(ADRAutoSlidingDoor)과 성격이 완전히 달라 별개 클래스로 둔다.
 *
 *   D1 (방1 -> 방2)  : Individual / Anyone
 *   E2 (방2 출구)    : TeamOnCarrier / Anyone, 상시 활성
 *   D3 (방3 -> 방4)  : Individual / CarrierOnly + bDeactivateOnUse
 *   R4 (방4 -> 방3)  : Individual / Anyone
 *
 * 길 안내 강조: 페이즈가 SetGuideHighlight 로 "다음에 갈 문"을 지정하면 그 게이트가 활성인 동안 문이 빛난다.
 *   D1 = 방1 클리어 ~ 부품 소지자 방2 퇴장, D3 = 방3 입장 ~ 부품 소지자 방4 입장 (방4 실패로 재시도하면 다시)
 *
 * 위치 이동은 서버 권한으로만 수행한다.
 */
UCLASS()
class DAERUNE_API ADRS2TeleportGate : public AActor
{
	GENERATED_BODY()

public:
	ADRS2TeleportGate();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 발광/작동 여부 (서버)
	UFUNCTION(BlueprintCallable, Category = "S2|Gate")
	void SetGateActive(bool bNewActive);

	UFUNCTION(BlueprintCallable, Category = "S2|Gate")
	bool IsGateActive() const { return bActive; }

	// 진입 자격 변경 (서버).
	// 방4 플레이어가 사망하면 부품이 방4 안에 남으므로 CarrierOnly -> Anyone 으로 전환해야 한다.
	UFUNCTION(BlueprintCallable, Category = "S2|Gate")
	void SetEntryRule(ES2GateEntryRule NewRule);

	UFUNCTION(BlueprintCallable, Category = "S2|Gate")
	ES2GateEntryRule GetEntryRule() const { return EntryRule; }

	// 길 안내 강조 (서버). 켜 두면 게이트가 활성인 동안 문이 빛나 다음에 갈 곳을 알린다.
	// 활성과 따로 두는 이유: D3 는 두더지 클리어 뒤에도 다시 켜지지만 그때는 방5로 가야 하므로 빛나면 안 된다.
	UFUNCTION(BlueprintCallable, Category = "S2|Gate")
	void SetGuideHighlight(bool bNewHighlight);

	UFUNCTION(BlueprintCallable, Category = "S2|Gate")
	bool IsGuideHighlighted() const { return bGuideHighlight; }

	// 서버 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "S2|Gate")
	FOnGateUsed OnGateUsed;

	UPROPERTY(BlueprintAssignable, Category = "S2|Gate")
	FOnTeamRecalled OnTeamRecalled;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Gate")
	TObjectPtr<UStaticMeshComponent> GateMesh;

	// 근접 판정 박스 (문 앞). 판정은 서버에서만 수행한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Gate")
	TObjectPtr<UBoxComponent> TriggerBox;

	// 기본 도착 지점 (DestinationOverride 가 없을 때 사용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Gate")
	TObjectPtr<USceneComponent> DefaultDestination;

	// 레벨에 배치한 도착 지점 액터 (있으면 우선)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Gate")
	TObjectPtr<AActor> DestinationOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Gate")
	ES2GateMode Mode = ES2GateMode::Individual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Gate")
	ES2GateEntryRule EntryRule = ES2GateEntryRule::Anyone;

	// 1명이 통과하면 자동으로 비활성화 (D3 = true)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Gate")
	bool bDeactivateOnUse = false;

	// 레벨 배치 시 활성 상태로 시작할지 (E2 = true)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Gate")
	bool bStartActive = false;

	// 동시 진입 시 목적지에서 서로 겹치지 않도록 분산할 반경
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Gate", meta = (ClampMin = "0.0"))
	float DestinationSpreadRadius = 150.f;

	// 길 안내 강조 오버레이 (MI_GateHighlight). 문 메시 위에 한 겹 더 그리고 GuideHighlightParameterName(0/1)으로 켜고 끈다.
	// 반투명(After DOF)이라 장난감 화풍 포스트 프로세스를 거치지 않고 그대로 보인다. 비우면 강조 없음.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Gate")
	TObjectPtr<UMaterialInterface> GuideHighlightMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Gate")
	FName GuideHighlightParameterName = TEXT("Highlight");

	UPROPERTY(ReplicatedUsing = OnRep_bActive, BlueprintReadOnly, Category = "S2|Gate")
	bool bActive = false;

	UFUNCTION()
	void OnRep_bActive();

	UPROPERTY(ReplicatedUsing = OnRep_bGuideHighlight, BlueprintReadOnly, Category = "S2|Gate")
	bool bGuideHighlight = false;

	UFUNCTION()
	void OnRep_bGuideHighlight();

	// 발광 전환 연출 (머티리얼 파라미터/사운드)
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Gate")
	void OnGateActiveChanged(bool bNowActive);

	// 진입 자격 미달로 거부되었을 때의 로컬 안내 연출
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Gate")
	void OnEntryDenied(ADRCharacter* Who);

	// 순간이동 연출 (출발/도착 VFX·사운드)
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayTeleportFX(ADRCharacter* Who);

	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Gate")
	void OnTeleportFX(ADRCharacter* Who);

private:
	// 목적지 트랜스폼 조회
	void GetDestinationTransform(FVector& OutLocation, FRotator& OutRotation) const;

	// 한 명을 목적지로 이동. SlotTotal > 1 이면 원형으로 분산 배치한다.
	void TeleportOne(ADRCharacter* Character, int32 SlotIndex, int32 SlotTotal);

	// 활성이면서 길 안내 강조일 때만 문을 빛낸다 (서버/클라 공통)
	void RefreshGuideHighlight();

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GuideHighlightMID;
};
