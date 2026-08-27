// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/DRUpgradeTypes.h"
#include "DRChipCatalog.generated.h"

/**
 * 모든 로봇의 업그레이드 칩 정의를 담는 DataAsset (디자이너 튜닝용).
 * 서버/클라 양쪽에서 참조한다 — 클라는 장착/표시, 서버는 복제받은 장착 목록 검증에 사용.
 * (Plan2.md 4.4 참조)
 */
UCLASS(BlueprintType)
class DAERUNE_API UDRChipCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chips", meta = (TitleProperty = "ChipId"))
	TArray<FDRUpgradeChipDefinition> Chips;

	// 스킬 태그 → 표시명 (칩 카드의 "적용 대상 스킬" 표기용). 없으면 태그 마지막 조각으로 폴백한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chips")
	TMap<FGameplayTag, FText> SkillDisplayNames;

	// ChipId 로 정의 조회. 없으면 nullptr.
	const FDRUpgradeChipDefinition* FindChip(FName ChipId) const;

	// 특정 로봇의 칩 정의 전체 (업그레이드 화면 목록 구성용).
	// 분류(Category)로 거르지 않는다 — 그건 UI 표시 필터라서 뷰모델 단계에서 처리한다.
	UFUNCTION(BlueprintCallable, Category = "Chips")
	void GetChipsForClass(EPlayerCharacterClass CharacterClass,
		TArray<FDRUpgradeChipDefinition>& OutChips) const;

	// 대상 스킬 표시명. 태그가 비어 있으면 "전체".
	UFUNCTION(BlueprintCallable, Category = "Chips")
	FText GetSkillDisplayName(const FGameplayTag& AbilityTag) const;

	// 장착 칩 목록을 즉시 조회 가능한 수치 캐시로 펼친다.
	// OwnerClass 가 다른 항목은 무시한다(잘못된 목록 방어).
	void ResolveLoadout(const TArray<FName>& EquippedChips, EPlayerCharacterClass CharacterClass,
		FDRUpgradeRuntime& OutRuntime) const;

	// 서버가 클라이언트 신고 장착 목록을 정화한다.
	// - 카탈로그에 없는 Id / 다른 클래스 소속 항목 제거
	// - 중복 Id 제거 (동일 칩 중복 장착 금지)
	// - 필요 슬롯 수 합이 MaxSlots 를 넘으면 초과분 제거 (구조적 상한)
	//   서버는 클라의 해금 단계를 모르지만 총 칸 수는 알기 때문에 이 선까지 막는다.
	// 반환: 하나라도 수정했으면 true (로깅용)
	bool SanitizeLoadout(TArray<FName>& InOutChips, EPlayerCharacterClass CharacterClass,
		int32 MaxSlots) const;

	// 카탈로그 정합성 검사 (중복 Id, 슬롯 수 범위, 효과 누락 등). 콘텐츠 작업 후 1회 실행.
	UFUNCTION(BlueprintCallable, Category = "Chips")
	bool ValidateCatalog(int32 MaxSlots, TArray<FString>& OutErrors) const;

protected:
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// ChipId → Chips 인덱스 캐시 (선형 탐색 제거)
	mutable TMap<FName, int32> IdToIndex;

	void BuildIndex() const;
	FORCEINLINE void EnsureIndex() const
	{
		if (IdToIndex.Num() != Chips.Num())
		{
			BuildIndex();
		}
	}
};
