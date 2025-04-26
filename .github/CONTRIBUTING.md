# Contributing to Declarative Sound Engine

Thank you for your interest in contributing!

This project follows a simple, clean workflow to ensure stability and clarity.

## Workflow

- Work from the `dev` branch.
- Create a feature branch for each new feature or fix:
  - `feature/your-topic`
- Open Pull Requests (PRs) targeting `dev`, not `main`.

## Commit Messages

Use clear, scoped commit messages:

[Scope] What you changed

Examples:

- `[AudioCore] Implement basic command processing`
- `[BehaviorLoader] Add inheritance and nested resolution`
- `[Tests] Add CLI unit tests for SoundManager`

## Code Style

- Prefer clarity over cleverness.
- Keep functions short and focused.
- Favor small, composable modules over giant files.

## Behavior System Notes

- All AudioBehaviors must be fully resolved (inheritance + nested behaviors) before runtime.
- SoundNodes must be flattened during the packing phase.
- Behavior IDs must be unique across the system.

## Testing

- All new features must be testable via the CLI test tool or automated tests.
- Log meaningful output during testing to verify behavior execution.

## Pull Request Checklist

Before submitting a PR:
- [ ] Code compiles without errors.
- [ ] Core functionality has been manually tested.
- [ ] Behavior packing phase completes without warnings.
- [ ] No unresolved `.audio` references remain in ResolvedBehavior database.

---

Thanks for helping keep the system clean, scalable, and powerful!