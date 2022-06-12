#include "CDonationAlertsIntegrator.h"

#include "HttpModule.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "Json.h"
#include "WebSocketsModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IHttpResponse.h"

DEFINE_LOG_CATEGORY(LogDonationAlerts);

	
/* STEP 1: Authorization (AccessToken)*/
void UCDonationAlertsIntegrator::Connect(FString AppID, int32 Port, FString CallbackURL,  FString CallbackPath)
{
	HttpRouter = FHttpServerModule::Get().GetHttpRouter(Port);

	const FHttpRequestHandler OnCallback = [&](const FHttpServerRequest& ServerRequest, const FHttpResultCallback& ResultCallback) -> bool
	{
		TUniquePtr<FHttpServerResponse> ServerResponse = FHttpServerResponse::Create(WebPage, TEXT("text/html"));
		ResultCallback(MoveTemp(ServerResponse));
		return true;
	};

	const FHttpRequestHandler OnURLReceived = [&](const FHttpServerRequest& ServerRequest, const FHttpResultCallback& ResultCallback) -> bool
	{
		TArray<uint8> Body = ServerRequest.Body;
		Body.Add((uint8)'\0');
		FString Data = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Body.GetData())));
		Data.RemoveFromStart(TEXT("currentURL=#access_token="));
		Data.Split("&", &AccessToken, nullptr);
		if(AccessToken.IsEmpty()) return false;
		GetUserProfile();

		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(TEXT("{\"success\": true}"), TEXT("application/json"));
		ResultCallback(MoveTemp(Response));
		return true;
	};
	
	Callback = HttpRouter->BindRoute(FHttpPath(CallbackPath), EHttpServerRequestVerbs::VERB_GET, OnCallback);
	URLReceived = HttpRouter->BindRoute(FHttpPath(PathReceivingURL), EHttpServerRequestVerbs::VERB_POST, OnURLReceived);
	FHttpServerModule::Get().StartAllListeners();

	const FString URLCallback = FString::Printf(TEXT("%s:%d%s"),*CallbackURL, Port, *CallbackPath);
	const FString URL = FString::Printf(TEXT("https://www.donationalerts.com/oauth/authorize?client_id=%s&redirect_uri=%s&response_type=token&scope=%s"),
		*AppID,
		*URLCallback,
		*FGenericPlatformHttp::UrlEncode(TEXT("oauth-user-show oauth-donation-subscribe")));
	FPlatformProcess::LaunchURL(*URL, TEXT(""), nullptr);
}

/* STEP 2: Http request to get user profile */
void UCDonationAlertsIntegrator::GetUserProfile()
{
	const FHttpRequestPtr HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(TEXT("https://www.donationalerts.com/api/v1/user/oauth"));
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UCDonationAlertsIntegrator::GetUserProfileRequestComplete);
	HttpRequest->ProcessRequest();
}

/* STEP 3: (ProfileID/ProfileConnectionToken) | ConnectToTheCentrifugo  */
void UCDonationAlertsIntegrator::GetUserProfileRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	const FString ResponseContent = HttpResponse->GetContentAsString();
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject)) return;
	if (!JsonObject->HasTypedField<EJson::Object>(TEXT("data"))) return;
	
	ProfileID = JsonObject->GetObjectField(TEXT("data"))->GetNumberField(TEXT("id"));
	ProfileConnectionToken = JsonObject->GetObjectField(TEXT("data"))->GetStringField(TEXT("socket_connection_token"));
	
	if(ProfileConnectionToken.IsEmpty()) return;
	
	ConnectToTheCentrifugoWebSocket();
}

/* STEP 4: Web socket connection */
void UCDonationAlertsIntegrator::ConnectToTheCentrifugoWebSocket()
{
	const FString EndpointURL = TEXT("wss://centrifugo.donationalerts.com/connection/websocket");
	const FString Protocol = TEXT("wss");
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(EndpointURL, Protocol);

	WebSocket->OnConnected().AddWeakLambda(this, [this]() -> void
	{
		UE_LOG(LogDonationAlerts, Warning, TEXT("Successful connection to DonationAlerts"));
		const FString Message = FString::Printf(TEXT("{\"params\":{\"token\":\"%s\"},\"id\":%d}"), *ProfileConnectionToken, MessageID);
		WebSocket->Send(Message);
		MessageID++;
	});
	WebSocket->OnMessage().AddWeakLambda(this, [this](const FString& Message) -> void
	{
		ParseMessage(Message);
	});
	WebSocket->Connect();
}

