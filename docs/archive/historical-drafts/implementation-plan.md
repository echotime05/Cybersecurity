# 网络安全课程设计实现计划

## 总目标

基于报告实现一个 C++ 版本的四人联机坦克大战安全通信系统。实现重点按优先级排序为：角色进程能独立启动、Kerberos 认证链路可跑通、C/V 证书交换可验证、应用层报文可同步、最后再完善游戏表现。

整体角色分为 `Client`、`AS`、`TGS`、`V`。其中 `V` 同时承担 Kerberos 第三阶段、证书交换和游戏服务器功能。

## 阶段 0：工程骨架与配置

实现内容：

- 建立 C++17 工程结构、CMake 构建文件、四个角色入口。
- 建立统一配置文件，保存实体 ID、Client 密码派生出的 Kc、Ktgs、Kv、CA 公私钥参数、端口。
- 建立公共枚举：`EntityId`、`MsgType`、`ErrorCode`、`AppCode`。
- 建立公共自检程序 `protocol_selftest`。

验收目标：

- `as_server`、`tgs_server`、`v_server`、`client` 四个目标可以编译。
- 每个角色执行 `--self-test` 能读取配置并完成一次报文序列化/反序列化自检。
- `protocol_selftest` 返回 0。

当前状态：已建立初始骨架和公共协议层，并已完成两套构建验收。Qt 自带 MinGW/CMake/Ninja 构建通过，Visual Studio 2022 解决方案 `build-vs/cyber_tank_design.sln` 生成并构建通过。`protocol_selftest` 通过，四个角色的 `--self-test` 均通过。

## 阶段 1：公共报文层

实现内容：

- 完成修订版通用包格式：`msg_type(1B) + src_ID(1B) + dst_ID(1B) + payload_len(4B) + reserved(4B) + payload`。
- 实现 `make_packet`、`parse_packet`、字节序转换、十六进制工具。
- 实现 `MSG_ERROR` payload：`err_code(1B) + err_msg`。
- 实现 `MSG_APP` payload：`app_code(1B) + app_payload`。
- 为每类 Kerberos 报文建立 payload 构造和解析函数。

验收目标：

- 任意 payload 经 `serialize -> parse` 后字段完全一致。
- 非法长度、未知 `msg_type`、未知 `err_code` 能返回明确错误。
- 单元测试覆盖 `AS_REQ`、`AS_REP`、`TGS_REQ`、`TGS_REP`、`V_AUTH_REQ`、`MSG_ERROR`、`MSG_APP`。

## 阶段 2：密码学基础模块

实现内容：

- DES：实现 56 bit key、64 bit block、分组加密、填充、解填充。
- RSA：复用已有 RSA 实验代码中的大整数、Miller-Rabin、模幂、模逆逻辑。
- Hash：先使用教学版 64 bit hash 或标准库包装，后续按老师要求替换。
- Certificate：定义证书结构，包含实体 ID、公钥 `(n,e)`、CA 签名。
- Signature：实现 `sign(hash(M), SK)` 与 `verify(hash(M), PK)`。

验收目标：

- DES 明文经加密再解密能恢复原文。
- 分组 DES 对任意长度 payload 可加密解密。
- RSA 对 64 bit hash 可签名验签。
- 使用 `PK_CA` 能验证每个角色自带证书。

## 阶段 3：角色独立功能

### AS

实现内容：

- 监听 `0.0.0.0:9001`。
- 接收 `AS_REQ`，查配置得到 Client 的 `Kc`。
- 生成 `Kc_tgs` 和 `Ticket_tgs`。
- 返回 `AS_REP`，其中外层用 `Kc` 加密，`Ticket_tgs` 用 `Ktgs` 加密。

验收目标：

- 单独启动 AS 后，测试 Client 能收到并解开 `AS_REP`。
- 错误密码导致 Client 解密失败并进入重新输入流程。
- AS 子线程处理完成后主动关闭连接。

### TGS

实现内容：

- 监听 `0.0.0.0:9002`。
- 接收 `TGS_REQ`，用 `Ktgs` 解开 `Ticket_tgs`。
- 校验 Auth 与 Ticket 身份一致、票据未过期、未重放。
- 生成 `Kc_v` 和 `Ticket_v`。
- 返回 `TGS_REP`，外层用 `Kc_tgs` 加密，`Ticket_v` 用 `Kv` 加密。

验收目标：

- 正常 `TGS_REQ` 能拿到 `Ticket_v` 和 `Kc_v`。
- 身份不一致返回 `ERR_TGS_ID_MISMATCH`。
- 票据过期返回 `ERR_TGT_EXPIRED`。
- 重复 `(ID, TS)` 返回 `ERR_REPLAY_DETECTED`。

