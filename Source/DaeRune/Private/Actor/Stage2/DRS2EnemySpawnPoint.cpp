// Copyright DaeRune

#include "Actor/Stage2/DRS2EnemySpawnPoint.h"

#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"

ADRS2EnemySpawnPoint::ADRS2EnemySpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// 순수 위치 마커. 복제할 상태가 없다 (서버가 위치만 읽어 스폰한다).
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	EditorBillboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorBillboard"));
	if (EditorBillboard)
	{
		EditorBillboard->SetupAttachment(SceneRoot);
		EditorBillboard->bIsScreenSizeScaled = true;
	}

	EditorArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("EditorArrow"));
	if (EditorArrow)
	{
		EditorArrow->SetupAttachment(SceneRoot);
		EditorArrow->ArrowColor = FColor(255, 96, 32);
		EditorArrow->ArrowSize = 1.5f;
	}
#endif
}
