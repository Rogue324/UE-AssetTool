#include "AssetToolModule.h"

#include "AssetToolsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Factories/PrimaryAssetLabelFactory.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAssetTools.h"
#include "Misc/LexFromString.h"
#include "Modules/ModuleManager.h"
#include "PrimaryAssetLabel.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SVerticalBox.h"
#include "Widgets/SWindow.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FAssetToolModule"

namespace
{
    constexpr TCHAR AssetMenuName[] = TEXT("ContentBrowser.AssetContextMenu");
    constexpr TCHAR FolderMenuName[] = TEXT("ContentBrowser.FolderContextMenu");

    FString MakeSafeAssetName(const FString& BaseName)
    {
        FString SafeName = BaseName;
        SafeName.ReplaceInline(TEXT("/"), TEXT("_"));
        SafeName.ReplaceInline(TEXT("."), TEXT("_"));
        SafeName.ReplaceInline(TEXT("-"), TEXT("_"));
        return SafeName;
    }
}

void FAssetToolModule::StartupModule()
{
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FAssetToolModule::RegisterMenus));
}

void FAssetToolModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);

    if (UToolMenus::IsToolMenusAvailable())
    {
        UToolMenus::UnregisterOwner(this);
    }
}

void FAssetToolModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu(AssetMenuName);
    FToolMenuSection& AssetSection = AssetMenu->FindOrAddSection("AssetTool");
    AssetSection.AddDynamicEntry("AssetTool_SetChunkIdAsset", FNewToolMenuSectionDelegate::CreateRaw(this, &FAssetToolModule::AddAssetMenuEntry));

    UToolMenu* FolderMenu = UToolMenus::Get()->ExtendMenu(FolderMenuName);
    FToolMenuSection& FolderSection = FolderMenu->FindOrAddSection("AssetTool");
    FolderSection.AddDynamicEntry("AssetTool_SetChunkIdFolder", FNewToolMenuSectionDelegate::CreateRaw(this, &FAssetToolModule::AddFolderMenuEntry));
}

