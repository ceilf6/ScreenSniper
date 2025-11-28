#!/bin/bash

echo "🚀 启动 ScreenSniper..."

# 检查是否已编译
if [ ! -f "build/ScreenSniper.app/Contents/MacOS/ScreenSniper" ]; then
    echo "❌ 未找到可执行文件，请先运行 ./build.sh 编译项目"
    exit 1
fi

# 运行应用
open build/ScreenSniper.app
