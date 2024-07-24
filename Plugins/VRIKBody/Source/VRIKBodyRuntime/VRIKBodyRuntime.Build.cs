// VR IK Body Plugin
// (c) Yuri N Kalinin, 2021, ykasczc@gmail.com. All right reserved.

using UnrealBuildTool;
using System.IO;

public class VRIKBodyRuntime : ModuleRules
{
	private string ResourcesPath
	{
		get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../../Resources")); }
	}
	private string ThirdPartyBinPath
	{
		get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../ThirdParty/pytorch/Binaries/Win64")); }
	}

	public VRIKBodyRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "Private/VRIKBodyRuntime.h";

		PublicIncludePaths.AddRange(
            new string[] {
                Path.Combine(ModuleDirectory, "Public")
            }
        );

        PublicIncludePaths.AddRange(
            new string[] {
                "Runtime/AnimGraphRuntime/Public/BoneControllers",
                "Runtime/Engine/Public/Animation"
            }
        );

        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
                "HeadMountedDisplay",
				"AnimGraphRuntime",
                "Projects",
                "MicroPyTorch01"
			}
		);

	    PrivateDependencyModuleNames.AddRange(new string[] {  });

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// copy torch model file
			RuntimeDependencies.Add("$(BinaryOutputDir)/vrfullbodymodel.tmod", Path.Combine(ResourcesPath, "vrfullbodymodel.tmod"), StagedFileType.NonUFS);
		}
		// We can evaluate Keras model at any platfrom
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
            RuntimeDependencies.Add("$(ProjectDir)/elbowsmodel.keras", Path.Combine(ResourcesPath, "elbowsmodel_editor.keras"), StagedFileType.NonUFS);
        }
		else
		{
			RuntimeDependencies.Add("$(BinaryOutputDir)/elbowsmodel.keras", Path.Combine(ResourcesPath, "elbowsmodel_editor.keras"), StagedFileType.NonUFS);
		}
	}
}
