// Copyright DaeRune


#include "AbilitySystem/DRAttributeSet.h"
#include "DRAbilityTypes.h"
#include "GameFramework/Character.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Interaction/CombatInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DRPlayerController.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Player/DRPlayerState.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"

UDRAttributeSet::UDRAttributeSet()
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	/* Primary Attributes */
	TagsToAttributes.Add(GameplayTags.Attributes_Primary_MaxHealth, GetMaxHealthAttribute);
}

void UDRAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Primary Attributes

	DOREPLIFETIME_CONDITION_NOTIFY(UDRAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRAttributeSet, MaxWater, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);

	// Vital Attributes

	DOREPLIFETIME_CONDITION_NOTIFY(UDRAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRAttributeSet, Water, COND_None, REPNOTIFY_Always);
}

void UDRAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	if (Attribute == GetWaterAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxWater());
	}
	if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 50.f, 2000.f);
	}
}

void UDRAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	if (Attribute == GetWaterAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxWater());
	}
	if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 50.f, 2000.f);
	}
}

void UDRAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	FEffectProperties Props;
	SetEffectProperties(Data, Props);

	if (Props.TargetCharacter && Props.TargetCharacter->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Props.TargetCharacter)) return;

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	if (Data.EvaluatedData.Attribute == GetWaterAttribute())
	{
		SetWater(FMath::Clamp(GetWater(), 0.f, GetMaxWater()));
	}
	if (Data.EvaluatedData.Attribute == GetMoveSpeedAttribute())
	{
		// 새 속도 값 설정
		float NewSpeed = GetMoveSpeed();
		SetMoveSpeed(NewSpeed);

		// CharacterMovementComponent 업데이트
		if (ACharacter* TargetCharacter = Cast<ACharacter>(Props.TargetAvatarActor))
		{
			if (UCharacterMovementComponent* MovementComp = TargetCharacter->GetCharacterMovement())
			{
				MovementComp->MaxWalkSpeed = NewSpeed;
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		HandleIncomingDamage(Props);
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		HandleIncomingHealing(Props);
	}
}

void UDRAttributeSet::HandleIncomingDamage(const FEffectProperties& Props)
{
	
}

void UDRAttributeSet::HandleIncomingHealing(const FEffectProperties& Props)
{
}

void UDRAttributeSet::Debuff(const FEffectProperties& Props)
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get(); 
	FGameplayEffectContextHandle EffectContext = Props.SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(Props.SourceAvatarActor);

	const FGameplayTag DamageType = UDRAbilitySystemLibrary::GetDamageType(Props.EffectContextHandle);
	const float DebuffDamage = UDRAbilitySystemLibrary::GetDebuffDamage(Props.EffectContextHandle);
	const float DebuffDuration = UDRAbilitySystemLibrary::GetDebuffDuration(Props.EffectContextHandle);
	const float DebuffFrequency = UDRAbilitySystemLibrary::GetDebuffFrequency(Props.EffectContextHandle);

	FString DebuffName = FString::Printf(TEXT("DynamicDebuff_%s"), *DamageType.ToString());
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(DebuffName));

	Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
	Effect->Period = DebuffFrequency; 
	Effect->DurationMagnitude = FScalableFloat(DebuffDuration);

	FInheritedTagContainer TagContainer = FInheritedTagContainer();
	UTargetTagsGameplayEffectComponent& Component = Effect->FindOrAddComponent<UTargetTagsGameplayEffectComponent>();
	const FGameplayTag DebuffTag = GameplayTags.DamageTypesToDebuffs[DamageType];
	TagContainer.Added.AddTag(DebuffTag);
	Component.SetAndApplyTargetTagChanges(TagContainer);
	if (DebuffTag.MatchesTagExact(GameplayTags.Debuff_Stun))
	{
		TagContainer.Added.AddTag(GameplayTags.Player_Block_InputHeld);
		TagContainer.Added.AddTag(GameplayTags.Player_Block_InputPressed);
		TagContainer.Added.AddTag(GameplayTags.Player_Block_InputReleased);
	}
	Component.SetAndApplyTargetTagChanges(TagContainer);

	Effect->StackingType = EGameplayEffectStackingType::AggregateBySource; 
	Effect->StackLimitCount = 1;

	const int32 Index = Effect->Modifiers.Num();
	Effect->Modifiers.Add(FGameplayModifierInfo());
	FGameplayModifierInfo& ModifierInfo = Effect->Modifiers[Index];

	ModifierInfo.ModifierMagnitude = FScalableFloat(DebuffDamage);
	ModifierInfo.ModifierOp = EGameplayModOp::Additive;
	ModifierInfo.Attribute = UDRAttributeSet::GetIncomingDamageAttribute();

	if (FGameplayEffectSpec* MutableSpec = new FGameplayEffectSpec(Effect, EffectContext, 1.f))
	{
		FDRGameplayEffectContext* DRContext = static_cast<FDRGameplayEffectContext*>(MutableSpec->GetContext().Get());
		TSharedPtr<FGameplayTag> DebuffDamageType = MakeShareable(new FGameplayTag(DamageType));
		DRContext->SetDamageType(DebuffDamageType);

		Props.TargetASC->ApplyGameplayEffectSpecToSelf(*MutableSpec);
	}
}

