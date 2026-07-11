// Copyright (c) 2026 Mihir Bathani. All Rights Reserved.

#include "SPivotControlPanel.h"
#include "PivotOffsetLibrary.h"

#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"

#include "Editor.h"
#include "Selection.h"
#include "ScopedTransaction.h"
#include "Misc/MessageDialog.h"
#include "HAL/PlatformProcess.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Level.h"

#define LOCTEXT_NAMESPACE "PivotControlToolLite"

namespace PivotToolStyle
{
	// Pull from the editor's own palette so the panel matches the active theme.
	static FLinearColor Accent()     { return FStyleColors::AccentOrange.GetSpecifiedColor(); }
	static FLinearColor DotMuted()   { return FLinearColor(0.42f, 0.42f, 0.42f, 1.0f); }
	static const FString WebsiteUrl(TEXT("https://www.mihirbathani.com"));

	static void OpenWebsite()
	{
		FPlatformProcess::LaunchURL(*WebsiteUrl, nullptr, nullptr);
	}
}

// A Details-panel style category: header bar + indented body.
static TSharedRef<SExpandableArea> MakeCategory(const FText& Title, TSharedRef<SWidget> Body, bool bCollapsed = false)
{
	return SNew(SExpandableArea)
		.InitiallyCollapsed(bCollapsed)
		.AreaTitle(Title)
		.AreaTitleFont(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
		.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryTop"))
		.BorderBackgroundColor(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)))
		.HeaderPadding(FMargin(6.0f, 4.0f))
		.Padding(FMargin(10.0f, 6.0f, 10.0f, 8.0f))
		.BodyContent()
		[
			Body
		];
}

