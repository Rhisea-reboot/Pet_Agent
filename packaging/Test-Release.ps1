[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ReleaseDirectory
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $ReleaseDirectory)) {
    throw "Release directory not found: $ReleaseDirectory"
}

$requiredFiles = @(
    "VPet.exe",
    "tts_config.json",
    "platforms\qwindows.dll",
    "GPT-SoVITS\api_v2.py",
    "GPT-SoVITS\GPT_SoVITS\configs\tts_infer.yaml",
    "GPT-SoVITS\TEMP\ref_audio.wav",
    "GPT-SoVITS\GPT_SoVITS\pretrained_models\chinese-roberta-wwm-ext-large\config.json",
    "GPT-SoVITS\GPT_SoVITS\pretrained_models\chinese-hubert-base\config.json",
    "GPT-SoVITS\GPT_SoVITS\pretrained_models\gsv-v2final-pretrained\s1bert25hz-5kh-longer-epoch=12-step=369668.ckpt",
    "GPT-SoVITS\GPT_SoVITS\pretrained_models\gsv-v2final-pretrained\s2G2333k.pth",
    "GPT-SoVITS\GPT_SoVITS\pretrained_models\fast_langdetect",
    "GPT-SoVITS\tools\asr\models\speech_fsmn_vad_zh-cn-16k-common-pytorch\model.pt",
    "GPT-SoVITS\tools\asr\models\punc_ct-transformer_zh-cn-common-vocab272727-pytorch\model.pt",
    "GPT-SoVITS\tools\asr\models\speech_paraformer-large_asr_nat-zh-cn-16k-common-vocab8404-pytorch\model.pt"
)

$missing = @($requiredFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $ReleaseDirectory $_)) })
if ($missing.Count -gt 0) {
    throw ("Release self-check failed. Missing:`n" + ($missing -join [Environment]::NewLine))
}

$animationCount = @(Get-ChildItem -LiteralPath (Join-Path $ReleaseDirectory "Animation") -Recurse -Filter *.png -File).Count
if ($animationCount -eq 0) { throw "Release self-check failed: no animation PNG files found" }

$pythonCandidates = @(
    (Join-Path $ReleaseDirectory "GPT-SoVITS\runtime\python.exe"),
    (Join-Path $ReleaseDirectory "GPT-SoVITS\runtime\Scripts\python.exe")
)
$python = $pythonCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ($null -eq $python) { throw "Release self-check failed: bundled Python runtime is missing" }
& $python -c "import os, sys; root=os.path.join(r'$ReleaseDirectory', 'GPT-SoVITS'); os.chdir(root); sys.path[:0]=[root, os.path.join(root, 'GPT_SoVITS')]; import torch, pytorch_lightning, tensorboard, torchmetrics, fast_langdetect; from GPT_SoVITS.TTS_infer_pack.TTS import TTS_Config; TTS_Config('GPT_SoVITS/configs/tts_infer.yaml'); print('runtime imports ok')"
if ($LASTEXITCODE -ne 0) { throw "Release self-check failed: Python runtime imports failed" }

Write-Host "Release self-check passed. Animation PNG count: $animationCount"
