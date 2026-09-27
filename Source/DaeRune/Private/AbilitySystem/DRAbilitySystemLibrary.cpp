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

	const AActor* TuningContext = Props.SourceASC ? Props.SourceASC->GetAvatarActor() : nullptr;
	return FMath::Max(0.f, GetChipTuning(TuningContext).Claw.LifestealRatio);
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
		// [임시 진단]
		UE_LOG(LogDR, Warning, TEXT("[MoleDiag] A. 반경 %.0f 오버랩 %d개 (중심 %s)"),
			Radius, Overlaps.Num(), *SphereOrigin.ToCompactString());

		for (FOverlapResult& Overlap : Overlaps)
		{
			// [임시 진단] 두더지만 골라 출력
			if (Overlap.GetActor() && Overlap.GetActor()->GetClass()->GetName().Contains(TEXT("Mole")))
			{
				UE_LOG(LogDR, Warning, TEXT("[MoleDiag] B. 두더지 발견: %s / Implements=%d / IsDead=%d"),
					*Overlap.GetActor()->GetName(),
					Overlap.GetActor()->Implements<UCombatInterface>() ? 1 : 0,
					Overlap.GetActor()->Implements<UCombatInterface>() ? (ICombatInterface::Execute_IsDead(Overlap.GetActor()) ? 1 : 0) : -1);
			}

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

	// [임시 진단] 이 함수에 도달한 모든 공격
	UE_LOG(LogDR, Warning, TEXT("[MoleDiag] F. ApplyDamageEffect 진입: 대상아바타=%s"),
		DamageEffectParams.TargetAbilitySystemComponent
			? *GetNameSafe(DamageEffectParams.TargetAbilitySystemComponent->GetAvatarActor())
			: TEXT("★TargetASC 없음"));

	// ===== 근접 전용 피격 대상 가로채기 (Plan6 §5.11) =====
	// 홀로그램 두더지처럼 "2m 안에서의 공격만 유효하고 그 밖은 투과"하는 대상을 여기서 처리한다.
	// 모든 공격이 이 함수를 지나므로 공격 종류마다 손대지 않아도 된다.
	// GE 는 적용하지 않는다 - 어트리뷰트를 쓰지 않는 대상이라 PostGameplayEffectExecute 를 태우면 위험하다.
	if (DamageEffectParams.TargetAbilitySystemComponent)
	{
		AActor* TargetAvatarActor = DamageEffectParams.TargetAbilitySystemComponent->GetAvatarActor();
		if (IDRProximityHitOnly* ProximityTarget = Cast<IDRProximityHitOnly>(TargetAvatarActor))
		{
			// [임시 진단]
			UE_LOG(LogDR, Warning, TEXT("[MoleDiag] D. ApplyDamageEffect 도달: 대상=%s / Accepts=%d"),
				*GetNameSafe(TargetAvatarActor), ProximityTarget->AcceptsHitFrom(SourceAvatarActor) ? 1 : 0);

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
