// Fill out your copyright notice in the Description page of Project Settings.


#include "OGWhenGameplayTriggerTask.h"

#include "OGGameplayTriggerSubsystem.h"

UOGWhenGameplayTriggerTask* UOGWhenGameplayTriggerTask::WhenGameplayTrigger(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, const FGameplayTag TriggerType, EOGTriggerListenerPhases TriggerPhase,
                                                                            const bool bOnce, const bool bShouldFireForExistingTriggers, const UObject* FilterInstigator, const UObject* FilterTarget)
{
	UOGWhenGameplayTriggerTask* Task = NewTask<UOGWhenGameplayTriggerTask>(TaskOwner);

	if (!Task)
		return nullptr;
	
	Task->TriggerType = TriggerType;
	Task->TriggerPhase = TriggerPhase;
	Task->bShouldFireForExistingTriggers = bShouldFireForExistingTriggers;
	Task->bOnce = bOnce;
	Task->FilterInstigator = FilterInstigator;
	Task->FilterTarget = FilterTarget;

	return Task;
}



void UOGWhenGameplayTriggerTask::OnTrigger(const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& Phases, const UOGGameplayTriggerContext* TriggerContext)
{
	When.Broadcast(TriggerHandle, TriggerContext);
	if (bOnce)
	{
		EndTask();
	}
}

void UOGWhenGameplayTriggerTask::Activate()
{
	UOGGameplayTriggerSubsystem* TriggerSubsystem = UOGGameplayTriggerSubsystem::Get(this);
	Handle = TriggerSubsystem->RegisterTriggerListener(TriggerType, TriggerPhase, FOGTriggerDelegate::CreateUObject(this, &UOGWhenGameplayTriggerTask::OnTrigger), FilterInstigator, FilterTarget, bShouldFireForExistingTriggers, &WhenListenerRemoved);
	WhenListenerRemoved->Then(TOGFuture<void>::FThenDelegate::CreateUObject(this, &UOGWhenGameplayTriggerTask::EndTask));
}

void UOGWhenGameplayTriggerTask::OnDestroy(bool bInOwnerFinished)
{
	//clear the delegate immediately in case there are other triggers pending
	When.Clear();
	//Remove the trigger listener if it hasn't been already.
	Handle.Reset();
	
	Super::OnDestroy(bInOwnerFinished);
}

UOGWhenGameplayTriggerTask_MultiPhase* UOGWhenGameplayTriggerTask_MultiPhase::WhenGameplayTrigger_MultiPhase(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner,
	const FGameplayTag TriggerType, const bool bShouldFireForExistingTriggers, const UObject* FilterInstigator, const UObject* FilterTarget)
{
	UOGWhenGameplayTriggerTask_MultiPhase* Task = NewTask<UOGWhenGameplayTriggerTask_MultiPhase>(TaskOwner);

	if (!Task)
		return nullptr;

	Task->TriggerType = TriggerType;
	Task->bShouldFireForExistingTriggers = bShouldFireForExistingTriggers;
	Task->FilterInstigator = FilterInstigator;
	Task->FilterTarget = FilterTarget;

	return Task;
}

void UOGWhenGameplayTriggerTask_MultiPhase::OnTrigger(const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& Phases, const UOGGameplayTriggerContext* TriggerContext) const
{
	if (!!(Phases & EOGTriggerListenerPhases::TriggerStart))
	{
		WhenStart.Broadcast(TriggerHandle, TriggerContext);
	}
	else if (!!(Phases & EOGTriggerListenerPhases::TriggerEnd))
	{
		WhenEnd.Broadcast(TriggerHandle, TriggerContext);
	}
	else
	{
		ensure(!!(Phases & EOGTriggerListenerPhases::TriggerUpdate));
		WhenUpdate.Broadcast(TriggerHandle, TriggerContext);
	}
}

void UOGWhenGameplayTriggerTask_MultiPhase::Activate()
{
	UOGGameplayTriggerSubsystem* TriggerSubsystem = UOGGameplayTriggerSubsystem::Get(this);
	Handle = TriggerSubsystem->RegisterTriggerListener(TriggerType, EOGTriggerListenerPhases::All, FOGTriggerDelegate::CreateUObject(this, &UOGWhenGameplayTriggerTask_MultiPhase::OnTrigger), FilterInstigator, FilterTarget, bShouldFireForExistingTriggers, &WhenListenerRemoved);
	WhenListenerRemoved->Then(TOGFuture<void>::FThenDelegate::CreateUObject(this, &UOGWhenGameplayTriggerTask::EndTask));
}

void UOGWhenGameplayTriggerTask_MultiPhase::OnDestroy(bool bInOwnerFinished)
{
	//clear the delegate immediately in case there are other triggers pending
	WhenStart.Clear();
	WhenUpdate.Clear();
	WhenEnd.Clear();
	//Remove the trigger listener if it hasn't been already.
	Handle.Reset();
	
	Super::OnDestroy(bInOwnerFinished);
}
