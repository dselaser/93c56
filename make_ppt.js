const pptxgen = require("pptxgenjs");
const pres = new pptxgen();

pres.layout = "LAYOUT_16x9";
pres.author = "DSE Laser";
pres.title = "93C56 Memory Board Tester";

const BLK = "000000";
const D_GRAY = "333333";
const M_GRAY = "666666";
const L_GRAY = "AAAAAA";
const VL_GRAY = "DDDDDD";
const WHITE = "FFFFFF";
const TITLE_FONT = "Arial Black";
const BODY_FONT = "Arial";

function addHR(s, y) {
  s.addShape(pres.shapes.LINE, { x: 0.5, y: y, w: 9, h: 0, line: { color: BLK, width: 1.5 } });
}
function addBar(s, text, y) {
  s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y: y, w: 9, h: 0.45, fill: { color: BLK } });
  s.addText(text, { x: 0.7, y: y, w: 8.6, h: 0.45, fontSize: 14, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });
}

// ══════ 1. 표지 ══════
{
  const s = pres.addSlide();
  s.addShape(pres.shapes.LINE, { x: 0.5, y: 1.2, w: 9, h: 0, line: { color: BLK, width: 3 } });
  s.addText("93C56 메모리 보드 검사기", { x: 0.5, y: 1.5, w: 9, h: 1.2, fontSize: 38, fontFace: TITLE_FONT, color: BLK, bold: true, align: "center", margin: 0 });
  s.addText("STM32H562RGT6  +  FreeRTOS", { x: 0.5, y: 2.6, w: 9, h: 0.5, fontSize: 20, fontFace: BODY_FONT, color: M_GRAY, align: "center", margin: 0 });
  s.addShape(pres.shapes.LINE, { x: 0.5, y: 3.3, w: 9, h: 0, line: { color: BLK, width: 3 } });
  s.addText("초보자를 위한 프로그램 설명서", { x: 0.5, y: 3.8, w: 9, h: 0.5, fontSize: 16, fontFace: BODY_FONT, color: M_GRAY, italic: true, align: "center", margin: 0 });
  s.addText("DSE Laser  |  2026.04", { x: 0.5, y: 4.8, w: 9, h: 0.4, fontSize: 12, fontFace: BODY_FONT, color: L_GRAY, align: "center", margin: 0 });
}

// ══════ 2. 시스템 구성도 ══════
{
  const s = pres.addSlide();
  s.addText("시스템 구성도", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  // 메인보드
  s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y: 1.2, w: 4.2, h: 3.8, fill: { color: WHITE }, line: { color: BLK, width: 2 } });
  s.addText("메인 보드", { x: 0.5, y: 1.2, w: 4.2, h: 0.4, fontSize: 14, fontFace: BODY_FONT, color: BLK, bold: true, align: "center", margin: 0 });

  s.addShape(pres.shapes.RECTANGLE, { x: 1.0, y: 1.8, w: 1.8, h: 0.7, fill: { color: VL_GRAY }, line: { color: BLK, width: 1 } });
  s.addText("STM32H562\nRGT6", { x: 1.0, y: 1.8, w: 1.8, h: 0.7, fontSize: 10, fontFace: BODY_FONT, color: BLK, bold: true, align: "center", valign: "middle", margin: 0 });

  s.addShape(pres.shapes.RECTANGLE, { x: 3.2, y: 1.8, w: 1.2, h: 0.7, fill: { color: VL_GRAY }, line: { color: BLK, width: 1 } });
  s.addText("SK9822\nLED x12", { x: 3.2, y: 1.8, w: 1.2, h: 0.7, fontSize: 9, fontFace: BODY_FONT, color: BLK, align: "center", valign: "middle", margin: 0 });

  s.addShape(pres.shapes.RECTANGLE, { x: 1.0, y: 2.8, w: 1.8, h: 0.6, fill: { color: VL_GRAY }, line: { color: BLK, width: 1 } });
  s.addText("MAX98357A\n음성 앰프", { x: 1.0, y: 2.8, w: 1.8, h: 0.6, fontSize: 9, fontFace: BODY_FONT, color: BLK, align: "center", valign: "middle", margin: 0 });

  s.addShape(pres.shapes.RECTANGLE, { x: 3.2, y: 2.8, w: 1.2, h: 0.6, fill: { color: VL_GRAY }, line: { color: BLK, width: 1 } });
  s.addText("CD4067\n16ch MUX", { x: 3.2, y: 2.8, w: 1.2, h: 0.6, fontSize: 9, fontFace: BODY_FONT, color: BLK, align: "center", valign: "middle", margin: 0 });

  s.addShape(pres.shapes.RECTANGLE, { x: 1.0, y: 3.7, w: 3.4, h: 0.5, fill: { color: VL_GRAY }, line: { color: BLK, width: 1, dashType: "dash" } });
  s.addText("포그핀 커넥터 x12  (6핀: VCC, GND, CS, CLK, DI, DO)", { x: 1.0, y: 3.7, w: 3.4, h: 0.5, fontSize: 7, fontFace: BODY_FONT, color: D_GRAY, align: "center", valign: "middle", margin: 0 });

  // 화살표
  s.addShape(pres.shapes.LINE, { x: 4.7, y: 3.2, w: 0.6, h: 0, line: { color: BLK, width: 2 } });
  s.addText("포그핀", { x: 4.55, y: 2.85, w: 0.9, h: 0.3, fontSize: 8, fontFace: BODY_FONT, color: M_GRAY, align: "center", margin: 0 });

  // 메모리보드
  s.addShape(pres.shapes.RECTANGLE, { x: 5.3, y: 1.2, w: 4.2, h: 3.8, fill: { color: WHITE }, line: { color: BLK, width: 2 } });
  s.addText("메모리 보드", { x: 5.3, y: 1.2, w: 4.2, h: 0.4, fontSize: 14, fontFace: BODY_FONT, color: BLK, bold: true, align: "center", margin: 0 });

  for (let row = 0; row < 3; row++) {
    for (let col = 0; col < 4; col++) {
      const num = row * 4 + col + 1;
      const x = 5.7 + col * 0.9;
      const y = 1.9 + row * 0.8;
      s.addShape(pres.shapes.RECTANGLE, { x, y, w: 0.7, h: 0.55, fill: { color: WHITE }, line: { color: BLK, width: 1 } });
      s.addText(`93C56\nU${num}`, { x, y, w: 0.7, h: 0.55, fontSize: 8, fontFace: BODY_FONT, color: BLK, align: "center", valign: "middle", margin: 0 });
    }
  }
  s.addText("4열 x 3행 = 12개  (93C46: 128B 또는 93C56: 256B, 자동 식별)", { x: 5.3, y: 4.3, w: 4.2, h: 0.3, fontSize: 9, fontFace: BODY_FONT, color: M_GRAY, align: "center", margin: 0 });
}

