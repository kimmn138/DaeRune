// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "DamageTextComponent.generated.h"

/**
 * 데미지 수치를 표시하는 3D 월드 위젯 컴포넌트
 */
UCLASS()
class DAERUNE_API UDamageTextComponent : public UWidgetComponent
{
	GENERATED_BODY()
	
public:
	// 데미지 수치 설정 및 애니메이션 시작
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetDamageText(float Damage);
};
