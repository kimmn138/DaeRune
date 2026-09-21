// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"

/**
 * 8퍼즐 기하 상수 + 규칙 SSOT (Plan6 §14.2.2).
 *
 * ★이 파일이 위젯 헤더에서 분리된 이유★ (2026-09-20)
 * 보드의 주인이 ADRS2SlidePuzzle(서버 액터)로 옮겨가면서, 같은 판정식을 두 곳이 쓰게 됐다.
 *   - 서버: 이동을 승인할지 결정한다 (진짜 판정)
 *   - 위젯: 못 가는 조각을 흔들어 줄지 결정한다 (연출 · 서버 왕복 없이 즉시)
 * 액터가 UI 위젯 헤더를 include 할 수는 없으므로 공용 헤더로 내렸다.
 * 양쪽에 복사해 두면 한쪽만 고쳐져 "내 화면에선 움직이는데 서버는 거절하는" 버그가 난다.
 *
 * 4x4(15퍼즐)로 바꾸려면 GridSize 를 4 로 올리고 CellSize 를 새 텍스처 크기로 맞춘다.
 * 좌표 변환 / 인접 판정 / 셔플이 전부 여기서 파생된다.
 */
namespace DRS2Puzzle
{
	// ===== 기하 =====

	inline constexpr int32 GridSize  = 3;                    // 한 변의 칸 수
	inline constexpr int32 CellCount = GridSize * GridSize;  // 9 (빈칸 포함한 전체 칸)
	inline constexpr int32 TileCount = CellCount - 1;        // 8 (실제 조각 수)

	inline constexpr float CellSize  = 224.f;                // 조각 PNG 원본 크기
	inline constexpr float CellGap   = 2.f;                  // 칸 사이 간격
	inline constexpr float CellPitch = CellSize + CellGap;   // 226 = 한 칸 이동 거리

	// ===== 규칙 =====
	// 보드 표현: 길이 9 배열. 값 = TileId(1~8), 0 = 빈칸. 인덱스 = Row * GridSize + Col

	/** 상하좌우로 맞닿은 칸 인덱스들. 줄이 넘어가는 이동은 여기서 걸러진다. */
	inline TArray<int32> GetNeighbors(int32 Index)
	{
		TArray<int32> Out;
		Out.Reserve(4);

		if (Index < 0 || Index >= CellCount)
		{
			return Out;
		}

		const int32 Row = Index / GridSize;
		const int32 Col = Index % GridSize;

		if (Row > 0)            { Out.Add(Index - GridSize); }
		if (Row < GridSize - 1) { Out.Add(Index + GridSize); }
		if (Col > 0)            { Out.Add(Index - 1); }
		if (Col < GridSize - 1) { Out.Add(Index + 1); }

		return Out;
	}

	/**
	 * 맨해튼 거리 1 검사.
	 *
	 * ★Abs(A-B)==1 로 하면 안 된다★ - 인덱스 2와 3은 화면에서 줄이 다른데 값 차이는 1이다.
	 * 오른쪽 끝 조각이 왼쪽 끝으로 순간이동하는 버그가 여기서 나온다.
	 */
	inline bool AreAdjacent(int32 A, int32 B)
	{
		if (A < 0 || B < 0 || A >= CellCount || B >= CellCount)
		{
			return false;
		}

		const int32 RowA = A / GridSize, ColA = A % GridSize;
		const int32 RowB = B / GridSize, ColB = B % GridSize;

		return FMath::Abs(RowA - RowB) + FMath::Abs(ColA - ColB) == 1;
	}

	/** 정답 배치 {1,2,...,8,0}. */
	inline TArray<int32> MakeSolvedBoard()
	{
		TArray<int32> Board;
		Board.Reserve(CellCount);

		for (int32 i = 1; i < CellCount; ++i)
		{
			Board.Add(i);
		}
		Board.Add(0);

		return Board;
	}

	/** 앞 8칸이 제자리면 나머지는 자동으로 빈칸이다. */
	inline bool IsSolvedBoard(const TArray<int32>& Board)
	{
		if (Board.Num() != CellCount)
		{
			return false;
		}

		for (int32 i = 0; i < CellCount - 1; ++i)
		{
			if (Board[i] != i + 1)
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * 반드시 풀 수 있는 배치를 만든다.
	 *
	 * ★무작위 순열을 쓰면 안 된다★ - 9칸을 그냥 섞으면 절반은 아무리 움직여도 안 풀린다
	 * (순열의 짝홀이 정답과 달라진다). 정답에서 출발해 합법 이동만 밟으면 항상 되짚어 갈 수 있다.
	 * 미로를 출구에서부터 걸어 나와 입구를 만드는 것과 같다.
	 */
	inline TArray<int32> MakeShuffledBoard(int32 Steps)
	{
		constexpr int32 MaxAttempts = 8;

		TArray<int32> Board = MakeSolvedBoard();

		for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
		{
			Board = MakeSolvedBoard();

			int32 Empty     = CellCount - 1;
			int32 PrevEmpty = INDEX_NONE;

			for (int32 Step = 0; Step < Steps; ++Step)
			{
				TArray<int32> Candidates = GetNeighbors(Empty);

				// 직전에 빈칸이 있던 자리로 되돌아가면 방금 한 이동이 취소된다.
				Candidates.Remove(PrevEmpty);
				if (Candidates.Num() == 0)
				{
					continue;
				}

				const int32 Pick = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
				Swap(Board[Empty], Board[Pick]);

				PrevEmpty = Empty;
				Empty     = Pick;
			}

			// 드물게 제자리로 돌아온다. 그 판은 버리고 다시 섞는다.
			if (!IsSolvedBoard(Board))
			{
				break;
			}
		}

		return Board;
	}
}
