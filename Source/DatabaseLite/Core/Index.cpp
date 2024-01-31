#include "Index.h"
#include "Range.h"




bool FIndexHelper::Read(FKeySequence& Keys, FFile::Ptr File, const FKeyTypeSequence& Types)
{
	
	for (auto Type : Types)
	{
		FAny Value;
		switch (Type)
		{
			case EKeyType::Integer:
			{
				int64 Data;
				if (File->Read(Data))
					Value = Data;
				else
					return false;
			}
			break;
			case EKeyType::String:
			{
				FString Data;
				if (File->ReadStaticString(Data))
					Value = Data;
				else
					return false;
			}
			break;
		}
		Keys.Add(MoveTemp(Value));
	}

	return true;
}

bool FIndexHelper::Write(const FKeySequence& Keys, FFile::Ptr File, const FKeyTypeSequence& Types)
{
	for (auto Index : XRange(Types.Num()))
	{
		auto& Data = Keys[Index];
		switch (Types[Index])
		{
		case EKeyType::Integer:
		{
			if (!File->Write(AnyCast<int64>(Data)))
				return false;
		}
		break;
		case EKeyType::String:
		{
			if (!File->WriteStaticString(AnyCast<FString>(Data)))
				return false;
		}
		break;
		}
		
	}

	return true;
}

int32 FIndexHelper::GetKeySize(const FKeyTypeSequence& KeyTypes)
{
	int Size = 0;
	for (auto& Type : KeyTypes)
	{
		switch (Type)
		{
		case EKeyType::Integer: Size += 8; break;
		case EKeyType::String: Size += 4; break;

		}
	}

	return Size;
}

bool FIndexHelper::Equal(const FKeySequence& Keys1, const FKeySequence& Keys2)
{
	if (Keys1.Num() != Keys2.Num())
		return false;

	auto Num = Keys1.Num();
	for (auto Index : XRange(Num))
	{
		if (Keys1[Index] == Keys2[Index])
			continue;
		return false;
	}

	return true;
}


uint32 FIndexHelper::Hash(const FKeySequence& Keys)
{
	auto HashValue = Keys[0].Hash();
	auto Num = Keys.Num();
	for (auto Index : XRange(1, Num))
	{
		HashValue = HashCombine(HashValue, Keys[Index].Hash());
	}
	return HashValue;
}