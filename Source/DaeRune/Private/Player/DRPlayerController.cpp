// Copyright DaeRune


#include "Player/DRPlayerController.h"
#include "Player/DRInteractionComponent.h"
#include "Player/DRCheatManager.h"
#include "UI/Widget/DRWardrobeScreenWidget.h"
#include "UI/Widget/DRUpgradeScreenWidget.h"
#include "UI/Widget/DRStageSelectScreenWidget.h"
#include "Game/DRProgressionConfig.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DRGameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "Input/DRInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UI/Widget/DamageTextComponent.h"
#include "Actor/DRCleanserPart.h"
#include "Actor/DRCleanserSite.h"
#include "Actor/Stage2/DRS2InteractProp.h"
#include "Actor/Stage2/DRS2SlidePuzzle.h"
#include "UI/Widget/Stage2/DRS2SlidePuzzleWidget.h"
#include "Actor/Stage2/DRS2TrainCar.h"
#include "Actor/Stage2/DRS2Train.h"
#include "Interaction/DRInteractable.h"
#include "Actor/DRWaitingRoomCameraActor.h"
#include "Tutorial/DRTutorialManager.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "Character/DRRobotVacuumCharacter.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Camera/CameraComponent.h"
#include "Game/DRStageGameMode.h"
#include "Phase/DRPhase3.h"
#include "Game/DRStageGameState.h"
#include "Game/DRLobbyGameState.h"
#include "UI/HUD/DRHUD.h"
#include "UI/Widget/DRUserWidget.h"
#include "Player/DRPlayerState.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "Game/DRSettingsManager.h"
#include "Game/DRGameUserSettings.h"
#include "Actor/DRBGMActor.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Widget/DRWaitingRoomWidget.h"
#include "UI/DRUpgradeUILibrary.h"
#include "Game/DRLobbyGameMode.h"
#include "Game/DRGameInstance.h"
#include "Game/DRCosmeticCatalog.h"
#include "Game/DRChipCatalog.h"
#include "DaeRune/DRLogChannels.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Tuning/DRTuningSubsystem.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/WorldSettings.h"
#include "UI/Loading/DRLoadingScreenSubsystem.h"

namespace
{
	// ========== 시작 연출 경로 (출발 시점 → 1인칭 눈, 로비 Power On · 스테이지 시작 공용) ==========
	//
	// 지나는 키: 출발 · 접근 중간 · 얼굴 앞 · 어깨 위 · 눈 위 · 눈.
	// 출발은 로비면 대기실 카메라, 스테이지면 캐릭터가 바라보는 방향 멀리 — 둘 다 캐릭터 앞에서 캐릭터를 본다.
	// 위치는 구심 Catmull-Rom 으로, 보는 방향은 키마다 정한 방향을 Hermite 로 잇는다.
	//  - 카메라는 캐릭터를 마주 보다가 1인칭(캐릭터가 보는 쪽)으로 들어가야 해서 180도 돌아야 한다.
	//    로비 슬롯 간격(~170)이 좁아 옆으로 돌면 벨보이(반지름 85) 같은 이웃과 부딪히므로 어깨 위로 넘는다.
	//  - ★보는 방향을 매 프레임 "캐릭터 바라보기"로 구하면 안 된다★ — 머리 가까이를 지날 때 초당 800도 넘게
	//    휙 돌았다 (2026-11-08 시뮬레이션). 키에서만 캐릭터를 보고 사이는 부드럽게 잇는다.
	//  - 진행은 (이동 거리 + 회전량 × DegreeCost) 기준으로 고르게 나눈다 — 많이 도는 어깨 위에서는 그만큼 천천히 간다.
	namespace IntroPath
	{
		constexpr int32 NumKeys = 6;
		constexpr int32 NumSegments = NumKeys - 1;
		constexpr int32 SamplesPerSegment = 24;
		constexpr int32 NumSamples = NumSegments * SamplesPerSegment + 1;

		// 회전 1도를 이동 몇 cm 로 칠지 — 클수록 많이 도는 구간을 천천히 지난다
		constexpr double DegreeCost = 6.0;

		enum EKey : int32 { Start, Approach, Front, Shoulder, AboveEye, Eye };

		struct FPath
		{
			FVector Points[NumKeys + 2];		// 앞뒤 유령점 포함 (양 끝 접선용)
			FVector Dirs[NumKeys];				// 키에서 보는 방향
			FVector DirTangents[NumKeys];
		};

		// 구심(alpha 0.5) Catmull-Rom 의 P1→P2 구간 — 키 간격이 들쭉날쭉해도 고리 · 뾰족점이 생기지 않는다
		FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, double T)
		{
			auto NextKnot = [](double Ti, const FVector& A, const FVector& B)
			{
				return Ti + FMath::Max(FMath::Sqrt(FVector::Dist(A, B)), 0.01);
			};
			const double T0 = 0.0;
			const double T1 = NextKnot(T0, P0, P1);
			const double T2 = NextKnot(T1, P1, P2);
			const double T3 = NextKnot(T2, P2, P3);
			const double TT = FMath::Lerp(T1, T2, T);

			const FVector A1 = ((T1 - TT) * P0 + (TT - T0) * P1) / (T1 - T0);
			const FVector A2 = ((T2 - TT) * P1 + (TT - T1) * P2) / (T2 - T1);
			const FVector A3 = ((T3 - TT) * P2 + (TT - T2) * P3) / (T3 - T2);
			const FVector B1 = ((T2 - TT) * A1 + (TT - T0) * A2) / (T2 - T0);
			const FVector B2 = ((T3 - TT) * A2 + (TT - T1) * A3) / (T3 - T1);
			return ((T2 - TT) * B1 + (TT - T1) * B2) / (T2 - T1);
		}

		// 가운데 키에서 바라보는 곳 — 캐릭터 가슴 높이
		FVector GetFocus(const ACharacter& Character)
		{
			const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
			return Capsule->GetComponentLocation() + FVector::UpVector * (0.55 * Capsule->GetScaledCapsuleHalfHeight());
		}

		FPath BuildPath(const FMinimalViewInfo& StartView, const FMinimalViewInfo& EyeView, const ACharacter& Character,
		                float OverheadClearance, float FrontDistance)
		{
			const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
			const double Radius = Capsule->GetScaledCapsuleRadius();
			const double HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			const FVector Center = Capsule->GetComponentLocation();

			// 캐릭터 기준 축 — 1인칭 카메라가 볼 방향(수평)이 정면
			const FRotator FacingYaw(0.f, EyeView.Rotation.Yaw, 0.f);
			const FVector Forward = FacingYaw.Vector();
			const FVector Right = FRotationMatrix(FacingYaw).GetUnitAxis(EAxis::Y);
			const FVector Up = FVector::UpVector;

			// 출발 시점이 있는 쪽 어깨 위로 넘는다 — 그쪽이 가장 짧게 돈다
			const double Side = FVector::DotProduct(StartView.Location - Center, Right) >= 0.0 ? 1.0 : -1.0;
			const double OverheadZ = FMath::Max(Center.Z + HalfHeight + OverheadClearance, EyeView.Location.Z);

			FVector Keys[NumKeys];
			Keys[Start] = StartView.Location;
			Keys[Front] = Center + Forward * (Radius + FrontDistance) + Right * (Side * 0.5 * Radius)
			            + Up * (HalfHeight + 0.5 * OverheadClearance);
			Keys[Approach] = FMath::Lerp(Keys[Start], Keys[Front], 0.5);
			Keys[Shoulder] = Center + Right * (Side * (0.8 * Radius + 20.0));
			Keys[Shoulder].Z = OverheadZ;
			Keys[AboveEye] = FVector(EyeView.Location.X, EyeView.Location.Y, OverheadZ);
			Keys[Eye] = EyeView.Location;

			FPath Path;
			Path.Points[0] = Keys[Start] * 2.0 - Keys[Approach];
			for (int32 i = 0; i < NumKeys; ++i)
			{
				Path.Points[i + 1] = Keys[i];
			}
			Path.Points[NumKeys + 1] = Keys[Eye] * 2.0 - Keys[AboveEye];

			// 가운데 키에서는 캐릭터 상체를 보고, 출발은 출발 시점 그대로, 눈 위 · 눈에서는 1인칭 정면을 본다
			const FVector Focus = GetFocus(Character);
			auto LookAtFocus = [&Focus](const FVector& From) { return (Focus - From).GetSafeNormal(); };
			const FVector EyeDir = EyeView.Rotation.Vector();
			Path.Dirs[Start] = StartView.Rotation.Vector();
			Path.Dirs[Approach] = LookAtFocus(Keys[Approach]);
			Path.Dirs[Front] = LookAtFocus(Keys[Front]);
			Path.Dirs[Shoulder] = LookAtFocus(Keys[Shoulder]);
			Path.Dirs[AboveEye] = EyeDir;
			Path.Dirs[Eye] = EyeDir;

			// 접선 — 출발에서는 멈춘 채 돌기 시작하고, 눈 위에서 회전을 끝낸 뒤 그대로 내려앉는다 (0). 가운데는 Catmull-Rom
			for (int32 i = 0; i < NumKeys; ++i)
			{
				Path.DirTangents[i] = (i >= Approach && i <= Shoulder) ? (Path.Dirs[i + 1] - Path.Dirs[i - 1]) * 0.5 : FVector::ZeroVector;
			}
			return Path;
		}

		void EvaluateSegment(const FPath& Path, int32 Segment, double T, FVector& OutLocation, FVector& OutDirection)
		{
			OutLocation = CatmullRom(Path.Points[Segment], Path.Points[Segment + 1], Path.Points[Segment + 2], Path.Points[Segment + 3], T);
			OutDirection = FMath::CubicInterp(Path.Dirs[Segment], Path.DirTangents[Segment],
			                                  Path.Dirs[Segment + 1], Path.DirTangents[Segment + 1], T).GetSafeNormal();
		}

		// Progress 0~1 (이동 + 회전량 기준) → 카메라 위치 · 보는 방향
		void Evaluate(const FPath& Path, double Progress, FVector& OutLocation, FVector& OutDirection)
		{
			FVector Locations[NumSamples];
			FVector Directions[NumSamples];
			double Cost[NumSamples];
			for (int32 i = 0; i < NumSamples; ++i)
			{
				const int32 Segment = FMath::Min(i / SamplesPerSegment, NumSegments - 1);
				const double T = double(i - Segment * SamplesPerSegment) / SamplesPerSegment;
				EvaluateSegment(Path, Segment, T, Locations[i], Directions[i]);
				if (i == 0)
				{
					Cost[i] = 0.0;
					continue;
				}
				if (Directions[i].IsNearlyZero())
				{
					Directions[i] = Directions[i - 1];
				}
				const double TurnDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Directions[i], Directions[i - 1]), -1.0, 1.0)));
				Cost[i] = Cost[i - 1] + FVector::Dist(Locations[i], Locations[i - 1]) + DegreeCost * TurnDegrees;
			}

			const double Target = FMath::Clamp(Progress, 0.0, 1.0) * Cost[NumSamples - 1];
			int32 Hi = 1;
			while (Hi < NumSamples - 1 && Cost[Hi] < Target)
			{
				++Hi;
			}
			const double Span = Cost[Hi] - Cost[Hi - 1];
			const double Alpha = Span > UE_KINDA_SMALL_NUMBER ? FMath::Clamp((Target - Cost[Hi - 1]) / Span, 0.0, 1.0) : 1.0;
			OutLocation = FMath::Lerp(Locations[Hi - 1], Locations[Hi], Alpha);
			OutDirection = FMath::Lerp(Directions[Hi - 1], Directions[Hi], Alpha).GetSafeNormal();
			if (OutDirection.IsNearlyZero())
			{
				OutDirection = Directions[Hi];
			}
		}

		// 카메라가 캐릭터 캡슐 안으로 들어갔는가 (몸 속이 보이기 직전)
		bool IsInsideCapsule(const FVector& CameraLocation, const ACharacter& Character)
		{
			const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
			const FVector Offset = CameraLocation - Capsule->GetComponentLocation();
			return FVector2D(Offset.X, Offset.Y).Size() < Capsule->GetScaledCapsuleRadius() + 5.0
			    && FMath::Abs(Offset.Z) < Capsule->GetScaledCapsuleHalfHeight() + 5.0;
		}

		// 3인칭 몸(메시 · 캡슐 + 머리 위 장식 여유)이 화면 아래 경계 밖으로 완전히 빠졌는가
		bool IsBodyBelowView(const FVector& CameraLocation, const FRotator& CameraRotation, float FOVDegrees, float AspectRatio, const ACharacter& Character)
		{
			FBox Body = Character.GetCapsuleComponent()->Bounds.GetBox();
			if (const USkeletalMeshComponent* Mesh = Character.GetMesh())
			{
				Body += Mesh->Bounds.GetBox();
			}
			Body = Body.ExpandBy(10.0);
			Body.Max.Z += 25.0;		// 모자 등 머리 위 장식

			// 화면 아래 경계면(카메라를 지나는 면)의 안쪽 법선 — 꼭짓점이 모두 그 바깥이면 몸 전체가 화면 아래에 있다
			const double HalfVertical = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(FOVDegrees * 0.5)) / FMath::Max(AspectRatio, 0.1f));
			const FRotationMatrix Axes(CameraRotation);
			const FVector BottomInward = Axes.GetUnitAxis(EAxis::Z) * FMath::Cos(HalfVertical) + Axes.GetUnitAxis(EAxis::X) * FMath::Sin(HalfVertical);

			FVector Corners[8];
			Body.GetVertices(Corners);
			for (const FVector& Corner : Corners)
			{
				if (FVector::DotProduct(Corner - CameraLocation, BottomInward) > 0.0)
				{
					return false;
				}
			}
			return true;
		}
	}

	// 스테이지 월드인가 — 클라는 게임스테이트가 복제되기 전에도 알아야 한다
	// (폰의 PlayerState 가 먼저 오면 InitAbilityActorInfo 가 HUD 를 띄우고, 스테이지 HUD 는 뜨자마자 로딩 화면을 닫는다).
	// 게임스테이트가 아직 없으면 맵의 게임 모드 설정(World Settings)으로 본다
	bool IsStageWorld(const UWorld& World)
	{
		if (const AGameStateBase* GameState = World.GetGameState())
		{
			return GameState->IsA<ADRStageGameState>();
		}

		const AWorldSettings* WorldSettings = World.GetWorldSettings();
		return WorldSettings && WorldSettings->DefaultGameMode && WorldSettings->DefaultGameMode->IsChildOf(ADRStageGameMode::StaticClass());
	}

	// 스테이지 시작 연출이 어깨를 넘을 쪽 (+1 = 오른쪽) — 옆에 선 다른 플레이어가 덜 가까운 쪽.
	// 이웃 쪽으로 넘으면 카메라가 그 머리를 스친다 (스폰 간격 ~140 — 2026-11-08 PIE 2인에서 60cm 옆을 지났다. 벨보이는 반지름 85)
	double ChooseIntroSide(const ACharacter& Character, const FVector& Right)
	{
		constexpr double NeighborRadius = 400.0;
		const FVector Center = Character.GetActorLocation();
		double Crowding = 0.0;	// + 면 오른쪽이 붐빈다 (가까울수록 크게 친다)
		for (TActorIterator<ADRCharacter> It(Character.GetWorld()); It; ++It)
		{
			if (*It == &Character) continue;

			const FVector Offset = It->GetActorLocation() - Center;
			const double Distance = FVector2D(Offset.X, Offset.Y).Size();
			if (Distance >= NeighborRadius) continue;

			Crowding += FMath::Sign(FVector::DotProduct(Offset, Right)) * (1.0 - Distance / NeighborRadius);
		}
		return Crowding > 0.0 ? -1.0 : 1.0;
	}

	// 보이는 메시가 모두 그릴 준비를 마쳤는가 — 패키지 빌드는 PSO 컴파일이 끝나야 메시 프록시를 만든다
	// (r.PSOPrecache.ProxyCreationWhenPSOReady, 메인 메뉴 공개와 같은 확인). 에디터 바이너리는 프리캐싱이 꺼져 있어 늘 true
	bool IsWorldReadyToRender(const UWorld& World)
	{
		for (const ULevel* Level : World.GetLevels())
		{
			if (!Level || !Level->bIsVisible) continue;

			for (const AActor* Actor : Level->Actors)
			{
				if (!Actor || Actor->IsHidden()) continue;

				const TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
				for (const UPrimitiveComponent* Primitive : Primitives)
				{
					if (Primitive->IsRegistered() && Primitive->IsVisible() && Primitive->IsPSOPrecaching())
					{
						return false;
					}
				}
			}
		}
		return true;
	}
}

