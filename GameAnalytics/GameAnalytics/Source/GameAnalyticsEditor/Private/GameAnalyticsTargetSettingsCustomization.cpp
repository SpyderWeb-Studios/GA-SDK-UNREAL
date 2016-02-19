// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameAnalyticsEditorPrivatePCH.h"
#include "GameAnalyticsTargetSettingsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "PropertyEditing.h"

#include "ObjectEditorUtils.h"
#include "IDocumentation.h"
#include "OutputDevice.h"

#include "Json.h"

#define LOCTEXT_NAMESPACE "GameAnalyticsTargetSettingsCustomization"

static void OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
{
	const FString* URL = Metadata.Find(TEXT("href"));

	if (URL)
	{
		FPlatformProcess::LaunchURL(**URL, nullptr, nullptr);
	}
}

class HttpCaller
{
public:
	void Close();
	void CreateAccountHttp();
	void OnCreateAccountResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	void LoginHttp(FName Email, FName Password);
	void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	void GetUserDataHttp(FString Token);
	void OnGetUserDataResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

private:
	FHttpModule* Http;
	bool bIsBusy;
};

void HttpCaller::Close()
{

}


//////////////////////////////////////////////////////////////////////////
// FGameAnalyticsTargetSettingsCustomization
namespace FGameAnalyticsTargetSettingsCustomizationConstants
{
	const FText DisabledTip = LOCTEXT("GitHubSourceRequiredToolTip", "This requires GitHub source.");
}

TSharedRef<IDetailCustomization> FGameAnalyticsTargetSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FGameAnalyticsTargetSettingsCustomization);
}

/*FGameAnalyticsTargetSettingsCustomization::FGameAnalyticsTargetSettingsCustomization()
{
}*/

void FGameAnalyticsTargetSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	SavedLayoutBuilder = &DetailLayout;

	IDetailCategoryBuilder& SetupCategory = DetailLayout.EditCategory(TEXT("Account"), FText::GetEmpty(), ECategoryPriority::Variable);
	IDetailCategoryBuilder& IosCategory = DetailLayout.EditCategory(TEXT("IosSetup"), FText::GetEmpty(), ECategoryPriority::Variable);
	//IDetailCategoryBuilder& AndroidCategory = DetailLayout.EditCategory(TEXT("AndroidSetup"), FText::GetEmpty(), ECategoryPriority::Variable);

	const FText StudioMenuStringIos = FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedStudioIos.Name.IsEmpty() ? LOCTEXT("StudioMenu", "Select Studio") : FText::FromString(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedStudioIos.Name);
	const FText GameMenuStringIos = FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos.Name.IsEmpty() ? LOCTEXT("GameMenu", "Select Game") : FText::FromString(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos.Name);
	//const FText StudioMenuStringAndroid = FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedStudioAndroid.Name.IsEmpty() ? LOCTEXT("StudioMenu", "Select Studio") : FText::FromString(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedStudioAndroid.Name);
	//const FText GameMenuStringAndroid = FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid.Name.IsEmpty() ? LOCTEXT("GameMenu", "Select Game") : FText::FromString(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid.Name);
	//const FText PlatformMenuString = LOCTEXT("PlatformMenu", "Select Platform");

	SetupCategory.AddCustomRow(LOCTEXT("DocumentationInfo", "Documentation Info"), false)
		.WholeRowWidget
		[
			SNew(SBorder)
			.Padding(1)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(10, 10, 10, 10))
				.FillWidth(1.0f)
				[
					SNew(SRichTextBlock)
					.Text(LOCTEXT("DocumentationInfoMessage", "<a id=\"browser\" href=\"http://support.gameanalytics.com\" style=\"HoverOnlyHyperlink\">View the GameAnalytics Unreal documentation here.</> Please login to your GameAnalytics account to automatically retrieve game and secret keys. If you don?t have an account yet, <a id=\"browser\" href=\"https://go.gameanalytics.com/signup\" style=\"HoverOnlyHyperlink\">please sign up to create your account.</>"))
					.TextStyle(FEditorStyle::Get(), "MessageLog")
					.DecoratorStyleSet(&FEditorStyle::Get())
					.AutoWrapText(true)
					+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateStatic(&OnBrowserLinkClicked))
				]
			]
		];

	SetupCategory.AddCustomRow(LOCTEXT("LoginUserName", "Login User Name"), false)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UserNameLabel", "User Name"))
				.Font(DetailLayout.GetDetailFont())
			]
		]
	.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SEditableTextBox)
				.Text(LOCTEXT("UserName", ""))
				.ToolTipText(LOCTEXT("UserName_Tooltip", "Your GameAnalytics account user name."))
				.OnTextCommitted(this, &FGameAnalyticsTargetSettingsCustomization::UsernameEntered)
				.OnTextChanged(this, &FGameAnalyticsTargetSettingsCustomization::UsernameChanged)
			]
		];

	SetupCategory.AddCustomRow(LOCTEXT("LoginPassword", "Login Password"), false)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PasswordLabel", "Password"))
				.Font(DetailLayout.GetDetailFont())
			]
		]
	.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SEditableTextBox)
				.Text(LOCTEXT("Password", ""))
				.ToolTipText(LOCTEXT("Password_Tooltip", "Your GameAnalytics account password."))
				.IsPassword(true)
				.OnTextCommitted(this, &FGameAnalyticsTargetSettingsCustomization::PasswordEntered)
				.OnTextChanged(this, &FGameAnalyticsTargetSettingsCustomization::PasswordChanged)
			]
		];

	SetupCategory.AddCustomRow(LOCTEXT("LoginButtonRow", "Login Button"), false)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LoginButtonLabel", "Login"))
				.Font(DetailLayout.GetDetailFont())
			]
		]
	.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("LoginButton", "Login"))
				.ToolTipText(LOCTEXT("LoginButton_Tooltip", "Login with your GameAnalytics account and password."))
				.OnClicked(this, &FGameAnalyticsTargetSettingsCustomization::Login)
			]
		];

	//if (FGameAnalyticsTargetSettingsCustomization::getInstance().StudiosAndGames.Num() > 0)
	//{
	IosCategory.AddCustomRow(LOCTEXT("StudiosRow", "Studios"), false)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("StudiosLabel", "Select Studio"))
				.Font(DetailLayout.GetDetailFont())
			]
		]
	.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SComboButton)
				//.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("SelectStudioTooltip", "Studio"))
				.OnGetMenuContent(this, &FGameAnalyticsTargetSettingsCustomization::UpdateStudiosIos)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(StudioMenuStringIos)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				.MenuContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LoginToSelectStudioLabel", "Login To Select Studio."))
						.Font(DetailLayout.GetDetailFont())
					]
			]
		];
	//}

	if (!FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedStudioIos.Name.IsEmpty())
	{
		IosCategory.AddCustomRow(LOCTEXT("GamesRow", "Games"), false)
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0, 1, 0, 1))
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GamesLabel", "Select Game"))
					.Font(DetailLayout.GetDetailFont())
				]
			]
		.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SComboButton)
					.VAlign(VAlign_Center)
					.ToolTipText(LOCTEXT("SelectGameTooltip", "Game"))
					.OnGetMenuContent(this, &FGameAnalyticsTargetSettingsCustomization::UpdateGamesIos)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(GameMenuStringIos)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
					.MenuContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("LoginToSelectGameLabel", "Please select a Studio."))
							.Font(DetailLayout.GetDetailFont())
						]
				]
			];
	}

	//if (FGameAnalyticsTargetSettingsCustomization::getInstance().StudiosAndGames.Num() > 0)
	//{
	/*AndroidCategory.AddCustomRow(LOCTEXT("StudiosRow", "Studios"), false)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0, 1, 0, 1))
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("StudiosLabel", "Select Studio"))
				.Font(DetailLayout.GetDetailFont())
			]
		]
	.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SComboButton)
				//.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("SelectStudioTooltip", "Studio"))
				.OnGetMenuContent(this, &FGameAnalyticsTargetSettingsCustomization::UpdateStudiosAndroid)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(StudioMenuStringAndroid)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				.MenuContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LoginToSelectStudioLabel", "Login To Select Studio."))
						.Font(DetailLayout.GetDetailFont())
					]
			]
		];*/
	//}

	/*if (!FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedStudioAndroid.Name.IsEmpty())
	{
		AndroidCategory.AddCustomRow(LOCTEXT("GamesRow", "Games"), false)
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0, 1, 0, 1))
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GamesLabel", "Select Game"))
					.Font(DetailLayout.GetDetailFont())
				]
			]
		.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SComboButton)
					.VAlign(VAlign_Center)
					.ToolTipText(LOCTEXT("SelectGameTooltip", "Game"))
					.OnGetMenuContent(this, &FGameAnalyticsTargetSettingsCustomization::UpdateGamesAndroid)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(GameMenuStringAndroid)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
					.MenuContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("LoginToSelectGameLabel", "Please select a Studio."))
							.Font(DetailLayout.GetDetailFont())
						]
				]
			];
	}*/

	/*if (!FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGame.Name.IsEmpty())
	{
		SetupCategory.AddCustomRow(LOCTEXT("PlatformRow", "Platforms"), false)
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(0, 1, 0, 1))
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PlatformsLabel", "Platforms"))
					.Font(DetailLayout.GetDetailFont())
				]
			]
		.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SComboButton)
					.VAlign(VAlign_Center)
					.ToolTipText(LOCTEXT("SelectPlatformTooltip", "Platform"))
					.OnGetMenuContent(this, &FGameAnalyticsTargetSettingsCustomization::UpdatePlatforms)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(PlatformMenuString)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
					.MenuContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("LoginToSelectPlatformLabel", "Please select a Studio and Game."))
							.Font(DetailLayout.GetDetailFont())
						]
				]
			];
	}*/
}

