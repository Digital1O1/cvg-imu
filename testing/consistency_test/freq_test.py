import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Rocking Test Dominant Rocking frequency: 0.1208 Hz
# 

# Load data
df = pd.read_csv('rocking_test.csv')
time_s = (df['timestamp_ms'] - df['timestamp_ms'].iloc[0]) / 1000.0
angle = df['angle_deg']

# Interpolate to uniform time grid if needed
dt = np.median(np.diff(time_s))
uniform_time = np.arange(time_s.iloc[0], time_s.iloc[-1], dt)
uniform_angle = np.interp(uniform_time, time_s, angle)

# FFT
N = len(uniform_angle)
Y = np.fft.fft(uniform_angle - np.mean(uniform_angle))
freqs = np.fft.fftfreq(N, d=dt)
P = np.abs(Y) / N

# Only look at positive frequencies
mask = freqs > 0
dominant_freq = freqs[mask][np.argmax(P[mask])]

print(f'Dominant rocking frequency: {dominant_freq:.4f} Hz')
plt.plot(freqs[mask], P[mask])
plt.xlabel('Frequency (Hz)')
plt.ylabel('Amplitude')
plt.title('FFT of Angle Data')
plt.show()