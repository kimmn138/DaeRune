// Copyright DaeRune

#include "Tuning/DRTuningProperty.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Engine/DataTable.h"
#include "Engine/CurveTable.h"
#include "Curves/RichCurve.h"
#include "Curves/SimpleCurve.h"
#include "GameplayTagContainer.h"
#if WITH_EDITORONLY_DATA
#include "Engine/Blueprint.h"
#endif

namespace
{
	/** 경로 한 칸 — 이름 + 대괄호들. 이름이 비면 표의 줄 고르기([RowName]) */
	struct FPathSegment
	{
		FString Name;
		TArray<FString> Keys;
	};

	bool ParsePath(const FString& Path, TArray<FPathSegment>& OutSegments, FString& OutError)
	{
		OutSegments.Reset();

		FPathSegment Current;
		FString NameToken;
		FString KeyToken;
		bool bInBracket = false;

		auto FinishSegment = [&]() -> bool
		{
			Current.Name = NameToken.TrimStartAndEnd();
			NameToken.Reset();
			if (Current.Name.IsEmpty() && Current.Keys.Num() == 0)
			{
				OutError = FString::Printf(TEXT("속성 경로에 빈 칸이 있습니다: '%s'"), *Path);
				return false;
			}
			OutSegments.Add(MoveTemp(Current));
			Current = FPathSegment();
			return true;
		};

		for (const TCHAR Ch : Path)
		{
			if (bInBracket)
			{
				if (Ch == TEXT(']'))
				{
					Current.Keys.Add(KeyToken.TrimStartAndEnd());
					KeyToken.Reset();
					bInBracket = false;
				}
				else
				{
					KeyToken.AppendChar(Ch);
				}
				continue;
			}

			if (Ch == TEXT('['))
			{
				bInBracket = true;
				continue;
			}
			if (Ch == TEXT('.'))
			{
				if (!FinishSegment()) return false;
				continue;
			}
			if (Current.Keys.Num() > 0 && !FChar::IsWhitespace(Ch))
			{
				OutError = FString::Printf(TEXT("대괄호 ] 뒤에는 점(.)이나 [ 가 와야 합니다: '%s'"), *Path);
				return false;
			}
			NameToken.AppendChar(Ch);
		}

		if (bInBracket)
		{
			OutError = FString::Printf(TEXT("대괄호가 닫히지 않았습니다: '%s'"), *Path);
			return false;
		}
		return FinishSegment();
	}

	bool ParseIndex(const FString& Key, int32& OutIndex)
	{
		if (Key.IsEmpty() || Key.Len() > 9) return false;
		for (const TCHAR Ch : Key)
		{
			if (!FChar::IsDigit(Ch)) return false;
		}
		OutIndex = FCString::Atoi(*Key);
		return true;
	}

	FProperty* FindPropertyByPathName(const UStruct* Struct, const FString& Name)
	{
		if (!Struct || Name.IsEmpty()) return nullptr;

		// FName 비교는 대소문자를 가리지 않는다. 처음 보는 이름을 네임 테이블에 넣지 않으려고 FNAME_Find
		const FName AsName(*Name, FNAME_Find);
		if (!AsName.IsNone())
		{
			if (FProperty* Found = FindFProperty<FProperty>(Struct, AsName))
			{
				return Found;
			}
		}

		// 블루프린트 구조체는 내부 이름이 Name_2_ABCD… 라서 작성자 이름으로 한 번 더
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (DRTuning::GetPropertyAuthoredName(*It).Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	FString EnumShortName(const UEnum* Enum, int64 Value)
	{
		return Enum ? Enum->GetNameStringByValue(Value) : FString::Printf(TEXT("%lld"), Value);
	}

	bool ParseEnumValue(const UEnum* Enum, const FString& Text, int64& OutValue)
	{
		const int32 Count = Enum->ContainsExistingMax() ? Enum->NumEnums() - 1 : Enum->NumEnums();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Enum->GetNameStringByIndex(Index).Equals(Text, ESearchCase::IgnoreCase)
				|| Enum->GetNameByIndex(Index).ToString().Equals(Text, ESearchCase::IgnoreCase)
				|| Enum->GetDisplayNameTextByIndex(Index).ToString().Equals(Text, ESearchCase::IgnoreCase))
			{
				OutValue = Enum->GetValueByIndex(Index);
				return true;
			}
		}

		int32 Number = 0;
		if (ParseIndex(Text, Number) && Enum->IsValidEnumValue(Number))
		{
			OutValue = Number;
			return true;
		}
		return false;
	}

