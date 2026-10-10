# Agent E evidence experiments: codec delay, TSM peak growth, generation loss.
# Uses ffmpeg (8.1.2 full build) via pipes; numpy for analysis.
import subprocess, numpy as np, os, json, re, sys
FF = "ffmpeg"
SR = 44100
os.chdir(os.path.dirname(os.path.abspath(__file__)))

def write_wav(path, x, sr=SR):
    p = subprocess.run([FF, "-y", "-v", "error", "-f", "f64le", "-ar", str(sr), "-ac", str(x.shape[1]),
                        "-i", "-", "-c:a", "pcm_f32le", path], input=x.astype("<f8").tobytes())
    assert p.returncode == 0

def read_pcm(path, extra_in=(), sr=None):
    cmd = [FF, "-v", "error", *extra_in, "-i", path]
    if sr: cmd += ["-ar", str(sr)]
    cmd += ["-f", "f64le", "-ac", "2", "-"]
    out = subprocess.run(cmd, capture_output=True).stdout
    return np.frombuffer(out, "<f8").reshape(-1, 2)

def enc(src, dst, args):
    p = subprocess.run([FF, "-y", "-v", "error", "-i", src, *args, dst], capture_output=True)
    assert p.returncode == 0, p.stderr

def peaks(x, sr=SR):
    sp = 20*np.log10(np.max(np.abs(x)) + 1e-30)
    # true peak via ffmpeg ebur128 (BS.1770 4x oversampling)
    tmp = "tp_tmp.wav"; write_wav(tmp, x, sr)
    r = subprocess.run([FF, "-hide_banner", "-nostats", "-i", tmp, "-af", "ebur128=peak=true", "-f", "null", "-"],
                       capture_output=True, text=True).stderr
    tp = float(re.findall(r"True peak:\s+Peak:\s+(-?[\d.]+|-inf)", r)[-1])
    lufs = float(re.findall(r"I:\s+(-?[\d.]+) LUFS", r)[-1])
    return sp, tp, lufs

# ---------- E1: codec delay ----------
def click_track(sr=SR, secs=6):
    x = np.zeros((sr*secs, 2)); rng = np.random.default_rng(1)
    x += rng.normal(0, 1e-4, x.shape)
    for t in np.arange(0.5, secs-0.5, 0.5):
        i = int(t*sr); x[i:i+32, :] += 0.5*np.hanning(32)[:, None]
    return x

def first_click(y):
    e = np.abs(y[:, 0]); return int(np.argmax(e > 0.1))

def e1():
    res = {}
    for sr in (44100, 48000):
        x = click_track(sr); write_wav("clk.wav", x, sr); ref = first_click(x); n = len(x)
        cases = {
            "mp3_cbr320": (["-c:a", "libmp3lame", "-b:a", "320k"], "mp3"),
            "mp3_V0": (["-c:a", "libmp3lame", "-q:a", "0"], "mp3"),
            "mp3_V2": (["-c:a", "libmp3lame", "-q:a", "2"], "mp3"),
            "aac_256k_m4a": (["-c:a", "aac", "-b:a", "256k"], "m4a"),
            "vorbis_q6": (["-c:a", "libvorbis", "-q:a", "6"], "ogg"),
            "opus_160k": (["-c:a", "libopus", "-b:a", "160k"], "opus"),
            "flac": (["-c:a", "flac"], "flac"),
        }
        for k, (a, ext) in cases.items():
            f = f"clk_{k}.{ext}"; enc("clk.wav", f, a)
            y = read_pcm(f, sr=sr)
            y_raw = read_pcm(f, extra_in=("-flags2", "+skip_manual"), sr=sr)
            res[f"{sr}/{k}"] = dict(offset_trimmed=first_click(y)-ref, len_diff_trimmed=len(y)-n,
                                    offset_untrimmed=first_click(y_raw)-ref, len_diff_untrimmed=len(y_raw)-n)
    return res

