// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.IO;
using UnrealBuildTool;

public class HoloPipes : ModuleRules
{
    const string SavedGameManagerName = "savedgamemanager";
    const string ThirdPartyDir = "thirdparty";
    const string LibDir = "lib";
    const string BinDir = "bin";

    private string ModulePath
    {
        get
        {
            return Path.GetDirectoryName(RulesCompiler.GetFileNameFromType(this.GetType()));
        }
    }

    private string SavedGameManagerRelativePath
    {
        get
        {
            return String.Format(@"..\..\{0}\{1}", ThirdPartyDir, SavedGameManagerName);
        }
    }

    private string SavedGameManagerPath
    {
        get
        {
            return Path.GetFullPath(Path.Combine(ModulePath, SavedGameManagerRelativePath));
        }
    }

	public HoloPipes(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        ShadowVariableWarningLevel = WarningLevel.Error;
        bLegacyPublicIncludePaths = true;


        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "XmlParser" });

        string sgm_inc = Path.Combine(SavedGameManagerPath, "inc");

        PublicIncludePaths.Add(sgm_inc);

        string config = Target.Configuration == UnrealTargetConfiguration.DebugGame ? "debug" : "release";

        string arch = "invalid";

        switch (Target.WindowsPlatform.Architecture)
        {
            case WindowsArchitecture.x64:
                arch = Target.Platform == UnrealTargetPlatform.HoloLens ? "emulator" : "desktop";
                break;

            case WindowsArchitecture.ARM64:
                arch = Target.Platform == UnrealTargetPlatform.HoloLens ? "hololens" : "invalid";
                break;
        }
        
        string sgm_lib_dir = Path.Combine(SavedGameManagerPath, LibDir, arch);
        string sgm_bin_dir = Path.Combine(SavedGameManagerPath, BinDir, arch);

        PublicAdditionalLibraries.AddRange(new string[] { Path.Combine(sgm_lib_dir, String.Format("{0}.lib", SavedGameManagerName)) });

        string sgm_dll = String.Format("{0}.dll", SavedGameManagerName);

        PublicDelayLoadDLLs.Add(sgm_dll);
        RuntimeDependencies.Add(Path.Combine(sgm_bin_dir, sgm_dll));

        string sgm_rel_def_path = String.Format("SGM_DLL_RELPATH=L\"{0}\\{1}\\{2}\\{3}\"", SavedGameManagerRelativePath, BinDir, arch, sgm_dll);
        sgm_rel_def_path = sgm_rel_def_path.Replace("\\", "\\\\");

        Console.WriteLine(sgm_rel_def_path);
        PublicDefinitions.Add(sgm_rel_def_path);
    }
}