// ══════ 3. 파일 구조 ══════
{
  const s = pres.addSlide();
  s.addText("소스 파일 구조", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  const files = [
    ["ee93c56.h / .c", "93C56 Microwire 비트뱅 드라이버\nGPIO CLK/DIN/DOUT + CD4067 MUX 제어"],
    ["sk9822.h / .c", "SK9822 (APA102 호환) LED 드라이버\nSPI1 프로토콜, 12개 LED 제어"],
    ["voice_data.h / .c", "음성 PCM 데이터 (8kHz 16bit 모노)\ngen_voice.py 로 TTS 자동 생성"],
    ["app_freertos.c", "메인 상태 머신 + 보드 감지 태스크\nLED 쇼, 검사, 프로그래밍, 음성 재생"],
    ["main.c", "주변장치 초기화 (SPI1, I2S3, UART1)\nGPIO 초기화, FreeRTOS 커널 시작"],
  ];
  for (let i = 0; i < files.length; i++) {
    const y = 1.1 + i * 0.85;
    s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y, w: 2.8, h: 0.65, fill: { color: BLK } });
    s.addText(files[i][0], { x: 0.6, y, w: 2.6, h: 0.65, fontSize: 12, fontFace: "Consolas", color: WHITE, bold: true, valign: "middle", margin: 0 });
    s.addText(files[i][1], { x: 3.5, y, w: 6, h: 0.65, fontSize: 11, fontFace: BODY_FONT, color: D_GRAY, valign: "middle", margin: 0 });
  }
}