void SPivotControlPanel::Construct(const FArguments& InArgs)
{
	// ---- A Details-style info row: muted label on the left, value on the right ----
	auto MakeInfoRow = [](const FText& Label, TAttribute<FText> Value) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 2.0f)
			[
				SNew(SBox).WidthOverride(58.0f)
				[
					SNew(STextBlock)
					.Text(Label)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
					.ColorAndOpacity(FSlateColor(FStyleColors::Foreground))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(4.0f, 2.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(Value)
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
				.ColorAndOpacity(FSlateColor(FStyleColors::White))
				.AutoWrapText(true)
			];
	};

	// ---- Box picker: a bounding-box outline with nine clickable anchor dots ----
	// Mirrors Modeling Mode's pivot picker. Each dot sets the in-plane X/Z anchor;
	// depth is always centered in Lite.
	auto MakeDot = [this](EPivotAnchor XAnchor, EPivotAnchor ZAnchor, const FText& Tooltip) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
			.ContentPadding(FMargin(3.0f))
			.ToolTipText(Tooltip)
			.OnClicked(this, &SPivotControlPanel::OnPresetClicked, XAnchor, ZAnchor)
			[
				SNew(SBox).WidthOverride(12.0f).HeightOverride(12.0f)
				[
					SNew(SColorBlock)
					.Color(TAttribute<FLinearColor>::CreateSP(this, &SPivotControlPanel::GetDotColor, XAnchor, ZAnchor))
				]
			];
	};

	auto DotH = [](EPivotAnchor A) { return A == EPivotAnchor::Min ? HAlign_Left : A == EPivotAnchor::Max ? HAlign_Right : HAlign_Center; };
	auto DotV = [](EPivotAnchor A) { return A == EPivotAnchor::Max ? VAlign_Top  : A == EPivotAnchor::Min ? VAlign_Bottom : VAlign_Center; };

	TSharedRef<SUniformGridPanel> DotGrid = SNew(SUniformGridPanel);
	auto AddDot = [&](int32 Col, int32 Row, EPivotAnchor X, EPivotAnchor Z, const FText& Tip)
	{
		DotGrid->AddSlot(Col, Row).HAlign(DotH(X)).VAlign(DotV(Z))[ MakeDot(X, Z, Tip) ];
	};
	AddDot(0, 0, EPivotAnchor::Min,    EPivotAnchor::Max,    LOCTEXT("TLTip", "Top-left"));
	AddDot(1, 0, EPivotAnchor::Center, EPivotAnchor::Max,    LOCTEXT("TCTip", "Top-center"));
	AddDot(2, 0, EPivotAnchor::Max,    EPivotAnchor::Max,    LOCTEXT("TRTip", "Top-right"));
	AddDot(0, 1, EPivotAnchor::Min,    EPivotAnchor::Center, LOCTEXT("CLTip", "Center-left"));
	AddDot(1, 1, EPivotAnchor::Center, EPivotAnchor::Center, LOCTEXT("CTip",  "Center"));
	AddDot(2, 1, EPivotAnchor::Max,    EPivotAnchor::Center, LOCTEXT("CRTip", "Center-right"));
	AddDot(0, 2, EPivotAnchor::Min,    EPivotAnchor::Min,    LOCTEXT("BLTip", "Bottom-left"));
	AddDot(1, 2, EPivotAnchor::Center, EPivotAnchor::Min,    LOCTEXT("BCTip", "Bottom-center"));
	AddDot(2, 2, EPivotAnchor::Max,    EPivotAnchor::Min,    LOCTEXT("BRTip", "Bottom-right"));

	TSharedRef<SWidget> BoxPicker =
		SNew(SBox).WidthOverride(150.0f).HeightOverride(150.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.0f))
			[
				DotGrid
			]
		];

	// ---- Live target readout: spells out exactly where the pivot will land ----
	TSharedRef<SWidget> TargetReadout =
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(FMargin(8.0f, 5.0f))
		[
			SNew(STextBlock)
			.Text(TAttribute<FText>::CreateSP(this, &SPivotControlPanel::GetTargetSummary))
			.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
			.ColorAndOpacity(FSlateColor(FStyleColors::White))
		];

	TSharedRef<SWidget> PivotBody =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PickerHint", "Pick where the pivot sits on the mesh bounds."))
			.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
			.ColorAndOpacity(FSlateColor(FStyleColors::Foreground))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ BoxPicker ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[ TargetReadout ];

	// ---- Selection category body ----
	TSharedRef<SWidget> SelectionBody =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("ActorLabel", "Actor"),  TAttribute<FText>::CreateSP(this, &SPivotControlPanel::GetActorSummary)) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("MeshLabel",  "Mesh"),   TAttribute<FText>::CreateSP(this, &SPivotControlPanel::GetMeshSummary)) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("OffsetLabel","Pivot"),  TAttribute<FText>::CreateSP(this, &SPivotControlPanel::GetOffsetSummary)) ];

	// ---- Actions category body: Reset + Undo / Redo only ----
	TSharedRef<SWidget> ActionsBody =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(6.0f))
			.ToolTipText(LOCTEXT("ResetTip", "Restore the pivot to where it was when the mesh was imported"))
			.Text(LOCTEXT("ResetToOriginal", "Reset to Original Pivot"))
			.OnClicked(this, &SPivotControlPanel::OnResetClicked)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(6.0f))
				.Text(LOCTEXT("Undo", "Undo")).OnClicked(this, &SPivotControlPanel::OnUndoClicked)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(6.0f))
				.Text(LOCTEXT("Redo", "Redo")).OnClicked(this, &SPivotControlPanel::OnRedoClicked)
			]
		];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(FMargin(0.0f))
		[
			SNew(SVerticalBox)

			// ---- Compact native header: brand dot + name, subtitle at right ----
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
				.Padding(FMargin(10.0f, 7.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(9.0f).HeightOverride(9.0f)
						[
							SNew(SColorBlock).Color(PivotToolStyle::Accent())
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(7.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "Fulcrum Lite"))
						.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
						.ColorAndOpacity(FSlateColor(FStyleColors::White))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Subtitle", "Pivot tools"))
						.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
						.ColorAndOpacity(FSlateColor(FStyleColors::Foreground))
					]
				]
			]

			// ---- Scrollable body (native Details-panel behaviour) ----
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[ MakeCategory(LOCTEXT("CatSelection", "Selection"),      SelectionBody) ]
				+ SScrollBox::Slot()[ MakeCategory(LOCTEXT("CatPivot",     "Pivot Position"), PivotBody) ]
				+ SScrollBox::Slot()[ MakeCategory(LOCTEXT("CatActions",   "Actions"),        ActionsBody) ]
			]

			// ---- Status bar (pinned, native footer style) ----
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
				.Padding(FMargin(10.0f, 5.0f))
				[
					SAssignNew(StatusText, STextBlock)
					.AutoWrapText(true)
					.Text(LOCTEXT("Ready", "Select an actor, then choose a pivot position."))
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
					.ColorAndOpacity(FSlateColor(FStyleColors::Foreground))
				]
			]

			// ---- Footer branding (pinned) ----
			+ SVerticalBox::Slot().AutoHeight().Padding(10.0f, 6.0f, 10.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MadeBy", "Made by Mihir Bathani  \x2022  "))
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
					.ColorAndOpacity(FSlateColor(FStyleColors::Foreground))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SHyperlink)
					.Text(LOCTEXT("WebsiteShort", "mihirbathani.com"))
					.ToolTipText(LOCTEXT("WebsiteTip", "Open mihirbathani.com"))
					.OnNavigate_Static(&PivotToolStyle::OpenWebsite)
				]
			]
		]
	];
}

