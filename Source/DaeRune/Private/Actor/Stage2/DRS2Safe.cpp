// Copyright DaeRune

#include "Actor/Stage2/DRS2Safe.h"

#include "Actor/DRCleanserPart.h"
#include "Actor/Stage2/DRS2CodeScreen.h"
#include "Actor/Stage2/DRS2InteractProp.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

ADRS2Safe::ADRS2Safe()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SafeBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SafeBodyMesh"));
	SafeBodyMesh->SetupAttachment(SceneRoot);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(SafeBodyMesh);

	PartSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PartSpawnPoint"));
	PartSpawnPoint->SetupAttachment(SceneRoot);
}

void ADRS2Safe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2Safe, InputDigits);
	DOREPLIFETIME(ADRS2Safe, bOpened);
}

void ADRS2Safe::BeginPlay()
{
	Super::BeginPlay();

	EnsureDisplayMIDs();
	RefreshDisplay();
}

void ADRS2Safe::EnsureDisplayMIDs()
{
	DisplayMIDs.Reset();

	if (DigitMaterialSlots.Num() == 0) return;   // 슬롯 방식을 쓰지 않는 설정

	UStaticMeshComponent* TargetMesh = bDisplayOnDoorMesh ? DoorMesh : SafeBodyMesh;
	if (!TargetMesh || !TargetMesh->GetStaticMesh())
	{
		UE_LOG(LogDR, Error, TEXT("[S2Safe] 표시용 메시(%s)에 메시가 지정되지 않았습니다."),
			bDisplayOnDoorMesh ? TEXT("DoorMesh") : TEXT("SafeBodyMesh"));
		return;
	}

	const int32 SlotTotal = TargetMesh->GetNumMaterials();

	for (int32 Slot : DigitMaterialSlots)
	{
		if (Slot < 0 || Slot >= SlotTotal)
		{
			UE_LOG(LogDR, Error, TEXT("[S2Safe] 머티리얼 슬롯 %d 이 범위(0~%d) 밖입니다."), Slot, SlotTotal - 1);
			DisplayMIDs.Add(nullptr);
			continue;
		}

		UMaterialInstanceDynamic* MID = TargetMesh->CreateAndSetMaterialInstanceDynamic(Slot);
		if (!MID)
		{
			UE_LOG(LogDR, Error, TEXT("[S2Safe] 슬롯 %d 의 MID 생성 실패. 그 슬롯에 머티리얼이 있는지 확인하세요."), Slot);
		}

		DisplayMIDs.Add(MID);
	}

	if (DigitTextures.Num() < 10)
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Safe] DigitTextures 가 %d개뿐입니다. 0~9 를 채우세요."), DigitTextures.Num());
	}
}

UTexture2D* ADRS2Safe::DigitToTexture(int32 Digit) const
{
	if (!DigitTextures.IsValidIndex(Digit))
	{
		return EmptySlotTexture.Get();
	}

	UTexture2D* Texture = DigitTextures[Digit].Get();
	return Texture ? Texture : EmptySlotTexture.Get();
}

void ADRS2Safe::ApplyDigitTextures(const TArray<int32>& DisplayDigits)
{
	for (int32 SlotIndex = 0; SlotIndex < DisplayMIDs.Num(); ++SlotIndex)
	{
		UMaterialInstanceDynamic* MID = DisplayMIDs[SlotIndex];
		if (!MID) continue;

		const int32 Digit = DisplayDigits.IsValidIndex(SlotIndex) ? DisplayDigits[SlotIndex] : INDEX_NONE;
		MID->SetTextureParameterValue(TextureParameterName, DigitToTexture(Digit));
	}
}