/*void FGameAnalyticsTargetSettingsCustomization::OnIosMenuItemClicked()
{
	if (!GConfig) return;

	UGameAnalyticsProjectSettings* GameAnalyticsProjectSettings = GetMutableDefault< UGameAnalyticsProjectSettings >();

	if (FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos.GameKey == GameAnalyticsProjectSettings->AndroidGameKey)
	{
		UE_LOG(LogTemp, Warning, TEXT("This game's keys are already in used. You cannot use the same keys for different platforms."));
		return;
	}

	FString GameAnalyticsSection = "/Script/GameAnalyticsEditor.GameAnalyticsProjectSettings";

	FString ConfigsPath = GEngineIni;//FPaths::SourceConfigDir();
	//ConfigsPath += "DefaultEngine.ini";

	GConfig->SetString(
		*GameAnalyticsSection,
		TEXT("IosGameKey"),
		*(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos.GameKey),
		ConfigsPath
		);

	GConfig->SetString(
		*GameAnalyticsSection,
		TEXT("IosSecretKey"),
		*(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos.SecretKey),
		ConfigsPath
		);

	GConfig->Flush(false, GEngineIni);

	UE_LOG(LogTemp, Warning, TEXT("Platform selected: iOS, Game Key Saved: %s"), *(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos.GameKey));
	UE_LOG(LogTemp, Warning, TEXT("Saved at: %s"), *ConfigsPath);

	GameAnalyticsProjectSettings->ReloadConfig();

	SavedLayoutBuilder->ForceRefreshDetails();
}*/

/*void FGameAnalyticsTargetSettingsCustomization::OnAndroidMenuItemClicked()
{
	if (!GConfig) return;

	UGameAnalyticsProjectSettings* GameAnalyticsProjectSettings = GetMutableDefault< UGameAnalyticsProjectSettings >();

	if (FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid.GameKey == GameAnalyticsProjectSettings->IosGameKey)
	{
		UE_LOG(LogTemp, Warning, TEXT("This game's keys are already in used. You cannot use the same keys for different platforms."));
		return;
	}

	FString GameAnalyticsSection = "/Script/GameAnalyticsEditor.GameAnalyticsProjectSettings";

	FString ConfigsPath = GEngineIni;//FPaths::SourceConfigDir();
	//ConfigsPath += "DefaultEngine.ini";

	GConfig->SetString(
		*GameAnalyticsSection,
		TEXT("AndroidGameKey"),
		*(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid.GameKey),
		ConfigsPath
		);

	GConfig->SetString(
		*GameAnalyticsSection,
		TEXT("AndroidSecretKey"),
		*(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid.SecretKey),
		ConfigsPath
		);

	GConfig->Flush(false, GEngineIni);

	UE_LOG(LogTemp, Warning, TEXT("Platform selected: Android, Game Key Saved: %s"), *(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid.GameKey));
	UE_LOG(LogTemp, Warning, TEXT("Saved at: %s"), *ConfigsPath);

	GameAnalyticsProjectSettings->ReloadConfig();

	SavedLayoutBuilder->ForceRefreshDetails();
}*/

