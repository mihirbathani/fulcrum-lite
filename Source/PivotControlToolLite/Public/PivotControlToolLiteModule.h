// Copyright (c) 2026 Mihir Bathani. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SDockTab;
class FSlateStyleSet;

class FPivotControlToolLiteModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<SDockTab> OnSpawnPivotControlTab(const FSpawnTabArgs& SpawnTabArgs);

	void RegisterStyle();
	void UnregisterStyle();
	void RegisterMenus();

	static const FName PivotControlTabName;

	TSharedPtr<FSlateStyleSet> StyleSet;
};
