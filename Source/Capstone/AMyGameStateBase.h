// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Http.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Net/UnrealNetwork.h"
#include "AMyGameStateBase.generated.h"


// 획득한 쪽지(단서) 아이템 정보 -> JSON: note_name, he_spy, item_id
USTRUCT(BlueprintType)
struct FNoteItemData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FName NoteName;

    // 스파이 여부 플래그 (JSON 키: he_spy)
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bIsHeSpy;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString ItemID;
};

// 문 잠금 상태 정보 -> JSON: door_id, locked
USTRUCT(BlueprintType)
struct FDoorStateData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bIsLocked;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString DoorID;
};

// 최종 답안 정보 -> JSON: sheet.answers[], sheet.isFinal
USTRUCT(BlueprintType)
struct FFinalAnswerData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString AnswerText1;

    // 최종 제출 여부
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool isFinal;
};
/**
 * 최종 제출 채점 결과(점수, 등급, 피드백)를 UI 등 BP로 알리는 델리게이트
 *
 * AMyGameStateBase
 * - 방(Room) 단위 공유 게임 상태: 쪽지 인벤토리, 문 상태, 최종 결과, AI 답변
 * - Spring Boot 백엔드와 HTTP(JSON) 통신 (현재 서버 비용 방지를 위해 요청부 주석 처리됨)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFinalSubmitResult, int32, Score, FString, Grade, FString, Feedback);



UCLASS()
class CAPSTONE_API AMyGameStateBase : public AGameStateBase
{
    GENERATED_BODY()

public:


    // Spring Boot 통신 함수 (BP에서 호출, 현재 HTTP 요청부 주석 처리됨)
    UFUNCTION(BlueprintCallable, Category = "RoomContext")
    void InitRoomContext(const FString& InRoomId);

    UFUNCTION(BlueprintCallable)
    void AddNoteItem(const FNoteItemData& NewItem);

    UFUNCTION(BlueprintCallable)
    void AddDoorState(const FDoorStateData& NewDoor);

    UFUNCTION(BlueprintCallable, Category = "RoomContext")
    void SendFinalSubmitToSpring(const FFinalAnswerData& FinalData);

    UFUNCTION(BlueprintCallable)
    void DeleteRoomOnSpring();

    UFUNCTION(BlueprintCallable)
    void SendAIQuestion(const FString& Question);



    // 네트워크 RPC / BP 이벤트
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_AIAnswerUpdated(const FString& NewAnswer);

    UFUNCTION(BlueprintImplementableEvent)
    void OnAIAnswerUpdated_BP(const FString& NewAnswer);

    UFUNCTION(Server, Reliable)
    void ServerSetFinalResult(int32 Score, const FString& Grade, const FString& Feedback);


    // 공유 게임 상태 (Replicated 표시된 변수만 클라이언트에 동기화)
    UPROPERTY(BlueprintReadOnly, Replicated)
    FString AIAnswer;

    // 방 식별자 (Steam SessionId), 모든 Spring 요청의 Room-Id 헤더로 사용
    UPROPERTY(BlueprintReadWrite)
    FString RoomID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FNoteItemData> SharedInventory;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FDoorStateData> DoorStates;

    UPROPERTY(BlueprintReadOnly, Replicated)
    int32 FinalScore;

    UPROPERTY(BlueprintReadOnly, Replicated)
    FString FinalGrade;

    UPROPERTY(BlueprintReadOnly, Replicated)
    FString FinalFeedback;


    UPROPERTY(BlueprintAssignable)
    FOnFinalSubmitResult OnFinalSubmitResult;

    
    UPROPERTY(BlueprintReadOnly, Category = "Login")
    FString AccessToken;

    
    UFUNCTION(BlueprintCallable, Category = "Login")
    void SetAccessToken(const FString& NewToken);


protected:
    virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

    virtual void BeginPlay() override;
 

private:

    // HTTP 요청/응답 처리 (OnInitRoomContextResponse, SendFinalAnswerToSpring, OnGetResultResponse는 선언만 있고 구현 없음)
    void OnInitRoomContextResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void SendNoteItemToSpring(const FNoteItemData& Item);
    void SendDoorStateToSpring(const FDoorStateData& Door);
    void SendFinalAnswerToSpring(const FFinalAnswerData& AnswerData);
    void OnFinalSubmitResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnDeleteRoomResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnGetResultResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    void OnAIResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};