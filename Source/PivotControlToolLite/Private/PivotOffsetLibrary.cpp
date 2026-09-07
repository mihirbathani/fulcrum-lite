// Copyright (c) 2026 Mihir Bathani. All Rights Reserved.

#include "PivotOffsetLibrary.h"
#include "PivotToolLiteOffsetData.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "PhysicsEngine/BodySetup.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Math/ScaleMatrix.h"
#include "Math/RotationMatrix.h"

FBox FPivotOffsetLibrary::ComputeLocalBounds(const UStaticMesh* Mesh)
{
	FBox Bounds(ForceInit);
	if (!Mesh)
	{
		return Bounds;
	}

	const FMeshDescription* MeshDesc = Mesh->GetMeshDescription(0);
	if (!MeshDesc)
	{
		return Bounds;
	}

	FStaticMeshConstAttributes Attributes(*MeshDesc);
	TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
	for (const FVertexID VertexID : MeshDesc->Vertices().GetElementIDs())
	{
		Bounds += FVector(Positions[VertexID]);
	}

	return Bounds;
}

FVector FPivotOffsetLibrary::ComputePresetPivotPoint(const UStaticMesh* Mesh, const FPivotPreset& Preset)
{
	const FBox Bounds = ComputeLocalBounds(Mesh);
	if (!Bounds.IsValid)
	{
		return FVector::ZeroVector;
	}

	auto AnchorCoord = [](EPivotAnchor Anchor, double MinValue, double MaxValue) -> double
	{
		switch (Anchor)
		{
		case EPivotAnchor::Min: return MinValue;
		case EPivotAnchor::Max: return MaxValue;
		case EPivotAnchor::Center:
		default:                return (MinValue + MaxValue) * 0.5;
		}
	};

	return FVector(
		AnchorCoord(Preset.X, Bounds.Min.X, Bounds.Max.X),
		AnchorCoord(Preset.Y, Bounds.Min.Y, Bounds.Max.Y),
		AnchorCoord(Preset.Z, Bounds.Min.Z, Bounds.Max.Z));
}

bool FPivotOffsetLibrary::ApplyPreset(UStaticMesh* Mesh, const FPivotPreset& Preset, FText& OutError)
{
	if (!Mesh)
	{
		OutError = NSLOCTEXT("PivotControlTool", "NoMesh", "No mesh to modify.");
		return false;
	}

	// PivotPoint becomes the new origin, so every vertex shifts by -PivotPoint.
	const FVector PivotPoint = ComputePresetPivotPoint(Mesh, Preset);
	return ApplyVertexShift(Mesh, -PivotPoint, OutError);
}

bool FPivotOffsetLibrary::ResetToImportPivot(UStaticMesh* Mesh, FText& OutError)
{
	if (!Mesh)
	{
		OutError = NSLOCTEXT("PivotControlTool", "NoMesh", "No mesh to modify.");
		return false;
	}

	const FVector Accumulated = GetAccumulatedOffset(Mesh);
	if (Accumulated.IsNearlyZero())
	{
		return true;
	}

	// Accumulated tracks where the current pivot sits in import-space, so shifting
	// vertices by +Accumulated moves the origin back to the original import pivot.
	return ApplyVertexShift(Mesh, Accumulated, OutError);
}

FVector FPivotOffsetLibrary::GetAccumulatedOffset(const UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return FVector::ZeroVector;
	}
	// GetAssetUserData is non-const in the engine, but this read-only lookup is safe.
	if (const UPivotToolLiteOffsetData* Data = const_cast<UStaticMesh*>(Mesh)->GetAssetUserData<UPivotToolLiteOffsetData>())
	{
		return Data->AccumulatedOffset;
	}
	return FVector::ZeroVector;
}

FVector FPivotOffsetLibrary::GetPivotBoundsFraction(const UStaticMesh* Mesh)
{
	// The current origin is local (0,0,0).
	return GetBoundsFraction(Mesh, FVector::ZeroVector);
}

FVector FPivotOffsetLibrary::GetBoundsFraction(const UStaticMesh* Mesh, const FVector& LocalPoint)
{
	const FBox Bounds = ComputeLocalBounds(Mesh);
	if (!Bounds.IsValid)
	{
		return FVector(0.5, 0.5, 0.5);
	}

	const FVector Size = Bounds.GetSize();
	auto Fraction = [](double PointValue, double MinValue, double SizeValue) -> double
	{
		return FMath::IsNearlyZero(SizeValue) ? 0.5 : (PointValue - MinValue) / SizeValue;
	};

	return FVector(
		Fraction(LocalPoint.X, Bounds.Min.X, Size.X),
		Fraction(LocalPoint.Y, Bounds.Min.Y, Size.Y),
		Fraction(LocalPoint.Z, Bounds.Min.Z, Size.Z));
}

