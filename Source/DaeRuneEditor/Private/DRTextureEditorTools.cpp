// Copyright DaeRune

#include "DRTextureEditorTools.h"

#include "Engine/Texture2D.h"

#if WITH_EDITOR
#include "TextureCompiler.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#endif

FString UDRTextureEditorTools::DescribeBuiltTexture(UTexture2D* Texture)
{
#if WITH_EDITOR
	if (!Texture) return TEXT("null");

	FTextureCompilingManager::Get().FinishCompilation({ Texture });

	const FTexturePlatformData* Data = Texture->GetPlatformData();
	if (!Data) return TEXT("no platform data");

	return FString::Printf(TEXT("%dx%d %s mips=%d stream=%d"),
		Data->SizeX, Data->SizeY, GetPixelFormatString(Data->PixelFormat), Data->Mips.Num(),
		Texture->NeverStream ? 0 : 1);
#else
	return FString();
#endif
}

FString UDRTextureEditorTools::DumpWidgetImages(UWidgetBlueprint* WidgetBlueprint)
{
#if WITH_EDITOR
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree) return TEXT("null");

	FString Out;
	WidgetBlueprint->WidgetTree->ForEachWidget([&Out](UWidget* Widget)
	{
		const UImage* Image = Cast<UImage>(Widget);
		if (!Image) return;

		const FSlateBrush& Brush = Image->GetBrush();
		const UObject* Resource = Brush.GetResourceObject();

		FString SlotInfo;
		if (const UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Image->Slot))
		{
			const FAnchorData Layout = Canvas->GetLayout();
			SlotInfo = FString::Printf(TEXT("canvas anchors=(%.2f,%.2f)-(%.2f,%.2f) offsets=(%.0f,%.0f,%.0f,%.0f) auto=%d"),
				Layout.Anchors.Minimum.X, Layout.Anchors.Minimum.Y, Layout.Anchors.Maximum.X, Layout.Anchors.Maximum.Y,
				Layout.Offsets.Left, Layout.Offsets.Top, Layout.Offsets.Right, Layout.Offsets.Bottom,
				Canvas->GetAutoSize() ? 1 : 0);
		}
		else if (Image->Slot)
		{
			SlotInfo = Image->Slot->GetClass()->GetName();
		}

		// 바로 위 SizeBox 가 크기를 정하는 경우가 많다
		if (const USizeBox* Box = Cast<USizeBox>(Image->GetParent()))
		{
			SlotInfo += FString::Printf(TEXT(" sizebox=(%.0f,%.0f)"), Box->GetWidthOverride(), Box->GetHeightOverride());
		}

		Out += FString::Printf(TEXT("%s | %s | %.0fx%.0f | drawas=%d | %s\n"),
			*Image->GetName(), Resource ? *Resource->GetPathName() : TEXT("-"),
			Brush.ImageSize.X, Brush.ImageSize.Y, static_cast<int32>(Brush.DrawAs), *SlotInfo);
	});
	return Out;
#else
	return FString();
#endif
}
