# 桌面宠物功能增强计划

## 一、功能概述

基于现有 3 组动画素材（SayHello、Swing、Sleep），新增以下功能：

1. **系统托盘图标 + 右键菜单**（3 个功能项：退出、唤起 Agent、关于）
2. **释放系统内存功能**
3. **窗口关闭时最小化到托盘**

---

## 二、技术方案

### 2.1 系统托盘图标

**类/组件：** `QSystemTrayIcon` + `QMenu` + `QAction`

**托盘图标：** 使用已有资源 `img/icon.png`

**右键菜单结构：**

```
┌─────────────────┐
│  🖥️ 唤起 Agent  │  → 启动终端运行 `deepy tui`
├─────────────────┤
│  🧹 释放内存     │  → 调用 malloc_trim(0) 释放堆内存
├─────────────────┤
│  ℹ️ 关于         │  → 显示 QMessageBox::about()
├─────────────────┤
│  ❌ 退出         │  → QApplication::quit()
└─────────────────┘
```

### 2.2 唤起 Agent（deepy tui）

**技术实现：** `QProcess::startDetached()`

**流程：**
1. 检测可用的终端模拟器（按优先级：`x-terminal-emulator` > `gnome-terminal` > `konsole` > `xterm`）
2. 在终端中执行 `deepy tui`
3. 启动后终端保持打开，显示 TUI 界面

```cpp
// 查找可用终端
QStringList termCandidates = {"x-terminal-emulator", "gnome-terminal", 
                               "konsole", "xfce4-terminal", "xterm"};
// 启动命令
QProcess::startDetached(terminal, {"-e", "bash -c 'deepy tui; exec bash'"});
```

### 2.3 释放系统内存

**技术实现：** Linux glibc `malloc_trim(0)`

**原理：** `malloc_trim(0)` 将堆内存中已释放的空闲块归还给操作系统，减小进程 RSS。

**流程：**
1. 调用 `sync()` 刷新文件系统缓冲区
2. 调用 `malloc_trim(0)` 释放堆内存
3. 通过托盘气泡通知用户"内存已释放"

```cpp
#include <malloc.h>   // for malloc_trim
#include <unistd.h>   // for sync

void releaseSystemMemory() {
    sync();            // 刷新文件系统缓存
    malloc_trim(0);    // 释放堆内存给OS
    trayIcon->showMessage("内存释放", "系统内存已成功释放！", 
                          QSystemTrayIcon::Information, 3000);
}
```

### 2.4 窗口关闭行为

**修改：** 重写 `closeEvent()`，关闭窗口时隐藏到托盘而不是退出

```cpp
void MainWindow::closeEvent(QCloseEvent *event) {
    if (trayIcon->isVisible()) {
        hide();
        event->ignore();  // 阻止真正退出
    }
}
```

---

## 三、文件修改清单

### 3.1 mainwindow.h

| 修改类型 | 内容 |
|---------|------|
| **新增 include** | `QSystemTrayIcon`, `QMenu`, `QAction`, `QProcess`, `QCloseEvent` |
| **新增成员变量** | `QSystemTrayIcon *trayIcon` |
| | `QMenu *trayMenu` |
| | `QAction *actionLaunchAgent` |
| | `QAction *actionFreeMemory` |
| | `QAction *actionAbout` |
| | `QAction *actionExit` |
| **新增 protected** | `void closeEvent(QCloseEvent *event) override` |
| **新增 private 方法** | `void setupTrayIcon()` |

### 3.2 mainwindow.cpp

| 修改位置 | 内容 |
|---------|------|
| **新增 include** | `<QSystemTrayIcon>`, `<QMenu>`, `<QAction>`, `<QProcess>` |
| | `<QMessageBox>`, `<QCloseEvent>` |
| | `<malloc.h>`, `<unistd.h>` |
| **构造函数** | 末尾调用 `setupTrayIcon()` |
| **新增 `setupTrayIcon()`** | 创建托盘图标、构建菜单、连接信号槽 |
| **新增 `closeEvent()`** | 隐藏到托盘 |
| **新增 lambda slot** | 启动 Agent 逻辑 |
| **新增 lambda slot** | 释放内存逻辑 |
| **新增 lambda slot** | 关于对话框 |

### 3.3 main.cpp

无需修改。

### 3.4 desktop_pet.pro

| 修改 |
|------|
| 可能需要添加 `QT += widgets`（如果尚未添加）—— 已存在 |

---

## 四、UI/UX 设计

### 托盘图标交互

| 操作 | 行为 |
|------|------|
| 左键点击托盘图标 | 显示/隐藏宠物窗口 |
| 右键点击托盘图标 | 显示上下文菜单 |
| 窗口关闭按钮 | 最小化到托盘（不退出） |
| 右键菜单「退出」 | 完全退出程序 |

### 关于对话框

```
┌─────────────────────────────┐
│     📌 桌面宠物 v1.0        │
│                             │
│  基于 Qt5 开发的桌面伙伴     │
│  三种状态: SayHello/Swing/  │
│  Sleep                      │
│                             │
│           [确定]             │
└─────────────────────────────┘
```

---

## 五、实施步骤

| 步骤 | 文件 | 操作 |
|------|------|------|
| 1 | `mainwindow.h` | 添加 include 和新成员声明 |
| 2 | `mainwindow.cpp` | 实现 `setupTrayIcon()` 方法 |
| 3 | `mainwindow.cpp` | 实现 `closeEvent()` |
| 4 | `mainwindow.cpp` | 实现各 Action 的 slot 连接 |
| 5 | 编译测试 | `qmake && make` 验证编译通过 |
| 6 | 功能测试 | 验证右键菜单、Agent 启动、内存释放、退出行为 |

---

## 六、依赖项

- Qt5 模块：`QtWidgets`（已包含）
- 系统库：`glibc`（已包含 `malloc.h`）
- 外部命令：`deepy`（需要用户在 PATH 中安装）
- 终端模拟器：系统预装的任意终端（x-terminal-emulator / gnome-terminal 等）