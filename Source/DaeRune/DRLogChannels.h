#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

// DaeRune 전용 로그 채널 정의
// 에디터 모듈(DaeRuneEditor)도 같은 채널을 쓰므로 내보낸다
DAERUNE_API DECLARE_LOG_CATEGORY_EXTERN(LogDR, Log, All);