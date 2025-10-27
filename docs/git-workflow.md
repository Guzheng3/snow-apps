# Git 工作流规范

本文档定义了项目的 Git 分支管理和提交规范，旨在保持代码库的整洁和可维护性。

## 📋 目录

- [分支规范](#分支规范)
- [Commit 规范](#commit-规范)
- [工作流最佳实践](#工作流最佳实践)
- [自动化工具](#自动化工具)

## 🌿 分支规范

### 主要分支

| 分支 | 说明 | 保护规则 |
|------|------|---------|
| `main` | 主分支，始终保持可发布状态 | 只接受 PR，每次合并打标签 |
| `develop` | 开发分支，包含最新的开发代码 | 作为功能分支的基础 |

### 辅助分支命名规范

| 类型 | 命名格式 | 示例 | 用途 |
|------|---------|------|------|
| 功能 | `feat/YYYYMMDD_描述` | `feat/20251018_add_dark_mode` | 开发新功能 |
| 修复 | `fix/YYYYMMDD_描述` | `fix/20251018_login_error` | 修复 bug |
| 紧急修复 | `hotfix/版本号_描述` | `hotfix/1.2.1_critical_fix` | 生产环境紧急修复 |
| 重构 | `refactor/YYYYMMDD_描述` | `refactor/20251018_optimize_code` | 代码重构 |
| 文档 | `docs/YYYYMMDD_描述` | `docs/20251018_update_readme` | 文档更新 |

### 工作流程图

```
main (生产)     ─────o────────────o────────>
                      ↑            ↑
                   hotfix        release
                                    ↑
develop (开发)   ─o─────o─────o────┴────o──>
                  ↑     ↑     ↑         ↑
                feat   fix  refactor  docs
```

## 📝 Commit 规范

### 格式

```
<type>(<scope>): <subject>

[可选的 body]

[可选的 footer]
```

### Type 类型

| Type | 说明 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat(screenshot): 添加区域截图功能` |
| `fix` | 修复 bug | `fix(draw): 修复颜色选择器显示问题` |
| `docs` | 文档变更 | `docs(readme): 更新安装说明` |
| `style` | 代码格式 | `style: 统一代码缩进` |
| `refactor` | 重构 | `refactor(api): 优化配置接口` |
| `perf` | 性能优化 | `perf(render): 优化渲染性能` |
| `test` | 测试 | `test(utils): 添加工具函数测试` |
| `build` | 构建系统 | `build: 升级 webpack 到 5.0` |
| `ci` | CI 配置 | `ci: 添加自动部署脚本` |
| `chore` | 其他改动 | `chore: 更新依赖包` |
| `revert` | 回滚 | `revert: 回滚 commit abc123` |

### Scope（可选）

表示 commit 影响的范围：
- `screenshot` - 截图功能
- `draw` - 绘图功能
- `ocr` - OCR 功能
- `ui` - UI 组件
- `config` - 配置文件
- `deps` - 依赖更新

### 示例

#### ✅ 好的示例

```bash
feat(screenshot): 添加区域截图功能

fix(draw): 修复绘图工具颜色选择器显示问题

docs(readme): 更新安装说明

refactor(ocr): 重构文本识别模块以提高性能

perf(render): 使用虚拟滚动优化长列表渲染
```

#### ❌ 错误示例

```bash
# 缺少 type
更新了一些文件

# subject 太短不明确
fix: bug

# 首字母大写（应该小写）
Feat: Add feature

# 使用了错误的 type
update(ui): 添加新按钮  # 应该用 feat
```

## 🔄 工作流最佳实践

### 1. 开发新功能

```bash
# 1. 从 develop 创建功能分支
git checkout develop
git pull origin develop
git checkout -b feat/20251018_new_feature

# 2. 开发并提交
git add .
git commit -m "feat(scope): 添加新功能"

# 3. 推送到远程
git push origin feat/20251018_new_feature

# 4. 创建 Pull Request
# 在 GitHub/GitLab 上创建 PR，等待 code review

# 5. 合并后删除分支
git checkout develop
git branch -d feat/20251018_new_feature
git push origin --delete feat/20251018_new_feature
```

### 2. 修复 Bug

```bash
# 从 develop 创建修复分支
git checkout develop
git pull origin develop
git checkout -b fix/20251018_bug_description

# 修复并提交
git add .
git commit -m "fix(scope): 修复具体问题"

# 推送并创建 PR
git push origin fix/20251018_bug_description
```

### 3. 紧急修复生产问题

```bash
# 从 main 创建 hotfix 分支
git checkout main
git pull origin main
git checkout -b hotfix/1.2.1_critical_fix

# 修复并提交
git add .
git commit -m "fix: 修复生产环境严重问题"

# 合并到 main
git checkout main
git merge --no-ff hotfix/1.2.1_critical_fix
git tag -a v1.2.1 -m "Version 1.2.1"
git push origin main --tags

# 也要合并到 develop
git checkout develop
git merge --no-ff hotfix/1.2.1_critical_fix
git push origin develop

# 删除 hotfix 分支
git branch -d hotfix/1.2.1_critical_fix
```

### 4. 提交规范

- ✅ **频繁提交**: 每完成一个小功能就提交
- ✅ **独立提交**: 每个 commit 应该是独立的改动
- ✅ **可运行**: 每次提交后代码都能正常运行
- ✅ **清晰描述**: 让别人能看懂你做了什么
- ❌ **避免**: 一次提交大量不相关的改动

## 🤖 自动化工具

项目已配置以下工具来自动检查和格式化代码：

### Husky

Git hooks 管理工具，在 commit 时自动执行检查。

### Commitlint

检查 commit message 格式，确保符合规范。

配置文件：`commitlint.config.js`

### Lint-staged

在提交前自动格式化暂存的代码。

配置文件：`.lintstagedrc.json`

### EditorConfig

统一编辑器配置，确保所有开发者使用相同的缩进风格和行尾符。

配置文件：`.editorconfig`

### Git Attributes

Git 行尾符处理配置，统一使用 LF (Unix 风格) 行尾符。

配置文件：`.gitattributes`

### 工作流程

```
                     git commit
                         │
                         ↓
                  ┌──────────────┐
                  │  pre-commit  │
                  │   (husky)    │
                  └──────┬───────┘
                         │
                         ↓
                  ┌──────────────┐
                  │ lint-staged  │
                  │ 格式化代码   │
                  └──────┬───────┘
                         │
                         ↓
                  ┌──────────────┐
                  │  commit-msg  │
                  │ (commitlint) │
                  └──────┬───────┘
                         │
                         ↓
                   ✅ 提交成功
```

## ⚙️ 编辑器配置

### 行尾符统一 (LF)

项目已配置统一使用 **LF (Unix 风格)** 行尾符，避免跨平台协作时的问题。

#### 配置文件

1. **VSCode 配置** (`.vscode/settings.json`)
   ```json
   "files.eol": "\n"
   ```
   新建文件会自动使用 LF

2. **EditorConfig** (`.editorconfig`)
   ```
   [*]
   end_of_line = lf
   ```
   支持所有主流编辑器（VSCode、WebStorm、Sublime 等）

3. **Git Attributes** (`.gitattributes`)
   ```
   * text=auto
   *.js text eol=lf
   *.ts text eol=lf
   ```
   确保提交到仓库的文件使用 LF

#### 转换现有文件

##### 方法 1: VSCode 手动转换
1. 打开文件
2. 点击右下角状态栏的 `CRLF` 或 `LF`
3. 选择 `LF`
4. 保存文件 (Ctrl+S)

##### 方法 2: Git 批量转换
```bash
# 让 Git 根据 .gitattributes 规范化所有文件
git add --renormalize .
git status  # 查看哪些文件会被修改

# 提交更改
git commit -m "chore: 统一行尾符为 LF"
```

#### 验证行尾符

```bash
# Windows PowerShell - 检查文件行尾符
(Get-Content -Raw file.js) -match "`r`n"
# True = CRLF, False = LF

# Git Bash / Linux / macOS
file file.js
# 会显示文件的行尾符类型
```

## 🛠️ 常见问题

### Q1: Commit 提交被拒绝怎么办？

**错误提示**: `subject may not be empty`

**解决方法**: 检查 commit message 格式
```bash
# ❌ 错误
git commit -m "fix"

# ✅ 正确
git commit -m "fix(draw): 修复绘图工具显示问题"
```

### Q2: 如何修改最后一次 commit？

```bash
# 修改 commit message
git commit --amend

# 添加遗漏的文件
git add forgotten_file.txt
git commit --amend --no-edit
```

### Q3: 如何撤销 commit？

```bash
# 撤销最后一次 commit，保留改动
git reset --soft HEAD~1

# 撤销最后一次 commit，丢弃改动（谨慎使用！）
git reset --hard HEAD~1
```

### Q4: 代码格式化失败怎么办？

```bash
# 手动运行格式化
pnpm run lint:fix

# 然后重新提交
git add .
git commit -m "fix: 修复问题"
```

### Q5: 如何跳过检查（紧急情况）？

```bash
# 跳过所有 hooks（不推荐）
git commit --no-verify -m "your message"
```

⚠️ **警告**: 只有在完全了解后果的情况下才使用 `--no-verify`

### Q6: 为什么 Git 显示大量文件被修改，但只是行尾符不同？

这通常发生在 Windows 上。解决方法：

```bash
# 1. 配置 Git 不自动转换行尾符
git config core.autocrlf false

# 2. 规范化所有文件
git add --renormalize .

# 3. 提交更改
git commit -m "chore: 统一行尾符为 LF"
```

之后新建的文件会自动使用 LF（由 `.editorconfig` 和 VSCode 配置控制）

## 📚 参考资源

- [Conventional Commits 规范](https://www.conventionalcommits.org/)
- [Git Flow 工作流](https://nvie.com/posts/a-successful-git-branching-model/)
- [语义化版本](https://semver.org/lang/zh-CN/)

## 🎯 快速参考

### 常用命令速查

```bash
# 创建功能分支
git checkout -b feat/$(date +%Y%m%d)_feature_name

# 查看当前分支
git branch

# 提交代码
git add .
git commit -m "feat(scope): description"

# 推送分支
git push origin branch_name

# 删除本地分支
git branch -d branch_name

# 删除远程分支
git push origin --delete branch_name

# 查看提交历史
git log --oneline --graph

# 查看未提交的改动
git status
git diff
```

### Commit Type 速查表

```
feat     → 新功能
fix      → 修 bug
docs     → 文档
style    → 格式
refactor → 重构
perf     → 性能
test     → 测试
build    → 构建
ci       → CI/CD
chore    → 杂项
revert   → 回滚
```

---

**记住**: 好的 Git 习惯能让团队协作更顺畅，代码历史更清晰！ 🚀

