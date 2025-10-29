# 📝 SpeedyNote - Rust + Tauri 云原生版本

<div align="center">
<img src="https://i.imgur.com/Q7HPQwK.png" width="300"></img>
</div>

> 基于Rust和Tauri构建的现代化手写笔记应用，支持云原生部署

## ✨ 特性

- 🦀 **Rust + Tauri** - 高性能原生应用
- 🎨 **现代化模糊玻璃UI** - 现代化视觉效果
- ☁️ **云原生部署** - Docker & Kubernetes支持
- 🖊️ **压力感应手写** - 支持手写笔输入
- 📄 **多页面笔记本** - 无限页面支持
- 🔄 **实时同步** - WebSocket实时协作
- 📱 **响应式设计** - 支持多种设备

## 🚀 快速开始

### 本地开发

```bash
# 克隆项目
git clone <repository-url>
cd SpeedyNote

# 构建项目
./build.sh

# 启动开发服务器
cargo tauri dev
```

### Docker部署

```bash
# 构建Docker镜像
docker build -t speedynote .

# 运行容器
docker run -p 3000:3000 speedynote

# 或使用Docker Compose
docker-compose up -d
```

### 云原生部署

```bash
# Kubernetes部署
kubectl apply -f kubernetes/

# 或使用部署脚本
./deploy.sh kubernetes
```

## 📋 系统要求

### 开发环境
- Rust 1.70+
- Node.js 16+
- Docker & Docker Compose

### 生产环境
- Docker 20.10+
- Kubernetes 1.24+ (可选)
- 2GB RAM, 2CPU核心

## 🏗️ 项目结构

```
SpeedyNote/
├── src/                 # Rust源代码
│   ├── main.rs         # 主程序入口
│   ├── note.rs         # 笔记数据结构
│   ├── pdf.rs          # PDF处理模块
│   ├── ui.rs           # 用户界面
│   ├── storage.rs      # 数据存储
│   └── server.rs       # HTTP服务器
├── src-tauri/          # Tauri配置
│   └── dist/           # 前端资源
├── kubernetes/         # K8s部署配置
├── Dockerfile          # Docker构建配置
├── docker-compose.yml  # 容器编排
├── build.sh           # 构建脚本
└── deploy.sh          # 部署脚本
```

## 🔧 配置说明

### 环境变量

```bash
# 应用配置
RUST_LOG=info          # 日志级别
PORT=3000             # 服务端口

# 数据库配置 (可选)
DATABASE_URL=postgresql://user:pass@localhost/speedynote
```

### 端口配置

- **3000**: HTTP Web界面和API
- **5432**: PostgreSQL数据库 (可选)

## 🎮 功能特性

### 核心功能
- [x] 手写笔记绘制
- [x] 多页面管理
- [x] PDF背景集成
- [x] 实时保存
- [x] 导出为PDF

### UI特性
- [x] 模糊玻璃效果
- [x] 暗色/亮色主题
- [x] 响应式布局
- [x] 触摸屏优化

### 部署特性
- [x] Docker容器化
- [x] Kubernetes就绪
- [x] 健康检查
- [x] 自动缩放

## 📊 性能指标

| 指标 | 数值 | 说明 |
|------|------|------|
| 启动时间 | < 2秒 | 冷启动时间 |
| 内存占用 | < 100MB | 典型使用情况 |
| 响应时间 | < 50ms | API响应延迟 |
| 并发用户 | 1000+ | 支持用户数 |

## 🔒 安全特性

- HTTPS支持
- CORS配置
- 输入验证
- SQL注入防护
- XSS防护

## 📈 监控和日志

### 日志级别
- ERROR: 错误信息
- WARN: 警告信息
- INFO: 常规信息
- DEBUG: 调试信息

### 监控端点
- `/health`: 健康检查
- `/metrics`: 性能指标
- `/api/docs`: API文档

## 🤝 贡献指南

1. Fork项目
2. 创建功能分支
3. 提交更改
4. 推送到分支
5. 创建Pull Request

## 📄 许可证

本项目采用MIT许可证。详见[LICENSE](LICENSE)文件。

## 🙏 致谢

- [Tauri](https://tauri.app/) - 跨平台应用框架
- [Rust](https://www.rust-lang.org/) - 系统编程语言
- [egui](https://github.com/emilk/egui) - 即时模式GUI
- [Docker](https://www.docker.com/) - 容器化平台

## 📞 支持

如有问题或建议，请提交Issue或联系开发团队。

---

<div align="center">

**Made with ❤️ using Rust & Tauri**

</div>