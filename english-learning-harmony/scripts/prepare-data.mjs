import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const rows = fs.readFileSync(path.join(root, 'data/words.tsv'), 'utf8').trim().split(/\r?\n/).slice(1);
const words = rows.map((row, index) => {
  const fields = row.split('\t');
  if (fields.length !== 6 || fields.some(x => !x.trim())) throw new Error(`Invalid word row ${index + 2}`);
  const [topic, word, pos, meaning, example, translation] = fields;
  return { topic, word, pos, meaning, example, translation, phonetic: '', source: '内置词库 · 原创例句' };
});
words.push(...JSON.parse(fs.readFileSync(path.join(root, 'data/extended-words.json'), 'utf8')));
const notes = JSON.parse(fs.readFileSync(path.join(root, 'data/word-study.json'), 'utf8'));
for (const [word, study] of Object.entries(notes)) {
  const entry = words.find(item => item.word === word);
  if (!entry) throw new Error(`Study note has no dictionary entry: ${word}`);
  entry.study = study;
}
if (words.length !== 1000 || new Set(words.map(x => x.word)).size !== 1000) throw new Error('Expected 1000 unique words');
if (words.some(word => /\babandon\b/i.test(JSON.stringify(word)))) throw new Error('Excluded content in dictionary');
const out = path.join(root, 'entry/src/main/resources/rawfile');
fs.mkdirSync(out, { recursive: true });
fs.writeFileSync(path.join(out, 'words.json'), JSON.stringify(words, null, 2) + '\n');
fs.copyFileSync(path.join(root, '../daily-english-vocabulary-history.json'), path.join(out, 'history.json'));
console.log(`Prepared ${words.length} words and original history.`);
