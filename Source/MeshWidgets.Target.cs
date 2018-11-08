
using UnrealBuildTool;
using System.Collections.Generic;

public class MeshWidgetsTarget : TargetRules
{
	public MeshWidgetsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;

		ExtraModuleNames.AddRange( new string[] { "MeshWidgets" } );
	}
}
