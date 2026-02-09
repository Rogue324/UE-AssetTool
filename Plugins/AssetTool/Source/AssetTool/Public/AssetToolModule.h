#pragma once

#include "Modules/ModuleManager.h"

class FAssetToolModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
