import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import * as core from '../entry/src/main/ets/model/LearningCore.ts';

const bank = JSON.parse(fs.readFileSync(new URL('../entry/src/main/resources/rawfile/words.json', import.meta.url)));
const legacy = JSON.parse(fs.readFileSync(new URL('../entry/src/main/resources/rawfile/history.json', import.meta.url)));
const state = (overrides = {}) => ({ version: 1, reminderSyncPending: false, settings: core.defaultSettings(), lessons: [], customWords: [], ...overrides });

test('1000 unique words cover five domains and preserve original bilingual examples and legacy words', () => {
  assert.equal(bank.length, 1000);
  assert.equal(new Set(bank.map(w => w.word)).size, bank.length);
  assert.ok(bank.every(core.allowedWord));
  assert.ok(bank.every(w => core.validWord(w) && w.meaning));
  assert.equal(bank.filter(w => w.example && w.translation).length, 150);
  assert.ok(legacy.entries.flatMap(e => e.words).every(w => bank.some(b => b.word === w)));
  for (const topic of core.TOPICS.slice(1)) assert.equal(bank.filter(w => w.topic === topic).length, 200);
});

test('calendar aligns Monday first, leap day and neighboring month cells', () => {
  const september = core.calendarCells(2026, 8);
  assert.equal(september.length, 42);
  assert.equal(september[0].date, '2026-08-31');
  assert.equal(september.filter(d => d.inMonth).length, 30);
  assert.equal(core.calendarCells(2028, 1).filter(d => d.inMonth).length, 29);
  assert.equal(core.calendarCells(2026, 1).filter(d => d.inMonth).length, 28);
  assert.equal(new Set(september.map(d => d.key)).size, 42);
});

test('local date arithmetic crosses year boundaries and rejects impossible dates', () => {
  assert.equal(core.addDays('2026-12-31', 1), '2027-01-01');
  assert.equal(core.addDays('2028-02-28', 1), '2028-02-29');
  assert.equal(core.validDate('2026-02-29'), false);
  assert.equal(core.validDate('2026-13-01'), false);
  assert.equal(core.validDate('2026-9-1'), false);
  assert.equal(core.validDate('2026-09-17'), true);
});

test('date arithmetic uses civil days across daylight saving changes', () => {
  const oldTZ = process.env.TZ;
  try {
    process.env.TZ = 'America/New_York';
    assert.equal(core.addDays('2026-03-07', 2), '2026-03-09');
    assert.equal(core.addDays('2026-10-31', 2), '2026-11-02');
    const date = new Date(core.reminderTime('2026-03-08', core.defaultSettings()));
    assert.equal(date.getHours(), 9);
  } finally { if (oldTZ) process.env.TZ = oldTZ; else delete process.env.TZ; }
});

test('legacy import preserves all dates, words and phrases without inventing notifications', () => {
  const imported = core.importLegacy(legacy, bank);
  assert.equal(imported.length, 9);
  imported.forEach((lesson, i) => {
    assert.deepEqual(lesson.words.map(w => w.word), legacy.entries[i].words);
    assert.equal(lesson.phrase, legacy.entries[i].phrase);
    assert.equal(lesson.scheduledAt, 0);
    assert.equal(lesson.eventId, -1);
    assert.equal(lesson.origin, '原历史记录');
  });
  assert.ok(!imported.some(e => e.date === '2026-09-14'));
});

test('new lessons exclude historical words when fresh words remain', () => {
  const history = core.importLegacy(legacy, bank);
  const seen = new Set(history.flatMap(l => l.words.map(w => w.word)));
  const lesson = core.createLesson('2026-09-17', core.defaultSettings(), bank, history);
  assert.equal(lesson.words.length, 5);
  assert.ok(lesson.words.every(w => !seen.has(w.word)));
  assert.deepEqual(lesson.reviewWords, []);
});

test('topic selection and all 1–20 word counts are honored', () => {
  for (const topic of core.TOPICS) for (let count = 1; count <= 20; count++) {
    const lesson = core.createLesson('2026-09-17', { ...core.defaultSettings(), topics: undefined, topic, count }, bank, []);
    const total = count * (topic === '全部领域' ? 5 : 1);
    assert.equal(lesson.words.length, total);
    assert.equal(new Set(lesson.words.map(w => w.word)).size, total);
    if (topic !== '全部领域') assert.ok(lesson.words.every(w => w.topic === topic));
  }
});

