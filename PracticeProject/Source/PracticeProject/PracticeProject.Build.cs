// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PracticeProject : ModuleRules
{
	public PracticeProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "CableComponent", "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
