
#include "DRAbilityTypes.h"

// 네트워크 직렬화 구현부임
bool FDRGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint32 RepBits = 0; // 전송 비트마스크 선언임
	if (Ar.IsSaving())
	{
		// 각 플래그 및 데이터 존재 여부 체크하여 비트 설정임
		if (bReplicateInstigator && Instigator.IsValid())
		{
			RepBits |= 1 << 0; // Instigator 존재 비트임
		}
		if (bReplicateEffectCauser && EffectCauser.IsValid())
		{
			RepBits |= 1 << 1; // EffectCauser 존재 비트임
		}
		if (AbilityCDO.IsValid())
		{
			RepBits |= 1 << 2; // AbilityCDO 존재 비트임
		}
		if (bReplicateSourceObject && SourceObject.IsValid())
		{
			RepBits |= 1 << 3; // SourceObject 존재 비트임
		}
		if (Actors.Num() > 0)
		{
			RepBits |= 1 << 4; // Actors 배열 비어있지 않음 비트임
		}
		if (HitResult.IsValid())
		{
			RepBits |= 1 << 5; // HitResult 존재 비트임
		}
		if (bHasWorldOrigin)
		{
			RepBits |= 1 << 6; // WorldOrigin 존재 비트임
		}
		if (bIsSuccessfulDebuff)
		{
			RepBits |= 1 << 7; // 디버프 성공 비트임
		}
		if (DebuffDamage > 0.f)
		{
			RepBits |= 1 << 8; // 디버프 데미지 비트임
		}
		if (DebuffDuration > 0.f)
		{
			RepBits |= 1 << 9; // 디버프 지속시간 비트임
		}
		if (DebuffFrequency > 0.f)
		{
			RepBits |= 1 << 10; // 디버프 빈도 비트임
		}
		if (DamageType.IsValid())
		{ 
			RepBits |= 1 << 11; // 데미지 타입 비트임
		}
		if (!DeathImpulse.IsZero())
		{
			RepBits |= 1 << 12; // 사망 임펄스 비트임
		}
		if (!KnockbackForce.IsZero())
		{
			RepBits |= 1 << 13; // 넉백 힘 비트임
		}
	}

	// 비트마스크 직렬화 처리임
	Ar.SerializeBits(&RepBits, 14);

	// 읽기/쓰기 플래그별 데이터 직렬화임
	if (RepBits & (1 << 0))
	{
		Ar << Instigator; // Instigator 직렬화임
	}
	if (RepBits & (1 << 1))
	{
		Ar << EffectCauser; // EffectCauser 직렬화임
	}
	if (RepBits & (1 << 2))
	{
		Ar << AbilityCDO; // AbilityCDO 직렬화임
	}
	if (RepBits & (1 << 3))
	{
		Ar << SourceObject; // SourceObject 직렬화임
	}
	if (RepBits & (1 << 4))
	{
		SafeNetSerializeTArray_Default<31>(Ar, Actors); // Actors 배열 직렬화임
	}
	if (RepBits & (1 << 5)) // HitResult 직렬화 처리임
	{
		if (Ar.IsLoading())
		{
			if (!HitResult.IsValid())
			{
				HitResult = TSharedPtr<FHitResult>(new FHitResult());
			}
		}
		HitResult->NetSerialize(Ar, Map, bOutSuccess); // 히트 결과 NetSerialize임
	}
	if (RepBits & (1 << 6)) // WorldOrigin 직렬화 임포스트
	{
		Ar << WorldOrigin;
		bHasWorldOrigin = true;
	}
	else
	{
		bHasWorldOrigin = false;
	}
	if (RepBits & (1 << 7))
	{
		Ar << bIsSuccessfulDebuff; // 디버프 성공 직렬화임
	}
	if (RepBits & (1 << 8))
	{
		Ar << DebuffDamage; // 디버프 데미지 직렬화임
	}
	if (RepBits & (1 << 9))
	{
		Ar << DebuffDuration; // 디버프 지속시간 직렬화임
	}
	if (RepBits & (1 << 10))
	{
		Ar << DebuffFrequency; // 디버프 빈도 직렬화임
	}
	if (RepBits & (1 << 11))
	{
		if (Ar.IsLoading())
		{
			if (!DamageType.IsValid())
			{
				DamageType = TSharedPtr<FGameplayTag>(new FGameplayTag());
			}
		}
		DamageType->NetSerialize(Ar, Map, bOutSuccess); // 데미지 타입 NetSerialize임
	}
	if (RepBits & (1 << 12))
	{ 
		DeathImpulse.NetSerialize(Ar, Map, bOutSuccess); // 사망 임펄스 NetSerialize임
	}
	if (RepBits & (1 << 13))
	{
		KnockbackForce.NetSerialize(Ar, Map, bOutSuccess); // 넉백 힘 NetSerialize임
	}

	// 로딩 시 추가 초기화 처리임
	if (Ar.IsLoading())
	{
		AddInstigator(Instigator.Get(), EffectCauser.Get()); // Instigator 초기화 콜백임
	}

	bOutSuccess = true; // 직렬화 성공 플래그 설정임
	return true; // 함수 종료 반환임
} 
