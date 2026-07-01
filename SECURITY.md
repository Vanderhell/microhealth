# Security Policy

## Scope

Use this policy for vulnerabilities that can cause memory corruption, undefined behavior, unsafe API misuse, or other security-relevant defects in `microhealth`.

## Reporting

Do not open public GitHub issues for suspected vulnerabilities.

Report privately to the maintainer with:

- affected commit or release reference,
- exact compiler and platform,
- minimal reproduction,
- impact assessment,
- any sanitizer or debugger output.

## What to include

- whether the issue requires malformed caller input or only normal API use,
- whether it affects C, C++, installed package consumption, or only tests,
- whether it depends on unsupported usage such as same-instance concurrent access or callback escape through `longjmp`/exceptions.

## Response expectations

- Initial triage: best effort.
- Fix timeline: no SLA is promised.
- Coordinated disclosure is preferred until a fix is available.

## Out of scope

- Missing persistence, encryption, logging, transport, or sandboxing features.
- Unsupported concurrent use of one instance without caller serialization.
- Unsupported callback escape through `longjmp` or C++ exceptions across C frames.

