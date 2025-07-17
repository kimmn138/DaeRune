// Copyright DaeRune


#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Interaction/CombatInterface.h"

UDebuffNiagaraComponent::UDebuffNiagaraComponent()
{
	// 자동 활성화 비활성화 설정
	bAutoActivate = false;
}

void UDebuffNiagaraComponent::BeginPlay()
{
	Super::BeginPlay();

	// 컴뱃 인터페이스 참조 변수
	ICombatInterface* CombatInterface = Cast<ICombatInterface>(GetOwner());
	// 어빌리티 시스템 컴포넌트 확인 및 이벤트 등록 로직
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		// 디버프 태그 NewOrRemoved 이벤트 등록 로직
		ASC->RegisterGameplayTagEvent(DebuffTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UDebuffNiagaraComponent::DebuffTagChanged);
	}
	else if (CombatInterface)
	{
		// ASC 등록 대기 후 이벤트 등록 로직
		CombatInterface->GetOnASCRegisteredDelegate().AddWeakLambda(this, [this](UAbilitySystemComponent* InASC)
		{
			// 디버프 태그 NewOrRemoved 이벤트 등록 로직
			InASC->RegisterGameplayTagEvent(DebuffTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UDebuffNiagaraComponent::DebuffTagChanged);
		});
	}
}

// 디버프 태그 변경 처리 함수 정의
void UDebuffNiagaraComponent::DebuffTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// 소유자 생존 여부 확인 로직
	const bool bOwnerAlive = IsValid(GetOwner()) && GetOwner()->Implements<UCombatInterface>() && !ICombatInterface::Execute_IsDead(GetOwner());

	if (NewCount > 0 && bOwnerAlive)
	{
		// 나이아가라 컴포넌트 활성화 처리
		Activate();
	}
	else
	{
		// 나이아가라 컴포넌트 비활성화 처리
		Deactivate();
	}
}
