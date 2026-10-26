// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Data/GameBalanceConfig.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Game/DRProgressionConfig.h"
#include "DRAbilityTypes.h"
#include "DRGameplayTags.h"
#include "Actor/DRCleanserSite.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "Game/DRGameModeBase.h"
#include "Game/DRGameInstance.h"
#include "Interaction/CombatInterface.h"
#include "Interaction/DRProximityHitOnly.h"
#include "DaeRune/DRLogChannels.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DRPlayerState.h"
#include "UI/HUD/DRHUD.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "Engine/OverlapResult.h"
#include "Game/DRStageGameState.h"
#include "GameFramework/Character.h"
// ★Plan8(벨보이)★ 모자·도발·몽타주 헬퍼
#include "AIController.h"
#include "AI/DRAIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Character/DRCharacterBase.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "Character/DRRobotVacuumCharacter.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"

bool UDRAbilitySystemLibrary::MakeWidgetControllerParams(const UObject* WorldContextObject, FWidgetControllerParams& OutWCParams, ADRHUD*& OutDRHUD)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		OutDRHUD = Cast<ADRHUD>(PC->GetHUD()); 
		if (OutDRHUD)
		{
			ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>();
			if (!PS) return false; // 클라이언트 초기화 중 PlayerState 복제 전이면 실패 처리

			UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
			UAttributeSet* AS = PS->GetAttributeSet();
			
			OutWCParams.AttributeSet = AS;
			OutWCParams.AbilitySystemComponent = ASC;
			OutWCParams.PlayerState = PS;
			OutWCParams.PlayerController = PC;
			return true;
		}
	}
	return false;
}

UOverlayWidgetController* UDRAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	FWidgetControllerParams WCParams;
	ADRHUD* DRHUD = nullptr;
	if (MakeWidgetControllerParams(WorldContextObject, WCParams, DRHUD))
	{
		return DRHUD->GetOverlayWidgetController(WCParams);
	}
	return nullptr;
}

void UDRAbilitySystemLibrary::InitializeDefaultAttributes(const UObject* WorldContextObject, ECharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC)
{
	AActor* AvatarActor = ASC->GetAvatarActor();

	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	FCharacterClassDefaultInfo ClassDefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);

	// 헬퍼 함수로 간소화
	CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.PrimaryAttributes, AvatarActor, Level);
	CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.VitalAttributes, AvatarActor, Level);
}

void UDRAbilitySystemLibrary::GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ECharacterClass CharacterClass)
{
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (CharacterClassInfo == nullptr) return;

	int32 CharacterLevel = 1;
	if (ASC->GetAvatarActor()->Implements<UCombatInterface>())
	{
		CharacterLevel = ICombatInterface::Execute_GetPlayerLevel(ASC->GetAvatarActor());
	}
	
	for (TSubclassOf<UGameplayAbility> AbilityClass : CharacterClassInfo->CommonAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CharacterLevel);
		ASC->GiveAbility(AbilitySpec);
	}
	const FCharacterClassDefaultInfo& DefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CharacterLevel);
		ASC->GiveAbility(AbilitySpec);
	}
	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.DeathAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CharacterLevel);
		ASC->GiveAbility(AbilitySpec);
	}
}

UCharacterClassInfo* UDRAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
	const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (DRGameMode == nullptr) return nullptr;
	return DRGameMode->EnemyCharacterClassInfo;
}

UPlayerCharacterClassInfo* UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(const UObject* WorldContextObject)
{
	// GameInstance는 서버/클라이언트 모두에 존재하므로 양쪽에서 안전하게 접근 가능
	const UDRGameInstance* DRGameInstance = Cast<UDRGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
	if (DRGameInstance == nullptr) return nullptr;
	return DRGameInstance->PlayerCharacterClassInfo;
}

TSubclassOf<UUserWidget> UDRAbilitySystemLibrary::GetCharacterInfoWidgetClass(const UObject* WorldContextObject, EPlayerCharacterClass PlayerClass)
{
	UPlayerCharacterClassInfo* ClassInfo = GetPlayerCharacterClassInfo(WorldContextObject);
	if (ClassInfo == nullptr) return nullptr;

	FCharacterClassDefaultInfo Info = ClassInfo->GetClassDefaultInfo(PlayerClass);
	return Info.CharacterInfoWidgetClass;
}

void UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(
	const UObject* WorldContextObject, EPlayerCharacterClass PlayerClass, float Level, UAbilitySystemComponent* ASC)
{
	AActor* AvatarActor = ASC->GetAvatarActor();

	UPlayerCharacterClassInfo* ClassInfo = GetPlayerCharacterClassInfo(WorldContextObject);
	if (!ClassInfo) return;

	// Step 1: 기존 활성 GE 제거 (Infinite/HasDuration GE 대비)
	for (auto& Pair : ClassInfo->CharacterClassInformation)
	{
		FCharacterClassDefaultInfo& Info = Pair.Value;
		if (Info.PrimaryAttributes)
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(Info.PrimaryAttributes, ASC);
		}
		if (Info.VitalAttributes)
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(Info.VitalAttributes, ASC);
		}
	}

	// Step 2: 속성 BaseValue 초기화 (Instant GE에 의한 누적값 제거)
	if (const UDRAttributeSet* DRAS = ASC->GetSet<UDRAttributeSet>())
	{
		ASC->SetNumericAttributeBase(DRAS->GetMaxHealthAttribute(), 0.f);
		ASC->SetNumericAttributeBase(DRAS->GetMaxWaterAttribute(), 0.f);
		ASC->SetNumericAttributeBase(DRAS->GetMoveSpeedAttribute(), 0.f);
		ASC->SetNumericAttributeBase(DRAS->GetHealthAttribute(), 0.f);
		ASC->SetNumericAttributeBase(DRAS->GetWaterAttribute(), 0.f);
	}

	// Step 3: 선택된 클래스의 GE 적용 (깨끗한 상태에서)
	FCharacterClassDefaultInfo ClassDefaultInfo = ClassInfo->GetClassDefaultInfo(PlayerClass);
	CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.PrimaryAttributes, AvatarActor, Level);
	CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.VitalAttributes, AvatarActor, Level);
}

void UDRAbilitySystemLibrary::GivePlayerStartupAbilities(
	const UObject* WorldContextObject, UAbilitySystemComponent* ASC, EPlayerCharacterClass PlayerClass)
{
	UPlayerCharacterClassInfo* ClassInfo = GetPlayerCharacterClassInfo(WorldContextObject);
	if (!ClassInfo) return;

	UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(ASC);
	if (!DRASC) return;

	// 공통 + 클래스별 어빌리티를 하나의 배열로 수집
	TArray<TSubclassOf<UGameplayAbility>> AllAbilities;
	AllAbilities.Append(ClassInfo->CommonAbilities);

	const FCharacterClassDefaultInfo& DefaultInfo = ClassInfo->GetClassDefaultInfo(PlayerClass);
	AllAbilities.Append(DefaultInfo.StartupAbilities);

	// AddCharacterAbilities가 DynamicAbilityTags, InputTagToAbilityMap,
	// bStartupAbilitiesGiven, AbilitiesGivenDelegate를 모두 처리
	DRASC->AddCharacterAbilities(AllAbilities);
}

UAbilityInfo* UDRAbilitySystemLibrary::GetAbilityInfo(const UObject* WorldContextObject)
{
	const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (DRGameMode == nullptr) return nullptr;
	return DRGameMode->AbilityInfo;
}

UGameBalanceConfig* UDRAbilitySystemLibrary::GetGameBalanceConfig(const UObject* WorldContextObject)
{
	const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (DRGameMode == nullptr) return nullptr;
	return DRGameMode->GameBalanceConfig;
}

// ========================= 업그레이드 칩 조회 (Plan3(공용칩) §17.5) =========================

const FDRUpgradeRuntime& UDRAbilitySystemLibrary::GetUpgradeRuntimeForASC(const UAbilitySystemComponent* ASC)
{
	// 칩이 없는 대상(적 / 클렌저 사이트)을 위한 항등 캐시.
	// 호출부가 null 분기를 하지 않아도 되도록 항상 유효한 참조를 돌려준다.
	static const FDRUpgradeRuntime EmptyRuntime;

	if (!ASC) return EmptyRuntime;

	// 플레이어는 ASC 를 PlayerState 가 소유하므로 OwnerActor 가 곧 PlayerState 다
	if (const ADRPlayerState* DRPS = Cast<ADRPlayerState>(ASC->GetOwnerActor()))
	{
		return DRPS->GetUpgradeRuntime();
	}

	// 폴백: 아바타의 PlayerState (ASC 소유 구조가 다른 경우 대비)
	if (const APawn* AvatarPawn = Cast<APawn>(ASC->GetAvatarActor()))
	{
		if (const ADRPlayerState* DRPS = AvatarPawn->GetPlayerState<ADRPlayerState>())
		{
			return DRPS->GetUpgradeRuntime();
		}
	}

	return EmptyRuntime;
}

