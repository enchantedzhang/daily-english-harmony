# 安装指南

## 环境要求

安装和使用OmniStateStore之前，请确保软硬件环境已经满足安装部署及应用程序正常运行的要求。

**硬件要求**

OmniStateStore软件在Docker容器中运行，硬件要求如[表1 硬件配置要求](#硬件配置要求)所示。

**表1 硬件配置要求**<a id="硬件配置要求"></a>

<table style="undefined;table-layout: fixed; width: 450px"><colgroup>
<col style="width: 173px">
<col style="width: 277px">
</colgroup>
<thead>
  <tr>
    <th>硬件名称</th>
    <th>配套关系</th>
  </tr></thead>
<tbody>
  <tr>
    <td>处理器</td>
    <td>鲲鹏920系列处理器</td>
  </tr>
  <tr>
    <td>内存大小</td>
    <td>256GB以上</td>
  </tr>
  <tr>
    <td>内存频率</td>
    <td>4800MT/s</td>
  </tr>
  <tr>
    <td>网卡</td>
    <td>NA</td>
  </tr>
  <tr>
    <td>磁盘（NVMe SSD）</td>
    <td>至少一块3.6TB或7.68TB磁盘</td>
  </tr>
</tbody>
</table>

**软件要求**

OmniStateStore软件安装前需要将前置依赖的软件安装成功，建议参考各软件安全标准规范安装，集群中各节点的操作系统和软件要求如[表2 软件要求](#软件要求)所示。

**表2 软件要求**<a id="软件要求"></a>

<table style="undefined;table-layout: fixed; width: 611px"><colgroup>
<col style="width: 165px">
<col style="width: 265px">
<col style="width: 181px">
</colgroup>
<thead>
  <tr>
    <th>软件名称</th>
    <th>软件版本</th>
    <th>获取方式</th>
  </tr></thead>
<tbody>
  <tr>
    <td>OS</td>
    <td>openEuler 22.03 LTS SP3</td>
    <td><a href="https://easysoftware.openeuler.openatom.cn/zh/field?os=openEuler-22.03-LTS-SP3">获取链接</a></td>
  </tr>
  <tr>
    <td>Java</td>
    <td>JDK 1.8.0_432</td>
    <td><a href="https://www.oracle.com/apac/java/technologies/downloads/#java8">获取链接</a></td>
  </tr>
  <tr>
    <td>Flink</td>
    <td><li>1.16.1</li><li>1.16.3</li><li>1.17.1</li><li>1.20.0</li></td>
    <td><a href="https://archive.apache.org/dist/flink/">获取链接</a></td>
  </tr>
</tbody>
</table>

**获取软件包**

**表3 OmniStateStore状态优化软件获取列表**<a id="OmniStateStore状态优化软件获取列表"></a>

<table style="undefined;table-layout: fixed; width: 758px"><colgroup>
<col style="width: 151px">
<col style="width: 220px">
<col style="width: 91px">
<col style="width: 154px">
<col style="width: 142px">
</colgroup>
<thead>
  <tr>
    <th>名称</th>
    <th>包名</th>
    <th>发布类型</th>
    <th>说明</th>
    <th>获取地址</th>
  </tr></thead>
<tbody>
  <tr>
    <td>OmniStateStore状态优化压缩包</td>
    <td>BoostKit-omniruntime-omnistatestore-1.1.0.zip</td>
    <td>开源</td>
    <td>OmniStateStore状态优化软件安装包。</td>
    <td><a href="https://atomgit.com/openeuler/OmniStateStore/releases">获取链接</a></td>
  </tr>
</tbody>
</table>

**验证软件包完整性**

为了防止软件包在传递过程或存储期间被恶意篡改，获取软件包时需下载对应的数字签名文件用于完整性验证。

1. 参见[表3 OmniStateStore状态优化软件获取列表](#OmniStateStore状态优化软件获取列表)获取软件包。
2. 获取《OpenPGP签名验证指南》。
    - 运营商客户：请访问[http://support.huawei.com/carrier/digitalSignatureAction](http://support.huawei.com/carrier/digitalSignatureAction)
    - 企业客户：请访问[https://support.huawei.com/enterprise/zh/tool/pgp-verify-TL1000000054](https://support.huawei.com/enterprise/zh/tool/pgp-verify-TL1000000054)

3. 根据《OpenPGP签名验证指南》进行软件包完整性检查。

    >![](public_sys-resources/icon-note.gif) **说明：** 
    >
    >- 如果校验失败，请不要使用该软件包，先联系华为技术支持工程师解决。
    >- 使用软件包安装或升级之前，也需要按上述过程先验证软件包的数字签名，确保软件包未被篡改。

安装和使用OmniStateStore之前，请确保软硬件环境已经满足安装部署及应用程序正常运行的要求。

## 安装OmniStateStore<a id="安装OmniStateStore"></a>

1. 请参见[表3 OmniStateStore状态优化软件获取列表](#OmniStateStore状态优化软件获取列表)获取软件包BoostKit-omniruntime-omnistatestore-1.1.0.zip。
2. 登录安装节点，并上传软件包BoostKit-omniruntime-omnistatestore-1.1.0.zip至“$\{FLINK\_HOME\}/lib/“子目录下。
3. 解压软件包。

    ```cmd
    unzip BoostKit-omniruntime-omnistatestore-1.1.0.zip
    tar -zxvf BoostKit-omniruntime-omnistatestore-1.1.0.tar.gz
    ```

4. 将Flink版本对应的JAR包复制到“$\{FLINK\_HOME\}/lib/“目录。

    以1.16.3版本为例。

    ```cmd
    cp BoostKit-omnistatestore_1.1.0/java/jars/flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-1.16.3.jar ./
    ```

5. 为了不占用磁盘空间，请执行以下命令删除软件包。

    ```cmd
    rm -f BoostKit-omniruntime-omnistatestore-1.1.0.tar.gz
    rm -f BoostKit-omniruntime-omnistatestore-1.1.0.zip
    rm -rf BoostKit-omnistatestore_1.1.0/
    ```

## 启动OmniStateStore

本章节介绍如何启动OmniStateStore服务，以启用Flink状态存储的加速功能。

1. 根据业务使用情况和待安装部署的环境，设置Flink的conf子目录下flink-conf.yaml中的相关配置项。

    配置项格式为_$\{配置项名称\} + \$\{英文冒号\} + \$\{空格\} + $\{配置项值\}_。OmniStateStore相关配置项说明请参见[配置项说明](#配置项说明)。以下为不同场景的配置项示例说明。

    - 使能OmniStateStore特性必须要在“$\{FLINK\_HOME\}/conf/flink-conf.yaml“中新增或修改的配置项，需要在Job Manager和所有Task Manager的配置文件中同步进行修改。

        **表1 同步配置项说明**<a id="同步配置项说明"></a>

        <table style="undefined;table-layout: fixed; width: 745px"><colgroup>
        <col style="width: 154px">
        <col style="width: 225px">
        <col style="width: 93px">
        <col style="width: 273px">
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

        配置项示例如下所示。

        ```cmd
        state.backend: com.huawei.ock.bss.OckDBStateBackendFactory
        state.backend.ockdb.localdir: /usr/local/flink/ockdb
        state.backend.ockdb.jni.logfile: /usr/local/flink/log/kv.log
        ```

    - 开启Priority Queue持久化存储

        配置项示例如下所示。

        ```cmd
        state.backend.ockdb.timer-service.factory: OCKDB
        ```

    - 开启KV分离存储

        配置项示例如下所示。

        ```cmd
        state.backend.ockdb.kv-separate.switch: true
        state.backend.ockdb.kv-separate.threshold: 200
        ```

2. 执行以下命令，创建必要的目录。

    示例中，state.backend.ockdb.localdir配置为“/usr/local/flink/ockdb“，state.backend.ockdb.checkpoint.backup配置为“/usr/local/flink/checkpoint/backup“，安装时请以实际配置为准。

    ```cmd
    mkdir -p /usr/local/flink/ockdb
    mkdir -p /usr/local/flink/checkpoint/backup
    ```

3. 启动Flink任务，查看日志中的配置项，检查配置是否成功。
4. 执行“$\{FLINK\_HOME\}/examples/streaming/WordCount.jar“示例应用程序。

    观察到Task Manager日志中打印“OmniStateStore service start success.”，说明OmniStateStore启动成功。

## 维护特性

升级或卸载OmniStateStore时请满足操作规范。

**升级软件**

请参见[安装OmniStateStore](#安装OmniStateStore)，在安装节点使用新版本的JAR包替换原有JAR包，无需卸载旧版本。

**卸载软件**

>![](public_sys-resources/icon-notice.gif) **须知：** 
>当前步骤仅供需要卸载OmniStateStore时参考，不属于部署OmniStateStore的必要操作步骤。

1. 在安装节点将配置的state.backend.ockdb.localdir路径删除。
2. 将“\$\{FLINK\_HOME\}/lib/“目录下的flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-$\{flink._x.x.x_\}.jar删除。

    ```cmd
    rm -f flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-x.x.x.jar
    ```

3. 将flink-conf.yaml配置文件中的state.backend切换为其他状态后端。

## 相关参考

### 配置项说明<a id="配置项说明"></a>

OmniStateStore的Log模块、StateStore模块和Metric模块的配置参数规范，涵盖日志管理、状态存储、性能监控等维度，为OmniStateStore在Flink场景下的部署与调优提供参数配置参考。

Log模块、StateStore模块和Metric模块的具体配置项说明请参见[表1 Log模块配置项说明](#Log模块配置项说明)、[表2 StateStore配置项说明](#StateStore配置项说明)和[表3 Metric配置项说明](#Metric配置项说明)。

**表1 Log模块配置项说明**<a id="Log模块配置项说明"></a>

<table style="undefined;table-layout: fixed; width: 1046px"><colgroup>
<col style="width: 155px">
<col style="width: 181px">
<col style="width: 93px">
<col style="width: 274px">
<col style="width: 343px">
</colgroup>
<thead>
  <tr>
    <th>配置项名称</th>
    <th>说明</th>
    <th>默认值</th>
    <th>合法值/区间</th>
    <th>注意事项</th>
  </tr></thead>
<tbody>
  <tr>
    <td>state.backend.ockdb.jni.logfile</td>
    <td>日志路径及日志文件名。</td>
    <td>/usr/local/flink/log/kv.log</td>
    <td>Flink运行用户具有读写权限的路径下面的文件（要求路径已存在）</td>
    <td>保证路径已存在且对Flink运行用户有读写权限。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.jni.loglevel</td>
    <td>日志级别。<li>1：DEBUG</li><li>2：INFO</li><li>3：WARN</li><li>4：ERROR</li></td>
    <td>2</td>
    <td>[1, 4]</td>
    <td>无特别说明</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.jni.lognum</td>
    <td>最大日志文件个数。</td>
    <td>20</td>
    <td>[10, 50]</td>
    <td>无特别说明</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.jni.logsize</td>
    <td>单个日志文件大小。单位MB。</td>
    <td>20</td>
    <td>[10, 50]</td>
    <td>无特别说明</td>
  </tr>
</tbody>
</table>

**表2 StateStore配置项说明**<a id="StateStore配置项说明"></a>

<table style="undefined;table-layout: fixed; width: 1339px"><colgroup>
<col style="width: 198px">
<col style="width: 232px">
<col style="width: 120px">
<col style="width: 350px">
<col style="width: 439px">
</colgroup>
<thead>
  <tr>
    <th>配置项名称</th>
    <th>说明</th>
    <th>默认值</th>
    <th>合法值/区间</th>
    <th>注意事项</th>
  </tr></thead>
<tbody>
  <tr>
    <td>state.backend</td>
    <td>Flink开源参数，用于配置state.backend状态后端。</td>
    <td>无</td>
    <td>com.huawei.ock.bss.OckDBStateBackendFactory</td>
    <td>保证字符完全正确，区分大小写。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.localdir</td>
    <td>OmniStateStore本地数据存储路径。</td>
    <td>无</td>
    <td>已存在且Flink运行用户具有读写权限的路径。</td>
    <td>保证路径已存在且对Flink运行用户有读写权限。确保该路径与taskmanager.state.local.root-dirs配置路径在同一个文件系统下。</td>
  </tr>
  <tr>
    <td>taskmanager.state.local.root-dirs</td>
    <td>Flink开源参数，用于配置本地Checkpoint临时目录。</td>
    <td>无</td>
    <td>已存在且Flink运行用户具有读写权限的路径。</td>
    <td>建议配置。如果不配置，默认使用io.tmp.dirs配置的路径。确保该路径与state.backend.ockdb.localdir配置路径在同一个文件系统下。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.savepoint.sort.local.dir</td>
    <td>使用savepoint功能时需要配置，创建savepoint过程产生的用于排序的临时文件存放路径。</td>
    <td>/usr/local/flink/savepoint/tmp</td>
    <td>已存在且Flink运行用户具有读写权限的路径。</td>
    <td>保证路径已存在且对Flink运行用户有读写权限。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.jni.slice.watermark.ratio</td>
    <td>缓存层通过设定高/低水位线比例阈值触发数据淘汰机制，将冷数据按预设策略迁移至LSM文件存储层，实现存储资源动态平衡。</td>
    <td>0.8</td>
    <td>(0, 1)</td>
    <td>一般情况下不需要单独设置。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.file.memory.fraction</td>
    <td>控制用于读写LSM层数据的内存缓存空间大小占整个DB实例的内存上限的比例。</td>
    <td>0.2</td>
    <td>[0.1, 0.5]</td>
    <td>一般情况下不需要单独设置。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.jni.lsmstore.compaction.switch</td>
    <td>LSM文件存储层整理合并开关。LSM文件存储层的分层合并机制通过开关控制数据文件的整理与合并操作，以优化存储性能和空间利用率。</td>
    <td>1</td>
    <td>0：关闭<br>1：开启</td>
    <td>建议开启。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.zero-copy.switch</td>
    <td>大ListState覆盖写场景的SST文件免拷贝复用开关。开启后，在文件数触发的Level 0到Level 1 Compaction中，对满足条件的单记录大PUT文件尝试直接复用原SST；未命中时执行普通合并。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>默认关闭。适用于频繁使用ListState.update且单个序列化Value严格大于4 MiB的场景。开启前建议评估目标层文件数量和压缩策略影响。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.ttl.filter.switch</td>
    <td>TTL过期数据后台压缩清理。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>当存在使用TTL State的业务场景时，建议开启。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.lsmstore.compression.policy</td>
    <td>LsmStore中的各层级Level的压缩策略。state.backend.ockdb.lsmstore.compression.level.policy默认值配合使用。<br>level0：不开启压缩<br>level1：不开启压缩<br>level2：开启lz4压缩<br>其余level：全压缩</td>
    <td>lz4</td>
    <td>none：不压缩<br>lz4：使用lz4压缩</td>
    <td>当Checkpoint文件上传过大时，建议开启。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.lsmstore.compression.level.policy</td>
    <td>手动配置LSM文件不同level配置压缩策略，默认值为“none,none,lz4”，表示level0不开启压缩，level1不开启压缩，level2开启lz4压缩。</td>
    <td>none,none,lz4</td>
    <td>none：不压缩<br>lz4：使用lz4压缩</td>
    <td>当Checkpoint成为瓶颈时，可适当将压缩策略往低层级提前，默认level层级范围[0, 5]。<br>level0为前台写压缩，建议使用None。<br>其余level为后台压缩。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.freshtable.snapshot.compression.policy</td>
    <td>FreshTable的Checkpoint快照文件压缩策略。</td>
    <td>none</td>
    <td>none：不压缩<br>lz4：使用lz4压缩</td>
    <td>当FreshTable快照文件上传成为Checkpoint瓶颈时，可配置为lz4以减少上传数据量。压缩会增加CPU开销。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.slicetable.snapshot.compression.policy</td>
    <td>SliceTable的Checkpoint快照文件压缩策略。</td>
    <td>none</td>
    <td>none：不压缩<br>lz4：使用lz4压缩</td>
    <td>当SliceTable快照文件上传成为Checkpoint瓶颈时，可配置为lz4以减少上传数据量。压缩会增加CPU开销。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.lazy.download.switch</td>
    <td>从Checkpoint恢复时启动懒加载开关。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>当Checkpoint很大时开启，缩短任务恢复为running的所需时间。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.bloom.filter.switch</td>
    <td>针对状态Key的布隆过滤器开关。</td>
    <td>true</td>
    <td>false：关闭<br>true：开启</td>
    <td>对于存在较多无效key访问的场景建议开启。开启时会增加数十兆字节的内存占用。</td>
  </tr>
  <tr>
    <td>state.backend.bloom.filter.expected.key.count</td>
    <td>单个状态中布隆过滤器需要过滤的key的数量级。</td>
    <td>8000000</td>
    <td>[1000000, 10000000]</td>
    <td>一般情况下不需要单独设置，配置数量越大，布隆过滤器需要占用的内存越多。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.cache.filter.and.index.switch</td>
    <td>开启LSM层filter与indexBlock使用LRU缓存的开关。</td>
    <td>true</td>
    <td>false：关闭<br>true：开启</td>
    <td>一般情况下不需要单独设置，文件数量大时，频繁读不同文件时建议开启。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.cache.filter.and.index.ratio</td>
    <td>filter与indexBlock独占缓存占总缓存的内存比，此内存不参与LRU压力过载释放。</td>
    <td>0</td>
    <td>(0, 1)</td>
    <td>一般情况下不需要单独设置。压力过大，filter与indexBlock频繁在缓存中被释放时建议开启。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.checkpoint.backup</td>
    <td>开启本地恢复时Checkpoint本地备份slice文件的目录。</td>
    <td>无</td>
    <td>Flink运行用户具有读写权限的路径下面的文件（要求路径已存在）。</td>
    <td>在开启本地恢复时需要配置，保证路径已存在且对Flink运行用户有读写权限。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.timer-service.factory</td>
    <td>控制Flink计时器存储的位置。</td>
    <td>OCKDB</td>
    <td>OCKDB：持久化存储在状态后端<br>HEAP：存储在JVM堆内存中</td>
    <td>当计时器数量较少时，基于堆的计时器可以具有更好的性能。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.kv-separate.switch</td>
    <td>控制KV分离启用的开关。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>Value值比较大时开启KV分离。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.kv-separate.threshold</td>
    <td>KV分离启用的阈值，大于该值需KV分离。</td>
    <td>200</td>
    <td>(8, 4294967295)</td>
    <td>大于该值的Value会经过KV分离单独存储。</td>
  </tr>
</tbody></table>

**表3 Metric配置项说明**<a id="Metric配置项说明"></a>

<table style="undefined;table-layout: fixed; width: 1329px"><colgroup>
<col style="width: 196px">
<col style="width: 231px">
<col style="width: 119px">
<col style="width: 347px">
<col style="width: 436px">
</colgroup>
<thead>
  <tr>
    <th>配置项名称</th>
    <th>说明</th>
    <th>默认值</th>
    <th>合法值/区间</th>
    <th>注意事项</th>
  </tr></thead>
<tbody>
  <tr>
    <td>state.backend.ockdb.metric.enable</td>
    <td>Metric功能总开关，开启后OmniStateStore才会采集Metric信息。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>此开关打开后，各模块的Metric开关才会生效。</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.metric.memory</td>
    <td>MemoryManager模块Metric信息采集开关。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>无特别说明</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.metric.fresh.table</td>
    <td>FreshTable模块Metric信息采集开关。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>无特别说明</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.metric.slice.table</td>
    <td>SliceTable模块Metric信息采集开关。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>无特别说明</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.metric.lsm.store</td>
    <td>LSM Store模块Metric信息采集开关。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>无特别说明</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.metric.lsm.cache</td>
    <td>LSM Cache模块Metric信息采集开关。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>无特别说明</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.metric.snapshot</td>
    <td>Snapshot模块Metric信息采集开关。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>无特别说明</td>
  </tr>
  <tr>
    <td>state.backend.ockdb.metric.restore</td>
    <td>Restore模块Metric信息采集开关。</td>
    <td>false</td>
    <td>false：关闭<br>true：开启</td>
    <td>无特别说明</td>
  </tr>
</tbody></table>

OmniStateStore的Log模块、StateStore模块和Metric模块的配置参数规范，涵盖日志管理、状态存储、性能监控等维度，为OmniStateStore在Flink场景下的部署与调优提供参数配置参考。

### Metric指标

OmniStateStore支持对接Flink Metric框架，并提供一系列Metric指标，用于在任务运行过程中监测OmniStateStore的内存占用、缓存命中率等内部运行状态信息，为OmniStateStore在Flink场景下的性能调优与运行状态分析提供了Metric指标参考。

用户可以通过Flink WebUI上任务运行时的Metric界面添加并查看这些指标，便于实时了解和分析OmniStateStore的运行表现。

>![](public_sys-resources/icon-notice.gif) **须知：** 
>
>- 采集Metric数据会产生额外的性能开销，可能对任务的运行性能造成影响，建议仅在任务测试阶段或对性能要求不高的任务中开启Metric功能。
>- 所有数据量相关指标的单位为字节，所有耗时相关指标的单位为秒。

**MemoryManager模块**

**表1 Metric指标参考**<a id="Metric指标参考"></a>

<table style="undefined;table-layout: fixed; width: 559px"><colgroup>
<col style="width: 286px">
<col style="width: 273px">
</colgroup>
<thead>
  <tr>
    <th>Metric指标项</th>
    <th>说明</th>
  </tr></thead>
<tbody>
  <tr>
    <td>ockdb_memory_used_fresh</td>
    <td>FreshTable类型的内存使用量</td>
  </tr>
  <tr>
    <td>ockdb_memory_used_slice</td>
    <td>SliceTable类型的内存使用量</td>
  </tr>
  <tr>
    <td>ockdb_memory_used_file</td>
    <td>LSMStore类型的内存使用量</td>
  </tr>
  <tr>
    <td>ockdb_memory_used_snapshot</td>
    <td>Snapshot类型的内存使用量</td>
  </tr>
  <tr>
    <td>ockdb_memory_used_borrow_heap</td>
    <td>BorrowHeap类型的内存使用量</td>
  </tr>
  <tr>
    <td>ockdb_memory_used_db</td>
    <td>单个TaskSlot的托管内存使用总量</td>
  </tr>
  <tr>
    <td>ockdb_memory_max_fresh</td>
    <td>FreshTable类型的内存分配总量</td>
  </tr>
  <tr>
    <td>ockdb_memory_max_slice</td>
    <td>SliceTable类型的内存分配总量</td>
  </tr>
  <tr>
    <td>ockdb_memory_max_file</td>
    <td>LSMStore类型的内存分配总量</td>
  </tr>
  <tr>
    <td>ockdb_memory_max_snapshot</td>
    <td>Snapshot类型的内存分配总量</td>
  </tr>
  <tr>
    <td>ockdb_memory_max_borrow_heap</td>
    <td>BorrowHeap类型的内存分配总量</td>
  </tr>
  <tr>
    <td>ockdb_memory_max_db</td>
    <td>单个TaskSlot的托管内存分配总量</td>
  </tr>
</tbody></table>

**FreshTable模块**

**表2 Metric指标参考**<a id="Metric指标参考_1"></a>

<table style="undefined;table-layout: fixed; width: 578px"><colgroup>
<col style="width: 296px">
<col style="width: 282px">
</colgroup>
<thead>
  <tr>
    <th>Metric指标项</th>
    <th>说明</th>
  </tr></thead>
<tbody>
  <tr>
    <td>ockdb_fresh_hit_count</td>
    <td>FreshTable访问命中次数</td>
  </tr>
  <tr>
    <td>ockdb_fresh_miss_count</td>
    <td>FreshTable访问未命中次数</td>
  </tr>
  <tr>
    <td>ockdb_fresh_record_count</td>
    <td>FreshTable访问总记录次数</td>
  </tr>
  <tr>
    <td>ockdb_fresh_flushing_record_count</td>
    <td>FreshTable正在淘汰的KV记录数</td>
  </tr>
  <tr>
    <td>ockdb_fresh_flushing_segment_count</td>
    <td>FreshTable正在淘汰的Segment数量</td>
  </tr>
  <tr>
    <td>ockdb_fresh_flushed_record_count</td>
    <td>FreshTable已淘汰的KV记录数</td>
  </tr>
  <tr>
    <td>ockdb_fresh_flushed_segment_count</td>
    <td>FreshTable已淘汰的Segment数量</td>
  </tr>
  <tr>
    <td>ockdb_fresh_segment_create_fail_count</td>
    <td>FreshTable创建Segment时内存不足导致失败的次数</td>
  </tr>
  <tr>
    <td>ockdb_fresh_flush_count</td>
    <td>FreshTable淘汰数据到SliceTable的总次数</td>
  </tr>
  <tr>
    <td>ockdb_fresh_binary_key_size</td>
    <td>FreshTable中当前所有Key的总大小</td>
  </tr>
  <tr>
    <td>ockdb_fresh_binary_value_size</td>
    <td>FreshTable中当前所有Value的总大小</td>
  </tr>
  <tr>
    <td>ockdb_fresh_binary_map_node_size</td>
    <td>FreshTable中当前所有MapNode的总大小</td>
  </tr>
  <tr>
    <td>ockdb_fresh_wasted_size</td>
    <td>FreshTable的Segment淘汰到SliceTable时，Segment空闲空间累积总大小</td>
  </tr>
</tbody></table>

**SliceTable模块**

**表3 Metric指标参考**<a id="Metric指标参考_2"></a>

<table style="undefined;table-layout: fixed; width: 578px"><colgroup>
<col style="width: 296px">
<col style="width: 282px">
</colgroup>
<thead>
  <tr>
    <th>Metric指标项</th>
    <th>说明</th>
  </tr></thead>
<tbody>
  <tr>
    <td>ockdb_slice_hit_count</td>
    <td>SliceTable访问命中次数</td>
  </tr>
  <tr>
    <td>ockdb_slice_miss_count</td>
    <td>SliceTable访问未命中次数</td>
  </tr>
  <tr>
    <td>ockdb_slice_read_count</td>
    <td>SliceTable访问总记录次数</td>
  </tr>
  <tr>
    <td>ockdb_slice_read_avg_size</td>
    <td>请求访问时平均遍历SliceTable中Slice链的长度</td>
  </tr>
  <tr>
    <td>ockdb_slice_evict_waiting_count</td>
    <td>待淘汰的Slice数量</td>
  </tr>
  <tr>
    <td>ockdb_slice_compaction_count</td>
    <td>SliceTable已完成的Compaction任务数</td>
  </tr>
  <tr>
    <td>ockdb_slice_compaction_slice_count</td>
    <td>SliceTable完成Compaction的Slice总数量</td>
  </tr>
  <tr>
    <td>ockdb_slice_compaction_avg_slice_count</td>
    <td>SliceTable每次Compaction任务平均处理的Slice数量</td>
  </tr>
  <tr>
    <td>ockdb_slice_chain_avg_size</td>
    <td>SliceChain的平均长度</td>
  </tr>
  <tr>
    <td>ockdb_slice_avg_size</td>
    <td>单个Slice的平均大小</td>
  </tr>
</tbody>
</table>

**FileCache模块**

**表4 Metric指标参考**<a id="Metric指标参考_3"></a>

<table style="undefined;table-layout: fixed; width: 789px"><colgroup>
<col style="width: 404px">
<col style="width: 385px">
</colgroup>
<thead>
  <tr>
    <th>Metric指标项</th>
    <th>说明</th>
  </tr></thead>
<tbody>
  <tr>
    <td>ockdb_index_block_hit_count</td>
    <td>IndexBlock在BlockCache中的访问命中次数</td>
  </tr>
  <tr>
    <td>ockdb_index_block_hit_size</td>
    <td>IndexBlock在BlockCache访问命中的数据量大小</td>
  </tr>
  <tr>
    <td>ockdb_index_block_miss_count</td>
    <td>IndexBlock在BlockCache访问未命中次数</td>
  </tr>
  <tr>
    <td>ockdb_index_block_miss_size</td>
    <td>IndexBlock在BlockCache访问未命中的数据量大小</td>
  </tr>
  <tr>
    <td>ockdb_index_block_cache_count</td>
    <td>IndexBlock在BlockCache中的缓存个数</td>
  </tr>
  <tr>
    <td>ockdb_index_block_cache_size</td>
    <td>IndexBlock在BlockCache中的缓存大小</td>
  </tr>
  <tr>
    <td>ockdb_data_block_hit_count</td>
    <td>DataBlock在BlockCache访问命中次数</td>
  </tr>
  <tr>
    <td>ockdb_data_block_hit_size</td>
    <td>DataBlock在BlockCache访问命中数据量大小</td>
  </tr>
  <tr>
    <td>ockdb_data_block_miss_count</td>
    <td>DataBlock在BlockCache访问未命中次数</td>
  </tr>
  <tr>
    <td>ockdb_data_block_miss_size</td>
    <td>DataBlock在BlockCache访问未命中数据量大小</td>
  </tr>
  <tr>
    <td>ockdb_data_block_cache_count</td>
    <td>DataBlock在BlockCache中的缓存个数</td>
  </tr>
  <tr>
    <td>ockdb_data_block_cache_size</td>
    <td>DataBlock在BlockCache中的缓存大小</td>
  </tr>
  <tr>
    <td>ockdb_filter_hit_count</td>
    <td>FilterBlock在BlockCache访问命中次数</td>
  </tr>
  <tr>
    <td>ockdb_filter_hit_size</td>
    <td>FilterBlock在BlockCache访问命中的数据量大小</td>
  </tr>
  <tr>
    <td>ockdb_filter_miss_count</td>
    <td>FilterBlock在BlockCache访问未命中次数</td>
  </tr>
  <tr>
    <td>ockdb_filter_miss_size</td>
    <td>FilterBlock在BlockCache访问未命中的数据量大小</td>
  </tr>
  <tr>
    <td>ockdb_filter_cache_count</td>
    <td>FilterBlock在BlockCache中的缓存个数</td>
  </tr>
  <tr>
    <td>ockdb_filter_cache_size</td>
    <td>FilterBlock在BlockCache中的缓存大小</td>
  </tr>
  <tr>
    <td>ockdb_filter_success_count</td>
    <td>FilterBlock对Key过滤结果为不存在的次数</td>
  </tr>
  <tr>
    <td>ockdb_filter_exist_success_count</td>
    <td>FilterBlock对Key过滤结果为存在且实际存在的次数</td>
  </tr>
  <tr>
    <td>ockdb_filter_exist_fail_count</td>
    <td>FilterBlock对Key过滤结果为存在且实际不存在的次数</td>
  </tr>
</tbody></table>

**FileStore模块**

**表5 Metric指标参考**<a id="Metric指标参考_4"></a>

<table style="undefined;table-layout: fixed; width: 904px"><colgroup>
<col style="width: 463px">
<col style="width: 441px">
</colgroup>
<thead>
  <tr>
    <th>Metric指标项</th>
    <th>说明</th>
  </tr></thead>
<tbody>
  <tr>
    <td>ockdb_lsm_flush_count</td>
    <td>LSMStore模块Flush到磁盘的文件总数</td>
  </tr>
  <tr>
    <td>ockdb_lsm_flush_size</td>
    <td>LSMStore模块Flush到磁盘的文件数据量总大小</td>
  </tr>
  <tr>
    <td>ockdb_lsm_compaction_count</td>
    <td>LSMStore完成的Compaction任务总次数</td>
  </tr>
  <tr>
    <td>ockdb_lsm_hit_count</td>
    <td>LSMStore访问命中次数</td>
  </tr>
  <tr>
    <td>ockdb_lsm_miss_count</td>
    <td>LSMStore访问未命中次数</td>
  </tr>
  <tr>
    <td>ockdb_level0_hit_count</td>
    <td>LSMStore的Level0层文件访问命中次数</td>
  </tr>
  <tr>
    <td>ockdb_level0_miss_count</td>
    <td>LSMStore的Level0层文件访问未命中次数</td>
  </tr>
  <tr>
    <td>ockdb_level1_hit_count</td>
    <td>LSMStore的Level1层文件访问命中次数</td>
  </tr>
  <tr>
    <td>ockdb_level1_miss_count</td>
    <td>LSMStore的Level1层文件访问未命中次数</td>
  </tr>
  <tr>
    <td>ockdb_level2_hit_count</td>
    <td>LSMStore的Level2层文件访问命中次数</td>
  </tr>
  <tr>
    <td>ockdb_level2_miss_count</td>
    <td>LSMStore的Level2层文件访问未命中次数</td>
  </tr>
  <tr>
    <td>ockdb_above_level2_hit_count</td>
    <td>LSMStore的Level3及以上层文件访问命中次数</td>
  </tr>
  <tr>
    <td>ockdb_above_level2_miss_count</td>
    <td>LSMStore的Level3及以上层文件访问未命中次数</td>
  </tr>
  <tr>
    <td>ockdb_level0_file_size</td>
    <td>LSMStore的Level0层文件数据量总大小</td>
  </tr>
  <tr>
    <td>ockdb_level1_file_size</td>
    <td>LSMStore的Level1层文件数据量总大小</td>
  </tr>
  <tr>
    <td>ockdb_level2_file_size</td>
    <td>LSMStore的Level2层文件数据量总大小</td>
  </tr>
  <tr>
    <td>ockdb_level3_file_size</td>
    <td>LSMStore的Level3层文件数据量总大小</td>
  </tr>
  <tr>
    <td>ockdb_above_level3_file_size</td>
    <td>LSMStore的Level4及以上层文件数据量总大小</td>
  </tr>
  <tr>
    <td>ockdb_lsm_file_size</td>
    <td>LSMStore所有层文件数据量总大小</td>
  </tr>
  <tr>
    <td>ockdb_lsm_compaction_read_size</td>
    <td>LSMStore Compaction任务执行时读取文件的总大小</td>
  </tr>
  <tr>
    <td>ockdb_lsm_compaction_write_size</td>
    <td>LSMStore Compaction任务执行时写入文件的总大小</td>
  </tr>
  <tr>
    <td>ockdb_level0_compaction_rate</td>
    <td>LSMStore的Level0层文件压缩率</td>
  </tr>
  <tr>
    <td>ockdb_level1_compaction_rate</td>
    <td>LSMStore的Level1层文件压缩率</td>
  </tr>
  <tr>
    <td>ockdb_level2_compaction_rate</td>
    <td>LSMStore的Level2层文件压缩率</td>
  </tr>
  <tr>
    <td>ockdb_level3_compaction_rate</td>
    <td>LSMStore的Level3层文件压缩率</td>
  </tr>
  <tr>
    <td>ockdb_lsm_compaction_rate</td>
    <td>LSMStore所有层文件总压缩率</td>
  </tr>
  <tr>
    <td>ockdb_lsm_file_count</td>
    <td>LSMStore所有层文件总数量</td>
  </tr>
</tbody></table>

**Snapshot模块**

**表6 Metric指标参考**<a id="Metric指标参考_5"></a>

<table style="undefined;table-layout: fixed; width: 1016px"><colgroup>
<col style="width: 520px">
<col style="width: 496px">
</colgroup>
<thead>
  <tr>
    <th>Metric指标项</th>
    <th>说明</th>
  </tr></thead>
<tbody>
  <tr>
    <td>ockdb_snapshot_total_time</td>
    <td>最近一次快照任务执行总耗时</td>
  </tr>
  <tr>
    <td>ockdb_snapshot_upload_time</td>
    <td>最近一次快照任务上传数据耗时</td>
  </tr>
  <tr>
    <td>ockdb_snapshot_file_count</td>
    <td>最近一次快照任务创建的文件数量</td>
  </tr>
  <tr>
    <td>ockdb_snapshot_file_size</td>
    <td>最近一次快照任务创建的文件大小</td>
  </tr>
  <tr>
    <td>ockdb_snapshot_incremental_size</td>
    <td>最近一次快照任务创建的增量文件大小</td>
  </tr>
  <tr>
    <td>ockdb_snapshot_slice_file_count</td>
    <td>最近一次快照任务创建的SliceTable快照文件数量</td>
  </tr>
  <tr>
    <td>ockdb_snapshot_slice_incremental_file_size</td>
    <td>最近一次快照任务创建的SliceTable增量文件大小</td>
  </tr>
  <tr>
    <td>ockdb_snapshot_slice_file_size</td>
    <td>最近一次快照任务创建的SliceTable快照文件大小</td>
  </tr>
  <tr>
    <td>ockdb_snapshot_sst_file_count</td>
    <td>最近一次快照任务创建的LSMStore快照文件数量</td>
  </tr>
  <tr>
    <td>ockdb_snapshot_sst_incremental_file_size</td>
    <td>最近一次快照任务创建的LSMStore增量文件大小</td>
  </tr>
  <tr>
    <td>ockdb_snapshot_sst_file_size</td>
    <td>最近一次快照任务创建的LSMStore快照文件大小</td>
  </tr>
</tbody></table>

**Restore模块**

**表7 Metric指标参考**<a id="Metric指标参考_6"></a>

<table style="undefined;table-layout: fixed; width: 689px"><colgroup>
<col style="width: 368px">
<col style="width: 321px">
</colgroup>
<thead>
  <tr>
    <th>Metric指标项</th>
    <th>说明</th>
  </tr></thead>
<tbody>
  <tr>
    <td>ockdb_restore_total_time</td>
    <td>最近一次快照恢复任务总耗时</td>
  </tr>
  <tr>
    <td>ockdb_restore_download_time</td>
    <td>最近一次快照恢复任务下载耗时</td>
  </tr>
  <tr>
    <td>ockdb_restore_lazy_download_time</td>
    <td>最近一次快照恢复任务懒加载耗时</td>
  </tr>
</tbody>
</table>

OmniStateStore支持对接Flink Metric框架，并提供一系列Metric指标，用于在任务运行过程中监测OmniStateStore的内存占用、缓存命中率等内部运行状态信息，为OmniStateStore在Flink场景下的性能调优与运行状态分析提供了Metric指标参考。

### 功能规格

对比OmniStateStore与RocksDB作为Flink状态后端在基本状态读写、Checkpoint、Savepoint等核心功能上的支持情况，两者在功能点上均保持一致支持，为用户评估OmniStateStore替代RocksDB的可行性提供参考依据。

开源Flink使用的RocksDB状态后端功能与OmniStateStore功能对比详见[表1 状态后端功能对比](#状态后端功能对比)。

**表1 状态后端功能对比**<a id="状态后端功能对比"></a>

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

对比OmniStateStore与RocksDB作为Flink状态后端在基本状态读写、Checkpoint、Savepoint等核心功能上的支持情况，两者在功能点上均保持一致支持，为用户评估OmniStateStore替代RocksDB的可行性提供参考依据。
