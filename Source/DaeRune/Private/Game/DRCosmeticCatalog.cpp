// Copyright DaeRune


#include "Game/DRCosmeticCatalog.h"
#include "DaeRune/DRLogChannels.h"

#define LOCTEXT_NAMESPACE "DRCosmeticCatalog"

void UDRCosmeticCatalog::BuildIndex() const
{
	IdToIndex.Reset();
	IdToIndex.Reserve(Skins.Num());

	for (int32 Index = 0; Index < Skins.Num(); ++Index)
	{
		const FName& Id = Skins[Index].SkinId;
		if (Id.IsNone()) continue;

		// 중복 Id 는 먼저 온 것을 남긴다. 실제 검출/경고는 ValidateCatalog 가 담당한다.
		if (!IdToIndex.Contains(Id))
		{
			IdToIndex.Add(Id, Index);
		}
	}
}

const FDRSkinDefinition* UDRCosmeticCatalog::FindSkin(FName SkinId) const
{
	if (SkinId.IsNone()) return nullptr;

	EnsureIndex();

	if (const int32* Index = IdToIndex.Find(SkinId))
	{
		return Skins.IsValidIndex(*Index) ? &Skins[*Index] : nullptr;
	}
	return nullptr;
}

void UDRCosmeticCatalog::GetSkinsForCategory(EPlayerCharacterClass CharacterClass,
	EDRCosmeticCategory Category, TArray<const FDRSkinDefinition*>& OutSkins) const
{
	OutSkins.Reset();

	if (!DRIsValidCosmeticCategory(Category)) return;

	for (const FDRSkinDefinition& Skin : Skins)
	{
		if (Skin.SkinId.IsNone()) continue;
		if (Skin.OwnerClass != CharacterClass) continue;
		if (Skin.Category != Category) continue;

		OutSkins.Add(&Skin);
	}

	// SortOrder 우선, 같으면 Id 순 — 카탈로그 배열 순서가 바뀌어도 목록이 흔들리지 않게 한다
	OutSkins.Sort([](const FDRSkinDefinition& A, const FDRSkinDefinition& B)
	{
		if (A.SortOrder != B.SortOrder)
		{
			return A.SortOrder < B.SortOrder;
		}
		return A.SkinId.LexicalLess(B.SkinId);
	});
}

FName UDRCosmeticCatalog::SanitizeSkin(FName SkinId, EPlayerCharacterClass ForClass,
	EDRCosmeticCategory Category) const
{
	// NAME_None 은 "장착 해제"라는 정상 값이다
	if (SkinId.IsNone()) return NAME_None;

	const FDRSkinDefinition* Def = FindSkin(SkinId);
	if (!Def) return NAME_None;
	if (Def->OwnerClass != ForClass) return NAME_None;
	if (Def->Category != Category) return NAME_None;

	return SkinId;
}

bool UDRCosmeticCatalog::SanitizeLoadout(TArray<FName>& InOutSkinIds, EPlayerCharacterClass ForClass) const
{
	const int32 Required = DRGetCosmeticCategoryCount();

	bool bModified = false;

	if (InOutSkinIds.Num() != Required)
	{
		InOutSkinIds.SetNum(Required);
		bModified = true;
	}

	for (int32 Index = 0; Index < Required; ++Index)
	{
		const FName Sanitized =
			SanitizeSkin(InOutSkinIds[Index], ForClass, static_cast<EDRCosmeticCategory>(Index));

		if (Sanitized != InOutSkinIds[Index])
		{
			InOutSkinIds[Index] = Sanitized;
			bModified = true;
		}
	}

	return bModified;
}

bool UDRCosmeticCatalog::ValidateCatalog(TArray<FString>& OutErrors) const
{
	OutErrors.Reset();

	TSet<FName> SeenIds;

	for (int32 Index = 0; Index < Skins.Num(); ++Index)
	{
		const FDRSkinDefinition& Skin = Skins[Index];

		if (Skin.SkinId.IsNone())
		{
			OutErrors.Add(FString::Printf(TEXT("[%d] SkinId 가 비어 있습니다."), Index));
			continue;
		}

		bool bAlreadySeen = false;
		SeenIds.Add(Skin.SkinId, &bAlreadySeen);
		if (bAlreadySeen)
		{
			OutErrors.Add(FString::Printf(TEXT("[%d] SkinId '%s' 가 중복입니다."),
				Index, *Skin.SkinId.ToString()));
		}

		if (!DRIsValidCosmeticCategory(Skin.Category))
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] Category 가 유효하지 않습니다."),
				*Skin.SkinId.ToString()));
		}

		if (!Skin.HasAnyVisual())
		{
			OutErrors.Add(FString::Printf(
				TEXT("[%s] 부착 메시도 머티리얼 오버라이드도 없습니다 — 장착해도 외형이 바뀌지 않습니다."),
				*Skin.SkinId.ToString()));
		}

		// 소켓 미지정 경고: LeaderPose 가 아닌데 소켓이 없으면 부착물이 원점(대개 발밑)에 박힌다
		if (Skin.ThirdPerson.HasMesh() && !Skin.ThirdPerson.bUseLeaderPose
			&& Skin.ThirdPerson.SocketName.IsNone())
		{
			OutErrors.Add(FString::Printf(
				TEXT("[%s] ThirdPerson 소켓이 지정되지 않았습니다 — 부착물이 캐릭터 원점에 붙습니다."),
				*Skin.SkinId.ToString()));
		}

		if (Skin.FirstPerson.HasMesh() && !Skin.FirstPerson.bUseLeaderPose
			&& Skin.FirstPerson.SocketName.IsNone())
		{
			OutErrors.Add(FString::Printf(
				TEXT("[%s] FirstPerson 소켓이 지정되지 않았습니다 — 부착물이 캐릭터 원점에 붙습니다."),
				*Skin.SkinId.ToString()));
		}

		if (Skin.DisplayName.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] DisplayName 이 비어 있습니다."),
				*Skin.SkinId.ToString()));
		}
	}

	return OutErrors.Num() == 0;
}

void UDRCosmeticCatalog::PostLoad()
{
	Super::PostLoad();
	BuildIndex();
}

#if WITH_EDITOR
void UDRCosmeticCatalog::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	BuildIndex();

	// 편집할 때마다 자동 검사해 로그로 알린다.
	// (수동 호출을 잊어 잘못된 데이터가 빌드에 들어가는 것을 막기 위함 — UDRProgressionConfig 와 같은 관례)
	TArray<FString> Errors;
	if (!ValidateCatalog(Errors))
	{
		for (const FString& Error : Errors)
		{
			UE_LOG(LogDR, Warning, TEXT("[Cosmetic] 카탈로그 검증: %s"), *Error);
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