bool UDRAbilitySystemLibrary::HasUpgradeFeature(const UAbilitySystemComponent* ASC, FGameplayTag FeatureTag)
{
	return GetUpgradeRuntimeForASC(ASC).HasFeature(FeatureTag);
}

const UDRPlayerAttributeSet* UDRAbilitySystemLibrary::GetPlayerAttributeSetFromASC(const UAbilitySystemComponent* ASC)
{
	if (!ASC) return nullptr;
	return Cast<UDRPlayerAttributeSet>(ASC->GetAttributeSet(UDRPlayerAttributeSet::StaticClass()));
}

const FDRChipTuning& UDRAbilitySystemLibrary::GetChipTuning(const UObject* WorldContextObject)
{
	// Config 미지정/월드 없음이어도 기본값으로 동작하게 한다 —
	// 튜닝 값이 없다고 전투 계산이 0 으로 무너지면 원인 추적이 훨씬 어려워진다.
	static const FDRChipTuning DefaultTuning;

	if (!WorldContextObject) return DefaultTuning;

	// GameBalanceConfig 와 달리 GameMode 를 타지 않는다 — 클라이언트에서도 필요하다.
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		if (const UDRGameInstance* GI = World->GetGameInstance<UDRGameInstance>())
		{
			if (const UDRProgressionConfig* Config = GI->ProgressionConfig)
			{
				return Config->ChipTuning;
			}
		}
	}

	return DefaultTuning;
}

// ========================= 공격 속도 (Plan5(공속칩)) =========================

int32 UDRAbilitySystemLibrary::GetEffectStackCountWithTag(const UAbilitySystemComponent* ASC, const FGameplayTag& Tag)
{
	if (!ASC || !Tag.IsValid()) return 0;

	// ★Plan5(공기탄칩)★ OwningTags 쿼리 — 에셋 태그 ∪ 부여(Target) 태그를 본다.
	// 예전의 GetActiveEffectsWithAllTags 는 ★에셋 태그만★ 비교해서(UE 5.5 FGameplayEffectQuery::Matches),
	// 부여 태그만 단 GE(흡착 표식 등)는 스택이 늘 0 으로 읽혔다. 공속 GE 들은 두 태그를 다 달고 있어 결과는 같다.
	for (const FActiveGameplayEffectHandle& Handle :
		ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tag))))
	{
		const int32 Stacks = ASC->GetCurrentStackCount(Handle);
		if (Stacks > 0) return Stacks;
	}
	return 0;
}

float UDRAbilitySystemLibrary::GetAttackSpeedMultiplier(const UAbilitySystemComponent* ASC)
{
	if (!ASC) return 1.f;

	const FDRGameplayTags& Tags = FDRGameplayTags::Get();
	const int32 SelfStacks = GetEffectStackCountWithTag(ASC, Tags.Buff_VendingMachine_AttackSpeed);
	const int32 SharedStacks = GetEffectStackCountWithTag(ASC, Tags.Buff_AttackSpeed_Shared);
	if (SelfStacks <= 0 && SharedStacks <= 0) return 1.f;

	const FDRAttackSpeedBuffChipTuning& Tuning = GetChipTuning(ASC).AttackSpeedBuff;

	// 자기 버프는 자기만 건다 → 스택당 값은 ★내 칩★ 으로 정한다 (D-64 · D-74).
	// ②(Share)를 낀 자판기는 자기 버프 대신 공유 버프를 받으므로 SelfStacks 가 0 이다.
	float SelfPerStack = Tuning.BaseSpeedPerStack;
	if (SelfStacks > 0)
	{
		const FDRUpgradeRuntime& Runtime = GetUpgradeRuntimeForASC(ASC);
		if (Runtime.HasFeature(Tags.Upgrade_Feature_Skill_AttackSpeedBuff_Overdrive))
		{
			SelfPerStack = Tuning.OverdriveSpeedPerStack;
		}
		else if (Runtime.HasFeature(Tags.Upgrade_Feature_Skill_AttackSpeedBuff_Decay))
		{
			SelfPerStack = Tuning.DecaySpeedPerStack;
		}
	}

	// 공유 버프를 거는 길은 칩 ②뿐이라 스택당 값이 하나다. 두 버프는 더한다 (D-64).
	return 1.f + SelfStacks * SelfPerStack + SharedStacks * Tuning.SharedSpeedPerStack;
}

void UDRAbilitySystemLibrary::GetAlivePlayersWithinRadius(const UObject* WorldContextObject, const FVector& Origin,
	float Radius, TArray<AActor*>& OutPlayers)
{
	OutPlayers.Reset();
	if (Radius <= 0.f) return;

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return;

	// 오버랩은 여유를 두고 넓게, 실제 판정은 아래 거리로 한다 (FindNearestTeammate 와 같은 방식)
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(Radius + 200.f));

	const float RadiusSq = FMath::Square(Radius);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!IsValid(Candidate) || OutPlayers.Contains(Candidate)) continue;

		// 팀원 판정 = 액터 태그 (IsNotFriend · FindNearestTeammate 와 같은 기준)
		if (!Candidate->ActorHasTag(FName("Player"))) continue;
		if (!Cast<ADRCharacter>(Candidate)) continue;
		if (Candidate->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Candidate)) continue;
		if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate)) continue;
		if (FVector::DistSquared(Origin, Candidate->GetActorLocation()) > RadiusSq) continue;

		OutPlayers.Add(Candidate);
	}
}

// ========== 액터가 스킬 단위 업그레이드를 읽는 길 (Plan5(씨앗칩)) ==========

FGameplayTag UDRAbilitySystemLibrary::GetUpgradeKeyTagFromAbilityTags(const FGameplayTagContainer& AbilityTags)
{
	// UDRAbilitySystemComponent::GetAbilityTagFromSpec 과 동일한 규칙 ("Abilities" 접두 태그)
	static const FGameplayTag AbilitiesTag = FGameplayTag::RequestGameplayTag(FName("Abilities"));

	for (const FGameplayTag& Tag : AbilityTags)
	{
		if (Tag.MatchesTag(AbilitiesTag))
		{
			return Tag;
		}
	}
	return FGameplayTag();
}

float UDRAbilitySystemLibrary::ApplySkillUpgrade(const UAbilitySystemComponent* SourceASC,
	const FGameplayTagContainer& SourceAbilityTags, EDRUpgradeStat Stat, float Base)
{
	const FGameplayTag KeyTag = GetUpgradeKeyTagFromAbilityTags(SourceAbilityTags);
	return GetUpgradeRuntimeForASC(SourceASC).ApplySkill(KeyTag, Stat, Base);
}

FActiveGameplayEffectHandle UDRAbilitySystemLibrary::ApplyTimedSpeedEffect(
	AActor* TargetActor, UAbilitySystemComponent* SourceASC,
	const FDRTimedSpeedEffect& Config, float SpeedMultiplier)
{
	if (!IsValid(TargetActor)) return FActiveGameplayEffectHandle();

	// TSoftClassPtr 는 1회 동기 로드한다 (UDRSynergyAuraComponent 와 같은 방식).
	// 미지정이면 조용히 넘어간다 — 호출부가 매 적중마다 경고를 쏟지 않도록.
	UClass* EffectClass = Config.Effect.LoadSynchronous();
	if (!EffectClass) return FActiveGameplayEffectHandle();

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC) return FActiveGameplayEffectHandle();

	// 소스 ASC 가 없으면(발사체 주인이 사라진 경우 등) 대상 자신을 소스로 삼는다.
	UAbilitySystemComponent* EffectSource = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = EffectSource->MakeEffectContext();
	Context.AddSourceObject(EffectSource->GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle = EffectSource->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (!SpecHandle.IsValid()) return FActiveGameplayEffectHandle();

	// ★Duration 이 SetByCaller 인데 값을 안 넣으면 지속시간 0 으로 즉시 사라진다.★
	const FDRGameplayTags& Tags = FDRGameplayTags::Get();
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_MoveSpeed_Mult, SpeedMultiplier);
	SpecHandle.Data->SetSetByCallerMagnitude(Tags.Data_Duration, FMath::Max(0.01f, Config.Duration));

	return TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