TSharedRef<SWidget> FGameAnalyticsTargetSettingsCustomization::UpdateGamesIos() const
{
	FMenuBuilder GameMenuBuilder(true, NULL);
	{
		GameMenuBuilder.BeginSection("Games");
		{
			for (auto& g : FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedStudioIos.Games)
			{
				GameMenuBuilder.AddMenuEntry(FText::FromString(g.Name), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &FGameAnalyticsTargetSettingsCustomization::OnGameMenuItemClickedIos, g)), NAME_None);
			}
		}
		GameMenuBuilder.EndSection();
	}

	return GameMenuBuilder.MakeWidget();
}

/*TSharedRef<SWidget> FGameAnalyticsTargetSettingsCustomization::UpdateGamesAndroid() const
{
	FMenuBuilder GameMenuBuilder(true, NULL);
	{
		GameMenuBuilder.BeginSection("Games");
		{
			for (auto& g : FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedStudioAndroid.Games)
			{
				GameMenuBuilder.AddMenuEntry(FText::FromString(g.Name), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &FGameAnalyticsTargetSettingsCustomization::OnGameMenuItemClickedAndroid, g)), NAME_None);
			}
		}
		GameMenuBuilder.EndSection();
	}

	return GameMenuBuilder.MakeWidget();
}*/

void FGameAnalyticsTargetSettingsCustomization::OnGameMenuItemClickedIos(FGameAnalyticsTargetSettingsCustomization::GAME GameItem)
{
	FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos = GameItem;

	UE_LOG(LogTemp, Warning, TEXT("Game selected: %s"), *GameItem.Name);

	if (!GConfig) return;

	UGameAnalyticsProjectSettings* GameAnalyticsProjectSettings = GetMutableDefault< UGameAnalyticsProjectSettings >();

	/*if (FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos.GameKey == GameAnalyticsProjectSettings->AndroidGameKey)
	{
		UE_LOG(LogTemp, Warning, TEXT("This game's keys are already in used. You cannot use the same keys for different platforms."));
		return;
	}*/

	FString GameAnalyticsSection = "/Script/GameAnalyticsEditor.GameAnalyticsProjectSettings";

	GConfig->SetString(*GameAnalyticsSection, TEXT("IosGameKey"), *(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos.GameKey), GEngineIni);
	GConfig->SetString(*GameAnalyticsSection, TEXT("IosSecretKey"), *(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos.SecretKey), GEngineIni);

	GConfig->Flush(false, GEngineIni);

	UE_LOG(LogTemp, Warning, TEXT("Platform selected: iOS, Game Key Saved: %s"), *(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos.GameKey));
	UE_LOG(LogTemp, Warning, TEXT("Saved at: %s"), *GEngineIni);

	GameAnalyticsProjectSettings->ReloadConfig();

	SavedLayoutBuilder->ForceRefreshDetails();
}

/*void FGameAnalyticsTargetSettingsCustomization::OnGameMenuItemClickedAndroid(FGameAnalyticsTargetSettingsCustomization::GAME GameItem)
{
	FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid = GameItem;

	UE_LOG(LogTemp, Warning, TEXT("Game selected: %s"), *GameItem.Name);

	if (!GConfig) return;

	UGameAnalyticsProjectSettings* GameAnalyticsProjectSettings = GetMutableDefault< UGameAnalyticsProjectSettings >();

	if (FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid.GameKey == GameAnalyticsProjectSettings->IosGameKey)
	{
		UE_LOG(LogTemp, Warning, TEXT("This game's keys are already in used. You cannot use the same keys for different platforms."));
		return;
	}

	FString GameAnalyticsSection = "/Script/GameAnalyticsEditor.GameAnalyticsProjectSettings";

	FString ConfigsPath = GEngineIni;//FPaths::SourceConfigDir();
	//ConfigsPath += "DefaultEngine.ini";

	GConfig->SetString(
		*GameAnalyticsSection,
		TEXT("AndroidGameKey"),
		*(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid.GameKey),
		ConfigsPath
		);

	GConfig->SetString(
		*GameAnalyticsSection,
		TEXT("AndroidSecretKey"),
		*(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid.SecretKey),
		ConfigsPath
		);

	GConfig->Flush(false, GEngineIni);

	UE_LOG(LogTemp, Warning, TEXT("Platform selected: Android, Game Key Saved: %s"), *(FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid.GameKey));
	//UE_LOG(LogTemp, Warning, TEXT("Saved at: %s"), *ConfigsPath);

	GameAnalyticsProjectSettings->ReloadConfig();

	SavedLayoutBuilder->ForceRefreshDetails();
}*/

