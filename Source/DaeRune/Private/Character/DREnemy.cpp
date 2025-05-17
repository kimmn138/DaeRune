// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/DREnemy.h"

ADREnemy::ADREnemy()
{
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}
