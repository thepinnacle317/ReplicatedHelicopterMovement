// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/MovementComponent.h"
#include "HelicopterMoverComponent.generated.h"

/*
 *
 */
USTRUCT(BlueprintType)
struct FHelicopterState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	FVector Velocity;

	FHelicopterState()
		: Position(FVector::ZeroVector), Rotation(FRotator::ZeroRotator), Velocity(FVector::ZeroVector) {}
};

/*
 * 
 */
UCLASS()
class HELICOPTERMOVEMENT_API UHelicopterMoverComponent : public UMovementComponent
{
	GENERATED_BODY()

public:
	UHelicopterMoverComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
    // Scene Components for Attachments //
	/* Pilot Pawn attachment location */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight")
    USceneComponent* PilotSeat;

	/* Passenger 1 Pawn attachment location */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight")
    USceneComponent* PassengerSeat1;

	/* Passenger 2 Pawn attachment location */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight")
    USceneComponent* PassengerSeat2;

    /* Weapon System Placeholder  *** Will create this after I have some solid movement and tie in my replicated targeting component
     * with this for reliable and fast hits
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight")
    class UHelicopterWeaponSystem* WeaponSystem;
    */

	/* Desired Pawn Inputs */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	FVector DesiredInput;  // X = Forward/Backward, Y = Sideways, Z = Vertical

	/* Desired Pawn Yaw Inputs */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	float DesiredYawInput; // Tail rotor input (yaw)

	/* Threshold that is allowed for the client to be out of sync and update for position in units */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Heli Movement | Network Correction")
	float PositionThreshold;

	/* Threshold that is allowed for the client to be out of sync and update for rotation in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Heli Movement | Network Correction")
	float RotationThreshold;

	/* Interpolation speed for the server to correct the client */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Heli Movement | Network Correction")
	float ServerCorrectionInterpolation;
	
	/* Current Pawn Velocity */
	FVector CurrentVelocity;
	/* Current Pawn Yaw Speed */
	float CurrentYawSpeed;

	/* Server Replicated State */
	UPROPERTY(ReplicatedUsing = OnRep_ServerState)
	FHelicopterState ServerState;

	/* Prediction States Stored */
	TArray<FHelicopterState> PredictedStates;

	// Movement parameters //
	/* Maximum allowed speed for the pawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight")
	float MaxForwardSpeed;

	/* Maximum allowed lateral speed for the pawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight")
	float MaxLateralSpeed;

	/* Maximum vertical speed for the pawn to climb */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight")
	float MaxVerticalSpeed;

	/* How fast the pawn can Yaw */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight")
	float YawSpeed;

	// Damping and smoothing //

	/* Reduces velocity over time */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight")
	float VelocityDamping; 

	/* Interpolation speed of helicopter body tilt */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight")
	float TiltSmoothingSpeed; // Speed of tilt adjustment

	/* Velocity interpolation speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Flight")
	float VelocitySmoothingSpeed;

	/* Max angle the helicopter can tilt on its axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tilt")
	float MaxTiltAngle = 15.0f;

	// Replication and reconciliation //
	/* Used to replicate the interpolated values */
	UFUNCTION()
	void OnRep_ServerState();

	/* Client side simulation of movement */
	void SimulateMovement(float DeltaTime);

	/* Used to bring the client in compliance with the server */
	void RewindAndReconcile();
	
	//void ApplyServerCorrection(); ** Delete me upon polish
	
	//void RewindForValidation(float DeltaTime); ** Delete me upon polish
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/* Used for the client to send input values to the server */
	UFUNCTION(Server, Reliable, WithValidation)
	void SendInputToServer(FVector InputVector, float YawInput);

protected:
	virtual void BeginPlay() override;

private:
	void UpdateThrottle(float DeltaTime);
	
	void UpdateMovement(float DeltaTime);
	
	void UpdateYaw(float DeltaTime);
	
	void UpdateTilt(float DeltaTime);

	/* Used in update movement for refined movement */
	FVector SmoothVelocity(FVector Current, FVector Target, float Damping, float DeltaTime);
	
};
