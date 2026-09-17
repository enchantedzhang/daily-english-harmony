import { common } from '@kit.AbilityKit';
import { fileIo } from '@kit.CoreFileKit';
import { media } from '@kit.MediaKit';
import { audioCache } from './AudioCacheService';

export class RecordedAudioService {
  private sequence: number = 0;
  private cancel: (() => void) | undefined = undefined;

  async play(text: string, speed: number, context: common.UIAbilityContext): Promise<boolean> {
    this.stop();
    const sequence: number = ++this.sequence;
    const descriptor: fileIo.File | undefined = audioCache.open(context, text);
    if (!descriptor) { return false; }
    let player: media.AVPlayer;
    try { player = await media.createAVPlayer(); }
    catch (_) { fileIo.closeSync(descriptor); return false; }
    if (sequence !== this.sequence) {
      try { await player.release(); } finally { fileIo.closeSync(descriptor); }
      return true;
    }
    await new Promise<void>((resolve, reject) => {
      let finished: boolean = false;
      const finish = (message: string): void => {
        if (finished) { return; }
        finished = true;
        clearTimeout(timer);
        this.cancel = undefined;
        player.off('stateChange');
        player.off('error');
        player.release().catch(() => {}).then(() => fileIo.closeSync(descriptor)).then(() => {
          if (message) { reject(new Error(message)); } else { resolve(); }
        }).catch(() => reject(new Error('音频资源释放失败，请重试。')));
      };
      const timer: number = setTimeout(() => finish('音频播放超时'), 30000);
      this.cancel = (): void => finish('');
      player.on('stateChange', (state: string): void => {
        if (state === 'initialized') { player.prepare().catch(() => finish('音频准备失败')); }
        else if (state === 'prepared') {
          try {
            player.setSpeed(speed < 1 ? media.PlaybackSpeed.SPEED_FORWARD_0_75_X : speed > 1 ? media.PlaybackSpeed.SPEED_FORWARD_1_25_X : media.PlaybackSpeed.SPEED_FORWARD_1_00_X);
            player.play().catch(() => finish('音频播放失败'));
          } catch (_) { finish('音频播放失败'); }
        } else if (state === 'completed') { finish(''); }
      });
      player.on('error', (): void => finish('音频播放失败'));
      try { player.fdSrc = { fd: descriptor.fd, offset: 0, length: fileIo.statSync(descriptor.fd).size }; }
      catch (_) { finish('音频读取失败'); }
    });
    return true;
  }

  stop(): void { this.sequence++; this.cancel?.(); this.cancel = undefined; }
}

export const recordedAudio: RecordedAudioService = new RecordedAudioService();
