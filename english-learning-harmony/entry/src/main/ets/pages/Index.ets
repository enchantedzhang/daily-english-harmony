import { common } from '@kit.AbilityKit';
import { CalendarCell, DayLesson, LearningSettings, LearningState, WordItem, TOPICS,
  availableWords, calendarCells, dateKey, defaultSettings, emptyLesson, parseDate, toggleReviewed, selectedTopics, isWorkday } from '../model/LearningCore';
import { learningStore } from '../service/LearningStore';
import { calendarReminders } from '../service/CalendarReminderService';
import { speechService } from '../service/SpeechService';
import { recordedAudio } from '../service/RecordedAudioService';
import { discoverWords, lookupWord } from '../service/DictionaryService';
import { WordCard } from '../components/WordCard';
import { audioCache } from '../service/AudioCacheService';
import { audioUpdates } from '../service/AudioUpdates';
import { workCalendar } from '../service/WorkCalendarService';

@Entry
@Component
struct Index {
  @State tab: number = 0;
  @State today: string = dateKey(new Date());
  @State selectedDate: string = dateKey(new Date());
  @State year: number = new Date().getFullYear();
  @State month: number = new Date().getMonth();
  @State cells: CalendarCell[] = [];
  @State lessons: DayLesson[] = [];
  @State lesson: DayLesson = emptyLesson(dateKey(new Date()));
  @State settings: LearningSettings = defaultSettings();
  @State topics: string[] = TOPICS.slice();
  @State draftCount: number = 5;
  @State draftTopics: string[] = ['日常交流'];
  @State draftAudio: boolean = true;
  @State audioStatus: string = '等待更新音频';
  @State backgroundStatus: string = '';
  @State workCalendarStatus: string = '中国大陆法定工作日（含调休）';
  @State draftHour: number = 9;
  @State draftMinute: number = 0;
  @State draftReminder: boolean = false;
  @State draftOnline: boolean = false;
  @State draftSpeed: number = 1;
  @State busy: boolean = false;
  @State ready: boolean = false;
  @State message: string = '';
  @State warning: boolean = false;
  @State speaking: boolean = false;
  @State query: string = '';
  @State domain: string = '';
  @State candidates: WordItem[] = [];
  @State selectedWords: string[] = [];
  @State onlineResult: WordItem | undefined = undefined;
  @State reminderStatus: string = '尚未启用提醒';
  @State wordCount: number = 1000;
  @StorageLink('foregroundVersion') @Watch('onForegroundChanged') foregroundVersion: number = 0;
  @StorageLink('appForeground') appForeground: boolean = true;
  private starting: boolean = false;
  private clockId: number = -1;

  aboutToAppear(): void {
    this.cells = calendarCells(this.year, this.month);
    this.initialize();
    this.clockId = setInterval(() => {
      this.audioStatus = audioCache.status;
      this.backgroundStatus = audioUpdates.backgroundStatus;
      if (this.appForeground && this.ready && !this.busy && dateKey(new Date()) !== this.today) { this.resume(); }
    }, 1000);
  }

  aboutToDisappear(): void {
    if (this.clockId !== -1) { clearInterval(this.clockId); }
    speechService.release();
    recordedAudio.stop();
    audioUpdates.stop();
  }

  onForegroundChanged(): void {
    if (this.ready) { audioUpdates.start(this.context()); }
    if (this.ready && !this.busy) { this.resume(); }
  }

  private context(): common.UIAbilityContext {
    return this.getUIContext().getHostContext() as common.UIAbilityContext;
  }

  private tell(message: string, warning: boolean = false): void {
    this.message = message;
    this.warning = warning;
  }

  private error(error: Error): void { this.tell(error.message || '操作失败，请重试。', true); }

  private async initialize(): Promise<void> {
    if (this.starting) { return; }
    this.starting = true;
    this.busy = true;
    try {
      await learningStore.initialize(this.context());
      this.ready = true;
      this.reload();
      this.loadDraft();
      if (this.appForeground) { audioUpdates.start(this.context()); }
      audioUpdates.schedule(this.settings.audioAutoUpdate !== false);
      if (learningStore.recoveryMessage) { this.tell(learningStore.recoveryMessage, true); }
      await this.updateWorkCalendar();
      if (this.settings.reminder || learningStore.snapshot().reminderSyncPending) { await this.sync(false); }
    } catch (error) { this.error(error as Error); }
    finally { this.busy = false; this.starting = false; }
  }

