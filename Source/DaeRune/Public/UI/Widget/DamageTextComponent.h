// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "DamageTextComponent.generated.h"

/**
 * UDamageTextComponent
 *
 * 위젯 컴포넌트 기반 데미지 텍스트 표시용 컴포넌트 클래스임
 */
UCLASS()
class DAERUNE_API UDamageTextComponent : public UWidgetComponent
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetDamageText(float Damage); // 데미지 텍스트 설정 이벤트 함수임
};
