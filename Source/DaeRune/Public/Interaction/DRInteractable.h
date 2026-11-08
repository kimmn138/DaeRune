// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Interface.h"
#include "DRInteractable.generated.h"

class ADRCharacter;

/**
 * 화면 상호작용 안내 한 줄 — 조준선 아래 "[F] 부품 줍기", 부품을 들고 있으면 그 아래 부품 아이콘.
 * UDRInteractionComponent 가 채워 UDRInteractionPromptWidget 에 넘긴다.
 */
USTRUCT(BlueprintType)
struct FDRInteractionPrompt
{
	GENERATED_BODY()

	// 안내 문구. 비어 있으면 안내 줄을 띄우지 않는다
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText Text;

	// 누를 키. 비어 있으면 키 없이 문구만 띄운다 (지금은 쓸 수 없다는 안내)
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FKey Key;

	// 들고 있는 부품 아이콘을 안내 아래에 띄운다
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bShowCarriedPart = false;

	bool HasText() const { return !Text.IsEmpty(); }
	bool IsEmpty() const { return !HasText() && !bShowCarriedPart; }

	// 같은 안내인지 — 감지 주기마다 다시 만들므로(금고 숫자는 Format) 글자로도 비교한다
	bool IsSameAs(const FDRInteractionPrompt& Other) const
	{
		return Key == Other.Key && bShowCarriedPart == Other.bShowCarriedPart
			&& (Text.IdenticalTo(Other.Text) || Text.ToString().Equals(Other.Text.ToString(), ESearchCase::CaseSensitive));
	}
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDRInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * F 로 상호작용하는 대상 — 시점 광선으로 고르는 것(부품 · 사이트 · 청소기 · 스테이지2 프롭 · 열차 칸)과
 * 범위에 들어가면 쓰는 장치(업그레이드 · 옷장 · 스테이지 선택 포털) 공용.
 * 감지 컴포넌트(UDRInteractionComponent)가 지금 F 가 할 일을 고르고, 그 대상의 문구를 화면 안내에 띄운다.
 */
class DAERUNE_API IDRInteractable
{
	GENERATED_BODY()

public:
	/**
	 * 이 대상 앞에서 화면에 띄울 안내 문구 (로컬 코스메틱 — 판정 근거가 전부 복제 프로퍼티라 RPC 불필요).
	 * 키는 감지 컴포넌트가 붙인다(상호작용 키). bOutUsable 을 false 로 두면 키 없이 문구만 띄운다
	 * (예: 참가자에게 "방장만 스테이지를 선택할 수 있습니다"). 부르는 쪽이 true 로 넣어 준다.
	 */
	virtual FText GetInteractionPromptText(const ADRCharacter* Interactor, bool& bOutUsable) const = 0;
};
