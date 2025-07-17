// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnemyInterface.generated.h"

/**
 * UEnemyInterface
 *
 * 적 대상 지정 및 조회용 인터페이스 클래스 정의문서
 */
UINTERFACE(MinimalAPI)
class UEnemyInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * IEnemyInterface
 *
 * 적 객체가 구현해야 하는 전투 대상 관리 인터페이스이며
 * 블루프린트 및 C++ 양쪽에서 사용 가능함
 */
class DAERUNE_API IEnemyInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/**
	 * SetCombatTarget
	 *
	 * 전투 타겟 Actor 설정 함수 선언임
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetCombatTarget(AActor* InCombatTarget);

	/**
	 * GetCombatTarget
	 *
	 * 현재 설정된 전투 타겟 Actor 반환 함수 선언임
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	AActor* GetCombatTarget() const;
};
