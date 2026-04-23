# Contributing Guidelines

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/HorsAVSDK.git`
3. Create a branch: `git checkout -b feature/your-feature`

## Development Workflow

### 1. Create Feature Branch

```bash
git checkout -b feature/description
# or
git checkout -b bugfix/issue-number
# or
git checkout -b hotfix/critical-bug
```

### 2. Make Changes

- Follow [Code Style](../context/development_guidelines.md)
- Add/update tests
- Update documentation

### 3. Commit

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```bash
git commit -m "feat(player): add progress callback support"
git commit -m "fix(audio): resolve buffer underrun issue"
git commit -m "docs(api): update Objective-C interface spec"
git commit -m "perf(rendering): optimize zero-copy path"
```

**Types:** `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `build`, `ci`, `perf`

### 4. Push and Create PR

```bash
git push -u origin feature/description
gh pr create --title "feat: description" --body "Details"
```

## Pull Request Process

1. Ensure tests pass locally
2. Update relevant documentation
3. Link related issues: `Fixes #123`
4. Request review from maintainers
5. Address review feedback
6. Squash commits if requested

## Code Review Checklist

- [ ] Code follows style guide
- [ ] Tests added/updated
- [ ] Documentation updated
- [ ] No memory leaks
- [ ] Error handling implemented
- [ ] Thread safety considered

## Branch Naming

| Type | Format | Example |
|------|--------|---------|
| Feature | `feature/description` | `feature/phase1-local-playback` |
| Bug Fix | `bugfix/description` | `bugfix/decoder-memory-leak` |
| Hotfix | `hotfix/description` | `hotfix/ios-crash` |
| Platform | `feature/platform/feature` | `feature/ios/metal-renderer` |
| Docs | `docs/description` | `docs/api-reference` |
| Refactor | `refactor/description` | `refactor/player-state-machine` |

## Reporting Issues

Use GitHub Issues with templates:

- **Bug Report**: Describe bug, reproduction steps, expected behavior
- **Feature Request**: Describe feature, use case, proposed API
- **Question**: Use Discussions instead

## Security

Report security issues privately to maintainers.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
