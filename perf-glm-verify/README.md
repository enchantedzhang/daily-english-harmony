# 项目介绍

## 背景

随着大型计算系统的出现，不同领域的组织以实时方式生成大量数据，Flink是一种开源的分布式计算框架，专为处理大规模数据流而设计，它能够在低时延和高吞吐的场景下高效地处理实时数据。

后端状态存储系统在其中扮演着重要角色，当作业状态规模较大时，状态存储系统无法将全部的状态数据存储在内存中，往往会将冷数据存储在大容量磁盘上，但是内存和磁盘两种介质在访问性能上存在巨大的差异，IO读写很容易成为数据处理的瓶颈，比如在计算过程中如果某个算子需要频繁从磁盘上加载状态数据，那么这个算子就很容易成为整个作业吞吐的性能瓶颈，因此后端状态存储系统在很多时候是决定Flink作业性能的决定性因素。

目前开源Flink采用的状态后端包括MemoryStateBackend、FsStateBackend和RocksDBStateBackend，每种stateBackend都有其优缺点，适用于不同的场景。

- MemoryStateBackend在大规模状态管理中，由于内存限制可能会导致OOM（Out of Memory）错误，并且在大规模任务中从内存中恢复状态速度慢。

- FsStateBackend依赖于外部存储系统的I/O性能，在高负载或网络延迟高的情况下状态访问速度可能变慢。

- RocksDBStateBackend是当前使用最为广泛的后端存储系统，但是RocksDB作为一个通用的KV存储引擎，并不完全适合流式计算场景，在实际生产使用中会遭遇以下问题：

    - RocksDB是一个本地嵌入式数据库，状态访问IO性能差，会消耗大量CPU资源。
    - 基于LSM-Tree实现的文件持久化存储中数据顺序批量读写磁盘，查询延迟较大，拖慢整体流计算吞吐率。
    - 扩缩并发场景下状态恢复较慢。
    并且随着互联网搜推应用快速发展，产生的状态数据量也日益增多，大规模作业下甚至会产生十几~几十TB的状态数据量，那么RocksDBStateBackend的局限性会被无限放大。

因此我们针对生产环境中遇到的问题，再结合实际应用，推出了高性能状态存储引擎OmniStateStore，旨在解决实际生产使用中遇到的痛点或瓶颈点，更好地适用于大规模Flink作业场景。

## OmniStateStore介绍

状态存储引擎OmniStateStore整体架构上采用以磁盘为主要存储，内存作为Cache的方案，软件架构视图从上到下可分为FreshTable、SliceTable和文件存储LsmStore模块，其核心优势如下所示。

- FreshTable模块能够快速终结应用IO，其状态数据采用紧密内存结构的哈希索引，相比传统排序索引性能提升显著。

- SliceTable模块中的状态数据重新根据state key进行hash打散，控制单个SliceDate数据，结合鲲鹏L1 Cache优异特性，具有更好地状态数据查询性能。

- 重新设计LSM-Tree中的文件数据，改善磁盘存储结构，优化大状态场景下的状态数据在文件层的读写性能。

- 提供状态数据懒加载功能，能够大幅提升启动和并行度变更场景下的状态恢复性能。

## 使能场景

- OmniStateStore是基于开源Flink的状态存储后端标准接口实现，未对开源Flink本身做任何侵入式修改，因此支持以Jar包形式平滑替换当前开源Flink支持的任一种StateBackend。

- 当前开源Flink广泛应用的状态后端是RocksDBStateBackend，OmniStateStore平滑替换后可以带来端到端的性能提升，其性能提升比例取决于状态后端在整个Flink作业期间的CPU占比，CPU占比越高性能提升越明显。

## 目录结构<a name="ZH-CN_TOPIC_0000002549907423"></a>

项目全量目录层级介绍如下所示。

