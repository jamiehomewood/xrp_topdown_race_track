using UnrealBuildTool;

public class xrp_TopDownRaceTarget : TargetRules
{
	public xrp_TopDownRaceTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("xrp_TopDownRace");
	}
}
