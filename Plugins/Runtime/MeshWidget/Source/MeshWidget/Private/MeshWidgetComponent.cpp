// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MeshWidgetPrivatePCH.h"

#include "MeshWidgetComponent.h"

#include "HittestGrid.h"
#if !UE_SERVER
	#include "ISlateRHIRendererModule.h"
	#include "ISlate3DRenderer.h"
#endif // !UE_SERVER
#include "DynamicMeshBuilder.h"
#include "Scalability.h"
#include "WidgetLayoutLibrary.h"
#include "PhysicsEngine/BodySetup.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SPopup.h"
#include "StaticMeshResources.h"
#include "Kismet/GameplayStatics.h"

DECLARE_CYCLE_STAT(TEXT("3DHitTesting"), STAT_Slate3DHitTesting, STATGROUP_Slate);

UMeshWidgetComponent::UMeshWidgetComponent( const FObjectInitializer& PCIP )
	: Super( PCIP )
	, DrawSize( FIntPoint( 500, 500 ) )
	, bManuallyRedraw(false)
	, bRedrawRequested(true)
	, RedrawTime(0)
	, LastWidgetRenderTime(0)
	, bWindowFocusable(true)
	, BackgroundColor( FLinearColor::Transparent )
	, TintColorAndOpacity( FLinearColor::White )
	, OpacityFromTexture( 1.0f )
	, BlendMode( EWidgetBlendMode::Masked )
	, bIsOpaque_DEPRECATED( false )
	, bIsTwoSided( false )
	, ParabolaDistortion( 0 )
	, TickWhenOffscreen( false )
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;

	RelativeRotation = FRotator::ZeroRotator;

	BodyInstance.SetCollisionProfileName(FName(TEXT("UI")));

	// Translucent material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentMaterial_Finder( TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent") );
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentMaterial_OneSided_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided"));
	TranslucentMaterial = TranslucentMaterial_Finder.Object;
	TranslucentMaterial_OneSided = TranslucentMaterial_OneSided_Finder.Object;

	// Opaque material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterial_Finder( TEXT( "/Engine/EngineMaterials/Widget3DPassThrough_Opaque" ) );
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterial_OneSided_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided"));
	OpaqueMaterial = OpaqueMaterial_Finder.Object;
	OpaqueMaterial_OneSided = OpaqueMaterial_OneSided_Finder.Object;

	// Masked material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaskedMaterial_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaskedMaterial_OneSided_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked_OneSided"));
	MaskedMaterial = MaskedMaterial_Finder.Object;
	MaskedMaterial_OneSided = MaskedMaterial_OneSided_Finder.Object;

	LastLocalHitLocation = FVector2D::ZeroVector;
	//bGenerateOverlapEvents = false;
	bUseEditorCompositing = false;

	bUseLegacyRotation = false;

	Pivot = FVector2D(0.5, 0.5);

	bAddedToScreen = false;

	// We want this because we want EndPlay to be called!
	bWantsBeginPlay = true;
}

void UMeshWidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseResources();
	Super::EndPlay(EndPlayReason);
}

void UMeshWidgetComponent::OnRegister()
{
	Super::OnRegister();

#if !UE_SERVER
	if ( !IsRunningDedicatedServer() )
	{
		if ( !WidgetRenderer.IsValid() && !GUsingNullRHI )
		{
			WidgetRenderer = MakeShareable(new FWidgetRenderer());
		}

		InitWidget();
	}
#endif // !UE_SERVER
}

void UMeshWidgetComponent::OnUnregister()
{
#if WITH_EDITOR
	if (!GetWorld()->IsGameWorld())
	{
		ReleaseResources();
	}
#endif

	Super::OnUnregister();
}

void UMeshWidgetComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	Super::DestroyComponent(bPromoteChildren);

	ReleaseResources();
}