# ---------- synthetic loud "master" ----------
def master(sr=SR, secs=20, bpm=120, seed=0):
    rng = np.random.default_rng(seed); t = np.arange(sr*secs)/sr; x = np.zeros((len(t), 2))
    beat = 60/bpm
    for b in np.arange(0, secs, beat):
        i = int(b*sr); L = int(0.25*sr); tt = np.arange(L)/sr
        kick = np.sin(2*np.pi*(50*tt + 60*(1-np.exp(-tt*30))/30*1))*np.exp(-tt*12)
        seg = slice(i, min(i+L, len(t))); x[seg] += 0.9*kick[:seg.stop-i, None]
        if int(round(b/beat)) % 2 == 1:
            sn = rng.normal(0, 1, (int(0.15*sr), 2))*np.exp(-np.arange(int(0.15*sr))/sr*25)[:, None]
            seg = slice(i, min(i+len(sn), len(t))); x[seg] += 0.5*sn[:seg.stop-i]
    for b in np.arange(0, secs, beat/2):  # hats
        i = int(b*sr); h = rng.normal(0, 1, (int(0.03*sr), 2))*0.15
        h = np.diff(h, axis=0, prepend=0); seg = slice(i, min(i+len(h), len(t))); x[seg] += h[:seg.stop-i]
    for f, g in ((55, .35), (110, .2), (220, .12), (261.6, .1), (329.6, .1), (392, .1)):  # bass + pad (saw)
        ph = (f*t + rng.random()) % 1; saw = 2*ph-1
        x[:, 0] += g*saw; x[:, 1] += g*((f*1.003*t) % 1*2-1)
    return x

def limit(x, drive_db=10, sr=SR):
    write_wav("pre.wav", x/np.max(np.abs(x))*10**(-6/20), sr)
    enc("pre.wav", "master.wav", ["-af", f"volume={drive_db}dB,alimiter=limit=0.9886:level=disabled:attack=1:release=50",
                                  "-c:a", "pcm_f32le"])  # 0.9886 = -0.1 dBFS
    return read_pcm("master.wav")

def e2(m):
    out = {"source": peaks(m)}
    for r in (0.90, 0.97, 0.99, 1.01, 1.03, 1.10):
        for name, af in (("rubberband_crisp", f"rubberband=tempo={r}"),
                         ("rubberband_together", f"rubberband=tempo={r}:channels=together"),
                         ("atempo_wsola", f"atempo={r}")):
            enc("master.wav", "ts.wav", ["-af", af, "-c:a", "pcm_f32le"])
            out[f"{name}@{r}"] = peaks(read_pcm("ts.wav"))
    return out

def snr(a, b):
    n = min(len(a), len(b)); a = a[:n]; b = b[:n]
    return 10*np.log10(np.sum(a**2)/np.sum((a-b)**2))

def e3(m):
    out = {}
    for name, a in (("mp3_V2", ["-c:a", "libmp3lame", "-q:a", "2"]), ("mp3_320", ["-c:a", "libmp3lame", "-b:a", "320k"]),
                    ("aac_256", ["-c:a", "aac", "-b:a", "256k"])):
        ext = "m4a" if name.startswith("aac") else "mp3"
        cur = "master.wav"; prev = m; rows = []
        for g in range(1, 11):
            enc(cur, f"gen.{ext}", a); y = read_pcm(f"gen.{ext}"); write_wav("gen.wav", y); cur = "gen.wav"
            sp, tp, _ = peaks(y)
            rows.append(dict(gen=g, snr_vs_orig=round(snr(m, y), 2), snr_vs_prev=round(snr(prev, y), 2),
                             len=len(y)-len(m), sample_peak=round(sp, 2), true_peak=round(tp, 2)))
            prev = y
        out[name] = rows
    return out

if __name__ == "__main__":
    R = {"E1_codec_delay": e1()}
    m = limit(master()); R["E2_tsm_peaks(sample_dBFS,true_dBTP,LUFS)"] = e2(m); R["E3_generation"] = e3(m)
    json.dump(R, open("results.json", "w"), indent=1); print(json.dumps(R, indent=1))