ADRPlayerController::ADRPlayerController()
{
	// 멀티플레이 리플리케이션 활성화
	bReplicates = true;

	// 상호작용 대상 감지 (부품 · 사이트 · 청소기 · 프롭 · 열차 칸)
	InteractionComponent = CreateDefaultSubobject<UDRInteractionComponent>(TEXT("InteractionComponent"));

	// 콘솔 치트(DR*) — 개발 빌드에서만 만들어진다 (ReceivedPlayer 참고)
	CheatClass = UDRCheatManager::StaticClass();
}

void ADRPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter)
{
	// 내가 적을 타격한 경우 조준선 히트마커 표시
	// (타겟이 플레이어인 경우는 내가 피격당한 폴백 경로이므로 제외)
	if (IsValid(TargetCharacter) && IsLocalController() && Cast<ADREnemy>(TargetCharacter))
	{
		if (ADRHUD* DRHUD = GetHUD<ADRHUD>())
		{
			if (UOverlayWidgetController* OverlayWC = DRHUD->GetOverlayWidgetControllerCached())
			{
				OverlayWC->NotifyCrosshairEnemyHit();
			}
		}
	}

	if (IsValid(TargetCharacter) && DamageTextComponentClass && IsLocalController())
	{
		// 만료된(BP 애니메이션 종료 후 파괴된) 항목 정리
		ActiveDamageTexts.RemoveAll([](const TWeakObjectPtr<UDamageTextComponent>& Text)
		{
			return !Text.IsValid();
		});

		// 동시 표시 상한 - AoE 다중 타격 시 컴포넌트 생성 스파이크 방지 (코스메틱이라 스킵해도 무방)
		if (ActiveDamageTexts.Num() >= MaxConcurrentDamageTexts)
		{
			return;
		}

		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();

		// 타겟 캐릭터에 붙였다가 떼어 월드 위치 고정
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		DamageText->SetDamageText(DamageAmount);
		ActiveDamageTexts.Add(DamageText);
	}
}

void ADRPlayerController::ClientShowPartPickupUI_Implementation()
{
	OnPartPickedUp();
}

void ADRPlayerController::ServerRequestDismount_Implementation()
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter || !DRCharacter->IsMounted()) return;
	if (!IsValid(DRCharacter->MountedOn)) return;

	DRCharacter->MountedOn->DismountRider(true);
}

void ADRPlayerController::ServerSetMountedYaw_Implementation(float NewYaw)
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter || !DRCharacter->IsMounted()) return;

	// 서버 액터 요 갱신 → AttachmentReplication 상대 회전으로 다른 클라이언트에 전파
	DRCharacter->SetActorRotation(FRotator(0.f, NewYaw, 0.f));
}

void ADRPlayerController::ServerRequestDashBrake_Implementation()
{
	// 서버 재검증: 실제 지속 돌진 중인 청소기만 브레이크 이벤트 전달
	ADRRobotVacuumCharacter* Vacuum = GetPawn<ADRRobotVacuumCharacter>();
	if (!Vacuum || !Vacuum->bSustainedDash || Vacuum->bDead) return;

	FGameplayEventData EventData;
	EventData.Instigator = Vacuum;
	EventData.Target = Vacuum;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Vacuum, FDRGameplayTags::Get().Event_Dash_Brake, EventData);
}

void ADRPlayerController::ServerReportTutorialCharacterInfoOpened_Implementation()
{
	// 튜토리얼 매니저에 캐릭터 설명창이 열렸음을 보고 (서버 권한)
	if (ADRTutorialManager* TM = Cast<ADRTutorialManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ADRTutorialManager::StaticClass())))
	{
		TM->ReportCharacterInfoOpened();
	}
}
void ADRPlayerController::ServerRequestInteract_Implementation(AActor* Interactable)
{
	if (!Interactable) return;

	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 부품 획득 요청
	if (ADRCleanserPart* Part = Cast<ADRCleanserPart>(Interactable))
	{
		// 서버 측 검증: 클라이언트가 보낸 포인터를 그대로 신뢰하지 않고 상태/거리 확인
		// (거리 검증 없이는 맵 반대편 부품을 줍는 치트가 가능)
		if (!Part->CanBePickedUp()) return;

		const float DistSq = FVector::DistSquared(DRCharacter->GetActorLocation(), Part->GetActorLocation());
		if (DistSq > FMath::Square(MaxInteractDistance)) return;

		DRCharacter->PickupPart(Part);
		return;
	}

	// 사이트에 부품 설치 요청
	if (ADRCleanserSite* Site = Cast<ADRCleanserSite>(Interactable))
	{
		// 서버 측 검증: 캐릭터가 실제로 사이트(상호작용 박스)와 오버랩 중인지 확인
		if (!Site->IsOverlappingActor(DRCharacter)) return;

		Site->InstallPart(DRCharacter);
		return;
	}

	// 열차 좌석 탑승 요청 (Plan6 §5.5)
	if (ADRS2TrainCar* Car = Cast<ADRS2TrainCar>(Interactable))
	{
		// 서버 측 검증: 상태 + 거리
		if (!Car->CanBeBoardedBy(DRCharacter)) return;

		const float CarDistSq = FVector::DistSquared(DRCharacter->GetActorLocation(), Car->GetActorLocation());
		if (CarDistSq > FMath::Square(MaxInteractDistance)) return;

		Car->Board(DRCharacter);
		return;
	}

	// 스테이지2 상호작용 프롭 요청 (Plan6 §5.5-b)
	// 레버/버튼/단말 20여 개를 이 분기 하나로 처리한다 (실제 로직은 소유 퍼즐이 수행).
	if (ADRS2InteractProp* Prop = Cast<ADRS2InteractProp>(Interactable))
	{
		// 서버 측 검증: 클라이언트가 보낸 포인터를 그대로 신뢰하지 않고 상태/거리 확인
		if (!Prop->CanInteract(DRCharacter))
		{
			// 프롬프트는 떴는데 눌러도 반응이 없는 두 원인 중 하나다 (비활성 / 사망 / 부품 운반 중).
			UE_LOG(LogDR, Warning, TEXT("[S2Prop] %s 조작 거부: CanInteract=false"), *Prop->GetName());
			return;
		}

		// ★거리는 '보이는 물체' 기준으로 잰다 (2026-09-21)★
		// 프롭의 루트는 빈 SceneComponent 다 (눌림 연출 때문에 PropMesh 를 자식으로 내렸다).
		// 그래서 BP 에서 메시를 루트에서 떼어 두면 액터 원점과 실제로 보이는 물체가 갈라지고,
		// 원점 기준으로 재면 눈앞에 서 있어도 거부된다 — 프롬프트는 메시에 닿은 라인트레이스라
		// 멀쩡히 떠 있으므로 증상이 "F 를 눌러도 아무 반응이 없다"로만 나타나 원인을 찾기 어렵다.
		// 콜리전까지의 최단 거리로 재면 메시를 어디에 두든 플레이어가 보는 것과 판정이 일치한다.
		FVector ClosestPoint = FVector::ZeroVector;
		float Dist = Prop->ActorGetDistanceToCollision(DRCharacter->GetActorLocation(), ECC_Visibility, ClosestPoint);
		if (Dist < 0.f)
		{
			// 콜리전 프리미티브가 없는 프롭 (라인트레이스에도 안 잡히지만 방어적으로 둔다)
			Dist = FVector::Dist(DRCharacter->GetActorLocation(), Prop->GetActorLocation());
		}

		if (Dist > MaxInteractDistance)
		{
			UE_LOG(LogDR, Warning, TEXT("[S2Prop] %s 조작 거부: 거리 %.0f > %.0f"),
				*Prop->GetName(), Dist, MaxInteractDistance);
			return;
		}

		UE_LOG(LogDR, Log, TEXT("[S2Prop] %s 조작 수락 (서버)"), *Prop->GetName());

		Prop->ServerHandleInteract(DRCharacter);
		return;
	}

	// 로봇 청소기 탑승 요청 (Plan3 §5.2)
	if (ADRRobotVacuumCharacter* Mount = Cast<ADRRobotVacuumCharacter>(Interactable))
	{
		// 서버 측 검증: 라이더 유무/중복 탑승/순환/사망/부품/스턴 (CanBeMountedBy) + 거리
		if (!Mount->CanBeMountedBy(DRCharacter)) return;

		const float DistSq = FVector::DistSquared(DRCharacter->GetActorLocation(), Mount->GetActorLocation());
		if (DistSq > FMath::Square(MaxInteractDistance)) return;

		Mount->MountRider(DRCharacter);
	}
}

void ADRPlayerController::ClientStartSpectating_Implementation()
{
	if (!IsLocalController()) return;
    
	bIsSpectating = true;
	CurrentSpectatedPlayerIndex = 0;

	ADRGameStateBase* GameStateBase = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!GameStateBase) return;

	TArray<ADRCharacter*> AlivePlayers = GameStateBase->GetAlivePlayers();
	
	if (AlivePlayers.Num() > 0)
	{
		SetSpectateTarget(AlivePlayers[0]);
	}
}

void ADRPlayerController::ClientStopSpectating_Implementation()
{
	if (!IsLocalController()) return;
	// 관전 상태 강제 초기화 (bIsSpectating 여부와 상관없이)
	bIsSpectating = false;
	CurrentSpectatedPlayerIndex = 0;

	UnbindSpectatedDeath();
	CurrentSpectatedCharacter.Reset();

	// 관전 중 HUD 가 다른 플레이어에게 묶여 있었다면 내 상태로 되돌린다 (부활)
	RestoreOwnOverlay();

	// 대기실이면 ViewTarget 복원 스킵 (ClientSetWaitingRoomView가 담당)
	if (bIsInWaitingRoom)
	{
		// FreeRoam에서 ClientStopSpectating이 호출된 경우 대기실 플래그 교정
		ADRLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<ADRLobbyGameState>() : nullptr;
		if (LGS && LGS->GetLobbyState() == ELobbyState::FreeRoam)
		{
			bIsInWaitingRoom = false;
			// 아래로 계속 진행하여 ViewTarget 복원
		}
		else
		{
			return;
		}
	}

	// ViewTarget 복원
	if (APawn* MyPawn = GetPawn())
	{
		SetViewTarget(MyPawn);
		RestoreDefaultInputMode();
	}
	else
	{
		// Pawn이 아직 복제되지 않았으면 OnPossessedPawnChanged 이벤트에서 복원
		bPendingViewTargetRestore = true;
	}
}

void ADRPlayerController::ScheduleSpectating(float Delay)
{
	if (!HasAuthority()) return;

	GetWorldTimerManager().SetTimer(PendingSpectateTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			// 예약과 실행 사이에 다른 경로로 새 폰을 받았으면(부활) 관전으로 넘어가지 않는다
			const APawn* MyPawn = GetPawn();
			if (MyPawn && !(MyPawn->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(MyPawn)))
			{
				return;
			}
			ClientStartSpectating();
		}),
		Delay, false);
}

void ADRPlayerController::CancelPendingSpectate()
{
	GetWorldTimerManager().ClearTimer(PendingSpectateTimerHandle);
}

void ADRPlayerController::RestoreOwnOverlay()
{
	ADRHUD* DRHUD = GetHUD<ADRHUD>();
	ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
	if (!DRHUD || !DRHUD->GetOverlayWidget() || !DRPS) return;

	UAbilitySystemComponent* ASC = DRPS->GetAbilitySystemComponent();
	UAttributeSet* AS = DRPS->GetAttributeSet();
	if (!ASC || !AS) return;

	DRHUD->RebindOverlay(this, DRPS, ASC, AS);
}

bool ADRPlayerController::SpectateOnlySurvivor(const TArray<ADRCharacter*>& AliveCharacters)
{
	if (AliveCharacters.Num() != 1) return false;

	// 생존자가 한 명뿐이면 그 사람으로 고정한다 — 보던 대상이 막 죽어 마지막 생존자로 넘어가야 하는 경우 포함
	if (CurrentSpectatedCharacter.Get() != AliveCharacters[0])
	{
		CurrentSpectatedPlayerIndex = 0;
		SetSpectateTarget(AliveCharacters[0]);
	}
	return true;
}

void ADRPlayerController::SpectateNextPlayer()
{
	if (!bIsSpectating || !IsLocalController()) return;

	ADRGameStateBase* GameStateBase = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!GameStateBase) return;

	TArray<ADRCharacter*> AliveCharacters = GameStateBase->GetAlivePlayers();
	if (AliveCharacters.Num() == 0)
	{
		// 모두 사망 - 관전 대상 없음
		CurrentSpectatedCharacter.Reset();
		return;
	}

	if (SpectateOnlySurvivor(AliveCharacters)) return;

	// 다음 인덱스 계산
	CurrentSpectatedPlayerIndex = (CurrentSpectatedPlayerIndex + 1) % AliveCharacters.Num();
    
	// 새 관전 대상 설정
	SetSpectateTarget(AliveCharacters[CurrentSpectatedPlayerIndex]);
}

void ADRPlayerController::SpectatePreviousPlayer()
{
	if (!bIsSpectating || !IsLocalController()) return;

	ADRGameStateBase* GameStateBase = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!GameStateBase) return;

	TArray<ADRCharacter*> AliveCharacters = GameStateBase->GetAlivePlayers();
	if (AliveCharacters.Num() == 0)
	{
		CurrentSpectatedCharacter.Reset();
		return;
	}

	if (SpectateOnlySurvivor(AliveCharacters)) return;

	// 이전 인덱스 계산
	CurrentSpectatedPlayerIndex--;
	if (CurrentSpectatedPlayerIndex < 0)
	{
		CurrentSpectatedPlayerIndex = AliveCharacters.Num() - 1;
	}

	// 새 관전 대상 설정
	SetSpectateTarget(AliveCharacters[CurrentSpectatedPlayerIndex]);
}

void ADRPlayerController::ClientStopAllAudio_Implementation()
{
	ADRBGMActor::StopAllBGM(this, 0.0f);
}

void ADRPlayerController::CheatSkipToNextPhase()
{
// 개발 빌드에서만 동작하도록 체크
#if !UE_BUILD_SHIPPING
	ServerCheatSkipToNextPhase();
#endif
}

void ADRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Pawn 복제 도착 이벤트 바인딩 (타이머 재시도 대체)
	OnPossessedPawnChanged.AddDynamic(this, &ADRPlayerController::HandlePossessedPawnChanged);

	// Enhanced Input Context가 설정되어 있는지 확인
	check(DRContext);

	// Enhanced Input 서브시스템에 매핑 컨텍스트 추가
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(DRContext, 0);
	}

	// UI 설정
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	// 플레이어는 TeamId 0
	SetGenericTeamId(FGenericTeamId(0));

	// 카메라 피치 제한 설정
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -ViewPitchMin;  // 아래 (음수)
		PlayerCameraManager->ViewPitchMax = ViewPitchMax;   // 위 (양수)
	}

	// 로컬 플레이어만 오디오 설정 적용
	if (IsLocalController())
	{
		// Manager 통해서 오디오 설정 적용
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDRSettingsManager* Manager = GI->GetSubsystem<UDRSettingsManager>())
			{
				Manager->ApplyAudioSettings();

				// 마우스 감도 캐시 초기화 + 변경 델리게이트 구독 (Look()의 매 입력 Subsystem 조회 제거)
				if (UDRGameUserSettings* Settings = Manager->GetSettings())
				{
					CachedMouseSensitivity = Settings->MouseSensitivity;
				}
				Manager->OnMouseSensitivityChanged.AddUniqueDynamic(this, &ADRPlayerController::HandleMouseSensitivityChanged);
			}
		}
	}

	RestoreDefaultInputMode();
}

void ADRPlayerController::HandleMouseSensitivityChanged(float NewSensitivity)
{
	CachedMouseSensitivity = NewSensitivity;
}

void ADRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Tab Hold 안전장치: 누른 채로 설정창이 열리거나 컨텍스트가 깨지면 Completed 이벤트가 유실될 수 있어 강제 숨김
	if (bIsCharacterInfoVisible && !CanShowCharacterInfo())
	{
		if (ADRHUD* DRHUD = GetHUD<ADRHUD>())
		{
			DRHUD->HideCharacterInfo();
		}
		bIsCharacterInfoVisible = false;
	}

	if (bIsSpectating)
	{
		// 관전 중에는 감지를 쉰다 — 죽기 직전에 떠 있던 상호작용 안내도 내린다
		InteractionComponent->HidePrompt();
		return;
	}

	// 로컬 컨트롤러에서만 라인트레이스 실행
	if (!IsLocalController()) return;

	// 탑승 중 요 동기화: MOVE_None이라 CMC 무브가 요를 서버로 안 나르므로 주기적으로 직접 전송 (클라 전용)
	if (!HasAuthority())
	{
		const ADRCharacter* MountedChar = GetPawn<ADRCharacter>();
		if (MountedChar && MountedChar->IsMounted())
		{
			MountedYawSyncTimer += DeltaTime;
			const float ControlYaw = GetControlRotation().Yaw;
			if (MountedYawSyncTimer >= 0.1f && !FMath::IsNearlyEqual(ControlYaw, LastSentMountedYaw, 0.5f))
			{
				MountedYawSyncTimer = 0.f;
				LastSentMountedYaw = ControlYaw;
				ServerSetMountedYaw(ControlYaw);
			}
		}
	}

	// 시작 연출(로비 · 스테이지) 중에는 시점이 연출 카메라라 감지를 쉰다
	if (IsPlayingStartIntro())
	{
		InteractionComponent->HidePrompt();
		return;
	}

	// 상호작용 대상 감지 + 화면 안내 (주기마다 시점 광선 1회 — 부품 · 사이트 · 청소기 · 프롭 · 열차 칸)
	InteractionComponent->TickDetection(DeltaTime);
}

void ADRPlayerController::ServerRequestTrainDeboard_Implementation()
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	ADRS2TrainCar* Car = DRCharacter->SeatedOn;
	if (!Car) return;

	// 이동 중 하차 거부 (Plan6 §14.6.5)
	if (ADRS2Train* Train = Car->GetOwningTrain())
	{
		if (!Train->CanDeboardNow()) return;
	}

	Car->Deboard();
}

void ADRPlayerController::Client_OpenSlidePuzzleUI_Implementation(ADRS2SlidePuzzle* Puzzle)
{
	OpenSlidePuzzleScreen(Puzzle);
}

void ADRPlayerController::Client_SlidePuzzleBusy_Implementation(ADRS2SlidePuzzle* Puzzle, APlayerState* Occupant)
{
	if (!IsLocalController() || !Puzzle) return;

	UE_LOG(LogDR, Log, TEXT("[S2Puzzle] 8퍼즐이 사용 중이다 (조작자: %s)"), *GetNameSafe(Occupant));

	OnSlidePuzzleScreenBusy(Occupant);
}

// ===== 8퍼즐 조작 요청 (2026-09-20) =====
// 넷 다 액터에 위임만 한다. 점유자 검사와 인접 검사는 ADRS2SlidePuzzle 안에서 하고,
// 거절은 조용히 무시된다 (보드가 안 바뀌므로 복제도 없다 - 위젯은 확정 타임아웃으로 잠금을 푼다).

void ADRPlayerController::Server_SlidePuzzleMove_Implementation(ADRS2SlidePuzzle* Puzzle, int32 TileId)
{
	if (!IsValid(Puzzle)) return;

	Puzzle->TryMove(this, TileId);
}

void ADRPlayerController::Server_SlidePuzzleUndo_Implementation(ADRS2SlidePuzzle* Puzzle)
{
	if (!IsValid(Puzzle)) return;

	Puzzle->TryUndo(this);
}

void ADRPlayerController::Server_SlidePuzzleReset_Implementation(ADRS2SlidePuzzle* Puzzle)
{
	if (!IsValid(Puzzle)) return;

	Puzzle->TryReset(this);
}

void ADRPlayerController::Server_SlidePuzzleRelease_Implementation(ADRS2SlidePuzzle* Puzzle)
{
	if (!IsValid(Puzzle)) return;

	Puzzle->ReleaseControl(this);
}
void ADRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// DRInputComponent로 캐스팅
	UDRInputComponent* DRInputComponent = CastChecked<UDRInputComponent>(InputComponent);
	// 기본 입력 액션 바인딩
	DRInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Move);
	DRInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Look);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ADRPlayerController::StartJump);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADRPlayerController::StopJump);
	// ★Started 여야 한다. Triggered 는 키를 누르고 있는 동안 **매 프레임** 발화해
	//   레버가 연속 토글되고 금고에 같은 숫자가 반복 입력된다 (2026-08-27 수정).
	//   상호작용은 전부 "한 번 누르면 한 번" 의미이므로 다른 단발 입력들과 동일하게 맞춘다.
	DRInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleInteract);
	DRInputComponent->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleSpectateNext);
	DRInputComponent->BindAction(SpectatePreviousAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleSpectatePrevious);
	DRInputComponent->BindAction(ToggleSettingsAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleToggleSettings);
	// Tab Hold: 누름/뗌 두 이벤트 모두 바인딩. Hold Trigger 없이 Started/Completed로 Hold 동작 구현.
	DRInputComponent->BindAction(CharacterInfoAction, ETriggerEvent::Started,   this, &ADRPlayerController::HandleCharacterInfoPressed);
	DRInputComponent->BindAction(CharacterInfoAction, ETriggerEvent::Completed, this, &ADRPlayerController::HandleCharacterInfoReleased);
	// 어빌리티 입력 바인딩 (InputConfig 사용)
	DRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void ADRPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

#if UE_WITH_CHEAT_MANAGER
	// 엔진은 단독 플레이 · 에디터에서만 치트 매니저를 만든다 — 개발 빌드는 멀티플레이에서도 DR* 콘솔 치트를 바로 쓰게 한다
	if (IsLocalController())
	{
		AddCheats(true);
	}
#endif

#if !UE_BUILD_SHIPPING
	// 참가자는 방장의 튜닝 값을 받아 쓴다 — 방장(리슨 서버)은 자기 파일을 쓰므로 묻지 않는다
	if (IsLocalController() && GetNetMode() == NM_Client)
	{
		ServerRequestTuning();
	}
#endif

	// 로컬 컨트롤러만 처리
	if (!IsLocalController()) return;
// 레벨 진입 시 공통 초기화 수행
	OnLevelEntered();
}

void ADRPlayerController::ServerRequestTuning_Implementation()
{
	if (const UDRTuningSubsystem* Tuning = UDRTuningSubsystem::Get(this))
	{
		Tuning->SendTuningToClient(this);
	}
}

void ADRPlayerController::ClientReceiveTuningChunk_Implementation(int32 Revision, int32 ChunkIndex, int32 ChunkCount, const TArray<FDRTuningOverride>& Chunk)
{
	if (UDRTuningSubsystem* Tuning = UDRTuningSubsystem::Get(this))
	{
		Tuning->ReceiveHostChunk(Revision, ChunkIndex, ChunkCount, Chunk);
	}
}

void ADRPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	// 로컬 컨트롤러만 처리
	if (!IsLocalController()) return;
// SeamlessTravel 후 레벨 진입 시 공통 초기화 수행
	OnLevelEntered();
}

void ADRPlayerController::OnLevelEntered()
{
	// 레벨 컨텍스트 캐시 재판별 (ReceivedPlayer/PostSeamlessTravel에서 호출되므로 레벨 이동마다 갱신됨)
	CachedLevelContext = DetermineLevelContext();

	// 이전 레벨의 로비 시작 연출이 남아 있으면 지운다 (컨트롤러는 심리스 트래블을 건너온다)
	ResetLobbyIntroState();
	StopArmsRaise();

	// 현재 레벨 확인
	UWorld* World = GetWorld();
	if (!World) return;

	// 게임오버 UI가 남아있으면 제거
	if (CurrentResultWidget)
	{
		CurrentResultWidget->RemoveFromParent();
		CurrentResultWidget = nullptr;
	}

	// 대기실 위젯 정리 (레벨 이동 시 무효화된 포인터 정리)
	DestroyWaitingRoomUI();

	// 열려 있던 전체 화면 정리 (레벨 이동 시 위젯이 남지 않게).
	// ★스테이지 선택은 열린 채로 출발하므로 도착한 레벨에서 반드시 여기서 닫힌다★
	CloseScreen(ActiveScreen, /*bLevelTransition*/ true);

	// 관전 상태 초기화 (델리게이트 정리 포함)
	if (CurrentSpectatedCharacter.IsValid())
	{
		if (ADRCharacterBase* OldTarget = Cast<ADRCharacterBase>(CurrentSpectatedCharacter.Get()))
		{
			OldTarget->OnDeathDelegate.RemoveDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
		}
	}
	bIsSpectating = false;
	CurrentSpectatedCharacter = nullptr;
	CurrentSpectatedPlayerIndex = 0;

	// 옷 보고는 ★로비 분기보다 앞★ 에서 한다 (Plan(옷장) M3) — 대기실이면 아래에서 일찍 return 하는데,
	// 리슨 서버 호스트는 OnRep_PlayerState 도 안 불려서 그러면 FreeRoam 에서 남에게 옷이 안 보인다.
	// (클라이언트는 OnRep_PlayerState 에서도 보고한다. 대기실 디스플레이 캐릭터도 이 목록을 입는다)
	ReportCosmeticLoadout();

	// 장착 칩 보고도 같은 이유로 ★로비 분기보다 앞★ 에서 한다.
	// 대기실이면 아래에서 일찍 return 하는데, 리슨 서버 호스트는 OnRep_PlayerState 가 안 불리고,
	// 저장된 클래스가 기본값(정원로봇)과 같으면 SetSelectedPlayerClass 도 일찍 return 해 재보고가 없다.
	// 그러면 정원로봇만 칩 기기를 열고 닫기 전까지 칩이 0개(HUD 모듈 칩 미적용 → HUD 숨김)였다.
	ReportUpgradeLoadout();

	// 대기실 감지 → ViewTarget 복원 차단
	ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>();

	if (IsInLobby())
	{
		if (!LGS)
		{
			// GameState 아직 복제 안됨 → 대기실로 가정 (서버 RPC가 이후 교정)
			bIsInWaitingRoom = true;
			bAutoManageActiveCameraTarget = false; // Pawn 수신 시 ViewTarget 자동 설정 차단
			if (PlayerCameraManager)
			{
				PlayerCameraManager->bClientSimulatingViewTarget = true;	// 서버 뷰 타깃 무시 (ClientSetWaitingRoomView 참고)
			}
			RestoreDefaultInputMode();
			// WaitingRoom UI는 ClientSetWaitingRoomView RPC에서 생성
			return;
		}

		if (LGS->GetLobbyState() == ELobbyState::WaitingRoom
			|| LGS->GetLobbyState() == ELobbyState::Transitioning)
		{
			bIsInWaitingRoom = true;
			bAutoManageActiveCameraTarget = false; // Pawn 수신 시 ViewTarget 자동 설정 차단
			if (PlayerCameraManager)
			{
				PlayerCameraManager->bClientSimulatingViewTarget = true;	// 서버 뷰 타깃 무시 (ClientSetWaitingRoomView 참고)
			}
			RestoreDefaultInputMode();
			CreateWaitingRoomUI();
			return;  // ViewTarget은 ClientSetWaitingRoomView RPC가 설정
		}
	}

	bIsInWaitingRoom = false;

	// ViewTarget 즉시 복원 (잘못된 타겟 참조 방지)
	if (APawn* MyPawn = GetPawn())
	{
		SetViewTarget(MyPawn);
	}
	else
	{
		// Pawn이 아직 없으면 임시로 자기 자신을 보다가 OnPossessedPawnChanged 이벤트에서 복원
		SetViewTarget(this);
		bPendingViewTargetRestore = true;
	}

	// 레벨에 맞는 기본 입력 모드로 복원
	RestoreDefaultInputMode();

	// BGM은 레벨에 배치된 DRBGMActor가 담당
}

void ADRPlayerController::HandleToggleSettings()
{
	// 설정이 아닌 화면이 열려 있으면 ESC 로 그 화면을 먼저 닫는다.
	// (각 화면 위젯도 NativeOnKeyDown 으로 ESC 를 받는다 — 포커스를 잃었을 때만 여기로 온다)
	if (ActiveScreen != EDRScreen::None && ActiveScreen != EDRScreen::Settings)
	{
		CloseScreen(ActiveScreen);
		return;
	}

	ToggleSettingsMenu();
}

bool ADRPlayerController::CanShowCharacterInfo() const
{
	// 로컬 컨트롤러만 UI 토글
	if (!IsLocalController()) return false;

	// 게임 레벨/튜토리얼/로비에서만 허용 (메인메뉴 차단)
	if (!IsInGameLevel() && !IsInTutorial() && !IsInLobby()) return false;

	// 설정창 열려있으면 차단
	if (IsSettingsMenuOpen()) return false;

	return true;
}

void ADRPlayerController::HandleCharacterInfoPressed()
{
	if (!CanShowCharacterInfo()) return;

	ADRHUD* DRHUD = GetHUD<ADRHUD>();
	if (DRHUD == nullptr) return;

	// 입력 모드는 변경하지 않음 — GameOnly 유지로 Tab Hold 중에도 이동/조작 가능.
	// 위젯 자체도 IsFocusable=false로 두어 Tab 키 이벤트가 컨트롤러로 그대로 흐르게 함.
	// CachedSelectedClass는 서버에서만 갱신되는 비복제 캐시이므로 클라이언트 UI 소스로 쓸 수 없음.
	// 복제되는 PlayerState 값을 우선 사용하고, PlayerState 미도착 시에만 캐시로 폴백.
	EPlayerCharacterClass SelectedClass = GetCachedSelectedClass();
	if (const ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>())
	{
		SelectedClass = DRPS->GetSelectedPlayerClass();
	}
	DRHUD->ShowCharacterInfo(SelectedClass);
	bIsCharacterInfoVisible = true;

	// 튜토리얼 중에는 매니저에 보고
	if (IsInTutorial())
	{
		ServerReportTutorialCharacterInfoOpened();
	}
}

