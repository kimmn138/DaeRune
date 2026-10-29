// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRWardrobeEditorTools.generated.h"

class USkeletalMesh;

/**
 * 옷장(Plan(옷장)) 에디터 자동화 도우미 — ★에디터 전용★.
 * 게임 빌드(에디터가 아닌 타깃)에서는 본문이 비어 있어 아무 일도 하지 않는다.
 *
 * Python(에디터 스크립팅)에서 부른다:
 *   ok, log = unreal.DRWardrobeEditorTools.build_wardrobe_tab_widget()
 *
 * 하는 일:
 *   - WBP 위젯 트리를 실측 사양(Plan(옷장) D-24)대로 맞춘다 (배치 · 좌표 · 브러시 · 폰트 · 위젯 이름 — D-27).
 *     Python 은 위젯 블루프린트의 트리를 만질 수 없어서 C++ 로 둔다.
 *   - BP 그래프로 만들다 만 배선(탭 SetSelected · 칸 Setup 등)은 C++ 로 옮겼으므로 지운다 (Plan(옷장) T-13).
 *   - Python 이 막는 EditDefaultsOnly 속성을 텍스트로 넣는다 (ImportPropertyText).
 * 저장은 하지 않는다 — 호출한 쪽(Python)이 EditorAssetLibrary 로 저장한다.
 * 몇 번을 다시 돌려도 같은 결과가 나오게(멱등) 짰다.
 */
UCLASS()
class DAERUNEEDITOR_API UDRWardrobeEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 오브젝트의 프로퍼티 하나를 UE 텍스트 형식(T3D — 복사/붙여넣기와 같은 형식)으로 넣는다.
	 * 예) ImportPropertyText(DA_ProgressionConfig, "CosmeticCatalog", "/Game/Blueprints/Progression/DA_CosmeticCatalog.DA_CosmeticCatalog")
	 * PostEditChangeProperty 를 불러 에디터에서 손으로 고친 것과 똑같이 검증 · 갱신된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool ImportPropertyText(UObject* Target, FName PropertyName, const FString& Text, FString& OutError);

	/**
	 * ImportPropertyText 의 짝 — 프로퍼티 하나를 같은 T3D 텍스트로 꺼낸다. 없거나 실패하면 빈 문자열.
	 * FText 는 NSLOCTEXT(네임스페이스, 키, 원문) 로 나오므로, 원문만 고쳐 다시 넣으면 번역 키가 유지된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static FString ExportPropertyText(UObject* Target, FName PropertyName);

	// WBP_WardrobeTab — 필 텍스처 · 투명 버튼 · 라벨 폰트/색, BP 잔재(SetSelected · CosmeticCategory · Label) 제거
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool BuildWardrobeTabWidget(FString& OutLog);

	// WBP_WardrobeSlot — "투명 버튼 + 상태 이미지" 구조로 트리를 다시 만들고 상태 텍스처 3장을 Class Defaults 에 넣는다
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool BuildWardrobeSlotWidget(FString& OutLog);

	// WBP_CosmeticPreview — 루트 Img_Preview 하나 + PreviewMaterial 지정 (에셋은 미리 만들어 둬야 한다)
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool BuildCosmeticPreviewWidget(FString& OutLog);

	// WBP_Wardrobe — 실측 좌표 · 블렌드 머티리얼 · 제목 폰트 · 탭 4개 · 프리뷰 · Clear All · SlotWidgetClass
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool BuildWardrobeScreenWidget(FString& OutLog);

	/**
	 * 고정 크기(예: 1920x1080)의 새 창으로 PIE 를 띄운다 — 시안(1080p)과 같은 비율로 스크린샷을 찍어 맞대 보려고 둔다.
	 * 레벨 뷰포트 PIE 는 에디터 배치에 따라 비율이 제각각이라(예: 2.7:1) 시안과 비교할 수 없다.
	 * NumPlayers 가 1 이면 넷 모드는 에디터 설정을 따른다. 2 이상이면 ★리슨 서버(첫 창) + 클라이언트★ 로,
	 * 한 프로세스 안에 띄운다 — Python(unreal.EditorLevelLibrary.get_pie_worlds)이 모든 월드를 본다 (Plan(옷장) M3 복제 확인용).
	 * ★에디터 Play 설정은 바꾸지 않는다★ (복사본만 고친다). 창 크기는 물리 픽셀 그대로다 (Windows 배율 영향 없음).
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool StartPlayInNewWindow(int32 Width = 1920, int32 Height = 1080, int32 NumPlayers = 1);

	/**
	 * 다음 엔진 틱에 Target 의 함수를 "이름 인자 …" 형식으로 부른다 (콘솔 명령과 같은 형식, Exec 아니어도 된다).
	 * 예) call_on_next_tick(client_pc, 'RequestChangeClass 1') / call_on_next_tick(client_pc, 'CloseWardrobeScreen')
	 * ★왜 필요한가★ — Python 의 UFunction 호출은 전부 FEditorScriptExecutionGuard 안에서 돌아서, 그 안에서 나가는
	 * Server RPC 가 네트워크로 안 가고 그 자리(클라이언트)에서 실행돼 버린다 (Plan(옷장) T-37). 틱에서 부르면 정상 경로를 탄다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool CallOnNextTick(UObject* Target, const FString& FunctionAndArgs);

	/**
	 * 스켈레탈 메시에 ★메시 전용 소켓★ 을 만들거나(이름이 같으면) 고친다 — 공용 옷의 부위 소켓(Cosmetic_Head 등)용 (Plan(옷장) D-32).
	 * 소켓 이름 · 뼈 이름은 Python 에서 못 바꾼다(VisibleAnywhere). 저장은 부른 쪽이 한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static bool SetMeshSocket(USkeletalMesh* Mesh, FName SocketName, FName BoneName, FVector Location, FRotator Rotation, FVector Scale);
};