FActiveGameplayEffectHandle UDRAbilitySystemLibrary::ApplyTimedTagEffect(
	AActor* TargetActor, UAbilitySystemComponent* SourceASC,
	const TSoftClassPtr<UGameplayEffect>& Effect, float Duration)
{
	if (!IsValid(TargetActor)) return FActiveGameplayEffectHandle();

	// TSoftClassPtr 는 1회 동기 로드한다 (ApplyTimedSpeedEffect 와 같은 방식).
	// 미지정이면 조용히 넘어간다 — 호출부가 매 적중마다 경고를 쏟지 않도록.
	UClass* EffectClass = Effect.LoadSynchronous();
	if (!EffectClass) return FActiveGameplayEffectHandle();

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC) return FActiveGameplayEffectHandle();

	// 소스 ASC 가 없으면 대상 자신을 소스로 삼는다.
	UAbilitySystemComponent* EffectSource = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = EffectSource->MakeEffectContext();
	Context.AddSourceObject(EffectSource->GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle = EffectSource->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (!SpecHandle.IsValid()) return FActiveGameplayEffectHandle();

	// ★Duration 이 SetByCaller 인데 값을 안 넣으면 지속시간 0 으로 즉시 사라진다.★
	SpecHandle.Data->SetSetByCallerMagnitude(
		FDRGameplayTags::Get().Data_Duration, FMath::Max(0.01f, Duration));

	return TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

// ========================= 회복 / 흡혈 (Plan5(발톱칩)) =========================

void UDRAbilitySystemLibrary::ApplyHealToActor(AActor* TargetActor, UAbilitySystemComponent* SourceASC,
	TSubclassOf<UGameplayEffect> HealEffect, float HealAmount, const UObject* SourceObject)
{
	if (!IsValid(TargetActor) || HealAmount <= 0.f || !HealEffect) return;

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC) return;

	// 소스 ASC 가 없으면 대상 자신이 소스가 된다(자가 회복 형태 — 씨앗 발사체가 이 경로를 쓴다).
	UAbilitySystemComponent* EffectSource = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = EffectSource->MakeEffectContext();
	Context.AddSourceObject(SourceObject ? SourceObject : EffectSource->GetAvatarActor());

	FGameplayEffectSpecHandle SpecHandle = EffectSource->MakeOutgoingSpec(HealEffect, 1.f, Context);
	if (!SpecHandle.IsValid()) return;

	// GE 는 UExecCalc_Heal 이 이 SetByCaller 를 읽어 IncomingHealing 으로 넘긴다.
	SpecHandle.Data->SetSetByCallerMagnitude(FDRGameplayTags::Get().Heal, HealAmount);

	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UDRAbilitySystemLibrary::ApplyHealToActor(AActor* TargetActor, UAbilitySystemComponent* SourceASC,
	const TSoftClassPtr<UGameplayEffect>& HealEffect, float HealAmount, const UObject* SourceObject)
{
	// TSoftClassPtr 는 1회 동기 로드한다 (ApplyTimedSpeedEffect 와 같은 방식).
	//
	// ★TSubclassOf 로 ★명시적으로★ 감싼다.★ LoadSynchronous() 의 반환형은 UClass* 인데,
	// 그건 TSubclassOf 와 TSoftClassPtr 양쪽으로 암시적 변환이 가능해
	// 그냥 넘기면 두 오버로드 중 어느 것인지 모호해진다(C2668).
	const TSubclassOf<UGameplayEffect> LoadedClass = HealEffect.LoadSynchronous();
	ApplyHealToActor(TargetActor, SourceASC, LoadedClass, HealAmount, SourceObject);
}

float UDRAbilitySystemLibrary::GetHealableAmount(AActor* TargetActor, float RequestedHeal)
{
	if (!IsValid(TargetActor) || RequestedHeal <= 0.f) return 0.f;

	const UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC) return 0.f;

	// ★회복 차단(칩 ⑥ 광폭화)★ — HandleIncomingHealing 이 힐을 통째로 버린다.
	// 버려질 힐에 대가를 치르면 안 되므로 여기서 0 을 돌려준다.
	if (HasUpgradeFeature(TargetASC, FDRGameplayTags::Get().Upgrade_Feature_Combat_NoHealing))
	{
		return 0.f;
	}

	const UDRAttributeSet* TargetAS = Cast<UDRAttributeSet>(
		TargetASC->GetAttributeSet(UDRAttributeSet::StaticClass()));
	if (!TargetAS) return 0.f;

	// ★오염 상태★ — HandleCorruptionPurification 이 회복량과 무관하게 1회 소비해 정화한다.
	// 체력이 가득 차 있어도 "쓸모 있는" 회복이므로 요청량 전액을 소비된 것으로 본다.
	if (const UDRPlayerAttributeSet* TargetPlayerAS = Cast<UDRPlayerAttributeSet>(TargetAS))
	{
		if (TargetPlayerAS->IsCorrupted())
		{
			return RequestedHeal;
		}
	}

	// 일반: 빈 체력만큼만 소비된다. 만피면 0 → 호출부가 비용을 물지 않는다.
	return FMath::Clamp(TargetAS->GetMaxHealth() - TargetAS->GetHealth(), 0.f, RequestedHeal);
}

float UDRAbilitySystemLibrary::GetLifestealRatioForDamage(const FEffectProperties& Props)
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	// ① 소스가 흡혈 칩을 끼고 있는가
	if (!HasUpgradeFeature(Props.SourceASC, GameplayTags.Upgrade_Feature_Skill_Claw_Lifesteal))
	{
		return 0.f;
	}

	// ② 이 피해가 ★기본 공격에서★ 나왔는가.
	//    이 검사를 빠뜨리면 씨앗 폭탄·물대포 피해까지 흡혈된다.
	//    ApplyDamageEffect 가 컨텍스트에 SourceAbilityTags 를 실어 주므로 추가 배선이 필요 없다.
	const FDRGameplayEffectContext* DRContext =
		static_cast<const FDRGameplayEffectContext*>(Props.EffectContextHandle.Get());
	if (!DRContext) return 0.f;

	if (!DRContext->GetSourceAbilityTags().HasTagExact(GameplayTags.Abilities_GardenRobot_ClawSwipe))
	{
		return 0.f;
	}

	// ③ ★시전자가 오염 중이면 막는다★ (2026-10-08 사용자 결정) — 동전 ② 적중 회복(D-42)과 같은 이유.
	//    HandleCorruptionPurification 은 회복량과 무관하게 정화 + 컨테이너 1칸을 주므로,
	//    막지 않으면 오염되자마자 적 한 대로 즉시 정화된다. 오염 해제는 아군 회복으로만 하는 팀 메커니즘이다.
	if (const UDRPlayerAttributeSet* SourcePlayerAS = GetPlayerAttributeSetFromASC(Props.SourceASC))
	{
		if (SourcePlayerAS->IsCorrupted()) return 0.f;
	}

	const AActor* TuningContext = Props.SourceASC ? Props.SourceASC->GetAvatarActor() : nullptr;
	return FMath::Max(0.f, GetChipTuning(TuningContext).Claw.LifestealRatio);
}

float UDRAbilitySystemLibrary::GetHitHealAmountForDamage(const FEffectProperties& Props)
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	// ① 소스가 적중 회복 칩을 끼고 있는가
	if (!HasUpgradeFeature(Props.SourceASC, GameplayTags.Upgrade_Feature_Skill_Coin_HitHeal))
	{
		return 0.f;
	}

	// ② 이 피해가 ★자판기 기본 공격에서★ 나왔는가 (흡혈의 ClawSwipe 검사와 같은 이유 —
	//    빠뜨리면 자판기의 다른 피해원까지 회복된다). 동전·잭팟 캡슐은 둘 다 이 GA 의 태그를 싣는다.
	const FDRGameplayEffectContext* DRContext =
		static_cast<const FDRGameplayEffectContext*>(Props.EffectContextHandle.Get());
	if (!DRContext) return 0.f;

	if (!DRContext->GetSourceAbilityTags().HasTagExact(GameplayTags.Abilities_VendingMachine_BasicAttack))
	{
		return 0.f;
	}

	// ③ ★시전자가 오염 중이면 막는다★ — HandleCorruptionPurification 은 회복량과 무관하게
	//    정화 + 컨테이너 1칸을 준다. 오염 해제는 아군 회복으로만 하는 팀 메커니즘이다.
	//    (오염은 플레이어에게만 있다 — 판정 대상은 적이 아니라 ★시전자★ 다)
	if (const UDRPlayerAttributeSet* SourcePlayerAS = GetPlayerAttributeSetFromASC(Props.SourceASC))
	{
		if (SourcePlayerAS->IsCorrupted()) return 0.f;
	}

	const AActor* TuningContext = Props.SourceASC ? Props.SourceASC->GetAvatarActor() : nullptr;
	return FMath::Max(0.f, GetChipTuning(TuningContext).Coin.HitHealAmount);
}

