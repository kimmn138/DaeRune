
#include "DRAbilityTypes.h"

bool FDRGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint32 RepBits = 0;
	// 직렬화할 데이터 비트 마스킹
	if (Ar.IsSaving())
	{
		if (bReplicateInstigator && Instigator.IsValid())
		{
			RepBits |= 1 << 0;
		}
		if (bReplicateEffectCauser && EffectCauser.IsValid())
		{
			RepBits |= 1 << 1;
		}
		if (AbilityCDO.IsValid())
		{
			RepBits |= 1 << 2;
		}
		if (bReplicateSourceObject && SourceObject.IsValid())
		{
			RepBits |= 1 << 3;
		}
		if (Actors.Num() > 0)
		{
			RepBits |= 1 << 4;
		}
		if (HitResult.IsValid())
		{
			RepBits |= 1 << 5;
		}
		if (bHasWorldOrigin)
		{
			RepBits |= 1 << 6;
		}
		if (bIsSuccessfulDebuff)
		{
			RepBits |= 1 << 7;
		}
		if (DebuffDamage > 0.f)
		{
			RepBits |= 1 << 8;
		}
		if (DebuffDuration > 0.f)
		{
			RepBits |= 1 << 9;
		}
		if (DamageType.IsValid())
		{
			RepBits |= 1 << 10;
		}
		if (!DeathImpulse.IsZero())
		{
			RepBits |= 1 << 11;
		}
		if (!KnockbackForce.IsZero())
		{
			RepBits |= 1 << 12;
		}
		if (!SourceAbilityTags.IsEmpty())
		{
			RepBits |= 1 << 13;
		}
	}

	// 비트 마스크 직렬화
	Ar.SerializeBits(&RepBits, 14);

	if (RepBits & (1 << 0))
	{
		Ar << Instigator;
	}
	if (RepBits & (1 << 1))
	{
		Ar << EffectCauser;
	}
	if (RepBits & (1 << 2))
	{
		Ar << AbilityCDO;
	}
	if (RepBits & (1 << 3))
	{
		Ar << SourceObject;
	}
	if (RepBits & (1 << 4))
	{
		SafeNetSerializeTArray_Default<31>(Ar, Actors);
	}
	if (RepBits & (1 << 5))
	{
		if (Ar.IsLoading())
		{
			if (!HitResult.IsValid())
			{
				HitResult = TSharedPtr<FHitResult>(new FHitResult());
			}
		}
		HitResult->NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 6))
	{
		Ar << WorldOrigin;
		bHasWorldOrigin = true;
	}
	else
	{
		bHasWorldOrigin = false;
	}
	// 디버프 성공 여부는 비트 자체가 값이다
	bIsSuccessfulDebuff = (RepBits & (1 << 7)) != 0;
	if (RepBits & (1 << 8))
	{
		Ar << DebuffDamage;
	}
	if (RepBits & (1 << 9))
	{
		Ar << DebuffDuration;
	}
	if (RepBits & (1 << 10))
	{
		DamageType.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 11))
	{
		DeathImpulse.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 12))
	{
		KnockbackForce.NetSerialize(Ar, Map, bOutSuccess);
	}
	// 소스 어빌리티 태그 — 예전에는 빠져 있어 클라 쪽 컨텍스트에서는 늘 비어 있었다
	if (RepBits & (1 << 13))
	{
		SourceAbilityTags.NetSerialize(Ar, Map, bOutSuccess);
	}

	// 로딩 시 InstigatorAbilitySystemComponent 초기화
	if (Ar.IsLoading())
	{
		AddInstigator(Instigator.Get(), EffectCauser.Get()); // Just to initialize InstigatorAbilitySystemComponent
	}

	bOutSuccess = true;
	return true;
}
