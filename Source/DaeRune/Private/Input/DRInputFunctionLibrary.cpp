// Copyright DaeRune

#include "Input/DRInputFunctionLibrary.h"
#include "Framework/Application/SlateApplication.h"

bool UDRInputFunctionLibrary::IsMouseButtonPressed(FKey Button)
{
	if (FSlateApplication::IsInitialized())
	{
		return FSlateApplication::Get().GetPressedMouseButtons().Contains(Button);
	}
	return false;
}
