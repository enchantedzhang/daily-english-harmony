import { UIAbility } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { speechService } from '../service/SpeechService';
import { recordedAudio } from '../service/RecordedAudioService';
import { audioUpdates } from '../service/AudioUpdates';

export default class EntryAbility extends UIAbility {
  onWindowStageCreate(windowStage: window.WindowStage): void {
    AppStorage.setOrCreate('appForeground', true);
    AppStorage.setOrCreate('foregroundVersion', Date.now());
    windowStage.loadContent('pages/Index');
  }

  onForeground(): void { AppStorage.setOrCreate('appForeground', true); AppStorage.setOrCreate('foregroundVersion', Date.now()); }
  onBackground(): void { AppStorage.setOrCreate('appForeground', false); audioUpdates.stop(); recordedAudio.stop(); speechService.stop(); }
  onDestroy(): void { audioUpdates.stop(); recordedAudio.stop(); speechService.release(); }
}
