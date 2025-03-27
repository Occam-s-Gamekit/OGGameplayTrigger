/// Copyright Occam's Gamekit contributors 2025


#include "OGGameplayTriggerSubsystem.h"

#include "OGAsyncUtils.h"

FOGTriggerListenerData::FOGTriggerListenerData(const FGameplayTag& InTriggerType, EOGTriggerListenerPhases InListenerPhases,
                                               const FOGTriggerDelegate& InCallback, const UObject* FilterInstigatorObject, const UObject* FilterTargetObject) :
	ListenerPhases(InListenerPhases),
	InstigatorObject(FilterInstigatorObject),
	TargetObject(FilterTargetObject),
	Callback(InCallback)
{
	TriggerType = InTriggerType;
	bFilterOnInstigator = FilterInstigatorObject != nullptr;
	bFilterOnTarget = FilterTargetObject != nullptr;
}

bool FOGTriggerListenerData::IsListenerValid() const
{
	if (!Callback.IsBound()) [[unlikely]]
		return false;
	if (bFilterOnInstigator && !InstigatorObject.IsValid()) [[unlikely]]
		return false;
	if (bFilterOnTarget && !TargetObject.IsValid()) [[unlikely]]
		return false;
	//TODO check validity of filters
	return true;
}

bool FOGTriggerListenerData::ShouldListenerProcessTrigger(EOGTriggerListenerPhases TriggerPhase, const UOGGameplayTriggerContext* Trigger) const
{
	if (!(ListenerPhases & TriggerPhase))
		return false;
	if (bFilterOnInstigator && Trigger->InitiatorObject != InstigatorObject.Get())
		return false;
	if (bFilterOnTarget && Trigger->TargetObject != TargetObject.Get())
		return false;
	
	//TODO - check additional filters
	return true;
}

UOGGameplayTriggerSubsystem* UOGGameplayTriggerSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject->GetWorld();
	if (!ensure(World))
		return nullptr;
	return World->GetSubsystem<UOGGameplayTriggerSubsystem>();
}

UOGGameplayTriggerContext* UOGGameplayTriggerSubsystem::MakeGameplayTriggerContext(const FGameplayTag& TriggerType, const FGameplayTagContainer& TriggerTags, UObject* Initiator,
	UObject* Target)
{
	UOGGameplayTriggerContext* NewTrigger = NewObject<UOGGameplayTriggerContext>(this);
	NewTrigger->TriggerType = TriggerType;
	NewTrigger->TriggerTags = TriggerTags;
	NewTrigger->InitiatorObject = Initiator;
	NewTrigger->TargetObject = Target;
	return NewTrigger;
}

UOGGameplayTriggerContext* UOGGameplayTriggerSubsystem::GetTriggerContextForUpdate(const FOGGameplayTriggerHandle& Handle)
{
	if (!Handle.IsValid())
		return nullptr;
	const TriggerMap* TriggerMap = ActiveTriggersByType.Find(Handle.TriggerType);
	if (!TriggerMap)
		return nullptr;
	const TStrongObjectPtr<UOGGameplayTriggerContext>* ContextPtrPtr = TriggerMap->Find(Handle);
	if (!ContextPtrPtr || !ContextPtrPtr->IsValid())
		return nullptr;
	
}

void UOGGameplayTriggerSubsystem::UpdateTrigger(const FOGGameplayTriggerHandle& Handle, const UOGGameplayTriggerContext* UpdatedTriggerContext)
{
}

FOGTriggerListenerHandle UOGGameplayTriggerSubsystem::RegisterTriggerListener(const FGameplayTag& TriggerType, const EOGTriggerListenerPhases Phases,
                                                                              const FOGTriggerDelegate& Delegate, const UObject* FilterInstigator, const UObject* FilterTarget,
                                                                              const bool bShouldFireForExistingTriggers, TOGFuture<void>* OutWhenListenerRemoved)
{
	if (!ensure(Delegate.IsBound()))
		return FOGHandleBase::EmptyHandle<FOGTriggerListenerHandle>();
	ensureMsgf(!bShouldFireForExistingTriggers || !!(Phases & EOGTriggerListenerPhases::TriggerStart), TEXT("If using bShouldFireForExisitngTriggers, you must respond to TriggerStart"));

	const TSharedRef<FOGTriggerListenerData> ListenerData = MakeShared<FOGTriggerListenerData>(TriggerType, Phases, Delegate, FilterInstigator, FilterTarget);
	FOGTriggerListenerHandle Handle = CreateNewListenerHandle(TriggerType);
	if (OutWhenListenerRemoved)
	{
		*OutWhenListenerRemoved = ListenerData->WhenListenerRemoved;
	}

	if (bShouldFireForExistingTriggers && ActiveTriggersByType.Contains(TriggerType))
	{
		TriggerMap& Triggers = ActiveTriggersByType.FindChecked(TriggerType);
		for (auto& [TriggerHandle,Trigger] : Triggers)
		{
			if (ListenerData->ShouldListenerProcessTrigger(EOGTriggerListenerPhases::TriggerStart, Trigger.Get()))
			{
				(void)Delegate.ExecuteIfBound(TriggerHandle, EOGTriggerListenerPhases::TriggerStart, Trigger.Get());
			}
		}
	}
	
	ListenersPendingAdd.Add(Handle, ListenerData);
	
	return Handle;
}