  private async resume(): Promise<void> {
    this.busy = true;
    try {
      const oldToday: string = this.today;
      learningStore.refresh();
      this.today = dateKey(new Date());
      if (this.selectedDate === oldToday) { this.selectDate(this.today); }
      this.reload();
      if (this.appForeground) { audioUpdates.start(this.context()); }
      await this.updateWorkCalendar();
      if (this.settings.reminder || learningStore.snapshot().reminderSyncPending) { await this.sync(false); }
    } catch (error) { this.error(error as Error); }
    finally { this.busy = false; }
  }

  private reload(): void {
    const state: LearningState = learningStore.snapshot();
    this.settings = state.settings;
    this.lessons = state.lessons;
    this.lesson = this.lessons.find((item: DayLesson) => item.date === this.selectedDate) || emptyLesson(this.selectedDate);
    const topics: string[] = TOPICS.slice();
    state.customWords.forEach((word: WordItem) => { if (!topics.includes(word.topic)) { topics.push(word.topic); } });
    this.topics = topics;
    this.wordCount = availableWords(learningStore.words(), state.customWords, '全部领域').length;
    const future: DayLesson[] = this.lessons.filter((item: DayLesson) => item.scheduledAt > Date.now()).sort((a: DayLesson, b: DayLesson) => a.scheduledAt - b.scheduledAt);
    this.reminderStatus = future.length > 0 ? `已登记 ${future.length} 天提醒 · 至 ${future[future.length - 1].date}` :
      this.settings.reminder ? '尚无已登记提醒，请点击同步提醒' : '尚未启用提醒';
    if (state.reminderSyncPending) { this.reminderStatus = '提醒设置待同步，请点击同步提醒'; }
  }

  private loadDraft(): void {
    this.draftCount = this.settings.count;
    this.draftTopics = selectedTopics(this.settings);
    this.draftAudio = this.settings.audioAutoUpdate !== false;
    this.draftHour = this.settings.hour;
    this.draftMinute = this.settings.minute;
    this.draftReminder = this.settings.reminder;
    this.draftOnline = this.settings.online;
    this.draftSpeed = this.settings.speechRate;
  }

  private selectDate(date: string): void {
    this.selectedDate = date;
    const selected: Date = parseDate(date);
    this.year = selected.getFullYear();
    this.month = selected.getMonth();
    this.cells = calendarCells(this.year, this.month);
    this.lesson = this.lessons.find((item: DayLesson) => item.date === date) || emptyLesson(date);
    this.onlineResult = undefined;
  }

  private changeMonth(delta: number): void {
    const date: Date = new Date(this.year, this.month + delta, 1, 12);
    this.year = date.getFullYear(); this.month = date.getMonth();
    this.cells = calendarCells(this.year, this.month);
  }

  private async sync(requestPermission: boolean): Promise<void> {
    try {
      const count: number = await calendarReminders.synchronize(this.context(), learningStore, requestPermission);
      this.reload();
      this.reminderStatus = this.settings.reminder ? this.reminderStatus : '提醒已关闭，未来日历提醒已移除';
      if (requestPermission) { this.tell(this.settings.reminder ? `已登记 ${count} 天提醒。请保持系统日历通知开启。` : '已关闭未来提醒。'); }
    } catch (error) {
      this.reload();
      this.reminderStatus = '提醒未完全同步，请重试';
      throw error;
    }
  }

