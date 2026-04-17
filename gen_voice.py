"""
gen_voice.py
Microsoft Edge Neural TTS (edge-tts, ko-KR-InJoonNeural) 로 합성 →
22050 Hz 16-bit mono PCM → G.711 μ-law 8-bit 압축 →
Core/Inc/voice_data.h 및 Core/Src/voice_data.c 자동 생성.

필요 패키지 (ffmpeg 불필요):
    python -m pip install edge-tts miniaudio numpy

실행:
    python gen_voice.py

인코딩 포맷 (펌웨어 디코더와 1:1 일치):
    - 샘플레이트: 22050 Hz (HAL I2S_AUDIOFREQ_22K 표준)
    - 저장 포맷 : 8-bit G.711 μ-law (ITU-T 표준)
    - 디코드   : 256-entry int16 LUT (voice_ulaw_table)
    - 용량     : 22050 × 1B = 22 KB/s
                 14초 음성 ≈ 300 KB (기존 ADPCM과 비슷)
"""

import os
import asyncio
import tempfile
import numpy as np
import edge_tts
import miniaudio

OUTPUT_C    = "Core/Src/voice_data.c"
OUTPUT_H    = "Core/Inc/voice_data.h"
TARGET_RATE = 22050                     # HAL I2S_AUDIOFREQ_22K
VOICE_KO    = "ko-KR-InJoonNeural"      # Neural (남성, 한국어)
GAIN        = 1.0                       # Neural TTS는 이미 풀스케일 — 1.0 권장

# 음성 클립 정의 — (심볼명, 텍스트)
CLIPS = [
    ("place_board",     "디아지용 팁 보드를 올려주세요"),
    ("complete",        "프로그램이 완료되었습니다"),
    ("defective",       "적색 표시는 불량입니다"),
    ("remove_board",    "보드를 분리하여 주십시요"),
    ("board_removed",   "보드가 분리되었습니다"),
    ("place_new_board", "새 보드를 올려 주십시요"),
    ("prog_start",      "프로그램을 시작합니다"),
]

# ═══════════════════════════════════════════════════════════════════
# edge-tts : 텍스트 → mp3
# ═══════════════════════════════════════════════════════════════════
async def synth_mp3(text, mp3_path):
    communicate = edge_tts.Communicate(text, VOICE_KO)
    await communicate.save(mp3_path)

# ═══════════════════════════════════════════════════════════════════
# mp3 → 22050 Hz / 16-bit / mono PCM
# ═══════════════════════════════════════════════════════════════════
def mp3_to_pcm(mp3_path, gain=1.0):
    decoded = miniaudio.decode_file(
        mp3_path,
        output_format=miniaudio.SampleFormat.SIGNED16,
        nchannels=1,
        sample_rate=TARGET_RATE,
    )
    pcm = np.array(decoded.samples, dtype=np.int16).astype(np.float32)
    if gain != 1.0:
        pcm = pcm * gain
    return np.clip(pcm, -32768, 32767).astype(np.int16)

# ═══════════════════════════════════════════════════════════════════
# G.711 μ-law 인코더 (ITU-T 표준)
# ═══════════════════════════════════════════════════════════════════
_ULAW_EXP_LUT = bytes([
    0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
])

def ulaw_encode(pcm16):
    """int16 PCM → μ-law 8-bit bytes (G.711)"""
    BIAS = 0x84
    CLIP = 32635
    out = bytearray(len(pcm16))
    for i in range(len(pcm16)):
        s = int(pcm16[i])
        if s < 0:
            sign = 0x80
            s = -s
        else:
            sign = 0
        if s > CLIP:
            s = CLIP
        s += BIAS
        exponent = _ULAW_EXP_LUT[(s >> 7) & 0xFF]
        mantissa = (s >> (exponent + 3)) & 0x0F
        out[i] = (~(sign | (exponent << 4) | mantissa)) & 0xFF
    return bytes(out)

def ulaw_decode_table():
    """256-entry μ-law → int16 LUT (펌웨어 디코더용)"""
    BIAS = 0x84
    table = [0] * 256
    for byte in range(256):
        b = (~byte) & 0xFF
        sign = b & 0x80
        exp  = (b >> 4) & 0x07
        mant = b & 0x0F
        val  = ((mant << 3) + BIAS) << exp
        val -= BIAS
        if sign:
            val = -val
        table[byte] = val
    return table