TSharedRef<SWidget> FGameAnalyticsTargetSettingsCustomization::UpdateStudiosIos() const
{
	FMenuBuilder StudioMenuBuilder(true, NULL);
	{
		StudioMenuBuilder.BeginSection("Studios");
		{
			for (auto& s : FGameAnalyticsTargetSettingsCustomization::getInstance().StudiosAndGames)
			{
				StudioMenuBuilder.AddMenuEntry(FText::FromString(s.Name), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &FGameAnalyticsTargetSettingsCustomization::OnStudioMenuItemClickedIos, s)), NAME_None);
			}
		}
		StudioMenuBuilder.EndSection();
	}

	return StudioMenuBuilder.MakeWidget();
}

/*TSharedRef<SWidget> FGameAnalyticsTargetSettingsCustomization::UpdateStudiosAndroid() const
{
	FMenuBuilder StudioMenuBuilder(true, NULL);
	{
		StudioMenuBuilder.BeginSection("Studios");
		{
			for (auto& s : FGameAnalyticsTargetSettingsCustomization::getInstance().StudiosAndGames)
			{
				StudioMenuBuilder.AddMenuEntry(FText::FromString(s.Name), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &FGameAnalyticsTargetSettingsCustomization::OnStudioMenuItemClickedAndroid, s)), NAME_None);
			}
		}
		StudioMenuBuilder.EndSection();
	}

	return StudioMenuBuilder.MakeWidget();
}*/

void FGameAnalyticsTargetSettingsCustomization::OnStudioMenuItemClickedIos(FGameAnalyticsTargetSettingsCustomization::STUDIO StudioItem)
{
	FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedStudioIos = StudioItem;

	FGameAnalyticsTargetSettingsCustomization::GAME Game;
	FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameIos = Game;

	UE_LOG(LogTemp, Warning, TEXT("iOS Studio selected: %s"), *StudioItem.Name);

	SavedLayoutBuilder->ForceRefreshDetails();
}

/*void FGameAnalyticsTargetSettingsCustomization::OnStudioMenuItemClickedAndroid(FGameAnalyticsTargetSettingsCustomization::STUDIO StudioItem)
{
	FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedStudioAndroid = StudioItem;

	FGameAnalyticsTargetSettingsCustomization::GAME Game;
	FGameAnalyticsTargetSettingsCustomization::getInstance().SelectedGameAndroid = Game;

	UE_LOG(LogTemp, Warning, TEXT("Android Studio selected: %s"), *StudioItem.Name);

	SavedLayoutBuilder->ForceRefreshDetails();
}*/

void FGameAnalyticsTargetSettingsCustomization::FillStudiosString(TArray<STUDIO> StudioList)
{
	StudiosAndGames = StudioList;

	//UE_LOG(LogTemp, Warning, TEXT("Studio length: %i"), StudioList.Num());
	//UE_LOG(LogTemp, Warning, TEXT("Studio and games length: %i"), StudiosAndGames.Num());
}

void FGameAnalyticsTargetSettingsCustomization::UsernameChanged(const FText& NewText)
{
	UsernameEntered(NewText, ETextCommit::Default);
}

void FGameAnalyticsTargetSettingsCustomization::UsernameEntered(const FText& NewText, ETextCommit::Type CommitInfo)
{
	{
		FName NewName = FName(*NewText.ToString());
		// we should accept NAME_None, that will invalidate "accpet" button
		if (NewName != Username)
		{
			Username = NewName;
		}
	}
}

void FGameAnalyticsTargetSettingsCustomization::PasswordChanged(const FText& NewText)
{
	PasswordEntered(NewText, ETextCommit::Default);
}

