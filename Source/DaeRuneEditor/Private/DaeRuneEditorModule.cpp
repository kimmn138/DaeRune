// Copyright DaeRune

#include "Modules/ModuleManager.h"
#include "UObject/ICookInfo.h"
#include "Tuning/DRTuningSubsystem.h"
#include "DaeRune/DRLogChannels.h"

/**
 * 에디터 전용 도구 모듈. 도구는 대부분 UBlueprintFunctionLibrary 라 시작/종료 처리가 없고,
 * 여기서는 쿡 훅 하나만 건다 — 패키징할 때마다 튜닝 목록(Content/Tuning/TuningCatalog.csv)을 새로 만들어
 * 빌드 안의 [현재값] · [설명] 이 언제나 그 빌드의 값과 같게 한다 (UDRTuningSubsystem).
 */
class FDaeRuneEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// 쿡이 ★끝날 때★ 만든다 — 시작할 때 만들면 빌드에 안 들어가는 고장 난 BP(GA_TestAbility)까지 불러 그 컴파일 오류로 쿡이 실패한다.
		// 스테이징은 쿡 프로세스가 끝난 뒤라 이때 써도 pak 에 들어간다
		CookFinishedHandle = UE::Cook::FDelegates::CookFinished.AddStatic(&FDaeRuneEditorModule::HandleCookFinished);
	}

	virtual void ShutdownModule() override
	{
		UE::Cook::FDelegates::CookFinished.Remove(CookFinishedHandle);
	}

private:
	static void HandleCookFinished(UE::Cook::ICookInfo& CookInfo)
	{
		if (CookInfo.GetCookType() != UE::Cook::ECookType::ByTheBook) return;
		// 여러 프로세스로 쿡하면 감독 프로세스에서만 (일꾼들이 같은 파일을 동시에 쓰지 않게)
		if (CookInfo.GetProcessType() == UE::Cook::EProcessType::Worker) return;

		// 이번 쿡에 들어간 패키지만 목록에 싣는다
		auto IsCooked = [&CookInfo](FName PackageName)
		{
			const UE::Cook::EInstigator Category = CookInfo.GetInstigator(PackageName).Category;
			return Category != UE::Cook::EInstigator::InvalidCategory
				&& Category != UE::Cook::EInstigator::NotYetRequested
				&& Category != UE::Cook::EInstigator::EditorOnlyLoad
				&& Category != UE::Cook::EInstigator::HardEditorOnlyDependency;
		};

		FString Path, Error;
		int32 RowCount = 0;
		if (UDRTuningSubsystem::WriteCatalogTemplate(Path, Error, RowCount, IsCooked))
		{
			UE_LOG(LogDR, Display, TEXT("[Tuning] 쿡용 튜닝 목록 %d 줄을 만들었습니다: %s"), RowCount, *Path);
		}
		else
		{
			UE_LOG(LogDR, Warning, TEXT("[Tuning] 쿡용 튜닝 목록을 만들지 못했습니다 — 빌드의 튜닝.csv 에 설명이 빠집니다: %s"), *Error);
		}
	}

	FDelegateHandle CookFinishedHandle;
};

IMPLEMENT_MODULE(FDaeRuneEditorModule, DaeRuneEditor);
