using UnrealBuildTool;

public class xrp_TopDownRaceEditorTarget : TargetRules
{
	public xrp_TopDownRaceEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("xrp_TopDownRace");
	}
}
