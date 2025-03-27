#include "..\Public\OGGameplayTriggerTestsModule.h"

#include "GameplayTagsManager.h"

#define LOCTEXT_NAMESPACE "FOGGameplayTriggerTestsModule"

void FOGGameplayTriggerTestsModule::StartupModule()
{
	// Register gameplay tags
	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();
    
	// Register test tags
	TagManager.AddNativeGameplayTag(TEXT("Test.Trigger.Basic"));
	TagManager.AddNativeGameplayTag(TEXT("Test.Trigger.Nested"));
	TagManager.AddNativeGameplayTag(TEXT("Test.Trigger.Tag1"));
	TagManager.AddNativeGameplayTag(TEXT("Test.Trigger.Tag2"));
    
	// Make sure tags are loaded into memory
	TagManager.DoneAddingNativeTags();
}

void FOGGameplayTriggerTestsModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FOGGameplayTriggerTestsModule, OGGameplayTriggerTests)