// Copyright DaeRune

#include "Tuning/DRTuningCatalog.h"
#include "Tuning/DRTuningProperty.h"
#include "DaeRune/DRLogChannels.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/ARFilter.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Curves/RichCurve.h"
#include "Curves/SimpleCurve.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "ScalableFloat.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_NativeEnum.h"
#include "Phase/DRPhaseBase.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformProperties.h"

namespace
{
	// [분류] 가 같을 때의 정렬 순서이기도 하다
	enum class ETargetKind : int32
	{
		Ability,
		Actor,
		Effect,
		Phase,
		AINode,
		BehaviorTree,
		DataAsset,
		DataTable,
		CurveTable,
		LevelActor,
		Other
	};

	// 어떤 속성을 싣는가
	enum class EFieldPolicy : uint8
	{
		Ours,		// 오브젝트 맨 위 — 우리 C++(/Script/DaeRune) · 블루프린트가 선언한 속성만 (엔진 공통 속성은 튜닝 값이 아니다)
		All,		// 구조체 · 표 줄 안 — 누가 선언했든
		AINode,		// 비헤이비어 트리 노드 — 엔진 노드라도 수치는 싣는다 (켜고 끄기 · 연산 종류는 동작 구조라 뺀다)
		Whitelist,	// 엔진 컴포넌트(이동 · 투사체 이동) — 정해 둔 이름만
	};

	struct FCatalogTarget
	{
		UObject* Root = nullptr;
		const UObject* CompareTo = nullptr;	// 배치 액터: 이 CDO 와 같은 값은 싣지 않는다 (CDO 줄이 이미 있다)
		FString ShortName;
		FString LoadPath;
		FString Group;
		FString LabelPrefix;
		int32 GroupOrder = 0;
		ETargetKind Kind = ETargetKind::Other;
	};

	struct FWalkContext
	{
		const FCatalogTarget* Target = nullptr;
		TArray<FDRTuningRow>* OutRows = nullptr;
		const DRTuningCatalog::FOriginalLookup* OriginalLookup = nullptr;
		TSet<const UObject*>* VisitedSubObjects = nullptr;
	};

	const FName OurScriptPackage(TEXT("/Script/DaeRune"));

	ETargetKind KindOf(const UObject* Root)
	{
		if (Root->IsA<UGameplayAbility>())	return ETargetKind::Ability;
		if (Root->IsA<UGameplayEffect>())	return ETargetKind::Effect;
		if (Root->IsA<UBTNode>())			return ETargetKind::AINode;
		if (Root->IsA<UDRPhaseBase>())		return ETargetKind::Phase;
		if (Root->IsA<AActor>())			return ETargetKind::Actor;
		if (Root->IsA<UBehaviorTree>())		return ETargetKind::BehaviorTree;
		if (Root->IsA<UDataTable>())		return ETargetKind::DataTable;
		if (Root->IsA<UCurveTable>())		return ETargetKind::CurveTable;
		if (Root->IsA<UDataAsset>())		return ETargetKind::DataAsset;
		return ETargetKind::Other;
	}

	const TCHAR* DefaultGroupLabel(ETargetKind Kind)
	{
		switch (Kind)
		{
		case ETargetKind::Ability:		return TEXT("스킬");
		case ETargetKind::Actor:		return TEXT("액터");
		case ETargetKind::Effect:		return TEXT("효과(GE)");
		case ETargetKind::Phase:		return TEXT("웨이브 · 페이즈");
		case ETargetKind::AINode:
		case ETargetKind::BehaviorTree:	return TEXT("적 AI");
		case ETargetKind::DataAsset:	return TEXT("데이터");
		case ETargetKind::DataTable:	return TEXT("데이터 테이블");
		case ETargetKind::CurveTable:	return TEXT("커브 테이블");
		case ETargetKind::LevelActor:	return TEXT("맵 배치");
		default:						return TEXT("기타");
		}
	}

	void ApplyGroupRule(const FDRTuningCatalogSettings& Settings, FCatalogTarget& Target)
	{
		for (int32 Index = 0; Index < Settings.GroupRules.Num(); ++Index)
		{
			const FDRTuningGroupRule& Rule = Settings.GroupRules[Index];
			if (Rule.PathContains.IsEmpty() || !Target.LoadPath.Contains(Rule.PathContains)) continue;

			// 같은 분류 이름을 여러 규칙이 쓰면 맨 처음 규칙 자리에 모은다 (시트에서 한 덩어리로 보이게)
			Target.Group = Rule.Label;
			Target.GroupOrder = Settings.GroupRules.IndexOfByPredicate([&Rule](const FDRTuningGroupRule& Other) { return Other.Label == Rule.Label; });
			return;
		}
		Target.Group = DefaultGroupLabel(Target.Kind);
		Target.GroupOrder = Settings.GroupRules.Num() + static_cast<int32>(Target.Kind);
	}

	FString FormatNumber(double Value)
	{
		if (Value == 0.0) return TEXT("0");
		return FString::Printf(TEXT("%.7g"), Value);
	}

