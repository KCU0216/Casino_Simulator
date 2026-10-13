#pragma once

#include "Commandlets/Commandlet.h"
#include "CasinoDealerSetupCommandlet.generated.h"

UCLASS()
class UCasinoDealerSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCasinoDealerSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