FVector FPivotOffsetLibrary::ComputeBoundsPoint(const UStaticMesh* Mesh, const FVector& Fraction)
{
	const FBox Bounds = ComputeLocalBounds(Mesh);
	if (!Bounds.IsValid)
	{
		return FVector::ZeroVector;
	}
	return Bounds.Min + Fraction * Bounds.GetSize();
}

bool FPivotOffsetLibrary::ApplyVertexShift(UStaticMesh* Mesh, const FVector& VertexShift, FText& OutError)
{
	// Pure translation is just a delta with no rotation.
	return ApplyVertexDelta(Mesh, FTransform(FQuat::Identity, VertexShift), /*bTrackTranslation*/ true, OutError);
}

bool FPivotOffsetLibrary::ApplyVertexDelta(UStaticMesh* Mesh, const FTransform& Delta, bool bTrackTranslation, FText& OutError)
{
	// Unscaled bake: with S = 1 the scaled-space formula reduces to P' = Q*P + d,
	// i.e. Delta applied directly.
	return ApplyPivotBake(Mesh, Delta.GetRotation(), Delta.GetTranslation(), FVector::OneVector, bTrackTranslation, OutError);
}

bool FPivotOffsetLibrary::ApplyPivotBake(UStaticMesh* Mesh, const FQuat& ScaledRotation, const FVector& ScaledTranslation, const FVector& ComponentScale, bool bTrackTranslation, FText& OutError)
{
	if (!Mesh)
	{
		OutError = NSLOCTEXT("PivotControlTool", "NoMesh", "No mesh to modify.");
		return false;
	}
	if (ScaledTranslation.IsNearlyZero() && ScaledRotation.IsIdentity())
	{
		return true;
	}
	if (FMath::Abs(ComponentScale.X) <= UE_KINDA_SMALL_NUMBER ||
		FMath::Abs(ComponentScale.Y) <= UE_KINDA_SMALL_NUMBER ||
		FMath::Abs(ComponentScale.Z) <= UE_KINDA_SMALL_NUMBER)
	{
		OutError = NSLOCTEXT("PivotControlTool", "ZeroScale", "The actor has a zero scale on one axis; set a non-zero scale first.");
		return false;
	}

	const FVector InvScale(1.0 / ComponentScale.X, 1.0 / ComponentScale.Y, 1.0 / ComponentScale.Z);
	// P' = S^-1 * (Q * (S * P) + d) — exact for any (even non-uniform) actor scale.
	auto MapPoint = [&ScaledRotation, &ScaledTranslation, &ComponentScale, &InvScale](const FVector& P)
	{
		return InvScale * (ScaledRotation.RotateVector(ComponentScale * P) + ScaledTranslation);
	};
	// Sanity: a degenerate input must never write garbage into the asset.
	if (MapPoint(FVector::ZeroVector).ContainsNaN())
	{
		OutError = NSLOCTEXT("PivotControlTool", "BadBake", "Could not compute a valid pivot transform for this actor.");
		return false;
	}

	// Directions (normals/tangents) must be rotated too, or lighting breaks (faces go
	// black) once the geometry is baked. The point map's linear part is
	// M = S^-1 * Q * S (row-vector convention: v' = v * M). Tangents transform by M;
	// normals by the inverse-transpose of M so they stay perpendicular under
	// non-uniform scale. Skip entirely for pure translation (rotation identity leaves
	// directions unchanged).
	const bool bRotatesDirections = !ScaledRotation.IsIdentity();
	const FMatrix PointLinear = FScaleMatrix(ComponentScale) * FQuatRotationMatrix(ScaledRotation) * FScaleMatrix(InvScale);
	const FMatrix NormalMatrix = PointLinear.Inverse().GetTransposed();
	auto MapNormal = [&NormalMatrix](const FVector3f& N)
	{
		const FVector Rotated(NormalMatrix.TransformVector(FVector(N)));
		return FVector3f(Rotated.GetSafeNormal());
	};
	auto MapTangent = [&PointLinear](const FVector3f& T)
	{
		const FVector Rotated(PointLinear.TransformVector(FVector(T)));
		return FVector3f(Rotated.GetSafeNormal());
	};

	Mesh->Modify();
	// CRITICAL for undo: the vertex data lives in separate per-LOD StaticMeshDescription
	// bulk-data sub-objects, which Modify() on the mesh alone does NOT capture in the
	// transaction. Without this, Ctrl+Z restores the actor's counter-moved transform but
	// leaves the baked vertices — the mesh visibly "moves" instead of the pivot reverting.
	Mesh->ModifyAllMeshDescriptions();

	const int32 NumSourceModels = Mesh->GetNumSourceModels();
	bool bAnyLodModified = false;

	for (int32 LODIndex = 0; LODIndex < NumSourceModels; ++LODIndex)
	{
		FMeshDescription* MeshDesc = Mesh->GetMeshDescription(LODIndex);
		if (!MeshDesc)
		{
			continue;
		}

		FStaticMeshAttributes Attributes(*MeshDesc);
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		for (const FVertexID VertexID : MeshDesc->Vertices().GetElementIDs())
		{
			Positions[VertexID] = FVector3f(MapPoint(FVector(Positions[VertexID])));
		}

		// Re-orient per-vertex-instance normals and tangents to match the rotation,
		// otherwise shading breaks (black faces) after the geometry is baked.
		if (bRotatesDirections)
		{
			TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
			TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
			for (const FVertexInstanceID InstanceID : MeshDesc->VertexInstances().GetElementIDs())
			{
				if (Normals.IsValid())
				{
					Normals[InstanceID] = MapNormal(Normals[InstanceID]);
				}
				if (Tangents.IsValid())
				{
					Tangents[InstanceID] = MapTangent(Tangents[InstanceID]);
				}
			}
		}

		Mesh->CommitMeshDescription(LODIndex);
		bAnyLodModified = true;
	}

	if (!bAnyLodModified)
	{
		OutError = NSLOCTEXT("PivotControlTool", "NoLODs", "Mesh has no editable LODs.");
		return false;
	}

	// Transform simple collision so it stays aligned with the new pivot.
	// Element rotations compose with Q (exact under uniform scale).
	if (UBodySetup* BodySetup = Mesh->GetBodySetup())
	{
		BodySetup->Modify();
		FKAggregateGeom& AggGeom = BodySetup->AggGeom;

		for (FKBoxElem& Elem : AggGeom.BoxElems)
		{
			Elem.Center = MapPoint(Elem.Center);
			Elem.Rotation = (ScaledRotation * Elem.Rotation.Quaternion()).Rotator();
		}
		for (FKSphereElem& Elem : AggGeom.SphereElems)
		{
			Elem.Center = MapPoint(Elem.Center);
		}
		for (FKSphylElem& Elem : AggGeom.SphylElems)
		{
			Elem.Center = MapPoint(Elem.Center);
			Elem.Rotation = (ScaledRotation * Elem.Rotation.Quaternion()).Rotator();
		}
		for (FKTaperedCapsuleElem& Elem : AggGeom.TaperedCapsuleElems)
		{
			Elem.Center = MapPoint(Elem.Center);
			Elem.Rotation = (ScaledRotation * Elem.Rotation.Quaternion()).Rotator();
		}
		for (FKConvexElem& Elem : AggGeom.ConvexElems)
		{
			for (FVector& Vertex : Elem.VertexData)
			{
				Vertex = MapPoint(Vertex);
			}
			Elem.UpdateElemBox();
		}

		BodySetup->InvalidatePhysicsData();
		BodySetup->CreatePhysicsMeshes();
	}

	// Transform sockets.
	for (UStaticMeshSocket* Socket : Mesh->Sockets)
	{
		if (Socket)
		{
			Socket->Modify();
			Socket->RelativeLocation = MapPoint(Socket->RelativeLocation);
			Socket->RelativeRotation = (ScaledRotation * Socket->RelativeRotation.Quaternion()).Rotator();
		}
	}

	// Track where the old origin ended up (the image of local zero) so Reset to
	// Import Pivot can undo pure moves; rotations aren't tracked.
	if (bTrackTranslation)
	{
		UPivotToolLiteOffsetData* OffsetData = Mesh->GetAssetUserData<UPivotToolLiteOffsetData>();
		if (!OffsetData)
		{
			OffsetData = NewObject<UPivotToolLiteOffsetData>(Mesh, NAME_None, RF_Public | RF_Transactional);
			Mesh->AddAssetUserData(OffsetData);
		}
		OffsetData->Modify();
		OffsetData->AccumulatedOffset -= InvScale * ScaledTranslation;
	}

	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	return true;
}

