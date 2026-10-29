// Copyright DaeRune

#include "Tuning/DRTuningSubsystem.h"
#include "Tuning/DRTuningProperty.h"
#include "Tuning/DRTuningSheet.h"
#include "Tuning/DRTuningCatalog.h"
#include "DaeRune/DRLogChannels.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Game/DRGameInstance.h"
#include "Game/DRProgressionConfig.h"
#include "Game/DRChipCatalog.h"
#include "Abilities/GameplayAbility.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/ARFilter.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProperties.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/UObjectHash.h"
#if WITH_EDITOR
#include "UObject/ICookInfo.h"
#endif

namespace
{
	TAutoConsoleVariable<int32> CVarTuningApplyInPIE(
		TEXT("dr.Tuning.ApplyInPIE"),
		0,
		TEXT("1 이면 에디터 PIE 에서도 Tuning/튜닝.csv 를 적용한다 (PIE 가 끝나면 되돌린다). 바꾼 뒤 DRTuningReload 또는 PIE 재시작"),
		ECVF_Default);

	// 화면 메시지 자리 — 다시 읽을 때마다 같은 줄을 덮어쓴다. 엔진 기본 크기는 너무 작아 키운다
	constexpr uint64 ScreenMessageKey = 0x54554E494E47ull;
	const FVector2D ScreenTextScale(1.4, 1.4);
	constexpr int32 HostChunkSize = 40;

	/**
	 * 덮어쓴 값 1건의 기록. ★프로세스 전체에서 하나★ 다 — PIE 를 한 프로세스로 돌리면 게임 인스턴스는 여럿이어도
	 * CDO 는 하나라서, 인스턴스마다 따로 기록하면 "원래 값" 이 이미 덮어쓴 값으로 잘못 남는다.
	 * CDO · 에셋만 기록한다. 실행 중 인스턴스는 그 클래스 CDO 기록을 보고 같이 맞춘다 (SyncInstances).
	 */
	struct FTouch
	{
		TWeakObjectPtr<UObject> Object;
		FString Path;
		DRTuning::FTypedValue Original;
		DRTuning::FTypedValue Current;
		FString OriginalText;
	};

	TMap<FString, FTouch> GTouches;		// "오브젝트 경로|속성 경로"
	TArray<FString> GTouchOrder;		// 되돌릴 때는 역순

	// 발동 중인 어빌리티 인스턴스는 끝난 뒤에 고친다 (발동 중에 칸 수 같은 값이 바뀌면 배열 범위를 넘을 수 있다)
	struct FPendingWrite
	{
		TWeakObjectPtr<UObject> Object;
		FString Path;
		DRTuning::FTypedValue From;
		DRTuning::FTypedValue To;
	};
	TArray<FPendingWrite> GPendingWrites;

	// 이 프로세스에서 값을 맡은 서브시스템 (PIE 한 프로세스 멀티에서는 처음 적용한 인스턴스)
	TWeakObjectPtr<UDRTuningSubsystem> GOwner;

	FString TouchKey(const UObject* Object, const FString& Path)
	{
		return Object->GetPathName() + TEXT("|") + Path;
	}

	FString CatalogKey(const FString& Path, const FString& Property)
	{
		return (Path + TEXT("|") + Property).ToLower();
	}

	void RecordTouch(UObject* Object, const FString& Path, const DRTuning::FTypedValue& Original, const DRTuning::FTypedValue& Current, const FString& OriginalText)
	{
		const FString Key = TouchKey(Object, Path);
		if (FTouch* Existing = GTouches.Find(Key))
		{
			Existing->Current = Current;	// 같은 항목이 두 번 — 원래 값은 처음 것 그대로
			return;
		}
		FTouch Touch;
		Touch.Object = Object;
		Touch.Path = Path;
		Touch.Original = Original;
		Touch.Current = Current;
		Touch.OriginalText = OriginalText;
		GTouches.Add(Key, MoveTemp(Touch));
		GTouchOrder.Add(Key);
	}

	void QueuePendingWrite(UObject* Object, const FString& Path, const DRTuning::FTypedValue& From, const DRTuning::FTypedValue& To)
	{
		for (int32 Index = 0; Index < GPendingWrites.Num(); ++Index)
		{
			FPendingWrite& Pending = GPendingWrites[Index];
			if (Pending.Object.Get() == Object && Pending.Path == Path)
			{
				// 되돌림(Y→X) 뒤 새 적용(X→Z)이 겹치면 Y→Z 하나로
				Pending.To = To;
				if (Pending.From == Pending.To)
				{
					GPendingWrites.RemoveAtSwap(Index);
				}
				return;
			}
		}
		GPendingWrites.Add({ Object, Path, From, To });
	}

