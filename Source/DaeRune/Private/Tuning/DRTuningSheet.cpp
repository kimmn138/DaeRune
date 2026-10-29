// Copyright DaeRune

#include "Tuning/DRTuningSheet.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/Csv/CsvParser.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
	enum EColumn : int32
	{
		Col_Group,
		Col_Target,
		Col_Property,
		Col_Current,
		Col_Override,
		Col_Range,
		Col_Description,
		Col_Note,
		Col_Path,
		Col_Count
	};

	// 머리줄 칸 이름 — 쓸 때는 한국어, 읽을 때는 영문 별칭도 받는다
	struct FColumnName
	{
		const TCHAR* Korean;
		const TCHAR* English;
	};

	const FColumnName ColumnNames[Col_Count] =
	{
		{ TEXT("분류"),	TEXT("Group") },
		{ TEXT("대상"),	TEXT("Target") },
		{ TEXT("항목"),	TEXT("Property") },
		{ TEXT("현재값"),	TEXT("Current") },
		{ TEXT("변경값"),	TEXT("Value") },
		{ TEXT("범위"),	TEXT("Range") },
		{ TEXT("설명"),	TEXT("Description") },
		{ TEXT("메모"),	TEXT("Note") },
		{ TEXT("경로"),	TEXT("Path") },
	};

	bool IsValidUtf8(const uint8* Data, int32 Size)
	{
		int32 Index = 0;
		while (Index < Size)
		{
			const uint8 Lead = Data[Index];
			int32 Extra = 0;
			if (Lead < 0x80)					Extra = 0;
			else if ((Lead & 0xE0) == 0xC0)	Extra = 1;
			else if ((Lead & 0xF0) == 0xE0)	Extra = 2;
			else if ((Lead & 0xF8) == 0xF0)	Extra = 3;
			else return false;

			if (Index + Extra >= Size && Extra > 0) return false;
			for (int32 Offset = 1; Offset <= Extra; ++Offset)
			{
				if ((Data[Index + Offset] & 0xC0) != 0x80) return false;
			}
			Index += Extra + 1;
		}
		return true;
	}

	bool IsPlainNumber(const FString& Text)
	{
		if (Text.IsEmpty()) return false;
		int32 Start = (Text[0] == TEXT('-') || Text[0] == TEXT('+')) ? 1 : 0;
		bool bDigit = false;
		for (int32 Index = Start; Index < Text.Len(); ++Index)
		{
			const TCHAR Ch = Text[Index];
			if (FChar::IsDigit(Ch)) { bDigit = true; continue; }
			if (Ch == TEXT('.') || Ch == TEXT('e') || Ch == TEXT('E') || Ch == TEXT('-') || Ch == TEXT('+')) continue;
			return false;
		}
		return bDigit;
	}

	FString EscapeCell(const FString& InCell)
	{
		FString Cell = InCell;

		// 엑셀은 = + - @ 로 시작하는 칸을 수식으로 읽는다 ("- 설명" → #NAME?). 숫자가 아니면 앞에 공백을 둔다
		if (!Cell.IsEmpty() && FCString::Strchr(TEXT("=+-@"), Cell[0]) && !IsPlainNumber(Cell))
		{
			Cell.InsertAt(0, TEXT(' '));
		}

		const bool bNeedsQuotes = Cell.Contains(TEXT(",")) || Cell.Contains(TEXT("\"")) || Cell.Contains(TEXT("\n"))
			|| Cell.Contains(TEXT("\r")) || Cell.StartsWith(TEXT(" ")) || Cell.EndsWith(TEXT(" "));
		if (!bNeedsQuotes) return Cell;
		return TEXT("\"") + Cell.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
	}

	FString RowKey(const FString& TargetOrPath, const FString& Property)
	{
		return TargetOrPath.TrimStartAndEnd().ToLower() + TEXT("|") + Property.TrimStartAndEnd().ToLower();
	}
}

bool DRTuningSheet::LoadFileText(const FString& FilePath, FString& OutText)
{
	TArray<uint8> Bytes;
	// FILEREAD_AllowWrite — 엑셀이 파일을 열고 있어도 읽는다
	if (!FFileHelper::LoadFileToArray(Bytes, *FilePath, FILEREAD_AllowWrite | FILEREAD_Silent))
	{
		return false;
	}

	const uint8* Data = Bytes.GetData();
	const int32 Size = Bytes.Num();

#if PLATFORM_WINDOWS
	const bool bHasBom = (Size >= 2 && ((Data[0] == 0xFF && Data[1] == 0xFE) || (Data[0] == 0xFE && Data[1] == 0xFF)))
		|| (Size >= 3 && Data[0] == 0xEF && Data[1] == 0xBB && Data[2] == 0xBF);
	if (!bHasBom && Size > 0 && !IsValidUtf8(Data, Size))
	{
		// 엑셀의 "CSV (쉼표로 분리)" 는 시스템 코드 페이지(한국어 윈도우 = CP949)로 저장한다
		const int32 WideLength = ::MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<LPCCH>(Data), Size, nullptr, 0);
		if (WideLength > 0)
		{
			TArray<WIDECHAR> Wide;
			Wide.SetNumZeroed(WideLength + 1);
			::MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<LPCCH>(Data), Size, Wide.GetData(), WideLength);
			OutText = Wide.GetData();
			return true;
		}
	}
