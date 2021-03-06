/*
* Copyright (c) <2017> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniApi.h"
#include "HoudiniAssetParameterFloat.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineString.h"

#if WITH_EDITOR
#include "UnitConversion.h"
#include "NumericUnitTypeInterface.inl"
#endif

#include "Internationalization.h"
#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

UHoudiniAssetParameterFloat::UHoudiniAssetParameterFloat( const FObjectInitializer & ObjectInitializer )
    : Super( ObjectInitializer )
    , ValueMin( TNumericLimits< float >::Lowest() )
    , ValueMax( TNumericLimits< float >::Max() )
    , ValueUIMin( TNumericLimits< float >::Lowest() )
    , ValueUIMax( TNumericLimits< float >::Max() )
    , ValueUnit ( TEXT("") )
{
    // Parameter will have at least one value.
    Values.AddZeroed( 1 );
}

UHoudiniAssetParameterFloat::~UHoudiniAssetParameterFloat()
{}

void
UHoudiniAssetParameterFloat::Serialize( FArchive & Ar )
{
    // Call base implementation.
    Super::Serialize( Ar );

    Ar.UsingCustomVersion( FHoudiniCustomSerializationVersion::GUID );

    Ar << Values;

    Ar << ValueMin;
    Ar << ValueMax;

    Ar << ValueUIMin;
    Ar << ValueUIMax;

    if ( HoudiniAssetParameterVersion >= VER_HOUDINI_PLUGIN_SERIALIZATION_VERSION_PARAMETERS_UNIT )
        Ar << ValueUnit;
}

UHoudiniAssetParameterFloat *
UHoudiniAssetParameterFloat::Create(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo )
{
    UObject * Outer = InPrimaryObject;
    if ( !Outer )
    {
        Outer = InParentParameter;
        if ( !Outer )
        {
            // Must have either component or parent not null.
            check( false );
        }
    }

    UHoudiniAssetParameterFloat * HoudiniAssetParameterFloat = NewObject< UHoudiniAssetParameterFloat >(
        Outer, UHoudiniAssetParameterFloat::StaticClass(), NAME_None, RF_Public | RF_Transactional );

    HoudiniAssetParameterFloat->CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo );
    return HoudiniAssetParameterFloat;
}

bool
UHoudiniAssetParameterFloat::CreateParameter(
    UObject * InPrimaryObject,
    UHoudiniAssetParameter * InParentParameter,
    HAPI_NodeId InNodeId, const HAPI_ParmInfo & ParmInfo )
{
    if ( !Super::CreateParameter( InPrimaryObject, InParentParameter, InNodeId, ParmInfo ) )
        return false;

    // We can only handle float type.
    if ( ParmInfo.type != HAPI_PARMTYPE_FLOAT )
    {
        return false;
    }

    // Assign internal Hapi values index.
    SetValuesIndex( ParmInfo.floatValuesIndex );

    if ( TupleSize <= 0 )
        return false;
    Values.SetNumZeroed(TupleSize);

    // Get the actual value for this property.
    if ( FHoudiniApi::GetParmFloatValues(
        FHoudiniEngine::Get().GetSession(), InNodeId, &Values[ 0 ],
        ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    // Set min and max for this property.
    if ( ParmInfo.hasMin )
        ValueMin = ParmInfo.min;

    if ( ParmInfo.hasMax )
        ValueMax = ParmInfo.max;

    bool bUsesDefaultMinMax = false;

    // Set min and max for UI for this property.
    if ( ParmInfo.hasUIMin )
    {
        ValueUIMin = ParmInfo.UIMin;
    }
    else
    {
        // If it is not set, use supplied min.
        if ( ParmInfo.hasMin )
        {
            ValueUIMin = ValueMin;
        }
        else
        {
            // Min value Houdini uses by default.
            ValueUIMin = HAPI_UNREAL_PARAM_FLOAT_UI_MIN;
            bUsesDefaultMinMax = true;
        }
    }

    if ( ParmInfo.hasUIMax )
    {
        ValueUIMax = ParmInfo.UIMax;
    }
    else
    {
        // If it is not set, use supplied max.
        if ( ParmInfo.hasMax )
        {
            ValueUIMax = ValueMax;
        }
        else
        {
            // Max value Houdini uses by default.
            ValueUIMax = HAPI_UNREAL_PARAM_FLOAT_UI_MAX;
            bUsesDefaultMinMax = true;
        }
    }

    if ( bUsesDefaultMinMax )
    {
        // If we are using defaults, we can detect some most common parameter names and alter defaults.

        FString LocalParameterName = TEXT( "" );
        FHoudiniEngineString HoudiniEngineString( ParmInfo.nameSH );
        HoudiniEngineString.ToFString( LocalParameterName );

        static const FString ParameterNameTranslate( TEXT( HAPI_UNREAL_PARAM_TRANSLATE ) );
        static const FString ParameterNameRotate( TEXT( HAPI_UNREAL_PARAM_ROTATE ) );
        static const FString ParameterNameScale( TEXT( HAPI_UNREAL_PARAM_SCALE ) );
        static const FString ParameterNamePivot( TEXT( HAPI_UNREAL_PARAM_PIVOT ) );

        if ( !LocalParameterName.IsEmpty() )
        {
            if ( LocalParameterName.Equals( ParameterNameTranslate )
                || LocalParameterName.Equals( ParameterNameScale )
                || LocalParameterName.Equals( ParameterNamePivot ) )
            {
                ValueUIMin = -1.0f;
                ValueUIMax = 1.0f;
            }
            else if ( LocalParameterName.Equals( ParameterNameRotate ) )
            {
                ValueUIMin = 0.0f;
                ValueUIMax = 360.0f;
            }
        }
    }

    // Get this parameter's unit if it has one
    FHoudiniEngineUtils::HapiGetParameterUnit( InNodeId, ParmId, ValueUnit );

    return true;
}

#if WITH_EDITOR

void
UHoudiniAssetParameterFloat::CreateWidget( IDetailCategoryBuilder & LocalDetailCategoryBuilder )
{
    Super::CreateWidget( LocalDetailCategoryBuilder );

    /** Should we swap Y and Z fields (only relevant for Vector3) */
    bool bSwappedAxis3Vector = false;
    if ( auto Settings = GetDefault< UHoudiniRuntimeSettings >() )
    {
        bSwappedAxis3Vector = TupleSize == 3 && Settings->ImportAxis == HRSAI_Unreal;
    }

    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow( FText::GetEmpty() );

    // Create the standard parameter name widget.
    CreateNameWidget( Row, true );

    // Helper function to find a unit from a string (name or abbreviation) 
    TOptional<EUnit> ParmUnit = FUnitConversion::UnitFromString( *ValueUnit );

    TSharedPtr<INumericTypeInterface<float>> TypeInterface;
    if ( FUnitConversion::Settings().ShouldDisplayUnits() && ParmUnit.IsSet() )
    {
        TypeInterface = MakeShareable(new TNumericUnitTypeInterface<float>( ParmUnit.GetValue() ) );
    }

    if ( TupleSize == 3 )
    {
        Row.ValueWidget.Widget = SNew( SVectorInputBox )
        .bColorAxisLabels( true )
        .X( TAttribute< TOptional< float > >::Create( TAttribute< TOptional< float > >::FGetter::CreateUObject( this, &UHoudiniAssetParameterFloat::GetValue, 0 ) ) )
        .Y( TAttribute< TOptional< float > >::Create( TAttribute< TOptional< float > >::FGetter::CreateUObject( this, &UHoudiniAssetParameterFloat::GetValue, bSwappedAxis3Vector ? 2 : 1 ) ) )
        .Z( TAttribute< TOptional< float > >::Create( TAttribute< TOptional< float > >::FGetter::CreateUObject( this, &UHoudiniAssetParameterFloat::GetValue, bSwappedAxis3Vector ? 1 : 2 ) ) )
        .OnXCommitted( FOnFloatValueCommitted::CreateLambda(
            [=]( float Val, ETextCommit::Type TextCommitType ) {
                SetValue( Val, 0, true, true );
        }))
        .OnYCommitted( FOnFloatValueCommitted::CreateLambda(
            [=]( float Val, ETextCommit::Type TextCommitType ) {
                SetValue( Val, bSwappedAxis3Vector ? 2 : 1, true, true );
        } ) )
        .OnZCommitted( FOnFloatValueCommitted::CreateLambda(
            [=]( float Val, ETextCommit::Type TextCommitType ) {
                SetValue( Val, bSwappedAxis3Vector ? 1 : 2, true, true );
        } ) )
        .TypeInterface( TypeInterface );
    }
    else
    {
        TSharedRef< SVerticalBox > VerticalBox = SNew( SVerticalBox );

        for ( int32 Idx = 0; Idx < TupleSize; ++Idx )
        {
            TSharedPtr< SNumericEntryBox< float > > NumericEntryBox;

            VerticalBox->AddSlot().Padding( 2, 2, 5, 2 )
            [
                SAssignNew( NumericEntryBox, SNumericEntryBox< float > )
                .AllowSpin( true )

                .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )

                .MinValue( ValueMin )
                .MaxValue( ValueMax )

                .MinSliderValue( ValueUIMin )
                .MaxSliderValue( ValueUIMax )

                .Value( TAttribute< TOptional< float > >::Create( TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    this, &UHoudiniAssetParameterFloat::GetValue, Idx ) ) )
                .OnValueChanged(SNumericEntryBox< float >::FOnValueChanged::CreateLambda(
                    [=]( float Val ) {
                    SetValue( Val, Idx, false, false );
                    } ) )
                .OnValueCommitted( SNumericEntryBox< float >::FOnValueCommitted::CreateLambda(
                    [=]( float Val, ETextCommit::Type TextCommitType ) {
                        SetValue( Val, Idx, true, true );
                    } ) )
                .OnBeginSliderMovement( FSimpleDelegate::CreateUObject(
                    this, &UHoudiniAssetParameterFloat::OnSliderMovingBegin, Idx ) )
                .OnEndSliderMovement( SNumericEntryBox< float >::FOnValueChanged::CreateUObject(
                    this, &UHoudiniAssetParameterFloat::OnSliderMovingFinish, Idx ) )

                .SliderExponent( 1.0f )
                .TypeInterface( TypeInterface )
            ];

            if ( NumericEntryBox.IsValid() )
                NumericEntryBox->SetEnabled( !bIsDisabled );
        }

        Row.ValueWidget.Widget = VerticalBox;
    }
    Row.ValueWidget.MinDesiredWidth( HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH );
}

