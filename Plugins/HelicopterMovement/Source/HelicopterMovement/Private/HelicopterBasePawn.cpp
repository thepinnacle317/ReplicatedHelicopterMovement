// Fill out your copyright notice in the Description page of Project Settings.


#include "HelicopterBasePawn.h"
#include "HelicopterMoverComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

// Sets default values
AHelicopterBasePawn::AHelicopterBasePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	/* Network Update */
	NetUpdateFrequency = 60.0f;
	MinNetUpdateFrequency = 15.0f;

	/* Construct Components */
	HelicopterBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Helicopter Body"));
	RootComponent = HelicopterBody;
	HelicopterBody->SetIsReplicated(true);
	HelicopterBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HelicopterBody->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	HelicopterBody->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	HelicopterBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// Ensure physics simulation is off if using SetActorLocation
	HelicopterBody->SetSimulatePhysics(false);
	
	MainRotor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Main Rotor"));
	MainRotor->SetupAttachment(HelicopterBody);
	
	TailRotor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tail Rotor"));
	TailRotor->SetupAttachment(HelicopterBody);

	HelicopterMover = CreateDefaultSubobject<UHelicopterMoverComponent>(TEXT("Helicopter Mover"));
	HelicopterMover->SetIsReplicated(true);
	
	RotorSpeed = 0.0f;
	TiltInterpSpeed = 5.0f;
}

void AHelicopterBasePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Send input to the HelicopterMoverComponent
	SendInputToMover();
	// Update rotor speed during spin-up
	UpdateRotorSpeed(DeltaSeconds);
	// Apply tilt based on input
	ApplyTilt(DeltaSeconds);
}

void AHelicopterBasePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Bind movement input
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHelicopterBasePawn::HandleMovementInput);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &AHelicopterBasePawn::HandleMovementInputReleased);

		// Bind yaw input
		EnhancedInput->BindAction(YawAction, ETriggerEvent::Triggered, this, &AHelicopterBasePawn::HandleYawInput);
		EnhancedInput->BindAction(YawAction, ETriggerEvent::Completed, this, &AHelicopterBasePawn::HandleYawInputReleased);

		// Bind throttle input
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AHelicopterBasePawn::HandleThrottleInput);
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AHelicopterBasePawn::HandleThrottleInputReleased);

		EnhancedInput->BindAction(StartAction, ETriggerEvent::Started, this, &AHelicopterBasePawn::StartHelicopter);
	}
}

// Not currently implemented
void AHelicopterBasePawn::StartHelicopter()
{
	bIsStartingUp = true;
	RotorSpeed = 0.0f; // Start from zero
}

void AHelicopterBasePawn::BeginPlay()
{
	Super::BeginPlay();

	// Add input mapping context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(HelicopterInputMapping, 0);
		}
	}
}

void AHelicopterBasePawn::HandleMovementInput(const FInputActionValue& Value)
{
	FVector2D Input = Value.Get<FVector2D>();
	if (HelicopterMover)
	{
		HelicopterMover->DesiredInput.X = Input.X; // Forward/Backward
		HelicopterMover->DesiredInput.Y = Input.Y; // Left/Right
		//UE_LOG(LogTemp, Log, TEXT("Movement Input - X: %f, Y: %f"), Input.X, Input.Y);
	}
}

void AHelicopterBasePawn::HandleMovementInputReleased(const FInputActionValue& Value)
{
	if (HelicopterMover)
	{
		HelicopterMover->DesiredInput.X = 0.0f;
		HelicopterMover->DesiredInput.Y = 0.0f;
		//UE_LOG(LogTemp, Log, TEXT("Movement Input Released"));
	}
}

void AHelicopterBasePawn::HandleYawInput(const FInputActionValue& Value)
{
	float Input = Value.Get<float>();
	if (HelicopterMover)
	{
		HelicopterMover->DesiredYawInput = Input;
		//UE_LOG(LogTemp, Log, TEXT("Yaw Input (HandleYawInput): %f"), Input);
	}
}

