#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HelicopterMoverComponent.generated.h"

/* * * Struct to hold state data for prediction and reconciliation * * */
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

/* * * Struct for inputs used in movement prediction * * */
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
	
	/* Helicopter movement configuration  *** will move variables to read only before shipping */
	
	/* Max Speed that the helicopter can move forward */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Speed")
	float MaxForwardSpeed;

	/* Max Speed the helicopter can move from side to side */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Speed")
	float MaxLateralSpeed;

	/* Max Speed that the helicopter can move up and down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Speed")
	float MaxVerticalSpeed;

	/* How fast the helicopter can rotate on its center axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Speed")
	float YawSpeed;

	/* Controls how fast the helicopter will come to a stop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Speed")
	float VelocityDamping;

	/* Max angle the body can rotate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Tilting")
	float MaxTiltAngle;

	/* The interpolation speed for tilting the helicopter body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Tilting")
	float TiltSmoothingSpeed;

	/* Adjust for how "bouncy" the surface is */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Colliding")
	float BounceDampingFactor;

	/* */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Colliding")
	float SkidVelocityThreshold;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Skipping")
	float SurfaceFriction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Skipping")
	float ImpactOffset; 

	/* */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Colliding")
	float CollisionSphere;

	/* The interpolation speed for position error correction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Server Corrections")
	float CorrectionInterpolation;

	/* How many units the client can be off before being corrected by the server */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Server Corrections")
	float PositionErrorThreshold;

	/* How many degrees the client can be off before being corrected by the server */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Helicopter Properties | Server Corrections")
	float RotationErrorThreshold;
	
	/* Input variables */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Input")
	FVector DesiredInput;
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Input")
	float DesiredYawInput;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/* Networking */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SendInput(const FHelicopterInput& Input);
	
	void Server_SendInput_Implementation(const FHelicopterInput& Input);
	bool Server_SendInput_Validate(const FHelicopterInput& Input);

	UFUNCTION()
	void OnRep_ServerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/* Helper function to check for the controlling actor */
	bool IsOwnerLocallyControlled() const;

private:
	/* Movement functions */
	void ApplyInput(float DeltaTime);
	void CorrectClientState();
	void SavePredictedState(float Timestamp);
	void ReconcileState();
	void UpdateServerState();

	void HandleCollision(const FHitResult& HitResult, float DeltaTime);

	/* Used to handle applying tilt to the helicopter body based on current velocity */
	void ApplyBodyTilt(float DeltaTime);

	/* Current velocity and yaw speed */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Helicopter Properties | Speed", meta=(AllowPrivateAccess = "true"))
	FVector CurrentVelocity;
	float CurrentYawSpeed;

	/* State management */
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_ServerState)
	FHelicopterState ServerState;

	/* Predicted states for reconciliation */
	TArray<FHelicopterState> PredictedStates;
};