void ADRPlayerController::HandleCharacterInfoReleased()
{
	if (!bIsCharacterInfoVisible) return;

	if (ADRHUD* DRHUD = GetHUD<ADRHUD>())
	{
		DRHUD->HideCharacterInfo();
	}
	bIsCharacterInfoVisible = false;
}

void ADRPlayerController::ToggleFirstPersonMeshAndHUDVisibility()
{
	if (!IsLocalController()) return;

	bool bNewVisible = true;
	bool bFoundVisibilitySource = false;

	if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
	{
		if (DRCharacter->FirstPersonMesh)
		{
			bNewVisible = !DRCharacter->FirstPersonMesh->IsVisible();
			bFoundVisibilitySource = true;
			DRCharacter->FirstPersonMesh->SetVisibility(bNewVisible, false);
		}
	}

	if (AHUD* BaseHUD = GetHUD())
	{
		if (!bFoundVisibilitySource)
		{
			bNewVisible = !BaseHUD->bShowHUD;
		}
		BaseHUD->bShowHUD = bNewVisible;
	}

	if (ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD()))
	{
		if (UDRUserWidget* OverlayWidget = DRHUD->GetOverlayWidget())
		{
			OverlayWidget->SetVisibility(bNewVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}

void ADRPlayerController::HandleSpectateNext()
{
	if (bIsSpectating)
	{
		SpectateNextPlayer();
	}
}

void ADRPlayerController::HandleSpectatePrevious()
{
	if (bIsSpectating)
	{
		SpectatePreviousPlayer();
	}
}

void ADRPlayerController::SetSpectateTarget(ACharacter* NewTarget)
{
	// ★사망 델리게이트는 로컬 컨트롤러에서 묶는다★ — 관전 여부(bIsSpectating)는 로컬에만 있고,
	// 사망 멀티캐스트가 모든 머신에서 OnDeathDelegate 를 방송하므로 원격 클라도 직접 받는다.
	// (서버 쪽 PC 객체에 묶으면 bIsSpectating 이 늘 false 라 원격 클라는 다음 대상으로 넘어가지 못했다)
	if (IsLocalController())
	{
		BindSpectatedDeath(NewTarget);
	}
	CurrentSpectatedCharacter = NewTarget;

	// 클라이언트: 서버에도 알려 서버 쪽 ViewTarget(관련성 판정 기준)을 맞추고, 로컬 ViewTarget 은 바로 바꾼다
	if (!HasAuthority())
	{
		ServerSetSpectateTarget(NewTarget);
		if (NewTarget)
		{
			SetViewTarget(NewTarget);
		}
		return;
	}

	if (NewTarget)
	{
		SetViewTarget(NewTarget);

		// HUD 를 관전 대상에게 묶는다
		ClientUpdateSpectatorUI(NewTarget);
	}
}

void ADRPlayerController::BindSpectatedDeath(ACharacter* NewTarget)
{
	UnbindSpectatedDeath();

	if (ADRCharacterBase* DRTarget = Cast<ADRCharacterBase>(NewTarget))
	{
		DRTarget->OnDeathDelegate.AddUniqueDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
	}
}

void ADRPlayerController::UnbindSpectatedDeath()
{
	if (ADRCharacterBase* OldTarget = Cast<ADRCharacterBase>(CurrentSpectatedCharacter.Get()))
	{
		OldTarget->OnDeathDelegate.RemoveDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
	}
}

void ADRPlayerController::ServerSetSpectateTarget_Implementation(ACharacter* NewTarget)
{
	// 서버에서 SetSpectateTarget 실행
	SetSpectateTarget(NewTarget);
}

void ADRPlayerController::ClientUpdateSpectatorUI_Implementation(ACharacter* SpectatedTarget)
{
	// 클라이언트에서만 실행
	if (!IsLocalController()) return;

	UpdateSpectatorUI(SpectatedTarget);
}

void ADRPlayerController::UpdateSpectatorUI(ACharacter* SpectatedTarget)
{
	if (!SpectatedTarget) return;

	// HUD 가져오기
	ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD());
	if (!DRHUD) return;

	// 관전 대상의 PlayerState 가져오기
	ADRPlayerState* SpectatedPS = SpectatedTarget->GetPlayerState<ADRPlayerState>();
	if (!SpectatedPS) return;

	// 관전 대상의 GAS 컴포넌트들 가져오기
	UAbilitySystemComponent* SpectatedASC = SpectatedPS->GetAbilitySystemComponent();
	UAttributeSet* SpectatedAS = SpectatedPS->GetAttributeSet();

	if (!SpectatedASC || !SpectatedAS) return;

	// HUD 를 관전 대상의 PlayerState 에 다시 묶는다
	DRHUD->RebindOverlay(this, SpectatedPS, SpectatedASC, SpectatedAS);
}

void ADRPlayerController::OnSpectatedPlayerDied(AActor* DeadActor)
{
	if (!bIsSpectating) return;

	// 약간의 딜레이 후 다음 플레이어로 전환
	FTimerHandle SwitchTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(
		SwitchTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (bIsSpectating)
			{
				SpectateNextPlayer();
			}
		}),
		1.0f,
		false
	);
}

void ADRPlayerController::Move(const FInputActionValue& InputActionValue)
{
	// 시작 연출(로비 · 스테이지) 중에는 막는다 — 방향키를 누른 채 Power On 하면 연출 동안 캐릭터가 걸어갔다
	if (bIsSpectating || IsPlayingStartIntro()) return;

	// 입력 블록 상태 확인
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	// 2D 입력을 월드 좌표계로 변환
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// 폰에 이동 입력 전달
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		// 탑승 중 이동 차단 (시점 회전은 Look에서 계속 허용, Plan3 §5.2)
		if (ADRCharacter* DRChar = Cast<ADRCharacter>(ControlledPawn))
		{
			if (DRChar->IsMounted()) return;
		}

		// 지속 돌진 중 후진 입력 = 브레이크 요청 (Plan3 §8.2 — 서버가 재검증 후 GA에 이벤트 전달)
		if (ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(ControlledPawn))
		{
			// ★Reliable RPC 를 매 프레임 보내지 않는다★ — 서버가 돌진을 끝내 복제될 때까지 S 를 누르고 있으면
			// 프레임마다 쌓였다. 짧은 간격으로 한 번만 보낸다 (응답이 늦어 유실되어도 다음 간격에 다시 간다)
			if (Vacuum->bSustainedDash && InputAxisVector.Y < -0.5f)
			{
				const double Now = GetWorld()->GetTimeSeconds();
				if (Now - LastDashBrakeRequestTime >= DashBrakeRequestInterval)
				{
					LastDashBrakeRequestTime = Now;
					ServerRequestDashBrake();
				}
			}
		}

		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void ADRPlayerController::Look(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating || IsPlayingStartIntro()) return;

	// 마우스 시선 처리
	const FVector2D Axis = InputActionValue.Get<FVector2D>();

	// Manager에서 감도 가져오기
	// 감도는 캐시 사용 (설정 변경 시 OnMouseSensitivityChanged 델리게이트로 갱신)
	const float Sensitivity = CachedMouseSensitivity;

	// 감도 적용
	AddYawInput(Axis.X * Sensitivity);
	AddPitchInput(Axis.Y * Sensitivity);
}

void ADRPlayerController::StartJump(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating || IsPlayingStartIntro()) return;

	// 탑승 중이면 점프키 = 하차 (Plan3 §5.3)
	if (ADRCharacter* DRChar = GetPawn<ADRCharacter>())
	{
		if (DRChar->IsMounted())
		{
			ServerRequestDismount();
			return;
		}

		// 열차 좌석에 앉아 있으면 점프키 = 하차 (Plan6 §14.6.5)
		if (DRChar->IsSeatedOnTrain())
		{
			ServerRequestTrainDeboard();
			return;
		}
	}

	// 점프 시작
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->Jump();
	}
}

void ADRPlayerController::StopJump(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// 점프 종료
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->StopJumping();
	}
}

void ADRPlayerController::HandleInteract()
{
	if (bIsSpectating || IsPlayingStartIntro()) return;

	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 이미 탑승 중이면 F키 무시 (하차는 점프키, Plan3 §5.1)
	if (DRCharacter->IsMounted()) return;

	// 부품을 들고 있지 않을 때만 부품 획득 시도
	if (!DRCharacter->IsCarryingPart() && InteractionComponent->GetDetectedPart())
	{
		ServerRequestInteract(InteractionComponent->GetDetectedPart());
		return;
	}

	// 부품을 들고 있고 클렌저 사이트 오버랩 중이면 설치
	if (DRCharacter->IsCarryingPart())
	{
		// 클렌저 사이트 범위 안이면 설치
		if (InteractionComponent->GetDetectedSite())
		{
			ServerRequestInteract(InteractionComponent->GetDetectedSite());
			return;
		}
		// 클렌저 사이트 범위 밖이면 떨어트리기
		else
		{
			ServerRequestDropPart();
			return;
		}
	}

	// 부품 미소지 && 탑승 후보 감지 → 탑승 요청 (Plan3 §5.1)
	if (!DRCharacter->IsCarryingPart() && InteractionComponent->GetDetectedMount())
	{
		ServerRequestInteract(InteractionComponent->GetDetectedMount());
		return;
	}

	// 열차 좌석 탑승 (Plan6 §5.5)
	if (InteractionComponent->GetDetectedTrainCar())
	{
		ServerRequestInteract(InteractionComponent->GetDetectedTrainCar());
		return;
	}

	// 스테이지2 상호작용 프롭 (레버/버튼/단말) — Plan6 §5.5-b
	if (InteractionComponent->GetDetectedProp())
	{
		UE_LOG(LogDR, Log, TEXT("[S2Prop] F -> %s 조작 요청 (클라)"), *InteractionComponent->GetDetectedProp()->GetName());

		ServerRequestInteract(InteractionComponent->GetDetectedProp());
		return;
	}

	OnInteractPressed.Broadcast();
}

FKey ADRPlayerController::GetInteractKey() const
{
	return GetMappedKey(InteractAction, EKeys::F);
}

FKey ADRPlayerController::GetJumpKey() const
{
	return GetMappedKey(JumpAction, EKeys::SpaceBar);
}

FKey ADRPlayerController::GetMappedKey(const UInputAction* Action, const FKey& Fallback) const
{
	if (DRContext && Action)
	{
		for (const FEnhancedActionKeyMapping& Mapping : DRContext->GetMappings())
		{
			// 화면 안내는 키보드 기준이다 (게임패드 매핑이 같이 있어도 키보드 키를 그린다)
			if (Mapping.Action == Action && Mapping.Key.IsValid() && !Mapping.Key.IsGamepadKey())
			{
				return Mapping.Key;
			}
		}
	}
	return Fallback;
}

void ADRPlayerController::ToggleSettingsMenu()
{
	if (IsSettingsMenuOpen())
	{
		CloseSettingsMenu();
	}
	else
	{
		OpenSettingsMenu();
	}
}

// ========================= 전체 화면 (설정 · 업그레이드 · 옷장 · 스테이지 선택 · 8퍼즐) =========================

bool ADRPlayerController::OpenScreen(EDRScreen Screen, ADRS2SlidePuzzle* Puzzle)
{
	if (!IsLocalController() || Screen == EDRScreen::None) return false;

	// 이미 열려 있으면 아무것도 하지 않는다 (같은 장치 · 단말을 두 번 눌렀을 때)
	if (ActiveScreen == Screen) return true;

	if (!CanOpenScreen(Screen, Puzzle)) return false;

	// 한 번에 한 화면 — 열려 있던 화면을 먼저 닫는다 (입력 모드가 겹치지 않게)
	if (ActiveScreen != EDRScreen::None)
	{
		CloseScreen(ActiveScreen);
	}

	// ★상태를 위젯보다 먼저 올린다★ — 위젯 NativeConstruct 가 열림 상태를 볼 수 있게
	ActiveScreen = Screen;

	// 화면이 떠 있는 동안은 상호작용 안내를 내린다 (닫으면 다시 띄운다)
	InteractionComponent->RefreshPrompt();

	// ★입력 모드는 여기서만 바꾼다★ (복귀는 CloseScreen → RestoreDefaultInputMode)
	//  포커스 위젯을 지정하지 않는 이유: 각 화면 위젯이 NativeConstruct(= AddToViewport) 에서
	//  SetKeyboardFocus 를 부르므로, 순서상 위젯이 마지막에 포커스를 가져간다.
	SetInputMode(FInputModeUIOnly());
	SetShowMouseCursor(true);

	if (!ShowScreenWidget(Screen, Puzzle))
	{
		// 위젯을 못 만들었다 — UI 입력에 갇히지 않게 되돌린다
		ActiveScreen = EDRScreen::None;
		RestoreDefaultInputMode();
		return false;
	}

	OnScreenOpened(Screen);   // BP 연출 훅 (선택)
	return true;
}

void ADRPlayerController::CloseScreen(EDRScreen Screen, bool bLevelTransition)
{
	if (!IsLocalController() || Screen == EDRScreen::None || ActiveScreen != Screen) return;

	// ★상태를 먼저 내린다★ — 위젯 제거(NativeDestruct)가 이 함수를 한 번 더 부른다 (8퍼즐)
	ActiveScreen = EDRScreen::None;

	HideScreenWidget(Screen);
	OnScreenClosed(Screen);   // BP 연출 훅 (선택)

	// 화면이 내려갔다 — 장치 앞에 그대로 서 있으면 안내가 바로 다시 뜬다
	InteractionComponent->RefreshPrompt();

	// 레벨 이동 정리 — 입력 모드 · 장착 보고는 OnLevelEntered 가 이어서 한다
	if (bLevelTransition) return;

	// 레벨 컨텍스트(메인메뉴/튜토리얼/대기실/스테이지)에 맞는 모드로 되돌린다
	RestoreDefaultInputMode();

	// 화면이 열린 동안의 변경은 프리뷰에만 보이고, 닫는 순간 서버에 한 번에 반영된다
	// (PlayerState 복제 → 내 캐릭터와 남의 화면에 적용 — Plan2.md 7.1 / Plan(옷장) M3)
	if (Screen == EDRScreen::Upgrade)
	{
		ReportUpgradeLoadout();
	}
	else if (Screen == EDRScreen::Wardrobe)
	{
		ReportCosmeticLoadout();
	}
}

