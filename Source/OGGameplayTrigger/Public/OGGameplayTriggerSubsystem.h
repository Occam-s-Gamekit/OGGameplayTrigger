/// Copyright Occam's Gamekit contributors 2025

#pragma once

#include "CoreMinimal.h"
#include "OGFuture.h"
#include "Subsystems/WorldSubsystem.h"
#include "OGGameplayTriggerTypes.h"
#include "OGGameplayTriggerSubsystem.generated.h"

DECLARE_DELEGATE_ThreeParams(FOGTriggerDelegate, const FOGGameplayTriggerHandle&, const EOGTriggerListenerPhases&, const UOGGameplayTriggerContext*)

class UOGGameplayTriggerSubsystem;

USTRUCT(BlueprintType)
struct OGGAMEPLAYTRIGGER_API FOGTriggerListenerData
{
	GENERATED_BODY()

	FOGTriggerListenerData() {}
	
	FOGTriggerListenerData(const FGameplayTag& InTriggerType, EOGTriggerListenerPhases InListenerPhases,
		const FOGTriggerDelegate& InCallback, const UObject* FilterInstigatorObject = nullptr, const UObject* FilterTargetObject = nullptr,
		const TArray<UOGGameplayTriggerFilter*>& Filters = TArray<UOGGameplayTriggerFilter*>());

	~FOGTriggerListenerData();
	
	FGameplayTag TriggerType = FGameplayTag::EmptyTag;

	EOGTriggerListenerPhases ListenerPhases = EOGTriggerListenerPhases::None;

	bool bFilterOnInstigator = false;
	TWeakObjectPtr<const UObject> InstigatorObject = nullptr;
	bool bFilterOnTarget = false;
	TWeakObjectPtr<const UObject> TargetObject = nullptr;
	
	FOGTriggerDelegate Callback;
	
	TArray<TStrongObjectPtr<UOGGameplayTriggerFilter>> FilterObjects;
	
	TOGPromise<void> WhenListenerRemoved;
	
	bool ShouldListenerProcessTrigger(EOGTriggerListenerPhases TriggerPhase, const UOGGameplayTriggerContext* Trigger, bool& bOutIsFilterStale) const;
};

/**
 * Central manager for a universal event / trigger system.
 * Triggers are primarily identified by GameplayTag, but can be further filtered based on the data in the event payload
 * Processing callbacks for an event are treated as atomic
 *	- changes to Trigger Listeners and Trigger operations are delayed until after the current callbacks are complete.
 * However, If an event listener is registered with the ShouldFireForExistingTriggers flag and
 * there are existing events that match the type / filters, then the callback will run for those immediately.
 */
UCLASS(BlueprintType)
class OGGAMEPLAYTRIGGER_API UOGGameplayTriggerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	struct FOGPendingTriggerOperation
	{
		FOGPendingTriggerOperation() {}
		FOGPendingTriggerOperation(const FOGGameplayTriggerHandle& InHandle, const EOGTriggerOperationFlags& InOperation) : Handle(InHandle), Operation(InOperation) {}
		FOGPendingTriggerOperation(const FOGGameplayTriggerHandle& InHandle, const EOGTriggerOperationFlags& InOperation, UOGGameplayTriggerContext* InTriggerContext)
			: Handle(InHandle), Operation(InOperation)
		{
			ensureMsgf(!!(Operation & (EOGTriggerOperationFlags::Op_AddActiveTrigger | EOGTriggerOperationFlags::Op_UpdateActiveTrigger)), TEXT("Only some operations support containing trigger context"));
			StoredTriggerContext = TStrongObjectPtr(InTriggerContext);
		}

		FOGGameplayTriggerHandle Handle = FOGHandleBase::EmptyHandle<FOGGameplayTriggerHandle>();
		EOGTriggerOperationFlags Operation = EOGTriggerOperationFlags::None;
		TStrongObjectPtr<UOGGameplayTriggerContext> StoredTriggerContext = nullptr;
	};

	typedef TMap<FOGTriggerListenerHandle, TSharedRef<FOGTriggerListenerData>> ListenerMap;
	typedef TMap<FOGGameplayTriggerHandle, TStrongObjectPtr<UOGGameplayTriggerContext>> TriggerMap;