// ══════ 4. FreeRTOS 태스크 ══════
{
  const s = pres.addSlide();
  s.addText("FreeRTOS 태스크 구조", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  const tasks = [
    { name: "defaultTask", prio: "Normal", stack: "128x4", desc: "빈 루프\n(CubeMX 자동 생성)" },
    { name: "mainAppTask", prio: "Normal", stack: "256x4", desc: "LED 쇼 + 메모리 검사\n+ 프로그래밍 + 음성 재생" },
    { name: "detectTask", prio: "BelowNormal", stack: "256x4", desc: "백그라운드 보드 감지\n(읽기 전용, 안전)" },
  ];

  s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y: 1.1, w: 9, h: 0.45, fill: { color: BLK } });
  s.addText("태스크 이름", { x: 0.6, y: 1.1, w: 2.2, h: 0.45, fontSize: 12, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });
  s.addText("우선순위", { x: 2.8, y: 1.1, w: 1.5, h: 0.45, fontSize: 12, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });
  s.addText("스택", { x: 4.3, y: 1.1, w: 1.2, h: 0.45, fontSize: 12, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });
  s.addText("역할", { x: 5.5, y: 1.1, w: 4, h: 0.45, fontSize: 12, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });

  for (let i = 0; i < tasks.length; i++) {
    const y = 1.6 + i * 0.7;
    const bg = i % 2 === 0 ? VL_GRAY : WHITE;
    s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y, w: 9, h: 0.65, fill: { color: bg }, line: { color: L_GRAY, width: 0.5 } });
    s.addText(tasks[i].name, { x: 0.6, y, w: 2.2, h: 0.65, fontSize: 11, fontFace: "Consolas", color: BLK, bold: true, valign: "middle", margin: 0 });
    s.addText(tasks[i].prio, { x: 2.8, y, w: 1.5, h: 0.65, fontSize: 11, fontFace: BODY_FONT, color: D_GRAY, valign: "middle", margin: 0 });
    s.addText(tasks[i].stack, { x: 4.3, y, w: 1.2, h: 0.65, fontSize: 11, fontFace: BODY_FONT, color: D_GRAY, valign: "middle", margin: 0 });
    s.addText(tasks[i].desc, { x: 5.5, y, w: 4, h: 0.65, fontSize: 10, fontFace: BODY_FONT, color: D_GRAY, valign: "middle", margin: 0 });
  }

  addBar(s, "태스크 간 통신 (volatile 전역 변수)", 3.8);
  s.addText([
    { text: "g_board_present ", options: { fontFace: "Consolas", fontSize: 11, color: BLK, bold: true } },
    { text: "  보드 감지 플래그 (true/false)", options: { fontSize: 11, color: D_GRAY, breakLine: true } },
    { text: "g_detected[12]  ", options: { fontFace: "Consolas", fontSize: 11, color: BLK, bold: true } },
    { text: "  칩별 감지 상태 배열", options: { fontSize: 11, color: D_GRAY, breakLine: true } },
    { text: "g_detect_enable ", options: { fontFace: "Consolas", fontSize: 11, color: BLK, bold: true } },
    { text: "  감지 태스크 활성화/비활성화", options: { fontSize: 11, color: D_GRAY } },
  ], { x: 0.7, y: 4.35, w: 8.8, h: 1.1, fontFace: BODY_FONT, margin: 0 });
}

