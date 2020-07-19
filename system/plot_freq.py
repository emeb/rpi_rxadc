#!/usr/bin/python3
#
# frequency plot
#
# 07-18-2020 E. Brombaugh

import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import scipy.signal as signal
from scipy.fftpack import fft, ifft, fftfreq, fftshift

def plot_freq(title, Fs, bits, data):
	data_len = len(data)
	data_scl = 2**(bits-1)-1
	f = Fs * fftshift(fftfreq(data_len))/1e3
	win = signal.blackmanharris(data_len)
	data_bhwin = data * win
	bh_gain = sum(win)/data_len
	data_dB = 20*np.log10(np.abs(fftshift(fft(data_bhwin)))/
						   (data_len*(data_scl/2)*bh_gain))
	plt.plot(f, data_dB)
	plt.grid()
	plt.xlabel("Frequency (kHz)")
	plt.ylabel("dB")
	plt.title(title)
	plt.show()
