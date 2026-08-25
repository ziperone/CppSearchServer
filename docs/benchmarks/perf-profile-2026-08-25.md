# perf CPU 采样报告（2026-08-25）

## 为什么做这次采样

QPS、P99 和请求分阶段耗时能定位到“排队和跨线程回传明显”，但无法说明 CPU 在具体哪些函数上消耗。此次使用 `perf record` 采样 CPU cycle，确认下一步是否值得优化检索、网络模型、锁，还是响应构造。

## 条件与复现

- Linux 6.17.0-35-generic，Intel Xeon Gold 6230，80 个在线 CPU。
- `RelWithDebInfo` 构建，启用 `-fno-omit-frame-pointer`；无缓存、关闭请求指标。
- 4 个工作线程、4 个网络线程。
- 负载：`wrk -t4 -c256 -d30s --latency http://127.0.0.1:18113/search?q=epoll`。
- 采样：`perf record -F 199 -g -p <服务 PID>`，事件 `cycles:P`。

执行入口：

```bash
cd ~/Desktop/sseattack/CppSearchServer
DURATION=30 bash scripts/profile_cpu.sh
```

普通用户需要 `kernel.perf_event_paranoid <= 1` 才能按当前脚本采样。此前值为 `4` 时脚本会提前失败；临时设为 `1` 后本次采样成功。

## 结果

```text
QPS:       374,351.54
平均延迟:  671.47us
P99:       1.06ms
总请求数:  11,238,631
perf 样本: 44K
丢失样本:  0
```

## 用户态热点

`perf report --no-children --no-call-graph` 的 self overhead 前几项：

```text
5.75%  cfree
4.63%  std::ostream::put
3.75%  _int_free
3.53%  _int_malloc
2.93%  escapeJson
2.67%  malloc
1.69%  pthread_mutex_unlock
1.58%  pthread_mutex_lock
```

调用栈显示这些函数主要落在 JSON 转义、HTTP 响应拼接、短生命周期字符串/回调对象释放，以及工作线程结果经任务队列回到网络线程的路径上。

## 结论与边界

1. 在当前小型内存倒排索引和热点查询下，检索匹配不是主要 CPU 热点；这与请求阶段统计中“工作线程计算约 6us”一致。
2. 响应序列化、内存分配释放、任务回传锁和唤醒构成了轻量请求的主要固定成本；这也解释了为什么单纯增加线程不一定继续提高吞吐。
3. 不能把热点百分比简单相加：`malloc/_int_malloc`、`cfree/_int_free` 是同一调用栈的不同层，`--children` 则是累计占比。
4. 系统仍限制内核符号解析，少量 `[k]` 地址不可读，因此不对它们的具体行为作推断。

## 后续验证方向

1. 对 JSON 响应预留容量、减少流式 `ostream` 写入，重新跑同一脚本验证分配热点和 P99 是否下降。
2. 复查跨线程任务复制和 `std::function` 分配，但不能破坏“网络线程独占连接状态、同一连接顺序返回”的正确性边界。
3. 接入真实密文检索或数据库后重测；计算量变大时，工作线程池的收益和今天的轻量查询结论可能会改变。

完整 `perf.data`、文本报告、wrk 输出与运行日志已单独保留在本地私有面试资料目录，不上传 GitHub，避免仓库携带二进制采样文件。