public:

	static UOGGameplayTriggerSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category="GameplayTrigger", meta=(AutoCreateRefTerm="TriggerType,TriggerTags"))
	UOGGameplayTriggerContext* MakeGameplayTriggerContext(const FGameplayTag& TriggerType, const FGameplayTagContainer& TriggerTags, UObject* Initiator = nullptr, UObject* Target = nullptr);

	/// Prepares a trigger context for modification before passing to UpdateTrigger. If there is any ongoing trigger processing,
	/// this will create a deep-copy of the context for the given handle so the pending triggers are not changed before they are processed.
	/// If there is no ongoing trigger processing, this will simply return the trigger context for the given trigger.
	/// @param TriggerHandle The handle of the trigger to copy the context for
	/// @return A trigger context that can be modified and passed to UpdateTrigger
	UFUNCTION(BlueprintCallable, Category="GameplayTrigger")
	UOGGameplayTriggerContext* GetTriggerContextForUpdate(const FOGGameplayTriggerHandle& TriggerHandle);
	
	//TODO: Add filter information
	FOGTriggerListenerHandle RegisterTriggerListener(const FGameplayTag& TriggerType, EOGTriggerListenerPhases Phases, const FOGTriggerDelegate& Delegate,
		const UObject* FilterInstigator = nullptr, const UObject* FilterTarget = nullptr, const bool bShouldFireForExistingTriggers = false, const TArray<UOGGameplayTriggerFilter*>& Filters = TArray<UOGGameplayTriggerFilter*>(), TOGFuture<void>* OutWhenListenerRemoved = nullptr);

	//Convenience function to make it easier to create listeners with weak lambda callbacks
	template<typename Func UE_REQUIRES(std::is_void_v<TInvokeResult_T<Func, const FOGGameplayTriggerHandle&, const EOGTriggerListenerPhases&, const UOGGameplayTriggerContext*>>)>
	FOGTriggerListenerHandle RegisterTriggerListener(const UObject* ContextObject, const FGameplayTag& TriggerType, EOGTriggerListenerPhases Phases, Func Lambda,
		const UObject* FilterInstigator = nullptr, const UObject* FilterTarget = nullptr, const bool bShouldFireForExistingTriggers = false, const TArray<UOGGameplayTriggerFilter*>& Filters = TArray<UOGGameplayTriggerFilter*>(), TOGFuture<void>* OutWhenListenerRemoved = nullptr)
	{
		return RegisterTriggerListener(TriggerType, Phases, FOGTriggerDelegate::CreateWeakLambda(ContextObject, Lambda),
			FilterInstigator, FilterTarget, bShouldFireForExistingTriggers, Filters, OutWhenListenerRemoved);
	}
	
	void RemoveTriggerListener(const FOGTriggerListenerHandle& Handle);
	
	// Start a trigger that does not persist - Takes a TriggerContext that has been created with MakeGameplayTriggerContext
	UFUNCTION(BlueprintCallable, Category="GameplayTrigger")
	FOGGameplayTriggerHandle InstantaneousTrigger(UOGGameplayTriggerContext* TriggerContext);
	// Start a trigger that does not persist - Creates the TriggerContext internally
	UFUNCTION(BlueprintCallable, Category="GameplayTrigger", DisplayName="InstantaneousTriggerSimple", meta=(AutoCreateRefTerm="TriggerType,TriggerTags"))
	FOGGameplayTriggerHandle InstantaneousTriggerImplicitContext(const FGameplayTag& TriggerType, const FGameplayTagContainer& TriggerTags, UObject* Initiator = nullptr, UObject* Target = nullptr);

	// Start a trigger that will remain active until you call EndTrigger - Takes a TriggerContext that has been created with MakeGameplayTriggerContext
	UFUNCTION(BlueprintCallable, Category="GameplayTrigger")
	FOGGameplayTriggerHandle StartTrigger(UOGGameplayTriggerContext* TriggerContext);
	// Start a trigger that will remain active until you call EndTrigger - Creates the TriggerContext internally
	UFUNCTION(BlueprintCallable, Category="GameplayTrigger", DisplayName="StartTriggerSimple", meta=(AutoCreateRefTerm="TriggerType,TriggerTags"))
	FOGGameplayTriggerHandle StartTriggerImplicitContext(const FGameplayTag& TriggerType, const FGameplayTagContainer& TriggerTags, UObject* Initiator = nullptr, UObject* Target = nullptr);

	// Update a trigger trigger with 
	UFUNCTION(BlueprintCallable, Category="GameplayTrigger")
	void UpdateTrigger(const FOGGameplayTriggerHandle& Handle, UOGGameplayTriggerContext* UpdatedTriggerContext);
	
	// End a trigger that was started with StartTrigger
	UFUNCTION(BlueprintCallable, Category="GameplayTrigger")
	void EndTrigger(const FOGGameplayTriggerHandle& Handle);

	//Checks if the handle references an active trigger, may not be true immediately after a StartTrigger call if the trigger was placed in the pending operations queue
	bool IsTriggerActive(const FOGGameplayTriggerHandle& Handle);
	//Checks if the handle references an active trigger, or a trigger that is pending activation (while not also pending removal)
	bool IsTriggerActiveOrPending(const FOGGameplayTriggerHandle& Handle);
	//Checks if the listener referenced by that handle is listening for new trigger events
	bool IsListenerHandleValid(const FOGTriggerListenerHandle& Handle);

	virtual void Deinitialize() override;

