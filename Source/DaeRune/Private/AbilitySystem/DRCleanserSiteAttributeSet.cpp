// Copyright DaeRune


#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DRGameplayTags.h"
#include "Actor/DRCleanserSite.h"

void UDRCleanserSiteAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UDRCleanserSiteAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRCleanserSiteAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UDRCleanserSiteAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRCleanserSiteAttributeSet, Health, OldHealth);
}

void UDRCleanserSiteAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRCleanserSiteAttributeSet, MaxHealth, OldMaxHealth);
}

void UDRCleanserSiteAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 체력은 0과 최대체력 사이로 제한
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
}

void UDRCleanserSiteAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// IncomingDamage 처리
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		HandleIncomingDamage(Data);
	}
}

void UDRCleanserSiteAttributeSet::HandleIncomingDamage(const FGameplayEffectModCallbackData& Data)
{
	float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.0f);

	if (LocalIncomingDamage > 0.0f)
	{
		if (ADRCleanserSite* CleanserSite = Cast<ADRCleanserSite>(GetOwningActor()))
		{
			if(UAbilitySystemComponent* ASC = CleanserSite->GetAbilitySystemComponent())
			{
				if (ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Debuff_Elite))
				{
					LocalIncomingDamage *= EliteDebuffModifier;
					FMath::RoundToFloat(LocalIncomingDamage);

					FGameplayCueParameters CueParams;
					CueParams.Location = CleanserSite ? CleanserSite->GetActorLocation() : FVector::ZeroVector;
					CueParams.RawMagnitude = LocalIncomingDamage;

					ASC->ExecuteGameplayCue(
						FDRGameplayTags::Get().GameplayCue_Cleanser_Damage,
						CueParams
					);
				}
			}
		}
		
		// 체력 감소
		const float NewHealth = GetHealth() - LocalIncomingDamage;
		SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));

		// 체력 비율 계산
		const float HealthRatio = GetMaxHealth() > 0.0f ? NewHealth / GetMaxHealth() : 0.0f;

		if (ADRCleanserSite* CleanserSite = Cast<ADRCleanserSite>(GetOwningActor()))
		{
			CleanserSite->UpdateWaterMeshScale(HealthRatio);
		}

		// 체력이 50% 이하이고 아직 트리거 안 됐으면
		if (HealthRatio <= 0.5f && !bHalfHealthTriggered)
		{
			bHalfHealthTriggered = true;
			OnHealthBelowHalfDelegate.Broadcast();
		}

		// 체력이 0이 되면 파괴 이벤트
		if (NewHealth <= 0.0f)
		{
			OnHealthZeroDelegate.Broadcast();
		}
	}
}