  private async saveSettings(): Promise<void> {
    if (this.busy) { return; }
    this.busy = true;
    try {
      const wasEnabled: boolean = this.settings.reminder;
      const next: LearningSettings = { count: this.draftCount, topic: this.draftTopics[0], topics: this.draftTopics.slice(), hour: this.draftHour, minute: this.draftMinute,
        reminder: this.draftReminder, online: this.draftOnline, audioAutoUpdate: this.draftAudio, speechRate: this.draftSpeed };
      learningStore.saveSettings(next);
      this.reload();
      audioUpdates.changed();
      audioUpdates.schedule(this.settings.audioAutoUpdate !== false);
      this.tell('设置已保存。数量和领域从明天起生效，今天的词单保持不变。');
      if (next.reminder || wasEnabled || learningStore.snapshot().reminderSyncPending) { await this.sync(true); }
    } catch (error) { this.error(error as Error); }
    finally { this.busy = false; }
  }

  private async manualSync(): Promise<void> {
    if (this.busy) { return; }
    this.busy = true;
    try { learningStore.refresh(); await this.updateWorkCalendar(true); await this.sync(true); }
    catch (error) { this.error(error as Error); }
    finally { this.busy = false; }
  }

  private async updateWorkCalendar(force: boolean = false): Promise<void> {
    await workCalendar.update(learningStore, force);
    this.workCalendarStatus = workCalendar.status;
    this.reload();
  }

  private learn(word: string): void {
    if (this.busy || this.selectedDate > this.today) { return; }
    try {
      const next: LearningState = learningStore.snapshot();
      const lesson: DayLesson | undefined = next.lessons.find((item: DayLesson) => item.date === this.selectedDate);
      if (lesson) { toggleReviewed(lesson, word); learningStore.commit(next); this.reload(); }
    } catch (error) { this.error(error as Error); }
  }

  private async speak(text: string): Promise<void> {
    if (this.speaking) { recordedAudio.stop(); speechService.stop(); return; }
    this.speaking = true;
    try {
      if (!await recordedAudio.play(text, this.settings.speechRate, this.context())) {
        await speechService.speak(text, this.settings.speechRate);
      }
    }
    catch (_) { this.tell('语音暂不可用，请检查手机的系统语音服务或语音资源。', true); }
    finally { this.speaking = false; }
  }

  private async lookup(word: string): Promise<void> {
    if (this.busy || !this.settings.online) { return; }
    this.busy = true;
    try { this.onlineResult = await lookupWord(word); this.tell('已查询英文原释义；本地中文释义保留。'); }
    catch (error) { this.error(error as Error); }
    finally { this.busy = false; }
  }

  private async discover(): Promise<void> {
    if (this.busy || !this.settings.online) { return; }
    this.busy = true;
    try {
      this.candidates = await discoverWords(this.domain);
      this.selectedWords = this.candidates.map((word: WordItem) => word.word);
      this.tell(`找到 ${this.candidates.length} 个相关词，请确认后加入词库。`);
    } catch (error) { this.error(error as Error); }
    finally { this.busy = false; }
  }

  private addCandidates(): void {
    try {
      const selected: WordItem[] = this.candidates.filter((word: WordItem) => this.selectedWords.includes(word.word));
      if (selected.length === 0) { this.tell('请先选择要加入的词。', true); return; }
      const count: number = learningStore.addWords(selected);
      this.reload();
      if (!this.draftTopics.includes(selected[0].topic)) { this.draftTopics = this.draftTopics.concat([selected[0].topic]); }
      this.candidates = [];
      this.tell(`已加入 ${count} 个新词。已选中该领域，点击“保存设置”后生效。`);
    } catch (error) { this.error(error as Error); }
  }

  private async exportBackup(): Promise<void> {
    this.busy = true;
    try { if (await learningStore.exportBackup(this.context())) { this.tell('备份已保存到所选位置。'); } }
    catch (error) { this.error(error as Error); }
    finally { this.busy = false; }
  }

  private async importBackup(): Promise<void> {
    this.busy = true;
    try {
      if (await learningStore.importBackup(this.context())) {
        this.reload();
        this.tell('已合并历史和自定义词库，当前已有词单与设置保持不变。');
      }
    } catch (error) { this.error(error as Error); }
    finally { this.busy = false; }
  }

  private history(): DayLesson[] {
    const query: string = this.query.trim().toLowerCase();
    return this.lessons.filter((lesson: DayLesson) => lesson.date <= this.today &&
      (!query || lesson.date.includes(query) || lesson.words.some((word: WordItem) => word.word.toLowerCase().includes(query) || word.meaning.includes(query))))
      .sort((a: DayLesson, b: DayLesson) => b.date.localeCompare(a.date));
  }

