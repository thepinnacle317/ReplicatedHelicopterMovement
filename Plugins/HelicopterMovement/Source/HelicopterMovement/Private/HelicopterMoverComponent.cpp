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

	BounceDampingFactor = 0.7f; 
	ImpactOffset = 5.0f;
	CollisionSphere = 150;
	SurfaceFriction = 0.9f;
	SkidVelocityThreshold = 400.0f;

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

	// Perform collision-aware movement
	FHitResult HitResult;
	FVector Start = GetOwner()->GetActorLocation();
	FVector End = Start + CurrentVelocity * DeltaTime;

	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(CollisionSphere);

	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		Start,
		End,
		FQuat::Identity,
		ECC_WorldStatic,
		CollisionShape,
		FCollisionQueryParams(FName(TEXT("HelicopterSweep")), true, GetOwner())
	);

	if (bHit && HitResult.IsValidBlockingHit())
	{
		HandleCollision(HitResult, DeltaTime);
	}
	else
	{
		// No collision, move normally
		GetOwner()->SetActorLocation(End, true);
	}
	
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
	if (PredictedStates.Num() == 0)	return;

	FHelicopterState LastPredictedState = PredictedStates[0];
	PredictedStates.RemoveAt(0);

	FHitResult HitResult;
	FVector Start = GetOwner()->GetActorLocation();
	FVector End = ServerState.Position;
	
	// Sweep to ensure collision compliance
	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(CollisionSphere); // Adjust size as needed
	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		Start,
		End,
		FQuat::Identity,
		ECC_WorldStatic,
		CollisionShape,
		FCollisionQueryParams(FName(TEXT("ReconcileSweep")), true, GetOwner())
	);

	if (bHit && HitResult.IsValidBlockingHit())
	{
		HandleCollision(HitResult, GetWorld()->DeltaTimeSeconds);
	}
	else
	{
		// Check for mismatches
		if (!LastPredictedState.Position.Equals(ServerState.Position, PositionErrorThreshold))
		{
			// Correct position
			GetOwner()->SetActorLocation(ServerState.Position, true);
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
}

void UHelicopterMoverComponent::UpdateServerState()
{
	ServerState.Position = GetOwner()->GetActorLocation();
	ServerState.Rotation = GetOwner()->GetActorRotation();
	ServerState.Velocity = CurrentVelocity;
	ServerState.Timestamp = GetWorld()->GetTimeSeconds();
}

void UHelicopterMoverComponent::HandleCollision(const FHitResult& HitResult, float DeltaTime)
{
	FVector ImpactNormal = HitResult.ImpactNormal;

	// Calculate the speed of the helicopter
	float Speed = CurrentVelocity.Size();

	// Check the threshold to decide if it is a skip or a skid
	if (Speed > SkidVelocityThreshold) 
	{
		// Reflect the velocity off the impact surface
		FVector ReflectedVelocity = FMath::GetReflectionVector(CurrentVelocity, ImpactNormal) * BounceDampingFactor;

		// Ensure upward movement after reflection
		if (ReflectedVelocity.Z < 0.0f)
		{
			ReflectedVelocity.Z = FMath::Abs(ReflectedVelocity.Z);
		}

		// Adjust position slightly above the surface
		FVector ImpactOffsetVec = ImpactNormal * ImpactOffset; // Prevent sinking
		FVector NewPosition = HitResult.ImpactPoint + ImpactOffsetVec;

		CurrentVelocity = ReflectedVelocity;
		GetOwner()->SetActorLocation(NewPosition, true);

		//UE_LOG(LogTemp, Log, TEXT("Helicopter skipped off surface. New Position: %s, New Velocity: %s"),
		//	   *NewPosition.ToString(), *CurrentVelocity.ToString());
	}
	else
	{
		// Project the velocity onto the plane defined by the impact normal to simulate sliding
		FVector SlidingVelocity = FVector::VectorPlaneProject(CurrentVelocity, ImpactNormal) * SurfaceFriction;

		// Adjust position to prevent sinking
		FVector ImpactOffsetVec = ImpactNormal * ImpactOffset;
		FVector NewPosition = HitResult.ImpactPoint + ImpactOffsetVec;

		CurrentVelocity = SlidingVelocity;
		GetOwner()->SetActorLocation(NewPosition, true);

		UE_LOG(LogTemp, Log, TEXT("Helicopter sliding along surface. New Position: %s, New Velocity: %s"),
			   *NewPosition.ToString(), *CurrentVelocity.ToString());
	}
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

	// Correct position if the error is significant
	if (!CurrentPosition.Equals(ServerState.Position, PositionErrorThreshold))
	{
		FHitResult HitResult;
		FVector End = ServerState.Position;

		FCollisionShape CollisionShape = FCollisionShape::MakeSphere(CollisionSphere); // Adjust as necessary
		bool bHit = GetWorld()->SweepSingleByChannel(
			HitResult,
			CurrentPosition,
			End,
			FQuat::Identity,
			ECC_WorldStatic,
			CollisionShape,
			FCollisionQueryParams(FName(TEXT("CorrectSweep")), true, GetOwner())
		);

		if (bHit && HitResult.IsValidBlockingHit())
		{
			HandleCollision(HitResult, GetWorld()->DeltaTimeSeconds);
		}
		else
		{
			// Smooth position correction
			FVector InterpolatedPosition = FMath::VInterpTo(CurrentPosition, ServerState.Position, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);
			GetOwner()->SetActorLocation(InterpolatedPosition, true);
		}
	}
	
	// Correct rotation if the error is significant
	if (!CurrentRotation.Equals(ServerState.Rotation, RotationErrorThreshold))
	{
		FRotator InterpolatedRotation = FMath::RInterpTo(CurrentRotation, ServerState.Rotation, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);
		GetOwner()->SetActorRotation(InterpolatedRotation);
	}

	/*
	// Only correct velocity if there's a large discrepancy
	if (!CurrentVelocity.Equals(ServerState.Velocity, PositionErrorThreshold))
	{
		CurrentVelocity = FMath::VInterpTo(CurrentVelocity, ServerState.Velocity, GetWorld()->DeltaTimeSeconds, CorrectionInterpolation);
	}
	*/

}
