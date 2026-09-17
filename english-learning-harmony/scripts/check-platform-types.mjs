import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import ts from 'typescript';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const sdk = path.resolve(process.argv[2] || '');
if (!fs.existsSync(path.join(sdk, 'kits/@kit.CalendarKit.d.ts'))) throw new Error('Pass the official OpenHarmony interface_sdk-js directory.');
const out = path.join(root, '.typecheck');
fs.mkdirSync(path.join(out, 'model'), { recursive: true });
fs.mkdirSync(path.join(out, 'service'), { recursive: true });
fs.copyFileSync(path.join(root, 'entry/src/main/ets/model/LearningCore.ts'), path.join(out, 'model/LearningCore.ts'));
fs.copyFileSync(path.join(root, 'entry/src/main/ets/model/AudioCacheCore.ts'), path.join(out, 'model/AudioCacheCore.ts'));
const services = ['LearningStore', 'CalendarReminderService', 'DictionaryService', 'RecordedAudioService', 'AudioCacheService', 'AudioUpdates', 'WorkCalendarService'];
const inputs = services.map(name => {
  const dest = path.join(out, `service/${name}.ts`);
  fs.copyFileSync(path.join(root, `entry/src/main/ets/service/${name}.ets`), dest);
  return dest;
});
fs.mkdirSync(path.join(out, 'background'), { recursive: true });
const extension = path.join(out, 'background/AudioUpdateAbility.ts');
fs.copyFileSync(path.join(root, 'entry/src/main/ets/background/AudioUpdateAbility.ets'), extension);
inputs.push(extension);
const program = ts.createProgram(inputs, {
  noEmit: true, strict: true, skipLibCheck: true, target: ts.ScriptTarget.ES2022,
  module: ts.ModuleKind.ESNext, moduleResolution: ts.ModuleResolutionKind.Node10,
  baseUrl: sdk, paths: { '@kit.*': ['kits/@kit.*.d.ts'], '@ohos.*': ['api/@ohos.*.d.ts'], '@hms.*': ['api/@hms.*.d.ts'] }
});
const diagnostics = ts.getPreEmitDiagnostics(program);
if (diagnostics.length) {
  console.error(ts.formatDiagnosticsWithColorAndContext(diagnostics, { getCanonicalFileName: f => f, getCurrentDirectory: () => root, getNewLine: () => '\n' }));
  process.exitCode = 1;
} else {
  console.log('PASS: core, local store, calendar, HTTP and audio service types against official OpenHarmony declarations.');
  console.log('Scope: excludes ArkUI DSL, Huawei CoreSpeechKit, packaging, device runtime and ArkTS-specific restrictions.');
}
