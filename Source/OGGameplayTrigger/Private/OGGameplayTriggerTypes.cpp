/// Copyright Occam's Gamekit contributors 2025


#include "OGGameplayTriggerTypes.h"

#include "OGGameplayTriggerSubsystem.h"
#include "Net/UnrealNetwork.h"

bool FOGGameplayTriggerHandle::IsActive() const
{
	return IsValid() && TriggerSubsystem->IsTriggerActive(*this);
}

bool FOGGameplayTriggerHandle::IsValid() const
{
	return FOGHandleBase::IsValid() && TriggerType.IsValid() && TriggerSubsystem.IsValid();
}

void FOGGameplayTriggerHandle::Reset()
{
	if (IsActive())
	{
		TriggerSubsystem->EndTrigger(*this);
	}
	TriggerType = FGameplayTag::EmptyTag;
	TriggerSubsystem.Reset();
	Super::Reset();
}


bool FOGTriggerListenerHandle::IsValid() const
{
	return FOGHandleBase::IsValid() && TriggerType.IsValid() && TriggerSubsystem.IsValid() &&
		TriggerSubsystem->IsListenerHandleValid(*this);
}

void FOGTriggerListenerHandle::Reset()
{
	if (IsValid())
	{
		TriggerSubsystem->RemoveTriggerListener(*this);
	}
	TriggerType = FGameplayTag::EmptyTag;
	TriggerSubsystem.Reset();
	FOGHandleBase::Reset();
}

void UOGGameplayTriggerContext::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, TriggerType, Params)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, TriggerTags, Params)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, InitiatorObject, Params)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, TargetObject, Params)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, AdditionalData, Params)
}
