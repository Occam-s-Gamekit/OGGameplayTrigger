#include "OGGameplayTriggerTests.h"

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "OGGameplayTriggerSubsystem.h"
#include "OGGameplayTriggerTypes.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGTriggerSubsystemBasicTest, "OccamsGamekit.OGGameplayTrigger.BasicFunctionality",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOGTriggerSubsystemBasicTest::RunTest(const FString& Parameters)
{
    // This will get cleaned up when it leaves scope
    FTestWorldWrapper WorldWrapper;
    WorldWrapper.CreateTestWorld(EWorldType::Game);
    UWorld* World = WorldWrapper.GetTestWorld();
    if (!World)
        return false;

    // Get the trigger subsystem
    UOGGameplayTriggerSubsystem* TriggerSubsystem = UOGGameplayTriggerSubsystem::Get(World);
    if (!TriggerSubsystem)
    {
        AddError(TEXT("Failed to get OGGameplayTriggerSubsystem"));
        return false;
    }

    // Test 1: Basic registration and triggering
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        bool bCallbackReceived = false;
        AActor* ReceivedInitiator = nullptr;
        AActor* ReceivedTarget = nullptr;
        FGameplayTagContainer ReceivedTriggerTags;

        // Create a test delegate
        FOGTriggerDelegate TriggerDelegate;
        TriggerDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            bCallbackReceived = true;
            ReceivedInitiator = Cast<AActor>(ActiveTrigger->InitiatorObject);
            ReceivedTarget = Cast<AActor>(ActiveTrigger->TargetObject);
            ReceivedTriggerTags = ActiveTrigger->TriggerTags;
        });

        // Register the listener
        FOGTriggerListenerHandle ListenerHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, EOGTriggerListenerPhases::TriggerStart, TriggerDelegate);
        TestTrue(TEXT("Listener handle should be valid"), ListenerHandle.IsValid());

        // Create test objects for initiator and target
        AActor* TestInitiator = World->SpawnActor<AActor>();
        AActor* TestTarget = World->SpawnActor<AActor>();

        // Create test trigger tags
        FGameplayTagContainer TestTriggerTags;
        TestTriggerTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Tag1")));
        TestTriggerTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Tag2")));

        // Fire the trigger
        UOGGameplayTriggerContext* TriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, TestTriggerTags, TestInitiator, TestTarget);
        TriggerSubsystem->InstantaneousTrigger(TriggerContext);

        // Verify the callback was received with correct parameters
        TestTrue(TEXT("Callback should have been received"), bCallbackReceived);
        TestEqual(TEXT("Initiator should match"), ReceivedInitiator, TestInitiator);
        TestEqual(TEXT("Target should match"), ReceivedTarget, TestTarget);
        TestEqual(TEXT("Trigger tags should match"), ReceivedTriggerTags, TestTriggerTags);

        // Clean up
        TriggerSubsystem->RemoveTriggerListener(ListenerHandle);
    }

    // Test 2: Multiple listeners for the same trigger type
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        int32 CallbackCount1 = 0;
        int32 CallbackCount2 = 0;

        // Create test delegates
        FOGTriggerDelegate TriggerDelegate1;
        TriggerDelegate1.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            CallbackCount1++;
        });

        FOGTriggerDelegate TriggerDelegate2;
        TriggerDelegate2.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            CallbackCount2++;
        });

        // Register the listeners
        FOGTriggerListenerHandle ListenerHandle1 = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, EOGTriggerListenerPhases::TriggerStart, TriggerDelegate1);
        FOGTriggerListenerHandle ListenerHandle2 = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, EOGTriggerListenerPhases::TriggerStart, TriggerDelegate2);

        // Fire the trigger
        UOGGameplayTriggerContext* TriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, FGameplayTagContainer::EmptyContainer);
        TriggerSubsystem->InstantaneousTrigger(TriggerContext);

        // Verify both callbacks were received
        TestEqual(TEXT("First callback should have been received once"), CallbackCount1, 1);
        TestEqual(TEXT("Second callback should have been received once"), CallbackCount2, 1);

        // Remove one listener and fire again
        TriggerSubsystem->RemoveTriggerListener(ListenerHandle1);
        TriggerSubsystem->InstantaneousTrigger(TriggerContext);

        // Verify only the second callback was received
        TestEqual(TEXT("First callback should still have been received once"), CallbackCount1, 1);
        TestEqual(TEXT("Second callback should have been received twice"), CallbackCount2, 2);

        // Clean up
        TriggerSubsystem->RemoveTriggerListener(ListenerHandle2);
    }

    // Test 3: Test bShouldFireForExistingTriggers parameter
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        int32 CallbackCount = 0;

        // Fire a trigger before registering any listeners
        UOGGameplayTriggerContext* TriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, FGameplayTagContainer::EmptyContainer);
        FOGGameplayTriggerHandle ActiveTriggerHandle = TriggerSubsystem->StartTrigger(TriggerContext);

        // Create a test delegate
        FOGTriggerDelegate TriggerDelegate;
        TriggerDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            CallbackCount++;
        });

        // Register with bShouldFireForExistingTriggers = false (default)
        FOGTriggerListenerHandle ListenerHandle1 = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, EOGTriggerListenerPhases::TriggerStart, TriggerDelegate);
        
        // Verify no callback was received for the pre-existing trigger
        TestEqual(TEXT("No callback should have been received"), CallbackCount, 0);
        
        FOGTriggerListenerHandle ListenerHandle2 = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, EOGTriggerListenerPhases::TriggerStart, TriggerDelegate, nullptr, nullptr, true);
        
        // Verify a callback was received for the pre-existing trigger
        TestEqual(TEXT("One callback should have been received"), CallbackCount, 1);
        
        // Clean up
        TriggerSubsystem->EndTrigger(ActiveTriggerHandle);
        TriggerSubsystem->RemoveTriggerListener(ListenerHandle1);
        TriggerSubsystem->RemoveTriggerListener(ListenerHandle2);
    }

    // Test 4: Removing a listener
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        int32 CallbackCount = 0;

        // Create a test delegate
        FOGTriggerDelegate TriggerDelegate;
        TriggerDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            CallbackCount++;
        });

        // Register the listener
        FOGTriggerListenerHandle ListenerHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, EOGTriggerListenerPhases::TriggerStart, TriggerDelegate);

        // Fire the trigger
        UOGGameplayTriggerContext* TriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, FGameplayTagContainer::EmptyContainer);
        TriggerSubsystem->InstantaneousTrigger(TriggerContext);
        TestEqual(TEXT("Callback should have been received once"), CallbackCount, 1);

        // Remove the listener
        TriggerSubsystem->RemoveTriggerListener(ListenerHandle);

        // Fire the trigger again
        TriggerSubsystem->InstantaneousTrigger(TriggerContext);
        TestEqual(TEXT("Callback count should still be 1"), CallbackCount, 1);
    }

    // Test 5: Verify TriggerPhase is set correctly for InstantaneousTrigger
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        EOGTriggerListenerPhases ReceivedPhase = EOGTriggerListenerPhases::None;

        // Register the listener
        FOGTriggerListenerHandle ListenerHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, EOGTriggerListenerPhases::TriggerStart, FOGTriggerDelegate::CreateLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            ReceivedPhase = TriggerPhase;
        }));

        // Fire the trigger
        UOGGameplayTriggerContext* TriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, FGameplayTagContainer::EmptyContainer);
        TriggerSubsystem->InstantaneousTrigger(TriggerContext);
        
        // For instantaneous triggers, we would expect the phase to be both start and end
        TestEqual(TEXT("Trigger phase should be Instantaneous"), ReceivedPhase, EOGTriggerListenerPhases::TriggerStart | EOGTriggerListenerPhases::TriggerEnd);

        // Clean up
        TriggerSubsystem->RemoveTriggerListener(ListenerHandle);
    }

    // Test 6: Invalid handle removal
    {
        // Create an invalid handle
        FOGTriggerListenerHandle InvalidHandle;

        // This should not crash
        TriggerSubsystem->RemoveTriggerListener(InvalidHandle);
        
        TestTrue(TEXT("Test completed without crashing"), true);
    }

    // Test 7: Test StartTrigger, UpdateTrigger, and EndTrigger flow with proper phases
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        EOGTriggerListenerPhases ReceivedStartPhase = EOGTriggerListenerPhases::None;
        EOGTriggerListenerPhases ReceivedUpdatePhase = EOGTriggerListenerPhases::None;
        EOGTriggerListenerPhases ReceivedEndPhase = EOGTriggerListenerPhases::None;
        int32 StartCallCount = 0;
        int32 UpdateCallCount = 0;
        int32 EndCallCount = 0;

        // Create test delegates
        FOGTriggerDelegate StartDelegate;
        StartDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            ReceivedStartPhase = TriggerPhase;
            StartCallCount++;
            TestEqual(TEXT("Data at Trigger Start"),ActiveTrigger->DataBank.GetConstChecked<FTestTriggerData_Int>().TestInt, 123);
        });

        // Create test delegates
        FOGTriggerDelegate UpdateDelegate;
        UpdateDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            ReceivedStartPhase = TriggerPhase;
            UpdateCallCount++;
            TestEqual(TEXT("Data at Trigger Update"),ActiveTrigger->DataBank.GetConstChecked<FTestTriggerData_Int>().TestInt, 456);
        });

        FOGTriggerDelegate EndDelegate;
        EndDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            ReceivedEndPhase = TriggerPhase;
            EndCallCount++;
            TestEqual(TEXT("Data at Trigger End"),ActiveTrigger->DataBank.GetConstChecked<FTestTriggerData_Int>().TestInt, 456);
        });

        // Register separate listeners for start and end phases
        FOGTriggerListenerHandle StartListenerHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, EOGTriggerListenerPhases::TriggerStart, StartDelegate);
        FOGTriggerListenerHandle UpdateListenerHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, EOGTriggerListenerPhases::TriggerUpdate, UpdateDelegate);
        FOGTriggerListenerHandle EndListenerHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, EOGTriggerListenerPhases::TriggerEnd, EndDelegate);

        // Create and start the trigger
        UOGGameplayTriggerContext* TriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, FGameplayTagContainer::EmptyContainer);
        TriggerContext->DataBank.AddUnique<FTestTriggerData_Int>().TestInt = 123;
        FOGGameplayTriggerHandle TriggerHandle = TriggerSubsystem->StartTrigger(TriggerContext);

        // Verify only start phase was fired
        TestEqual(TEXT("Start callback should have been called once"), StartCallCount, 1);
        TestEqual(TEXT("Update callback should not have been called yet"), UpdateCallCount, 0);
        TestEqual(TEXT("End callback should not have been called yet"), EndCallCount, 0);
        TestEqual(TEXT("Start phase should be TriggerStart"), ReceivedStartPhase, EOGTriggerListenerPhases::TriggerStart);

        UOGGameplayTriggerContext* ContextToUpdate = TriggerSubsystem->GetTriggerContextForUpdate(TriggerHandle);
        ContextToUpdate->DataBank.GetChecked<FTestTriggerData_Int>().TestInt = 456;
        TriggerSubsystem->UpdateTrigger(TriggerHandle, ContextToUpdate);
        
        // Verify update was called 
        TestEqual(TEXT("Start callback should have been called once"), StartCallCount, 1);
        TestEqual(TEXT("Update callback should have been called once"), UpdateCallCount, 1);
        TestEqual(TEXT("End callback should not have been called yet"), EndCallCount, 0);
        TestEqual(TEXT("Update phase should be TriggerUpdate"), ReceivedStartPhase, EOGTriggerListenerPhases::TriggerUpdate);
        
        // End the trigger and verify end phase was fired
        TriggerSubsystem->EndTrigger(TriggerHandle);
        TestEqual(TEXT("Start callback should still be called once"), StartCallCount, 1);
        TestEqual(TEXT("Update callback should have been called once"), UpdateCallCount, 1);
        TestEqual(TEXT("End callback should have been called once"), EndCallCount, 1);
        TestEqual(TEXT("End phase should be TriggerEnd"), ReceivedEndPhase, EOGTriggerListenerPhases::TriggerEnd);

        // Clean up
        TriggerSubsystem->RemoveTriggerListener(StartListenerHandle);
        TriggerSubsystem->RemoveTriggerListener(UpdateListenerHandle);
        TriggerSubsystem->RemoveTriggerListener(EndListenerHandle);
    }

    // Test 8: Testing filter functionality for initiator and target
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        
        // Create test objects for initiator and target
        AActor* TestInitiator1 = World->SpawnActor<AActor>();
        AActor* TestInitiator2 = World->SpawnActor<AActor>();
        AActor* TestTarget1 = World->SpawnActor<AActor>();
        AActor* TestTarget2 = World->SpawnActor<AActor>();
        
        int32 UnfilteredCallCount = 0;
        int32 InitiatorFilteredCallCount = 0;
        int32 TargetFilteredCallCount = 0;
        int32 BothFilteredCallCount = 0;
        
        // Create test delegates
        FOGTriggerDelegate UnfilteredDelegate;
        UnfilteredDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            UnfilteredCallCount++;
        });
        
        FOGTriggerDelegate InitiatorFilteredDelegate;
        InitiatorFilteredDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            InitiatorFilteredCallCount++;
        });
        
        FOGTriggerDelegate TargetFilteredDelegate;
        TargetFilteredDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            TargetFilteredCallCount++;
        });
        
        FOGTriggerDelegate BothFilteredDelegate;
        BothFilteredDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            BothFilteredCallCount++;
        });

        // Register listeners with different filter combinations
        FOGTriggerListenerHandle UnfilteredHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, UnfilteredDelegate);
        
        FOGTriggerListenerHandle InitiatorFilteredHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, InitiatorFilteredDelegate, TestInitiator1, nullptr);
        
        FOGTriggerListenerHandle TargetFilteredHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, TargetFilteredDelegate, nullptr, TestTarget1);
        
        FOGTriggerListenerHandle BothFilteredHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, BothFilteredDelegate, TestInitiator1, TestTarget1);

        // Test Case 1: Should hit all but target-filtered and both-filtered
        UOGGameplayTriggerContext* TriggerContext1 = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer, TestInitiator1, TestTarget2);
        TriggerSubsystem->InstantaneousTrigger(TriggerContext1);
        
        TestEqual(TEXT("Unfiltered callback should have been called once"), UnfilteredCallCount, 1);
        TestEqual(TEXT("Initiator-filtered callback should have been called once"), InitiatorFilteredCallCount, 1);
        TestEqual(TEXT("Target-filtered callback should not have been called"), TargetFilteredCallCount, 0);
        TestEqual(TEXT("Both-filtered callback should not have been called"), BothFilteredCallCount, 0);

        // Test Case 2: Should hit all but initiator-filtered and both-filtered
        UOGGameplayTriggerContext* TriggerContext2 = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer, TestInitiator2, TestTarget1);
        TriggerSubsystem->InstantaneousTrigger(TriggerContext2);
        
        TestEqual(TEXT("Unfiltered callback should have been called twice"), UnfilteredCallCount, 2);
        TestEqual(TEXT("Initiator-filtered callback should still have been called once"), InitiatorFilteredCallCount, 1);
        TestEqual(TEXT("Target-filtered callback should have been called once"), TargetFilteredCallCount, 1);
        TestEqual(TEXT("Both-filtered callback should not have been called"), BothFilteredCallCount, 0);

        // Test Case 3: Should hit all callbacks
        UOGGameplayTriggerContext* TriggerContext3 = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer, TestInitiator1, TestTarget1);
        TriggerSubsystem->InstantaneousTrigger(TriggerContext3);
        
        TestEqual(TEXT("Unfiltered callback should have been called three times"), UnfilteredCallCount, 3);
        TestEqual(TEXT("Initiator-filtered callback should have been called twice"), InitiatorFilteredCallCount, 2);
        TestEqual(TEXT("Target-filtered callback should have been called twice"), TargetFilteredCallCount, 2);
        TestEqual(TEXT("Both-filtered callback should have been called once"), BothFilteredCallCount, 1);

        // Clean up
        TriggerSubsystem->RemoveTriggerListener(UnfilteredHandle);
        TriggerSubsystem->RemoveTriggerListener(InitiatorFilteredHandle);
        TriggerSubsystem->RemoveTriggerListener(TargetFilteredHandle);
        TriggerSubsystem->RemoveTriggerListener(BothFilteredHandle);
    }

    // Test 9: Test multiple phases in a single listener
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        TArray<EOGTriggerListenerPhases> ReceivedPhases;
        
        // Create test delegate that records all received phases
        FOGTriggerDelegate MultiPhaseDelegate;
        MultiPhaseDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            ReceivedPhases.Add(TriggerPhase);
        });
        
        // Register a listener for both start and end phases
        FOGTriggerListenerHandle MultiPhaseHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart | EOGTriggerListenerPhases::TriggerEnd, MultiPhaseDelegate);
        
        // Test instantaneous trigger (should get one callback with both phases)
        ReceivedPhases.Empty();
        UOGGameplayTriggerContext* InstantTriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer);
        TriggerSubsystem->InstantaneousTrigger(InstantTriggerContext);
        
        TestEqual(TEXT("Should receive 1 callback for instantaneous trigger"), ReceivedPhases.Num(), 1);
        TestEqual(TEXT("Phase for instantaneous trigger should be Start|End"), 
            ReceivedPhases[0], EOGTriggerListenerPhases::TriggerStart | EOGTriggerListenerPhases::TriggerEnd);
        
        // Test separate start/end triggers (should get two separate callbacks)
        ReceivedPhases.Empty();
        UOGGameplayTriggerContext* DurationTriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer);
        FOGGameplayTriggerHandle TriggerHandle = TriggerSubsystem->StartTrigger(DurationTriggerContext);
        
        TestEqual(TEXT("Should receive 1 callback after StartTrigger"), ReceivedPhases.Num(), 1);
        TestEqual(TEXT("First phase should be TriggerStart"), ReceivedPhases[0], EOGTriggerListenerPhases::TriggerStart);
        
        TriggerSubsystem->EndTrigger(TriggerHandle);
        TestEqual(TEXT("Should receive 2 callbacks total after EndTrigger"), ReceivedPhases.Num(), 2);
        TestEqual(TEXT("Second phase should be TriggerEnd"), ReceivedPhases[1], EOGTriggerListenerPhases::TriggerEnd);
        
        // Clean up
        TriggerSubsystem->RemoveTriggerListener(MultiPhaseHandle);
    }

    // Test 10: test filters
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        int ReceivedFilteredCount = 0;
        int ReceivedUnfilteredCount = 0;

        // Create test delegates that record filtered and unfiltered counts
        FOGTriggerDelegate FilteredTriggerDelegate;
        FilteredTriggerDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            ReceivedFilteredCount++;
        });
        
        // Create test delegates that record filtered and unfiltered counts
        FOGTriggerDelegate UnfilteredTriggerDelegate;
        UnfilteredTriggerDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            ReceivedUnfilteredCount++;
        });
        
        // Register a listener for both start and end phases
        FOGTriggerListenerHandle FilteredHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, FilteredTriggerDelegate, nullptr, nullptr, false, {UOGTestTriggerFilter_DataIsPositive::StaticClass()->GetDefaultObject<UOGTestTriggerFilter_DataIsPositive>()});

        FOGTriggerListenerHandle UnfilteredHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, UnfilteredTriggerDelegate);

        UOGGameplayTriggerContext* InstantTriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer);
        TriggerSubsystem->InstantaneousTrigger(InstantTriggerContext);

        TestEqual(TEXT("Trigger with no data should not pass the filter"), ReceivedFilteredCount, 0);
        TestEqual(TEXT("Unfiltered count should be 1"), ReceivedUnfilteredCount, 1);

        InstantTriggerContext->DataBank.AddUnique<FTestTriggerData_Int>().TestInt = -123;
        TriggerSubsystem->InstantaneousTrigger(InstantTriggerContext);

        TestEqual(TEXT("Trigger with negative value in data should not pass the filter"), ReceivedFilteredCount, 0);
        TestEqual(TEXT("Unfiltered count should be 2"), ReceivedUnfilteredCount, 2);

        InstantTriggerContext->DataBank.GetChecked<FTestTriggerData_Int>().TestInt = 456;
        TriggerSubsystem->InstantaneousTrigger(InstantTriggerContext);

        TestEqual(TEXT("Trigger with positive value in data should pass the filter"), ReceivedFilteredCount, 1);
        TestEqual(TEXT("Unfiltered count should be 3"), ReceivedUnfilteredCount, 3);

        //Clean up
        FilteredHandle.Reset();
        UnfilteredHandle.Reset();
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGTriggerSubsystemPendingOperationsTest, "OccamsGamekit.OGGameplayTrigger.PendingOperationsQueue",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOGTriggerSubsystemPendingOperationsTest::RunTest(const FString& Parameters)
{
    // Setup test environment
    FTestWorldWrapper WorldWrapper;
    WorldWrapper.CreateTestWorld(EWorldType::Game);
    UWorld* World = WorldWrapper.GetTestWorld();
    if (!World)
        return false;

    // Get the trigger subsystem
    UOGGameplayTriggerSubsystem* TriggerSubsystem = UOGGameplayTriggerSubsystem::Get(World);
    if (!TriggerSubsystem)
    {
        AddError(TEXT("Failed to get OGGameplayTriggerSubsystem"));
        return false;
    }

    // Test 1: Adding a listener during trigger processing
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        int32 PrimaryCallbackCount = 0;
        int32 SecondaryCallbackCount = 0;
        FOGTriggerListenerHandle SecondaryHandle;

        // Create a primary delegate that will register a new listener during execution
        FOGTriggerDelegate PrimaryDelegate;
        PrimaryDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            PrimaryCallbackCount++;

            if (SecondaryHandle.IsValid())
                return;
            // Register a secondary listener during the primary callback
            FOGTriggerDelegate SecondaryDelegate;
            SecondaryDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
            {
                SecondaryCallbackCount++;
            });
            
            SecondaryHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
                EOGTriggerListenerPhases::TriggerStart | EOGTriggerListenerPhases::TriggerEnd, 
                SecondaryDelegate);
        });

        // Register the primary listener
        FOGTriggerListenerHandle PrimaryHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, PrimaryDelegate);

        // Fire the trigger
        UOGGameplayTriggerContext* TriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer);
        TriggerSubsystem->InstantaneousTrigger(TriggerContext);

        // Verify that the primary callback was called, but not the secondary (should be pending)
        TestEqual(TEXT("Primary callback should have been called once"), PrimaryCallbackCount, 1);
        TestEqual(TEXT("Secondary callback should not have been called for the current trigger"), SecondaryCallbackCount, 0);

        // Fire another trigger to verify the secondary listener is now active
        TriggerSubsystem->InstantaneousTrigger(TriggerContext);
        TestEqual(TEXT("Primary callback should have been called twice"), PrimaryCallbackCount, 2);
        TestEqual(TEXT("Secondary callback should have been called once"), SecondaryCallbackCount, 1);

        // Clean up
        TriggerSubsystem->RemoveTriggerListener(PrimaryHandle);
        TriggerSubsystem->RemoveTriggerListener(SecondaryHandle);
    }

    // Test 2: Removing a listener during trigger processing
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        int32 FirstCallbackCount = 0;
        int32 SecondCallbackCount = 0;
        int32 ThirdCallbackCount = 0;
        FOGTriggerListenerHandle SecondHandle;
        FOGTriggerListenerHandle ThirdHandle;

        // Create delegates
        FOGTriggerDelegate FirstDelegate;
        FirstDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            FirstCallbackCount++;
            
            // Remove the second listener during the first callback
            if (SecondHandle.IsValid())
            {
                TriggerSubsystem->RemoveTriggerListener(SecondHandle);
            }
        });

        FOGTriggerDelegate SecondDelegate;
        SecondDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            SecondCallbackCount++;
        });

        FOGTriggerDelegate ThirdDelegate;
        ThirdDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            ThirdCallbackCount++;
        });

        // Register the listeners in a specific order
        FOGTriggerListenerHandle FirstHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, FirstDelegate);
        SecondHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, SecondDelegate);
        ThirdHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, ThirdDelegate);

        // Fire the trigger
        UOGGameplayTriggerContext* TriggerContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer);
        TriggerSubsystem->InstantaneousTrigger(TriggerContext);

        // Verify that all callbacks were called for the current trigger despite the removal
        TestEqual(TEXT("First callback should have been called once"), FirstCallbackCount, 1);
        TestEqual(TEXT("Second callback should have been called once"), SecondCallbackCount, 1);
        TestEqual(TEXT("Third callback should have been called once"), ThirdCallbackCount, 1);

        // Fire another trigger to verify the second listener is now inactive
        TriggerSubsystem->InstantaneousTrigger(TriggerContext);
        TestEqual(TEXT("First callback should have been called twice"), FirstCallbackCount, 2);
        TestEqual(TEXT("Second callback should still have been called only once"), SecondCallbackCount, 1);
        TestEqual(TEXT("Third callback should have been called twice"), ThirdCallbackCount, 2);

        // Clean up
        TriggerSubsystem->RemoveTriggerListener(FirstHandle);
        TriggerSubsystem->RemoveTriggerListener(ThirdHandle);
    }

    // Test 3: Starting a trigger during trigger processing
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        FGameplayTag NestedTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Nested"));
        int32 OuterCallbackCount = 0;
        int32 NestedCallbackCount = 0;
        FOGGameplayTriggerHandle NestedTriggerHandle;

        // Create delegates
        FOGTriggerDelegate OuterDelegate;
        OuterDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            OuterCallbackCount++;
            
            // Start a nested trigger during the outer callback
            UOGGameplayTriggerContext* NestedContext = TriggerSubsystem->MakeGameplayTriggerContext(NestedTriggerType, 
                FGameplayTagContainer::EmptyContainer);
            NestedTriggerHandle = TriggerSubsystem->StartTrigger(NestedContext);
        });

        FOGTriggerDelegate NestedDelegate;
        NestedDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            NestedCallbackCount++;
        });

        // Register the listeners
        FOGTriggerListenerHandle OuterHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, OuterDelegate);
        FOGTriggerListenerHandle NestedHandle = TriggerSubsystem->RegisterTriggerListener(NestedTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, NestedDelegate);

        // Fire the outer trigger
        UOGGameplayTriggerContext* OuterContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer);
        TriggerSubsystem->InstantaneousTrigger(OuterContext);

        // Verify that the outer callback was called, and the nested trigger was processed after
        TestEqual(TEXT("Outer callback should have been called once"), OuterCallbackCount, 1);
        TestEqual(TEXT("Nested callback should have been called once"), NestedCallbackCount, 1);
        TestTrue(TEXT("Nested trigger handle should be valid"), NestedTriggerHandle.IsValid());

        // Clean up
        TriggerSubsystem->EndTrigger(NestedTriggerHandle);
        TriggerSubsystem->RemoveTriggerListener(OuterHandle);
        TriggerSubsystem->RemoveTriggerListener(NestedHandle);
    }

    // Test 4: Ending a trigger during trigger processing
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        FGameplayTag TestTriggerType2 = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Nested"));
        int32 StartCallbackCount = 0;
        int32 EndCallbackCount = 0;
        FOGGameplayTriggerHandle TriggerToEndHandle;

        // Create a trigger to end before testing
        UOGGameplayTriggerContext* PreExistingContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType2, 
            FGameplayTagContainer::EmptyContainer);
        TriggerToEndHandle = TriggerSubsystem->StartTrigger(PreExistingContext);

        // Create delegates
        FOGTriggerDelegate StartDelegate;
        StartDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            StartCallbackCount++;
            // End the pre-existing trigger during a callback
            TriggerSubsystem->EndTrigger(TriggerToEndHandle);
            TestEqual(TEXT("End callback should not run until after the Start callback is finished"), EndCallbackCount, 0);
        });

        FOGTriggerDelegate EndDelegate;
        EndDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            EndCallbackCount++;
        });

        // Register listeners for both start and end phases
        FOGTriggerListenerHandle StartHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, StartDelegate);
        FOGTriggerListenerHandle EndHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType2, 
            EOGTriggerListenerPhases::TriggerEnd, EndDelegate);

        // Fire a trigger to trigger the start callback
        UOGGameplayTriggerContext* NewContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer);
        TriggerSubsystem->InstantaneousTrigger(NewContext);

        // Verify that the start callback was called, and the end callback was processed after
        TestEqual(TEXT("Start callback should have been called once"), StartCallbackCount, 1);
        TestEqual(TEXT("End callback should have been called once"), EndCallbackCount, 1);

        // Clean up
        TriggerSubsystem->RemoveTriggerListener(StartHandle);
        TriggerSubsystem->RemoveTriggerListener(EndHandle);
    }

    // Test 5: Queuing multiple operations during trigger processing
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        int32 MainCallbackCount = 0;
        int32 SecondCallbackCount = 0;
        FOGTriggerListenerHandle SecondHandle;
        FOGGameplayTriggerHandle TriggerHandleA;
        FOGGameplayTriggerHandle TriggerHandleB;

        // Create delegates
        FOGTriggerDelegate MainDelegate;
        MainDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            MainCallbackCount++;
            
            if (MainCallbackCount > 1)
                return;
            
            // Queue multiple operations during the main callback
            // 1. Start a trigger
            UOGGameplayTriggerContext* ContextA = TriggerSubsystem->MakeGameplayTriggerContext(
                TestTriggerType, FGameplayTagContainer::EmptyContainer);
            TriggerHandleA = TriggerSubsystem->StartTrigger(ContextA);
            
            // 2. Register a new listener
            FOGTriggerDelegate SecondDelegate;
            SecondDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
            {
                SecondCallbackCount++;
                
                // 3. Start another trigger from the second callback
                UOGGameplayTriggerContext* ContextB = TriggerSubsystem->MakeGameplayTriggerContext(
                    TestTriggerType, FGameplayTagContainer::EmptyContainer);
                TriggerHandleB = TriggerSubsystem->StartTrigger(ContextB);

                TriggerSubsystem->RemoveTriggerListener(SecondHandle);
            });
            
            SecondHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
                EOGTriggerListenerPhases::TriggerStart, SecondDelegate);
        });

        // Register the main listener
        FOGTriggerListenerHandle MainHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, MainDelegate);

        // Initial trigger fire
        UOGGameplayTriggerContext* InitialContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer);
        TriggerSubsystem->InstantaneousTrigger(InitialContext);

        // Verify operation ordering
        TestEqual(TEXT("Main callback should have been called three times"), MainCallbackCount, 3);
        TestEqual(TEXT("Second callback should have been called once"), SecondCallbackCount, 1);
        TestTrue(TEXT("TriggerHandleA should be valid"), TriggerHandleA.IsActive());
        TestTrue(TEXT("TriggerHandleB should be valid"), TriggerHandleB.IsActive());

        // Clean up
        TriggerSubsystem->EndTrigger(TriggerHandleA);
        TriggerSubsystem->EndTrigger(TriggerHandleB);
        TriggerSubsystem->RemoveTriggerListener(MainHandle);
        TriggerSubsystem->RemoveTriggerListener(SecondHandle);
    }

    // Test 6: Multiple 'simultaneous' updates to a trigger
    {
        FGameplayTag TestTriggerType = FGameplayTag::RequestGameplayTag(TEXT("Test.Trigger.Basic"));
        int32 StartCallbackCount = 0;
        int32 UpdateCallbackCount = 0;

        // Create delegates
        FOGTriggerDelegate MainDelegate;
        MainDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            StartCallbackCount++;

            TestEqual(TEXT("Data On Start"), ActiveTrigger->DataBank.GetConstChecked<FTestTriggerData_Int>().TestInt, 789);

            UOGGameplayTriggerContext* ContextForUpdate = TriggerSubsystem->GetTriggerContextForUpdate(TriggerHandle);
            ContextForUpdate->DataBank.GetChecked<FTestTriggerData_Int>().TestInt++;
            TriggerSubsystem->UpdateTrigger(TriggerHandle, ContextForUpdate);
        });

        // Create delegates
        FOGTriggerDelegate UpdateDelegate;
        UpdateDelegate.BindLambda([&](const FOGGameplayTriggerHandle& TriggerHandle, const EOGTriggerListenerPhases& TriggerPhase, const UOGGameplayTriggerContext* ActiveTrigger)
        {
            TestEqual(TEXT("Start should be called twice before any updates"), StartCallbackCount, 2);
            
            UpdateCallbackCount++;
            
            TestEqual(TEXT("Data On Update in first update should be one more than data at start, and in second update should be one higher again"),
                ActiveTrigger->DataBank.GetConstChecked<FTestTriggerData_Int>().TestInt, 789 + UpdateCallbackCount);
        });

        // Register two listeners that will update the trigger on start
        FOGTriggerListenerHandle StartHandleOne = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, MainDelegate);
        FOGTriggerListenerHandle StartHandleTwo = TriggerSubsystem->RegisterTriggerListener(TestTriggerType, 
            EOGTriggerListenerPhases::TriggerStart, MainDelegate);

        FOGTriggerListenerHandle UpdateHandle = TriggerSubsystem->RegisterTriggerListener(TestTriggerType,
            EOGTriggerListenerPhases::TriggerUpdate, UpdateDelegate);
        
        // Initial trigger fire
        UOGGameplayTriggerContext* InitialContext = TriggerSubsystem->MakeGameplayTriggerContext(TestTriggerType, 
            FGameplayTagContainer::EmptyContainer);
        InitialContext->DataBank.AddUnique<FTestTriggerData_Int>().TestInt = 789;
        FOGGameplayTriggerHandle TriggerHandle = TriggerSubsystem->StartTrigger(InitialContext);

        // Verify operation ordering
        TestEqual(TEXT("Main callback should have been called twice"), StartCallbackCount, 2);
        TestEqual(TEXT("Update callback should have been called twice"), UpdateCallbackCount, 2);

        // Clean up
        TriggerSubsystem->RemoveTriggerListener(StartHandleOne);
        TriggerSubsystem->RemoveTriggerListener(StartHandleTwo);
        TriggerSubsystem->RemoveTriggerListener(UpdateHandle);
        TriggerHandle.Reset();
    }

    return true;
}

bool UOGTestTriggerFilter_DataIsPositive::DoesTriggerPassFilter_Native(const EOGTriggerListenerPhases TriggerPhase, const UOGGameplayTriggerContext* Trigger, bool& OutIsFilterStale) const
{
    const FTestTriggerData_Int* Data = Trigger->DataBank.FindConst<FTestTriggerData_Int>();
    if (!Data)
        return false;
    return Data->TestInt > 0;
}
