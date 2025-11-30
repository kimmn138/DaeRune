// Copyright DaeRune


#include "Game/DRDetectionManager.h"
#include "Game/DRGameStateBase.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"

ADRDetectionManager::ADRDetectionManager()
{
	PrimaryActorTick.bCanEverTick = false;

	// 서버에서만 의미 있음
	bReplicates = false;
}

void ADRDetectionManager::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 실행
    if (!HasAuthority()) return;

    // 미리 계산
    FOVCosine = FMath::Cos(FMath::DegreesToRadians(PlayerFOVAngle / 2.f));
    DetectionRangeSq = DetectionRange * DetectionRange;

    // 타이머 시작
    GetWorldTimerManager().SetTimer(
        DetectionTimerHandle,
        this,
        &ADRDetectionManager::PerformDetectionCheck,
        CheckInterval,
        true  // 반복
    );
}

void ADRDetectionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(DetectionTimerHandle);

    Super::EndPlay(EndPlayReason);
}

void ADRDetectionManager::PerformDetectionCheck()
{
    // 살아있는 플레이어 수집
    TArray<ADRCharacter*> Players = GetAlivePlayers();
    if (Players.Num() == 0) return;

    // 플레이어들 주변 적 수집
    TArray<ADREnemy*> CandidateEnemies = GetEnemiesInDetectionRange(Players);
    if (CandidateEnemies.Num() == 0) return;

    // 각 적에 대해 가시성 체크
    for (ADREnemy* Enemy : CandidateEnemies)
    {
        if (!IsValid(Enemy)) continue;

        bool bSeenByAnyone = false;

        // 아무 플레이어 한 명이라도 보면 발각
        for (const ADRCharacter* Player : Players)
        {
            if (CanPlayerSeeEnemy(Player, Enemy))
            {
                bSeenByAnyone = true;
                break;
            }
        }

        // 태그 업데이트
        UpdateEnemyDetectionTag(Enemy, bSeenByAnyone);
    }
}

TArray<ADRCharacter*> ADRDetectionManager::GetAlivePlayers() const
{
    if (ADRGameStateBase* GameState = GetWorld()->GetGameState<ADRGameStateBase>())
    {
        return GameState->GetAlivePlayers();
    }

    return TArray<ADRCharacter*>();
}

TArray<ADREnemy*> ADRDetectionManager::GetEnemiesInDetectionRange(const TArray<ADRCharacter*>& Players) const
{
    // 중복 제거
    TSet<ADREnemy*> EnemySet;

    for (const ADRCharacter* Player : Players)
    {
        if (!IsValid(Player)) continue;

        // OverlapSphere로 범위 내 적 수집
        TArray<FOverlapResult> Overlaps;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(Player);

        GetWorld()->OverlapMultiByChannel(
            Overlaps,
            Player->GetActorLocation(),
            FQuat::Identity,
            ECC_Pawn,  // 적이 Pawn 채널이라고 가정
            FCollisionShape::MakeSphere(DetectionRange),
            QueryParams
        );

        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (ADREnemy* Enemy = Cast<ADREnemy>(Overlap.GetActor()))
            {
                if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Enemy))
                {
                    if (!CombatInterface->Execute_IsDead(Enemy))
                    {
                        EnemySet.Add(Enemy);
                    }
                }
            }
        }
    }

    return EnemySet.Array();
}

bool ADRDetectionManager::CanPlayerSeeEnemy(const ADRCharacter* Player, const ADREnemy* Enemy) const
{
    if (!IsValid(Player) || !IsValid(Enemy)) return false;

    const FVector PlayerLocation = Player->GetActorLocation();
    const FVector EnemyLocation = Enemy->GetActorLocation();

    // 거리 체크
    const float DistSq = FVector::DistSquared(PlayerLocation, EnemyLocation);
    if (DistSq > DetectionRangeSq) return false;

    // 각도 체크
    const FVector ToEnemy = (EnemyLocation - PlayerLocation).GetSafeNormal();
    const FVector PlayerForward = Player->GetControlRotation().Vector();

    const float DotResult = FVector::DotProduct(ToEnemy, PlayerForward);
    if (DotResult < FOVCosine)
    {
        return false;  // 시야각 밖
    }

    // 라인트레이스
    FHitResult HitResult;
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(Player);
    TraceParams.AddIgnoredActor(Enemy);

    // 플레이어 눈 위치에서 적 중심으로 트레이스
    const FVector EyeLocation = Player->GetPawnViewLocation();

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        EyeLocation,
        EnemyLocation,
        VisibilityChannel,
        TraceParams
    );

    // 뭔가에 막혔으면 안 보이는 것
    if (bHit)
    {
        return false;
    }

    return true; 
}

void ADRDetectionManager::UpdateEnemyDetectionTag(ADREnemy* Enemy, bool bIsDetected) const
{
    UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
    if (!ASC) return;

    const FGameplayTag DetectedTag = FDRGameplayTags::Get().Enemy_Detected;
    const bool bCurrentlyDetected = ASC->HasMatchingGameplayTag(DetectedTag);

    // 상태 변화 있을 때만 처리
    if (bIsDetected && !bCurrentlyDetected)
    {
        ASC->AddLooseGameplayTag(DetectedTag);
    }
    else if (!bIsDetected && bCurrentlyDetected)
    {
        ASC->RemoveLooseGameplayTag(DetectedTag);
    }
}