bool ADRPlayerController::CanOpenScreen(EDRScreen Screen, const ADRS2SlidePuzzle* Puzzle) const
{
	switch (Screen)
	{
	case EDRScreen::Upgrade:
		// 로비에서만 슬롯 해금/칩 교체가 가능하다. 화면을 닫을 때 서버로 재보고되고 그 즉시 스탯 GE 가
		// 재적용되므로, 이 가드가 없으면 스테이지 도중 칩 교체가 실제로 반영돼 버린다.
		if (!IsInLobby())
		{
			UE_LOG(LogDR, Warning, TEXT("[Upgrade] 로비가 아닌 곳에서 업그레이드 화면 열기를 시도했습니다 — 무시합니다."));
			return false;
		}
		break;

	case EDRScreen::Wardrobe:
		// 옷장도 로비 전용이다. 스테이지 도중 외형을 바꿔 끼우는 경로를 만들지 않는다.
		if (!IsInLobby())
		{
			UE_LOG(LogDR, Warning, TEXT("[Cosmetic] 로비가 아닌 곳에서 옷장 열기를 시도했습니다 — 무시합니다."));
			return false;
		}
		break;

	case EDRScreen::StageSelect:
		// 리슨 서버의 호스트만 — 클라이언트는 스테이지를 고를 수 없다
		if (!HasAuthority())
		{
			UE_LOG(LogDR, Warning, TEXT("[StageSelect] 호스트가 아닌 플레이어가 스테이지 선택을 열려고 했습니다 — 무시합니다."));
			return false;
		}
		if (!IsInLobby())
		{
			UE_LOG(LogDR, Warning, TEXT("[StageSelect] 로비가 아닌 곳에서 스테이지 선택 열기를 시도했습니다 — 무시합니다."));
			return false;
		}
		break;

	case EDRScreen::SlidePuzzle:
		if (!IsValid(Puzzle))
		{
			UE_LOG(LogDR, Warning, TEXT("[S2Puzzle] 창 열기 무시 — 퍼즐이 없다"));
			return false;
		}
		break;

	default:
		break;
	}

	// 위젯 클래스는 BP_DRPlayerController 의 UI 카테고리에서 지정한다
	if (!GetScreenWidgetClass(Screen))
	{
		UE_LOG(LogDR, Error, TEXT("[Screen] %s 화면의 위젯 클래스가 비어 있다 — BP_DRPlayerController 의 UI 카테고리에서 지정할 것"),
			*StaticEnum<EDRScreen>()->GetNameStringByValue(static_cast<int64>(Screen)));
		return false;
	}
	return true;
}

TSubclassOf<UUserWidget> ADRPlayerController::GetScreenWidgetClass(EDRScreen Screen) const
{
	switch (Screen)
	{
	case EDRScreen::Settings:    return SettingsScreenWidgetClass;
	case EDRScreen::Upgrade:     return UpgradeScreenWidgetClass;
	case EDRScreen::Wardrobe:    return WardrobeScreenWidgetClass;
	case EDRScreen::StageSelect: return StageSelectScreenWidgetClass;
	case EDRScreen::SlidePuzzle: return SlidePuzzleWidgetClass;
	default:                     return nullptr;
	}
}

bool ADRPlayerController::ShowScreenWidget(EDRScreen Screen, ADRS2SlidePuzzle* Puzzle)
{
	switch (Screen)
	{
	case EDRScreen::Settings:
		if (!IsValid(SettingsScreenWidget))
		{
			SettingsScreenWidget = CreateWidget<UUserWidget>(this, SettingsScreenWidgetClass);
		}
		if (!SettingsScreenWidget) return false;
		SettingsScreenWidget->AddToViewport();
		return true;

	case EDRScreen::Upgrade:
		UpgradeScreenWidget = CreateWidget<UDRUpgradeScreenWidget>(this, UpgradeScreenWidgetClass);
		if (!UpgradeScreenWidget) return false;
		UpgradeScreenWidget->AddToViewport();
		return true;

	case EDRScreen::Wardrobe:
		// 키보드 포커스는 화면의 NativeConstruct 가 직접 잡는다 (ESC / C 키)
		WardrobeScreenWidget = CreateWidget<UDRWardrobeScreenWidget>(this, WardrobeScreenWidgetClass);
		if (!WardrobeScreenWidget) return false;
		WardrobeScreenWidget->AddToViewport();
		return true;

	case EDRScreen::StageSelect:
		StageSelectScreenWidget = CreateWidget<UDRStageSelectScreenWidget>(this, StageSelectScreenWidgetClass);
		if (!StageSelectScreenWidget) return false;
		// 전체화면 모달이라 HUD(0) · 퍼즐(10) 위, 결과창/캐릭터 정보(100) · 로딩(10000) 아래.
		// 0 이면 화면을 연 ★뒤에★ 만들어지는 HUD 위젯(대기실 → 자유이동 전환 직후의 크로스헤어 등)이 위에 덮인다.
		StageSelectScreenWidget->AddToViewport(50);
		return true;

	case EDRScreen::SlidePuzzle:
		// ★창은 재사용한다★ 판 상태는 서버에 있으므로 새로 만들어도 동작하지만,
		// 재사용하면 등장 연출이 매번 다시 돌지 않고 조각 8개를 다시 만들 일도 줄어든다.
		if (!IsValid(SlidePuzzleWidget))
		{
			SlidePuzzleWidget = CreateWidget<UDRS2SlidePuzzleWidget>(this, SlidePuzzleWidgetClass);
		}
		if (!SlidePuzzleWidget) return false;
		// ★BindToPuzzle 이 AddToViewport 보다 먼저다★
		// 그래야 NativeConstruct 가 조각을 만든 직후 서버 보드를 그대로 그릴 수 있다.
		SlidePuzzleWidget->BindToPuzzle(Puzzle);
		SlidePuzzleWidget->AddToViewport(10);
		return true;

	default:
		return false;
	}
}

void ADRPlayerController::HideScreenWidget(EDRScreen Screen)
{
	auto RemoveAndForget = [](auto& Widget)
	{
		if (IsValid(Widget))
		{
			Widget->RemoveFromParent();
		}
		Widget = nullptr;
	};

	switch (Screen)
	{
	case EDRScreen::Settings:
		if (IsValid(SettingsScreenWidget))
		{
			// 설정 화면이 따로 띄운 드롭다운 팝업부터 닫는다 (WBP_SettingsScreen 의 BP 함수 — 안 닫으면 화면 위에 남는다)
			static const FName HideDropdownPopupName(TEXT("HideDropdownPopup"));
			UFunction* HideDropdownPopup = SettingsScreenWidget->FindFunction(HideDropdownPopupName);
			if (HideDropdownPopup && HideDropdownPopup->NumParms == 0)
			{
				SettingsScreenWidget->ProcessEvent(HideDropdownPopup, nullptr);
			}
		}
		RemoveAndForget(SettingsScreenWidget);
		break;

	case EDRScreen::Upgrade:     RemoveAndForget(UpgradeScreenWidget); break;
	case EDRScreen::Wardrobe:    RemoveAndForget(WardrobeScreenWidget); break;
	case EDRScreen::StageSelect: RemoveAndForget(StageSelectScreenWidget); break;

	case EDRScreen::SlidePuzzle:
		// 파괴하지 않는다 — 다음에 열 때 같은 인스턴스를 다시 쓴다 (점유 반납은 위젯 NativeDestruct 가 한다)
		if (IsValid(SlidePuzzleWidget))
		{
			SlidePuzzleWidget->RemoveFromParent();
		}
		break;

	default:
		break;
	}
}

void ADRPlayerController::ClientCloseSettingsMenu_Implementation()
{
	CloseSettingsMenu();
}

// ========================= 업그레이드 칩 (Plan2.md 7.1) =========================

void ADRPlayerController::ReportUpgradeLoadout()
{
	if (!IsLocalController()) return;

	const ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
	if (!DRPS) return;

	const UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	if (!GI) return;

	const EPlayerCharacterClass SelectedClass = DRPS->GetSelectedPlayerClass();
	ServerReportUpgradeLoadout(SelectedClass, GI->GetEquippedChips(SelectedClass));
}

void ADRPlayerController::ServerReportUpgradeLoadout_Implementation(EPlayerCharacterClass ForClass,
	const TArray<FName>& Chips)
{
	ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
	if (!DRPS) return;

	// 경합/스푸핑 방어: 서버가 확정한 선택 클래스와 다른 보고는 폐기한다
	if (ForClass != DRPS->GetSelectedPlayerClass()) return;

	const UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	UDRChipCatalog* Catalog = GI ? GI->GetChipCatalog() : nullptr;
	if (!Catalog)
	{
		// 카탈로그가 없으면 검증할 수 없다 — 아무 것도 싣지 않는다(잘못된 강화 방지)
		DRPS->SetEquippedChips(TArray<FName>());
		return;
	}

	TArray<FName> Sanitized = Chips;
	const bool bModified = Catalog->SanitizeLoadout(Sanitized, ForClass, GI->GetMaxSlotCount());

	if (bModified)
	{
		UE_LOG(LogDR, Warning, TEXT("[Upgrade] %s 의 장착 목록을 정화했습니다 (%d → %d)."),
			*DRPS->GetPlayerName(), Chips.Num(), Sanitized.Num());
	}

	DRPS->SetEquippedChips(Sanitized);
}

// ========================= 코스메틱 (Plan(옷장) M3 · D-26) =========================

void ADRPlayerController::ReportCosmeticLoadout()
{
	if (!IsLocalController()) return;

	const ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
	if (!DRPS) return;

	const UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	if (!GI) return;

	const EPlayerCharacterClass SelectedClass = DRPS->GetSelectedPlayerClass();
	ServerReportCosmeticLoadout(SelectedClass, GI->GetEquippedSkins(SelectedClass));
}

void ADRPlayerController::ServerReportCosmeticLoadout_Implementation(EPlayerCharacterClass ForClass,
	const TArray<FName>& SkinIds)
{
	ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
	if (!DRPS) return;

	// 경합/스푸핑 방어: 서버가 확정한 선택 클래스와 다른 보고는 폐기한다 (칩과 같은 가드)
	if (ForClass != DRPS->GetSelectedPlayerClass()) return;

	const UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	const UDRCosmeticCatalog* Catalog = GI ? GI->GetCosmeticCatalog() : nullptr;
	if (!Catalog)
	{
		// 검증할 수 없으면 기본 외형 — 없는 에셋을 가리키는 Id 로 다른 클라가 헤매지 않게 한다
		DRPS->SetEquippedSkinIds(TArray<FName>());
		return;
	}

	// 길이를 카테고리 수로 맞추고, 카탈로그에 없는 Id · 다른 로봇 · 다른 부위를 None 으로.
	// ★해금 여부는 보지 않는다★ — 협동 PvE 라 잠긴 옷을 보고해도 남에게 피해가 없고, 서버는 남의 해금 상태를 모른다.
	TArray<FName> Sanitized = SkinIds;
	if (Catalog->SanitizeLoadout(Sanitized, ForClass))
	{
		UE_LOG(LogDR, Warning, TEXT("[Cosmetic] %s 의 옷 목록을 정화했습니다."), *DRPS->GetPlayerName());
	}

	DRPS->SetEquippedSkinIds(Sanitized);
}

void ADRPlayerController::Client_GrantStageReward_Implementation(const FDRStageRewardReport& Report)
{
	UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	if (!GI) return;

	// 지갑 반영을 먼저 끝낸 뒤 결과창이 그 결과를 읽는다
	LastStageRewardResult = GI->ApplyStageReward(Report);

	UE_LOG(LogDR, Log, TEXT("[Progression] 스테이지 보상 반영: +%d (보유 %d → %d)"),
		LastStageRewardResult.TotalGained,
		LastStageRewardResult.CurrencyBefore,
		LastStageRewardResult.CurrencyAfter);
}

// ========================= 로비 업그레이드 화면 =========================

// ========================= 로비 옷장 화면 (Plan(옷장)) =========================

// ========================= 스테이지 선택 화면 =========================

bool ADRPlayerController::CanUseStageSelectScreen() const
{
	if (!StageSelectScreenWidgetClass) return false;

	const UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	return GI && GI->ProgressionConfig && GI->ProgressionConfig->StageSelectList.Num() > 0;
}

bool ADRPlayerController::RequestStageTravel(FName StageId)
{
	if (!IsLocalController() || !HasAuthority())
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] 스테이지 이동은 호스트만 요청할 수 있습니다."));
		return false;
	}

	const UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	const FDRStageSelectDef* Def = (GI && GI->ProgressionConfig) ? GI->ProgressionConfig->FindStageSelectDef(StageId) : nullptr;
	if (!Def || Def->Map.IsNull())
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] '%s' 의 정의 또는 Map 이 없습니다 (DA_ProgressionConfig > StageSelectList)."),
			*StageId.ToString());
		return false;
	}

	// 화면을 거치지 않는 호출(BP · 치트)도 같은 규칙을 따른다
	if (GI->GetStageSelectState(StageId) == EDRStageSelectState::Locked)
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] '%s' 는 잠긴 스테이지입니다 — 이동하지 않습니다."), *StageId.ToString());
		return false;
	}

	ADRLobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADRLobbyGameMode>() : nullptr;
	if (!LobbyGameMode)
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] 로비 게임모드가 아닙니다 — 이동하지 않습니다."));
		return false;
	}

	// "/Game/Maps/Stage1" 형태. ExecuteTravel 이 "?listen" 을 붙여 ServerTravel 한다.
	LobbyGameMode->TravelToStage(Def->Map.GetLongPackageName(), this);
	return true;
}

void ADRPlayerController::ServerCheatDie_Implementation()
{
#if !UE_BUILD_SHIPPING
	// Die() 는 오염 상태가 아니면 사망을 막는다 — 확인용이라 사망 처리만 바로 부른다
	ADRCharacterBase* CheatTarget = Cast<ADRCharacterBase>(GetPawn());
	if (!CheatTarget || ICombatInterface::Execute_IsDead(CheatTarget)) return;

	CheatTarget->MulticastHandleDeath(FVector::ZeroVector);
	UE_LOG(LogDR, Warning, TEXT("[Cheat] %s 사망 처리"), *CheatTarget->GetName());
#endif
}

void ADRPlayerController::ServerCheatRevive_Implementation()
{
#if !UE_BUILD_SHIPPING
	ADRGameModeBase* GM = GetWorld()->GetAuthGameMode<ADRGameModeBase>();
	if (!GM) return;

	// 시체가 남아 있으면 그 자리, 이미 정리됐으면 시작 지점에서 되살린다
	FTransform At = GetPawn() ? GetPawn()->GetActorTransform() : FTransform::Identity;
	if (!GetPawn())
	{
		if (const AActor* Start = GM->FindPlayerStart(this))
		{
			At = Start->GetActorTransform();
		}
	}

	if (GM->RevivePlayer(this, At, 1.0f, 1.0f))
	{
		UE_LOG(LogDR, Warning, TEXT("[Cheat] %s 부활"), *GetName());
	}
#endif
}

