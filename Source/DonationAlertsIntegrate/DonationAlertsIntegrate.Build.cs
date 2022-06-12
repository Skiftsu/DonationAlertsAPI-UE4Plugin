// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DonationAlertsIntegrate : ModuleRules
{
	public DonationAlertsIntegrate(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			});
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Networking",
				"HTTP",
				"Json",
				"JsonUtilities",
				"HTTPServer",
				"WebSockets"
			});
	}
}
