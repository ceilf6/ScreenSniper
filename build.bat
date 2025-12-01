@echo off
echo 🔨 开始编译 ScreenSniper...
echo.

REM 检查 Node.js 是否安装
where node >nul 2>nul
if %errorlevel% neq 0 (
    echo ❌ 未找到 Node.js，无法安装翻译文件
    echo 💡 请先安装 Node.js: https://nodejs.org/
    pause
    exit /b 1
)

REM 检查并安装翻译文件
if exist package.json (
    echo 🌐 检查翻译文件...
    if not exist node_modules (
        echo 📦 首次构建，正在安装依赖...
        call npm install
    )
    call npm run install-locales
    echo.
)

REM 创建构建目录
if not exist build (
    mkdir build
)

cd build

REM 运行 qmake
echo 📝 运行 qmake...
qmake ..\ScreenSniper.pro
if %errorlevel% neq 0 (
    echo ❌ qmake 失败，请检查 Qt 环境配置
    cd ..
    pause
    exit /b 1
)

REM 检查使用的编译器
if exist Makefile.Debug (
    echo 🔧 使用 nmake 编译项目...
    nmake
) else (
    echo 🔧 使用 mingw32-make 编译项目...
    mingw32-make
)

if %errorlevel% equ 0 (
    echo.
    echo ✅ 编译成功！
    echo.
    echo 运行程序：
    if exist debug\ScreenSniper.exe (
        echo   .\build\debug\ScreenSniper.exe
    ) else if exist release\ScreenSniper.exe (
        echo   .\build\release\ScreenSniper.exe
    ) else (
        echo   .\build\ScreenSniper.exe
    )
) else (
    echo ❌ 编译失败，请检查错误信息
    cd ..
    pause
    exit /b 1
)

cd ..
pause