void ADRPlayerController::RestoreDefaultInputMode()
{
	if (!IsLocalController()) return;

	if (IsInMainMenu())
	{
		// 메인메뉴: UI 모드
		SetInputMode(FInputModeUIOnly());
		SetShowMouseCursor(true);
	}
	else if (IsInTutorial())
	{
		// 튜토리얼: 게임 모드
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
	else if (IsInLobby())
	{
		// ★로비 상태(복제값)가 아니라 이 컨트롤러의 대기실 플래그로 가른다★ — 서버의 FreeRoam 전환은
		// 시작 연출이 끝난 뒤에 오므로, 그 사이 화면(설정 등)을 닫으면 커서가 다시 켜졌다
		if (bIsInWaitingRoom)
		{
			// 대기실은 GameAndUI 로 통일 (ClientSetWaitingRoomView 와 일치)
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			SetInputMode(InputMode);
			SetShowMouseCursor(true);
		}
		else
		{
			SetInputMode(FInputModeGameOnly());
			SetShowMouseCursor(false);
		}
	}
	else
	{
		// 스테이지: 게임 모드
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

EDRLevelContext ADRPlayerController::DetermineLevelContext() const
{
	UWorld* World = GetWorld();
	if (!World) return EDRLevelContext::Unknown;

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	if (CurrentLevelName.Contains(TEXT("MainMenu"))) return EDRLevelContext::MainMenu;
	if (CurrentLevelName.Contains(TEXT("Lobby"))) return EDRLevelContext::Lobby;
	if (CurrentLevelName.Contains(TEXT("Tutorial"))) return EDRLevelContext::Tutorial;

	// 스테이지 레벨은 맵 이름 하드코딩("Stage1") 대신 GameState 타입으로 판별 (Stage2가 생겨도 동작)
	if (World->GetGameState<ADRStageGameState>()) return EDRLevelContext::GameLevel;

	return EDRLevelContext::Unknown;
}

EDRLevelContext ADRPlayerController::GetLevelContext() const
{
	// 초기 프레임에 GameState 복제가 늦어 판별에 실패(Unknown)했으면 재시도
	if (CachedLevelContext == EDRLevelContext::Unknown)
	{
		CachedLevelContext = DetermineLevelContext();
	}
	return CachedLevelContext;
}

bool ADRPlayerController::IsInMainMenu() const
{
	return GetLevelContext() == EDRLevelContext::MainMenu;
}

bool ADRPlayerController::IsInLobby() const
{
	return GetLevelContext() == EDRLevelContext::Lobby;
}

bool ADRPlayerController::IsInGameLevel() const
{
	return GetLevelContext() == EDRLevelContext::GameLevel;
}

bool ADRPlayerController::IsInTutorial() const
{
	return GetLevelContext() == EDRLevelContext::Tutorial;
}

void ADRPlayerController::ShowResultWidget(TSubclassOf<UUserWidget> WidgetClass)
{
	// 이미 결과 UI 가 떠 있거나 위젯 클래스가 없으면 무시
	if (CurrentResultWidget || !WidgetClass) return;

	CurrentResultWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	if (CurrentResultWidget)
	{
		CurrentResultWidget->AddToViewport(100);
	}
}

void ADRPlayerController::Client_ShowGameOverUI_Implementation()
{
	ShowResultWidget(GameOverWidgetClass);
}

void ADRPlayerController::Client_ShowGameClearUI_Implementation()
{
	ShowResultWidget(GameClearWidgetClass);
}

void ADRPlayerController::Client_ShowTutorialClearUI_Implementation()
{
	ShowResultWidget(TutorialGameClearWidgetClass);
}

void ADRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	// 홀드 추적은 아래 차단 게이트보다 먼저 — Pressed 만 걸리고 Released 는 통과하는 경우에도
	// 집합이 어긋나지 않도록 (Released 쪽 Remove 도 동일하게 최상단에서 수행)
	if (InputTag.IsValid())
	{
		HeldInputTags.Add(InputTag);
	}

	if (bIsSpectating || IsPlayingStartIntro()) return;

	// 입력 블록 확인 후 어빌리티 입력 처리 (블록 중이면 아래 UI 피드백도 내지 않는다 — 그래서 ASC 가 아니라 여기서 본다)
	UDRAbilitySystemComponent* ASC = GetASC();
	if (!ASC || ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	ASC->AbilityInputTagPressed(InputTag);

	// 스킬 아이콘 UI 피드백 브로드캐스트 (로컬 컨트롤러만, 차단 중이면 발화 안 함)
	if (IsLocalController() && !IsAbilityInputBlocked(InputTag))
	{
		if (ADRHUD* HUD = Cast<ADRHUD>(GetHUD()))
		{
			if (UOverlayWidgetController* WC = HUD->GetOverlayWidgetControllerCached())
			{
				WC->OnAbilityInputPressed.Broadcast(InputTag);
			}
		}
	}
}

void ADRPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	// 반드시 아래 early return 들보다 먼저 — 관전/Block.InputReleased 에 걸려 리턴하면
	// 태그가 "눌린 채"로 영구히 남아 이후 GA 종료발 Released 재방송이 계속 억제된다
	HeldInputTags.Remove(InputTag);

	if (bIsSpectating) return;

	// 입력 해제 블록 확인
	UDRAbilitySystemComponent* ASC = GetASC();
	if (!ASC || ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputReleased)) return;

	ASC->AbilityInputTagReleased(InputTag);

	// 스킬 아이콘 UI 피드백 브로드캐스트 (로컬 컨트롤러만, 차단 중이면 발화 안 함)
	if (IsLocalController() && !IsAbilityInputBlocked(InputTag))
	{
		if (ADRHUD* HUD = Cast<ADRHUD>(GetHUD()))
		{
			if (UOverlayWidgetController* WC = HUD->GetOverlayWidgetControllerCached())
			{
				WC->OnAbilityInputReleased.Broadcast(InputTag);
			}
		}
	}
}

void ADRPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	if (bIsSpectating || IsPlayingStartIntro()) return;

	// 입력 홀드 블록 확인
	UDRAbilitySystemComponent* ASC = GetASC();
	if (!ASC || ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputHeld)) return;

	ASC->AbilityInputTagHeld(InputTag);
}

UDRAbilitySystemComponent* ADRPlayerController::GetASC() const
{
	return Cast<UDRAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
}

bool ADRPlayerController::IsAbilityInputBlocked(const FGameplayTag& InputTag) const
{
	UDRAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return false;

	// Carrying 중에는 모든 스킬 입력이 차단된 것으로 간주 (UI 피드백도 막음)
	if (ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Carrying))
	{
		return true;
	}

	// InputTag → AbilitySpec → AbilityTag → 차단 카운터 검사
	if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecByInputTag(InputTag))
	{
		const FGameplayTag AbilityTag = UDRAbilitySystemComponent::GetAbilityTagFromSpec(*Spec);
		if (AbilityTag.IsValid() && ASC->IsAbilityTagBlocked(AbilityTag))
		{
			return true;
		}
	}

	return false;
}

void ADRPlayerController::ServerCheatSkipToNextPhase_Implementation()
{
#if !UE_BUILD_SHIPPING
	// GameMode 가져오기
	ADRStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<ADRStageGameMode>();
	if (!StageGameMode) return;

	// 페이즈 전환
	if (UDRPhase3* Phase3 = Cast<UDRPhase3>(StageGameMode->GetCurrentPhase()))
	{
		Phase3->SkipToNextWave();
		return;
	}

	// 정상 클리어와 같은 경로 — 목표 완료 연출(Multicast_ObjectiveCompleted)을 보내야 목표 UI 가 대기 중인 새 목표로 바뀐다.
	// (TransitionToNextPhase 를 바로 부르면 연출이 없어 이전 목표 제목이 남았다. 첫 페이즈 시작 전에는 아무것도 하지 않는다)
	StageGameMode->EndCurrentPhase();
#endif
}

void ADRPlayerController::ServerRequestDropPart_Implementation()
{
	// 캐릭터 가져오기
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 부품 떨어트리기
	DRCharacter->DropCarriedPart();
}

void ADRPlayerController::RequestChangeClass(bool bNext)
{
	ServerRequestChangeClass(bNext);
}

void ADRPlayerController::ServerRequestChangeClass_Implementation(bool bNext)
{
	// 대기실 상태에서만 가능
	ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
	if (!LGS || LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

	ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
	if (!PS) return;

	// 현재 클래스 가져오기
	int32 CurrentIndex = static_cast<int32>(PS->GetSelectedPlayerClass());
	constexpr int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);

	// 순환
	int32 NewIndex;
	if (bNext)
		NewIndex = (CurrentIndex + 1) % ClassCount;
	else
		NewIndex = (CurrentIndex - 1 + ClassCount) % ClassCount;

	EPlayerCharacterClass NewClass = static_cast<EPlayerCharacterClass>(NewIndex);
	PS->SetSelectedPlayerClass(NewClass);

	// PlayerController 에도 캐싱 (Seamless Travel 시 PlayerState 값 유실 방지)
	CachedSelectedClass = NewClass;
}

void ADRPlayerController::ClientTeleportToSlot_Implementation(FVector SlotLocation, FRotator SlotRotation)
{
	if (APawn* MyPawn = GetPawn())
	{
		MyPawn->TeleportTo(SlotLocation, SlotRotation);

		if (UCharacterMovementComponent* MovementComp =
			Cast<UCharacterMovementComponent>(MyPawn->GetMovementComponent()))
		{
			MovementComp->Velocity = FVector::ZeroVector;
			MovementComp->DisableMovement();
		}
	}
}

void ADRPlayerController::ClientSetWaitingRoomView_Implementation(
	ADRWaitingRoomCameraActor* CameraActor)
{
	if (!CameraActor) return;

	// 카메라 캐시 저장 (ClientRestart에서 재고정에 사용)
	CachedWaitingRoomCamera = CameraActor;

	bIsInWaitingRoom = true;
	bAutoManageActiveCameraTarget = false; // Pawn 변경 시 자동 ViewTarget 전환 차단

	// 서버가 보내는 뷰 타깃도 무시한다 — 서버 쪽 이 컨트롤러는 자동 관리가 켜져 있어 Power On 빙의 순간
	// ClientSetViewTarget(폰)을 보내고, 그대로 받으면 원격 클라는 시작 연출 전에 1인칭으로 튄다 (연출이 끝나면 ResetIntroCamera 가 해제)
	if (PlayerCameraManager)
	{
		PlayerCameraManager->bClientSimulatingViewTarget = true;
	}

	// 잘못 생성된 LobbyOverlay가 있으면 제거
	if (ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD()))
	{
		DRHUD->RemoveOverlay();
	}

	// 즉시 고정 카메라로 전환
	SetViewTargetWithBlend(CameraActor, 0.f);

	// 대기실 입력: GameAndUI + 커서 (UIOnly 는 Tab/ESC 가 위젯 포커스에 막혀 GameAndUI 로 바꿨다)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	SetShowMouseCursor(true);

	// 소유 Pawn의 메시 가시성 전환 (1P 숨기기, 3P 보이기)
	if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
	{
		DRCharacter->SetWaitingRoomVisibility(true);
	}

	// 대기실 UI 생성 (아직 없으면)
	CreateWaitingRoomUI();
}

void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
	bIsInWaitingRoom = false;

	// 폰 도착을 기다리는 동안에도 연출 중으로 친다 — 입력 · HUD 오버레이를 미룬다
	bIsPlayingLobbyIntro = true;
	IntroCameraKind = EDRIntroCameraKind::Lobby;

	// 대기실 UI 는 로비 상태(Transitioning)로 이미 닫혔다. 연출 동안 커서는 숨기고 게임 입력으로 둔다 (이동 · 시선은 위 플래그가 막는다)
	if (ActiveScreen == EDRScreen::None)
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}

	if (!GetPawn())
	{
		// Pawn이 아직 복제되지 않았으면 OnPossessedPawnChanged 이벤트에서 전환 실행.
		// 5초 안에 안 오면 연출 없이 끝내 입력 · HUD 고착을 막는다
		bPendingCameraTransition = true;
		GetWorldTimerManager().SetTimer(
			CameraTransitionRetryHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (bPendingCameraTransition)
				{
					FinishIntroCamera();
				}
			}),
			5.0f,
			false
		);
		return;
	}

	// Pawn이 이미 있으면 즉시 실행 (호스트 또는 빠른 리플리케이션)
	ExecuteCameraTransitionToCharacter();
}

void ADRPlayerController::ExecuteCameraTransitionToCharacter()
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();

	// ★폰의 BeginPlay 뒤에 시작한다★ — 원격 클라는 폰 첫 복제의 OnRep_Controller → OnRep_Pawn 으로 BeginPlay 보다
	// 먼저 여기 들어온다. 그대로 시작하면 곧이은 BeginPlay 의 UpdateMeshVisibility 가 3인칭 몸을 다시 숨겨
	// 연출 내내 내 캐릭터 자리가 비어 보였다 (2026-11-08 PIE 2인). 다음 틱에 다시 부른다
	if (DRCharacter && !DRCharacter->HasActorBegunPlay())
	{
		bPendingCameraTransition = true;
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (bPendingCameraTransition)
			{
				ExecuteCameraTransitionToCharacter();
			}
		}));
		return;
	}

	bPendingCameraTransition = false;
	GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);

	// 출발 시점 = 대기실 카메라 (없으면 지금 화면)
	FMinimalViewInfo StartView;
	if (ADRWaitingRoomCameraActor* WaitingCamera = CachedWaitingRoomCamera.Get(); WaitingCamera && WaitingCamera->CameraComponent)
	{
		WaitingCamera->CameraComponent->GetCameraView(0.f, StartView);
	}
	else if (PlayerCameraManager)
	{
		StartView = PlayerCameraManager->GetCameraCacheView();
	}

	// 시선(컨트롤 회전)은 서버가 PowerOn 에서 ClientSetRotation 으로 스폰 방향에 맞춰 보낸다 — 연출의 도착 방향도 그것이다
	if (!BeginIntroCamera(DRCharacter, StartView))
	{
		FinishIntroCamera();
		return;
	}
	StartIntroCameraMove(LobbyIntroDuration);
}

bool ADRPlayerController::BeginIntroCamera(ADRCharacter* DRCharacter, const FMinimalViewInfo& StartView)
{
	UWorld* World = GetWorld();
	if (!DRCharacter || !World) return false;

	// 연출용 카메라 — 이 클라이언트에만 있다 (복제 안 함).
	// ★폰이 아닌 시점이어야 한다★ — 폰(또는 블렌드 중인 폰)이 뷰 타깃이면 주인 시점 규칙이 걸려
	// 3인칭 몸(OwnerNoSee)이 사라지고 1인칭 팔(OnlyOwnerSee)만 허공에 뜬다
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	ACameraActor* NewCamera = World->SpawnActor<ACameraActor>(StartView.Location, StartView.Rotation, SpawnParams);
	if (!NewCamera) return false;

	UCameraComponent* CameraComponent = NewCamera->GetCameraComponent();
	CameraComponent->SetFieldOfView(StartView.FOV);
	CameraComponent->SetConstraintAspectRatio(StartView.bConstrainAspectRatio);
	CameraComponent->SetAspectRatio(StartView.AspectRatio);
	CameraComponent->PostProcessSettings = StartView.PostProcessSettings;
	CameraComponent->PostProcessBlendWeight = StartView.PostProcessBlendWeight;

	// 혹시 남은 이전 연출 카메라는 뷰 타깃을 옮긴 뒤 지운다
	ACameraActor* PreviousCamera = IntroCamera.Get();

	IntroCamera = NewCamera;
	IntroPawn = DRCharacter;
	IntroStartView = StartView;
	bIntroCameraActive = true;
	bIntroCameraMoving = false;
	bIntroBodyHidden = false;

	// 몸은 3인칭으로 보이고 1인칭 팔은 숨긴다 (원격 클라는 폰이 막 도착해 1인칭 상태일 수 있다)
	DRCharacter->SetWaitingRoomVisibility(true);

	SetViewTarget(NewCamera);

	if (PreviousCamera && PreviousCamera != NewCamera)
	{
		PreviousCamera->Destroy();
	}
	return true;
}

void ADRPlayerController::StartIntroCameraMove(float Duration)
{
	IntroStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	IntroDuration = Duration;
	bIntroCameraMoving = true;
}

void ADRPlayerController::UpdateCameraManager(float DeltaSeconds)
{
	// 모든 액터가 틱한 뒤라 폰 · 1인칭 카메라 위치가 이번 프레임 값이다
	TickStageIntro();
	TickIntroCamera();
	TickArmsRaise();

	Super::UpdateCameraManager(DeltaSeconds);
}