```cmd
├── docs                                      # 项目文档目录
│   └── zh                                    # 中文文档目录
│       ├── figures                           # 中文文档图片资源目录
│       ├── quick_start.md                    # 快速入门
│       ├── release_notes.md                  # OmniStateStore版本说明书
│       ├── installation_guide.md             # OmniStateStore安装指导
│       ├── user_guide.md                     # OmniStateStore使用指导
│       ├── best_practices.md                 # OmniStateStore场景化应用最佳实践
│       ├── api_reference.md                  # OmniStateStore接口参考
│       ├── design_guide.md                   # OmniStateStore设计指南
│       ├── faq.md                            # OmniStateStore安装使用常见问题
```

## 版本说明<a name="ZH-CN_TOPIC_0000002518267672"></a>

每个版本的特性变更详细信息，请参见\[quick\_start.md\]\(./docs/zh/quick\_start.md\)。

## 环境部署<a name="ZH-CN_TOPIC_0000002550027431"></a>

介绍OmniStateStore的环境依赖及安装方式，具体请参见\[installation\_guide.md\]\(./docs/zh/installation\_guide.md\)。

## 快速入门<a name="ZH-CN_TOPIC_0000002518427582"></a>

安装OmniStateStore后如何快速验证OmniStateStore是否生效，性能是否提升，具体请参见\[quick\_start.md\]\(./docs/zh/quick\_start.md\)。

## 学习文档<a name="ZH-CN_TOPIC_0000002549907425"></a>

|名称|路径|简介|
|--|--|--|
|快速入门|[quick_start.md](./docs/zh/quick_start.md)|提供快速使能并验证OmniStateStore加速能力的快速入门指导。|
|版本说明书|[release_notes.md](./docs/zh/release_notes.md)|提供OmniStateStore每个发布版本的基础信息和特性更新信息。|
|安装指南|[installation_guide.md](./docs/zh/installation_guide.md)|提供安装OmniStateStore的详细指导。|
|视频课程|OmniRuntime特性大揭秘|提供操作视频，帮助开发者在鲲鹏服务器上了解、使能OmniRuntime特性。|

## 安全声明<a name="ZH-CN_TOPIC_0000002518267674"></a>

### 防病毒软件例行检查<a name="ZH-CN_TOPIC_0000002550027433"></a>

定期开展对集群和Spark组件的防病毒扫描，防病毒例行检查会帮助集群免受病毒、恶意代码、间谍软件以及恶意程序，降低系统瘫痪、信息泄露等风险。建议使用业界主流防病毒软件进行防病毒检查。

### 日志控制<a name="ZH-CN_TOPIC_0000002518427584"></a>

- 检查系统是否可以限制单个日志文件的大小。
- 检查日志空间占满后，是否存在机制进行清理。

### 漏洞修复<a name="ZH-CN_TOPIC_0000002549907427"></a>

为保证生产环境的安全，降低被攻击的风险，请开启防火墙，并定期修复以下漏洞。