void UDRAttributeSet::NotifyEnterCombat(const FEffectProperties& Props) const
{
	// 서버에서만 처리
	if (!Props.TargetAvatarActor || !Props.TargetAvatarActor->HasAuthority()) return;

	// 소스가 플레이어인 경우
	if (Props.SourceController && Props.SourceController->IsPlayerController())
	{
		if (ADRPlayerState* PS = Props.SourceController->GetPlayerState<ADRPlayerState>())
		{
			PS->EnterCombat();
		}
	}

	// 타겟이 플레이어인 경우
	if (Props.TargetController && Props.TargetController->IsPlayerController())
	{
		if (ADRPlayerState* PS = Props.TargetController->GetPlayerState<ADRPlayerState>())
		{
			PS->EnterCombat();
		}
	}
}

void UDRAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxHealthAttribute() && bTopOffHealth)
	{
		SetHealth(GetMaxHealth());
		bTopOffHealth = false;
	}
	if (Attribute == GetMaxWaterAttribute() && bTopOffWater)
	{
		SetWater(GetMaxWater());
		bTopOffWater = false;
	}
	if (Attribute == GetMoveSpeedAttribute())
	{
		// ASC에서 Avatar Actor 가져오기 (Enemy든 Player든 상관없이)
		if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
		{
			if (ACharacter* AvatarCharacter = Cast<ACharacter>(ASC->GetAvatarActor()))
			{
				if (UCharacterMovementComponent* MovementComp = AvatarCharacter->GetCharacterMovement())
				{
					MovementComp->MaxWalkSpeed = NewValue;
				}
			}
		}
	}
}

void UDRAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRAttributeSet, Health, OldHealth);
}

void UDRAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRAttributeSet, MaxHealth, OldMaxHealth);
}

void UDRAttributeSet::OnRep_Water(const FGameplayAttributeData& OldWater) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRAttributeSet, Water, OldWater);
}

void UDRAttributeSet::OnRep_MaxWater(const FGameplayAttributeData& OldMaxWater) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRAttributeSet, MaxWater, OldMaxWater);
}

void UDRAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRAttributeSet, MoveSpeed, OldMoveSpeed);

	// 클라이언트에서 즉시 이동속도 업데이트
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (ACharacter* AvatarCharacter = Cast<ACharacter>(ASC->GetAvatarActor()))
		{
			if (UCharacterMovementComponent* MovementComp = AvatarCharacter->GetCharacterMovement())
			{
				MovementComp->MaxWalkSpeed = GetMoveSpeed();

				// 클라이언트 예측을 위한 추가 설정
				MovementComp->bNetworkSmoothingComplete = true;
			}
		}
	}
}

void UDRAttributeSet::SetEffectProperties(const FGameplayEffectModCallbackData& Data, FEffectProperties& Props) const
{
	// Source = causer of the effect, Target = target of the effect (owner of this AS)

	Props.EffectContextHandle = Data.EffectSpec.GetContext();
	Props.SourceASC = Props.EffectContextHandle.GetOriginalInstigatorAbilitySystemComponent();

	if (IsValid(Props.SourceASC) && Props.SourceASC->AbilityActorInfo.IsValid() && Props.SourceASC->AbilityActorInfo->AvatarActor.IsValid())
	{
		Props.SourceAvatarActor = Props.SourceASC->AbilityActorInfo->AvatarActor.Get();
		Props.SourceController = Props.SourceASC->AbilityActorInfo->PlayerController.Get();
		if (Props.SourceController == nullptr && Props.SourceAvatarActor != nullptr)
		{
			if (const APawn* Pawn = Cast<APawn>(Props.SourceAvatarActor))
			{
				Props.SourceController = Pawn->GetController();
			}
		}
		if (Props.SourceController)
		{
			Props.SourceCharacter = Cast<ACharacter>(Props.SourceController->GetPawn());
		}
	}

	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		Props.TargetAvatarActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		Props.TargetController = Data.Target.AbilityActorInfo->PlayerController.Get();
		Props.TargetCharacter = Cast<ACharacter>(Props.TargetAvatarActor);
		Props.TargetASC = &Data.Target;
	}
}

void UDRAttributeSet::ShowFloatingText(const FEffectProperties& Props, float Damage) const
{
	if (Props.SourceCharacter != Props.TargetCharacter)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(Props.SourceCharacter->Controller))
		{
			PC->ShowDamageNumber(Damage, Props.TargetCharacter);
			return;
		}
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(Props.TargetCharacter->Controller))
		{
			PC->ShowDamageNumber(Damage, Props.TargetCharacter);
		}
	}
}
