// Copyright DaeRune


#include "Actor/DRCosmeticPreviewStage.h"
#include "Character/DRCharacter.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Game/DRCosmeticCatalog.h"
#include "Game/DRGameInstance.h"
#include "AnimationRuntime.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInterface.h"
#include "DaeRune/DRLogChannels.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif

ADRCosmeticPreviewStage::ADRCosmeticPreviewStage()
{
	PrimaryActorTick.bCanEverTick = false;

	// 프리뷰는 각 머신이 자기 것만 본다. 복제하면 인원수만큼 스튜디오가 생긴다.
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PreviewSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewSpawnPoint"));
	PreviewSpawnPoint->SetupAttachment(RootComponent);

	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureComponent"));
	CaptureComponent->SetupAttachment(RootComponent);

	// 화면이 열려 있을 때만 켠다 (SetCaptureActive)
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;

	// 지정한 액터만 찍는다 — 월드의 다른 물체가 배경에 섞이지 않게 한다
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

	/**
	 * ★배경을 투명하게 남기기 위한 설정★
	 *
	 * SCS_SceneColorHDR 은 알파에 ★역불투명도(Inv Opacity)★ 를 담는다.
	 * 즉 아무 것도 없는 픽셀은 A=1, 캐릭터가 있는 픽셀은 A=0 이다.
	 * → UI 머티리얼에서 Opacity = 1 - A 로 쓰면 배경이 뚫려
	 *   뒤에 깔린 캐릭터 글로우(Img_CharBack)가 비쳐 보인다. (Plan(옷장) D-11)
	 *
	 * 대안은 프로젝트 설정에서 알파 채널 전파를 켜고 SCS_FinalColorLDR 을 쓰는 것인데,
	 * 렌더링 전역 설정을 바꾸는 비용이 커서 택하지 않았다.
	 * 단점: 톤매퍼를 거치지 않아 색이 인게임과 다를 수 있다 —
	 *       스튜디오 조명을 눈으로 맞추는 것으로 흡수한다.
	 */
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;

	// 옷장 칸 아이콘 촬영 (Plan(옷장) D-33) — 옷을 입은 자리에 메시만 따로 세워 같은 조명으로 찍는다
	IconMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IconMesh"));
	IconMeshComponent->SetupAttachment(RootComponent);
	IconMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IconMeshComponent->SetCastShadow(false);
	IconMeshComponent->bVisibleInSceneCaptureOnly = true;	// 일반 화면에는 안 그린다
	IconMeshComponent->SetCanEverAffectNavigation(false);

	IconCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("IconCapture"));
	IconCaptureComponent->SetupAttachment(RootComponent);
	IconCaptureComponent->bCaptureEveryFrame = false;
	IconCaptureComponent->bCaptureOnMovement = false;	// 찍을 때마다 옮긴다 — 옮길 때 찍히면 안 된다
	IconCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	IconCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;	// 배경 투명 — 캐릭터 프리뷰와 같은 방식 (D-11)

	// 꼬리는 옆에서 봐야 휘어진 모양이 산다 (뒤에서 보면 한 줄로 겹친다)
	IconViewOffsetByCategory.Add(EDRCosmeticCategory::Tail, FRotator(-15.0f, -70.0f, 0.0f));
}

ADRCosmeticPreviewStage* ADRCosmeticPreviewStage::Find(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;

	AActor* Found = UGameplayStatics::GetActorOfClass(WorldContext, ADRCosmeticPreviewStage::StaticClass());
	return Cast<ADRCosmeticPreviewStage>(Found);
}

