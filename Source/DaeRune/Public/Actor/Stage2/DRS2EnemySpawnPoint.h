// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2EnemySpawnPoint.generated.h"

class UBillboardComponent;
class UArrowComponent;

/**
 * 스테이지2 적 스폰 지점 (Plan6 §14.1.5)
 *
 * 스테이지1의 스폰 포인트 방식(레벨에 배치 후 런타임에 수집)을 계승하되,
 * 태그 문자열 대신 RoomID 프로퍼티로 방을 구분하는 전용 액터로 만든다.
 * 태그 오타로 스폰 지점이 조용히 누락되는 사고를 막기 위함이다.
 *
 * 사용법:
 *   1) 레벨의 방 안에 이 액터를 배치한다 (에디터에서 화살표 = 스폰 시 바라볼 방향).
 *   2) RoomID 를 해당 방으로 지정한다 ("Room1" / "Room3" / "Room5").
 *   3) 페이즈가 시작될 때 RoomID 로 자동 수집된다. Director 배선은 필요 없다.
 *
 * 스폰 지점 선택은 비반복 랜덤 풀 방식이다 (모든 지점을 한 번씩 쓴 뒤 다시 채운다).
 */
UCLASS()
class DAERUNE_API ADRS2EnemySpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ADRS2EnemySpawnPoint();

	FName GetRoomID() const { return RoomID; }

	// 공중 유닛(잠자리) 전용 지점인지. 방5처럼 공중 비중이 큰 방에서 사용한다.
	bool IsAirSpawn() const { return bAirSpawn; }

protected:
	// 이 스폰 지점이 속한 방. 페이즈가 이 값으로 수집한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|SpawnPoint")
	FName RoomID = TEXT("Room1");

	// 공중 스폰 지점 표시 (지상/공중 구분이 필요해지면 페이즈가 필터링에 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|SpawnPoint")
	bool bAirSpawn = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|SpawnPoint")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	// 에디터에서 위치/방향을 눈으로 확인하기 위한 표시용 컴포넌트 (런타임 영향 없음)
	UPROPERTY()
	TObjectPtr<UBillboardComponent> EditorBillboard;

	UPROPERTY()
	TObjectPtr<UArrowComponent> EditorArrow;
#endif
};