void FAssetToolModule::AddAssetMenuEntry(FToolMenuSection& Section)
{
    const UContentBrowserAssetContextMenuContext* Context = Section.FindContext<UContentBrowserAssetContextMenuContext>();
    if (!Context || Context->SelectedAssets.IsEmpty())
    {
        return;
    }

    const TArray<FAssetData> SelectedAssets = Context->SelectedAssets;
    Section.AddMenuEntry(
        "AssetTool_SetChunkId",
        LOCTEXT("AssetMenu_SetChunkId", "Set Cook Chunk ID"),
        LOCTEXT("AssetMenu_SetChunkId_Tooltip", "Assign selected assets to a cook chunk ID package via PrimaryAssetLabel."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Adjust"),
        FToolUIActionChoice(FExecuteAction::CreateRaw(this, &FAssetToolModule::SetChunkIdForAssets, SelectedAssets)));
}

void FAssetToolModule::AddFolderMenuEntry(FToolMenuSection& Section)
{
    const UContentBrowserDataMenuContext_FolderMenu* Context = Section.FindContext<UContentBrowserDataMenuContext_FolderMenu>();
    if (!Context || Context->SelectedPackagePaths.IsEmpty())
    {
        return;
    }

    TArray<FString> SelectedFolders;
    SelectedFolders.Reserve(Context->SelectedPackagePaths.Num());
    for (const FName PackagePath : Context->SelectedPackagePaths)
    {
        SelectedFolders.Add(PackagePath.ToString());
    }

    Section.AddMenuEntry(
        "AssetTool_SetChunkIdFolder",
        LOCTEXT("FolderMenu_SetChunkId", "Set Cook Chunk ID"),
        LOCTEXT("FolderMenu_SetChunkId_Tooltip", "Assign selected folders to a cook chunk ID package via PrimaryAssetLabel."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Adjust"),
        FToolUIActionChoice(FExecuteAction::CreateRaw(this, &FAssetToolModule::SetChunkIdForFolders, SelectedFolders)));
}

void FAssetToolModule::SetChunkIdForAssets(const TArray<FAssetData>& SelectedAssets)
{
    int32 ChunkId = 0;
    if (!PromptForChunkId(ChunkId))
    {
        return;
    }

    const FString LabelFolder = TEXT("/Game/AssetToolChunkRules");
    UEditorAssetLibrary::MakeDirectory(LabelFolder);

    const FString LabelName = FString::Printf(TEXT("PAL_Chunk_%d_Assets_%u"), ChunkId, GetTypeHash(SelectedAssets.Num() > 0 ? SelectedAssets[0].GetObjectPathString() : FString()));
    UPrimaryAssetLabel* Label = CreateOrLoadLabel(LabelFolder, LabelName);
    if (!Label)
    {
        NotifyResult(LOCTEXT("CreateAssetLabelFailed", "Failed to create chunk label asset."), false);
        return;
    }

    Label->bIsRuntimeLabel = true;
    Label->bLabelAssetsInMyDirectory = false;
    Label->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
    Label->Rules.ChunkId = ChunkId;
    Label->ExplicitAssets.Reset();

    for (const FAssetData& Asset : SelectedAssets)
    {
        Label->ExplicitAssets.AddUnique(TSoftObjectPtr<UObject>(Asset.ToSoftObjectPath()));
    }

    Label->MarkPackageDirty();

    NotifyResult(FText::Format(LOCTEXT("SetAssetChunkDone", "Assigned {0} assets to Chunk ID {1}."), SelectedAssets.Num(), ChunkId), true);
}

void FAssetToolModule::SetChunkIdForFolders(const TArray<FString>& SelectedFolders)
{
    int32 ChunkId = 0;
    if (!PromptForChunkId(ChunkId))
    {
        return;
    }

    int32 SucceededCount = 0;
    for (const FString& Folder : SelectedFolders)
    {
        FString AssetName = FString::Printf(TEXT("PAL_Chunk_%d_%s"), ChunkId, *MakeSafeAssetName(Folder));
        AssetName.LeftInline(96);

        UPrimaryAssetLabel* Label = CreateOrLoadLabel(Folder, AssetName);
        if (!Label)
        {
            continue;
        }

        Label->bIsRuntimeLabel = true;
        Label->bLabelAssetsInMyDirectory = true;
        Label->Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
        Label->Rules.ChunkId = ChunkId;
        Label->ExplicitAssets.Reset();
        Label->MarkPackageDirty();
        ++SucceededCount;
    }

    NotifyResult(
        FText::Format(LOCTEXT("SetFolderChunkDone", "Assigned {0} folders to Chunk ID {1}."), SucceededCount, ChunkId),
        SucceededCount == SelectedFolders.Num());
}

bool FAssetToolModule::PromptForChunkId(int32& OutChunkId) const
{
    bool bAccepted = false;
    TSharedPtr<SEditableTextBox> ChunkIdTextBox;

    TSharedRef<SWindow> Dialog = SNew(SWindow)
        .Title(LOCTEXT("ChunkIdDialogTitle", "Set Cook Chunk ID"))
        .ClientSize(FVector2D(380.f, 120.f))
        .SupportsMinimize(false)
        .SupportsMaximize(false);

    Dialog->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.f, 12.f, 12.f, 6.f)
        [
            SAssignNew(ChunkIdTextBox, SEditableTextBox)
            .HintText(LOCTEXT("ChunkIdHint", "Input chunk ID (>=0)"))
            .Text(FText::FromString(TEXT("0")))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(12.f)
        [
            SNew(SUniformGridPanel)
            .SlotPadding(FMargin(6.f, 0.f))
            + SUniformGridPanel::Slot(0, 0)
            [
                SNew(SButton)
                .Text(LOCTEXT("ChunkIdCancel", "Cancel"))
                .OnClicked_Lambda([&Dialog]()
                {
                    Dialog->RequestDestroyWindow();
                    return FReply::Handled();
                })
            ]
            + SUniformGridPanel::Slot(1, 0)
            [
                SNew(SButton)
                .Text(LOCTEXT("ChunkIdOK", "OK"))
                .OnClicked_Lambda([&Dialog, &ChunkIdTextBox, &OutChunkId, &bAccepted]()
                {
                    if (!ChunkIdTextBox.IsValid())
                    {
                        return FReply::Handled();
                    }

                    int32 InputChunkId = 0;
                    if (!LexTryParseString(InputChunkId, *ChunkIdTextBox->GetText().ToString()) || InputChunkId < 0)
                    {
                        return FReply::Handled();
                    }

                    OutChunkId = InputChunkId;
                    bAccepted = true;
                    Dialog->RequestDestroyWindow();
                    return FReply::Handled();
                })
            ]
        ]);

    if (GEditor)
    {
        GEditor->EditorAddModalWindow(Dialog);
    }

    return bAccepted;
}

UPrimaryAssetLabel* FAssetToolModule::CreateOrLoadLabel(const FString& PackagePath, const FString& AssetName) const
{
    const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
    if (UPrimaryAssetLabel* Existing = LoadObject<UPrimaryAssetLabel>(nullptr, *ObjectPath))
    {
        return Existing;
    }

    UPrimaryAssetLabelFactory* Factory = NewObject<UPrimaryAssetLabelFactory>();
    if (!Factory)
    {
        return nullptr;
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    return Cast<UPrimaryAssetLabel>(AssetTools.CreateAsset(AssetName, PackagePath, UPrimaryAssetLabel::StaticClass(), Factory));
}

void FAssetToolModule::NotifyResult(const FText& Message, bool bSuccess) const
{
    FNotificationInfo Info(Message);
    Info.ExpireDuration = 4.0f;
    Info.bUseSuccessFailIcons = true;

    TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
    if (Notification.IsValid())
    {
        Notification->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAssetToolModule, AssetTool)
