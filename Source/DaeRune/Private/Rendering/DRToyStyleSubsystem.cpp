// Copyright DaeRune

#include "Rendering/DRToyStyleSubsystem.h"
#include "DaeRune/DRLogChannels.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "SceneView.h"
#include "SceneViewExtension.h"

namespace
{
	TAutoConsoleVariable<int32> CVarToyStyle(
		TEXT("dr.ToyStyle"),
		1,
		TEXT("장난감 화풍 포스트 프로세스. 0 = 끔(원래 화면), 1 = 켬. 수치는 MI_PP_ToyStyle 에서 고친다"),
		ECVF_Default);
}

/** 게임 · 레벨 에디터 뷰에 장난감 화풍 머티리얼을 붙인다 */
class FDRToyStyleViewExtension : public FSceneViewExtensionBase
{
public:
	FDRToyStyleViewExtension(const FAutoRegister& AutoRegister, UDRToyStyleSubsystem* InOwner)
		: FSceneViewExtensionBase(AutoRegister)
		, Owner(InOwner)
	{
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
	{
		// 씬 캡처(옷장 아이콘 · 미리보기 · 반사)는 원래대로. 뷰 상태가 없으면 엔진이 블렌더블을 받지 않는다
		if (InView.bIsSceneCapture || InView.bIsReflectionCapture || InView.bIsPlanarReflection || !InView.State)
		{
			return;
		}
		// 언릿 · 와이어프레임 같은 에디터 보기 모드
		if (!InViewFamily.EngineShowFlags.PostProcessing || !InViewFamily.EngineShowFlags.PostProcessMaterial)
		{
			return;
		}

		UDRToyStyleSubsystem* Subsystem = Owner.Get();
		if (!Subsystem)
		{
			return;
		}
		// 맵 PPV 의 블렌더블과 같은 경로 — 뷰마다 MID 를 재사용한다. EndFinalPostprocessSettings 뒤라 그대로 렌더러로 간다
		for (UMaterialInterface* ToyMaterial : Subsystem->GetMaterials())
		{
			ToyMaterial->OverrideBlendableSettings(InView, 1.f);
		}
	}

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		if (CVarToyStyle.GetValueOnAnyThread() == 0)
		{
			return false;
		}

		const UDRToyStyleSubsystem* Subsystem = Owner.Get();
		const UWorld* World = Context.GetWorld();
		if (!Subsystem || !World)
		{
			return false;
		}

		switch (World->WorldType)
		{
		case EWorldType::Game:
		case EWorldType::PIE:
			return true;
		case EWorldType::Editor:
			return Subsystem->ShouldApplyInEditorViewports();
		default:
			return false;	// 에셋 에디터 미리보기 · 비활성 월드
		}
	}

private:
	TWeakObjectPtr<UDRToyStyleSubsystem> Owner;
};

bool UDRToyStyleSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!FApp::CanEverRender() || IsRunningCommandlet())
	{
		return false;
	}
	return Super::ShouldCreateSubsystem(Outer) && bEnabled;
}

void UDRToyStyleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ViewExtension = FSceneViewExtensions::NewExtension<FDRToyStyleViewExtension>(this);
	UE_LOG(LogDR, Log, TEXT("[ToyStyle] 장난감 화풍 켜짐 — %s (dr.ToyStyle 0 으로 끔)"), *Material.ToString());
}

void UDRToyStyleSubsystem::Deinitialize()
{
	ViewExtension.Reset();
	LoadedMaterials.Reset();

	Super::Deinitialize();
}

const TArray<TObjectPtr<UMaterialInterface>>& UDRToyStyleSubsystem::GetMaterials()
{
	// 엔진 서브시스템은 콘텐츠보다 먼저 만들어진다 — 처음 그릴 때 읽는다
	if (!bTriedLoad)
	{
		bTriedLoad = true;

		TArray<TSoftObjectPtr<UMaterialInterface>> Paths = LevelPassMaterials;
		Paths.Add(Material);
		for (const TSoftObjectPtr<UMaterialInterface>& Path : Paths)
		{
			UMaterialInterface* Loaded = Path.LoadSynchronous();
			if (!Loaded)
			{
				// 축소 패스가 빠지면 본 패스가 빈 텍스처를 밝기 기준으로 읽어 화면이 하얗게 된다 — 통째로 끈다
				UE_LOG(LogDR, Warning, TEXT("[ToyStyle] 머티리얼 '%s' 를 읽지 못해 장난감 화풍을 씌우지 않습니다."), *Path.ToString());
				LoadedMaterials.Reset();
				break;
			}
			LoadedMaterials.Add(Loaded);
		}
	}
	return LoadedMaterials;
}
