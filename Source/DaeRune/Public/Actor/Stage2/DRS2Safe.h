// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Phase/Stage2/DRS2Types.h"
#include "DRS2Safe.generated.h"

class ADRS2CodeScreen;
class ADRCleanserPart;
class ADRS2SafeButton;
class UMaterialInstanceDynamic;

/**
 * 금고 (Plan6 §14.2.5)
 *
 * 실제 모델의 버튼(ADRS2SafeButton)을 눌러 3자리를 입력하고 Enter 로 판정한다.
 * 자리 순서: ① 8퍼즐 ② 스위치 퍼즐 ③ CCTV 관찰 개수
 *
 * 버튼 4종: 숫자(0~9) / Delete(맨 뒤 1자리) / Reset(전체) / Enter(확인)
 * ★자릿수가 다 차도 자동으로 판정하지 않는다. 판정은 Enter 를 눌러야 일어난다.
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

	// 숫자 버튼 입력 (서버). 자릿수가 가득 차면 무시한다.
	// ★자동 판정하지 않는다 - 판정은 SubmitCode() 로만 일어난다.
	void PushDigit(uint8 Digit);

	// Delete 버튼: 맨 뒤 1자리를 지운다 (서버)
	void DeleteLastDigit();

	// Reset 버튼: 입력 전체 초기화 (서버)
	void ClearInput();

	// Enter 버튼: 현재 입력을 정답과 비교한다 (서버).
	// 일치하면 개방, 아니면 입력 초기화 + 오답 연출.
	void SubmitCode();

	// 버튼이 BeginPlay 에서 자기를 등록한다 (서버·클라 공통).
	// 등록된 버튼은 ① 문(DoorMesh)에 부착되어 함께 움직이고
	//              ② 금고 개방 시 조작이 차단된다.
	void RegisterButton(ADRS2SafeButton* Button);

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

	// ========== 입력 표시 (금고 본체 메시에 직접) ==========
	//
	// 금고 메시에 자릿수만큼 머티리얼 슬롯이 있고, 각 슬롯 머티리얼에 텍스처 파라미터가 있다.
	// 그 파라미터를 숫자 텍스처로 교체해 입력 상황을 보여준다.
	// (별도 표시판 액터가 필요 없다 - 화면이 금고 모델의 일부이기 때문이다.)

	// 숫자를 표시할 머티리얼 슬롯 번호들. 배열 순서 = 자릿수 순서(왼쪽부터).
	// 스태틱 메시 에디터의 Material Slots 목록에서 번호를 확인한다. 예: [3, 4, 5]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Safe|표시")
	TArray<int32> DigitMaterialSlots;

	// true 면 DoorMesh, false 면 SafeBodyMesh 의 슬롯을 사용한다.
	// 화면이 문에 달려 있으면 true (문이 열릴 때 함께 회전한다).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Safe|표시")
	bool bDisplayOnDoorMesh = true;

	// 머티리얼의 텍스처 파라미터 이름.
	// ★에셋 머티리얼 인스턴스의 파라미터 이름과 정확히 일치해야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Safe|표시")
	FName TextureParameterName = TEXT("BaseTexture");

	// 숫자 텍스처 0~9. ★인덱스가 곧 숫자다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Safe|표시")
	TArray<TObjectPtr<UTexture2D>> DigitTextures;

	// 미입력 자리에 쓸 텍스처 ("-" 또는 빈 화면). 비우면 텍스처를 지운다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Safe|표시")
	TObjectPtr<UTexture2D> EmptySlotTexture;

	// (선택) 별도 표시판 액터를 쓰는 경우. 위 슬롯 방식과 함께 써도 된다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Safe|표시")
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

	// 표시 갱신 (금고 메시 슬롯 + 선택적 표시판 액터)
	void RefreshDisplay();

	// 금고 메시의 자리 슬롯마다 MID 를 만든다 (BeginPlay 1회)
	void EnsureDisplayMIDs();

	// 자릿값 배열을 텍스처로 반영한다
	void ApplyDigitTextures(const TArray<int32>& DisplayDigits);

	// 자릿값 -> 텍스처 (음수/범위 밖이면 EmptySlotTexture)
	UTexture2D* DigitToTexture(int32 Digit) const;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DisplayMIDs;

	// 등록된 버튼들 (서버·클라 양쪽에 존재)
	UPROPERTY()
	TArray<TObjectPtr<ADRS2SafeButton>> RegisteredButtons;

	// 서버 전용 정답 코드
	TArray<uint8> SecretCode;
};
