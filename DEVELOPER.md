# ScreenSniper 开发者指南

## 🚀 快速开始

### 1. 环境准备

确保已安装以下工具：
- Qt 5.15+ 或 Qt 6.x
- Node.js 14+ 和 npm
- C++ 编译器（MSVC 2019+, GCC 8+, 或 Clang 10+）

### 2. 克隆项目

```bash
git clone https://github.com/ceilf6/ScreenSniper.git
cd ScreenSniper
```

### 3. 首次构建

**macOS/Linux:**
```bash
# 一键构建（会自动安装翻译文件）
./build.sh
```

**Windows:**
```bat
REM 一键构建（会自动安装翻译文件）
build.bat
```

**或者手动步骤：**
```bash
# 1. 安装翻译文件
npm install
npm run install-locales

# 2. 编译项目
mkdir build && cd build
qmake ../ScreenSniper.pro
make  # 或 nmake/mingw32-make (Windows)
```

## 📁 项目结构

```
ScreenSniper/
├── main.cpp                 # 程序入口
├── mainwindow.{h,cpp,ui}   # 主窗口
├── screenshotwidget.{h,cpp} # 截图组件
├── pinwidget.{h,cpp}        # Pin图组件
├── ocrmanager.{h,cpp}       # OCR管理器
├── watermark_robust.{h,cpp} # 水印功能
├── macocr.{h,mm}            # macOS OCR (Vision API)
├── resources.qrc            # Qt 资源文件
├── ScreenSniper.pro         # Qt 项目文件
├── locales/                 # 翻译文件（npm 管理）
│   ├── zh.json             # 简体中文
│   ├── en.json             # 英文
│   └── zhHK.json           # 繁体中文
├── icons/                   # 图标资源
├── scripts/                 # 工具脚本
│   ├── install-locales.js  # 翻译文件安装脚本
│   └── publish-locales.sh  # 翻译文件发布脚本
├── build.sh                 # macOS/Linux 构建脚本
└── build.bat                # Windows 构建脚本
```

## 🌐 翻译系统

### 工作原理

1. 翻译文件存储在 npm 包 `@screensniper/locales` 中
2. 运行 `npm install` 下载翻译包到 `node_modules/@screensniper/locales`
3. 运行 `npm run install-locales` 将翻译文件复制到 `locales/` 目录
4. 翻译文件通过 `resources.qrc` 打包到 Qt 资源中
5. 编译时，翻译文件会被嵌入到可执行文件中

### 为什么这样设计？

- ✅ **集中管理**：翻译文件统一在 npm 包中维护，多个项目可共享
- ✅ **版本控制**：通过 npm 版本号管理翻译更新
- ✅ **无运行时依赖**：翻译文件编译到可执行文件，无需外部文件
- ✅ **团队协作**：翻译人员无需了解 C++，只需修改 JSON 文件

### 修改翻译

**方法1：本地修改（临时测试）**

直接修改 `locales/*.json` 文件，然后重新编译。

**方法2：更新 npm 包（正式发布）**

1. 修改翻译文件
2. 发布到 npm（需要权限）
3. 更新 `package.json` 中的版本号
4. 运行 `npm install && npm run install-locales`

### 添加新语言

1. 在 `locales/` 目录添加新的语言文件，如 `ja.json`（日语）
2. 在 `resources.qrc` 中添加：
   ```xml
   <file>locales/ja.json</file>
   ```
3. 在代码中添加语言选项（`mainwindow.cpp`）

## ⚠️ 常见问题

### Q1: 编译时提示 "无法打开语言文件"

**原因**：翻译文件未安装。

**解决**：
```bash
npm install
npm run install-locales
# 然后重新编译
```

### Q2: 修改了翻译文件但没有生效

**原因**：翻译文件被编译到可执行文件中，需要重新编译。

**解决**：
```bash
# 清理构建
rm -rf build
# 重新编译
./build.sh
```

### Q3: Git 提示 locales/ 目录被忽略

**原因**：之前 `.gitignore` 忽略了 `locales/` 目录，现已修复。

**解决**：
```bash
# 强制添加 locales 目录到版本控制
git add -f locales/
git commit -m "Add locales files to version control"
```

### Q4: Windows 上提示 "QSystemTrayIcon::setVisible: No Icon set"

**原因**：系统托盘图标未正确设置。

**解决**：检查 `mainwindow.cpp` 中的 `setupTrayIcon()` 函数，确保设置了图标。

## 🔧 本地配置

如果需要自定义 Tesseract 或 OpenCV 路径：

1. 复制配置文件：
   ```bash
   cp local_config.pri.example local_config.pri
   ```

2. 编辑 `local_config.pri`：
   ```qmake
   # macOS
   macx {
       DEFINES += USE_TESSERACT
       INCLUDEPATH += /opt/homebrew/include
       LIBS += -L/opt/homebrew/lib -ltesseract
   }
   
   # Windows
   win32 {
       DEFINES += USE_TESSERACT
       INCLUDEPATH += C:/Tesseract-OCR/include
       LIBS += -LC:/Tesseract-OCR/lib -ltesseract
   }
   ```

3. 重新编译

## 🐛 调试技巧

### 启用 Qt 调试输出

编译前在 `.pro` 文件中添加：
```qmake
DEFINES += QT_MESSAGELOGCONTEXT
```

### 查看翻译加载情况

运行程序时会在控制台输出：
```
成功加载语言文件: :/locales/zh.json 包含 42 个键
```

### 检查资源文件是否正确打包

```bash
# macOS
strings build/ScreenSniper.app/Contents/MacOS/ScreenSniper | grep "app_title"

# Linux
strings build/ScreenSniper | grep "app_title"

# Windows
findstr "app_title" build\debug\ScreenSniper.exe
```

## 📦 发布构建

### macOS

```bash
./build.sh
# 可执行文件位置：build/ScreenSniper.app
```

创建 DMG：
```bash
# TODO: 添加 DMG 打包脚本
```

### Windows

```bat
build.bat
REM 可执行文件位置：build\debug\ScreenSniper.exe 或 build\release\ScreenSniper.exe
```

使用 windeployqt 打包依赖：
```bat
cd build\release
windeployqt ScreenSniper.exe
```

### Linux

```bash
./build.sh
# 可执行文件位置：build/ScreenSniper
```

创建 AppImage：
```bash
# TODO: 添加 AppImage 打包脚本
```

## 🤝 贡献指南

1. Fork 项目
2. 创建功能分支：`git checkout -b feature/AmazingFeature`
3. 提交更改：`git commit -m 'Add some AmazingFeature'`
4. 推送分支：`git push origin feature/AmazingFeature`
5. 提交 Pull Request

### 代码规范

- 使用 4 空格缩进
- 遵循 Qt 命名约定（驼峰命名）
- 添加适当的注释
- 提交前确保代码能够编译通过

## 📝 许可证

MIT License - 详见 [LICENSE](LICENSE) 文件
