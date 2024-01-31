#pragma once

#include "CoreMinimal.h"

class FFileSystem;
class FFile;
class FBTree;
class FStaticText
{
public:
	FStaticText(FFileSystem* System);

	FString Get(uint32 Index);
	
	uint32 FindOrCreate(const FString& String);
	uint32 Find(const FString& String);

	void Init();
	void Open();
private:
	void FlushHeader();
private:

	FFileSystem* FileSystem;
	TSharedPtr<FFile> File;
	TSharedPtr<FBTree> BTree;

	struct
	{
		int MagicNum;
		int Count;
	}Header;
};