protected:
	FOGGameplayTriggerHandle StartTrigger_Internal(UOGGameplayTriggerContext* TriggerContext, EOGTriggerOperationFlags Operations);
	
private:

	FOGTriggerListenerHandle CreateNewListenerHandle(const FGameplayTag& TriggerType);
	FOGGameplayTriggerHandle CreateNewTriggerHandle(const FGameplayTag& TriggerType);

	void EnqueueAndProcessOperation(const FOGPendingTriggerOperation& Operation);
	void ProcessTriggerOperation(const FOGPendingTriggerOperation& TriggerOperation);
	void ProcessTriggerCallbacks(const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases TriggerPhase, const UOGGameplayTriggerContext* TriggerContext);
	
	//TODO: real system for replicated event types
	static bool IsTriggerTypeReplicated(const FGameplayTag& TriggerType) { return false; }
	
	void AddActiveTrigger_Internal(const FOGGameplayTriggerHandle& Handle, UOGGameplayTriggerContext* Trigger);
	void UpdateActiveTrigger_Internal(const FOGGameplayTriggerHandle& Handle, UOGGameplayTriggerContext* Trigger);
	void RemoveActiveTrigger_Internal(const FOGGameplayTriggerHandle& Handle);

	void AddTriggerListener_Internal(const FOGTriggerListenerHandle& Handle, const TSharedRef<FOGTriggerListenerData>& Listener);
	void RemoveTriggerListener_Internal(const FOGTriggerListenerHandle& Handle);

	void EnqueueOperation(const FOGPendingTriggerOperation& Operation);
	bool PeekOperation(FOGPendingTriggerOperation*& OutOperation) const;
	void PopOperation();
	
	TMap<FGameplayTag, ListenerMap> ListenersByType;

	//Used for event replication
	//TODO: make this a fast array
	UPROPERTY()
	TArray<TObjectPtr<UOGGameplayTriggerContext>> ReplicatedTriggers;

	TMap<FGameplayTag, TriggerMap> ActiveTriggersByType;
	
	/**
	 * Data for pending operations
	 */
	ListenerMap ListenersPendingAdd;
	TSet<FOGTriggerListenerHandle> ListenersPendingRemove;

	TDoubleLinkedList<FOGPendingTriggerOperation> OperationQueue;
};
