import wave
import struct
import math

# Generate a 10-second stereo WAV with slight tempo drift around 115 BPM
sample_rate = 44100
duration = 10.0
total_samples = int(sample_rate * duration)

base_bpm = 115.0

# Calculate beat times with a slight human drift +/- 2%
beat_times = []
t = 0.2
while t < duration - 0.2:
    beat_times.append(t)
    drift = 0.02 * math.sin(t * 1.5)
    current_bpm = base_bpm * (1.0 + drift)
    t += 60.0 / current_bpm

audio_l = [0.0] * total_samples
audio_r = [0.0] * total_samples

for idx, bt in enumerate(beat_times):
    start_s = int(bt * sample_rate)
    is_downbeat = (idx % 4 == 0)
    freq = 60.0 if is_downbeat else 180.0
    decay = 25.0 if is_downbeat else 40.0
    amp = 0.8 if is_downbeat else 0.5
    
    # Generate 150ms percussive pulse
    pulse_len = min(int(0.15 * sample_rate), total_samples - start_s)
    for i in range(pulse_len):
        time_sec = i / sample_rate
        env = math.exp(-decay * time_sec)
        val = amp * math.sin(2.0 * math.pi * freq * time_sec) * env
        audio_l[start_s + i] += val
        audio_r[start_s + i] += val * (0.9 if is_downbeat else 1.1)

with wave.open("synth_drift_115bpm.wav", "w") as wav:
    wav.setnchannels(2)
    wav.setsampwidth(2) # 16-bit
    wav.setframerate(sample_rate)
    
    frames = bytearray()
    for l, r in zip(audio_l, audio_r):
        sl = max(-32767, min(32767, int(l * 32767)))
        sr = max(-32767, min(32767, int(r * 32767)))
        frames.extend(struct.pack("<hh", sl, sr))
    wav.writeframes(frames)

print("Generated synth_drift_115bpm.wav successfully.")