	/** Class 의 실행 중 어빌리티 인스턴스 중 값이 From 인 것을 To 로. 액터 · 투사체는 새로 생기는 것부터라 건드리지 않는다 */
	int32 SyncInstances(UClass* Class, const FString& Path, const DRTuning::FTypedValue& From, const DRTuning::FTypedValue& To)
	{
		if (!Class || !Class->IsChildOf(UGameplayAbility::StaticClass()) || From == To) return 0;

		int32 Count = 0;
		ForEachObjectOfClass(Class, [&](UObject* Object)
		{
			if (Object->GetClass() != Class || Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return;

			DRTuning::FResolvedValue Value;
			FString Error;
			if (!DRTuning::ResolvePath(Object, Path, Value, Error)) return;
			// 실행 중에 따로 바뀐 값은 건드리지 않는다
			if (DRTuning::ReadValue(Value) != From) return;

			if (CastChecked<UGameplayAbility>(Object)->IsActive())
			{
				QueuePendingWrite(Object, Path, From, To);
			}
			else
			{
				DRTuning::WriteValue(Value, To);
			}
			++Count;
		}, /*bIncludeDerivedClasses*/ false);
		return Count;
	}

	void ProcessPendingWrites()
	{
		for (int32 Index = GPendingWrites.Num() - 1; Index >= 0; --Index)
		{
			const FPendingWrite& Pending = GPendingWrites[Index];
			UObject* Object = Pending.Object.Get();
			if (!Object)
			{
				GPendingWrites.RemoveAtSwap(Index);
				continue;
			}
			if (const UGameplayAbility* Ability = Cast<UGameplayAbility>(Object); Ability && Ability->IsActive())
			{
				continue;
			}

			DRTuning::FResolvedValue Value;
			FString Error;
			if (DRTuning::ResolvePath(Object, Pending.Path, Value, Error) && DRTuning::ReadValue(Value) == Pending.From)
			{
				DRTuning::WriteValue(Value, Pending.To);
			}
			GPendingWrites.RemoveAtSwap(Index);
		}
	}

	int32 RevertAllTouches()
	{
		int32 Reverted = 0;
		for (int32 Index = GTouchOrder.Num() - 1; Index >= 0; --Index)
		{
			const FTouch* Touch = GTouches.Find(GTouchOrder[Index]);
			UObject* Object = Touch ? Touch->Object.Get() : nullptr;
			if (!Object) continue;

			DRTuning::FResolvedValue Value;
			FString Error;
			if (!DRTuning::ResolvePath(Object, Touch->Path, Value, Error)) continue;

			DRTuning::WriteValue(Value, Touch->Original);
			if (Object->HasAnyFlags(RF_ClassDefaultObject))
			{
				SyncInstances(Object->GetClass(), Touch->Path, Touch->Current, Touch->Original);
			}
			++Reverted;
		}
		GTouches.Reset();
		GTouchOrder.Reset();
		return Reverted;
	}

	DRTuningCatalog::FOriginalLookup MakeOriginalLookup()
	{
		return [](const UObject* Root, const FString& Path, FString& OutText) -> bool
		{
			if (!Root) return false;
			if (const FTouch* Touch = GTouches.Find(TouchKey(Root, Path)))
			{
				OutText = Touch->OriginalText;
				return true;
			}
			return false;
		};
	}

	FDRTuningCatalogSettings MakeCatalogSettings(const UDRTuningSubsystem& Config)
	{
		FDRTuningCatalogSettings Settings;
		Settings.BlueprintBases = Config.ListedBlueprintBases;
		Settings.AssetClasses = Config.ListedAssetClasses;
		Settings.Assets = Config.ListedAssets;
		Settings.Maps = Config.ListedMaps;
		Settings.ExcludedPathContains = Config.ExcludedPathContains;
		Settings.GroupRules = Config.GroupRules;
		return Settings;
	}

	// /Game/…/GA_SeedCannon.GA_SeedCannon_C → GA_SeedCannon · 배치 액터는 Stage1:BP_Door_C_1
	FString ShortTargetName(const FString& Target)
	{
		if (!Target.StartsWith(TEXT("/"))) return Target;

		FName MapPackage, ActorName;
		if (DRTuning::ParseLevelActorPath(Target, MapPackage, ActorName))
		{
			return FPackageName::GetShortName(MapPackage) + TEXT(":") + ActorName.ToString();
		}

		FString Name = FPackageName::ObjectPathToObjectName(Target);
		if (Name.IsEmpty())
		{
			Name = FPackageName::GetShortName(Target);
		}
		Name.RemoveFromEnd(TEXT("_C"));
		return Name;
	}

	bool FindAssetPathByName(const FString& Name, FString& OutPath, FString& OutError)
	{
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

		FARFilter Filter;
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Found;
		AssetRegistry.EnumerateAssets(Filter, [&Found, &Name](const FAssetData& Asset)
		{
			if (Asset.AssetName.ToString().Equals(Name, ESearchCase::IgnoreCase))
			{
				Found.Add(Asset);
			}
			return true;
		});

		if (Found.Num() == 0)
		{
			OutError = FString::Printf(TEXT("'%s' 이라는 에셋이 없습니다 — [경로] 칸에 /Game/… 경로를 적으세요"), *Name);
			return false;
		}
		if (Found.Num() > 1)
		{
			OutError = FString::Printf(TEXT("'%s' 이름의 에셋이 %d 개라 고를 수 없습니다 — [경로] 칸에 전체 경로를 적으세요"), *Name, Found.Num());
			return false;
		}
		OutPath = Found[0].GetObjectPathString();
		return true;
	}

	// 쿠킹되지 않은 에셋(아무 데서도 안 쓰는 BP)의 줄은 이 빌드에서 바꿀 수 없으니 뺀다
	void RemoveRowsNotInBuild(TArray<FDRTuningRow>& Rows)
	{
		TMap<FString, bool> PackageExists;
		for (const FDRTuningRow& Row : Rows)
		{
			const FString Package = FPackageName::ObjectPathToPackageName(Row.Path);
			if (!Package.IsEmpty() && !PackageExists.Contains(Package))
			{
				PackageExists.Add(Package, FPackageName::DoesPackageExist(Package));
			}
		}

		// 하나도 없다고 나오면 조회 자체가 안 되는 환경이다 — 거르지 않는다
		bool bAnyExists = false;
		for (const TPair<FString, bool>& Pair : PackageExists)
		{
			bAnyExists |= Pair.Value;
		}
		if (!bAnyExists) return;

		Rows.RemoveAll([&PackageExists](const FDRTuningRow& Row)
		{
			const bool* bExists = PackageExists.Find(FPackageName::ObjectPathToPackageName(Row.Path));
			return bExists && !*bExists;
		});
	}

	UDRTuningSubsystem* FindSubsystemForConsole(UWorld* World)
	{
		if (World && World->GetGameInstance())
		{
			if (UDRTuningSubsystem* Tuning = World->GetGameInstance()->GetSubsystem<UDRTuningSubsystem>())
			{
				return Tuning;
			}
		}
		return GOwner.Get();
	}

	void PrintToConsole(const FString& Text, const FColor& Color = FColor::Cyan)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			UE_LOG(LogDR, Display, TEXT("[Tuning] %s"), *Line);
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(ScreenMessageKey + 1, 10.f, Color, Text, /*bNewerOnTop*/ true, ScreenTextScale);
		}
	}

	const TCHAR* NoSubsystemMessage = TEXT("튜닝 서브시스템이 없습니다 (쉬핑 빌드 · -NoTuning · DefaultGame.ini bEnabled=False)");

	FAutoConsoleCommandWithWorldAndArgs GTuningReloadCommand(
		TEXT("DRTuningReload"),
		TEXT("Tuning/튜닝.csv 를 다시 읽어 적용한다 (혼자 · 방장). 참가자는 방장 값을 따른다"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UDRTuningSubsystem* Tuning = FindSubsystemForConsole(World)) Tuning->ReloadLocalSheet(TEXT("콘솔"));
			else PrintToConsole(NoSubsystemMessage, FColor::Orange);
		}));

	FAutoConsoleCommandWithWorldAndArgs GTuningRevertCommand(
		TEXT("DRTuningRevert"),
		TEXT("덮어쓴 튜닝 값을 모두 빌드 기준값으로 되돌린다 (비교용 — DRTuningReload 나 파일 저장으로 다시 적용)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UDRTuningSubsystem* Tuning = FindSubsystemForConsole(World)) Tuning->RevertAllOverrides(TEXT("콘솔"));
			else PrintToConsole(NoSubsystemMessage, FColor::Orange);
		}));

	FAutoConsoleCommandWithWorldAndArgs GTuningStatusCommand(
		TEXT("DRTuningStatus"),
		TEXT("튜닝 파일 위치 · 적용 중인 값 수 · 방장 값 사용 여부를 보여 준다"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UDRTuningSubsystem* Tuning = FindSubsystemForConsole(World)) PrintToConsole(Tuning->GetStatusText());
			else PrintToConsole(NoSubsystemMessage, FColor::Orange);
		}));

	FAutoConsoleCommandWithWorldAndArgs GTuningExportCommand(
		TEXT("DRTuningExport"),
		TEXT("튜닝.csv 의 목록(현재값 · 설명 · 범위)을 다시 쓴다. 변경값 · 메모는 그대로 둔다. 에디터 · -game 에서는 목록을 에셋에서 새로 만든다"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			FString Report;
			if (UDRTuningSubsystem* Tuning = FindSubsystemForConsole(World))
			{
				Tuning->RefreshSheet(Report, /*bRebuildCatalog*/ true);
			}
#if WITH_EDITOR
			else
			{
				// 게임이 안 돌고 있는 에디터 — 쿡용 목록 파일만 만든다
				FString TemplatePath, Error;
				int32 RowCount = 0;
				Report = UDRTuningSubsystem::WriteCatalogTemplate(TemplatePath, Error, RowCount)
					? FString::Printf(TEXT("목록 파일 %d 줄: %s"), RowCount, *TemplatePath)
					: FString::Printf(TEXT("목록 파일을 만들지 못했습니다: %s"), *Error);
			}
#endif
			PrintToConsole(Report.IsEmpty() ? FString(NoSubsystemMessage) : Report);
		}));

	const TCHAR* ReadmeText =
		TEXT("대룬 튜닝 파일 사용법\r\n")
		TEXT("====================\r\n")
		TEXT("\r\n")
		TEXT("1. 튜닝.csv 를 엑셀(또는 구글 시트)로 엽니다.\r\n")
		TEXT("2. 바꾸고 싶은 줄의 [변경값] 칸에 새 값을 씁니다. 비워 두면 빌드 기준값([현재값]) 그대로입니다.\r\n")
		TEXT("     숫자     25 / 0.5 / -0.2   (10% 처럼 % 를 붙이면 0.1 로 읽습니다)\r\n")
		TEXT("     켜고 끄기 true / false\r\n")
		TEXT("3. CSV 형식 그대로 저장합니다. 게임이 켜져 있으면 1초쯤 뒤 자동으로 다시 적용됩니다.\r\n")
		TEXT("   (게임 콘솔 ~ 에서 DRTuningReload 로 직접 다시 읽을 수도 있습니다)\r\n")
		TEXT("4. 화면 왼쪽 위 메시지와 _적용결과.txt 에서 결과를 확인합니다. 틀린 줄은 이유가 적혀 있습니다.\r\n")
		TEXT("\r\n")
		TEXT("무엇이 들어 있나 ([분류] 칸으로 거르세요)\r\n")
		TEXT("- 로봇별 스킬 · 캐릭터 · 투사체 · 효과(GE), 업그레이드 칩 · 설정, 밸런스\r\n")
		TEXT("- 적: 스킬 · 캐릭터 · 효과 · 레벨별 스탯 커브(CT_EnemyAttributes) / 적 AI: 비헤이비어 트리 노드(추격 · 공격 거리, 대기 시간 등)\r\n")
		TEXT("- 스테이지1 · 2: 퍼즐 · 문 · 스폰 · 페이즈 / 웨이브 · 페이즈: 웨이브 표 · 페이즈 목표\r\n")
		TEXT("- \"… 배치\": 맵에 놓인 액터 중 그 액터만 따로 정한 값 (예: 문마다 다른 적 소환 수). 그 맵이 열릴 때 적용됩니다.\r\n")
		TEXT("\r\n")
		TEXT("알아 둘 것\r\n")
		TEXT("- 멀티플레이는 방장(호스트)의 튜닝.csv 가 모두에게 적용됩니다. 참가자의 파일은 무시됩니다.\r\n")
		TEXT("- 언제 바뀌나: 스킬 · 칩 수치는 다음 사용부터, 적 · 투사체는 새로 나오는 것부터 바로 바뀝니다.\r\n")
		TEXT("  이미 맵에 있는 캐릭터 · 퍼즐 · 배치 값, 캐릭터 기본 스탯(GE), 웨이브 표처럼 시작할 때 한 번 읽는 값은\r\n")
		TEXT("  다음 스테이지(맵을 다시 열 때)부터 바뀝니다.\r\n")
		TEXT("- DRTuningRevert 는 모든 변경을 잠시 끄고 기준값으로 되돌립니다 (비교용). 저장하거나 DRTuningReload 하면 다시 켜집니다.\r\n")
		TEXT("- 새 빌드를 받으면 예전 폴더의 튜닝.csv 를 새 빌드의 Tuning 폴더에 복사하세요.\r\n")
		TEXT("  게임이 [변경값] · [메모] 는 그대로 두고 [현재값] · [설명] 만 새 빌드 기준으로 고쳐 씁니다.\r\n")
		TEXT("- [분류] [대상] [항목] [경로] 칸은 고치지 마세요. 직접 남길 말은 [메모] 칸에 씁니다.\r\n")
		TEXT("- 목록에 없는 값도 줄을 직접 추가해 바꿀 수 있습니다 — [대상](에셋 이름) · [항목](속성 경로) · [변경값].\r\n")
		TEXT("  예) GA_SeedCannon , WaterCost , 25\r\n")
		TEXT("- 엑셀에서 튜닝.csv 를 연 채로 게임을 시작하면 목록 갱신만 건너뜁니다 (값 적용은 됩니다).\r\n")
		TEXT("- 확정된 값은 개발자가 에디터에서 에셋에 반영합니다 (DRTuningBake).\r\n");
}

