#include "AssetToolModule.h"

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FAssetToolModule"

void FAssetToolModule::StartupModule()
{
}

void FAssetToolModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAssetToolModule, AssetTool)