void ADRCosmeticPreviewStage::SetPreviewClass(EPlayerCharacterClass CharacterClass)
{
	// 데디케이티드 서버에는 볼 사람이 없다
	if (GetNetMode() == NM_DedicatedServer) return;

	if (PreviewCharacter && CurrentPreviewClass == CharacterClass) return;

	DestroyPreviewCharacter();

	UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this);
	if (!ClassInfo) return;

	TSubclassOf<ADRCharacter>* BPClassPtr = ClassInfo->CharacterBPClasses.Find(CharacterClass);
	if (!BPClassPtr || !*BPClassPtr)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cosmetic] 클래스 %d 의 캐릭터 BP 가 없어 프리뷰를 세우지 못했습니다."),
			static_cast<int32>(CharacterClass));
		return;
	}

	CurrentYaw = BaseYaw;

	const FTransform SpawnTransform(
		FRotator(0.0f, CurrentYaw, 0.0f),
		PreviewSpawnPoint->GetComponentLocation());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;	// 세이브/레벨에 남지 않는다

	PreviewCharacter = GetWorld()->SpawnActor<ADRCharacter>(*BPClassPtr, SpawnTransform, Params);
	if (!PreviewCharacter)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cosmetic] 프리뷰 캐릭터 스폰에 실패했습니다."));
		return;
	}

	// ★로컬 전용★ — 켜두면 모든 클라가 서로의 프리뷰를 스폰하게 된다
	PreviewCharacter->SetReplicates(false);
	PreviewCharacter->SetReplicateMovement(false);

	if (UCharacterMovementComponent* Movement =
		Cast<UCharacterMovementComponent>(PreviewCharacter->GetMovementComponent()))
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// 3인칭 메시를 보여주고 1인칭 메시/오버헤드 닉네임을 숨긴다.
	// 대기실 디스플레이 캐릭터와 같은 처리라 함수를 그대로 재사용한다. (DRCharacter.cpp)
	PreviewCharacter->SetWaitingRoomVisibility(true);

	CurrentPreviewClass = CharacterClass;

	// 로봇별 구도 (Plan(옷장) D-28)
	if (!DefaultCaptureLocation.IsSet())
	{
		DefaultCaptureLocation = CaptureComponent->GetRelativeLocation();
	}
	const FVector* ClassCaptureLocation = CaptureLocationByClass.Find(CharacterClass);
	CaptureComponent->SetRelativeLocation(ClassCaptureLocation ? *ClassCaptureLocation : DefaultCaptureLocation.GetValue());

	RefreshShowOnlyList();
}

void ADRCosmeticPreviewStage::SetPreviewSkins(const TArray<FName>& SkinIds)
{
	if (!PreviewCharacter) return;

	// 복제를 거치지 않는 순수 로컬 경로 — 클릭한 프레임에 바로 반영된다
	// (메시 비동기 로드가 끝나는 한두 프레임 뒤에 실제로 붙는다)
	PreviewCharacter->ApplyCosmeticSkins(SkinIds);
}

void ADRCosmeticPreviewStage::AddPreviewYaw(float DeltaYaw)
{
	if (!PreviewCharacter) return;

	CurrentYaw = FRotator::NormalizeAxis(CurrentYaw + DeltaYaw);
	PreviewCharacter->SetActorRotation(FRotator(0.0f, CurrentYaw, 0.0f));
}

void ADRCosmeticPreviewStage::ResetPreviewYaw()
{
	CurrentYaw = BaseYaw;

	if (PreviewCharacter)
	{
		PreviewCharacter->SetActorRotation(FRotator(0.0f, CurrentYaw, 0.0f));
	}
}

void ADRCosmeticPreviewStage::SetCaptureActive(bool bActive)
{
	if (!CaptureComponent) return;

	if (bActive && !RenderTarget)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[Cosmetic] 프리뷰 스테이지에 RenderTarget 이 지정되지 않았습니다 — 프리뷰가 비어 보입니다."));
	}

	CaptureComponent->TextureTarget = bActive ? RenderTarget : nullptr;
	CaptureComponent->bCaptureEveryFrame = bActive;

	if (bActive)
	{
		// 첫 프레임을 즉시 찍어 화면이 열리는 순간 빈 사각형이 보이지 않게 한다
		CaptureComponent->CaptureScene();
	}
}

void ADRCosmeticPreviewStage::RefreshShowOnlyList()
{
	if (!CaptureComponent) return;

	CaptureComponent->ShowOnlyActors.Reset();

	for (const TObjectPtr<AActor>& Backdrop : BackdropActors)
	{
		if (Backdrop)
		{
			CaptureComponent->ShowOnlyActors.Add(Backdrop);
		}
	}

	if (PreviewCharacter)
	{
		CaptureComponent->ShowOnlyActors.Add(PreviewCharacter);
	}
}

