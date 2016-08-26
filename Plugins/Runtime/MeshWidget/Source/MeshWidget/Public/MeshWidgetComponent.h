// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetComponent.h"

#include "MeshWidgetComponent.generated.h"

struct FVirtualPointerPosition;

//UENUM(BlueprintType)
//enum class EWidgetBlendMode : uint8
//{
//	Opaque,
//	Masked,
//	Transparent
//};


/**
 * Beware! This feature is experimental and may be substantially changed or removed in future releases.
 * A 3D instance of a Widget Blueprint that can be interacted with in the world.
 *
 * SlateUI [Texture]
 * BackColor [Vector]
 * TintColorAndOpacity [Vector]
 * OpacityFromTexture [Scalar]
 * ParabolaDistortion [Scalar]
 */
UCLASS(Blueprintable, ClassGroup=Experimental, hidecategories=(Object,Activation,"Components|Activation",Sockets,Base,Lighting,LOD,Mesh), editinlinenew, meta=(BlueprintSpawnableComponent, DevelopmentStatus=Experimental) )
class MESHWIDGET_API UMeshWidgetComponent : public UStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** UActorComponent Interface */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	/* UPrimitiveComponent Interface */
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	int32 GetNumMaterials() const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	// Begin UObject
	virtual void PostLoad() override;
	// End UObject

	// Begin MeshComponent
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	// End MeshComponent
	virtual UMaterialInterface* GetBaseMaterial() const;

	FActorComponentInstanceData* GetComponentInstanceData() const override;
	
	void ApplyComponentInstanceData(class FMeshWidgetComponentInstanceData* WidgetInstanceData);

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Ensures the user widget is initialized */
	virtual void InitWidget();

	/** Release resources associated with the widget. */
	virtual void ReleaseResources();

	/** Ensures the 3d window is created its size and content. */
	virtual void UpdateWidget();

	/** Ensure the render target is initialized and updates it if needed. */
	virtual void UpdateRenderTarget(FIntPoint DesiredRenderTargetSize);

	/**
	 * Converts a world-space hit result to a hit location on the widget
	 * @param HitResult The hit on this widget in the world
	 * @param (Out) The transformed 2D hit location on the widget
	 */
	FVector2D GetLocalHitLocation(const FHitResult& Hit) const;

	/** @return Gets the last local location that was hit */
	FVector2D GetLastLocalHitLocation() const
	{
		return LastLocalHitLocation;
	}
	
	/** @return The class of the user widget displayed by this component */
	TSubclassOf<UUserWidget> GetWidgetClass() const { return WidgetClass; }

	/** @return The user widget object displayed by this component */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	UUserWidget* GetUserWidgetObject() const;

	/** @return Returns the Slate widget that was assigned to this component, if any */
	const TSharedPtr<SWidget>& GetSlateWidget() const;

	/** @return List of widgets with their geometry and the cursor position transformed into this Widget component's space. */
	TArray<FWidgetAndPointer> GetHitWidgetPath(const FHitResult& Hit, bool bIgnoreEnabledStatus, float CursorRadius = 0.0f);

	/** @return The render target to which the user widget is rendered */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	UTextureRenderTarget2D* GetRenderTarget() const;

	/** @return The dynamic material instance used to render the user widget */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	UMaterialInstanceDynamic* GetMaterialInstance() const;

	/** @return The window containing the user widget content */
	TSharedPtr<SWindow> GetSlateWindow() const;

	/**  
	 *  Sets the widget to use directly. This function will keep track of the widget till the next time it's called
	 *	with either a newer widget or a nullptr
	 */ 
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	virtual void SetWidget(UUserWidget* Widget);

	/**  
	 *  Sets a Slate widget to be rendered.  You can use this to draw native Slate widgets using a WidgetComponent, instead
	 *  of drawing user widgets.
	 */ 
	virtual void SetSlateWidget( const TSharedPtr<SWidget>& InSlateWidget);

	/**
	 * Sets the local player that owns this widget component.  Setting the owning player controls
	 * which player's viewport the widget appears on in a split screen scenario.  Additionally it
	 * forwards the owning player to the actual UserWidget that is spawned.
	 */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetOwnerPlayer(ULocalPlayer* LocalPlayer);

	/** Gets the local player that owns this widget component. */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	ULocalPlayer* GetOwnerPlayer() const;

	/** @return The draw size of the quad in the world */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	FVector2D GetDrawSize() const;

	/** Sets the draw size of the quad in the world */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetDrawSize(FVector2D Size);

	/** Requests that the widget be redrawn.  */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	virtual void RequestRedraw();

	/** Gets the blend mode for the widget. */
	EWidgetBlendMode GetBlendMode() const { return BlendMode; }

	/** Sets the blend mode to use for this widget */
	void SetBlendMode( const EWidgetBlendMode NewBlendMode );

	/** Sets whether the widget is two-sided or not */
	void SetTwoSided( const bool bWantTwoSided );

	/** Sets the background color and opacityscale for this widget */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetBackgroundColor( const FLinearColor NewBackgroundColor );

	/** Sets the tint color and opacity scale for this widget */
	void SetTintColorAndOpacity( const FLinearColor NewTintColorAndOpacity );

	/** Sets how much opacity from the UI widget's texture alpha is used when rendering to the viewport (0.0-1.0) */
	void SetOpacityFromTexture( const float NewOpacityFromTexture );

	/** @return The pivot point where the UI is rendered about the origin. */
	FVector2D GetPivot() const { return Pivot; }

	void SetPivot( const FVector2D& InPivot ) { Pivot = InPivot; }

	/** Get the fake window we create for widgets displayed in the world. */
	TSharedPtr< SWindow > GetVirtualWindow() const;
	
	/** Whether or not this component uses legacy default rotation */
	bool IsUsingLegacyRotation() const { return bUseLegacyRotation; }
	
	/** Updates the dynamic parameters on the material instance, without re-creating it */
	void UpdateMaterialInstanceParameters();

	/** Sets the widget class used to generate the widget for this component */
	void SetWidgetClass(TSubclassOf<UUserWidget> InWidgetClass);

	bool GetEditTimeUsable() const { return bEditTimeUsable; }

	void SetEditTimeUsable(bool Value) { bEditTimeUsable = Value; }