	FString ListEnumNames(const UEnum* Enum)
	{
		TArray<FString> Names;
		const int32 Count = Enum->ContainsExistingMax() ? Enum->NumEnums() - 1 : Enum->NumEnums();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Names.Add(Enum->GetNameStringByIndex(Index));
		}
		return FString::Join(Names, TEXT(" / "));
	}

	bool ParseBool(const FString& Text, bool& bOut)
	{
		static const TCHAR* TrueWords[] = { TEXT("true"), TEXT("1"), TEXT("yes"), TEXT("on"), TEXT("참"), TEXT("예"), TEXT("켬"), TEXT("o") };
		static const TCHAR* FalseWords[] = { TEXT("false"), TEXT("0"), TEXT("no"), TEXT("off"), TEXT("거짓"), TEXT("아니오"), TEXT("끔"), TEXT("x") };
		for (const TCHAR* Word : TrueWords)
		{
			if (Text.Equals(Word, ESearchCase::IgnoreCase)) { bOut = true; return true; }
		}
		for (const TCHAR* Word : FalseWords)
		{
			if (Text.Equals(Word, ESearchCase::IgnoreCase)) { bOut = false; return true; }
		}
		return false;
	}

	// [+-] 숫자 [. 숫자] [e [+-] 숫자] — 소수점 앞뒤 중 한쪽에는 숫자가 있어야 한다
	bool IsNumberText(const FString& Text)
	{
		int32 Pos = 0;
		const int32 Len = Text.Len();
		if (Pos < Len && (Text[Pos] == TEXT('+') || Text[Pos] == TEXT('-'))) ++Pos;

		int32 Digits = 0;
		while (Pos < Len && FChar::IsDigit(Text[Pos])) { ++Pos; ++Digits; }
		if (Pos < Len && Text[Pos] == TEXT('.'))
		{
			++Pos;
			while (Pos < Len && FChar::IsDigit(Text[Pos])) { ++Pos; ++Digits; }
		}
		if (Digits == 0) return false;

		if (Pos < Len && (Text[Pos] == TEXT('e') || Text[Pos] == TEXT('E')))
		{
			++Pos;
			if (Pos < Len && (Text[Pos] == TEXT('+') || Text[Pos] == TEXT('-'))) ++Pos;
			int32 ExpDigits = 0;
			while (Pos < Len && FChar::IsDigit(Text[Pos])) { ++Pos; ++ExpDigits; }
			if (ExpDigits == 0) return false;
		}
		return Pos == Len;
	}

	// 엑셀 천 단위 서식 "1,200" / "-12,000.5" 만 쉼표를 허용한다 — "0,5"(소수점 쉼표)는 1/10 로 잘못 읽힐 수 있어 거절
	bool IsThousandsGrouped(const FString& Text)
	{
		FString IntegerPart = Text;
		int32 DotIndex = INDEX_NONE;
		if (Text.FindChar(TEXT('.'), DotIndex))
		{
			IntegerPart = Text.Left(DotIndex);
		}
		if (IntegerPart.StartsWith(TEXT("-")) || IntegerPart.StartsWith(TEXT("+")))
		{
			IntegerPart.RightChopInline(1);
		}

		TArray<FString> Groups;
		IntegerPart.ParseIntoArray(Groups, TEXT(","), false);
		if (Groups.Num() < 2 || Groups[0].IsEmpty() || Groups[0].Len() > 3) return false;
		for (int32 Index = 1; Index < Groups.Num(); ++Index)
		{
			if (Groups[Index].Len() != 3) return false;
		}
		return true;
	}

	bool ParseNumber(const FString& InText, double& OutNumber, FString& OutError)
	{
		FString Text = InText.TrimStartAndEnd();
		Text.ReplaceInline(TEXT(" "), TEXT(""));

		bool bPercent = false;
		if (Text.EndsWith(TEXT("%")))
		{
			bPercent = true;
			Text.LeftChopInline(1);
		}
		if (Text.Contains(TEXT(",")))
		{
			if (!IsThousandsGrouped(Text))
			{
				OutError = FString::Printf(TEXT("'%s' — 쉼표는 천 단위(1,200)에만 씁니다. 소수점은 마침표(0.5)입니다"), *InText);
				return false;
			}
			Text.ReplaceInline(TEXT(","), TEXT(""));
		}
		if (!IsNumberText(Text))
		{
			OutError = FString::Printf(TEXT("'%s' 는 숫자가 아닙니다"), *InText);
			return false;
		}

		OutNumber = FCString::Atod(*Text);
		if (bPercent)
		{
			OutNumber /= 100.0;
		}
		if (!FMath::IsFinite(OutNumber))
		{
			OutError = FString::Printf(TEXT("'%s' 는 너무 큰 숫자입니다"), *InText);
			return false;
		}
		return true;
	}

	void GetIntegerRange(const FNumericProperty* Property, int64& OutMin, int64& OutMax)
	{
		const bool bUnsigned = Property->IsA<FByteProperty>() || Property->IsA<FUInt16Property>()
			|| Property->IsA<FUInt32Property>() || Property->IsA<FUInt64Property>();
		const int32 Bits = Property->GetElementSize() * 8;
		if (Bits >= 64)
		{
			OutMin = bUnsigned ? 0 : MIN_int64;
			OutMax = MAX_int64;
			return;
		}
		if (bUnsigned)
		{
			OutMin = 0;
			OutMax = (int64(1) << Bits) - 1;
		}
		else
		{
			OutMin = -(int64(1) << (Bits - 1));
			OutMax = (int64(1) << (Bits - 1)) - 1;
		}
	}

	FString KeyToString(const FProperty* KeyProperty, const void* KeyPtr)
	{
		if (const FNameProperty* NameProp = CastField<FNameProperty>(KeyProperty))
		{
			return NameProp->GetPropertyValue(KeyPtr).ToString();
		}
		if (const FStrProperty* StrProp = CastField<FStrProperty>(KeyProperty))
		{
			return StrProp->GetPropertyValue(KeyPtr);
		}
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(KeyProperty))
		{
			return EnumShortName(EnumProp->GetEnum(), EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(KeyPtr));
		}
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(KeyProperty); ByteProp && ByteProp->Enum)
		{
			return EnumShortName(ByteProp->Enum, ByteProp->GetSignedIntPropertyValue(KeyPtr));
		}
		if (const FStructProperty* StructProp = CastField<FStructProperty>(KeyProperty);
			StructProp && StructProp->Struct == FGameplayTag::StaticStruct())
		{
			return static_cast<const FGameplayTag*>(KeyPtr)->ToString();
		}

		FString Text;
		KeyProperty->ExportText_Direct(Text, KeyPtr, KeyPtr, nullptr, PPF_None);
		return Text;
	}

	FString FormatNumber(double Value)
	{
		// float 에서 온 값이라 유효숫자 7자리면 충분하다 (0.2f → "0.2", 25 → "25")
		if (Value == 0.0) return TEXT("0");
		return FString::Printf(TEXT("%.7g"), Value);
	}
}

