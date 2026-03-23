#include "Json2AnimBPConverter.h"

#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Math/UnrealMathUtility.h"

FString FJson2AnimBPConverter::LastError;

// ─────────────────────────────────────────────────────────────────────────────
// Known prefixes (must match Python KNOWN_PREFIXES order)
// ─────────────────────────────────────────────────────────────────────────────
static const TArray<FString> KnownPrefixes =
{
    TEXT("AnimGraphNode_KawaiiPhysics"),
    TEXT("AnimGraphNode_ModifyBone"),
    TEXT("AnimGraphNode_Constraint"),
    TEXT("AnimGraphNode_LayeredBoneBlend"),
    TEXT("AnimGraphNode_SpringBone"),
};

// ─────────────────────────────────────────────────────────────────────────────
// Formatter function pointer type
// ─────────────────────────────────────────────────────────────────────────────
using FFormatterFn = FString(*)(const FString&, const TSharedPtr<FJsonObject>&, int32,
                                const FString&, const FString&,
                                const FString&, const FString&,
                                const FString&, const FString&);

// ─────────────────────────────────────────────────────────────────────────────
// JSON helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace JsonHelpers
{
    static double GetFloat(const TSharedPtr<FJsonObject>& Obj, const FString& Key, double Def = 0.0)
    {
        if (!Obj.IsValid()) return Def;
        double V = Def;
        Obj->TryGetNumberField(Key, V);
        return V;
    }

    static FString GetStr(const TSharedPtr<FJsonObject>& Obj, const FString& Key, const FString& Def = TEXT(""))
    {
        if (!Obj.IsValid()) return Def;
        FString V = Def;
        Obj->TryGetStringField(Key, V);
        return V;
    }

    static bool GetBool(const TSharedPtr<FJsonObject>& Obj, const FString& Key, bool Def = false)
    {
        if (!Obj.IsValid()) return Def;
        bool V = Def;
        Obj->TryGetBoolField(Key, V);
        return V;
    }

    static FString BoolStr(bool b)  { return b ? TEXT("True")  : TEXT("False"); }
    static FString BoolLow(bool b)  { return b ? TEXT("true")  : TEXT("false"); }

    static FString GetBoneName(const TSharedPtr<FJsonObject>& Obj, const FString& BoneKey)
    {
        if (!Obj.IsValid()) return TEXT("");
        const TSharedPtr<FJsonObject>* Sub = nullptr;
        if (Obj->TryGetObjectField(BoneKey, Sub) && Sub)
            return GetStr(*Sub, TEXT("BoneName"));
        return TEXT("");
    }

    /** Load and parse a JSON array from a file. Returns false on failure. */
    static bool LoadJsonArray(const FString& Path, TArray<TSharedPtr<FJsonValue>>& OutArray)
    {
        FString Raw;
        if (!FFileHelper::LoadFileToString(Raw, *Path)) return false;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
        return FJsonSerializer::Deserialize(Reader, OutArray);
    }
}
using namespace JsonHelpers;

// ─────────────────────────────────────────────────────────────────────────────
// Basic helpers
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::FormatFloat(double Val)
{
    return FString::Printf(TEXT("%.6f"), Val);
}

FString FJson2AnimBPConverter::NewGuid()
{
    return FGuid::NewGuid().ToString(EGuidFormats::Digits).ToUpper();
}

void FJson2AnimBPConverter::GetNodePos(int32 Index, int32& OutX, int32& OutY)
{
    OutX = FMath::FloorToInt(Index / 10.0f) * 255;
    OutY = (Index % 10) * 144;
}

FString FJson2AnimBPConverter::SplitLast(const FString& Str)
{
    int32 Idx = INDEX_NONE;
    Str.FindLastChar(TEXT(':'), Idx);
    return (Idx != INDEX_NONE) ? Str.RightChop(Idx + 1) : Str;
}

