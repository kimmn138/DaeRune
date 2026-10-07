// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRMeleeAttack.generated.h"

/**
 * 근접 공격 어빌리티의 공통 베이스.
 *
 * ⚠️ ★정원로봇 전용 클래스가 아니다.★ 적 어빌리티도 이 클래스를 상속한다
 *    (GA_DogBite / GA_MeleeAttack / GA_Explosion / GA_DeathExplosion /
 *     GA_Armadillo_BasicAttack / GA_Bear_BasicAttack / GA_ClawSwipe).
 *    따라서 여기에 추가하는 것은 ★모든 근접 공격에 영향을 줄 수 있다★ —
 *    아래 프로퍼티·함수는 전부 "BP 가 명시적으로 읽어야만" 효과가 생기도록 설계했고,
 *    현재 배선된 BP 는 GA_ClawSwipe 뿐이다. 나머지 GA 는 동작이 전혀 바뀌지 않는다.
 *
 *    EditDefaultsOnly 라 값은 ★GA 블루프린트마다 따로★ 갖는다. 적 GA 가 기본값을
 *    그대로 둬도 무해하다(아무도 읽지 않으므로).
 */
UCLASS()
class DAERUNE_API UDRMeleeAttack : public UDRDamageGameplayAbility
{
	GENERATED_BODY()

public:
	// ========== 업그레이드 칩이 반영된 조회 (Plan5(발톱칩)) ==========

	/**
	 * ★Plan5(발톱칩)★ 칩이 반영된 공격 판정 반경.
	 *
	 * BP 의 GetLiveObjectsWithinRadius(Radius) 와 디버그 구(Radius) ★양쪽★ 에 연결한다.
	 * 한쪽만 바꾸면 "그려진 범위 밖인데 맞는다" 가 되어 범위를 눈으로 검증할 수 없다.
	 *
	 * 칩이 없으면 AttackRadius 를 그대로 돌려준다(항등).
	 */
	UFUNCTION(BlueprintPure, Category = "Melee|Upgrade")
	float GetUpgradedAttackRadius() const;

	/**
	 * ★Plan5(발톱칩)★ 선딜 칩이 반영된 몽타주 재생 속도.
	 *
	 * BP 의 PlayMontageAndWait(1P·3P) ★양쪽★ PlayRate 핀에 연결한다.
	 * 한쪽만 바꾸면 손과 몸이 따로 논다.
	 *
	 * 선딜 배율이 3 이면 1/3 속도가 되어, 몽타주의 타격 노티파이까지 걸리는
	 * ★실시간★ 이 3배가 된다. 이것이 곧 "선딜레이 3배" 다.
	 */
	UFUNCTION(BlueprintPure, Category = "Melee|Upgrade")
	float GetWindupPlayRate() const;

	/**
	 * ★Plan5(발톱칩)★ 몽타주 재생 속도를 기본값으로 되돌린다.
	 *
	 * ★타격 노티파이를 받은 직후에 부른다.★ 그래야 느려지는 구간이 "휘두르기 전"에서 끝나고
	 * 후딜(되돌아오는 동작)은 원래 속도로 재생된다 — 요구사항의 "선딜레이 3배" 가 그 뜻이다.
	 *
	 * 이 호출을 BP 에 붙이지 않으면 ★후딜까지 3배★ 가 된다(기능은 동작하지만 체감이 다르다).
	 *
	 * ★Plan5(공속칩)★ "기본값" = BaseMontagePlayRate × 공속 배율. 공속 버프를 받은 동안은 후딜도 빨라진다 (D-67).
	 */
	UFUNCTION(BlueprintCallable, Category = "Melee|Upgrade")
	void RestoreMontagePlayRate();

	/**
	 * ★Plan5(발톱칩)★ 아군 하나에게 "내 체력을 나눠" 회복시킨다. ★서버에서만 유효★
	 *
	 * BP 의 ForEachLoop → Branch(IsValidDamageTarget) 의 ★False 분기★ 에서 호출한다.
	 * 칩 미장착 / 아군이 아님 / 체력 부족 / 아군이 만피 이면 ★조용히 아무 일도 하지 않는다★ —
	 * 그래서 BP 는 조건 없이 항상 이 노드를 부르면 되고, 켜짐·꺼짐 판단이 BP 로 새지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Melee|Upgrade")
	void TryHealAllyWithOwnHealth(AActor* AllyActor);

protected:
	// ========== 어빌리티 기본값 (Plan5(발톱칩)) ==========

	/**
	 * 공격 판정 구의 반경(기본값). 칩은 SkillAreaRadius 로 이 값에 곱해진다.
	 *
	 * ★BP 의 GetLiveObjectsWithinRadius 에 들어 있던 숫자를 그대로 옮겨 적을 것.★
	 * 안 옮기면 칩이 없을 때의 사거리가 조용히 바뀐다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "1.0"))
	float AttackRadius = 150.f;

	/**
	 * 몽타주 기본 재생 속도. 선딜 칩은 이 값을 배율로 나눠서 쓴다.
	 * ★BP 의 PlayMontageAndWait 에 들어 있던 PlayRate 를 그대로 옮겨 적을 것.★
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0.01"))
	float BaseMontagePlayRate = 1.f;
};
