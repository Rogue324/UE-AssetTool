#pragma once

#include "AssetData.h"
#include "Modules/ModuleManager.h"

class FToolMenuSection;

class FAssetToolModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void AddAssetMenuEntry(FToolMenuSection& Section);
    void AddFolderMenuEntry(FToolMenuSection& Section);

    void SetChunkIdForAssets(const TArray<FAssetData>& SelectedAssets);
    void SetChunkIdForFolders(const TArray<FString>& SelectedFolders);

    bool PromptForChunkId(int32& OutChunkId) const;

    class UPrimaryAssetLabel* CreateOrLoadLabel(const FString& PackagePath, const FString& AssetName) const;
    void NotifyResult(const FText& Message, bool bSuccess) const;
};
