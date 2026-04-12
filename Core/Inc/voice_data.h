#ifndef VOICE_DATA_H
#define VOICE_DATA_H

#include <stdint.h>

/* gen_voice.py 로 자동 생성 — 93C56 검사기 음성 클립
 * 포맷: 8 kHz, 16-bit signed mono PCM
 *
 * 인덱스:
 *   0 = VOICE_PLACE_BOARD   "디아지용 팁 보드를 올려주세요"
 *   1 = VOICE_COMPLETE      "프로그램이 완료되었습니다"
 *   2 = VOICE_DEFECTIVE     "적색 표시는 불량입니다"
 *   3 = VOICE_REMOVE_BOARD  "보드를 분리하여 주십시요"
 */

#define VOICE_NUM_CLIPS      4

#define VOICE_PLACE_BOARD    0
#define VOICE_COMPLETE       1
#define VOICE_DEFECTIVE      2
#define VOICE_REMOVE_BOARD   3

typedef struct {
    const int16_t  *data;
    uint32_t        count;
} VoiceClip;

extern const VoiceClip voice_clips[VOICE_NUM_CLIPS];

#endif /* VOICE_DATA_H */