void FGameAnalyticsTargetSettingsCustomization::PasswordEntered(const FText& NewText, ETextCommit::Type CommitInfo)
{
	{
		FName NewName = FName(*NewText.ToString());
		// we should accept NAME_None, that will invalidate "accpet" button
		if (NewName != Password)
		{
			Password = NewName;
		}
	}
}

FReply FGameAnalyticsTargetSettingsCustomization::Login()
{
	HttpCaller* Caller = new HttpCaller();
	Caller->LoginHttp(Username, Password);

	return FReply::Handled();
}

void HttpCaller::LoginHttp(FName Email, FName Password)
{
	if (!Http)
	{
		Http = &FHttpModule::Get();
	}

	if (bIsBusy) return;
	if (!Http) return;
	if (!Http->IsHttpEnabled()) return;

	bIsBusy = true;

	//~~~~~~~~~~~~~~~~~~~~~~

	TSharedRef < IHttpRequest > Request = Http->CreateRequest();

	/**
	* Instantiates a new Http request for the current platform
	*
	* @return new Http request instance
	*/
	//virtual TSharedRef CreateRequest();
	//~~~~~~~~~~~~~~~

	Request->SetVerb("POST");
	Request->SetURL("https://userapi.gameanalytics.com/ext/v1/token");
	Request->SetContentAsString("{\"email\":\"" + Email.ToString() + "\", \"password\":\"" + Password.ToString() + "\"}");
	Request->SetHeader("User-Agent", "GameAnalyticsLinkClient/1.0");
	Request->SetHeader("Content-Type", "application/x-www-form-urlencoded");
	Request->SetHeader("X-Caller", "UnrealEditor");
	Request->SetHeader("X-Caller-Version", "1.0.0"); // Todo : get unreal editor version
	Request->SetHeader("X-Caller-Platform", "Windows"); // Todo: get development platform

	Request->OnProcessRequestComplete().BindRaw(this, &HttpCaller::OnLoginResponseReceived);

	if (!Request->ProcessRequest())
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to process HTTP Request!"));
	}

	return;
}

void HttpCaller::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bIsBusy = false;

	FString MessageBody = "";

	// If HTTP fails client-side, this will still be called but with a NULL shared pointer!
	if (!Response.IsValid())
	{
		MessageBody = "Error: Unable to process HTTP Request!";
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		MessageBody = Response->GetContentAsString();

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef< TJsonReader<TCHAR> > JsonReader = TJsonReaderFactory<TCHAR>::Create(MessageBody);

		if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
		{
			TArray<TSharedPtr<FJsonValue>> Errors = JsonObject->GetArrayField(TEXT("errors"));

			if (Errors.Num() == 0)
			{
				TArray<TSharedPtr<FJsonValue>> Results = JsonObject->GetArrayField(TEXT("results"));

				TSharedPtr<FJsonObject> JsonResults = Results[0]->AsObject();

				FString Token = JsonResults->GetStringField(TEXT("token"));

				GetUserDataHttp(Token);
                
                FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Login successful.\nPlease select your studio below.")));
			}
            else
            {
                FString errorMessage = "Login failed:\n";
                for(int i = 0; i < Errors.Num(); ++i)
                {
                    TSharedPtr<FJsonObject> JsonResults = Errors[i]->AsObject();
                    FString msg = JsonResults->GetStringField(TEXT("msg"));
                    errorMessage += msg + "\n";
                }
                FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(errorMessage));
            }
		}
	}
	else
	{
		MessageBody = Response->GetContentAsString();
        
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef< TJsonReader<TCHAR> > JsonReader = TJsonReaderFactory<TCHAR>::Create(MessageBody);
        
        
        if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
        {
            TArray<TSharedPtr<FJsonValue>> Errors = JsonObject->GetArrayField(TEXT("errors"));
            
            FString errorMessage = "Login failed:\n";
            for(int i = 0; i < Errors.Num(); ++i)
            {
                TSharedPtr<FJsonObject> JsonResults = Errors[i]->AsObject();
                FString msg = JsonResults->GetStringField(TEXT("msg"));
                errorMessage += msg + "\n";
            }
            FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(errorMessage));
        }
        else
        {
            FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Login failed:\n" + MessageBody)));
        }
	}

	//UE_LOG(LogTemp, Warning, TEXT("%s"), *MessageBody);
}

