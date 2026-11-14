#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DemoWeaponVisualData.generated.h"

UENUM(BlueprintType)
enum class EDemoWeaponAttachment : uint8 { Body, Magazine, Bolt, ChargingHandle, Trigger, MuzzleFlash };

USTRUCT(BlueprintType)
struct FDemoWeaponPart
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UMaterialInterface> Material;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform Transform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDemoWeaponAttachment Attachment = EDemoWeaponAttachment::Body;
};

/** Replaceable weapon art with sight and hand calibration in the same local space. */
UCLASS(BlueprintType)
class SHOOTER_API UDemoWeaponVisualData : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDemoWeaponPart> Parts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform Sight = FTransform(FVector(2, 0, 8.2));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="10")) float EyeRelief = 24.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform RightHand = FTransform(FRotator(0, 0, 90), FVector(-1, 3.2, -6));
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform LeftHand = FTransform(FRotator(0, 0, -90), FVector(18, -2.8, -4));
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform MagazineHand = FTransform(FRotator(0, 0, 90), FVector(8, -4, -11));
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Muzzle = FVector(59, 0, 0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector HipPosition = FVector(25, 12, -16);
};
