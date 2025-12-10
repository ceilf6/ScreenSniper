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

REM 国际化支持
echo 📥 更新 locales 包...
REM 修复 npm 缓存权限问题（Windows通过清理缓存解决）
if exist "%USERPROFILE%\.npm" (
    echo 🧹 清理 npm 缓存...
    call npm cache clean --force
)
call npm install @screensniper/locales
call npm run install-locales
echo.

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