bool UDRAbilitySystemLibrary::IsSuccessfulDebuff(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->IsSuccessfulDebuff();
	}
	return false;
}

float UDRAbilitySystemLibrary::GetDebuffDamage(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetDebuffDamage();
	}
	return 0.f;
}

float UDRAbilitySystemLibrary::GetDebuffDuration(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetDebuffDuration();
	}
	return 0.f;
}

FGameplayTag UDRAbilitySystemLibrary::GetDamageType(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		if (DREffectContext->GetDamageType().IsValid())
		{
			return *DREffectContext->GetDamageType();
		}
	}
	return FGameplayTag();
}

FVector UDRAbilitySystemLibrary::GetDeathImpulse(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetDeathImpulse();
	}
	return FVector::ZeroVector;
}

FVector UDRAbilitySystemLibrary::GetKnockbackForce(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetKnockbackForce();
	}
	return FVector::ZeroVector;
}

FGameplayTagContainer UDRAbilitySystemLibrary::GetSourceAbilityTags(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetSourceAbilityTags();
	}
	return FGameplayTagContainer();
}

void UDRAbilitySystemLibrary::SetIsSuccessfulDebuff(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, bool bInSuccessfulDebuff)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetIsSuccessfulDebuff(bInSuccessfulDebuff);
	}
}

void UDRAbilitySystemLibrary::SetDebuffDamage(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, float InDamage)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetDebuffDamage(InDamage);
	}
}

void UDRAbilitySystemLibrary::SetDebuffDuration(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, float InDuration)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetDebuffDuration(InDuration);
	}
}

void UDRAbilitySystemLibrary::SetDamageType(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, const FGameplayTag& InDamageType)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		const TSharedPtr<FGameplayTag> DamageType = MakeShared<FGameplayTag>(InDamageType);
		DREffectContext->SetDamageType(DamageType);
	}
}

void UDRAbilitySystemLibrary::SetDeathImpulse(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, const FVector& InImpulse)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetDeathImpulse(InImpulse);
	}
}

void UDRAbilitySystemLibrary::SetKnockbackForce(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, const FVector& InForce)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{ 
		DREffectContext->SetKnockbackForce(InForce);
	}
}

void UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(const UObject* WorldContextObject, TArray<AActor*>& OutOverlappingActors, const TArray<AActor*>& ActorsToIgnore, float Radius, const FVector& SphereOrigin)
{
	FCollisionQueryParams SphereParams;
	SphereParams.AddIgnoredActors(ActorsToIgnore);

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, SphereOrigin, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects), FCollisionShape::MakeSphere(Radius), SphereParams);

		for (FOverlapResult& Overlap : Overlaps)
		{
			if (Overlap.GetActor()->Implements<UCombatInterface>() && !ICombatInterface::Execute_IsDead(Overlap.GetActor()))
			{
				OutOverlappingActors.AddUnique(ICombatInterface::Execute_GetAvatar(Overlap.GetActor()));
			}
			// ★근접 전용 피격 대상(홀로그램 두더지 등)도 수집한다 (2026-08-27 추가).
			//   ICombatInterface 를 구현하지 않아 위 분기에서 걸러지고 있었고,
			//   그 결과 광역·근접 공격이 두더지를 대상으로 잡지 못했다.
			else if (Cast<IDRProximityHitOnly>(Overlap.GetActor()))
			{
				OutOverlappingActors.AddUnique(Overlap.GetActor());
			}
			else if (ADRCleanserSite* CleanserSite = Cast<ADRCleanserSite>(Overlap.GetActor()))
			{
				OutOverlappingActors.AddUnique(CleanserSite);
			}
		}
	}
}

void UDRAbilitySystemLibrary::GetClosestTargets(int32 MaxTargets, const TArray<AActor*>& Actors, TArray<AActor*>& OutClosestTargets, const FVector& Origin)
{
	if (Actors.Num() <= MaxTargets)
	{
		OutClosestTargets = Actors;
		return;
	}

	TArray<AActor*> ActorsToCheck = Actors;
	int32 NumTargetsFound = 0;

	while (NumTargetsFound < MaxTargets)
	{
		if (ActorsToCheck.Num() == 0) break;
		double ClosestDistance = TNumericLimits<double>::Max();
		AActor* ClosestActor = nullptr;
		for (AActor* PotentialTarget : ActorsToCheck)
		{
			const double Distance = (PotentialTarget->GetActorLocation() - Origin).Length();
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestActor = PotentialTarget;
			}
		}
		ActorsToCheck.Remove(ClosestActor);
		OutClosestTargets.AddUnique(ClosestActor); 
		++NumTargetsFound;
	}
}

AActor* UDRAbilitySystemLibrary::GetClosestCleanserSite(APawn* ControlledPawn)
{
	if (!ControlledPawn) return nullptr;

	UWorld* World = ControlledPawn->GetWorld();
	if (!World) return nullptr;

	ADRStageGameState* GameState = Cast<ADRStageGameState>(World->GetGameState());
	if (!GameState) return nullptr;

	const TArray<TObjectPtr<ADRCleanserSite>>& CleanserSites = GameState->GetCleanserSites();
	if (CleanserSites.Num() == 0) return nullptr;

	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	ADRCleanserSite* Closest = nullptr;
	float ClosestDistSq = TNumericLimits<float>::Max();

	for (ADRCleanserSite* Site : CleanserSites)
	{
		if (!Site) continue;
		const float DistSq = FVector::DistSquared(Site->GetActorLocation(), PawnLocation);
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			Closest = Site;
		}
	}

	return Closest;
}

bool UDRAbilitySystemLibrary::IsActorReachable(APawn* Asker, AActor* Target)
{
	if (!IsValid(Asker) || !IsValid(Target)) return false;

	UWorld* World = Asker->GetWorld();
	if (!World) return false;

	// 벽/지형(WorldStatic)만 차단으로 간주. 다른 캐릭터/적은 무시.
	const FVector Start = Asker->GetActorLocation();
	const FVector End = Target->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(IsActorReachable), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Asker);
	Params.AddIgnoredActor(Target);

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);
	return !bBlocked;
}

bool UDRAbilitySystemLibrary::IsNotFriend(AActor* FirstActor, AActor* SecondActor)
{
	if (!IsValid(FirstActor) || !IsValid(SecondActor)) return false;
	const bool bBothArePlayers = FirstActor->ActorHasTag(FName("Player")) && SecondActor->ActorHasTag(FName("Player"));
	const bool bBothAreEnemies = FirstActor->ActorHasTag(FName("Enemy")) && SecondActor->ActorHasTag(FName("Enemy"));
	const bool bFriends = bBothArePlayers || bBothAreEnemies;
	return !bFriends;
}

bool UDRAbilitySystemLibrary::IsValidDamageTarget(AActor* Attacker, AActor* Target)
{
	if (!IsValid(Attacker) || !IsValid(Target)) return false;

	// 아군 판정 (액터 태그 기반)
	if (!IsNotFriend(Attacker, Target)) return false;

	// 전투 대상이면 사망 여부까지 본다. 인터페이스가 없어도 유효 대상일 수 있으므로 통과시킨다.
	if (Target->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Target))
	{
		return false;
	}

	return true;
}

FGameplayEffectContextHandle UDRAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams)
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
	AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();

	// ===== 근접 전용 피격 대상 가로채기 (Plan6 §5.11) =====
	// 홀로그램 두더지처럼 "2m 안에서의 공격만 유효하고 그 밖은 투과"하는 대상을 여기서 처리한다.
	// 모든 공격이 이 함수를 지나므로 공격 종류마다 손대지 않아도 된다.
	// GE 는 적용하지 않는다 - 어트리뷰트를 쓰지 않는 대상이라 PostGameplayEffectExecute 를 태우면 위험하다.
	if (DamageEffectParams.TargetAbilitySystemComponent)
	{
		AActor* TargetAvatarActor = DamageEffectParams.TargetAbilitySystemComponent->GetAvatarActor();
		if (IDRProximityHitOnly* ProximityTarget = Cast<IDRProximityHitOnly>(TargetAvatarActor))
		{
			if (ProximityTarget->AcceptsHitFrom(SourceAvatarActor))
			{
				ProximityTarget->HandleProximityHit(SourceAvatarActor);
			}
			// 범위 밖이면 아무 일도 하지 않는다 (투과)
			return FGameplayEffectContextHandle();
		}
	}

	FGameplayEffectContextHandle EffectContexthandle = DamageEffectParams.SourceAbilitySystemComponent->MakeEffectContext();
	EffectContexthandle.AddSourceObject(SourceAvatarActor);
	SetDeathImpulse(EffectContexthandle, DamageEffectParams.DeathImpulse);
	SetKnockbackForce(EffectContexthandle, DamageEffectParams.KnockbackForce);
	if (FDRGameplayEffectContext* DRContext = static_cast<FDRGameplayEffectContext*>(EffectContexthandle.Get()))
	{
		DRContext->SetSourceAbilityTags(DamageEffectParams.SourceAbilityTags);
	}
	const FGameplayEffectSpecHandle SpecHandle = DamageEffectParams.SourceAbilitySystemComponent->MakeOutgoingSpec(DamageEffectParams.DamageGameplayEffectClass, DamageEffectParams.AbilityLevel, EffectContexthandle);

	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, DamageEffectParams.DamageType, DamageEffectParams.BaseDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Chance, DamageEffectParams.DebuffChance);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Damage, DamageEffectParams.DebuffDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Duration, DamageEffectParams.DebuffDuration);

	DamageEffectParams.TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	return EffectContexthandle;
}

