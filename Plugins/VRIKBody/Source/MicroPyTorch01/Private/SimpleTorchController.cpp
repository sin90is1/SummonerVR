// VR IK Body Plugin
// (c) Yuri N Kalinin, 2021, ykasczc@gmail.com. All right reserved.

#include "SimpleTorchController.h"
#include "MicroPyTorch01.h"
#include "Modules/ModuleManager.h"
#include "HAL/UnrealMemory.h"
#include <vector>

USimpleTorchController::USimpleTorchController()
	: ModelId(INDEX_NONE)
{
	Buffer = NULL;
	BufferDims = NULL;
	BufferSize = 256;
}

void USimpleTorchController::BeginDestroy()
{
	Super::BeginDestroy();
	if (Buffer != NULL)
	{
		delete[] Buffer;
		Buffer = NULL;
	}
	if (BufferDims != NULL)
	{
		delete[] BufferDims;
		BufferDims = NULL;
	}
}

USimpleTorchController* USimpleTorchController::CreateSimpleTorchController(UObject* InParent)
{
	return InParent
		? NewObject<USimpleTorchController>(InParent)
		: NewObject<USimpleTorchController>();
}

bool USimpleTorchController::LoadTorchScriptModel(FString FileName)
{
	FMicroPyTorch01Module& Module = FModuleManager::GetModuleChecked<FMicroPyTorch01Module>(TEXT("MicroPyTorch01"));

	bool bResult = false;
	if (Module.bDllLoaded)
	{
		if (Buffer != NULL)
		{
			delete[] Buffer;
			Buffer = NULL;
		}
		Buffer = new float[BufferSize];
		if (BufferDims != NULL)
		{
			delete[] BufferDims;
			BufferDims = NULL;
		}
		BufferDims = new int[16];

#if PLATFORM_WINDOWS
		ModelId = Module.FuncTSW_LoadScriptModel(TCHAR_TO_ANSI(*FileName));
		bResult = (ModelId != INDEX_NONE);
#endif
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("USimpleTorchController: DLL not loaded"));
	}

	return bResult;
}

bool USimpleTorchController::IsTorchModelLoaded() const
{
	FMicroPyTorch01Module& Module = FModuleManager::GetModuleChecked<FMicroPyTorch01Module>(TEXT("MicroPyTorch01"));

	bool bResult = false;
#if PLATFORM_WINDOWS
	if (Module.bDllLoaded && Module.FuncTSW_CheckModel)
	{
		return Module.FuncTSW_CheckModel(ModelId);
	}
#endif

	return bResult;
}

bool USimpleTorchController::DoForwardCall(const FSimpleTensor& InData, FSimpleTensor& OutData)
{
	return ExecuteModelMethod(TEXT("forward"), InData, OutData);
}

bool USimpleTorchController::ExecuteModelMethod(const FString& MethodName, const FSimpleTensor& InData, FSimpleTensor& OutData)
{
	FMicroPyTorch01Module& Module = FModuleManager::GetModuleChecked<FMicroPyTorch01Module>(TEXT("MicroPyTorch01"));

	bool bResult = false;
	if (Module.bDllLoaded && Buffer != NULL && OutData.IsDataOwner())
	{
		TArray<int> InDims = InData.GetDimensions();

		float* pOutData = OutData.IsValid()
			? OutData.GetRawData()
			: Buffer;

		int OutDimsCount = 0;
#if PLATFORM_WINDOWS
		if (MethodName == TEXT("forward"))
		{
			if (Module.FuncTSW_ForwardPass_Def == NULL) return false;

			Module.FuncTSW_ForwardPass_Def(ModelId, InData.GetRawData(), InDims.GetData(), InDims.Num(),
				pOutData, BufferDims, &OutDimsCount);
		}
		else
		{
			if (Module.FuncTSW_Execute_Def == NULL) return false;

			Module.FuncTSW_Execute_Def(ModelId, TCHAR_TO_ANSI(*MethodName), InData.GetRawData(), InDims.GetData(), InDims.Num(),
				pOutData, BufferDims, &OutDimsCount);
		}
#endif

		bResult = (OutDimsCount > 0) && pOutData != NULL && BufferDims != NULL;
		if (bResult)
		{
			TArray<int32> OldOutDims = OutData.GetDimensions();
			bool bOutTensorMatches = (OutDimsCount == OutData.GetDimensions().Num());
			TArray<int32> NewOutDims;

			int32 Length = 1;
			for (int i = 0; i < OutDimsCount; i++)
			{
				Length *= BufferDims[i];
				NewOutDims.Add(BufferDims[i]);
				if (bOutTensorMatches && BufferDims[i] != OldOutDims[i])
				{
					bOutTensorMatches = false;
				}
			}

			if (bOutTensorMatches)
			{
				// do nothing
			}
			else
			{
				if (OutData.IsValid())
				{
					if (!OutData.Reshape(NewOutDims))
					{
						OutData.Cleanup();
						OutData.Create(NewOutDims);
					}
				}
				else
				{
					OutData.Create(NewOutDims);
				}
				FMemory::Memcpy(OutData.GetRawData(), Buffer, Length * sizeof(float));
			}
		}
	}

	return bResult;
}

