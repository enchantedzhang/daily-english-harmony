# 快速入门<a name="ZH-CN_TOPIC_0000002520772070"></a>

## 编译构建<a name="ZH-CN_TOPIC_0000002520932058"></a>

### 编译依赖

|硬件依赖|硬件说明|
|--|:-:|
|CPU|Kunpeng-920<br>Kunpeng-920B|
|Architecture|aarch64|
|内存|32GB及以上|

|软件依赖|软件版本|
|--|:-:|
|操作系统|Kunpeng-920<br>Kunpeng-920B|
|CMake|3.22.0|
|GCC|10.3.1|
|JDK|1.8.0_432|

### 源码编译

1. 下载源代码.
    从OpenEuler开源社区下载OmniStateStore的源代码到编译服务器上；
2. 执行编译命令，以编译release包为例：

    ```cmd
    bash scripts/build.sh -t release
    ```

    其它编译选项如下表所示，不同的编译选项可以组合使用。

    | 编译参数  | 编译选项  | 简要说明  |
    | ------------ | ------------ | ------------ |
    | -t  | debug/release  | 编译debug/release包  |
    | --ut  | 无  | 编译UT测试程序  |
    | --sve  | 无  | 使能鲲鹏高性能SVE指令  |
    | -h  | 无  | 帮助  |

3. 检查编译成功的软件包。
编译成功则在目录dist/下存在：
OmniStateStore软件包BoostKit-omnistatestore_1.x.x_aarch64_xxx.tar.gz。

### 开发者测试

1. 执行测试运行脚本。

    ```cmd
    sh test/run_dt.sh
    ```

2. 执行测试运行脚本后会自动编译和测试用例执行，最后观测测试用例执行结果即可。

## 环境部署<a name="ZH-CN_TOPIC_0000002520932058"></a>

环境部署请参见[安装指南](installation_guide.md)。

## 测试验证<a name="ZH-CN_TOPIC_0000002520932058"></a>

1. 进入Flink安装目录下的bin目录，并启动Flink。

    ```cmd
    cd $FLINK_HOME/bin/ && ./start-cluster.sh
    ```

2. 执行“${FLINK_HOME}/examples/streaming/WordCount.jar”示例程序。

    ```cmd
    $FLINK_HOME/bin/flink run $FLINK_HOME/examples/streaming/WordCount.jar
    ```

    观察到Task Manager日志中打印“OmniStateStore service start success.”，说明OmniStateStore启动成功。
