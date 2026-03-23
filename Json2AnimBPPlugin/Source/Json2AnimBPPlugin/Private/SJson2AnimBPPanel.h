#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"

class UAnimBlueprint;

class SJson2AnimBPPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SJson2AnimBPPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetAnimBlueprint(UAnimBlueprint* InAnimBP);
    UAnimBlueprint* GetAnimBlueprint() const;

    virtual void   OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
    virtual void   OnDragLeave(const FDragDropEvent& DragDropEvent) override;
    virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
    virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
    FReply OnBrowseClicked();
    void   ProcessFile(const FString& FilePath);
    bool   ApplyNodesToAnimBP(const FString& NodesText);
    void   SetStatus(const FString& Msg, bool bError = false);
    bool   IsExternalJsonDrop(const FDragDropEvent& DragDropEvent, TArray<FString>& OutFiles) const;

    TWeakObjectPtr<UAnimBlueprint> AnimBPPtr;
    TSharedPtr<STextBlock>         StatusTextWidget;
    TSharedPtr<STextBlock>         AnimBPNameWidget;
    TSharedPtr<SBorder>            DropBorderWidget;
    TSharedPtr<SEditableTextBox>   PathInputWidget;

    bool bConnectNodes        = true;
    bool bCreateInputPoseChain = true;
};
