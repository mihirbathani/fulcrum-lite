#include "PivotControlToolLiteModule.h"
#include "SPivotControlPanel.h"

#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Textures/SlateIcon.h"
#include "Interfaces/IPluginManager.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "PivotControlToolLite"

const FName FPivotControlToolLiteModule::PivotControlTabName(TEXT("PivotControlToolLite"));

namespace
{
	const FName FulcrumLiteStyleName(TEXT("FulcrumLiteStyle"));
	const FName FulcrumLiteTabIconName(TEXT("FulcrumLite.TabIcon"));
}

void FPivotControlToolLiteModule::RegisterStyle()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(FulcrumLiteStyleName);

	FString ResourcesDir;
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PivotControlToolLite")))
	{
		ResourcesDir = Plugin->GetBaseDir() / TEXT("Resources");
	}
	StyleSet->SetContentRoot(ResourcesDir);

	const FVector2D Icon16(16.0f, 16.0f);
	StyleSet->Set(FulcrumLiteTabIconName, new FSlateImageBrush(ResourcesDir / TEXT("Icon128.png"), Icon16));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

void FPivotControlToolLiteModule::UnregisterStyle()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		StyleSet.Reset();
	}
}

void FPivotControlToolLiteModule::RegisterMenus()
{
	// Add "Fulcrum Lite" under the level editor's Tools menu (instead of Window).
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	if (!ToolsMenu)
	{
		return;
	}

	FToolMenuSection& Section = ToolsMenu->FindOrAddSection("FulcrumLite");
	const FName TabName = PivotControlTabName;
	Section.AddMenuEntry(
		"OpenFulcrumLite",
		LOCTEXT("OpenFulcrumLabel", "Fulcrum Lite"),
		LOCTEXT("OpenFulcrumTooltip", "Open Fulcrum Lite - reposition a static mesh's pivot in one click"),
		FSlateIcon(FulcrumLiteStyleName, FulcrumLiteTabIconName),
		FUIAction(FExecuteAction::CreateLambda([TabName]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(TabName);
		})));
}

void FPivotControlToolLiteModule::StartupModule()
{
	RegisterStyle();

	// Tab spawner is registered WITHOUT a Window-menu group so it no longer appears
	// under Window; it is reached through the Tools menu entry below (or restored
	// docking layout). It still must be registered so the tab can be spawned.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PivotControlTabName, FOnSpawnTab::CreateRaw(this, &FPivotControlToolLiteModule::OnSpawnPivotControlTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Fulcrum Lite"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Fulcrum Lite - reposition a static mesh's pivot in one click"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FulcrumLiteStyleName, FulcrumLiteTabIconName));

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPivotControlToolLiteModule::RegisterMenus));
}

void FPivotControlToolLiteModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PivotControlTabName);
	}

	UnregisterStyle();
}

TSharedRef<SDockTab> FPivotControlToolLiteModule::OnSpawnPivotControlTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("TabLabel", "Fulcrum Lite"))
		[
			SNew(SPivotControlPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPivotControlToolLiteModule, PivotControlToolLite)
