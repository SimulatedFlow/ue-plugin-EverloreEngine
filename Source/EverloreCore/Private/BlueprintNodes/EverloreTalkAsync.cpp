// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "BlueprintNodes/EverloreTalkAsync.h"
#include "Components/EverloreConversationComponent.h"

UEverloreTalkAsync* UEverloreTalkAsync::TalkToNpc(UObject* WorldContextObject, UEverloreConversationComponent* Conversation, const FString& Message)
{
	UEverloreTalkAsync* Node = NewObject<UEverloreTalkAsync>();
	Node->WorldContextObject = WorldContextObject;
	Node->Conversation = Conversation;
	Node->Message = Message;
	Node->RegisterWithGameInstance(WorldContextObject);
	return Node;
}

void UEverloreTalkAsync::Activate()
{
	if (!Conversation)
	{
		OnError.Broadcast(TEXT("No conversation component provided to Talk To NPC."));
		SetReadyToDestroy();
		return;
	}

	FEverloreChatExchangeCallbacks Cb;
	Cb.OnDelta = FEverloreChatOnDelta::CreateWeakLambda(this, [this](const FString& Delta, bool bDone)
	{
		OnDelta.Broadcast(Delta, bDone);
	});
	Cb.OnIntent = FEverloreChatOnIntent::CreateWeakLambda(this, [this](const FEverloreChatIntent& Intent)
	{
		OnIntent.Broadcast(Intent);
	});
	Cb.OnReply = FEverloreChatOnReply::CreateWeakLambda(this, [this](const FString& Reply)
	{
		OnReply.Broadcast(Reply);
		SetReadyToDestroy(); // reply is the terminal signal of the exchange
	});
	Cb.OnError = FEverloreChatOnError::CreateWeakLambda(this, [this](const FString& Error)
	{
		OnError.Broadcast(Error);
		SetReadyToDestroy();
	});

	Conversation->Ask(Message, Cb);
}
