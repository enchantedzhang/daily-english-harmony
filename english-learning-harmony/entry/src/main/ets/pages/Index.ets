import { SAMPLE_LESSON, DayLesson, WordItem } from '../model/LearningModels';
import { speak } from '../service/SpeechService';

@Entry
@Component
struct Index {
  @State selectedDate: string = SAMPLE_LESSON.date;
  @State lesson: DayLesson = SAMPLE_LESSON;
  @State count: number = 5;
  @State topic: string = '全部领域';
  @State showSettings: boolean = false;

  private days: string[] = ['1','2','3','4','5','6','7','8','9','10','11','12','13','14','15','16','17','18','19','20','21','22','23','24','25','26','27','28','29','30'];

  build() {
    Column() {
      Row() {
        Column() { Text('每日英语').fontSize(26).fontWeight(FontWeight.Bold); Text('5分钟，积累一点点').fontSize(13).fontColor('#7A8499').margin({ top: 4 }) }.alignItems(HorizontalAlign.Start)
        Blank(); Button('⚙').type(ButtonType.Circle).width(42).height(42).backgroundColor('#EEF2FF').fontColor('#5267D8').onClick(() => this.showSettings = true)
      }.width('100%').padding({ left: 22, right: 22, top: 18, bottom: 18 })
      Scroll() { Column() {
        Text('2026年 9月').fontSize(20).fontWeight(FontWeight.Bold).alignSelf(ItemAlign.Start).margin({ bottom: 14 })
        Grid() { ForEach(this.days, day => GridItem() {
          Column() { Text(day).fontSize(15).fontColor(day === '17' ? Color.White : '#26324A'); if (['6','7','8','9','10','11','12','13','14','15','16','17'].includes(day)) Text('•').fontSize(16).fontColor(day === '17' ? Color.White : '#5267D8') }
        }.width(42).height(46).justifyContent(FlexAlign.Center).backgroundColor(day === '17' ? '#5267D8' : Color.Transparent).borderRadius(14).onClick(() => this.selectedDate = `2026-09-${day.padStart(2, '0')}`)) }.columnsTemplate('1fr 1fr 1fr 1fr 1fr 1fr 1fr').rowsGap(6).columnsGap(4)
        Row() { Text('今日学习').fontSize(18).fontWeight(FontWeight.Bold); Blank(); Text(`${this.lesson.words.length} 个单词`).fontSize(13).fontColor('#7A8499') }.width('100%').margin({ top: 24, bottom: 12 })
        ForEach(this.lesson.words.slice(0, this.count), (item: WordItem) => Row() {
          Column() { Text(item.word).fontSize(19).fontWeight(FontWeight.Bold).fontColor('#26324A'); Text(`${item.phonetic}  ${item.meaning}`).fontSize(13).fontColor('#7A8499').margin({ top: 5 }) }.alignItems(HorizontalAlign.Start)
          Blank(); Button('▶').type(ButtonType.Circle).width(38).height(38).backgroundColor('#EEF2FF').fontColor('#5267D8').onClick(() => speak(item.word))
        }.width('100%').padding(16).margin({ bottom: 10 }).backgroundColor(Color.White).borderRadius(18))
        Text(`Phrase  ·  ${this.lesson.phrase}`).fontSize(15).fontColor('#5267D8').alignSelf(ItemAlign.Start).margin({ top: 8, bottom: 24 })
      }.padding({ left: 22, right: 22, bottom: 30 }) }.layoutWeight(1).scrollBar(BarState.Off)
    }.width('100%').height('100%').backgroundColor('#F7F8FC')
    if (this.showSettings) this.settingsDialog()
  }

  @Builder settingsDialog() { Column() { Text('学习设置').fontSize(22).fontWeight(FontWeight.Bold).margin({ bottom: 18 }); Text('每天学习几个单词？').fontSize(15).alignSelf(ItemAlign.Start); Row() { ForEach([3,5,10], n => Button(`${n} 个`).fontColor(this.count === n ? Color.White : '#5267D8').backgroundColor(this.count === n ? '#5267D8' : '#EEF2FF').margin({ right: 8 }).onClick(() => this.count = n)) }.margin({ top: 12, bottom: 20 }); Text('学习领域').fontSize(15).alignSelf(ItemAlign.Start); Row() { ForEach(['全部领域','日常交流','职场沟通','阅读表达'], t => Button(t).fontSize(12).fontColor(this.topic === t ? Color.White : '#5267D8').backgroundColor(this.topic === t ? '#5267D8' : '#EEF2FF').margin({ right: 6 }).onClick(() => this.topic = t)) }.margin({ top: 12, bottom: 20 }); Button('完成').width('100%').backgroundColor('#5267D8').onClick(() => this.showSettings = false) }.width('88%').padding(22).backgroundColor(Color.White).borderRadius(24).position({ x: '6%', y: '28%' }) }
}
