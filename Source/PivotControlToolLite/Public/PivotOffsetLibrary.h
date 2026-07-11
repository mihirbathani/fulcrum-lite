// Copyright (c) 2026 Mihir Bathani. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class AActor;

// Which side of the bounding box the pivot snaps to along a single axis.
enum class EPivotAnchor : uint8
{
	Min,     // minimum extent (Left / Bottom / Front)
	Center,  // midpoint
	Max      // maximum extent (Right / Top / Back)
};

// A pivot preset expressed as an independent anchor on each axis, so any of the
// 27 box positions (including all 8 true 3D corners) can be described.
//
// Axis convention: X = Left(Min)/Right(Max), Z = Bottom(Min)/Top(Max),
// Y = Front(Min)/Back(Max) depth.
struct FPivotPreset
{
	EPivotAnchor X = EPivotAnchor::Center;
	EPivotAnchor Y = EPivotAnchor::Center;
	EPivotAnchor Z = EPivotAnchor::Center;

	FPivotPreset() = default;
	FPivotPreset(EPivotAnchor InX, EPivotAnchor InY, EPivotAnchor InZ)
		: X(InX), Y(InY), Z(InZ) {}
};

// Core geometry logic for Pivot Control Tool: computing preset pivot points and
// shifting a Static Mesh's vertex, collision, and socket data to match.
class FPivotOffsetLibrary
{
public:
	// Computes the local-space point (based on the mesh's LOD0 bounds) that the
	// given preset wants to become the new origin.
	static FVector ComputePresetPivotPoint(const UStaticMesh* Mesh, const FPivotPreset& Preset);

	// Applies the given preset to Mesh: shifts vertices (all LODs), collision, and
	// sockets so the preset's pivot point becomes the new local origin.
	static bool ApplyPreset(UStaticMesh* Mesh, const FPivotPreset& Preset, FText& OutError);

	// Reverts all accumulated Pivot Control Tool offsets, restoring the pivot to
	// where it was at import time.
	static bool ResetToImportPivot(UStaticMesh* Mesh, FText& OutError);

	// Low-level: shifts every vertex (all LODs), collision element, and socket on
	// Mesh by VertexShift, and updates the mesh's accumulated-offset tracking data.
	static bool ApplyVertexShift(UStaticMesh* Mesh, const FVector& VertexShift, FText& OutError);

	// Bakes an arbitrary local-space transform (rotation + translation) into the mesh:
	// every vertex (all LODs), collision element, and socket is transformed by Delta.
	// When bTrackTranslation is true the translation part updates accumulated-offset
	// tracking (Reset to Import undoes moves; rotations are not tracked).
	static bool ApplyVertexDelta(UStaticMesh* Mesh, const FTransform& Delta, bool bTrackTranslation, FText& OutError);

	// The general bake, done in the component's SCALED space so it stays exact for
	// any actor scale (including non-uniform): every point P becomes
	//   P' = S^-1 * (Q * (S * P) + d)
	// where S is ComponentScale, Q is ScaledRotation and d is ScaledTranslation.
	// This lets a world-axis pivot rotation/move bake correctly into a mesh used by
	// a scaled or rotated actor without shearing artifacts in world space.
	// Collision element rotations and socket rotations are composed with Q (exact
	// under uniform scale). Fails cleanly on a near-zero scale axis.
	static bool ApplyPivotBake(UStaticMesh* Mesh, const FQuat& ScaledRotation, const FVector& ScaledTranslation, const FVector& ComponentScale, bool bTrackTranslation, FText& OutError);

	// Returns the accumulated offset Pivot Control Tool has applied to this mesh
	// since import (zero vector if none).
	static FVector GetAccumulatedOffset(const UStaticMesh* Mesh);

	// Fraction (0..1 per axis) describing where the current local origin sits inside
	// the mesh's bounding box. Copy Pivot stores this so a pivot's relative placement
	// can be pasted onto differently-sized meshes. Zero-size axes report 0.5.
	static FVector GetPivotBoundsFraction(const UStaticMesh* Mesh);

	// Fraction (0..1 per axis) of an arbitrary local-space point within the mesh's
	// bounding box (inverse of ComputeBoundsPoint). Zero-size axes report 0.5.
	static FVector GetBoundsFraction(const UStaticMesh* Mesh, const FVector& LocalPoint);

	// The local-space point at the given 0..1 bounds fraction (inverse of the above).
	static FVector ComputeBoundsPoint(const UStaticMesh* Mesh, const FVector& Fraction);

	// Number of OTHER static mesh components (excluding those in ExcludingComponents)
	// in the given world that reference Mesh.
	static int32 CountOtherUsagesInWorld(const UWorld* World, const UStaticMesh* Mesh, const TArray<UStaticMeshComponent*>& ExcludingComponents);

	// Number of external asset-registry referencers (blueprints/levels/etc, project-wide)
	// of Mesh's package, beyond CurrentLevelPackageName (pass NAME_None to count all).
	static int32 CountExternalReferencers(const UStaticMesh* Mesh, FName CurrentLevelPackageName);

	// Duplicates Mesh into a new uniquely-named asset alongside the original and
	// reassigns Component to use it. Returns the new mesh, or nullptr on failure.
	static UStaticMesh* DuplicateMeshForComponent(UStaticMeshComponent* Component, FText& OutError);

	// Non-destructive pivot support: guarantees Component's owning actor has a dedicated
	// scene-component root (the movable "pivot") that the mesh hangs beneath, inserting
	// one (attaching the mesh under it, world transform preserved) the first time. This
	// lets the pivot/gizmo be repositioned by moving the root — no mesh asset is touched.
	// Idempotent: returns the existing pivot root if one is already in place. Returns the
	// pivot-root scene component, or nullptr on failure.
	static USceneComponent* EnsurePivotRoot(UStaticMeshComponent* Component, FText& OutError);

private:
	static FBox ComputeLocalBounds(const UStaticMesh* Mesh);
};