void UOGGameplayTriggerSubsystem::RemoveTriggerListener(const FOGTriggerListenerHandle& Handle)
{
	if (!Handle.IsValid())
		return;
	ListenersPendingRemove.Add(Handle);
}

FOGGameplayTriggerHandle UOGGameplayTriggerSubsystem::InstantaneousTrigger(UOGGameplayTriggerContext* TriggerContext)
{
	return StartTrigger_Internal(TriggerContext, EOGTriggerOperationFlags::InstantaneousTrigger);
}

FOGGameplayTriggerHandle UOGGameplayTriggerSubsystem::InstantaneousTriggerImplicitContext(const FGameplayTag& TriggerType, const FGameplayTagContainer& TriggerTags,
	UObject* Initiator, UObject* Target)
{
	return StartTrigger_Internal(MakeGameplayTriggerContext(TriggerType, TriggerTags, Initiator, Target), EOGTriggerOperationFlags::InstantaneousTrigger);
}

FOGGameplayTriggerHandle UOGGameplayTriggerSubsystem::StartTrigger(UOGGameplayTriggerContext* TriggerContext)
{
	return StartTrigger_Internal(TriggerContext, EOGTriggerOperationFlags::OpenTrigger);
}

FOGGameplayTriggerHandle UOGGameplayTriggerSubsystem::StartTriggerImplicitContext(const FGameplayTag& TriggerType, const FGameplayTagContainer& TriggerTags, UObject* Initiator,
	UObject* Target)
{
	return StartTrigger_Internal(MakeGameplayTriggerContext(TriggerType, TriggerTags, Initiator, Target), EOGTriggerOperationFlags::OpenTrigger);
}

FOGGameplayTriggerHandle UOGGameplayTriggerSubsystem::StartTrigger_Internal(UOGGameplayTriggerContext* TriggerContext, EOGTriggerOperationFlags Operations)
{
	FOGGameplayTriggerHandle Handle = CreateNewTriggerHandle(TriggerContext->TriggerType);
	const FOGPendingTriggerOperation Operation(Handle, Operations, TriggerContext);
	PendingOperations.Enqueue(Operation);
	ProcessPendingOperations();
	return Handle;
}

FOGTriggerListenerHandle UOGGameplayTriggerSubsystem::CreateNewListenerHandle(const FGameplayTag& TriggerType)
{
	FOGTriggerListenerHandle Handle = FOGHandleBase::GenerateHandle<FOGTriggerListenerHandle>();
	Handle.TriggerType = TriggerType;
	Handle.TriggerSubsystem = this;
	return Handle;
}

FOGGameplayTriggerHandle UOGGameplayTriggerSubsystem::CreateNewTriggerHandle(const FGameplayTag& TriggerType)
{
	FOGGameplayTriggerHandle Handle = FOGHandleBase::GenerateHandle<FOGGameplayTriggerHandle>();
	Handle.TriggerType = TriggerType;
	Handle.TriggerSubsystem = this;
	return Handle;
}

void UOGGameplayTriggerSubsystem::EndTrigger(const FOGGameplayTriggerHandle& Handle)
{
	const FOGPendingTriggerOperation Operation(Handle, EOGTriggerOperationFlags::CloseTrigger);
	PendingOperations.Enqueue(Operation);
	ProcessPendingOperations();
}

bool UOGGameplayTriggerSubsystem::IsTriggerActive(const FOGGameplayTriggerHandle& Handle)
{
	if (!Handle.IsValid())
		return false;
	const TriggerMap* TriggerMap = ActiveTriggersByType.Find(Handle.TriggerType);
	if (!TriggerMap)
		return false;
	return TriggerMap->Contains(Handle);
}

bool UOGGameplayTriggerSubsystem::IsListenerHandleValid(const FOGTriggerListenerHandle& Handle)
{
	// If the listener is about to be removed, it's invalid
	if (ListenersPendingRemove.Contains(Handle))
		return false;
	// If the listener is about to be added (and not immediately removed) it's valid
	if (ListenersPendingAdd.ContainsByHash(GetTypeHash(Handle), Handle))
		return true;

	// Otherwise, it's valid iff we can find it in the Listeners map
	const ListenerMap* ListenerMap = ListenersByType.Find(Handle.TriggerType);
	if (!ListenerMap)
		return false;
	return ListenerMap->Contains(Handle);
}

