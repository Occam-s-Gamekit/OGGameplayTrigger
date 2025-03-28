/// Copyright Occam's Gamekit contributors 2025

#pragma once

#include "OGGameplayTriggerTypes.h"
#include "OGGameplayTriggerTests.generated.h"

USTRUCT(BlueprintType)
struct FTestTriggerData_Int : public FOGTriggerDataType
{
	GENERATED_BODY()

	UPROPERTY()
	int TestInt;
};