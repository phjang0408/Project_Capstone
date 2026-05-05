#include "OnlineSessionManager.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "HttpModule.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"


UOnlineSessionManager::UOnlineSessionManager()
{
	UE_LOG(LogOnlineSession, Warning, TEXT("[SessionManager] Constructor called"));
}

/**
 * 서브시스템 초기화 함수
 */
void UOnlineSessionManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogOnlineSession, Warning, TEXT("[SessionManager] Initialize() called"));

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (!OnlineSubsystem)
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[SessionManager] No OnlineSubsystem found"));
		return;
	}
	UE_LOG(LogOnlineSession, Warning, TEXT("[SessionManager] OnlineSubsystem = %s"),
		*OnlineSubsystem->GetSubsystemName().ToString());

	if (OnlineSubsystem)
	{
		SessionInterface = OnlineSubsystem->GetSessionInterface();
		IdentityInterface = OnlineSubsystem->GetIdentityInterface();
		UE_LOG(LogOnlineSession, Warning, TEXT("[SessionManager] SessionInterface valid = %d"), SessionInterface.IsValid());
		UE_LOG(LogOnlineSession, Warning, TEXT("[SessionManager] IdentityInterface valid = %d"), IdentityInterface.IsValid());


		if (IdentityInterface.IsValid())
		{
			// Delegate 등록
			LoginStatusChangedHandle =
				IdentityInterface->AddOnLoginStatusChangedDelegate_Handle(
					0,
					FOnLoginStatusChangedDelegate::CreateUObject(this, &UOnlineSessionManager::OnLoginStatusChanged)
				);

			auto Status = IdentityInterface->GetLoginStatus(0);

			if (Status == ELoginStatus::LoggedIn)
			{
				UE_LOG(LogOnlineSession, Log, TEXT("Already LoggedIn. Calling LoginToBackend() immediately."));
				LoginToBackend();
			}
		}
	}

}
// 재로그인 및 재요청 안전용
void UOnlineSessionManager::OnLoginStatusChanged(int32 LocalUserNum,
	ELoginStatus::Type PreviousStatus,
	ELoginStatus::Type NewStatus,
	const FUniqueNetId& LoggedInUserId)
{
	UE_LOG(LogOnlineSession, Warning,
		TEXT("[SessionManager] LoginStatus Changed (%d): %d -> %d"),
		LocalUserNum,
		(int32)PreviousStatus,
		(int32)NewStatus
	);

	if (!LoggedInUserId.IsValid())
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[SessionManager] Invalid LoggedInUserId on status change"));
	}

	if (NewStatus == ELoginStatus::LoggedIn)
	{
		UE_LOG(LogOnlineSession, Log, TEXT("Steam LoggedIn! Now calling LoginToBackend()"));
		LoginToBackend();
	}
	else
	{
		UE_LOG(LogOnlineSession, Warning,
			TEXT("[SessionManager] New login status is not LoggedIn. No backend login."));
	}
}
void UOnlineSessionManager::Deinitialize()
{
	UE_LOG(LogOnlineSession, Log, TEXT("[Deinitialize] 세션 인터페이스 해제"));
	if (IdentityInterface.IsValid())
	{
		IdentityInterface->ClearOnLoginStatusChangedDelegate_Handle(0, LoginStatusChangedHandle);
	}
	SessionInterface = nullptr;
	IdentityInterface = nullptr;
	
	Super::Deinitialize();
}



#pragma region CreateSession
//기존 세션 검사와 없다면 Internal생성
void UOnlineSessionManager::CreateSession(int32 NumPublicConnections, bool bIsLAN)
{
	// 인터페이스 유효성 검사
	if (!SessionInterface.IsValid())
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[CreateSession] SessionInterface is invalid!"));
		return;
	}
	UE_LOG(LogOnlineSession, Log, TEXT("[CreateSession] Request received: Connections=%d, LAN=%d"), NumPublicConnections, bIsLAN);
	// 만약 기존 세션을 파괴하고 다시 만들어야 할 경우를 대비해 설정값 저장
	NumPublicConnectionsCached = NumPublicConnections;
	bIsLANCached = bIsLAN;

	// 'GameSession'이라는 이름의 세션이 이미 존재하는지 확인
	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("[CreateSession] 이미 세션이 존재합니다. 파괴 후 재생성합니다."));

		// 기존에 연결된 델리게이트가 있다면 정리
		if (OnDestroySessionCompleteHandle.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteHandle);
		}

		// 파괴가 완료되면 'OnDestroySessionThenCreate' 함수를 호출하도록 등록
		OnDestroySessionCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UOnlineSessionManager::OnDestroySessionThenCreate));

		// 세션 파괴 실행
		SessionInterface->DestroySession(NAME_GameSession);
	}
	else
	{
		// 세션이 없으면 바로 생성 로직 진입
		CreateSessionInternal(NumPublicConnections, bIsLAN);
	}
}

