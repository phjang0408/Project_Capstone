#include "AMyGameStateBase.h"

#include "Http.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Net/UnrealNetwork.h"

void AMyGameStateBase::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("[GameState BeginPlay] Started"));

}
void AMyGameStateBase::InitRoomContext(const FString& InRoomId)
{
    UE_LOG(LogTemp, Warning, TEXT("[InitRoomContext] InRoomId = %s"), *InRoomId);
    RoomID = InRoomId;
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();
    
    Request->SetURL(TEXT("http://127.0.0.1:7777/api/room-context/init"));
    Request->SetVerb(TEXT("POST"));

    // 헤더 추가
    Request->SetHeader(TEXT("Room-Id"), RoomID);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    //Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));

    // 바디 없음
    Request->SetContentAsString(TEXT(""));

    Request->OnProcessRequestComplete().BindLambda(
        [](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
        {
            if (bSuccess && Resp.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("[InitRoomContext] Room created / TTL set"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[InitRoomContext] Failed to init room"));
            }
        }
    );

    Request->ProcessRequest();
}
//AccessToken을 GameState로 옮겨버리기
void AMyGameStateBase::SetAccessToken(const FString& NewToken)
{
    AccessToken = NewToken;

    UE_LOG(LogTemp, Warning, TEXT("[GameState] AccessToken updated: %s"), *AccessToken);
}
//---------------------------------------------
//  NoteItemData 추가 그전에 Type해야하나..?
//---------------------------------------------
void AMyGameStateBase::AddNoteItem(const FNoteItemData& NewItem)
{
    SharedInventory.Add(NewItem);
    SendNoteItemToSpring(NewItem);
}

//---------------------------------------------
//  DoorStateData 추가
//---------------------------------------------
void AMyGameStateBase::AddDoorState(const FDoorStateData& NewDoor)
{
    DoorStates.Add(NewDoor);
    SendDoorStateToSpring(NewDoor);
}
//---------------------------------------------
//  NoteItemData -> Spring Boot POST
//---------------------------------------------
void AMyGameStateBase::SendNoteItemToSpring(const FNoteItemData& Item)
{

    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

    JsonObject->SetStringField(TEXT("note_name"), Item.NoteName.ToString()); //FName
    JsonObject->SetBoolField(TEXT("he_spy"), Item.bIsHeSpy); //Boolean
    JsonObject->SetStringField(TEXT("item_id"), Item.ItemID); //FStirng


    //잠시만요

    FString Body;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    // HTTP POST 생성
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("http://127.0.0.1:7777/room-context/noteItem"));     //요기요기 스프링 부트 엔드포인트
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Room-Id"), RoomID);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    //Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));

    Request->SetContentAsString(Body);

    Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
        {
            if (bSuccess)
            {
                UE_LOG(LogTemp, Warning, TEXT("[SendNoteItem] Success: %s"), *Resp->GetContentAsString());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[SendNoteItem] Failed"));
            }
        });

    Request->ProcessRequest();
}


//---------------------------------------------
//  DoorState -> Spring Boot POST
//---------------------------------------------
void AMyGameStateBase::SendDoorStateToSpring(const FDoorStateData& Door)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

    // JSON 형식 정확히 맞추기
    JsonObject->SetStringField(TEXT("door_id"), Door.DoorID);   // FString
    JsonObject->SetBoolField(TEXT("locked"), Door.bIsLocked);   // bool

    FString Body;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    // HTTP POST
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("http://127.0.0.1:7777/room-context/doorState"));
    Request->SetVerb(TEXT("POST"));

    // 필수 헤더
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Room-Id"), RoomID); // 내꺼에서도 대문자 맞춰야 하지않을까..? D를 d로.. 문제되면 그 때
   // Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));

    Request->SetContentAsString(Body);

    Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
        {
            if (bSuccess)
            {
                UE_LOG(LogTemp, Warning, TEXT("[SendDoorState] Success"));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[SendDoorState] Failed"));
            }
        });

    Request->ProcessRequest();
}


