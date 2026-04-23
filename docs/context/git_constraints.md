# Git Constraints

## Branch Protection Rules

### Main Branch Protection

`main` 分支已配置以下保护规则：

- **必需 Pull Request 审查**: 至少需要 1 个 approving review
- **自动 dismiss stale reviews**: 当新代码推送时，旧的审查会被自动 dismiss
- **禁止强制推送**: `git push --force` 被拒绝
- **禁止删除分支**: 无法直接删除 main 分支

### 本地保护 (Git Hooks)

#### pre-push Hook
阻止直接推送到 main 分支，强制使用 Pull Request：

```bash
❌ Direct push to 'main' branch is not allowed!

Please create a feature branch and submit a Pull Request instead:

  git checkout -b feature/your-feature-name
  git push -u origin feature/your-feature-name
  gh pr create --title "Your PR title" --body "Your description"
```

#### pre-commit Hook
代码质量检查：
- 检查代码格式（clang-format）
- 阻止提交 `.DS_Store` 文件
- 阻止提交大文件（>10MB）

#### commit-msg Hook
提交信息格式验证，遵循 Conventional Commits 规范：

```
<type>(<scope>): <description>
```

**有效类型**: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`, `build`, `ci`, `perf`

**示例**:
- `feat(player): add progress callback support`
- `fix(audio): resolve buffer underrun issue`
- `docs(api): update Objective-C interface spec`
- `chore: remove build artifacts from git`

**限制**: 首行最多 72 个字符

---

## Workflow

### 标准开发流程

```bash
# 1. 创建功能分支
git checkout -b feature/my-feature

# 2. 开发并提交（遵循 commit-msg 规范）
git add .
git commit -m "feat(player): add new feature"

# 3. 推送分支
git push -u origin feature/my-feature

# 4. 创建 Pull Request
gh pr create --title "feat: add new feature" --body "Description"

# 5. 等待审查并合并
gh pr merge
```

### 紧急修复流程 (Hotfix)

```bash
# 创建 hotfix 分支
git checkout -b hotfix/critical-bug

# 修复并提交
git commit -m "fix: resolve critical bug"

# 推送并创建 PR（同样需要审查）
git push -u origin hotfix/critical-bug
gh pr create --title "hotfix: critical bug" --body "Emergency fix"
```

---

## Branch Naming Convention

| 类型 | 格式 | 示例 |
|------|------|------|
| 功能开发 | `feature/<description>` | `feature/phase1-local-playback` |
| Bug 修复 | `bugfix/<description>` | `bugfix/decoder-memory-leak` |
| 紧急修复 | `hotfix/<description>` | `hotfix/ios-crash` |
| 平台特定 | `feature/<platform>/<feature>` | `feature/ios/metal-renderer` |
| 文档更新 | `docs/<description>` | `docs/api-reference` |
| 重构 | `refactor/<description>` | `refactor/player-state-machine` |

---

## Commit Message Guidelines

### 格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

### 类型说明

| 类型 | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 文档更新 |
| `style` | 代码格式调整（不影响功能） |
| `refactor` | 代码重构 |
| `perf` | 性能优化 |
| `test` | 测试相关 |
| `chore` | 构建/工具链更新 |
| `build` | 构建系统变更 |
| `ci` | CI/CD 配置变更 |

### 范围 (Scope)

可选，用于指定影响的模块：
- `player` - 播放器核心
- `audio` - 音频模块
- `video` - 视频模块
- `decoder` - 解码器
- `renderer` - 渲染器
- `demuxer` - 解封装器
- `encoder` - 编码器
- `platform` - 平台适配层
- `objc` - Objective-C 绑定
- `docs` - 文档

---

## Git Hooks 安装

Git hooks 存储在 `.git/hooks/` 目录下，不会被提交到仓库。如需在团队间共享 hooks，可以：

1. 将 hooks 复制到 `tools/githooks/` 目录
2. 提供安装脚本：

```bash
#!/bin/bash
# tools/install-hooks.sh
cp tools/githooks/* .git/hooks/
chmod +x .git/hooks/*
echo "Git hooks installed successfully"
```

---

## 故障排除

### 绕过 Hooks（仅限紧急情况）

```bash
# 绕过 pre-commit
git commit --no-verify

# 绕过 pre-push
git push --no-verify
```

⚠️ **警告**: 仅在紧急情况下使用，且仍需通过 PR 流程合并到 main。

### Hook 未执行

检查 hook 是否有执行权限：
```bash
ls -la .git/hooks/pre-*
```

如需添加执行权限：
```bash
chmod +x .git/hooks/pre-push .git/hooks/pre-commit .git/hooks/commit-msg
```