void UOnlineSessionManager::CreateRoomOnSpring(const FString& RoomId, int32 MaxPlayers)
{
	//JSON 파싕
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField("room_id", RoomId);
	JsonObject->SetNumberField("host_user_id", UserId);
	JsonObject->SetNumberField("max_players", MaxPlayers);

	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	auto Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("http://127.0.0.1:7777/api/room-context")); //@@@@@@@@@END포인트@@@@@@
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Room-Id"), RoomId);
	//Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetContentAsString(Body);
	Request->ProcessRequest();
}
//실제 세션 생성 요청
void UOnlineSessionManager::CreateSessionInternal(int32 NumPublicConnections, bool bIsLAN)
{
	UE_LOG(LogOnlineSession, Log, TEXT("[CreateSession] 세션 생성 시도. 인원: %d, LAN: %d"), NumPublicConnections, bIsLAN);

	// 세션 설정 (FOnlineSessionSettings)
	FOnlineSessionSettings SessionSettings;
	SessionSettings.bIsLANMatch = false;                        // LAN 여부
	SessionSettings.NumPublicConnections = NumPublicConnections; // 최대 접속 인원
	SessionSettings.bAllowJoinInProgress = true;                 // 게임 도중 난입 허용
	SessionSettings.bShouldAdvertise = true;                     // 다른 플레이어가 이 방을 검색할 수 있게 함 (필수)
	SessionSettings.bUsesPresence = true;                        // 스팀 오버레이/친구 초대 기능 사용 (스팀 필수)
	SessionSettings.bUseLobbiesIfAvailable = true;               // 스팀 로비 API 사용 (이걸 켜야 스팀 로비로 잡힘)


	SessionSettings.Set(SEARCH_PRESENCE, true, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings.Set(SEARCH_LOBBIES, true, EOnlineDataAdvertisementType::ViaOnlineService);


	// 델리게이트 핸들 초기화 및 등록
	if (OnCreateSessionCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteHandle);
	}
	// 생성이 완료되면 OnCreateSessionComplete 호출
	OnCreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UOnlineSessionManager::OnCreateSessionComplete));
	UE_LOG(LogOnlineSession, Warning,
		TEXT("Advertise=%d Presence=%d"),
		SessionSettings.bShouldAdvertise,
		SessionSettings.bUsesPresence
	);
	// 세션 생성 요청 보내기
	if (!SessionInterface->CreateSession(0, NAME_GameSession, SessionSettings))
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[CreateSession] CreateSession API Not Found"));
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteHandle);
	}
}

void UOnlineSessionManager::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 델리게이트 해제
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteHandle);
	}

	if (bWasSuccessful)
	{
		FNamedOnlineSession* NamedSession = SessionInterface->GetNamedSession(NAME_GameSession);
		if (NamedSession && NamedSession->SessionInfo.IsValid())
		{
			// 여기서 CurrentRoomId를 SessionId로 설정, CurrentRoomId를 또 GameState로 넘겨야댄다 기억해!
			CurrentRoomId = NamedSession->SessionInfo->GetSessionId().ToString();
			// 델리게이트로 추가해버리기~
			OnRoomIdUpdated.Broadcast(CurrentRoomId);

			
			UE_LOG(LogOnlineSession, Log, TEXT("[OnCreateSessionComplete] RoomId(SessionId): %s"), *CurrentRoomId);
		}
		
		CreateRoomOnSpring(CurrentRoomId, NumPublicConnectionsCached);

		UE_LOG(LogOnlineSession, Log, TEXT("[OnCreateSessionComplete] SessionName: %s"), *SessionName.ToString());

		// ServerTravel
		UWorld* World = GetWorld();
		if (World)
		{
			// ?listen 옵션
			FString LobbyPath = TEXT("/Game/FPMovement/Demo/Maps/Lobby?listen");
			World->ServerTravel(LobbyPath);
		}
	}
	else
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[OnCreateSessionComplete] 세션 생성 실패."));
	}
	
}

