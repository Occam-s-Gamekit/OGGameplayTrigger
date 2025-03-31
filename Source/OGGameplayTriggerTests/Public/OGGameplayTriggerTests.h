/// Copyright Occam's Gamekit contributors 2025

#pragma once

#include "OGGameplayTriggerTypes.h"
#include "OGGameplayTriggerTests.generated.h"

USTRUCT(BlueprintType)
struct FTestTriggerData_Int : public FOGTriggerDataType
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int TestInt;
};

UCLASS(NotBlueprintType)
class UOGTestTriggerFilter_DataIsPositive : public UOGGameplayTriggerFilter
{
	GENERATED_BODY()

protected:
	virtual bool DoesTriggerPassFilter_Native(const EOGTriggerListenerPhases TriggerPhase, const UOGGameplayTriggerContext* Trigger, bool& OutIsFilterStale) const override;
};