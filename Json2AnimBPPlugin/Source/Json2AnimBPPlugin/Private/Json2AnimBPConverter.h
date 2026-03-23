#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * FJson2AnimBPConverter
 *
 * Ports the Python Json2AnimBP conversion logic to C++.
 * Takes a JSON file path (produced by the original tool) and returns the UE
 * clipboard-format text that can be fed straight into
 * FEdGraphUtilities::ImportNodesFromText().
 *
 * Supported node types:
 *   AnimGraphNode_KawaiiPhysics   (requires KawaiiPhysics plugin)
 *   AnimGraphNode_ModifyBone
 *   AnimGraphNode_Constraint
 *   AnimGraphNode_LayeredBoneBlend
 *   AnimGraphNode_SpringBone
 */
class FJson2AnimBPConverter
{
public:
    /**
     * Scan the JSON and return all AnimBP class names whose Properties block
     * contains at least one recognised anim-graph-node key.
     */
    static TArray<FString> DetectAnimClasses(const FString& JsonFilePath);

    /**
     * Convert nodes from the given JSON file.
     *
     * @param JsonFilePath   Path to the exported JSON.
     * @param AnimBPClass    Type name to look up in the JSON array.
     *                       Pass an empty string to auto-detect the first match.
     * @param bConnectNodes  Wire consecutive nodes together with pin GUIDs.
     * @return               UE "Begin Object…End Object" clipboard text,
     *                       or an empty string on failure.
     */
    static FString Convert(const FString& JsonFilePath,
                           const FString& AnimBPClass,
                           bool           bConnectNodes = true);

    /** Last error message set during Convert(). */
    static FString LastError;

    // ── Formatting helpers ─────────────────────────────────────────────────

    static FString FormatFloat(double Val);
    static FString NewGuid();
    static void    GetNodePos(int32 Index, int32& OutX, int32& OutY);

    /** Split on ':' and return the last segment (mirrors Python split(":")[-1]). */
    static FString SplitLast(const FString& Str);

    static FString FormatCurveKeys(const TSharedPtr<FJsonObject>& CurveObj);
    static FString FormatLimits(const TArray<TSharedPtr<FJsonValue>>& Limits,
                                const FString& LimitType);
    static FString FormatLimitsDataAsset(const TSharedPtr<FJsonObject>& Asset);

    static FString PinLines(const FString& InGuid,       const FString& OutGuid,
                            const FString& PrevKey,      const FString& PrevOutGuid,
                            const FString& NextKey,      const FString& NextInGuid);

    // ── Node formatters ───────────────────────────────────────────────────

    static FString FormatKawaiiPhysicsNode(
        const FString& Key, const TSharedPtr<FJsonObject>& Node, int32 Index,
        const FString& InGuid,   const FString& OutGuid,
        const FString& PrevKey,  const FString& PrevOutGuid,
        const FString& NextKey,  const FString& NextInGuid);

    static FString FormatModifyBoneNode(
        const FString& Key, const TSharedPtr<FJsonObject>& Node, int32 Index,
        const FString& InGuid,   const FString& OutGuid,
        const FString& PrevKey,  const FString& PrevOutGuid,
        const FString& NextKey,  const FString& NextInGuid);

    static FString FormatConstraintNode(
        const FString& Key, const TSharedPtr<FJsonObject>& Node, int32 Index,
        const FString& InGuid,   const FString& OutGuid,
        const FString& PrevKey,  const FString& PrevOutGuid,
        const FString& NextKey,  const FString& NextInGuid);

    static FString FormatLayeredBoneBlendNode(
        const FString& Key, const TSharedPtr<FJsonObject>& Node, int32 Index,
        const FString& InGuid,   const FString& OutGuid,
        const FString& PrevKey,  const FString& PrevOutGuid,
        const FString& NextKey,  const FString& NextInGuid);

    static FString FormatSpringBoneNode(
        const FString& Key, const TSharedPtr<FJsonObject>& Node, int32 Index,
        const FString& InGuid,   const FString& OutGuid,
        const FString& PrevKey,  const FString& PrevOutGuid,
        const FString& NextKey,  const FString& NextInGuid);
};