test('exhausted topic reviews least recent words and labels every repeat', () => {
  const s = state({ settings: { ...core.defaultSettings(), topics: ['计算机技术'], reminder: true } });
  const lessons = core.planLessons(s, bank.filter(w => w.topic === '计算机技术').slice(0, 30), '2026-09-17', false);
  assert.equal(lessons.length, 30);
  assert.equal(new Set(lessons.slice(0, 6).flatMap(l => l.words.map(w => w.word))).size, 30);
  assert.deepEqual(lessons[6].words.map(w => w.word), lessons[0].words.map(w => w.word));
  assert.equal(lessons[6].reviewWords.length, 5);
  assert.ok(lessons.every(l => l.scheduledAt === 0));
});

test('settings change preserves today and past records exactly and replaces only future', () => {
  const s = state({ settings: { ...core.defaultSettings(), reminder: true }, lessons: core.importLegacy(legacy, bank) });
  s.lessons = core.planLessons(s, bank, '2026-09-17', false);
  s.lessons.find(l => l.date === '2026-09-17').reviewed = [s.lessons.find(l => l.date === '2026-09-17').words[0].word];
  const immutable = structuredClone(s.lessons.filter(l => l.date <= '2026-09-17'));
  s.settings = { ...s.settings, count: 10, topics: ['旅行生活'] };
  const changed = core.planLessons(s, bank, '2026-09-17', true);
  assert.deepEqual(changed.filter(l => l.date <= '2026-09-17'), immutable);
  assert.ok(changed.filter(l => l.date > '2026-09-17').every(l => l.words.length === 10 && l.words.every(w => w.topic === '旅行生活')));
});

test('planning is idempotent, rolls the window and never fills missed past days', () => {
  const s = state({ settings: { ...core.defaultSettings(), reminder: true } });
  s.lessons = core.planLessons(s, bank, '2026-09-17', false);
  assert.deepEqual(core.planLessons(s, bank, '2026-09-17', false), s.lessons);
  const roll = core.planLessons(s, bank, '2026-09-18', false);
  assert.equal(roll.length, 31);
  assert.equal(roll.at(-1).date, '2026-10-17');
  const offline = state();
  offline.lessons = core.planLessons(offline, bank, '2026-09-17', false);
  assert.equal(offline.lessons.length, 21);
  const afterGap = core.planLessons(offline, bank, '2026-11-20', false);
  assert.equal(afterGap.length, 42);
  assert.ok(!afterGap.some(l => l.date === '2026-11-19'));
});

test('custom domains deduplicate spellings and do not accidentally lose matching words', () => {
  const custom = [{ ...bank[0], topic: 'engineering' }, { ...bank[0], topic: 'engineering' }];
  assert.equal(core.availableWords(bank, custom, 'engineering').length, 1);
  assert.equal(core.availableWords(bank, custom, '全部领域').length, bank.length);
  assert.throws(() => core.createLesson('2026-09-17', { ...core.defaultSettings(), topics: ['engineering'] }, custom.slice(0, 1), []), /只有/);
});

test('learn toggle only accepts words in that daily lesson', () => {
  const lesson = core.createLesson('2026-09-17', core.defaultSettings(), bank, []);
  core.toggleReviewed(lesson, 'missing-word');
  assert.deepEqual(lesson.reviewed, []);
  core.toggleReviewed(lesson, lesson.words[0].word);
  assert.equal(lesson.reviewed.length, 1);
  core.toggleReviewed(lesson, lesson.words[0].word);
  assert.equal(lesson.reviewed.length, 0);
});

test('invalid persisted settings and broken history fail closed', () => {
  for (const count of [0, 21, 1.5, NaN]) assert.throws(() => core.validateSettings({ ...core.defaultSettings(), count }));
  for (const hour of [-1, 24]) assert.throws(() => core.validateSettings({ ...core.defaultSettings(), hour }));
  assert.throws(() => core.validateSettings({ ...core.defaultSettings(), minute: 60 }));
  assert.throws(() => core.validateSettings({ ...core.defaultSettings(), online: 'yes' }));
  assert.throws(() => core.validateState({ ...state(), version: 99 }));
  const s = state({ lessons: core.importLegacy(legacy, bank) });
  core.validateState(s);
  s.lessons.push(s.lessons[0]);
  assert.throws(() => core.validateState(s));
});