bool DRTuning::FTypedValue::operator==(const FTypedValue& Other) const
{
	if (Kind != Other.Kind) return false;
	switch (Kind)
	{
	case EKind::Float:	return Float == Other.Float;
	case EKind::Int:	return Int == Other.Int;
	case EKind::Bool:	return bBool == Other.bBool;
	case EKind::Text:	return Text.Equals(Other.Text, ESearchCase::CaseSensitive);
	default:			return true;
	}
}

FString DRTuning::GetPropertyAuthoredName(const FProperty* Property)
{
	if (!Property) return FString();
	if (const UStruct* Owner = Property->GetOwnerStruct())
	{
		return Owner->GetAuthoredNameForField(Property);
	}
	return Property->GetName();
}

FString DRTuning::KeyToText(const FProperty* KeyProperty, const void* KeyPtr)
{
	return (KeyProperty && KeyPtr) ? KeyToString(KeyProperty, KeyPtr) : FString();
}

bool DRTuning::ParseLevelActorPath(const FString& ObjectPath, FName& OutMapPackage, FName& OutActorName)
{
	// /Game/Maps/Stage1.Stage1:PersistentLevel.BP_Door_C_1
	FString Outer, ActorName;
	if (!ObjectPath.Split(TEXT(":PersistentLevel."), &Outer, &ActorName) || ActorName.IsEmpty() || ActorName.Contains(TEXT(".")))
	{
		return false;
	}
	OutMapPackage = FName(*FPackageName::ObjectPathToPackageName(Outer));
	OutActorName = FName(*ActorName);
	return !OutMapPackage.IsNone();
}