/* STEP 5: Parsing a message from a WebSocket | Subscribe Centrifugo Channel (ClientID)*/
void UCDonationAlertsIntegrator::ParseMessage(FString Message)
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*Message);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject)) return; //Deserialize Json
	
	if (!JsonObject->HasTypedField<EJson::Object>(TEXT("result"))) return; 
	JsonObject = JsonObject->GetObjectField(TEXT("result"));
	
	if (JsonObject->HasTypedField<EJson::String>(TEXT("client"))) // CLIENT
	{
		ClientID = JsonObject->GetStringField(TEXT("client"));
		FString AlertsChannel = FString::Printf(TEXT("$alerts:donation_%d"), ProfileID);
		
		const FString Channels = FString::Printf(TEXT("\"$alerts:donation_%d\""), ProfileID);
		const FString PostContent = FString::Printf(TEXT("{\"channels\":[%s], \"client\":\"%s\"}"), *Channels, *ClientID);
	
		const FHttpRequestPtr HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetURL(TEXT("https://www.donationalerts.com/api/v1/centrifuge/subscribe"));
		HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		HttpRequest->SetContentAsString(PostContent);
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UCDonationAlertsIntegrator::SubscribeCentrifugoChannelRequestComplete);
		HttpRequest->ProcessRequest();
	}
	else if (JsonObject->HasTypedField<EJson::String>(TEXT("channel"))) // CHANNEL
	{
		if (const auto Data = JsonObject->GetObjectField(TEXT("data")))
		{
			if (!Data->HasTypedField<EJson::Object>(TEXT("data"))) return;
			auto SecondLayerDataJson = Data->GetObjectField(TEXT("data"));
			
			const FString ChannelName = JsonObject->GetStringField(TEXT("channel"));
			if (ChannelName.StartsWith(TEXT("$alerts")))
			{
				const FString DUsername = SecondLayerDataJson->GetStringField("Username");
				const FString DMessage = SecondLayerDataJson->GetStringField("Message");
				const int32 DAmount = SecondLayerDataJson->GetIntegerField("Amount");
				const FString DCurrency = SecondLayerDataJson->GetStringField("Currency");
				OnDonateReceived.Broadcast(DUsername, DMessage, DAmount, GetCurrencyType(DCurrency));
			}
		}
	}
}

/* STEP 6: Subscribe Centrifugo Channel */
void UCDonationAlertsIntegrator::SubscribeCentrifugoChannelRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	const FString ResponseContent = HttpResponse->GetContentAsString();
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*ResponseContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject)) return;

	if (!JsonObject->HasTypedField<EJson::Array>(TEXT("channels"))) return;
	auto ChannelsArray = JsonObject->GetArrayField(TEXT("channels"));
			
	for (const auto Channel : ChannelsArray)
	{
		auto ChannelObject = Channel->AsObject();
		if (!ChannelObject.IsValid()) continue;

		TSharedPtr<FJsonObject> RequestJsonObject = MakeShareable(new FJsonObject());
		RequestJsonObject->SetStringField(TEXT("channel"), ChannelObject->GetStringField("channel"));
		RequestJsonObject->SetStringField(TEXT("token"), ChannelObject->GetStringField("token"));

		FString ParamsContent;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamsContent);
		FJsonSerializer::Serialize(RequestJsonObject.ToSharedRef(), Writer);

		WebSocket->Send(FString::Printf(TEXT("{\"params\":%s,\"id\":%d,\"method\":1}"), *ParamsContent, MessageID));
		MessageID++;
	}
}

// =====================================================================================================================
void UCDonationAlertsIntegrator::Disconnect()
{
	if (HttpRouter.IsValid())
	{
		if (Callback.IsValid()) HttpRouter->UnbindRoute(Callback);
		if (URLReceived.IsValid()) HttpRouter->UnbindRoute(URLReceived);
	}
	if (WebSocket.IsValid()) WebSocket->Close();
}

void UCDonationAlertsIntegrator::BeginDestroy()
{
	Super::BeginDestroy();
	Disconnect();
}

ECurrencyType UCDonationAlertsIntegrator::GetCurrencyType(const FString CurrencyText)
{
	if(CurrencyText.IsEmpty()) return ECurrencyType::ECT_MAX;
	const TMap<FString, ECurrencyType> CurrencyMap{
		{"RUB", ECurrencyType::ECT_RUB},
		{"EUR", ECurrencyType::ECT_EUR},
		{"USD", ECurrencyType::ECT_USD},
		{"UAH", ECurrencyType::ECT_UAH},
		{"BYN", ECurrencyType::ECT_BYN},
		{"KZT", ECurrencyType::ECT_KZT},
		{"BRL", ECurrencyType::ECT_BRL},
		{"TRY", ECurrencyType::ECT_TRY},
		{"PLN", ECurrencyType::ECT_PLN}
	};
	
	if(!CurrencyMap.Contains(CurrencyText)) return ECurrencyType::ECT_MAX;
	
	return CurrencyMap[CurrencyText];
}
