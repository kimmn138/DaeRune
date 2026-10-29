// Copyright DaeRune

using UnrealBuildTool;

// 에디터 전용 도구 모듈 — WBP 위젯 트리 생성 · 텍스처 · 이펙트 · BP 정리 도우미.
// 게임(쿠킹) 빌드에는 들어가지 않는다 (.uproject 의 Type = Editor).
public class DaeRuneEditor : ModuleRules
{
	public DaeRuneEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"DaeRune",
			"Slate", "SlateCore", "UMG", "UMGEditor",
			"UnrealEd", "BlueprintGraph", "Kismet", "AssetRegistry",
			"Niagara", "NiagaraEditor",
			"GameplayTags", "GameplayAbilities"
		});
	}
}
