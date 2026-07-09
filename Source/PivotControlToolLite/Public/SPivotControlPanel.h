#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "PivotOffsetLibrary.h"

class UStaticMesh;
class UStaticMeshComponent;
class STextBlock;

// Fulcrum Lite panel: a stripped-down pivot tool. Just the box picker, a live target
// readout, and Undo / Redo / Reset to Original Pivot. No Advanced Options; the mesh is
// always kept in place (only the pivot moves).
class SPivotControlPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPivotControlPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<STextBlock> StatusText;

	// The last-clicked in-plane anchor, highlighted orange in the box picker and
	// echoed in the target readout.
	EPivotAnchor SelectedX = EPivotAnchor::Center;
	EPivotAnchor SelectedZ = EPivotAnchor::Center;

	// Live info-row values, bound as attributes so they track the editor selection.
	FText GetActorSummary() const;
	FText GetMeshSummary() const;
	FText GetOffsetSummary() const;

	// Picker dot supplies the X and Z anchors (depth is always centered in Lite).
	FReply OnPresetClicked(EPivotAnchor XAnchor, EPivotAnchor ZAnchor);

	// Colour for a box-picker dot: accent orange when selected, muted grey otherwise.
	FLinearColor GetDotColor(EPivotAnchor XAnchor, EPivotAnchor ZAnchor) const;

	// Plain-language "Target: Bottom-Left" readout under the picker.
	FText GetTargetSummary() const;

	FReply OnResetClicked();
	FReply OnUndoClicked();
	FReply OnRedoClicked();

	// Gathers one UStaticMeshComponent per currently-selected actor that has a mesh.
	TArray<UStaticMeshComponent*> GetSelectedMeshComponents() const;

	// Runs the shared-mesh-asset safety check for one component and, if needed, prompts
	// the user to duplicate or edit the shared asset in place. Returns the mesh that
	// should actually be modified (possibly a new duplicate), or nullptr if cancelled.
	UStaticMesh* ResolveMeshToModify(UStaticMeshComponent* Component, const TArray<UStaticMeshComponent*>& AllSelected);

	// Shared driver for the preset/reset buttons: resolves each selected component's
	// mesh (deduping shared meshes and running the safety check once per unique mesh),
	// applies Operation once per unique resolved mesh, and — because Lite always keeps
	// the mesh in place — counter-moves each affected actor by the applied shift so only
	// the pivot moves. Operation writes the local-space vertex shift it applied.
	void ApplyToSelection(const FText& TransactionLabel, const FText& SuccessFormat, TFunctionRef<bool(UStaticMesh*, FVector&, FText&)> Operation);

	void SetStatus(const FText& Message);
};
