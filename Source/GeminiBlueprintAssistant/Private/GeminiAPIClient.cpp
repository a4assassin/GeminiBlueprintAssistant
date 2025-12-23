// Private/GeminiAPIClient.cpp
#include "GeminiAPIClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"

#define LOCTEXT_NAMESPACE "FGeminiAPIClient"

FGeminiAPIClient::FGeminiAPIClient()
{
	
}

void FGeminiAPIClient::GenerateContent(const FString& InPrompt, const FString& APIKey)
{
	if (InPrompt.IsEmpty() || APIKey.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("GeminiAPIClient: Prompt or API Key is empty. Skipping request."));
		OnGeminiResponseReceived.ExecuteIfBound(TEXT(""), false, TEXT("Prompt or API Key was empty."));
		return;
	}

	CurrentAPIKey = APIKey;

	FString Url = TEXT("https://generativelanguage.googleapis.com/v1beta/models/gemini-3-flash-preview:generateContent?key=") + APIKey;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindRaw(this, &FGeminiAPIClient::OnRequestComplete);
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// Construct the JSON request body for Gemini Pro
	TSharedPtr<FJsonObject> RequestBody = MakeShareable(new FJsonObject());
	TArray<TSharedPtr<FJsonValue>> ContentsArray;
	TSharedPtr<FJsonObject> ContentObject = MakeShareable(new FJsonObject());
	TArray<TSharedPtr<FJsonValue>> PartsArray;

	// Create a new FJsonObject to hold the "text" field for the part
	TSharedPtr<FJsonObject> TextPartObject = MakeShareable(new FJsonObject());
	TextPartObject->SetStringField(TEXT("text"), InPrompt); // Set the "text" field with the actual prompt

	// Add this TextPartObject (wrapped as an FJsonValueObject) to the PartsArray
	PartsArray.Add(MakeShareable(new FJsonValueObject(TextPartObject)));

	ContentObject->SetArrayField(TEXT("parts"), PartsArray);
	ContentsArray.Add(MakeShareable(new FJsonValueObject(ContentObject)));
	RequestBody->SetArrayField(TEXT("contents"), ContentsArray);

	// Optional: Add generation config (e.g., temperature, max output tokens)
	// TSharedPtr<FJsonObject> GenerationConfig = MakeShareable(new FJsonObject());
	// GenerationConfig->SetNumberField(TEXT("temperature"), 0.7);
	// RequestBody->SetObjectField(TEXT("generationConfig"), GenerationConfig);

	FString RequestBodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

	Request->SetContentAsString(RequestBodyString);
	Request->ProcessRequest();

	UE_LOG(LogTemp, Log, TEXT("GeminiAPIClient: Sending request to Gemini API..."));
}

void FGeminiAPIClient::OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	FString ResponseContent = TEXT("");
	bool bSuccess = false;
	FString ErrorMessage = TEXT("Unknown error.");

	if (bConnectedSuccessfully)
	{
		if (Response.IsValid() && Response->GetResponseCode() >= 200 && Response->GetResponseCode() <= 299)
		{
			ResponseContent = Response->GetContentAsString();
			UE_LOG(LogTemp, Log, TEXT("GeminiAPIClient: Raw Response: %s"), *ResponseContent);

			// Attempt to parse the JSON response
			TSharedPtr<FJsonObject> JsonResponse;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

			if (FJsonSerializer::Deserialize(Reader, JsonResponse))
			{
				// Check for 'candidates' array and extract the text
				const TArray<TSharedPtr<FJsonValue>>* CandidatesArray;
				if (JsonResponse->TryGetArrayField(TEXT("candidates"), CandidatesArray))
				{
					if (CandidatesArray->Num() > 0)
					{
						const TSharedPtr<FJsonObject>* CandidateObjectValue;
						if ((*CandidatesArray)[0]->TryGetObject(CandidateObjectValue))
						{
							const TSharedPtr<FJsonObject>* ContentObjectValue;
							if ((*CandidateObjectValue)->TryGetObjectField(TEXT("content"), ContentObjectValue))
							{
								const TArray<TSharedPtr<FJsonValue>>* PartsArray;
								if ((*ContentObjectValue)->TryGetArrayField(TEXT("parts"), PartsArray))
								{
									if (PartsArray->Num() > 0)
									{
										const TSharedPtr<FJsonObject>* PartObjectValue;
										if ((*PartsArray)[0]->TryGetObject(PartObjectValue))
										{
											FString Text;
											if ((*PartObjectValue)->TryGetStringField(TEXT("text"), Text))
											{
												ResponseContent = Text;
												bSuccess = true;
												ErrorMessage = TEXT("");
											}
										}
									}
								}
							}
						}
					}
				}
				else if (JsonResponse->HasField(TEXT("error")))
				{
					const TSharedPtr<FJsonObject>* ErrorObject;
					if (JsonResponse->TryGetObjectField(TEXT("error"), ErrorObject))
					{
						FString ErrorMessageFromAPI;
						if ((*ErrorObject)->TryGetStringField(TEXT("message"), ErrorMessageFromAPI))
						{
							ErrorMessage = FString::Printf(TEXT("Gemini API Error: %s"), *ErrorMessageFromAPI);
						}
					}
				}
			}
			else
			{
				ErrorMessage = FString::Printf(TEXT("Failed to parse JSON response: %s"), *ResponseContent);
			}
		}
		else
		{
			ErrorMessage = FString::Printf(TEXT("HTTP Request Failed: Response code %d - %s"), Response->GetResponseCode(), *Response->GetContentAsString());
		}
	}
	else
	{
		ErrorMessage = TEXT("HTTP Request Failed: No connection or invalid response.");
	}

	OnGeminiResponseReceived.ExecuteIfBound(ResponseContent, bSuccess, ErrorMessage);
}

#undef LOCTEXT_NAMESPACE

