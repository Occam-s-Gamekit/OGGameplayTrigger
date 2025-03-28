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

	for (const FOGPendingTriggerOperation& PendingOperation : OperationQueue)
	{
		if (Handle != PendingOperation.Handle)
			continue;
		if (!ensureMsgf(!(PendingOperation.Operation & EOGTriggerOperationFlags::Op_RemoveActiveTrigger),TEXT("Trying to update a handle that is pending removal")))
			return nullptr;
		if (!!(PendingOperation.Operation & (EOGTriggerOperationFlags::Op_AddActiveTrigger | EOGTriggerOperationFlags::Op_UpdateActiveTrigger)))
		{
			ensure(PendingOperation.StoredTriggerContext.IsValid());
			//Deep copy the stored trigger context and return it so the changes don't interfere with pending/current operations
			return NewObject<UOGGameplayTriggerContext>(this, NAME_None, RF_NoFlags, PendingOperation.StoredTriggerContext.Get());
		}
	}

	const TriggerMap* TriggerMap = ActiveTriggersByType.Find(Handle.TriggerType);
	if (!ensureMsgf(TriggerMap,TEXT("Could not find active trigger for handle")))
		return nullptr;
	const TStrongObjectPtr<UOGGameplayTriggerContext>* ContextPtrPtr = TriggerMap->Find(Handle);
	if (!ensureMsgf(ContextPtrPtr && ContextPtrPtr->IsValid(), TEXT("Could not find active trigger for handle")))
		return nullptr;
	//if there is nothing pending on the handle, just return the stored trigger context for in-place modification
	return ContextPtrPtr->Get();
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

void UOGGameplayTriggerSubsystem::UpdateTrigger(const FOGGameplayTriggerHandle& Handle, UOGGameplayTriggerContext* UpdatedTriggerContext)
{
	const FOGPendingTriggerOperation Operation(Handle, EOGTriggerOperationFlags::UpdateTrigger, UpdatedTriggerContext);
	EnqueueAndProcessOperation(Operation);
}

FOGGameplayTriggerHandle UOGGameplayTriggerSubsystem::StartTrigger_Internal(UOGGameplayTriggerContext* TriggerContext, EOGTriggerOperationFlags Operations)
{
	FOGGameplayTriggerHandle Handle = CreateNewTriggerHandle(TriggerContext->TriggerType);
	const FOGPendingTriggerOperation Operation(Handle, Operations, TriggerContext);
	EnqueueAndProcessOperation(Operation);
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
	EnqueueAndProcessOperation(Operation);
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

bool UOGGameplayTriggerSubsystem::IsTriggerActiveOrPending(const FOGGameplayTriggerHandle& Handle)
{
	for (const FOGPendingTriggerOperation& PendingOperation : OperationQueue)
	{
		if (Handle != PendingOperation.Handle)
			continue;
		if (!!(PendingOperation.Operation & EOGTriggerOperationFlags::Op_RemoveActiveTrigger))
			return false;
		if (!!(PendingOperation.Operation & (EOGTriggerOperationFlags::Op_AddActiveTrigger | EOGTriggerOperationFlags::Op_UpdateActiveTrigger)))
		{
			return true;
		}
	}
	return IsTriggerActive(Handle);
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

void UOGGameplayTriggerSubsystem::EnqueueAndProcessOperation(const FOGPendingTriggerOperation& Operation)
{
	const bool bShouldProcess = OperationQueue.IsEmpty();
	EnqueueOperation(Operation);
	if (!bShouldProcess)
		return;
	
	FOGPendingTriggerOperation CurrentOperation;
	while (PeekOperation(CurrentOperation))
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
        
        ProcessTriggerOperation(CurrentOperation);
		
		PopOperation();
	}
}

void UOGGameplayTriggerSubsystem::ProcessTriggerOperation(const FOGPendingTriggerOperation& TriggerOperation)
{
	UOGGameplayTriggerContext* TriggerContext;

	if (!!(TriggerOperation.Operation & EOGTriggerOperationFlags::Op_AddActiveTrigger))
	{
		TriggerContext = TriggerOperation.StoredTriggerContext.Get();
		AddActiveTrigger_Internal(TriggerOperation.Handle, TriggerContext);
	}
	else if (!!(TriggerOperation.Operation & EOGTriggerOperationFlags::Op_UpdateActiveTrigger))
	{
		TriggerContext = TriggerOperation.StoredTriggerContext.Get();
		UpdateActiveTrigger_Internal(TriggerOperation.Handle, TriggerContext);
	}
	else
	{
		TriggerContext = ActiveTriggersByType.FindChecked(TriggerOperation.Handle.TriggerType).FindChecked(TriggerOperation.Handle).Get();
	}
	
	if (!!(TriggerOperation.Operation & EOGTriggerOperationFlags::Op_ProcessCallbacks))
	{
		ProcessTriggerCallbacks(TriggerOperation.Handle, EOGTriggerListenerPhases(uint8(TriggerOperation.Operation) & uint8(EOGTriggerListenerPhases::All)), TriggerContext);
	}
	if (!!(TriggerOperation.Operation & EOGTriggerOperationFlags::Op_NetworkRPC))
	{
		//TODO: replicate via RPC, necessary for networked instantaneous triggers since they will never stay in the array long enough to replicate by value
		//Caution! I believe you can only fire a reliable RPC once per net update, so if there are multiple instantaneous networked triggers at once it will likely cause problems.
		//I could batch the triggers but that gets to be a lot of extra work to support networked gameplay triggers, especially if I try to maintain trigger order.
		//I do want to support networked persistent triggers though
	}
	if (!!(TriggerOperation.Operation & EOGTriggerOperationFlags::Op_RemoveActiveTrigger))
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

void UOGGameplayTriggerSubsystem::UpdateActiveTrigger_Internal(const FOGGameplayTriggerHandle& Handle, UOGGameplayTriggerContext* Trigger)
{
	const TStrongObjectPtr<UOGGameplayTriggerContext> TriggerBeingModified = ActiveTriggersByType.FindChecked(Handle.TriggerType).FindChecked(Handle);
	
	//TODO: I'm not 100% sure that uobject equality check will just check if the pointers are the same
	if (TriggerBeingModified.Get() == Trigger)
	{
		//The trigger may have been modified in place
		if (IsTriggerTypeReplicated(TriggerBeingModified->TriggerType))
		{
			//TODO: find element and mark it dirty
		}
	}
	else
	{
		if (IsTriggerTypeReplicated(TriggerBeingModified->TriggerType))
		{
			ReplicatedTriggers.RemoveSwap(TriggerBeingModified.Get());
			ReplicatedTriggers.Add(Trigger);
		}
		const TStrongObjectPtr StrongTrigger(Trigger);
        ActiveTriggersByType.FindOrAdd(Trigger->TriggerType).Add(Handle, StrongTrigger);
	}
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

void UOGGameplayTriggerSubsystem::EnqueueOperation(const FOGPendingTriggerOperation& Operation)
{
	//Enqueue at Head and Dequeue at Tail so that iterating through the queue will access the last operations first
	OperationQueue.AddHead(Operation);
}

bool UOGGameplayTriggerSubsystem::PeekOperation(FOGPendingTriggerOperation& OutOperation) const
{
	if (OperationQueue.IsEmpty())
		return false;
	OutOperation = OperationQueue.GetTail()->GetValue();
	return true;
}

void UOGGameplayTriggerSubsystem::PopOperation()
{
	if (OperationQueue.IsEmpty())
		return;
	OperationQueue.RemoveNode(OperationQueue.GetTail());
}