FText SPivotControlPanel::GetActorSummary() const
{
	TArray<UStaticMeshComponent*> Components = GetSelectedMeshComponents();
	if (Components.Num() == 0)
	{
		return LOCTEXT("NothingSelected", "\x2014 nothing selected \x2014");
	}
	if (Components.Num() == 1)
	{
		AActor* Owner = Components[0]->GetOwner();
		return FText::FromString(Owner ? Owner->GetActorNameOrLabel() : TEXT("Actor"));
	}
	return FText::Format(LOCTEXT("NActors", "{0} actors selected"), FText::AsNumber(Components.Num()));
}

FText SPivotControlPanel::GetMeshSummary() const
{
	TArray<UStaticMeshComponent*> Components = GetSelectedMeshComponents();
	if (Components.Num() == 0)
	{
		return FText::GetEmpty();
	}

	TSet<UStaticMesh*> UniqueMeshes;
	for (UStaticMeshComponent* Comp : Components)
	{
		UniqueMeshes.Add(Comp->GetStaticMesh());
	}

	if (UniqueMeshes.Num() == 1)
	{
		const UStaticMesh* Mesh = Components[0]->GetStaticMesh();
		return FText::FromString(Mesh ? Mesh->GetName() : TEXT("none"));
	}
	return FText::Format(LOCTEXT("NMeshes", "{0} unique meshes"), FText::AsNumber(UniqueMeshes.Num()));
}

FText SPivotControlPanel::GetOffsetSummary() const
{
	TArray<UStaticMeshComponent*> Components = GetSelectedMeshComponents();
	if (Components.Num() != 1)
	{
		return LOCTEXT("OffsetDash", "\x2014");
	}
	UStaticMesh* Mesh = Components[0]->GetStaticMesh();
	if (!Mesh)
	{
		return LOCTEXT("OffsetDash", "\x2014");
	}
	const FVector Offset = FPivotOffsetLibrary::GetAccumulatedOffset(Mesh);
	if (Offset.IsNearlyZero())
	{
		return LOCTEXT("OffsetOriginal", "original (import)");
	}
	return FText::FromString(FString::Printf(TEXT("X %.1f   Y %.1f   Z %.1f"), Offset.X, Offset.Y, Offset.Z));
}

TArray<UStaticMeshComponent*> SPivotControlPanel::GetSelectedMeshComponents() const
{
	TArray<UStaticMeshComponent*> Result;
	if (!GEditor)
	{
		return Result;
	}

	USelection* Selection = GEditor->GetSelectedActors();
	TArray<AActor*> SelectedActors;
	Selection->GetSelectedObjects<AActor>(SelectedActors);

	for (AActor* Actor : SelectedActors)
	{
		if (!Actor)
		{
			continue;
		}
		UStaticMeshComponent* Comp = Actor->FindComponentByClass<UStaticMeshComponent>();
		if (Comp && Comp->GetStaticMesh())
		{
			Result.Add(Comp);
		}
	}

	return Result;
}

