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

	MaxTiltAngle = 15.0f;
	TiltSmoothingSpeed = 5.0f;

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

	// Apply this after all the corrections have been made
	ApplyBodyTilt(DeltaTime);
}

void UHelicopterMoverComponent::Server_SendInput_Implementation(const FHelicopterInput& Input)
{
	DesiredInput = Input.DesiredInput;
	DesiredYawInput = Input.DesiredYawInput;

	// Apply input on the server
	//ApplyInput(GetWorld()->DeltaTimeSeconds);

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

	// Helicopter Velocity
	DOREPLIFETIME(UHelicopterMoverComponent, CurrentVelocity);

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

	// Apply yaw without overriding pitch and roll
	FRotator CurrentRotation = GetOwner()->GetActorRotation();
	float NewYaw = CurrentRotation.Yaw + CurrentYawSpeed * DeltaTime;

	// Preserve the pitch and roll while updating yaw
	FRotator NewRotation = FRotator(CurrentRotation.Pitch, NewYaw, CurrentRotation.Roll);
	GetOwner()->SetActorRotation(NewRotation);
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

void UHelicopterMoverComponent::ApplyBodyTilt(float DeltaTime)
{
	if (!GetOwner()) return;

	// Get the helicopter's body mesh (if it exists)
	UStaticMeshComponent* HelicopterBody = Cast<UStaticMeshComponent>(GetOwner()->GetRootComponent());
	if (!HelicopterBody) return;

	FVector ForwardVector = GetOwner()->GetActorForwardVector();
	FVector RightVector = GetOwner()->GetActorRightVector();

	// Calculate tilt angles (pitch and roll) based on velocity
	float TargetPitch = FVector::DotProduct(CurrentVelocity, ForwardVector) / MaxForwardSpeed * -MaxTiltAngle;
	float TargetRoll = FVector::DotProduct(CurrentVelocity, RightVector) / MaxLateralSpeed * MaxTiltAngle;

	// Clamp tilt angles
	TargetPitch = FMath::Clamp(TargetPitch, -MaxTiltAngle, MaxTiltAngle);
	TargetRoll = FMath::Clamp(TargetRoll, -MaxTiltAngle, MaxTiltAngle);

	// Get the current relative rotation
	FRotator CurrentRotation = HelicopterBody->GetRelativeRotation();

	// Calculate target rotation
	FRotator TargetRotation = FRotator(TargetPitch, CurrentRotation.Yaw, TargetRoll);

	// Smoothly interpolate to the target rotation
	FRotator SmoothedRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, TiltSmoothingSpeed);

	// Apply the smoothed tilt to the helicopter body
	HelicopterBody->SetRelativeRotation(SmoothedRotation);

	// Debugging
	//UE_LOG(LogTemp, Log, TEXT("Tilt - Pitch: %f, Roll: %f"), TargetPitch, TargetRoll);
}

void UHelicopterMoverComponent::CorrectClientState()
{
	// Interpolate to the server's authoritative state
	FVector CurrentPosition = GetOwner()->GetActorLocation();
	FRotator CurrentRotation = GetOwner()->GetActorRotation();

	//FVector InterpolatedPosition = FMath::VInterpTo(CurrentPosition, ServerState.Position, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);
	//FRotator InterpolatedRotation = FMath::RInterpTo(CurrentRotation, ServerState.Rotation, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);

	//GetOwner()->SetActorLocation(InterpolatedPosition);
	//GetOwner()->SetActorRotation(InterpolatedRotation);

	// Correct position if the error is significant
	if (!CurrentPosition.Equals(ServerState.Position, PositionErrorThreshold))
	{
		FVector InterpolatedPosition = FMath::VInterpTo(CurrentPosition, ServerState.Position, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);
		GetOwner()->SetActorLocation(InterpolatedPosition);
	}

	
	// Correct rotation if the error is significant
	if (!CurrentRotation.Equals(ServerState.Rotation, RotationErrorThreshold))
	{
		FRotator InterpolatedRotation = FMath::RInterpTo(CurrentRotation, ServerState.Rotation, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);
		GetOwner()->SetActorRotation(InterpolatedRotation);
	}
	

	/*
	// Correct yaw only, ignore pitch and roll
	if (FMath::Abs(CurrentRotation.Yaw - ServerState.Rotation.Yaw) > RotationErrorThreshold)
	{
		FRotator TargetRotation = FRotator(CurrentRotation.Pitch, ServerState.Rotation.Yaw, CurrentRotation.Roll);
		FRotator InterpolatedRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);
		GetOwner()->SetActorRotation(InterpolatedRotation);
	}
	*/

	/*
	// Only correct velocity if there's a large discrepancy
	if (!CurrentVelocity.Equals(ServerState.Velocity, PositionErrorThreshold))
	{
		CurrentVelocity = FMath::VInterpTo(CurrentVelocity, ServerState.Velocity, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);
	}
	*/

}
