// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class HoloPipesEditorTarget : TargetRules
{
	public HoloPipesEditorTarget(TargetInfo Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V2;
		Type = TargetType.Editor;

		ExtraModuleNames.AddRange( new string[] { "HoloPipes" } );
	}
}