#endif

	FFileHelper::BufferToString(OutText, Data, Size);
	return true;
}

bool DRTuningSheet::ParseSheet(const FString& Text, TArray<FDRTuningRow>& OutRows, FString& OutError)
{
	OutRows.Reset();

	const FCsvParser Parser(Text);
	const FCsvParser::FRows& Rows = Parser.GetRows();

	// 머리줄 찾기 — [항목] 칸이 있는 첫 줄. 칸 순서는 보지 않는다
	int32 ColumnIndex[Col_Count];
	int32 HeaderRow = INDEX_NONE;
	for (int32 RowIndex = 0; RowIndex < Rows.Num() && HeaderRow == INDEX_NONE; ++RowIndex)
	{
		for (int32 Column = 0; Column < Col_Count; ++Column)
		{
			ColumnIndex[Column] = INDEX_NONE;
		}

		const TArray<const TCHAR*>& Cells = Rows[RowIndex];
		for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
		{
			const FString Cell = FString(Cells[CellIndex]).TrimStartAndEnd();
			for (int32 Column = 0; Column < Col_Count; ++Column)
			{
				if (ColumnIndex[Column] == INDEX_NONE
					&& (Cell.Equals(ColumnNames[Column].Korean, ESearchCase::IgnoreCase) || Cell.Equals(ColumnNames[Column].English, ESearchCase::IgnoreCase)))
				{
					ColumnIndex[Column] = CellIndex;
				}
			}
		}
		if (ColumnIndex[Col_Property] != INDEX_NONE)
		{
			HeaderRow = RowIndex;
		}
	}

	if (HeaderRow == INDEX_NONE)
	{
		OutError = TEXT("머리줄([항목] 칸이 있는 줄)을 찾지 못했습니다");
		return false;
	}
	if (ColumnIndex[Col_Override] == INDEX_NONE)
	{
		OutError = TEXT("[변경값] 칸이 없습니다");
		return false;
	}
	if (ColumnIndex[Col_Target] == INDEX_NONE && ColumnIndex[Col_Path] == INDEX_NONE)
	{
		OutError = TEXT("[대상] 칸도 [경로] 칸도 없습니다");
		return false;
	}

	auto CellAt = [&ColumnIndex](const TArray<const TCHAR*>& Cells, int32 Column) -> FString
	{
		const int32 Index = ColumnIndex[Column];
		return (Index != INDEX_NONE && Cells.IsValidIndex(Index)) ? FString(Cells[Index]).TrimStartAndEnd() : FString();
	};

	for (int32 RowIndex = HeaderRow + 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& Cells = Rows[RowIndex];

		FDRTuningRow Row;
		Row.Group = CellAt(Cells, Col_Group);
		Row.Target = CellAt(Cells, Col_Target);
		Row.Property = CellAt(Cells, Col_Property);
		Row.Current = CellAt(Cells, Col_Current);
		Row.Override = CellAt(Cells, Col_Override);
		Row.Range = CellAt(Cells, Col_Range);
		Row.Description = CellAt(Cells, Col_Description);
		Row.Note = CellAt(Cells, Col_Note);
		Row.Path = CellAt(Cells, Col_Path);
		Row.LineNumber = RowIndex + 1;

		if (Row.Target.IsEmpty() && Row.Property.IsEmpty() && Row.Override.IsEmpty() && Row.Path.IsEmpty() && Row.Note.IsEmpty())
		{
			continue;	// 빈 줄
		}
		if (Row.Group.StartsWith(TEXT("#")) || (Row.Group.IsEmpty() && Row.Target.StartsWith(TEXT("#"))))
		{
			continue;	// 주석 줄
		}
		OutRows.Add(MoveTemp(Row));
	}
	return true;
}

FString DRTuningSheet::BuildSheetText(const TArray<FDRTuningRow>& Rows)
{
	FString Text;
	Text.Reserve(Rows.Num() * 160 + 128);

	for (int32 Column = 0; Column < Col_Count; ++Column)
	{
		if (Column > 0) Text += TEXT(",");
		Text += ColumnNames[Column].Korean;
	}
	Text += TEXT("\r\n");

	for (const FDRTuningRow& Row : Rows)
	{
		const FString* Cells[Col_Count] =
		{
			&Row.Group, &Row.Target, &Row.Property, &Row.Current, &Row.Override,
			&Row.Range, &Row.Description, &Row.Note, &Row.Path
		};
		for (int32 Column = 0; Column < Col_Count; ++Column)
		{
			if (Column > 0) Text += TEXT(",");
			Text += EscapeCell(*Cells[Column]);
		}
		Text += TEXT("\r\n");
	}
	return Text;
}

