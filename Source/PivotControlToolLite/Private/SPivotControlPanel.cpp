// Copyright (c) 2026 Mihir Bathani. All Rights Reserved.

#include "SPivotControlPanel.h"
#include "PivotOffsetLibrary.h"

#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateStyleRegistry.h"

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

// An all-caps section label with a rule running off to the right, e.g. "PRESETS ———".
static TSharedRef<SWidget> MakeSectionHeader(const FText& Title)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Title)
			.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
			.ColorAndOpacity(FSlateColor(FStyleColors::Foreground))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SSeparator).Thickness(1.0f)
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

	// ---- Preset buttons: a 3x3 grid of labelled anchor tiles ----
	// Each tile draws a miniature bounding box with a dot marking exactly where the
	// pivot will land, above a plain-language label. The active preset is filled with
	// the accent colour so the current choice reads at a glance.
	auto DotH = [](EPivotAnchor A) { return A == EPivotAnchor::Min ? HAlign_Left : A == EPivotAnchor::Max ? HAlign_Right : HAlign_Center; };
	auto DotV = [](EPivotAnchor A) { return A == EPivotAnchor::Max ? VAlign_Top  : A == EPivotAnchor::Min ? VAlign_Bottom : VAlign_Center; };

	auto MakePresetTile = [this, &DotH, &DotV](EPivotAnchor X, EPivotAnchor Z, const FText& Label, const FText& Tooltip) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ContentPadding(FMargin(4.0f, 7.0f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ToolTipText(Tooltip)
			.ButtonColorAndOpacity(TAttribute<FSlateColor>::CreateSP(this, &SPivotControlPanel::GetPresetButtonColor, X, Z))
			.OnClicked(this, &SPivotControlPanel::OnPresetClicked, X, Z)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(24.0f).HeightOverride(24.0f)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
						.Padding(FMargin(3.0f))
						[
							SNew(SBox).HAlign(DotH(X)).VAlign(DotV(Z))
							[
								SNew(SBox).WidthOverride(7.0f).HeightOverride(7.0f)
								[
									SNew(SColorBlock)
									.Color(TAttribute<FLinearColor>::CreateSP(this, &SPivotControlPanel::GetDotColor, X, Z))
								]
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 5.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(Label)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
					.ColorAndOpacity(TAttribute<FSlateColor>::CreateSP(this, &SPivotControlPanel::GetPresetLabelColor, X, Z))
				]
			];
	};

	TSharedRef<SUniformGridPanel> PresetGrid = SNew(SUniformGridPanel).SlotPadding(FMargin(3.0f));
	auto AddPreset = [&](int32 Col, int32 Row, EPivotAnchor X, EPivotAnchor Z, const FText& Label, const FText& Tip)
	{
		PresetGrid->AddSlot(Col, Row)[ MakePresetTile(X, Z, Label, Tip) ];
	};
	AddPreset(0, 0, EPivotAnchor::Min,    EPivotAnchor::Max,    LOCTEXT("TL", "Top Left"),      LOCTEXT("TLTip", "Pivot to the top-left of the mesh bounds"));
	AddPreset(1, 0, EPivotAnchor::Center, EPivotAnchor::Max,    LOCTEXT("TC", "Top Center"),    LOCTEXT("TCTip", "Pivot to the top-center of the mesh bounds"));
	AddPreset(2, 0, EPivotAnchor::Max,    EPivotAnchor::Max,    LOCTEXT("TR", "Top Right"),     LOCTEXT("TRTip", "Pivot to the top-right of the mesh bounds"));
	AddPreset(0, 1, EPivotAnchor::Min,    EPivotAnchor::Center, LOCTEXT("CL", "Left Center"),   LOCTEXT("CLTip", "Pivot to the left-center of the mesh bounds"));
	AddPreset(1, 1, EPivotAnchor::Center, EPivotAnchor::Center, LOCTEXT("C",  "Center"),        LOCTEXT("CTip",  "Pivot to the center of the mesh bounds"));
	AddPreset(2, 1, EPivotAnchor::Max,    EPivotAnchor::Center, LOCTEXT("CR", "Right Center"),  LOCTEXT("CRTip", "Pivot to the right-center of the mesh bounds"));
	AddPreset(0, 2, EPivotAnchor::Min,    EPivotAnchor::Min,    LOCTEXT("BL", "Bottom Left"),   LOCTEXT("BLTip", "Pivot to the bottom-left of the mesh bounds"));
	AddPreset(1, 2, EPivotAnchor::Center, EPivotAnchor::Min,    LOCTEXT("BC", "Bottom Center"), LOCTEXT("BCTip", "Pivot to the bottom-center of the mesh bounds"));
	AddPreset(2, 2, EPivotAnchor::Max,    EPivotAnchor::Min,    LOCTEXT("BR", "Bottom Right"),  LOCTEXT("BRTip", "Pivot to the bottom-right of the mesh bounds"));

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

	// ---- Selection rows ----
	TSharedRef<SWidget> SelectionBody =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("ActorLabel", "Actor"),  TAttribute<FText>::CreateSP(this, &SPivotControlPanel::GetActorSummary)) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("MeshLabel",  "Mesh"),   TAttribute<FText>::CreateSP(this, &SPivotControlPanel::GetMeshSummary)) ]
		+ SVerticalBox::Slot().AutoHeight()[ MakeInfoRow(LOCTEXT("OffsetLabel","Pivot"),  TAttribute<FText>::CreateSP(this, &SPivotControlPanel::GetOffsetSummary)) ];

	// ---- Action tiles: icon over label, matching the preset tiles ----
	auto MakeActionTile = [](const FName IconName, const FText& Label, const FText& Tooltip, FOnClicked OnClicked) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ContentPadding(FMargin(4.0f, 7.0f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ToolTipText(Tooltip)
			.OnClicked(OnClicked)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)
					[
						SNew(SImage).Image(FAppStyle::Get().GetBrush(IconName))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 5.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(Label)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
				]
			];
	};

	TSharedRef<SWidget> ActionsBody =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)
		[
			MakeActionTile("Icons.Refresh", LOCTEXT("ResetToOriginal", "Reset Pivot"),
				LOCTEXT("ResetTip", "Restore the pivot to where it was when the mesh was imported"),
				FOnClicked::CreateSP(this, &SPivotControlPanel::OnResetClicked))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 3.0f, 0.0f)
		[
			MakeActionTile("GenericCommands.Undo", LOCTEXT("Undo", "Undo"),
				LOCTEXT("UndoTip", "Undo the last pivot change"),
				FOnClicked::CreateSP(this, &SPivotControlPanel::OnUndoClicked))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)
		[
			MakeActionTile("GenericCommands.Redo", LOCTEXT("Redo", "Redo"),
				LOCTEXT("RedoTip", "Redo the last undone pivot change"),
				FOnClicked::CreateSP(this, &SPivotControlPanel::OnRedoClicked))
		];

	// ---- Branded header: plugin logo, product name, tagline ----
	const ISlateStyle* FulcrumStyle = FSlateStyleRegistry::FindSlateStyle("FulcrumLiteStyle");
	const FSlateBrush* LogoBrush = FulcrumStyle ? FulcrumStyle->GetBrush("FulcrumLite.Logo") : nullptr;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(FMargin(0.0f))
		[
			SNew(SVerticalBox)

			// ---- Branded header: logo, product name, tagline ----
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
				.Padding(FMargin(12.0f, 10.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(40.0f).HeightOverride(40.0f)
						[
							SNew(SImage).Image(LogoBrush)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Title", "FULCRUM LITE"))
							.Font(FAppStyle::Get().GetFontStyle("HeadingSmall"))
							.ColorAndOpacity(FSlateColor(FStyleColors::White))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Subtitle", "One-Click Pivot Editor"))
							.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
							.ColorAndOpacity(FSlateColor(FStyleColors::Foreground))
						]
					]
				]
			]

			// ---- Scrollable body: flat sections with all-caps headers ----
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot().Padding(12.0f, 0.0f, 12.0f, 12.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 6.0f)
					[ MakeSectionHeader(LOCTEXT("SecSelection", "SELECTION")) ]
					+ SVerticalBox::Slot().AutoHeight()[ SelectionBody ]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 6.0f)
					[ MakeSectionHeader(LOCTEXT("SecPresets", "PRESETS")) ]
					+ SVerticalBox::Slot().AutoHeight()[ PresetGrid ]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)[ TargetReadout ]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 6.0f)
					[ MakeSectionHeader(LOCTEXT("SecActions", "ACTIONS")) ]
					+ SVerticalBox::Slot().AutoHeight()[ ActionsBody ]
				]
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
	// On the highlighted (orange) tile the dot switches to white so it stays visible
	// against the accent fill.
	const bool bSelected = (XAnchor == SelectedX && ZAnchor == SelectedZ);
	return bSelected ? FLinearColor::White : PivotToolStyle::Accent();
}

FSlateColor SPivotControlPanel::GetPresetButtonColor(EPivotAnchor XAnchor, EPivotAnchor ZAnchor) const
{
	const bool bSelected = (XAnchor == SelectedX && ZAnchor == SelectedZ);
	return bSelected ? FSlateColor(FStyleColors::AccentOrange) : FSlateColor(FLinearColor::White);
}

FSlateColor SPivotControlPanel::GetPresetLabelColor(EPivotAnchor XAnchor, EPivotAnchor ZAnchor) const
{
	const bool bSelected = (XAnchor == SelectedX && ZAnchor == SelectedZ);
	return bSelected ? FSlateColor(FLinearColor::White) : FSlateColor(FStyleColors::Foreground);
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
