# 🎵 Real-Time Signal Processing on BB AI  

## 📌 Project Overview  
This project implements **real-time signal processing** on the **BB AI** module, utilizing **Fast Fourier Transform (FFT)**, **Finite Impulse Response (FIR) filtering**, and **DTMF signal decoding**. The system enables real-time audio analysis, filtering, and decoding using optimized DSP algorithms and an **interrupt-driven** architecture.  

## 🛠 Features  
✅ **Real-time audio processing** using McASP  
✅ **Fast Fourier Transform (FFT) for frequency-domain analysis**  
✅ **Windowing techniques** to improve spectral resolution:  
   - Rectangular  
   - Hanning  
   - Blackman-Harris  
✅ **Finite Impulse Response (FIR) filtering** for signal conditioning  
✅ **DTMF signal decoding** using FFT and Blackman-Harris window  
✅ **Harmonic filtering** to eliminate unwanted tones  
✅ **Interrupt-based data acquisition for low-latency processing**  
✅ **DTMF keypad mapping** for recognizing keypresses (1-9, *, 0, #, A-D)  
✅ **Zero-padding for improved frequency resolution**  

## 📂 Project Structure  
- `codec/codec_v1_0.c` – Codec configuration for audio signal processing  
- `fft/utility.c` – Helper functions for FFT, bit-reversal, and windowing  
- `fft/DSPF_sp_cfftr2_dit.c` – Optimized FFT implementation  
- `main.c` – Core source file handling real-time processing (FIR filtering & DTMF decoding)  
- `math.h` – Mathematical functions for signal processing  

## 📡 How It Works  

### 🔹 Real-Time FIR Filtering  
1. **Audio data** is captured from **McASP1** via an interrupt-driven process.  
2. The incoming samples are stored in a buffer and processed using a **FIR filter** with predefined coefficients.  
3. The filter applies weighted sums of recent samples using the **filter coefficients** defined in `B[]`.  
4. The filtered sample is rounded and transmitted via **McASP1**.  

### 🔹 FFT-Based Signal Analysis  
1. **Audio data** is acquired and preprocessed with **windowing functions**.  
2. **FFT** is applied using the **DSPF_sp_cfftr2_dit()** function for spectral analysis.  
3. The magnitude spectrum is computed and stored for further processing.  
4. Processed data can be used for visualization or further filtering.  

### 🔹 DTMF Signal Decoding  
1. The incoming signal is preprocessed with a **Blackman-Harris window** to minimize spectral leakage.  
2. **FFT** is applied to extract the dominant frequencies.  
3. The detected frequencies are mapped to their corresponding **DTMF keypad characters**.  
4. The decoded keypresses are processed for further action.  

## ⚙️ Dependencies  
- **TI SYS/BIOS** (Real-Time Operating System)  
- **TI DSP Library** (Optimized FFT functions)  
- **McASP Driver** (for audio streaming)  

