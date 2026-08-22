# Day 28: RedisClient 与真实集成测试

## 本阶段解决的问题

项目已经有本地 L1 LRU，但它不是跨 Worker、跨服务实例共享的缓存。本阶段先实现 Redis L2 的最小客户端封装；尚未接入搜索请求主链路，目的是先独立验证 hiredis、连接超时、TTL 与故障降级的正确性。

## RedisClient 的职责和边界

`RedisClient` 管理一条同步 hiredis 连接：

- `get(key)`：命中时返回完整 String value；不存在、过期或 Redis 不可用时返回 `nullopt`；
- `setEx(key, value, ttl)`：使用单条 `SET key value EX seconds` 命令原子写入 value 和 TTL；
- `disconnect()`：释放失效的 `redisContext`，下次请求再惰性重连；
- 它不是线程安全对象。后续由 Worker 独占连接或连接池借出，不能被多个 Worker 共享，更不能在 EventLoop 回调中进行同步 Redis 网络调用。

`nullopt` 暂时同时表达 Redis miss 和 Redis 不可用。这是有意的 fail-open 语义：二者都回源本地检索，Redis 故障不应让 HTTP 服务不可用。以后若需要监控，再通过独立统计区分 miss、超时和错误。

## hiredis 的核心 API

```cpp
void* redisCommandArgv(redisContext* context,
                       int argc,
                       const char** argv,
                       const size_t* argvlen);
```

选择 `redisCommandArgv` 而非格式化字符串命令，因为 key 和 JSON value 都按“指针 + 长度”传入，能正确处理空格、百分号和二进制数据。

每次命令返回的 `redisReply*` 都必须 `freeReplyObject(reply)`；`redisContext*` 则在连接失效或析构时通过 `redisFree` 释放。二者生命周期不同，不能混淆。

常见 reply：

- `REDIS_REPLY_STRING`：`GET` 命中，使用 `std::string(reply->str, reply->len)` 复制；
- `REDIS_REPLY_NIL`：key 不存在或已过期，这是正常缓存 miss；
- `REDIS_REPLY_STATUS` 且内容为 `OK`：`SET ... EX ...` 成功；
- `REDIS_REPLY_ERROR` 或 `reply == nullptr`：命令/连接失败，释放 reply（若存在）、断开 context、让业务回源。

必须在 `freeReplyObject(reply)` 前完成对 `reply->type`、`reply->str`、`reply->len` 的判断；释放后访问 reply 属于悬空指针错误。`reply->str != nullptr` 是指针有效性检查，不等于 value 不能为空；有效空 String 的长度仍可为 0。

## CMake 与测试

使用：

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED IMPORTED_TARGET hiredis)
target_link_libraries(target PRIVATE PkgConfig::HIREDIS)
```

`PkgConfig::HIREDIS` 将系统 `pkg-config` 提供的 include 路径和 `-lhiredis` 链接参数导入目标。服务器 hiredis 版本为 1.2.0；该版本的 `redisSetTimeout` 按值接收 `timeval`，不能传 `&timeout`。这说明第三方库 API 不能凭记忆使用，应以当前环境的头文件和构建结果验证。

`redis_client_test` 是真实 Redis 集成测试，不是 mock：

1. 非法 TTL 返回 false；
2. `setEx` 写入后 `get` 命中相同 value；
3. 等待 1.2 秒后，1 秒 TTL 的 key 变为 `REDIS_REPLY_NIL`；
4. 连到未监听的 6399 端口时，`get`/`setEx` 正常降级而不抛出异常。

远端 Release 验证：18/18 CTest 通过，`redis_client_test` 约 1.21 秒。

## 面试表达

“我没有把 Redis 调用直接放在 epoll 的 EventLoop 中，因为 hiredis 的同步命令会等待网络回复，阻塞 I/O 线程会拖慢全部连接。RedisClient 当前是非线程安全的单连接对象，后续由 Worker 独占或池化借用；Redis miss 和故障都 fail-open 回源本地索引。缓存写入使用 `SET ... EX` 保证 value 和 TTL 原子设置，reply 与 context 分别释放，避免 C API 资源泄漏。”
