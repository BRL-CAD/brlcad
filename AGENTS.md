# Code Quality Standards

Write code as a senior engineer would for long-term maintenance. These are non-negotiable defaults, not suggestions to apply when asked.

## Principles

- **DRY.** Extract duplicate logic. Refactor. Once and only once.
- **Minimize surprises.** Preserve existing style and conventions. Test when you can.
- **No magic values.** Use named constants, enums, or configuration. Every literal needs justification.
- **Minimize scope.** Prefer tightest scope that works - local variables, parameters, encapsulation. Broader scope needs justification.
- **No dead code.** Delete commented-out code, unused variables, unreachable branches. Version control remembers.
- **Comments explain why.** Document intent, constraints, and non-obvious decisions. Use "what" summaries only for complex or non-obvious algorithms. If routine code needs a comment to explain it, the code should be clearer.
- **Clean up what you touch.** Improve code you are changing: rename unclear variables, extract functions, simplify conditionals. Do not mix unrelated cleanup with logic changes - separate concerns, separate commits.
- **Handle errors deliberately.** Whether failing loudly or quietly, make it an intentional choice. No accidental swallowing.
- **YAGNI.** Do not add speculative abstractions, unused parameters, or unplanned extension points. Build what is needed now. Refactor when requirements actually change.
- **Perfection is when there is nothing left to take away.** Prefer reduction over addition, simpler is better. Too clever to debug easily is too clever.

## Before Finishing

Review your changes for code smells, duplicated logic, hardcoded values, overly broad scope, unclear names, missing error handling, unnecessary complexity, etc. Fix issues in the same pass - do not defer cleanup.

## Git

Never commit without permission (ask every time).  Never push to remote.  Never use `codex`, `chatgpt`, `openai`, `gemini`, `antigravity`, `copilot`, `claude`, `llm`, `ai`, or similar terms in branch names, tag names, commits, PRs, comments, and other metadata.  Do not add generated-by notices or otherwise indicate authorship unless user explicitly requests.  Do not include specific machine information - full file system paths, machine names, IP addresses, or other identifiable information in commit messages, code, comments or documentation.

## Repository Coding Conventions

The HACKING file in the root of the source tree defines coding conventions specific to BRL-CAD, including versions of common functions that are used in our code base instead of raw standard functions.  Review this file before writing code so your changes and additions adhere to BRL-CAD standards.
