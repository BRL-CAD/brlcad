# Code Quality Standards

Write code as a senior engineer would for long-term maintenance. These are non-negotiable defaults, not suggestions to apply when asked.

## Principles

- **DRY.** Extract duplicate logic. Refactor. Once and only once.
- **Minimize surprises.** Preserve existing style and conventions. Test when you can.
- **No magic values.** Use named constants, enums, or configuration. Every literal needs justification.
- **Minimize scope.** Prefer tightest scope that works — local variables, parameters, encapsulation. Broader scope needs justification.
- **No dead code.** Delete commented-out code, unused variables, unreachable branches. Version control remembers.
- **Comments explain why.** Document intent, constraints, and non-obvious decisions. Use "what" summaries only for complex or non-obvious algorithms. If routine code needs a comment to explain it, the code should be clearer.
- **Clean up what you touch.** Improve code you are changing: rename unclear variables, extract functions, simplify conditionals. Do not mix unrelated cleanup with logic changes — separate concerns, separate commits.
- **Handle errors deliberately.** Whether failing loudly or quietly, make it an intentional choice. No accidental swallowing.
- **YAGNI.** Do not add speculative abstractions, unused parameters, or unplanned extension points. Build what is needed now. Refactor when requirements actually change.
- **Perfection is when there is nothing left to take away.** Prefer reduction over addition, simpler is better. Too clever to debug easily is too clever.

## Before Finishing

Review your changes for code smells, duplicated logic, hardcoded values, overly broad scope, unclear names, missing error handling, unnecessary complexity, etc. Fix issues in the same pass — do not defer cleanup.