TArray<FRotator> UDRAbilitySystemLibrary::EvenlySpacedRotators(const FVector& Forward, const FVector& Axis, float Spread, int32 NumRotators)
{
	TArray<FRotator> Rotators; 

	const FVector LeftOfSpread = Forward.RotateAngleAxis(-Spread / 2.f, Axis);
	if (NumRotators > 1)
	{
		const float DeltaSpread = Spread / (NumRotators - 1);
		for (int32 i = 0; i < NumRotators; i++)
		{
			const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, Axis);
			Rotators.Add(Direction.Rotation());
		}
	}
	else
	{
		Rotators.Add(Forward.Rotation());
	}
	return Rotators;
}

TArray<FVector> UDRAbilitySystemLibrary::EvenlyRotatedVectors(const FVector& Forward, const FVector& Axis, float Spread, int32 NumVectors)
{
	TArray<FVector> Vectors; 

	const FVector LeftOfSpread = Forward.RotateAngleAxis(-Spread / 2.f, Axis);
	if (NumVectors > 1)
	{
		const float DeltaSpread = Spread / (NumVectors - 1);
		for (int32 i = 0; i < NumVectors; i++)
		{
			const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, Axis);
			Vectors.Add(Direction);
		}
	}
	else
	{
		Vectors.Add(Forward);
	}
	return Vectors;
}

bool UDRAbilitySystemLibrary::CheckActorWallCollision(AActor* Target, float CheckDistance)
{
	if (!Target)
	{
		return false;
	}

	// 넉백 방향 계산 (타겟의 현재 속도 사용)
	FVector Velocity = FVector::ZeroVector;
	if (ACharacter* Character = Cast<ACharacter>(Target))
	{
		Velocity = Character->GetVelocity();
	}

	if (Velocity.IsNearlyZero())
	{
		return false;
	}

	// 벽 체크를 위한 트레이스
	FVector StartLocation = Target->GetActorLocation();
	FVector EndLocation = StartLocation + (Velocity.GetSafeNormal() * CheckDistance);

	FHitResult WallHitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Target);

	bool bHitWall = Target->GetWorld()->LineTraceSingleByChannel(
		WallHitResult,
		StartLocation,
		EndLocation,
		ECC_WorldStatic,
		QueryParams
	);

	if (bHitWall && WallHitResult.bBlockingHit)
	{
		return true;
	}

	return false;
}

FGameplayEffectSpecHandle UDRAbilitySystemLibrary::CreateAndApplyEffectSpec(
	UAbilitySystemComponent* ASC,
	TSubclassOf<UGameplayEffect> EffectClass,
	AActor* SourceObject,
	float Level)
{
	FGameplayEffectSpecHandle SpecHandle = CreateEffectSpec(ASC, EffectClass, SourceObject, Level);

	if (SpecHandle.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UDRAbilitySystemLibrary::CreateEffectSpec(
	UAbilitySystemComponent* ASC,
	TSubclassOf<UGameplayEffect> EffectClass,
	AActor* SourceObject,
	float Level)
{
	if (!ASC || !EffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	if (SourceObject)
	{
		ContextHandle.AddSourceObject(SourceObject);
	}

	return ASC->MakeOutgoingSpec(EffectClass, Level, ContextHandle);
}

void UDRAbilitySystemLibrary::ApplyEffectSpecWithSetByCaller(
	UAbilitySystemComponent* ASC,
	FGameplayEffectSpecHandle& SpecHandle,
	const FGameplayTag& Tag,
	float Magnitude)
{
	if (!ASC || !SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data.Get()->SetSetByCallerMagnitude(Tag, Magnitude);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

// ========================= ★Plan8(모자)★ 모자 보호막 =========================

bool UDRAbilitySystemLibrary::HasBellBoyHat(AActor* Target)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	return ASC && ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Buff_BellBoy_Hat);   // 부모 태그 — 자식 둘 다 잡힌다
}

bool UDRAbilitySystemLibrary::TryEquipBellBoyHat(AActor* Target, UAbilitySystemComponent* SourceASC, int32 Charges)
{
	ADRCharacterBase* Character = Cast<ADRCharacterBase>(Target);
	if (!IsValid(Character) || !Character->HasAuthority()) return false;
	if (ICombatInterface::Execute_IsDead(Character)) return false;

	UAbilitySystemComponent* TargetASC = Character->GetAbilitySystemComponent();
	if (!TargetASC) return false;

	// 최대 1개 (D-22) — 이미 쓰고 있으면 호출부가 모자를 남긴다
	if (TargetASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Buff_BellBoy_Hat)) return false;

	const UGameBalanceConfig* Config = GetGameBalanceConfig(Character);
	const TSubclassOf<UGameplayEffect> EffectClass = !Config ? nullptr
		: (Charges >= 2 ? Config->BellBoy.HatDoubleEffect : Config->BellBoy.HatSingleEffect);
	if (!EffectClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[BellBoy] DA_GameBalanceConfig.BellBoy 의 모자 GE 가 비어 있다 — 모자 무동작"));
		return false;
	}

	// 소스 ASC 가 없으면(던진 벨보이가 나감 등) 대상 자신을 소스로 — ApplyTimedTagEffect 와 같은 처리
	UAbilitySystemComponent* Source = SourceASC ? SourceASC : TargetASC;
	FGameplayEffectContextHandle Context = Source->MakeEffectContext();
	Context.AddSourceObject(Source->GetAvatarActor());

	const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(EffectClass, 1.f, Context);
	return Spec.IsValid() && TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).IsValid();
}

bool UDRAbilitySystemLibrary::TryConsumeBellBoyHat(UAbilitySystemComponent* TargetASC)
{
	if (!TargetASC || !TargetASC->IsOwnerActorAuthoritative()) return false;

	const FDRGameplayTags& Tags = FDRGameplayTags::Get();
	// ★빠른 탈출★ — 모든 피해가 이 함수를 지난다. 모자가 없으면 태그 조회 한 번으로 끝난다.
	if (!TargetASC->HasMatchingGameplayTag(Tags.Buff_BellBoy_Hat)) return false;

	FGameplayCueParameters Cue;
	Cue.Location = TargetASC->GetAvatarActor() ? TargetASC->GetAvatarActor()->GetActorLocation() : FVector::ZeroVector;

	// ① Double → Single ★Single 을 먼저 걸고 Double 을 지운다★ (D-20)
	//    순서를 거꾸로 하면 한 순간 "모자 없음"이 되어 그 사이 들어온 피해를 못 막는다.
	const TArray<FActiveGameplayEffectHandle> Doubles =
		TargetASC->GetActiveEffects(
			FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tags.Buff_BellBoy_Hat_Double)));
	if (Doubles.Num() > 0)
	{
		const UGameBalanceConfig* Config = GetGameBalanceConfig(TargetASC->GetAvatarActor());
		if (Config && Config->BellBoy.HatSingleEffect)
		{
			const FGameplayEffectSpecHandle Single = TargetASC->MakeOutgoingSpec(
				Config->BellBoy.HatSingleEffect, 1.f, TargetASC->MakeEffectContext());
			if (Single.IsValid())
			{
				TargetASC->ApplyGameplayEffectSpecToSelf(*Single.Data.Get());
			}
		}
		TargetASC->RemoveActiveGameplayEffect(Doubles[0]);
		TargetASC->ExecuteGameplayCue(Tags.GameplayCue_BellBoy_HatBlock, Cue);
		return true;
	}

	// ② Single → 제거
	const TArray<FActiveGameplayEffectHandle> Singles =
		TargetASC->GetActiveEffects(
			FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tags.Buff_BellBoy_Hat_Single)));
	if (Singles.Num() > 0)
	{
		TargetASC->RemoveActiveGameplayEffect(Singles[0]);
		TargetASC->ExecuteGameplayCue(Tags.GameplayCue_BellBoy_HatBlock, Cue);
		return true;
	}
	return false;
}

