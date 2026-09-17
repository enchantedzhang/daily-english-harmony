import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import ts from 'typescript';
import os from 'node:os';
import path from 'node:path';
import * as core from '../entry/src/main/ets/model/LearningCore.ts';

const bank = JSON.parse(fs.readFileSync(new URL('../entry/src/main/resources/rawfile/words.json', import.meta.url)));
function service(name, mocks) {
  const source = fs.readFileSync(new URL(`../entry/src/main/ets/service/${name}.ets`, import.meta.url), 'utf8');
  const compiled = ts.transpileModule(source, { compilerOptions: { target: ts.ScriptTarget.ES2022, module: ts.ModuleKind.CommonJS }, reportDiagnostics: true });
  assert.equal(compiled.diagnostics?.length, 0);
  const exports = {};
  vm.runInNewContext(compiled.outputText, {
    exports, require: id => {
      if (id === '../model/LearningCore') return core;
      if (!(id in mocks)) throw new Error(`Missing mock: ${id}`);
      return mocks[id];
    }, console, setTimeout, clearTimeout, Date, ArrayBuffer, Uint8Array
  });
  return exports;
}

function calendarHarness() {
  let state = { version: 1, reminderSyncPending: true, settings: { ...core.defaultSettings(), reminder: true }, lessons: [], customWords: [] };
  const tomorrow = core.addDays(core.dateKey(new Date()), 1);
  state.lessons = core.planLessons(state, bank, tomorrow, false).filter(l => core.isWorkday(l.date) === true).slice(0, 3);
  let serial = 100;
  const events = [];
  const calls = { add: 0, update: 0, delete: [], prompts: 0, granted: true, failAddAt: -1, failCommit: false, failDelete: false };
  const calendar = {
    getAccount: () => ({ name: 'dailyenglish.personal.v1' }),
    setConfig: async () => {},
    getEvents: async () => structuredClone(events),
    addEvent: async event => {
      calls.add++;
      if (calls.add === calls.failAddAt) throw new Error('calendar unavailable');
      const id = serial++;
      events.push({ ...structuredClone(event), id }); return id;
    },
    updateEvent: async event => { calls.update++; events[events.findIndex(e => e.id === event.id)] = structuredClone(event); },
    deleteEvent: async id => {
      if (calls.failDelete) throw new Error('calendar deletion unavailable');
      calls.delete.push(id); events.splice(events.findIndex(e => e.id === id), 1);
    }
  };
  const store = {
    snapshot: () => structuredClone(state),
    commit: value => {
      if (calls.failCommit) throw new Error('disk full');
      core.validateState(value); state = structuredClone(value);
    }
  };
  const mocks = {
    '@kit.AbilityKit': { abilityAccessCtrl: {
      GrantStatus: { PERMISSION_GRANTED: 0 },
      createAtManager: () => ({
        checkAccessTokenSync: () => calls.granted ? 0 : -1,
        requestPermissionsFromUser: async () => { calls.prompts++; return { authResults: calls.granted ? [0, 0] : [-1, -1] }; }
      })
    } },
    '@kit.CalendarKit': { calendarManager: {
      CalendarType: { LOCAL: 'local' }, EventType: { NORMAL: 0 },
      getCalendarManager: () => ({ getAllCalendars: async () => [calendar], createCalendar: async () => calendar })
    } }
  };
  const { CalendarReminderService } = service('CalendarReminderService', mocks);
  const subject = new CalendarReminderService();
  const context = { applicationInfo: { accessTokenId: 1 } };
  return { subject, store, calls, events, context, getState: () => state, setState: s => { state = s; } };
}

test('calendar synchronization is idempotent and records only accepted events', async () => {
  const h = calendarHarness();
  assert.equal(await h.subject.synchronize(h.context, h.store, false), 3);
  assert.equal(h.events.length, 3);
  assert.equal(h.getState().reminderSyncPending, false);
  assert.ok(h.getState().lessons.every(l => l.eventId >= 100 && l.scheduledAt > Date.now()));
  await h.subject.synchronize(h.context, h.store, false);
  assert.equal(h.events.length, 3);
  assert.equal(h.calls.add, 3);
  assert.equal(h.calls.update, 0);
});

