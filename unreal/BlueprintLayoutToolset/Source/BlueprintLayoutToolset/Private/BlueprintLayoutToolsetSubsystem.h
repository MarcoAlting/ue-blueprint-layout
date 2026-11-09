#pragma once

#include "EditorSubsystem.h"

#include "BlueprintLayoutToolsetSubsystem.generated.h"

/** Registers UBlueprintLayoutToolset with the toolset registry for the lifetime of the editor. */
UCLASS()
class UBlueprintLayoutToolsetSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
};
