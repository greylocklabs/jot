---
tags: [ashfall]
---
Every crossing Mara logs, in her own hand. The route is drawn in [Route](Route.md), and the
timetable is in [crossings.csv](Records/crossings.csv).

## Ledger

| Day | Passenger    | Fare      |
| --- | ------------ | --------- |
| 1   | A tinker     | Two coins |
| 2   | Soren        | A promise |
| 3   | The stranger | Nothing   |

## Still to do

- [x] Match the ledger to [Mara Vey](Characters/Mara%20Vey.md)'s voice
- [ ] Fares for act two #ashfall/act-two @due 2026-10-27 @p2
- [ ] Ask playtesters about the stranger @p4

## Fare rule

```python
def fare(passenger):
    if passenger.is_child:
        return 0  # she never charges children
    return passenger.coins // 2
```