void UOnlineSessionManager::OnDestroySessionThenCreate(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteHandle);
	}

	// 파괴가 끝났으므로 아까 저장해둔 설정값으로 다시 생성 시도
	CreateSessionInternal(NumPublicConnectionsCached, bIsLANCached);
}
#pragma endregion

#pragma region FindSession

void UOnlineSessionManager::FindSession()
{
	if (!SessionInterface.IsValid()) return;

	UE_LOG(LogOnlineSession, Log, TEXT("[FindSession] 세션 검색 시작..."));

	// 검색 객체 생성 (Shared Pointer 사용)
	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 10; // 최대 10개 검색
	SessionSearch->bIsLanQuery = false;   // 스팀OSS기본이 false면 LAN스팀은 어떻게 만든걸까

	//Presence(상태 정보)와 Lobby(로비) 검색 활성화
	SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	// 델리게이트 등록
	if (OnFindSessionsCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteHandle);
	}
	OnFindSessionsCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UOnlineSessionManager::OnFindSessionsComplete));

	// 검색 실행
	SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
}

void UOnlineSessionManager::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteHandle);
	}

	TArray<int32> OutMaxPlayers; // 블루프린트 UI 표시용

	if (bWasSuccessful && SessionSearch.IsValid())
	{
		UE_LOG(LogOnlineSession, Log, TEXT("[OnFindSessionsComplete] 검색 완료. 찾은 방 개수: %d"), SessionSearch->SearchResults.Num());

		// 검색 결과= 멤버 변수 SearchResults에 저장 -> 나중에 Join할 때 쓰기
		SearchResults = SessionSearch->SearchResults;

		// 검색된 각 방의 정보를 순회
		for (const auto& Result : SearchResults)
		{
			if (Result.IsValid())
			{
				// UI에 보여줄 정보 추출 (여기서는 최대 인원수만 뽑아도 핑이랑 name은 거기 있었으니까)
				int32 MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
				OutMaxPlayers.Add(MaxPlayers);
			}
		}
	}
	else
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("[OnFindSessionsComplete] 검색 실패 또는 결과 없음."));
	}

	// Broadcast
	OnFindSessionsCompleteBP.Broadcast(OutMaxPlayers);
}

#pragma endregion

#pragma region JoinSession

void UOnlineSessionManager::JoinSession(int32 SessionIndex)
{
	// 유효검사
	if (!SessionInterface.IsValid() || !SessionSearch.IsValid()) return;

	// 배열 인덱스 범위 체크(크러쉬 방지)
	if (!SearchResults.IsValidIndex(SessionIndex))
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[JoinSession] 잘못된 인덱스입니다: %d"), SessionIndex);
		return;
	}

	PendingSessionIndex = SessionIndex;

	// 혹시 이미 다른 방에 들어가 있다면? -> 먼저 나오고(Destroy) 들어가기(Join)? 안 되면 순서 반대로
	FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (ExistingSession)
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("[JoinSession] 기존 세션 발견. 파괴 후 참가합니다."));

		if (OnDestroySessionCompleteHandle.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteHandle);
		}
		OnDestroySessionCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UOnlineSessionManager::OnDestroySessionThenJoin));

		SessionInterface->DestroySession(NAME_GameSession);
	}
	else
	{
		JoinSessionInternal();
	}
}

void UOnlineSessionManager::JoinSessionInternal()
{
	if (!SessionInterface.IsValid()) return;

	// 델리게이트 등록
	if (OnJoinSessionCompleteHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteHandle);
	}
	OnJoinSessionCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UOnlineSessionManager::OnJoinSessionComplete));

	UE_LOG(LogOnlineSession, Log, TEXT("[JoinSession] %d번 세션에 참가 요청 중..."), PendingSessionIndex);

	// 실제 Join 요청 (로컬 유저(controller=0)가, GameSession 이름으로, 해당 검색 결과에 접속)
	SessionInterface->JoinSession(0, NAME_GameSession, SearchResults[PendingSessionIndex]);
}

void UOnlineSessionManager::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	//유효검사
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteHandle);
	}

	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		UE_LOG(LogOnlineSession, Log, TEXT("[OnJoinSessionComplete] 참가 성공! 서버 주소를 받아옵니다."));

		// 등록 성공. 그러면 이동을 위해 ClientTravel
		FString ConnectString;
		if (SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
		{
			UE_LOG(LogOnlineSession, Log, TEXT("[ClientTravel] 주소: %s"), *ConnectString);

			APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
			if (PC)
			{
				// 절대 경로(Absolute)로 여행 떠남 -> 로딩 화면 후 해당 서버 맵으로 진입
				PC->ClientTravel(ConnectString, ETravelType::TRAVEL_Absolute);
			}
		}
	}
	else
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[OnJoinSessionComplete] 참가 실패 코드: %d"), (int32)Result);
	}
}

