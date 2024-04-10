// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSetting.h"
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/RichTextMarkupProcessing.h"
#include "Engine/LocalPlayer.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSetting)

#define LOCTEXT_NAMESPACE "GameSetting"

#define UE_CAN_SHOW_SETTINGS_DEBUG_INFO (!UE_BUILD_SHIPPING)

namespace GameSettingsConsoleVars
{
#if UE_CAN_SHOW_SETTINGS_DEBUG_INFO
	int32 ShowDebugInfoMode = -1;

	static FAutoConsoleVariableRef CVarGameSettingsShowDebugInfo(
		TEXT("GameSettings.ShowDebugInfo"),
		ShowDebugInfoMode,
		TEXT("Should we show the developer name and class as part of dynamic details?\n")
		TEXT("  -1: Default (enabled in editor, disabled in -game or cooked builds)\n")
		TEXT("   0: Never show it\n")
		TEXT("   1: Always show it\n")
		TEXT("\n")
		TEXT("  Note: Shipping builds always disable this"),
		ECVF_Default);
#endif
}


//--------------------------------------
// UGameSetting
//--------------------------------------

void UGameSetting::Initialize(ULocalPlayer* InLocalPlayer)
{
	// If we've already gotten this local player we're already initialized.
	if (LocalPlayer == InLocalPlayer)
	{
		return;
	}

	LocalPlayer = InLocalPlayer;

	//TODO: GameSettings
	//LocalPlayer->OnPlayerLoggedIn().AddUObject(this, &UGameSetting::RefreshEditableState, true);

#if !UE_BUILD_SHIPPING
	ensureAlwaysMsgf(DevName != NAME_None, TEXT("You must provide a DevName for the setting."));
	ensureAlwaysMsgf(!DisplayName.IsEmpty(), TEXT("You must provide a DisplayName for settings."));
#endif

	for (const TSharedRef<FGameSettingEditCondition>& EditCondition : EditConditions)
	{
		EditCondition->Initialize(LocalPlayer);
	}

	// If there are any child settings go ahead and initialize them as well.
	for (UGameSetting* Setting : GetChildSettings())
	{
		Setting->Initialize(LocalPlayer);
	}

	Startup();
}

void UGameSetting::Startup()
{
	StartupComplete();
}

void UGameSetting::StartupComplete()
{
	ensureMsgf(!bReady, TEXT("StartupComplete called twice."));

	if (!bReady)
	{
		bReady = true;
		OnInitialized();
	}
}

void UGameSetting::Apply()
{
	OnApply();

	// Run through any edit conditions and let them know things changed.
	for (const TSharedRef<FGameSettingEditCondition>& EditCondition : EditConditions)
	{
		EditCondition->SettingApplied(LocalPlayer, this);
	}

	OnSettingAppliedEvent.Broadcast(this);
}

void UGameSetting::OnInitialized()
{
	ensureMsgf(bReady, TEXT("OnInitialized called directly instead of via StartupComplete."));
	EditableStateCache = ComputeEditableState();
}

void UGameSetting::OnApply()
{
	// No-Op by default.
}

UWorld* UGameSetting::GetWorld() const
{
	return LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
}

void UGameSetting::SetSettingParent(UGameSetting* InSettingParent)
{
	SettingParent = InSettingParent;
}

FGameSettingEditableState UGameSetting::ComputeEditableState() const
{
	FGameSettingEditableState EditState;

	// Does this setting itself have any special rules?
	OnGatherEditState(EditState);

	// Run through any edit conditions
	for (const TSharedRef<FGameSettingEditCondition>& EditCondition : EditConditions)
	{
		EditCondition->GatherEditState(LocalPlayer, EditState);
	}

	return EditState;
}

void UGameSetting::OnGatherEditState(FGameSettingEditableState& InOutEditState) const
{

}

const FString& UGameSetting::GetDescriptionPlainText() const
{
	RefreshPlainText();
	return AutoGenerated_DescriptionPlainText;
}

void UGameSetting::RefreshPlainText() const
{
	//TODO: GameSettings
	// TODO NDarnell Settings - Will need to recache if the language changes.

	if (bRefreshPlainSearchableText)
	{
		TArray<FTextLineParseResults> ActualResultsArray;
		FString ActualOutput;
		FDefaultRichTextMarkupParser::GetStaticInstance()->Process(ActualResultsArray, DescriptionRichText.ToString(), ActualOutput);

		AutoGenerated_DescriptionPlainText.Reset();
		for (const FTextLineParseResults& Line : ActualResultsArray)
		{
			for (const FTextRunParseResults& Run : Line.Runs)
			{
				if (Run.Name.IsEmpty())
				{
					AutoGenerated_DescriptionPlainText.Append(ActualOutput.Mid(Run.OriginalRange.BeginIndex, Run.OriginalRange.Len()));
				}
				else if (!Run.ContentRange.IsEmpty())
				{
					AutoGenerated_DescriptionPlainText.Append(ActualOutput.Mid(Run.ContentRange.BeginIndex, Run.ContentRange.Len()));
				}
			}
		}

		bRefreshPlainSearchableText = false;
	}
}

