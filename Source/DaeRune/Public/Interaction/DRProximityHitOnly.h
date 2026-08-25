// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DRProximityHitOnly.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDRProximityHitOnly : public UInterface
{
	GENERATED_BODY()
};

/**
 * 근접 전용 피격 대상 (Plan6 §5.11 / §14.4.4)
 *
 * 홀로그램 두더지처럼 "일정 거리 안에서의 공격만 유효하고, 그 밖의 공격은 투과"하는 대상을 위한 인터페이스.
 *
 * 프로젝트의 모든 플레이어 공격은 UDRAbilitySystemLibrary::ApplyDamageEffect 단일 관문을 지난다.
 * 그 진입부에서 이 인터페이스를 구현한 대상을 가로채므로, 공격 종류(투사체/근접/대시/제트점프)마다
 * 손댈 필요가 없다.
 *
 * ★중요: 가로챈 뒤에는 GameplayEffect 를 적용하지 않는다.
 *   AttributeSet 의 PostGameplayEffectExecute 는 대상이 ACharacter 라고 가정하고
 *   Props.TargetCharacter 를 널 가드 없이 참조하는 경로가 있어(DREnemyAttributeSet.cpp:161),
 *   Actor 기반 대상에 GE 를 적용하면 크래시 위험이 있다.
 *   따라서 체력/어트리뷰트를 쓰지 않고 이 인터페이스로 피격을 직접 처리한다.
 */
class DAERUNE_API IDRProximityHitOnly
{
	GENERATED_BODY()

public:
	// 이 공격자의 공격을 받아들일 수 있는지 (거리 조건). false 면 공격이 투과된다.
	virtual bool AcceptsHitFrom(const AActor* Attacker) const = 0;

	// 유효 피격 처리 (서버 전용). 데미지 수치와 무관하게 1히트로 처리한다.
	virtual void HandleProximityHit(AActor* Attacker) = 0;
};
