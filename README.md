# Leizi the Thunderbringer⚡

> 司霆惊蛰 — 为 Electron 打造的高性能 libgit2 原生绑定

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

## 简介

Thunderbringer 是一个基于 [Node-API](https://nodejs.org/api/n-api.html) 的现代 [libgit2](https://libgit2.org/) 绑定，专为 Electron 应用设计。它解决了 NodeGit 在 Electron 9+ 上的兼容性问题，提供 ABI 稳定的原生 Git 操作能力。

### 特性

- **ABI 稳定** — 基于 Node-API，跨 Node.js/Electron 版本无需重编译
- **异步优先** — 耗时操作返回 Promise，不阻塞主线程
- **零依赖** — libgit2 静态链接，无需系统级安装
- **TypeScript 支持** — 完整的类型声明
- **全功能覆盖** — Repository、Index、Commit、Branch、Diff、Remote、Merge、Tag、Stash、Checkout、Config 等

## 安装

```bash
npm install thunderbringer
```

> 安装时会自动编译原生模块，需要 CMake 3.15+ 和 C++ 编译器。

## 快速开始

```javascript
const {
  Repository, Status, Index, Commit, Signature,
  Branch, Diff, Remote, Tag, Stash, Checkout, Config
} = require('thunderbringer');

// 打开仓库
const repo = await Repository.open('/path/to/repo');

// 查看状态
const status = await Status.getStatus(repo);
console.log('Changed files:', status.length);

// 获取当前分支
const branch = Branch.current(repo);
console.log('Current branch:', branch.name);

// 查看 HEAD
const head = repo.head();
console.log('HEAD:', head.oid);

// 释放资源
repo.free();
```

## API 概览

| 模块 | 关键操作 |
|------|----------|
| **Repository** | `open`, `init`, `discover`, `path`, `workdir`, `head`, `state` |
| **Status** | `getStatus`, `getFileStatus` + 状态常量 |
| **Index** | `add`, `addAll`, `remove`, `write`, `writeTree`, `entries` |
| **Commit** | `create`, `lookup` |
| **Signature** | `create`, `createWithTime`, `default` |
| **Revwalk** | `walk` + 排序常量 |
| **Reference** | `lookup`, `resolve`, `list`, `create`, `createSymbolic`, `delete` |
| **Branch** | `create`, `delete`, `list`, `rename`, `lookup`, `current` |
| **Diff** | `indexToWorkdir`, `treeToTree`, `treeToIndex` + delta 常量 |
| **Patch** | `fromWorkdir`, `fromTrees` |
| **Blame** | `file`, `getHunkByLine` |
| **Remote** | `list`, `add`, `delete`, `getUrl`, `fetch`, `push` |
| **Merge** | `analysis`, `merge`, `base` |
| **Tag** | `create`, `createLightweight`, `delete`, `list` |
| **Stash** | `save`, `pop`, `apply`, `drop`, `list` |
| **Checkout** | `head`, `branch`, `reference` |
| **Config** | `get`, `set`, `getInt`, `getBool`, `setInt`, `setBool`, `deleteEntry` |

## 开发

```bash
# 克隆仓库（含 submodule）
git clone --recursive https://github.com/zixiao-labs/Thunderbringer.git
cd Thunderbringer

# 安装依赖并编译
npm install

# 编译（增量）
npm run build

# Debug 构建
npm run build:debug

# 运行测试
npm test

# 清理构建产物
npm run clean
```

### 技术栈

| 组件 | 选择 |
|------|------|
| 绑定层 | Node-API + node-addon-api |
| 构建系统 | cmake-js |
| libgit2 | v1.9.0 (git submodule, 静态链接) |
| C++ 标准 | C++17 |
| 测试框架 | Jest |

### 项目结构

```
thunderbringer/
├── CMakeLists.txt          # CMake 构建配置
├── package.json            # npm 包配置
├── index.js                # 入口：加载 native binding
├── deps/libgit2/           # libgit2 submodule (v1.9.0)
├── src/                    # C++ 原生绑定
│   ├── addon.cc/h          # 模块初始化
│   ├── common/             # 共享基础设施
│   │   ├── guard.h         # RAII 模板
│   │   ├── error.h         # 错误处理
│   │   ├── async_worker.h  # Promise 异步基类
│   │   └── util.h          # OID/签名工具函数
│   ├── repository.cc/h     # Repository ObjectWrap
│   ├── status.cc/h         # 状态查询
│   └── ...                 # 其余模块
├── lib/                    # JavaScript API 层
├── types/                  # TypeScript 声明
└── test/                   # Jest 测试
```

## Electron 使用

```bash
# 使用 @electron/rebuild 重编译
npx @electron/rebuild -m .
```

## 许可证

MIT
