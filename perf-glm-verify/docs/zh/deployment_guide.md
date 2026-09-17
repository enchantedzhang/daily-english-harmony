# 部署指南

## 环境要求

安装OmniStateStore前，请提前准备软硬件安装环境，以确保后续安装操作顺利进行。

### 硬件要求

OmniStateStore软件在Docker容器执行，硬件要求如[表1](#硬件要求)所示。

**表 1**  硬件要求<a id="硬件要求"></a>

<table><tbody><tr><th class="firstcol" valign="top" width="30%"><p>服务器名称</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.1.1 "><p>TaiShan 200服务器</p>
</td>
</tr>
<tr><th class="firstcol" valign="top" width="30%"><p>处理器</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.2.1 "><p>鲲鹏920处理器、鲲鹏920新型号处理器</p>
</td>
</tr>
<tr><th class="firstcol" valign="top" width="30%" id="mcps1.2.3.3.1"><p>内存大小</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.3.1 "><p>256GB以上</p>
</td>
</tr>
<tr><th class="firstcol" valign="top" width="30%"><p>内存频率</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.4.1 "><p>4800 MT/s</p>
</td>
</tr>
<tr><th class="firstcol" valign="top" width="30%" id="mcps1.2.3.5.1"><p>网卡</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.5.1 "><p>NA</p>
</td>
</tr>
<tr><th class="firstcol" valign="top" width="30%" id="mcps1.2.3.6.1"><p>磁盘（NVMe SSD）</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.6.1 "><p>至少一块3.6TB或7.68TB磁盘</p>
</td>
</tr>
</tbody>
</table>

### 软件要求

OmniStateStore软件安装前需要将前置依赖的软件安装成功，建议参考各软件安全标准规范安装，集群中各节点的操作系统和软件要求如[表1](#软件版本)所示。

**表 1**  软件版本<a id="软件版本"></a>

|软件名称|软件版本|
|--|--|
|OS|openEuler 20.03<br>openEuler 22.03<br>openEuler 24.03|
|Java|JDK 1.8.0_432|
|Flink|1.16.1<br>1.16.3<br>1.17.1|

## 安装指导

1. 登录到待安装节点，上传软件包omnistatestore\_1.1.0\_aarch64\_release.tar.gz到“$\{FLINK\_HOME\}/lib/”子目录下。
2. 解压软件包。

    ```cmd
    tar -zxvf omnistatestore_1.1.0_aarch64_release.tar.gz
    ```

    解压后的软件包里包含以下OmniStateStore对应的插件JAR包：

    - flink-boost-statebackend-1.x.x-SNAPSHOT-for-flink-1.16.1.jar
    - flink-boost-statebackend-1.x.x-SNAPSHOT-for-flink-1.16.3.jar
    - flink-boost-statebackend-1.x.x-SNAPSHOT-for-flink-1.17.1.jar

    根据具体的Flink版本选择对应的版本JAR包保留即可，删除其他不需要的JAR包。以保留Flink 1.16.3版本为例：

    ```cmd
    rm -f flink-boost-statebackend-1.x.x-SNAPSHOT-for-flink-1.17.1.jar
    rm -f flink-boost-statebackend-1.x.x-SNAPSHOT-for-flink-1.16.1.jar
    ```

3. 删除软件压缩包。

    ```cmd
    rm -f omnistatestore_1.1.0_aarch64_release.tar.gz
    ```

## 启动软件

1. 配置参数项。

    根据业务使用情况和待安装部署的环境设置Flink的conf子目录下flink-conf.yaml中的相关配置项，OmniStateStore相关配置项说明参考[配置项说明](#配置项说明)。

    以下为使能OmniStateStore特性必须要在$\{FLINK\_HOME\}/conf/flink-conf.yaml中新增或修改的配置项，需要在JobManager和所有TaskManager的配置文件中同时修改。

    **表 1**  同步配置项说明

    <table style="undefined;table-layout: fixed; width: 973px"><colgroup>
    <col style="width: 238px">
    <col style="width: 250px">
    <col style="width: 249px">
    <col style="width: 236px">
    </colgroup>
    <thead>
    <tr>
        <th>配置项名称</th>
        <th>简要描述</th>
        <th>配置示例</th>
        <th>注意事项</th>
    </tr></thead>
    <tbody>
    <tr>
        <td>state.backend</td>
        <td>Flink开源参数，用于配置state.backend状态后端。</td>
        <td>com.huawei.ock.bss.OckDBStateBackendFactory</td>
        <td>此配置用于切换状态后端的类型，需要保证字符完全正确并区分大小写。</td>
    </tr>
    <tr>
        <td>state.backend.ockdb.localdir</td>
        <td>OmniStateStore状态数据本地存储路径。</td>
        <td>/usr/local/flink/ockdb</td>
        <td>保证路径已存在且对Flink运行用户有读写权限。</td>
    </tr>
    <tr>
        <td>state.backend.ockdb.jni.logfile</td>
        <td>OmniStateStore日志路径。</td>
        <td>/usr/local/flink/log/kv.log</td>
        <td>建议配置为Flink的日志目录。</td>
    </tr>
    </tbody>
    </table>

    配置项格式：\$\{配置项名称\} + \$\{英文冒号\} + \$\{空格\} + $\{配置项值\}，如下所示。

    ```cmd
    state.backend: com.huawei.ock.bss.OckDBStateBackendFactory
    state.backend.ockdb.localdir: /usr/local/flink/ockdb
    state.backend.ockdb.jni.logfile: /usr/local/flink/log/kv.log
    ```

2. 启动Flink任务，查看日志中的配置项，检查配置是否成功。
3. 执行“\$\{FLINK\_HOME\}/examples/streaming/WordCount.jar”示例程序，观察到Task Manager日志中打印“OmniStateStore service start success.”，说明OmniStateStore启动成功。

## 卸载软件

1. 将配置的state.backend.ockdb.localdir路径删除。
2. 将“\$\{FLINK\_HOME\}/lib/”目录下的flink-boost-statebackend-1.x.x-SNAPSHOT-for-flink-$\{flink.version\}.jar删除。
3. 将flink-conf.yaml配置文件中的state.backend切换为其他状态后端。

## 相关参考

- **[配置项说明](#配置项说明)**  
OmniStateStore的Log模块、StateStore模块和Metric模块的配置参数规范，涵盖日志管理、状态存储、性能监控等维度，为OmniStateStore在Flink场景下的部署与调优提供参数配置参考。

- **[Metric指标](#Metric指标)**  
OmniStateStore支持对接Flink Metric框架，并提供一系列Metric指标，用于在任务运行过程中监测OmniStateStore的内存占用、缓存命中率等内部运行状态信息，为OmniStateStore在Flink场景下的性能调优与运行状态分析提供了Metric指标参考。

- **[功能规格](#功能规格)**  
对比OmniStateStore与RocksDB作为Flink状态后端在基本状态读写、Checkpoint、Savepoint等核心功能上的支持情况，两者在功能点上均保持一致支持，为用户评估OmniStateStore替代RocksDB的可行性提供参考依据。

### 配置项说明<a id="配置项说明"></a>

OmniStateStore的Log模块、StateStore模块和Metric模块的配置参数规范，涵盖日志管理、状态存储、性能监控等维度，为OmniStateStore在Flink场景下的部署与调优提供参数配置参考。

Log模块、StateStore模块和Metric模块的具体配置项说明请参见[表1](#Log模块配置项说明)、[表2](#StateStore配置项说明)和[表3](#Metric配置项说明)。

**表 1**  Log模块配置项说明<a id="Log模块配置项说明"></a>

|配置项名称|说明|默认值|取值范围|注意事项|
|--|--|--|--|--|
|state.backend.ockdb.jni.logfile|日志路径及日志文件名。|/usr/local/flink/log/kv.log|Flink运行用户具有读写权限的路径下面的文件（要求路径已存在）|保证路径已存在且对Flink运行用户有读写权限。|
|state.backend.ockdb.jni.loglevel|日志级别。<br>1：DEBUG<br>2：INFO<br>3：WARN<br>4：ERROR|2|[1, 4]|无|
|state.backend.ockdb.jni.lognum|最大日志文件个数。|20|[10, 50]|无|
|state.backend.ockdb.jni.logsize|单个日志文件大小。单位MB。|20|[10, 50]|无|

**表 2**  StateStore配置项说明<a id="StateStore配置项说明"></a>

|配置项名称|说明|默认值|取值范围|注意事项|
|--|--|--|--|--|
|state.backend|Flink开源参数，用于配置state.backend状态后端。|无|com.huawei.ock.bss.OckDBStateBackendFactory|保证字符完全正确，区分大小写。|
|state.backend.ockdb.localdir|OmniStateStore本地数据存储路径。|无|已存在且Flink运行用户具有读写权限的路径。|保证路径已存在且对Flink运行用户有读写权限。确保该路径与taskmanager.state.local.root-dirs配置路径在同一个文件系统下。|
|taskmanager.state.local.root-dirs|Flink开源参数，用于配置本地Checkpoint临时目录。|无|已存在且Flink运行用户具有读写权限的路径。|建议配置。如果不配置，默认使用io.tmp.dirs配置的路径。确保该路径与state.backend.ockdb.localdir配置路径在同一个文件系统下。|
|state.backend.ockdb.jni.slice.watermark.ratio|缓存层通过设定高/低水位线比例阈值触发数据淘汰机制，将冷数据按预设策略迁移至LSM文件存储层，实现存储资源动态平衡。|0.8|(0, 1)|一般情况下不需要单独设置。|
|state.backend.ockdb.file.memory.fraction|控制用于读写LSM层数据的内存缓存空间大小占整个DB实例的内存上限的比例。|0.2|[0.1, 0.5]|一般情况下不需要单独设置。|
|state.backend.ockdb.jni.lsmstore.compaction.switch|LSM文件存储层整理合并开关。LSM文件存储层的分层合并机制通过开关控制数据文件的整理与合并操作，以优化存储性能和空间利用率。|1|0：关闭<br>1：开启|建议开启。|
|state.backend.ockdb.zero-copy.switch|大ListState覆盖写场景的SST文件免拷贝复用开关。开启后，在文件数触发的Level 0到Level 1 Compaction中，对满足条件的单记录大PUT文件尝试直接复用原SST；未命中时执行普通合并。|false|false：关闭<br>true：开启|默认关闭。适用于频繁使用ListState.update且单个序列化Value严格大于4 MiB的场景。开启前建议评估目标层文件数量和压缩策略影响。|
|state.backend.ockdb.ttl.filter.switch|TTL过期数据后台压缩清理。|false|false：关闭<br>true：开启|当存在使用TTL State的业务场景时，建议开启。|
|state.backend.ockdb.lsmstore.compression.policy|LsmStore中的各层级Level的压缩策略。state.backend.ockdb.lsmstore.compression.level.policy默认值配合使用。<br>level0：不开启压缩<br>level1：不开启压缩<br>level2：开启lz4压缩<br>其余level：全压缩|lz4|none：不压缩<br>lz4：使用lz4压缩|当Checkpoint文件上传过大时，建议开启。|
|state.backend.ockdb.lsmstore.compression.level.policy|手动配置LSM文件不同level配置压缩策略，默认值为“none,none,lz4”，表示level0不开启压缩，level1不开启压缩，level2开启lz4压缩。|none,none,lz4|none：不压缩<br>lz4：使用lz4压缩|当Checkpoint成为瓶颈时，可适当将压缩策略往低层级提前，默认level层级范围[0, 5]。<br>level0为前台写压缩，建议使用None。<br>其余level为后台压缩。|
|state.backend.ockdb.freshtable.snapshot.compression.policy|FreshTable的Checkpoint快照文件压缩策略。|none|none：不压缩<br>lz4：使用lz4压缩|当FreshTable快照文件上传成为Checkpoint瓶颈时，可配置为lz4以减少上传数据量。压缩会增加CPU开销。|
|state.backend.ockdb.slicetable.snapshot.compression.policy|SliceTable的Checkpoint快照文件压缩策略。|none|none：不压缩<br>lz4：使用lz4压缩|当SliceTable快照文件上传成为Checkpoint瓶颈时，可配置为lz4以减少上传数据量。压缩会增加CPU开销。|
|state.backend.ockdb.lazy.download.switch|从Checkpoint恢复时启动懒加载开关。|false|false：关闭<br>true：开启|当Checkpoint很大时开启，缩短任务恢复为running的所需时间。|
|state.backend.ockdb.bloom.filter.switch|针对状态Key的布隆过滤器开关。|true|false：关闭<br>true：开启|对于存在较多无效key访问的场景建议开启。开启时会增加数十兆字节的内存占用。|
|state.backend.bloom.filter.expected.key.count|单个状态中布隆过滤器需要过滤的key的数量级。|8000000|[1000000, 10000000]|一般情况下不需要单独设置，配置数量越大，布隆过滤器需要占用的内存越多。|
|state.backend.ockdb.cache.filter.and.index.switch|开启LSM层filter与indexBlock使用LRU缓存的开关。|true|false：关闭<br>true：开启|一般情况下不需要单独设置，文件数量大时，频繁读不同文件时建议开启。|
|state.backend.ockdb.cache.filter.and.index.ratio|filter与indexBlock独占缓存占总缓存的内存比，此内存不参与LRU压力过载释放。|0|(0, 1)|一般情况下不需要单独设置。压力过大，filter与indexBlock频繁在缓存中被释放时建议开启。|
|state.backend.ockdb.checkpoint.backup|开启本地恢复时Checkpoint本地备份slice文件的目录。|无|Flink运行用户具有读写权限的路径下面的文件（要求路径已存在）。|在开启本地恢复时需要配置，保证路径已存在且对Flink运行用户有读写权限。|
|state.backend.ockdb.timer-service.factory|控制Flink计时器存储的位置。|OCKDB|OCKDB：持久化存储在状态后端<br>HEAP：存储在JVM堆内存中|当计时器数量较少时，基于堆的计时器可以具有更好的性能。|
|state.backend.ockdb.kv-separate.switch|控制KV分离启用的开关。|false|false：关闭<br>true：开启|Value值比较大时开启KV分离。|
|state.backend.ockdb.kv-separate.threshold|KV分离启用的阈值，大于该值需KV分离。|200|(8, 4294967295)|大于该值的Value会经过KV分离单独存储。|

**表 3**  Metric配置项说明<a id="Metric配置项说明"></a>

|配置项名称|说明|默认值|取值范围|注意事项|
|--|--|--|--|--|
|state.backend.ockdb.metric.enable|Metric功能总开关，开启后OmniStateStore才会采集Metric信息。|false|false：关闭<br>true：开启|此开关打开后，各模块的Metric开关才会生效。|
|state.backend.ockdb.metric.memory|MemoryManager模块Metric信息采集开关。|false|false：关闭<br>true：开启|无|
|state.backend.ockdb.metric.fresh.table|FreshTable模块Metric信息采集开关。|false|false：关闭<br>true：开启|无|
|state.backend.ockdb.metric.slice.table|SliceTable模块Metric信息采集开关。|false|false：关闭<br>true：开启|无|
|state.backend.ockdb.metric.lsm.store|LSM Store模块Metric信息采集开关。|false|false：关闭<br>true：开启|无|
|state.backend.ockdb.metric.lsm.cache|LSM Cache模块Metric信息采集开关。|false|false：关闭<br>true：开启|无|
|state.backend.ockdb.metric.snapshot|Snapshot模块Metric信息采集开关。|false|false：关闭<br>true：开启|无|
|state.backend.ockdb.metric.restore|Restore模块Metric信息采集开关。|false|false：关闭<br>true：开启|无|

### Metric指标<a id="Metric指标"></a>

OmniStateStore支持对接Flink Metric框架，并提供一系列Metric指标，用于在任务运行过程中监测OmniStateStore的内存占用、缓存命中率等内部运行状态信息，为OmniStateStore在Flink场景下的性能调优与运行状态分析提供了Metric指标参考。

用户可以通过Flink WebUI上任务运行时的Metric界面添加并查看这些指标，便于实时了解和分析OmniStateStore的运行表现。

>![](public_sys-resources/icon-note.gif) **说明：**
>
>- 采集Metric数据会产生额外的性能开销，可能对任务的运行性能造成影响，建议仅在任务测试阶段或对性能要求不高的任务中开启Metric功能。
>
>- 所有数据量相关指标的单位为字节，所有耗时相关指标的单位为秒。

**MemoryManager模块<a name="MemoryManager模块"></a>**

**表 1**  Metric指标参考

|Metric指标项|说明|
|--|--|
|ockdb_memory_used_fresh|FreshTable类型的内存使用量|
|ockdb_memory_used_slice|SliceTable类型的内存使用量|
|ockdb_memory_used_file|LSMStore类型的内存使用量|
|ockdb_memory_used_snapshot|Snapshot类型的内存使用量|
|ockdb_memory_used_borrow_heap|BorrowHeap类型的内存使用量|
|ockdb_memory_used_db|单个TaskSlot的托管内存使用总量|
|ockdb_memory_max_fresh|FreshTable类型的内存分配总量|
|ockdb_memory_max_slice|SliceTable类型的内存分配总量|
|ockdb_memory_max_file|LSMStore类型的内存分配总量|
|ockdb_memory_max_snapshot|Snapshot类型的内存分配总量|
|ockdb_memory_max_borrow_heap|BorrowHeap类型的内存分配总量|
|ockdb_memory_max_db|单个TaskSlot的托管内存分配总量|

**FreshTable模块**

**表 2**  Metric指标参考

|Metric指标项|说明|
|--|--|
|ockdb_fresh_hit_count|FreshTable访问命中次数|
|ockdb_fresh_miss_count|FreshTable访问未命中次数|
|ockdb_fresh_record_count|FreshTable访问总记录次数|
|ockdb_fresh_flushing_record_count|FreshTable正在淘汰的KV记录数|
|ockdb_fresh_flushing_segment_count|FreshTable正在淘汰的Segment数量|
|ockdb_fresh_flushed_record_count|FreshTable已淘汰的KV记录数|
|ockdb_fresh_flushed_segment_count|FreshTable已淘汰的Segment数量|
|ockdb_fresh_segment_create_fail_count|FreshTable创建Segment时内存不足导致失败的次数|
|ockdb_fresh_flush_count|FreshTable淘汰数据到SliceTable的总次数|
|ockdb_fresh_binary_key_size|FreshTable中当前所有Key的总大小|
|ockdb_fresh_binary_value_size|FreshTable中当前所有Value的总大小|
|ockdb_fresh_binary_map_node_size|FreshTable中当前所有MapNode的总大小|
|ockdb_fresh_wasted_size|FreshTable的Segment淘汰到SliceTable时，Segment空闲空间累积总大小|

**SliceTable模块**

**表 3**  Metric指标参考

|Metric指标项|说明|
|--|--|
|ockdb_slice_hit_count|SliceTable访问命中次数|
|ockdb_slice_miss_count|SliceTable访问未命中次数|
|ockdb_slice_read_count|SliceTable访问总记录次数|
|ockdb_slice_read_avg_size|请求访问时平均遍历SliceTable中Slice链的长度|
|ockdb_slice_evict_waiting_count|待淘汰的Slice数量|
|ockdb_slice_compaction_count|SliceTable已完成的Compaction任务数|
|ockdb_slice_compaction_slice_count|SliceTable完成Compaction的Slice总数量|
|ockdb_slice_compaction_avg_slice_count|SliceTable每次Compaction任务平均处理的Slice数量|
|ockdb_slice_chain_avg_size|SliceChain的平均长度|
|ockdb_slice_avg_size|单个Slice的平均大小|

**FileCache模块**

**表 4**  Metric指标参考

|Metric指标项|说明|
|--|--|
|ockdb_index_block_hit_count|IndexBlock在BlockCache中的访问命中次数|
|ockdb_index_block_hit_size|IndexBlock在BlockCache访问命中的数据量大小|
|ockdb_index_block_miss_count|IndexBlock在BlockCache访问未命中次数|
|ockdb_index_block_miss_size|IndexBlock在BlockCache访问未命中的数据量大小|
|ockdb_index_block_cache_count|IndexBlock在BlockCache中的缓存个数|
|ockdb_index_block_cache_size|IndexBlock在BlockCache中的缓存大小|
|ockdb_data_block_hit_count|DataBlock在BlockCache访问命中次数|
|ockdb_data_block_hit_size|DataBlock在BlockCache访问命中数据量大小|
|ockdb_data_block_miss_count|DataBlock在BlockCache访问未命中次数|
|ockdb_data_block_miss_size|DataBlock在BlockCache访问未命中数据量大小|
|ockdb_data_block_cache_count|DataBlock在BlockCache中的缓存个数|
|ockdb_data_block_cache_size|DataBlock在BlockCache中的缓存大小|
|ockdb_filter_hit_count|FilterBlock在BlockCache访问命中次数|
|ockdb_filter_hit_size|FilterBlock在BlockCache访问命中的数据量大小|
|ockdb_filter_miss_count|FilterBlock在BlockCache访问未命中次数|
|ockdb_filter_miss_size|FilterBlock在BlockCache访问未命中的数据量大小|
|ockdb_filter_cache_count|FilterBlock在BlockCache中的缓存个数|
|ockdb_filter_cache_size|FilterBlock在BlockCache中的缓存大小|
|ockdb_filter_success_count|FilterBlock对Key过滤结果为不存在的次数|
|ockdb_filter_exist_success_count|FilterBlock对Key过滤结果为存在且实际存在的次数|
|ockdb_filter_exist_fail_count|FilterBlock对Key过滤结果为存在且实际不存在的次数|

**FileStore模块**

**表 5**  Metric指标参考

|Metric指标项|说明|
|--|--|
|ockdb_lsm_flush_count|LSMStore模块Flush到磁盘的文件总数|
|ockdb_lsm_flush_size|LSMStore模块Flush到磁盘的文件数据量总大小|
|ockdb_lsm_compaction_count|LSMStore完成的Compaction任务总次数|
|ockdb_lsm_hit_count|LSMStore访问命中次数|
|ockdb_lsm_miss_count|LSMStore访问未命中次数|
|ockdb_level0_hit_count|LSMStore的Level0层文件访问命中次数|
|ockdb_level0_miss_count|LSMStore的Level0层文件访问未命中次数|
|ockdb_level1_hit_count|LSMStore的Level1层文件访问命中次数|
|ockdb_level1_miss_count|LSMStore的Level1层文件访问未命中次数|
|ockdb_level2_hit_count|LSMStore的Level2层文件访问命中次数|
|ockdb_level2_miss_count|LSMStore的Level2层文件访问未命中次数|
|ockdb_above_level2_hit_count|LSMStore的Level3及以上层文件访问命中次数|
|ockdb_above_level2_miss_count|LSMStore的Level3及以上层文件访问未命中次数|
|ockdb_level0_file_size|LSMStore的Level0层文件数据量总大小|
|ockdb_level1_file_size|LSMStore的Level1层文件数据量总大小|
|ockdb_level2_file_size|LSMStore的Level2层文件数据量总大小|
|ockdb_level3_file_size|LSMStore的Level3层文件数据量总大小|
|ockdb_above_level3_file_size|LSMStore的Level4及以上层文件数据量总大小|
|ockdb_lsm_file_size|LSMStore所有层文件数据量总大小|
|ockdb_lsm_compaction_read_size|LSMStore Compaction任务执行时读取文件的总大小|
|ockdb_lsm_compaction_write_size|LSMStore Compaction任务执行时写入文件的总大小|
|ockdb_level0_compaction_rate|LSMStore的Level0层文件压缩率|
|ockdb_level1_compaction_rate|LSMStore的Level1层文件压缩率|
|ockdb_level2_compaction_rate|LSMStore的Level2层文件压缩率|
|ockdb_level3_compaction_rate|LSMStore的Level3层文件压缩率|
|ockdb_lsm_compaction_rate|LSMStore所有层文件总压缩率|
|ockdb_lsm_file_count|LSMStore所有层文件总数量|

**Snapshot模块**

**表 6**  Metric指标参考

|Metric指标项|说明|
|--|--|
|ockdb_snapshot_total_time|最近一次快照任务执行总耗时|
|ockdb_snapshot_upload_time|最近一次快照任务上传数据耗时|
|ockdb_snapshot_file_count|最近一次快照任务创建的文件数量|
|ockdb_snapshot_file_size|最近一次快照任务创建的文件大小|
|ockdb_snapshot_incremental_size|最近一次快照任务创建的增量文件大小|
|ockdb_snapshot_slice_file_count|最近一次快照任务创建的SliceTable快照文件数量|
|ockdb_snapshot_slice_incremental_file_size|最近一次快照任务创建的SliceTable增量文件大小|
|ockdb_snapshot_slice_file_size|最近一次快照任务创建的SliceTable快照文件大小|
|ockdb_snapshot_sst_file_count|最近一次快照任务创建的LSMStore快照文件数量|
|ockdb_snapshot_sst_incremental_file_size|最近一次快照任务创建的LSMStore增量文件大小|
|ockdb_snapshot_sst_file_size|最近一次快照任务创建的LSMStore快照文件大小|

**Restore模块**

**表 7**  Metric指标参考

|Metric指标项|说明|
|--|--|
|ockdb_restore_time|最近一次快照恢复任务总耗时|
|ockdb_download_time|最近一次快照恢复任务下载耗时|
|ockdb_restore_lazy_download_time|最近一次快照恢复任务懒加载耗时|

### 功能规格

对比OmniStateStore与RocksDB作为Flink状态后端在基本状态读写、Checkpoint、Savepoint等核心功能上的支持情况，两者在功能点上均保持一致支持，为用户评估OmniStateStore替代RocksDB的可行性提供参考依据。

开源Flink使用的RocksDB状态后端功能与OmniStateStore功能对比详见[表1](#状态后端功能对比)。

**表 1**  状态后端功能对比<a id="状态后端功能对比"></a>

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