#endif // WITH_EDITOR

bool
UHoudiniAssetParameterFloat::UploadParameterValue()
{
    if ( FHoudiniApi::SetParmFloatValues( FHoudiniEngine::Get().GetSession(), NodeId, &Values[ 0 ],
        ValuesIndex, TupleSize ) != HAPI_RESULT_SUCCESS )
    {
        return false;
    }

    return Super::UploadParameterValue();
}

bool
UHoudiniAssetParameterFloat::SetParameterVariantValue( const FVariant & Variant, int32 Idx, bool bTriggerModify, bool bRecordUndo )
{
    int32 VariantType = Variant.GetType();
    float VariantValue = 0.0f;

    if ( Idx >= 0 && Idx < Values.Num() )
        return false;

    if ( VariantType == EVariantTypes::Float )
        VariantValue = Variant.GetValue<float>();
    else if ( VariantType == EVariantTypes::Double )
        VariantValue = (float) Variant.GetValue< double >();
    else
        return false;

#if WITH_EDITOR

    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniAssetParameterFloatChange", "Houdini Parameter Float: Changing a value"),
        PrimaryObject );

    Modify();

    if ( !bRecordUndo )
        Transaction.Cancel();

#endif // WITH_EDITOR

    MarkPreChanged( bTriggerModify );
    Values[ Idx ] = VariantValue;
    MarkChanged( bTriggerModify );

    return true;
}

