#!/usr/bin/env python3
"""
PCM Stream Sender for STM32 SAI PCM Player
------------------------------------------
Reads audio file (WAV/FLAC/MP3/OGG...), resamples to 8kHz 16-bit mono,
sends via frame protocol (AA 55 len cmd data xor) at 460800 baud.

Requirements: pip install pyserial pydub
  pydub requires ffmpeg — download from https://ffmpeg.org/

Without pydub, falls back to stdlib wave module (WAV only, simpler resampling).

Usage:
    python pcm_sender.py song.flac COM3
    python pcm_sender.py song.wav  COM3 --baud 460800
"""

import sys
import struct
import argparse
import time

try:
    import serial
except ImportError:
    print("ERROR: pyserial required.  pip install pyserial")
    sys.exit(1)

try:
    from pydub import AudioSegment
    HAS_PYDUB = True
except ImportError:
    HAS_PYDUB = False
    import wave

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False


FRAME_CMD_PCM = 0x10
FRAME_HDR1    = 0xAA
FRAME_HDR2    = 0x55


def build_frame(data: bytes) -> bytes:
    """AA 55 len cmd data xor"""
    length = 1 + len(data)
    body = bytes([FRAME_CMD_PCM]) + data
    xor = length
    for b in body:
        xor ^= b
    return bytes([FRAME_HDR1, FRAME_HDR2, length]) + body + bytes([xor & 0xFF])


def read_audio_pydub(path: str, target_rate: int = 8000) -> bytes:
    """Use pydub+ffmpeg to decode any format → 8kHz 16-bit mono PCM."""
    audio = AudioSegment.from_file(path)
    print(f'  Source: {audio.frame_rate}Hz {audio.sample_width*8}bit {audio.channels}ch '
          f'{len(audio)/1000:.1f}s  ({path.rsplit(".",1)[-1].upper()})')

    if audio.channels > 1:
        audio = audio.set_channels(1)
    if audio.frame_rate != target_rate:
        audio = audio.set_frame_rate(target_rate)
    if audio.sample_width != 2:
        audio = audio.set_sample_width(2)

    pcm = audio.raw_data
    duration = len(pcm) / (target_rate * 2)
    print(f'  Output: {len(pcm)/1024:.1f} KB PCM, {duration:.1f}s')
    return pcm