void ADRCosmeticPreviewStage::DestroyPreviewCharacter()
{
	if (PreviewCharacter)
	{
		PreviewCharacter->Destroy();
		PreviewCharacter = nullptr;
	}

	CurrentPreviewClass = EPlayerCharacterClass::Count;

	RefreshShowOnlyList();
}

void ADRCosmeticPreviewStage::PrepareSkinIcons(EPlayerCharacterClass CharacterClass, FSimpleDelegate OnReady)
{
	// 데디케이티드 서버에는 볼 사람이 없다
	if (GetNetMode() == NM_DedicatedServer) return;

	if (IconClass != CharacterClass)
	{
		SkinIcons.Reset();
		IconClass = CharacterClass;
	}

	const UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	const UDRCosmeticCatalog* Catalog = GI ? GI->GetCosmeticCatalog() : nullptr;
	if (!Catalog) return;

	TArray<FName> Pending;
	TArray<FSoftObjectPath> ToLoad;
	for (const FDRSkinDefinition& Skin : Catalog->Skins)
	{
		if (Skin.SkinId.IsNone() || !Skin.IsUsableBy(CharacterClass)) continue;
		if (!Skin.PreviewIcon.IsNull() || SkinIcons.Contains(Skin.SkinId)) continue;

		const FDRSkinAttachSpec Spec = Skin.GetThirdPersonFor(CharacterClass);
		if (Spec.StaticMesh.IsNull()) continue;

		Pending.Add(Skin.SkinId);
		ToLoad.Add(Spec.StaticMesh.ToSoftObjectPath());
		for (const FDRSkinMaterialOverride& Override : Spec.MaterialOverrides)
		{
			if (!Override.Material.IsNull())
			{
				ToLoad.Add(Override.Material.ToSoftObjectPath());
			}
		}
	}

	if (Pending.Num() == 0) return;

	// 앞선 요청은 버린다 — 그 옷들도 아직 SkinIcons 에 없으니 이번 Pending 에 다시 들어 있다
	const uint32 Generation = ++IconRequestGeneration;
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	TWeakObjectPtr<ADRCosmeticPreviewStage> WeakThis(this);
	IconLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		MoveTemp(ToLoad),
		FStreamableDelegate::CreateLambda([WeakThis, Generation, CharacterClass, Pending, OnReady]()
		{
			ADRCosmeticPreviewStage* Self = WeakThis.Get();
			if (!Self || Self->IconRequestGeneration != Generation || Self->IconClass != CharacterClass) return;

			if (Self->RenderSkinIcons(Pending))
			{
				OnReady.ExecuteIfBound();
			}
		}));
}

UTextureRenderTarget2D* ADRCosmeticPreviewStage::FindSkinIcon(FName SkinId, EPlayerCharacterClass CharacterClass) const
{
	if (CharacterClass != IconClass) return nullptr;

	const TObjectPtr<UTextureRenderTarget2D>* Found = SkinIcons.Find(SkinId);
	return Found ? Found->Get() : nullptr;
}

