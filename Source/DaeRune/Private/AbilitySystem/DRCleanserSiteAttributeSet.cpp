// Copyright DaeRune


#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemBlueprintLibrary.h"

UDRCleanserSiteAttributeSet::UDRCleanserSiteAttributeSet()
{
	// 기본값 설정
	InitHealth(1000.0f);
	InitMaxHealth(1000.0f);
}

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
		const float LocalIncomingDamage = GetIncomingDamage();
		SetIncomingDamage(0.0f);

		if (LocalIncomingDamage > 0.0f)
		{
			// 체력 감소
			const float NewHealth = GetHealth() - LocalIncomingDamage;
			SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));

			// 체력이 0이 되면 파괴 이벤트
			if (GetHealth() <= 0.0f)
			{
				// TODO: 클렌저 사이트 파괴 처리
				UE_LOG(LogTemp, Warning, TEXT("CleanserSite destroyed!"));
			}
		}
	}
}