//---------------------------------------------
//  Final Anser -> Spring Boot로 POST 전송
//---------------------------------------------
void AMyGameStateBase::SendFinalSubmitToSpring(const FFinalAnswerData& FinalData)
{
    UE_LOG(LogTemp, Warning, TEXT("[SendFinalSubmit] Start | isFinal=%d"), FinalData.isFinal ? 1 : 0);
    UE_LOG(LogTemp, Warning, TEXT("[SendFinalSubmit] Ans=%s"),
        *FinalData.AnswerText1);

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

    // sheet
    TSharedPtr<FJsonObject> Sheet = MakeShared<FJsonObject>();

    Sheet->SetBoolField(TEXT("isFinal"), FinalData.isFinal);

    // answers array
    TArray<TSharedPtr<FJsonValue>> Answers;
    if (!FinalData.AnswerText1.IsEmpty())
    {
        Answers.Add(MakeShared<FJsonValueString>(FinalData.AnswerText1));
    }
    Sheet->SetArrayField(TEXT("answers"), Answers);

    Root->SetObjectField(TEXT("sheet"), Sheet);

    // shared_inventory
    TArray<TSharedPtr<FJsonValue>> InvArray;
    for (const FNoteItemData& Item : SharedInventory)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("note_name"), Item.NoteName.ToString());
        O->SetBoolField(TEXT("he_spy"), Item.bIsHeSpy);
        O->SetStringField(TEXT("item_id"), Item.ItemID);
        InvArray.Add(MakeShared<FJsonValueObject>(O));
    }
    UE_LOG(LogTemp, Warning, TEXT("[SendFinalSubmit] SharedInventoryCount=%d"), InvArray.Num());
    Root->SetArrayField(TEXT("shared_inventory"), InvArray);

    // door_states
    TArray<TSharedPtr<FJsonValue>> DoorArray;
    for (const FDoorStateData& D : DoorStates)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("door_id"), D.DoorID);
        O->SetBoolField(TEXT("locked"), D.bIsLocked);
        DoorArray.Add(MakeShared<FJsonValueObject>(O));
    }
    UE_LOG(LogTemp, Warning, TEXT("[SendFinalSubmit] DoorStateCount=%d"), DoorArray.Num());
    Root->SetArrayField(TEXT("door_states"), DoorArray);

    // serialize
    FString Body;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    UE_LOG(LogTemp, Warning, TEXT("[SendFinalSubmit] JSON Length = %d"), Body.Len());

    // request
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("http://127.0.0.1:7777/final-submit"));  //@@@@@@@@@END포인트@@@@@@
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Room-Id"), RoomID);
    //Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Request->SetContentAsString(Body);
    UE_LOG(LogTemp, Warning, TEXT("[SendFinalSubmit] Sending request… RoomID=%s"), *RoomID);
    Request->OnProcessRequestComplete().BindUObject(this, &AMyGameStateBase::OnFinalSubmitResponse);
    Request->ProcessRequest();
}

//---------------------------------------------
//  Spring Boot -> UE에게로 
//---------------------------------------------
void AMyGameStateBase::OnFinalSubmitResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    // 1. 응답 유효성 검사
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[FinalSubmit] Request failed"));
        return;
    }

    FString RespBody = Response->GetContentAsString();
    if (RespBody.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[FinalSubmit] Empty response"));
        return;
    }

    // 2. JSON Deserialization
    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespBody);

    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[FinalSubmit] JSON parse error"));
        return;
    }

    // 3. [핵심 수정] 'result' 객체 내부로 진입하여 데이터 파싱
    const TSharedPtr<FJsonObject>* ResultObj;
    if (Json->TryGetObjectField(TEXT("result"), ResultObj))
    {
        // 'result' 안에서 score 가져오기
        FinalScore = (*ResultObj)->GetIntegerField(TEXT("score"));

        // Grade와 Feedback은 null일 수 있으므로 TryGetStringField로 안전하게 가져옴
        if (!(*ResultObj)->TryGetStringField(TEXT("grade"), FinalGrade))
        {
            FinalGrade = TEXT("");
        }

        if (!(*ResultObj)->TryGetStringField(TEXT("feedback"), FinalFeedback))
        {
            FinalFeedback = TEXT("");
        }

        UE_LOG(LogTemp, Warning, TEXT("[FinalSubmit] Success | Score: %d, Grade: %s"), FinalScore, *FinalGrade);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[FinalSubmit] 'result' field not found!"));
        FinalScore = 0;
    }

    // 4. 서버 RPC로 변수 업데이트 (Replication)
    ServerSetFinalResult(FinalScore, FinalGrade, FinalFeedback);

    // 5. UI 등을 위해 Broadcast
    OnFinalSubmitResult.Broadcast(FinalScore, FinalGrade, FinalFeedback);
}


