# 词库来源

- 原 150 词：`words.tsv`，本工程原有中文释义和原创双语例句，全部保留。
- 新增 850 词：ECDICT（https://github.com/skywind3000/ECDICT），按五个领域筛选，每领域新增 170 词；保留原始中文释义及音标，不补造缺失例句。
- 上游 CSV：https://raw.githubusercontent.com/skywind3000/ECDICT/master/ecdict.csv
- 取得日期：2026-09-17；完整文件 65,933,428 字节。
- SHA-256：`1a6947e04785db63613a92e14903cdae7954f7e84860b10e68e5c7cbb3f9c3cf`。
- 上游 MIT 许可原文见 `ECDICT-LICENSE.txt`，同一许可随应用 rawfile 资源分发。
- `scripts/expand-dictionary.py` 中保留领域候选表及确定性筛选规则；`scripts/prepare-data.mjs` 合并并强制验证 1000 个唯一词条。

运行时音频：Free Dictionary API（https://dictionaryapi.dev/）返回的 HTTPS MP3。每个下载文件的来源 URL、原录音来源及许可记录在缓存 `index.json`，未修改音频。此工程不附带或重新分发全量网络录音。
