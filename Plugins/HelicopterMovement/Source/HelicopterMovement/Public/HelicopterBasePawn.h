// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "HelicopterBasePawn.generated.h"

/* Forward Declarations */
class UHelicopterMoverComponent;

UCLASS()
class HELICOPTERMOVEMENT_API AHelicopterBasePawn : public APawn
{
	GENERATED_BODY()

public:
	AHelicopterBasePawn();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Helicopter Core Components //
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Helicopter Components")
	TObjectPtr<UStaticMeshComponent> HelicopterBody;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Helicopter Components")
	TObjectPtr<UStaticMeshComponent> MainRotor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Helicopter Components")
	TObjectPtr<UStaticMeshComponent> TailRotor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Helicopter Components")
	TObjectPtr<UHelicopterMoverComponent> HelicopterMover;

	// Enhanced Input Actions //
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputMappingContext* HelicopterInputMapping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* YawAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* ThrottleAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* StartAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* StopAction;

	/* How long it will take for the rotors to spin-up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotor")
	float RotorSpinUpTime = 2.0f; // Time in seconds for rotors to reach full speed

	/* Current rotor speed, interpolates during spin-up ** Move to movement comp? */
	UPROPERTY(BlueprintReadOnly, Category = "Rotor")
	float RotorSpeed; 

	/* Max angle the helicopter can tilt on its axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tilt")
	float MaxTiltAngle = 15.0f;

	/* How fast the helicopter will interpolate the tilt values */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tilt")
	float TiltInterpSpeed;

	/* Determines if the helicopter should start and begin warm-up/engine start */
	bool bIsStartingUp = false;

	/* Starts the helicopter */
	void StartHelicopter();

protected:
	virtual void BeginPlay() override;

private:
	// Input Handler Methods //
	void HandleMovementInput(const FInputActionValue& Value);
	void HandleMovementInputReleased(const FInputActionValue& Value);
	void HandleYawInput(const FInputActionValue& Value);
	void HandleYawInputReleased(const FInputActionValue& Value);
	void HandleThrottleInput(const FInputActionValue& Value);
	void HandleThrottleInputReleased(const FInputActionValue& Value);

	/* Sends the input to the mover ** Redundant at this point *** Review for polish and fix */
	void SendInputToMover();

	/* Used to update the rotor speeds */
	void UpdateRotorSpeed(float DeltaTime);
	/* Applies the tilt to the body */
	void ApplyTilt(float DeltaTime);

	//TODO: Apply small tilt to the main rotor based on direction ** probably around 3-5 degrees

};

