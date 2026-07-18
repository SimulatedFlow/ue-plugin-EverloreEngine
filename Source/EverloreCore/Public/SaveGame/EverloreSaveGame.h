// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "EverloreSaveGame.generated.h"

/**
 * Drop-in SaveGame for one-click SaveQuestLog / LoadQuestLog. It stores the SAME portable
 * blob ExportSaveData() produces, in a SaveGame-tagged field — because the standard
 * USaveGame serializer only persists UPROPERTY(SaveGame) members, a plain nested struct
 * would be silently dropped. Studios who own their save format use ExportSaveData() /
 * ImportSaveData() directly and nest the blob themselves.
 */
UCLASS()
class EVERLORECORE_API UEverloreSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FString Blob;
};