// ─────────────────────────────────────────────────────────────────────────────
// Curve keys  –  mirrors format_curve_keys()
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::FormatCurveKeys(const TSharedPtr<FJsonObject>& CurveObj)
{
    if (!CurveObj.IsValid()) return TEXT("");

    const TSharedPtr<FJsonObject>* EdData = nullptr;
    if (!CurveObj->TryGetObjectField(TEXT("EditorCurveData"), EdData) || !EdData)
        return TEXT("");

    const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
    if (!(*EdData)->TryGetArrayField(TEXT("Keys"), Keys) || !Keys || Keys->IsEmpty())
        return TEXT("");

    TArray<FString> Parts;
    for (const TSharedPtr<FJsonValue>& KV : *Keys)
    {
        const TSharedPtr<FJsonObject>* K = nullptr;
        if (!KV->TryGetObject(K) || !K) continue;
        Parts.Add(FString::Printf(TEXT("(Time=%s,Value=%s)"),
            *FormatFloat(GetFloat(*K, TEXT("Time"))),
            *FormatFloat(GetFloat(*K, TEXT("Value")))));
    }
    return FString::Printf(TEXT("EditorCurveData=(Keys=(%s))"), *FString::Join(Parts, TEXT(",")));
}

// ─────────────────────────────────────────────────────────────────────────────
// Limit arrays  –  mirrors format_limits()
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::FormatLimits(const TArray<TSharedPtr<FJsonValue>>& Limits,
                                            const FString& LimitType)
{
    TArray<FString> Items;
    for (const TSharedPtr<FJsonValue>& LV : Limits)
    {
        const TSharedPtr<FJsonObject>* L = nullptr;
        if (!LV->TryGetObject(L) || !L) continue;

        const TSharedPtr<FJsonObject>* OL  = nullptr;
        const TSharedPtr<FJsonObject>* OR_ = nullptr;
        (*L)->TryGetObjectField(TEXT("OffsetLocation"), OL);
        (*L)->TryGetObjectField(TEXT("OffsetRotation"), OR_);

        auto FV = [&](const TSharedPtr<FJsonObject>* Ptr, const FString& K) -> FString
        {
            return FormatFloat(Ptr ? GetFloat(*Ptr, K) : 0.0);
        };

        Items.Add(FString::Printf(
            TEXT("(Radius=%s,Length=%s,DrivingBone=(BoneName=\"%s\"),")
            TEXT("OffsetLocation=(X=%s,Y=%s,Z=%s),")
            TEXT("OffsetRotation=(Pitch=%s,Yaw=%s,Roll=%s))"),
            *FormatFloat(GetFloat(*L, TEXT("Radius"))),
            *FormatFloat(GetFloat(*L, TEXT("Length"))),
            *GetBoneName(*L, TEXT("DrivingBone")),
            *FV(OL, TEXT("X")), *FV(OL, TEXT("Y")), *FV(OL, TEXT("Z")),
            *FV(OR_, TEXT("Pitch")), *FV(OR_, TEXT("Yaw")), *FV(OR_, TEXT("Roll"))));
    }
    return FString::Printf(TEXT("%s=(%s)"), *LimitType, *FString::Join(Items, TEXT(",")));
}

// ─────────────────────────────────────────────────────────────────────────────
// LimitsDataAsset  –  mirrors format_limits_data_asset()
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::FormatLimitsDataAsset(const TSharedPtr<FJsonObject>& Asset)
{
    if (!Asset.IsValid()) return TEXT("");
    FString ObjectPath;
    if (!Asset->TryGetStringField(TEXT("ObjectPath"), ObjectPath)) return TEXT("");
    ObjectPath = ObjectPath.Replace(TEXT(".0"), TEXT(""));

    FString ObjectName = GetStr(Asset, TEXT("ObjectName"));
    FString ShortName  = ObjectName;
    TArray<FString> Parts;
    ObjectName.ParseIntoArray(Parts, TEXT("''"));
    if (Parts.Num() >= 2) ShortName = Parts[Parts.Num() - 2];

    return FString::Printf(
        TEXT("LimitsDataAsset=\"/Script/KawaiiPhysics.KawaiiPhysicsLimitsDataAsset''%s.%s'\"'"),
        *ObjectPath, *ShortName);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pin wiring lines  –  mirrors _pin_lines()
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::PinLines(const FString& InGuid,  const FString& OutGuid,
                                        const FString& PrevKey, const FString& PrevOutGuid,
                                        const FString& NextKey, const FString& NextInGuid)
{
    FString InLinked  = (!PrevKey.IsEmpty() && !PrevOutGuid.IsEmpty())
        ? FString::Printf(TEXT("LinkedTo=(%s %s,),"), *PrevKey, *PrevOutGuid) : TEXT("");
    FString OutLinked = (!NextKey.IsEmpty() && !NextInGuid.IsEmpty())
        ? FString::Printf(TEXT("LinkedTo=(%s %s,),"), *NextKey, *NextInGuid)  : TEXT("");

    return FString::Printf(
        TEXT("   CustomProperties Pin (PinId=%s,PinName=\"ComponentPose\",Direction=\"EGPD_Input\",PinType.PinCategory=\"\",%s)\n")
        TEXT("   CustomProperties Pin (PinId=%s,PinName=\"Pose\",Direction=\"EGPD_Output\",PinType.PinCategory=\"\",%s)\n"),
        *InGuid, *InLinked, *OutGuid, *OutLinked);
}

// ─────────────────────────────────────────────────────────────────────────────
// KawaiiPhysics  –  mirrors format_kawaii_physics_node()
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::FormatKawaiiPhysicsNode(
    const FString& Key, const TSharedPtr<FJsonObject>& Node, int32 Index,
    const FString& InGuid,  const FString& OutGuid,
    const FString& PrevKey, const FString& PrevOutGuid,
    const FString& NextKey, const FString& NextInGuid)
{
    FString RootBone        = GetBoneName(Node, TEXT("RootBone"));
    FString DummyBoneLength = FormatFloat(GetFloat(Node, TEXT("DummyBoneLength")));
    FString BoneAxis        = SplitLast(GetStr(Node, TEXT("BoneForwardAxis")));
    FString CompType        = SplitLast(GetStr(Node, TEXT("BoneConstraintGlobalComplianceType")));
    FString TPDist          = FormatFloat(GetFloat(Node, TEXT("TeleportDistanceThreshold")));
    FString TPRot           = FormatFloat(GetFloat(Node, TEXT("TeleportRotationThreshold")));

    // PhysicsSettings
    FString PhysStr;
    const TSharedPtr<FJsonObject>* PS = nullptr;
    if (Node->TryGetObjectField(TEXT("PhysicsSettings"), PS) && PS)
    {
        PhysStr = FString::Printf(
            TEXT("Damping=%s,Stiffness=%s,WorldDampingLocation=%s,WorldDampingRotation=%s,Radius=%s,LimitAngle=%s"),
            *FormatFloat(GetFloat(*PS, TEXT("Damping"))),
            *FormatFloat(GetFloat(*PS, TEXT("Stiffness"))),
            *FormatFloat(GetFloat(*PS, TEXT("WorldDampingLocation"))),
            *FormatFloat(GetFloat(*PS, TEXT("WorldDampingRotation"))),
            *FormatFloat(GetFloat(*PS, TEXT("Radius"))),
            *FormatFloat(GetFloat(*PS, TEXT("LimitAngle"))));
    }

    // ExcludeBones
    FString ExcludePart;
    const TArray<TSharedPtr<FJsonValue>>* ExBones = nullptr;
    if (Node->TryGetArrayField(TEXT("ExcludeBones"), ExBones) && ExBones && !ExBones->IsEmpty())
    {
        TArray<FString> Ex;
        for (const TSharedPtr<FJsonValue>& BV : *ExBones)
        {
            const TSharedPtr<FJsonObject>* BO = nullptr;
            if (BV->TryGetObject(BO) && BO)
                Ex.Add(FString::Printf(TEXT("(BoneName=\"%s\")"), *GetStr(*BO, TEXT("BoneName"))));
        }
        ExcludePart = FString::Printf(TEXT(",ExcludeBones=(%s)"), *FString::Join(Ex, TEXT(",")));
    }

    // Curves + Limits + LimitsDataAsset
    TArray<FString> Extras;

    static const TArray<FString> CurveNames =
    {
        TEXT("DampingCurveData"),              TEXT("StiffnessCurveData"),
        TEXT("WorldDampingLocationCurveData"), TEXT("WorldDampingRotationCurveData"),
        TEXT("RadiusCurveData"),               TEXT("LimitAngleCurveData"),
        TEXT("LimitLinearCurveData"),          TEXT("GravityCurveData"),
    };
    for (const FString& CN : CurveNames)
    {
        const TSharedPtr<FJsonObject>* CO = nullptr;
        if (Node->TryGetObjectField(CN, CO) && CO)
        {
            FString S = FormatCurveKeys(*CO);
            if (!S.IsEmpty())
                Extras.Add(FString::Printf(TEXT("%s=(%s)"), *CN, *S));
        }
    }

    static const TArray<FString> LimitTypes =
        {TEXT("CapsuleLimits"), TEXT("BoxLimits"), TEXT("PlanarLimits"), TEXT("SphericalLimits")};
    for (const FString& LT : LimitTypes)
    {
        const TArray<TSharedPtr<FJsonValue>>* LA = nullptr;
        if (Node->TryGetArrayField(LT, LA) && LA && !LA->IsEmpty())
            Extras.Add(FormatLimits(*LA, LT));
    }

    const TSharedPtr<FJsonObject>* LDA = nullptr;
    if (Node->TryGetObjectField(TEXT("LimitsDataAsset"), LDA) && LDA)
    {
        FString S = FormatLimitsDataAsset(*LDA);
        if (!S.IsEmpty()) Extras.Add(S);
    }

    FString Extra = Extras.Num() > 0 ? TEXT(",") + FString::Join(Extras, TEXT(",")) : TEXT("");
    int32 PX, PY; GetNodePos(Index, PX, PY);

    FString Out;
    Out += FString::Printf(TEXT("Begin Object Class=/Script/KawaiiPhysicsEd.AnimGraphNode_KawaiiPhysics Name=\"%s\"\n"), *Key);
    Out += FString::Printf(
        TEXT("   Node=(RootBone=(BoneName=\"%s\")%s,DummyBoneLength=%s,BoneForwardAxis=%s,")
        TEXT("TeleportDistanceThreshold=%s,TeleportRotationThreshold=%s,")
        TEXT("BoneConstraintGlobalComplianceType=%s,PhysicsSettings=(%s)%s)\n"),
        *RootBone, *ExcludePart, *DummyBoneLength, *BoneAxis,
        *TPDist, *TPRot, *CompType, *PhysStr, *Extra);
    Out += TEXT("   ShowPinForProperties(0)=(PropertyName=\"ComponentPose\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(1)=(PropertyName=\"bAlphaBoolEnabled\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(2)=(PropertyName=\"Alpha\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(3)=(PropertyName=\"AlphaCurveName\",bShowPin=True)\n");
    if (!InGuid.IsEmpty()) Out += PinLines(InGuid, OutGuid, PrevKey, PrevOutGuid, NextKey, NextInGuid);
    Out += FString::Printf(TEXT("   NodePosX=%d\n   NodePosY=%d\n"), PX, PY);
    Out += FString::Printf(TEXT("   NodeGuid=%s\n"), *NewGuid());
    Out += TEXT("End Object\n");
    return Out;
}

// ─────────────────────────────────────────────────────────────────────────────
// ModifyBone  –  mirrors format_modify_bone_node()
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::FormatModifyBoneNode(
    const FString& Key, const TSharedPtr<FJsonObject>& Node, int32 Index,
    const FString& InGuid,  const FString& OutGuid,
    const FString& PrevKey, const FString& PrevOutGuid,
    const FString& NextKey, const FString& NextInGuid)
{
    FString Bone = GetBoneName(Node, TEXT("BoneToModify"));

    auto GetSub = [&](const FString& K) -> TSharedPtr<FJsonObject>
    {
        const TSharedPtr<FJsonObject>* S = nullptr;
        if (Node->TryGetObjectField(K, S) && S) return *S;
        return nullptr;
    };
    TSharedPtr<FJsonObject> T = GetSub(TEXT("Translation"));
    TSharedPtr<FJsonObject> R = GetSub(TEXT("Rotation"));
    TSharedPtr<FJsonObject> S = GetSub(TEXT("Scale"));

    int32 PX, PY; GetNodePos(Index, PX, PY);

    FString Out;
    Out += FString::Printf(TEXT("Begin Object Class=/Script/AnimGraph.AnimGraphNode_ModifyBone Name=\"%s\"\n"), *Key);
    Out += FString::Printf(
        TEXT("   Node=(BoneToModify=(BoneName=\"%s\"),")
        TEXT("Translation=(X=%s,Y=%s,Z=%s),")
        TEXT("Rotation=(Pitch=%s,Yaw=%s,Roll=%s),")
        TEXT("Scale=(X=%s,Y=%s,Z=%s),")
        TEXT("TranslationMode=%s,RotationMode=%s,ScaleMode=%s,")
        TEXT("TranslationSpace=%s,RotationSpace=%s,ScaleSpace=%s)\n"),
        *Bone,
        *FormatFloat(GetFloat(T,TEXT("X"))),     *FormatFloat(GetFloat(T,TEXT("Y"))),     *FormatFloat(GetFloat(T,TEXT("Z"))),
        *FormatFloat(GetFloat(R,TEXT("Pitch"))), *FormatFloat(GetFloat(R,TEXT("Yaw"))),   *FormatFloat(GetFloat(R,TEXT("Roll"))),
        *FormatFloat(GetFloat(S,TEXT("X"))),     *FormatFloat(GetFloat(S,TEXT("Y"))),     *FormatFloat(GetFloat(S,TEXT("Z"))),
        *SplitLast(GetStr(Node,TEXT("TranslationMode"))),
        *SplitLast(GetStr(Node,TEXT("RotationMode"))),
        *SplitLast(GetStr(Node,TEXT("ScaleMode"))),
        *SplitLast(GetStr(Node,TEXT("TranslationSpace"))),
        *SplitLast(GetStr(Node,TEXT("RotationSpace"))),
        *SplitLast(GetStr(Node,TEXT("ScaleSpace"))));
    Out += TEXT("   ShowPinForProperties(0)=(PropertyName=\"ComponentPose\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(1)=(PropertyName=\"bAlphaBoolEnabled\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(2)=(PropertyName=\"Alpha\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(3)=(PropertyName=\"AlphaCurveName\",bShowPin=True)\n");
    if (!InGuid.IsEmpty()) Out += PinLines(InGuid, OutGuid, PrevKey, PrevOutGuid, NextKey, NextInGuid);
    Out += FString::Printf(TEXT("   NodePosX=%d\n   NodePosY=%d\n"), PX, PY);
    Out += FString::Printf(TEXT("   NodeGuid=%s\n"), *NewGuid());
    Out += TEXT("End Object\n");
    return Out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constraint  –  mirrors format_constraint_node()
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::FormatConstraintNode(
    const FString& Key, const TSharedPtr<FJsonObject>& Node, int32 Index,
    const FString& InGuid,  const FString& OutGuid,
    const FString& PrevKey, const FString& PrevOutGuid,
    const FString& NextKey, const FString& NextInGuid)
{
    FString Bone = GetBoneName(Node, TEXT("BoneToModify"));

    TArray<FString> Entries;
    const TArray<TSharedPtr<FJsonValue>>* Setup = nullptr;
    if (Node->TryGetArrayField(TEXT("ConstraintSetup"), Setup) && Setup)
    {
        for (const TSharedPtr<FJsonValue>& CV : *Setup)
        {
            const TSharedPtr<FJsonObject>* CO = nullptr;
            if (!CV->TryGetObject(CO) || !CO) continue;
            FString TBone  = GetBoneName(*CO, TEXT("TargetBone"));
            FString Offset = SplitLast(GetStr(*CO, TEXT("OffsetOption")));
            FString TType  = SplitLast(GetStr(*CO, TEXT("TransformType")));
            const TSharedPtr<FJsonObject>* PA = nullptr;
            bool bX=false, bY=false, bZ=false;
            if ((*CO)->TryGetObjectField(TEXT("PerAxis"), PA) && PA)
            {
                (*PA)->TryGetBoolField(TEXT("bX"), bX);
                (*PA)->TryGetBoolField(TEXT("bY"), bY);
                (*PA)->TryGetBoolField(TEXT("bZ"), bZ);
            }
            // Note: Python uses .lower() for PerAxis booleans
            Entries.Add(FString::Printf(
                TEXT("(TargetBone=(BoneName=\"%s\"),OffsetOption=%s,TransformType=%s,")
                TEXT("PerAxis=(bX=%s,bY=%s,bZ=%s))"),
                *TBone, *Offset, *TType,
                *BoolLow(bX), *BoolLow(bY), *BoolLow(bZ)));
        }
    }

    TArray<FString> Wts;
    const TArray<TSharedPtr<FJsonValue>>* WA = nullptr;
    if (Node->TryGetArrayField(TEXT("ConstraintWeights"), WA) && WA)
    {
        for (const TSharedPtr<FJsonValue>& WV : *WA)
        { double W=0.0; WV->TryGetNumber(W); Wts.Add(FormatFloat(W)); }
    }

    int32 PX, PY; GetNodePos(Index, PX, PY);

    FString Out;
    Out += FString::Printf(TEXT("Begin Object Class=/Script/AnimGraph.AnimGraphNode_Constraint Name=\"%s\"\n"), *Key);
    Out += FString::Printf(
        TEXT("   Node=(BoneToModify=(BoneName=\"%s\"),ConstraintSetup=(%s),ConstraintWeights=(%s))\n"),
        *Bone, *FString::Join(Entries, TEXT(",")), *FString::Join(Wts, TEXT(",")));
    Out += TEXT("   ShowPinForProperties(0)=(PropertyName=\"ComponentPose\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(1)=(PropertyName=\"bAlphaBoolEnabled\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(2)=(PropertyName=\"Alpha\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(3)=(PropertyName=\"AlphaCurveName\",bShowPin=True)\n");
    if (!InGuid.IsEmpty()) Out += PinLines(InGuid, OutGuid, PrevKey, PrevOutGuid, NextKey, NextInGuid);
    Out += FString::Printf(TEXT("   NodePosX=%d\n   NodePosY=%d\n"), PX, PY);
    Out += FString::Printf(TEXT("   NodeGuid=%s\n"), *NewGuid());
    Out += TEXT("End Object\n");
    return Out;
}

// ─────────────────────────────────────────────────────────────────────────────
// LayeredBoneBlend  –  mirrors format_layered_bone_blend_node()
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::FormatLayeredBoneBlendNode(
    const FString& Key, const TSharedPtr<FJsonObject>& Node, int32 Index,
    const FString& InGuid,  const FString& OutGuid,
    const FString& PrevKey, const FString& PrevOutGuid,
    const FString& NextKey, const FString& NextInGuid)
{
    TArray<FString> Layers;
    const TArray<TSharedPtr<FJsonValue>>* LayerSetup = nullptr;
    if (Node->TryGetArrayField(TEXT("LayerSetup"), LayerSetup) && LayerSetup)
    {
        for (const TSharedPtr<FJsonValue>& LV : *LayerSetup)
        {
            const TSharedPtr<FJsonObject>* LO = nullptr;
            if (!LV->TryGetObject(LO) || !LO) continue;
            TArray<FString> Filters;
            const TArray<TSharedPtr<FJsonValue>>* BF = nullptr;
            if ((*LO)->TryGetArrayField(TEXT("BranchFilters"), BF) && BF)
            {
                for (const TSharedPtr<FJsonValue>& FV : *BF)
                {
                    const TSharedPtr<FJsonObject>* FO = nullptr;
                    if (!FV->TryGetObject(FO) || !FO) continue;
                    Filters.Add(FString::Printf(TEXT("(BoneName=\"%s\",BlendDepth=%d)"),
                        *GetStr(*FO, TEXT("BoneName")),
                        (int32)GetFloat(*FO, TEXT("BlendDepth"))));
                }
            }
            Layers.Add(FString::Printf(TEXT("(BranchFilters=(%s))"), *FString::Join(Filters, TEXT(","))));
        }
    }

    FString WeightsPart;
    const TArray<TSharedPtr<FJsonValue>>* BW = nullptr;
    if (Node->TryGetArrayField(TEXT("BlendWeights"), BW) && BW && !BW->IsEmpty())
    {
        TArray<FString> WS;
        for (const TSharedPtr<FJsonValue>& WV : *BW)
        { double W=0.0; WV->TryGetNumber(W); WS.Add(FormatFloat(W)); }
        WeightsPart = TEXT(",BlendWeights=(") + FString::Join(WS, TEXT(",")) + TEXT(")");
    }

    int32 PX, PY; GetNodePos(Index, PX, PY);

    FString Out;
    Out += FString::Printf(TEXT("Begin Object Class=/Script/AnimGraph.AnimGraphNode_LayeredBoneBlend Name=\"%s\"\n"), *Key);
    Out += FString::Printf(
        TEXT("   Node=(LayerSetup=(%s),bMeshSpaceRotationBlend=%s,bMeshSpaceScaleBlend=%s,")
        TEXT("CurveBlendOption=%s,bBlendRootMotionBasedOnRootBone=%s%s)\n"),
        *FString::Join(Layers, TEXT(",")),
        *BoolStr(GetBool(Node, TEXT("bMeshSpaceRotationBlend"))),
        *BoolStr(GetBool(Node, TEXT("bMeshSpaceScaleBlend"))),
        *SplitLast(GetStr(Node, TEXT("CurveBlendOption"))),
        *BoolStr(GetBool(Node, TEXT("bBlendRootMotionBasedOnRootBone"))),
        *WeightsPart);
    if (!InGuid.IsEmpty()) Out += PinLines(InGuid, OutGuid, PrevKey, PrevOutGuid, NextKey, NextInGuid);
    Out += FString::Printf(TEXT("   NodePosX=%d\n   NodePosY=%d\n"), PX, PY);
    Out += FString::Printf(TEXT("   NodeGuid=%s\n"), *NewGuid());
    Out += TEXT("End Object\n");
    return Out;
}

// ─────────────────────────────────────────────────────────────────────────────
// SpringBone  –  mirrors format_spring_bone_node()
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::FormatSpringBoneNode(
    const FString& Key, const TSharedPtr<FJsonObject>& Node, int32 Index,
    const FString& InGuid,  const FString& OutGuid,
    const FString& PrevKey, const FString& PrevOutGuid,
    const FString& NextKey, const FString& NextInGuid)
{
    int32 PX, PY; GetNodePos(Index, PX, PY);
    auto B = [&](const FString& K){ return BoolStr(GetBool(Node, K)); };

    FString Out;
    Out += FString::Printf(TEXT("Begin Object Class=/Script/AnimGraph.AnimGraphNode_SpringBone Name=\"%s\"\n"), *Key);
    Out += FString::Printf(
        TEXT("   Node=(SpringBone=(BoneName=\"%s\"),MaxDisplacement=%s,SpringStiffness=%s,SpringDamping=%s,")
        TEXT("ErrorResetThresh=%s,bLimitDisplacement=%s,bTranslateX=%s,bTranslateY=%s,bTranslateZ=%s,")
        TEXT("bRotateX=%s,bRotateY=%s,bRotateZ=%s)\n"),
        *GetBoneName(Node, TEXT("SpringBone")),
        *FormatFloat(GetFloat(Node, TEXT("MaxDisplacement"))),
        *FormatFloat(GetFloat(Node, TEXT("SpringStiffness"))),
        *FormatFloat(GetFloat(Node, TEXT("SpringDamping"))),
        *FormatFloat(GetFloat(Node, TEXT("ErrorResetThresh"))),
        *B(TEXT("bLimitDisplacement")), *B(TEXT("bTranslateX")), *B(TEXT("bTranslateY")),
        *B(TEXT("bTranslateZ")),        *B(TEXT("bRotateX")),    *B(TEXT("bRotateY")),
        *B(TEXT("bRotateZ")));
    Out += TEXT("   ShowPinForProperties(0)=(PropertyName=\"ComponentPose\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(1)=(PropertyName=\"bAlphaBoolEnabled\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(2)=(PropertyName=\"Alpha\",bShowPin=True)\n");
    Out += TEXT("   ShowPinForProperties(3)=(PropertyName=\"AlphaCurveName\",bShowPin=True)\n");
    if (!InGuid.IsEmpty()) Out += PinLines(InGuid, OutGuid, PrevKey, PrevOutGuid, NextKey, NextInGuid);
    Out += FString::Printf(TEXT("   NodePosX=%d\n   NodePosY=%d\n"), PX, PY);
    Out += FString::Printf(TEXT("   NodeGuid=%s\n"), *NewGuid());
    Out += TEXT("End Object\n");
    return Out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Formatter dispatch table
// ─────────────────────────────────────────────────────────────────────────────
static const TPair<FString, FFormatterFn> FormatterTable[] =
{
    {TEXT("AnimGraphNode_KawaiiPhysics"),    &FJson2AnimBPConverter::FormatKawaiiPhysicsNode},
    {TEXT("AnimGraphNode_ModifyBone"),       &FJson2AnimBPConverter::FormatModifyBoneNode},
    {TEXT("AnimGraphNode_Constraint"),       &FJson2AnimBPConverter::FormatConstraintNode},
    {TEXT("AnimGraphNode_LayeredBoneBlend"), &FJson2AnimBPConverter::FormatLayeredBoneBlendNode},
    {TEXT("AnimGraphNode_SpringBone"),       &FJson2AnimBPConverter::FormatSpringBoneNode},
};

static FFormatterFn FindFormatter(const FString& Key)
{
    for (const auto& Pair : FormatterTable)
        if (Key.StartsWith(Pair.Key)) return Pair.Value;
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// DetectAnimClasses  –  mirrors detect_anim_classes()
// ─────────────────────────────────────────────────────────────────────────────
TArray<FString> FJson2AnimBPConverter::DetectAnimClasses(const FString& JsonFilePath)
{
    TArray<TSharedPtr<FJsonValue>> Root;
    if (!JsonHelpers::LoadJsonArray(JsonFilePath, Root)) return {};

    TArray<FString> Candidates;
    for (const TSharedPtr<FJsonValue>& IV : Root)
    {
        const TSharedPtr<FJsonObject>* IO = nullptr;
        if (!IV->TryGetObject(IO) || !IO) continue;
        FString TypeName = GetStr(*IO, TEXT("Type"));
        const TSharedPtr<FJsonObject>* Props = nullptr;
        if (!(*IO)->TryGetObjectField(TEXT("Properties"), Props) || !Props) continue;
        for (const auto& PropPair : (*Props)->Values)
            if (FindFormatter(PropPair.Key)) { Candidates.AddUnique(TypeName); break; }
    }
    return Candidates;
}

// ─────────────────────────────────────────────────────────────────────────────
// Convert  –  mirrors convert()
// ─────────────────────────────────────────────────────────────────────────────
FString FJson2AnimBPConverter::Convert(const FString& JsonFilePath,
                                       const FString& AnimBPClass,
                                       bool           bConnectNodes)
{
    LastError.Empty();

    TArray<TSharedPtr<FJsonValue>> Root;
    if (!JsonHelpers::LoadJsonArray(JsonFilePath, Root))
    {
        LastError = TEXT("Failed to parse JSON file.");
        return TEXT("");
    }

    // Resolve target class
    FString TargetClass = AnimBPClass;
    if (TargetClass.IsEmpty())
    {
        TArray<FString> Candidates = DetectAnimClasses(JsonFilePath);
        if (Candidates.IsEmpty())
        {
            LastError = TEXT("No AnimGraph nodes found in JSON.");
            return TEXT("");
        }
        TargetClass = Candidates[0];
    }

    // Find target object
    TSharedPtr<FJsonObject> Target;
    for (const TSharedPtr<FJsonValue>& IV : Root)
    {
        const TSharedPtr<FJsonObject>* IO = nullptr;
        if (!IV->TryGetObject(IO) || !IO) continue;
        if (GetStr(*IO, TEXT("Type")) == TargetClass) { Target = *IO; break; }
    }
    if (!Target.IsValid())
    {
        LastError = FString::Printf(TEXT("Class '%s' not found in JSON."), *TargetClass);
        return TEXT("");
    }

    const TSharedPtr<FJsonObject>* Props = nullptr;
    if (!Target->TryGetObjectField(TEXT("Properties"), Props) || !Props)
    {
        LastError = TEXT("Target object has no Properties field.");
        return TEXT("");
    }

    // Collect nodes preserving JSON order
    struct FEntry { FString Key; TSharedPtr<FJsonObject> Node; FFormatterFn Fmt; };
    TArray<FEntry> Nodes;
    for (const auto& PropPair : (*Props)->Values)
    {
        FFormatterFn Fn = FindFormatter(PropPair.Key);
        if (!Fn) continue;
        const TSharedPtr<FJsonObject>* NO = nullptr;
        if (PropPair.Value->TryGetObject(NO) && NO)
            Nodes.Add({PropPair.Key, *NO, Fn});
    }

    if (Nodes.IsEmpty())
    {
        LastError = TEXT("No recognised anim nodes found in Properties.");
        return TEXT("");
    }

    // Generate GUIDs for pin wiring
    int32 N = Nodes.Num();
    TArray<FString> InGuids, OutGuids;
    for (int32 i = 0; i < N; ++i)
    {
        InGuids.Add (bConnectNodes ? NewGuid() : TEXT(""));
        OutGuids.Add(bConnectNodes ? NewGuid() : TEXT(""));
    }

    // Format nodes
    TArray<FString> Parts;
    for (int32 i = 0; i < N; ++i)
    {
        FString PrevKey     = (bConnectNodes && i > 0    ) ? Nodes[i-1].Key   : TEXT("");
        FString PrevOutGuid = (bConnectNodes && i > 0    ) ? OutGuids[i-1]    : TEXT("");
        FString NextKey     = (bConnectNodes && i < N-1  ) ? Nodes[i+1].Key   : TEXT("");
        FString NextInGuid  = (bConnectNodes && i < N-1  ) ? InGuids [i+1]    : TEXT("");

        Parts.Add(Nodes[i].Fmt(
            Nodes[i].Key, Nodes[i].Node, i,
            InGuids[i], OutGuids[i],
            PrevKey, PrevOutGuid,
            NextKey, NextInGuid));
    }
    return FString::Join(Parts, TEXT("\n"));
}
