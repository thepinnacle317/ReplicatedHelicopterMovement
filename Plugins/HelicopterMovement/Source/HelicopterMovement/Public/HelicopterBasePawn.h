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
	EES_Starting UMETA(DisplayName = "Starting Engine"),
	EES_Stopping UMETA(DisplayName = "Stopping Engine"),

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

	/* * * Helicopter Components * * */
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Core Components")
	TObjectPtr<UStaticMeshComponent> HelicopterBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Core Components")
	TObjectPtr<UStaticMeshComponent> MainRotor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Core Components")
	TObjectPtr<UStaticMeshComponent> TailRotor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Core Components")
	TObjectPtr<UHelicopterMoverComponent> HelicopterMover;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Helicopter Properties | Seating")
	TObjectPtr<USceneComponent> PilotsSeat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Helicopter Properties | Seating")
	TObjectPtr<USceneComponent> LeftPassenger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Helicopter Properties | Seating")
	TObjectPtr<USceneComponent> RightPassenger;

	/* Input Mapping Context */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Input Actions")
	TObjectPtr<UInputMappingContext> HelicopterInputMapping;

	/* * * Input Actions * * */ // All input will be moved to a player controller eventually
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Input Actions")
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Input Actions")
	TObjectPtr<UInputAction> YawAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Input Actions")
	TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Input Actions")
	TObjectPtr<UInputAction> EngineToggleAction;

	/* How long it takes for the rotors to get to full RPM speed for startup */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Helicopter Properties | Rotor Configs")
	float RotorSpinUpTime;

	/* Used to track the state of the engines being on/off */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Helicopter Properties | Engines")
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
