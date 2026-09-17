import { common } from '@kit.AbilityKit';
import { fileIo, picker } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import { LearningSettings, LearningState, LegacyHistory, WordItem, dateKey, defaultSettings,
  importLegacy, mergeBackup, planLessons, validateState, validWord, allowedWord } from '../model/LearningCore';

export class LearningStore {
  private state: LearningState = { version: 1, reminderSyncPending: false, settings: defaultSettings(), lessons: [], customWords: [] };
  private bank: WordItem[] = [];
  private path: string = '';
  private ready: boolean = false;
  recoveryMessage: string = '';

  async initialize(context: common.UIAbilityContext): Promise<void> {
    if (this.ready) { return; }
    this.path = `${context.filesDir}/learning-v1.json`;
    const decoder: util.TextDecoder = util.TextDecoder.create('utf-8', { fatal: true });
    this.bank = JSON.parse(decoder.decodeToString(await context.resourceManager.getRawFileContent('words.json'))) as WordItem[];
    if (!Array.isArray(this.bank) || !this.bank.every(validWord)) { throw new Error('内置词库无法读取'); }
    if (fileIo.accessSync(this.path)) {
      try { this.state = this.read(this.path); }
      catch (_) {
        if (!fileIo.accessSync(`${this.path}.bak`)) { throw new Error('学习数据无法读取，未覆盖原数据。'); }
        this.state = this.read(`${this.path}.bak`);
        fileIo.copyFileSync(this.path, `${this.path}.damaged-${Date.now()}`);
        this.recoveryMessage = '主记录异常，已从上一次本地备份恢复。';
        fileIo.copyFileSync(`${this.path}.bak`, this.path);
      }
    } else if (fileIo.accessSync(`${this.path}.bak`)) {
      this.state = this.read(`${this.path}.bak`);
      this.recoveryMessage = '已从本地备份恢复学习记录。';
    } else {
      const text: string = decoder.decodeToString(await context.resourceManager.getRawFileContent('history.json'));
      this.state.lessons = importLegacy(JSON.parse(text) as LegacyHistory, this.bank);
    }
    this.ready = true;
    try { this.refresh(); }
    catch (error) { this.ready = false; throw error; }
  }

  private read(path: string): LearningState {
    const state: LearningState = JSON.parse(fileIo.readTextSync(path)) as LearningState;
    validateState(state);
    return state;
  }

  snapshot(): LearningState { return JSON.parse(JSON.stringify(this.state)) as LearningState; }
  words(): WordItem[] { return this.bank.slice(); }

  commit(state: LearningState): void {
    if (!this.ready) { throw new Error('数据尚未加载完成'); }
    validateState(state);
    const text: string = JSON.stringify(state);
    if (text === JSON.stringify(this.state) && fileIo.accessSync(this.path)) { return; }
    this.write(`${this.path}.tmp`, text);
    if (fileIo.accessSync(this.path)) { fileIo.copyFileSync(this.path, `${this.path}.bak`); }
    fileIo.renameSync(`${this.path}.tmp`, this.path);
    this.state = JSON.parse(JSON.stringify(state)) as LearningState;
  }

  private write(path: string, text: string): void {
    const file: fileIo.File = fileIo.openSync(path, fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try {
      const data: Uint8Array = new util.TextEncoder().encodeInto(text);
      let offset: number = 0;
      while (offset < data.byteLength) {
        const written: number = fileIo.writeSync(file.fd, data.slice(offset).buffer);
        if (written <= 0) { throw new Error('本地存储写入失败'); }
        offset += written;
      }
      fileIo.fsyncSync(file.fd);
    } finally { fileIo.closeSync(file); }
  }

  refresh(): void {
    const next: LearningState = this.snapshot();
    next.customWords = next.customWords.filter(allowedWord);
    next.lessons = planLessons(next, this.bank, dateKey(new Date()), false);
    if (JSON.stringify(next) !== JSON.stringify(this.state) || !fileIo.accessSync(this.path)) {
      if (next.settings.reminder) { next.reminderSyncPending = true; }
      this.commit(next);
    }
  }

  saveSettings(settings: LearningSettings): void {
    const next: LearningState = this.snapshot();
    next.reminderSyncPending = next.reminderSyncPending || next.settings.reminder || settings.reminder;
    next.settings = settings;
    next.lessons = planLessons(next, this.bank, dateKey(new Date()), true);
    this.commit(next);
  }

  addWords(words: WordItem[]): number {
    const next: LearningState = this.snapshot();
    let added: number = 0;
    for (const word of words) {
      if (validWord(word) && allowedWord(word) && !this.bank.concat(next.customWords).some((item: WordItem) => item.word === word.word && item.topic === word.topic)) {
        next.customWords.push(word);
        added++;
      }
    }
    this.commit(next);
    return added;
  }

  async exportBackup(context: common.UIAbilityContext): Promise<boolean> {
    const documentPicker: picker.DocumentViewPicker = new picker.DocumentViewPicker(context);
    const destinations: string[] = await documentPicker.save({ newFileNames: [`每日英语-${dateKey(new Date())}.json`], fileSuffixChoices: ['JSON|.json'] });
    if (destinations.length === 0) { return false; }
    this.write(destinations[0], JSON.stringify(this.state, null, 2));
    return true;
  }

  async importBackup(context: common.UIAbilityContext): Promise<boolean> {
    const documentPicker: picker.DocumentViewPicker = new picker.DocumentViewPicker(context);
    const files: string[] = await documentPicker.select({ maxSelectNumber: 1, fileSuffixFilters: ['.json'] });
    if (files.length === 0) { return false; }
    const file: fileIo.File = fileIo.openSync(files[0], fileIo.OpenMode.READ_ONLY);
    let text: string = '';
    try {
      const size: number = fileIo.statSync(file.fd).size;
      if (size <= 0 || size > 10 * 1024 * 1024) { throw new Error('备份文件为空或超过 10 MB。'); }
      const buffer: ArrayBuffer = new ArrayBuffer(size);
      const count: number = fileIo.readSync(file.fd, buffer);
      if (count !== size) { throw new Error('备份文件未能完整读取，请重试。'); }
      text = util.TextDecoder.create('utf-8', { fatal: true }).decodeToString(new Uint8Array(buffer));
    } finally { fileIo.closeSync(file); }
    const backup: LearningState = JSON.parse(text) as LearningState;
    this.commit(mergeBackup(this.snapshot(), backup, dateKey(new Date())));
    return true;
  }
}

export const learningStore: LearningStore = new LearningStore();
