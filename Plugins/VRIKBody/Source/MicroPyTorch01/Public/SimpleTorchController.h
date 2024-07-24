// VR IK Body Plugin
// (c) Yuri N Kalinin, 2021, ykasczc@gmail.com. All right reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SimpleTorchController.generated.h"

/*
* Simple struct to represent multidimensional float arrays
* It's not intended to process large data.
* Use it to store NN inputs and outputs.
* Don't create it every time. Create tensor object once and then set its data when needed.
*/
USTRUCT(BlueprintType)
struct MICROPYTORCH01_API FSimpleTensor
{
	GENERATED_USTRUCT_BODY()

protected:
	float* Data;

	/** Data dimensions */
	UPROPERTY()
	TArray<int32> Dimensions;

	/** Is memory for Data allocated by this object? */
	UPROPERTY()
	bool bDataOwner;

	/** Internal cache is navigate through Data */
	UPROPERTY()
	TArray<int32> AddressMultipliersCache;

	/** Size of Data (size in bytes = sizeof(float) * DataSize */
	UPROPERTY()
	int32 DataSize;

	/** Object created Data (for bDataOwner == false) */
	FSimpleTensor* ParentTensor;

	/** Other objects using Data (for bDataOwner == true) */
	TSet<FSimpleTensor*> DataUsers;

	/** Initialize AddressMultipliersCache */
	void InitAddressSpace();

	/** Get flat address in Data from multidimensional address */
	int32 GetAddress(TArray<int32> Address) const;
public:

	FSimpleTensor()
		: Data(NULL)
		, bDataOwner(true)
		, DataSize(0)
		, ParentTensor(nullptr)
	{}
	FSimpleTensor(FSimpleTensor* Parent, TArray<int32> SubAddress)
		: Data(NULL)
		, bDataOwner(true)
		, DataSize(0)
		, ParentTensor(nullptr)
	{
		CreateAsChild(Parent, SubAddress);
	}
	FSimpleTensor(TArray<int32> Dimensions)
		: Data(NULL)
		, bDataOwner(true)
		, DataSize(0)
		, ParentTensor(nullptr)
	{
		Create(Dimensions);
	}
	~FSimpleTensor()
	{
		Cleanup();
	}

	// Clear Data
	void Cleanup();

	// Set dimensions and allocate memory
	bool Create(TArray<int32> TensorDimensions);

	// Create tensor as a subtensor in another tensor (share the same memory)
	bool CreateAsChild(FSimpleTensor* Parent, TArray<int32> Address);

	// Is tensor initialized?
	bool IsValid() const { return Data != NULL; }

	// Is tensor parent fro this data?
	bool IsDataOwner() const { return bDataOwner; }

	// Get current tensor dimensions
	TArray<int32> GetDimensions() const;

	// Get number of itemes in flat array
	int32 GetDataSize() const { return DataSize; }

	// Get first value (without offset) as float
	float Item();

	// Get raw data as points of [Size] size
	float* GetRawData(int32* Size) const;
	float* GetRawData() const { return Data; }

	// Convert multidimensional address to flat address
	int32 GetRawAddress(TArray<int32> Address) const { return GetAddress(Address); }

	// Get reference to single float value with address
	float* GetCell(TArray<int32> Address);
	float GetValue(TArray<int32> Address) const;

	/* Create float array. Only works for tensor with one dimension.
	* Ex: auto p = FSimpleTensor({ 4, 12 });
	* p[2].ToArray(arr);
	*	(arr.Num() == 12)
	* p.ToArray(arr);
	*	no, p has two dimensions
	*/
	bool ToArray(TArray<float>& OutArray) const;

	/* Fill tensor from array. Only works for tensor with one dimension.
	* Ex. auto p = FSimpleTensor({ 8, 2 });
	* TArray<float> InData; InData.Add(12.f); InData.Add(24.f);
	* p[7].FromArray(InData);
	*	correct, (p[7][0] == 12.f && p[7][1] == 24.f)
	* p.FromArray(InData);
	*	error: p has two dimensions
	*/
	bool FromArray(const TArray<float>& InData);

	// Change dimensions.
	// Only keeps data if new overall size is equal to old sizse
	bool Reshape(TArray<int32> NewShape);

	// Create copy of this tensor
	FSimpleTensor Detach();

	// Apply scale and then offset only to first item of tensor
	void ScaleItem(float Scale, float Offset);

	FSimpleTensor operator[](int32 SubElement);
	float operator=(const float& Value);
	bool operator==(const float& Value) const;
	FSimpleTensor& operator*=(float Scale);
	FSimpleTensor& operator+=(float Value);
	FSimpleTensor& operator-=(float Value);
};


/**
 * PyTorch Jit Module controller: load model and execute its methods
 */
UCLASS(Blueprintable)
class MICROPYTORCH01_API USimpleTorchController : public UObject
{
	GENERATED_BODY()
	
public:
	USimpleTorchController();

	virtual void BeginDestroy() override;

	/** Create new controller from blueprint */
	UFUNCTION(BlueprintCallable, Category = "Simple Torch")
	static USimpleTorchController* CreateSimpleTorchController(UObject* InParent);

	/** Load neural net model from file */
	UFUNCTION(BlueprintCallable, Category = "Simple Torch")
	bool LoadTorchScriptModel(FString FileName);

	/** Is any neural net model loaded? */
	UFUNCTION(BlueprintPure, Category = "Simple Torch")
	bool IsTorchModelLoaded() const;

	/** Execute Forward(...) method in loaded model */
	UFUNCTION(BlueprintCallable, Category = "Simple Torch", meta = (DisplayName = "Do Forward Call"))
	bool DoForwardCall(const FSimpleTensor& InData, FSimpleTensor& OutData);

	/** Execute any method by name in loaded model */
	UFUNCTION(BlueprintCallable, Category = "Simple Torch", meta = (DisplayName = "Do Forward Call"))
	bool ExecuteModelMethod(const FString& MethodName, const FSimpleTensor& InData, FSimpleTensor& OutData);

private:
	/** Identified of the loaded torch script model */
	int32 ModelId;

	/** Buffer size (default) */
	int32 BufferSize;

	/** Buffer to get results from torch method */
	float* Buffer;
	/** Output Buffer dimensions */
	int* BufferDims;
};
