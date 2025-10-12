// Copyright DaeRune


#include "PlayLoop/DRPhase1.h"
#include "PlayLoop/StageManager.h"
#include "PlayLoop/CleanserSite.h"
#include "Kismet/GameplayStatics.h"	

void UDRPhase1::Enter()
{
	// 매니저 존재 체크
	if (!Manager)
	{ UE_LOG(LogTemp, Warning, TEXT("StageManager(Manager)가 null 입니다")); return; }

	// 클렌저사이트 존재 체크
	if(!CleanserSiteClass)
	{ UE_LOG(LogTemp, Warning, TEXT("CleanserSiteClass is empty")); return; }

	// 1) 스폰 포인트 찾기
	AActor* Point = FindRandomSpawnPoint();
	if (!Point) 
	{ UE_LOG(LogTemp, Warning, TEXT("No spawn point with tag")); return; }

	// 2) 클렌저 사이트 스폰, 초기화
	UWorld* W = Manager->GetWorld();
	if (!W) return;

	ActiveSite = W->SpawnActor<ACleanserSite>(CleanserSiteClass, Point->GetActorTransform());
	if (ActiveSite)
	{
		// 사이트 완료 이벤트 바인딩
		ActiveSite->OnSiteCleared.AddDynamic(this, &UDRPhase1::OnSiteCleared);
		ActiveSite->InitializeAndSpawn(); // 주변 적 4마리 스폰
	}
}

void UDRPhase1::Exit()
{
	if (ActiveSite)
	{
		ActiveSite->OnSiteCleared.RemoveDynamic(this, &UDRPhase1::OnSiteCleared);
		ActiveSite = nullptr;
	}
}

void UDRPhase1::OnSiteCleared(ACleanserSite* /*Site*/)
{
	if (!Manager) return;

	// 페이즈1 끝났다고 알림
	Manager->OnPhase1Completed.Broadcast();
}

AActor* UDRPhase1::FindRandomSpawnPoint() const
{
	if (!Manager) return nullptr;
	UWorld* W = Manager->GetWorld();
	if (!W) return nullptr;

	TArray<AActor*> Points;
	UGameplayStatics::GetAllActorsWithTag(W, SpawnPointTag, Points);
	if (Points.Num() == 0) return nullptr;

	int32 Index = FMath::RandRange(0, Points.Num() - 1);
	return Points[Index];
}