void ADRS2Safe::RegisterButton(ADRS2SafeButton* Button)
{
	if (!IsValid(Button)) return;

	RegisteredButtons.AddUnique(Button);

	// ★문에 부착해 개방 연출 때 버튼이 함께 회전하게 한다.
	//   부착은 서버·클라 각자 수행한다 (이동 복제를 끈 상태라 자동 동기화되지 않는다).
	if (Button->ShouldAttachToDoor() && DoorMesh)
	{
		Button->AttachToComponent(DoorMesh, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void ADRS2Safe::SetSecretCode(const TArray<uint8>& InCode)
{
	if (!HasAuthority()) return;

	SecretCode = InCode;
	CodeLength = SecretCode.Num();

	InputDigits.Reset();
	OnRep_InputDigits();
}

void ADRS2Safe::PushDigit(uint8 Digit)
{
	if (!HasAuthority() || bOpened) return;

	// 자릿수가 가득 차면 더 받지 않는다.
	// Delete 로 지우거나 Reset 으로 비운 뒤 다시 입력해야 한다.
	if (InputDigits.Num() >= CodeLength) return;

	InputDigits.Add(Digit);
	OnRep_InputDigits();

	// ★여기서 자동 판정하지 않는다. 판정은 Enter(SubmitCode) 에서만 일어난다.
}

void ADRS2Safe::DeleteLastDigit()
{
	if (!HasAuthority() || bOpened) return;
	if (InputDigits.Num() == 0) return;

	InputDigits.Pop();
	OnRep_InputDigits();
}

void ADRS2Safe::ClearInput()
{
	if (!HasAuthority() || bOpened) return;

	InputDigits.Reset();
	OnRep_InputDigits();
}

void ADRS2Safe::SubmitCode()
{
	if (!HasAuthority() || bOpened) return;

	// 자릿수가 모자라도 그대로 판정한다 (어차피 불일치 -> 오답 처리).
	ValidateInput();
}

void ADRS2Safe::ValidateInput()
{
	if (SecretCode.Num() == 0)
	{
		UE_LOG(LogDR, Error, TEXT("[S2Safe] 정답 코드가 주입되지 않았습니다. 페이즈 배선을 확인하세요."));
		InputDigits.Reset();
		OnRep_InputDigits();
		return;
	}

	const bool bMatched = (InputDigits == SecretCode);

	if (bMatched)
	{
		OpenSafe();
	}
	else
	{
		// 오답은 입력만 초기화한다. 페널티나 잠금 시간은 두지 않는다.
		InputDigits.Reset();
		OnRep_InputDigits();
		Multicast_PlayWrongCodeFX();

		UE_LOG(LogDR, Verbose, TEXT("[S2Safe] 오답 - 입력 초기화"));
	}
}

void ADRS2Safe::OpenSafe()
{
	if (bOpened) return;

	bOpened = true;
	OnRep_bOpened();

	// ★개방 후에는 버튼을 잠근다. 그러지 않으면 F 프롬프트가 계속 뜨고
	//   문에 붙어 회전한 버튼을 계속 누를 수 있다 (입력은 무시되지만 혼란스럽다).
	for (const TObjectPtr<ADRS2SafeButton>& Button : RegisteredButtons)
	{
		if (IsValid(Button))
		{
			Button->SetPropEnabled(false);
		}
	}

	// 내부 부품 스폰 -> 이후는 기존 픽업 파이프라인을 그대로 탄다
	AActor* SpawnedPart = nullptr;
	if (PartClass && PartSpawnPoint)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		SpawnedPart = GetWorld()->SpawnActor<ADRCleanserPart>(
			PartClass, PartSpawnPoint->GetComponentTransform(), SpawnParams);
	}
	else
	{
		UE_LOG(LogDR, Error, TEXT("[S2Safe] PartClass 가 지정되지 않아 부품을 스폰하지 못했습니다."));
	}

	UE_LOG(LogDR, Log, TEXT("[S2Safe] 금고 개방"));

	OnSafeOpened.Broadcast(SpawnedPart);
}

void ADRS2Safe::OnRep_InputDigits()
{
	RefreshDisplay();
}

void ADRS2Safe::OnRep_bOpened()
{
	if (bOpened)
	{
		OnSafeOpenedVisual();
	}
}

void ADRS2Safe::RefreshDisplay()
{
	// 입력된 자리는 숫자로, 남은 자리는 -1(미입력)로 표시한다
	TArray<int32> DisplayDigits;
	DisplayDigits.Reserve(CodeLength);

	for (int32 i = 0; i < CodeLength; ++i)
	{
		DisplayDigits.Add(InputDigits.IsValidIndex(i) ? static_cast<int32>(InputDigits[i]) : -1);
	}

	// 금고 메시에 직접 표시
	ApplyDigitTextures(DisplayDigits);

	// 별도 표시판 액터를 쓰는 경우에도 전달한다 (선택)
	if (InputDisplay)
	{
		InputDisplay->SetDigits(DisplayDigits);
	}
}

void ADRS2Safe::Multicast_PlayWrongCodeFX_Implementation()
{
	OnWrongCodeVisual();
}
