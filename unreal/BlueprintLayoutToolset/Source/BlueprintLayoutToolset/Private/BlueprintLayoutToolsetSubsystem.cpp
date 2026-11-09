#include "BlueprintLayoutToolsetSubsystem.h"

#include "BlueprintLayoutToolset.h"
#include "Subsystems/SubsystemCollection.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

void UBlueprintLayoutToolsetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// RegisterToolsetClass does nothing when the registry subsystem is not initialized yet.
	Collection.InitializeDependency<UToolsetRegistrySubsystem>();
	UToolsetRegistry::RegisterToolsetClass(UBlueprintLayoutToolset::StaticClass());
}

void UBlueprintLayoutToolsetSubsystem::Deinitialize()
{
	UToolsetRegistry::UnregisterToolsetClass(UBlueprintLayoutToolset::StaticClass());
	Super::Deinitialize();
}
