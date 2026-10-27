// Copyright DaeRune


#include "AbilitySystem/DRSynergyAuraComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DRCharacter.h"
#include "DRGameplayTags.h"
#include "DaeRune/DRLogChannels.h"
#include "Engine/World.h"
#include "Game/DRProgressionConfig.h"
#include "GameplayEffect.h"
#include "Interaction/CombatInterface.h"
#include "Player/DRPlayerState.h"
#include "TimerManager.h"

UDRSynergyAuraComponent::UDRSynergyAuraComponent()
{
	// 타이머로 돌리므로 컴포넌트 틱은 쓰지 않는다
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);   // 서버 전용 로직 — 복제할 상태가 없다
}

void UDRSynergyAuraComponent::RefreshFromUpgrade()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;   // ★서버 전용★ — GE 부여는 서버만

	UWorld* World = GetWorld();
	if (!World) return;

	const ADRCharacter* DRChar = Cast<ADRCharacter>(Owner);
	const ADRPlayerState* DRPS = DRChar ? DRChar->GetPlayerState<ADRPlayerState>() : nullptr;

	const bool bWantActive = DRPS && DRPS->HasUpgradeFeature(
		FDRGameplayTags::Get().Upgrade_Feature_Combat_Synergy);

	if (!bWantActive)
	{
		World->GetTimerManager().ClearTimer(AuraTickTimerHandle);
		ClearAllBuffs();   // ★칩을 빼면 내가 건 버프를 전부 회수한다★
		return;
	}

	if (World->GetTimerManager().IsTimerActive(AuraTickTimerHandle)) return;   // 이미 돌고 있음

	const float Interval = FMath::Max(0.05f,
		UDRAbilitySystemLibrary::GetChipTuning(Owner).SynergyTickInterval);

	World->GetTimerManager().SetTimer(
		AuraTickTimerHandle, this, &UDRSynergyAuraComponent::TickAura, Interval, /*bLoop*/ true);

	TickAura();   // 첫 틱을 기다리지 않고 즉시 1회
}

void UDRSynergyAuraComponent::TickAura()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	// 시전자가 죽으면 회수한다 — 시체가 계속 팀을 버프하지 않게
	if (Owner->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Owner))
	{
		ClearAllBuffs();
		return;
	}

	const FDRChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(Owner);

	// --- Step 1: 대상 결정 ---
	AActor* Nearest = FindNearestTeammate(Owner->GetActorLocation(), Tuning.SynergyRadius);

	TSet<AActor*> DesiredTargets;
	if (Nearest)
	{
		// ★팀원이 있을 때만 자신도 버프를 받는다★ — 혼자면 아무 효과가 없다(요구사항 명시)
		DesiredTargets.Add(Owner);
		DesiredTargets.Add(Nearest);
	}

	// --- Step 2: 대상에서 빠진 것 회수 ---
	TArray<TWeakObjectPtr<AActor>> ToRemove;
	for (const TPair<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle>& Pair : AppliedBuffs)
	{
		AActor* Buffed = Pair.Key.Get();
		if (!Buffed || !DesiredTargets.Contains(Buffed))
		{
			RemoveBuffFrom(Pair.Key, Pair.Value);
			ToRemove.Add(Pair.Key);
		}
	}
	for (const TWeakObjectPtr<AActor>& Key : ToRemove)
	{
		AppliedBuffs.Remove(Key);
	}

	// --- Step 3: 새로 들어온 대상에 부여 ---
	for (AActor* Target : DesiredTargets)
	{
		if (!AppliedBuffs.Contains(TWeakObjectPtr<AActor>(Target)))
		{
			ApplyBuffTo(Target);
		}
	}
}

AActor* UDRSynergyAuraComponent::FindNearestTeammate(const FVector& Origin, float Radius) const
{
	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;

	// 팀원 판정("Player" 태그 → ADRCharacter → 생존 → ASC 보유 → 거리)은 라이브러리와 같다 — 그중 가장 가까운 한 명
	TArray<AActor*> Teammates;
	UDRAbilitySystemLibrary::GetAlivePlayersWithinRadius(Owner, Origin, Radius, Teammates);

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (AActor* Candidate : Teammates)
	{
		if (Candidate == Owner) continue;

		const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), Origin);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}
	return Best;
}

void UDRSynergyAuraComponent::ApplyBuffTo(AActor* Target)
{
	if (!Target) return;

	UAbilitySystemComponent* SourceASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!SourceASC || !TargetASC) return;

	if (!CachedBuffEffectClass)
	{
		const FDRChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(GetOwner());
		CachedBuffEffectClass = Tuning.SynergyBuffEffect.LoadSynchronous();

		if (!CachedBuffEffectClass)
		{
			if (!bLoggedMissingEffect)
			{
				bLoggedMissingEffect = true;
				UE_LOG(LogDR, Warning,
					TEXT("[Upgrade] ChipTuning.SynergyBuffEffect 미지정 — 시너지 칩이 아무 효과도 내지 않습니다. ")
					TEXT("DA_ProgressionConfig 에 GE_Upgrade_Synergy 를 지정하세요."));
			}
			return;
		}
	}

	FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
	ContextHandle.AddSourceObject(GetOwner());

	const FGameplayEffectSpecHandle SpecHandle =
		SourceASC->MakeOutgoingSpec(CachedBuffEffectClass, 1.f, ContextHandle);
	if (!SpecHandle.IsValid()) return;

	const FActiveGameplayEffectHandle Handle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	if (Handle.IsValid())
	{
		AppliedBuffs.Add(TWeakObjectPtr<AActor>(Target), Handle);
	}
}

void UDRSynergyAuraComponent::RemoveBuffFrom(const TWeakObjectPtr<AActor>& TargetKey,
	FActiveGameplayEffectHandle Handle)
{
	AActor* Target = TargetKey.Get();
	if (!Target || !Handle.IsValid()) return;   // 이미 파괴된 대상이면 조용히 넘어간다

	if (UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		TargetASC->RemoveActiveGameplayEffect(Handle);
	}
}

void UDRSynergyAuraComponent::ClearAllBuffs()
{
	for (const TPair<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle>& Pair : AppliedBuffs)
	{
		RemoveBuffFrom(Pair.Key, Pair.Value);
	}
	AppliedBuffs.Reset();
}

void UDRSynergyAuraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuraTickTimerHandle);
	}

	// ★사망/디스커넥트/레벨 전환 시 남의 몸에 영구 버프를 남기지 않는다★
	ClearAllBuffs();

	Super::EndPlay(EndPlayReason);
}
