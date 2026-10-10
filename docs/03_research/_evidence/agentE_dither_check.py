# Reference check for DITHERING.md equations: TPDF error RMS = delta/2, moments independent of input.
import numpy as np
rng = np.random.default_rng(7); N = 16; d = 2.0**-(N-1); n = 2_000_000
def q(x, dither=True):
    t = (rng.random(x.shape) - rng.random(x.shape)) * d if dither else 0
    return np.clip(np.round((x + t) / d), -2**(N-1), 2**(N-1)-1) * d
e0 = q(np.zeros(n)) - 0
print("TPDF err rms dBFS", 20*np.log10(e0.std()), "expected", 20*np.log10(d/2))
for off in np.linspace(0, d, 5):
    x = np.full(n, off); e = q(x) - x
    print(f"offset {off/d:.2f} LSB  mean {e.mean()/d:+.4f} LSB  var {e.var()/d**2:.4f} LSB^2 (expect 0.25)")
x = np.full(n, 0.3*d); e = q(x, False) - x; print("undithered: mean", e.mean()/d, "var", e.var()/d**2)
print("24-bit TPDF dBFS", 20*np.log10(2.0**-23/2), " undithered 16 FS-sine SNR", 6.02*16+1.76)