USceneComponent* UDRAbilitySystemLibrary::GetHatAttachPoint(AActor* Target, FName& OutSocketName, FVector& OutRelativeOffset)
{
	OutSocketName = NAME_None;
	OutRelativeOffset = FVector::ZeroVector;

	ACharacter* Character = Cast<ACharacter>(Target);
	if (!Character) return Target ? Target->GetRootComponent() : nullptr;

	static const FName HatSocketName(TEXT("HatSocket"));
	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		if (Mesh->DoesSocketExist(HatSocketName))
		{
			OutSocketName = HatSocketName;
			return Mesh;
		}
	}

	// 폴백: 캡슐 꼭대기 + 10uu (머리를 따라 흔들리지는 않지만 모든 적에게 동작한다)
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	OutRelativeOffset = FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 10.f);
	return Capsule;
}

// ========================= ★Plan8(시선집중)★ 도발 =========================

void UDRAbilitySystemLibrary::GetAliveEnemiesWithinRadius(const UObject* WorldContextObject, const FVector& Origin,
	float Radius, TArray<AActor*>& OutEnemies)
{
	OutEnemies.Reset();
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return;

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius));

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ADREnemy* Enemy = Cast<ADREnemy>(Overlap.GetActor());
		if (!IsValid(Enemy) || ICombatInterface::Execute_IsDead(Enemy)) continue;
		OutEnemies.AddUnique(Enemy);   // 커스텀 히트박스도 Pawn 타입이라 한 적이 여러 번 잡힌다
	}
}

bool UDRAbilitySystemLibrary::CanBeTaunted(AActor* Enemy)
{
	const APawn* Pawn = Cast<APawn>(Enemy);
	const AAIController* AIC = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	const UBlackboardComponent* BB = AIC ? AIC->GetBlackboardComponent() : nullptr;
	if (!BB) return false;

	return BB->GetKeyID(DRBlackboardKeys::FirstAttacker) != FBlackboard::InvalidKey
		|| BB->GetKeyID(DRBlackboardKeys::TargetToFollow) != FBlackboard::InvalidKey;
}

namespace
{
	// 이 ASC 에 걸린 도발 GE. 도발 중이 아니면 nullptr. 새 도발이 이전 것을 지우므로 항상 1개다 (Plan8 D-15)
	// GetTauntInfo 와 ★Plan5(도발칩)★ GetTauntDamageTakenMultiplier 가 같이 쓴다.
	const FActiveGameplayEffect* FindActiveTauntEffect(const UAbilitySystemComponent* ASC,
		FActiveGameplayEffectHandle* OutHandle = nullptr)
	{
		const FDRGameplayTags& Tags = FDRGameplayTags::Get();
		if (!ASC || !ASC->HasMatchingGameplayTag(Tags.Debuff_Taunted)) return nullptr;   // 빠른 탈출

		// ★OwningTags 쿼리★ — Debuff.Taunted 는 GE 의 "부여 태그"(Target Tags)다. GetActiveEffectsWithAllTags 는
		// "에셋 태그"만 비교해서 태그가 붙어 있어도 GE 를 못 찾는다 (Plan8 T-29). 모자 조회도 같은 이유
		const TArray<FActiveGameplayEffectHandle> Handles =
			ASC->GetActiveEffects(
				FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tags.Debuff_Taunted)));
		if (Handles.Num() == 0) return nullptr;

		if (OutHandle) *OutHandle = Handles[0];
		return ASC->GetActiveGameplayEffect(Handles[0]);
	}
}

bool UDRAbilitySystemLibrary::GetTauntInfo(AActor* Enemy, AActor*& OutTaunter, float& OutDamageMultiplier,
	FActiveGameplayEffectHandle* OutHandle)
{
	OutTaunter = nullptr;
	OutDamageMultiplier = 1.f;

	FActiveGameplayEffectHandle Handle;
	const FActiveGameplayEffect* Active =
		FindActiveTauntEffect(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy), &Handle);
	if (!Active) return false;

	if (const UAbilitySystemComponent* SourceASC = Active->Spec.GetContext().GetOriginalInstigatorAbilitySystemComponent())
	{
		OutTaunter = SourceASC->GetAvatarActor();                  // ★Instigator(PlayerState)가 아니다 (T-6)★
	}
	OutDamageMultiplier = Active->Spec.GetSetByCallerMagnitude(FDRGameplayTags::Get().Data_Taunt_DamageMult, false, 1.f);
	if (OutHandle) *OutHandle = Handle;
	return true;
}

AActor* UDRAbilitySystemLibrary::GetTauntTarget(AActor* Enemy)
{
	AActor* Taunter = nullptr;
	float Multiplier = 1.f;
	if (!GetTauntInfo(Enemy, Taunter, Multiplier) || !IsValid(Taunter)) return nullptr;
	if (Taunter->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Taunter)) return nullptr;
	return Taunter;
}

float UDRAbilitySystemLibrary::GetTauntDamageMultiplier(AActor* SourceAvatar)
{
	AActor* Taunter = nullptr;
	float Multiplier = 1.f;
	if (!IsValid(SourceAvatar) || !GetTauntInfo(SourceAvatar, Taunter, Multiplier)) return 1.f;
	return FMath::Max(0.f, Multiplier);   // 음수로 피해가 회복으로 뒤집히지 않게
}

float UDRAbilitySystemLibrary::GetTauntDamageTakenMultiplier(const UAbilitySystemComponent* TargetASC)
{
	// ★Plan5(도발칩)★ ① 적 피해 관문이 모든 피해마다 부른다 — 도발 중이 아니면 태그 조회 한 번으로 끝난다
	const FActiveGameplayEffect* Active = FindActiveTauntEffect(TargetASC);
	if (!Active) return 1.f;

	// 칩 없는 벨보이의 도발은 1.0 을 넣는다(값이 없어도 기본 1.0). 음수로 피해가 회복으로 뒤집히지 않게 (D-129)
	return FMath::Max(0.f,
		Active->Spec.GetSetByCallerMagnitude(FDRGameplayTags::Get().Data_Taunt_DamageTakenMult, false, 1.f));
}

