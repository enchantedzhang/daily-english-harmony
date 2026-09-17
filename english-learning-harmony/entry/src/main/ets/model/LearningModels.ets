export interface WordItem {
  word: string;
  phonetic: string;
  pos: string;
  meaning: string;
  example: string;
  translation: string;
  topic: string;
}

export interface DayLesson {
  date: string;
  words: WordItem[];
  phrase: string;
}

export const SAMPLE_LESSON: DayLesson = {
  date: '2026-09-17',
  phrase: 'make progress',
  words: [
    { word: 'deadline', phonetic: '/ˈdedlaɪn/', pos: 'n.', meaning: '截止日期；最后期限', example: 'We need to meet the deadline for this report.', translation: '我们需要按时完成这份报告。', topic: '职场沟通' },
    { word: 'insight', phonetic: '/ˈɪnsaɪt/', pos: 'n.', meaning: '洞察；深刻见解', example: 'Her insight helped us understand the customer.', translation: '她的洞察帮助我们理解了客户。', topic: '阅读表达' },
    { word: 'reliable', phonetic: '/rɪˈlaɪəbl/', pos: 'adj.', meaning: '可靠的；可信赖的', example: 'We need a reliable way to track our progress.', translation: '我们需要一种可靠的方式跟踪进度。', topic: '日常交流' },
    { word: 'allocate', phonetic: '/ˈæləkeɪt/', pos: 'v.', meaning: '分配；拨出', example: 'The team allocated more time to testing.', translation: '团队为测试分配了更多时间。', topic: '职场沟通' },
    { word: 'worthwhile', phonetic: '/ˌwɜːθˈwaɪl/', pos: 'adj.', meaning: '值得的；有价值的', example: 'A few minutes of review is worthwhile.', translation: '花几分钟复习是值得的。', topic: '日常交流' }
  ]
};
