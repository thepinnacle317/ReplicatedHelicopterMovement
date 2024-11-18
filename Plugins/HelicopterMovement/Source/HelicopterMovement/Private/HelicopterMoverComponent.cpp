// Fill out your copyright notice in the Description page of Project Settings.


#include "HelicopterMoverComponent.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"

UHelicopterMoverComponent::UHelicopterMoverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Initialize defaults
	DesiredInput = FVector::ZeroVector;
	DesiredYawInput = 0.0f;
	CurrentVelocity = FVector::ZeroVector;
	CurrentYawSpeed = 0.0f;

	MaxForwardSpeed = 1500.0f;
	MaxLateralSpeed = 1000.0f;
	MaxVerticalSpeed = 500.0f;
	YawSpeed = 90.0f;

	VelocityDamping = 0.95f;
	TiltSmoothingSpeed = 5.0f;
	VelocitySmoothingSpeed = 5.0f;

	// Units 
	PositionThreshold = 10.0f;
	// Degrees
	RotationThreshold = 5.0f;  

	SetIsReplicatedByDefault(true);
}

void UHelicopterMoverComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UHelicopterMoverComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner()->HasAuthority())
	{
		// Update authoritative state
		UpdateThrottle(DeltaTime); // Ensure vertical movement is applied
		UpdateMovement(DeltaTime); // Ensure horizontal movement is applied
		UpdateYaw(DeltaTime);      // Ensure yaw is applied

		ServerState.Position = GetOwner()->GetActorLocation();
		ServerState.Rotation = GetOwner()->GetActorRotation();
		ServerState.Velocity = CurrentVelocity;
		
		//UE_LOG(LogTemp, Log, TEXT("ServerState Updated - Position: %s, Velocity: %s"),
		//	   *ServerState.Position.ToString(), *ServerState.Velocity.ToString());
	}
	else
	{
		// Client prediction
		SimulateMovement(DeltaTime);
		RewindAndReconcile();
	}

	// Smooth interpolation
	//OnRep_ServerState();
}

void UHelicopterMoverComponent::OnRep_ServerState()
{
	FVector CurrentPosition = GetOwner()->GetActorLocation();
	FRotator CurrentRotation = GetOwner()->GetActorRotation();

	FVector InterpolatedPosition = FMath::VInterpTo(CurrentPosition, ServerState.Position, GetWorld()->DeltaTimeSeconds, 15.0f);
	FRotator InterpolatedRotation = FMath::RInterpTo(CurrentRotation, ServerState.Rotation, GetWorld()->DeltaTimeSeconds, 15.0f);

	GetOwner()->SetActorLocation(InterpolatedPosition);
	GetOwner()->SetActorRotation(InterpolatedRotation);

	//UE_LOG(LogTemp, Log, TEXT("Interpolating to ServerState Position: %s, Rotation: %s"), *ServerState.Position.ToString(), *ServerState.Rotation.ToString());
}

void UHelicopterMoverComponent::SimulateMovement(float DeltaTime)
{
	if (!GetOwner()) return;

	// Save the predicted state
	FHelicopterState PredictedState;
	PredictedState.Position = GetOwner()->GetActorLocation();
	PredictedState.Rotation = GetOwner()->GetActorRotation();
	PredictedState.Velocity = CurrentVelocity;
	PredictedStates.Add(PredictedState);

	// Simulate movement as on the server
	UpdateThrottle(DeltaTime);
	UpdateMovement(DeltaTime);
	UpdateYaw(DeltaTime);
	UpdateTilt(DeltaTime);
	
	//UE_LOG(LogTemp, Log, TEXT("Simulated Movement - Position: %s, Velocity: %s"),
	//	   *GetOwner()->GetActorLocation().ToString(), *CurrentVelocity.ToString());
}

/* This function is designed to both rewind the client state for validation and smooth discrepancies between the client and server */
void UHelicopterMoverComponent::RewindAndReconcile()
{
	if (PredictedStates.Num() == 0) return;

	FHelicopterState LastPredictedState = PredictedStates[0];
	PredictedStates.RemoveAt(0);

	// Allow a small threshold to avoid unnecessary corrections
	bool bPositionMismatch = !LastPredictedState.Position.Equals(ServerState.Position, PositionThreshold);
	bool bRotationMismatch = !LastPredictedState.Rotation.Equals(ServerState.Rotation, RotationThreshold);

	if (bPositionMismatch || bRotationMismatch)
	{
		//FRotator SmoothedRotation = FMath::RInterpTo(GetOwner()->GetActorRotation(), ServerState.Rotation, GetWorld()->DeltaTimeSeconds, TiltSmoothingSpeed);
		CurrentVelocity = ServerState.Velocity;
		
		SmoothVelocity(GetOwner()->GetActorLocation(), ServerState.Position,VelocityDamping,  GetWorld()->DeltaTimeSeconds);
		SmoothVelocity(GetOwner()->GetVelocity(), CurrentVelocity, VelocityDamping,  GetWorld()->DeltaTimeSeconds);
		GetOwner()->SetActorRotation(ServerState.Rotation);
	}
}

void UHelicopterMoverComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHelicopterMoverComponent, ServerState);
}

void UHelicopterMoverComponent::SendInputToServer_Implementation(FVector InputVector, float YawInput)
{
	// Update server-side input replication
	DesiredInput = InputVector;
	DesiredYawInput = YawInput;
}

bool UHelicopterMoverComponent::SendInputToServer_Validate(FVector InputVector, float YawInput)
{
	// TODO:Optionally add input validation here ** No current use yet and will add a blueprint method upon movement polish
	return true;
}

void UHelicopterMoverComponent::UpdateThrottle(float DeltaTime)
{
	if (!GetOwner()) return;

	// Check for vertical input and calculate target velocity
	float TargetVerticalVelocity = FMath::IsNearlyZero(DesiredInput.Z)
		? 0.0f  // No input, hold altitude 
		: DesiredInput.Z * MaxVerticalSpeed;

	// Smoothly interpolate to the target vertical velocity
	CurrentVelocity.Z = FMath::FInterpTo(CurrentVelocity.Z, TargetVerticalVelocity, DeltaTime, VelocitySmoothingSpeed);

	// Apply the vertical velocity to move the helicopter
	FVector NewPosition = GetOwner()->GetActorLocation() + FVector(0.0f, 0.0f, CurrentVelocity.Z * DeltaTime);
	GetOwner()->SetActorLocation(NewPosition, true);
	
	//UE_LOG(LogTemp, Log, TEXT("Vertical Movement: Desired Z: %f, Current Velocity Z: %f"), DesiredInput.Z, CurrentVelocity.Z);
}

void UHelicopterMoverComponent::UpdateMovement(float DeltaTime)
{
	if (!GetOwner()) return;

	FVector ForwardVector = GetOwner()->GetActorForwardVector();
	FVector RightVector = GetOwner()->GetActorRightVector();

	FVector TargetVelocity =
		ForwardVector * DesiredInput.X * MaxForwardSpeed +
		RightVector * DesiredInput.Y * MaxLateralSpeed;

	CurrentVelocity = SmoothVelocity(CurrentVelocity, TargetVelocity, VelocityDamping, DeltaTime);

	FVector NewPosition = GetOwner()->GetActorLocation() + CurrentVelocity * DeltaTime;

	FHitResult Hit;
	bool bMoved = GetOwner()->SetActorLocation(NewPosition, true, &Hit);

	if (!bMoved && Hit.IsValidBlockingHit())
	{
	//	UE_LOG(LogTemp, Warning, TEXT("Blocked by: %s at location: %s"), *Hit.GetActor()->GetName(), *Hit.ImpactPoint.ToString());
	}
}

void UHelicopterMoverComponent::UpdateYaw(float DeltaTime)
{
	if (!GetOwner()) return;

	// Calculate yaw change based on input
	float TargetYawSpeed = DesiredYawInput * YawSpeed;

	// Smoothly interpolate yaw speed
	CurrentYawSpeed = FMath::FInterpTo(CurrentYawSpeed, TargetYawSpeed, DeltaTime, TiltSmoothingSpeed);

	// Apply yaw change
	FRotator CurrentRotation = GetOwner()->GetActorRotation();
	float NewYaw = CurrentRotation.Yaw + CurrentYawSpeed * DeltaTime;
	GetOwner()->SetActorRotation(FRotator(CurrentRotation.Pitch, NewYaw, CurrentRotation.Roll));
}

void UHelicopterMoverComponent::UpdateTilt(float DeltaTime)
{
	FVector ForwardVector = GetOwner()->GetActorForwardVector();
	FVector RightVector = GetOwner()->GetActorRightVector();

	float TargetPitch = FVector::DotProduct(CurrentVelocity, ForwardVector) / MaxForwardSpeed * -MaxTiltAngle;
	float TargetRoll = FVector::DotProduct(CurrentVelocity, RightVector) / MaxLateralSpeed * MaxTiltAngle;

	FRotator CurrentRotation = GetOwner()->GetActorRotation();
	FRotator TargetRotation = FRotator(TargetPitch, CurrentRotation.Yaw, TargetRoll);

	FRotator SmoothedRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, TiltSmoothingSpeed);
	GetOwner()->SetActorRotation(SmoothedRotation);
}

FVector UHelicopterMoverComponent::SmoothVelocity(FVector Current, FVector Target, float Damping, float DeltaTime)
{
	return FMath::Lerp(Current, Target, 1.0f - FMath::Pow(Damping, DeltaTime));
}