  private dayStatus(): string {
    if (this.lesson.origin === '原历史记录') { return '原历史推送 · 已导入'; }
    const workday: boolean | undefined = isWorkday(this.selectedDate, learningStore.snapshot().workCalendars);
    if (workday === undefined) { return '年度调休安排待更新 · 暂不推送'; }
    if (!workday) { return '休息日不推送 · 可自主学习'; }
    if (this.lesson.scheduledAt > Date.now()) { return '已登记系统日历提醒'; }
    if (this.lesson.scheduledAt > 0) { return '已到提醒时间 · 送达状态由系统决定'; }
    return this.selectedDate > this.today ? '未来词单预览 · 尚未登记提醒' : '本地每日词单';
  }

  @Builder
  header() {
    Row() {
      Column({ space: 5 }) {
        Text('每日英语').fontSize(27).fontWeight(FontWeight.Bold).fontColor('#26324A')
        Text('每天一点，慢慢积累').fontSize(13).fontColor('#828BA0')
      }.alignItems(HorizontalAlign.Start)
      Blank()
      Text('EN').fontSize(18).fontWeight(FontWeight.Bold).fontColor('#5267D8').width(44).height(44)
        .textAlign(TextAlign.Center).backgroundColor('#E9EDFF').borderRadius(15)
    }.width('100%').padding({ left: 22, right: 22, top: 16, bottom: 18 })
  }

  @Builder
  calendar() {
    Column({ space: 14 }) {
      Row() {
        Text(`${this.year} 年 ${this.month + 1} 月`).fontSize(20).fontWeight(FontWeight.Bold).fontColor('#27324B')
        Blank()
        Button('‹').width(36).height(36).fontSize(24).backgroundColor('#F2F4FA').fontColor('#5267D8')
          .onClick(() => this.changeMonth(-1)).accessibilityText('上个月')
        Button('›').width(36).height(36).fontSize(24).backgroundColor('#F2F4FA').fontColor('#5267D8').margin({ left: 8 })
          .onClick(() => this.changeMonth(1)).accessibilityText('下个月')
      }.width('100%')
      Row() {
        ForEach(['一', '二', '三', '四', '五', '六', '日'], (day: string) => {
          Text(day).fontSize(12).fontColor('#9BA2B2').textAlign(TextAlign.Center).layoutWeight(1)
        })
      }.width('100%')
      Grid() {
        ForEach(this.cells, (cell: CalendarCell) => {
          GridItem() {
            Column({ space: 3 }) {
              Text(`${cell.day}`).fontSize(15).fontWeight(cell.date === this.today ? FontWeight.Bold : FontWeight.Normal)
                .fontColor(cell.date === this.selectedDate ? '#FFFFFF' : cell.inMonth ? '#36415B' : '#C0C5D0')
              Circle().width(4).height(4).fill(this.lessons.some((lesson: DayLesson) => lesson.date === cell.date) ?
                cell.date === this.selectedDate ? '#FFFFFF' : cell.date > this.today ? '#BBC3DD' : '#5267D8' : Color.Transparent)
            }.width('100%').height(40).justifyContent(FlexAlign.Center).borderRadius(12)
              .backgroundColor(cell.date === this.selectedDate ? '#5267D8' : cell.date === this.today ? '#EEF1FF' : Color.Transparent)
              .onClick(() => this.selectDate(cell.date)).accessibilityText(`${cell.date}${cell.date === this.today ? ' 今天' : ''}`)
          }
        }, (cell: CalendarCell) => cell.key)
      }.columnsTemplate('1fr 1fr 1fr 1fr 1fr 1fr 1fr').rowsTemplate('1fr 1fr 1fr 1fr 1fr 1fr').height(258).rowsGap(3).columnsGap(3)
      Row() {
        Text('● 有词单   ·   浅色为未来计划').fontSize(11).fontColor('#9099AD')
        Blank()
        Button('回到今天').fontSize(12).height(28).fontColor('#5267D8').backgroundColor(Color.Transparent).padding(0)
          .onClick(() => this.selectDate(this.today))
      }.width('100%')
    }.padding(18).backgroundColor(Color.White).borderRadius(22).width('100%')
  }

