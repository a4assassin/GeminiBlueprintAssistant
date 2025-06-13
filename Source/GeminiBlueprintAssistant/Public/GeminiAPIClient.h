#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h" // For FHttpRequestRef
#include "Interfaces/IHttpResponse.h" // For FHttpResponsePtr

// Declare a delegate for when the Gemini request is complete
// FString response content, bool success, FString error message
DECLARE_DELEGATE_ThreeParams(FGeminiResponseDelegate, FString /* ResponseContent */, bool /* bSuccess */, FString /* ErrorMessage */);

/**
 * C++ class to handle communication with the Google Gemini API.
 */
class GEMINIBLUEPRINTASSISTANT_API FGeminiAPIClient : public TSharedFromThis<FGeminiAPIClient>
{
public:
	// Constructor
	FGeminiAPIClient();

	// Function to send a text prompt to Gemini
	void GenerateContent(const FString& InPrompt, const FString& APIKey);

	// Delegate to be called when the Gemini response is received
	FGeminiResponseDelegate OnGeminiResponseReceived;

private:
	// Callback for when the HTTP request completes
	void OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	// The current API key (for internal use during a request)
	FString CurrentAPIKey;
};