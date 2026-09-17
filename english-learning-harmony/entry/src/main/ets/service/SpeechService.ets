import { textToSpeech } from '@kit.CoreSpeechKit';

export class SpeechService {
  private engine: textToSpeech.TextToSpeechEngine | undefined = undefined;
  private sequence: number = 0;
  private pending: ((message: string) => void) | undefined = undefined;

  async speak(text: string, speed: number): Promise<void> {
    this.stop();
    const sequence: number = ++this.sequence;
    if (!this.engine) {
      let created: textToSpeech.TextToSpeechEngine;
      try {
        created = await textToSpeech.createEngine({ language: 'en-US', person: 0, online: 1 });
      } catch (_) {
        created = await textToSpeech.createEngine({ language: 'zh-CN', person: 0, online: 1 });
      }
      if (sequence !== this.sequence) { created.shutdown(); return; }
      this.engine = created;
    }
    if (sequence !== this.sequence) { return; }
    const engine: textToSpeech.TextToSpeechEngine = this.engine;
    const requestId: string = `daily-${Date.now()}-${sequence}`;
    await new Promise<void>((resolve, reject) => {
      let timer: number = -1;
      const finish = (message: string): void => {
        if (timer === -1) { return; }
        clearTimeout(timer);
        timer = -1;
        this.pending = undefined;
        if (message) { reject(new Error(message)); } else { resolve(); }
      };
      timer = setTimeout(() => { finish('语音播放超时，请检查系统语音服务。'); engine.stop(); }, 30000);
      this.pending = finish;
      const listener: textToSpeech.SpeakListener = {
        onStart: (_id: string, _response: textToSpeech.StartResponse): void => {},
        onComplete: (id: string, response: textToSpeech.CompleteResponse): void => {
          if (id === requestId && response.type === 1) { finish(''); }
        },
        onStop: (id: string, _response: textToSpeech.StopResponse): void => { if (id === requestId) { finish(''); } },
        onData: (_id: string, _audio: ArrayBuffer, _response: textToSpeech.SynthesisResponse): void => {},
        onError: (id: string, _code: number, message: string): void => {
          if (id === requestId) { finish(`语音暂不可用：${message}`); }
        }
      };
      try {
        engine.setListener(listener);
        engine.speak(text, { requestId: requestId, extraParams: { speed: speed, volume: 1, queueMode: 1 } });
      } catch (_) { finish('语音启动失败，请检查系统语音服务。'); }
    });
  }

  stop(): void {
    this.sequence++;
    this.pending?.('');
    this.pending = undefined;
    this.engine?.stop();
  }

  release(): void {
    this.stop();
    this.engine?.shutdown();
    this.engine = undefined;
  }
}

export const speechService: SpeechService = new SpeechService();
