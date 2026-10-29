// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRCheatManager.generated.h"

class ADRPlayerController;
class UDRGameInstance;

/**
 * DaeRune 콘솔 치트 (`~` 콘솔의 DR* 명령).
 *
 * 개발 빌드 전용 — 쉬핑에는 치트 매니저 자체가 만들어지지 않는다(UE_WITH_CHEAT_MANAGER).
 * 엔진은 단독 플레이 · 에디터에서만 치트 매니저를 만드는데, 멀티플레이 개발 빌드에서도 바로 쓰도록
 * ADRPlayerController::ReceivedPlayer 가 AddCheats(true) 로 만든다.
 *
 * 로컬 세이브를 바꾸는 치트는 그 기계에서만 돈다. 서버 판정이 필요한 것(사망 · 부활 · 페이즈 넘기기)은
 * PlayerController 의 Server RPC 를 부른다 — 치트 매니저는 복제되지 않아 RPC 를 가질 수 없다.
 */
UCLASS()
class DAERUNE_API UDRCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	// ========== 업그레이드 ==========

	/**
	 * 업그레이드 시스템 해금. (Plan2.md 5.1 — 정식 경로는 스테이지1 최초 클리어)
	 * 세이브에 즉시 기록되므로 재시작해도 유지된다 — 되돌리려면 DRLockUpgrade 를 쓴다.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRUnlockUpgrade();

	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRAddCurrency(int32 Amount = 5000);

	// 해금 전 상태(잠김 프롬프트/차단 문구)를 다시 확인하고 싶을 때
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRLockUpgrade();

	/**
	 * 슬롯을 Count 개 해금한다(재화는 자동으로 채워 준다).
	 * ★칩은 슬롯을 1칸 이상 해금해야 열린다★ — IsChipUnlocked() 가 UnlockedSlots >= RequiredSlotTier 로 판정한다.
	 * 시스템 해금(DRUnlockUpgrade)만으로는 칩이 잠긴 채로 남는다.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRUnlockSlot(int32 Count = 1);

	// 현재 진행도를 로그로 덤프 (해금 수 / 재화 / 칩 상태)
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRDumpUpgrade();

	/**
	 * 기본 장착 칩(DefaultEquippedChips)을 다시 꽂고 서버에 재보고한다. (Plan3(공용칩) §6.6)
	 * ★HUD 표시 칩을 뺀 상태에서 빠져나오기 위한 안전망이다★ — 로비까지 못 가는 상황(스테이지 도중 등)에서는
	 * 이 명령이 유일한 복구 수단이다. 빈 칸이 없으면 꽂히지 않는다.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Upgrade")
	void DRResetDefaultChips();

	// ========== 코스메틱 ==========

	// 코스메틱 카탈로그의 모든 해금 조건을 달성 처리한다 (옷장 UI 의 잠김/해금 상태 확인용)
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Cosmetic")
	void DRUnlockSkins();

	// 옷장 화면을 강제로 연다 (로비에서 옷장 액터 없이 UI 를 확인할 때)
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Cosmetic")
	void DROpenWardrobe();

	// 현재 로봇의 장착 코스메틱을 로그로 덤프
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Cosmetic")
	void DRDumpCosmetic();

	// ========== 스테이지 선택 ==========

	// 스테이지 선택 화면을 강제로 연다 (포털 없이 UI 확인)
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|StageSelect")
	void DROpenStageSelect();

	/**
	 * 카드 상태를 덮어쓴다 — 시안의 "미리보기 상태" 패널. ★세이브에 쓰지 않는다★
	 *   DRStageState Stage2 locked | normal | cleared | reset
	 *   DRStageState all reset   (전부 해제)
	 * 화면이 열려 있으면 바로 다시 그려진다.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|StageSelect")
	void DRStageState(const FString& StageId, const FString& State);

	// ========== 사망 · 부활 · 페이즈 (서버 RPC 경유) ==========

	/**
	 * 내 캐릭터를 바로 죽이고 / 되살린다 — 사망 중 옷(부착물) 숨김 확인용 (Plan(옷장) T-3).
	 * 정식 사망은 오염(Corrupt) 상태에서만 일어나서 확인하기 번거롭다.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Cosmetic")
	void DRDie();

	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Cosmetic")
	void DRRevive();

	// 다음 페이즈(방어 페이즈면 다음 웨이브)로 넘긴다 — BP 의 0 키와 같다
	UFUNCTION(Exec, BlueprintCallable, Category = "Cheat|Phase")
	void DRSkipPhase();

private:
	ADRPlayerController* GetDRPlayerController() const;

	// 로컬 세이브를 가진 게임 인스턴스 (없으면 경고 로그 후 null)
	UDRGameInstance* GetDRGameInstance() const;

	// 치트가 조작할 대상 로봇 (업그레이드 화면의 GetViewedClass 와 같은 기준)
	EPlayerCharacterClass GetViewedUpgradeClass() const;
};
