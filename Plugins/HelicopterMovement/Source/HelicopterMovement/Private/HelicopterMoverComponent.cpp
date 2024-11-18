#include "HelicopterMoverComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

UHelicopterMoverComponent::UHelicopterMoverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	MaxForwardSpeed = 1500.0f;
	MaxLateralSpeed = 1000.0f;
	MaxVerticalSpeed = 500.0f;
	YawSpeed = 90.0f;

	VelocityDamping = 0.95f;
	CorrectionInterpolation = 5.0f;

	PositionErrorThreshold = 10.0f;
	RotationErrorThreshold = 5.0f;

	SetIsReplicatedByDefault(true);
}

void UHelicopterMoverComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UHelicopterMoverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	
	if (GetOwner()->HasAuthority())
	{
		// Update authoritative state
		ServerState.Position = GetOwner()->GetActorLocation();
		ServerState.Rotation = GetOwner()->GetActorRotation();
		ServerState.Velocity = CurrentVelocity;
		ServerState.Timestamp = GetWorld()->GetTimeSeconds();
		ApplyInput(DeltaTime);
	}
	else
	{
		// Simulate client-side movement
		ApplyInput(DeltaTime);

		// Save predicted state
		SavePredictedState(GetWorld()->GetTimeSeconds());

		// Send input to the server
		FHelicopterInput Input;
		Input.DesiredInput = DesiredInput;
		Input.DesiredYawInput = DesiredYawInput;
		Input.Timestamp = GetWorld()->GetTimeSeconds();
		Server_SendInput(Input);

		// Reconcile state with server
		ReconcileState();
	}
}

void UHelicopterMoverComponent::Server_SendInput_Implementation(const FHelicopterInput& Input)
{
	DesiredInput = Input.DesiredInput;
	DesiredYawInput = Input.DesiredYawInput;

	// Apply input on the server
	ApplyInput(GetWorld()->DeltaTimeSeconds);

	// Update server state
	ServerState.Position = GetOwner()->GetActorLocation();
	ServerState.Rotation = GetOwner()->GetActorRotation();
	ServerState.Velocity = CurrentVelocity;
	ServerState.Timestamp = Input.Timestamp;
}

bool UHelicopterMoverComponent::Server_SendInput_Validate(const FHelicopterInput& Input)
{
	return true;
}

void UHelicopterMoverComponent::OnRep_ServerState()
{
	CorrectClientState();
}

void UHelicopterMoverComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Replicate the input variables
	DOREPLIFETIME(UHelicopterMoverComponent, DesiredInput);
	DOREPLIFETIME(UHelicopterMoverComponent, DesiredYawInput);

	// Replicate the server state for correction
	DOREPLIFETIME(UHelicopterMoverComponent, ServerState);
}

bool UHelicopterMoverComponent::IsOwnerLocallyControlled() const
{
	if (GetOwner() && GetOwner()->IsA<APawn>())
	{
		APawn* OwningPawn = Cast<APawn>(GetOwner());
		if (OwningPawn)
		{
			// Check if the pawn is controlled locally or if it is the listen server's player
			return OwningPawn->IsLocallyControlled() && GetOwner()->HasAuthority();
		}
	}
	return false;
}

void UHelicopterMoverComponent::ApplyInput(float DeltaTime)
{
	FVector Forward = GetOwner()->GetActorForwardVector();
	FVector Right = GetOwner()->GetActorRightVector();
	FVector Up = FVector::UpVector;

	// Calculate target velocity
	FVector TargetVelocity =
		Forward * DesiredInput.X * MaxForwardSpeed +
		Right * DesiredInput.Y * MaxLateralSpeed +
		Up * DesiredInput.Z * MaxVerticalSpeed;

	// Smooth velocity
	CurrentVelocity = FMath::VInterpTo(CurrentVelocity, TargetVelocity, DeltaTime, VelocityDamping);

	// Apply movement
	FVector NewPosition = GetOwner()->GetActorLocation() + CurrentVelocity * DeltaTime;
	GetOwner()->SetActorLocation(NewPosition, true);

	// Calculate yaw
	float TargetYawSpeed = DesiredYawInput * YawSpeed;
	CurrentYawSpeed = FMath::FInterpTo(CurrentYawSpeed, TargetYawSpeed, DeltaTime, VelocityDamping);

	// Apply yaw
	FRotator CurrentRotation = GetOwner()->GetActorRotation();
	float NewYaw = CurrentRotation.Yaw + CurrentYawSpeed * DeltaTime;
	GetOwner()->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
}

void UHelicopterMoverComponent::SavePredictedState(float Timestamp)
{
	FHelicopterState PredictedState;
	PredictedState.Position = GetOwner()->GetActorLocation();
	PredictedState.Rotation = GetOwner()->GetActorRotation();
	PredictedState.Velocity = CurrentVelocity;
	PredictedState.Timestamp = Timestamp;
	PredictedStates.Add(PredictedState);
}

void UHelicopterMoverComponent::ReconcileState()
{
	if (PredictedStates.Num() == 0)
		return;

	FHelicopterState LastPredictedState = PredictedStates[0];
	PredictedStates.RemoveAt(0);

	// Check for mismatches
	if (!LastPredictedState.Position.Equals(ServerState.Position, PositionErrorThreshold))
	{
		// Correct position
		GetOwner()->SetActorLocation(ServerState.Position);
	}

	if (!LastPredictedState.Rotation.Equals(ServerState.Rotation, RotationErrorThreshold))
	{
		// Correct rotation
		GetOwner()->SetActorRotation(ServerState.Rotation);
	}

	// Reapply remaining predicted inputs
	for (int32 i = 1; i < PredictedStates.Num(); i++)
	{
		ApplyInput(GetWorld()->DeltaTimeSeconds);
	}
}

void UHelicopterMoverComponent::UpdateServerState()
{
	ServerState.Position = GetOwner()->GetActorLocation();
	ServerState.Rotation = GetOwner()->GetActorRotation();
	ServerState.Velocity = CurrentVelocity;
	ServerState.Timestamp = GetWorld()->GetTimeSeconds();
}

void UHelicopterMoverComponent::CorrectClientState()
{
	// Interpolate to the server's authoritative state
	FVector CurrentPosition = GetOwner()->GetActorLocation();
	FRotator CurrentRotation = GetOwner()->GetActorRotation();

	FVector InterpolatedPosition = FMath::VInterpTo(CurrentPosition, ServerState.Position, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);
	FRotator InterpolatedRotation = FMath::RInterpTo(CurrentRotation, ServerState.Rotation, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);

	GetOwner()->SetActorLocation(InterpolatedPosition);
	GetOwner()->SetActorRotation(InterpolatedRotation);

	// Update velocity to match server state ** maybe???
	//CurrentVelocity = ServerState.Velocity; 
}
