using UnrealBuildTool;

public class xrp_TopDownRace : ModuleRules
{
	public xrp_TopDownRace(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Each source file keeps small helpers (asset paths, mesh builders) in its own anonymous namespace;
		// unity builds would merge files and clash on those names. The module is small, so build files separately.
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"DeveloperSettings",
			"AudioMixer",
			"ProceduralMeshComponent",
		});
	}
}
