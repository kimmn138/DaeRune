// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRAutoSlidingDoor.generated.h"

UENUM(BlueprintType)
enum class EDoorState : uint8
{
	Closed,
	Opening,
	Open,
	Closing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDoorStateChanged);

class UBoxComponent;

UCLASS()
class DAERUNE_API ADRAutoSlidingDoor : public AActor
{
	GENERATED_BODY()
	
public:
	ADRAutoSlidingDoor();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 외부에서 호출할 함수들
	UFUNCTION(BlueprintCallable, Category = "Door")
	void OpenDoor();

	UFUNCTION(BlueprintCallable, Category = "Door")
	void CloseDoor();

	UFUNCTION(BlueprintCallable, Category = "Door")
	void SetDoorLocked(bool bLocked);

	// Delegate
	UPROPERTY(BlueprintAssignable, Category = "Door")
	FOnDoorStateChanged OnDoorOpened;

	UPROPERTY(BlueprintAssignable, Category = "Door")
	FOnDoorStateChanged OnDoorClosed;

protected:
	virtual void BeginPlay() override;

	// 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftDoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightDoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerVolume;

	/*UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> DoorAudioComponent;*/

	// 에디터 설정 변수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Settings")
	float DoorOpenOffset = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Settings")
	float DoorSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Settings")
	bool bStartOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Settings")
	bool bCloseOnPlayerOverlap = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Settings")
	bool bOpenOnPlayerLeave = false;

	// 사운드
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Sound")
	TObjectPtr<USoundBase> DoorOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Sound")
	TObjectPtr<USoundBase> DoorCloseSound;

private:
	UPROPERTY(ReplicatedUsing = OnRep_DoorState)
	EDoorState CurrentDoorState = EDoorState::Closed;

	UPROPERTY(Replicated)
	bool bIsLocked = false;

	FVector LeftDoorClosedLocation;
	FVector RightDoorClosedLocation;
	FVector LeftDoorOpenLocation;
	FVector RightDoorOpenLocation;

	float CurrentOpenRatio = 0.0f;
	float TargetOpenRatio = 0.0f;

	UFUNCTION()
	void OnRep_DoorState();

	void UpdateDoorPosition(float DeltaTime);
	void SetDoorState(EDoorState NewState);
	void CalculateDoorLocations();

	// Overlap 이벤트
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	int32 PlayersInTrigger = 0;

};
