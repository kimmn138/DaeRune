// Copyright DaeRune

#include "DRTuningEditorTools.h"
#include "Tuning/DRTuningSubsystem.h"
#include "Tuning/DRTuningSheet.h"
#include "Tuning/DRTuningProperty.h"
#include "DaeRune/DRLogChannels.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "ScopedTransaction.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/UObjectHash.h"

#define LOCTEXT_NAMESPACE "DRTuningEditorTools"

namespace
{
	FString ShortTargetName(const FString& Target)
	{
		if (!Target.StartsWith(TEXT("/"))) return Target;
		FString Name = FPackageName::ObjectPathToObjectName(Target);
		if (Name.IsEmpty())
		{
			Name = FPackageName::GetShortName(Target);
		}
		Name.RemoveFromEnd(TEXT("_C"));
		return Name;
	}

	bool FindAssetPathByShortName(const FString& Name, FString& OutPath, FString& OutError)
	{
		FARFilter Filter;
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Found;
		IAssetRegistry::GetChecked().EnumerateAssets(Filter, [&Found, &Name](const FAssetData& Asset)
		{
			if (Asset.AssetName.ToString().Equals(Name, ESearchCase::IgnoreCase))
			{
				Found.Add(Asset);
			}
			return true;
		});

		if (Found.Num() != 1)
		{
			OutError = Found.Num() == 0
				? FString::Printf(TEXT("'%s' 이라는 에셋이 없습니다"), *Name)
				: FString::Printf(TEXT("'%s' 이름의 에셋이 %d 개라 고를 수 없습니다 — [경로] 칸에 전체 경로를 적으세요"), *Name, Found.Num());
			return false;
		}
		OutPath = Found[0].GetObjectPathString();
		return true;
	}

	FAutoConsoleCommand GTuningBakeCommand(
		TEXT("DRTuningBake"),
		TEXT("튜닝.csv 의 [변경값] 을 에셋에 반영한다. 인자: [시트 경로.csv] [save] [clear] — save=바로 저장, clear=반영한 변경값 비우기"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			FString SheetPath;
			bool bSave = false;
			bool bClear = false;
			for (const FString& Arg : Args)
			{
				if (Arg.Equals(TEXT("save"), ESearchCase::IgnoreCase))			bSave = true;
				else if (Arg.Equals(TEXT("clear"), ESearchCase::IgnoreCase))	bClear = true;
				else															SheetPath = Arg.TrimQuotes();
			}

			const FString Report = UDRTuningEditorTools::BakeTuningSheet(SheetPath, bSave, bClear);
			TArray<FString> Lines;
			Report.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				UE_LOG(LogDR, Display, TEXT("[TuningBake] %s"), *Line);
			}
		}));
}

FString UDRTuningEditorTools::ExportTuningCatalog()
{
	FString Path, Error;
	int32 RowCount = 0;
	if (UDRTuningSubsystem::WriteCatalogTemplate(Path, Error, RowCount))
	{
		return FString::Printf(TEXT("목록 %d 줄: %s"), RowCount, *Path);
	}
	return FString::Printf(TEXT("목록을 만들지 못했습니다: %s"), *Error);
}