  @Builder
  dayWords() {
    Row() {
      Column({ space: 5 }) {
        Text(this.selectedDate === this.today ? '今日学习' : this.selectedDate).fontSize(20).fontWeight(FontWeight.Bold).fontColor('#27324B')
        if (this.lesson.words.length > 0) { Text(this.dayStatus()).fontSize(11).fontColor('#8B94A8') }
      }.alignItems(HorizontalAlign.Start)
      Blank()
      Text(`${this.lesson.reviewed.length} / ${this.lesson.words.length}`).fontSize(14).fontColor('#5267D8')
    }.width('100%').margin({ top: 8 })
    if (this.lesson.words.length === 0) {
      Column({ space: 10 }) {
        Text('这一天还没有词单').fontSize(17).fontColor('#6C7891')
        Text(this.selectedDate > this.today ? '启用提醒后会准备未来 30 天的词单。' : '没有记录的日期会保持空白。').fontSize(13).fontColor('#939CAF')
      }.width('100%').padding(28).backgroundColor(Color.White).borderRadius(20)
    } else {
      ForEach(this.lesson.words, (word: WordItem) => {
        WordCard({ item: word, learned: this.lesson.reviewed.includes(word.word), review: this.lesson.reviewWords.includes(word.word),
          canLearn: this.selectedDate <= this.today, canLookup: this.settings.online, busy: this.busy,
          onSpeak: (text: string): void => { this.speak(text); }, onLearn: (value: string): void => { this.learn(value); },
          onLookup: (value: string): void => { this.lookup(value); } })
      }, (word: WordItem) => `${this.selectedDate}:${word.word}`)
      if (this.lesson.phrase) {
        Column({ space: 8 }) {
          Text('历史短语').fontSize(12).fontColor('#8B94A8')
          Text(this.lesson.phrase).fontSize(18).fontColor('#5267D8')
          Button('朗读短语').fontSize(12).fontColor('#5267D8').backgroundColor('#EEF1FF').onClick(() => this.speak(this.lesson.phrase))
        }.alignItems(HorizontalAlign.Start).width('100%').padding(18).backgroundColor('#EDF1FF').borderRadius(20)
      }
    }
    if (this.onlineResult) {
      Row() {
        Text('在线词典 · 英文原释义').fontSize(15).fontColor('#5267D8')
        Blank()
        Button('关闭').fontSize(12).fontColor('#5267D8').backgroundColor(Color.Transparent).onClick(() => this.onlineResult = undefined)
      }.width('100%')
      WordCard({ item: this.onlineResult as WordItem, canLearn: false, canLookup: false, onSpeak: (text: string): void => { this.speak(text); } })
    }
  }

  @Builder
  historyPage() {
    Text('学习足迹').fontSize(23).fontWeight(FontWeight.Bold).fontColor('#27324B').width('100%')
    Text('历史词单与已到期计划 · 不等同于系统送达回执').fontSize(12).fontColor('#8B94A8').width('100%')
    TextInput({ text: this.query, placeholder: '搜索日期、单词或中文释义' }).height(46).backgroundColor(Color.White)
      .onChange((value: string) => this.query = value)
    if (this.history().length === 0) { Text('没有找到相关记录').fontSize(15).fontColor('#8B94A8').padding(30) }
    ForEach(this.history(), (lesson: DayLesson) => {
      Column({ space: 9 }) {
        Row() {
          Text(lesson.date).fontSize(17).fontWeight(FontWeight.Bold).fontColor('#27324B')
          Blank()
          Text(`${lesson.reviewed.length}/${lesson.words.length} 已学  ›`).fontSize(12).fontColor('#5267D8')
        }.width('100%')
        Text(lesson.words.map((word: WordItem) => word.word).join('  ·  ')).fontSize(14).lineHeight(23).fontColor('#78839B').width('100%')
        Text(lesson.origin === '原历史记录' ? '原历史推送' : lesson.scheduledAt > 0 ? '已登记过日历提醒' : '本地词单').fontSize(11).fontColor('#A0A7B5')
      }.alignItems(HorizontalAlign.Start).width('100%').padding(18).backgroundColor(Color.White).borderRadius(18)
        .onClick(() => { this.selectDate(lesson.date); this.tab = 0; })
    }, (lesson: DayLesson) => lesson.date)
  }