void AMyGameStateBase::SendAIQuestion(const FString& Question)
{
    UE_LOG(LogTemp, Warning, TEXT("[AI] SendAIQuestion() Called. Q = %s"), *Question);
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("question"), Question);

    FString Body;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    const FString URL =
        FString::Printf(TEXT("http://127.0.0.1:7777/api/rooms/ai/ask")); //@@@@@@@@@END포인트@@@@@@

    UE_LOG(LogTemp, Warning, TEXT("[AI] POST URL = %s"), *URL);

    Request->SetURL(URL);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Room-Id"), RoomID);
    //Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));

    Request->SetContentAsString(Body);

    UE_LOG(LogTemp, Warning, TEXT("[AI] Request Body = %s"), *Body);

    Request->OnProcessRequestComplete().BindUObject(this, &AMyGameStateBase::OnAIResponse);
    Request->ProcessRequest();
}
void AMyGameStateBase::Multicast_AIAnswerUpdated_Implementation(const FString& NewAnswer)
{
    AIAnswer = NewAnswer;
    UE_LOG(LogTemp, Log, TEXT("[AIAnswer] = %s"), *AIAnswer);
    OnAIAnswerUpdated_BP(AIAnswer);
}
void AMyGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    UE_LOG(LogTemp, Warning, TEXT("[AI] Register Replicated Property: AIAnswer"));

    DOREPLIFETIME(AMyGameStateBase, AIAnswer); 
    DOREPLIFETIME(AMyGameStateBase, FinalScore);
    DOREPLIFETIME(AMyGameStateBase, FinalGrade);
    DOREPLIFETIME(AMyGameStateBase, FinalFeedback);
}
void AMyGameStateBase::OnAIResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    UE_LOG(LogTemp, Warning, TEXT("[AI] OnAIResponse() called. Success = %d"), bWasSuccessful);
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[AI] Response Failed OR Invalid Response Ptr"));

        AIAnswer = TEXT("CANT CONNECT SERVER;;");
        Multicast_AIAnswerUpdated(AIAnswer);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[AI] Raw Response = %s"), *Response->GetContentAsString());

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[AI] JSON Parsing Failed"));

        AIAnswer = TEXT("CANNOT PARSHING SORRY");
        Multicast_AIAnswerUpdated(AIAnswer);
        return;
    }
    if (!Root->HasTypedField<EJson::Object>(TEXT("result")))
    {
        UE_LOG(LogTemp, Error, TEXT("[AI] 'result' field missing in JSON"));

        AIAnswer = TEXT("INVALID RESPONSE");
        Multicast_AIAnswerUpdated(AIAnswer);
        return;
    }

    TSharedPtr<FJsonObject> Result = Root->GetObjectField(TEXT("result"));


    if (!Result->HasTypedField<EJson::String>(TEXT("answer")))
    {
        UE_LOG(LogTemp, Error, TEXT("[AI] 'answer' field missing in result object"));

        AIAnswer = TEXT("INVALID RESPONSE (NO ANSWER)");
        Multicast_AIAnswerUpdated(AIAnswer);
        return;
    }

    AIAnswer = Result->GetStringField(TEXT("answer"));
    UE_LOG(LogTemp, Warning, TEXT("[AI] Parsed Answer = %s"), *AIAnswer);
    Multicast_AIAnswerUpdated(AIAnswer);
}


void AMyGameStateBase::DeleteRoomOnSpring()
{
    UE_LOG(LogTemp, Warning, TEXT("[DeleteRoom] DeleteRoomOnSpring() called"));
    UE_LOG(LogTemp, Warning, TEXT("[DeleteRoom] Target RoomID = %s"), *RoomID);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();

    Req->SetURL(TEXT("http://127.0.0.1:7777/room-context/force"));  //@@@@@@@@@END포인트@@@@@@
    Req->SetVerb(TEXT("DELETE"));
    Req->SetHeader(TEXT("Room-Id"), RoomID);
    //Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
    Req->OnProcessRequestComplete().BindUObject(this, &AMyGameStateBase::OnDeleteRoomResponse);
    Req->ProcessRequest();
}

void AMyGameStateBase::OnDeleteRoomResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[DeleteRoom] Fail"));
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("[DeleteRoom] Success"));
}
void AMyGameStateBase::ServerSetFinalResult_Implementation(
    int32 Score, const FString& Grade, const FString& Feedback)
{
    FinalScore = Score;
    FinalGrade = Grade;
    FinalFeedback = Feedback;
}