void HttpCaller::GetUserDataHttp(FString Token)
{
	//UE_LOG(LogTemp, Warning, TEXT("START GETUSERDATA!"));

	if (!Http)
	{
		Http = &FHttpModule::Get();
	}

	if (bIsBusy) return;
	if (!Http) return;
	if (!Http->IsHttpEnabled()) return;

	bIsBusy = true;

	//~~~~~~~~~~~~~~~~~~~~~~

	TSharedRef < IHttpRequest > Request = Http->CreateRequest();

	/**
	* Instantiates a new Http request for the current platform
	*
	* @return new Http request instance
	*/
	//virtual TSharedRef CreateRequest();
	//~~~~~~~~~~~~~~~

	Request->SetVerb("GET");
	Request->SetURL("https://userapi.gameanalytics.com/ext/v1/user");
	Request->SetHeader("User-Agent", "GameAnalyticsLinkClient/1.0");
	Request->SetHeader("Content-Type", "application/x-www-form-urlencoded");
	Request->SetHeader("X-Authorization", Token);
	Request->SetHeader("X-Caller", "UnrealEditor");
	Request->SetHeader("X-Caller-Version", "1.0.0"); // Todo : get unreal editor version
	Request->SetHeader("X-Caller-Platform", "Windows"); // Todo: get development platform

	Request->OnProcessRequestComplete().BindRaw(this, &HttpCaller::OnGetUserDataResponseReceived);

	if (!Request->ProcessRequest())
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to process HTTP Request!"));
	}

	//UE_LOG(LogTemp, Warning, TEXT("END GETUSERDATA!"));

	return;
}

void HttpCaller::OnGetUserDataResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bIsBusy = false;

	FString MessageBody = "";

	// If HTTP fails client-side, this will still be called but with a NULL shared pointer!
	if (!Response.IsValid())
	{
		MessageBody = "Error: Unable to process HTTP Request!";
	}
	else if (EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		MessageBody = Response->GetContentAsString();

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef< TJsonReader<TCHAR> > JsonReader = TJsonReaderFactory<TCHAR>::Create(MessageBody);

		if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
		{
			TArray<TSharedPtr<FJsonValue>> Errors = JsonObject->GetArrayField(TEXT("errors"));

			if (Errors.Num() == 0)
			{
				TArray<TSharedPtr<FJsonValue>> Results = JsonObject->GetArrayField(TEXT("results"));

				TSharedPtr<FJsonObject> JsonResults = Results[0]->AsObject();

				TArray<TSharedPtr<FJsonValue>> Studios = JsonResults->GetArrayField(TEXT("studios"));

				TArray<FGameAnalyticsTargetSettingsCustomization::STUDIO> StudioList;

				for (int i = 0; i < Studios.Num(); i++)
				{
					TSharedPtr<FJsonObject> JsonStudio = Studios[i]->AsObject();

					FString StudioName = JsonStudio->GetStringField(TEXT("name"));

					FGameAnalyticsTargetSettingsCustomization::STUDIO Studio;
					Studio.Name = JsonStudio->GetStringField(TEXT("name"));
					Studio.Id = (int)JsonStudio->GetNumberField(TEXT("id"));

					TArray<TSharedPtr<FJsonValue>> Games = JsonStudio->GetArrayField(TEXT("games"));

					for (int u = 0; u < Games.Num(); u++)
					{
						TSharedPtr<FJsonObject> JsonGame = Games[u]->AsObject();

						FGameAnalyticsTargetSettingsCustomization::GAME Game;
						Game.Name = JsonGame->GetStringField(TEXT("name"));
						Game.Id = (int)JsonGame->GetNumberField(TEXT("id"));
						Game.GameKey = JsonGame->GetStringField(TEXT("key"));
						Game.SecretKey = JsonGame->GetStringField(TEXT("secret"));

						Studio.Games.Add(Game);
					}

					StudioList.Add(Studio);
				}

				FGameAnalyticsTargetSettingsCustomization::getInstance().FillStudiosString(StudioList);
			}
		}
	}
	else
	{
		MessageBody = Response->GetContentAsString();
	}

	//UE_LOG(LogTemp, Warning, TEXT("%s"), *MessageBody);
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE