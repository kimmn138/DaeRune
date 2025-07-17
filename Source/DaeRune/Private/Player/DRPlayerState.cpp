// Copyright DaeRune


#include "Player/DRPlayerState.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Net/UnrealNetwork.h"

ADRPlayerState::ADRPlayerState()
{
	// GAS 컴포넌트 생성 및 복제 설정임
	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true); // 복제 활성화임
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed); // 복제 모드 설정임

	// 어트리뷰트 세트 생성 임
	AttributeSet = CreateDefaultSubobject<UDRAttributeSet>("AttributeSet");
	
	// 네트워크 업데이트 빈도 설정 임
	NetUpdateFrequency = 100.f;
}

void ADRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const // 복제 프로퍼티 등록 구현부임
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ADRPlayerState, Level); // 레벨 복제 등록임
}

UAbilitySystemComponent* ADRPlayerState::GetAbilitySystemComponent() const // GAS 컴포넌트 반환 구현부임
{
	return AbilitySystemComponent; // 어빌리티 시스템 컴포넌트 반환임
} 

void ADRPlayerState::OnRep_Level(int32 OldLevel) // 레벨 변경 복제 콜백 구현부임
{
}