- 操作系统漏洞
- JDK漏洞
- Hadoop及Spark漏洞
- ZooKeeper漏洞
- Kerberos漏洞
- OpenSSL漏洞
- 其他相关组件漏洞

    以CVE-2021-37137为例。

    漏洞描述

    Netty 4.1.17版本存在两个Content-Length的http header可能会发生混淆的风险通告，漏洞编号：CVE-2021-37137。

    本系统使用hdfs-ceph（version 3.2.0）服务作为存算分离的存储对象，它因依赖aws-java-sdk-bundle-1.11.375.jar而涉及该漏洞。建议用户及时更新漏洞补丁进行防护，以免遭受黑客攻击。

    影响范围

    Netty 4.1.68及以前版本。

    修复建议

    目前厂商已发布升级补丁以修复漏洞，请参见[Github](https://github.com/netty/netty/security/advisories/GHSA-9vjp-v76f-g363)修复漏洞。

### SSH加固<a name="ZH-CN_TOPIC_0000002518267676"></a>

在部署安装过程中，需要通过SSH连接服务器。由于root用户拥有最高权限，直接使用root用户登录服务器可能会存在安全风险。建议您使用普通用户登录服务器进行安装部署，并建议您通过配置禁止root用户SSH登录的选项，来提升系统安全性。操作步骤：

用户登录系统后检查“/etc/ssh/sshd\_config”配置项“PermitRootLogin“。

- 如果显示no，说明禁止了root用户SSH登录。
- 如果显示yes，说明需要修改PermitRootLogin为no。

## 贡献声明<a name="ZH-CN_TOPIC_0000002518267678"></a>

- 提交错误报告：如果您在OmniStateStore中发现了一个不存在安全问题的漏洞，请在OmniStateStore仓库中的Issues中搜索，以防该漏洞被重复提交，如果找不到漏洞可以创建一个新的Issues。如果发现了一个安全问题请不要将其公开，请参阅安全问题处理方式。提交错误报告时应该包含完整信息。
- 安全问题处理：本项目中对安全问题处理的形式，请通过邮箱通知项目核心人员确认编辑。
- 解决现有问题：通过查看仓库的Issues列表可以发现需要处理的问题信息，可以尝试解决其中的某个问题。
- 如何提出新功能：请使用Issues的Feature标签进行标记，我们会定期处理和确认开发。
- 开始贡献：
    - Fork本项目的仓库。
    - Clone到本地。
    - 创建开发分支。
    - 本地测试：提交前请通过所有单元测试，包括新增的测试用例。
    - 提交代码。
    - 新建Pull Request。
    - 代码检视：您需要根据评审意见修改代码，并重新提交更新。此流程可能涉及多轮迭代。
    - 当您的PR获得足够数量的检视者批准后，Committer会进行最终审核。
    - 审核和测试通过后，CI会将您的PR合并入到项目的主干分支。

## 免责声明<a name="ZH-CN_TOPIC_0000002518427586"></a>

**致OmniStateStore使用者**

- 本工具仅供调试和开发之用，使用者需自行承担使用风险，并理解以下内容：
    - 数据处理及删除：用户在使用本工具过程中产生的数据属于用户责任范畴。建议用户在使用完毕后及时删除相关数据，以防信息泄露。
    - 数据保密与传播：使用者了解并同意不得将通过本工具产生的数据随意外发或传播。对于由此产生的信息泄露、数据泄露或其他不良后果，本工具及其开发者概不负责。
    - 用户输入安全性：用户需自行保证输入的命令行的安全性，并承担因输入不当而导致的任何安全风险或损失。对于输入命令行不当所导致的问题，本工具及其开发者概不负责。

- 免责声明范围：本免责声明适用于所有使用本工具的个人或实体。使用本工具即表示您同意并接受本声明的内容，并愿意承担因使用该功能而产生的风险和责任，如有异议请停止使用本工具。
- 在使用本工具之前，请**谨慎阅读并理解以上免责声明的内容**。对于使用本工具所产生的任何问题或疑问，请及时联系开发者。

**致数据所有者**

如果您不希望您的模型或数据集等信息在OmniStateStore中被提及，或希望更新OmniStateStore中有关的描述，请在GitCode提交issue，我们将根据您的issue要求删除或更新您相关描述。衷心感谢您对OmniStateStore的理解和贡献。

## 关于社区<a name="ZH-CN_TOPIC_0000002518727184"></a>

OmniStateStore由华为公司的下列部门联合贡献：

- 鲲鹏计算BoostKit开发部

感谢来自社区的每一个PR，欢迎贡献OmniStateStore！

## 贡献、建议与交流<a name="ZH-CN_TOPIC_0000002518427588"></a>

欢迎大家为社区做贡献。如果有任何疑问或建议，请提交[Issues](https://gitcode.com/openeuler/OmniStateStore)，我们会尽快回复。感谢您的支持。

## License<a name="ZH-CN_TOPIC_0000002549907429"></a>

OmniStateStore的使用许可证，具体请参见[LICENSE](./LICENSE)文件。

docs目录下的文档适用CC-BY 4.0许可证，具体请参见[LICENSE](/LICENSE)文件。

## 说明

此开源项目非华为产品，仅提供有限支持。