test('backup merge preserves existing lessons/settings and does not restore device calendar IDs', () => {
  const current = state();
  current.lessons = core.planLessons(current, bank, '2026-09-17', false);
  const incoming = state({ settings: { ...core.defaultSettings(), count: 20 }, lessons: core.importLegacy(legacy, bank) });
  incoming.lessons[0].eventId = 500;
  incoming.lessons[0].scheduledAt = 1;
  incoming.lessons.push({ ...structuredClone(current.lessons[0]), words: [] });
  incoming.lessons.push(core.createLesson('2026-11-01', incoming.settings, bank, []));
  const merged = core.mergeBackup(current, incoming, '2026-09-17');
  assert.equal(merged.lessons.length, 30);
  assert.deepEqual(merged.settings, current.settings);
  assert.deepEqual(merged.lessons.find(l => l.date === '2026-09-17'), current.lessons[0]);
  assert.equal(merged.lessons[0].eventId, -1);
  assert.equal(merged.lessons[0].scheduledAt, 0);
  assert.equal(merged.lessons[0].origin, '备份导入');
  assert.deepEqual(core.mergeBackup(merged, incoming, '2026-09-17'), merged);
  assert.equal(current.lessons.length, 21);
});

test('statutory holidays override weekdays and makeup workdays override weekends', () => {
  assert.equal(core.validWorkCalendar(core.BUILTIN_WORK_CALENDAR), true);
  for (const date of ['2026-01-04', '2026-02-14', '2026-02-28', '2026-05-09', '2026-09-20', '2026-10-10']) {
    assert.equal(core.isWeekday(date), false);
    assert.equal(core.isWorkday(date), true, date);
  }
  for (const date of ['2026-01-01', '2026-02-23', '2026-04-06', '2026-05-05', '2026-06-19', '2026-09-25', '2026-10-07']) {
    assert.equal(core.isWeekday(date), true);
    assert.equal(core.isWorkday(date), false, date);
  }
  assert.equal(core.isWorkday('2026-09-19'), false);
  assert.equal(core.isWorkday('2026-09-21'), true);
  assert.equal(core.isWorkday('2027-01-04'), undefined);
  assert.equal(core.validWorkCalendar({ ...core.BUILTIN_WORK_CALENDAR, papers: [] }), false);
  assert.equal(core.validWorkCalendar({ ...core.BUILTIN_WORK_CALENDAR, days: [] }), false);
  assert.equal(core.validWorkCalendar({ ...core.BUILTIN_WORK_CALENDAR, days: [...core.BUILTIN_WORK_CALENDAR.days, core.BUILTIN_WORK_CALENDAR.days[0]] }), false);
});

test('excluded word cannot return through custom words, existing plans or backup imports', () => {
  const forbidden = { ...bank[0], word: ' AbAnDoN ' };
  assert.equal(core.allowedWord(forbidden), false);
  assert.equal(core.allowedWord({ ...bank[0], example: 'Please abandon this plan.' }), false);
  assert.ok(!core.availableWords([forbidden, ...bank], [forbidden], '全部领域').some(w => !core.allowedWord(w)));
  const s = state();
  s.lessons = core.planLessons(s, bank, '2026-09-17', false);
  s.lessons[0].words[0] = forbidden;
  s.lessons[0].reviewed = [forbidden.word];
  s.lessons[0].reviewWords = [forbidden.word];
  s.lessons[0].phrase = 'abandon';
  const past = { ...structuredClone(s.lessons[0]), date: '2026-09-16' };
  s.lessons.unshift(past);
  const planned = core.planLessons(s, bank, '2026-09-17', false);
  assert.equal(planned.find(l => l.date === '2026-09-17').words.length, 5);
  assert.equal(planned[0].words.length, 4);
  assert.ok(!/\babandon\b/i.test(JSON.stringify(planned)));
  assert.deepEqual(core.planLessons({ ...s, lessons: planned }, bank, '2026-09-17', false), planned);
  const restored = core.mergeBackup(state(), { ...s, customWords: [forbidden] }, '2026-09-17');
  assert.ok(!/\babandon\b/i.test(JSON.stringify(restored)));
});
