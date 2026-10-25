// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRTextureEditorTools.generated.h"

class UTexture2D;
class UWidgetBlueprint;

/**
 * 텍스처 · UI 이미지 점검 도우미 — ★에디터 전용★ (게임 빌드에서는 본문이 비어 있다).
 * Python 은 텍스처의 실제 빌드 결과(포맷 · 밉 수)와 WBP 위젯 트리를 읽을 수 없어서 C++ 로 둔다.
 *   unreal.DRTextureEditorTools.describe_built_texture(tex)
 *   unreal.DRTextureEditorTools.dump_widget_images(wbp)
 */
UCLASS()
class DAERUNE_API UDRTextureEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 빌드(컴파일)가 끝나길 기다린 뒤 "가로x세로 포맷 mips=N stream=0/1" — 게임에서 실제로 쓰는 값 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static FString DescribeBuiltTexture(UTexture2D* Texture);

	/**
	 * WBP 의 Image 위젯마다 한 줄: "위젯 | 리소스 | ImageSize | DrawAs | 슬롯 정보".
	 * 슬롯 정보는 캔버스면 앵커 · 오프셋 · 크기, SizeBox 부모면 덮어쓴 크기.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static FString DumpWidgetImages(UWidgetBlueprint* WidgetBlueprint);
};
