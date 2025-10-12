// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageManager.generated.h"

class UDRPhaseBase;
class UDRPhase1;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPhase1Completed);

UCLASS()
class DAERUNE_API AStageManager : public AActor
{
	GENERATED_BODY()
	
public:	
    AStageManager();
public:
    // 페이즈 인스턴스(간단히 소유)
    UPROPERTY(EditAnywhere, Category = "Phases")
    TSubclassOf<UDRPhase1> Phase1Class;

    UPROPERTY(BlueprintReadOnly) UDRPhaseBase* CurrentPhase = nullptr;
    UPROPERTY(BlueprintAssignable) FOnPhase1Completed OnPhase1Completed;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // 전환
    UFUNCTION(BlueprintCallable) void StartPhase1();

    // 헬퍼
    template<typename T>
    T* CreatePhase(TSubclassOf<T> Cls)
    {
        if (!Cls) return nullptr;
        T* Phase = NewObject<T>(this, Cls);
        if (Phase) Phase->Setup(this); // 매니저 주입
        return Phase;
    }
};