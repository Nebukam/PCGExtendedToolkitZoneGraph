// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

using UnrealBuildTool;

public class PCGExtendedToolkitZoneGraph : ModuleRules
{
	public PCGExtendedToolkitZoneGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PrivateIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"PCG",
				"PCGExtendedToolkit",
				"ZoneGraph",
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"GeometryCore",
				"GeometryFramework",
				"GeometryScriptingCore",
				"GeometryAlgorithms",
				"RenderCore",
				"RHI",
				"PhysicsCore",
				"NavigationSystem",
				"Slate",
				"SlateCore",
				"GameplayTags",
				"PropertyPath",
				"Settings",
				"DeveloperSettings",
				"PCGExtendedToolkit"
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"Settings"
				});
		}
	}
}