// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Http.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Net/UnrealNetwork.h"
#include "AMyGameStateBase.generated.h"


USTRUCT(BlueprintType)
struct FNoteItemData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FName NoteName;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bIsHeSpy;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString ItemID;
};

USTRUCT(BlueprintType)
struct FDoorStateData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bIsLocked;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString DoorID;
};

USTRUCT(BlueprintType)
struct FFinalAnswerData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString AnswerText1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool isFinal;
};
/**
 *
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFinalSubmitResult, int32, Score, FString, Grade, FString, Feedback);



UCLASS()
class CAPSTONE_API AMyGameStateBase : public AGameStateBase
{
    GENERATED_BODY()

public:


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



    UFUNCTION(NetMulticast, Reliable)
    void Multicast_AIAnswerUpdated(const FString& NewAnswer);

    UFUNCTION(BlueprintImplementableEvent)
    void OnAIAnswerUpdated_BP(const FString& NewAnswer);

    UFUNCTION(Server, Reliable)
    void ServerSetFinalResult(int32 Score, const FString& Grade, const FString& Feedback);


    UPROPERTY(BlueprintReadOnly, Replicated)
    FString AIAnswer;

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

    void OnInitRoomContextResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void SendNoteItemToSpring(const FNoteItemData& Item);
    void SendDoorStateToSpring(const FDoorStateData& Door);
    void SendFinalAnswerToSpring(const FFinalAnswerData& AnswerData);
    void OnFinalSubmitResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnDeleteRoomResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
    void OnGetResultResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    void OnAIResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};