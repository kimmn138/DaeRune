// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "UObject/NoExportTypes.h"
#include "DRWidgetController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAbilityInfoSignature, const FDRAbilityInfo&, Info); // 능력 정보 브로드캐스트 델리게이트임

class UAttributeSet;
class UAbilitySystemComponent;
class ADRPlayerController;
class ADRPlayerState; 
class UDRAbilitySystemComponent;
class UDRAttributeSet;
class UAbilityInfo;

USTRUCT(BlueprintType)
struct FWidgetControllerParams // 위젯 컨트롤러 초기화용 파라미터 구조체임
{
	GENERATED_BODY()

	FWidgetControllerParams() {} // 기본 생성자임
	FWidgetControllerParams(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
	: PlayerController(PC), PlayerState(PS), AbilitySystemComponent(ASC), AttributeSet(AS) {} // 멤버 초기화 생성자임

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<APlayerController> PlayerController = nullptr; // 플레이어 컨트롤러 참조임

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<APlayerState> PlayerState = nullptr; // 플레이어 스테이트 참조임

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr; // 어빌리티 시스템 컴포넌트 참조임

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAttributeSet> AttributeSet = nullptr; // 어트리뷰트 세트 참조임
};

/**
 * UDRWidgetController
 *
 * 위젯 UI와 GAS 데이터를 연결하는 컨트롤러 클래스임
 */
UCLASS()
class DAERUNE_API UDRWidgetController : public UObject // UObject 기반 UI 컨트롤러임
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void SetWidgetControllerParams(const FWidgetControllerParams& WCParams); // 파라미터 설정 함수임

	UFUNCTION(BlueprintCallable)
	virtual void BroadcastInitialValues(); // 초기값 브로드캐스트 함수임
	virtual void BindCallbacksToDependencies(); // 종속성 콜백 바인딩 함수임

	UPROPERTY(BlueprintAssignable, Category = "GAS|Messages")
	FAbilityInfoSignature AbilityInfoDelegate; // 능력 정보 전송 델리게이트임

	void BroadcastAbilityInfo(); // 현재 능력 정보 브로드캐스트 함수임
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget Data")
	TObjectPtr<UAbilityInfo> AbilityInfo; // 능력 정보 데이터 참조임

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<APlayerController> PlayerController; // 플레이어 컨트롤러 멤버임

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<APlayerState> PlayerState; // 플레이어 스테이트 멤버임

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent; // ASC 멤버임

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<UAttributeSet> AttributeSet; // 어트리뷰트 세트 멤버임

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<ADRPlayerController> DRPlayerController; // DR 전용 플레이어 컨트롤러 캐시 임

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<ADRPlayerState> DRPlayerState; // DR 전용 플레이어 스테이트 캐시 임

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<UDRAbilitySystemComponent> DRAbilitySystemComponent; // DR ASC 캐시 임

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<UDRAttributeSet> DRAttributeSet; // DR 어트리뷰트 세트 캐시 임

	ADRPlayerController* GetDRPC(); // DR 플레이어 컨트롤러 반환 헬퍼임
	ADRPlayerState* GetDRPS(); // DR 플레이어 스테이트 반환 헬퍼임
	UDRAbilitySystemComponent* GetDRASC(); // DR ASC 반환 헬퍼임
	UDRAttributeSet* GetDRAS(); // DR 어트리뷰트 세트 반환 헬퍼임
};