  @Builder
  settingsPage() {
    Text('按自己的节奏学习').fontSize(23).fontWeight(FontWeight.Bold).fontColor('#27324B').width('100%')
    Column({ space: 18 }) {
      Row() {
        Text('每个领域每日词数').fontSize(16).fontColor('#27324B')
        Blank()
        Button('−').width(34).height(34).backgroundColor('#EEF1FF').fontColor('#5267D8')
          .enabled(!this.busy && this.draftCount > 1).onClick(() => this.draftCount--)
        Text(`${this.draftCount}`).width(40).textAlign(TextAlign.Center).fontSize(19).fontColor('#5267D8')
        Button('+').width(34).height(34).backgroundColor('#EEF1FF').fontColor('#5267D8')
          .enabled(!this.busy && this.draftCount < 20).onClick(() => this.draftCount++)
      }.width('100%')
      Text(`学习领域（可多选） · 每天共 ${this.draftCount * this.draftTopics.length} 词`).fontSize(16).fontColor('#27324B').width('100%')
      Flex({ wrap: FlexWrap.Wrap }) {
        ForEach(this.topics.filter((topic: string) => topic !== '全部领域'), (topic: string) => {
          Button(topic).fontSize(13).height(36).margin({ right: 8, bottom: 8 }).enabled(!this.busy)
            .fontColor(this.draftTopics.includes(topic) ? '#FFFFFF' : '#687894').backgroundColor(this.draftTopics.includes(topic) ? '#5267D8' : '#F1F3FA')
            .onClick(() => {
              if (!this.draftTopics.includes(topic)) { this.draftTopics = this.draftTopics.concat([topic]); }
              else if (this.draftTopics.length > 1) { this.draftTopics = this.draftTopics.filter((value: string) => value !== topic); }
              else { this.tell('至少选择一个领域。'); }
            })
        })
      }.width('100%')
      Text('数量和领域修改从明天生效；当前词库用完后会安排复习，不会虚构新词。').fontSize(12).lineHeight(19).fontColor('#939CAF').width('100%')
    }.padding(20).backgroundColor(Color.White).borderRadius(20).width('100%')
    Column({ space: 14 }) {
      Row() {
        Text('工作日提醒（含调休）').fontSize(16).fontColor('#27324B')
        Blank()
        Toggle({ type: ToggleType.Switch, isOn: this.draftReminder }).selectedColor('#5267D8').enabled(!this.busy)
          .onChange((value: boolean) => this.draftReminder = value)
      }.width('100%')
      TimePicker({ selected: new Date(2026, 0, 1, this.draftHour, this.draftMinute) }).useMilitaryTime(true).height(110)
        .enabled(!this.busy).onChange((value: TimePickerResult) => { this.draftHour = value.hour ?? 9; this.draftMinute = value.minute ?? 0; })
      Text(this.reminderStatus).fontSize(12).fontColor('#5267D8').width('100%')
      Text(this.workCalendarStatus).fontSize(12).fontColor('#5267D8').width('100%')
      Text('按中国大陆法定节假日安排：补班周末推送，放假工作日不推送。内置 2026 年安排，联网自动更新并离线保存；未取得完整年度安排时暂停该年份推送。休息日仍可自主学习。').fontSize(12).lineHeight(20).fontColor('#939CAF').width('100%')
      Text('通过系统日历提醒，仅法定工作日每天一条通知包含当天单词。每次打开应用同步未来 30 个自然日；长时间不打开，计划到期后需重新同步。请开启系统日历通知。').fontSize(12).lineHeight(20).fontColor('#939CAF').width('100%')
      Button('同步提醒 / 重试').fontSize(13).backgroundColor('#EEF1FF').fontColor('#5267D8').enabled(!this.busy)
        .onClick(() => this.manualSync())
    }.padding(20).backgroundColor(Color.White).borderRadius(20).width('100%')
    Column({ space: 14 }) {
      Text('发音速度').fontSize(16).fontColor('#27324B').width('100%')
      Row({ space: 10 }) {
        ForEach([0.75, 1, 1.25], (speed: number) => {
          Button(`${speed}×`).fontSize(14).layoutWeight(1).enabled(!this.busy)
            .backgroundColor(this.draftSpeed === speed ? '#5267D8' : '#EEF1FF').fontColor(this.draftSpeed === speed ? '#FFFFFF' : '#5267D8')
            .onClick(() => this.draftSpeed = speed)
        })
      }.width('100%')
      Row() {
        Text('允许在线查词').fontSize(16).fontColor('#27324B')
        Blank()
        Toggle({ type: ToggleType.Switch, isOn: this.draftOnline }).selectedColor('#5267D8').enabled(!this.busy)
          .onChange((value: boolean) => this.draftOnline = value)
      }.width('100%')
      Text('开启并保存后，可直接访问公共词典。仅发送查询词或领域关键词。例句和未缓存单词使用手机系统语音。').fontSize(12).lineHeight(20).fontColor('#939CAF').width('100%')
      Row() {
        Text('联网自动更新音频').fontSize(16).fontColor('#27324B')
        Blank()
        Toggle({ type: ToggleType.Switch, isOn: this.draftAudio }).selectedColor('#5267D8').enabled(!this.busy)
          .onChange((value: boolean) => this.draftAudio = value)
      }.width('100%')
      Text('只保留过去 7 天、今天、未来 20 天词单的单词音频，重复词只存一份。支持 Wi-Fi 和移动网络；打开应用、恢复联网时自动补齐，后台执行由系统调度。历史文字一直保留。').fontSize(12).lineHeight(20).fontColor('#939CAF').width('100%')
      Text(this.audioStatus).fontSize(12).fontColor('#5267D8').width('100%')
      Text(this.backgroundStatus).fontSize(12).fontColor('#939CAF').width('100%')
      Button('立即更新音频 / 重试').fontSize(13).backgroundColor('#EEF1FF').fontColor('#5267D8')
        .enabled(this.settings.audioAutoUpdate !== false).onClick(() => audioUpdates.update(true))
    }.padding(20).backgroundColor(Color.White).borderRadius(20).width('100%')
    Button(this.busy ? '处理中…' : '保存设置').width('100%').height(48).backgroundColor('#5267D8').fontColor(Color.White)
      .enabled(!this.busy).onClick(() => this.saveSettings())
    this.wordLibrary()
    Button('导出学习备份').width('100%').height(44).fontColor('#5267D8').backgroundColor('#E9EDFF').enabled(!this.busy)
      .onClick(() => this.exportBackup())
    Button('从备份合并历史').width('100%').height(44).fontColor('#5267D8').backgroundColor('#E9EDFF').enabled(!this.busy)
      .onClick(() => this.importBackup())
    Text('每日英语 1.0 · 个人版\n本地保存，无自建后端。卸载前请导出备份。\n在线词源：Datamuse / WordNet / Wiktionary / Free Dictionary API。\nDatamuse 公告：2027 年起需要 API Key；届时在线领域扩充可能不可用，已下载词库不受影响。')
      .fontSize(11).lineHeight(18).fontColor('#A0A7B5').width('100%').padding({ top: 8, bottom: 18 })
  }