// ══════ 5. 상태 머신 흐름 ══════
{
  const s = pres.addSlide();
  s.addText("상태 머신 흐름도", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  const states = [
    { label: "A", title: "전원 ON + LED 쇼", desc: "인트로: \"디에스이 아이엔시의 디아지 팁용 프로그래머입니다\"\n\"팁 보드를 올려주세요\" + LED 8패턴(A~H)", y: 1.1 },
    { label: "B", title: "보드 감지 + 칩 식별", desc: "detectTask 백그라운드 감지 -> 안정화 확인\n칩 식별: \"93C56 보드입니다\" / \"93C46 보드입니다\"", y: 1.95 },
    { label: "C", title: "메모리 검사", desc: "0xAA, 0x55 쓰기/읽기 검증 (전 비트 전환 확인)\nLED: 백색=정상, 적색=불량", y: 2.8 },
    { label: "D", title: "프로그래밍", desc: "0x00 쓰기 + 검증 (93C46:128B / 93C56:256B)\nLED: 청색=완료, 적색=불량", y: 3.65 },
    { label: "E", title: "완료 / 보드 제거", desc: "\"프로그램 완료\" -> \"보드를 분리하세요\" (30초 반복)\n분리 감지 -> \"보드가 분리되었습니다\" -> 상태 A 복귀", y: 4.5 },
  ];

  for (let i = 0; i < states.length; i++) {
    const st = states[i];
    s.addShape(pres.shapes.OVAL, { x: 0.6, y: st.y, w: 0.55, h: 0.55, fill: { color: BLK } });
    s.addText(st.label, { x: 0.6, y: st.y, w: 0.55, h: 0.55, fontSize: 18, fontFace: TITLE_FONT, color: WHITE, bold: true, align: "center", valign: "middle", margin: 0 });
    s.addText(st.title, { x: 1.35, y: st.y, w: 2.5, h: 0.55, fontSize: 13, fontFace: BODY_FONT, color: BLK, bold: true, valign: "middle", margin: 0 });
    s.addText(st.desc, { x: 3.9, y: st.y, w: 5.6, h: 0.55, fontSize: 10, fontFace: BODY_FONT, color: D_GRAY, valign: "middle", margin: 0 });
    if (i < states.length - 1) {
      s.addShape(pres.shapes.LINE, { x: 0.87, y: st.y + 0.55, w: 0, h: 0.35, line: { color: L_GRAY, width: 1.5 } });
    }
  }
  s.addText("E -> A  (무한 반복)", { x: 0.3, y: 5.25, w: 1.2, h: 0.3, fontSize: 8, fontFace: BODY_FONT, color: L_GRAY, align: "center", margin: 0 });
}

// ══════ 6. LED 쇼 8패턴 ══════
{
  const s = pres.addSlide();
  s.addText("LED 쇼 - 8가지 패턴", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  const pats = [
    ["A", "번호순 왕복", "U19->U30->U19 순차 ON-OFF"],
    ["B", "세로 지그재그", "열 단위 상하 뱀 형태 이동"],
    ["C", "대각선 쓸기", "대각선 그룹이 좌상->우하 이동"],
    ["D", "소용돌이", "외곽에서 중심으로 나선 이동"],
    ["E", "확산/수축", "중심에서 퍼졌다 다시 모임"],
    ["F", "랜덤 반짝", "무작위 위치에 별처럼 반짝"],
    ["G", "행/열 스캔", "1행 전체 ON->OFF, 다음 열 스캔"],
    ["H", "체커보드", "체스판처럼 흑백 교대 깜빡"],
  ];
  for (let i = 0; i < 4; i++) {
    for (let j = 0; j < 2; j++) {
      const idx = i * 2 + j;
      const x = 0.5 + j * 4.7;
      const y = 1.1 + i * 1.1;
      s.addShape(pres.shapes.RECTANGLE, { x, y, w: 0.45, h: 0.45, fill: { color: BLK } });
      s.addText(pats[idx][0], { x, y, w: 0.45, h: 0.45, fontSize: 16, fontFace: TITLE_FONT, color: WHITE, bold: true, align: "center", valign: "middle", margin: 0 });
      s.addText(pats[idx][1], { x: x + 0.55, y, w: 3.5, h: 0.22, fontSize: 12, fontFace: BODY_FONT, color: BLK, bold: true, valign: "middle", margin: 0 });
      s.addText(pats[idx][2], { x: x + 0.55, y: y + 0.22, w: 3.5, h: 0.22, fontSize: 10, fontFace: BODY_FONT, color: M_GRAY, valign: "middle", margin: 0 });
    }
  }
  s.addText("각 패턴마다 다른 색상 사용 (등휘도 보정 적용).  A~H 전체 완료 후 음성 출력.", { x: 0.5, y: 5.0, w: 9, h: 0.4, fontSize: 10, fontFace: BODY_FONT, color: L_GRAY, italic: true, align: "center", margin: 0 });
}

// ══════ 7. 93C56 Microwire 프로토콜 ══════
{
  const s = pres.addSlide();
  s.addText("93C56 Microwire 프로토콜", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  addBar(s, "93C46 vs 93C56 비교", 1.0);
  // 비교 테이블
  s.addShape(pres.shapes.RECTANGLE, { x: 0.7, y: 1.55, w: 8.6, h: 0.35, fill: { color: BLK } });
  s.addText("항목", { x: 0.8, y: 1.55, w: 2.5, h: 0.35, fontSize: 10, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });
  s.addText("93C46 (x8)", { x: 3.5, y: 1.55, w: 2.8, h: 0.35, fontSize: 10, fontFace: BODY_FONT, color: WHITE, bold: true, align: "center", valign: "middle", margin: 0 });
  s.addText("93C56 (x8)", { x: 6.5, y: 1.55, w: 2.8, h: 0.35, fontSize: 10, fontFace: BODY_FONT, color: WHITE, bold: true, align: "center", valign: "middle", margin: 0 });

  const cmpRows = [
    ["용량", "1Kbit (128 x 8)", "2Kbit (256 x 8)"],
    ["주소 비트", "7비트", "8비트"],
    ["명령 길이", "SB+OP+7 = 10비트", "SB+OP+8 = 11비트"],
    ["식별 방법", "주소 200 쓰기 실패", "주소 200 쓰기 성공"],
  ];
  for (let i = 0; i < cmpRows.length; i++) {
    const y = 1.92 + i * 0.3;
    const bg = i % 2 === 0 ? VL_GRAY : WHITE;
    s.addShape(pres.shapes.RECTANGLE, { x: 0.7, y, w: 8.6, h: 0.28, fill: { color: bg } });
    s.addText(cmpRows[i][0], { x: 0.8, y, w: 2.5, h: 0.28, fontSize: 10, fontFace: BODY_FONT, color: BLK, bold: true, valign: "middle", margin: 0 });
    s.addText(cmpRows[i][1], { x: 3.5, y, w: 2.8, h: 0.28, fontSize: 10, fontFace: "Consolas", color: D_GRAY, align: "center", valign: "middle", margin: 0 });
    s.addText(cmpRows[i][2], { x: 6.5, y, w: 2.8, h: 0.28, fontSize: 10, fontFace: "Consolas", color: D_GRAY, align: "center", valign: "middle", margin: 0 });
  }
  s.addText("프로그램이 자동으로 식별하여 주소 비트와 메모리 크기를 전환합니다.", { x: 0.7, y: 3.15, w: 8.6, h: 0.25, fontSize: 9, fontFace: BODY_FONT, color: M_GRAY, italic: true, margin: 0 });

  addBar(s, "명령어 형식 (x8 모드)", 3.5);
  const fields = [
    { label: "SB", w: 0.5, desc: "1비트" },
    { label: "OP", w: 0.7, desc: "2비트" },
    { label: "주소", w: 2.2, desc: "7 or 8비트" },
    { label: "데이터", w: 2.5, desc: "8비트 (읽기/쓰기)" },
  ];
  let fx = 1.0;
  for (const f of fields) {
    s.addShape(pres.shapes.RECTANGLE, { x: fx, y: 4.05, w: f.w, h: 0.5, fill: { color: VL_GRAY }, line: { color: BLK, width: 1 } });
    s.addText(f.label, { x: fx, y: 4.05, w: f.w, h: 0.3, fontSize: 10, fontFace: "Consolas", color: BLK, bold: true, align: "center", valign: "middle", margin: 0 });
    s.addText(f.desc, { x: fx, y: 4.35, w: f.w, h: 0.2, fontSize: 8, fontFace: BODY_FONT, color: M_GRAY, align: "center", valign: "middle", margin: 0 });
    fx += f.w + 0.05;
  }

  addBar(s, "주요 명령어", 4.75);
  const cmdsL = [
    ["READ  (10)", "주소에서 8비트 읽기"],
    ["WRITE (01)", "주소에 8비트 쓰기"],
  ];
  const cmdsR = [
    ["EWEN (00+11xx)", "쓰기 허용"],
    ["EWDS (00+00xx)", "쓰기 금지"],
  ];
  for (let i = 0; i < 2; i++) {
    const y = 5.22 + i * 0.22;
    s.addText(cmdsL[i][0], { x: 0.7, y, w: 2.2, h: 0.22, fontSize: 9, fontFace: "Consolas", color: BLK, bold: true, valign: "middle", margin: 0 });
    s.addText(cmdsL[i][1], { x: 2.9, y, w: 2, h: 0.22, fontSize: 9, fontFace: BODY_FONT, color: D_GRAY, valign: "middle", margin: 0 });
    s.addText(cmdsR[i][0], { x: 5.3, y, w: 2.2, h: 0.22, fontSize: 9, fontFace: "Consolas", color: BLK, bold: true, valign: "middle", margin: 0 });
    s.addText(cmdsR[i][1], { x: 7.5, y, w: 2, h: 0.22, fontSize: 9, fontFace: BODY_FONT, color: D_GRAY, valign: "middle", margin: 0 });
  }
}

// ══════ 8. 메모리 검사 방법 ══════
{
  const s = pres.addSlide();
  s.addText("메모리 셀 검사 방법", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  addBar(s, "0xAA / 0x55 패턴 검사란?", 1.0);
  s.addText([
    { text: "0xAA = 10101010  /  0x55 = 01010101", options: { fontFace: "Consolas", fontSize: 13, bold: true, breakLine: true } },
    { text: "모든 비트가 0 -> 1 -> 0 으로 전환되는지 확인", options: { fontSize: 12, breakLine: true } },
    { text: "8비트 x 256주소 x 2패스 = 칩당 총 1,024회 쓰기/읽기", options: { fontSize: 12 } },
  ], { x: 0.7, y: 1.55, w: 8.8, h: 0.9, fontFace: BODY_FONT, color: D_GRAY, margin: 0 });

  addBar(s, "검출 가능한 결함", 2.7);
  // 비트 다이어그램
  s.addText("Pass 1:  AA 쓰기", { x: 0.7, y: 3.25, w: 2, h: 0.3, fontSize: 11, fontFace: BODY_FONT, color: BLK, bold: true, margin: 0 });
  const bits_aa = "10101010";
  for (let b = 0; b < 8; b++) {
    const x = 2.8 + b * 0.5;
    const fill = bits_aa[b] === "1" ? BLK : WHITE;
    const tc   = bits_aa[b] === "1" ? WHITE : BLK;
    s.addShape(pres.shapes.RECTANGLE, { x, y: 3.25, w: 0.4, h: 0.3, fill: { color: fill }, line: { color: BLK, width: 1 } });
    s.addText(bits_aa[b], { x, y: 3.25, w: 0.4, h: 0.3, fontSize: 12, fontFace: "Consolas", color: tc, align: "center", valign: "middle", margin: 0 });
  }

  s.addText("Pass 2:  55 쓰기", { x: 0.7, y: 3.65, w: 2, h: 0.3, fontSize: 11, fontFace: BODY_FONT, color: BLK, bold: true, margin: 0 });
  const bits_55 = "01010101";
  for (let b = 0; b < 8; b++) {
    const x = 2.8 + b * 0.5;
    const fill = bits_55[b] === "1" ? BLK : WHITE;
    const tc   = bits_55[b] === "1" ? WHITE : BLK;
    s.addShape(pres.shapes.RECTANGLE, { x, y: 3.65, w: 0.4, h: 0.3, fill: { color: fill }, line: { color: BLK, width: 1 } });
    s.addText(bits_55[b], { x, y: 3.65, w: 0.4, h: 0.3, fontSize: 12, fontFace: "Consolas", color: tc, align: "center", valign: "middle", margin: 0 });
  }

  s.addText("-> 모든 비트가 0과 1 모두 저장/읽기 가능한지 확인", { x: 6.9, y: 3.35, w: 2.8, h: 0.5, fontSize: 10, fontFace: BODY_FONT, color: M_GRAY, valign: "middle", margin: 0 });

  const defects = [
    ["Stuck-at-0", "항상 0으로 고정된 비트 검출"],
    ["Stuck-at-1", "항상 1로 고정된 비트 검출"],
    ["인접 비트 간섭", "이웃 비트와 합선(bridging) 검출"],
  ];
  for (let i = 0; i < defects.length; i++) {
    const y = 4.2 + i * 0.35;
    s.addText(defects[i][0], { x: 0.7, y, w: 2.5, h: 0.35, fontSize: 11, fontFace: BODY_FONT, color: BLK, bold: true, valign: "middle", margin: 0 });
    s.addText(defects[i][1], { x: 3.2, y, w: 6, h: 0.35, fontSize: 11, fontFace: BODY_FONT, color: D_GRAY, valign: "middle", margin: 0 });
  }
}

// ══════ 9. 안전 설계 ══════
{
  const s = pres.addSlide();
  s.addText("안전 설계", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  addBar(s, "문제: 포그핀 접촉 불안정", 1.0);
  s.addText("사람이 손으로 메모리 보드를 올릴 때 일부 핀만 먼저 접촉될 수 있음.\n불완전 접촉 상태에서 EEPROM에 쓰기 하면 데이터 손상 위험.", { x: 0.7, y: 1.55, w: 8.8, h: 0.55, fontSize: 11, fontFace: BODY_FONT, color: D_GRAY, margin: 0 });

  addBar(s, "해결: 2단계 감지 방식", 2.3);

  s.addShape(pres.shapes.RECTANGLE, { x: 0.7, y: 2.85, w: 4, h: 1.2, fill: { color: WHITE }, line: { color: BLK, width: 1.5 } });
  s.addText("1단계: detectTask (백그라운드)", { x: 0.7, y: 2.85, w: 4, h: 0.35, fontSize: 11, fontFace: BODY_FONT, color: BLK, bold: true, align: "center", margin: 0 });
  s.addText([
    { text: "EE_DetectReadOnly()", options: { fontFace: "Consolas", fontSize: 10, bold: true, breakLine: true } },
    { text: "읽기만 수행 - 쓰기 없음 (안전)", options: { fontSize: 10, breakLine: true } },
    { text: "3회 연속 동일 결과 확인 (디바운싱)", options: { fontSize: 10 } },
  ], { x: 0.8, y: 3.25, w: 3.8, h: 0.75, fontFace: BODY_FONT, color: D_GRAY, margin: 0 });

  s.addShape(pres.shapes.LINE, { x: 4.7, y: 3.45, w: 0.6, h: 0, line: { color: BLK, width: 2 } });
  s.addText("안정", { x: 4.6, y: 3.1, w: 0.8, h: 0.3, fontSize: 8, fontFace: BODY_FONT, color: M_GRAY, align: "center", margin: 0 });

  s.addShape(pres.shapes.RECTANGLE, { x: 5.3, y: 2.85, w: 4, h: 1.2, fill: { color: WHITE }, line: { color: BLK, width: 1.5 } });
  s.addText("2단계: mainAppTask (확정)", { x: 5.3, y: 2.85, w: 4, h: 0.35, fontSize: 11, fontFace: BODY_FONT, color: BLK, bold: true, align: "center", margin: 0 });
  s.addText([
    { text: "EE_Detect() + 쓰기/읽기 검증", options: { fontFace: "Consolas", fontSize: 10, bold: true, breakLine: true } },
    { text: "안정 접촉 확인 후에만 쓰기 수행", options: { fontSize: 10, breakLine: true } },
    { text: "이후 메모리 검사/프로그래밍 진행", options: { fontSize: 10 } },
  ], { x: 5.4, y: 3.25, w: 3.8, h: 0.75, fontFace: BODY_FONT, color: D_GRAY, margin: 0 });

  addBar(s, "FreeRTOS 호환성", 4.4);
  s.addText("ee_wait_ready() 에서 HAL_GetTick() 대신 루프 카운터 사용\n(FreeRTOS 환경에서 HAL_GetTick이 증가하지 않는 문제 해결)", { x: 0.7, y: 4.95, w: 8.8, h: 0.5, fontSize: 10, fontFace: BODY_FONT, color: D_GRAY, margin: 0 });
}

// ══════ 10. 핀 매핑 ══════
{
  const s = pres.addSlide();
  s.addText("GPIO 핀 매핑", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  const pinData = [
    ["SPI1 (SK9822 LED)", "PA5 (SCK), PA7 (MOSI)"],
    ["I2S3 (음성 앰프)", "PA4 (WS), PB2 (SDO), PC10 (CK)"],
    ["93C56 CLK / DIN / DOUT", "PA8 / PC0 / PC1"],
    ["CD4067 MUX A,B,C,D", "PC6, PC7, PC8, PC9"],
    ["CD4067 INHIBIT", "PB12"],
    ["CS0 ~ CS3", "PA9, PA10, PA11, PA12"],
    ["CS4 ~ CS8", "PB4, PB5, PB6, PB7, PB9"],
    ["CS9 ~ CS11", "PC11, PC12, PD2"],
    ["MAX98357A SD_MODE", "PB8 (R47 풀업, GPIO 미사용)"],
  ];

  s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y: 1.0, w: 9, h: 0.4, fill: { color: BLK } });
  s.addText("기능", { x: 0.6, y: 1.0, w: 4, h: 0.4, fontSize: 11, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });
  s.addText("GPIO 핀", { x: 4.8, y: 1.0, w: 4.6, h: 0.4, fontSize: 11, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });

  for (let i = 0; i < pinData.length; i++) {
    const y = 1.45 + i * 0.4;
    const bg = i % 2 === 0 ? VL_GRAY : WHITE;
    s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y, w: 9, h: 0.38, fill: { color: bg } });
    s.addText(pinData[i][0], { x: 0.6, y, w: 4, h: 0.38, fontSize: 10, fontFace: BODY_FONT, color: BLK, bold: true, valign: "middle", margin: 0 });
    s.addText(pinData[i][1], { x: 4.8, y, w: 4.6, h: 0.38, fontSize: 10, fontFace: "Consolas", color: D_GRAY, valign: "middle", margin: 0 });
  }
}

// ══════ 11. 등휘도 보정 ══════
{
  const s = pres.addSlide();
  s.addText("LED 등휘도 보정", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  s.addText("인간 눈 감응도 (ITU-R BT.709):", { x: 0.5, y: 1.0, w: 9, h: 0.35, fontSize: 13, fontFace: BODY_FONT, color: BLK, bold: true, margin: 0 });
  s.addText("Y = 0.2126 x R  +  0.7152 x G  +  0.0722 x B", { x: 0.5, y: 1.35, w: 9, h: 0.4, fontSize: 14, fontFace: "Consolas", color: D_GRAY, align: "center", margin: 0 });
  s.addText("같은 RGB 값이라도 녹색이 적색보다 3.4배, 청색보다 10배 밝게 보임", { x: 0.5, y: 1.75, w: 9, h: 0.35, fontSize: 10, fontFace: BODY_FONT, color: M_GRAY, italic: true, align: "center", margin: 0 });

  addBar(s, "보정된 색상값 (체감 밝기 Y ~ 108 통일)", 2.3);
  const colors = [
    ["적색", "255, 0, 0", "br=4", "108"],
    ["청색", "0, 0, 255", "br=12", "110"],
    ["녹색", "0, 76, 0", "br=4", "109"],
    ["주황", "200, 20, 0", "br=4", "114"],
    ["청록", "0, 50, 255", "br=4", "108"],
    ["자홍", "180, 0, 200", "br=4", "105"],
  ];
  s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y: 2.85, w: 9, h: 0.4, fill: { color: BLK } });
  s.addText("색상", { x: 0.6, y: 2.85, w: 1.8, h: 0.4, fontSize: 11, color: WHITE, fontFace: BODY_FONT, bold: true, valign: "middle", margin: 0 });
  s.addText("RGB 값", { x: 2.4, y: 2.85, w: 2.5, h: 0.4, fontSize: 11, color: WHITE, fontFace: BODY_FONT, bold: true, valign: "middle", margin: 0 });
  s.addText("밝기 레지스터", { x: 4.9, y: 2.85, w: 2, h: 0.4, fontSize: 11, color: WHITE, fontFace: BODY_FONT, bold: true, valign: "middle", margin: 0 });
  s.addText("체감 밝기 Y", { x: 6.9, y: 2.85, w: 2.5, h: 0.4, fontSize: 11, color: WHITE, fontFace: BODY_FONT, bold: true, valign: "middle", margin: 0 });

  for (let i = 0; i < colors.length; i++) {
    const y = 3.3 + i * 0.35;
    const bg = i % 2 === 0 ? VL_GRAY : WHITE;
    s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y, w: 9, h: 0.33, fill: { color: bg } });
    s.addText(colors[i][0], { x: 0.6, y, w: 1.8, h: 0.33, fontSize: 10, fontFace: BODY_FONT, color: BLK, bold: true, valign: "middle", margin: 0 });
    s.addText(colors[i][1], { x: 2.4, y, w: 2.5, h: 0.33, fontSize: 10, fontFace: "Consolas", color: D_GRAY, valign: "middle", margin: 0 });
    s.addText(colors[i][2], { x: 4.9, y, w: 2, h: 0.33, fontSize: 10, fontFace: "Consolas", color: D_GRAY, valign: "middle", margin: 0 });
    s.addText(colors[i][3], { x: 6.9, y, w: 2.5, h: 0.33, fontSize: 10, fontFace: BODY_FONT, color: D_GRAY, align: "center", valign: "middle", margin: 0 });
  }
}

