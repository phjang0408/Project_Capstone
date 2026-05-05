#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Http.h"
#include "OnlineSessionSettings.h"
#include "OnlineSessionManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFindSessionsCompleteBP, const TArray<int32>&, MaxPlayersList);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAccessTokenUpdated, const FString&, NewAccessToken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomIdUpdated, const FString&, NewRoomId);

UCLASS()
class CAPSTONE_API UOnlineSessionManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UOnlineSessionManager();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Session 기능
	UFUNCTION(BlueprintCallable)
	void CreateSession(int32 NumPublicConnections = 4, bool bIsLAN = false);
	UFUNCTION(BlueprintCallable)
	void CreateRoomOnSpring(const FString& RoomId, int32 MaxPlayers);
	UFUNCTION(BlueprintCallable)
	void FindSession();
	UFUNCTION(BlueprintCallable)
	void JoinSession(int32 SessionIndex);
	UFUNCTION(BlueprintCallable)
	void LeaveServer(const FString& InRoomId);

	// 로그인 데이터 저장용 변수
	UPROPERTY() int32 Code;
	UPROPERTY() FString BackendMessage;
	UPROPERTY(BlueprintReadOnly) FString AccessToken;
	UPROPERTY() FString RefreshToken;
	UPROPERTY() int32 UserId;
	UPROPERTY() FString SteamId;
	UPROPERTY() FString Nickname;
	UPROPERTY() FString Role;

	UPROPERTY(BlueprintAssignable, Category = "Login")
	FOnAccessTokenUpdated OnAccessTokenUpdated;

	UPROPERTY(BlueprintAssignable)
	FOnRoomIdUpdated OnRoomIdUpdated;

	UPROPERTY(BlueprintReadOnly)
	FString CurrentRoomId;

	UPROPERTY(BlueprintAssignable)
	FOnFindSessionsCompleteBP OnFindSessionsCompleteBP;

	UFUNCTION(BlueprintCallable)
	void LoginToBackend();

	UFUNCTION(BlueprintCallable, Category = "Session")
	void DeleteRoomOnSpring(const FString& RoomId);
	void OnLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type PreviousStatus, ELoginStatus::Type NewStatus, const FUniqueNetId& LoggedInUserId);

	FDelegateHandle LoginStatusChangedHandle;
	void SetAccessToken(const FString& NewToken);

private:
	void CreateSessionInternal(int32 NumPublicConnections, bool bIsLAN);
	void JoinSessionInternal();
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionThenCreate(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionThenJoin(FName SessionName, bool bWasSuccessful);
	void OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnDeleteRoomResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

private:
	FString CurrentLeavingRoomId;
	IOnlineSessionPtr SessionInterface;
	IOnlineIdentityPtr IdentityInterface;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	TArray<FOnlineSessionSearchResult> SearchResults;

	FDelegateHandle OnCreateSessionCompleteHandle;
	FDelegateHandle OnFindSessionsCompleteHandle;
	FDelegateHandle OnJoinSessionCompleteHandle;
	FDelegateHandle OnDestroySessionCompleteHandle;

	int32 NumPublicConnectionsCached = 4;
	bool bIsLANCached = false;
	int32 PendingSessionIndex = 0;
};
