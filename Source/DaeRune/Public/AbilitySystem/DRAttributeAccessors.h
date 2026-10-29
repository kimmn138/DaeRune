// Copyright DaeRune

#pragma once

#include "AttributeSet.h"

// 어트리뷰트 Getter / Setter / Initter 를 한 번에 만드는 매크로 (프로젝트의 모든 AttributeSet 공용)
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
