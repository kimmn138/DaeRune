// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"

// 비헤이비어 트리 블랙보드 키 이름 (BB 에셋의 키 이름과 같아야 한다).
// 적 · AI 컨트롤러 · AttributeSet 이 문자열을 각자 쓰지 않도록 한 곳에 둔다.
namespace DRBlackboardKeys
{
	// AI 상태
	inline const FName HitReacting = TEXT("HitReacting");
	inline const FName RangedAttacker = TEXT("RangedAttacker");
	inline const FName HomeLocation = TEXT("HomeLocation");
	inline const FName Dead = TEXT("Dead");
	inline const FName Stunned = TEXT("Stunned");

	// 부품 시스템
	inline const FName HasPart = TEXT("HasPart");

	// 광폭화 시스템
	inline const FName IsEnraged = TEXT("bIsEnraged");
	inline const FName AttackSpeed = TEXT("AttackSpeed");
	inline const FName EliteAttackSpeed = TEXT("EliteAttackSpeed");

	// 타겟팅
	inline const FName FirstAttacker = TEXT("FirstAttacker");
	inline const FName HasFirstAttacker = TEXT("HasFirstAttacker");
	inline const FName TargetToFollow = TEXT("TargetToFollow");
	// ★Plan8(시선집중)★ 타깃 서비스들이 쓰는 거리 키 (Plan8 D-16)
	inline const FName DistanceToTarget = TEXT("DistanceToTarget");
	// ★Plan8(시선집중)★ BTS_CheckValidTarget 의 "벽에 가림" 타이머 — 도발 동안 0 으로 되돌린다 (Plan8 D-16)
	inline const FName UnreachableSince = TEXT("UnreachableSince");

	// DragonFly 전용
	inline const FName IsLockedDown = TEXT("IsLockedDown");
	inline const FName BasicAttackCount = TEXT("BasicAttackCount");

	// 전투 · 피격 (AI 컨트롤러 · 적 AttributeSet)
	inline const FName AttackingPlayer = TEXT("AttackingPlayer");
	inline const FName IsHealthLow = TEXT("IsHealthLow");
	inline const FName IsInitialized = TEXT("IsInitialized");
	inline const FName LastCombatTime = TEXT("LastCombatTime");
	inline const FName HasPlayerInRange = TEXT("HasPlayerInRange");
}
