// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRCosmeticTypes.generated.h"

class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

/**
 * 코스메틱 부위 분류. 옷장 화면의 탭 4개와 1:1 대응한다. (Plan.md 15.11 참조)
 *
 * 한 로봇은 카테고리마다 하나씩, 최대 4개를 ★동시에★ 장착한다.
 * 세이브의 TMap 키가 아니라 배열 인덱스로 쓰이므로 값 자체가 직렬화된다 —
 * ★중간 삽입 금지 / Count 직전에만 추가★ (EPlayerCharacterClass 와 같은 규약).
 */
UENUM(BlueprintType)
enum class EDRCosmeticCategory : uint8
{
	Head	UMETA(DisplayName = "HEAD"),
	Face	UMETA(DisplayName = "FACE"),
	Body	UMETA(DisplayName = "BODY"),
	Tail	UMETA(DisplayName = "TAIL"),

	Count	UMETA(Hidden)	// 카테고리 개수 계산용 - 항상 마지막에 유지
};

/** 카테고리 개수 (배열 길이). enum 을 늘리면 자동으로 따라간다. */
FORCEINLINE int32 DRGetCosmeticCategoryCount()
{
	return static_cast<int32>(EDRCosmeticCategory::Count);
}

/** 유효한 카테고리인가 (Count 와 범위 밖을 함께 거른다) */
FORCEINLINE bool DRIsValidCosmeticCategory(EDRCosmeticCategory Category)
{
	const int32 Index = static_cast<int32>(Category);
	return Index >= 0 && Index < DRGetCosmeticCategoryCount();
}

/**
 * 메시의 ★일부 머티리얼 슬롯만★ 덮어쓰는 명세.
 *
 * 인덱스 기반 배열(TArray<UMaterialInterface*>)로 하지 않는 이유:
 * 부위별 스킨은 대체로 슬롯 하나만 건드리는데, 인덱스 배열은 그 앞을 전부 빈 칸으로
 * 채워야 해서 데이터 입력이 번거롭고 슬롯 번호가 눈에 보이지 않는다.
 */
USTRUCT(BlueprintType)
struct FDRSkinMaterialOverride
{
	GENERATED_BODY()

	// 덮어쓸 머티리얼 슬롯 번호 (메시의 Material Slot 인덱스)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic", meta = (ClampMin = "0"))
	int32 MaterialSlot = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
	TSoftObjectPtr<UMaterialInterface> Material;
};

/**
 * 메시 1개를 캐릭터에 붙이는 명세. 3인칭/1인칭 각각 따로 지정한다. (Plan.md 15.11 참조)
 *
 * 부착물 컴포넌트는 ★복제하지 않는다★ — 각 머신이 복제된 EquippedSkinIds 를 보고
 * 로컬에서 만든다. 컴포넌트를 복제하면 스폰 순서/소유권 문제만 생기고 얻는 게 없다.
 */
USTRUCT(BlueprintType)
struct FDRSkinAttachSpec
{
	GENERATED_BODY()

	// 둘 다 지정하면 Skeletal 이 우선한다. 둘 다 비어 있으면 "이쪽에는 붙이지 않는다".
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	// 붙을 소켓/본 이름. None 이면 부모 컴포넌트 원점에 붙는다.
	// ★bUseLeaderPose 가 true 면 무시된다★ (스켈레톤을 공유하므로 소켓 개념이 없다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
	FName SocketName;

	// 소켓 기준 추가 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
	FTransform RelativeTransform;

	/**
	 * 부모 메시의 포즈를 그대로 따라갈지 (SetLeaderPoseComponent).
	 *   몸통 의상처럼 ★같은 스켈레톤을 공유하는 메시★ → true (전용 ABP 불필요, 성능도 유리)
	 *   모자처럼 소켓에 매달리는 소품                  → false
	 * Skeletal 메시일 때만 의미가 있다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
	bool bUseLeaderPose = false;

	// 부착물 ★자체★ 의 머티리얼 오버라이드 (같은 메시의 색상 변형용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attach")
	TArray<FDRSkinMaterialOverride> MaterialOverrides;

	// 붙일 메시가 하나라도 지정돼 있는가
	bool HasMesh() const { return !SkeletalMesh.IsNull() || !StaticMesh.IsNull(); }
};

/**
 * 스킨(코스메틱 아이템) 1종의 정의. UDRCosmeticCatalog 가 배열로 보유한다.
 * (Plan.md 4.4 / 15.11 참조)
 *
 * 부착물과 본체 리컬러를 ★둘 다 선택★ 으로 담는다.
 * → "부착물만" / "리컬러만" / "둘 다" 스킨이 전부 같은 구조로 표현된다.
 */
USTRUCT(BlueprintType)
struct FDRSkinDefinition
{
	GENERATED_BODY()

	// 세이브에 저장되는 키. ★변경 금지★ (바꾸면 유저의 장착 상태가 사라진다).
	// 명명 규약: "Skin.<Class>.<Category>.<Name>" 예) Skin.Gardener.Head.Cap
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
	FName SkinId;

	// 어느 로봇의 아이템인가
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
	EPlayerCharacterClass OwnerClass = EPlayerCharacterClass::Gardener;

