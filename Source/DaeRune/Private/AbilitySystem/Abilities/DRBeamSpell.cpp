// Copyright DaeRune


#include "AbilitySystem/Abilities/DRBeamSpell.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"

/**
 * 카메라 데이터 정보 저장 구현부
 */
void UDRBeamSpell::StoreCameraDataInfo(const FHitResult& HitResult)
{
	// 히트 유무 검사 처리
	if (HitResult.bBlockingHit)
	{
		// 카메라 히트 위치 및 액터 저장 처리
		CameraHitLocation = HitResult.ImpactPoint;
		CameraHitActor = HitResult.GetActor();
	}
	else
	{
		// 어빌리티 취소 처리
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}
}

/**
 * 소유자 변수 저장 구현부
 */
void UDRBeamSpell::StoreOwnerVariables()
{
	// 액터 정보 유효성 검사 처리
	if (CurrentActorInfo)
	{
		// 소유자 캐릭터 캐스팅 처리
		OwnerCharacter = Cast<ACharacter>(CurrentActorInfo->AvatarActor);
	}
}

/**
 * 첫 번째 대상 트레이스 구현부
 */
void UDRBeamSpell::TraceFirstTarget(const FVector& BeamTargetLocation)
{
	// 소유자 캐릭터 유효성 검사 처리
	check(OwnerCharacter); 
	// 전투 인터페이스 검사 처리
	if (OwnerCharacter->Implements<UCombatInterface>())
	{
		// 무기 소켓 위치 획득 처리
		if (USkeletalMeshComponent* Weapon = ICombatInterface::Execute_GetWeapon(OwnerCharacter))
		{
			TArray<AActor*> ActorsToIgnore;
			ActorsToIgnore.Add(OwnerCharacter);
			FHitResult HitResult;
			// 스피어 트레이스 실행 처리
			const FVector SocketLocation = Weapon->GetSocketLocation(FName("TipSocket"));
			UKismetSystemLibrary::SphereTraceSingle(
				OwnerCharacter,
				SocketLocation,
				BeamTargetLocation,
				10.f,
				TraceTypeQuery1,
				false,
				ActorsToIgnore,
				EDrawDebugTrace::None,
				HitResult,
				true);

			// 히트 결과 저장 처리
			if (HitResult.bBlockingHit)
			{
				CameraHitLocation = HitResult.ImpactPoint;
				CameraHitActor = HitResult.GetActor();
			}
		}
	}
	// 사망 델리게이트 바인딩 처리
	if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(CameraHitActor))
	{
		if (!CombatInterface->GetOnDeathDelegate().IsAlreadyBound(this, &UDRBeamSpell::PrimaryTargetDied))
		{
			CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRBeamSpell::PrimaryTargetDied);
		}
	}
}

/**
 * 추가 대상 저장 구현부
 */
void UDRBeamSpell::StoreAdditionalTargets(TArray<AActor*>& OutAdditionalTargets)
{
	// 무시 대상 배열 생성 처리
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(GetAvatarActorFromActorInfo());
	ActorsToIgnore.Add(CameraHitActor);

	// 겹치는 액터 배열 생성 처리
	TArray<AActor*> OverlappingActors;
	UDRAbilitySystemLibrary::GetLivePlayersWithinRadius(
		GetAvatarActorFromActorInfo(),
		OverlappingActors,
		ActorsToIgnore,
		850.f,
		CameraHitActor->GetActorLocation());

	//int32 NumAdditionalTargets = FMath::Min(GetAbilityLevel() - 1, MaxNumShockTargets);
	// 추가 대상 수 계산 처리
	int32 NumAdditionTargets = MaxNumShockTargets;

	// 가장 가까운 대상 선택 처리
	UDRAbilitySystemLibrary::GetClosestTargets(
		NumAdditionTargets,
		OverlappingActors,
		OutAdditionalTargets,
		CameraHitActor->GetActorLocation());

	// 사망 델리게이트 바인딩 반복 처리
	for (AActor* Target : OutAdditionalTargets)
	{
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Target))
		{
			if (!CombatInterface->GetOnDeathDelegate().IsAlreadyBound(this, &UDRBeamSpell::AdditionalTargetDied))
			{
				CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UDRBeamSpell::AdditionalTargetDied);
			}
		}
	}
}