/******************************************************************************************************/
/* FSimpleTensor																					  */
/******************************************************************************************************/

void FSimpleTensor::InitAddressSpace()
{
	AddressMultipliersCache.Empty();
	AddressMultipliersCache.AddUninitialized(Dimensions.Num());

	int32 CurrVal = 1;
	int32 Last = Dimensions.Num() - 1;
	AddressMultipliersCache[Last] = 1;
	for (int32 Dim = Last - 1; Dim != INDEX_NONE; Dim--)
	{
		CurrVal *= Dimensions[Dim + 1];
		AddressMultipliersCache[Dim] = CurrVal;
	}
}

void FSimpleTensor::Cleanup()
{
	if (Data)
	{
		if (bDataOwner)
		{
			for (auto& Child : DataUsers)
			{
				if (Child) Child->Cleanup();
			}
			DataUsers.Empty();
			delete[] Data;
		}
		else
		{
			if (ParentTensor)
			{
				ParentTensor->DataUsers.Remove(this);
			}
		}
		Data = NULL;
		Dimensions.Empty();
	}
}

int32 FSimpleTensor::GetAddress(TArray<int32> Address) const
{
	if (!Data)
	{
		return INDEX_NONE;
	}

	TArray<int32> AddrArray = Address;
	int32 Addr = 0;
	for (int32 i = 0; i < AddressMultipliersCache.Num(); i++)
	{
		int32 n = AddrArray.IsValidIndex(i) ? AddrArray[i] : 0;
		Addr += AddressMultipliersCache[i] * n;

		if (Addr >= DataSize)
		{
			return INDEX_NONE;
		}
	}

	return Addr;
}

bool FSimpleTensor::Create(TArray<int32> TensorDimensions)
{
	if (Data)
	{
		Cleanup();
	}

	if (TensorDimensions.Num() == 0) return false;

	DataSize = 1;
	for (const auto& Dim : TensorDimensions)
		DataSize *= Dim;

	if (DataSize == 0) return false;

	Dimensions = TensorDimensions;
	InitAddressSpace();

	Data = new float[DataSize];

	bDataOwner = true;
	return true;
}

bool FSimpleTensor::CreateAsChild(FSimpleTensor* Parent, TArray<int32> Address)
{
	bDataOwner = false;

	if (!Parent)
	{
		return false;
	}

	if (Address.Num() > Parent->Dimensions.Num())
	{
		UE_LOG(LogTemp, Log, TEXT("CreateAsChild: dimenstions error: Address (%d) vs Parent (%d)"), Address.Num(), Parent->Dimensions.Num());
		return false;
	}

	int32 Addr = Parent->GetAddress(Address);
	if (Addr == INDEX_NONE)
	{
		UE_LOG(LogTemp, Log, TEXT("CreateAsChild: parent has no address"));
		return false;
	}

	if (Address.Num() == Parent->Dimensions.Num())
	{
		Dimensions.Empty();
		Dimensions.Add(1);
	}
	else
	{
		Dimensions.Empty();
		for (int32 i = Address.Num(); i < Parent->Dimensions.Num(); i++)
		{
			Dimensions.Add(Parent->Dimensions[i]);
		}
	}

	DataSize = 1;
	for (const auto& Dim : Dimensions)
		DataSize *= Dim;

	Data = &(Parent->Data[Addr]);
	ParentTensor = Parent->bDataOwner ? Parent : Parent->ParentTensor;
	if (!ParentTensor)
	{
		return false;
	}

	ParentTensor->DataUsers.Add(this);

	InitAddressSpace();

	return true;
}

