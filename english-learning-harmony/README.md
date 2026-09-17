# 每日英语 · HarmonyOS 6

面向 HarmonyOS 6 的 ArkTS 学习应用原型。主流程：日历 → 日期词汇 → 单词朗读；设置中可配置每日单词数量、学习领域和推送时间。

## 当前原型

- `Index.ets`：日历首页、当天学习卡片、设置弹窗、历史日期入口
- `WordDetail.ets`：单词释义、例句、中文翻译、英式/美式发音按钮
- `Settings.ets`：每日数量、领域、推送开关和时间
- `LearningStore.ets`：Preferences 本地持久化接口；接入服务端时替换 `loadToday()` 即可
- `SpeechService.ets`：封装 `@kit.CoreSpeech` 的 TTS 调用位置

## 接入现有每日推送

保留 `daily-english-vocabulary-history.json` 作为服务端或构建脚本的去重历史：每次生成前读取已推送单词/短语，输出后追加日期记录。App 端只负责展示历史、筛选领域和播放语音。

在 DevEco Studio 中新建 HarmonyOS Stage 模板工程后，将 `entry/src/main/ets` 下对应文件替换为本目录文件，并按 `module.json5` 配置权限即可。
