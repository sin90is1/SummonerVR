// VR IK Body Component
// (c) Yuri N Kalinin, 2017, ykasczc@gmail.com. All right reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class VRIKBodyEditor : ModuleRules
    {
        public VRIKBodyEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
            PrivatePCHHeaderFile = "Private/VRIKBodyEditor.h";

            PublicIncludePaths.AddRange(
                new string[] {
                    Path.Combine(ModuleDirectory, "Public")
                }
            );


            PublicIncludePaths.AddRange(
                new string[] {
                    "Editor/UnrealEd/Public/Kismet2"
                }
            );

            PrivateIncludePaths.AddRange(
                new string[] {
                    "VRIKBodyEditor/Private",
                    "VRIKBodyRuntime/Private"
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "VRIKBodyRuntime",
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "AnimGraphRuntime",
                    "AnimGraph",
                    "BlueprintGraph",
                    "UnrealEd"
                }
            );
        }
    }
}