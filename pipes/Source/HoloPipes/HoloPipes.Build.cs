// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.IO;
using UnrealBuildTool;

public class HoloPipes : ModuleRules
{
	public HoloPipes(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        ShadowVariableWarningLevel = WarningLevel.Error;
        bLegacyPublicIncludePaths = true;
        bEnableExceptions = true;
        bUseUnity = false;
        CppStandard = CppStandardVersion.Cpp17;
        PublicSystemLibraries.AddRange(new string[] { "shlwapi.lib", "runtimeobject.lib" });
        PrivateIncludePaths.Add(Path.Combine(Target.WindowsPlatform.WindowsSdkDir, "Include", Target.WindowsPlatform.WindowsSdkVersion, "cppwinrt"));
        PrivatePCHHeaderFile = @"Private\pch.h";

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "XmlParser" });
    }
}