void ADRPlayerController::TickIntroCamera()
{
	if (!bIntroCameraActive) return;

	ACameraActor* Camera = IntroCamera.Get();
	ADRCharacter* DRCharacter = IntroPawn.Get();
	UWorld* World = GetWorld();
	if (!Camera || !DRCharacter || !World || GetPawn() != DRCharacter)
	{
		// 폰이 바뀌었거나 사라졌다 — 연출을 접는다
		FinishIntroCamera();
		return;
	}

	// 뷰 타깃은 끝까지 연출 카메라다 — 도중에 폰으로 옮겨졌으면 되돌린다 (폰 시점이면 3인칭 몸이 사라지고 1인칭 팔만 뜬다)
	if (GetViewTarget() != Camera)
	{
		SetViewTarget(Camera);
	}

	// 출발 시점에서 기다린다 (스테이지: 다른 플레이어의 로딩)
	if (!bIntroCameraMoving) return;

	const double Alpha = FMath::Clamp((World->GetTimeSeconds() - IntroStartTime) / FMath::Max(IntroDuration, 0.01f), 0.0, 1.0);
	// smootherstep — 천천히 출발해 천천히 내려앉는다
	const double Progress = Alpha * Alpha * Alpha * (Alpha * (Alpha * 6.0 - 15.0) + 10.0);

	// 도착점 = 폰의 1인칭 카메라. 매 프레임 다시 읽는다 (스폰 직후 바닥에 내려앉는 중일 수 있다)
	FMinimalViewInfo EyeView;
	DRCharacter->CalcCamera(0.f, EyeView);

	FVector Location = EyeView.Location;
	FRotator Rotation = EyeView.Rotation;
	if (Alpha < 1.0)
	{
		const IntroPath::FPath Path = IntroPath::BuildPath(IntroStartView, EyeView, *DRCharacter,
		                                                   LobbyIntroOverheadClearance, LobbyIntroFrontDistance);
		FVector Direction;
		IntroPath::Evaluate(Path, Progress, Location, Direction);
		Rotation = Direction.Rotation();
	}
	Camera->SetActorLocationAndRotation(Location, Rotation);

	UCameraComponent* CameraComponent = Camera->GetCameraComponent();
	CameraComponent->SetFieldOfView(FMath::Lerp(IntroStartView.FOV, EyeView.FOV, static_cast<float>(Progress)));

	// 3인칭 몸 숨기기 — 머리 위에서 정면을 봐 몸이 화면 아래로 빠졌을 때 (카메라가 몸 속으로 들어가려 하면 그 전에라도)
	if (!bIntroBodyHidden)
	{
		float AspectRatio = CameraComponent->AspectRatio;
		if (!CameraComponent->bConstrainAspectRatio)
		{
			int32 ViewportX = 0, ViewportY = 0;
			GetViewportSize(ViewportX, ViewportY);
			if (ViewportX > 0 && ViewportY > 0)
			{
				AspectRatio = static_cast<float>(ViewportX) / ViewportY;
			}
		}

		if ((Alpha > 0.4 && IntroPath::IsBodyBelowView(Location, Rotation, CameraComponent->FieldOfView, AspectRatio, *DRCharacter))
			|| IntroPath::IsInsideCapsule(Location, *DRCharacter))
		{
			bIntroBodyHidden = true;

			// 1인칭 상태로 — 팔은 주인 시점에서만 보여 이 카메라에는 아직 안 보인다. 머리 위 닉네임은 도착 후에 켠다
			DRCharacter->SetWaitingRoomVisibility(false);
			DRCharacter->SetOverheadWidgetVisibility(false);
		}
	}

	if (Alpha >= 1.0)
	{
		FinishIntroCamera();
	}
}

void ADRPlayerController::FinishIntroCamera()
{
	// 시작 연출 동안 막았던 엔진 카메라 자동 관리 복원
	bAutoManageActiveCameraTarget = true;

	APawn* MyPawn = GetPawn();
	if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(MyPawn))
	{
		// 일반 FPS 가시성 (3P 숨김 · 1P 보임 · 머리 위 닉네임)
		DRCharacter->SetWaitingRoomVisibility(false);
		StartArmsRaise(DRCharacter);
	}

	// ★연출 카메라를 지우기 전에★ 뷰 타깃을 옮긴다 (마지막 프레임의 연출 카메라 = 폰의 1인칭 시점이라 끊김이 없다)
	if (MyPawn)
	{
		SetViewTarget(MyPawn);
	}
	else
	{
		// 폰이 끝내 안 왔다 — 오면 HandlePossessedPawnChanged 가 시점을 옮긴다. 그동안은 대기실 카메라(로비)나 컨트롤러를 본다
		bPendingViewTargetRestore = true;
		if (IntroCamera.IsValid() && GetViewTarget() == IntroCamera.Get())
		{
			const bool bUseWaitingCamera = IntroCameraKind == EDRIntroCameraKind::Lobby && CachedWaitingRoomCamera.IsValid();
			SetViewTarget(bUseWaitingCamera ? static_cast<AActor*>(CachedWaitingRoomCamera.Get()) : this);
		}
	}

	if (IntroCameraKind == EDRIntroCameraKind::Stage)
	{
		// 로딩 완료를 알리기 전에 끝났으면(폰이 바뀜 · 끝내 안 옴) 지금 알린다 — 서버가 기다림 상한까지 붙잡고 있지 않게
		if (StageIntroStep == EDRStageIntroStep::WaitingForPawn || StageIntroStep == EDRStageIntroStep::WaitingForRender)
		{
			ServerNotifyStageLoaded();
		}
		StageIntroStep = EDRStageIntroStep::Done;
		ResetIntroCamera();

		// 함께 시작한 플레이어가 모두 끝내면 서버가 첫 페이즈 · 스테이지 BGM 을 시작한다
		ServerNotifyStageIntroFinished();

		// 연출 없이 끝났으면 로딩 화면이 아직 덮여 있다 (이미 걷었으면 아무 일도 없다)
		if (UDRLoadingScreenSubsystem* LoadingScreen = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDRLoadingScreenSubsystem>() : nullptr)
		{
			LoadingScreen->FinishLoadingScreen();
		}
	}
	else
	{
		ResetLobbyIntroState();
	}

	// 연출 중 설정 화면 등을 열었으면 입력 모드는 그 화면이 닫힐 때 맞춘다
	if (ActiveScreen == EDRScreen::None)
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}

	// HUD 오버레이 초기화 (대기실 · 연출 동안 미뤘다)
	InitOverlayForFreeRoam();
}

void ADRPlayerController::ResetIntroCamera()
{
	bIntroCameraActive = false;
	bIntroCameraMoving = false;
	bIntroBodyHidden = false;

	// 대기실 · 연출 동안 서버가 보내는 뷰 타깃(ClientSetViewTarget)을 무시하던 것 해제
	if (PlayerCameraManager)
	{
		PlayerCameraManager->bClientSimulatingViewTarget = false;
	}

	if (ACameraActor* Camera = IntroCamera.Get())
	{
		Camera->Destroy();
	}
	IntroCamera.Reset();
	IntroPawn.Reset();
}

void ADRPlayerController::ResetLobbyIntroState()
{
	bIsPlayingLobbyIntro = false;
	bPendingCameraTransition = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CameraTransitionRetryHandle);
	}

	ResetIntroCamera();
}

// ========== 스테이지 시작 연출 ==========

bool ADRPlayerController::IsStageIntroPending() const
{
	if (!IsLocalController()) return false;

	const UWorld* World = GetWorld();
	if (!World) return false;

	// 이 월드에서 진행을 정했으면 그대로 — 아직 첫 틱 전이면(트래블 직후 빙의 · HUD 초기화가 먼저 올 수 있다) 스테이지인지만 본다
	if (StageIntroWorld.Get() == World)
	{
		return StageIntroStep != EDRStageIntroStep::NotStage && StageIntroStep != EDRStageIntroStep::Done;
	}
	return IsStageWorld(*World);
}

void ADRPlayerController::ServerNotifyStageLoaded_Implementation()
{
	if (ADRStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<ADRStageGameMode>())
	{
		StageGameMode->NotifyPlayerStageLoaded(this);
	}
}

void ADRPlayerController::ServerNotifyStageIntroFinished_Implementation()
{
	if (ADRStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<ADRStageGameMode>())
	{
		StageGameMode->NotifyPlayerStageIntroFinished(this);
	}
}

void ADRPlayerController::TickStageIntro()
{
	if (!IsLocalController()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 월드가 바뀌었다 (첫 진입 · 심리스 트래블) — 원격 클라는 트래블 뒤 OnLevelEntered 가 불리지 않아 여기서 처음부터 시작한다
	if (StageIntroWorld.Get() != World)
	{
		// 이전 스테이지의 연출을 마치지 못하고 떠났다 (도중에 스테이지가 끝남) — 막아 둔 카메라 관리를 풀고 카메라를 지운다
		if (StageIntroStep != EDRStageIntroStep::NotStage && StageIntroStep != EDRStageIntroStep::Done)
		{
			if (bIntroCameraActive && IntroCameraKind == EDRIntroCameraKind::Stage)
			{
				ResetIntroCamera();
			}
			bAutoManageActiveCameraTarget = true;
		}
		StageIntroWorld = World;
		StageIntroStep = EDRStageIntroStep::NotStage;
	}

	if (StageIntroStep == EDRStageIntroStep::NotStage)
	{
		if (!IsStageWorld(*World)) return;

		// 스테이지에 들어왔다 — 연출이 끝날 때까지 빙의 · 서버(ClientSetViewTarget)가 뷰 타깃을 폰으로 옮기지 못하게 한다
		StageIntroStep = EDRStageIntroStep::WaitingForPawn;
		StageIntroEnterTime = World->GetRealTimeSeconds();
		IntroCameraKind = EDRIntroCameraKind::Stage;
		bAutoManageActiveCameraTarget = false;
		if (PlayerCameraManager)
		{
			PlayerCameraManager->bClientSimulatingViewTarget = true;
		}
	}

	// 서버가 시작 연출을 틀었는가 — 매번 다시 읽는다 (호스트는 아래에서 알리는 즉시 같은 프레임에 바뀐다)
	auto HasServerStartedIntro = [World]()
	{
		const ADRStageGameState* StageGameState = World->GetGameState<ADRStageGameState>();
		return StageGameState && StageGameState->GetStageStartState() != EDRStageStartState::WaitingForPlayers;
	};

	if (StageIntroStep == EDRStageIntroStep::WaitingForPawn)
	{
		// ★폰의 BeginPlay 뒤에 시작한다★ — 원격 클라는 빙의가 BeginPlay 보다 먼저 오고,
		// BeginPlay 의 UpdateMeshVisibility 가 3인칭 몸을 다시 숨긴다 (로비 연출과 같은 이유)
		ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
		if (!DRCharacter || !DRCharacter->HasActorBegunPlay() || !GetPlayerState<ADRPlayerState>())
		{
			// 폰이 끝내 안 온다 (관전 등) — 서버가 시작하고도 한참이면 연출 없이 끝내 로딩 화면 · 입력이 갇히지 않게 한다
			constexpr double PawnWaitLimit = 15.0;
			if (HasServerStartedIntro() && World->GetRealTimeSeconds() - StageIntroEnterTime > PawnWaitLimit)
			{
				UE_LOG(LogDR, Warning, TEXT("[StageIntro] 폰이 %.0f초 안에 오지 않아 연출 없이 시작한다"), PawnWaitLimit);
				FinishIntroCamera();
			}
			return;
		}

		// 연출이 끝나면 스폰 방향을 수평으로 본다 — 출발 시점도 이 방향 멀리다
		SetControlRotation(FRotator(0.f, DRCharacter->GetActorRotation().Yaw, 0.f));
		if (!BeginIntroCamera(DRCharacter, MakeStageIntroStartView(*DRCharacter)))
		{
			FinishIntroCamera();
			return;
		}

		StageIntroStep = EDRStageIntroStep::WaitingForRender;
		StageRenderWaitStartTime = World->GetRealTimeSeconds();
		NextStageRenderCheckTime = 0.0;
	}

	if (StageIntroStep == EDRStageIntroStep::WaitingForRender)
	{
		// 로딩 화면 아래에서 메시가 그릴 준비를 마치길 기다린다 — 패키지 빌드는 PSO 컴파일이 끝나야 메시가 보여
		// 그냥 걷으면 연출 도중 배경 · 캐릭터가 튀어나온다 (메인 메뉴와 같은 문제). 전체 확인이라 0.2초마다
		const double Now = World->GetRealTimeSeconds();
		if (Now < NextStageRenderCheckTime) return;
		NextStageRenderCheckTime = Now + 0.2;

		const double Waited = Now - StageRenderWaitStartTime;
		const bool bReady = IsWorldReadyToRender(*World);
		if (!bReady && Waited < StageIntroMaxRenderWaitTime) return;

		if (bReady)
		{
			UE_LOG(LogDR, Log, TEXT("[StageIntro] 로딩 완료 (화면 준비 %.2f초) — 서버에 알림"), Waited);
		}
		else
		{
			UE_LOG(LogDR, Warning, TEXT("[StageIntro] 메시가 %.1f초 안에 그릴 준비를 마치지 못했지만 로딩 완료로 알린다"), Waited);
		}

		ServerNotifyStageLoaded();
		StageIntroStep = EDRStageIntroStep::WaitingForPlayers;
	}

	UDRLoadingScreenSubsystem* LoadingScreen = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDRLoadingScreenSubsystem>() : nullptr;

	if (StageIntroStep == EDRStageIntroStep::WaitingForPlayers)
	{
		// 모두 로딩을 마쳤다 (서버 진행 Intro — 늦게 온 플레이어는 이미 Started) — 로딩 화면을 걷는다
		if (!HasServerStartedIntro()) return;

		if (LoadingScreen)
		{
			LoadingScreen->FinishLoadingScreen();
		}
		StageIntroRevealTime = World->GetTimeSeconds();
		StageIntroStep = EDRStageIntroStep::Revealing;
	}

	if (StageIntroStep == EDRStageIntroStep::Revealing)
	{
		// 로딩 화면이 다 걷힐 때까지 출발 시점에 머문다 — 걷히는 동안(퇴장 애니) 움직이면 멀리서 다가오는 앞부분이 가려진다
		// (2026-11-08 PIE: 최소 표시 시간 + 퇴장 애니가 3초 중 앞 1초를 덮었다). 로딩 화면이 끝내 안 닫혀도 상한 뒤에는 출발한다
		constexpr double MaxLoadingScreenWait = 3.0;
		const double Elapsed = World->GetTimeSeconds() - StageIntroRevealTime;
		const bool bLoadingScreenVisible = LoadingScreen && LoadingScreen->IsLoadingScreenVisible();
		if (Elapsed < StageIntroHoldTime || (bLoadingScreenVisible && Elapsed < StageIntroHoldTime + MaxLoadingScreenWait)) return;

		StartIntroCameraMove(StageIntroDuration);
		StageIntroStep = EDRStageIntroStep::Playing;
	}

	// Playing — TickIntroCamera 가 옮기고, 도착하면 FinishIntroCamera 가 Done 으로 바꾼다
}

FMinimalViewInfo ADRPlayerController::MakeStageIntroStartView(ADRCharacter& DRCharacter) const
{
	// 도착할 1인칭 시점에서 시작한다 — FOV · 후처리가 연출 도중에 바뀌지 않게
	FMinimalViewInfo View;
	DRCharacter.CalcCamera(0.f, View);

	const FVector Focus = IntroPath::GetFocus(DRCharacter);
	const FRotator FacingYaw(0.f, View.Rotation.Yaw, 0.f);
	const FVector Forward = FacingYaw.Vector();
	const FVector Right = FRotationMatrix(FacingYaw).GetUnitAxis(EAxis::Y);

	// 정면에서 살짝(거리의 1/10) 비켜 선다 — 경로는 출발 시점이 있는 쪽 어깨로 넘으므로, 이웃이 덜한 쪽으로 비킨다
	const double Side = ChooseIntroSide(DRCharacter, Right);
	const FVector Desired = Focus + Forward * StageIntroStartDistance + Right * (Side * 0.1 * StageIntroStartDistance)
	                      + FVector::UpVector * StageIntroStartHeight;

	// 벽 · 문 등 카메라를 막는 면에 걸리면 그 앞에서 출발한다. Camera 채널이라 트리거 박스 같은 겹침 전용 볼륨은 지나간다
	// (오브젝트 타입으로 쟀더니 문 · 방 트리거에 걸려 7m 안쪽에서 출발했다). 스테이지 메시는 단순 충돌이 없어
	// (충돌은 블로킹 볼륨이 맡는다) 보이는 면은 복합 충돌로 잰다. 플레이어 캐릭터들은 무시한다
	FVector Start = Desired;
	if (UWorld* World = GetWorld())
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DRStageIntroStartView), /*bInTraceComplex*/ true);
		for (TActorIterator<ADRCharacter> It(World); It; ++It)
		{
			QueryParams.AddIgnoredActor(*It);
		}

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Focus, Desired, ECC_Camera, QueryParams))
		{
			// 면에서 조금 떨어진 곳. 그래도 경로의 다음 키(얼굴 앞)보다는 멀리 — 더 가까우면 물러났다가 다가온다
			const double MinDistance = DRCharacter.GetCapsuleComponent()->GetScaledCapsuleRadius() + LobbyIntroFrontDistance + 30.0;
			Start = Focus + (Desired - Focus).GetSafeNormal() * FMath::Max(Hit.Distance - 30.0, MinDistance);
		}
	}

	View.Location = Start;
	View.Rotation = (Focus - Start).Rotation();
	return View;
}

