import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import vm from 'node:vm';
import ts from 'typescript';
import * as core from '../entry/src/main/ets/model/LearningCore.ts';

function load(relative, mocks) {
  const text = fs.readFileSync(new URL('../entry/src/main/ets/' + relative, import.meta.url), 'utf8');
  const result = ts.transpileModule(text, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS } });
  const exports = {};
  vm.runInNewContext(result.outputText, { exports, require: name => {
    if (name.endsWith('/LearningCore')) return core;
    if (!(name in mocks)) throw new Error('Missing mock ' + name);
    return mocks[name];
  }, ArrayBuffer, Uint8Array, Date, console, setTimeout, clearTimeout, setInterval, clearInterval });
  return exports;
}
const policy = load('model/AudioCacheCore.ts', {});
const bank = JSON.parse(fs.readFileSync(new URL('../entry/src/main/resources/rawfile/words.json', import.meta.url)));
const today = core.dateKey(new Date());
const wordLesson = (date, word) => ({ ...core.emptyLesson(date), words: [{ ...bank[0], word }] });

test('audio window includes exactly past seven, today and next twenty dates; prioritizes today and deduplicates', () => {
  const lessons = Array.from({ length: 32 }, (_, i) => wordLesson(core.addDays(today, i - 9), 'word' + (i - 9)));
  const words = policy.audioWords(lessons, today);
  assert.equal(words.length, 28);
  assert.equal(words[0], 'word0');
  assert.ok(words.includes('word-7') && words.includes('word20'));
  assert.ok(!words.includes('word-8') && !words.includes('word21'));
  assert.deepEqual(Array.from(policy.audioWords([wordLesson(today, 'HELLO'), wordLesson(core.addDays(today, 1), 'hello')], today)), ['hello']);
  assert.equal(policy.audioWords([wordLesson(today, 'ABANDON')], today).length, 0);
});

test('audio filenames remain bounded and safe; only allowed HTTPS MP3 hosts pass; HTML never passes as audio', () => {
  for (const word of ['../../password', 'a'.repeat(80), '中文测试', 'hello']) assert.match(policy.audioFile(word), /^[a-f0-9]{1,176}\.mp3$/);
  assert.notEqual(policy.audioFile('a'.repeat(79) + 'b'), policy.audioFile('a'.repeat(79) + 'c'));
  assert.ok(policy.allowedAudioURL('https://api.dictionaryapi.dev/media/pronunciations/en/hello-us.mp3'));
  for (const url of ['http://api.dictionaryapi.dev/media/pronunciations/en/a.mp3', 'https://localhost/a.mp3', 'https://api.dictionaryapi.dev.evil.test/a.mp3']) assert.equal(policy.allowedAudioURL(url), false);
  assert.equal(policy.validMP3(new TextEncoder().encode('<html>'.repeat(30)).buffer), false);
  assert.equal(policy.validMP3(new ArrayBuffer(policy.AUDIO_FILE_LIMIT + 1)), false);
});

test('multiple selected domains each receive the configured quota and existing dates remain unchanged', () => {
  const settings = { ...core.defaultSettings(), topics: ['职场沟通', '计算机技术'], count: 5 };
  const lesson = core.createLesson(today, settings, bank, []);
  assert.equal(lesson.words.length, 10);
  for (const topic of settings.topics) assert.equal(lesson.words.filter(w => w.topic === topic).length, 5);
  assert.equal(new Set(lesson.words.map(w => w.word)).size, 10);
  const state = { version: 1, reminderSyncPending: false, settings, lessons: [lesson], customWords: [] };
  state.settings = { ...settings, topics: ['旅行生活', '阅读表达', '日常交流'], count: 2 };
  const changed = core.planLessons(state, bank, today, true);
  assert.deepEqual(changed[0], lesson);
  assert.equal(changed[1].words.length, 6);
  for (const topic of state.settings.topics) assert.equal(changed[1].words.filter(w => w.topic === topic).length, 2);
  assert.throws(() => core.validateSettings({ ...settings, topics: [] }));
  assert.throws(() => core.validateSettings({ ...settings, topics: ['职场沟通', '职场沟通'] }));
  assert.deepEqual(core.selectedTopics({ ...core.defaultSettings(), topics: undefined, topic: '旅行生活' }), ['旅行生活']);
});

