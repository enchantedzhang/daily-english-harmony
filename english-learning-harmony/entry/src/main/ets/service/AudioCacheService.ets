import { common } from '@kit.AbilityKit';
import { fileIo } from '@kit.CoreFileKit';
import { util } from '@kit.ArkTS';
import { http, connection } from '@kit.NetworkKit';
import { LearningState, WordItem, dateKey, planLessons, validateState } from '../model/LearningCore';
import { AudioEntry, Pronunciation, AUDIO_FILE_LIMIT, AUDIO_TOTAL_LIMIT, audioWords, audioFile, allowedAudioURL, validMP3 } from '../model/AudioCacheCore';

interface Failure { word: string; retryAt: number; }
interface CacheIndex { entries: AudioEntry[]; failures: Failure[]; }
interface License { name?: string; url?: string; }
interface Phonetic { audio?: string; sourceUrl?: string; license?: License; }
interface DictionaryEntry { phonetics?: Phonetic[]; }
interface CachePlan { words: string[]; enabled: boolean; key: string; }

export class AudioCacheService {
  status: string = '等待更新音频';
  private bank: WordItem[] = [];
  private generation: number = 0;
  private request: http.HttpRequest | undefined = undefined;
  private running: boolean = false;

  cancel(): void { this.generation++; this.request?.destroy(); this.request = undefined; }

  private directory(context: common.Context): string {
    const directory: string = `${context.cacheDir}/word-audio-v2`;
    if (!fileIo.accessSync(directory)) { fileIo.mkdirSync(directory); }
    return directory;
  }

  private plan(context: common.Context): CachePlan {
    const state: LearningState = JSON.parse(fileIo.readTextSync(`${context.filesDir}/learning-v1.json`)) as LearningState;
    validateState(state);
    const today: string = dateKey(new Date());
    const words: string[] = audioWords(planLessons(state, this.bank, today, false), today);
    const enabled: boolean = state.settings.audioAutoUpdate !== false;
    return { words: words, enabled: enabled, key: `${today}:${enabled}:${words.join('\n')}` };
  }

  private readIndex(directory: string): CacheIndex {
    try {
      const index: CacheIndex = JSON.parse(fileIo.readTextSync(`${directory}/index.json`)) as CacheIndex;
      if (!Array.isArray(index.entries) || !Array.isArray(index.failures)) { throw new Error('Invalid cache'); }
      index.entries = index.entries.filter((entry: AudioEntry) => typeof entry.word === 'string' && entry.word.length <= 80 &&
        entry.file === audioFile(entry.word) && entry.bytes > 0 && entry.bytes <= AUDIO_FILE_LIMIT &&
        fileIo.accessSync(`${directory}/${entry.file}`) && fileIo.statSync(`${directory}/${entry.file}`).size === entry.bytes);
      return index;
    } catch (_) { return { entries: [], failures: [] }; }
  }

