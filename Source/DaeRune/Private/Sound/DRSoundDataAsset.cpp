// Copyright DaeRune


#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DataValidation.h"

const UDRSoundDataAsset* UDRSoundDataAsset::Get()
{
	const UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized());
	return AssetManager ? AssetManager->GetSoundDataAsset() : nullptr;
}

USoundBase* UDRSoundDataAsset::Find(FSoundMember Sound)
{
	const UDRSoundDataAsset* Data = Get();
	return Data ? (Data->*Sound).Get() : nullptr;
}

void UDRSoundDataAsset::PlayAtLocation(const UObject* WorldContext, FSoundMember Sound, const FVector& Location)
{
	if (USoundBase* SoundAsset = Find(Sound))
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContext, SoundAsset, Location);
	}
}

void UDRSoundDataAsset::Play2D(const UObject* WorldContext, FSoundMember Sound)
{
	if (USoundBase* SoundAsset = Find(Sound))
	{
		UGameplayStatics::PlaySound2D(WorldContext, SoundAsset);
	}
}

#if WITH_EDITOR
EDataValidationResult UDRSoundDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);

	// 설정창 음악(또는 마스터) 볼륨을 0 으로 내리면 BGM 이 들리지 않게 되는데, 기본값(Restart)이면 엔진이 그 소리를 멈춘다.
	// 반복 곡은 볼륨을 다시 올리면 처음부터 시작하고, 스테이지 플레이리스트 곡(반복 없음)은 끝난 것으로 처리돼
	// ADRBGMActor 가 다음 곡으로 넘긴다 — 호스트가 내리면 복제로 모두의 곡이 바뀐다.
	// Play When Silent 면 들리지 않아도 제자리에서 계속 재생된다.
	auto CheckBGM = [&Context](const USoundBase* Sound)
	{
		if (Sound && !Sound->IsPlayWhenSilent())
		{
			Context.AddWarning(FText::FromString(FString::Printf(
				TEXT("BGM %s 의 Virtualization Mode 가 Play When Silent 가 아니다 — 음악 볼륨을 0 으로 내렸다 올리면 곡이 처음부터 다시 나오거나 다음 곡으로 바뀐다"),
				*Sound->GetName())));
		}
	};

	CheckBGM(BGM_MainMenu);
	CheckBGM(BGM_Lobby);
	CheckBGM(BGM_Tutorial);
	CheckBGM(BGM_Boss);
	for (const TObjectPtr<USoundBase>& Track : BGM_StagePlaylist)
	{
		CheckBGM(Track);
	}
	return Result;
}
#endif