void ADRPlayerController::StartArmsRaise(ADRCharacter* DRCharacter)
{
	StopArmsRaise();

	USkeletalMeshComponent* Arms = DRCharacter ? DRCharacter->FirstPersonMesh.Get() : nullptr;
	if (!Arms || !GetWorld() || LobbyIntroArmsRaiseTime <= 0.f || LobbyIntroArmsRaiseDepth <= 0.f) return;

	// 1인칭 메시는 카메라에 붙어 있어 상대 위치의 -Z 가 화면 아래쪽이다
	ArmsRaiseMesh = Arms;
	ArmsRaiseBaseLocation = Arms->GetRelativeLocation();
	ArmsRaiseStartTime = GetWorld()->GetTimeSeconds();
	Arms->SetRelativeLocation(ArmsRaiseBaseLocation - FVector(0.f, 0.f, LobbyIntroArmsRaiseDepth));
}

void ADRPlayerController::TickArmsRaise()
{
	USkeletalMeshComponent* Arms = ArmsRaiseMesh.Get();
	if (!Arms || !GetWorld())
	{
		ArmsRaiseMesh.Reset();
		return;
	}

	const double Alpha = FMath::Clamp((GetWorld()->GetTimeSeconds() - ArmsRaiseStartTime) / FMath::Max(LobbyIntroArmsRaiseTime, 0.01f), 0.0, 1.0);
	const double Remaining = FMath::Pow(1.0 - Alpha, 3.0);	// ease-out — 빠르게 올라와 부드럽게 멈춘다
	Arms->SetRelativeLocation(ArmsRaiseBaseLocation - FVector(0.0, 0.0, LobbyIntroArmsRaiseDepth * Remaining));

	if (Alpha >= 1.0)
	{
		ArmsRaiseMesh.Reset();
	}
}

void ADRPlayerController::StopArmsRaise()
{
	if (USkeletalMeshComponent* Arms = ArmsRaiseMesh.Get())
	{
		Arms->SetRelativeLocation(ArmsRaiseBaseLocation);
	}
	ArmsRaiseMesh.Reset();
}

void ADRPlayerController::HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn)
{
	// 폰 교체(사망/리스폰/클래스 변경) 시 홀드 상태 초기화 —
	// 키를 누른 채 폰이 바뀌면 Released 입력이 유실돼 태그가 눌린 채 남을 수 있다
	HeldInputTags.Reset();

	if (!NewPawn) return;

	// 캐릭터별 상하 시야각 제한 — 청소기만 ±ViewPitchLimit(기본 30도), 그 외 캐릭터는 기본값 복원.
	// OnPossessedPawnChanged는 서버·소유 클라 양쪽에서 발화하고, 카메라 클램프는 로컬 카메라 매니저에만 의미가 있다.
	if (PlayerCameraManager)
	{
		if (const ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(NewPawn))
		{
			PlayerCameraManager->ViewPitchMin = -Vacuum->ViewPitchLimit;
			PlayerCameraManager->ViewPitchMax = Vacuum->ViewPitchLimit;
		}
		else
		{
			PlayerCameraManager->ViewPitchMin = -ViewPitchMin;
			PlayerCameraManager->ViewPitchMax = ViewPitchMax;
		}
	}

	// ViewTarget 복원 대기 중이었으면 즉시 복원 (기존 0.5초 재시도 타이머 대체)
	if (bPendingViewTargetRestore)
	{
		bPendingViewTargetRestore = false;

		// 대기실이면 ViewTarget 복원 스킵 (ClientSetWaitingRoomView가 담당)
		if (!bIsInWaitingRoom)
		{
			SetViewTarget(NewPawn);
			RestoreDefaultInputMode();
		}
	}

	// 카메라 전환 대기 중이었으면 즉시 실행 (기존 0.1초 폴링 대체)
	if (bPendingCameraTransition)
	{
		bPendingCameraTransition = false;
		GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);
		ExecuteCameraTransitionToCharacter();
	}
}

void ADRPlayerController::OnRep_Pawn()
{
	// 엔진 기본 처리 (AcknowledgePossession 등)
	Super::OnRep_Pawn();

	// 대기실에서는 엔진이 변경한 ViewTarget을 즉시 복원
	if (bIsInWaitingRoom && CachedWaitingRoomCamera.IsValid())
	{
		bAutoManageActiveCameraTarget = false;
		SetViewTargetWithBlend(CachedWaitingRoomCamera.Get(), 0.f);
	}
}

void ADRPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트가 자기 PlayerState 를 처음 알게 되는 시점 — 여기서 장착 칩을 보고한다.
	// (PlayerState 의 BeginPlay 시점에는 컨트롤러 연결이 아직 없을 수 있다. Plan2.md 7.1)
	ReportUpgradeLoadout();
	ReportCosmeticLoadout();	// 옷도 같은 이유로 (Plan(옷장) M3)
}

void ADRPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	// 대기실에서는 ViewTarget 자동 전환만 차단하고, 나머지 엔진 초기화는 정상 수행
	if (bIsInWaitingRoom)
	{
		// bAutoManageActiveCameraTarget = false로 Super 내부의 ViewTarget 자동 전환 차단
		bAutoManageActiveCameraTarget = false;

		// Super 호출: ResetIgnoreInputFlags, AcknowledgePossession, PawnClientRestart 등
		// 엔진 초기화를 정상 수행 (호스트에서 Look/Move 입력이 무시되는 버그 방지)
		Super::ClientRestart_Implementation(NewPawn);

		// 새 폰에 대기실 가시성 적용
		if (NewPawn)
		{
			if (ADRCharacter* DRChar = Cast<ADRCharacter>(NewPawn))
			{
				DRChar->SetWaitingRoomVisibility(true);
			}
		}

		// Super가 ViewTarget을 변경했을 수 있으므로 캐시된 카메라로 즉시 복원
		if (CachedWaitingRoomCamera.IsValid())
		{
			SetViewTargetWithBlend(CachedWaitingRoomCamera.Get(), 0.f);
		}

		return;
	}

	Super::ClientRestart_Implementation(NewPawn);
}

void ADRPlayerController::ClientKicked_Implementation(const FString& Reason)
{
	// 대기실 UI 제거
	DestroyWaitingRoomUI();

	// 세션 떠나기 → 메인 메뉴로 이동
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMultiplayerSessionsSubsystem* Subsystem = GI->GetSubsystem<UMultiplayerSessionsSubsystem>())
		{
			Subsystem->LeaveServer();
		}
	}
}

// ========== 대기실 UI ==========

void ADRPlayerController::CreateWaitingRoomUI()
{
	if (!IsLocalController()) return;
	if (WaitingRoomWidget) return; // 이미 존재
	if (!WaitingRoomWidgetClass) return;

	WaitingRoomWidget = CreateWidget<UDRWaitingRoomWidget>(this, WaitingRoomWidgetClass);
	if (WaitingRoomWidget)
	{
		WaitingRoomWidget->AddToViewport();

		// 호스트 여부 설정 (Listen Server의 로컬 컨트롤러 = 호스트)
		bool bIsHost = HasAuthority();
		WaitingRoomWidget->SetIsHost(bIsHost);

		// LobbyState 변경 구독
		if (ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>())
		{
			LGS->OnLobbyStateChanged.AddDynamic(this, &ADRPlayerController::OnLobbyStateChangedForUI);
		}

		RefreshWaitingRoomUI();
	}
}

void ADRPlayerController::DestroyWaitingRoomUI()
{
	// 델리게이트 해제
	if (UWorld* World = GetWorld())
	{
		if (ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>())
		{
			LGS->OnLobbyStateChanged.RemoveDynamic(this, &ADRPlayerController::OnLobbyStateChangedForUI);
		}
	}

	if (WaitingRoomWidget)
	{
		WaitingRoomWidget->RemoveFromParent();
		WaitingRoomWidget = nullptr;
	}
}

void ADRPlayerController::ClientRefreshWaitingRoomUI_Implementation()
{
	RefreshWaitingRoomUI();
}

void ADRPlayerController::RefreshWaitingRoomUI()
{
	if (!WaitingRoomWidget) return;

	ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
	ADRGameStateBase* GS = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!LGS || !GS) return;

	// 로컬 PlayerState 유효성 검사 + 재시도
	APlayerState* MyPS = GetPlayerState<APlayerState>();
	if (!MyPS)
	{
		FTimerHandle RetryTimer;
		GetWorldTimerManager().SetTimer(
			RetryTimer,
			[WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
			{
				if (ADRPlayerController* PC = WeakThis.Get())
				{
					PC->RefreshWaitingRoomUI();
				}
			},
			0.2f, false
		);
		return;
	}

	TArray<FWaitingRoomPlayerInfo> Infos;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) continue;
		ADRPlayerState* DRPS = Cast<ADRPlayerState>(PS);
		if (!DRPS) continue;

		FWaitingRoomPlayerInfo Info;
		Info.PlayerName = PS->GetPlayerName();
		Info.SelectedClass = DRPS->GetSelectedPlayerClass();
		Info.bIsHost = GS->IsPlayerHost(PS);
		Info.bIsReady = Info.bIsHost ? true : DRPS->IsReady(); // 호스트는 항상 준비 상태로 표시
		Info.OwningPlayerState = PS;

		// 서버 권위 슬롯 인덱스 사용
		Info.SlotIndex = DRPS->GetWaitingRoomSlotIndex();

		// bIsLocalPlayer 판정 (PlayerId 비교)
		Info.bIsLocalPlayer = (PS->GetPlayerId() == MyPS->GetPlayerId());

		Infos.Add(Info);
	}

	// SlotIndex 기준으로 정렬하여 UI에 전달
	Infos.Sort([](const FWaitingRoomPlayerInfo& A, const FWaitingRoomPlayerInfo& B)
	{
		return A.SlotIndex < B.SlotIndex;
	});

	WaitingRoomWidget->RefreshPlayerSlots(Infos);
}

void ADRPlayerController::OnLobbyStateChangedForUI(ELobbyState NewState)
{
	if (NewState == ELobbyState::Transitioning || NewState == ELobbyState::FreeRoam)
	{
		DestroyWaitingRoomUI();
	}
}

void ADRPlayerController::InitOverlayForFreeRoam()
{
	ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD());
	if (!DRHUD) return;

	ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
	if (!PS) return;

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	UAttributeSet* AS = PS->GetAttributeSet();
	if (!ASC || !AS) return;

	DRHUD->InitOverlay(this, PS, ASC, AS);

	// 위젯 트리 완전 초기화 후 어빌리티 아이콘 강제 갱신
	// 위젯 트리 구성이 끝난 다음 틱에 갱신 (매직 딜레이 대신 프레임 확정 타이밍)
	GetWorldTimerManager().SetTimerForNextTick(
		[WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
		{
			ADRPlayerController* PC = WeakThis.Get();
			if (!PC) return;

			ADRHUD* HUD = Cast<ADRHUD>(PC->GetHUD());
			if (!HUD) return;

			ADRPlayerState* PlayerState = PC->GetPlayerState<ADRPlayerState>();
			if (!PlayerState) return;

			UAbilitySystemComponent* AbilitySystem = PlayerState->GetAbilitySystemComponent();
			UAttributeSet* Attributes = PlayerState->GetAttributeSet();
			if (!AbilitySystem || !Attributes) return;

			const FWidgetControllerParams Params(PC, PlayerState, AbilitySystem, Attributes);
			if (UOverlayWidgetController* WC = HUD->GetOverlayWidgetController(Params))
			{
				WC->BroadcastAbilityInfo();
			}
		}
	);
}

void ADRPlayerController::ServerRequestPowerOn_Implementation()
{
	// 호스트 검증: Server RPC이므로 서버에서 실행됨
	// IsLocalController()로 호스트(Listen Server) 확인
	if (!IsLocalController() || !HasAuthority()) return;

	ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
	if (LobbyGM)
	{
		LobbyGM->PowerOn(this);
	}
}

void ADRPlayerController::OnPowerOnButtonPressed()
{
	// 버튼 클릭 시 로컬에서 호출됨. 호스트(리슨 서버)면 PowerOn 시도, 원격 클라이언트면 준비 토글.
	if (HasAuthority())
	{
		ServerRequestPowerOn();
	}
	else
	{
		ServerToggleReady();
	}
}

void ADRPlayerController::ServerToggleReady_Implementation()
{
	// 클라이언트 요청 → 서버에서 실행. 리슨 서버 호스트는 준비 토글 대상이 아님.
	// (Server RPC 안이므로 HasAuthority()는 항상 true - 실질 판별은 로컬 컨트롤러 여부)
	if (IsListenServerHost()) return;

	ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
	if (!LobbyGM) return;

	ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
	if (!PS) return;

	LobbyGM->SetPlayerReady(this, !PS->IsReady());
}

void ADRPlayerController::ServerRequestKickPlayer_Implementation(APlayerState* TargetPlayerState)
{
	// 호스트 검증
	if (!IsLocalController() || !HasAuthority()) return;
	if (!TargetPlayerState) return;

	// TargetPlayerState → PlayerController 찾기
	APlayerController* TargetPC = Cast<APlayerController>(TargetPlayerState->GetOwner());
	ADRPlayerController* TargetDRPC = Cast<ADRPlayerController>(TargetPC);

	ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
	if (LobbyGM && TargetDRPC)
	{
		LobbyGM->KickPlayer(this, TargetDRPC);
	}
}

