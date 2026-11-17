// Six Inches - airframe integrity.

#include "Components/SIHealthComponent.h"

USIHealthComponent::USIHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USIHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
}

void USIHealthComponent::ApplyDamage(float Amount, ESIDamageSource Source)
{
	if (!bDamageEnabled || Health <= 0.0f || Amount <= 0.0f)
	{
		return;
	}
	const float Applied = FMath::Min(Amount, Health);
	Health -= Applied;
	DamageTaken += Applied;
	OnDamaged.Broadcast(Applied, Source, Health);
	if (Health <= 0.0f)
	{
		OnDepleted.Broadcast(Source);
	}
}

void USIHealthComponent::Kill(ESIDamageSource Source)
{
	if (!bDamageEnabled || Health <= 0.0f)
	{
		return;
	}
	DamageTaken += Health;
	const float Applied = Health;
	Health = 0.0f;
	OnDamaged.Broadcast(Applied, Source, 0.0f);
	OnDepleted.Broadcast(Source);
}