FProperty* DRTuning::FindKeyProperty(const UScriptStruct* Struct)
{
	if (!Struct) return nullptr;
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		if (!It->IsA<FNameProperty>() && !It->IsA<FStrProperty>()) continue;
		if (GetPropertyAuthoredName(*It).EndsWith(TEXT("Id"), ESearchCase::CaseSensitive))
		{
			return *It;
		}
	}
	return nullptr;
}

bool DRTuning::ResolvePath(UObject* Root, const FString& Path, FResolvedValue& Out, FString& OutError)
{
	Out = FResolvedValue();
	if (!Root)
	{
		OutError = TEXT("대상이 없습니다");
		return false;
	}

	TArray<FPathSegment> Segments;
	if (!ParsePath(Path, Segments, OutError)) return false;

	void* Container = Root;
	const UStruct* Struct = Root->GetClass();
	UObject* Owner = Root;
	int32 SegmentIndex = 0;

	// ── 표는 첫 칸이 줄 이름이다 ([RowName]) ──
	const bool bDataTable = Root->IsA<UDataTable>();
	const bool bCurveTable = Root->IsA<UCurveTable>();
	if (bDataTable || bCurveTable)
	{
		const FPathSegment& First = Segments[0];
		if (!First.Name.IsEmpty() || First.Keys.Num() != 1 || Segments.Num() < 2)
		{
			OutError = FString::Printf(TEXT("표는 [줄 이름].필드 형식으로 적습니다: '%s'"), *Path);
			return false;
		}

		const FName RowName(*First.Keys[0]);
		if (const UDataTable* Table = Cast<UDataTable>(Root))
		{
			Container = Table->FindRowUnchecked(RowName);
			Struct = Table->GetRowStruct();
		}
		else
		{
			const UCurveTable* Curves = CastChecked<UCurveTable>(Root);
			Container = Curves->FindCurveUnchecked(RowName);
			Struct = Curves->GetCurveTableMode() == ECurveTableMode::SimpleCurves
				? FSimpleCurve::StaticStruct()
				: FRichCurve::StaticStruct();
		}
		if (!Container || !Struct)
		{
			OutError = FString::Printf(TEXT("'%s' 줄이 %s 에 없습니다"), *First.Keys[0], *Root->GetName());
			return false;
		}
		SegmentIndex = 1;
	}

	FProperty* Property = nullptr;
	void* ValuePtr = nullptr;

	for (; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		const FPathSegment& Segment = Segments[SegmentIndex];
		if (Segment.Name.IsEmpty())
		{
			OutError = FString::Printf(TEXT("[줄 이름] 은 표의 맨 앞에서만 씁니다: '%s'"), *Path);
			return false;
		}

		Property = FindPropertyByPathName(Struct, Segment.Name);
		if (!Property && Container == Owner && Segment.Keys.Num() == 0 && SegmentIndex < Segments.Num() - 1)
		{
			// 속성이 아니면 그 이름의 서브오브젝트 — 비헤이비어 트리 노드(BTTask_MoveTo_3)처럼 속성으로 이어지지 않은 것
			const FName SubObjectName(*Segment.Name, FNAME_Find);
			UObject* SubObject = SubObjectName.IsNone() ? nullptr : StaticFindObjectFast(UObject::StaticClass(), Owner, SubObjectName);
			if (SubObject)
			{
				Container = SubObject;
				Struct = SubObject->GetClass();
				Owner = SubObject;
				continue;
			}
		}
		if (!Property)
		{
			OutError = FString::Printf(TEXT("'%s' 속성이 %s 에 없습니다"), *Segment.Name, *Struct->GetName());
			return false;
		}

		// 고정 배열(int32 X[3])은 첫 대괄호가 칸 번호
		int32 KeyIndex = 0;
		if (Property->ArrayDim > 1)
		{
			int32 Index = INDEX_NONE;
			if (Segment.Keys.Num() == 0 || !ParseIndex(Segment.Keys[0], Index) || Index >= Property->ArrayDim)
			{
				OutError = FString::Printf(TEXT("'%s' 는 칸이 %d 개인 고정 배열이라 [0]~[%d] 로 고릅니다"), *Segment.Name, Property->ArrayDim, Property->ArrayDim - 1);
				return false;
			}
			ValuePtr = Property->ContainerPtrToValuePtr<void>(Container, Index);
			KeyIndex = 1;
		}
		else
		{
			ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
		}

		for (; KeyIndex < Segment.Keys.Num(); ++KeyIndex)
		{
			const FString& Key = Segment.Keys[KeyIndex];

			if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				FScriptArrayHelper Helper(ArrayProp, ValuePtr);
				int32 Index = INDEX_NONE;
				if (!ParseIndex(Key, Index))
				{
					// 칸 번호가 아니면 원소의 …Id 로 찾는다 (칩 · 스테이지 · 업적)
					const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner);
					const FProperty* KeyProperty = InnerStruct ? FindKeyProperty(InnerStruct->Struct) : nullptr;
					if (!KeyProperty)
					{
						OutError = FString::Printf(TEXT("'%s' 는 칸 번호([0], [1] …)로만 고를 수 있습니다"), *Segment.Name);
						return false;
					}
					for (int32 Element = 0; Element < Helper.Num(); ++Element)
					{
						const void* KeyPtr = KeyProperty->ContainerPtrToValuePtr<void>(Helper.GetRawPtr(Element));
						if (KeyToString(KeyProperty, KeyPtr).Equals(Key, ESearchCase::IgnoreCase))
						{
							Index = Element;
							break;
						}
					}
					if (Index == INDEX_NONE)
					{
						OutError = FString::Printf(TEXT("%s 에 '%s' 가 없습니다"), *Segment.Name, *Key);
						return false;
					}
				}
				if (!Helper.IsValidIndex(Index))
				{
					OutError = FString::Printf(TEXT("%s[%d] — 칸이 %d 개뿐입니다"), *Segment.Name, Index, Helper.Num());
					return false;
				}
				ValuePtr = Helper.GetRawPtr(Index);
				Property = ArrayProp->Inner;
			}
			else if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
			{
				FScriptMapHelper Helper(MapProp, ValuePtr);
				void* Found = nullptr;
				for (int32 Element = 0; Element < Helper.GetMaxIndex(); ++Element)
				{
					if (!Helper.IsValidIndex(Element)) continue;
					if (KeyToString(MapProp->KeyProp, Helper.GetKeyPtr(Element)).Equals(Key, ESearchCase::IgnoreCase))
					{
						Found = Helper.GetValuePtr(Element);
						break;
					}
				}
				if (!Found)
				{
					OutError = FString::Printf(TEXT("%s 에 '%s' 열쇠가 없습니다"), *Segment.Name, *Key);
					return false;
				}
				ValuePtr = Found;
				Property = MapProp->ValueProp;
			}
			else
			{
				OutError = FString::Printf(TEXT("'%s' 는 배열 · 맵이 아니라 [ ] 를 쓸 수 없습니다"), *Segment.Name);
				return false;
			}
		}

		if (SegmentIndex == Segments.Num() - 1) break;

		// ── 다음 칸으로 내려간다 ──
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			Container = ValuePtr;
			Struct = StructProp->Struct;
		}
		else if (const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
		{
			// 다른 에셋을 따라가면 엉뚱한 에셋을 덮어쓰게 된다 — 이 대상 안의 서브오브젝트(컴포넌트 템플릿)만
			UObject* SubObject = ObjectProp->GetObjectPropertyValue(ValuePtr);
			if (!SubObject || !SubObject->IsIn(Root))
			{
				OutError = FString::Printf(TEXT("'%s' 는 다른 에셋을 가리킵니다 — 그 에셋을 [대상] 으로 적으세요"), *Segment.Name);
				return false;
			}
			Container = SubObject;
			Struct = SubObject->GetClass();
			Owner = SubObject;
		}
		else
		{
			OutError = FString::Printf(TEXT("'%s' 는 구조체가 아니라 그 아래(.%s)로 내려갈 수 없습니다"), *Segment.Name, *Segments[SegmentIndex + 1].Name);
			return false;
		}
	}

	Out.Property = Property;
	Out.ValuePtr = ValuePtr;
	Out.OwnerObject = Owner;
	return Out.IsValid();
}