bool ADRCosmeticPreviewStage::RenderSkinIcons(const TArray<FName>& SkinIds)
{
	const UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	const UDRCosmeticCatalog* Catalog = GI ? GI->GetCosmeticCatalog() : nullptr;
	if (!Catalog || !IconMeshComponent || !IconCaptureComponent || !CaptureComponent) return false;

	// 캐릭터 프리뷰와 같은 표시 설정으로 찍는다 — BP 에서 CaptureComponent 를 손봐도 아이콘이 따라간다
	IconCaptureComponent->ShowFlags = CaptureComponent->ShowFlags;
	IconCaptureComponent->PostProcessSettings = CaptureComponent->PostProcessSettings;
	IconCaptureComponent->PostProcessBlendWeight = CaptureComponent->PostProcessBlendWeight;
	IconCaptureComponent->FOVAngle = IconFOV;
	IconCaptureComponent->ClearShowOnlyComponents();
	IconCaptureComponent->ShowOnlyComponents.Add(IconMeshComponent);

#if WITH_EDITOR
	// ★에디터는 막 로드한 메시를 비동기로 빌드한다★ — 빌드 중이면 렌더 프록시가 없어 빈 그림이 찍힌다
	// (프리뷰 캐릭터가 이미 입고 있던 옷만 그림이 나왔다). 패키지 빌드에는 이 단계가 없다
	TArray<UStaticMesh*> MeshesToFinish;
	for (const FName& SkinId : SkinIds)
	{
		if (const FDRSkinDefinition* Skin = Catalog->FindSkin(SkinId))
		{
			if (UStaticMesh* Mesh = Skin->GetThirdPersonFor(IconClass).StaticMesh.Get())
			{
				MeshesToFinish.Add(Mesh);
			}
		}
	}
	FStaticMeshCompilingManager::Get().FinishCompilation(MeshesToFinish);
#endif

	bool bRenderedAny = false;
	for (const FName& SkinId : SkinIds)
	{
		const FDRSkinDefinition* Skin = Catalog->FindSkin(SkinId);
		if (!Skin) continue;

		if (UTextureRenderTarget2D* Icon = RenderSkinIcon(*Skin))
		{
			SkinIcons.Add(SkinId, Icon);
			bRenderedAny = true;
		}
	}

	// CaptureScene 은 그 순간의 장면을 렌더 큐에 넣는다 — 바로 비워도 찍힌 그림은 그대로다
	IconMeshComponent->SetStaticMesh(nullptr);
	IconMeshComponent->EmptyOverrideMaterials();
	IconCaptureComponent->TextureTarget = nullptr;

	return bRenderedAny;
}

UTextureRenderTarget2D* ADRCosmeticPreviewStage::RenderSkinIcon(const FDRSkinDefinition& Skin)
{
	const FDRSkinAttachSpec Spec = Skin.GetThirdPersonFor(IconClass);
	UStaticMesh* Mesh = Spec.StaticMesh.Get();
	if (!Mesh) return nullptr;

	IconMeshComponent->EmptyOverrideMaterials();
	IconMeshComponent->SetStaticMesh(Mesh);
	for (const FDRSkinMaterialOverride& Override : Spec.MaterialOverrides)
	{
		if (UMaterialInterface* Material = Override.Material.Get())
		{
			IconMeshComponent->SetMaterial(Override.MaterialSlot, Material);
		}
	}

	// 입은 자리 그대로 둔다 — 프리뷰 캐릭터와 같은 스튜디오 조명을 받는다
	IconMeshComponent->SetWorldTransform(ComputeWornTransform(IconClass, Spec));

	const FBox LocalBox = Mesh->GetBoundingBox();
	const FTransform& MeshToWorld = IconMeshComponent->GetComponentTransform();
	const FVector Center = MeshToWorld.TransformPosition(LocalBox.GetCenter());

	// 시점: 옷이 있는 쪽에서 본다 — 앞 = 프리뷰 카메라의 수평 방향(캐릭터 정면을 보는 방향), 뒤 = 그 반대.
	// 거기서 부위별 오프셋만큼 돈다
	const bool bBehind = GetRestingActorTransform().InverseTransformPosition(Center).X < -IconBehindDepth;
	const FRotator* CategoryOffset = IconViewOffsetByCategory.Find(Skin.Category);
	const FRotator ViewOffset = CategoryOffset ? *CategoryOffset : DefaultIconViewOffset;
	const double SideYaw = CaptureComponent->GetComponentRotation().Yaw + (bBehind ? 180.0 : 0.0);
	const FRotator ViewRotation(ViewOffset.Pitch, SideYaw + ViewOffset.Yaw, 0.0f);
	const FRotationMatrix ViewAxes(ViewRotation);
	const FVector Forward = ViewAxes.GetUnitAxis(EAxis::X);
	const FVector Right = ViewAxes.GetUnitAxis(EAxis::Y);
	const FVector Up = ViewAxes.GetUnitAxis(EAxis::Z);

	// 메시 상자의 꼭짓점 8개를 화면 평면에 대 보고 가장 긴 쪽에 맞춘다 (월드 AABB 보다 꼭 맞는다)

	double HalfSize = 0.0;
	double NearDepth = 0.0;	// 중심보다 카메라 쪽으로 나온 깊이 — 가까운 면이 원근으로 커지는 몫
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Local(
			(Corner & 1) ? LocalBox.Max.X : LocalBox.Min.X,
			(Corner & 2) ? LocalBox.Max.Y : LocalBox.Min.Y,
			(Corner & 4) ? LocalBox.Max.Z : LocalBox.Min.Z);
		const FVector FromCenter = MeshToWorld.TransformPosition(Local) - Center;

		HalfSize = FMath::Max3(HalfSize, FMath::Abs(FromCenter | Right), FMath::Abs(FromCenter | Up));
		NearDepth = FMath::Max(NearDepth, -(FromCenter | Forward));
	}
	if (HalfSize <= UE_KINDA_SMALL_NUMBER) return nullptr;

	const double TanHalfFOV = FMath::Tan(FMath::DegreesToRadians(IconFOV * 0.5));
	const double Distance = HalfSize / (IconFill * TanHalfFOV) + NearDepth;
	IconCaptureComponent->SetWorldLocationAndRotation(Center - Forward * Distance, ViewRotation);

	// RGBA16f — 알파에 역불투명도가 담긴다(캐릭터 프리뷰와 같은 형식, D-11).
	// 칸에서는 줄여 그리므로 밉맵을 만든다 — 없으면 가장자리가 깨진다
	UTextureRenderTarget2D* Target = UKismetRenderingLibrary::CreateRenderTarget2D(
		this, IconResolution, IconResolution, RTF_RGBA16f, FLinearColor::Black, true);
	if (!Target) return nullptr;

	IconCaptureComponent->TextureTarget = Target;
	IconCaptureComponent->CaptureScene();

	return Target;
}

