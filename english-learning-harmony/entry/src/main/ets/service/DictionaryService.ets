import { http } from '@kit.NetworkKit';
import { WordItem, allowedText, allowedWord } from '../model/LearningCore';

interface RelatedWord { word: string; defs?: string[]; tags?: string[]; }
interface DictionaryDefinition { definition: string; example?: string; }
interface DictionaryMeaning { partOfSpeech?: string; definitions?: DictionaryDefinition[]; }
interface DictionaryPhonetic { text?: string; }
interface DictionaryLicense { name?: string; url?: string; }
interface DictionaryEntry {
  word: string;
  phonetic?: string;
  phonetics?: DictionaryPhonetic[];
  meanings?: DictionaryMeaning[];
  sourceUrls?: string[];
  license?: DictionaryLicense;
}

async function getJson(url: string): Promise<string> {
  const request: http.HttpRequest = http.createHttp();
  try {
    const response: http.HttpResponse = await request.request(url, {
      method: http.RequestMethod.GET, expectDataType: http.HttpDataType.STRING,
      connectTimeout: 8000, readTimeout: 12000, maxLimit: 1024 * 1024,
      header: { 'Accept': 'application/json' }
    });
    if (response.responseCode === 404) { throw new Error('暂未查到这个词，请保留本地释义。'); }
    if (response.responseCode !== 200) { throw new Error(`词典服务暂不可用（${response.responseCode}），本地学习不受影响。`); }
    if (typeof response.result !== 'string') { throw new Error('词典返回格式异常。'); }
    return response.result;
  } finally { request.destroy(); }
}

export async function lookupWord(word: string): Promise<WordItem> {
  if (!allowedText(word)) { throw new Error('该词已排除。'); }
  if (!/^[a-zA-Z][a-zA-Z '-]{0,79}$/.test(word)) { throw new Error('请输入英文单词。'); }
  const entries: DictionaryEntry[] = JSON.parse(await getJson(`https://api.dictionaryapi.dev/api/v2/entries/en/${encodeURIComponent(word)}`)) as DictionaryEntry[];
  if (!Array.isArray(entries) || entries.length === 0) { throw new Error('词典未返回释义。'); }
  const entry: DictionaryEntry = entries[0];
  const meaning: DictionaryMeaning | undefined = entry.meanings?.find((item: DictionaryMeaning) => !!item.definitions && item.definitions.length > 0);
  const definition: DictionaryDefinition | undefined = meaning?.definitions?.[0];
  if (!definition || typeof definition.definition !== 'string') { throw new Error('词典未返回可用释义。'); }
  const result: WordItem = {
    word: word, phonetic: entry.phonetic || entry.phonetics?.find((item: DictionaryPhonetic) => !!item.text)?.text || '',
    pos: meaning?.partOfSpeech || '', meaning: definition.definition, example: definition.example || '', translation: '', topic: '在线词典',
    source: `Free Dictionary API · ${entry.license?.name || 'Wiktionary'}\n${entry.sourceUrls?.[0] || 'https://dictionaryapi.dev/'}\n${entry.license?.url || ''}`
  };
  if (!allowedWord(result)) { throw new Error('该词典结果包含已排除内容。'); }
  return result;
}

export async function discoverWords(topic: string): Promise<WordItem[]> {
  const query: string = topic.trim();
  if (!/^[a-zA-Z][a-zA-Z ,'-]{1,59}$/.test(query)) { throw new Error('请用英文填写领域，例如 software engineering 或 finance。'); }
  const items: RelatedWord[] = JSON.parse(await getJson(`https://api.datamuse.com/words?ml=${encodeURIComponent(query)}&md=dp&max=100`)) as RelatedWord[];
  if (!Array.isArray(items)) { throw new Error('在线词库返回格式异常。'); }
  const words: WordItem[] = [];
  for (const item of items) {
    if (typeof item.word !== 'string' || !/^[a-zA-Z][a-zA-Z '-]{0,79}$/.test(item.word) || !Array.isArray(item.defs) || item.defs.length === 0) { continue; }
    const definition: string = item.defs[0];
    if (typeof definition !== 'string') { continue; }
    const separator: number = definition.indexOf('\t');
    words.push({ word: item.word.toLowerCase(), phonetic: '', pos: separator >= 0 ? definition.slice(0, separator) : '',
      meaning: separator >= 0 ? definition.slice(separator + 1) : definition, example: '', translation: '', topic: query,
      source: 'Datamuse / WordNet / Wiktionary · https://www.datamuse.com/api/ · 英文原释义' });
  }
  if (words.length === 0) { throw new Error('未找到带释义的相关词，请尝试更常见的领域名称。'); }
  return words.filter(allowedWord);
}
