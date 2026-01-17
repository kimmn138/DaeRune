// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "DRGameUserSettings.generated.h"

/**
 * DaeRune 커스텀 설정 저장 클래스
 */
UCLASS()
class DAERUNE_API UDRGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	
public:
    // 싱글톤 접근자
    static UDRGameUserSettings* GetDRGameUserSettings();

    // 커스텀 설정 적용
    void ApplyCustomSettings();

    // 기본값으로 초기화
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
};
