import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const walk = dir => fs.readdirSync(dir, { withFileTypes: true }).flatMap(item => {
  if (['node_modules', 'oh_modules', 'build', '.hvigor', '.typecheck', 'dist'].includes(item.name)) return [];
  const absolute = path.join(dir, item.name);
  return item.isDirectory() ? walk(absolute) : [absolute];
});
const files = walk(root);
let parsed = 0;
for (const file of files.filter(file => /\.json5?$/.test(file))) { JSON.parse(fs.readFileSync(file, 'utf8')); parsed++; }
const resources = path.join(root, 'entry/src/main/resources/base');
const strings = new Set(JSON.parse(fs.readFileSync(path.join(resources, 'element/string.json'))).string.map(s => s.name));
const colors = new Set(JSON.parse(fs.readFileSync(path.join(resources, 'element/color.json'))).color.map(c => c.name));
const moduleText = fs.readFileSync(path.join(root, 'entry/src/main/module.json5'), 'utf8');
for (const [, type, name] of moduleText.matchAll(/\$(string|color|media|profile):([a-z_]+)/g)) {
  const found = type === 'string' ? strings.has(name) : type === 'color' ? colors.has(name) :
    fs.readdirSync(path.join(resources, type)).some(file => file.startsWith(`${name}.`));
  if (!found) throw new Error(`Missing resource: ${type}:${name}`);
}
for (const file of files.filter(file => /\.(ets|ts)$/.test(file))) {
  for (const [, relative] of fs.readFileSync(file, 'utf8').matchAll(/from ['"](\.[^'"]+)['"]/g)) {
    if (!['.ets', '.ts', ''].some(ext => fs.existsSync(path.resolve(path.dirname(file), relative + ext)))) throw new Error(`Missing import ${relative} in ${file}`);
  }
}
const app = JSON.parse(fs.readFileSync(path.join(root, 'AppScope/app.json5')));
if (app.app.bundleName !== 'com.example.dailyenglish') throw new Error('Bundle name changed unexpectedly');
console.log(`PASS: ${parsed} JSON/config files, resources and relative imports. This is not a HarmonyOS compile.`);