test('multi-domain reminders contain every selected domain and show the total word count', async () => {
  const h = calendarHarness();
  const state = h.getState();
  state.settings.topics = ['职场沟通', '计算机技术'];
  state.lessons = core.planLessons({ ...state, lessons: [] }, bank, core.addDays(core.dateKey(new Date()), 1), false).filter(l => core.isWorkday(l.date) === true).slice(0, 1);
  await h.subject.synchronize(h.context, h.store, false);
  assert.match(h.events[0].title, /10 词/);
  for (const word of state.lessons[0].words) assert.ok(h.events[0].description.includes(`【${word.topic}】${word.word}：`));
});

test('sync removes owned rest-day reminders, registers makeup workdays and strips excluded words', async () => {
  const h = calendarHarness();
  const state = h.getState();
  state.lessons = core.planLessons({ ...state, lessons: [] }, bank, core.addDays(core.dateKey(new Date()), 1), false);
  const rest = state.lessons.find(l => core.isWorkday(l.date) === false);
  assert.ok(rest);
  rest.eventId = 900; rest.scheduledAt = core.reminderTime(rest.date, state.settings);
  h.events.push({ id: 900, startTime: rest.scheduledAt, description: `[daily-english:${rest.date}]\nold reminder` });
  const work = state.lessons.find(l => core.isWorkday(l.date) === true);
  work.words.push({ ...bank[0], word: 'ABANDON' });
  await h.subject.synchronize(h.context, h.store, false);
  assert.ok(h.calls.delete.includes(900));
  assert.ok(h.events.every(e => core.isWorkday(core.dateKey(new Date(e.startTime))) === true));
  assert.ok(!/\babandon\b/i.test(JSON.stringify(h.events)));
  const makeup = state.lessons.filter(l => !core.isWeekday(l.date) && core.isWorkday(l.date));
  for (const lesson of makeup) assert.ok(h.events.some(e => e.description.startsWith(`[daily-english:${lesson.date}]`)));
  assert.equal(h.getState().lessons.find(l => l.date === rest.date).scheduledAt, 0);
});

test('holiday updater saves complete years, preserves concurrent edits, and retains offline data on failure', async () => {
  let state = { version: 1, reminderSyncPending: false, settings: { ...core.defaultSettings(), reminder: true }, lessons: [], customWords: [] };
  let payload = structuredClone(core.BUILTIN_WORK_CALENDAR), destroyed = 0, fail = false;
  const http = { RequestMethod: { GET: 0 }, HttpDataType: { STRING: 0 }, createHttp: () => ({
    request: async () => { state.settings.count = 7; if (fail) throw new Error('offline'); return { responseCode: 200, result: JSON.stringify(payload) }; },
    destroy: () => { destroyed++; }
  }) };
  const { WorkCalendarService } = service('WorkCalendarService', { '@kit.NetworkKit': { http } });
  const subject = new WorkCalendarService();
  const store = { snapshot: () => structuredClone(state), commit: next => { core.validateState(next); state = structuredClone(next); } };
  await subject.update(store, true);
  assert.equal(state.settings.count, 7);
  assert.equal(state.workCalendars.length, 1);
  assert.equal(state.reminderSyncPending, true);
  const saved = JSON.stringify(state.workCalendars);
  payload.papers = []; await subject.update(store, true);
  assert.equal(JSON.stringify(state.workCalendars), saved);
  fail = true; await subject.update(store, true);
  assert.equal(JSON.stringify(state.workCalendars), saved);
  assert.match(subject.status, /继续使用/);
  assert.equal(destroyed, 3);
});

test('dictionary exclusion blocks requests and removes excluded online candidates', async () => {
  let calls = 0;
  const http = { RequestMethod: { GET: 0 }, HttpDataType: { STRING: 0 }, createHttp: () => ({
    request: async () => { calls++; return { responseCode: 200, result: JSON.stringify([
      { word: 'abandon', defs: ['v\tleave'] }, { word: 'cache', defs: ['n\ta temporary store'] }
    ]) }; }, destroy: () => {}
  }) };
  const dictionary = service('DictionaryService', { '@kit.NetworkKit': { http } });
  await assert.rejects(dictionary.lookupWord('ABANDON'), /已排除/);
  assert.equal(calls, 0);
  const words = await dictionary.discoverWords('software');
  assert.deepEqual(Array.from(words, w => w.word), ['cache']);
});

