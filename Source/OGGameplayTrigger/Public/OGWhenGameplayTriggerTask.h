// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTask.h"
#include "OGFuture.h"
#include "OGGameplayTriggerTypes.h"
#include "OGWhenGameplayTriggerTask.generated.h"

class UOGGameplayTriggerContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOGGameplayTriggerTaskDelegate, const FOGGameplayTriggerHandle&, TriggerHandle, const UOGGameplayTriggerContext*, TriggerContext);

/**
 * 
 */
UCLASS()
class OGGAMEPLAYTRIGGER_API UOGWhenGameplayTriggerTask : public UGameplayTask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintAssignable)
	FOGGameplayTriggerTaskDelegate When;

	UFUNCTION(BlueprintCallable, Category = "GameplayTrigger", meta = (DefaultToSelf="TaskOwner", BlueprintInternalUseOnly = "TRUE",
		GameplayTagFilter="Trigger", AutoCreateRefTerm="Filters", AdvancedDisplay="FilterInstigator,FilterTarget,Filters"))
	static UOGWhenGameplayTriggerTask* WhenGameplayTrigger(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, const FGameplayTag TriggerType, EOGTriggerListenerPhases TriggerPhase,
		const bool bOnce, const bool bShouldFireForExistingTriggers, const UObject* FilterInstigator, const UObject* FilterTarget, const TArray<UOGGameplayTriggerFilter*>& Filters);
	
protected:
	UFUNCTION()
	void OnTrigger(const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& Phases, const UOGGameplayTriggerContext* TriggerContext);

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:

	FGameplayTag TriggerType;
	EOGTriggerListenerPhases TriggerPhase;
	bool bOnce = false;
	bool bShouldFireForExistingTriggers;
	UPROPERTY()
	TObjectPtr<const UObject> FilterInstigator;
	UPROPERTY()
	TObjectPtr<const UObject> FilterTarget;

	UPROPERTY()
	TArray<TObjectPtr<UOGGameplayTriggerFilter>> FilterObjects;
	
	FOGTriggerListenerHandle Handle;
	TOGPromise<void> WhenExpired = TOGPromise<void>(nullptr);
	TOGFuture<void> WhenListenerRemoved;
};

UCLASS()
class OGGAMEPLAYTRIGGER_API UOGWhenGameplayTriggerTask_MultiPhase : public UGameplayTask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintAssignable)
	FOGGameplayTriggerTaskDelegate WhenStart;
	UPROPERTY(BlueprintAssignable)
	FOGGameplayTriggerTaskDelegate WhenUpdate;
	UPROPERTY(BlueprintAssignable)
	FOGGameplayTriggerTaskDelegate WhenEnd;

	UFUNCTION(BlueprintCallable, Category = "GameplayTrigger", meta = (DefaultToSelf="TaskOwner", BlueprintInternalUseOnly = "TRUE",
		GameplayTagFilter="Trigger", AutoCreateRefTerm="Filters", AdvancedDisplay="FilterInstigator,FilterTarget,Filters"))
	static UOGWhenGameplayTriggerTask_MultiPhase* WhenGameplayTrigger_MultiPhase(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, const FGameplayTag TriggerType,
		const bool bShouldFireForExistingTriggers, const UObject* FilterInstigator, const UObject* FilterTarget, const TArray<UOGGameplayTriggerFilter*>& Filters);
	
protected:
	UFUNCTION()
	void OnTrigger(const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& Phases, const UOGGameplayTriggerContext* TriggerContext) const;

	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:

	FGameplayTag TriggerType;
	bool bShouldFireForExistingTriggers;
	UPROPERTY()
	TObjectPtr<const UObject> FilterInstigator;
	UPROPERTY()
	TObjectPtr<const UObject> FilterTarget;
	UPROPERTY()
	TArray<TObjectPtr<UOGGameplayTriggerFilter>> FilterObjects;
	
	FOGTriggerListenerHandle Handle;
	TOGPromise<void> WhenExpired = TOGPromise<void>(nullptr);
	TOGFuture<void> WhenListenerRemoved;
};