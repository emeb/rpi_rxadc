#!/usr/bin/python3
#
# Digital DownConverter testbench
#
# 07-23-2015 E. Brombaugh

# Test out the DDC
import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import scipy.signal as signal
from scipy.fftpack import fft, ifft, fftfreq, fftshift
from ddc import ddc
from plot_freq import plot_freq
      
# generate a signal
data_len = 2**18
Fs = 50e6
Ft = 7.125e6
data_bits = 14
data_scl = 2**(data_bits-1)-1
t = np.arange(data_len)/Fs
data_in = np.floor(data_scl/2 * (np.sin(2*np.pi*(Ft-5000)*t) + np.sin(2*np.pi*(Ft+12000)*t)) + 0.5)

# frequency plot of input
plt.figure(1)
plot_freq("Input - freq", 50e6, data_bits, data_in[0:16384])
        
# init the model
#cic_rate = 125 # for ~48kHz output sample rate with proper I2S timing
#cic_rate = 136 # for ~44.1kHz output sample rate with proper I2S timing
cic_rate = 68 # for ~88.2kHz output sample rate with proper I2S timing
uut = ddc(data_bits, cic_rate)
uut.set_ftune(Ft/Fs)

# run the ddc
ddc_out = uut.calc(data_in)

# prepare to plot
data = ddc_out
rate = Fs/(uut.cic_i_inst.dec_rate*8)
data_len = len(data)
t = np.arange(data_len)/rate

# plot of time
fig = plt.figure(2)
plt.plot(t, np.real(data), label="real")
plt.plot(t, np.imag(data), label="imag")
plt.grid()
plt.xlabel("Time")
plt.ylabel("data")
plt.title("DDC Output - time")
plt.legend()

# plot of frequency
fig = plt.figure(3)
plot_freq("DDC Output - freq", rate, data_bits, data)
