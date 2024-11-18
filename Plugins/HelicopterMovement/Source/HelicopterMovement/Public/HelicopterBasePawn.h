#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "HelicopterBasePawn.generated.h"

class UHelicopterMoverComponent;

UCLASS()
class HELICOPTERMOVEMENT_API AHelicopterBasePawn : public APawn
{
	GENERATED_BODY()

public:
	AHelicopterBasePawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Helicopter Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* HelicopterBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MainRotor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TailRotor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHelicopterMoverComponent* HelicopterMover;

	// Input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* HelicopterInputMapping;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* YawAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* ThrottleAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* StartAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* StopAction;

	// Rotor Spin-Up
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotor")
	float RotorSpinUpTime;

protected:
	virtual void BeginPlay() override;

private:
	// Rotor controls
	void StartHelicopter();
	void StopHelicopter();
	void UpdateRotorSpeed(float DeltaTime);

	// Input handlers
	void HandleMovementInput(const FInputActionValue& Value);
	void HandleMovementInputReleased(const FInputActionValue& Value);
	void HandleYawInput(const FInputActionValue& Value);
	void HandleYawInputReleased(const FInputActionValue& Value);
	void HandleThrottleInput(const FInputActionValue& Value);
	void HandleThrottleInputReleased(const FInputActionValue& Value);

	// Rotor-related state
	bool bIsStartingUp;
	float RotorSpeed;
};