bool UDRAbilitySystemLibrary::ApplyTauntAggro(AAIController* OwnerController)
{
	if (!OwnerController || !OwnerController->HasAuthority()) return false;

	APawn* Pawn = OwnerController->GetPawn();
	UBlackboardComponent* BB = OwnerController->GetBlackboardComponent();
	auto HasKey = [BB](const FName& Key) { return BB && BB->GetKeyID(Key) != FBlackboard::InvalidKey; };

	AActor* Taunter = nullptr;
	float Multiplier = 1.f;
	FActiveGameplayEffectHandle Handle;
	const bool bTaunted = Pawn && GetTauntInfo(Pawn, Taunter, Multiplier, &Handle);

	const bool bTaunterAlive = bTaunted && IsValid(Taunter)
		&& !(Taunter->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Taunter));

	if (!bTaunterAlive)
	{
		// [임시 진단] 도발 갱신이 멈추는 이유
		UE_LOG(LogDR, Warning, TEXT("[TauntDiag] 5. 갱신 종료 %s — Taunted=%d Taunter=%s (BB=%d)"),
			*GetNameSafe(Pawn), bTaunted ? 1 : 0, *GetNameSafe(Taunter), BB ? 1 : 0);

		// 도발자가 죽었거나 사라졌다 → 도발 GE 를 지운다 (D-12).
		// 블랙보드는 건드리지 않는다 — 죽은 타깃은 BTS_CheckValidTarget 이 알아서 비운다.
		if (bTaunted)
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn))
			{
				ASC->RemoveActiveGameplayEffect(Handle);
			}
		}
		return false;
	}

	// ★키 3개를 같이 쓴다★ — TargetToFollow 만 쓰면 HasFirstAttacker 가 false 로 남아
	// BTS_FindNearestPlayer_* 가 다음 틱에 가장 가까운 플레이어로 덮어쓴다 (바이트코드 확인 — Plan8 §2.4).
	// 값이 같으면 블랙보드는 옵저버를 깨우지 않으므로 주기적으로 다시 써도 MoveTo 가 재경로를 잡지 않는다.

	// [임시 진단] 타깃이 벨보이가 아닐 때만 찍는다 — 첫 적용, 또는 0.2초 사이에 누가 덮어썼을 때(로그 도배 방지)
	{
		const UObject* PrevTarget = HasKey(DRBlackboardKeys::TargetToFollow) ? BB->GetValueAsObject(DRBlackboardKeys::TargetToFollow) : nullptr;
		if (PrevTarget != Taunter)
		{
			const UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(OwnerController->GetBrainComponent());
			const UBTNode* ActiveNode = BTComp ? BTComp->GetActiveNode() : nullptr;
			UE_LOG(LogDR, Warning, TEXT("[TauntDiag] 4. 어그로 쓰기 %s : 이전 TargetToFollow=%s → %s | 키 FA=%d HFA=%d TTF=%d Dist=%d Unreach=%d | BT=%s 활성노드=%s"),
				*GetNameSafe(Pawn), *GetNameSafe(PrevTarget), *GetNameSafe(Taunter),
				HasKey(DRBlackboardKeys::FirstAttacker) ? 1 : 0, HasKey(DRBlackboardKeys::HasFirstAttacker) ? 1 : 0,
				HasKey(DRBlackboardKeys::TargetToFollow) ? 1 : 0, HasKey(DRBlackboardKeys::DistanceToTarget) ? 1 : 0,
				HasKey(DRBlackboardKeys::UnreachableSince) ? 1 : 0,
				BTComp ? *GetNameSafe(BTComp->GetRootTree()) : TEXT("없음"),
				ActiveNode ? *ActiveNode->GetNodeName() : TEXT("없음"));
		}
	}

	if (HasKey(DRBlackboardKeys::FirstAttacker))    BB->SetValueAsObject(DRBlackboardKeys::FirstAttacker, Taunter);
	if (HasKey(DRBlackboardKeys::HasFirstAttacker)) BB->SetValueAsBool(DRBlackboardKeys::HasFirstAttacker, true);
	if (HasKey(DRBlackboardKeys::TargetToFollow))   BB->SetValueAsObject(DRBlackboardKeys::TargetToFollow, Taunter);
	if (HasKey(DRBlackboardKeys::DistanceToTarget)) BB->SetValueAsFloat(DRBlackboardKeys::DistanceToTarget, Pawn->GetDistanceTo(Taunter));

	OwnerController->SetFocus(Taunter);

	// ★전투 시간 갱신★ — BTS_CheckValidTarget 은 "타깃이 시야 거리보다 멀고 3초간 전투 없음"이면 타깃을 비운다.
	// 30m 도발은 이 조건에 쉽게 걸리므로, 도발 동안 계속 갱신해 막는다.
	if (ADRAIController* DRAIC = Cast<ADRAIController>(OwnerController))
	{
		DRAIC->UpdateCombatTime();
	}
	// ★가림 타이머 초기화★ — BTS_CheckValidTarget 은 벨보이가 벽에 가린 채(IsActorReachable = WorldStatic 라인 트레이스)
	// 서비스 2틱(≈0.5초)이 지나면 타깃을 비운다. 0 으로 되돌려 두면 서비스는 매 틱 "이제 막 가려짐"으로 보고 넘어간다.
	// 도발 판정은 시야를 안 보므로(D-19) 벽 뒤에서 걸린 적이 흔하다 — 빼면 0.5초마다 사이트 쪽으로 틀었다 돌아온다 (T-28).
	if (HasKey(DRBlackboardKeys::UnreachableSince)) BB->SetValueAsFloat(DRBlackboardKeys::UnreachableSince, 0.f);

	// 어그로 연출(ABP) — 첫 피격 어그로(DREnemyAttributeSet)와 같은 플래그
	if (ADREnemy* Enemy = Cast<ADREnemy>(Pawn))
	{
		Enemy->bIsAggroed = true;
	}
	return true;
}

void UDRAbilitySystemLibrary::StartTauntAggroRefresh(AAIController* OwnerController, float Interval)
{
	if (!OwnerController || !OwnerController->HasAuthority()) return;
	UWorld* World = OwnerController->GetWorld();
	if (!World) return;

	// 타이머 핸들을 람다가 들고 있어야 스스로 멈출 수 있다.
	// 같은 적에게 다시 도발이 걸려 타이머가 두 개 돌아도 무해하다(같은 값을 쓰고, 둘 다 GE 만료 때 멈춘다).
	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	TWeakObjectPtr<AAIController> WeakController(OwnerController);
	TWeakObjectPtr<UWorld> WeakWorld(World);
	World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([WeakController, WeakWorld, Handle]()
	{
		if (!WeakController.IsValid() || !ApplyTauntAggro(WeakController.Get()))
		{
			if (UWorld* LiveWorld = WeakWorld.Get())
			{
				LiveWorld->GetTimerManager().ClearTimer(*Handle);   // 도발 만료·도발자 사망·적 소멸
			}
		}
	}), FMath::Max(0.05f, Interval), true);
}

// ========================= ★Plan8(가방)★ 몽타주 =========================

float UDRAbilitySystemLibrary::FindMontageEventTime(const UAnimMontage* Montage, const FGameplayTag& EventTag)
{
	if (!Montage || !EventTag.IsValid()) return -1.f;

	static const FName EventTagPropertyName(TEXT("EventTag"));
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		// AN_MontageEvent 는 BP 노티파이다 — C++ 타입을 모르므로 "EventTag" 라는 FGameplayTag 변수를 반사로 찾는다
		const UAnimNotify* Notify = NotifyEvent.Notify;
		if (!Notify) continue;

		const FStructProperty* Property =
			CastField<FStructProperty>(Notify->GetClass()->FindPropertyByName(EventTagPropertyName));
		if (!Property || Property->Struct != FGameplayTag::StaticStruct()) continue;

		const FGameplayTag* Tag = Property->ContainerPtrToValuePtr<FGameplayTag>(Notify);
		if (Tag && Tag->MatchesTagExact(EventTag))
		{
			return NotifyEvent.GetTriggerTime();
		}
	}
	return -1.f;
}

// ========================= ★Plan8(FP)★ 1인칭 몽타주 =========================

namespace
{
	// 로컬 제어 + FP 메시가 보일 때만 FP AnimInstance 를 돌려준다 (D-33 / T-39)
	UAnimInstance* GetLocalFirstPersonAnimInstance(AActor* Avatar)
	{
		const ADRCharacter* DRChar = Cast<ADRCharacter>(Avatar);
		if (!DRChar || !DRChar->IsLocallyControlled()) return nullptr;

		USkeletalMeshComponent* FPMesh = DRChar->FirstPersonMesh;
		if (!FPMesh || !FPMesh->IsVisible()) return nullptr;

		return FPMesh->GetAnimInstance();
	}
}

UAnimMontage* UDRAbilitySystemLibrary::PlayFirstPersonMontage(AActor* Avatar, UAnimMontage* Montage, float PlayRate)
{
	if (!Montage) return nullptr;

	UAnimInstance* AnimInstance = GetLocalFirstPersonAnimInstance(Avatar);
	if (!AnimInstance) return nullptr;

	return AnimInstance->Montage_Play(Montage, FMath::Max(0.01f, PlayRate)) > 0.f ? Montage : nullptr;
}

void UDRAbilitySystemLibrary::SetFirstPersonMontagePlayRate(AActor* Avatar, UAnimMontage* Montage, float PlayRate)
{
	if (!Montage) return;

	UAnimInstance* AnimInstance = GetLocalFirstPersonAnimInstance(Avatar);
	if (AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_SetPlayRate(Montage, FMath::Max(0.01f, PlayRate));
	}
}

void UDRAbilitySystemLibrary::StopFirstPersonMontage(AActor* Avatar, UAnimMontage* Montage, float BlendOutTime)
{
	if (!Montage) return;

	// 숨겨진 뒤에도 멈출 수 있게 가시성은 보지 않는다 — 사망으로 숨긴 FP 메시에 스윙이 남지 않도록
	const ADRCharacter* DRChar = Cast<ADRCharacter>(Avatar);
	if (!DRChar || !DRChar->IsLocallyControlled() || !DRChar->FirstPersonMesh) return;

	UAnimInstance* AnimInstance = DRChar->FirstPersonMesh->GetAnimInstance();
	if (AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_Stop(BlendOutTime, Montage);
	}
}