UStaticMesh* SPivotControlPanel::ResolveMeshToModify(UStaticMeshComponent* Component, const TArray<UStaticMeshComponent*>& AllSelected)
{
	UStaticMesh* Mesh = Component->GetStaticMesh();
	if (!Mesh)
	{
		return nullptr;
	}

	UWorld* World = Component->GetWorld();
	const int32 OtherInLevel = FPivotOffsetLibrary::CountOtherUsagesInWorld(World, Mesh, AllSelected);

	FName LevelPackageName = NAME_None;
	if (World && World->PersistentLevel)
	{
		LevelPackageName = World->PersistentLevel->GetOutermost()->GetFName();
	}
	const int32 ExternalReferencers = FPivotOffsetLibrary::CountExternalReferencers(Mesh, LevelPackageName);

	if (OtherInLevel <= 0 && ExternalReferencers <= 0)
	{
		return Mesh;
	}

	const FText Message = FText::Format(
		LOCTEXT("SharedMeshWarning",
			"Mesh Used Elsewhere\n\n"
			"'{0}' appears to be used elsewhere ({1} other placement(s) in this level, {2} other referencing asset(s)).\n\n"
			"Editing it here will move the pivot for ALL of them.\n\n"
			"Choose Yes to duplicate the mesh so only this actor is affected, No to edit the shared asset anyway, or Cancel to skip it."),
		FText::FromString(Mesh->GetName()),
		FText::AsNumber(OtherInLevel),
		FText::AsNumber(ExternalReferencers));

	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::YesNoCancel, Message);

	if (Choice == EAppReturnType::Cancel)
	{
		return nullptr;
	}
	if (Choice == EAppReturnType::No)
	{
		return Mesh;
	}

	FText Error;
	UStaticMesh* NewMesh = FPivotOffsetLibrary::DuplicateMeshForComponent(Component, Error);
	if (!NewMesh)
	{
		SetStatus(Error);
		return nullptr;
	}
	return NewMesh;
}

void SPivotControlPanel::ApplyToSelection(const FText& TransactionLabel, const FText& SuccessFormat, TFunctionRef<bool(UStaticMesh*, FVector&, FText&)> Operation)
{
	TArray<UStaticMeshComponent*> Components = GetSelectedMeshComponents();
	if (Components.Num() == 0)
	{
		SetStatus(LOCTEXT("NoSelectionStatus", "Select one or more actors with a static mesh first."));
		return;
	}

	FScopedTransaction Transaction(TransactionLabel);

	TMap<UStaticMesh*, UStaticMesh*> ResolvedMeshCache;
	TSet<UStaticMesh*> CancelledOriginals;
	// Resolved mesh -> every selected component now using it, so we can counter-move
	// each affected actor after the shift (Lite always keeps the mesh in place).
	TMap<UStaticMesh*, TArray<UStaticMeshComponent*>> MeshToComponents;

	for (UStaticMeshComponent* Comp : Components)
	{
		UStaticMesh* OriginalMesh = Comp->GetStaticMesh();
		if (!OriginalMesh || CancelledOriginals.Contains(OriginalMesh))
		{
			continue;
		}

		UStaticMesh* Resolved = nullptr;
		if (UStaticMesh** Cached = ResolvedMeshCache.Find(OriginalMesh))
		{
			Resolved = *Cached;
			if (Resolved != OriginalMesh)
			{
				Comp->Modify();
				Comp->SetStaticMesh(Resolved);
			}
		}
		else
		{
			Resolved = ResolveMeshToModify(Comp, Components);
			if (!Resolved)
			{
				CancelledOriginals.Add(OriginalMesh);
				continue;
			}
			ResolvedMeshCache.Add(OriginalMesh, Resolved);
		}

		MeshToComponents.FindOrAdd(Resolved).Add(Comp);
	}

	int32 SuccessCount = 0;
	int32 FailCount = 0;
	bool bMovedActors = false;
	for (const TPair<UStaticMesh*, TArray<UStaticMeshComponent*>>& Pair : MeshToComponents)
	{
		UStaticMesh* Mesh = Pair.Key;
		FVector AppliedShift = FVector::ZeroVector;
		FText Error;
		if (Operation(Mesh, AppliedShift, Error))
		{
			++SuccessCount;

			// Counter-move each affected actor so the mesh stays put in the level; the
			// vertices moved by +AppliedShift (local), so the geometry moved by
			// +TransformVector(AppliedShift) in world — cancel it out. Lite always keeps
			// the mesh in place, so this is unconditional.
			if (!AppliedShift.IsNearlyZero())
			{
				for (UStaticMeshComponent* Comp : Pair.Value)
				{
					if (Comp)
					{
						const FVector WorldDelta = Comp->GetComponentTransform().TransformVector(AppliedShift);
						Comp->Modify();
						Comp->AddWorldOffset(-WorldDelta);

						// Refresh the editor so the pivot gizmo and Details panel jump to
						// the new location instead of looking like nothing happened.
						if (AActor* Owner = Comp->GetOwner())
						{
							Owner->PostEditMove(true);
						}
						bMovedActors = true;
					}
				}
			}
		}
		else
		{
			++FailCount;
			SetStatus(Error);
		}
	}

	if (bMovedActors && GEditor)
	{
		// Recompute the selection's pivot so the transform gizmo and Details panel jump
		// to the new location, then redraw.
		GEditor->NoteSelectionChange();
		GEditor->RedrawLevelEditingViewports(true);
	}

	if (FailCount == 0 && SuccessCount > 0)
	{
		SetStatus(FText::Format(SuccessFormat, FText::AsNumber(SuccessCount)));
	}
	else if (SuccessCount == 0 && FailCount == 0)
	{
		SetStatus(LOCTEXT("CancelledStatus", "No changes made."));
	}
}

