#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HelicopterMoverComponent.generated.h"

/** Struct to hold state data for prediction and reconciliation */
USTRUCT()
struct FHelicopterState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	FVector Velocity;

	UPROPERTY()
	float Timestamp;
};

/** Struct for inputs used in movement prediction */
USTRUCT()
struct FHelicopterInput
{
	GENERATED_BODY()

	UPROPERTY()
	FVector DesiredInput;

	UPROPERTY()
	float DesiredYawInput;

	UPROPERTY()
	float Timestamp;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HELICOPTERMOVEMENT_API UHelicopterMoverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHelicopterMoverComponent();

	/** Input variables */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Input")
	FVector DesiredInput;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Input")
	float DesiredYawInput;

	/** Helicopter movement configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Settings")
	float MaxForwardSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Settings")
	float MaxLateralSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Settings")
	float MaxVerticalSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Settings")
	float YawSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Settings")
	float VelocityDamping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Settings")
	float CorrectionInterpolation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Settings")
	float PositionErrorThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Settings")
	float RotationErrorThreshold;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Networking */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SendInput(const FHelicopterInput& Input);

	void Server_SendInput_Implementation(const FHelicopterInput& Input);
	bool Server_SendInput_Validate(const FHelicopterInput& Input);

	UFUNCTION()
	void OnRep_ServerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsOwnerLocallyControlled() const;

private:
	/** Movement functions */
	void ApplyInput(float DeltaTime);
	void CorrectClientState();
	void SavePredictedState(float Timestamp);
	void ReconcileState();
	void UpdateServerState();

	/** Current velocity and yaw speed */
	FVector CurrentVelocity;
	float CurrentYawSpeed;

	/** State management */
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_ServerState)
	FHelicopterState ServerState;

	/** Predicted states for reconciliation */
	TArray<FHelicopterState> PredictedStates;
};