	// 시트에 싣지 않는 구조체 — 숫자가 들어 있어도 튜닝 값이 아니다 (좌표 · 색 · 태그 · 경로 · 커브 편집 데이터)
	bool IsSkippedStruct(const UScriptStruct* Struct)
	{
		static const TSet<FName> Skipped =
		{
			TEXT("GameplayTag"), TEXT("GameplayTagContainer"), TEXT("GameplayTagQuery"), TEXT("GameplayAttribute"),
			TEXT("GameplayEffectQuery"), TEXT("Vector"), TEXT("Vector2D"), TEXT("Vector3f"), TEXT("Vector4"),
			TEXT("Rotator"), TEXT("Quat"), TEXT("Transform"), TEXT("Color"), TEXT("LinearColor"), TEXT("IntPoint"),
			TEXT("IntVector"), TEXT("Box"), TEXT("Box2D"), TEXT("SoftObjectPath"), TEXT("SoftClassPath"),
			TEXT("RuntimeFloatCurve"), TEXT("CurveTableRowHandle"), TEXT("DataTableRowHandle"), TEXT("DataRegistryType"),
			TEXT("PointerToUberGraphFrame"), TEXT("Guid"), TEXT("DateTime"), TEXT("Timespan"), TEXT("FrameRate"),
			TEXT("Key"), TEXT("SlateBrush"), TEXT("SlateColor"), TEXT("Margin"), TEXT("BlackboardKeySelector"),
			TEXT("BodyInstance"), TEXT("WalkableSlopeOverride"), TEXT("LightingChannels"), TEXT("CollisionResponse"),
		};
		return Skipped.Contains(Struct->GetFName());
	}

