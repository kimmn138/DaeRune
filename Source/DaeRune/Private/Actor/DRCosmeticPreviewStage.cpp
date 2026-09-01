// Copyright DaeRune


#include "Actor/DRCosmeticPreviewStage.h"
#include "Character/DRCharacter.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DaeRune/DRLogChannels.h"

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
	 *   뒤에 깔린 캐릭터 글로우(Img_CharBack)가 비쳐 보인다. (Plan.md 15.3 / 15.4)
	 *
	 * 대안은 프로젝트 설정에서 알파 채널 전파를 켜고 SCS_FinalColorLDR 을 쓰는 것인데,
	 * 렌더링 전역 설정을 바꾸는 비용이 커서 택하지 않았다.
	 * 단점: 톤매퍼를 거치지 않아 색이 인게임과 다를 수 있다 —
	 *       스튜디오 조명을 눈으로 맞추는 것으로 흡수한다.
	 */
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
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

void ADRCosmeticPreviewStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetCaptureActive(false);
	DestroyPreviewCharacter();

	Super::EndPlay(EndPlayReason);
}