UDRTuningSubsystem* UDRTuningSubsystem::Get(const UObject* WorldContextObject)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetSubsystem<UDRTuningSubsystem>() : nullptr;
}

bool UDRTuningSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!Super::ShouldCreateSubsystem(Outer)) return false;
	if (FParse::Param(FCommandLine::Get(), TEXT("NoTuning"))) return false;
	return bEnabled;
#endif
}

FString UDRTuningSubsystem::GetSheetPath()
{
	// -TuningFolder="D:/다른 폴더" 로 바꿀 수 있다 (한 PC 에서 방장 · 참가자를 따로 띄워 볼 때)
	FString Folder;
	if (!FParse::Value(FCommandLine::Get(), TEXT("TuningFolder="), Folder))
	{
		Folder = FPaths::ProjectDir() / TEXT("Tuning");
	}
	return FPaths::ConvertRelativePathToFull(Folder / TEXT("튜닝.csv"));
}

FString UDRTuningSubsystem::GetCatalogTemplatePath()
{
	// pak 은 상대 경로(../../../DaeRune/Content/)로 마운트된다 — 전체 경로로 바꾸지 않는다
	return FPaths::ProjectContentDir() / TEXT("Tuning") / TEXT("TuningCatalog.csv");
}

bool UDRTuningSubsystem::IsApplyAllowed() const
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return CVarTuningApplyInPIE.GetValueOnGameThread() != 0;
	}
#endif
	return true;
}

void UDRTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	WorldActorsInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &ThisClass::HandleWorldActorsInitialized);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::HandleTick), 0.5f);

	if (!IsApplyAllowed())
	{
		UE_LOG(LogDR, Log, TEXT("[Tuning] 에디터 PIE 라 튜닝 파일을 적용하지 않습니다 (dr.Tuning.ApplyInPIE 1 로 켤 수 있음)"));
		return;
	}
	if (GOwner.IsValid() && GOwner.Get() != this)
	{
		return;	// PIE 한 프로세스 멀티 — 처음 인스턴스가 이미 맡았다 (CDO 를 같이 쓴다)
	}

	const bool bFirstRun = !IFileManager::Get().FileExists(*GetSheetPath());

	EnsureReadme();
	FString Message;
	if (RefreshSheet(Message))
	{
		UE_LOG(LogDR, Log, TEXT("[Tuning] %s (%s)"), *Message, *GetSheetPath());
	}
	else
	{
		UE_LOG(LogDR, Warning, TEXT("[Tuning] %s (%s)"), *Message, *GetSheetPath());
	}

	ReloadLocalSheet(TEXT("게임 시작"));

	if (bFirstRun && IFileManager::Get().FileExists(*GetSheetPath()))
	{
		ShowScreenMessage(FString::Printf(TEXT("[튜닝] 튜닝 파일을 만들었습니다 — %s\n[변경값] 칸을 채우고 저장하면 적용됩니다 (Tuning/_사용법.txt)"), *GetSheetPath()), FColor::Cyan);
	}
}

void UDRTuningSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldActorsInitializedHandle);

	if (GOwner.Get() == this)
	{
#if WITH_EDITOR
		// 에디터: CDO 를 바꾼 채로 두면 BP 를 저장할 때 그 값이 에셋에 박힌다 — PIE 가 끝나면 반드시 되돌린다
		if (GIsEditor)
		{
			const int32 Reverted = RevertAllTouches();
			if (Reverted > 0)
			{
				UE_LOG(LogDR, Log, TEXT("[Tuning] PIE 종료 — 덮어쓴 값 %d 개를 되돌렸습니다"), Reverted);
			}
		}
#endif
		GPendingWrites.Reset();
		GOwner.Reset();
	}

	PinnedObjects.Reset();
	Super::Deinitialize();
}

void UDRTuningSubsystem::ReloadLocalSheet(const FString& Reason)
{
	if (GOwner.IsValid() && GOwner.Get() != this)
	{
		GOwner->ReloadLocalSheet(Reason);
		return;
	}
	if (!IsApplyAllowed())
	{
		ShowScreenMessage(TEXT("[튜닝] 에디터 PIE 에서는 꺼져 있습니다 — 콘솔에서 dr.Tuning.ApplyInPIE 1 뒤 DRTuningReload"), FColor::Orange);
		return;
	}
	if (bFollowingHost)
	{
		ShowScreenMessage(TEXT("[튜닝] 참가자는 방장 값을 씁니다 — 방장 컴퓨터의 튜닝.csv 를 고치세요"), FColor::Orange);
		return;
	}

	RememberSheetStamp();

	const FString SheetPath = GetSheetPath();
	TArray<FDRTuningOverride> Overrides;
	TArray<int32> LineNumbers;

	if (IFileManager::Get().FileExists(*SheetPath))
	{
		FString Text, Error;
		TArray<FDRTuningRow> Rows;
		if (!DRTuningSheet::LoadFileText(SheetPath, Text) || !DRTuningSheet::ParseSheet(Text, Rows, Error))
		{
			// 읽지 못하면 지금 적용 중인 값을 그대로 둔다 (저장 도중일 수 있다)
			const FString Message = FString::Printf(TEXT("[튜닝] 튜닝.csv 를 읽지 못해 이전 값을 유지합니다 — %s"), Error.IsEmpty() ? TEXT("파일을 열 수 없음") : *Error);
			UE_LOG(LogDR, Warning, TEXT("%s"), *Message);
			ShowScreenMessage(Message, FColor::Red);
			return;
		}
		DRTuningSheet::ExtractOverrides(Rows, Overrides, LineNumbers);
	}

	// 파일이 없으면 빈 목록 — 덮어쓴 값이 모두 기준값으로 돌아간다
	ApplyOverrides(Overrides, LineNumbers, /*bFromHost*/ false, FString::Printf(TEXT("내 파일 · %s"), *Reason));
}

void UDRTuningSubsystem::RevertAllOverrides(const FString& Reason)
{
	if (GOwner.IsValid() && GOwner.Get() != this)
	{
		GOwner->RevertAllOverrides(Reason);
		return;
	}
	if (bFollowingHost)
	{
		ShowScreenMessage(TEXT("[튜닝] 참가자는 방장 값을 쓰므로 되돌릴 수 없습니다"), FColor::Orange);
		return;
	}
	ApplyOverrides({}, {}, /*bFromHost*/ false, FString::Printf(TEXT("모두 되돌림 · %s"), *Reason));
}

void UDRTuningSubsystem::ApplyOverrides(const TArray<FDRTuningOverride>& Overrides, const TArray<int32>& LineNumbers, bool bFromHost, const FString& SourceLabel)
{
	GOwner = this;

	const int32 Reverted = RevertAllTouches();
	AppliedOverrides.Reset();
	LevelOverrides.Reset();
	PinnedObjects.Reset();

	TArray<FString> BaseIssues;
	CollectProgressionIssues(BaseIssues);

	TArray<FString> Lines;
	TMap<FString, int32> AppliedIndexByKey;
	int32 AppliedCount = 0;
	int32 WarningCount = 0;
	int32 ErrorCount = 0;

	for (int32 Index = 0; Index < Overrides.Num(); ++Index)
	{
		const FDRTuningOverride& Override = Overrides[Index];
		const int32 LineNumber = LineNumbers.IsValidIndex(Index) ? LineNumbers[Index] : 0;
		const FString Where = LineNumber > 0 ? FString::Printf(TEXT("%d번째 줄  "), LineNumber) : FString();
		const FString Label = FString::Printf(TEXT("%s · %s"), *ShortTargetName(Override.Target), *Override.Property);

		FDRTuningOverride Normalized;
		FString Detail, Warning, Error;
		if (Override.Property.TrimStartAndEnd().IsEmpty())
		{
			Error = TEXT("[항목] 이 비었습니다");
		}
		if (!Error.IsEmpty() || !ApplyOne(Override, Normalized, Detail, Warning, Error))
		{
			++ErrorCount;
			Lines.Add(FString::Printf(TEXT("✘ %s%s = %s  →  %s"), *Where, *Label, *Override.Value, *Error));
			continue;
		}

		// 같은 항목이 또 나오면 아래 줄이 이긴다 (보낼 목록에서도 앞 것을 바꾼다)
		const FString Key = CatalogKey(Normalized.Target, Normalized.Property);
		if (const int32* Existing = AppliedIndexByKey.Find(Key))
		{
			AppliedOverrides[*Existing] = Normalized;
			Warning = TEXT("같은 항목이 위에도 있습니다 — 이 줄 값이 쓰입니다");
		}
		else
		{
			AppliedIndexByKey.Add(Key, AppliedOverrides.Add(Normalized));
		}

		++AppliedCount;
		if (Warning.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("✔ %s%s  %s"), *Where, *Label, *Detail));
		}
		else
		{
			++WarningCount;
			Lines.Add(FString::Printf(TEXT("⚠ %s%s  %s  — %s"), *Where, *Label, *Detail, *Warning));
		}
	}

	// 칩 카탈로그 · 진행 설정 검사 — 이번 값 때문에 새로 생긴 문제만
	TArray<FString> NewIssues;
	CollectProgressionIssues(NewIssues);
	for (const FString& Issue : NewIssues)
	{
		if (!BaseIssues.Contains(Issue))
		{
			++WarningCount;
			Lines.Add(FString::Printf(TEXT("⚠ 검사  %s"), *Issue));
		}
	}

	RefreshUpgradeCaches();
	++Revision;

	// ── 보고 ──
	const FString Summary = FString::Printf(TEXT("적용 %d · 경고 %d · 오류 %d%s"),
		AppliedCount, WarningCount, ErrorCount, Reverted > 0 ? *FString::Printf(TEXT("  (이전 값 %d 개 되돌림)"), Reverted) : TEXT(""));

	FString Report = FString::Printf(TEXT("[%s] 튜닝 적용 — %s\r\n파일: %s\r\n%s\r\n\r\n"),
		*FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")), *SourceLabel, *GetSheetPath(), *Summary);
	for (const FString& Line : Lines)
	{
		Report += Line + TEXT("\r\n");
	}
	if (Lines.Num() == 0)
	{
		Report += TEXT("변경값이 채워진 줄이 없습니다 — 모두 빌드 기준값입니다.\r\n");
	}
	if (!bFromHost)
	{
		WriteReport(Report);
	}

	UE_LOG(LogDR, Log, TEXT("[Tuning] %s — %s"), *SourceLabel, *Summary);
	for (const FString& Line : Lines)
	{
		UE_LOG(LogDR, Log, TEXT("[Tuning]   %s"), *Line);
	}

	const FString Source = bFromHost ? TEXT("방장 값") : TEXT("내 파일");
	if (ErrorCount > 0)
	{
		ShowScreenMessage(FString::Printf(TEXT("[튜닝] %s %d 개 적용 · 오류 %d — Tuning/_적용결과.txt 를 확인하세요"), *Source, AppliedCount, ErrorCount), FColor::Red);
	}
	else if (AppliedCount > 0)
	{
		ShowScreenMessage(FString::Printf(TEXT("[튜닝] %s %d 개 적용%s"), *Source, AppliedCount, WarningCount > 0 ? *FString::Printf(TEXT(" · 경고 %d (_적용결과.txt)"), WarningCount) : TEXT("")),
			WarningCount > 0 ? FColor::Yellow : FColor::Green);
	}
	else if (Reverted > 0)
	{
		ShowScreenMessage(FString::Printf(TEXT("[튜닝] 변경값이 없어 기준값으로 되돌렸습니다 (%d 개)"), Reverted), FColor::White);
	}

	if (!bFromHost)
	{
		BroadcastToClients();
	}
}

