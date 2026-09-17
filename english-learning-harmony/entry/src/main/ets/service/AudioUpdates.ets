import { common } from '@kit.AbilityKit';
import { connection } from '@kit.NetworkKit';
import { workScheduler } from '@kit.BackgroundTasksKit';
import { audioCache } from './AudioCacheService';

const WORK: workScheduler.WorkInfo = { workId: 1701, bundleName: 'com.example.dailyenglish',
  abilityName: 'AudioUpdateAbility', isPersisted: true, networkType: workScheduler.NetworkType.NETWORK_TYPE_ANY,
  isRepeat: true, repeatCycleTime: 6 * 60 * 60 * 1000 };

export class AudioUpdates {
  private context: common.Context | undefined = undefined;
  private timer: number = -1;
  private network: connection.NetConnection | undefined = undefined;
  private running: boolean = false;
  private again: boolean = false;
  backgroundStatus: string = '';

  start(context: common.Context): void {
    this.context = context;
    if (this.timer === -1) {
      this.timer = setInterval(() => this.update(), 60000);
      try {
        const network: connection.NetConnection = connection.createNetConnection();
        this.network = network;
        network.on('netAvailable', () => this.update());
        network.register(() => {});
      } catch (_) { /* The periodic foreground retry remains available. */ }
    }
    this.update();
  }

  async schedule(enabled: boolean): Promise<void> {
    try {
      let existing: workScheduler.WorkInfo | undefined = undefined;
      try { existing = await workScheduler.getWorkStatus(WORK.workId); } catch (_) {}
      if (!enabled && existing) { workScheduler.stopWork(existing, true); }
      else if (enabled && !existing) { workScheduler.startWork(WORK); }
      this.backgroundStatus = enabled ? '后台更新已登记，执行时间由系统决定' : '后台自动更新已关闭';
    } catch (_) { this.backgroundStatus = '后台更新登记失败，打开应用仍会自动更新'; }
  }

  changed(): void { audioCache.cancel(); this.again = true; this.update(); }

  async update(force: boolean = false): Promise<void> {
    if (!this.context) { return; }
    if (this.running) { return; }
    this.running = true;
    this.again = false;
    try { await audioCache.synchronize(this.context, 70000, force); }
    finally { this.running = false; if (this.again && this.context) { this.update(); } }
  }

  stop(): void {
    this.context = undefined;
    if (this.timer !== -1) { clearInterval(this.timer); this.timer = -1; }
    try { this.network?.unregister(() => {}); } catch (_) {}
    this.network = undefined;
    audioCache.cancel();
  }
}
export const audioUpdates: AudioUpdates = new AudioUpdates();