### V

实现内容：

- 监听 `0.0.0.0:9003`。
- 按 `msg_type` 路由到 V_AUTH、证书交换或应用层。
- 用 `Kv` 解开 `Ticket_v`，校验 Auth、票据、重放。
- 完成 `V_AUTH_REP`。
- 完成 `CERT_C2V/CERT_V2C`，建立 `ID -> PublicKey` 映射。

验收目标：

- Client 第三阶段认证成功后得到 V 确认。
- 未知 `msg_type` 返回 `ERR_UNSUPPORTED_MSG_TYPE`。
- 证书验签失败时拒绝进入应用层。

### Client

实现内容：

- 加载自身 ID、密码和 Kc。
- 依次执行 AS、TGS、V_AUTH、证书交换。
- 保存 `Kc_tgs`、`Kc_v`、`Ticket_tgs`、`Ticket_v`、V 公钥。
- 对错误码执行阶段回退。

验收目标：

- 输入正确密码能完整跑通认证。
- 密码错误只回退 AS 阶段。
- TGT 过期回退 AS 阶段。
- Ticket_v 过期回退 TGS 阶段。
- 重放错误终止 Client。

## 阶段 4：Kerberos 集成链路

实现内容：

- 将 AS、TGS、V、Client 四个进程真正串起来。
- 每个 Kerberos 阶段结束后断开连接，符合报告约定。
- 增加日志，输出每个阶段的消息类型、发送方、接收方、关键校验结果。

验收目标：

- 依次启动 AS、TGS、V、Client，Client 可以完成完整认证。
- 人为修改 Ticket、Auth、TS 可触发对应错误码。
- 每个阶段失败后只重做必要阶段。

## 阶段 5：双向证书与不可否认

实现内容：

- C/V 互换证书。
- 每个应用层消息支持 `M || sig(hash(M))`。
- ACK 与响应分离，ACK 也携带签名。
- V 和 Client 缓存对方证书验证结果。

验收目标：

- 首次通信验证证书，后续通信直接使用缓存公钥。
- 篡改应用层报文能被签名校验发现。
- 篡改 ACK 能被签名校验发现。

## 阶段 6：应用层事件与 V 权威世界

实现内容：

- 实现 `KEY_DOWN`、`KEY_UP`、`AIM_EVENT`、`FIRE_EVENT`、`GAME_JOIN_REQ`、`GAME_START`、`GAME_STATE`、`APP_ACK`。
- V 网络线程只收包、验签、入队、回 ACK。
- V 游戏线程每 50ms 消费事件队列并更新世界状态。
- 位移按事件到达时间分段计算。

验收目标：

- 单 Client 可以移动、瞄准、开火，V 下发权威状态。
- 50ms 内快速按下又松开不会被吞掉。
- Client 不上报坐标，坐标只由 V 计算。
- `APP_ACK` 用于应用层报文确认和双向不可否认日志展示。

## 阶段 7：四人联机与渲染

实现内容：

- V 等待 C1 到 C4 完成认证和证书交换。
- V 下发 `GAME_START`，包含地图、出生点、玩家编号。
- Client 使用 SDL2 或简单控制台渲染。若课程重点是网络安全，优先实现控制台状态输出；图形界面作为最后增强。
- V 广播 `GAME_STATE` 到所有 Client。

验收目标：

- 四个 Client 同时连接后能收到同一份世界状态。
- 任一 Client 输入会影响所有 Client 看到的状态。
- 子弹、碰撞、生命值、死亡状态由 V 统一判定。

## 阶段 8：演示与报告收尾

实现内容：

- 准备正常登录演示。
- 准备密码错误、票据过期、身份不一致、消息重放、未知消息类型演示。
- 准备篡改签名失败演示。
- 整理代码结构图、线程图、协议表、关键日志截图。

验收目标：

- 一条命令或脚本能启动 AS/TGS/V。
- Client 正常流程和错误流程都有稳定演示步骤。
- 报告中的协议字段与代码常量保持一致。

## 建议执行顺序

1. 先完成阶段 0 到阶段 1，让所有角色能启动并共享协议定义。
2. 再完成阶段 2，密码学模块必须先有自测。
3. 然后按 `AS -> Client AS 阶段 -> TGS -> Client TGS 阶段 -> V_AUTH -> 证书交换` 逐段集成。
4. 最后再做应用层游戏同步，避免图形界面过早干扰安全协议调试。