FPrimitiveSceneProxy* UMeshWidgetComponent::CreateSceneProxy()
{
	if ( WidgetRenderer.IsValid() )
	{
		// Create a new MID for the current base material
		UMaterialInterface* BaseMaterial = GetBaseMaterial();
		MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		UpdateMaterialInstanceParameters();

		if(OverrideMaterials.Num() == 0)
			OverrideMaterials.AddZeroed(1);
		OverrideMaterials[0] = MaterialInstance;

		RequestRedraw();
		LastWidgetRenderTime = 0;

		return UStaticMeshComponent::CreateSceneProxy();
	}
	
	return nullptr;
}

void UMeshWidgetComponent::ReleaseResources()
{
	if ( Widget  )
	{
		Widget = nullptr;
	}

	WidgetRenderer.Reset();
	HitTestGrid.Reset();

	UnregisterWindow();
}

void UMeshWidgetComponent::RegisterWindow()
{
	if ( SlateWindow.IsValid() )
	{
		if ( FSlateApplication::IsInitialized() )
		{
			FSlateApplication::Get().RegisterVirtualWindow(SlateWindow.ToSharedRef());
		}
	}
}

void UMeshWidgetComponent::UnregisterWindow()
{
	if ( SlateWindow.IsValid() )
	{
		if ( FSlateApplication::IsInitialized() )
		{
			FSlateApplication::Get().UnregisterVirtualWindow(SlateWindow.ToSharedRef());
		}

		SlateWindow.Reset();
	}
}

void UMeshWidgetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		UpdateWidget();

		if ( Widget == nullptr && !SlateWidget.IsValid() )
		{
			return;
		}

		if ( ShouldDrawWidget() )
		{
			DrawWidgetToRenderTarget(DeltaTime);
		}
	}
#endif // !UE_SERVER
}

bool UMeshWidgetComponent::ShouldDrawWidget() const
{
	const float RenderTimeThreshold = .5f;
	if ( IsVisible() )
	{
		// If we don't tick when off-screen, don't bother ticking if it hasn't been rendered recently
		if ( TickWhenOffscreen || GetWorld()->TimeSince(LastRenderTime) <= RenderTimeThreshold )
		{
			if ( GetWorld()->TimeSince(LastWidgetRenderTime) >= RedrawTime )
			{
				return bManuallyRedraw ? bRedrawRequested : true;
			}
		}
	}

	return false;
}

void UMeshWidgetComponent::DrawWidgetToRenderTarget(float DeltaTime)
{
	if ( GUsingNullRHI )
	{
		return;
	}

	if ( !SlateWindow.IsValid() )
	{
		return;
	}

	if ( DrawSize.X == 0 || DrawSize.Y == 0 )
	{
		return;
	}

	CurrentDrawSize = DrawSize;

	const float DrawScale = 1.0f;

	if ( bDrawAtDesiredSize )
	{
		SlateWindow->SlatePrepass(DrawScale);

		FVector2D DesiredSize = SlateWindow->GetDesiredSize();
		DesiredSize.X = FMath::RoundToInt(DesiredSize.X);
		DesiredSize.Y = FMath::RoundToInt(DesiredSize.Y);
		CurrentDrawSize = DesiredSize.IntPoint();

		WidgetRenderer->SetIsPrepassNeeded(false);
	}
	else
	{
		WidgetRenderer->SetIsPrepassNeeded(true);
	}

	if ( CurrentDrawSize != DrawSize )
	{
		DrawSize = CurrentDrawSize;
		RecreatePhysicsState();
	}

	UpdateRenderTarget(CurrentDrawSize);

	bRedrawRequested = false;

	WidgetRenderer->DrawWindow(
		RenderTarget,
		HitTestGrid.ToSharedRef(),
		SlateWindow.ToSharedRef(),
		DrawScale,
		CurrentDrawSize,
		DeltaTime);

	LastWidgetRenderTime = GetWorld()->TimeSeconds;
}

class FMeshWidgetComponentInstanceData : public FSceneComponentInstanceData
{
public:
	FMeshWidgetComponentInstanceData( const UMeshWidgetComponent* SourceComponent )
		: FSceneComponentInstanceData(SourceComponent)
		, WidgetClass ( SourceComponent->GetWidgetClass() )
		, RenderTarget( SourceComponent->GetRenderTarget() )
	{}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<UMeshWidgetComponent>(Component)->ApplyComponentInstanceData(this);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		FSceneComponentInstanceData::AddReferencedObjects(Collector);

