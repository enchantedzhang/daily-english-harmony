import { WordItem, StudyExample } from '../model/LearningCore';

@Component
export struct WordCard {
  @Prop item: WordItem = { word: '', phonetic: '', pos: '', meaning: '', example: '', translation: '', topic: '', source: '' };
  @Prop learned: boolean = false;
  @Prop review: boolean = false;
  @Prop canLearn: boolean = true;
  @Prop canLookup: boolean = false;
  @Prop busy: boolean = false;
  @State expanded: boolean = false;
  onSpeak: (text: string) => void = (_text: string): void => {};
  onLearn: (word: string) => void = (_word: string): void => {};
  onLookup: (word: string) => void = (_word: string): void => {};

  build() {
    Column({ space: 10 }) {
      Row({ space: 10 }) {
        Column({ space: 5 }) {
          Text(this.item.word).fontSize(22).fontWeight(FontWeight.Bold).fontColor('#27324B')
          if (this.item.study?.phonetic || this.item.phonetic) { Text(this.item.study?.phonetic || this.item.phonetic).fontSize(13).fontColor('#758099') }
        }.alignItems(HorizontalAlign.Start).layoutWeight(1)
        if (this.review) { Text('复习').fontSize(11).fontColor('#916C36').padding(5).backgroundColor('#FFF4DF').borderRadius(6) }
        Button('朗读').fontSize(13).height(36).fontColor('#5267D8').backgroundColor('#EEF1FF')
          .onClick(() => this.onSpeak(this.item.word)).accessibilityText(`朗读 ${this.item.word}`)
      }.width('100%')
      Text(this.item.topic).fontSize(11).fontColor('#8793B0').width('100%')
      Text(this.item.study?.meaning || `${this.item.pos}  ${this.item.meaning}`).fontSize(15).lineHeight(23).fontColor('#49536C').width('100%')
      if (this.item.study?.englishMeaning) {
        Text(this.item.study.englishMeaning).fontSize(13).lineHeight(21).fontColor('#758099').width('100%')
      }
      if (this.item.study && this.item.study.examples.length > 0) {
        ForEach(this.expanded ? this.item.study.examples : this.item.study.examples.slice(0, 1), (example: StudyExample) => {
          Column({ space: 6 }) {
            Text(example.english).fontSize(14).lineHeight(22).fontColor('#576580').width('100%')
            Text(example.chinese).fontSize(13).lineHeight(21).fontColor('#8690A4').width('100%')
            Button('朗读例句').fontSize(12).fontColor('#5267D8').backgroundColor('#F4F5FC').height(32)
              .onClick(() => this.onSpeak(example.english)).alignSelf(ItemAlign.Start)
          }.alignItems(HorizontalAlign.Start).width('100%')
        }, (example: StudyExample) => example.english)
      } else if (this.item.example) {
        Text(this.item.example).fontSize(14).lineHeight(22).fontColor('#576580').width('100%')
        if (this.expanded) {
          if (this.item.translation) { Text(this.item.translation).fontSize(13).fontColor('#8690A4').width('100%') }
          Button('朗读例句').fontSize(12).fontColor('#5267D8').backgroundColor('#F4F5FC').height(32)
            .onClick(() => this.onSpeak(this.item.example)).alignSelf(ItemAlign.Start)
        }
      }
      if (this.item.study) {
        Column({ space: 7 }) {
          Text(this.item.study.kind).fontSize(12).fontColor('#5267D8')
          Text(this.item.study.formation).fontSize(16).fontWeight(FontWeight.Medium).fontColor('#34466D')
          Text(this.item.study.explanation).fontSize(13).lineHeight(22).fontColor('#576580')
          Text(`💡 记忆联想：${this.item.study.memory}`).fontSize(13).lineHeight(22).fontColor('#77633D')
        }.alignItems(HorizontalAlign.Start).width('100%').padding(14).backgroundColor('#F0F3FF').borderRadius(12)
      } else if (this.expanded) {
        Text('该词暂未收录经核对的构词讲解，可先结合释义和例句记忆。').fontSize(12).lineHeight(20).fontColor('#9299AA')
      }
      Row() {
        Button(this.expanded ? '收起详情' : '展开详情').fontSize(12).fontColor('#8992A5').backgroundColor(Color.Transparent)
          .padding(0).height(32).onClick(() => this.expanded = !this.expanded)
        Blank()
        if (this.canLearn) {
          Button(this.learned ? '✓ 已学习' : '标记已学').fontSize(12).height(32)
            .fontColor(this.learned ? '#238375' : '#5267D8').backgroundColor(this.learned ? '#E7F5EF' : '#EEF1FF')
            .enabled(!this.busy).onClick(() => this.onLearn(this.item.word))
        }
      }.width('100%')
      if (this.expanded) {
        if (this.item.study) {
          Text('构词解释与记忆联想分开标注；联想不是词源。').fontSize(11).lineHeight(17).fontColor('#9299AA')
          ForEach(this.item.study.sources, (source: string) => {
            Text(`参考：${source}`).fontSize(10).lineHeight(16).fontColor('#9299AA').copyOption(CopyOptions.LocalDevice)
          }, (source: string) => source)
        }
        Text(`${this.item.topic} · ${this.item.source}`).fontSize(11).lineHeight(17).fontColor('#9299AA').width('100%')
        Button('在线查词').fontSize(12).fontColor('#5267D8').backgroundColor('#EEF1FF').height(32)
          .enabled(this.canLookup && !this.busy).onClick(() => this.onLookup(this.item.word)).alignSelf(ItemAlign.Start)
      }
    }.alignItems(HorizontalAlign.Start).padding(18).width('100%').backgroundColor(Color.White).borderRadius(20)
  }
}