// ══════ 12. 음성 메시지 목록 ══════
{
  const s = pres.addSlide();
  s.addText("음성 메시지 안내 (8종)", { x: 0.5, y: 0.2, w: 9, h: 0.6, fontSize: 28, fontFace: TITLE_FONT, color: BLK, bold: true, margin: 0 });
  addHR(s, 0.8);

  s.addText("gen_voice.py 로 Windows TTS(Heami)에서 자동 생성, 8kHz 16bit 모노 PCM", { x: 0.5, y: 0.9, w: 9, h: 0.3, fontSize: 10, fontFace: BODY_FONT, color: M_GRAY, italic: true, margin: 0 });

  s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y: 1.25, w: 9, h: 0.38, fill: { color: BLK } });
  s.addText("#", { x: 0.6, y: 1.25, w: 0.4, h: 0.38, fontSize: 10, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });
  s.addText("음성 내용", { x: 1.1, y: 1.25, w: 4, h: 0.38, fontSize: 10, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });
  s.addText("사용 시점", { x: 5.2, y: 1.25, w: 4.3, h: 0.38, fontSize: 10, fontFace: BODY_FONT, color: WHITE, bold: true, valign: "middle", margin: 0 });

  const voices = [
    ["0", "이장치는 디에스이 아이엔시의 디아지 팁용 프로그래머입니다", "전원 ON 직후 (최초 1회)"],
    ["1", "디아지용 팁 보드를 올려주세요", "상태 A: LED 쇼 중 보드 대기"],
    ["2", "프로그램이 완료되었습니다", "상태 E: 프로그래밍 성공 후"],
    ["3", "적색 표시는 불량입니다", "상태 E: 불량 칩이 있을 때"],
    ["4", "보드를 분리하여 주십시요", "상태 E: 보드 제거 요청 (30초 반복)"],
    ["5", "93C56 보드입니다", "상태 B: 칩 식별 결과 93C56"],
    ["6", "93C46 보드입니다", "상태 B: 칩 식별 결과 93C46"],
    ["7", "보드가 분리되었습니다", "상태 E: 보드 제거 감지 직후"],
  ];
  for (let i = 0; i < voices.length; i++) {
    const y = 1.68 + i * 0.44;
    const bg = i % 2 === 0 ? VL_GRAY : WHITE;
    s.addShape(pres.shapes.RECTANGLE, { x: 0.5, y, w: 9, h: 0.42, fill: { color: bg } });
    s.addText(voices[i][0], { x: 0.6, y, w: 0.4, h: 0.42, fontSize: 10, fontFace: BODY_FONT, color: M_GRAY, valign: "middle", align: "center", margin: 0 });
    s.addText(voices[i][1], { x: 1.1, y, w: 4, h: 0.42, fontSize: 11, fontFace: BODY_FONT, color: BLK, bold: true, valign: "middle", margin: 0 });
    s.addText(voices[i][2], { x: 5.2, y, w: 4.3, h: 0.42, fontSize: 10, fontFace: BODY_FONT, color: D_GRAY, valign: "middle", margin: 0 });
  }
}