  @Builder
  wordLibrary() {
    Column({ space: 14 }) {
      Text(`扩充词库 · 已有 ${this.wordCount} 个词`).fontSize(17).fontWeight(FontWeight.Bold).fontColor('#27324B').width('100%')
      TextInput({ text: this.domain, placeholder: '英文领域，如 finance / software' }).height(44).backgroundColor('#F5F6FA')
        .maxLength(60).enabled(!this.busy).onChange((value: string) => this.domain = value)
      Button('联网寻找相关单词').width('100%').fontColor('#5267D8').backgroundColor('#EEF1FF')
        .enabled(this.settings.online && !this.busy).onClick(() => this.discover())
      Text(this.settings.online ? '结果是相关候选词，释义为英文，请检查后加入。' : '先开启“允许在线查词”并保存设置。').fontSize(12).fontColor('#939CAF').width('100%')
      if (this.candidates.length > 0) {
        Row() {
          Text(`已选 ${this.selectedWords.length} / ${this.candidates.length}`).fontSize(13).fontColor('#5267D8')
          Blank()
          Button(this.selectedWords.length === this.candidates.length ? '全不选' : '全选').fontSize(12).backgroundColor('#EEF1FF').fontColor('#5267D8')
            .onClick(() => this.selectedWords = this.selectedWords.length === this.candidates.length ? [] : this.candidates.map((word: WordItem) => word.word))
        }.width('100%')
        ForEach(this.candidates, (word: WordItem) => {
          Row({ space: 10 }) {
            Checkbox({ name: word.word }).select(this.selectedWords.includes(word.word)).selectedColor('#5267D8')
              .onChange((selected: boolean) => {
                this.selectedWords = selected ? (this.selectedWords.includes(word.word) ? this.selectedWords : this.selectedWords.concat([word.word])) : this.selectedWords.filter((value: string) => value !== word.word);
              })
            Column({ space: 3 }) {
              Text(word.word).fontSize(15).fontColor('#27324B')
              Text(word.meaning).fontSize(12).fontColor('#939CAF').maxLines(3).textOverflow({ overflow: TextOverflow.Ellipsis })
            }.alignItems(HorizontalAlign.Start).layoutWeight(1)
          }.width('100%')
        }, (word: WordItem) => word.word)
        Button('加入选中的词').width('100%').backgroundColor('#5267D8').fontColor(Color.White).enabled(!this.busy).onClick(() => this.addCandidates())
      }
    }.padding(20).backgroundColor(Color.White).borderRadius(20).width('100%')
  }

