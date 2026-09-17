# Installation and User Guide<a name="EN-US_TOPIC_0000002550026769"></a>

## Environment Requirements<a name="EN-US_TOPIC_0000002518426734"></a>

Before installing and using OmniStateStore, ensure that the hardware and software environments meet the requirements for installation, deployment, and normal operation.

**Hardware Requirements<a name="en-us_topic_0000002466592290_section10915213184318"></a>**

The OmniStateStore software runs in Docker containers.  [Table 1](#en-us_topic_0000002466592290_en-us_topic_0000001713099729___d0e1171)  describes the hardware requirements.

**Table  1**  Hardware configuration requirements

<a name="en-us_topic_0000002466592290_en-us_topic_0000001713099729___d0e1171"></a>
<table><tbody><tr id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_row420mcpsimp"><th class="firstcol" valign="top" width="30%" id="mcps1.2.3.1.1"><p id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p422mcpsimp"><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p422mcpsimp"></a><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p422mcpsimp"></a>Processor</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.1.1 "><p id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p46522361582"><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p46522361582"></a><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p46522361582"></a>Kunpeng 920 series processors</p>
</td>
</tr>
<tr id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_row425mcpsimp"><th class="firstcol" valign="top" width="30%" id="mcps1.2.3.2.1"><p id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p427mcpsimp"><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p427mcpsimp"></a><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p427mcpsimp"></a>Memory size</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.2.1 "><p id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p429mcpsimp"><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p429mcpsimp"></a><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p429mcpsimp"></a>256 GB or above</p>
</td>
</tr>
<tr id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_row13668133081414"><th class="firstcol" valign="top" width="30%" id="mcps1.2.3.3.1"><p id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_en-us_topic_0000001713099729_p760mcpsimp"><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_en-us_topic_0000001713099729_p760mcpsimp"></a><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_en-us_topic_0000001713099729_p760mcpsimp"></a>Memory frequency</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.3.1 "><p id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_en-us_topic_0000001713099729_p762mcpsimp"><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_en-us_topic_0000001713099729_p762mcpsimp"></a><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_en-us_topic_0000001713099729_p762mcpsimp"></a>4800 MT/s</p>
</td>
</tr>
<tr id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_row781362621418"><th class="firstcol" valign="top" width="30%" id="mcps1.2.3.4.1"><p id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_en-us_topic_0000001713099729_p769mcpsimp"><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_en-us_topic_0000001713099729_p769mcpsimp"></a><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_en-us_topic_0000001713099729_p769mcpsimp"></a>NIC</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.4.1 "><p id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p91269393116"><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p91269393116"></a><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p91269393116"></a>NA</p>
</td>
</tr>
<tr id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_row430mcpsimp"><th class="firstcol" valign="top" width="30%" id="mcps1.2.3.5.1"><p id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p432mcpsimp"><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p432mcpsimp"></a><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p432mcpsimp"></a>Drive</p>
</th>
<td class="cellrowborder" valign="top" width="70%" headers="mcps1.2.3.5.1 "><p id="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p434mcpsimp"><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p434mcpsimp"></a><a name="en-us_topic_0000002466592290_en-us_topic_0000002466592194_p434mcpsimp"></a>At least one 3.6 TB or 7.68 TB NVMe SSD</p>
</td>
</tr>
</tbody>
</table>

**Software Requirements<a name="en-us_topic_0000002466592290_section5999819104313"></a>**

Before installing the OmniStateStore software, check that you have installed all the dependency software. Install these dependencies based on their security standards.  [Table 2](#en-us_topic_0000002466592290_en-us_topic_0000001664980070_table564mcpsimp)  lists the OS and software requirements of each node in the cluster.

**Table  2**  Software requirements<a name="en-us_topic_0000002466592290_en-us_topic_0000001664980070_table564mcpsimp"></a>

|Software Name|Software Version|How to Obtain|
|--|--|--|
|OS|openEuler 22.03 LTS SP3|Link|
|Java|JDK 1.8.0_432|Link|
|Flink|1.16.11.16.31.17.1|Link|

**Obtaining the Software Package<a name="en-us_topic_0000002466592290_en-us_topic_0000001664980070_section3489574613"></a>**

**Table  3**  OmniStateStore software list<a name="en-us_topic_0000002466592290_en-us_topic_0000001664980070__table677mcpsimp"></a>

|Software Name|Package Name|Release Type|Description|Download URL|
|--|--|--|--|--|
|OmniStateStore package|omnistatestore_1.1.0_aarch64_release.tar.gz|Open source|OmniStateStore software installation package|Link|

**Verifying Software Package Integrity<a name="en-us_topic_0000002466592290_section15465143812508"></a>**

To prevent a software package from being maliciously tampered with during transfer or storage, download also the corresponding digital signature file for integrity verification while obtaining the software package.

1. Obtain the software package based on  [Table 3](#en-us_topic_0000002466592290_en-us_topic_0000001664980070__table677mcpsimp).
2. Obtain the  _OpenPGP Signature Verification Guide_.
    - For carriers:  [https://support.huawei.com/carrier/digitalSignatureAction](http://support.huawei.com/carrier/digitalSignatureAction).
    - For enterprises:  [https://support.huawei.com/enterprise/en/tool/pgp-verify-TL1000000054](https://support.huawei.com/enterprise/en/tool/pgp-verify-TL1000000054).

3. Verify the software package integrity by following instructions in  _OpenPGP Signature Verification Guide_.

    > ![](public_sys-resources/icon-note.gif) **NOTE:**
    >
    > - If the verification fails, do not use the software package, and contact Huawei technical support.
    > - Before using the software package for an installation or upgrade, verify the digital signature to ensure that the software has not been tampered with.

Before installing and using OmniStateStore, ensure that the hardware and software environments meet the requirements for installation, deployment, and normal operation.

## Installing OmniStateStore<a name="EN-US_TOPIC_0000002518426732"></a>

1. Obtain  **omnistatestore\_1.1.0\_aarch64\_release.tar.gz**  based on  [Table 3](#en-us_topic_0000002466592290_en-us_topic_0000001664980070__table677mcpsimp).
2. Log in to the installation node and upload the  **BoostKit-omnistatestore\_1.1.0\_aarch64\_release.tar.gz**  software package to the  **$\{FLINK\_HOME\}/lib/**  subdirectory.
3. Extract the software package.

    ```bash
    tar -zxvf BoostKit-omnistatestore_1.1.0_aarch64_release.tar.gz
    ```

    The software package contains only the OmniStateStore plugin JAR packages.

    - flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-1.16.1.jar
    - flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-1.16.3.jar
    - flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-1.17.1.jar

    Retain only the JAR package that matches the current Flink version and remove the other JAR packages. For example, retain Flink 1.16.3:

    ```bash
    rm -f flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-1.17.1.jar
    rm -f flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-1.16.1.jar
    ```

4. To release drive space, run the following command to delete the software package:

    ```bash
    rm -f BoostKit-omnistatestore_1.1.0_aarch64_release.tar.gz
    ```

## Starting OmniStateStore<a name="EN-US_TOPIC_0000002550026579"></a>

This section describes how to start the OmniStateStore service to enable the acceleration function of Flink state storage.

1. Set the configuration items in the  **flink-conf.yaml**  file located in the Flink  **conf**  directory based on the service requirements and deployment environment.

    Configuration item format:  _$\{Configuration item name\} + $\{Colon\} + $\{Space\} + $\{Configuration item value\}_.  [Configuration Items](#EN-US_TOPIC_0000002550026577)  describes the configuration items. The following describes the configuration items in different scenarios.

    - Add or modify the following configuration items in  **$\{FLINK\_HOME\}/conf/flink-conf.yaml**  to enable OmniStateStore. Update the configuration files of the Job Manager and all Task Managers.

        **Table  1**  Configuration items

|Configuration Item|Description|Example|Remarks|
|--|--|--|--|
|state.backend|Open-source Flink parameter, which is used to configure the state backend.|com.huawei.ock.bss.OckDBStateBackendFactory|This configuration item determines the state backend type. Verify that the value is case-sensitive and correctly spelled.|
|state.backend.ockdb.localdir|Local OmniStateStore state data path.|/usr/local/flink/ockdb|Check that the path exists and the Flink run user has the read and write permissions on the path.|
|state.backend.ockdb.jni.logfile|OmniStateStore log path.|/usr/local/flink/log/kv.log|The Flink log path is recommended for this path.|

        A configuration example is as follows:

        ```yaml
        state.backend: com.huawei.ock.bss.OckDBStateBackendFactory
        state.backend.ockdb.localdir: /usr/local/flink/ockdb
        state.backend.ockdb.jni.logfile: /usr/local/flink/log/kv.log
        ```

    - Enable persistent storage of priority queues.

        A configuration example is as follows:

        ```yaml
        state.backend.ockdb.timer-service.factory: OCKDB
        ```

    - Enable KV separated storage.

        A configuration example is as follows:

        ```yaml
        state.backend.ockdb.kv-separate.switch: true
        state.backend.ockdb.kv-separate.threshold: 200
        ```

2. Create the necessary directories.

    In the example,  **state.backend.ockdb.localdir**  is set to  **/usr/local/flink/ockdb**  and  **state.backend.ockdb.checkpoint.backup**  is set to  **/usr/local/flink/checkpoint/backup**. Replace the example directories with the actual directories used in your installation.

    ```bash
    mkdir -p /usr/local/flink/ockdb
    mkdir -p /usr/local/flink/checkpoint/backup
    ```

3. Start a Flink task and view the configuration items in the log to check whether the configuration is successful.
4. Run the  **$\{FLINK\_HOME\}/examples/streaming/WordCount.jar**  demo application.

    If "OmniStateStore service start success" is displayed in the Task Manager logs, OmniStateStore is started successfully.

This section describes how to start the OmniStateStore service to enable the acceleration function of Flink state storage.

## Maintaining the Feature<a name="EN-US_TOPIC_0000002549906573"></a>

Upgrade or uninstall the feature by following the instructions below.

**Upgrading the Software<a name="en-us_topic_0000002516475165_section8320132214814"></a>**

Replace the existing JAR package with the JAR package of the new version on the installation node. You do not need to uninstall the existing version. For details, see  [Installing OmniStateStore](#EN-US_TOPIC_0000002518426732).

**Uninstalling the Software<a name="en-us_topic_0000002516475165_section777110331398"></a>**

> ![](public_sys-resources/icon-notice.gif) **NOTICE:**
>
> Perform the following steps only when you need to uninstall OmniStateStore.

1. Delete the configured  **state.backend.ockdb.localdir**  path from the installation node.
2. Delete the  **flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-$\{flink._x.x.x_\}.jar**  file from the  **$\{FLINK\_HOME\}/lib/**  directory.

    ```bash
    rm -f flink-boost-statebackend-1.1.0-SNAPSHOT-for-flink-x.x.x.jar
    ```

3. Set the  **state.backend**  configuration item in the  **flink-conf.yaml**  configuration file to another state backend.

## References<a name="EN-US_TOPIC_0000002518266822"></a>

### Configuration Items<a name="EN-US_TOPIC_0000002550026577"></a>

The parameter configuration rules for the Log, StateStore, and Metric modules of OmniStateStore cover log management, state storage, and performance monitoring, providing guidance for deploying and tuning OmniStateStore on Flink.

[Table 1](#en-us_topic_0000002516476219_table19127204118479),  [Table 2](#en-us_topic_0000002516476219_table5331123964816), and  [Table 3](#en-us_topic_0000002516476219_table8874183794213)  describe the configuration items of the Log, StateStore, and Metric modules.

**Table  1**  Configuration items of the Log module<a name="en-us_topic_0000002516476219_table19127204118479"></a>

|Configuration Item|Description|Default Value|Value Range|Remarks|
|--|--|--|--|--|
|state.backend.ockdb.jni.logfile|Log file path and name.|/usr/local/flink/log/kv.log|Files in the path on which the Flink run user has the read and write permissions (The path must exist.)|Check that the path exists and the Flink run user has the read and write permissions on the path.|
|state.backend.ockdb.jni.loglevel|Log level.<br>**1**: DEBUG<br>**2**: INFO<br>**3**: WARN<br>**4**: ERROR|2|[1, 4]|None|
|state.backend.ockdb.jni.lognum|Maximum number of log files.|20|[10, 50]|None|
|state.backend.ockdb.jni.logsize|Size of a single log file. Unit: MB.|20|[10, 50]|None|

**Table  2**  Configuration items of the StateStore module<a name="en-us_topic_0000002516476219_table5331123964816"></a>

|Configuration Item|Description|Default Value|Value Range|Remarks|
|--|--|--|--|--|
|state.backend|Open source Flink parameter, which is used to configure the state backend.|None|com.huawei.ock.bss.OckDBStateBackendFactory|Ensure that the case-sensitive characters are correctly spelled.|
|state.backend.ockdb.localdir|Local OmniStateStore data path.|None|An existing path for which the Flink run user has the read and write permissions.|Check that the path exists and the Flink run user has the read and write permissions on the path.<br>Check that the path and the **taskmanager.state.local.root-dirs** path are in the same file system.|
|taskmanager.state.local.root-dirs|Open-source Flink parameter, which is used to set the local checkpoint temporary directory.|None|An existing path for which the Flink run user has the read and write permissions.|You are recommended to set this configuration item. If you choose not to set this configuration item, the path specified by **io.tmp.dirs** is used by default.<br>Check that the path and the **state.backend.ockdb.localdir** path are in the same file system.|
|state.backend.ockdb.savepoint.sort.local.dir|Path for storing temporary sorting files generated during savepoint creation. This parameter is required for using savepoints.|/usr/local/flink/savepoint/tmp|An existing path for which the Flink run user has the read and write permissions.|Check that the path exists and the Flink run user has the read and write permissions on the path.|
|state.backend.ockdb.jni.slice.watermark.ratio|The cache layer triggers data eviction by setting the high and low watermark ratio thresholds. Cold data is migrated to the LSM file storage layer based on the preset policy to dynamically balance storage resources.|0.8|(0, 1)|Generally, you do not need to set it separately.|
|state.backend.ockdb.file.memory.fraction|Ratio of the memory cache space used for reading and writing data at the LSM layer to the maximum memory of the entire database instance.|0.2|[0.1, 0.5]|Generally, you do not need to set it separately.|
|state.backend.ockdb.jni.lsmstore.compaction.switch|Indicates whether to sort and merge data in the LSM file storage layer. The leveled compaction mechanism of the LSM file storage layer controls the sorting and compaction of data files to optimize storage performance and space utilization.|1|**0**: disable<br>**1**: enable|It is recommended that you enable this option.|
|state.backend.ockdb.ttl.filter.switch|Compresses time to live (TTL) expired data in the background.|false|**false**: disable<br>**true**: enable|You are advised to enable this function for TTL State.|
|state.backend.ockdb.lsmstore.compression.policy|Compression policy of each level in the LSM store. It is used with the default value of "state.backend.ockdb.lsmstore.compression.level.policy".<br>**level0**: Compression is disabled.<br>**level1**: Compression is disabled.<br>**level2**: LZ4 compression is enabled.<br>Other levels: Full compression is enabled.|lz4|**none**: Compression is disabled.<br>**lz4**: LZ4 compression is enabled.|If the checkpoint file to be uploaded is too large, you are advised to enable this function.|
|state.backend.ockdb.lsmstore.compression.level.policy|Configures the LSM file compression policy for different levels. The default value is **none,none,lz4**, which indicates that compression is disabled at level 0 and level 1 and LZ4 compression is enabled at level 2.|none,none,lz4|**none**: Compression is disabled.<br>**lz4**: LZ4 compression is enabled.|When checkpoints become a bottleneck, you can advance the compression policy to a lower level. The default level range is [0, 5].<br>**level0** indicates foreground write compression. **None** is recommended.<br>Other levels indicate background compression.|
|state.backend.ockdb.freshtable.snapshot.compression.policy|Compression policy for FreshTable checkpoint snapshot files.|none|**none**: Compression is disabled.<br>**lz4**: LZ4 compression is enabled.|When FreshTable snapshot file uploads become a checkpoint bottleneck, you can set this parameter to **lz4** to reduce the amount of uploaded data. Compression increases CPU usage.|
|state.backend.ockdb.slicetable.snapshot.compression.policy|Compression policy for SliceTable checkpoint snapshot files.|none|**none**: Compression is disabled.<br>**lz4**: LZ4 compression is enabled.|When SliceTable snapshot file uploads become a checkpoint bottleneck, you can set this parameter to **lz4** to reduce the amount of uploaded data. Compression increases CPU usage.|
|state.backend.ockdb.lazy.download.switch|Indicates whether to enable lazy loading during recovery from a checkpoint.|false|**false**: disable<br>**true**: enable|When the checkpoint size is large, enable this option to shorten the time required for restoring a task to the running state.|
|state.backend.ockdb.bloom.filter.switch|Enables or disables the Bloom filter for status keys.|true|**false**: disable<br>**true**: enable|You are advised to enable this function in scenarios where a large number of invalid key accesses. When this function is enabled, the memory usage increases by dozens of megabytes.|
|state.backend.bloom.filter.expected.key.count|Order of magnitude of keys to be filtered by the Bloom filter in a single state.|8000000|[1000000, 10000000]|Generally, you do not need to set it separately. A larger value indicates that the Bloom filter occupies more memory.|
|state.backend.ockdb.cache.filter.and.index.switch|Enables or disables the use of the least recently used (LRU) cache for filter and index blocks at the LSM layer.|true|**false**: disable<br>**true**: enable|Generally, you do not need to set it separately. If there are a large number of files and different files are frequently read, you are advised to enable this function.|
|state.backend.ockdb.cache.filter.and.index.ratio|Ratio of the memory occupied by the filter and index blocks to the total memory. This memory is not subject to LRU-based eviction.|0|(0, 1)|Generally, you do not need to set it separately. You are advised to enable this function when the filter and index blocks are frequently released in the cache due to heavy pressure.|
|state.backend.ockdb.checkpoint.backup|Directory for storing local checkpoint backup slice files when local restoration is enabled.|None|Files in the path on which the Flink run user has the read and write permissions (The path must exist.)|Set this parameter when local restoration is enabled. Check that the path exists and the Flink run user has the read and write permissions on the path.|
|state.backend.ockdb.timer-service.factory|Location where the Flink timer is stored.|OCKDB|**OCKDB**: persistently stored in the state backend<br>**HEAP**: stored in the JVM heap memory|When the number of timers is small, the heap-based timer may have better performance.|
|state.backend.ockdb.kv-separate.switch|Enables or disables KV separation.|false|**false**: disable<br>**true**: enable|Enable KV separation if this value is large.|
|state.backend.ockdb.kv-separate.threshold|Threshold for enabling KV separation. Enable KV separation when this value is exceeded.|200|(8, 4294967295)|Values greater than this threshold are stored separately after KV separation.|

**Table  3**  Configuration items of the Metric module<a name="en-us_topic_0000002516476219_table8874183794213"></a>

|Configuration Item|Description|Default Value|Value Range|Remarks|
|--|--|--|--|--|
|state.backend.ockdb.metric.enable|Enables or disables the overall metric function. OmniStateStore collects metric information after this function is enabled.|false|**false**: disable<br>**true**: enable|The metric function of each module takes effect only after this option is enabled.|
|state.backend.ockdb.metric.memory|Enables or disables the metric function of the MemoryManager module.|false|**false**: disable<br>**true**: enable|None|
|state.backend.ockdb.metric.fresh.table|Enables or disables the metric function of the FreshTable module.|false|**false**: disable<br>**true**: enable|None|
|state.backend.ockdb.metric.slice.table|Enables or disables the metric function of the SliceTable module.|false|**false**: disable<br>**true**: enable|None|
|state.backend.ockdb.metric.lsm.store|Enables or disables the metric function of the LSM Store module.|false|**false**: disable<br>**true**: enable|None|
|state.backend.ockdb.metric.lsm.cache|Enables or disables the metric function of the LSM Cache module.|false|**false**: disable<br>**true**: enable|None|
|state.backend.ockdb.metric.snapshot|Enables or disables the metric function of the Snapshot module.|false|**false**: disable<br>**true**: enable|None|
|state.backend.ockdb.metric.restore|Enables or disables the metric function of the Restore module.|false|**false**: disable<br>**true**: enable|None|

The parameter configuration rules for the Log, StateStore, and Metric modules of OmniStateStore cover log management, state storage, and performance monitoring, providing guidance for deploying and tuning OmniStateStore on Flink.

### Metrics<a name="EN-US_TOPIC_0000002550026581"></a>

OmniStateStore can connect to the Flink Metric framework to provide metrics for monitoring its internal status, such as memory usage and cache hit ratio, during task execution. These metrics serve as a reference for performance tuning and operational analysis in Flink scenarios.

You can add and view these metrics on the  **Metric**  page during task execution on the Flink WebUI to learn and analyze the running performance of OmniStateStore in real time.

> ![](public_sys-resources/icon-notice.gif) **NOTICE:**
>
> - Collecting metric data introduces additional performance overhead, which may affect task execution performance. It is recommended to enable the metric feature only during task testing or for performance-insensitive tasks.
> - The unit of all data volume metrics is byte, and the unit of all time metrics is second.

**MemoryManager Module<a name="en-us_topic_0000002505799817_section7735163610488"></a>**

**Table  1**  Metric references

|Metric|Description|
|--|--|
|ockdb_memory_used_fresh|Used memory of the FreshTable type.|
|ockdb_memory_used_slice|Used memory of the SliceTable type.|
|ockdb_memory_used_file|Used memory of the LSMStore type.|
|ockdb_memory_used_snapshot|Used memory of the Snapshot type.|
|ockdb_memory_used_borrow_heap|Used memory of the BorrowHeap type.|
|ockdb_memory_used_db|Managed memory used by a single TaskSlot.|
|ockdb_memory_max_fresh|Allocated memory of the FreshTable type.|
|ockdb_memory_max_slice|Allocated memory of the SliceTable type.|
|ockdb_memory_max_file|Allocated memory of the LSMStore type.|
|ockdb_memory_max_snapshot|Allocated memory of the Snapshot type.|
|ockdb_memory_max_borrow_heap|Allocated memory of the BorrowHeap type.|
|ockdb_memory_max_db|Managed memory allocated to a single TaskSlot.|

**FreshTable Module<a name="en-us_topic_0000002505799817_section12799165145018"></a>**

**Table  2**  Metric references

|Metric|Description|
|--|--|
|ockdb_fresh_hit_count|Number of FreshTable hits.|
|ockdb_fresh_miss_count|Number of FreshTable misses.|
|ockdb_fresh_record_count|Total number of FreshTable accesses.|
|ockdb_fresh_flushing_record_count|Number of KV records that are being evicted by the FreshTable.|
|ockdb_fresh_flushing_segment_count|Number of segments that are being evicted by the FreshTable.|
|ockdb_fresh_flushed_record_count|Number of KV records that have been evicted by the FreshTable.|
|ockdb_fresh_flushed_segment_count|Number of segments that have been evicted by the FreshTable.|
|ockdb_fresh_segment_create_fail_count|Number of times that FreshTable fails to generate segments due to insufficient memory.|
|ockdb_fresh_flush_count|Total number of times that FreshTable evicts data to SliceTable.|
|ockdb_fresh_binary_key_size|Total size of all keys in the FreshTable.|
|ockdb_fresh_binary_value_size|Total size of all values in the FreshTable.|
|ockdb_fresh_binary_map_node_size|Total size of all MapNodes in the FreshTable.|
|ockdb_fresh_wasted_size|Total size of free segment space when segments in the FreshTable are evicted to the SliceTable.|

**SliceTable Module<a name="en-us_topic_0000002505799817_section9213174155116"></a>**

**Table  3**  Metric references

|Metric|Description|
|--|--|
|ockdb_slice_hit_count|Number of SliceTable hits.|
|ockdb_slice_miss_count|Number of SliceTable misses.|
|ockdb_slice_read_count|Total number of SliceTable accesses.|
|ockdb_slice_read_avg_size|Average length of the slice chain traversed in the SliceTable during access.|
|ockdb_slice_evict_waiting_count|Number of slices to be evicted.|
|ockdb_slice_compaction_count|Completed compaction tasks in the SliceTable.|
|ockdb_slice_compaction_slice_count|Total number of slices for compaction in the SliceTable.|
|ockdb_slice_compaction_avg_slice_count|Average number of slices processed by each compaction task in the SliceTable.|
|ockdb_slice_chain_avg_size|Average SliceChain length.|
|ockdb_slice_avg_size|Average size of a single slice.|

**FileCache Module<a name="en-us_topic_0000002505799817_section1596412433523"></a>**

**Table  4**  Metric references

|Metric|Description|
|--|--|
|ockdb_index_block_hit_count|Number of IndexBlock hits in the BlockCache.|
|ockdb_index_block_hit_size|Data volume of IndexBlock hits in the BlockCache.|
|ockdb_index_block_miss_count|Number of IndexBlock misses in the BlockCache.|
|ockdb_index_block_miss_size|Data volume of IndexBlock misses in the BlockCache.|
|ockdb_index_block_cache_count|Number of IndexBlocks cached in the BlockCache.|
|ockdb_index_block_cache_size|Size of the IndexBlock cache in the BlockCache.|
|ockdb_data_block_hit_count|Number of DataBlock hits in the BlockCache.|
|ockdb_data_block_hit_size|Data volume of DataBlock hits in the BlockCache.|
|ockdb_data_block_miss_count|Number of DataBlock misses in the BlockCache.|
|ockdb_data_block_miss_size|Data volume of DataBlock misses in the BlockCache.|
|ockdb_data_block_cache_count|Number of DataBlocks cached in the BlockCache.|
|ockdb_data_block_cache_size|Size of the DataBlock cache in the BlockCache.|
|ockdb_filter_hit_count|Number of FilterBlock hits in the BlockCache.|
|ockdb_filter_hit_size|Data volume of FilterBlock hits in the BlockCache.|
|ockdb_filter_miss_count|Number of FilterBlock misses in the BlockCache.|
|ockdb_filter_miss_size|Data volume of FilterBlock misses in the BlockCache.|
|ockdb_filter_cache_count|Number of FilterBlocks cached in the BlockCache.|
|ockdb_filter_cache_size|Size of the FilterBlock cache in the BlockCache.|
|ockdb_filter_success_count|Number of occurrences where the key is not present in the FilterBlock's filtered result.|
|ockdb_filter_exist_success_count|Number of occurrences where the key is present in the FilterBlock's filtered result and actually exists.|
|ockdb_filter_exist_fail_count|Number of occurrences where the key is present in the FilterBlock's filtered result but actually does not exist.|

**FileStore Module<a name="en-us_topic_0000002505799817_section1634518277549"></a>**

**Table  5**  Metric references

|Metric|Description|
|--|--|
|ockdb_lsm_flush_count|Total number of files flushed by the LSMStore module to drives.|
|ockdb_lsm_flush_size|Total data volume of files flushed by the LSMStore module to drives.|
|ockdb_lsm_compaction_count|Total number of compaction tasks completed in the LSMStore module.|
|ockdb_lsm_hit_count|Number of LSMStore hits.|
|ockdb_lsm_miss_count|Number of LSMStore misses.|
|ockdb_level0_hit_count|Number of LSMStore Level0 file hits.|
|ockdb_level0_miss_count|Number of LSMStore Level0 file misses.|
|ockdb_level1_hit_count|Number of LSMStore Level1 file hits.|
|ockdb_level1_miss_count|Number of LSMStore Level1 file misses.|
|ockdb_level2_hit_count|Number of LSMStore Level2 file hits.|
|ockdb_level2_miss_count|Number of LSMStore Level2 file misses.|
|ockdb_above_level2_hit_count|Number of LSMStore Level3 and above file hits.|
|ockdb_above_level2_miss_count|Number of LSMStore Level3 and above file misses.|
|ockdb_level0_file_size|Total data volume of LSMStore Level0 files.|
|ockdb_level1_file_size|Total data volume of LSMStore Level1 files.|
|ockdb_level2_file_size|Total data volume of LSMStore Level2 files.|
|ockdb_level3_file_size|Total data volume of LSMStore Level3 files.|
|ockdb_above_level3_file_size|Total data volume of LSMStore Level4 and above files.|
|ockdb_lsm_file_size|Total data volume of files at all LSMStore layers.|
|ockdb_lsm_compaction_read_size|Total size of files read during LSMStore compaction task execution.|
|ockdb_lsm_compaction_write_size|Total size of files written during LSMStore compaction task execution.|
|ockdb_level0_compaction_rate|Compression rate of LSMStore Level0 files.|
|ockdb_level1_compaction_rate|Compression rate of LSMStore Level1 files.|
|ockdb_level2_compaction_rate|Compression rate of LSMStore Level2 files.|
|ockdb_level3_compaction_rate|Compression rate of LSMStore Level3 files.|
|ockdb_lsm_compaction_rate|Compression rate of files at all LSMStore levels.|
|ockdb_lsm_file_count|Number of files at all LSMStore levels.|

**Snapshot Module<a name="en-us_topic_0000002505799817_section15501109155814"></a>**

**Table  6**  Metric reference

|Metric|Description|
|--|--|
|ockdb_snapshot_total_time|Total execution duration of the latest snapshot task.|
|ockdb_snapshot_upload_time|Total upload duration of the latest snapshot task.|
|ockdb_snapshot_file_count|Number of files created in the latest snapshot task.|
|ockdb_snapshot_file_size|Size of files created in the latest snapshot task.|
|ockdb_snapshot_incremental_size|Size of incremental files created in the latest snapshot task.|
|ockdb_snapshot_slice_file_count|Number of SliceTable snapshot files created in the latest snapshot task.|
|ockdb_snapshot_slice_incremental_file_size|Size of incremental SliceTable files created in the latest snapshot task.|
|ockdb_snapshot_slice_file_size|Size of SliceTable snapshot files created in the latest snapshot task.|
|ockdb_snapshot_sst_file_count|Number of LSMStore snapshot files created in the latest snapshot task.|
|ockdb_snapshot_sst_incremental_file_size|Size of incremental LSMStore files created in the latest snapshot task.|
|ockdb_snapshot_sst_file_size|Size of LSMStore snapshot files created in the latest snapshot task.|

**Restore Module<a name="en-us_topic_0000002505799817_section19402193125815"></a>**

**Table  7**  Metric references

|Metric|Description|
|--|--|
|ockdb_restore_total_time|Total duration of the latest snapshot restoration task.|
|ockdb_restore_download_time|Download duration of the latest snapshot restoration task.|
|ockdb_restore_lazy_download_time|Lazy loading duration of the latest snapshot restoration task.|

OmniStateStore can connect to the Flink Metric framework to provide metrics for monitoring its internal status, such as memory usage and cache hit ratio, during task execution. These metrics serve as a reference for performance tuning and operational analysis in Flink scenarios.

### Function Specifications<a name="EN-US_TOPIC_0000002518266820"></a>

When used as a Flink state backend, OmniStateStore and RocksDB both support core functions such as basic state read/write, checkpoints, and savepoints, providing a reference for evaluating the feasibility of replacing RocksDB with OmniStateStore.

[Table 1](#en-us_topic_0000002499631625_table462152614498)  describes the comparison between RocksDB and OmniStateStore.

**Table  1**  State backend comparison<a name="en-us_topic_0000002499631625_table462152614498"></a>

|Item|Function|RocksDB State Backend|OmniStateStore State Backend|
|--|--|--|--|
|Basic status read and write APIs|Operator state|Supported|Supported|
|Basic status read and write APIs|Broadcast state|Supported|Supported|
|Basic status read and write APIs|Value state|Supported|Supported|
|Basic status read and write APIs|List state|Supported|Supported|
|Basic status read and write APIs|Map state|Supported|Supported|
|Basic status read and write APIs|Reducing state|Supported|Supported|
|Basic status read and write APIs|Aggregating state|Supported|Supported|
|Basic status read and write APIs|Time to live (TTL)|Supported|Supported|
|Basic status read and write APIs|Timer|Supported|Supported|
|Checkpoint|Full snapshot|Supported|Supported|
|Checkpoint|Incremental snapshot|Supported|Supported|
|Checkpoint|Aligned snapshot|Supported|Supported|
|Checkpoint|Non-aligned snapshot|Supported|Supported|
|Checkpoint|Common snapshot restoration|Supported|Supported|
|Checkpoint|Snapshot restoration in scenarios with scaling parallelism|Supported|Supported|
|Savepoint|Hitless savepoint|Supported|Supported|
|Savepoint|Blocking savepoint|Supported|Supported|
|Savepoint|Standard savepoint|Supported|Supported|
|Savepoint|Native savepoint|Supported|Supported|
|Savepoint|Savepoint release|Supported|Supported|
|Savepoint|Common savepoint restoration|Supported|Supported|
|Savepoint|Savepoint restoration in scenarios with scaling parallelism|Supported|Supported|
|Savepoint|Status data structure upgrade with savepoints|Supported|Supported|

When used as a Flink state backend, OmniStateStore and RocksDB both support core functions such as basic state read/write, checkpoints, and savepoints, providing a reference for evaluating the feasibility of replacing RocksDB with OmniStateStore.