FString DRTuning::ValueToText(const FResolvedValue& Value)
{
	if (!Value.IsValid()) return FString();

	const FProperty* Property = Value.Property;
	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return BoolProp->GetPropertyValue(Value.ValuePtr) ? TEXT("true") : TEXT("false");
	}
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		return EnumShortName(EnumProp->GetEnum(), EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value.ValuePtr));
	}
	if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (const UEnum* Enum = NumProp->GetIntPropertyEnum())
		{
			return EnumShortName(Enum, NumProp->GetSignedIntPropertyValue(Value.ValuePtr));
		}
		if (NumProp->IsFloatingPoint())
		{
			return FormatNumber(NumProp->GetFloatingPointPropertyValue(Value.ValuePtr));
		}
		return FString::Printf(TEXT("%lld"), NumProp->GetSignedIntPropertyValue(Value.ValuePtr));
	}

	FString Text;
	Property->ExportText_Direct(Text, Value.ValuePtr, Value.ValuePtr, Value.OwnerObject, PPF_None);
	return Text;
}

DRTuning::FTypedValue DRTuning::ReadValue(const FResolvedValue& Value)
{
	FTypedValue Out;
	if (!Value.IsValid()) return Out;

	const FProperty* Property = Value.Property;
	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		Out.Kind = FTypedValue::EKind::Bool;
		Out.bBool = BoolProp->GetPropertyValue(Value.ValuePtr);
	}
	else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		Out.Kind = FTypedValue::EKind::Int;
		Out.Int = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value.ValuePtr);
	}
	else if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		if (NumProp->IsFloatingPoint())
		{
			Out.Kind = FTypedValue::EKind::Float;
			Out.Float = NumProp->GetFloatingPointPropertyValue(Value.ValuePtr);
		}
		else
		{
			Out.Kind = FTypedValue::EKind::Int;
			Out.Int = NumProp->GetSignedIntPropertyValue(Value.ValuePtr);
		}
	}
	else
	{
		Out.Kind = FTypedValue::EKind::Text;
		Property->ExportText_Direct(Out.Text, Value.ValuePtr, Value.ValuePtr, Value.OwnerObject, PPF_None);
	}
	return Out;
}

