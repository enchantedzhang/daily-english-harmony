import { http } from '@kit.NetworkKit';
import { WorkCalendar, LearningState, addDays, dateKey, isWorkday, validWorkCalendar } from '../model/LearningCore';
import { LearningStore } from './LearningStore';

export class WorkCalendarService {
  status: string = '中国大陆法定工作日（含调休）';
  private attempted: number = 0;
  private running: boolean = false;

  async update(store: LearningStore, force: boolean = false): Promise<void> {
    if (this.running) { return; }
    const today: string = dateKey(new Date());
    if (!force && Date.now() - this.attempted < 24 * 60 * 60 * 1000) { return; }
    this.running = true;
    this.attempted = Date.now();
    const years: number[] = [Number(today.slice(0, 4))];
    const lastYear: number = Number(addDays(today, 29).slice(0, 4));
    if (!years.includes(lastYear)) { years.push(lastYear); }
    let failed: boolean = false;
    try {
      for (const year of years) {
        const request: http.HttpRequest = http.createHttp();
        try {
          const response: http.HttpResponse = await request.request(`https://cdn.jsdelivr.net/gh/NateScarlet/holiday-cn@master/${year}.json`, {
            method: http.RequestMethod.GET, expectDataType: http.HttpDataType.STRING,
            connectTimeout: 5000, readTimeout: 5000, maxLimit: 128 * 1024, usingCache: false
          });
          if (response.responseCode !== 200 || typeof response.result !== 'string') { throw new Error('日历不可用'); }
          const calendar: WorkCalendar = JSON.parse(response.result) as WorkCalendar;
          if (!validWorkCalendar(calendar) || calendar.year !== year) { throw new Error('年度安排不完整'); }
          // Read fresh state after the await so a download cannot overwrite settings or learning marks.
          const next: LearningState = store.snapshot();
          const existing: WorkCalendar | undefined = next.workCalendars?.find((item: WorkCalendar) => item.year === year);
          if (JSON.stringify(existing) !== JSON.stringify(calendar)) {
            next.workCalendars = (next.workCalendars || []).filter((item: WorkCalendar) => item.year !== year).concat([calendar]);
            if (next.settings.reminder) { next.reminderSyncPending = true; }
            store.commit(next);
          }
        } catch (_) { failed = true; }
        finally { request.destroy(); }
      }
      const state: LearningState = store.snapshot();
      const missing: boolean = years.some((year: number) => isWorkday(`${year}-01-01`, state.workCalendars) === undefined);
      this.status = missing ? '部分年份调休安排尚不可用：暂停这些日期的推送，请联网更新' :
        failed ? '调休日历更新失败，继续使用已保存的年度安排' : '调休日历已更新，按中国大陆法定工作日推送';
    } finally { this.running = false; }
  }
}

export const workCalendar: WorkCalendarService = new WorkCalendarService();