void UOGGameplayTriggerSubsystem::ProcessPendingOperations()
{
	if (bIsProcessingTrigger)
		return;
	
	bIsProcessingTrigger = true;
	ON_SCOPE_EXIT
	{
		bIsProcessingTrigger = false;
	};

	while (!PendingOperations.IsEmpty())
	{
		for (auto& [Handle,SharedListenerData] : ListenersPendingAdd)
        {
        	AddTriggerListener_Internal(Handle, SharedListenerData);
        }
        ListenersPendingAdd.Empty();
    
        for (const FOGTriggerListenerHandle& RemovedListener : ListenersPendingRemove)
        {
        	RemoveTriggerListener_Internal(RemovedListener);
        }
        ListenersPendingRemove.Empty();
        
        ProcessNextTriggerOperation();
	}
}

void UOGGameplayTriggerSubsystem::ProcessNextTriggerOperation()
{
	FOGPendingTriggerOperation TriggerOperation;
	if (!PendingOperations.Dequeue(TriggerOperation))
		return;
	UOGGameplayTriggerContext* TriggerContext;

	if (!!(TriggerOperation.Operation & EOGTriggerOperationFlags::AddActiveTrigger))
	{
		TriggerContext = TriggerOperation.NewTriggerContext.Get();
		AddActiveTrigger_Internal(TriggerOperation.Handle, TriggerContext);
	}
	else
	{
		TriggerContext = ActiveTriggersByType.FindChecked(TriggerOperation.Handle.TriggerType).FindChecked(TriggerOperation.Handle).Get();
	}
	
	if (!!(TriggerOperation.Operation & EOGTriggerOperationFlags::ProcessCallbacks))
	{
		ProcessTriggerCallbacks(TriggerOperation.Handle, EOGTriggerListenerPhases(uint8(TriggerOperation.Operation) & uint8(EOGTriggerListenerPhases::All)), TriggerContext);
	}
	if (!!(TriggerOperation.Operation & EOGTriggerOperationFlags::NetworkRPC))
	{
		//TODO: replicate via RPC, necessary for networked instantaneous triggers since they will never stay in the array long enough to replicate by value
	}
	if (!!(TriggerOperation.Operation & EOGTriggerOperationFlags::RemoveActiveTrigger))
	{
		RemoveActiveTrigger_Internal(TriggerOperation.Handle);
	}
}

void UOGGameplayTriggerSubsystem::ProcessTriggerCallbacks(const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases TriggerPhase, const UOGGameplayTriggerContext* TriggerContext)
{
	ListenerMap* Listeners = ListenersByType.Find(TriggerContext->TriggerType);
	if (!Listeners)
		return;

	for (auto& [Handle, Listener] : *Listeners)
	{
		if (Listener->IsListenerValid()) [[likely]]
		{
			if (Listener->ShouldListenerProcessTrigger(TriggerPhase, TriggerContext))
			{
				(void)Listener->Callback.ExecuteIfBound(TriggerHandle, TriggerPhase, TriggerContext);
			}
		}
		else
		{
			//If the listener is no longer valid, remove it
			ListenersPendingRemove.Add(Handle);
		}
	}
}

void UOGGameplayTriggerSubsystem::AddActiveTrigger_Internal(const FOGGameplayTriggerHandle& Handle, UOGGameplayTriggerContext* Trigger)
{
	if (IsTriggerTypeReplicated(Trigger->TriggerType))
	{
		ReplicatedTriggers.Add(Trigger);
	}
	const TStrongObjectPtr StrongTrigger(Trigger);
	ActiveTriggersByType.FindOrAdd(Trigger->TriggerType).Add(Handle, StrongTrigger);
}

void UOGGameplayTriggerSubsystem::RemoveActiveTrigger_Internal(const FOGGameplayTriggerHandle& Handle)
{
	const TStrongObjectPtr<UOGGameplayTriggerContext> TriggerBeingRemoved = ActiveTriggersByType.FindChecked(Handle.TriggerType).FindAndRemoveChecked(Handle);

	if (IsTriggerTypeReplicated(TriggerBeingRemoved->TriggerType))
	{
		ReplicatedTriggers.RemoveSwap(TriggerBeingRemoved.Get());
	}
}

void UOGGameplayTriggerSubsystem::AddTriggerListener_Internal(const FOGTriggerListenerHandle& Handle, const TSharedRef<FOGTriggerListenerData>& Listener)
{
	ListenersByType.FindOrAdd(Listener->TriggerType).Add(Handle, Listener);
}

void UOGGameplayTriggerSubsystem::RemoveTriggerListener_Internal(const FOGTriggerListenerHandle& Handle)
{
	ListenersByType.FindChecked(Handle.TriggerType).Remove(Handle);
}
