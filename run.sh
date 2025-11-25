#!/bin/bash

echo "🚀 启动 ScreenSniper..."

# 设置 Qt 环境变量
export PATH="/opt/homebrew/opt/qt@5/bin:$PATH"
export LDFLAGS="-L/opt/homebrew/opt/qt@5/lib"
export CPPFLAGS="-I/opt/homebrew/opt/qt@5/include"

# 检查是否已编译
if [ ! -f "build/ScreenSniper.app/Contents/MacOS/ScreenSniper" ]; then
    echo "❌ 未找到可执行文件，请先运行 ./build.sh 编译项目"
    exit 1
fi

# 运行应用
./build/ScreenSniper.app/Contents/MacOS/ScreenSniper