  private write(path: string, data: ArrayBuffer): void {
    const file: fileIo.File = fileIo.openSync(path + '.part', fileIo.OpenMode.CREATE | fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    try {
      let offset: number = 0;
      while (offset < data.byteLength) {
        const written: number = fileIo.writeSync(file.fd, data.slice(offset));
        if (written <= 0) { throw new Error('音频缓存写入失败'); }
        offset += written;
      }
      fileIo.fsyncSync(file.fd);
    } finally { fileIo.closeSync(file); }
    fileIo.renameSync(path + '.part', path);
  }

  private save(directory: string, index: CacheIndex): void {
    const bytes: Uint8Array = new util.TextEncoder().encodeInto(JSON.stringify(index));
    this.write(`${directory}/index.json`, bytes.buffer as ArrayBuffer);
  }

  private prune(directory: string, index: CacheIndex, words: string[]): void {
    index.entries = index.entries.filter((entry: AudioEntry) => words.includes(entry.word));
    index.failures = index.failures.filter((failure: Failure) => words.includes(failure.word) && failure.retryAt > Date.now());
    const keep: string[] = index.entries.map((entry: AudioEntry) => entry.file);
    // Only generated cache files inside this dedicated directory are removed.
    fileIo.listFileSync(directory).forEach((name: string) => {
      if ((/^[0-9a-f]+\.mp3(?:\.part)?$/.test(name) && !keep.includes(name)) || name === 'index.json.part') {
        fileIo.unlinkSync(`${directory}/${name}`);
      }
    });
    this.save(directory, index);
  }

  private async get(url: string, binary: boolean): Promise<http.HttpResponse> {
    const request: http.HttpRequest = http.createHttp();
    this.request = request;
    try {
      const response: http.HttpResponse = await request.request(url, {
        method: http.RequestMethod.GET, expectDataType: binary ? http.HttpDataType.ARRAY_BUFFER : http.HttpDataType.STRING,
        connectTimeout: 8000, readTimeout: 8000, maxLimit: binary ? AUDIO_FILE_LIMIT : 1024 * 1024,
        usingCache: false
      });
      if (response.responseCode !== 200) { throw new Error(`词典响应 ${response.responseCode}`); }
      return response;
    } finally { request.destroy(); if (this.request === request) { this.request = undefined; } }
  }

  private async pronunciation(word: string): Promise<Pronunciation> {
    const response: http.HttpResponse = await this.get(`https://api.dictionaryapi.dev/api/v2/entries/en/${encodeURIComponent(word)}`, false);
    const entries: DictionaryEntry[] = JSON.parse(response.result as string) as DictionaryEntry[];
    if (!Array.isArray(entries)) { throw new Error('词典数据无效'); }
    for (const entry of entries) {
      if (!Array.isArray(entry.phonetics)) { continue; }
      for (const item of entry.phonetics) {
        if (item.audio && allowedAudioURL(item.audio)) {
          return { url: item.audio, source: item.sourceUrl || 'https://dictionaryapi.dev/',
            license: `${item.license?.name || '详见音频来源'} ${item.license?.url || ''}`.trim() };
        }
      }
    }
    throw new Error('该词暂无可下载发音');
  }

  async synchronize(context: common.Context, budget: number = 70000, force: boolean = false): Promise<void> {
    if (this.running) { return; }
    this.running = true;
    const generation: number = this.generation;
    let lock: fileIo.File | undefined = undefined;
    let locked: boolean = false;
    try {
      const directory: string = this.directory(context);
      lock = fileIo.openSync(`${directory}/update.lock`, fileIo.OpenMode.CREATE | fileIo.OpenMode.READ_WRITE);
      try { lock.tryLock(true); locked = true; }
      catch (_) { this.status = '另一个更新任务正在处理音频'; return; }
      if (this.bank.length === 0) {
        this.bank = JSON.parse(util.TextDecoder.create('utf-8').decodeToString(await context.resourceManager.getRawFileContent('words.json'))) as WordItem[];
      }
      const plan: CachePlan = this.plan(context);
      const index: CacheIndex = this.readIndex(directory);
      this.prune(directory, index, plan.words);
      if (force) { index.failures = []; }
      const online: boolean = plan.enabled && await connection.hasDefaultNet();
      const deadline: number = Date.now() + budget;
      let limited: boolean = false;
      for (const word of plan.words) {
        if (!online || generation !== this.generation || Date.now() >= deadline || this.plan(context).key !== plan.key) { break; }
        if (index.entries.some((entry: AudioEntry) => entry.word === word) || index.failures.some((failure: Failure) => failure.word === word)) { continue; }
        this.status = `正在更新音频 · ${index.entries.length}/${plan.words.length}`;
        try {
          const pronunciation: Pronunciation = await this.pronunciation(word);
          if (generation !== this.generation || Date.now() >= deadline || this.plan(context).key !== plan.key) { break; }
          const response: http.HttpResponse = await this.get(pronunciation.url, true);
          if (generation !== this.generation || this.plan(context).key !== plan.key) { break; }
          const data: ArrayBuffer = response.result as ArrayBuffer;
          if (!(data instanceof ArrayBuffer) || !validMP3(data)) { throw new Error('下载内容不是有效的 MP3 音频'); }
          if (index.entries.reduce((total: number, entry: AudioEntry) => total + entry.bytes, 0) + data.byteLength > AUDIO_TOTAL_LIMIT) {
            limited = true; break;
          }
          const filename: string = audioFile(word);
          this.write(`${directory}/${filename}`, data);
          index.entries.push({ word: word, file: filename, bytes: data.byteLength, url: pronunciation.url, source: pronunciation.source, license: pronunciation.license });
        } catch (_) {
          if (generation !== this.generation) { break; }
          index.failures.push({ word: word, retryAt: Date.now() + 60 * 60 * 1000 });
        }
        this.save(directory, index);
      }
      // Re-check after network waits: never retain audio for a superseded plan.
      this.prune(directory, index, this.plan(context).words);
      const megabytes: string = (index.entries.reduce((sum: number, entry: AudioEntry) => sum + entry.bytes, 0) / 1048576).toFixed(1);
      this.status = `音频 ${index.entries.length}/${this.plan(context).words.length} · ${megabytes} MB` +
        (!plan.enabled ? ' · 自动更新已关闭' : !online ? ' · 等待联网' : limited ? ' · 已达 64 MB 上限' : index.failures.length > 0 ? ' · 部分发音暂不可用，将自动重试' : '');
    } catch (_) { this.status = '音频更新未完成，稍后自动重试'; }
    finally {
      if (lock) { try { if (locked) { lock.unlock(); } } finally { fileIo.closeSync(lock); } }
      this.running = false;
    }
  }

  open(context: common.Context, text: string): fileIo.File | undefined {
    // Open before awaiting the player. An open descriptor remains valid if pruning unlinks its file.
    try {
      const directory: string = this.directory(context);
      const state: LearningState = JSON.parse(fileIo.readTextSync(`${context.filesDir}/learning-v1.json`)) as LearningState;
      const word: string = text.trim().toLowerCase();
      if (!audioWords(state.lessons, dateKey(new Date())).includes(word)) { return undefined; }
      const entry: AudioEntry | undefined = this.readIndex(directory).entries.find((item: AudioEntry) => item.word === word);
      return entry ? fileIo.openSync(`${directory}/${entry.file}`, fileIo.OpenMode.READ_ONLY) : undefined;
    } catch (_) { return undefined; }
  }
}

export const audioCache: AudioCacheService = new AudioCacheService();
