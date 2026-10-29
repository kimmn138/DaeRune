// Copyright DaeRune

#pragma once

// 에디터 위젯 도구(캐릭터 정보 · 스테이지 선택 · 옷장)가 같이 쓰는 헬퍼.
// 세 도구가 똑같이 복붙해 갖고 있던 것을 모았다. 각 cpp 의 #if WITH_EDITOR 안에서만 포함한다.
#if WITH_EDITOR

#include "CoreMinimal.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Engine/Font.h"
#include "Fonts/CompositeFont.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

namespace DREditorToolCommon
{
	// 시안의 CSS 색(sRGB)을 그대로 옮긴다 — UMG 색 값은 선형이다
	inline FLinearColor SRGB(uint8 R, uint8 G, uint8 B, float A = 1.0f)
	{
		FLinearColor Color = FLinearColor::FromSRGBColor(FColor(R, G, B, 255));
		Color.A = A;
		return Color;
	}

	inline FName PickTypeface(const UObject* FontObject)
	{
		if (const UFont* Font = Cast<UFont>(FontObject))
		{
			if (const FCompositeFont* Composite = Font->GetCompositeFont())
			{
				if (Composite->DefaultTypeface.Fonts.Num() > 0)
				{
					return Composite->DefaultTypeface.Fonts[0].Name;
				}
			}
		}
		return NAME_None;
	}

	inline void DiscardRecursive(UWidgetTree* Tree, UWidget* Widget)
	{
		if (!Widget) return;

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			TArray<UWidget*> Children = Panel->GetAllChildren();
			for (UWidget* Child : Children)
			{
				DiscardRecursive(Tree, Child);
			}
		}

		Tree->RemoveWidget(Widget);
		Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
	}

	template <typename T, typename TLog>
	T* Load(const TCHAR* Path, TLog& Log)
	{
		T* Object = LoadObject<T>(nullptr, Path);
		if (!Object)
		{
			Log.Add(FString::Printf(TEXT("★에셋 없음★ %s"), Path));
		}
		return Object;
	}

	template <typename TLog>
	bool CompileAndMark(UWidgetBlueprint* Blueprint, TLog& Log)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Blueprint->MarkPackageDirty();

		const bool bOk = Blueprint->Status != BS_Error;
		Log.Add(FString::Printf(TEXT("컴파일 %s: %s"), bOk ? TEXT("성공") : TEXT("★실패★"), *Blueprint->GetName()));
		return bOk;
	}

	template <typename TLog>
	bool SetObjectProperty(UObject* Object, FName PropertyName, UObject* Value, TLog& Log)
	{
		FObjectPropertyBase* Property = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName) : nullptr;
		if (!Property)
		{
			Log.Add(FString::Printf(TEXT("프로퍼티 없음: %s"), *PropertyName.ToString()));
			return false;
		}

		Object->Modify();
		Property->SetObjectPropertyValue_InContainer(Object, Value);
		Log.Add(FString::Printf(TEXT("기본값 %s = %s"), *PropertyName.ToString(), Value ? *Value->GetName() : TEXT("None")));
		return true;
	}
}

#endif // WITH_EDITOR