bool UDRTuningSubsystem::ApplyOne(const FDRTuningOverride& Override, FDRTuningOverride& OutNormalized, FString& OutDetail, FString& OutWarning, FString& OutError)
{
	FString TargetPath;
	if (!ResolveTargetPath(Override.Target, TargetPath, OutError)) return false;

	// 맵 배치 액터 — 맵을 불러오지 않고 그 맵이 열릴 때 적용한다
	FName MapPackage, ActorName;
	if (DRTuning::ParseLevelActorPath(TargetPath, MapPackage, ActorName))
	{
		return ApplyLevelOverride(Override, MapPackage, ActorName, TargetPath, OutNormalized, OutDetail, OutError);
	}

	FString LoadPath;
	UObject* Root = DRTuning::LoadTargetObject(TargetPath, LoadPath, OutError);
	if (!Root) return false;

	// CDO 는 클래스가 GC 로 내려가면 새로 만들어지며 덮어쓴 값을 잃는다
	PinnedObjects.AddUnique(Root->HasAnyFlags(RF_ClassDefaultObject) ? static_cast<UObject*>(Root->GetClass()) : Root);

	const FString Property = Override.Property.TrimStartAndEnd();
	DRTuning::FResolvedValue Value;
	if (!DRTuning::ResolvePath(Root, Property, Value, OutError)) return false;

	const DRTuning::FTypedValue OldValue = DRTuning::ReadValue(Value);
	const FString OldText = DRTuning::ValueToText(Value);
	if (!DRTuning::SetValueFromText(Value, Override.Value, OutError)) return false;

	const DRTuning::FTypedValue NewValue = DRTuning::ReadValue(Value);
	const FString NewText = DRTuning::ValueToText(Value);
	RecordTouch(Root, Property, OldValue, NewValue, OldText);

	int32 DerivedCount = 0;
	int32 InstanceCount = 0;
	if (Root->HasAnyFlags(RF_ClassDefaultObject))
	{
		InstanceCount += SyncInstances(Root->GetClass(), Property, OldValue, NewValue);

		// 하위 BP — 이 값을 물려받은(부모와 같은 값인) CDO 만. 자기 값을 따로 정한 BP 는 그대로 둔다
		TArray<UClass*> DerivedClasses;
		GetDerivedClasses(Root->GetClass(), DerivedClasses, /*bRecursive*/ true);
		for (UClass* Derived : DerivedClasses)
		{
			if (Derived->HasAnyClassFlags(CLASS_NewerVersionExists | CLASS_Deprecated)) continue;
			UObject* DerivedDefault = Derived->GetDefaultObject(/*bCreateIfNeeded*/ false);
			if (!DerivedDefault) continue;

			DRTuning::FResolvedValue DerivedValue;
			FString IgnoredError;
			if (!DRTuning::ResolvePath(DerivedDefault, Property, DerivedValue, IgnoredError)) continue;
			if (DRTuning::ReadValue(DerivedValue) != OldValue) continue;

			const FString DerivedOldText = DRTuning::ValueToText(DerivedValue);
			DRTuning::WriteValue(DerivedValue, NewValue);
			RecordTouch(DerivedDefault, Property, OldValue, NewValue, DerivedOldText);
			InstanceCount += SyncInstances(Derived, Property, OldValue, NewValue);
			++DerivedCount;
		}
	}

	OutNormalized.Target = LoadPath;
	OutNormalized.Property = Property;
	OutNormalized.Value = NewText;

	OutDetail = FString::Printf(TEXT("%s → %s"), *OldText, *NewText);
	if (DerivedCount > 0 || InstanceCount > 0)
	{
		OutDetail += FString::Printf(TEXT("  (하위 BP %d · 실행 중 인스턴스 %d 에도)"), DerivedCount, InstanceCount);
	}

	// 에디터 허용 범위(ClampMin/Max) — 시험이라 막지는 않고 알리기만 한다
	if (const FString* RangeText = RangeByKey.Find(CatalogKey(LoadPath, Property)))
	{
		TOptional<double> Min, Max;
		double Number = 0.0;
		if (DRTuningSheet::ParseRange(*RangeText, Min, Max) && DRTuning::TryGetNumber(Value, Number)
			&& ((Min.IsSet() && Number < Min.GetValue()) || (Max.IsSet() && Number > Max.GetValue())))
		{
			OutWarning = FString::Printf(TEXT("에디터 허용 범위(%s) 밖입니다"), **RangeText);
		}
	}
	return true;
}

bool UDRTuningSubsystem::ResolveTargetPath(const FString& InTarget, FString& OutPath, FString& OutError) const
{
	OutPath = InTarget.TrimStartAndEnd();
	if (OutPath.IsEmpty())
	{
		OutError = TEXT("[대상] 이 비었습니다");
		return false;
	}
	if (OutPath.StartsWith(TEXT("/")))
	{
		return true;
	}

	// 짧은 이름 — 목록에서 먼저, 없으면 에셋 레지스트리에서
	if (const FString* Path = AliasToPath.Find(OutPath.ToLower()))
	{
		OutPath = *Path;
		return true;
	}
	return FindAssetPathByName(InTarget.TrimStartAndEnd(), OutPath, OutError);
}

