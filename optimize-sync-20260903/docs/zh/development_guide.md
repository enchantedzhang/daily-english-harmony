# 开发指南

## 特性简介

### 背景信息

随着大型计算系统的出现，不同领域的组织以实时方式生成大量数据。Flink是一种开源的分布式计算框架，专为处理大规模数据流而设计，它能够在低时延和高吞吐的场景下高效地处理实时数据。后端状态存储系统在其中扮演着重要角色，当作业状态规模较大时，状态存储系统无法将全部的状态数据存储在内存中，通常会将冷数据存储在大容量磁盘上，但是内存和磁盘两种介质在访问性能上存在巨大的差异，IO读写很容易成为数据处理的瓶颈。比如在计算过程中如果某个算子需要频繁从磁盘上加载状态数据，那么这个算子就很容易成为整个作业吞吐的性能瓶颈。因此后端状态存储系统在很多时候是决定Flink作业性能的决定性因素。

目前开源Flink采用的状态后端包括MemoryStateBackend、FsStateBackend和RocksDBStateBackend，每种stateBackend都有其优缺点，适用于不同的场景。MemoryStateBackend在大规模状态管理中，由于内存限制可能会导致OOM（Out of Memory）错误，并且在大规模任务中从内存中恢复状态速度慢。FsStateBackend依赖于外部存储系统的I/O性能，在高负载或网络延迟高的情况下状态访问速度可能变慢。因此RocksDBStateBackend是当前使用最为广泛的后端存储系统，但是RocksDB作为一个通用的KV存储引擎，并不完全适合流式计算场景，在实际生产使用中会遭遇以下问题：

- RocksDB是一个本地嵌入式数据库，状态访问IO性能差，会消耗大量CPU资源。
- 基于LSM-Tree实现的文件持久化存储中数据顺序批量读写磁盘，查询延迟较大，拖慢整体流计算吞吐率。
- 扩缩并发场景下状态恢复较慢。

互联网应用高速发展，产生的状态数据日益增多，大规模作业下甚至会产生十几到几十TB的状态数据量，那么RocksDBStateBackend的局限性将会被无限放大，因此针对生产环境中遇到的问题，再结合实际大数据应用，推出了高性能状态存储引擎OmniStateStore，旨在解决实际生产使用中遇到的痛点或瓶颈点，更好地适用于大规模Flink作业场景。

### 使用场景

- OmniStateStore是基于Flink状态存储后端标准接口实现的，并且未对Flink做任何侵入式修改，因此支持以JAR包形式平滑替换当前Flink支持的StateBackend。
- 当前Flink广泛应用的状态后端是RocksDBStateBackend，OmniStateStore可以平滑替换它，替换后可以带来端到端的性能提升，性能提升比例则取决于状态后端在整个Flink作业期间的CPU占比，CPU占比越高性能提升越明显。

### 约束限制

