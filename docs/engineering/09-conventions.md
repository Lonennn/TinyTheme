# 编码规范

> 状态：草案 v0.1 ｜ 更新：2026-11-01

## 1. 文件与编码

- 源文件统一 UTF-8（无 BOM），行尾 LF；`.gitattributes` 声明 `* text=auto eol=lf`，图标等二进制文件标注 `binary`。
- 头文件使用 `#pragma once`。
- 文件名小写下划线：`theme_engine.cpp`、`config.h`。
- 单文件不超过 500 行，超过则拆分。

## 2. 命名

| 类别 | 风格 | 示例 |
|---|---|---|
| 类型 / 类 / 枚举 | PascalCase | `ThemeSnapshot`、`Config` |
| 函数 / 方法 | snake_case | `apply`、`set_tooltip` |
| 局部变量 / 参数 | snake_case | `include_system`、`light` |
| 成员变量 | snake_case + 尾下划线 | `window_`、`config_` |
| 常量 | `k` + PascalCase | `kPersonalize`、`kMaxRetry` |
| 命名空间 | 小写 | `theme`、`config`、`tray` |
| 宏 | 全大写 + `TT_` 前缀 | `TT_ASSERT` |
| 资源 ID | `ID` + 类型 + 名称 | `IDI_LIGHT`、`IDS_MENU_EXIT` |

规则由 `.clang-tidy` 的 `readability-identifier-naming` 强制执行，冲突时以配置为准。资源 ID 与 doctest 宏在配置里显式豁免。

Win32 API、标准库与第三方接口沿用其原有命名，不受本表约束。

## 3. 格式

4 空格缩进、Allman、100 列、指针符号靠近类型——具体规则全在 `.clang-format` 里，不在文档里再抄一遍。

提交前执行 `clang-format --style=file -i`（CI 会检查 diff）。

## 4. 错误处理

- 模块边界不抛异常；使用 `bool`、`std::optional`、`std::expected<T, E>` 表达失败，需要携带失败原因时用 `std::expected`。
- 失败时记录 `GetLastError()`，日志中包含函数名与错误码。
- 面向用户的错误只提示一次，文案不包含错误码；错误码写入日志。
- 不使用 `assert` 做运行时校验；`assert` 仅用于开发期不变量。
- 不忽略返回值：`[[nodiscard]]` 标注可能失败的函数。

## 5. 资源管理

- 所有系统句柄使用 RAII 包装（`util.h` 里的 `UniqueHandle` / `UniqueHkey` / `UniqueMenu` / `UniqueIcon`）。
- 禁止裸 `CloseHandle` / `RegCloseKey` 出现在业务代码中。
- `UniqueHandle` 会把 `INVALID_HANDLE_VALUE` 归一化为空：`CreateFileW` 之类的 API 失败时返回的不是 `nullptr`，别自己写 `if (handle != INVALID_HANDLE_VALUE)`。
- 窗口、图标、菜单的销毁顺序在 `App::shutdown` 中集中处理。
- 不使用 `new` / `delete`；需要动态内存时用 `std::make_unique`。

## 6. 字符串

- 内部统一 UTF-16：`std::wstring` / `std::wstring_view`。
- 与 UTF-8 的转换集中在 `util.h`，使用 `WideCharToMultiByte` / `MultiByteToWideChar`。
- 字面量加 `L` 前缀；源码必须带 `/utf-8`。
- 不拼接用户可见文案，统一走资源字符串。

## 7. 注释

- 注释解释"为什么"，不解释"是什么"。
- 公共接口在头文件中用 `//` 简述契约（入参约束、返回值含义、线程要求）。
- 复杂分支、魔数、系统 API 的坑必须写注释。
- `TODO` 格式：`// TODO(name): 原因与期望`。
- 中文注释允许，同一文件内保持一致。

## 8. Git

### 提交信息

Conventional Commits：

```
feat(tray): 左键单击切换主题
fix(theme): 广播后未刷新托盘图标
docs(product): 补充临时覆盖状态机
refactor(config): 抽出 INI 解析
test(solar): 增加极昼极夜用例
chore(ci): 固定 windows-2025 runner
```

- 一个提交只做一件事。
- 标题不超过 72 字符，正文说明动机与影响。
- 不提交生成物、`build/`、`.vs/`。

### 分支

- `main` 为唯一长期分支。
- 功能分支：`feat/tray-icon`、`fix/hotkey-conflict`。
- 合并方式：squash merge，保持主干线性。

### 本地钩子

钩子脚本在 `.githooks/`，克隆后执行一次即可启用：

```
pwsh -File tools/install-hooks.ps1
```

- `pre-commit` 格式：把暂存的 C++ 文件用 clang-format 修好并重新暂存；做不到时（工作区存在未暂存改动、找不到 clang-format）给出明确提示。
- `pre-commit` 空白与冲突标记：`git diff --cached --check`，只报本次新增的行。

钩子只是快速反馈，`git commit --no-verify` 可以绕过；真正阻断合并的是 CI。本地 clang-format 与 CI 镜像版本不一致时以 CI 为准（当前镜像为 20.1.8）。

## 9. 代码评审

评审关注：

- [ ] 是否新增了不必要的依赖或全局状态
- [ ] 系统调用失败是否处理
- [ ] 句柄是否通过 RAII 释放
- [ ] 新增文案是否进入资源表
- [ ] 是否写了只对当前机器成立的特判
- [ ] 是否影响高对比度、多屏、DPI 场景
- [ ] 体积增长是否合理（> 20 KB 需说明）

## 10. 禁止事项

- 不申请管理员权限，不写 `HKLM`。
- 不注入其他进程，不安装全局钩子，不修改系统文件。
- 不重启 `explorer.exe`，不结束用户进程。
- 不发起网络请求，不收集遥测。
- 不修改 `.theme` 文件与 `CurrentTheme`。
- 不在代码中硬编码用户路径或机器名。
- 不使用未文档化 API 作为唯一实现路径（`uxtheme` 相关调用必须带 fallback）。