TArray<int32> FSimpleTensor::GetDimensions() const
{
	TArray<int32> t;
	for (const auto& d : Dimensions)
		t.Add(d);

	return t;
}

float FSimpleTensor::Item()
{
	return IsValid() ? Data[0] : 0.f;
}

float* FSimpleTensor::GetRawData(int32* Size) const
{
	if (!Data)
	{
		return nullptr;
	}
	else
	{
		int32 s = 1;
		for (const auto& Dim : Dimensions)
			if (Dim != 0)
				s *= Dim;

		*Size = s;

		return Data;
	}
}

float* FSimpleTensor::GetCell(TArray<int32> Address)
{
	int32 Addr = GetAddress(Address);
	return Addr == INDEX_NONE ? NULL : &Data[Addr];
}

float FSimpleTensor::GetValue(TArray<int32> Address) const
{
	int32 Addr = GetAddress(Address);
	return Addr == INDEX_NONE ? 0 : Data[Addr];
}

bool FSimpleTensor::ToArray(TArray<float>& OutArray) const
{
	if (Dimensions.Num() == 1)
	{
		int32 Num = Dimensions[0];
		OutArray.SetNumUninitialized(Num);
		FMemory::Memcpy(OutArray.GetData(), Data, Num * sizeof(float));

		return true;
	}

	return false;
}

bool FSimpleTensor::FromArray(const TArray<float>& InData)
{
	if (Dimensions.Num() == 1)
	{
		int32 Num = Dimensions[0];

		if (InData.Num() <= Num)
		{
			FMemory::Memcpy(Data, InData.GetData(), InData.Num() * sizeof(float));
			return true;
		}
	}

	return false;
}

bool FSimpleTensor::Reshape(TArray<int32> NewShape)
{
	if (NewShape.Num() == 0)
	{
		return false;
	}
	if (!Data)
	{
		return false;
	}

	int32 NewDataSize = 1;
	for (const auto& Dim : NewShape)
	{
		NewDataSize *= Dim;
	}

	if (NewDataSize == DataSize)
	{
		Dimensions = NewShape;
		InitAddressSpace();
	}
	else
	{
		if (bDataOwner && DataUsers.Num() == 0)
		{
			Cleanup();

			DataSize = NewDataSize;
			Dimensions = NewShape;
			Data = new float[DataSize];

			InitAddressSpace();
		}
		else
		{
			// don't reshape sub-tensors?
			return false;
		}
	}

	return true;
}

FSimpleTensor FSimpleTensor::Detach()
{
	if (!Data)
	{
		return FSimpleTensor();
	}

	TArray<int32> Dims;
	for (const auto& Val : Dimensions) Dims.Add(Val);

	FSimpleTensor ret = FSimpleTensor(Dims);
	FMemory::Memcpy(ret.GetRawData(), Data, DataSize);

	return ret;
}

void FSimpleTensor::ScaleItem(float Scale, float Offset)
{
	if (Data)
	{
		Data[0] = Data[0] * Scale + Offset;
	}
}

FSimpleTensor FSimpleTensor::operator[](int32 SubElement)
{
	if (!IsValid())
	{
		return *this;
	}
	if (SubElement < 0 || SubElement > Dimensions[0])
	{
		return *this;
	}
	
	FSimpleTensor t = FSimpleTensor(this, { SubElement });
	return (t.IsValid() ? t : *this);
}

float FSimpleTensor::operator=(const float& Value)
{
	if (!Data)
	{
		return Value;
	}

	Data[0] = Value;
	return Data[0];
}

bool FSimpleTensor::operator==(const float& Value) const
{
	if (!Data)
	{
		return false;
	}

	if (Dimensions.Num() != 1 || Dimensions[0] != 1)
	{
		return false;
	}

	return Data[0] == Value;
}

FSimpleTensor& FSimpleTensor::operator*=(float Scale)
{
	check(Data);
	check(Dimensions.Num() == 1 && Dimensions[0] == 1);
	
	Data[0] *= Scale;

	return *this;
}

FSimpleTensor& FSimpleTensor::operator+=(float Value)
{
	check(Data);
	check(Dimensions.Num() == 1 && Dimensions[0] == 1);

	Data[0] += Value;

	return *this;
}

FSimpleTensor& FSimpleTensor::operator-=(float Value)
{
	check(Data);
	check(Dimensions.Num() == 1 && Dimensions[0] == 1);

	Data[0] -= Value;

	return *this;
}