test('changing reminder time updates existing events instead of duplicating them', async () => {
  const h = calendarHarness();
  await h.subject.synchronize(h.context, h.store, false);
  h.getState().settings.hour = 17;
  await h.subject.synchronize(h.context, h.store, false);
  assert.equal(h.calls.update, 3);
  assert.equal(h.calls.add, 3);
  assert.ok(h.events.every(e => new Date(e.startTime).getHours() === 17));
});

test('interrupted calendar registration retries without duplicate dates', async () => {
  const h = calendarHarness();
  h.calls.failAddAt = 2;
  await assert.rejects(h.subject.synchronize(h.context, h.store, false), /unavailable/);
  assert.equal(h.events.length, 1);
  assert.equal(h.getState().lessons.filter(l => l.scheduledAt > 0).length, 1);
  assert.equal(h.getState().reminderSyncPending, true);
  h.calls.failAddAt = -1;
  await h.subject.synchronize(h.context, h.store, false);
  assert.equal(h.events.length, 3);
  assert.equal(new Set(h.events.map(e => e.description.split('\n')[0])).size, 3);
});

test('crash between calendar insertion and local commit is recovered by event marker', async () => {
  const h = calendarHarness();
  h.calls.failCommit = true;
  await assert.rejects(h.subject.synchronize(h.context, h.store, false), /disk full/);
  assert.equal(h.events.length, 1);
  h.calls.failCommit = false;
  await h.subject.synchronize(h.context, h.store, false);
  assert.equal(h.events.length, 3);
  assert.equal(h.calls.add, 3);
});

test('disabling reminders removes only owned future events, retaining past and unrelated data', async () => {
  const h = calendarHarness();
  await h.subject.synchronize(h.context, h.store, false);
  h.events.push({ id: 500, startTime: Date.now() + 100000, description: 'personal event' });
  h.events.push({ id: 501, startTime: Date.now() - 100000, description: '[daily-english:2020-01-01]\npast' });
  h.getState().settings.reminder = false;
  h.getState().reminderSyncPending = true;
  await h.subject.synchronize(h.context, h.store, false);
  assert.deepEqual(h.events.map(e => e.id), [500, 501]);
  assert.ok(h.getState().lessons.every(l => l.scheduledAt === 0 && l.eventId === -1));
  assert.equal(h.getState().reminderSyncPending, false);
});

test('failed disable stays pending across restart rather than reporting success', async () => {
  const h = calendarHarness();
  await h.subject.synchronize(h.context, h.store, false);
  h.getState().settings.reminder = false;
  h.getState().reminderSyncPending = true;
  h.calls.failDelete = true;
  await assert.rejects(h.subject.synchronize(h.context, h.store, false), /unavailable/);
  assert.equal(h.getState().reminderSyncPending, true);
  assert.equal(h.events.length, 3);
  h.calls.failDelete = false;
  await h.subject.synchronize(h.context, h.store, false);
  assert.equal(h.getState().reminderSyncPending, false);
  assert.equal(h.events.length, 0);
});

test('permission denial has no calendar effects and automatic refresh never prompts', async () => {
  const h = calendarHarness(); h.calls.granted = false;
  await assert.rejects(h.subject.synchronize(h.context, h.store, false), /日历权限/);
  assert.equal(h.calls.prompts, 0);
  await assert.rejects(h.subject.synchronize(h.context, h.store, true), /日历权限/);
  assert.equal(h.calls.prompts, 1);
  assert.equal(h.events.length, 0);
  assert.equal(h.getState().reminderSyncPending, true);
});

test('concurrent synchronization is rejected to avoid duplicate reminders', async () => {
  const h = calendarHarness();
  const first = h.subject.synchronize(h.context, h.store, false);
  await assert.rejects(h.subject.synchronize(h.context, h.store, false), /正在同步/);
  await first;
  assert.equal(h.events.length, 3);
});

test('speech does not shut down on synthesis completion, only resolves on playback completion', async () => {
  let listener, requestId, shutdowns = 0;
  const engine = { setListener: l => { listener = l; }, speak: (_text, params) => { requestId = params.requestId; }, stop: () => {}, shutdown: () => { shutdowns++; } };
  const { SpeechService } = service('SpeechService', { '@kit.CoreSpeechKit': { textToSpeech: { createEngine: async () => engine } } });
  const subject = new SpeechService();
  let complete = false;
  const speaking = subject.speak('reliable', 1).then(() => { complete = true; });
  await new Promise(resolve => setImmediate(resolve));
  listener.onComplete(requestId, { type: 0 });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(complete, false); assert.equal(shutdowns, 0);
  listener.onComplete(requestId, { type: 1 });
  await speaking;
  assert.equal(complete, true); assert.equal(shutdowns, 0);
  subject.release(); assert.equal(shutdowns, 1);
});

