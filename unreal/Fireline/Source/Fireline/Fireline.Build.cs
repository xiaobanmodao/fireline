// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Fireline : ModuleRules
{
	public Fireline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
            "AnimGraphRuntime", "AnimationCore",
            "AudioMixer",
            "ProceduralMeshComponent",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate", "SlateCore", "Json"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore", "RenderCore" });
        if (Target.Platform == UnrealTargetPlatform.Mac) PublicFrameworks.Add("GameController");

		PublicIncludePaths.AddRange(new string[] {
			"Fireline",
			"Fireline/Variant_Horror",
			"Fireline/Variant_Horror/UI",
			"Fireline/Variant_Shooter",
			"Fireline/Variant_Shooter/AI",
			"Fireline/Variant_Shooter/UI",
			"Fireline/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "Json", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