void AHelicopterBasePawn::HandleYawInputReleased(const FInputActionValue& Value)
{
	if (HelicopterMover)
	{
		HelicopterMover->DesiredYawInput = 0.0f;
		//UE_LOG(LogTemp, Log, TEXT("Yaw Input Released"));
	}
}

void AHelicopterBasePawn::HandleThrottleInput(const FInputActionValue& Value)
{
	if (HelicopterMover)
	{
		HelicopterMover->DesiredInput.Z = Value.Get<float>();
		//UE_LOG(LogTemp, Log, TEXT("Throttle Input: %f"), HelicopterMover->DesiredInput.Z);
	}
}

void AHelicopterBasePawn::HandleThrottleInputReleased(const FInputActionValue& Value)
{
	if (HelicopterMover)
	{
		HelicopterMover->DesiredInput.Z = 0.0f;
		//UE_LOG(LogTemp, Log, TEXT("Throttle Input Released"));
	}
}

// Seems redundant at this point 
void AHelicopterBasePawn::SendInputToMover()
{
	if (HelicopterMover)
	{
		if (HasAuthority())
		{
			// Server Directly apply the current DesiredInput and DesiredYawInput
			//HelicopterMover->DesiredInput = HelicopterMover->DesiredInput;
			//HelicopterMover->DesiredYawInput = HelicopterMover->DesiredYawInput;
		}
		else
		{
			// Client Send DesiredInput and DesiredYawInput to the server
			HelicopterMover->SendInputToServer(HelicopterMover->DesiredInput, HelicopterMover->DesiredYawInput);
		}
	}
}

// Not currently implemented
void AHelicopterBasePawn::UpdateRotorSpeed(float DeltaTime)
{
	if (bIsStartingUp)
	{
		// Gradually increase rotor speed over time
		RotorSpeed = FMath::Clamp(RotorSpeed + (DeltaTime / RotorSpinUpTime), 0.0f, 1.0f);

		// Stop spin-up when full speed is reached
		if (RotorSpeed >= 1.0f)
		{
			bIsStartingUp = false;
		}

		// Apply rotor speed to visual components
		MainRotor->AddRelativeRotation(FRotator(0.0f, RotorSpeed * 720.0f * DeltaTime, 0.0f)); // Faster spin
		TailRotor->AddRelativeRotation(FRotator(0.0f, RotorSpeed * 720.0f * DeltaTime, 0.0f));
	}
}

void AHelicopterBasePawn::ApplyTilt(float DeltaTime)
{
	if (!HelicopterMover) return;

	// Retrieve input directly from the HelicopterMoverComponent
	FVector DesiredInput = HelicopterMover->DesiredInput;

	// Calculate tilt based on input
	float TargetPitch = FMath::Clamp(-DesiredInput.X * MaxTiltAngle, -MaxTiltAngle, MaxTiltAngle); // Forward/Backward tilt
	float TargetRoll = FMath::Clamp(DesiredInput.Y * MaxTiltAngle, -MaxTiltAngle, MaxTiltAngle);   // Left/Right tilt

	// Get the current rotation of the helicopter body
	FRotator CurrentRotation = HelicopterBody->GetRelativeRotation();
	
	//UE_LOG(LogTemp, Log, TEXT("Desired Input: X = %f, Y = %f"), DesiredInput.X, DesiredInput.Y);
	//UE_LOG(LogTemp, Log, TEXT("Target Pitch: %f, Target Roll: %f"), TargetPitch, TargetRoll);

	// Smoothly return to neutral tilt if no input
	if (DesiredInput.IsNearlyZero())
	{
		TargetPitch = 0.0f;
		TargetRoll = 0.0f;
		//UE_LOG(LogTemp, Log, TEXT("Returning to neutral tilt"));
	}

	// Smoothly interpolate tilt to target
	FRotator TargetRotation = FRotator(TargetPitch, CurrentRotation.Yaw, TargetRoll);
	FRotator SmoothedRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, TiltInterpSpeed);

	// Apply the smoothed rotation
	HelicopterBody->SetRelativeRotation(SmoothedRotation);
	
	//UE_LOG(LogTemp, Log, TEXT("Final Rotation: %s"), *HelicopterBody->GetRelativeRotation().ToString());
}


