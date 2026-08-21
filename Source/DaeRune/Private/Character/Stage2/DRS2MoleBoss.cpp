// Copyright DaeRune

#include "Character/Stage2/DRS2MoleBoss.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Actor/Stage2/DRS2ElectricField.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DRGameplayTags.h"
#include "DaeRune/DRLogChannels.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

ADRS2MoleBoss::ADRS2MoleBoss()
{
	// 처치 시 전역 물 지급 (ADREnemy::GrantWaterToPlayers)
	bIsBoss = true;

	CharacterClass = ECharacterClass::MoleBoss;

	// 부동체이므로 회피 계산에서 뺀다
	GetCharacterMovement()->bUseRVOAvoidance = false;
	GetCharacterMovement()->GravityScale = 0.f;

	// 느리게 도는 보스 연출. 부모 기본값은 10.0 이다. (Plan7 §2.1)
	RotationInterpSpeed = 4.f;

	// 보스는 일반 적(30)보다 높은 갱신 빈도가 필요하다
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(30.f);

}

void ADRS2MoleBoss::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2MoleBoss, SegmentIndex);
	DOREPLIFETIME(ADRS2MoleBoss, bBurrowed);
}

void ADRS2MoleBoss::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		LockMovement();
		SnapToBuriedPose();

		// ★Level(= 기준 인원)이 제대로 심겼는지, 커브가 반영됐는지 한눈에 확인하는 로그.
		//   Level 이 항상 1이면 페이즈가 지연 스폰 경로를 안 탄 것이다.
		UE_LOG(LogDR, Log, TEXT("[MoleBoss] 스폰 — Level(=기준 인원) %d, MaxHealth %.0f"),
			Level, GetAbilitySystemComponent()
				? GetAbilitySystemComponent()->GetNumericAttribute(UDRAttributeSet::GetMaxHealthAttribute())
				: 0.f);
	}
}

void ADRS2MoleBoss::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyActiveField();

	Super::EndPlay(EndPlayReason);
}

void ADRS2MoleBoss::LockMovement()
{
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
		// ★MOVE_None: 이동·중력·LaunchCharacter 를 전부 무효화한다.
		//   덕분에 플레이어 넉백 스킬로 보스를 밀 수 없고, 월 스턴(ADREnemy::OnHit)도 자동 봉인된다.
		CMC->DisableMovement();
	}
}

// ================= 페이즈 주입 =================

void ADRS2MoleBoss::SetSegmentIndex(int32 InIndex)
{
	if (!HasAuthority()) return;

	SegmentIndex = InIndex;

	// 리슨 서버에서도 연출이 돌아야 하므로 RepNotify 수동 호출 (Plan6 §15.0 규약)
	OnRep_SegmentIndex();

	UE_LOG(LogDR, Verbose, TEXT("[MoleBoss] 구간 인덱스 = %d"), SegmentIndex);
}

void ADRS2MoleBoss::OnRep_SegmentIndex()
{
	OnSegmentChangedVisual(SegmentIndex);
}

void ADRS2MoleBoss::NotifyStashed()
{
	if (!HasAuthority()) return;

	// ★진행 중인 굴착 강습을 반드시 취소한다 (Plan7 §1.2 계약 C5).
	//   취소하지 않으면 GA 의 타이머 체인이 살아남아 숨겨진 보스가 융기해 데미지를 준다.
	if (AbilitySystemComponent)
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(FDRGameplayTags::Get().Abilities_MoleBoss_BurrowStrike);
		AbilitySystemComponent->CancelAbilities(&CancelTags);
	}

	// 취소 경로가 놓쳤을 때의 이중 안전 (숨긴 채로 보관되지 않도록)
	ExitBurrowedState();

	// 구간이 바뀌면 이전 구간의 전기장과 "직전 융기 지점"을 버린다.
	// → 새 구간의 1번째 융기에는 전기장이 생기지 않는다 (사용자 예시와 일치).
	DestroyActiveField();
	bHasPreviousErupt = false;
	PreviousEruptGround = FVector::ZeroVector;
	EruptCount = 0;
}

void ADRS2MoleBoss::NotifyReappeared()
{
	if (!HasAuthority()) return;

	// 페이즈가 SetActorTransform 으로 옮긴 직후다. 이동 봉인을 재적용하고 자세를 정렬한다.
	LockMovement();
	SnapToBuriedPose();
}

// ================= 반매몰 자세 =================

FVector ADRS2MoleBoss::MakeBuriedLocation(const FVector& GroundPoint) const
{
	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;

	// CenterZ = GroundZ + H * (1 - 2r)
	//   r = 0.0 → GroundZ + H (지면에 선 일반 캐릭터)
	//   r = 0.5 → GroundZ     (몸의 절반만 노출 = 사양)
	//   r = 1.0 → GroundZ - H (완전 매몰)
	return FVector(GroundPoint.X, GroundPoint.Y, GroundPoint.Z + HalfHeight * (1.f - 2.f * BuriedRatio));
}