FReply SPivotControlPanel::OnPresetClicked(EPivotAnchor XAnchor, EPivotAnchor ZAnchor)
{
	SelectedX = XAnchor;
	SelectedZ = ZAnchor;

	// Depth is always centered in Lite.
	const FPivotPreset Preset(XAnchor, EPivotAnchor::Center, ZAnchor);

	ApplyToSelection(
		LOCTEXT("ApplyPresetTransaction", "Change Mesh Pivot"),
		LOCTEXT("AppliedStatus", "Pivot updated on {0} mesh(es)."),
		[Preset](UStaticMesh* Mesh, FVector& OutShift, FText& OutError)
		{
			OutShift = -FPivotOffsetLibrary::ComputePresetPivotPoint(Mesh, Preset);
			return FPivotOffsetLibrary::ApplyPreset(Mesh, Preset, OutError);
		});
	return FReply::Handled();
}

FLinearColor SPivotControlPanel::GetDotColor(EPivotAnchor XAnchor, EPivotAnchor ZAnchor) const
{
	const bool bSelected = (XAnchor == SelectedX && ZAnchor == SelectedZ);
	return bSelected ? PivotToolStyle::Accent() : PivotToolStyle::DotMuted();
}

FText SPivotControlPanel::GetTargetSummary() const
{
	// Plain-language name for an in-plane anchor pair, e.g. "Bottom-Left", "Top".
	auto PlaneLabel = [](EPivotAnchor X, EPivotAnchor Z) -> FText
	{
		const bool bLeft  = X == EPivotAnchor::Min;
		const bool bRight = X == EPivotAnchor::Max;
		const bool bTop   = Z == EPivotAnchor::Max;
		const bool bBot   = Z == EPivotAnchor::Min;

		if (bTop && bLeft)  return LOCTEXT("PL_TL", "Top-Left");
		if (bTop && bRight) return LOCTEXT("PL_TR", "Top-Right");
		if (bBot && bLeft)  return LOCTEXT("PL_BL", "Bottom-Left");
		if (bBot && bRight) return LOCTEXT("PL_BR", "Bottom-Right");
		if (bTop)           return LOCTEXT("PL_T",  "Top");
		if (bBot)           return LOCTEXT("PL_B",  "Bottom");
		if (bLeft)          return LOCTEXT("PL_L",  "Left");
		if (bRight)         return LOCTEXT("PL_R",  "Right");
		return LOCTEXT("PL_C", "Center");
	};

	return FText::Format(LOCTEXT("TargetFmtPlain", "Target: {0}"), PlaneLabel(SelectedX, SelectedZ));
}

FReply SPivotControlPanel::OnResetClicked()
{
	ApplyToSelection(
		LOCTEXT("ResetTransaction", "Reset Mesh Pivot"),
		LOCTEXT("ResetStatus", "Reset {0} mesh(es) to original pivot."),
		[](UStaticMesh* Mesh, FVector& OutShift, FText& OutError)
		{
			// ResetToImportPivot shifts vertices by +AccumulatedOffset; capture it before
			// the call (afterwards the accumulated offset is cleared).
			OutShift = FPivotOffsetLibrary::GetAccumulatedOffset(Mesh);
			return FPivotOffsetLibrary::ResetToImportPivot(Mesh, OutError);
		});
	return FReply::Handled();
}

FReply SPivotControlPanel::OnUndoClicked()
{
	if (GEditor)
	{
		GEditor->UndoTransaction();
	}
	return FReply::Handled();
}

FReply SPivotControlPanel::OnRedoClicked()
{
	if (GEditor)
	{
		GEditor->RedoTransaction();
	}
	return FReply::Handled();
}

void SPivotControlPanel::SetStatus(const FText& Message)
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(Message);
	}
}

#undef LOCTEXT_NAMESPACE