void UOnlineSessionManager::OnDestroySessionThenJoin(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteHandle);
	}

	// 기존 세션 파괴가 끝났으니 다시 참가를 시도
	JoinSessionInternal();
}

#pragma endregion

#pragma region Leave server

void UOnlineSessionManager::LeaveServer(const FString& InRoomId)
{
	CurrentLeavingRoomId = CurrentRoomId;

	if (!SessionInterface.IsValid()) return;

	APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController();
	if (!PC) return;

	// 내가 호스트인지 클라이언트인지 확인
	if (PC->HasAuthority())
	{
		// [호스트인 경우] 세션 파괴 이후에 DestroySessionComplete로, 그래야 State보다 먼저 끊고, 다음 State 종료 인정?
		UE_LOG(LogOnlineSession, Log, TEXT("[LeaveServer] Host: destroying session"));

		if (OnDestroySessionCompleteHandle.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteHandle);
		}
		OnDestroySessionCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &UOnlineSessionManager::OnDestroySessionComplete));

		// 세션을 파괴하면(서버가 닫히면) 클라이언트들도 끊어버리기
		SessionInterface->DestroySession(NAME_GameSession);
	}
	else
	{
		// 클라이언트인 경우
		UE_LOG(LogOnlineSession, Log, TEXT("[LeaveServer] Client: travel to MainMenu"));

		// 먼저 메인 메뉴로 이동 (연결 끊기)
		PC->ClientTravel(TEXT("/Game/FPMovement/Demo/Maps/MainMenu"), ETravelType::TRAVEL_Absolute);

		//로컬에 남아있는 세션 정보 정리 엔진(listen)서버로
		SessionInterface->DestroySession(NAME_GameSession);
	}
}

void UOnlineSessionManager::DeleteRoomOnSpring(const FString& RoomId)
{
	if (RoomId.IsEmpty())
	{
		UE_LOG(LogOnlineSession, Warning, TEXT("[DeleteRoomOnSpring] Empty RoomId"));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("http://127.0.0.1:7777/api/room-context/force")); //@@@@@@@@@END포인트@@@@@@
	Request->SetVerb(TEXT("DELETE")); // 서버가 DELETE를 지원하면 사용, (안하면 POST로 변경으로 다시 시도해라 장지욱)
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Room-Id"), RoomId);
	//Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	Request->SetContentAsString(TEXT("")); // 바디 없음

	Request->OnProcessRequestComplete().BindUObject(this, &UOnlineSessionManager::OnDeleteRoomResponse);
	Request->ProcessRequest();
}

void UOnlineSessionManager::OnDeleteRoomResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		UE_LOG(LogOnlineSession, Log, TEXT("[DeleteRoomOnSpring] Success RoomId header sent"));
	}
	else
	{
		const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : -1;
		UE_LOG(LogOnlineSession, Warning, TEXT("[DeleteRoomOnSpring] Failed (code=%d)"), ResponseCode);
	}
}

void UOnlineSessionManager::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteHandle);
	}

	// 호스트가 세션을 성공적으로 닫았으면 메인 메뉴로 이동
	if (bWasSuccessful)
	{
		//Spring에 방 삭제 요청 보내고
		DeleteRoomOnSpring(CurrentLeavingRoomId);

		//그 다음 메인으로 가자
		UWorld* World = GetWorld();
		if (World)
		{
			UGameplayStatics::OpenLevel(World, TEXT("MainMenu"));
		}
	}
}