bool UDRTuningSubsystem::ApplyLevelOverride(const FDRTuningOverride& Override, FName MapPackage, FName ActorName, const FString& TargetPath,
	FDRTuningOverride& OutNormalized, FString& OutDetail, FString& OutError)
{
	FLevelOverride Level;
	Level.MapPackage = MapPackage;
	Level.ActorName = ActorName;
	Level.Property = Override.Property.TrimStartAndEnd();
	Level.Value = Override.Value.TrimStartAndEnd();

	// 같은 액터 · 항목이 또 나오면 아래 줄이 이긴다
	LevelOverrides.RemoveAll([&Level](const FLevelOverride& Existing)
	{
		return Existing.MapPackage == Level.MapPackage && Existing.ActorName == Level.ActorName && Existing.Property == Level.Property;
	});
	LevelOverrides.Add(Level);

	OutNormalized.Target = TargetPath;
	OutNormalized.Property = Level.Property;
	OutNormalized.Value = Level.Value;

	// 그 맵이 지금 열려 있으면 바로 (이미 BeginPlay 한 액터라 시작할 때 읽는 값은 맵을 다시 열어야 바뀐다)
	int32 LiveApplied = 0;
	TArray<FString> LiveErrors;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld() || !World->AreActorsInitialized()) continue;
			LiveApplied += ApplyLevelOverridesToWorld(World, MakeArrayView(&Level, 1), LiveErrors);
		}
	}
	if (LiveErrors.Num() > 0)
	{
		OutError = LiveErrors[0];
		LevelOverrides.Pop();
		return false;
	}

	OutDetail = LiveApplied > 0
		? FString::Printf(TEXT("= %s  (지금 맵에 적용 — 시작할 때 읽는 값은 맵을 다시 열면 바뀐다)"), *Level.Value)
		: FString::Printf(TEXT("= %s  (%s 맵이 열릴 때 적용)"), *Level.Value, *FPackageName::GetShortName(MapPackage));
	return true;
}

int32 UDRTuningSubsystem::ApplyLevelOverridesToWorld(UWorld* World, TArrayView<const FLevelOverride> Overrides, TArray<FString>& OutErrors) const
{
	if (!World) return 0;

	// PIE 월드는 패키지 이름 앞에 UEDPIE_0_ 이 붙는다
	const FName WorldPackage(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));

	int32 Applied = 0;
	for (const FLevelOverride& Level : Overrides)
	{
		if (Level.MapPackage != WorldPackage) continue;

		AActor* Actor = nullptr;
		for (const ULevel* Loaded : World->GetLevels())
		{
			if (!Loaded) continue;
			for (AActor* Candidate : Loaded->Actors)
			{
				if (Candidate && Candidate->GetFName() == Level.ActorName)
				{
					Actor = Candidate;
					break;
				}
			}
			if (Actor) break;
		}

		const FString Label = FString::Printf(TEXT("%s:%s · %s"), *FPackageName::GetShortName(Level.MapPackage), *Level.ActorName.ToString(), *Level.Property);
		if (!Actor)
		{
			OutErrors.Add(FString::Printf(TEXT("%s — 맵에 그 액터가 없습니다 (이름이 바뀌었거나 지워졌을 수 있음)"), *Label));
			continue;
		}

		DRTuning::FResolvedValue Value;
		FString Error;
		if (!DRTuning::ResolvePath(Actor, Level.Property, Value, Error))
		{
			OutErrors.Add(FString::Printf(TEXT("%s — %s"), *Label, *Error));
			continue;
		}

		const DRTuning::FTypedValue OldValue = DRTuning::ReadValue(Value);
		const FString OldText = DRTuning::ValueToText(Value);
		if (!DRTuning::SetValueFromText(Value, Level.Value, Error))
		{
			OutErrors.Add(FString::Printf(TEXT("%s — %s"), *Label, *Error));
			continue;
		}
		RecordTouch(Actor, Level.Property, OldValue, DRTuning::ReadValue(Value), OldText);
		UE_LOG(LogDR, Log, TEXT("[Tuning]   배치 액터 %s  %s → %s"), *Label, *OldText, *DRTuning::ValueToText(Value));
		++Applied;
	}
	return Applied;
}

bool UDRTuningSubsystem::LoadCatalog(TArray<FDRTuningRow>& OutRows, FString& OutMessage, bool bRebuildCatalog)
{
	OutRows.Reset();

	FString Text, Error;
	const bool bCooked = FPlatformProperties::RequiresCookedData();

	// 패키지 빌드는 쿡 때 에디터가 만든 목록(pak 안). 에디터 · -game 도 저장해 둔 목록이 있으면 그것 —
	// 목록을 새로 만들려면 BP · 맵을 전부 불러와야 해서 시작이 수십 초 느려진다 (새로 만들기는 DRTuningExport)
	if ((bCooked || !bRebuildCatalog)
		&& DRTuningSheet::LoadFileText(GetCatalogTemplatePath(), Text) && DRTuningSheet::ParseSheet(Text, OutRows, Error) && OutRows.Num() > 0)
	{
		RemoveRowsNotInBuild(OutRows);
		return true;
	}

	if (!bCooked)
	{
		// 에디터 · -game(쿠킹 전 에셋): 지금 에셋에서 바로 — 설명 · 범위도 메타데이터에서. 다음 시작을 위해 목록 파일로도 남긴다
		DRTuningCatalog::BuildLiveCatalog(MakeCatalogSettings(*this), OutRows, MakeOriginalLookup());
		if (OutRows.Num() > 0)
		{
			DRTuningSheet::SaveText(FPaths::ConvertRelativePathToFull(GetCatalogTemplatePath()), DRTuningSheet::BuildSheetText(OutRows));
		}
		return OutRows.Num() > 0;
	}

	// 목록 파일이 없다(쿡 훅이 돌지 않았다) — 지금 에셋을 훑는다. 메타데이터가 없어 [설명] · [범위] 는 빈다
	DRTuningCatalog::BuildLiveCatalog(MakeCatalogSettings(*this), OutRows, MakeOriginalLookup());
	OutMessage = TEXT("빌드에 목록 파일(Content/Tuning/TuningCatalog.csv)이 없어 설명 없이 만들었습니다. ");
	return OutRows.Num() > 0;
}

void UDRTuningSubsystem::IndexCatalog(const TArray<FDRTuningRow>& Rows)
{
	AliasToPath.Reset();
	RangeByKey.Reset();

	TSet<FString> Ambiguous;
	for (const FDRTuningRow& Row : Rows)
	{
		if (Row.Path.IsEmpty()) continue;

		const FString Alias = Row.Target.ToLower();
		if (!Alias.IsEmpty())
		{
			if (const FString* Existing = AliasToPath.Find(Alias))
			{
				if (!Existing->Equals(Row.Path, ESearchCase::IgnoreCase)) Ambiguous.Add(Alias);
			}
			else
			{
				AliasToPath.Add(Alias, Row.Path);
			}
		}
		if (!Row.Range.IsEmpty())
		{
			RangeByKey.Add(CatalogKey(Row.Path, Row.Property), Row.Range);
		}
	}

	// 이름이 겹치는 에셋은 짧은 이름으로 찾지 않는다 — [경로] 로만
	for (const FString& Alias : Ambiguous)
	{
		AliasToPath.Remove(Alias);
	}
}