test('speech initialization after leaving page closes its newly created engine', async () => {
  let resolveEngine, shutdowns = 0;
  const engine = { stop: () => {}, shutdown: () => { shutdowns++; } };
  const { SpeechService } = service('SpeechService', { '@kit.CoreSpeechKit': { textToSpeech: { createEngine: () => new Promise(resolve => { resolveEngine = resolve; }) } } });
  const subject = new SpeechService();
  const speaking = subject.speak('hello', 1);
  subject.release(); resolveEngine(engine);
  await speaking; assert.equal(shutdowns, 1);
});

test('network parsing preserves original definitions, encodes query, and always releases requests', async () => {
  let response = { responseCode: 200, result: JSON.stringify([{ word: 'cache', defs: ['n\tA temporary store.'] }]) };
  let destroyed = 0, requested = '';
  const http = { RequestMethod: { GET: 'GET' }, HttpDataType: { STRING: 0 }, createHttp: () => ({
    request: async url => { requested = url; return response; }, destroy: () => { destroyed++; }
  }) };
  const dictionary = service('DictionaryService', { '@kit.NetworkKit': { http } });
  const words = await dictionary.discoverWords('software engineering');
  assert.ok(requested.includes('software%20engineering'));
  assert.equal(words[0].meaning, 'A temporary store.');
  assert.equal(words[0].topic, 'software engineering');
  response = { responseCode: 404, result: '{}' };
  await assert.rejects(dictionary.lookupWord('unknownword'), /未查到/);
  assert.equal(destroyed, 2);
  response = { responseCode: 200, result: '<html>invalid</html>' };
  await assert.rejects(dictionary.discoverWords('finance'));
  assert.equal(destroyed, 3);
});

function storeHarness(t) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'daily-english-test-'));
  t.after(() => {
    const relative = path.relative(os.tmpdir(), directory);
    assert.ok(relative.startsWith('daily-english-test-') && !relative.includes(path.sep));
    fs.rmSync(directory, { recursive: true, force: true });
  });
  const controls = { failWrites: false, selectedFile: '', exportFile: path.join(directory, 'export.json') };
  const fileIo = {
    OpenMode: { CREATE: fs.constants.O_CREAT, WRITE_ONLY: fs.constants.O_WRONLY, TRUNC: fs.constants.O_TRUNC, READ_ONLY: fs.constants.O_RDONLY },
    accessSync: file => fs.existsSync(file), readTextSync: file => fs.readFileSync(file, 'utf8'),
    copyFileSync: fs.copyFileSync, renameSync: fs.renameSync,
    openSync: (file, flags) => ({ fd: fs.openSync(file, flags) }), closeSync: file => fs.closeSync(file.fd),
    fsyncSync: fs.fsyncSync,
    writeSync: (fd, buffer) => { if (controls.failWrites) throw new Error('disk full'); return fs.writeSync(fd, Buffer.from(buffer)); },
    readSync: (fd, buffer) => fs.readSync(fd, Buffer.from(buffer)), statSync: fd => fs.fstatSync(fd)
  };
  const picker = { DocumentViewPicker: class {
    async save() { return controls.exportFile ? [controls.exportFile] : []; }
    async select() { return controls.selectedFile ? [controls.selectedFile] : []; }
  } };
  const util = { TextDecoder: { create: () => ({ decodeToString: data => new TextDecoder('utf-8', { fatal: true }).decode(data) }) },
    TextEncoder: class { encodeInto(text) { return new TextEncoder().encode(text); } } };
  const { LearningStore } = service('LearningStore', { '@kit.CoreFileKit': { fileIo, picker }, '@kit.ArkTS': { util } });
  const context = { filesDir: directory, resourceManager: { getRawFileContent: async name =>
    new Uint8Array(fs.readFileSync(new URL(`../entry/src/main/resources/rawfile/${name}`, import.meta.url))) } };
  return { directory, controls, context, LearningStore, subject: new LearningStore() };
}