	// 편집 가능한 값인가. 배치 액터(인스턴스)는 "인스턴스만 편집"(EditInstanceOnly) 값이 바로 맵마다 정한 값이라 받고,
	// 클래스 기본값(CDO)은 반대로 "기본값만 편집"(EditDefaultsOnly)까지 받는다
	bool IsEditableField(const FProperty* Property, bool bInstance)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Deprecated | CPF_Transient))
		{
			return false;
		}
		return !Property->HasAnyPropertyFlags(bInstance ? CPF_DisableEditOnInstance : CPF_DisableEditOnTemplate);
	}

	bool IsOursOrBlueprint(const UClass* Class)
	{
		return Class && (!Class->HasAnyClassFlags(CLASS_Native) || Class->GetOutermost()->GetFName() == OurScriptPackage);
	}

	bool IsOurTopLevelProperty(const FProperty* Property)
	{
		return IsOursOrBlueprint(Property->GetOwnerClass());
	}

	// 엔진 컴포넌트 중 기획자가 만질 만한 값만 (이동 속도는 MoveSpeed 어트리뷰트가 덮어쓰므로 MaxWalkSpeed 는 뺀다)
	const TSet<FName>* EngineComponentWhitelist(const UClass* Class)
	{
		static const TSet<FName> CharacterMovement =
		{
			TEXT("JumpZVelocity"), TEXT("AirControl"), TEXT("GravityScale"), TEXT("MaxAcceleration"),
			TEXT("BrakingDecelerationWalking"), TEXT("GroundFriction"), TEXT("MaxFlySpeed"), TEXT("MaxStepHeight"),
		};
		static const TSet<FName> ProjectileMovement =
		{
			TEXT("InitialSpeed"), TEXT("MaxSpeed"), TEXT("ProjectileGravityScale"), TEXT("Bounciness"), TEXT("Friction"),
			TEXT("HomingAccelerationMagnitude"),
		};
		if (Class->IsChildOf(UCharacterMovementComponent::StaticClass()))	return &CharacterMovement;
		if (Class->IsChildOf(UProjectileMovementComponent::StaticClass()))	return &ProjectileMovement;
		return nullptr;
	}

	FString FirstLine(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, /*bCullEmpty*/ true);
		for (FString& Line : Lines)
		{
			Line.ReplaceInline(TEXT("★"), TEXT(""));
			// 구역 머리 주석("// ===== 슬롯 =====")이 바로 위에 붙은 속성은 그 줄이 툴팁이 된다 — 장식만 떼고 쓴다
			Line.TrimStartAndEndInline();
			while (Line.RemoveFromStart(TEXT("="))) {}
			while (Line.RemoveFromEnd(TEXT("="))) {}
			Line.TrimStartAndEndInline();
			if (Line.IsEmpty()) continue;
			if (Line.Len() > 150)
			{
				Line = Line.Left(148) + TEXT("…");
			}
			return Line;
		}
		return FString();
	}

	FString DescribeProperty(const FProperty* Property)
	{
#if WITH_EDITOR
		const FString ToolTip = Property->GetToolTipText().ToString();
		// 주석이 없으면 엔진이 속성 이름을 띄어 써서 돌려준다 — [항목] 과 같은 말이라 비운다
		if (ToolTip.IsEmpty() || ToolTip.Equals(Property->GetDisplayNameText().ToString()) || ToolTip.Equals(Property->GetName()))
		{
			return FString();
		}
		return FirstLine(ToolTip);
#else
		return FString();
#endif
	}

	FString DescribeRange(const FProperty* Property)
	{
#if WITH_EDITOR
		FString Min = Property->GetMetaData(TEXT("ClampMin"));
		FString Max = Property->GetMetaData(TEXT("ClampMax"));
		if (Min.IsEmpty() && Max.IsEmpty())
		{
			Min = Property->GetMetaData(TEXT("UIMin"));
			Max = Property->GetMetaData(TEXT("UIMax"));
		}
		if (Min.IsEmpty() && Max.IsEmpty()) return FString();

		auto Normalize = [](const FString& Text) { return Text.IsNumeric() ? FormatNumber(FCString::Atod(*Text)) : Text; };
		return FString::Printf(TEXT("%s ~ %s"), *Normalize(Min), *Normalize(Max)).TrimStartAndEnd();
#else
		return FString();
#endif
	}

	FString EnumDisplay(const UEnum* Enum, int64 Value)
	{
#if WITH_EDITOR
		const FString Display = Enum->GetDisplayNameTextByValue(Value).ToString();
		if (!Display.IsEmpty()) return Display;
#endif
		return Enum->GetNameStringByValue(Value);
	}

	// 구조체 배열 원소의 짧은 이름 — DisplayName 칸이 있으면 그것, 없으면 열거형 칸들 (칩 모디파이어: "스킬 피해량 · 곱연산")
	FString DescribeElement(const UScriptStruct* Struct, const void* ElementPtr)
	{
		if (const FTextProperty* NameProp = FindFProperty<FTextProperty>(Struct, TEXT("DisplayName")))
		{
			const FText& Name = NameProp->GetPropertyValue_InContainer(ElementPtr);
			if (!Name.IsEmpty()) return Name.ToString();
		}

		TArray<FString> Parts;
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const void* FieldPtr = It->ContainerPtrToValuePtr<void>(ElementPtr);
			if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(*It))
			{
				Parts.Add(EnumDisplay(EnumProp->GetEnum(), EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(FieldPtr)));
			}
			else if (const FByteProperty* ByteProp = CastField<FByteProperty>(*It); ByteProp && ByteProp->Enum)
			{
				Parts.Add(EnumDisplay(ByteProp->Enum, ByteProp->GetSignedIntPropertyValue(FieldPtr)));
			}
		}
		return FString::Join(Parts, TEXT(" · "));
	}

	FString JoinLabel(const FString& Outer, const FString& Inner)
	{
		if (Inner.IsEmpty()) return Outer;
		if (Outer.IsEmpty()) return FString::Printf(TEXT("[%s]"), *Inner);
		return Outer + TEXT(" ") + Inner;
	}

	void AddRow(const FWalkContext& Context, const FString& Path, const FProperty* Property, const void* ValuePtr,
		const FString& Label, const FString& Description, const FString& Range)
	{
		DRTuning::FResolvedValue Value;
		Value.Property = const_cast<FProperty*>(Property);
		Value.ValuePtr = const_cast<void*>(ValuePtr);
		Value.OwnerObject = Context.Target->Root;

		// 배치 액터는 클래스 기본값과 다른 값(맵에서 그 액터만 따로 정한 값)만 — 같은 값은 BP 줄에서 바꾼다
		if (Context.Target->CompareTo)
		{
			DRTuning::FResolvedValue Default;
			FString IgnoredError;
			if (DRTuning::ResolvePath(const_cast<UObject*>(Context.Target->CompareTo), Path, Default, IgnoredError)
				&& DRTuning::ReadValue(Default) == DRTuning::ReadValue(Value))
			{
				return;
			}
		}

		FDRTuningRow Row;
		Row.Group = Context.Target->Group;
		Row.Target = Context.Target->ShortName;
		Row.Property = Path;
		Row.Path = Context.Target->LoadPath;
		Row.Range = Range;
		Row.GroupOrder = Context.Target->GroupOrder;
		Row.KindOrder = static_cast<int32>(Context.Target->Kind);

		if (Label.IsEmpty())				Row.Description = Description;
		else if (Description.IsEmpty())	Row.Description = Label;
		else								Row.Description = Label + TEXT(" — ") + Description;

		// [현재값] 은 빌드 기준값 — 이미 덮어쓴 값이면 덮기 전 값을 싣는다
		FString OriginalText;
		if (Context.OriginalLookup && *Context.OriginalLookup && (*Context.OriginalLookup)(Context.Target->Root, Path, OriginalText))
		{
			Row.Current = OriginalText;
		}
		else
		{
			Row.Current = DRTuning::ValueToText(Value);
		}

		Context.OutRows->Add(MoveTemp(Row));
	}

	void WalkValue(const FWalkContext& Context, const FProperty* Property, const void* ValuePtr, const FString& Path,
		const FString& Label, const FString& Description, const FString& Range, int32 Depth);

	void WalkFields(const FWalkContext& Context, const UStruct* Struct, const void* Container, const FString& PathPrefix,
		const FString& Label, int32 Depth, EFieldPolicy Policy, const TSet<FName>* Whitelist = nullptr);

	// 블랙보드 조건의 비교값은 키 종류에 맞는 칸만 (실수 키면 FloatValue, 정수 · 열거형 키면 IntValue) — 나머지는 쓰이지 않는 칸이다
	bool IsBlackboardValueRelevant(const UBehaviorTree* Tree, const UObject* Node, FName ValueName)
	{
		const FStructProperty* KeyProp = FindFProperty<FStructProperty>(Node->GetClass(), TEXT("BlackboardKey"));
		const UBlackboardData* Blackboard = Tree ? Tree->BlackboardAsset.Get() : nullptr;
		if (!KeyProp || KeyProp->Struct != FBlackboardKeySelector::StaticStruct() || !Blackboard) return true;

		const FBlackboardKeySelector* Key = KeyProp->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node);
		const TSubclassOf<UBlackboardKeyType> KeyType = Blackboard->GetKeyType(Blackboard->GetKeyID(Key->SelectedKeyName));
		if (!KeyType) return true;

		if (ValueName == TEXT("FloatValue"))
		{
			return KeyType->IsChildOf(UBlackboardKeyType_Float::StaticClass());
		}
		return KeyType->IsChildOf(UBlackboardKeyType_Int::StaticClass())
			|| KeyType->IsChildOf(UBlackboardKeyType_Enum::StaticClass())
			|| KeyType->IsChildOf(UBlackboardKeyType_NativeEnum::StaticClass());
	}

	bool IsTunableAINodeField(const FWalkContext& Context, const void* Container, const FProperty* Property)
	{
		static const TSet<FName> Structural = { TEXT("OperationType"), TEXT("NodeName"), TEXT("TickInterval") };
		if (Structural.Contains(Property->GetFName())) return false;

		const UClass* OwnerClass = Property->GetOwnerClass();
		const bool bEngineOwner = OwnerClass && !IsOursOrBlueprint(OwnerClass);
		if (bEngineOwner && Property->IsA<FBoolProperty>()) return false;

		const FName Name = Property->GetFName();
		if (Name == TEXT("FloatValue") || Name == TEXT("IntValue"))
		{
			return IsBlackboardValueRelevant(Cast<UBehaviorTree>(Context.Target->Root), static_cast<const UObject*>(Container), Name);
		}
		return true;
	}

	// 이 대상 안의 서브오브젝트(컴포넌트 템플릿)에 들어간다 — 우리 컴포넌트는 우리 속성, 엔진 이동 컴포넌트는 정해 둔 값만
	void WalkSubObject(const FWalkContext& Context, UObject* SubObject, const FString& Path, const FString& Label, int32 Depth)
	{
		if (!SubObject || Depth > 2 || !SubObject->IsIn(Context.Target->Root)) return;

		bool bAlreadyVisited = false;
		Context.VisitedSubObjects->Add(SubObject, &bAlreadyVisited);
		if (bAlreadyVisited) return;

		const UClass* Class = SubObject->GetClass();
		if (IsOursOrBlueprint(Class))
		{
			WalkFields(Context, Class, SubObject, Path + TEXT("."), Label, Depth + 1, EFieldPolicy::Ours);
		}
		else if (const TSet<FName>* Whitelist = EngineComponentWhitelist(Class))
		{
			WalkFields(Context, Class, SubObject, Path + TEXT("."), Label, Depth + 1, EFieldPolicy::Whitelist, Whitelist);
		}
	}

	void WalkFields(const FWalkContext& Context, const UStruct* Struct, const void* Container, const FString& PathPrefix,
		const FString& Label, int32 Depth, EFieldPolicy Policy, const TSet<FName>* Whitelist)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;

			// 컴포넌트 포인터는 엔진이 선언했고 보이기만 하는 속성이어도 따라간다 (안의 값은 편집 가능한 것만 싣는다)
			if (const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
			{
				if (Policy == EFieldPolicy::Ours && Property->ArrayDim == 1)
				{
					UObject* SubObject = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(Container));
					WalkSubObject(Context, SubObject, PathPrefix + DRTuning::GetPropertyAuthoredName(Property), Label, Depth);
				}
				continue;
			}

			if (!IsEditableField(Property, Context.Target->Kind == ETargetKind::LevelActor)) continue;

			bool bInclude = true;
			switch (Policy)
			{
			case EFieldPolicy::Ours:		bInclude = IsOurTopLevelProperty(Property); break;
			case EFieldPolicy::AINode:		bInclude = IsTunableAINodeField(Context, Container, Property); break;
			case EFieldPolicy::Whitelist:	bInclude = Whitelist && Whitelist->Contains(Property->GetFName()); break;
			default:						break;
			}
			if (!bInclude) continue;

			const FString Name = DRTuning::GetPropertyAuthoredName(Property);
			const FString Description = DescribeProperty(Property);
			const FString Range = DescribeRange(Property);
			for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
			{
				const FString Path = PathPrefix + Name + (Property->ArrayDim > 1 ? FString::Printf(TEXT("[%d]"), Index) : FString());
				WalkValue(Context, Property, Property->ContainerPtrToValuePtr<void>(Container, Index), Path, Label, Description, Range, Depth + 1);
			}
		}
	}

	// FScalableFloat — 실제 값은 .Value 하나. 커브를 쓰면 "커브에 곱하는 배율" 이라 설명에 밝힌다
	void AddScalableFloat(const FWalkContext& Context, const void* ScalablePtr, const FString& Path, const FString& Label, const FString& Description)
	{
		if (!ScalablePtr) return;
		const FScalableFloat* Scalable = static_cast<const FScalableFloat*>(ScalablePtr);
		const FProperty* ValueProp = FindFProperty<FProperty>(FScalableFloat::StaticStruct(), TEXT("Value"));
		if (!ValueProp) return;

		FString FullDescription = Description;
		if (Scalable->Curve.CurveTable)
		{
			FullDescription += FString::Printf(TEXT(" (커브 %s / %s 에 곱하는 배율)"),
				*Scalable->Curve.CurveTable->GetName(), *Scalable->Curve.RowName.ToString());
		}
		AddRow(Context, Path + TEXT(".Value"), ValueProp, ValueProp->ContainerPtrToValuePtr<void>(ScalablePtr), Label, FullDescription.TrimStartAndEnd(), FString());
	}

	const void* FieldPtr(const UStruct* Struct, const void* Container, const TCHAR* FieldName, const UScriptStruct** OutStruct = nullptr)
	{
		const FProperty* Field = FindFProperty<FProperty>(Struct, FieldName);
		if (!Field) return nullptr;
		if (OutStruct)
		{
			const FStructProperty* StructField = CastField<FStructProperty>(Field);
			*OutStruct = StructField ? StructField->Struct : nullptr;
		}
		return Field->ContainerPtrToValuePtr<void>(Container);
	}

	// GE 수치 — 계산 방식에 따라 실제로 쓰이는 칸만 싣는다 (SetByCaller 는 코드가 넣는 값이라 싣지 않는다)
	void AddMagnitude(const FWalkContext& Context, const void* MagnitudePtr, const FString& Path, const FString& Label, const FString& Description)
	{
		if (!MagnitudePtr) return;
		const FGameplayEffectModifierMagnitude* Magnitude = static_cast<const FGameplayEffectModifierMagnitude*>(MagnitudePtr);
		const UScriptStruct* MagnitudeStruct = FGameplayEffectModifierMagnitude::StaticStruct();

		switch (Magnitude->GetMagnitudeCalculationType())
		{
		case EGameplayEffectMagnitudeCalculation::ScalableFloat:
			AddScalableFloat(Context, FieldPtr(MagnitudeStruct, MagnitudePtr, TEXT("ScalableFloatMagnitude")),
				Path + TEXT(".ScalableFloatMagnitude"), Label, Description);
			break;

		case EGameplayEffectMagnitudeCalculation::AttributeBased:
		case EGameplayEffectMagnitudeCalculation::CustomCalculationClass:
			{
				const bool bAttribute = Magnitude->GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::AttributeBased;
				const TCHAR* Member = bAttribute ? TEXT("AttributeBasedMagnitude") : TEXT("CustomMagnitude");
				const UScriptStruct* InnerStruct = nullptr;
				const void* Inner = FieldPtr(MagnitudeStruct, MagnitudePtr, Member, &InnerStruct);
				if (!Inner || !InnerStruct) break;

				const FString Prefix = Path + TEXT(".") + Member;
				AddScalableFloat(Context, FieldPtr(InnerStruct, Inner, TEXT("Coefficient")), Prefix + TEXT(".Coefficient"), Label,
					Description + (bAttribute ? TEXT(" — 어트리뷰트에 곱하는 계수") : TEXT(" — 계산 결과에 곱하는 계수")));

				// 더하는 값은 0 이 아닐 때만 (보통 안 쓴다)
				for (const TCHAR* Additive : { TEXT("PreMultiplyAdditiveValue"), TEXT("PostMultiplyAdditiveValue") })
				{
					const void* AdditivePtr = FieldPtr(InnerStruct, Inner, Additive);
					if (AdditivePtr && !FMath::IsNearlyZero(static_cast<const FScalableFloat*>(AdditivePtr)->Value))
					{
						AddScalableFloat(Context, AdditivePtr, Prefix + TEXT(".") + Additive, Label, Description + TEXT(" — ") + Additive);
					}
				}
			}
			break;

		default:
			break;
		}
	}

	void WalkValue(const FWalkContext& Context, const FProperty* Property, const void* ValuePtr, const FString& Path,
		const FString& Label, const FString& Description, const FString& Range, int32 Depth)
	{
		if (Depth > 8) return;

		if (DRTuning::IsListableLeaf(Property))
		{
			AddRow(Context, Path, Property, ValuePtr, Label, Description, Range);
			return;
		}

		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			const UScriptStruct* Struct = StructProp->Struct;
			if (Struct == FScalableFloat::StaticStruct())
			{
				AddScalableFloat(Context, ValuePtr, Path, Label, Description);
			}
			else if (Struct == FGameplayEffectModifierMagnitude::StaticStruct())
			{
				AddMagnitude(Context, ValuePtr, Path, Label, Description);
			}
			else if (!IsSkippedStruct(Struct))
			{
				WalkFields(Context, Struct, ValuePtr, Path + TEXT("."), Label, Depth, EFieldPolicy::All);
			}
			return;
		}

		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProp, ValuePtr);
			const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner);
			const FProperty* KeyProperty = InnerStruct ? DRTuning::FindKeyProperty(InnerStruct->Struct) : nullptr;

			// 원소를 Id 로 부르려면 Id 가 비지 않고 겹치지 않아야 한다 — 아니면 칸 번호로
			TArray<FString> Keys;
			Keys.SetNum(Helper.Num());
			TMap<FString, int32> KeyCounts;
			for (int32 Index = 0; Index < Helper.Num() && KeyProperty; ++Index)
			{
				Keys[Index] = DRTuning::KeyToText(KeyProperty, KeyProperty->ContainerPtrToValuePtr<void>(Helper.GetRawPtr(Index)));
				KeyCounts.FindOrAdd(Keys[Index].ToLower())++;
			}

			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				const void* ElementPtr = Helper.GetRawPtr(Index);

				FString Key = FString::FromInt(Index);
				if (KeyProperty && !Keys[Index].IsEmpty() && !Keys[Index].Contains(TEXT("]")) && KeyCounts.FindRef(Keys[Index].ToLower()) == 1)
				{
					Key = Keys[Index];
				}

				const FString ElementLabel = InnerStruct ? JoinLabel(Label, DescribeElement(InnerStruct->Struct, ElementPtr)) : Label;
				WalkValue(Context, ArrayProp->Inner, ElementPtr, FString::Printf(TEXT("%s[%s]"), *Path, *Key), ElementLabel, Description, Range, Depth + 1);
			}
			return;
		}

		// 맵 — 열쇠가 이름 · 열거형 · 태그면 [열쇠] 로 부른다 (클래스별 정보 · 스테이지별 설정 등)
		if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper Helper(MapProp, ValuePtr);
			for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
			{
				if (!Helper.IsValidIndex(Index)) continue;
				const FString Key = DRTuning::KeyToText(MapProp->KeyProp, Helper.GetKeyPtr(Index));
				if (Key.IsEmpty() || Key.Contains(TEXT("]")) || Key.Contains(TEXT("("))) continue;

				WalkValue(Context, MapProp->ValueProp, Helper.GetValuePtr(Index), FString::Printf(TEXT("%s[%s]"), *Path, *Key),
					JoinLabel(Label, Key), Description, Range, Depth + 1);
			}
			return;
		}

		// 오브젝트 · 이름 · 글자 · 열거형은 목록에 싣지 않는다 (줄을 직접 적으면 바꿀 수는 있다)
	}

	void WalkGameplayEffect(const FWalkContext& Context, const UGameplayEffect* Effect)
	{
		const UClass* Class = Effect->GetClass();

		if (Effect->DurationPolicy == EGameplayEffectDurationType::HasDuration)
		{
			AddMagnitude(Context, FieldPtr(Class, Effect, TEXT("DurationMagnitude")), TEXT("DurationMagnitude"), FString(), TEXT("지속 시간(초)"));
		}

		if (Effect->DurationPolicy != EGameplayEffectDurationType::Instant && Effect->Period.Value > 0.f)
		{
			AddScalableFloat(Context, FieldPtr(Class, Effect, TEXT("Period")), TEXT("Period"), FString(), TEXT("주기(초) — 이 간격마다 효과가 다시 들어간다"));
		}

		const UEnum* ModOpEnum = StaticEnum<EGameplayModOp::Type>();
		for (int32 Index = 0; Index < Effect->Modifiers.Num(); ++Index)
		{
			const FGameplayModifierInfo& Modifier = Effect->Modifiers[Index];
			const FString OpName = ModOpEnum ? EnumDisplay(ModOpEnum, Modifier.ModifierOp) : FString();
			const FString Label = FString::Printf(TEXT("[%s %s]"), *Modifier.Attribute.GetName(), *OpName);
			AddMagnitude(Context, &Modifier.ModifierMagnitude, FString::Printf(TEXT("Modifiers[%d].ModifierMagnitude"), Index), Label, TEXT("수치"));
		}

		if (Effect->StackingType != EGameplayEffectStackingType::None)
		{
			if (const FProperty* StackProp = FindFProperty<FProperty>(Class, TEXT("StackLimitCount")))
			{
				AddRow(Context, TEXT("StackLimitCount"), StackProp, StackProp->ContainerPtrToValuePtr<void>(Effect), FString(), TEXT("최대 중첩 수"), FString());
			}
		}

		// GE 를 상속한 우리 C++ 클래스 · BP 변수
		WalkFields(Context, Class, Effect, FString(), FString(), 0, EFieldPolicy::Ours);
	}

	void WalkDataTable(const FWalkContext& Context, const UDataTable* Table)
	{
		const UScriptStruct* RowStruct = Table->GetRowStruct();
		if (!RowStruct) return;

		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			const FString RowName = Pair.Key.ToString();
			WalkFields(Context, RowStruct, Pair.Value, FString::Printf(TEXT("[%s]."), *RowName), FString::Printf(TEXT("[%s]"), *RowName), 0, EFieldPolicy::All);
		}
	}

	void WalkCurveTable(const FWalkContext& Context, const UCurveTable* Curves)
	{
		const UScriptStruct* CurveStruct = Curves->GetCurveTableMode() == ECurveTableMode::SimpleCurves
			? FSimpleCurve::StaticStruct()
			: FRichCurve::StaticStruct();
		const FArrayProperty* KeysProp = FindFProperty<FArrayProperty>(CurveStruct, TEXT("Keys"));
		const FStructProperty* KeyStruct = KeysProp ? CastField<FStructProperty>(KeysProp->Inner) : nullptr;
		const FProperty* TimeProp = KeyStruct ? FindFProperty<FProperty>(KeyStruct->Struct, TEXT("Time")) : nullptr;
		const FProperty* ValueProp = KeyStruct ? FindFProperty<FProperty>(KeyStruct->Struct, TEXT("Value")) : nullptr;
		if (!TimeProp || !ValueProp) return;

		for (const TPair<FName, FRealCurve*>& Pair : Curves->GetRowMap())
		{
			if (!Pair.Value) continue;

			const FString RowName = Pair.Key.ToString();
			FScriptArrayHelper Helper(KeysProp, KeysProp->ContainerPtrToValuePtr<void>(Pair.Value));
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				const void* KeyPtr = Helper.GetRawPtr(Index);
				const float Time = *TimeProp->ContainerPtrToValuePtr<float>(KeyPtr);
				AddRow(Context, FString::Printf(TEXT("[%s].Keys[%d].Value"), *RowName, Index), ValueProp, ValueProp->ContainerPtrToValuePtr<void>(KeyPtr),
					FString::Printf(TEXT("[%s]"), *RowName), FString::Printf(TEXT("레벨 %s 의 값"), *FormatNumber(Time)), FString());
			}
		}
	}

	// 비헤이비어 트리 — 노드(조건 · 작업 · 서비스)마다 수치. 노드 요약("Blackboard: Distance is Less Than 500")을 설명 앞에 붙인다
	void WalkBehaviorTree(const FWalkContext& Context, const UBehaviorTree* Tree)
	{
		TArray<UObject*> SubObjects;
		GetObjectsWithOuter(Tree, SubObjects, /*bIncludeNestedObjects*/ false);
		SubObjects.Sort([](const UObject& A, const UObject& B) { return A.GetName() < B.GetName(); });

		for (UObject* Object : SubObjects)
		{
			UBTNode* Node = Cast<UBTNode>(Object);
			if (!Node) continue;

			// 블랙보드 키가 풀려야 요약이 "MoveTo: invalid" 가 아니라 "MoveTo: TargetToFollow" 로 나온다 (트랜지언트 값만 바뀐다)
			Node->InitializeFromAsset(*const_cast<UBehaviorTree*>(Tree));

			// 요약은 여러 줄이다 ("( aborts self )" + "Blackboard: Distance is Less Than 500") — 한 줄로 잇는다
			TArray<FString> Lines;
			Node->GetStaticDescription().ParseIntoArrayLines(Lines, /*bCullEmpty*/ true);
			FString Summary = FString::Join(Lines, TEXT(" · ")).TrimStartAndEnd();
			if (Summary.Len() > 120)
			{
				Summary = Summary.Left(118) + TEXT("…");
			}
			if (Summary.IsEmpty())
			{
				Summary = Node->GetNodeName();
			}
			WalkFields(Context, Node->GetClass(), Node, Node->GetName() + TEXT("."), FString::Printf(TEXT("[%s]"), *Summary), 0, EFieldPolicy::AINode);
		}
	}

	bool IsExcludedPath(const FDRTuningCatalogSettings& Settings, const FString& Path)
	{
		for (const FString& Excluded : Settings.ExcludedPathContains)
		{
			if (!Excluded.IsEmpty() && Path.Contains(Excluded)) return true;
		}
		return false;
	}

	void AddTarget(const FDRTuningCatalogSettings& Settings, UObject* Root, const FString& ShortName, const FString& LoadPath, TArray<FCatalogTarget>& OutTargets)
	{
		FCatalogTarget Target;
		Target.Root = Root;
		Target.ShortName = ShortName;
		Target.LoadPath = LoadPath;
		Target.Kind = KindOf(Root);
		ApplyGroupRule(Settings, Target);
		OutTargets.Add(MoveTemp(Target));
	}

	void CollectTargets(const FDRTuningCatalogSettings& Settings, const DRTuningCatalog::FPackageFilter& PackageFilter, TArray<FCatalogTarget>& OutTargets)
	{
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		if (!FPlatformProperties::RequiresCookedData())
		{
			// 쿠킹 전 에셋으로 도는 -game 은 에디터와 달리 /Game 을 전부 훑어 두지 않는다 — 목록이 비어 나온다
			AssetRegistry.ScanPathsSynchronous({ TEXT("/Game/") }, /*bForceRescan*/ false);
		}
		AssetRegistry.WaitForCompletion();

		TSet<FString> Seen;

		// 빌드에 안 들어가는 에셋은 불러오지도 않는다 (방치된 고장 난 BP 의 컴파일 오류가 쿡을 실패시킨다)
		auto IsWanted = [&](const FName PackageName, bool bRequireReferencer) -> bool
		{
			const FString PackageString = PackageName.ToString();
			if (IsExcludedPath(Settings, PackageString)) return false;
			if (PackageFilter) return PackageFilter(PackageName);
			if (bRequireReferencer && !FPlatformProperties::RequiresCookedData())
			{
				TArray<FName> Referencers;
				AssetRegistry.GetReferencers(PackageName, Referencers);
				return Referencers.Num() > 0;
			}
			return true;
		};

		// ── 블루프린트: 네이티브 부모가 BlueprintBases 중 하나를 상속한 것 (CDO 값) ──
		TArray<const UClass*> Bases;
		for (const FSoftClassPath& BasePath : Settings.BlueprintBases)
		{
			if (const UClass* Base = BasePath.TryLoadClass<UObject>()) Bases.Add(Base);
			else UE_LOG(LogDR, Warning, TEXT("[Tuning] 목록 설정의 클래스를 찾지 못했습니다: %s"), *BasePath.ToString());
		}

		if (Bases.Num() > 0)
		{
			FARFilter Filter;
			Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
			Filter.bRecursiveClasses = true;
			Filter.PackagePaths.Add(TEXT("/Game"));
			Filter.bRecursivePaths = true;

			TArray<FAssetData> Blueprints;
			AssetRegistry.GetAssets(Filter, Blueprints);
			Blueprints.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

			for (const FAssetData& Asset : Blueprints)
			{
				// 네이티브 부모를 태그로 먼저 거른다 — 블루프린트를 전부 불러오지 않으려고
				FString NativeParentPath;
				if (!Asset.GetTagValue(FBlueprintTags::NativeParentClassPath, NativeParentPath)) continue;

				const UClass* NativeParent = FindObject<UClass>(nullptr, *FPackageName::ExportTextPathToObjectPath(NativeParentPath));
				if (!NativeParent) continue;
				if (!Bases.ContainsByPredicate([NativeParent](const UClass* Base) { return NativeParent->IsChildOf(Base); })) continue;
				if (!IsWanted(Asset.PackageName, /*bRequireReferencer*/ true)) continue;

				FString LoadPath, Error;
				UObject* Root = DRTuning::LoadTargetObject(Asset.PackageName.ToString() + TEXT(".") + Asset.AssetName.ToString() + TEXT("_C"), LoadPath, Error);
				if (!Root || Seen.Contains(LoadPath)) continue;
				if (Root->GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;

				Seen.Add(LoadPath);
				AddTarget(Settings, Root, Asset.AssetName.ToString(), LoadPath, OutTargets);
			}
		}

		// ── 에셋 종류: 데이터 에셋 · 데이터 테이블 · 커브 테이블 · 비헤이비어 트리 전부 ──
		TArray<FSoftObjectPath> AssetPaths = Settings.Assets;
		for (const FSoftClassPath& ClassPath : Settings.AssetClasses)
		{
			const UClass* AssetClass = ClassPath.TryLoadClass<UObject>();
			if (!AssetClass)
			{
				UE_LOG(LogDR, Warning, TEXT("[Tuning] 목록 설정의 에셋 종류를 찾지 못했습니다: %s"), *ClassPath.ToString());
				continue;
			}

			FARFilter Filter;
			Filter.ClassPaths.Add(AssetClass->GetClassPathName());
			Filter.bRecursiveClasses = true;
			Filter.PackagePaths.Add(TEXT("/Game"));
			Filter.bRecursivePaths = true;

			TArray<FAssetData> Assets;
			AssetRegistry.GetAssets(Filter, Assets);
			Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
			for (const FAssetData& Asset : Assets)
			{
				AssetPaths.Add(Asset.GetSoftObjectPath());
			}
		}

		for (const FSoftObjectPath& AssetPath : AssetPaths)
		{
			// ini · 에셋 매니저로만 쓰이는 데이터 에셋도 있어 참조자는 따지지 않는다
			if (!IsWanted(FName(*AssetPath.GetLongPackageName()), /*bRequireReferencer*/ false)) continue;

			FString LoadPath, Error;
			UObject* Root = DRTuning::LoadTargetObject(AssetPath.ToString(), LoadPath, Error);
			if (!Root)
			{
				UE_LOG(LogDR, Warning, TEXT("[Tuning] 목록에 넣을 에셋을 찾지 못했습니다: %s"), *AssetPath.ToString());
				continue;
			}
			if (Seen.Contains(LoadPath)) continue;

			Seen.Add(LoadPath);
			AddTarget(Settings, Root, FPackageName::GetShortName(AssetPath.GetLongPackageName()), LoadPath, OutTargets);
		}

		// ── 맵 배치 액터: 그 액터만 따로 정한 값 (클래스 기본값과 다른 것) ──
		for (const FString& MapEntry : Settings.Maps)
		{
			const FString PackageName = FPackageName::ObjectPathToPackageName(MapEntry);
			if (!IsWanted(FName(*PackageName), /*bRequireReferencer*/ false)) continue;
			if (!FPackageName::DoesPackageExist(PackageName))
			{
				UE_LOG(LogDR, Warning, TEXT("[Tuning] 목록 설정의 맵이 없습니다: %s"), *PackageName);
				continue;
			}

			UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
			UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
			if (!World || !World->PersistentLevel) continue;

			for (AActor* Actor : World->PersistentLevel->Actors)
			{
				if (!IsValid(Actor) || Actor->IsTemplate()) continue;
				const UClass* Class = Actor->GetClass();
				// 엔진 액터(조명 · 브러시 · 타깃 포인트)는 빼고 우리 C++ · BP 액터만
				if (!IsOursOrBlueprint(Class) || IsExcludedPath(Settings, Class->GetPathName())) continue;

				FCatalogTarget Target;
				Target.Root = Actor;
				Target.CompareTo = Class->GetDefaultObject();
				Target.ShortName = Actor->GetName();
				Target.LoadPath = Actor->GetPathName();
				Target.Kind = ETargetKind::LevelActor;
#if WITH_EDITOR
				Target.LabelPrefix = FString::Printf(TEXT("[%s]"), *Actor->GetActorLabel());
#endif
				ApplyGroupRule(Settings, Target);
				OutTargets.Add(MoveTemp(Target));
			}
		}
	}
}