bool UDRTuningSubsystem::RefreshSheet(FString& OutMessage, bool bRebuildCatalog)
{
	TArray<FDRTuningRow> Catalog;
	FString CatalogMessage;
	if (!LoadCatalog(Catalog, CatalogMessage, bRebuildCatalog))
	{
		OutMessage = CatalogMessage + TEXT("목록이 비었습니다 — DefaultGame.ini 의 ListedBlueprintBases · ListedAssets 를 확인하세요");
		return false;
	}
	IndexCatalog(Catalog);

	const FString SheetPath = GetSheetPath();
	const bool bExists = IFileManager::Get().FileExists(*SheetPath);

	FString ExistingText;
	TArray<FDRTuningRow> Existing;
	if (bExists)
	{
		FString Error;
		if (!DRTuningSheet::LoadFileText(SheetPath, ExistingText) || !DRTuningSheet::ParseSheet(ExistingText, Existing, Error))
		{
			// 기획자가 고친 파일일 수 있다 — 읽지 못하면 절대 덮어쓰지 않는다
			OutMessage = FString::Printf(TEXT("튜닝.csv 를 읽지 못해 목록을 갱신하지 않았습니다 (%s)"), *Error);
			return false;
		}
	}

	TArray<FDRTuningRow> Merged;
	DRTuningSheet::MergeSheet(Catalog, Existing, Merged);
	const FString NewText = DRTuningSheet::BuildSheetText(Merged);

	if (bExists && ExistingText.Equals(NewText, ESearchCase::CaseSensitive))
	{
		OutMessage = CatalogMessage + FString::Printf(TEXT("목록이 이미 최신입니다 (%d 줄)"), Merged.Num());
		return true;
	}
	if (!DRTuningSheet::SaveText(SheetPath, NewText))
	{
		OutMessage = CatalogMessage + TEXT("튜닝.csv 를 쓰지 못했습니다 — 엑셀에서 열려 있으면 닫고 DRTuningExport 하세요");
		return false;
	}

	RememberSheetStamp();
	OutMessage = CatalogMessage + FString::Printf(TEXT("튜닝.csv 목록을 %d 줄로 갱신했습니다"), Merged.Num());
	return true;
}

#if WITH_EDITOR
bool UDRTuningSubsystem::WriteCatalogTemplate(FString& OutPath, FString& OutError, int32& OutRowCount, const TFunction<bool(FName)>& PackageFilter)
{
	// 쿡 중이면 목록을 만들려고 불러온 에셋이 "게임에서 쓰는 것" 으로 잡혀 빌드에 끌려 들어가지 않게 한다
	FCookLoadScope EditorOnlyLoads(ECookLoadType::EditorOnly);

	TArray<FDRTuningRow> Rows;
	DRTuningCatalog::BuildLiveCatalog(MakeCatalogSettings(*GetDefault<UDRTuningSubsystem>()), Rows, MakeOriginalLookup(), PackageFilter);
	OutRowCount = Rows.Num();
	OutPath = FPaths::ConvertRelativePathToFull(GetCatalogTemplatePath());

	if (Rows.Num() == 0)
	{
		OutError = TEXT("목록이 비었습니다 — DefaultGame.ini 의 ListedBlueprintBases · ListedAssets 를 확인하세요");
		return false;
	}
	if (!DRTuningSheet::SaveText(OutPath, DRTuningSheet::BuildSheetText(Rows)))
	{
		OutError = FString::Printf(TEXT("파일을 쓰지 못했습니다: %s"), *OutPath);
		return false;
	}
	return true;
}
#endif

void UDRTuningSubsystem::SendTuningToClient(ADRPlayerController* PlayerController) const
{
	if (!PlayerController) return;

	// 값을 맡은 쪽의 목록 (PIE 한 프로세스면 처음 인스턴스)
	const UDRTuningSubsystem* Source = GOwner.IsValid() ? GOwner.Get() : this;
	const TArray<FDRTuningOverride>& All = Source->AppliedOverrides;

	// 한 번에 다 보내면 RPC 크기 제한에 걸릴 수 있어 나눠 보낸다. 값이 없어도 한 조각은 보낸다(참가자가 자기 값을 되돌리게)
	const int32 ChunkCount = FMath::Max(1, FMath::DivideAndRoundUp(All.Num(), HostChunkSize));
	for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
	{
		TArray<FDRTuningOverride> Chunk;
		const int32 Begin = ChunkIndex * HostChunkSize;
		const int32 End = FMath::Min(All.Num(), Begin + HostChunkSize);
		for (int32 Index = Begin; Index < End; ++Index)
		{
			Chunk.Add(All[Index]);
		}
		PlayerController->ClientReceiveTuningChunk(Source->Revision, ChunkIndex, ChunkCount, Chunk);
	}
}

void UDRTuningSubsystem::ReceiveHostChunk(int32 InRevision, int32 ChunkIndex, int32 ChunkCount, const TArray<FDRTuningOverride>& Chunk)
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return;	// PIE 한 프로세스 — 방장과 CDO 를 같이 쓰므로 받을 것이 없다
	}
#endif
	if (ChunkCount <= 0 || ChunkIndex < 0 || ChunkIndex >= ChunkCount) return;

	if (InRevision != PendingHostRevision || ChunkCount != PendingHostChunkCount)
	{
		PendingHostRevision = InRevision;
		PendingHostChunkCount = ChunkCount;
		PendingHostChunks.Reset();
	}
	PendingHostChunks.Add(ChunkIndex, Chunk);
	if (PendingHostChunks.Num() < ChunkCount) return;

	TArray<FDRTuningOverride> All;
	for (int32 Index = 0; Index < ChunkCount; ++Index)
	{
		if (const TArray<FDRTuningOverride>* Part = PendingHostChunks.Find(Index))
		{
			All.Append(*Part);
		}
	}
	PendingHostChunks.Reset();
	PendingHostRevision = INDEX_NONE;
	PendingHostChunkCount = 0;

	bFollowingHost = true;
	ApplyOverrides(All, {}, /*bFromHost*/ true, TEXT("방장 값"));
}

void UDRTuningSubsystem::BroadcastToClients() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return;

	const ENetMode NetMode = World->GetNetMode();
	if (NetMode != NM_ListenServer && NetMode != NM_DedicatedServer) return;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		ADRPlayerController* PlayerController = Cast<ADRPlayerController>(It->Get());
		if (PlayerController && !PlayerController->IsLocalController())
		{
			SendTuningToClient(PlayerController);
		}
	}
}

