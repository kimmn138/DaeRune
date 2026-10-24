// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRProjectileSpell.h"
// FDRJackpotOdds — GetJackpotOdds() 반환형 (Plan5(잭팟칩))
#include "Game/DRProgressionConfig.h"
#include "DRVendingMachineBasicAttack.generated.h"

class ADRProjectile;

/**
 * 자판기 로봇 기본 공격 GA
 * LMB 홀드 시 일정 간격으로 투사체를 연속 발사한다.
 * 잭팟 패시브: 일반 공격 MaxJackpotStacks 회(BP 값 10) 후 랜덤 캡슐 투사체를 발사한다.
 * 확률·피해는 잭팟 칩(Plan5(잭팟칩))이 바꿀 수 있다 — GetJackpotOdds() 참고.
 */
UCLASS()
class DAERUNE_API UDRVendingMachineBasicAttack : public UDRProjectileSpell
{
	GENERATED_BODY()

public:
	// ============================================================
	// BlueprintCallable (Blueprint에서 호출하는 도구)
	// ============================================================

	/** 자동 연사 시작. ActivateAbility BP에서 호출. */
	UFUNCTION(BlueprintCallable, Category = "VendingMachine|BasicAttack")
	void StartAutoFire();

	/** 자동 연사 중지. InputReleased BP에서 호출. */
	UFUNCTION(BlueprintCallable, Category = "VendingMachine|BasicAttack")
	void StopAutoFire();

	// ============================================================
	// BlueprintPure (Blueprint에서 상태 조회)
	// ============================================================

	UFUNCTION(BlueprintPure, Category = "VendingMachine|Jackpot")
	int32 GetJackpotStacks() const { return CurrentJackpotStacks; }

	UFUNCTION(BlueprintPure, Category = "VendingMachine|Jackpot")
	bool IsJackpotReady() const { return CurrentJackpotStacks >= MaxJackpotStacks; }

	UFUNCTION(BlueprintPure, Category = "VendingMachine|BasicAttack")
	float GetCurrentFireInterval() const;

	/**
	 * ★Plan5(잭팟칩)★ 다음 잭팟의 확률표 (잭팟 칩 · ③ 누적 보정 반영, 합 = 1).
	 * 기본표 = BP 의 BronzeCapsuleChance / SilverCapsuleChance (골드는 나머지).
	 *
	 * ⚠️ ③(적립 카드)의 누적 상태는 ★서버에만★ 있다 — 클라에서 부르면 보정이 빠진 값이 나온다 (Plan5 T-53).
	 */
	UFUNCTION(BlueprintPure, Category = "VendingMachine|Jackpot")
	FDRJackpotOdds GetJackpotOdds() const;

	// ============================================================
	// BlueprintImplementableEvent (C++ → Blueprint 콜백)
	// ============================================================

	/** 일반 투사체 발사 직후 (BP에서 VFX/SFX 처리) */
	UFUNCTION(BlueprintImplementableEvent, Category = "VendingMachine|BasicAttack")
	void OnNormalShotFired();

	/** 잭팟 캡슐 발사 직후 (BP에서 잭팟 연출 처리) */
	UFUNCTION(BlueprintImplementableEvent, Category = "VendingMachine|Jackpot")
	void OnCapsuleShotFired(int32 CapsuleTier);
	// CapsuleTier: 0 = Bronze, 1 = Silver, 2 = Gold

	/** 잭팟 스택 변경 시 (BP에서 UI 업데이트) */
	UFUNCTION(BlueprintImplementableEvent, Category = "VendingMachine|Jackpot")
	void OnJackpotStacksChanged(int32 NewStacks, int32 MaxStacks);

protected:
	// ============================================================
	// EditDefaultsOnly (BP 에디터에서 설정)
	// ============================================================

	/** 기본 발사 간격 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|BasicAttack")
	float BaseFireInterval = 0.3f;

	/**
	 * ★Plan5(동전칩)★ 발사 간격의 하한(초).
	 *
	 * 발사 간격 칩(SkillFireInterval)과 공격 속도 버프가 겹쳐 0 이하가 되면 SetTimer 가 타이머를
	 * ★지워서★ 연사가 조용히 멈춘다 — 그걸 막는 바닥값이다(UDRGameplayAbility::MinCooldownDuration 과 같은 역할).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|BasicAttack", meta = (ClampMin = "0.01"))
	float MinFireInterval = 0.05f;

	/** 투사체 발사 소켓 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|BasicAttack")
	FGameplayTag FireSocketTag;

	// --- 잭팟 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
	int32 MaxJackpotStacks = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
	TSubclassOf<ADRProjectile> BronzeCapsuleClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
	TSubclassOf<ADRProjectile> SilverCapsuleClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
	TSubclassOf<ADRProjectile> GoldCapsuleClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
	float BronzeCapsuleDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
	float SilverCapsuleDamage = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot")
	float GoldCapsuleDamage = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BronzeCapsuleChance = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|Jackpot",
	          meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SilverCapsuleChance = 0.4f;

private:
	int32 CurrentJackpotStacks = 0;
	FTimerHandle AutoFireTimerHandle;
	bool bIsFiring = false;
	double LastShotTime = 0.0;

	void FireShotAndScheduleNext();
	void ExecuteShot();
	// CalculateTargetLocation()은 UDRProjectileSpell 공용 유틸로 승격됨 (Plan3 §6.1)

	// ========== ★Plan5(잭팟칩)★ 잭팟 한 번 = 확률 계산 → 굴림 → 발사 → 보정 ==========

	// 확률표로 등급을 굴린다. 0 = 브론즈, 1 = 실버, 2 = 골드 (OnCapsuleShotFired 와 같은 번호)
	int32 RollJackpotTier(const FDRJackpotOdds& Odds) const;

	// 등급의 기본 피해 (③ 적립 카드면 골드만 ChipTuning.Jackpot.PityGoldDamage)
	float GetJackpotCapsuleBaseDamage(int32 Tier) const;

	// 캡슐 1발 스폰 — 크기 칩 · SkillDamage · 사운드 멀티캐스트까지. ② 의 두 번째 발도 이 함수로 쏜다.
	void SpawnJackpotCapsule(int32 Tier, const FVector& TargetLocation);

	// ③ 적립 카드를 끼고 있을 때만 누적 보정을 갱신한다 (골드면 초기화)
	void AdvanceJackpotPity(int32 Tier);

	// ② 쌍둥이 캡슐 — 타이머 콜백. 같은 등급으로 한 발 더.
	void FireJackpotDoubleShot();

	bool HasJackpotFeature(const FGameplayTag& FeatureTag) const;

	// ③ 누적 보정 — 서버 전용, 복제하지 않는다 (Plan5 D-58). 스테이지 동안 유지, 골드가 나오면 0.
	int32 PityBronzeSteps = 0;
	int32 PitySilverSteps = 0;

	// ② 추가 발사 대기
	FTimerHandle JackpotDoubleShotTimerHandle;
	int32 PendingDoubleShotTier = INDEX_NONE;
};