# ═══════════════════════════════════════════════════════════════════
# 메인
# ═══════════════════════════════════════════════════════════════════
def main():
    print(f"Voice: {VOICE_KO}, target: {TARGET_RATE} Hz, gain: {GAIN}x, codec: μ-law 8bit")
    all_clips = []

    for i, (name, text) in enumerate(CLIPS):
        print(f"[{i+1}/{len(CLIPS)}] '{text}'")

        tmp_mp3 = tempfile.mktemp(suffix=f"_{name}.mp3")
        asyncio.run(synth_mp3(text, tmp_mp3))

        pcm  = mp3_to_pcm(tmp_mp3, gain=GAIN)
        os.remove(tmp_mp3)

        data = ulaw_encode(pcm)

        n_samp = len(pcm)
        n_byte = len(data)
        dur    = n_samp / TARGET_RATE
        print(f"      → {n_samp} samples ({dur:.2f}s), μ-law {n_byte} B ({n_byte/1024:.1f} KB)")

        all_clips.append((name, text, n_samp, n_byte, dur, data))

    # ── .c 파일 출력 ───────────────────────────────────────────
    print(f"\n{OUTPUT_C} 생성 중...")
    lines = []
    lines.append(
        f"/* voice_data.c  —  자동 생성 파일, 수정하지 마세요.\n"
        f" * 포맷    : {TARGET_RATE} Hz, G.711 μ-law 8-bit, mono\n"
        f" * 생성 도구: gen_voice.py  (edge-tts {VOICE_KO}) */\n\n"
        f"#include \"voice_data.h\"\n"
    )

    # μ-law 디코드 LUT
    tbl = ulaw_decode_table()
    lines.append("/* G.711 μ-law → int16 PCM 디코드 LUT (ITU-T 표준) */")
    lines.append("const int16_t voice_ulaw_table[256] = {")
    for row_off in range(0, 256, 8):
        chunk = tbl[row_off:row_off + 8]
        lines.append("    " + ", ".join(f"{v:6d}" for v in chunk) + ",")
    lines.append("};\n")

    # 각 클립 데이터
    for name, text, n_samp, n_byte, dur, data in all_clips:
        lines.append(f"/* '{text}' ({dur:.2f}s, {n_samp} samples, {n_byte} B) */")
        lines.append(f"static const uint8_t voice_{name}[{n_byte}] = {{")
        for row_off in range(0, n_byte, 16):
            chunk = data[row_off:row_off + 16]
            lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
        lines.append("};\n")

    lines.append("const VoiceClip voice_clips[VOICE_NUM_CLIPS] = {")
    for name, text, n_samp, n_byte, dur, data in all_clips:
        lines.append(f"    {{ voice_{name}, {n_samp}UL }},  /* {text} */")
    lines.append("};")
    lines.append("")

    os.makedirs(os.path.dirname(OUTPUT_C), exist_ok=True)
    with open(OUTPUT_C, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    # ── .h 파일 출력 ───────────────────────────────────────────
    print(f"{OUTPUT_H} 생성 중...")
    h_lines = []
    h_lines.append("#ifndef VOICE_DATA_H")
    h_lines.append("#define VOICE_DATA_H")
    h_lines.append("")
    h_lines.append("#include <stdint.h>")
    h_lines.append("")
    h_lines.append("/* gen_voice.py 로 자동 생성 — 93C56 검사기 음성 클립")
    h_lines.append(f" * 포맷: {TARGET_RATE} Hz, G.711 μ-law 8-bit, mono")
    h_lines.append(f" * 화자: {VOICE_KO} (edge-tts Neural)")
    h_lines.append(" *")
    h_lines.append(" * 인덱스:")
    for i, (name, text) in enumerate(CLIPS):
        h_lines.append(f" *   {i} = VOICE_{name.upper():<16s} \"{text}\"")
    h_lines.append(" */")
    h_lines.append("")
    h_lines.append(f"#define VOICE_NUM_CLIPS    {len(CLIPS)}")
    h_lines.append(f"#define VOICE_SAMPLE_RATE  {TARGET_RATE}u")
    h_lines.append("")
    for i, (name, _) in enumerate(CLIPS):
        h_lines.append(f"#define VOICE_{name.upper():<16s} {i}")
    h_lines.append("")
    h_lines.append("typedef struct {")
    h_lines.append("    const uint8_t *ulaw;         /* G.711 μ-law 바이트 스트림 */")
    h_lines.append("    uint32_t       sample_count; /* 총 샘플 수 = 바이트 수 */")
    h_lines.append("} VoiceClip;")
    h_lines.append("")
    h_lines.append("extern const VoiceClip voice_clips[VOICE_NUM_CLIPS];")
    h_lines.append("extern const int16_t   voice_ulaw_table[256];")
    h_lines.append("")
    h_lines.append("#endif /* VOICE_DATA_H */")
    h_lines.append("")

    os.makedirs(os.path.dirname(OUTPUT_H), exist_ok=True)
    with open(OUTPUT_H, "w", encoding="utf-8") as f:
        f.write("\n".join(h_lines))

    total_kb = sum(n_byte for _, _, _, n_byte, _, _ in all_clips) / 1024
    print(f"\n완료! 플래시 사용량: {total_kb:.1f} KB")
    print("STM32CubeIDE 에서 Refresh 후 빌드·플래시하세요.")

if __name__ == "__main__":
    main()