// ══════ 13. 요약 ══════
{
  const s = pres.addSlide();
  s.addShape(pres.shapes.LINE, { x: 0.5, y: 1.0, w: 9, h: 0, line: { color: BLK, width: 3 } });
  s.addText("요약", { x: 0.5, y: 1.3, w: 9, h: 0.8, fontSize: 36, fontFace: TITLE_FONT, color: BLK, bold: true, align: "center", margin: 0 });
  s.addShape(pres.shapes.LINE, { x: 0.5, y: 2.1, w: 9, h: 0, line: { color: BLK, width: 3 } });

  s.addText([
    { text: "93C46 / 93C56 자동 식별 및 이중 지원", options: { bullet: true, breakLine: true, fontSize: 14 } },
    { text: "12개 EEPROM 자동 검사 및 0x00 초기화", options: { bullet: true, breakLine: true, fontSize: 14 } },
    { text: "FreeRTOS 3개 태스크 (메인 + 감지 + 기본)", options: { bullet: true, breakLine: true, fontSize: 14 } },
    { text: "포그핀 불안정 대비: 읽기 전용 감지 + 3회 디바운싱", options: { bullet: true, breakLine: true, fontSize: 14 } },
    { text: "0xAA/0x55 패턴으로 전 비트 정상 동작 확인", options: { bullet: true, breakLine: true, fontSize: 14 } },
    { text: "8가지 LED 쇼 패턴 (등휘도 6색 순환)", options: { bullet: true, breakLine: true, fontSize: 14 } },
    { text: "한국어 TTS 음성 안내 8종 자동 생성", options: { bullet: true, fontSize: 14 } },
  ], { x: 1.5, y: 2.6, w: 7, h: 2.8, fontFace: BODY_FONT, color: D_GRAY, margin: 0 });
}

pres.writeFile({ fileName: "D:/stm32/93c56/93c56_overview.pptx" })
  .then(() => console.log("DONE: D:/stm32/93c56/93c56_overview.pptx"))
  .catch(err => console.error(err));
