// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Phase/Stage2/DRS2Types.h"
#include "DRS2Safe.generated.h"

class ADRS2CodeScreen;
class ADRCleanserPart;

/**
 * 금고 (Plan6 §14.2.5)
 *
 * 실제 모델의 숫자 버튼(ADRS2SafeButton)을 눌러 3자리를 입력한다.
 * 자리 순서: ① 8퍼즐 ② 스위치 퍼즐 ③ CCTV 관찰 개수
 *
 * 정답이면 문이 열리고 내부에 부품이 스폰된다. 오답이면 입력만 초기화되며
 * 페널티는 없다 (요구사항 "실패 시 리셋 없음"과 일관).
 *
 * 정답 코드는 서버 전용이며 끝까지 복제하지 않는다. 복제되는 것은 입력 진행 상황과 개방 여부뿐이다.
 */
UCLASS()
class DAERUNE_API ADRS2Safe : public AActor
{
	GENERATED_BODY()

public:
	ADRS2Safe();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 페이즈가 매 판 생성한 3자리 코드를 주입한다 (서버 전용)
	void SetSecretCode(const TArray<uint8>& InCode);

	// 숫자 버튼 입력 (서버). 자리 수가 차면 자동 판정한다.
	void PushDigit(uint8 Digit);

	// 입력 초기화 (서버)
	void ClearInput();

	UFUNCTION(BlueprintCallable, Category = "S2|Safe")
	bool IsOpened() const { return bOpened; }

	// 금고 개방 알림 (스폰된 부품을 전달)
	UPROPERTY(BlueprintAssignable, Category = "S2|Safe")
	FOnS2SafeOpened OnSafeOpened;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Safe")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Safe")
	TObjectPtr<UStaticMeshComponent> SafeBodyMesh;

	// 개방 연출용 문 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Safe")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	// 부품 스폰 위치 (금고 내부)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Safe")
	TObjectPtr<USceneComponent> PartSpawnPoint;

	// 입력 진행 표시판
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Safe")
	TObjectPtr<ADRS2CodeScreen> InputDisplay;

	// 금고 안에 들어 있는 부품
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Safe")
	TSubclassOf<ADRCleanserPart> PartClass;

	// 비밀번호 자리 수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Safe", meta = (ClampMin = "1"))
	int32 CodeLength = 3;

	// ========== 복제 상태 ==========

	// 현재까지 입력된 숫자 (팀 전원에게 표시)
	UPROPERTY(ReplicatedUsing = OnRep_InputDigits, BlueprintReadOnly, Category = "S2|Safe")
	TArray<uint8> InputDigits;

	UPROPERTY(ReplicatedUsing = OnRep_bOpened, BlueprintReadOnly, Category = "S2|Safe")
	bool bOpened = false;

	UFUNCTION() void OnRep_InputDigits();
	UFUNCTION() void OnRep_bOpened();

	// ========== 연출 훅 ==========

	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Safe")
	void OnSafeOpenedVisual();

	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Safe")
	void OnWrongCodeVisual();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayWrongCodeFX();

private:
	// 입력이 다 찼을 때 검증 (서버)
	void ValidateInput();

	// 개방 처리 + 부품 스폰 (서버)
	void OpenSafe();

	// 표시판 갱신
	void RefreshDisplay();

	// 서버 전용 정답 코드
	TArray<uint8> SecretCode;
};