void ADRS2MoleBoss::SnapToBuriedPose()
{
	if (!HasAuthority()) return;

	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	const FVector Origin = GetActorLocation();

	// ★트레이스를 캡슐 중심보다 위에서 시작한다.
	//   이미 반매몰 상태면 중심이 지면과 같은 높이라, 중심에서 쏘면 지면을 못 맞출 수 있다.
	const FVector Start = Origin + FVector(0.f, 0.f, HalfHeight);
	const FVector End = Origin - FVector(0.f, 0.f, GroundTraceDistance);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FVector Ground = FVector(Origin.X, Origin.Y, Origin.Z - HalfHeight);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		Ground = Hit.ImpactPoint;
	}
	else
	{
		UE_LOG(LogDR, Warning,
			TEXT("[MoleBoss] 지면을 찾지 못했습니다. 배치 위치 아래에 WorldStatic 지형이 있는지 확인하세요."));
	}

	SetActorLocation(MakeBuriedLocation(Ground), false, nullptr, ETeleportType::TeleportPhysics);
}

// ================= 애니메이션 =================

float ADRS2MoleBoss::PlayMoleMontage(UAnimMontage* Montage)
{
	if (!Montage || !HasAuthority()) return 0.f;

	// 서버(리슨 호스트 포함) 로컬 재생
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage);
		}
	}

	// 원격 클라이언트 (헤더 주석 참고 — GA 몽타주 복제에만 의존하지 않는다)
	Multicast_PlayMoleMontage(Montage);

	return Montage->GetPlayLength();
}

void ADRS2MoleBoss::Multicast_PlayMoleMontage_Implementation(UAnimMontage* Montage)
{
	// 서버는 PlayMoleMontage 에서 이미 재생했다 (이중 재생 방지)
	if (HasAuthority() || !Montage) return;

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->Montage_Play(Montage);
		}
	}
}

float ADRS2MoleBoss::PlayBurrowMontage()
{
	if (!BurrowMontage)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[MoleBoss] BurrowMontage 미설정 — 잠수 연출 없이 바로 숨습니다. BP_S2MoleBoss 를 확인하세요."));
	}
	return PlayMoleMontage(BurrowMontage);
}

float ADRS2MoleBoss::GetEmergeMontageLength() const
{
	return EmergeMontage ? EmergeMontage->GetPlayLength() : 0.f;
}

// ================= 잠수(무적) =================

void ADRS2MoleBoss::EnterBurrowedState()
{
	if (!HasAuthority() || bBurrowed) return;

	bBurrowed = true;

	// ★콜리전만 끄면 이미 걸려 있는 화상 DoT 가 무적을 뚫는다.
	//   그 상태로 임계가 걸리면 페이즈가 StashBoss 를 불러 연출이 깨지므로 여기서 정리한다. (Plan7 §6.5)
	if (bClearDebuffsOnBurrow && AbilitySystemComponent)
	{
		// (AActor::Tags 와 이름이 겹치지 않도록 지역 변수명을 구분한다)
		const FDRGameplayTags& DRTags = FDRGameplayTags::Get();

		// "Debuff" 부모 태그의 등록 여부에 의존하지 않도록 개별 태그로 컨테이너를 조립한다
		FGameplayTagContainer DebuffTags;
		DebuffTags.AddTag(DRTags.Debuff_Burn);
		DebuffTags.AddTag(DRTags.Debuff_Stun);
		DebuffTags.AddTag(DRTags.Debuff_Arcane);
		DebuffTags.AddTag(DRTags.Debuff_Physical);
		DebuffTags.AddTag(DRTags.Debuff_Bleed);

		AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(DebuffTags);
	}

	OnRep_Burrowed();
}

void ADRS2MoleBoss::ExitBurrowedState()
{
	if (!HasAuthority() || !bBurrowed) return;

	bBurrowed = false;

	OnRep_Burrowed();
}

void ADRS2MoleBoss::OnRep_Burrowed()
{
	// 숨김 + 콜리전 해제로 직격/AoE 를 모두 차단한다.
	// GetLiveObjectsWithinRadius 는 오브젝트 타입 오버랩이라 콜리전이 꺼지면 수집되지 않는다.
	SetActorHiddenInGame(bBurrowed);
	SetActorEnableCollision(!bBurrowed);

	if (bBurrowed)
	{
		OnBurrowVisual();
	}
}

// ================= 융기 =================

float ADRS2MoleBoss::GetEruptRadius() const
{
	const float CapsuleRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
	return CapsuleRadius * EruptRadiusScale;
}

