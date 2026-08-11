# 补充知识：C++ 构建、头文件与链接

> 范围：这是项目学习中的独立基础补充，不改变主线项目计划。主线在完成性能基线后继续验证 Keep-Alive 优化。

## 构建流程

```text
CMakeLists.txt
-> cmake 配置阶段，生成构建规则
-> make/ninja 调度规则
-> g++ 编译 .cpp 为 .o
-> g++/链接器将 .o 与库链接为可执行文件
-> ctest 运行已注册测试
```

常用命令：

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
./build-release/cpp_search_server 18085 data/docs
```

- `-S` 指定包含 `CMakeLists.txt` 的源代码目录。
- `-B` 指定构建产物目录；可与 Debug 构建目录并存。
- `cmake --build` 委托底层 make/ninja，再由 g++ 完成编译与链接。
- Release 适合性能基线；Debug 适合调试。

## 头文件与实现文件

- `.h` 通常提供声明，即函数签名、类成员和类型接口。
- `.cpp` 通常提供实现细节。
- 调用方只需包含声明，就能独立完成类型检查；它不需要知道实现位置或函数体。
- 链接器根据目标文件中的符号引用和符号定义完成组装。

```text
main.o 需要 SearchService::search
SearchService.o 提供 SearchService::search
-> 链接器连接二者
```

## 增量构建

- 只改 `.cpp` 实现：通常只重编译对应目标文件并重新链接。
- 改公共 `.h` 声明：所有包含该头文件的 `.cpp` 可能需要重编译。

头文件并不保存实现位置，也不直接减少所有编译工作；它通过声明与实现分离支持独立编译、封装和更小的增量构建范围。
