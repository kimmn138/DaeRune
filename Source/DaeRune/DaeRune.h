// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// 프로젝트 전역에서 사용되는 공통 정의, 매크로를 관리

// 발사체 전용 콜리전 채널
#define ECC_Projectile ECollisionChannel::ECC_GameTraceChannel1
// 타겟 전용 콜리전 채널
#define ECC_Target ECollisionChannel::ECC_GameTraceChannel2