test('file storage survives restart and creates a last-known-good backup', async t => {
  const h = storeHarness(t);
  await h.subject.initialize(h.context);
  h.subject.saveSettings({ ...core.defaultSettings(), count: 10 });
  const restarted = new h.LearningStore();
  await restarted.initialize(h.context);
  assert.equal(restarted.snapshot().settings.count, 10);
  assert.ok(fs.existsSync(path.join(h.directory, 'learning-v1.json.bak')));
  assert.deepEqual(restarted.snapshot().lessons, h.subject.snapshot().lessons);
});

test('storage write failure leaves memory and committed history unchanged', async t => {
  const h = storeHarness(t);
  await h.subject.initialize(h.context);
  const before = JSON.stringify(h.subject.snapshot());
  const diskBefore = fs.readFileSync(path.join(h.directory, 'learning-v1.json'), 'utf8');
  h.controls.failWrites = true;
  assert.throws(() => h.subject.saveSettings({ ...core.defaultSettings(), count: 10 }), /disk full/);
  assert.equal(JSON.stringify(h.subject.snapshot()), before);
  assert.equal(fs.readFileSync(path.join(h.directory, 'learning-v1.json'), 'utf8'), diskBefore);
});

test('corrupted main file recovers backup and retains the damaged original', async t => {
  const h = storeHarness(t);
  await h.subject.initialize(h.context);
  h.subject.saveSettings({ ...core.defaultSettings(), count: 10 });
  fs.writeFileSync(path.join(h.directory, 'learning-v1.json'), 'damaged content');
  const restarted = new h.LearningStore();
  await restarted.initialize(h.context);
  assert.equal(restarted.snapshot().settings.count, 5);
  assert.match(restarted.recoveryMessage, /恢复/);
  assert.ok(fs.readdirSync(h.directory).some(name => name.includes('.damaged-')));
});

test('unrecoverable storage refuses to replace user history with defaults', async t => {
  const h = storeHarness(t);
  fs.writeFileSync(path.join(h.directory, 'learning-v1.json'), 'damaged content');
  await assert.rejects(h.subject.initialize(h.context), /未覆盖/);
  assert.equal(fs.readFileSync(path.join(h.directory, 'learning-v1.json'), 'utf8'), 'damaged content');
});

test('export and merge-import use chosen files without replacing current settings', async t => {
  const h = storeHarness(t);
  await h.subject.initialize(h.context);
  assert.equal(await h.subject.exportBackup(h.context), true);
  const backup = JSON.parse(fs.readFileSync(h.controls.exportFile, 'utf8'));
  core.validateState(backup);
  h.subject.saveSettings({ ...core.defaultSettings(), count: 10 });
  h.controls.selectedFile = h.controls.exportFile;
  assert.equal(await h.subject.importBackup(h.context), true);
  assert.equal(h.subject.snapshot().settings.count, 10);
  h.controls.selectedFile = '';
  assert.equal(await h.subject.importBackup(h.context), false);
});

test('recorded audio releases the player and cached file after playback', async () => {
  const callbacks = {};
  let released = 0, closed = 0;
  const player = {
    on: (name, callback) => { callbacks[name] = callback; }, off: () => {},
    set fdSrc(_) { queueMicrotask(() => callbacks.stateChange('initialized')); },
    prepare: async () => { queueMicrotask(() => callbacks.stateChange('prepared')); },
    setSpeed: () => {}, play: async () => { queueMicrotask(() => callbacks.stateChange('completed')); },
    release: async () => { released++; }
  };
  const context = {};
  const mocks = {
    './AudioCacheService': { audioCache: { open: (_, text) => text === 'hello' ? { fd: 5 } : undefined } },
    '@kit.CoreFileKit': { fileIo: { closeSync: () => { closed++; }, statSync: () => ({ size: 100 }) } },
    '@kit.MediaKit': { media: { createAVPlayer: async () => player, PlaybackSpeed: { SPEED_FORWARD_1_00_X: 1 } } } };
  const { RecordedAudioService } = service('RecordedAudioService', mocks);
  const subject = new RecordedAudioService();
  assert.equal(await subject.play('unknown', 1, context), false);
  assert.equal(released, 0);
  assert.equal(await subject.play('hello', 1, context), true);
  assert.equal(released, 1); assert.equal(closed, 1);
});
