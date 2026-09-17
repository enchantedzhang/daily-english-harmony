import { textToSpeech } from '@kit.CoreSpeech';

export async function speak(text: string): Promise<void> {
  try {
    await textToSpeech.createSynthesizer({ language: 'en-US', online: false }).then(async synthesizer => {
      await synthesizer.speak({ text });
      synthesizer.release();
    });
  } catch (_) {
    // 设备无离线语音包时，界面仍可正常使用；可在此处接入在线语音服务。
  }
}
