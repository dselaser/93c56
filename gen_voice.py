"""
gen_voice.py
Windows SAPI5 TTS (win32com 직접 제어) 를 사용하여
93C56 검사기 음성 데이터를 Core/Src/voice_data.c 로 생성합니다.

필요 패키지:
    pip install pywin32 numpy

실행:
    python gen_voice.py
"""

import os, wave, tempfile
import numpy as np
import win32com.client

OUTPUT_C    = "Core/Src/voice_data.c"
OUTPUT_H    = "Core/Inc/voice_data.h"
TARGET_RATE = 8000   # STM32 I2S 샘플레이트

# 음성 클립 정의 (voice_data.h 의 인덱스와 동일)
#   (name, voice_gender, text)
#   KO = Microsoft Heami
CLIPS = [
    ("place_board",  "KO", "디아지용 팁 보드를 올려주세요"),
    ("complete",     "KO", "프로그램이 완료되었습니다"),
    ("defective",    "KO", "적색 표시는 불량입니다"),
    ("remove_board", "KO", "보드를 분리하여 주십시요"),
]

# ── SAPI5 화자 ID 수집 ───────────────────────────────────────
sapi = win32com.client.Dispatch("SAPI.SpVoice")
voice_id = {"KO": None, "EN": None}
for v in sapi.GetVoices():
    lang = v.GetAttribute("Language") or ""
    name = v.GetAttribute("Name") or ""
    if "412" in lang or "ko" in name.lower() or "heami" in name.lower():
        voice_id["KO"] = v
        print(f"KO 화자: {name}")
    elif "409" in lang or "zira" in name.lower() or "en-us" in name.lower():
        voice_id["EN"] = v
        print(f"EN 화자: {name}")

if voice_id["KO"] is None:
    raise RuntimeError("Korean SAPI5 voice not found")
if voice_id["EN"] is None:
    print("EN 화자 없음 → KO 화자로 대체")
    voice_id["EN"] = voice_id["KO"]

# ── WAV → 8kHz int16 모노 PCM 변환 ──────────────────────────
def wav_to_pcm(wav_path):
    with wave.open(wav_path, "r") as wf:
        src_rate   = wf.getframerate()
        n_ch       = wf.getnchannels()
        sampwidth  = wf.getsampwidth()
        n_frames   = wf.getnframes()
        raw        = wf.readframes(n_frames)

    if sampwidth == 2:
        pcm = np.frombuffer(raw, dtype=np.int16).astype(np.float32)
    elif sampwidth == 4:
        pcm = np.frombuffer(raw, dtype=np.int32).astype(np.float32)
        pcm = (pcm / 65536).astype(np.float32)
    else:
        raise ValueError(f"지원하지 않는 샘플 폭: {sampwidth}")

    if n_ch == 2:
        pcm = pcm.reshape(-1, 2).mean(axis=1)

    n_src = len(pcm)
    n_dst = int(n_src * TARGET_RATE / src_rate)
    pcm_8k = np.interp(np.linspace(0, n_src - 1, n_dst), np.arange(n_src), pcm)
    return np.clip(pcm_8k, -32768, 32767).astype(np.int16).tolist()

# ── 각 클립 생성 (win32com: 매 호출마다 독립 SpFileStream) ──
COLS = 16
all_arrays = []

for i, (name, vkey, text) in enumerate(CLIPS):
    print(f"[{i+1}/{len(CLIPS)}] [{vkey}] '{text}' 생성 중...")

    tmp = tempfile.mktemp(suffix=f"_{name}.wav")

    # SpFileStream 으로 직접 WAV 파일에 출력
    stream = win32com.client.Dispatch("SAPI.SpFileStream")
    stream.Open(tmp, 3)   # 3 = SSFMCreateForWrite
    sapi.AudioOutputStream = stream
    sapi.Voice = voice_id[vkey]
    sapi.Speak(text)      # 동기 호출 (기본값 SVSFDefault=0)
    stream.Close()

    samples = wav_to_pcm(tmp)
    os.remove(tmp)

    n = len(samples)
    dur = n / TARGET_RATE
    print(f"      → {n} samples ({dur:.2f}s), {n*2/1024:.1f} KB")

    rows = []
    for j in range(0, n, COLS):
        chunk = samples[j : j + COLS]
        rows.append("    " + ", ".join(f"{s:6d}" for s in chunk) + ",")
    all_arrays.append((name, vkey, text, n, dur, rows))

# ── C 파일 생성 ──────────────────────────────────────────────
print(f"\n{OUTPUT_C} 생성 중...")
lines = []
lines.append(f"""\
/* voice_data.c  —  자동 생성 파일, 수정하지 마세요.
 * 포맷    : {TARGET_RATE} Hz, 16-bit signed mono PCM
 * 생성 도구: gen_voice.py  (win32com SAPI5: Heami KO) */

#include "voice_data.h"
""")

for name, vkey, text, n, dur, rows in all_arrays:
    lines.append(f"/* [{vkey}] '{text}' ({dur:.2f}s, {n} samples) */")
    lines.append(f"static const int16_t voice_{name}[{n}] = {{")
    lines.extend(rows)
    lines.append("};\n")

lines.append(f"const VoiceClip voice_clips[VOICE_NUM_CLIPS] = {{")
for name, vkey, text, n, dur, rows in all_arrays:
    lines.append(f"    {{ voice_{name}, {n}UL }},  /* [{vkey}] {text} */")
lines.append("};")
lines.append("")

os.makedirs(os.path.dirname(OUTPUT_C), exist_ok=True)
with open(OUTPUT_C, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))

# ── H 파일 생성 ──────────────────────────────────────────────
print(f"{OUTPUT_H} 생성 중...")
h_src = """\
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
"""
os.makedirs(os.path.dirname(OUTPUT_H), exist_ok=True)
with open(OUTPUT_H, "w", encoding="utf-8") as f:
    f.write(h_src)

total_kb = sum(n for _, _, _, n, _, _ in all_arrays) * 2 / 1024
print(f"\n완료! 총 플래시 사용량: {total_kb:.1f} KB")
print("STM32CubeIDE 에서 Refresh 후 빌드·플래시하세요.")
