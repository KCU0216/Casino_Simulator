#include "Modules/ModuleManager.h"
#include "DealerVisualPreview.h"
#include "CasinoMenuVisualPreview.h"
#include "CasinoBlackjackUIVisualPreview.h"
#include "CasinoShopUIVisualPreview.h"

class FCasinoDealerEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        RegisterDealerVisualPreviewCallbacks();
        RegisterCasinoMenuPreviewCommands();
        RegisterCasinoBlackjackPreviewCommands();
        RegisterCasinoShopPreviewCommands();
    }
    virtual void ShutdownModule() override
    {
        UnregisterCasinoShopPreviewCommands();
        UnregisterCasinoBlackjackPreviewCommands();
        UnregisterCasinoMenuPreviewCommands();
        UnregisterDealerVisualPreviewCallbacks();
    }
};

IMPLEMENT_MODULE(FCasinoDealerEditorModule, CasinoDealerEditor)