	// 어느 탭에 표시되는가
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
	EDRCosmeticCategory Category = EDRCosmeticCategory::Head;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin")
	FText Description;

	// ========== 해금 조건 ==========

	// 필요한 업적/보상 Id. ★비어 있으면 기본 제공★ (항상 해금).
	// SaveGame 의 EarnedAchievements ∪ ClaimedRewards 와 대조한다. (Plan.md 4.5)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Unlock")
	TArray<FName> RequiredAchievements;

	// true = 전부 달성해야 해금(AND) / false = 하나만 달성해도 해금(OR)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Unlock")
	bool bRequireAll = true;

	// 잠금 툴팁 문구. 비면 UI 가 기본 문구로 폴백한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Unlock")
	FText HowToUnlock;

	// ========== 외형: 부착물 (주 경로) ==========

	// 타인이 보는 3인칭 메시에 붙는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Attach")
	FDRSkinAttachSpec ThirdPerson;

	// 본인이 보는 1인칭 메시에 붙는다.
	// ★비워두는 것이 기본★ — 1인칭에서는 자기 머리 위의 모자도 꼬리도 보이지 않는다.
	// 청소기는 FP 스켈레톤이 TP 와 완전히 별개라 소켓 이름이 다르거나 없을 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Attach")
	FDRSkinAttachSpec FirstPerson;

	// ========== 외형: 본체 머티리얼 오버라이드 (보조 경로, 선택) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Material")
	TArray<FDRSkinMaterialOverride> BodyMaterialsTP;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Material")
	TArray<FDRSkinMaterialOverride> BodyMaterialsFP;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|Material")
	TArray<FDRSkinMaterialOverride> WeaponMaterials;

	// ========== UI ==========

	// 옷장 칸에 표시할 썸네일. 미지정이면 칸이 비어 보인다(동작에는 문제 없음).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|UI")
	TSoftObjectPtr<UTexture2D> PreviewIcon;

	// 목록 정렬 순서 (작을수록 앞). 같으면 SkinId 순.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skin|UI")
	int32 SortOrder = 0;

	// 외형을 하나라도 바꾸는가 (전부 비어 있으면 "기본 외형"과 다를 게 없다 — 에디터 검증용)
	bool HasAnyVisual() const
	{
		return ThirdPerson.HasMesh() || FirstPerson.HasMesh()
			|| BodyMaterialsTP.Num() > 0 || BodyMaterialsFP.Num() > 0 || WeaponMaterials.Num() > 0;
	}
};

/**
 * 한 로봇(클래스)의 카테고리별 장착 상태. (세이브 저장 단위)
 *
 * FDRClassUpgradeState::SlotChips 와 ★같은 관용구★ 를 쓴다 —
 * 고정 길이 배열 + 빈 칸은 NAME_None. 자료구조만으로 "카테고리당 최대 1개"가 보장된다.
 * (Plan.md 15.11 참조)
 */
USTRUCT(BlueprintType)
struct FDRClassCosmeticState
{
	GENERATED_BODY()

	// 길이 = EDRCosmeticCategory::Count. 인덱스 = 카테고리. 빈 칸은 NAME_None.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetic")
	TArray<FName> EquippedByCategory;

	// 배열 길이를 카테고리 수에 맞춘다. 늘어난 칸은 NAME_None 으로 채운다.
	// enum 이 늘어나도 세이브가 자동으로 따라오게 하는 지점이다.
	void EnsureSize()
	{
		const int32 Required = DRGetCosmeticCategoryCount();
		if (EquippedByCategory.Num() != Required)
		{
			EquippedByCategory.SetNum(Required);
		}
	}

	FName Get(EDRCosmeticCategory Category) const
	{
		const int32 Index = static_cast<int32>(Category);
		return EquippedByCategory.IsValidIndex(Index) ? EquippedByCategory[Index] : NAME_None;
	}

	// 하나라도 장착 중인가
	bool HasAnyEquipped() const
	{
		for (const FName& Id : EquippedByCategory)
		{
			if (!Id.IsNone()) return true;
		}
		return false;
	}
};

/**
 * 옷장 화면의 칸 1개를 그리는 데 필요한 전부. (FDRChipViewModel 의 코스메틱 버전)
 * UI 가 카탈로그와 세이브를 직접 뒤지지 않게 하기 위한 뷰모델이다. (Plan.md 15.7 참조)
 */
USTRUCT(BlueprintType)
struct FDRSkinViewModel
{
	GENERATED_BODY()

	// NAME_None 이면 "기본"(장착 해제) 칸이다
	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	FName SkinId;

	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	EDRCosmeticCategory Category = EDRCosmeticCategory::Head;

	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	FText Description;

	// 동기 로드된 썸네일. ★미지정이면 nullptr★ — 위젯이 이미지를 숨기는 신호로 쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	TObjectPtr<UTexture2D> PreviewIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	bool bUnlocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	bool bEquipped = false;

	// bUnlocked == false 일 때 표시할 조건 문구 (HowToUnlock 또는 기본 문구)
	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	FText UnlockHint;

	// "기본"(장착 해제) 칸인가 — 위젯이 다르게 그리고 싶을 때 쓴다
	UPROPERTY(BlueprintReadOnly, Category = "Skin")
	bool bIsNoneSlot = false;
};