function harness(t) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'english-cache-test-'));
  t.after(() => {
    const relative = path.relative(os.tmpdir(), directory);
    assert.ok(relative.startsWith('english-cache-test-') && !relative.includes(path.sep));
    fs.rmSync(directory, { recursive: true, force: true });
  });
  const controls = { online: true, requests: [], fail: false, html: false, writeFail: false, beforeResponse: undefined };
  const locks = new Set();
  const fileIo = {
    OpenMode: { CREATE: fs.constants.O_CREAT, WRITE_ONLY: fs.constants.O_WRONLY, READ_WRITE: fs.constants.O_RDWR,
      TRUNC: fs.constants.O_TRUNC, READ_ONLY: fs.constants.O_RDONLY },
    accessSync: fs.existsSync, mkdirSync: fs.mkdirSync, listFileSync: fs.readdirSync, unlinkSync: fs.unlinkSync,
    readTextSync: file => fs.readFileSync(file, 'utf8'), renameSync: fs.renameSync, fsyncSync: fs.fsyncSync,
    statSync: file => typeof file === 'number' ? fs.fstatSync(file) : fs.statSync(file),
    openSync: (file, flags) => ({ fd: fs.openSync(file, flags), held: false, tryLock() {
      if (locks.has(file)) throw new Error('locked'); locks.add(file); this.held = true;
    }, unlock() { if (this.held) { locks.delete(file); this.held = false; } } }),
    closeSync: file => { file.unlock(); fs.closeSync(file.fd); },
    writeSync: (fd, bytes) => { if (controls.writeFail) throw new Error('disk full'); return fs.writeSync(fd, Buffer.from(bytes)); }
  };
  const http = { RequestMethod: { GET: 0 }, HttpDataType: { STRING: 0, ARRAY_BUFFER: 1 }, createHttp: () => ({
    destroyed: false, destroy() { this.destroyed = true; }, async request(url) {
      controls.requests.push(url);
      if (controls.beforeResponse) await controls.beforeResponse(url);
      if (this.destroyed || controls.fail) throw new Error('offline');
      if (url.includes('/api/v2/')) return { responseCode: 200, result: JSON.stringify([{ phonetics: [{
        audio: 'https://api.dictionaryapi.dev/media/pronunciations/en/hello-us.mp3', sourceUrl: 'https://commons.wikimedia.org/wiki/File:hello.ogg',
        license: { name: 'CC BY-SA 4.0', url: 'https://creativecommons.org/licenses/by-sa/4.0/' }
      }] }]) };
      const bytes = new Uint8Array(256); bytes.set(controls.html ? [60, 104, 116] : [73, 68, 51]);
      return { responseCode: 200, result: bytes.buffer };
    }
  }) };
  const util = { TextDecoder: { create: () => ({ decodeToString: bytes => new TextDecoder().decode(bytes) }) },
    TextEncoder: class { encodeInto(text) { return new TextEncoder().encode(text); } } };
  const { AudioCacheService } = load('service/AudioCacheService.ets', { '../model/AudioCacheCore': policy,
    '@kit.ArkTS': { util }, '@kit.CoreFileKit': { fileIo }, '@kit.NetworkKit': { http, connection: { hasDefaultNet: async () => controls.online } } });
  const context = { filesDir: directory, cacheDir: directory, resourceManager: {
    getRawFileContent: async () => new TextEncoder().encode(JSON.stringify(bank))
  } };
  const state = { version: 1, reminderSyncPending: false, settings: { ...core.defaultSettings(), count: 1 }, lessons: [], customWords: [] };
  state.lessons = core.planLessons(state, bank, today, false);
  const stateFile = path.join(directory, 'learning-v1.json');
  const saveState = () => fs.writeFileSync(stateFile, JSON.stringify(state));
  saveState();
  const cacheDir = path.join(directory, 'word-audio-v2');
  return { directory, cacheDir, stateFile, state, saveState, controls, context, fileIo, AudioCacheService,
    subject: new AudioCacheService(), index: () => JSON.parse(fs.readFileSync(path.join(cacheDir, 'index.json'))) };
}

test('downloads only planned words, persists attribution, reuses files, and does not alter learning history', async t => {
  const h = harness(t), before = fs.readFileSync(h.stateFile, 'utf8');
  await h.subject.synchronize(h.context);
  assert.equal(h.index().entries.length, 21);
  assert.equal(h.controls.requests.length, 42);
  assert.match(h.index().entries[0].license, /CC BY-SA/);
  await h.subject.synchronize(h.context);
  assert.equal(h.controls.requests.length, 42);
  assert.equal(fs.readFileSync(h.stateFile, 'utf8'), before);
  const opened = h.subject.open(h.context, h.state.lessons[0].words[0].word);
  assert.ok(opened); h.fileIo.closeSync(opened);
  assert.equal(h.subject.open(h.context, 'out-of-window'), undefined);
});

