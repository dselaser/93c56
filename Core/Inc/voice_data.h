#ifndef VOICE_DATA_H
#define VOICE_DATA_H

#include <stdint.h>

/* gen_voice.py 로 자동 생성 — 93C46/93C56 검사기 음성 클립
 * 포맷: 8 kHz, 16-bit signed mono PCM
 *
 * 인덱스:
 *   0 = VOICE_INTRO          "디에스이 아이엔시의 디아지 팁용 프로그래머입니다"
 *   1 = VOICE_PLACE_BOARD    "디아지용 팁 보드를 올려주세요"
 *   2 = VOICE_COMPLETE       "프로그램이 완료되었습니다"
 *   3 = VOICE_DEFECTIVE      "적색 표시는 불량입니다"
 *   4 = VOICE_REMOVE_BOARD   "보드를 분리하여 주십시요"
 *   5 = VOICE_IS_93C56       "93C56 보드입니다"
 *   6 = VOICE_IS_93C46       "93C46 보드입니다"
 *   7 = VOICE_BOARD_REMOVED  "보드가 분리되었습니다"
 */

#define VOICE_NUM_CLIPS      8

#define VOICE_INTRO          0
#define VOICE_PLACE_BOARD    1
#define VOICE_COMPLETE       2
#define VOICE_DEFECTIVE      3
#define VOICE_REMOVE_BOARD   4
#define VOICE_IS_93C56       5
#define VOICE_IS_93C46       6
#define VOICE_BOARD_REMOVED  7

typedef struct {
    const int16_t  *data;
    uint32_t        count;
} VoiceClip;

extern const VoiceClip voice_clips[VOICE_NUM_CLIPS];

#endif /* VOICE_DATA_H */