protected:
	void RegisterWindow();
	void UnregisterWindow();

	/** Allows subclasses to control if the widget should be drawn.  Called right before we draw the widget. */
	virtual bool ShouldDrawWidget() const;

	/** Draws the current widget to the render target if possible. */
	virtual void DrawWidgetToRenderTarget(float DeltaTime);
protected:

	/** The class of User Widget to create and display an instance of */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	TSubclassOf<UUserWidget> WidgetClass;
	
	/** The size of the displayed quad. */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	FIntPoint DrawSize;

	/** Should we wait to be told to redraw to actually draw? */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	bool bManuallyRedraw;

	/** Has anyone requested we redraw? */
	UPROPERTY()
	bool bRedrawRequested;

	/**
	 * The time in between draws, if 0 - we would redraw every frame.  If 1, we would redraw every second.
	 * This will work with bManuallyRedraw as well.  So you can say, manually redraw, but only redraw at this
	 * maximum rate.
	 */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	float RedrawTime;

	/** What was the last time we rendered the widget? */
	UPROPERTY()
	float LastWidgetRenderTime;

	/** Is the virtual window created to host the widget focusable? */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	bool bWindowFocusable;

	/**
	 * The actual draw size, this changes based on DrawSize - or the desired size of the widget if
	 * bDrawAtDesiredSize is true.
	 */
	UPROPERTY()
	FIntPoint CurrentDrawSize;

	/**
	 * Causes the render target to automatically match the desired size.
	 * 
	 * WARNING: If you change this every frame, it will be very expensive.  If you need 
	 *    that effect, you should keep the outer widget's sized locked and dynamically
	 *    scale or resize some inner widget.
	 */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	bool bDrawAtDesiredSize;

	/** The Alignment/Pivot point that the widget is placed at relative to the position. */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	FVector2D Pivot;

	/**
	 * The owner player for a widget component, if this widget is drawn on the screen, this controls
	 * what player's screen it appears on for split screen, if not set, users player 0.
	 */
	UPROPERTY()
	ULocalPlayer* OwnerPlayer;

	/** The background color of the component */
	UPROPERTY(EditAnywhere, Category=Rendering)
	FLinearColor BackgroundColor;

	/** Tint color and opacity for this component */
	UPROPERTY(EditAnywhere, Category=Rendering)
	FLinearColor TintColorAndOpacity;

	/** Sets the amount of opacity from the widget's UI texture to use when rendering the translucent or masked UI to the viewport (0.0-1.0) */
	UPROPERTY(EditAnywhere, Category=Rendering, meta=(ClampMin=0.0f, ClampMax=1.0f))
	float OpacityFromTexture;

	/** The blend mode for the widget. */
	UPROPERTY(EditAnywhere, Category=Rendering)
	EWidgetBlendMode BlendMode;

	UPROPERTY()
	bool bIsOpaque_DEPRECATED;

	/** Is the component visible from behind? */
	UPROPERTY(EditAnywhere, Category=Rendering)
	bool bIsTwoSided;
	
	/**
	 * When enabled, distorts the UI along a parabola shape giving the UI the appearance 
	 * that it's on a curved surface in front of the users face.  This only works for UI 
	 * rendered to a render target.
	 */
	UPROPERTY(EditAnywhere, Category=Rendering)
	float ParabolaDistortion;

	/** Should the component tick the widget when it's off screen? */
	UPROPERTY(EditAnywhere, Category=Animation)
	bool TickWhenOffscreen;

	/** The User Widget object displayed and managed by this component */
	UPROPERTY(Transient, DuplicateTransient)
	UUserWidget* Widget;
	
	/** The Slate widget to be displayed by this component.  Only one of either Widget or SlateWidget can be used */
	TSharedPtr<SWidget> SlateWidget;

	/** The slate widget currently being drawn. */
	TWeakPtr<SWidget> CurrentSlateWidget;

	/** The material instance for translucent widget components */
	UPROPERTY()
	UMaterialInterface* TranslucentMaterial;

	/** The material instance for translucent, one-sided widget components */
	UPROPERTY()
	UMaterialInterface* TranslucentMaterial_OneSided;

	/** The material instance for opaque widget components */
	UPROPERTY()
	UMaterialInterface* OpaqueMaterial;

	/** The material instance for opaque, one-sided widget components */
	UPROPERTY()
	UMaterialInterface* OpaqueMaterial_OneSided;

	/** The material instance for masked widget components. */
	UPROPERTY()
	UMaterialInterface* MaskedMaterial;

	/** The material instance for masked, one-sided widget components. */
	UPROPERTY()
	UMaterialInterface* MaskedMaterial_OneSided;

	/** The target to which the user widget is rendered */
	UPROPERTY(Transient, DuplicateTransient)
	UTextureRenderTarget2D* RenderTarget;

	/** The dynamic instance of the material that the render target is attached to */
	UPROPERTY(Transient, DuplicateTransient)
	UMaterialInstanceDynamic* MaterialInstance;

	UPROPERTY()
	bool bUseLegacyRotation;

	UPROPERTY(Transient, DuplicateTransient)
	bool bAddedToScreen;

	/**
	 * Allows the widget component to be used at editor time.  For use in the VR-Editor.
	 */
	UPROPERTY()
	bool bEditTimeUsable;

protected:

	/** The grid used to find actual hit actual widgets once input has been translated to the components local space */
	TSharedPtr<class FHittestGrid> HitTestGrid;
	
	/** The slate window that contains the user widget content */
	TSharedPtr<class SVirtualWindow> SlateWindow;

	/** The relative location of the last hit on this component */
	FVector2D LastLocalHitLocation;

	/** The hit tester to use for this component */
	static TSharedPtr<class FWidget3DHitTester> WidgetHitTester;

	/** Helper class for drawing widgets to a render target. */
	TSharedPtr<class FWidgetRenderer> WidgetRenderer;
};
