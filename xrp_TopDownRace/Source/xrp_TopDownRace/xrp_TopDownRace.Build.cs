using UnrealBuildTool;

public class xrp_TopDownRace : ModuleRules
{
	public xrp_TopDownRace(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"DeveloperSettings",
			"AudioMixer",
		});
	}
}
