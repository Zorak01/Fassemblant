#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DemoEditorLibrary.generated.h"

UCLASS()
class SHOOTER_API UDemoEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Demo|Editor") static USkeletalMesh* PrepareMannequin();
    /** Each section is (axial X, half width Y, half height Z). */
    UFUNCTION(BlueprintCallable, Category="Demo|Editor") static UStaticMesh* CreateProfileMesh(const FString& Name, const TArray<FVector>& Sections, int32 Sides, float WallThickness, UMaterialInterface* Material);
};
