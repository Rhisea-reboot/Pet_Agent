# -*- coding:utf-8 -*-
# TTS 冒烟测试：调用 api_v2 核心链路合成中文音频，供精简回归验证
import os
import sys
import traceback

now_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "GPT-SoVITS")
os.chdir(now_dir)
sys.path.append(now_dir)
sys.path.append(os.path.join(now_dir, "GPT_SoVITS"))

OUT_WAV = os.path.join(now_dir, "TEMP", "smoke_test.wav")


def main():
    from GPT_SoVITS.TTS_infer_pack.TTS import TTS, TTS_Config

    config_path = os.path.join(now_dir, "GPT_SoVITS", "configs", "tts_infer.yaml")
    print(f"[TTS smoke] loading config: {config_path}")
    tts_config = TTS_Config(config_path)
    tts_pipeline = TTS(tts_config)
    print(f"[TTS smoke] device={tts_config.device}, version={tts_config.version}")

    req = {
        "text": "你好，我是你的桌面宠物，很高兴见到你。",
        "text_lang": "zh",
        "ref_audio_path": os.path.join(now_dir, "TEMP", "ref_audio.wav"),
        "prompt_text": "关注我的公众号奥德园，一起学习爱，一起追赶时代。",
        "prompt_lang": "zh",
        "text_split_method": "cut5",
        "batch_size": 1,
        "batch_threshold": 0.75,
        "speed_factor": 1.0,
        "fragment_interval": 0.3,
        "seed": -1,
        "media_type": "wav",
        "streaming_mode": False,
        "parallel_infer": True,
        "repetition_penalty": 1.35,
        "super_sampling": False,
        "sample_steps": 32,
    }

    sr, audio_data = next(tts_pipeline.run(req))
    print(f"[TTS smoke] synthesized sr={sr}, frames={len(audio_data)}")
    if len(audio_data) == 0:
        raise RuntimeError("empty audio output")

    import soundfile as sf
    sf.write(OUT_WAV, audio_data, sr, format="wav")
    print(f"[TTS smoke] OK -> {OUT_WAV}")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        traceback.print_exc()
        sys.exit(1)
