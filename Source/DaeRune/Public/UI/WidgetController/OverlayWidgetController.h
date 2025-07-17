// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "OverlayWidgetController.generated.h"

struct FDRAbilityInfo; // 능력 정보 구조체 전방 선언임

USTRUCT(BlueprintType)
struct FUIWidgetRow : public FTableRowBase // UI 메시지 데이터 테이블 행 구조체임
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag MessageTag = FGameplayTag(); // 메시지 태그 식별자임

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Message = FText(); // 표시할 메시지 텍스트임

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UDRUserWidget> MessageWidget; // 사용 위젯 클래스 유형임

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UTexture2D* Image = nullptr; // 메시지 아이콘 텍스처임
};

class UDRUserWidget;
class UAbilityInfo;
class UDRAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeChangedSignature, float, NewValue); // 속성 변경 브로드캐스트 델리게이트임
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMessageWidgetRowSignature, FUIWidgetRow, Row); // 메시지 위젯 행 델리게이트임

/**
 * UOverlayWidgetController
 *
 * 게임 UI 오버레이와 GAS 데이터 연결 컨트롤러 클래스임
 */
UCLASS(BlueprintType, Blueprintable)
class DAERUNE_API UOverlayWidgetController : public UDRWidgetController
{
	GENERATED_BODY()
	
public:
	// 초기값 브로드캐스트 재정의 함수임	
	virtual void BroadcastInitialValues() override;
	// 종속성 콜백 바인딩 재정의 함수임
	virtual void BindCallbacksToDependencies() override;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnHealthChanged; // 체력 변경 델리게이트임

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnMaxHealthChanged; // 최대 체력 변경 델리게이트임

	UPROPERTY(BlueprintAssignable, Category = "GAS|Messages")
	FMessageWidgetRowSignature MessageWidgetRowDelegate; // 메시지 행 델리게이트임

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget Data")
	TObjectPtr<UDataTable> MessageWidgetDataTable; // 메시지 위젯 데이터 테이블 참조임

	template<typename T>
	T* GetDataTableRowByTag(UDataTable* DataTable, const FGameplayTag& Tag);
};

/**
* 태그 기반으로 데이터 테이블 행 검색 제네릭 함수임
*/
template <typename T>
T* UOverlayWidgetController::GetDataTableRowByTag(UDataTable* DataTable, const FGameplayTag& Tag)
{
	return DataTable->FindRow<T>(Tag.GetTagName(), TEXT("")); // 태그 이름으로 행 조회임
}