FTransform ADRCosmeticPreviewStage::GetRestingActorTransform() const
{
	// SetPreviewClass 가 캐릭터를 세우는 변환과 같다
	return FTransform(FRotator(0.0f, BaseYaw, 0.0f), PreviewSpawnPoint->GetComponentLocation());
}

FTransform ADRCosmeticPreviewStage::ComputeWornTransform(EPlayerCharacterClass CharacterClass,
	const FDRSkinAttachSpec& Spec) const
{
	const FTransform ActorToWorld = GetRestingActorTransform();

	FTransform MeshToActor = FTransform::Identity;
	FTransform SocketToMesh = FTransform::Identity;

	const UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this);
	const TSubclassOf<ADRCharacter>* BPClassPtr = ClassInfo ? ClassInfo->CharacterBPClasses.Find(CharacterClass) : nullptr;
	const ADRCharacter* CDO = (BPClassPtr && *BPClassPtr) ? BPClassPtr->Get()->GetDefaultObject<ADRCharacter>() : nullptr;

	if (const USkeletalMeshComponent* MeshComp = CDO ? CDO->GetMesh() : nullptr)
	{
		MeshToActor = MeshComp->GetRelativeTransform();

		// 소켓을 못 찾으면 인게임처럼 메시 원점에 붙는다 (SpawnCosmeticAttachment 의 경고와 같은 경우)
		const USkeletalMesh* SkeletalMesh = MeshComp->GetSkeletalMeshAsset();
		const USkeletalMeshSocket* Socket = (SkeletalMesh && !Spec.SocketName.IsNone())
			? SkeletalMesh->FindSocket(Spec.SocketName) : nullptr;
		if (Socket)
		{
			const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
			const int32 BoneIndex = RefSkeleton.FindBoneIndex(Socket->BoneName);
			if (BoneIndex != INDEX_NONE)
			{
				SocketToMesh = Socket->GetSocketLocalTransform()
					* FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex);
			}
		}
	}

	return Spec.RelativeTransform * SocketToMesh * MeshToActor * ActorToWorld;
}

void ADRCosmeticPreviewStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetCaptureActive(false);
	DestroyPreviewCharacter();

	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}