bool DRTuningSheet::SaveText(const FString& FilePath, const FString& Text)
{
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), /*Tree*/ true);
	// BOM 이 있어야 엑셀이 한글을 깨뜨리지 않고 연다
	return FFileHelper::SaveStringToFile(Text, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8);
}

void DRTuningSheet::ExtractOverrides(const TArray<FDRTuningRow>& Rows, TArray<FDRTuningOverride>& OutOverrides, TArray<int32>& OutLineNumbers)
{
	OutOverrides.Reset();
	OutLineNumbers.Reset();
	for (const FDRTuningRow& Row : Rows)
	{
		if (Row.Override.IsEmpty()) continue;

		FDRTuningOverride Override;
		Override.Target = Row.Path.IsEmpty() ? Row.Target : Row.Path;
		Override.Property = Row.Property;
		Override.Value = Row.Override;
		OutOverrides.Add(MoveTemp(Override));
		OutLineNumbers.Add(Row.LineNumber);
	}
}

void DRTuningSheet::MergeSheet(const TArray<FDRTuningRow>& Catalog, const TArray<FDRTuningRow>& Existing, TArray<FDRTuningRow>& OutMerged)
{
	// 같은 항목을 가리키는 기존 줄이 여럿일 수 있다 — 목록 줄 + 기획자가 아래에 직접 적은 줄 (경로 없이 짧은 이름으로)
	TMap<FString, TArray<int32>> ByPath;
	TMap<FString, TArray<int32>> ByTarget;
	for (int32 Index = 0; Index < Existing.Num(); ++Index)
	{
		const FDRTuningRow& Row = Existing[Index];
		if (!Row.Path.IsEmpty())	ByPath.FindOrAdd(RowKey(Row.Path, Row.Property)).Add(Index);
		if (!Row.Target.IsEmpty())	ByTarget.FindOrAdd(RowKey(Row.Target, Row.Property)).Add(Index);
	}

	TSet<int32> Used;
	OutMerged.Reset(Catalog.Num() + 16);

	for (const FDRTuningRow& CatalogRow : Catalog)
	{
		FDRTuningRow Row = CatalogRow;
		Row.Override.Reset();
		Row.Note.Reset();

		TArray<int32> Candidates;
		if (const TArray<int32>* Found = ByPath.Find(RowKey(Row.Path, Row.Property)))		Candidates.Append(*Found);
		if (const TArray<int32>* Found = ByTarget.Find(RowKey(Row.Target, Row.Property)))	Candidates.Append(*Found);
		Candidates.Sort();

		// 겹치면 아래쪽(나중) 줄의 변경값이 이긴다 — 적용할 때 "아래 줄이 이긴다" 와 같은 규칙. 겹친 줄은 이 한 줄로 합친다
		for (const int32 Index : Candidates)
		{
			if (Used.Contains(Index)) continue;
			Used.Add(Index);

			const FDRTuningRow& Source = Existing[Index];
			if (!Source.Override.IsEmpty())
			{
				Row.Override = Source.Override;
			}
			if (!Source.Note.IsEmpty() && !Row.Note.Contains(Source.Note))
			{
				Row.Note = Row.Note.IsEmpty() ? Source.Note : Row.Note + TEXT(" / ") + Source.Note;
			}
		}
		OutMerged.Add(MoveTemp(Row));
	}

	// 목록에 없는 줄 — 직접 추가한 줄이나 이번 빌드에서 사라진 항목. 변경값 · 메모가 있으면 지우지 않고 끝에 남긴다
	for (int32 Index = 0; Index < Existing.Num(); ++Index)
	{
		if (Used.Contains(Index)) continue;

		const FDRTuningRow& Row = Existing[Index];
		if (Row.Override.IsEmpty() && Row.Note.IsEmpty()) continue;

		FDRTuningRow Kept = Row;
		if (Kept.Group.IsEmpty())
		{
			Kept.Group = TEXT("직접 추가");
		}
		OutMerged.Add(MoveTemp(Kept));
	}
}

bool DRTuningSheet::ParseRange(const FString& RangeText, TOptional<double>& OutMin, TOptional<double>& OutMax)
{
	OutMin.Reset();
	OutMax.Reset();

	FString Left, Right;
	if (!RangeText.Split(TEXT("~"), &Left, &Right))
	{
		return false;
	}
	Left.TrimStartAndEndInline();
	Right.TrimStartAndEndInline();
	if (!Left.IsEmpty() && Left.IsNumeric())	OutMin = FCString::Atod(*Left);
	if (!Right.IsEmpty() && Right.IsNumeric())	OutMax = FCString::Atod(*Right);
	return OutMin.IsSet() || OutMax.IsSet();
}
