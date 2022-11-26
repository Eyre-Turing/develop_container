一个漏洞百出的容器管理方案，写来玩的

# 怎么编译

```bash
make
```

只支持Linux，用了很多Linux的才有的系统调用以及shell调用

# 怎么用

```bash
sudo ./serv	# 开启容器管理服务
```

```bash
sudo ./cli ps			# 查看有哪些正在运行的容器
sudo ./cli run <容器镜像>	# 运行容器
sudo ./cli stop <容器id>	# 停止容器，其中容器id可以在./cli ps查
sudo ./cli off			# 结束容器管理服务
sudo ./cli fg <容器id> <命令>	# 进入指定容器，并且执行<命令>。如果只要进入容器，请传入<命令>为/bin/sh
sudo ./cli bg <容器id> <命令>	# 在指定容器后台执行命令
```

# 怎么生成容器镜像

```bash
sudo ./mkimg.sh <生成的镜像文件名> <大小(单位为M)>
```

# 使用到的工具

busybox-x86_64: https://busybox.net/downloads/binaries/1.31.0-defconfig-multiarch-musl/

```bash
# 直接使用这个命令即可下载
wget https://busybox.net/downloads/binaries/1.31.0-defconfig-multiarch-musl/busybox-x86_64 --no-check-certificate
```

用于创建chroot环境里必要的工具