  build() {
    Column() {
      this.header()
      if (this.message) {
        Row({ space: 8 }) {
          Text(this.message).fontSize(12).lineHeight(18).fontColor(this.warning ? '#9B5C2C' : '#5267D8').layoutWeight(1)
          Button('×').width(28).height(28).padding(0).fontColor('#8790A3').backgroundColor(Color.Transparent).onClick(() => this.message = '')
        }.padding(12).margin({ left: 22, right: 22, bottom: 10 }).backgroundColor(this.warning ? '#FFF2E4' : '#EAF0FF').borderRadius(12)
      }
      if (this.speaking) {
        Button('正在朗读 · 点击停止').fontSize(12).fontColor('#5267D8').backgroundColor('#E9EDFF').height(32)
          .onClick(() => { recordedAudio.stop(); speechService.stop(); }).margin({ bottom: 8 })
      }
      if (this.ready) {
        Scroll() {
          Column({ space: 14 }) {
            if (this.tab === 0) {
              this.calendar()
              this.dayWords()
            }
            else if (this.tab === 1) { this.historyPage() }
            else { this.settingsPage() }
          }.padding({ left: 20, right: 20, bottom: 28 }).width('100%')
        }.layoutWeight(1).width('100%').scrollBar(BarState.Off)
        Row() {
          ForEach(['学习日历', '历史词单', '学习设置'], (label: string, index: number) => {
            Button(label).layoutWeight(1).height(48).fontSize(14).fontWeight(this.tab === index ? FontWeight.Bold : FontWeight.Normal)
              .backgroundColor(Color.Transparent).fontColor(this.tab === index ? '#5267D8' : '#99A1B3').enabled(!this.busy)
              .onClick(() => { this.tab = index; if (index === 2) { this.loadDraft(); } })
          })
        }.width('100%').padding({ left: 12, right: 12, top: 5, bottom: 5 }).backgroundColor(Color.White)
      } else {
        Column({ space: 20 }) {
          Text(this.busy ? '正在准备学习日历…' : '学习数据未能加载').fontSize(17).fontColor('#758099')
          if (!this.busy) { Button('重新加载').backgroundColor('#5267D8').onClick(() => this.initialize()) }
        }.layoutWeight(1).justifyContent(FlexAlign.Center)
      }
    }.width('100%').height('100%').backgroundColor('#F7F8FC')
  }
}
