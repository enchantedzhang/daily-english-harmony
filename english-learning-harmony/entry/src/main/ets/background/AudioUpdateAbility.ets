import { WorkSchedulerExtensionAbility, workScheduler } from '@kit.BackgroundTasksKit';
import { AudioCacheService } from '../service/AudioCacheService';

export default class AudioUpdateAbility extends WorkSchedulerExtensionAbility {
  private cache: AudioCacheService = new AudioCacheService();
  onWorkStart(work: workScheduler.WorkInfo): void {
    if (work.workId === 1701) {
      this.cache.synchronize(this.context, 70000).finally(() => {
        try { workScheduler.stopWork(work, false); } catch (_) {}
      });
    }
  }
  onWorkStop(_work: workScheduler.WorkInfo): void { this.cache.cancel(); }
}
