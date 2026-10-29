// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Tuning/DRTuningTypes.h"

/** 목록(시트에 실을 튜닝 값)을 무엇에서 모을지 — UDRTuningSubsystem 의 ini 설정을 옮겨 담은 것 */
struct FDRTuningCatalogSettings
{
	TArray<FSoftClassPath> BlueprintBases;	// 이 C++ 클래스를 상속한 블루프린트 전부 (CDO 값)
	TArray<FSoftClassPath> AssetClasses;	// 이 클래스의 에셋 전부 (데이터 에셋 · 데이터 테이블 · 커브 테이블 · 비헤이비어 트리)
	TArray<FSoftObjectPath> Assets;			// 그 밖에 따로 실을 에셋
	TArray<FString> Maps;					// 배치 액터의 "인스턴스마다 따로 정한 값" 을 실을 맵 (/Game/Maps/Stage1)
	TArray<FString> ExcludedPathContains;	// 경로에 이 글자가 들어 있으면 뺀다
	TArray<FDRTuningGroupRule> GroupRules;	// [분류] 칸 규칙
};

/**
 * 목록 만들기 — 대상 에셋을 실제로 불러와 리플렉션으로 숫자 · 참거짓 속성을 훑는다.
 *
 * 에디터(쿡 포함)에서는 C++ 주석(툴팁)과 ClampMin/Max 메타데이터로 [설명] · [범위] 를 채운다.
 * 쿠킹된 게임에는 메타데이터가 없어서, 게임은 보통 쿡 때 만든 목록 파일(TuningCatalog.csv)을 읽는다.
 */
namespace DRTuningCatalog
{
	/** 이미 덮어쓴 값의 "덮기 전 값" 조회 — [현재값] 칸은 언제나 빌드 기준값이어야 한다 */
	using FOriginalLookup = TFunction<bool(const UObject* /*Root*/, const FString& /*Path*/, FString& /*OutText*/)>;

	/**
	 * 목록에 넣을 패키지인가 — 쿡에서는 "이번 빌드에 들어간 패키지" 만 넘긴다.
	 * 아무 데서도 안 쓰는 고장 난 BP(GA_TestAbility 등)를 목록 때문에 불러 오면 그 컴파일 오류로 쿡이 실패한다.
	 * 비어 있으면: 에디터 · -game 에서는 아무것도 참조하지 않는 BP 를 뺀다 (빌드에 안 들어가는 것들)
	 */
	using FPackageFilter = TFunction<bool(FName /*PackageName*/)>;

	void BuildLiveCatalog(const FDRTuningCatalogSettings& Settings, TArray<FDRTuningRow>& OutRows,
		const FOriginalLookup& OriginalLookup, const FPackageFilter& PackageFilter = nullptr);
}
