// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "DRGameplayCue_Sound.generated.h"

/**
 * 사운드 재생용 GameplayCue 베이스 클래스
 * 
 * 사용법:
 * 1. 이 클래스를 상속한 블루프린트 생성 (예: GC_Player_Damage)
 * 2. Sound 변수에 사운드 에셋 설정
 * 3. GameplayEffect의 GameplayCues에 태그 추가
 * 
 * Execute vs WhileActive:
 * - Execute (OnExecute): 즉시 한 번 재생 (데미지, 사망 등)
 * - WhileActive (OnActive/OnRemove): 지속 시간 동안 (버프/디버프 등)
 */
UCLASS()
class DAERUNE_API UDRGameplayCue_Sound : public UGameplayCueNotify_Static
{
	GENERATED_BODY()
	
public:
	UDRGameplayCue_Sound();

	// 재생할 사운드
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> Sound;

	// 3D 사운드 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	bool bIs3DSound = true;

	// Execute 시 사운드 재생
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