void DRTuningCatalog::BuildLiveCatalog(const FDRTuningCatalogSettings& Settings, TArray<FDRTuningRow>& OutRows,
	const FOriginalLookup& OriginalLookup, const FPackageFilter& PackageFilter)
{
	OutRows.Reset();

	TArray<FCatalogTarget> Targets;
	CollectTargets(Settings, PackageFilter, Targets);

	for (const FCatalogTarget& Target : Targets)
	{
		TSet<const UObject*> VisitedSubObjects;
		FWalkContext Context;
		Context.Target = &Target;
		Context.OutRows = &OutRows;
		Context.OriginalLookup = &OriginalLookup;
		Context.VisitedSubObjects = &VisitedSubObjects;

		if (const UDataTable* Table = Cast<UDataTable>(Target.Root))
		{
			WalkDataTable(Context, Table);
		}
		else if (const UCurveTable* Curves = Cast<UCurveTable>(Target.Root))
		{
			WalkCurveTable(Context, Curves);
		}
		else if (const UGameplayEffect* Effect = Cast<UGameplayEffect>(Target.Root))
		{
			WalkGameplayEffect(Context, Effect);
		}
		else if (const UBehaviorTree* Tree = Cast<UBehaviorTree>(Target.Root))
		{
			WalkBehaviorTree(Context, Tree);
		}
		else
		{
			WalkFields(Context, Target.Root->GetClass(), Target.Root, FString(), Target.LabelPrefix, 0, EFieldPolicy::Ours);
		}
	}

	// 분류 규칙 순서 → 종류(스킬 · 액터 · 효과 · 데이터 · 배치) → 대상 이름. 같은 대상 안은 선언 순서 그대로
	OutRows.StableSort([](const FDRTuningRow& A, const FDRTuningRow& B)
	{
		if (A.GroupOrder != B.GroupOrder) return A.GroupOrder < B.GroupOrder;
		if (A.KindOrder != B.KindOrder) return A.KindOrder < B.KindOrder;
		return A.Target.Compare(B.Target, ESearchCase::IgnoreCase) < 0;
	});
}
