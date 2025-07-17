// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "GameplayTagContainer.h"
#include "DebuffNiagaraComponent.generated.h"

/**
 * 디버프 나이아가라 컴포넌트 클래스 정의
 */
UCLASS()
class DAERUNE_API UDebuffNiagaraComponent : public UNiagaraComponent
{
	GENERATED_BODY()
	
public:
	UDebuffNiagaraComponent();

	// 디버프 태그 저장 변수
	UPROPERTY(VisibleAnywhere)
	FGameplayTag DebuffTag;

protected:
	virtual void BeginPlay() override;
	// 디버프 태그 변경 시 호출 처리 함수 선언
	void DebuffTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
};
