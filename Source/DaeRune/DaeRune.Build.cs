// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DaeRune : ModuleRules
{
	public DaeRune(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayAbilities", "Slate", "SlateCore", "UMG", "MoviePlayer", "MultiplayerSessions", "NavigationSystem", "OnlineSubsystem", "OnlineSubsystemUtils" });

		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayTags", "GameplayTasks", "Niagara", "NiagaraAnimNotifies", "AIModule" });

		// 에디터 전용 도우미(UDRWardrobeEditorTools — WBP 위젯 트리 정리 · 속성 텍스트 임포트)와
		// 카탈로그 저장 시 검증(GameMapsSettings 로 GameInstance 기본값을 찾는다 — EngineSettings)용.
		// 게임 빌드(에디터 아님)에는 들어가지 않는다 — 코드도 WITH_EDITOR 로 감싸 둔다.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "UMGEditor", "EngineSettings", "BlueprintGraph" });
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