TOptional< float >
UHoudiniAssetParameterFloat::GetValue( int32 Idx ) const
{
    return TOptional< float >( Values[ Idx ] );
}

void
UHoudiniAssetParameterFloat::SetValue( float InValue, int32 Idx, bool bTriggerModify, bool bRecordUndo )
{
    if ( InValue != Values[ Idx ] )
    {

#if WITH_EDITOR

        // If this is not a slider change (user typed in values manually), record undo information.
        FScopedTransaction Transaction(
            TEXT( HOUDINI_MODULE_RUNTIME ),
            LOCTEXT( "HoudiniAssetParameterFloatChange", "Houdini Parameter Float: Changing a value" ),
            PrimaryObject );
        Modify();

        if ( !bRecordUndo )
            Transaction.Cancel();

#endif // WITH_EDITOR

        MarkPreChanged( bTriggerModify );

        Values[ Idx ] = FMath::Clamp< float >( InValue, ValueMin, ValueMax );

        // Mark this parameter as changed.
        MarkChanged( bTriggerModify );
    }
}

float
UHoudiniAssetParameterFloat::GetParameterValue( int32 Idx, float DefaultValue ) const
{
    if ( Values.Num() <= Idx )
        return DefaultValue;

    return Values[ Idx ];
}

#if WITH_EDITOR

void
UHoudiniAssetParameterFloat::OnSliderMovingBegin( int32 Idx )
{
    bSliderDragged = true;
    
    // We want to record undo increments only when user lets go of the slider.
    FScopedTransaction Transaction(
        TEXT( HOUDINI_MODULE_RUNTIME ),
        LOCTEXT( "HoudiniAssetParameterIntChange", "Houdini Parameter Float: Changing a value" ),
        PrimaryObject );

    Modify();
}

void
UHoudiniAssetParameterFloat::OnSliderMovingFinish( float InValue, int32 Idx )
{
    bSliderDragged = false;

    // Mark this parameter as changed.
    MarkChanged( true );
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE