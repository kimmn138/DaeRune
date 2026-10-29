// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DaeRune : ModuleRules
{
	public DaeRune(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayAbilities", "Slate", "SlateCore", "UMG", "MoviePlayer", "MultiplayerSessions", "NavigationSystem", "OnlineSubsystem", "OnlineSubsystemUtils" });

		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayTags", "GameplayTasks", "Niagara", "NiagaraAnimNotifies", "AIModule" });

		// 카탈로그 저장 시 검증(GameMapsSettings 로 GameInstance 기본값을 찾는다 — EngineSettings)용.
		// 에디터 도구는 별도 모듈(DaeRuneEditor)로 옮겼다. 코드도 WITH_EDITOR 로 감싸 둔다.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("EngineSettings");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
