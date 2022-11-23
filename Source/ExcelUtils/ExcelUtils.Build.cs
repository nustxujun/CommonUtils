// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
public class ExcelUtils : ModuleRules
{
	public ExcelUtils(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);


		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string libxlPath = Path.Combine(ModuleDirectory, "LibXL");
			string IncludePath = Path.Combine(libxlPath, "Include");
			string LibPath = Path.Combine(libxlPath, "Win64", "Lib");
			string DllPath = Path.Combine(libxlPath, "Win64", "Bin");
			string BinPath = Path.Combine(ModuleDirectory, "../../Binaries/Win64");

			// include
			PublicIncludePaths.Add(IncludePath);

			// static library
			PublicLibraryPaths.Add(LibPath);
			PublicAdditionalLibraries.Add(Path.Combine(LibPath,"libxl.lib"));

			// dynamic library
			PublicLibraryPaths.Add(DllPath);

			//PublicDelayLoadDLLs.Add("libxl.dll");
			//RuntimeDependencies.Add(new RuntimeDependency(Path.Combine(DllPath, "libxl.dll")));

			if (!Directory.Exists(BinPath))
				Directory.CreateDirectory(BinPath);
            try
            {
                File.Copy(Path.Combine(DllPath, "libxl.dll"), Path.Combine(BinPath, "libxl.dll"), true);
            }
            catch (Exception e)
            {
                System.Console.WriteLine("copy dll exception,maybe have exists,err=", e.ToString());
            }
        }
	}
}
