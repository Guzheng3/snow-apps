# Git 规范配置完成指南

## ✅ 已完成的配置

### 1. 安装的依赖包

```json
{
  "@commitlint/cli": "20.1.0",
  "@commitlint/config-conventional": "20.0.0",
  "husky": "9.1.7",
  "lint-staged": "16.2.4"
}
```

### 2. 创建的配置文件

- **commitlint.config.js** - Commit message 格式规范配置
- **.lintstagedrc.json** - Git 提交前代码格式化配置
- **.husky/pre-commit** - 提交前执行 lint-staged
- **.husky/commit-msg** - 提交时检查 commit message 格式

### 3. 创建的文档

- **docs/git-workflow.md** - 完整的 Git 工作流规范
- **docs/git-setup-guide.md** - 快速上手指南  
- **docs/development.md** - 已更新，添加了 Git 规范链接

### 4. 更新的文件

- **package.json** - 已添加 `prepare` 脚本用于自动初始化 husky

## 🚀 如何使用

### 首次使用

其他开发者首次克隆项目后，运行以下命令安装依赖：

```bash
pnpm install
```

这会自动触发 `prepare` 脚本，初始化 husky git hooks。

### 日常开发

#### 1. 创建分支

```bash
# 功能分支
git checkout -b feat/20251018_功能描述

# 修复分支
git checkout -b fix/20251018_bug描述
```

#### 2. 提交代码

```bash
git add .
git commit -m "feat(scope): 添加新功能"
```

**自动执行的检查**：
1. **pre-commit**: 自动格式化暂存的代码（使用 biome）
2. **commit-msg**: 检查 commit message 格式是否符合规范

#### 3. Commit Message 格式

```
<type>(<scope>): <subject>

[可选的详细描述]
```

**Type 类型**：
- `feat` - 新功能
- `fix` - 修复 bug
- `docs` - 文档更新
- `style` - 代码格式
- `refactor` - 重构
- `perf` - 性能优化
- `test` - 测试
- `build` - 构建系统
- `ci` - CI 配置
- `chore` - 其他改动

**示例**：
```bash
git commit -m "feat(screenshot): 添加区域截图功能"
git commit -m "fix(draw): 修复颜色选择器显示问题"
git commit -m "docs(readme): 更新安装说明"
```

### 遇到问题？

#### 问题 1: Commit 被拒绝

```
✖   subject may not be empty [subject-empty]
```

**解决方法**：检查 commit message 格式，确保包含 type 和 subject。

```bash
# ❌ 错误
git commit -m "fix"

# ✅ 正确
git commit -m "fix(draw): 修复绘图工具显示问题"
```

#### 问题 2: 代码格式化失败

手动运行格式化命令：

```bash
pnpm run lint:fix
```

然后重新提交。

#### 问题 3: 紧急情况需要跳过检查

```bash
git commit --no-verify -m "your message"
```

⚠️ **警告**：只有在完全了解后果的情况下才使用此命令。

## 📚 详细文档

- [完整的 Git 工作流规范](./git-workflow.md)
- [开发文档](./development.md)

## 🎯 快速参考

### Commit Type 速查

```
feat     → 新功能
fix      → 修 bug
docs     → 文档
refactor → 重构
perf     → 性能
chore    → 杂项
```

### 常用命令

```bash
# 查看当前分支
git branch

# 创建并切换分支
git checkout -b feat/20251018_new_feature

# 查看状态
git status

# 提交
git add .
git commit -m "feat(scope): description"

# 推送
git push origin branch_name
```

## ✨ 好处

1. **统一的代码风格** - 自动格式化代码，保持一致性
2. **清晰的提交历史** - 规范的 commit message 让历史更易读
3. **更好的协作** - 团队成员遵循相同的规范
4. **自动化检查** - 减少人工审查的工作量

---

**下一步**：阅读 [Git 工作流规范](./git-workflow.md) 了解详细的分支管理和开发流程。

