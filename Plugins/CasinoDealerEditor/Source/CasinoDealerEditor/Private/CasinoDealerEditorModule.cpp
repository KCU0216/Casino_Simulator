#include "Modules/ModuleManager.h"
#include "DealerVisualPreview.h"
#include "CasinoMenuVisualPreview.h"
#include "CasinoBlackjackUIVisualPreview.h"
#include "CasinoShopUIVisualPreview.h"
#include "CasinoTradeUIVisualPreview.h"

class FCasinoDealerEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        RegisterDealerVisualPreviewCallbacks();
        RegisterCasinoMenuPreviewCommands();
        RegisterCasinoBlackjackPreviewCommands();
        RegisterCasinoShopPreviewCommands();
        RegisterCasinoTradePreviewCommands();
    }
    virtual void ShutdownModule() override
    {
        UnregisterCasinoTradePreviewCommands();
        UnregisterCasinoShopPreviewCommands();
        UnregisterCasinoBlackjackPreviewCommands();
        UnregisterCasinoMenuPreviewCommands();
        UnregisterDealerVisualPreviewCallbacks();
    }
};

IMPLEMENT_MODULE(FCasinoDealerEditorModule, CasinoDealerEditor)
