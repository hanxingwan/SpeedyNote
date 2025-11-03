# 使用Ubuntu 24.04作为基础镜像
FROM ubuntu:24.04

# 设置环境变量
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# 安装构建依赖项
RUN apt-get update && apt-get install -y \
    cmake \
    make \
    pkg-config \
    qt6-base-dev \
    qt6-tools-dev \
    libpoppler-qt6-dev \
    libsdl2-dev \
    libasound2-dev \
    dpkg-dev \
    devscripts \
    rpm \
    wget \
    curl \
    git \
    && rm -rf /var/lib/apt/lists/*

# 创建工作目录
WORKDIR /app

# 复制项目文件
COPY . .

# 设置执行权限
RUN chmod +x ./build-package.sh ./build-alpine-arm64.sh

# 构建项目
RUN ./build-package.sh --deb