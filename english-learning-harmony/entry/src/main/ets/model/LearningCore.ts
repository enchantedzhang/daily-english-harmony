export interface WordItem {
  word: string;
  phonetic: string;
  pos: string;
  meaning: string;
  example: string;
  translation: string;
  topic: string;
  source: string;
}

export interface DayLesson {
  date: string;
  words: WordItem[];
  phrase: string;
  topic: string;
  origin: string;
  reviewed: string[];
  reviewWords: string[];
  scheduledAt: number;
  eventId: number;
}

export interface LearningSettings {
  count: number;
  topic: string;
  hour: number;
  topics?: string[];
  minute: number;
  reminder: boolean;
  online: boolean;
  audioAutoUpdate?: boolean;
  speechRate: number;
}

export interface LearningState {
  version: number;
  reminderSyncPending: boolean;
  settings: LearningSettings;
  lessons: DayLesson[];
  customWords: WordItem[];
  workCalendars?: WorkCalendar[];
}

export interface WorkDay { name: string; date: string; isOffDay: boolean; }
export interface WorkCalendar { year: number; papers: string[]; days: WorkDay[]; }

export function validWorkCalendar(calendar: WorkCalendar): boolean {
  return !!calendar && Number.isInteger(calendar.year) && calendar.year >= 2026 && calendar.year <= 2100 &&
    Array.isArray(calendar.papers) && calendar.papers.length > 0 && calendar.papers.every((paper: string) =>
      typeof paper === 'string' && /^https:\/\/(www\.)?gov\.cn\//.test(paper)) &&
    Array.isArray(calendar.days) && calendar.days.length >= 20 && calendar.days.length <= 100 &&
    calendar.days.every((day: WorkDay) => !!day && typeof day.date === 'string' && validDate(day.date) &&
      Number(day.date.slice(0, 4)) === calendar.year && typeof day.name === 'string' && typeof day.isOffDay === 'boolean') &&
    new Set(calendar.days.map((day: WorkDay) => day.date)).size === calendar.days.length &&
    ['元旦', '春节', '清明节', '劳动节', '端午节', '中秋节', '国庆节'].every((name: string) =>
      calendar.days.some((day: WorkDay) => day.name.includes(name)));
}

export function isWorkday(date: string, calendars: WorkCalendar[] = []): boolean | undefined {
  const year: number = Number(date.slice(0, 4));
  const calendar: WorkCalendar | undefined = calendars.find((item: WorkCalendar) => item.year === year && validWorkCalendar(item)) ||
    (BUILTIN_WORK_CALENDAR.year === year ? BUILTIN_WORK_CALENDAR : undefined);
  if (!calendar) { return undefined; }
  const exception: WorkDay | undefined = calendar.days.find((day: WorkDay) => day.date === date);
  return exception ? !exception.isOffDay : isWeekday(date);
}

export interface LegacyEntry { date: string; words: string[]; phrase: string; }
export interface LegacyHistory { entries: LegacyEntry[]; }
export interface CalendarCell { key: string; date: string; day: number; inMonth: boolean; }

export const TOPICS: string[] = ['全部领域', '日常交流', '职场沟通', '计算机技术', '旅行生活', '阅读表达'];
export const PLAN_DAYS: number = 30;

export function isWeekday(date: string): boolean {
  const day: number = parseDate(date).getDay();
  return day >= 1 && day <= 5;
}

export function allowedText(text: string): boolean { return !/\babandon\b/i.test(text); }

export function allowedWord(word: WordItem): boolean {
  return [word.word, word.example, word.meaning, word.translation, word.source, word.topic].every(allowedText);
}

export function cleanLesson(lesson: DayLesson): DayLesson {
  const clean: DayLesson = JSON.parse(JSON.stringify(lesson)) as DayLesson;
  clean.words = clean.words.filter(allowedWord);
  clean.reviewed = clean.reviewed.filter(allowedText);
  clean.reviewWords = clean.reviewWords.filter(allowedText);
  if (!allowedText(clean.phrase)) { clean.phrase = ''; }
  return clean;
}

export function defaultSettings(): LearningSettings {
  return { count: 5, topic: '日常交流', topics: ['日常交流'], hour: 9, minute: 0, reminder: false, online: false, audioAutoUpdate: true, speechRate: 1 };
}

export function dateKey(date: Date): string {
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
}

export function parseDate(key: string): Date {
  const parts: number[] = key.split('-').map((part: string) => Number(part));
  return new Date(parts[0], parts[1] - 1, parts[2], 12);
}

export function validDate(key: string): boolean {
  return /^\d{4}-\d{2}-\d{2}$/.test(key) && dateKey(parseDate(key)) === key;
}

export function addDays(key: string, days: number): string {
  const date: Date = parseDate(key);
  date.setDate(date.getDate() + days);
  return dateKey(date);
}

export function reminderTime(key: string, settings: LearningSettings): number {
  const date: Date = parseDate(key);
  date.setHours(settings.hour, settings.minute, 0, 0);
  return date.getTime();
}

export function calendarCells(year: number, month: number): CalendarCell[] {
  const first: Date = new Date(year, month, 1, 12);
  const offset: number = (first.getDay() + 6) % 7;
  const cells: CalendarCell[] = [];
  for (let i: number = 0; i < 42; i++) {
    const date: Date = new Date(year, month, 1 - offset + i, 12);
    const key: string = dateKey(date);
    cells.push({ key: key, date: key, day: date.getDate(), inMonth: date.getMonth() === month });
  }
  return cells;
}

export function validateSettings(settings: LearningSettings): void {
  if (!Number.isInteger(settings.count) || settings.count < 1 || settings.count > 20 ||
    !Number.isInteger(settings.hour) || settings.hour < 0 || settings.hour > 23 ||
    !Number.isInteger(settings.minute) || settings.minute < 0 || settings.minute > 59 ||
    typeof settings.topic !== 'string' || !settings.topic.trim() || settings.topic.length > 60 ||
    typeof settings.online !== 'boolean' || typeof settings.reminder !== 'boolean' ||
    (settings.audioAutoUpdate !== undefined && typeof settings.audioAutoUpdate !== 'boolean') ||
    (settings.topics !== undefined && (!Array.isArray(settings.topics) || settings.topics.length === 0 ||
      settings.topics.some((topic: string) => typeof topic !== 'string' || !topic.trim() || topic.length > 60) ||
      new Set(settings.topics).size !== settings.topics.length)) ||
    ![0.75, 1, 1.25].includes(settings.speechRate)) {
    throw new Error('设置无效，请检查每日数量、领域和时间。');
  }
}

export function emptyLesson(date: string): DayLesson {
  return { date: date, words: [], phrase: '', topic: '', origin: '', reviewed: [], reviewWords: [], scheduledAt: 0, eventId: -1 };
}

export function importLegacy(history: LegacyHistory, bank: WordItem[]): DayLesson[] {
  return history.entries.filter((entry: LegacyEntry) => validDate(entry.date)).map((entry: LegacyEntry) => {
    const lesson: DayLesson = emptyLesson(entry.date);
    lesson.origin = '原历史记录';
    lesson.topic = '阅读表达';
    lesson.phrase = entry.phrase;
    lesson.words = entry.words.map((word: string) => {
      const known: WordItem | undefined = bank.find((item: WordItem) => item.word === word);
      return known ? known : { word: word, phonetic: '', pos: '', meaning: '原记录未包含释义，可点击在线查词', example: '', translation: '', topic: '阅读表达', source: '原历史记录' };
    });
    return lesson;
  });
}

export function availableWords(bank: WordItem[], custom: WordItem[], topic: string): WordItem[] {
  const result: WordItem[] = [];
  const seen: Set<string> = new Set<string>();
  // Custom domains may contain the same spelling as a bundled domain.
  for (const item of bank.concat(custom)) {
    const key: string = item.word.toLowerCase();
    if (allowedWord(item) && (topic === '全部领域' || item.topic === topic) && !seen.has(key)) {
      seen.add(key);
      result.push(item);
    }
  }
  return result;
}

export function createLesson(date: string, settings: LearningSettings, bank: WordItem[], previous: DayLesson[]): DayLesson {
  validateSettings(settings);
  if (!validDate(date)) { throw new Error('日期无效'); }
  const lastSeen: Map<string, string> = new Map<string, string>();
  previous.filter((lesson: DayLesson) => lesson.date < date).sort((a: DayLesson, b: DayLesson) => a.date.localeCompare(b.date))
    .forEach((lesson: DayLesson) => lesson.words.forEach((word: WordItem) => lastSeen.set(word.word.toLowerCase(), lesson.date)));
  // New words first; once exhausted, review the least recently scheduled words.
  const compare = (a: WordItem, b: WordItem): number => {
    const aSeen: string = lastSeen.get(a.word.toLowerCase()) || '';
    const bSeen: string = lastSeen.get(b.word.toLowerCase()) || '';
    return aSeen.localeCompare(bSeen) || a.word.localeCompare(b.word);
  };
  const lesson: DayLesson = emptyLesson(date);
  const topics: string[] = selectedTopics(settings);
  lesson.topic = topics.join('、');
  lesson.origin = '每日词单';
  for (const topic of topics) {
    const pool: WordItem[] = availableWords(bank, [], topic).filter((item: WordItem) =>
      !lesson.words.some((chosen: WordItem) => chosen.word.toLowerCase() === item.word.toLowerCase()));
    if (pool.length < settings.count) { throw new Error(`${topic}只有 ${pool.length} 个可用的不重复词，请减少每领域数量或扩充词库。`); }
    pool.sort(compare);
    lesson.words = lesson.words.concat(pool.slice(0, settings.count));
  }
  lesson.reviewWords = lesson.words.filter((word: WordItem) => lastSeen.has(word.word.toLowerCase())).map((word: WordItem) => word.word);
  return lesson;
}

export function selectedTopics(settings: LearningSettings): string[] {
  const topics: string[] = settings.topics || [settings.topic];
  return topics.includes('全部领域') ? TOPICS.slice(1) : topics.slice();
}

export function planLessons(state: LearningState, bank: WordItem[], today: string, replaceFuture: boolean): DayLesson[] {
  const pool: WordItem[] = bank.concat(state.customWords);
  const lessons: DayLesson[] = state.lessons.filter((lesson: DayLesson) => !replaceFuture || lesson.date <= today).map((lesson: DayLesson) => {
    const clean: DayLesson = cleanLesson(lesson);
    if (lesson.date >= today) {
      for (const removed of lesson.words.filter((word: WordItem) => !allowedWord(word))) {
        const candidates: WordItem[] = availableWords(pool, [], removed.topic).filter((word: WordItem) =>
          !clean.words.some((existing: WordItem) => existing.word.toLowerCase() === word.word.toLowerCase()));
        if (candidates.length > 0) {
          const settings: LearningSettings = { count: 1, topic: removed.topic, topics: [removed.topic], hour: 9, minute: 0, reminder: false, online: false, speechRate: 1 };
          const replacement: DayLesson = createLesson(lesson.date, settings, candidates, state.lessons);
          clean.words = clean.words.concat(replacement.words);
          clean.reviewWords = clean.reviewWords.concat(replacement.reviewWords);
        }
      }
    }
    return clean;
  });
  const days: number = state.settings.reminder ? PLAN_DAYS : 21;
  for (let offset: number = 0; offset < days; offset++) {
    const date: string = addDays(today, offset);
    if (!lessons.some((lesson: DayLesson) => lesson.date === date)) {
      lessons.push(createLesson(date, state.settings, pool, lessons));
    }
  }
  return lessons.sort((a: DayLesson, b: DayLesson) => a.date.localeCompare(b.date));
}

export function toggleReviewed(lesson: DayLesson, word: string): void {
  if (!lesson.words.some((item: WordItem) => item.word === word)) { return; }
  lesson.reviewed = lesson.reviewed.includes(word) ? lesson.reviewed.filter((item: string) => item !== word) : lesson.reviewed.concat([word]);
}

export function validateState(state: LearningState): void {
  if (!state || state.version !== 1 || typeof state.reminderSyncPending !== 'boolean' || !state.settings || !Array.isArray(state.lessons) || !Array.isArray(state.customWords)) {
    throw new Error('本地数据格式无法读取。为保护历史记录，未覆盖原数据。');
  }
  validateSettings(state.settings);
  if (state.workCalendars !== undefined && (!Array.isArray(state.workCalendars) || !state.workCalendars.every(validWorkCalendar))) {
    throw new Error('工作日日历数据无效。');
  }
  const dates: Set<string> = new Set<string>();
  for (const lesson of state.lessons) {
    if (!validDate(lesson.date) || dates.has(lesson.date) || !Array.isArray(lesson.words) || !Array.isArray(lesson.reviewed) || !Array.isArray(lesson.reviewWords) ||
      typeof lesson.scheduledAt !== 'number' || !Number.isInteger(lesson.eventId) || !lesson.words.every(validWord)) {
      throw new Error('历史词单损坏，未覆盖原数据。');
    }
    dates.add(lesson.date);
  }
  if (!state.customWords.every(validWord)) { throw new Error('自定义词库损坏，未覆盖原数据。'); }
}

export function validWord(word: WordItem): boolean {
  return !!word && typeof word.word === 'string' && word.word.length > 0 && word.word.length <= 80 &&
    typeof word.meaning === 'string' && typeof word.topic === 'string' && typeof word.source === 'string' &&
    typeof word.phonetic === 'string' && typeof word.pos === 'string' && typeof word.example === 'string' && typeof word.translation === 'string';
}

export function mergeBackup(current: LearningState, incoming: LearningState, today: string): LearningState {
  validateState(current);
  validateState(incoming);
  const merged: LearningState = JSON.parse(JSON.stringify(current)) as LearningState;
  for (const lesson of incoming.lessons) {
    if (lesson.date <= today && !merged.lessons.some((item: DayLesson) => item.date === lesson.date)) {
      const restored: DayLesson = JSON.parse(JSON.stringify(lesson)) as DayLesson;
      // Device calendar IDs are not portable; imported history must never manipulate another calendar entry.
      restored.eventId = -1;
      restored.scheduledAt = 0;
      restored.origin = '备份导入';
      merged.lessons.push(cleanLesson(restored));
    }
  }
  for (const word of incoming.customWords) {
    if (allowedWord(word) && !merged.customWords.some((item: WordItem) => item.word === word.word && item.topic === word.topic)) {
      merged.customWords.push(word);
    }
  }
  merged.lessons.sort((a: DayLesson, b: DayLesson) => a.date.localeCompare(b.date));
  return merged;
}

// holiday-cn (MIT), verified against the State Council 2026 notice.
export const BUILTIN_WORK_CALENDAR: WorkCalendar = {
  "year": 2026,
  "papers": [
    "https://www.gov.cn/zhengce/zhengceku/202511/content_7047091.htm"
  ],
  "days": [
    {
      "name": "元旦",
      "date": "2026-01-01",
      "isOffDay": true
    },
    {
      "name": "元旦",
      "date": "2026-01-02",
      "isOffDay": true
    },
    {
      "name": "元旦",
      "date": "2026-01-03",
      "isOffDay": true
    },
    {
      "name": "元旦",
      "date": "2026-01-04",
      "isOffDay": false
    },
    {
      "name": "春节",
      "date": "2026-02-14",
      "isOffDay": false
    },
    {
      "name": "春节",
      "date": "2026-02-15",
      "isOffDay": true
    },
    {
      "name": "春节",
      "date": "2026-02-16",
      "isOffDay": true
    },
    {
      "name": "春节",
      "date": "2026-02-17",
      "isOffDay": true
    },
    {
      "name": "春节",
      "date": "2026-02-18",
      "isOffDay": true
    },
    {
      "name": "春节",
      "date": "2026-02-19",
      "isOffDay": true
    },
    {
      "name": "春节",
      "date": "2026-02-20",
      "isOffDay": true
    },
    {
      "name": "春节",
      "date": "2026-02-21",
      "isOffDay": true
    },
    {
      "name": "春节",
      "date": "2026-02-22",
      "isOffDay": true
    },
    {
      "name": "春节",
      "date": "2026-02-23",
      "isOffDay": true
    },
    {
      "name": "春节",
      "date": "2026-02-28",
      "isOffDay": false
    },
    {
      "name": "清明节",
      "date": "2026-04-04",
      "isOffDay": true
    },
    {
      "name": "清明节",
      "date": "2026-04-05",
      "isOffDay": true
    },
    {
      "name": "清明节",
      "date": "2026-04-06",
      "isOffDay": true
    },
    {
      "name": "劳动节",
      "date": "2026-05-01",
      "isOffDay": true
    },
    {
      "name": "劳动节",
      "date": "2026-05-02",
      "isOffDay": true
    },
    {
      "name": "劳动节",
      "date": "2026-05-03",
      "isOffDay": true
    },
    {
      "name": "劳动节",
      "date": "2026-05-04",
      "isOffDay": true
    },
    {
      "name": "劳动节",
      "date": "2026-05-05",
      "isOffDay": true
    },
    {
      "name": "劳动节",
      "date": "2026-05-09",
      "isOffDay": false
    },
    {
      "name": "端午节",
      "date": "2026-06-19",
      "isOffDay": true
    },
    {
      "name": "端午节",
      "date": "2026-06-20",
      "isOffDay": true
    },
    {
      "name": "端午节",
      "date": "2026-06-21",
      "isOffDay": true
    },
    {
      "name": "国庆节",
      "date": "2026-09-20",
      "isOffDay": false
    },
    {
      "name": "中秋节",
      "date": "2026-09-25",
      "isOffDay": true
    },
    {
      "name": "中秋节",
      "date": "2026-09-26",
      "isOffDay": true
    },
    {
      "name": "中秋节",
      "date": "2026-09-27",
      "isOffDay": true
    },
    {
      "name": "国庆节",
      "date": "2026-10-01",
      "isOffDay": true
    },
    {
      "name": "国庆节",
      "date": "2026-10-02",
      "isOffDay": true
    },
    {
      "name": "国庆节",
      "date": "2026-10-03",
      "isOffDay": true
    },
    {
      "name": "国庆节",
      "date": "2026-10-04",
      "isOffDay": true
    },
    {
      "name": "国庆节",
      "date": "2026-10-05",
      "isOffDay": true
    },
    {
      "name": "国庆节",
      "date": "2026-10-06",
      "isOffDay": true
    },
    {
      "name": "国庆节",
      "date": "2026-10-07",
      "isOffDay": true
    },
    {
      "name": "国庆节",
      "date": "2026-10-10",
      "isOffDay": false
    }
  ]
};
