// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRUpgradeUIStyle.generated.h"

class UTexture2D;

/**
 * 업그레이드 화면의 색/브러시 SSOT. (Plan2.md 18.2 팔레트 / 21.6 참조)
 *
 * 위젯이 색을 각자 하드코딩하면 톤을 한 번 바꿀 때마다 위젯 10개를 전부 열어야 한다.
 * 위젯은 이 에셋을 UDRGameInstance::GetUpgradeUIStyle() 로 읽어 쓴다.
 *
 * 콘텐츠: Content/DaeRuneAssets/UI/Upgrade/DA_UpgradeUIStyle (Plan2.md 22 STEP 2)
 */
UCLASS(BlueprintType)
class DAERUNE_API UDRUpgradeUIStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	// ========== 텍스트 ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color")
	FLinearColor TextPrimary = FLinearColor(0.92f, 0.96f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color")
	FLinearColor TextSecondary = FLinearColor(0.62f, 0.70f, 0.78f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color")
	FLinearColor TextDim = FLinearColor(0.40f, 0.46f, 0.53f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color")
	FLinearColor TextDisabled = FLinearColor(0.28f, 0.31f, 0.35f, 1.f);

	// ========== 강조 / 상태 ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color")
	FLinearColor AccentCyan = FLinearColor(0.20f, 0.80f, 0.92f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color")
	FLinearColor AccentCyanGlow = FLinearColor(0.35f, 0.95f, 1.f, 1.f);

	// 단점 모디파이어 / 나빠지는 미리보기 / 실패 토스트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color")
	FLinearColor WarnRed = FLinearColor(0.90f, 0.28f, 0.30f, 1.f);

	// 장점 모디파이어 / 좋아지는 미리보기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color")
	FLinearColor OkGreen = FLinearColor(0.35f, 0.85f, 0.45f, 1.f);

	// ========== 칩 카드 오버레이 불투명도 ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chip", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OverlayOpacityNormal = 0.10f;

	// 장착 불가(빈 칸 부족 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chip", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OverlayOpacityDisabled = 0.70f;

	// 해금 단계 미달
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chip", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OverlayOpacityLocked = 0.35f;

	// ========== 공용 아이콘 ==========

	// 칩에 Icon 이 지정되지 않았을 때의 폴백
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chip")
	TObjectPtr<UTexture2D> DefaultChipIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chip")
	TObjectPtr<UTexture2D> LockIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chip")
	TObjectPtr<UTexture2D> CurrencyIcon;
};
