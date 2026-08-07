# -*- coding:utf-8 -*-
# ASR 冒烟测试：识别 smoke_test.wav，供精简回归验证（对应语音输入链路）
import os
import sys
import traceback

now_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "GPT-SoVITS")
os.chdir(now_dir)
sys.path.append(now_dir)
sys.path.append(os.path.join(now_dir, "GPT_SoVITS"))

WAV = os.path.join(now_dir, "TEMP", "smoke_test.wav")


def main():
    if not os.path.exists(WAV):
        raise RuntimeError(f"missing input wav: {WAV} (run smoke_tts.py first)")
    sys.path.insert(0, os.path.join(now_dir, "tools", "asr"))
    from funasr_asr import only_asr

    text = only_asr(WAV, "zh")
    print(f"[ASR smoke] recognized: {text!r}")
    if not text.strip():
        raise RuntimeError("ASR returned empty text")
    print("[ASR smoke] OK")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        traceback.print_exc()
        sys.exit(1)