void UOnlineSessionManager::LoginToBackend()
{
	// 1. Identity Interface 유효성 검사
	if (!IdentityInterface.IsValid())
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[LoginToBackend] IDENTITYINTERFACE IS INVALID!"));
		return;
	}

	// 2. 현재 0번(로컬) 유저의 고유 ID 가져오기
	FUniqueNetIdPtr LocalUserId = IdentityInterface->GetUniquePlayerId(0);
	if (!LocalUserId.IsValid())
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[LoginToBackend] IS NOT VALID ID"));
		return;
	}

	// 3. Steam Auth Token (Ticket) 가져오기
	FString AuthTicket = IdentityInterface->GetAuthToken(0);

	if (AuthTicket.IsEmpty())
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[LoginToBackend] CAN'T GET AUTHTICKET"));
		return;
	}

	UE_LOG(LogOnlineSession, Log, TEXT("[LoginToBackend] SUCCESS STEAM TICKET GET! TRYING TO SENDING SPRING"));

	// 4. JSON 객체 생성
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField("auth_ticket", AuthTicket);

	// 5. JSON을 String으로 직렬화
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	UE_LOG(LogTemp, Warning, TEXT("[LoginToBackend] JSON Payload Ready: %s"), *JsonString);

	// 7. HTTP 요청 생성(POST)
	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();

	Request->OnProcessRequestComplete().BindUObject(this, &UOnlineSessionManager::OnLoginResponseReceived);
	Request->SetURL("http://127.0.0.1:7777/api/auth/steam-client-login"); //@@@@@@@@@END포인트@@@@@@
	Request->SetVerb("POST");
	Request->SetHeader("Content-Type", "application/json");
	// 8. 바이트 배열 전달
	Request->SetContentAsString(JsonString);
	UE_LOG(LogTemp, Warning, TEXT("[LoginToBackend] Sending HTTP Request to backend..."));
	// 9. 요청 전송
	Request->ProcessRequest();
}

void UOnlineSessionManager::OnLoginResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	// 확인
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[Backend Login] REQUEST FAILED. CANT CONNECT TO SERVER"));
		return;
	}

	const FString ResponseBody = Response->GetContentAsString();
	UE_LOG(LogTemp, Warning, TEXT("[Backend Login] Response Received (Length=%d)"), ResponseBody.Len());
	UE_LOG(LogTemp, Warning, TEXT("[Backend Login] Raw Body: %s"), *ResponseBody);

	// JSON 파싱
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[Backend Login] JSON PARSE ERROR"));
		return;
	}

	// 필드 추출
	this->Code = JsonObject->GetIntegerField(TEXT("code"));
	this->BackendMessage = JsonObject->GetStringField(TEXT("message"));

	UE_LOG(LogOnlineSession, Log, TEXT("[Backend Login] Code: %d, Message: %s"), this->Code, *this->BackendMessage);

	// result 내부 파싱 
	if (!JsonObject->HasTypedField<EJson::Object>(TEXT("result")))
	{
		UE_LOG(LogOnlineSession, Error, TEXT("[Backend Login] NO RESULT FIELD!"));
		return;
	}

	const TSharedPtr<FJsonObject> ResultObj = JsonObject->GetObjectField(TEXT("result"));
	UE_LOG(LogTemp, Warning, TEXT("[Backend Login] Parsing result fields..."));
	// GameInstance 저장
	// GameInstanceSubsystem을 사용하므로 GameInstance로 갈 필요 없다.
	// 그냥 Subsystem 내부 변수에 직접 저장으로 변경
	// 애초에 그냥 Instance로 만들걸 후회하고있어용~~ 안 되면 INstance로

	SetAccessToken(ResultObj->GetStringField(TEXT("access_token")));

	this->RefreshToken = ResultObj->GetStringField(TEXT("refresh_token"));
	this->UserId = ResultObj->GetIntegerField(TEXT("user_id"));
	this->SteamId = ResultObj->GetStringField(TEXT("steam_id"));
	this->Nickname = ResultObj->GetStringField(TEXT("nickname"));
	this->Role = ResultObj->GetStringField(TEXT("role"));
	/*
	UE_LOG(LogTemp, Warning, TEXT("[Backend Login] RefreshToken length: %d"), RefreshToken.Len());
	UE_LOG(LogTemp, Warning, TEXT("[Backend Login] UserID: %d"), UserId);
	UE_LOG(LogTemp, Warning, TEXT("[Backend Login] SteamID: %s"), *SteamId);
	UE_LOG(LogTemp, Warning, TEXT("[Backend Login] Nickname: %s"), *Nickname);
	UE_LOG(LogTemp, Warning, TEXT("[Backend Login] Role: %s"), *Role);
	*/
	UE_LOG(LogTemp, Warning, TEXT("[Backend Login] Backend login COMPLETED successfully"));
}

void UOnlineSessionManager::SetAccessToken(const FString& NewToken)
{
	AccessToken = NewToken;

	// 블루프린트로 신호 보내기
	OnAccessTokenUpdated.Broadcast(AccessToken);
	UE_LOG(LogTemp, Warning, TEXT("[Backend Login] AccessToken length: %d"), AccessToken.Len());

}