#pragma once

#include "HelicopterBasePawn.h"
#include "HelicopterMoverComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Net/UnrealNetwork.h"

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

void AHelicopterBasePawn::OnRep_RotorSpeed()
{
	
}

void AHelicopterBasePawn::OnRep_EngineState()
{
	
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

	// Only execute rotor speed updates on the server
	if (HasAuthority())
	{
		UpdateRotorSpeed(DeltaSeconds);
	}
	// Always spin the rotors locally for visuals
	SpinRotors(DeltaSeconds);
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
		EnhancedInput->BindAction(EngineToggleAction, ETriggerEvent::Started, this, &AHelicopterBasePawn::ToggleEngines);
	}
}

void AHelicopterBasePawn::StartHelicopter()
{
	if (HasAuthority())
	{
		Server_StartEngine();
	}
}

void AHelicopterBasePawn::StopHelicopter()
{
	if (HasAuthority())
	{
		Server_StopEngine();
	}
}

void AHelicopterBasePawn::ToggleEngines()
{
	if (HasAuthority())
	{
		if (EngineState == EEngine_State::EES_EngineOff || EngineState == EEngine_State::EES_Stopping)
		{
			StartHelicopter();
		}
		else if (EngineState == EEngine_State::EES_EngineOn || EngineState == EEngine_State::EES_Starting)
		{
			StopHelicopter();
		}
	}
}

void AHelicopterBasePawn::Server_StartEngine_Implementation()
{
	EngineState = EEngine_State::EES_Starting;
}

void AHelicopterBasePawn::Server_StopEngine_Implementation()
{
	EngineState = EEngine_State::EES_Stopping;
}

void AHelicopterBasePawn::UpdateRotorSpeed(float DeltaTime)
{
	if (EngineState == EEngine_State::EES_Starting)
	{
		// Spin-up logic
		RotorSpeed = FMath::Clamp(RotorSpeed + DeltaTime / RotorSpinUpTime, 0.0f, 1.0f);

		if (RotorSpeed >= 1.0f)
		{
			// Transition to Engine On state once rotors reach full speed
			EngineState = EEngine_State::EES_EngineOn;
		}
	}
	else if (EngineState == EEngine_State::EES_Stopping)
	{
		// Spin-down logic
		RotorSpeed = FMath::Clamp(RotorSpeed - DeltaTime / RotorSpinUpTime, 0.0f, 1.0f);

		if (RotorSpeed <= 0.0f)
		{
			// Transition to Engine Off state once rotors stop
			EngineState = EEngine_State::EES_EngineOff;
		}
	}
	else if (EngineState == EEngine_State::EES_EngineOff)
	{
		// Ensure rotor speed is zero in the Engine Off state
		RotorSpeed = 0.0f;
	}
}

void AHelicopterBasePawn::SpinRotors(float DeltaTime)
{
	if (MainRotor && TailRotor)
	{
		MainRotor->AddRelativeRotation(FRotator(0.0f, RotorSpeed * 720.0f * DeltaTime, 0.0f));
		TailRotor->AddRelativeRotation(FRotator(RotorSpeed * 540.0f * DeltaTime, 0.0f, 0.0f));
	}
}

void AHelicopterBasePawn::HandleMovementInput(const FInputActionValue& Value)
{
	if (EngineState != EEngine_State::EES_EngineOn) return;
	
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
	if (EngineState != EEngine_State::EES_EngineOn) return;
	
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
	if (EngineState != EEngine_State::EES_EngineOn) return;
	
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

void AHelicopterBasePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHelicopterBasePawn, RotorSpeed);
	DOREPLIFETIME(AHelicopterBasePawn, EngineState);
}