		UClass* WidgetUClass = *WidgetClass;
		Collector.AddReferencedObject(WidgetUClass);
		Collector.AddReferencedObject(RenderTarget);
	}

public:
	TSubclassOf<UUserWidget> WidgetClass;
	UTextureRenderTarget2D* RenderTarget;
};

FActorComponentInstanceData* UMeshWidgetComponent::GetComponentInstanceData() const
{
	return new FMeshWidgetComponentInstanceData( this );
}

void UMeshWidgetComponent::ApplyComponentInstanceData(FMeshWidgetComponentInstanceData* WidgetInstanceData)
{
	check(WidgetInstanceData);

	// Note: ApplyComponentInstanceData is called while the component is registered so the rendering thread is already using this component
	// That means all component state that is modified here must be mirrored on the scene proxy, which will be recreated to receive the changes later due to MarkRenderStateDirty.

	if (GetWidgetClass() != WidgetClass)
	{
		return;
	}

	RenderTarget = WidgetInstanceData->RenderTarget;
	if( MaterialInstance && RenderTarget )
	{
		MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
	}

	MarkRenderStateDirty();
}

#if WITH_EDITORONLY_DATA
void UMeshWidgetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* Property = PropertyChangedEvent.MemberProperty;

	if( Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
	{
		static FName DrawSizeName("DrawSize");
		static FName PivotName("Pivot");
		static FName WidgetClassName("WidgetClass");
		static FName IsOpaqueName("bIsOpaque");
		static FName IsTwoSidedName("bIsTwoSided");
		static FName BackgroundColorName("BackgroundColor");
		static FName TintColorAndOpacityName("TintColorAndOpacity");
		static FName OpacityFromTextureName("OpacityFromTexture");
		static FName ParabolaDistortionName(TEXT("ParabolaDistortion"));
		static FName BlendModeName( TEXT( "BlendMode" ) );

		auto PropertyName = Property->GetFName();

		if( PropertyName == WidgetClassName )
		{
			//in editor rendering is not working so this does nothing
			//Widget = nullptr;
			//RenderTarget->UpdateResourceImmediate(true); //clear the old widget render from texture, should probably set base material if set to none
			//UpdateWidget();
			MarkRenderStateDirty();
		}
		else if ( PropertyName == DrawSizeName || PropertyName == PivotName )
		{
			MarkRenderStateDirty();
			RecreatePhysicsState();
		}
		else if ( PropertyName == IsOpaqueName || PropertyName == IsTwoSidedName || PropertyName == BlendModeName )
		{
			MarkRenderStateDirty();
		}
		else if( PropertyName == BackgroundColorName || PropertyName == ParabolaDistortionName )
		{
			MarkRenderStateDirty();
		}
		else if( PropertyName == TintColorAndOpacityName || PropertyName == OpacityFromTextureName )
		{
			MarkRenderStateDirty();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UMeshWidgetComponent::InitWidget()
{
	// Don't do any work if Slate is not initialized
	if ( FSlateApplication::IsInitialized() )
	{
		if ( WidgetClass && Widget == nullptr && GetWorld() )
		{
			Widget = CreateWidget<UUserWidget>(GetWorld(), WidgetClass);
		}
		
#if WITH_EDITOR
		if ( Widget && !GetWorld()->IsGameWorld() && !bEditTimeUsable )
		{
			if( !GEnableVREditorHacks )
			{
				// Prevent native ticking of editor component previews
				Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
			}
		}
#endif
	}
}

void UMeshWidgetComponent::SetOwnerPlayer(ULocalPlayer* LocalPlayer)
{
	if ( OwnerPlayer != LocalPlayer )
	{
		OwnerPlayer = LocalPlayer;
	}
}

ULocalPlayer* UMeshWidgetComponent::GetOwnerPlayer() const
{
	return OwnerPlayer ? OwnerPlayer : GEngine->GetLocalPlayerFromControllerId(GetWorld(), 0);
}

void UMeshWidgetComponent::SetWidget(UUserWidget* InWidget)
{
	if( InWidget != nullptr )
	{
		SetSlateWidget( nullptr );
	}

	Widget = InWidget;

	UpdateWidget();
}

void UMeshWidgetComponent::SetSlateWidget( const TSharedPtr<SWidget>& InSlateWidget )
{
	if( Widget != nullptr )
	{
		SetWidget( nullptr );
	}

	if( SlateWidget.IsValid() )
	{
		SlateWidget.Reset();
	}

	SlateWidget = InSlateWidget;

	UpdateWidget();
}

void UMeshWidgetComponent::UpdateWidget()
{
	// Don't do any work if Slate is not initialized
	if ( FSlateApplication::IsInitialized() )
	{
		TSharedPtr<SWidget> NewSlateWidget;
		if (Widget)
		{
			NewSlateWidget = Widget->TakeWidget();
		}

		if ( !SlateWindow.IsValid() )
		{
			SlateWindow = SNew(SVirtualWindow).Size(DrawSize);
			SlateWindow->SetIsFocusable(bWindowFocusable);
			RegisterWindow();
		}

		if ( !HitTestGrid.IsValid() )
		{
			HitTestGrid = MakeShareable(new FHittestGrid);
		}

		SlateWindow->Resize(DrawSize);

		if ( NewSlateWidget.IsValid() )
		{
			if ( NewSlateWidget != CurrentSlateWidget )
			{
				CurrentSlateWidget = NewSlateWidget;
				SlateWindow->SetContent(NewSlateWidget.ToSharedRef());
			}
		}
		else if( SlateWidget.IsValid() )
		{
			if ( SlateWidget != CurrentSlateWidget )
			{
				CurrentSlateWidget = SlateWidget;
				SlateWindow->SetContent(SlateWidget.ToSharedRef());
			}
		}
		else
		{
			CurrentSlateWidget = SNullWidget::NullWidget;
			SlateWindow->SetContent( SNullWidget::NullWidget );
		}
	}
}

void UMeshWidgetComponent::UpdateRenderTarget(FIntPoint DesiredRenderTargetSize)
{
	bool bWidgetRenderStateDirty = false;
	bool bClearColorChanged = false;

	FLinearColor ActualBackgroundColor = BackgroundColor;
	switch ( BlendMode )
	{
	case EWidgetBlendMode::Opaque:
		ActualBackgroundColor.A = 1.0f;
	case EWidgetBlendMode::Masked:
		ActualBackgroundColor.A = 0.0f;
	}

	if ( DesiredRenderTargetSize.X != 0 && DesiredRenderTargetSize.Y != 0 )
	{
		if ( RenderTarget == nullptr )
		{
			RenderTarget = NewObject<UTextureRenderTarget2D>(this);
			RenderTarget->ClearColor = ActualBackgroundColor;

			bClearColorChanged = bWidgetRenderStateDirty = true;

			RenderTarget->InitCustomFormat(DesiredRenderTargetSize.X, DesiredRenderTargetSize.Y, PF_B8G8R8A8, false);

			MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
		}
		else
		{
			// Update the format
			if ( RenderTarget->SizeX != DesiredRenderTargetSize.X || RenderTarget->SizeY != DesiredRenderTargetSize.Y )
			{
				RenderTarget->InitCustomFormat(DesiredRenderTargetSize.X, DesiredRenderTargetSize.Y, PF_B8G8R8A8, false);
				RenderTarget->UpdateResourceImmediate(false);
				bWidgetRenderStateDirty = true;
			}

			// Update the clear color
			if ( RenderTarget->ClearColor != ActualBackgroundColor )
			{
				RenderTarget->ClearColor = ActualBackgroundColor;
				bClearColorChanged = bWidgetRenderStateDirty = true;
			}

			if ( bWidgetRenderStateDirty )
			{
				RenderTarget->UpdateResource();
			}
		}
	}

	if ( RenderTarget )
	{
		// If the clear color of the render target changed, update the BackColor of the material to match
		if ( bClearColorChanged )
		{
			MaterialInstance->SetVectorParameterValue("BackColor", RenderTarget->ClearColor);
		}

		static FName ParabolaDistortionName(TEXT("ParabolaDistortion"));

		float CurrentParabolaValue;
		if ( MaterialInstance->GetScalarParameterValue(ParabolaDistortionName, CurrentParabolaValue) && CurrentParabolaValue != ParabolaDistortion )
		{
			MaterialInstance->SetScalarParameterValue(ParabolaDistortionName, ParabolaDistortion);
		}

		if ( bWidgetRenderStateDirty )
		{
			MarkRenderStateDirty();
		}
	}
}

FVector2D UMeshWidgetComponent::GetLocalHitLocation(const FHitResult& Hit) const
{
	FVector2D UV;
	UGameplayStatics::FindCollisionUV(Hit, 0, UV);
	UE_LOG(LogTemp, Warning, TEXT("%f %f"), UV.X, UV.Y);

	return FVector2D(DrawSize.X * UV.X, DrawSize.Y * UV.Y);
}

UUserWidget* UMeshWidgetComponent::GetUserWidgetObject() const
{
	return Widget;
}

UTextureRenderTarget2D* UMeshWidgetComponent::GetRenderTarget() const
{
	return RenderTarget;
}

UMaterialInstanceDynamic* UMeshWidgetComponent::GetMaterialInstance() const
{
	return MaterialInstance;
}

const TSharedPtr<SWidget>& UMeshWidgetComponent::GetSlateWidget() const
{
	return SlateWidget;
}

TArray<FWidgetAndPointer> UMeshWidgetComponent::GetHitWidgetPath(const FHitResult& Hit, bool bIgnoreEnabledStatus, float CursorRadius)
{
	FVector2D LocalHitLocation = GetLocalHitLocation(Hit);

	TSharedRef<FVirtualPointerPosition> VirtualMouseCoordinate = MakeShareable( new FVirtualPointerPosition );

	VirtualMouseCoordinate->CurrentCursorPosition = LocalHitLocation;
	VirtualMouseCoordinate->LastCursorPosition = LastLocalHitLocation;

	// Cache the location of the hit
	LastLocalHitLocation = LocalHitLocation;

	TArray<FWidgetAndPointer> ArrangedWidgets;
	if ( HitTestGrid.IsValid() )
	{
		ArrangedWidgets = HitTestGrid->GetBubblePath( LocalHitLocation, CursorRadius, bIgnoreEnabledStatus );

		for( FWidgetAndPointer& ArrangedWidget : ArrangedWidgets )
		{
			ArrangedWidget.PointerPosition = VirtualMouseCoordinate;
		}
	}

	return ArrangedWidgets;
}

TSharedPtr<SWindow> UMeshWidgetComponent::GetSlateWindow() const
{
	return SlateWindow;
}

FVector2D UMeshWidgetComponent::GetDrawSize() const
{
	return DrawSize;
}

void UMeshWidgetComponent::SetDrawSize(FVector2D Size)
{
	FIntPoint NewDrawSize((int32)Size.X, (int32)Size.Y);

	if ( NewDrawSize != DrawSize )
	{
		DrawSize = NewDrawSize;
		MarkRenderStateDirty();
		RecreatePhysicsState();
	}
}

void UMeshWidgetComponent::RequestRedraw()
{
	bRedrawRequested = true;
}

void UMeshWidgetComponent::SetBlendMode( const EWidgetBlendMode NewBlendMode )
{
	if( NewBlendMode != this->BlendMode )
	{
		this->BlendMode = NewBlendMode;
		if( IsRegistered() )
		{
			MarkRenderStateDirty();
		}
	}
}

void UMeshWidgetComponent::SetTwoSided( const bool bWantTwoSided )
{
	if( bWantTwoSided != this->bIsTwoSided )
	{
		this->bIsTwoSided = bWantTwoSided;
		if( IsRegistered() )
		{
			MarkRenderStateDirty();
		}
	}
}

void UMeshWidgetComponent::SetBackgroundColor( const FLinearColor NewBackgroundColor )
{
	if( NewBackgroundColor != this->BackgroundColor)
	{
		this->BackgroundColor = NewBackgroundColor;
		MarkRenderStateDirty();
	}
}

void UMeshWidgetComponent::SetTintColorAndOpacity( const FLinearColor NewTintColorAndOpacity )
{
	if( NewTintColorAndOpacity != this->TintColorAndOpacity )
	{
		this->TintColorAndOpacity = NewTintColorAndOpacity;
		UpdateMaterialInstanceParameters();
	}
}

void UMeshWidgetComponent::SetOpacityFromTexture( const float NewOpacityFromTexture )
{
	if( NewOpacityFromTexture != this->OpacityFromTexture )
	{
		this->OpacityFromTexture = NewOpacityFromTexture;
		UpdateMaterialInstanceParameters();
	}
}

TSharedPtr< SWindow > UMeshWidgetComponent::GetVirtualWindow() const
{
	return StaticCastSharedPtr<SWindow>(SlateWindow);
}

void UMeshWidgetComponent::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_ADD_PIVOT_TO_WIDGET_COMPONENT )
	{
		Pivot = FVector2D(0, 0);
	}

	if ( GetLinkerUE4Version() < VER_UE4_ADD_BLEND_MODE_TO_WIDGET_COMPONENT )
	{
		BlendMode = bIsOpaque_DEPRECATED ? EWidgetBlendMode::Opaque : EWidgetBlendMode::Transparent;
	}

	if( GetLinkerUE4Version() < VER_UE4_FIXED_DEFAULT_ORIENTATION_OF_WIDGET_COMPONENT )
	{	
		// This indicates the value does not differ from the default.  In some rare cases this could cause incorrect rotation for anyone who directly set a value of 0,0,0 for rotation
		// However due to delta serialization we have no way to know if this value is actually different from the default so assume it is not.
		if( RelativeRotation == FRotator::ZeroRotator )
		{
			RelativeRotation = FRotator(0.f, 0.f, 90.f);
		}
		bUseLegacyRotation = true;
	}
}

UMaterialInterface* UMeshWidgetComponent::GetMaterial(int32 MaterialIndex) const
{
	if ( OverrideMaterials.IsValidIndex(MaterialIndex) && ( OverrideMaterials[MaterialIndex] != nullptr ) )
	{
		return OverrideMaterials[MaterialIndex];
	}
	else
	{
		return GetBaseMaterial();
	}

	return nullptr;
}

UMaterialInterface* UMeshWidgetComponent::GetBaseMaterial() const
{
	switch ( BlendMode )
	{
	case EWidgetBlendMode::Opaque:
		return bIsTwoSided ? OpaqueMaterial : OpaqueMaterial_OneSided;
		break;
	case EWidgetBlendMode::Masked:
		return bIsTwoSided ? MaskedMaterial : MaskedMaterial_OneSided;
		break;
	case EWidgetBlendMode::Transparent:
		return bIsTwoSided ? TranslucentMaterial : TranslucentMaterial_OneSided;
		break;
	}
	return nullptr;
}

int32 UMeshWidgetComponent::GetNumMaterials() const
{
	return FMath::Max<int32>(OverrideMaterials.Num(), 1);
}

void UMeshWidgetComponent::UpdateMaterialInstanceParameters()
{
	if ( MaterialInstance )
	{
		MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
		MaterialInstance->SetVectorParameterValue("TintColorAndOpacity", TintColorAndOpacity);
		MaterialInstance->SetScalarParameterValue("OpacityFromTexture", OpacityFromTexture);
	}
}

void UMeshWidgetComponent::SetWidgetClass(TSubclassOf<UUserWidget> InWidgetClass)
{
	WidgetClass = InWidgetClass;
}
