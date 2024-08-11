// Copyright Epic Games, Inc. All Rights Reserved.

#include "PracticeProjectCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "CableComponent.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// APracticeProjectCharacter

APracticeProjectCharacter::APracticeProjectCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	ThirdPersonCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm
	ThirdPersonCamera->bAutoActivate = false;

	// Create a CameraComponent	
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	//FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetupAttachment(GetMesh(), "head");
	FirstPersonCamera->SetRelativeLocation(FVector(5.f, 0.f, 0.f)); // Position the first person camera
	FirstPersonCamera->SetRelativeRotation(FRotator(0.f, 90.f, -90.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// Create grappling hook cable
	GrappleCable = CreateDefaultSubobject<UCableComponent>(TEXT("GrapplingHookCable"));
	GrappleCable->SetupAttachment(FirstPersonCamera);
	GrappleCable->SetHiddenInGame(true);

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void APracticeProjectCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();
}

void APracticeProjectCharacter::Tick(float DeltaSeconds)
{
	// Call the base class
	Super::Tick(DeltaSeconds);

	if (bGrappleAttached)
	{
		UpdateGrapple();
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void APracticeProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APracticeProjectCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APracticeProjectCharacter::Look);

		// Camera Toggle
		EnhancedInputComponent->BindAction(CameraToggleAction, ETriggerEvent::Completed, this, &APracticeProjectCharacter::SwitchCamera);

		// Crouch
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &ACharacter::Crouch, false);
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ACharacter::UnCrouch, false);

		// Grapple
		EnhancedInputComponent->BindAction(GrappleAction, ETriggerEvent::Completed, this, &APracticeProjectCharacter::CheckGrapple);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void APracticeProjectCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void APracticeProjectCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void APracticeProjectCharacter::SwitchCamera()
{
	if (bThirdPersonCameraEnabled)
	{
		FirstPersonCamera->SetActive(true);
		ThirdPersonCamera->SetActive(false);
		bUseControllerRotationYaw = true;
	}
	else
	{
		ThirdPersonCamera->SetActive(true);
		FirstPersonCamera->SetActive(false);
		bUseControllerRotationYaw = false;
	}
	bThirdPersonCameraEnabled = !bThirdPersonCameraEnabled;
}

void APracticeProjectCharacter::CheckGrapple()
{
	if (!bGrappleAttached)
	{
		ConnectGrapplingHook();
	}
	else
	{
		DisconnectGrapplingHook();
	}
}

// Function that adds a force when the grappling hook is connected to propel the player towards the connection point
void APracticeProjectCharacter::UpdateGrapple()
{
	// Adjust grapple cable end point
	GrappleCable->EndLocation = GetActorTransform().InverseTransformPosition(vGrappleLocation);
	
	// Add Force
	FVector force, direction;
	double magnitude;
	//direction = vGrappleLocation - GetActorLocation();
	direction = GetActorLocation() - vGrappleLocation;
	//DrawDebugLine(GetWorld(), GetActorLocation(), direction * -2, FColor::Black);
	FVector velocity = GetCharacterMovement()->Velocity;
	magnitude = FVector::DotProduct(direction, velocity);
	magnitude = UKismetMathLibrary::FClamp(magnitude, -magnitudeClamp, magnitude);
	//magnitude = direction.Dot(GetCharacterMovement()->Velocity);
	force = (magnitude > .5 || magnitude < -0.5) ? direction.GetSafeNormal() * magnitude : direction.GetSafeNormal();
	//DrawDebugLine(GetWorld(), GetActorLocation(), vGrappleLocation, FColor::Magenta);
	
	// force in the direction of the grapple location
	GetCharacterMovement()->AddForce(force * 2);
	//GetCharacterMovement()->AddForce(direction.GetSafeNormal() * 1000);
	//GetCharacterMovement()->Launch(direction.GetSafeNormal() * 10);

	// force in the direction the character is facing for smoothness
	//GetCharacterMovement()->AddForce(FirstPersonCamera->GetForwardVector() * 1000);
		
	//GetCharacterMovement()->AddForce((vGrappleLocation - GetActorLocation()).GetSafeNormal() * 10000);
}

void APracticeProjectCharacter::ConnectGrapplingHook()
{
	// Find the point to attempt to grapple
	FVector start = FirstPersonCamera->GetComponentLocation();
	FVector end = start + (FirstPersonCamera->GetForwardVector() * fGrappleDistance);
	
	TArray<AActor*> ignoreActors;
	TArray<TEnumAsByte<EObjectTypeQuery>> objectTypes;
	objectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	FHitResult hit;

	bool hasHit = UKismetSystemLibrary::SphereTraceSingleForObjects(this, start, end, 5.f, objectTypes, false, ignoreActors,
		EDrawDebugTrace::ForOneFrame, hit, true, FLinearColor::Blue, FLinearColor::Red);
	if (hasHit)
	{
		// Set that point as Grapple Location
		vGrappleLocation = hit.ImpactPoint;
		bGrappleAttached = true;
		//GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		GrappleCable->SetHiddenInGame(false);
	}
}


void APracticeProjectCharacter::DisconnectGrapplingHook()
{
	bGrappleAttached = false;
	GrappleCable->SetHiddenInGame(true);

	if (GetCharacterMovement()->MovementMode != MOVE_Falling)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
}