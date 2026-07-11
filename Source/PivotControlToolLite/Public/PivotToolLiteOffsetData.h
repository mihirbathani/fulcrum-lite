// Copyright (c) 2026 Mihir Bathani. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "PivotToolLiteOffsetData.generated.h"

// Tracks the total pivot offset Fulcrum Lite has applied to a Static Mesh since it
// was imported, so "Reset to Original Pivot" can undo it exactly. This is a distinct
// UObject class from the full Fulcrum plugin's tracker so the two can coexist in one
// project without a duplicate-class name clash.
UCLASS()
class PIVOTCONTROLTOOLLITE_API UPivotToolLiteOffsetData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FVector AccumulatedOffset = FVector::ZeroVector;
};
