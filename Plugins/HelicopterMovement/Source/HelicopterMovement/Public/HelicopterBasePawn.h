#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "HelicopterBasePawn.generated.h"

/* Forward Declarations */
class UHelicopterMoverComponent;

UENUM(BlueprintType)
enum class EEngine_State : uint8
{
	EES_EngineOn UMETA(DisplayName = "Engine On"),
	EES_EngineOff UMETA(DisplayName = "Engine Off"),

	EES_Max UMETA(DisplayName = "Default Max")
};

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Actions")
	UInputMappingContext* HelicopterInputMapping;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Actions")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Actions")
	UInputAction* YawAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Actions")
	UInputAction* ThrottleAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Actions")
	UInputAction* EngineToggleAction;

	// Rotor Spin-Up
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotor")
	float RotorSpinUpTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Helicopter Properties")
	EEngine_State EngineState;

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
