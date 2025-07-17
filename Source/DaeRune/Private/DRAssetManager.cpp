// Copyright DaeRune


#include "DRAssetManager.h"
#include "AbilitySystemGlobals.h"
#include "DRGameplayTags.h"

// 에셋 매니저 싱글톤 인스턴스 접근 구현부
UDRAssetManager& UDRAssetManager::Get()
{
	check(GEngine); // GEngine 유효성 검사

	// 글로벌 AssetManager 캐스팅 과정
	UDRAssetManager* DRAssetManager = Cast<UDRAssetManager>(GEngine->AssetManager);
	// 싱글톤 인스턴스 반환
	return *DRAssetManager;
}

// 초기 로딩 단계 시작 시 네이티브 태그 및 글로벌 데이터 초기화 처리 기능
void UDRAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	// 네이티브 게임플레이 태그 초기화
	FDRGameplayTags::InitializeNativeGameplayTags();

	// 글로벌 어빌리티 시스템 데이터 초기화
	UAbilitySystemGlobals::Get().InitGlobalData();
}