void DRTuning::WriteValue(const FResolvedValue& Value, const FTypedValue& NewValue)
{
	if (!Value.IsValid() || !NewValue.IsSet()) return;

	FProperty* Property = Value.Property;
	switch (NewValue.Kind)
	{
	case FTypedValue::EKind::Bool:
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			BoolProp->SetPropertyValue(Value.ValuePtr, NewValue.bBool);
		}
		break;

	case FTypedValue::EKind::Int:
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(Value.ValuePtr, NewValue.Int);
		}
		else if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property); NumProp && NumProp->IsInteger())
		{
			NumProp->SetIntPropertyValue(Value.ValuePtr, NewValue.Int);
		}
		break;

	case FTypedValue::EKind::Float:
		if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property); NumProp && NumProp->IsFloatingPoint())
		{
			NumProp->SetFloatingPointPropertyValue(Value.ValuePtr, NewValue.Float);
		}
		break;

	case FTypedValue::EKind::Text:
		{
			FStringOutputDevice Errors;
			Property->ImportText_Direct(*NewValue.Text, Value.ValuePtr, Value.OwnerObject, PPF_None, &Errors);
		}
		break;

	default:
		break;
	}
}

bool DRTuning::SetValueFromText(const FResolvedValue& Value, const FString& InText, FString& OutError)
{
	if (!Value.IsValid())
	{
		OutError = TEXT("값 위치가 없습니다");
		return false;
	}

	const FString Text = InText.TrimStartAndEnd();
	FProperty* Property = Value.Property;

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool bNewValue = false;
		if (!ParseBool(Text, bNewValue))
		{
			OutError = FString::Printf(TEXT("'%s' 는 켜기/끄기 값이 아닙니다 (true/false · 1/0)"), *Text);
			return false;
		}
		BoolProp->SetPropertyValue(Value.ValuePtr, bNewValue);
		return true;
	}

	// 열거형 — 이름(Percent · EDRUpgradeOp::Percent)이나 숫자
	const UEnum* Enum = nullptr;
	FNumericProperty* EnumStorage = nullptr;
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		Enum = EnumProp->GetEnum();
		EnumStorage = EnumProp->GetUnderlyingProperty();
	}
	else if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property); NumProp && NumProp->GetIntPropertyEnum())
	{
		Enum = NumProp->GetIntPropertyEnum();
		EnumStorage = NumProp;
	}
	if (Enum && EnumStorage)
	{
		int64 EnumValue = 0;
		if (!ParseEnumValue(Enum, Text, EnumValue))
		{
			OutError = FString::Printf(TEXT("'%s' 는 %s 값이 아닙니다 (%s)"), *Text, *Enum->GetName(), *ListEnumNames(Enum));
			return false;
		}
		EnumStorage->SetIntPropertyValue(Value.ValuePtr, EnumValue);
		return true;
	}

	if (FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		double Number = 0.0;
		if (!ParseNumber(Text, Number, OutError)) return false;

		if (NumProp->IsFloatingPoint())
		{
			NumProp->SetFloatingPointPropertyValue(Value.ValuePtr, Number);
			return true;
		}

		const double Rounded = FMath::RoundToDouble(Number);
		if (FMath::Abs(Number - Rounded) > 1.e-6)
		{
			OutError = FString::Printf(TEXT("'%s' — 정수만 넣을 수 있는 칸입니다"), *Text);
			return false;
		}
		int64 Min = 0, Max = 0;
		GetIntegerRange(NumProp, Min, Max);
		if (Rounded < static_cast<double>(Min) || Rounded > static_cast<double>(Max))
		{
			OutError = FString::Printf(TEXT("'%s' — 이 칸은 %lld ~ %lld 만 됩니다"), *Text, Min, Max);
			return false;
		}
		NumProp->SetIntPropertyValue(Value.ValuePtr, static_cast<int64>(Rounded));
		return true;
	}

	// 그 밖의 타입은 엔진 텍스트 형식 그대로 — 구조체 (X=1,Y=2) · 이름 · 문자열 · 태그 (TagName="A.B")
	FStringOutputDevice Errors;
	if (!Property->ImportText_Direct(*Text, Value.ValuePtr, Value.OwnerObject, PPF_None, &Errors))
	{
		OutError = FString::Printf(TEXT("'%s' 를 %s 값으로 읽지 못했습니다 %s"), *Text, *Property->GetCPPType(), *Errors);
		return false;
	}
	return true;
}