void UDRTuningSubsystem::RefreshUpgradeCaches() const
{
	if (!GEngine) return;

	// 칩 수치 캐시(FDRUpgradeRuntime)는 장착이 바뀔 때만 다시 만들어진다 — 바뀐 칩 수치가 바로 들도록 모두 다시 만든다
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || !World->IsGameWorld()) continue;

		for (TActorIterator<ADRPlayerState> It(World); It; ++It)
		{
			It->RebuildUpgradeRuntime();
		}
	}
}

void UDRTuningSubsystem::CollectProgressionIssues(TArray<FString>& OutIssues) const
{
	const UDRGameInstance* GameInstance = Cast<UDRGameInstance>(GetGameInstance());
	const UDRProgressionConfig* Config = GameInstance ? GameInstance->ProgressionConfig.Get() : nullptr;
	if (!Config) return;

	Config->ValidateChipTuning(OutIssues);
	if (const UDRChipCatalog* Catalog = GameInstance->GetChipCatalog())
	{
		TArray<FString> CatalogIssues;	// ValidateCatalog 는 배열을 비우고 시작한다
		Catalog->ValidateCatalog(Config->GetMaxSlotCount(), CatalogIssues);
		OutIssues.Append(CatalogIssues);
	}
}

FString UDRTuningSubsystem::GetStatusText() const
{
	const UDRTuningSubsystem* Owner = GOwner.IsValid() ? GOwner.Get() : this;

	FString Mode;
	if (!IsApplyAllowed())			Mode = TEXT("꺼짐 (에디터 PIE — dr.Tuning.ApplyInPIE 1 로 켬)");
	else if (Owner->bFollowingHost)	Mode = TEXT("방장 값 사용 중 (내 파일은 무시)");
	else								Mode = TEXT("내 파일");

	return FString::Printf(TEXT("튜닝 상태\n파일: %s\n모드: %s\n적용 중: %d 개 (적용 %d 회)\n자동 다시 읽기: %s"),
		*GetSheetPath(), *Mode, Owner->AppliedOverrides.Num(), Owner->Revision, bAutoReload ? TEXT("켬") : TEXT("끔"));
}

void UDRTuningSubsystem::WriteReport(const FString& Text) const
{
	const FString ReportPath = FPaths::GetPath(GetSheetPath()) / TEXT("_적용결과.txt");
	if (!DRTuningSheet::SaveText(ReportPath, Text))
	{
		UE_LOG(LogDR, Warning, TEXT("[Tuning] 결과 파일을 쓰지 못했습니다: %s"), *ReportPath);
	}
}

void UDRTuningSubsystem::EnsureReadme() const
{
	const FString ReadmePath = FPaths::GetPath(GetSheetPath()) / TEXT("_사용법.txt");
	FString Existing;
	if (DRTuningSheet::LoadFileText(ReadmePath, Existing) && Existing.Equals(ReadmeText, ESearchCase::CaseSensitive))
	{
		return;
	}
	DRTuningSheet::SaveText(ReadmePath, ReadmeText);
}

void UDRTuningSubsystem::ShowScreenMessage(const FString& Message, const FColor& Color)
{
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->AddOnScreenDebugMessage(ScreenMessageKey, 8.f, Color, Message, /*bNewerOnTop*/ true, ScreenTextScale);
	}
	else
	{
		// 게임 시작 직후에는 아직 화면이 없다 — 첫 맵이 열리면 띄운다
		PendingScreenMessage = Message;
		PendingScreenColor = Color;
	}
}

void UDRTuningSubsystem::RememberSheetStamp()
{
	const FFileStatData Stat = IFileManager::Get().GetStatData(*GetSheetPath());
	WatchedStamp = Stat.bIsValid ? Stat.ModificationTime : FDateTime::MinValue();
	WatchedSize = Stat.bIsValid ? Stat.FileSize : -1;
	bChangeSeen = false;
}

bool UDRTuningSubsystem::HandleTick(float DeltaTime)
{
	if (GOwner.Get() == this)
	{
		ProcessPendingWrites();
	}

	if (!bAutoReload || bFollowingHost || !IsApplyAllowed()) return true;
	if (GOwner.IsValid() && GOwner.Get() != this) return true;

	const FFileStatData Stat = IFileManager::Get().GetStatData(*GetSheetPath());
	const FDateTime Stamp = Stat.bIsValid ? Stat.ModificationTime : FDateTime::MinValue();
	const int64 Size = Stat.bIsValid ? Stat.FileSize : -1;

	if (Stamp == WatchedStamp && Size == WatchedSize)
	{
		bChangeSeen = false;
		return true;
	}

	// 바뀐 뒤 한 번 더 같은 모습이면(저장이 끝났으면) 다시 읽는다 — 메모장은 지우고 다시 쓰므로 반쯤 쓴 파일을 읽지 않게
	if (!bChangeSeen || Stamp != SeenStamp || Size != SeenSize)
	{
		bChangeSeen = true;
		SeenStamp = Stamp;
		SeenSize = Size;
		return true;
	}

	ReloadLocalSheet(TEXT("파일 저장"));
	return true;
}

void UDRTuningSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance()) return;

	if (!PendingScreenMessage.IsEmpty() && GEngine && GEngine->GameViewport)
	{
		GEngine->AddOnScreenDebugMessage(ScreenMessageKey, 10.f, PendingScreenColor, PendingScreenMessage, /*bNewerOnTop*/ true, ScreenTextScale);
		PendingScreenMessage.Reset();
	}

	// 방장과의 연결이 끝났다 (메인 메뉴로 돌아옴 · 혼자 하기) — 내 파일로 돌아간다
	if (bFollowingHost && LoadedWorld->GetNetMode() != NM_Client)
	{
		bFollowingHost = false;
		ReloadLocalSheet(TEXT("방장 연결 끝"));
	}
}

void UDRTuningSubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams& Params)
{
	// 액터가 BeginPlay 하기 직전 — 시작할 때 읽는 배치 액터 값도 이때 넣으면 그대로 쓰인다
	UWorld* World = Params.World;
	if (!World || !World->IsGameWorld() || World->GetGameInstance() != GetGameInstance()) return;

	// PIE 한 프로세스 멀티는 값을 맡은 인스턴스의 목록을 쓴다 (배치 액터는 월드마다 따로라 월드마다 넣어야 한다)
	const UDRTuningSubsystem* Owner = GOwner.IsValid() ? GOwner.Get() : this;
	if (Owner->LevelOverrides.Num() == 0) return;

	TArray<FString> Errors;
	const int32 Applied = ApplyLevelOverridesToWorld(World, Owner->LevelOverrides, Errors);
	if (Applied == 0 && Errors.Num() == 0) return;

	const FString MapName = FPackageName::GetShortName(UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()));
	UE_LOG(LogDR, Log, TEXT("[Tuning] %s 맵 배치 액터 값 %d 개 적용 · 오류 %d"), *MapName, Applied, Errors.Num());
	for (const FString& Error : Errors)
	{
		UE_LOG(LogDR, Warning, TEXT("[Tuning]   ✘ %s"), *Error);
	}
	if (Errors.Num() > 0)
	{
		ShowScreenMessage(FString::Printf(TEXT("[튜닝] %s 배치 액터 값 오류 %d — %s"), *MapName, Errors.Num(), *Errors[0]), FColor::Red);
	}
}
