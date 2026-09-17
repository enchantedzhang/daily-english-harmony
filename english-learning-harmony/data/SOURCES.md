# 词库来源

构词讲解见 `word-study.json`，首批 32 词。patient 的三条例句与双关联想来自用户提供内容；历史词源核对 Etymonline 的 patient、reliable、algorithm 条目，用中文独立简述。现代词缀及合成词采用教学拆解，参考 Cambridge 的词缀、合成词语法说明。逐词来源随数据保存。未复制来源站点整篇解释或词条，未将记忆联想标为词源，也未批量猜测剩余词条。

- 原 150 词：`words.tsv`，本工程原有中文释义和原创双语例句，全部保留。
- 新增 850 词：ECDICT（https://github.com/skywind3000/ECDICT），按五个领域筛选，每领域新增 170 词；保留原始中文释义及音标，不补造缺失例句。
- 上游 CSV：https://raw.githubusercontent.com/skywind3000/ECDICT/master/ecdict.csv
- 取得日期：2026-09-17；完整文件 65,933,428 字节。
- SHA-256：`1a6947e04785db63613a92e14903cdae7954f7e84860b10e68e5c7cbb3f9c3cf`。
- 上游 MIT 许可原文见 `ECDICT-LICENSE.txt`，同一许可随应用 rawfile 资源分发。
- `scripts/expand-dictionary.py` 中保留领域候选表及确定性筛选规则；`scripts/prepare-data.mjs` 合并并强制验证 1000 个唯一词条。

运行时音频：Free Dictionary API（https://dictionaryapi.dev/）返回的 HTTPS MP3。每个下载文件的来源 URL、原录音来源及许可记录在缓存 `index.json`，未修改音频。此工程不附带或重新分发全量网络录音。

工作日日历：NateScarlet/holiday-cn（MIT），2026 年数据于 2026-09-17 获取并对照国务院办公厅国办发明电〔2025〕7号核对。内置于 LearningCore.ts，版权许可随应用 rawfile/HOLIDAY-LICENSE.txt 提供。在线更新使用 https://cdn.jsdelivr.net/gh/NateScarlet/holiday-cn@master/{year}.json ，非自行部署的后端。排除词从筛选候选表移除，自动选入同领域下一词，保持每领域 200 词。