float UDRAbilitySystemLibrary::GetFirstPersonSyncTime(const UAnimMontage* FPMontage, const FGameplayTag& SyncTag,
	const UAnimMontage* TPMontage, float TPEventTime)
{
	if (!FPMontage) return TPEventTime;

	const float NotifyTime = FindMontageEventTime(FPMontage, SyncTag);
	if (NotifyTime > 0.f) return NotifyTime;

	// 노티파이가 없으면 길이 비례 — FP·TP 애니는 같은 동작이라 거의 맞는다 (경고 없음, D-35)
	const float TPLength = TPMontage ? TPMontage->GetPlayLength() : 0.f;
	if (TPLength <= KINDA_SMALL_NUMBER) return TPEventTime;
	return TPEventTime * FPMontage->GetPlayLength() / TPLength;
}

// ========== 로봇 청소기 탑승 칩 (Plan5(탑승칩)) ==========

float UDRAbilitySystemLibrary::GetRideDamageMultiplier(const AActor* SourceAvatar)
{
	const ADRCharacter* Rider = Cast<ADRCharacter>(SourceAvatar);
	if (!Rider || !IsValid(Rider->MountedOn)) return 1.f;   // 빠른 탈출 — 거의 모든 피해가 여기서 끝난다

	const ADRRobotVacuumCharacter* Bottom = Rider->MountedOn->GetStackBottom();   // 맨 아래만 (D-79)
	if (!IsValid(Bottom) || Bottom->bDead) return 1.f;
	if (!HasUpgradeFeature(Bottom->GetAbilitySystemComponent(), FDRGameplayTags::Get().Upgrade_Feature_Skill_Ride_Power)) return 1.f;

	const FDRRideChipTuning& Tuning = GetChipTuning(SourceAvatar).Ride;
	const float Bonus = FMath::Min(Tuning.PowerDamagePerRider * Bottom->GetRiderCountAbove(), Tuning.PowerMaxDamageBonus);
	return 1.f + FMath::Max(0.f, Bonus);
}

ADRRobotVacuumCharacter* UDRAbilitySystemLibrary::FindRideGuard(const ADRCharacter* Rider)
{
	if (!Rider) return nullptr;
	const FGameplayTag& GuardTag = FDRGameplayTags::Get().Upgrade_Feature_Skill_Ride_Guard;

	// 아래로 내려가며 가장 가까운 칩② 청소기 ("위 전원" 보호 — D-83). 상한 8 은 순환 방어용(CanBeMountedBy 가 이미 막는다)
	int32 Steps = 0;
	for (ADRRobotVacuumCharacter* Below = Rider->MountedOn; IsValid(Below) && Steps++ < 8; Below = Below->MountedOn)
	{
		if (!Below->bDead && HasUpgradeFeature(Below->GetAbilitySystemComponent(), GuardTag))   // 죽은 청소기 건너뛰기 (T-89)
		{
			return Below;
		}
	}
	return nullptr;
}

bool UDRAbilitySystemLibrary::TransferRideDamageToGuard(ADRCharacter* Rider, ADRRobotVacuumCharacter* Guard, float Damage)
{
	if (!IsValid(Rider) || !IsValid(Guard) || Guard->bDead) return false;

	// 80% 감소는 ★이전분에만★ — 피격 자체의 1칸 공유는 호출부가 PropagateSharedDamage(L) 로 그대로 보낸다 (D-85 · T-93)
	const float Ratio = FMath::Clamp(GetChipTuning(Rider).Ride.GuardTransferRatio, 0.f, 1.f);
	return Rider->ApplyMountSharedDamageTo(Guard, Damage * Ratio);
}

void UDRAbilitySystemLibrary::TryRideEmergencyEject(ADRCharacter* Rider, bool bEnteredCorruptionThisHit)
{
	if (!IsValid(Rider) || !Rider->HasAuthority() || Rider->bDead) return;   // 사망 하차가 이미 일어났다

	ADRRobotVacuumCharacter* Mount = Rider->MountedOn;   // ★바로 위 1명만★ (D-88)
	if (!IsValid(Mount) || Mount->bDead) return;
	if (!HasUpgradeFeature(Mount->GetAbilitySystemComponent(), FDRGameplayTags::Get().Upgrade_Feature_Skill_Ride_Eject)) return;

	const UDRPlayerAttributeSet* RiderAS = GetPlayerAttributeSetFromASC(Rider->GetAbilitySystemComponent());
	if (!RiderAS) return;

	// 오염 진입은 체력이 오염 최대치로 다시 차서 비율로는 못 잡는다 → 따로 받는다 (T-79)
	const float Ratio = GetChipTuning(Rider).Ride.EjectHealthRatio;
	const bool bLow = bEnteredCorruptionThisHit
		|| (RiderAS->GetMaxHealth() > 0.f && RiderAS->GetHealth() <= RiderAS->GetMaxHealth() * Ratio);
	if (bLow)
	{
		Mount->EmergencyEjectRider();
	}
}

bool UDRAbilitySystemLibrary::IsRideBannedFrom(const AActor* Rider, const ADRRobotVacuumCharacter* Mount)
{
	const UAbilitySystemComponent* RiderASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Rider);
	const UAbilitySystemComponent* MountASC = Mount ? Mount->GetAbilitySystemComponent() : nullptr;
	if (!RiderASC || !MountASC) return false;

	const FGameplayTag& BanTag = FDRGameplayTags::Get().Debuff_Ride_Banned;
	if (!RiderASC->HasMatchingGameplayTag(BanTag)) return false;   // 빠른 탈출

	// ★OwningTags 쿼리★ — 금지 태그는 GE 의 부여 태그다 (T-82, Plan8 T-29 와 같은 이유)
	for (const FActiveGameplayEffectHandle& Handle :
		RiderASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(BanTag))))
	{
		const FActiveGameplayEffect* Active = RiderASC->GetActiveGameplayEffect(Handle);
		// 시전자 ASC(복제됨)로 "어느 청소기"인지 가린다 (T-83)
		if (Active && Active->Spec.GetContext().GetOriginalInstigatorAbilitySystemComponent() == MountASC)
		{
			return true;
		}
	}
	return false;
}

// ========== 청소기 기본 공격 칩 (Plan5(공기탄칩)) ==========

float UDRAbilitySystemLibrary::GetAirShotDragDamageTakenMultiplier(const UAbilitySystemComponent* TargetASC)
{
	const FGameplayTag& DragTag = FDRGameplayTags::Get().Debuff_AirShot_Drag;
	if (!TargetASC || !TargetASC->HasMatchingGameplayTag(DragTag)) return 1.f;   // 빠른 탈출

	const FDRAirShotChipTuning& Tuning = GetChipTuning(TargetASC->GetAvatarActor()).AirShot;
	// ★스택 수는 태그 카운트가 아니다★ — Aggregate by Target 이라 인스턴스 하나의 스택 (T-98)
	const int32 Stacks = FMath::Min(GetEffectStackCountWithTag(TargetASC, DragTag), FMath::Max(1, Tuning.DragMaxStacks));
	return 1.f + FMath::Max(0.f, Tuning.DragDamageTakenPerStack) * Stacks;
}

void UDRAbilitySystemLibrary::TryApplyAirShotDrag(const FEffectProperties& Props)
{
	const FDRGameplayTags& Tags = FDRGameplayTags::Get();
	if (!HasUpgradeFeature(Props.SourceASC, Tags.Upgrade_Feature_Skill_AirShot_Drag)) return;

	// 공기탄 피해만 — 같은 청소기의 돌진 충돌 피해로는 쌓이지 않는다 (T-99)
	const FDRGameplayEffectContext* DRContext = static_cast<const FDRGameplayEffectContext*>(Props.EffectContextHandle.Get());
	if (!DRContext || !DRContext->GetSourceAbilityTags().HasTagExact(Tags.Abilities_RobotVacuum_AirShot)) return;

	ADREnemy* Enemy = Cast<ADREnemy>(Props.TargetAvatarActor);
	if (!IsValid(Enemy) || Enemy->IsBoss()) return;   // 보스는 감속 · 피해 증가 둘 다 없다 (T-100)

	const FDRAirShotChipTuning& Tuning = GetChipTuning(Enemy).AirShot;
	ApplyTimedSpeedEffect(Enemy, Props.SourceASC, Tuning.Drag, 1.f - Tuning.Drag.Magnitude);   // 0.9 — 스택은 GE 가 쌓는다
}

// ========== 무적 (Plan5(물점프칩)) ==========

bool UDRAbilitySystemLibrary::IsInvulnerable(const UAbilitySystemComponent* ASC)
{
	return ASC && ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Invulnerable);
}

bool UDRAbilitySystemLibrary::LineTraceWorldGeometry(const UWorld* World, FHitResult& OutHit, const FVector& Start,
	const FVector& End, const FCollisionQueryParams& Params)
{
	return World && World->LineTraceSingleByObjectType(OutHit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}