bool DRTuning::IsListableLeaf(const FProperty* Property)
{
	if (CastField<FBoolProperty>(Property)) return true;
	if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		return NumProp->GetIntPropertyEnum() == nullptr;
	}
	return false;
}

bool DRTuning::TryGetNumber(const FResolvedValue& Value, double& OutNumber)
{
	const FNumericProperty* NumProp = Value.IsValid() ? CastField<FNumericProperty>(Value.Property) : nullptr;
	if (!NumProp || NumProp->GetIntPropertyEnum()) return false;

	OutNumber = NumProp->IsFloatingPoint()
		? NumProp->GetFloatingPointPropertyValue(Value.ValuePtr)
		: static_cast<double>(NumProp->GetSignedIntPropertyValue(Value.ValuePtr));
	return true;
}

UObject* DRTuning::LoadTargetObject(const FString& ObjectPath, FString& OutLoadPath, FString& OutError)
{
	FString Path = FPackageName::ExportTextPathToObjectPath(ObjectPath.TrimStartAndEnd());
	if (!Path.StartsWith(TEXT("/")))
	{
		OutError = FString::Printf(TEXT("경로는 /Game/… 으로 시작해야 합니다: '%s'"), *ObjectPath);
		return nullptr;
	}
	if (!Path.Contains(TEXT(".")))
	{
		// /Game/X/GA_Y → /Game/X/GA_Y.GA_Y
		Path += TEXT(".") + FPackageName::GetShortName(Path);
	}

	UObject* Loaded = nullptr;
	FString LoadPath = Path;
	if (Path.EndsWith(TEXT("_C")))
	{
		Loaded = StaticLoadObject(UClass::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	else
	{
		Loaded = StaticLoadObject(UObject::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
#if WITH_EDITORONLY_DATA
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(Loaded))
		{
			Loaded = Blueprint->GeneratedClass;
			LoadPath = Path + TEXT("_C");
		}
#endif
		if (!Loaded)
		{
			// 쿠킹 빌드에는 UBlueprint 가 없다 — 생성 클래스(_C)로 다시
			const FString ClassPath = Path + TEXT("_C");
			Loaded = StaticLoadObject(UClass::StaticClass(), nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (Loaded)
			{
				LoadPath = ClassPath;
			}
		}
	}

	if (!Loaded)
	{
		OutError = FString::Printf(TEXT("에셋을 찾지 못했습니다 (이 빌드에 없을 수 있음): %s"), *Path);
		return nullptr;
	}

	OutLoadPath = LoadPath;
	if (UClass* Class = Cast<UClass>(Loaded))
	{
		return Class->GetDefaultObject();
	}
	return Loaded;
}
