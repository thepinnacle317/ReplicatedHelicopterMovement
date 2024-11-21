#include "HelicopterBasePawn.h"
#include "HelicopterMoverComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

AHelicopterBasePawn::AHelicopterBasePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Network Configuration
	NetUpdateFrequency = 100.0f;
	MinNetUpdateFrequency = 30.0f;

	// Components
	HelicopterBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HelicopterBody"));
	RootComponent = HelicopterBody;
	HelicopterBody->SetIsReplicated(true);
	HelicopterBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HelicopterBody->SetCollisionResponseToAllChannels(ECR_Block);

	MainRotor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainRotor"));
	MainRotor->SetupAttachment(HelicopterBody);

	TailRotor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TailRotor"));
	TailRotor->SetupAttachment(HelicopterBody);

	HelicopterMover = CreateDefaultSubobject<UHelicopterMoverComponent>(TEXT("HelicopterMover"));
	HelicopterMover->SetIsReplicated(true);

	RotorSpinUpTime = 10.0f;
	RotorSpeed = 0.0f;
	bIsStartingUp = false;

	EngineState = EEngine_State::EES_EngineOff;
}

void AHelicopterBasePawn::BeginPlay()
{
	Super::BeginPlay();

	// Bind input mapping context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(HelicopterInputMapping, 0);
		}
	}
}

void AHelicopterBasePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsStartingUp)
	{
		UpdateRotorSpeed(DeltaSeconds);
	}
}

void AHelicopterBasePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Bind movement
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHelicopterBasePawn::HandleMovementInput);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &AHelicopterBasePawn::HandleMovementInputReleased);

		// Bind yaw
		EnhancedInput->BindAction(YawAction, ETriggerEvent::Triggered, this, &AHelicopterBasePawn::HandleYawInput);
		EnhancedInput->BindAction(YawAction, ETriggerEvent::Completed, this, &AHelicopterBasePawn::HandleYawInputReleased);

		// Bind throttle
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AHelicopterBasePawn::HandleThrottleInput);
		EnhancedInput->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AHelicopterBasePawn::HandleThrottleInputReleased);

		// Bind rotor controls
		EnhancedInput->BindAction(EngineToggleAction, ETriggerEvent::Triggered, this, &AHelicopterBasePawn::StartHelicopter);
	}
}

void AHelicopterBasePawn::StartHelicopter()
{
	bIsStartingUp = true;
	RotorSpeed = 0.0f;
}

void AHelicopterBasePawn::StopHelicopter()
{
	bIsStartingUp = false;
	RotorSpeed = 0.0f;
}

void AHelicopterBasePawn::UpdateRotorSpeed(float DeltaTime)
{
	if (bIsStartingUp)
	{
		RotorSpeed = FMath::Clamp(RotorSpeed + DeltaTime / RotorSpinUpTime, 0.0f, 1.0f);

		MainRotor->AddRelativeRotation(FRotator(0.0f, RotorSpeed * 720.0f * DeltaTime, 0.0f));
		TailRotor->AddRelativeRotation(FRotator(RotorSpeed * 540.0f * DeltaTime, 0.0f, 0.0f));
		
		if (RotorSpeed >= 1.0f)
		{
			EngineState = EEngine_State::EES_EngineOn;
		}
	}
	else
	{
		EngineState = EEngine_State::EES_EngineOff;
		bIsStartingUp = false;
	}
}

void AHelicopterBasePawn::HandleMovementInput(const FInputActionValue& Value)
{
	FVector2D Input = Value.Get<FVector2D>();
	if (HelicopterMover)
	{
		HelicopterMover->DesiredInput.X = Input.X; // Forward/Backward
		HelicopterMover->DesiredInput.Y = Input.Y; // Left/Right
	}
}

void AHelicopterBasePawn::HandleMovementInputReleased(const FInputActionValue& Value)
{
	if (HelicopterMover)
	{
		HelicopterMover->DesiredInput.X = 0.0f;
		HelicopterMover->DesiredInput.Y = 0.0f;
	}
}

void AHelicopterBasePawn::HandleYawInput(const FInputActionValue& Value)
{
	if (HelicopterMover)
	{
		HelicopterMover->DesiredYawInput = Value.Get<float>();
	}
}

void AHelicopterBasePawn::HandleYawInputReleased(const FInputActionValue& Value)
{
	if (HelicopterMover)
	{
		HelicopterMover->DesiredYawInput = 0.0f;
	}
}

void AHelicopterBasePawn::HandleThrottleInput(const FInputActionValue& Value)
{
	if (HelicopterMover)
	{
		HelicopterMover->DesiredInput.Z = Value.Get<float>();
	}
}

void AHelicopterBasePawn::HandleThrottleInputReleased(const FInputActionValue& Value)
{
	if (HelicopterMover)
	{
		HelicopterMover->DesiredInput.Z = 0.0f;
	}
}
