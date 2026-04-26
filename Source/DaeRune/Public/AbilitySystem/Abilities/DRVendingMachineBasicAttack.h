// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRProjectileSpell.h"
#include "DRVendingMachineBasicAttack.generated.h"

class ADRProjectile;

/**
 * 자판기 로봇 기본 공격 GA
 * LMB 홀드 시 일정 간격으로 투사체를 연속 발사한다.
 * 잭팟 패시브: 일반 공격 5회 후 랜덤 캡슐 투사체를 발사한다.
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

	void FireShotAndScheduleNext();
	void ExecuteShot();
	FVector CalculateTargetLocation() const;
};