test('expired and orphaned files are removed offline while historical text remains; future +21 audio is excluded', async t => {
  const h = harness(t);
  await h.subject.synchronize(h.context);
  const stale = h.index().entries[0];
  h.state.lessons = [wordLesson(core.addDays(today, -8), stale.word), ...core.planLessons({ ...h.state, lessons: [], settings: { ...h.state.settings, topics: ['旅行生活'] } }, bank, today, false)];
  h.saveState(); h.controls.online = false;
  fs.writeFileSync(path.join(h.cacheDir, 'aabb.mp3.part'), 'partial');
  await h.subject.synchronize(h.context);
  assert.equal(fs.existsSync(path.join(h.cacheDir, stale.file)), false);
  assert.equal(fs.existsSync(path.join(h.cacheDir, 'aabb.mp3.part')), false);
  assert.ok(JSON.parse(fs.readFileSync(h.stateFile)).lessons.some(l => l.date === core.addDays(today, -8)));
  assert.equal(h.subject.open(h.context, stale.word), undefined);
  assert.match(h.subject.status, /等待联网/);
});

test('failed downloads back off, manual retry resumes, and HTML never enters the cache', async t => {
  const h = harness(t); h.controls.html = true;
  await h.subject.synchronize(h.context);
  assert.equal(h.index().entries.length, 0);
  assert.equal(h.index().failures.length, 21);
  const requests = h.controls.requests.length;
  await h.subject.synchronize(h.context);
  assert.equal(h.controls.requests.length, requests);
  h.controls.html = false;
  await h.subject.synchronize(h.context, 70000, true);
  assert.equal(h.index().entries.length, 21);
});

test('canceling an in-flight request writes no audio and releases the cross-process update lock', async t => {
  const h = harness(t);
  h.controls.beforeResponse = async () => h.subject.cancel();
  await h.subject.synchronize(h.context);
  assert.equal(h.index().entries.length, 0);
  h.controls.beforeResponse = undefined;
  const next = new h.AudioCacheService();
  await next.synchronize(h.context);
  assert.equal(h.index().entries.length, 21);
});

test('a changed selection during downloading cannot commit stale audio', async t => {
  const h = harness(t);
  h.controls.beforeResponse = async url => {
    if (url.endsWith('.mp3')) {
      h.state.settings.topics = ['计算机技术'];
      h.state.lessons = core.planLessons({ ...h.state, lessons: [] }, bank, today, true);
      h.saveState();
    }
  };
  await h.subject.synchronize(h.context);
  assert.equal(h.index().entries.length, 0);
});

test('concurrent foreground and background synchronizers use a single writer', async t => {
  const h = harness(t), other = new h.AudioCacheService();
  h.controls.beforeResponse = async () => { await other.synchronize(h.context); };
  await h.subject.synchronize(h.context);
  assert.equal(h.index().entries.length, 21);
  assert.equal(h.controls.requests.length, 42);
  assert.match(other.status, /另一个/);
});

test('disabled automatic audio prevents requests; low disk failure leaves learning data and lock intact', async t => {
  const h = harness(t); h.state.settings.audioAutoUpdate = false; h.saveState();
  await h.subject.synchronize(h.context);
  assert.equal(h.controls.requests.length, 0);
  h.state.settings.audioAutoUpdate = true; h.saveState();
  const before = fs.readFileSync(h.stateFile, 'utf8');
  h.controls.writeFail = true;
  await h.subject.synchronize(h.context);
  assert.match(h.subject.status, /未完成/);
  assert.equal(fs.readFileSync(h.stateFile, 'utf8'), before);
  h.controls.writeFail = false;
  await h.subject.synchronize(h.context);
  assert.equal(h.index().entries.length, 21);
});

test('audio storage cap prevents further downloads from being committed', async t => {
  const h = harness(t);
  h.state.settings.topics = core.TOPICS.slice(1);
  h.state.settings.count = 20;
  h.state.lessons = core.planLessons({ ...h.state, lessons: [] }, bank, today, false);
  h.saveState();
  h.controls.online = false;
  await h.subject.synchronize(h.context);
  const words = policy.audioWords(h.state.lessons, today);
  const entries = words.slice(1, 129).map(word => {
    const file = policy.audioFile(word), target = path.join(h.cacheDir, file);
    const fd = fs.openSync(target, 'w'); fs.ftruncateSync(fd, policy.AUDIO_FILE_LIMIT); fs.closeSync(fd);
    return { word, file, bytes: policy.AUDIO_FILE_LIMIT, url: '', source: '', license: '' };
  });
  fs.writeFileSync(path.join(h.cacheDir, 'index.json'), JSON.stringify({ entries, failures: [] }));
  h.controls.online = true;
  await h.subject.synchronize(h.context);
  assert.equal(h.index().entries.length, 128);
  assert.equal(h.index().entries.reduce((sum, e) => sum + e.bytes, 0), policy.AUDIO_TOTAL_LIMIT);
  assert.match(h.subject.status, /64 MB 上限/);
});
