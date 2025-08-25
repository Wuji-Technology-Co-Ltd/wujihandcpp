# Docker 开发指南

## 安装 Docker

对于 Ubuntu，可直接将以下命令粘贴至终端中运行（已换源为阿里云）：

```bash
sudo apt-get update &&
sudo apt-get -y install ca-certificates curl &&
sudo install -m 0755 -d /etc/apt/keyrings &&
sudo curl -fsSL https://mirrors.aliyun.com/docker-ce/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc &&
sudo chmod a+r /etc/apt/keyrings/docker.asc &&
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://mirrors.aliyun.com/docker-ce/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null &&
sudo apt-get update &&
sudo apt-get -y install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
```

其他发行版用户可自行通过 [References](#references) 中的官方文档安装 Docker。

### （选做）设置无需 sudo

```bash
sudo usermod -aG docker $USER
```

完成后需要重新登录。

### （选做）设置 docker pull 代理

国内用户需要配置代理，否则可能无法拉取镜像。

```bash
sudo mkdir -p /etc/systemd/system/docker.service.d &&
sudo tee /etc/systemd/system/docker.service.d/proxy.conf <<EOF
[Service]
Environment="HTTP_PROXY=http://127.0.0.1:7890/"
Environment="HTTPS_PROXY=http://127.0.0.1:7890/"
EOF
sudo systemctl daemon-reload &&
sudo systemctl restart docker
```

请自行把 `http://127.0.0.1:7890` 改为合适的代理地址。

## 拉取镜像

可从 [DockerHub](https://hub.docker.com/repository/docker/wujitechnology/wujihandcpp-develop) 上获取，镜像支持 amd64 和 arm64 两种指令集架构。

```bash
docker pull wujitechnology/wujihandcpp-develop:latest
```

## 拉取仓库

```bash
git clone https://github.com/Wuji-Technology-Co-Ltd/wujihandcpp.git
cd wujihandcpp
```

## 使用 VSCode 打开仓库


在 [VSCode](https://code.visualstudio.com/) 中安装 [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) 扩展。

将仓库目录用 VSCode 打开，按 Ctrl+Shift+P，点击 Dev Containers: Reopen in Container 选项。

![Dev Containers: Reopen in Container](../images/docker-develop-guide/1.png)

等待容器加载即可。

## References

[Install Docker Engine on Ubuntu - Docker Docs](https://docs.docker.com/engine/install/ubuntu/)