/// Copyright Occam's Gamekit contributors 2025

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "OGHandleBase.h"
#include "OGPolymorphicDataBank.h"
#include "OGGameplayTriggerTypes.generated.h"

class UOGGameplayTriggerSubsystem;

UENUM(meta = (BitFlags))
enum class EOGTriggerOperationFlags : uint8
{
    None                        = 0,
    AddActiveTrigger            = 1 << 0,
    TriggerUpdate               = 1 << 1, //Does not represent an operation, but useful for listener filtering
    ProcessCallbacks            = 1 << 2,
    NetworkRPC                  = 1 << 3,
    RemoveActiveTrigger         = 1 << 4,
    
    InstantaneousTrigger        = AddActiveTrigger | ProcessCallbacks | RemoveActiveTrigger,
    NetworkedInstantaneous      = InstantaneousTrigger | NetworkRPC,
    OpenTrigger                 = AddActiveTrigger | ProcessCallbacks,
    CloseTrigger                = ProcessCallbacks | RemoveActiveTrigger
};
ENUM_CLASS_FLAGS(EOGTriggerOperationFlags)

UENUM(BlueprintType, meta = (BitFlags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EOGTriggerListenerPhases : uint8
{
    None            = 0 UMETA(Hidden),
    TriggerStart    = EOGTriggerOperationFlags::AddActiveTrigger,
    TriggerUpdate   = EOGTriggerOperationFlags::TriggerUpdate,
    TriggerEnd      = EOGTriggerOperationFlags::RemoveActiveTrigger,

    All             = TriggerStart | TriggerUpdate | TriggerEnd UMETA(Hidden)
};
ENUM_CLASS_FLAGS(EOGTriggerListenerPhases)

USTRUCT(NotBlueprintType)
struct OGGAMEPLAYTRIGGER_API FOGTriggerDataType : public FOGPolymorphicStructBase
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct OGGAMEPLAYTRIGGER_API FOGTriggerData : public FOGPolymorphicDataBankBase
{
    GENERATED_BODY();

    virtual UScriptStruct* GetInnerStruct() const override {return FOGTriggerDataType::StaticStruct();}
};

template<>
struct TStructOpsTypeTraits<FOGTriggerData> : public TStructOpsTypeTraitsBase2<FOGTriggerData>
{
    enum
    {
        WithAddStructReferencedObjects = true,
        WithNetSerializer = true,
        WithNetDeltaSerializer = true,
    };
};

USTRUCT()
struct OGGAMEPLAYTRIGGER_API FOGGameplayTriggerHandle : public FOGHandleBase
{
    GENERATED_BODY()

    FOGGameplayTriggerHandle() : FOGHandleBase() {}
    FOGGameplayTriggerHandle(OGHandleIdType InHandle) : FOGHandleBase(InHandle) {}

    bool IsActive() const;
    virtual bool IsValid() const override;
    virtual void Reset() override;

    UPROPERTY()
    FGameplayTag TriggerType;

    TWeakObjectPtr<UOGGameplayTriggerSubsystem> TriggerSubsystem = nullptr;
};

USTRUCT()
struct OGGAMEPLAYTRIGGER_API FOGTriggerListenerHandle : public FOGHandleBase
{
    GENERATED_BODY()

    FOGTriggerListenerHandle() : FOGHandleBase() {}
    FOGTriggerListenerHandle(OGHandleIdType InHandle) : FOGHandleBase(InHandle) {}

    virtual bool IsValid() const override;
    virtual void Reset() override;

    UPROPERTY()
    FGameplayTag TriggerType;

    TWeakObjectPtr<UOGGameplayTriggerSubsystem> TriggerSubsystem = nullptr;
};

UCLASS(BlueprintType)
class OGGAMEPLAYTRIGGER_API UOGGameplayTriggerContext : public UObject
{
    GENERATED_BODY()

public:

    virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;
    
    UPROPERTY(Replicated)
    FGameplayTag TriggerType = FGameplayTag::EmptyTag;

    UPROPERTY(Replicated)
    TObjectPtr<UObject> InitiatorObject = nullptr;
    
    UPROPERTY(Replicated)
    TObjectPtr<UObject> TargetObject = nullptr;

    UPROPERTY(Replicated, BlueprintReadWrite)
    FGameplayTagContainer TriggerTags = FGameplayTagContainer::EmptyContainer;
    
    UPROPERTY(Replicated, BlueprintReadWrite)
    FOGTriggerData AdditionalData;
};