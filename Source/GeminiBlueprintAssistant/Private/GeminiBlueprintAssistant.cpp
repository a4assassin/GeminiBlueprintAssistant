// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeminiBlueprintAssistant.h"
#include "GeminiAssistantPanel.h" // Include our Slate panel
#include "LevelEditor.h" // For accessing Level Editor menu
#include "Widgets/Docking/SDockTab.h" // For creating a dockable tab
#include "Framework/Application/SlateApplication.h" // For Slate application functions
#include "ToolMenus.h" // For adding menu entries

#define LOCTEXT_NAMESPACE "FGeminiBlueprintAssistantModule"

static const FName GeminiBlueprintAssistantTabID("GeminiBlueprintAssistant");

void FGeminiBlueprintAssistantModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GeminiBlueprintAssistantTabID, FOnSpawnTab::CreateRaw(this, &FGeminiBlueprintAssistantModule::OnSpawnTab))
		.SetDisplayName(LOCTEXT("GeminiBlueprintAssistantTabTitle", "Gemini BP Assistant"))
		.SetMenuType(ETabSpawnerMenuType::Enabled); // Blutility means it shows up in "Window -> Blutility" but we can also add it to "Window" directly

	// Add a menu entry to the main Window menu
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGeminiBlueprintAssistantModule::RegisterMenus));
}

void FGeminiBlueprintAssistantModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GeminiBlueprintAssistantTabID);

	// Unregister menus
	//UToolMenus::UnregisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this); // Unregister any tool menus owned by this module

}

TSharedRef<SDockTab> FGeminiBlueprintAssistantModule::OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(GeminiAssistantPanel)
		];
}

void FGeminiBlueprintAssistantModule::RegisterMenus()
{
	// Owner will be on the list of registered add-ins for the specified menu.
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		// Extend the "Window" menu
		UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = WindowMenu->FindOrAddSection("WindowLayout"); // Or create a new section
			Section.AddMenuEntry(
				NAME_None, // Unique name if you need to reference it later, otherwise NAME_None
				LOCTEXT("GeminiBPAssistantMenuItemLabel", "Gemini BP Assistant"),
				LOCTEXT("GeminiBPAssistantMenuItemTooltip", "Opens the Gemini Blueprint Assistant panel."),
				FSlateIcon(), // You can add an icon here
				FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(GeminiBlueprintAssistantTabID); }))
			);
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGeminiBlueprintAssistantModule, GeminiBlueprintAssistant)