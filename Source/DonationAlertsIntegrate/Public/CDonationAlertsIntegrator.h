#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "IHttpRouter.h"
#include "IWebSocket.h"
#include "Interfaces/IHttpRequest.h"
#include "CDonationAlertsIntegrator.generated.h"

UENUM(BlueprintType)
enum class ECurrencyType :uint8
{
	ECT_RUB UMETA(DisplayName = "RUB"),
	ECT_EUR UMETA(DisplayName = "EUR"),
	ECT_USD UMETA(DisplayName = "USD"),
	ECT_UAH UMETA(DisplayName = "UAH"),
	ECT_BYN UMETA(DisplayName = "BYN"),
	ECT_KZT UMETA(DisplayName = "KZT"),
	ECT_BRL UMETA(DisplayName = "BRL"),
	ECT_TRY UMETA(DisplayName = "TRY"),
	ECT_PLN UMETA(DisplayName = "PLN"),

	ECT_MAX UMETA(Hidden)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FDonateReceived, FString, Username, FString, Message, int32, Amount, ECurrencyType, Currency);
DECLARE_LOG_CATEGORY_EXTERN(LogDonationAlerts, Log, All);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DONATIONALERTSINTEGRATE_API UCDonationAlertsIntegrator : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FDonateReceived OnDonateReceived;
	
	UFUNCTION(BlueprintCallable, Category = "DonationAlerts")
	void Connect(FString AppID = "0000", int32 Port = 8000, FString CallbackURL = TEXT("http://localhost"), FString CallbackPath = TEXT("/callback"));

	UFUNCTION(BlueprintCallable, Category = "DonationAlerts")
	void Disconnect();
	
protected:
	TSharedPtr<IHttpRouter> HttpRouter;
	FHttpRouteHandle URLReceived;
	FHttpRouteHandle Callback;
	TSharedPtr<IWebSocket> WebSocket;
	int32 MessageID = 1;
	
	FString PathReceivingURL = TEXT("/get_url");
	const FString WebPage = TEXT("<!DOCTYPE html><html><title>DonationAlerts UE</title><body><h1>DonationAlerts API</h1><p>Successful authorization.</p><script>var currentURL = window.location.hash; var oReq = new XMLHttpRequest(); oReq.open(\"POST\", \"get_url\", true); oReq.setRequestHeader(\"Content-type\", \"application/x-www-form-urlencoded\"); oReq.send(\"currentURL=\"+currentURL); </script></body></html>");

	FString AccessToken;
	FString ProfileConnectionToken;
	int32 ProfileID;
	FString ClientID;
	
	void GetUserProfile();
	void GetUserProfileRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void ConnectToTheCentrifugoWebSocket();
	void ParseMessage(FString Message);
	void SubscribeCentrifugoChannelRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	virtual void BeginDestroy() override;
	static ECurrencyType GetCurrencyType(const FString CurrencyText);
};