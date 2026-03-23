#include "SJson2AnimBPPanel.h"
#include "Json2AnimBPConverter.h"

#if __has_include("DragAndDrop/ExternalDragOperation.h")
#  include "DragAndDrop/ExternalDragOperation.h"
#  define J2ABP_HAS_EXTERNAL_DRAG 1
#else
#  define J2ABP_HAS_EXTERNAL_DRAG 0
#endif

#include "Animation/AnimBlueprint.h"
#include "AnimationGraph.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "EdGraphUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "EdGraph/EdGraphSchema.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Styling/AppStyle.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "Json2AnimBP"

// ─────────────────────────────────────────────────────────────────────────────
void SJson2AnimBPPanel::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(8.f)
        [
            SNew(SVerticalBox)

            // ── Target AnimBP ───────────────────────────────────────────
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
            [
                SAssignNew(AnimBPNameWidget, STextBlock)
                .Text(LOCTEXT("NoAnimBP", "No AnimBP open"))
                .ColorAndOpacity(FSlateColor(FLinearColor(0.6f,0.6f,0.6f)))
            ]

            // ── Drop zone (fills all available space) ───────────────────
            + SVerticalBox::Slot().FillHeight(1.f)
            [
                SAssignNew(DropBorderWidget, SBorder)
                .BorderImage(FAppStyle::GetBrush("Menu.Background"))
                .Padding(4.f)
                [
                    SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
#if J2ABP_HAS_EXTERNAL_DRAG
                        .Text(LOCTEXT("DropHint", "Drop .json here"))
#else
                        .Text(LOCTEXT("DropHintNoDrag", "Drop not available\nUse Browse or paste path below"))
#endif
                        .Justification(ETextJustify::Center)
                        .ColorAndOpacity(FSlateColor(FLinearColor(0.5f,0.5f,0.5f)))
                    ]
                ]
            ]

            // ── Browse button ───────────────────────────────────────────
            + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
            [
                SNew(SButton)
                .HAlign(HAlign_Center)
                .Text(LOCTEXT("Browse", "Browse for JSON..."))
                .OnClicked(this, &SJson2AnimBPPanel::OnBrowseClicked)
            ]

            // ── Manual path (Linux fallback) ────────────────────────────
            + SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.f)
                [
                    SAssignNew(PathInputWidget, SEditableTextBox)
                    .HintText(LOCTEXT("PathHint", "Or paste file path here..."))
                    .OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type Type)
                    {
                        if (Type == ETextCommit::OnEnter)
                        {
                            FString Path = Text.ToString().TrimStartAndEnd();
                            if (!Path.IsEmpty()) ProcessFile(Path);
                        }
                    })
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(4,0,0,0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("Load", "Load"))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (PathInputWidget.IsValid())
                        {
                            FString Path = PathInputWidget->GetText().ToString().TrimStartAndEnd();
                            if (!Path.IsEmpty()) ProcessFile(Path);
                        }
                        return FReply::Handled();
                    })
                ]
            ]

            // ── Options ─────────────────────────────────────────────────
            + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
            [
                SNew(SCheckBox)
                .IsChecked(bConnectNodes ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                .OnCheckStateChanged_Lambda([this](ECheckBoxState S){ bConnectNodes = (S == ECheckBoxState::Checked); })
                [ SNew(STextBlock).Text(LOCTEXT("ConnectNodes", "Connect nodes in chain")) ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)
            [
                SNew(SCheckBox)
                .IsChecked(bCreateInputPoseChain ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                .OnCheckStateChanged_Lambda([this](ECheckBoxState S){ bCreateInputPoseChain = (S == ECheckBoxState::Checked); })
                [ SNew(STextBlock).Text(LOCTEXT("CreateChain", "Create InputPose | LocalToComponent")) ]
            ]

            // ── Status ──────────────────────────────────────────────────
            + SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
            [
                SAssignNew(StatusTextWidget, STextBlock)
                .Text(LOCTEXT("Ready", "Ready."))
                .AutoWrapText(true)
            ]
        ]
    ];
}

// ─────────────────────────────────────────────────────────────────────────────
void SJson2AnimBPPanel::SetAnimBlueprint(UAnimBlueprint* InAnimBP)
{
    AnimBPPtr = InAnimBP;
    if (!AnimBPNameWidget.IsValid()) return;
    FText Label = InAnimBP
        ? FText::Format(LOCTEXT("TargetFmt", "Target: {0}"), FText::FromString(InAnimBP->GetName()))
        : LOCTEXT("NoAnimBP", "No AnimBP open");
    AnimBPNameWidget->SetText(Label);
    AnimBPNameWidget->SetColorAndOpacity(FSlateColor(
        InAnimBP ? FLinearColor(0.5f,1.f,0.5f) : FLinearColor(0.6f,0.6f,0.6f)));
}

UAnimBlueprint* SJson2AnimBPPanel::GetAnimBlueprint() const { return AnimBPPtr.Get(); }

// ─────────────────────────────────────────────────────────────────────────────
// Drag-and-drop
// ─────────────────────────────────────────────────────────────────────────────
bool SJson2AnimBPPanel::IsExternalJsonDrop(const FDragDropEvent& E, TArray<FString>& Out) const
{
#if J2ABP_HAS_EXTERNAL_DRAG
    TSharedPtr<FExternalDragOperation> Ext = E.GetOperationAs<FExternalDragOperation>();
    if (!Ext.IsValid()) return false;
    for (const FString& F : Ext->GetFiles())
        if (F.ToLower().EndsWith(TEXT(".json"))) Out.Add(F);
    return Out.Num() > 0;
#else
    return false;
#endif
}

void   SJson2AnimBPPanel::OnDragEnter(const FGeometry&, const FDragDropEvent& E)
{
    TArray<FString> F;
    if (IsExternalJsonDrop(E, F) && DropBorderWidget.IsValid())
        DropBorderWidget->SetBorderBackgroundColor(FLinearColor(0.2f,0.6f,0.2f,0.5f));
}
void   SJson2AnimBPPanel::OnDragLeave(const FDragDropEvent&)
{
    if (DropBorderWidget.IsValid()) DropBorderWidget->SetBorderBackgroundColor(FLinearColor::White);
}
FReply SJson2AnimBPPanel::OnDragOver(const FGeometry&, const FDragDropEvent& E)
{
    TArray<FString> F; return IsExternalJsonDrop(E, F) ? FReply::Handled() : FReply::Unhandled();
}
FReply SJson2AnimBPPanel::OnDrop(const FGeometry&, const FDragDropEvent& E)
{
    if (DropBorderWidget.IsValid()) DropBorderWidget->SetBorderBackgroundColor(FLinearColor::White);
    TArray<FString> Files;
    if (!IsExternalJsonDrop(E, Files)) return FReply::Unhandled();
    ProcessFile(Files[0]);
    return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
FReply SJson2AnimBPPanel::OnBrowseClicked()
{
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) return FReply::Handled();
    TArray<FString> Files;
    void* Wnd = FSlateApplication::Get().GetActiveTopLevelWindow().IsValid()
        ? FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle()
        : nullptr;
    if (DP->OpenFileDialog(Wnd, TEXT("Select JSON File"), TEXT(""), TEXT(""),
                           TEXT("JSON Files|*.json"), EFileDialogFlags::None, Files) && Files.Num() > 0)
    {
        if (PathInputWidget.IsValid()) PathInputWidget->SetText(FText::FromString(Files[0]));
        ProcessFile(Files[0]);
    }
    return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
void SJson2AnimBPPanel::ProcessFile(const FString& FilePath)
{
    FString Clean = FilePath.TrimStartAndEnd();
    if (Clean.StartsWith(TEXT("\"")) && Clean.EndsWith(TEXT("\"")))
        Clean = Clean.Mid(1, Clean.Len() - 2);

    if (!FPaths::FileExists(Clean))
    {
        SetStatus(FString::Printf(TEXT("Error: file not found:\n%s"), *Clean), true);
        return;
    }

    SetStatus(TEXT("Converting..."));
    FString NodesText = FJson2AnimBPConverter::Convert(Clean, TEXT(""), bConnectNodes);
    if (NodesText.IsEmpty())
    {
        SetStatus(FString::Printf(TEXT("Error: %s"), *FJson2AnimBPConverter::LastError), true);
        return;
    }

    if (!ApplyNodesToAnimBP(NodesText)) return;

    int32 NodeCount = 0, Pos = 0;
    while (true)
    {
        int32 Idx = NodesText.Find(TEXT("Begin Object"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);
        if (Idx == INDEX_NONE) break;
        ++NodeCount; Pos = Idx + 1;
    }
    SetStatus(FString::Printf(TEXT("Done - %d node(s) applied."), NodeCount));
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: first non-hidden pin with given direction (optional name preference)
// ─────────────────────────────────────────────────────────────────────────────
static UEdGraphPin* FindPin(UEdGraphNode* Node, EEdGraphPinDirection Dir,
                             const FString& PreferName = TEXT(""))
{
    if (!Node) return nullptr;
    if (!PreferName.IsEmpty())
        for (UEdGraphPin* P : Node->Pins)
            if (P && !P->bHidden && P->Direction == Dir && P->PinName.ToString() == PreferName)
                return P;
    for (UEdGraphPin* P : Node->Pins)
        if (P && !P->bHidden && P->Direction == Dir) return P;
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
bool SJson2AnimBPPanel::ApplyNodesToAnimBP(const FString& NodesText)
{
    // ── Resolve AnimBP ─────────────────────────────────────────────────
    if (!AnimBPPtr.IsValid())
    {
        if (GEditor)
            if (auto* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
                for (UObject* A : Sub->GetAllEditedAssets())
                    if (UAnimBlueprint* ABP = Cast<UAnimBlueprint>(A))
                    { AnimBPPtr = ABP; SetAnimBlueprint(ABP); break; }
    }
    UAnimBlueprint* AnimBP = AnimBPPtr.Get();
    if (!AnimBP) { SetStatus(TEXT("Error: No Animation Blueprint is open."), true); return false; }

    // ── Find AnimGraph ─────────────────────────────────────────────────
    UAnimationGraph* AnimGraph = nullptr;
    for (UEdGraph* G : AnimBP->FunctionGraphs)   { AnimGraph = Cast<UAnimationGraph>(G); if (AnimGraph) break; }
    if (!AnimGraph)
        for (UEdGraph* G : AnimBP->UbergraphPages) { AnimGraph = Cast<UAnimationGraph>(G); if (AnimGraph) break; }
    if (!AnimGraph) { SetStatus(TEXT("Error: AnimGraph not found."), true); return false; }

    // ── Transaction ────────────────────────────────────────────────────
    const FScopedTransaction Tx(LOCTEXT("Import", "Json2AnimBP: Import Nodes"));
    AnimBP->Modify();
    AnimGraph->Modify();

    // ── Import ─────────────────────────────────────────────────────────
    TSet<UEdGraphNode*> Imported;
    FEdGraphUtilities::ImportNodesFromText(AnimGraph, NodesText, Imported);
    if (Imported.IsEmpty())
    {
        SetStatus(TEXT("Warning: 0 nodes imported. Are required plugins enabled?"), true);
        return false;
    }

    // ── PostPaste + Reconstruct ────────────────────────────────────────
    for (UEdGraphNode* N : Imported) { N->PostPasteNode(); N->ReconstructNode(); }

    // ── Sort imported by X then Y ──────────────────────────────────────
    TArray<UEdGraphNode*> Chain = Imported.Array();
    Chain.Sort([](const UEdGraphNode& A, const UEdGraphNode& B)
    {
        return A.NodePosX != B.NodePosX ? A.NodePosX < B.NodePosX : A.NodePosY < B.NodePosY;
    });
    UEdGraphNode* FirstImported = Chain[0];
    UEdGraphNode* LastImported  = Chain.Last();

    const UEdGraphSchema* Schema = AnimGraph->GetSchema();

    // ── Optionally create InputPose + LocalToComponentSpace ────────────
    if (bCreateInputPoseChain)
    {
        // Place InputPose to the left of the first imported node
        const int32 InputPoseX = FirstImported->NodePosX - 400;
        const int32 InputPoseY = FirstImported->NodePosY;

        // Create LinkedInputPose
        UAnimGraphNode_LinkedInputPose* InputPoseNode =
            NewObject<UAnimGraphNode_LinkedInputPose>(AnimGraph);
        InputPoseNode->NodePosX = InputPoseX;
        InputPoseNode->NodePosY = InputPoseY;
        AnimGraph->AddNode(InputPoseNode, /*bFromUI=*/false, /*bSelectNewNode=*/false);
        InputPoseNode->PostPlacedNewNode();
        InputPoseNode->AllocateDefaultPins();
        InputPoseNode->NodeGuid = FGuid::NewGuid();

        // Create LocalToComponentSpace between InputPose and first imported
        UAnimGraphNode_LocalToComponentSpace* L2CNode =
            NewObject<UAnimGraphNode_LocalToComponentSpace>(AnimGraph);
        L2CNode->NodePosX = InputPoseX + 200;
        L2CNode->NodePosY = InputPoseY;
        AnimGraph->AddNode(L2CNode, false, false);
        L2CNode->PostPlacedNewNode();
        L2CNode->AllocateDefaultPins();
        L2CNode->NodeGuid = FGuid::NewGuid();

        // InputPose (out) → LocalToComponentSpace (in)
        UEdGraphPin* IPOut  = FindPin(InputPoseNode, EGPD_Output);
        UEdGraphPin* L2CIn  = FindPin(L2CNode,       EGPD_Input);
        if (IPOut && L2CIn) Schema->TryCreateConnection(IPOut, L2CIn);

        // LocalToComponentSpace (out) → first imported node (in)
        UEdGraphPin* L2COut     = FindPin(L2CNode,       EGPD_Output);
        UEdGraphPin* FirstIn    = FindPin(FirstImported,  EGPD_Input, TEXT("ComponentPose"));
        if (L2COut && FirstIn) Schema->TryCreateConnection(L2COut, FirstIn);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
void SJson2AnimBPPanel::SetStatus(const FString& Msg, bool bError)
{
    if (!StatusTextWidget.IsValid()) return;
    StatusTextWidget->SetText(FText::FromString(Msg));
    StatusTextWidget->SetColorAndOpacity(FSlateColor(
        bError ? FLinearColor(1.f,0.4f,0.4f) : FLinearColor(0.8f,0.8f,0.8f)));
}

#undef LOCTEXT_NAMESPACE
