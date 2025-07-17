// Copyright DaeRune


#include "AbilitySystem/AsyncTasks/WaitCooldownChange.h"
#include "AbilitySystemComponent.h"

// 쿨다운 변경 대기 작업 생성 구현부
UWaitCooldownChange* UWaitCooldownChange::WaitForCooldownChange(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTag& InCooldownTag)
{
	// 인스턴스 생성 코드
	UWaitCooldownChange* WaitCooldownChange = NewObject<UWaitCooldownChange>(); 
	// 컴포넌트와 태그 설정 코드
	WaitCooldownChange->ASC = AbilitySystemComponent;
	WaitCooldownChange->CooldownTag = InCooldownTag;

	// 유효성 검사 및 취소 처리 코드
	if (!IsValid(AbilitySystemComponent) || !InCooldownTag.IsValid())
	{
		WaitCooldownChange->EndTask();
		return nullptr;
	}

	// 태그 이벤트 등록 코드
	// To know when a cooldown has ended (Cooldown Tag has been removed)
	AbilitySystemComponent->RegisterGameplayTagEvent(
		InCooldownTag,
		EGameplayTagEventType::NewOrRemoved).AddUObject(
			WaitCooldownChange,
			&UWaitCooldownChange::CooldownTagChanged);

	// 액티브 이펙트 추가 델리게이트 등록 코드
	// To know when a cooldown effect has been applied
	AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(WaitCooldownChange, &UWaitCooldownChange::OnActiveEffectAdded);

	// 대기 작업 반환 코드
	return WaitCooldownChange;
}

// 작업 종료 로직 구현부
void UWaitCooldownChange::EndTask()
{
	// 유효성 검사 코드
	if (!IsValid(ASC)) return; 
	// 태그 이벤트 제거 코드
	ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);

	// 객체 준비 해제 호출 코드
	SetReadyToDestroy();
	MarkAsGarbage();
}

// 태그 변경 이벤트 핸들러 구현부
void UWaitCooldownChange::CooldownTagChanged(const FGameplayTag InCooldownTag, int32 NewCount)
{
	// 태그 제거 시 종료 이벤트 브로드캐스트 코드
	if (NewCount == 0)
	{
		CooldownEnd.Broadcast(0.f);
	}
}

// 액티브 효과 추가 시 처리 로직 구현부
void UWaitCooldownChange::OnActiveEffectAdded(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	// 애셋 태그 컨테이너 생성 코드
	FGameplayTagContainer AssetTags;
	SpecApplied.GetAllAssetTags(AssetTags);

	// 부여 태그 컨테이너 생성 코드
	FGameplayTagContainer GrantedTags;
	SpecApplied.GetAllGrantedTags(GrantedTags);

	// 쿨다운 태그 포함 여부 확인 코드
	if (AssetTags.HasTagExact(CooldownTag) || GrantedTags.HasTagExact(CooldownTag))
	{
		// 쿨다운 타임 계산용 이펙트 쿼리 생성 코드
		FGameplayEffectQuery GameplayEffectQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTag.GetSingleTagContainer());
		TArray<float> TimesRemaining = ASC->GetActiveEffectsTimeRemaining(GameplayEffectQuery);
		// 가장 긴 남은 시간 검색 코드
		if (TimesRemaining.Num() > 0)
		{
			float TimeRemaining = TimesRemaining[0];
			for (int32 i = 0; i < TimesRemaining.Num(); i++)
			{
				if (TimesRemaining[i] > TimeRemaining)
				{
					TimeRemaining = TimesRemaining[i];
				}
			}

			// 시작 이벤트 브로드캐스트 코드
			CooldownStart.Broadcast(TimeRemaining);
		}
	}
}