void UGameSetting::NotifySettingChanged(EGameSettingChangeReason Reason)
{
	OnSettingChanged(Reason);
	
	// Run through any edit conditions and let them know things changed.
	for (const TSharedRef<FGameSettingEditCondition>& EditCondition : EditConditions)
	{
		EditCondition->SettingChanged(LocalPlayer, this, Reason);
	}

	if (!bOnSettingChangedEventGuard)
	{
		TGuardValue<bool> Guard(bOnSettingChangedEventGuard, true);
		OnSettingChangedEvent.Broadcast(this, Reason);
	}
}

void UGameSetting::OnSettingChanged(EGameSettingChangeReason Reason)
{
	// No-Op
}

void UGameSetting::AddEditCondition(const TSharedRef<FGameSettingEditCondition>& InEditCondition)
{
	EditConditions.Add(InEditCondition);

	InEditCondition->OnEditConditionChangedEvent.AddUObject(this, &ThisClass::RefreshEditableState);
}

void UGameSetting::AddEditDependency(UGameSetting* DependencySetting)
{
	if (ensure(DependencySetting))
	{
		DependencySetting->OnSettingChangedEvent.AddUObject(this, &ThisClass::HandleEditDependencyChanged);
		DependencySetting->OnSettingEditConditionChangedEvent.AddUObject(this, &ThisClass::HandleEditDependencyChanged);
	}
}

void UGameSetting::RefreshEditableState(bool bNotifyEditConditionsChanged)
{
	// The LocalPlayer may be destroyed out from under us, if that happens,
	// we need to ignore attempts to refresh the editable state.
	if (!LocalPlayer)
	{
		return;
	}

	//TODO: GameSettings
	//// We should wait until the player is fully logged in before trying to refresh settings.
	//if (!LocalPlayer->IsLoggedIn())
	//{
	//	return;
	//}

	if (!bOnEditConditionsChangedEventGuard)
	{
		TGuardValue<bool> Guard(bOnEditConditionsChangedEventGuard, true);
	
		EditableStateCache = ComputeEditableState();

		if (bNotifyEditConditionsChanged)
		{
			NotifyEditConditionsChanged();
		}		
	}
}

void UGameSetting::NotifyEditConditionsChanged()
{
	OnEditConditionsChanged();

	OnSettingEditConditionChangedEvent.Broadcast(this);
}

void UGameSetting::OnEditConditionsChanged()
{

}

void UGameSetting::HandleEditDependencyChanged(UGameSetting* DependencySetting)
{
	OnDependencyChanged();
	RefreshEditableState();
}

void UGameSetting::HandleEditDependencyChanged(UGameSetting* DependencySetting, EGameSettingChangeReason Reason)
{
	OnDependencyChanged();
	RefreshEditableState();

	if (Reason != EGameSettingChangeReason::DependencyChanged)
	{
		NotifySettingChanged(EGameSettingChangeReason::DependencyChanged);
	}
}

void UGameSetting::OnDependencyChanged()
{

}

FText UGameSetting::GetDynamicDetails() const
{
	if (!LocalPlayer)
	{
		return FText::GetEmpty();
	}

	FText DynamicDetailsText = DynamicDetails.IsBound() ? DynamicDetails.Execute(*LocalPlayer) : FText::GetEmpty();
	
#if UE_CAN_SHOW_SETTINGS_DEBUG_INFO
	if ((GameSettingsConsoleVars::ShowDebugInfoMode == 1) || ((GameSettingsConsoleVars::ShowDebugInfoMode == -1) && GIsEditor))
	{
		const FString DevSettingDetails = FString::Printf(TEXT("%s<debug>DevName: %s</>\n<debug>Class: %s</>"),
			DynamicDetailsText.IsEmpty() ? TEXT("") : TEXT("\n"),
			*DevName.ToString(),
			*GetClass()->GetName());

		DynamicDetailsText = FText::Format(LOCTEXT("DevDynamicDetailsFormat", "{0}{1}"),
			DynamicDetailsText,
			FText::FromString(DevSettingDetails));
	}
#endif

	return DynamicDetailsText;
}

FText UGameSetting::GetDynamicDetailsInternal() const
{
	return FText::GetEmpty();
}

UGameSetting::FStringCultureCache::FStringCultureCache(TFunction<FString()> InStringGetter)
	: Culture(FInternationalization::Get().GetCurrentCulture())
	, StringGetter(InStringGetter)
{
	StringCache = StringGetter();
}

void UGameSetting::FStringCultureCache::Invalidate()
{
	StringCache = StringGetter();
	Culture = FInternationalization::Get().GetCurrentCulture();
}

FString UGameSetting::FStringCultureCache::Get() const
{
	if (Culture == FInternationalization::Get().GetCurrentCulture())
	{
		return StringCache;
	}

	StringCache = StringGetter();
	Culture = FInternationalization::Get().GetCurrentCulture();

	return StringCache;
}

#undef LOCTEXT_NAMESPACE

