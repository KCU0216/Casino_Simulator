# Ore Carry Movement

광물 운반 속도는 광물 무게를 현재 운반자 수로 나눈 뒤, 1인당 부담 무게를 기준 무게와 비교해서 계산한다.

## Formula

```text
EffectiveCarrierCount = Max(Carriers.Num(), 1)
WeightPerCarrier = OreWeight / EffectiveCarrierCount
WeightRatio = Clamp(WeightPerCarrier / MaxWeight, 0.0, 1.0)
SpeedMultiplier = Clamp(1.0 - WeightRatio, MinimumSpeedMultiplier, 1.0)
```

현재 코드 기준:

```text
MaxWeight = WeightForFullPenalty = 100.0
MinimumSpeedMultiplier = 0.1
```

## Carrier Count Rules

| Value | Meaning |
|---|---|
| OreWeight | 광물 자체 무게 |
| CarrierCount | 현재 광물을 들고 있는 플레이어 수 |
| MinCarryCount | 이 인원보다 적으면 광물이 움직이지 않음 |
| MaxCarryCount | 이 인원 이상이면 추가 운반자가 참여할 수 없음 |
| MaxWeight | 1명이 감당 가능한 기준 무게 |
| SpeedMultiplier | 캐릭터 이동속도에 곱해지는 운반 배율 |

## Examples

예시는 `MaxWeight = 100`, `MinimumSpeedMultiplier = 0.1` 기준이다.

| OreWeight | CarrierCount | MinCarryCount | MaxCarryCount | Can Move? | WeightPerCarrier | SpeedMultiplier |
|---:|---:|---:|---:|:---:|---:|---:|
| 20 | 1 | 1 | 2 | Yes | 20.0 | 0.80 |
| 50 | 1 | 1 | 2 | Yes | 50.0 | 0.50 |
| 50 | 2 | 1 | 2 | Yes | 25.0 | 0.75 |
| 100 | 1 | 1 | 2 | Yes | 100.0 | 0.10 |
| 100 | 2 | 1 | 2 | Yes | 50.0 | 0.50 |
| 100 | 1 | 2 | 3 | No | 100.0 | 0.10 |
| 100 | 2 | 2 | 3 | Yes | 50.0 | 0.50 |
| 100 | 3 | 2 | 3 | Yes | 33.3 | 0.67 |
| 200 | 1 | 2 | 4 | No | 200.0 | 0.10 |
| 200 | 2 | 2 | 4 | Yes | 100.0 | 0.10 |
| 200 | 4 | 2 | 4 | Yes | 50.0 | 0.50 |

## Notes

- `CanMoveCarry()` controls whether the ore receives movement force.
- `GetCarryMovementMultiplier()` controls how much the carrier's movement speed is reduced.
- If `CarrierCount` is lower than `MinCarryCount`, the ore does not move, but the current carrier can still receive the speed penalty.
- More carriers reduce `WeightPerCarrier`, so the speed penalty becomes lighter.
