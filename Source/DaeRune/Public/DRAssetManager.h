// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "DRAssetManager.generated.h"

/**
 * UDRAssetManager
 *
 * 프로젝트 전역 에셋 관리용 AssetManager 싱글톤 클래스 정의문서
 */
UCLASS()
class DAERUNE_API UDRAssetManager : public UAssetManager
{
	GENERATED_BODY()
	
public:
	// 에셋 매니저 싱글톤 인스턴스 반환 기능
	static UDRAssetManager& Get();

protected:
	// 초기 로딩 단계 시작 시 호출 처리 기능
	virtual void StartInitialLoading() override;
};
