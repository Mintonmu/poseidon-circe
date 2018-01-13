# poseidon-circe
HTTP + WebSocket load balancer based on Poseidon

### 编译并运行

```sh
git clone https://github.com/lhmouse/poseidon-circe.git
cd poseidon-circe
git submodule init
git submodule update

pushd poseidon
git checkout master && git pull
./reconfig_optimized_debug_cxx11.sh
# 如果已安装 poseidon 可以跳过这一步，直接转到 [1]。
make -j$(nproc)
./makedeb.sh
# [1]
popd

./reconfig_optimized_debug_cxx11.sh
make -j$(nproc)
./run_server.sh
```

### 测试

默认情况下，Circe 侦听于端口 3809。在浏览器中访问 http://server.ip:3809/ 将打开一个包含一行 `Hello World!` 的页面。

使用 WebSocket 访问 ws://server.ip:3809/ 可以测试发送和接收文本消息。


### 定制功能

修改以下两个文件来自定义 HTTP 和 WebSocket 消息的处理行为：

```text
circe-auth/src/user_defined_functions.cpp
circe-box/src/user_defined_functions.cpp
```
