// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "DRGameUserSettings.generated.h"

/**
 * DaeRune Ŀ���� ���� ���� Ŭ����
 */
UCLASS()
class DAERUNE_API UDRGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	
public:
    // �̱��� ������
    static UDRGameUserSettings* GetDRGameUserSettings();

    // Ŀ���� ���� ����
    void ApplyCustomSettings();

    // �⺻������ �ʱ�ȭ
    virtual void SetToDefaults() override;

public:
    UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Audio")
    float MasterVolume = 1.0f;

    UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Audio")
    float BGMVolume = 1.0f;

    UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Audio")
    float SFXVolume = 1.0f;

    UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Gameplay")
    float MouseSensitivity = 1.0f;

    UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Graphics")
    float Gamma = 80.0f;

    /** Preferred UI culture code: "ko" (Korean) or "en" (English). Saved to GameUserSettings.ini. */
    UPROPERTY(Config, BlueprintReadWrite, Category = "Settings|Localization")
    FString PreferredCulture = TEXT("ko");
};