int32 FPivotOffsetLibrary::CountOtherUsagesInWorld(const UWorld* World, const UStaticMesh* Mesh, const TArray<UStaticMeshComponent*>& ExcludingComponents)
{
	int32 Count = 0;
	if (!World || !Mesh)
	{
		return 0;
	}

	for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		TArray<UStaticMeshComponent*> Components;
		It->GetComponents<UStaticMeshComponent>(Components);
		for (UStaticMeshComponent* Comp : Components)
		{
			if (Comp && Comp->GetStaticMesh() == Mesh && !ExcludingComponents.Contains(Comp))
			{
				++Count;
			}
		}
	}

	return Count;
}

int32 FPivotOffsetLibrary::CountExternalReferencers(const UStaticMesh* Mesh, FName CurrentLevelPackageName)
{
	if (!Mesh)
	{
		return 0;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FName> Referencers;
	AssetRegistryModule.Get().GetReferencers(Mesh->GetOutermost()->GetFName(), Referencers);

	int32 Count = 0;
	for (const FName& Referencer : Referencers)
	{
		if (Referencer != CurrentLevelPackageName)
		{
			++Count;
		}
	}
	return Count;
}

UStaticMesh* FPivotOffsetLibrary::DuplicateMeshForComponent(UStaticMeshComponent* Component, FText& OutError)
{
	if (!Component || !Component->GetStaticMesh())
	{
		OutError = NSLOCTEXT("PivotControlTool", "NoComponent", "No mesh component to duplicate onto.");
		return nullptr;
	}

	UStaticMesh* SourceMesh = Component->GetStaticMesh();
	const FString PackagePath = FPackageName::GetLongPackagePath(SourceMesh->GetOutermost()->GetName());
	const FString BaseName = SourceMesh->GetName();

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FString NewPackageName, NewAssetName;
	// Don't stack suffixes when duplicating an earlier duplicate (avoids
	// "SM_Crate_Pivot_Pivot"); CreateUniqueAssetName still appends a number.
	const FString Suffix = BaseName.Contains(TEXT("_Pivot")) ? FString() : TEXT("_Pivot");
	AssetToolsModule.Get().CreateUniqueAssetName(PackagePath / BaseName, Suffix, NewPackageName, NewAssetName);

	UObject* NewAsset = AssetToolsModule.Get().DuplicateAsset(NewAssetName, PackagePath, SourceMesh);
	UStaticMesh* NewMesh = Cast<UStaticMesh>(NewAsset);
	if (!NewMesh)
	{
		OutError = NSLOCTEXT("PivotControlTool", "DuplicateFailed", "Failed to duplicate the mesh asset.");
		return nullptr;
	}

	Component->Modify();
	Component->SetStaticMesh(NewMesh);

	return NewMesh;
}

USceneComponent* FPivotOffsetLibrary::EnsurePivotRoot(UStaticMeshComponent* Component, FText& OutError)
{
	if (!Component)
	{
		OutError = NSLOCTEXT("PivotControlTool", "NoComponent", "No mesh component to modify.");
		return nullptr;
	}

	AActor* Actor = Component->GetOwner();
	if (!Actor)
	{
		OutError = NSLOCTEXT("PivotControlTool", "NoOwner", "The mesh component has no owning actor.");
		return nullptr;
	}

	USceneComponent* Root = Actor->GetRootComponent();

	// A pivot root is already in place (the actor's root is something other than the
	// mesh itself) — reuse it so repeated edits keep moving the same pivot.
	if (Root && Root != Component)
	{
		return Root;
	}

	// Insert a dedicated scene-component root above the mesh. The mesh is re-attached
	// beneath it keeping its world transform, so nothing moves visually; only the
	// actor's pivot/gizmo is now driven by this new root.
	Actor->Modify();
	Component->Modify();

	USceneComponent* PivotRoot = NewObject<USceneComponent>(Actor, TEXT("FulcrumPivotRoot"), RF_Transactional);
	if (!PivotRoot)
	{
		OutError = NSLOCTEXT("PivotControlTool", "PivotRootFailed", "Could not create a pivot root for this actor.");
		return nullptr;
	}

	// Match the mesh's mobility so the editor doesn't warn about a static child under a
	// movable parent (or vice-versa).
	PivotRoot->SetMobility(Component->Mobility);
	PivotRoot->SetWorldLocationAndRotation(Component->GetComponentLocation(), Component->GetComponentQuat());
	PivotRoot->SetWorldScale3D(FVector::OneVector);

	Actor->AddInstanceComponent(PivotRoot);
	PivotRoot->RegisterComponent();
	Actor->SetRootComponent(PivotRoot);
	Component->AttachToComponent(PivotRoot, FAttachmentTransformRules::KeepWorldTransform);

	return PivotRoot;
}
