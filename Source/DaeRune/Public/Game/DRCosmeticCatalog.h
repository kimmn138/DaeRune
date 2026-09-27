// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/DRCosmeticTypes.h"
#include "DRCosmeticCatalog.generated.h"

/**
 * 모든 로봇의 코스메틱(옷) 정의를 담는 DataAsset. 디자이너 튜닝용.
 * UDRChipCatalog 의 자매 에셋이며 같은 규약을 따른다. (Plan.md 4.4 / 15.11 참조)
 *
 * 서버/클라 양쪽에서 참조한다 —
 *   클라: 옷장 화면 목록 구성 + 해금 판정
 *   서버: 복제받은 장착 목록 검증(존재하지 않는 에셋 참조로 클라가 죽는 것을 막는다)
 */
UCLASS(BlueprintType)
class DAERUNE_API UDRCosmeticCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic", meta = (TitleProperty = "SkinId"))
	TArray<FDRSkinDefinition> Skins;

	// SkinId 로 정의 조회. 없으면 nullptr.
	const FDRSkinDefinition* FindSkin(FName SkinId) const;

	/**
	 * 특정 로봇 + 카테고리의 스킨 정의 전체. SortOrder → SkinId 순으로 정렬해 돌려준다.
	 * ★"기본"(장착 해제) 칸은 포함하지 않는다★ — 그건 UI 표시 규칙이라 GameInstance 가 앞에 붙인다.
	 */
	void GetSkinsForCategory(EPlayerCharacterClass CharacterClass, EDRCosmeticCategory Category,
		TArray<const FDRSkinDefinition*>& OutSkins) const;

	/**
	 * 서버가 클라이언트 신고 장착 Id 를 정화한다.
	 * 카탈로그에 없거나 / 다른 로봇 소속이거나 / 다른 카테고리면 NAME_None 을 돌려준다.
	 * (UDRChipCatalog::SanitizeLoadout 과 같은 역할)
	 */
	FName SanitizeSkin(FName SkinId, EPlayerCharacterClass ForClass, EDRCosmeticCategory Category) const;

	// 길이 4 배열을 통째로 정화한다. 하나라도 바뀌면 true (로깅용).
	bool SanitizeLoadout(TArray<FName>& InOutSkinIds, EPlayerCharacterClass ForClass) const;

	// 카탈로그 정합성 검사 (중복 Id, 외형 누락, 소켓 미지정 등). 콘텐츠 작업 후 1회 실행.
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	bool ValidateCatalog(TArray<FString>& OutErrors) const;

protected:
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// SkinId → Skins 인덱스 캐시 (선형 탐색 제거). UDRChipCatalog 와 같은 방식.
	mutable TMap<FName, int32> IdToIndex;

	void BuildIndex() const;

	FORCEINLINE void EnsureIndex() const
	{
		if (IdToIndex.Num() != Skins.Num())
		{
			BuildIndex();
		}
	}
};
