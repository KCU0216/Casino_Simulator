#include "Modules/ModuleManager.h"
#include "DealerVisualPreview.h"
#include "CasinoMenuVisualPreview.h"

class FCasinoDealerEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        RegisterDealerVisualPreviewCallbacks();
        RegisterCasinoMenuPreviewCommands();
    }
    virtual void ShutdownModule() override
    {
        UnregisterCasinoMenuPreviewCommands();
        UnregisterDealerVisualPreviewCallbacks();
    }
};

IMPLEMENT_MODULE(FCasinoDealerEditorModule, CasinoDealerEditor)