- 仅支持在华为鲲鹏算力平台上运行，支持鲲鹏920、920B等服务器类型。
- 适用于Flink 1.16.1、1.16.3和1.17.1三个版本的状态后端，对于其他Flink版本则需要进行相应的接口适配，适配方式请参见[支持更多Flink版本](#支持更多Flink版本)。
- 支持容器规格最小存算比为1:2。

## 编译构建

### 编译依赖

**软件依赖**

**表 1**  软件依赖

|软件|软件版本|
|--|--|
|操作系统|openEuler 20.03<br>openEuler 22.03<br>openEuler 24.03|
|CMake|3.22.0|
|GCC|10.3.1|
|JDK|1.8.0_432|

**硬件依赖**

**表 2**  硬件依赖

<table><tbody><tr><th class="firstcol" valign="top" width="50%" id="mcps1.2.3.1.1"><p>CPU</p>
</th>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.2.3.1.1 "><ul><li>Kunpeng-920</li><li>Kunpeng-920B</li></ul>
</td>
</tr>
<tr><th class="firstcol" valign="top" width="50%" id="mcps1.2.3.2.1"><p>Architecture</p>
</th>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.2.3.2.1 "><p>aarch64</p>
</td>
</tr>
<tr id="row153569203719"><th class="firstcol" valign="top" width="50%" id="mcps1.2.3.3.1"><p>内存</p>
</th>
<td class="cellrowborder" valign="top" width="50%" headers="mcps1.2.3.3.1 "><p>32GB及以上</p>
</td>
</tr>
</tbody>
</table>

### 源码编译

1. 下载源代码。

    从开源社区下载源代码到编译服务器上。

2. 执行编译命令，以编译release包为例。

    ```cmd
    bash scripts/build.sh -t release
    ```

    其它编译选项如下表所示，不同的编译选项可以组合使用。

    **表 1**  编译参数选项

    |编译参数|编译选项|简要说明|
    |--|--|--|
    |-t|debugrelease|编译debug包，开发阶段使用。<br>编译release发布包，测试和发布阶段使用。|
    |--ut|无|编译UT测试程序。|
    |--sve|无|使能鲲鹏高性能SVE指令集。|
    |-h|无|帮助。|

3. 检查编译成功的软件包。

    编译成功则在目录dist/下存在OmniStateStore软件包BoostKit-omnistatestore\_1.x.x\_aarch64\_xxx.tar.gz。

### 测试指导

1. 执行测试运行脚本。

    ```cmd
    sh test/run_dt.sh
    ```

2. 执行测试运行脚本后会自动编译和测试用例执行，查看测试用例执行结果。

## 技术细节

### 总体架构

**核心优势**

状态存储引擎OmniStateStore整体架构上采用以磁盘为主要存储，内存作为Cache的方案，其中内存Cache分为FreshTable和SliceTable两个子模块，磁盘主存为LSMStore模块，其核心优势如下所示：

- FreshTable状态数据采用紧密内存结构的哈希索引，相比传统排序索引性能提升显著。
- SliceTable状态数据按照Hash打散，控制SliceData数据，使能L1 Cache，具有更好的查询性能。
- 基于LSM-Tree重新设计文件格式，改进磁盘数据存储结构，优化大状态场景下的文件读写性能。
- 支持状态数据懒加载功能，大幅提升启动和并行度变更场景的状态恢复性能。

**整体架构**

OmniStateStore的整体架构逻辑图如[图1](#fig4959714185618)所示。

**图 1**  OmniStateStore架构逻辑<a name="fig4959714185618"></a> 

![](figures/OmniStateStore架构逻辑.png "OmniStateStore架构逻辑")

- ShimLayer：OmniStateStore状态存储引擎适配接入层，该模块使用Java编程语言实现。
    - 基于Flink原生stateBackend的基类继承实现了OmniStateStore的一套功能API，未对Flink进行任何侵入式修改。
    - StateTable模块中构建了KvTable、ListTable和MapTable三个表，主要用于进行业务分流，方便处理Flink的五种不同的state类型（value-state、list-state、map-state、reducing-state和aggregation-state）的请求命令。
    - 基于Flink原生序列化器提供了state的Key和Value数据的序列化和反序列化功能，支持后端存储系统进行有效的数据管理和存储。

- OmniStateStore：状态存储加速套件，负责Flink的状态数据的存储加速，对上层提供状态的写入、读取和删除的访问接口，支持增量/全量快照Snapshot功能，支持state状态的故障恢复restore功能，该模块使用C/C++编程语言实现。
    - FreshTable：包含JNI、BinaryHash、MemSegment、Transform和Snapshot五个子功能模块。在MemSegment上以紧密内存结构的哈希索引方式管理状态数据，支持高性能数据查找。当MemorySegment写满后触发后台数据转换，将数据转为Slice格式后写到SliceTable中。
    - SliceTable：包含BinaryMap、SliceBucket、SliceChain、MemoryCompaction和Evictor等共六个子模块。SliceTable按照HashMap和链表List的方式管理状态数据，支持高性能范围查找。当数据量触发淘汰水位启动后台淘汰，首先根据数据冷热算法选择待淘汰的SliceChain，然后将数据写到LsmStore中。
    - LsmStore：包含FileBlock、FileManager、VersionManager、FileCompaction和Snapshot等六个子模块。所有状态文件按照LSM-Tree的结构进行组织管理，在IndexBlock中建立元数据索引，支持高性能的状态数据读写。每一次SliceTable的数据写入生成一个新的状态文件放置到Level 0并更新Version，同时再生成一个Compaction Task加入到后台调度框架，文件Compaction是按照规定的策略从Level 0开始顺序执行。

- Infrastructure：包含内存管理和后台调度框架，内存管理主要给三层模块提供不同容量的内存资源池，既保证内存高速申请/释放，又保证不超过内存限制。调度框架主要控制所有后台任务流程的调度执行。

### 关键技术

#### FreshTable

FreshTable是状态存储引擎的第一层，接收Shim Layout层分发的各个状态类型的请求命令，对于写（Put、Add、AddAll等）请求，正确写到FreshTable的MemorySegment中即可返回调用者Success。FreshTable根据不同的状态类型创新性设计了三个不同类型的紧凑的内存哈希索引结构，包括KV-Table、KMAP-Table和KLIST-Table，巧妙的索引设计即可实现write-in-place（相同key且NewValueSize<=OldValueSize），又支持Append-write，并且兼具高性能的KV状态查找，提升端到端的读性能。

**KV-Table组织结构**

**图 1**  KV-Table内存布局

![](figures/KV-Table内存布局.png "KV-Table内存布局")

**KMAP-Table组织结构**

**图 2**  KMAP-Table内存布局

![](figures/KMAP-Table内存布局.png "KMAP-Table内存布局")

**KLIST-Table组织结构**

**图 3**  KLIST-Table内存布局

![](figures/KLIST-Table内存布局.png "KLIST-Table内存布局")

**Transform流程**

**图 4**  Transform执行步骤

![](figures/Transform执行步骤.png "Transform执行步骤")

- 在初始化流程中，FreshTable对象会从空闲队列中取出一个MemorySegment作为当前可用的队列。
- 上层的写IO会将状态数据写入到Active MemorySegment，写入成功则返回，当MemorySegment剩余空间无法支撑当前写IO，则触发Transform流程。
- 将Active MemorySegment放入待淘汰队列并生成Transform task，再从空闲队列中取出一个MemorySegment作为当前可用的队列，这样就可以继续完成写IO。
- 异步的Transform task则从待淘汰队列中取出MemorySegment执行Transform，将数据写入到下层SliceTable中，Transform完成后将MemorySegment放入到空闲队列中。

**FreshTable技术功能**

OmniStateStore在FreshTable这一层采用的紧密内存哈希表的实现方式，使其状态数据的查找、删除和插入的平均时间复杂度为O\(1\)，最坏时间复杂度O\(logN\)（退化为链表二分查找），而RocksDBStateBackend中MemTable的内部实现结构为SkipList/AVL，虽然实现和维护更加简单，但是其查找、删除和插入的平均时间复杂度为O\(logN\)。

#### SliceTable

SliceTable是状态存储引擎的第二层，同样属于内存Cache，它主要是承接FreshTable中MemorySegment在写满后的Transform IO。Transform流程会将每个状态数据按照一定的hash打散规则（通常使用stateKeyHashCode进行一致性hash打散）重新映射到不同的SliceBucket中，发生冲突时使用SliceChain（链表）按照时间序进行管理，该设计主要用于优化点查和范围查找的性能，同时保证读取SliceData数据时能够尽可能在L1 Cache命中，提升端到端读性能。

**SliceBucket组织结构**

**图 1**  SliceBucket内存布局

![](figures/SliceBucket内存布局.png "SliceBucket内存布局")

**Evict和Compaction流程**

**图 2**  Evict和Compaction流程步骤

![](figures/Evict和Compaction流程步骤.png "Evict和Compaction流程步骤")

Evict流程如下所示.

1. 首先获取到当前的SliceBucketGroup作为待Evict SliceBucketGroup。
2. 从Index=1开始遍历每个SliceBucket下的SliceChain，从前往后（时间序从旧往新）取出状态为Normal的SliceData并标记为evicting。
3. 当SliceData个数和待淘汰数据量都满足设置条件或遍历结束则停止，同时生成Evict Task加入到Evict Executor中。
4. Evict Task处理完成后将SliceData的标记由evicting更新为evicted。

Compaction流程如下所示.

1. 当Transform IO将数据写入到某个SliceChain后生成Compaction Task，并加入到Compaction Executor中。
2. 从后往前（时间序从新往旧）取出状态为Normal的SliceData并标记为compacting。
3. 当达到SliceChain头节点或遭遇状态为evicting、evicted状态的SliceData则停止，同时生成Compaciton Task加入到Compaction Excutor中。
4. Compaciton Task处理完成后将生成的新的SliceData替换掉之前选中的SliceData，被替换的SliceData释放资源。

**SliceTable技术功能**

OmniStateStore在SliceTable这一层采用哈希表+链表（冲突链）的实现方式，Slice为数据存储最小单元，有序且大小尽量不超过20KB，完全可以放在L1 Cache，使得二分查找性能较高，并且哈希表的插入和查找效率也非常高。Evict流程按照数据冷热进行淘汰，能够提高热的状态数据内存命中率，进而提升读性能。而RocksDBStateBackend中的Flush流程是采用FIFO方式进行数据淘汰。

#### LsmStore

LsmStore是状态存储引擎的第三层，使用高性能的NVMeSSD磁盘作为大容量存储层，它主要是承接SliceTable在达成淘汰水位后，根据冷热统计算法得到待淘汰的SliceChain，进而产生Evict IO。Evict流程首先将选择的多个SliceChain上的状态数据进行合并和排序，然后写到LsmStore中，生成一个完整的.sst文件，最后基于LSM-Tree进行分层管理所有的文件。

**Version Manager组织结构**

**图 1**  VersionSet内存布局
![](figures/VersionSet内存布局.png "VersionSet内存布局")

**State File组织结构**

**图 2**  State文件组织布局  
![](figures/State文件组织布局.png "State文件组织布局")

**LsmStore技术功能**

OmniStateStore在LsmStore这一层的State File存放在NVMeSSD磁盘上，在文件中的状态数据通过HashMap+二分查找进行索引。而RocksDBStateBackend中SSTable的file虽然同样存放在磁盘上，但是其文件内的状态数据却是通过二分查找进行索引，其查找效率较低。

### 功能规格

OmniStateStore的主要应用场景是平滑替换Flink原生的RocksDBStateStore状态后端，加速Flink端到端的作业性能，因此罗列出对比RocksDBStateBackend支持的功能规格，旨在指导使用者和开发者能够清楚OmniStateStore的功能规范，便于集成到实际应用生产中。

**表 1**  OmniStateStore功能规格列表

<table style="undefined;table-layout: fixed; width: 987px"><colgroup>
<col style="width: 234px">
<col style="width: 281px">
<col style="width: 236px">
<col style="width: 236px">
</colgroup>
<thead>
  <tr>
    <th>功能分类</th>
    <th>功能点</th>
    <th>RocksDBStateBackend</th>
    <th>OmniStateStore</th>
  </tr></thead>
<tbody>
  <tr>
    <td rowspan="9">基本状态读写API</td>
    <td>Operator State</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>Broadcast State</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>Value State</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>List State</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>Map State</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>Reducing State</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>Aggregating State</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>状态有效期（TTL）</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>计时器（Timer）</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td rowspan="7">Checkpoint</td>
    <td>全量快照</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>增量快照</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>对齐快照</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>非对齐快照</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>普通快照恢复</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>扩缩并行度场景下快照恢复</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>懒加载</td>
    <td>不支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td rowspan="8">Savepoint</td>
    <td>不停作业执行Savepoint</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>停作业执行Savepoint</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>标准格式Savepoint</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>原生格式Savepoint</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>删除Savepoint</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>普通Savepoint恢复</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>扩缩并行度场景下Savepoint恢复</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
  <tr>
    <td>Savepoint支持状态数据结构升级</td>
    <td>支持</td>
    <td>支持</td>
  </tr>
</tbody></table>

## 开发样例

### 支持更多Flink版本<a id="支持更多Flink版本"></a>

OmniStateStore已经支持Flink 1.16.1、1.16.3和1.17.1三个版本，如果使用者或开发者希望支持更多Flink 1.x版本，可以按照如下方式进行适配。

1. 确认共享源代码目录与版本独享目录，src\plugin\state_store_plugin为Java侧对接Flink框架的主要代码目录，以下分为共享源代码目录(src、test)与版本独享目录(flink-1.16.3、flink-1.17.1)。
2. 在src\plugin\state_store_plugin下创建新的版本独享目录，如flink-1.x.x。请根据具体Flink适配新版本进行更改。
3. 修改src\plugin\state_store_plugin\pom.xml，增加新的profile。
4. 将具有版本差异的Java文件放在flink-1.x.x目录下，保证flink-1.x.x目录与共享源代码目录下同名文件只有一份。
5. maven compile编译通过后，修改src\plugin\CmakeLists.txt，增加flink-1.x.x编译目标。

    ```cmd
    set(VERSION_1_x_x flink-1.x.x)
    add_custom_target(build_bss_java_version_1_x_x COMMAND mvn clean package -Drevision=1.1.0-SNAPSHOT -Dmaven.test.skip=true -P${VERSION_1_x_x} -Dmaven.compiler.fork=true -Dmaven.wagon.http.ssl.insecure=true -Dmaven.wagon.http.ssl.allowall=true WORKING_DIRECTORY ${JAVA_BSS_PATH} )
    add_custom_target(build_bss_install_java_version_1_x_x COMMAND mkdir -p ${OUTPUT_PATH}/${BSS_OUTPUT_DIR_NAME}/java/jars COMMAND cp ${JAVA_BSS_PATH}/state_store_all/target/maven-assembly-plugin/flink-boost-statebackend-1.1.0-SNAPSHOT-for-${VERSION_1_x_x}.jar ${OUTPUT_PATH}/${BSS_OUTPUT_DIR_NAME}/java/jars)
    ```

6. 修改源代码目录最外层CmakeLists.txt，增加flink-1.x.x编译。
7. 执行脚本sh scripts/build.sh -t release执行编译。

    生成的tar包里存在flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-1.x.x.jar，即新版本适配完成。

## 问题定位

1. 根据Flink呈现的问题现象展开定位，其主要问题现象大概表现为作业异常Cancel，作业异常Abort，作业Running无法正常结束等。
2. 查看Flink的日志信息，在Flink日志归档路径中找到最新的Log日志文件，在日志文件中搜索ERROR/WARNING关键字可以定位到日志报错点，查看日志报错上下文能够定界是否状态后端异常，从而导致Flink作业中断。
3. 从Flink的日志文件报错点沿时间线查看，可以找到OmniStateStore ShimLayer层的日志打印，查看日志打印上下文是否能够定界OmniStateStore下层模块异常。
4. 查看OmniStateStore日志目录配置项找到日志归档目录（kv.log、kv.\*.log），根据问题发生的时间点找到对应的Log日志文件，在日志文件中搜索ERROR/WARNING关键字，再结合问题时间点可以快速定位到相对应的日志上下文中。
5. 结合日志打印信息和代码逻辑分析问题根因。