void ADRS2MoleBoss::EruptAt(const FVector& GroundPoint, const FRotator& FacingRotation)
{
	if (!HasAuthority()) return;

	const FRotator YawOnly(0.f, FacingRotation.Yaw, 0.f);

	// ① 숨겨진 상태에서 텔레포트한다.
	//    ADREnemy 는 NetworkSmoothingMode = Exponential 이라 큰 점프가 슬라이딩으로 보일 수 있는데,
	//    가시화 전에 옮기면 그 과정이 화면에 나오지 않는다.
	SetActorLocation(MakeBuriedLocation(GroundPoint), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(YawOnly);

	// ② ControlRotation 도 맞춘다.
	//    부모 ADREnemy::Tick 이 ControlRotation 을 추종하므로, 맞추지 않으면
	//    융기 직후 이전에 보던 방향으로 홱 돌아간다.
	if (AController* OwnController = GetController())
	{
		OwnController->SetControlRotation(YawOnly);
	}

	// ③ ★가시화보다 먼저 융기 몽타주를 시작한다.
	//    숨겨진 메시도 포즈는 계속 틱하므로(SkeletalMeshComponent 기본값
	//    AlwaysTickPoseAndRefreshBones) 첫 프레임(= 땅속 포즈)이 적용된 뒤 보이기 시작한다.
	//    순서를 뒤집으면 기본 포즈로 한 프레임 튀어 보이고, 클라에서는 언하이드(프로퍼티 복제)와
	//    몽타주(RepAnimMontageInfo)가 다른 경로라 그 틈이 더 벌어진다.
	if (!EmergeMontage)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[MoleBoss] EmergeMontage 미설정 — 융기 연출 없이 즉시 나타납니다. BP_S2MoleBoss 를 확인하세요."));
	}
	PlayMoleMontage(EmergeMontage);

	// ④ 가시화 + 콜리전 복구
	ExitBurrowedState();

	// ⑤ 전기장 링 버퍼 갱신
	++EruptCount;
	UpdateElectricFieldRing(GroundPoint);

	OnEruptVisual(GetActorLocation());
}

// ================= 전기장 링 버퍼 =================

void ADRS2MoleBoss::UpdateElectricFieldRing(const FVector& NewEruptGround)
{
	if (!HasAuthority()) return;

	// 3차 구간이 아니면 전기장을 만들지 않는다. 단 "직전 지점" 추적은 필요 없으므로 갱신만 한다.
	if (!IsElectricFieldSegment())
	{
		PreviousEruptGround = NewEruptGround;
		bHasPreviousErupt = true;
		return;
	}

	// ① 이전 전기장 제거 = P(n-2) 소멸
	DestroyActiveField();

	// ② 직전 융기 지점에 새 전기장 생성 = P(n-1) 생성.
	//    첫 융기에는 직전 지점이 없으므로 아무것도 만들지 않는다.
	if (bHasPreviousErupt && ElectricFieldClass)
	{
		const FTransform SpawnTransform(FRotator::ZeroRotator, PreviousEruptGround);

		ADRS2ElectricField* Field = GetWorld()->SpawnActorDeferred<ADRS2ElectricField>(
			ElectricFieldClass, SpawnTransform, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (Field)
		{
			// ★인원수를 GE 스펙 레벨로 넘긴다. 실제 데미지 수치는 GE 의 Modifier 커브가 정한다.
			Field->InitField(this, GetEruptRadius(), GetBasePlayerCount());
			Field->FinishSpawning(SpawnTransform);
			ActiveField = Field;

			UE_LOG(LogDR, Verbose,
				TEXT("[MoleBoss] 전기장 생성 (융기 %d회차, 스펙 레벨(=인원) %d)"),
				EruptCount, GetBasePlayerCount());
		}
	}
	else if (bHasPreviousErupt && !ElectricFieldClass)
	{
		UE_LOG(LogDR, Error,
			TEXT("[MoleBoss] ElectricFieldClass 미설정 — 3차 구간 전기장이 생성되지 않습니다. BP_S2MoleBoss 를 확인하세요."));
	}

	// ③ 현재 지점을 다음 회차의 "직전 지점"으로 승격
	PreviousEruptGround = NewEruptGround;
	bHasPreviousErupt = true;
}

void ADRS2MoleBoss::DestroyActiveField()
{
	// 복제 액터라 서버에서만 파괴한다 (클라 파괴는 경고 로그만 남기고 무시된다)
	if (HasAuthority())
	{
		if (ADRS2ElectricField* Field = ActiveField.Get())
		{
			Field->Destroy();
		}
	}
	ActiveField = nullptr;
}

// ================= 사망 =================

void ADRS2MoleBoss::Die(const FVector& DeathImpulse)
{
	if (HasAuthority())
	{
		if (AbilitySystemComponent)
		{
			FGameplayTagContainer CancelTags;
			CancelTags.AddTag(FDRGameplayTags::Get().Abilities_MoleBoss_BurrowStrike);
			AbilitySystemComponent->CancelAbilities(&CancelTags);
		}

		// 잠수 중 사망해도 시체가 숨겨진 채 남지 않도록 가시화를 되돌린다
		ExitBurrowedState();

		DestroyActiveField();
	}

	Super::Die(DeathImpulse);
}
