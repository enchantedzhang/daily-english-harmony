import { abilityAccessCtrl, common, Permissions } from '@kit.AbilityKit';
import { calendarManager } from '@kit.CalendarKit';
import { DayLesson, LearningState, WordItem, reminderTime, isWorkday, cleanLesson } from '../model/LearningCore';
import { LearningStore } from './LearningStore';

const ACCOUNT_NAME: string = 'dailyenglish.personal.v1';
const MARKER: string = '[daily-english:';

export class CalendarReminderService {
  private busy: boolean = false;

  async synchronize(context: common.UIAbilityContext, store: LearningStore, requestPermission: boolean): Promise<number> {
    if (this.busy) { throw new Error('提醒正在同步，请稍后重试。'); }
    this.busy = true;
    try { return await this.performSync(context, store, requestPermission); }
    finally { this.busy = false; }
  }

  private async performSync(context: common.UIAbilityContext, store: LearningStore, requestPermission: boolean): Promise<number> {
    const permissions: Permissions[] = ['ohos.permission.READ_CALENDAR', 'ohos.permission.WRITE_CALENDAR'];
    const access: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
    const missing: boolean = permissions.some((permission: Permissions) =>
      access.checkAccessTokenSync(context.applicationInfo.accessTokenId, permission) !== abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED);
    if (missing) {
      if (!requestPermission) { throw new Error('日历权限未开启。请在设置中点击“同步提醒”，授权后才能按时提醒。'); }
      const result = await access.requestPermissionsFromUser(context, permissions);
      if (result.authResults.length !== permissions.length || result.authResults.some((value: number) => value !== 0)) {
        throw new Error('未获得日历权限，词单已保存，提醒尚未同步。');
      }
    }
    const state: LearningState = store.snapshot();
    state.lessons = state.lessons.map(cleanLesson);
    const manager: calendarManager.CalendarManager = calendarManager.getCalendarManager(context);
    const calendars: calendarManager.Calendar[] = await manager.getAllCalendars();
    let calendar: calendarManager.Calendar | undefined = calendars.find((item: calendarManager.Calendar) => item.getAccount().name === ACCOUNT_NAME);
    if (!calendar && !state.settings.reminder) {
      for (const lesson of state.lessons) {
        if (lesson.scheduledAt > Date.now()) { lesson.scheduledAt = 0; lesson.eventId = -1; }
      }
      state.reminderSyncPending = false;
      store.commit(state);
      return 0;
    }
    if (!calendar) {
      calendar = await manager.createCalendar({ name: ACCOUNT_NAME, type: calendarManager.CalendarType.LOCAL, displayName: '每日英语' });
    }
    await calendar.setConfig({ enableReminder: state.settings.reminder, color: '#5267D8' });
    const events: calendarManager.Event[] = await calendar.getEvents();
    const now: number = Date.now();
    const desired: DayLesson[] = state.settings.reminder ? state.lessons.filter((lesson: DayLesson) =>
      isWorkday(lesson.date, state.workCalendars) === true && lesson.words.length > 0 && reminderTime(lesson.date, state.settings) > now) : [];
    const retained: Set<number> = new Set<number>();
    let count: number = 0;
    for (const lesson of desired) {
      const marker: string = `${MARKER}${lesson.date}]`;
      const existing: calendarManager.Event | undefined = events.find((event: calendarManager.Event) =>
        (event.description || '').startsWith(marker + '\n') && event.id !== undefined);
      const start: number = reminderTime(lesson.date, state.settings);
      const event: calendarManager.Event = {
        type: calendarManager.EventType.NORMAL,
        title: `每日英语 · ${lesson.words.length} 词 · ${lesson.words.slice(0, 3).map((word: WordItem) => word.word).join(' / ')}${lesson.words.length > 3 ? '…' : ''}`,
        startTime: start,
        endTime: start + 5 * 60 * 1000,
        isAllDay: false,
        reminderTime: [0],
        description: marker + '\n' + lesson.words.map((word: WordItem) => `【${word.topic}】${word.word}：${word.meaning}`).join('\n') + '\n打开「每日英语」查看例句和发音。',
        identifier: `daily-english-${lesson.date}`
      };
      let id: number;
      if (existing && existing.id !== undefined) {
        id = existing.id;
        event.id = id;
        if (existing.startTime !== start || existing.title !== event.title || existing.description !== event.description ||
          !existing.reminderTime || existing.reminderTime.length !== 1 || existing.reminderTime[0] !== 0) {
          await calendar.updateEvent(event);
        }
      } else {
        id = await calendar.addEvent(event);
        if (id < 0) { throw new Error('系统日历未接受提醒，请重试。'); }
      }
      retained.add(id);
      if (lesson.eventId !== id || lesson.scheduledAt !== start) {
        lesson.eventId = id;
        lesson.scheduledAt = start;
        store.commit(state);
      }
      count++;
    }
    for (const event of events) {
      if (event.id !== undefined && event.startTime > now && (event.description || '').startsWith(MARKER) && !retained.has(event.id)) {
        await calendar.deleteEvent(event.id);
      }
    }
    for (const lesson of state.lessons) {
      if (lesson.scheduledAt > now && !retained.has(lesson.eventId)) { lesson.scheduledAt = 0; lesson.eventId = -1; }
    }
    state.reminderSyncPending = false;
    store.commit(state);
    return count;
  }
}

export const calendarReminders: CalendarReminderService = new CalendarReminderService();
