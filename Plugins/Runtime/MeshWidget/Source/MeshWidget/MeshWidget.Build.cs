// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MeshWidget : ModuleRules
	{
		public MeshWidget(TargetInfo Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/MeshWidget/Private",
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core", "CoreUObject", "Engine", "InputCore", "ShaderCore", "RenderCore", "UMG", "SlateCore", "Slate", "RHI"
					// ... add other public dependencies that you statically link with here ...
				}
				);

            if (Target.Type != TargetRules.TargetType.Server)
            {
                PrivateIncludePathModuleNames.AddRange(
                    new string[] {
                    "SlateRHIRenderer",
                    }
                );
            }
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);
		}
	}
}