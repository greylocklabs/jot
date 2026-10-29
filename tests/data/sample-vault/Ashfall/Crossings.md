---
tags: [ashfall]
---
Every crossing Mara logs, in her own hand.

## Ledger

| Day | Passenger    | Fare      |
| --- | ------------ | --------- |
| 1   | A tinker     | Two coins |
| 2   | Soren        | A promise |
| 3   | The stranger | Nothing   |

## Still to do

- [x] Match the ledger to [[Mara Vey]]'s voice
- [ ] Fares for act two @writing 📅 2026-10-27
- [ ] Ask playtesters about the stranger @writing

## Fare rule

```python
def fare(passenger):
    if passenger.is_child:
        return 0  # she never charges children
    return passenger.coins // 2
```
