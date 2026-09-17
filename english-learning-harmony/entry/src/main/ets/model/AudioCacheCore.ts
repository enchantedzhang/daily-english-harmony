import { addDays } from './LearningCore';
import type { DayLesson } from './LearningCore';

export const AUDIO_FILE_LIMIT: number = 512 * 1024;
export const AUDIO_TOTAL_LIMIT: number = 64 * 1024 * 1024;

export interface AudioEntry {
  word: string;
  file: string;
  bytes: number;
  url: string;
  source: string;
  license: string;
}
export interface AudioIndex { entries: AudioEntry[]; }
export interface Pronunciation { url: string; source: string; license: string; }

// Today plus the previous seven and following twenty civil dates, inclusive.
export function audioWords(lessons: DayLesson[], today: string): string[] {
  const first: string = addDays(today, -7);
  const last: string = addDays(today, 20);
  const selected: DayLesson[] = lessons.filter((item: DayLesson) => item.date >= first && item.date <= last);
  selected.sort((a: DayLesson, b: DayLesson) => {
    const aGroup: number = a.date === today ? 0 : a.date > today ? 1 : 2;
    const bGroup: number = b.date === today ? 0 : b.date > today ? 1 : 2;
    return aGroup - bGroup || (aGroup === 2 ? b.date.localeCompare(a.date) : a.date.localeCompare(b.date));
  });
  const result: string[] = [];
  selected.forEach((lesson: DayLesson) => lesson.words.forEach((item) => {
    const word: string = item.word.trim().toLowerCase();
    if (!result.includes(word)) { result.push(word); }
  }));
  return result;
}

export function audioFile(word: string): string {
  let first: number = 2166136261;
  let second: number = 5381;
  for (let i: number = 0; i < word.length; i++) {
    first = Math.imul(first ^ word.charCodeAt(i), 16777619);
    second = Math.imul(second, 33) ^ word.charCodeAt(i);
  }
  return word.slice(0, 40).split('').map((letter: string) => letter.charCodeAt(0).toString(16).padStart(4, '0')).join('') +
    (first >>> 0).toString(16).padStart(8, '0') + (second >>> 0).toString(16).padStart(8, '0') + '.mp3';
}

export function allowedAudioURL(url: string): boolean {
  return /^https:\/\/(api\.dictionaryapi\.dev\/media\/pronunciations\/en\/|upload\.wikimedia\.org\/)[^\s?#]+\.mp3$/i.test(url);
}

export function validMP3(data: ArrayBuffer): boolean {
  const bytes: Uint8Array = new Uint8Array(data);
  return bytes.length >= 128 && bytes.length <= AUDIO_FILE_LIMIT &&
    ((bytes[0] === 73 && bytes[1] === 68 && bytes[2] === 51) || (bytes[0] === 255 && (bytes[1] & 224) === 224));
}