def read_wav_stdlib(path: str, target_rate: int = 8000) -> bytes:
    """Fallback: stdlib wave + manual resampling (WAV only)."""
    with wave.open(path, 'rb') as w:
        src_rate   = w.getframerate()
        src_width  = w.getsampwidth()
        nchannels  = w.getnchannels()
        nframes    = w.getnframes()
        raw        = w.readframes(nframes)

    print(f'  Source: {src_rate}Hz {src_width*8}bit {nchannels}ch {nframes} frames')

    if src_width == 2:
        fmt = f'<{nframes * nchannels}h'
        samples = list(struct.unpack(fmt, raw))
    elif src_width == 1:
        fmt = f'<{nframes * nchannels}B'
        samples = [v - 128 for v in struct.unpack(fmt, raw)]
    elif src_width == 3:
        samples = []
        for i in range(0, len(raw), 3 * nchannels):
            for ch in range(nchannels):
                b0 = raw[i + ch*3]
                b1 = raw[i + ch*3 + 1]
                b2 = raw[i + ch*3 + 2]
                val = (b0 | (b1 << 8) | (b2 << 16))
                if val & 0x800000:
                    val -= 0x1000000
                samples.append(val)
    else:
        raise ValueError(f'Unsupported sample width: {src_width}')

    if nchannels == 2:
        mono = [(samples[i] + samples[i+1]) // 2 for i in range(0, len(samples), 2)]
    else:
        mono = samples

    if src_rate != target_rate:
        if HAS_NUMPY:
            arr = np.array(mono, dtype=np.float64)
            out_len = int(len(arr) * target_rate / src_rate)
            idx = np.linspace(0, len(arr) - 1, out_len)
            mono = np.interp(idx, np.arange(len(arr)), arr).astype(int).tolist()
        else:
            out_len = int(len(mono) * target_rate / src_rate)
            resampled = []
            for i in range(out_len):
                src_idx = i * src_rate / target_rate
                lo = int(src_idx)
                hi = min(lo + 1, len(mono) - 1)
                resampled.append(int(mono[lo] + (mono[hi] - mono[lo]) * (src_idx - lo)))
            mono = resampled
        print(f'  Resampled: {src_rate} → {target_rate} Hz')

    pcm = b''.join(struct.pack('<h', max(-32768, min(32767, s))) for s in mono)
    duration = len(pcm) / (target_rate * 2)
    print(f'  Output: {len(pcm)/1024:.1f} KB PCM, {duration:.1f}s')
    return pcm


def main():
    ap = argparse.ArgumentParser(description='PCM Stream Sender for STM32 SAI')
    ap.add_argument('audio_file', help='Audio file (WAV/FLAC/MP3/OGG...)')
    ap.add_argument('port',       help='Serial port, e.g. COM3')
    ap.add_argument('--baud',     type=int, default=460800)
    ap.add_argument('--rate',     type=int, default=8000)
    ap.add_argument('--chunk',    type=int, default=240, help='Max 254 PCM bytes/frame')
    ap.add_argument('--fps',      type=int, default=50,  help='Frames per second target')
    args = ap.parse_args()
    if args.chunk > 254:
        args.chunk = 254
    args.chunk &= ~1  # make even

    # ---- decode audio ----
    if HAS_PYDUB:
        pcm_data = read_audio_pydub(args.audio_file, args.rate)
    else:
        pcm_data = read_wav_stdlib(args.audio_file, args.rate)

    audio_bps       = args.rate * 2
    bytes_per_frame = args.chunk + 5
    frames_total    = (len(pcm_data) + args.chunk - 1) // args.chunk
    sec_total       = len(pcm_data) / audio_bps
    send_interval   = 1.0 / args.fps
    bps_needed      = bytes_per_frame / send_interval

    print(f'  Audio drain: {audio_bps/1024:.1f} KB/s  '
          f'Send: {args.fps} fps x {bytes_per_frame}B = {bps_needed/1024:.1f} KB/s')

    # ---- connect ----
    print(f'  Opening {args.port} @ {args.baud}...')
    ser = serial.Serial(args.port, args.baud, timeout=0.5)
    time.sleep(0.1)

    # ---- send ----
    sent       = 0
    start      = time.time()
    total      = len(pcm_data)
    ring_bytes = 65536       # MCU ring buffer size
    burst_end  = ring_bytes * 60 // 100  # burst the first ~60% of ring

    try:
        while sent < total:
            chunk = pcm_data[sent:sent + args.chunk]
            frame = build_frame(chunk)
            ser.write(frame)
            ser.flush()
            sent += len(chunk)

            # Fill ring buffer at full speed for the first burst_end bytes,
            # then pace to audio rate to avoid overflow.
            if sent > burst_end:
                elapsed   = time.time() - start
                expected  = sent / audio_bps
                if elapsed < expected:
                    time.sleep(expected - elapsed)

            if sent % (args.chunk * 20) == 0 or sent >= total:
                pct = sent * 100 / total
                print(f'\r  [{pct:5.1f}%]  {sent//1024:5d}/{total//1024} KB', end='')

        elapsed = time.time() - start
        if elapsed < sec_total:
            remain = sec_total - elapsed + 2.0
            print(f'\n  Waiting {remain:.1f}s for playback...')
            time.sleep(remain)

    except KeyboardInterrupt:
        print('\n  [Aborted]')
    finally:
        ser.close()
        print(f'\n  Done — {sent/1024:.1f} KB in {time.time()-start:.1f}s')

if __name__ == '__main__':
    main()