FString UDRTuningEditorTools::BakeTuningSheet(const FString& InSheetPath, bool bSavePackages, bool bClearBakedValues)
{
	// PIE 중에는 튜닝 값이 CDO 에 덮여 있을 수 있어, 그 상태로 저장하면 "원래 값" 이 무엇인지 알 수 없게 된다
	if (GEditor && GEditor->PlayWorld)
	{
		return TEXT("PIE 중에는 반영하지 않습니다 — PIE 를 끝내고 다시 실행하세요");
	}

	const FString SheetPath = InSheetPath.IsEmpty() ? UDRTuningSubsystem::GetSheetPath() : FPaths::ConvertRelativePathToFull(InSheetPath);

	FString Text, Error;
	TArray<FDRTuningRow> Rows;
	if (!DRTuningSheet::LoadFileText(SheetPath, Text))
	{
		return FString::Printf(TEXT("파일을 열지 못했습니다: %s"), *SheetPath);
	}
	if (!DRTuningSheet::ParseSheet(Text, Rows, Error))
	{
		return FString::Printf(TEXT("시트를 읽지 못했습니다: %s (%s)"), *Error, *SheetPath);
	}

	TArray<FString> Lines;
	TSet<UPackage*> ChangedPackages;
	TMap<int32, FString> BakedRows;	// 줄 인덱스 → 반영된 값
	int32 BakedCount = 0;
	int32 SameCount = 0;
	int32 FailedCount = 0;

	{
		const FScopedTransaction Transaction(LOCTEXT("BakeTuning", "튜닝 값을 에셋에 반영"));

		for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
		{
			const FDRTuningRow& Row = Rows[RowIndex];
			if (Row.Override.IsEmpty()) continue;

			const FString Where = FString::Printf(TEXT("%d번째 줄  %s · %s"), Row.LineNumber,
				*(Row.Target.IsEmpty() ? ShortTargetName(Row.Path) : Row.Target), *Row.Property);

			FString Target = Row.Path.IsEmpty() ? Row.Target : Row.Path;
			if (!Target.StartsWith(TEXT("/")) && !FindAssetPathByShortName(Target, Target, Error))
			{
				++FailedCount;
				Lines.Add(FString::Printf(TEXT("✘ %s  →  %s"), *Where, *Error));
				continue;
			}

			FString LoadPath;
			UObject* Root = DRTuning::LoadTargetObject(Target, LoadPath, Error);
			DRTuning::FResolvedValue Value;
			if (!Root || !DRTuning::ResolvePath(Root, Row.Property, Value, Error))
			{
				++FailedCount;
				Lines.Add(FString::Printf(TEXT("✘ %s  →  %s"), *Where, *Error));
				continue;
			}

			const DRTuning::FTypedValue OldValue = DRTuning::ReadValue(Value);
			const FString OldText = DRTuning::ValueToText(Value);
			if (!DRTuning::SetValueFromText(Value, Row.Override, Error))
			{
				++FailedCount;
				Lines.Add(FString::Printf(TEXT("✘ %s = %s  →  %s"), *Where, *Row.Override, *Error));
				continue;
			}
			const DRTuning::FTypedValue NewValue = DRTuning::ReadValue(Value);
			const FString NewText = DRTuning::ValueToText(Value);

			if (NewValue == OldValue)
			{
				++SameCount;
				BakedRows.Add(RowIndex, NewText);
				Lines.Add(FString::Printf(TEXT("= %s  이미 %s"), *Where, *NewText));
				continue;
			}

			// 실행 취소가 되도록 원래 값으로 돌려 놓고 기록(Modify)한 뒤 다시 쓴다
			DRTuning::WriteValue(Value, OldValue);
			Root->Modify();
			if (Value.OwnerObject && Value.OwnerObject != Root)
			{
				Value.OwnerObject->Modify();
			}
			DRTuning::WriteValue(Value, NewValue);
			if (Value.OwnerObject)
			{
				Value.OwnerObject->PostEditChange();
			}
			ChangedPackages.Add(Root->GetOutermost());

			// 하위 BP 는 부모 기본값을 물려받는다(차이만 저장) — 지금 열린 CDO 만 맞추면 따로 저장할 것이 없다
			int32 DerivedCount = 0;
			if (Root->HasAnyFlags(RF_ClassDefaultObject))
			{
				TArray<UClass*> DerivedClasses;
				GetDerivedClasses(Root->GetClass(), DerivedClasses, /*bRecursive*/ true);
				for (UClass* Derived : DerivedClasses)
				{
					if (Derived->HasAnyClassFlags(CLASS_NewerVersionExists)) continue;
					UObject* DerivedDefault = Derived->GetDefaultObject(/*bCreateIfNeeded*/ false);
					DRTuning::FResolvedValue DerivedValue;
					FString IgnoredError;
					if (DerivedDefault && DRTuning::ResolvePath(DerivedDefault, Row.Property, DerivedValue, IgnoredError)
						&& DRTuning::ReadValue(DerivedValue) == OldValue)
					{
						DerivedDefault->Modify();
						DRTuning::WriteValue(DerivedValue, NewValue);
						++DerivedCount;
					}
				}
			}

			++BakedCount;
			BakedRows.Add(RowIndex, NewText);
			Lines.Add(FString::Printf(TEXT("✔ %s  %s → %s%s"), *Where, *OldText, *NewText,
				DerivedCount > 0 ? *FString::Printf(TEXT("  (물려받는 하위 BP %d)"), DerivedCount) : TEXT("")));
		}
	}

	if (bSavePackages && ChangedPackages.Num() > 0)
	{
		const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(ChangedPackages.Array(), /*bOnlyDirty*/ true);
		Lines.Add(bSaved ? FString::Printf(TEXT("에셋 %d 개를 저장했습니다"), ChangedPackages.Num()) : TEXT("⚠ 일부 에셋을 저장하지 못했습니다 — 콘텐츠 브라우저에서 확인하세요"));
	}

	if (bClearBakedValues && BakedRows.Num() > 0)
	{
		const FString Stamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
		for (const TPair<int32, FString>& Pair : BakedRows)
		{
			FDRTuningRow& Row = Rows[Pair.Key];
			const FString Record = FString::Printf(TEXT("[%s 반영 %s]"), *Stamp, *Pair.Value);
			Row.Note = Row.Note.IsEmpty() ? Record : Row.Note + TEXT(" ") + Record;
			Row.Current = Pair.Value;
			Row.Override.Reset();
		}
		if (!DRTuningSheet::SaveText(SheetPath, DRTuningSheet::BuildSheetText(Rows)))
		{
			Lines.Add(TEXT("⚠ 시트를 다시 쓰지 못했습니다 — 엑셀에서 열려 있으면 닫고 다시 하세요"));
		}
	}

	FString Report = FString::Printf(TEXT("튜닝 반영 — %s\n반영 %d · 이미 같음 %d · 실패 %d · 바뀐 에셋 %d 개"),
		*SheetPath, BakedCount, SameCount, FailedCount, ChangedPackages.Num());
	if (!bSavePackages && ChangedPackages.Num() > 0)
	{
		Report += TEXT("\n아직 저장하지 않았습니다 — 확인 후 저장하세요 (모두 저장 또는 DRTuningBake save)");
	}
	if (BakedCount + SameCount + FailedCount == 0)
	{
		Report += TEXT("\n[변경값] 이 채워진 줄이 없습니다");
	}
	for (const FString& Line : Lines)
	{
		Report += TEXT("\n") + Line;
	}
	return Report;
}

#undef LOCTEXT_NAMESPACE
