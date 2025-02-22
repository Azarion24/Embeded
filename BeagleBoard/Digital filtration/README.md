# 🎶 Real-Time Signal Processing with FIR Filter

## 📌 Project Overview  
This project implements a **real-time FIR (Finite Impulse Response) filter** on the **BB AI** module, utilizing **interrupt-driven data acquisition**. It processes audio signals and applies a **custom FIR filter** for signal conditioning, useful for applications like noise reduction or frequency shaping.

## 🛠 Features  
✅ **Real-time FIR filtering**  
✅ **Finite Impulse Response (FIR) filter implementation**  
✅ **Windowed FIR filter coefficients**  
✅ **Audio processing using McASP interface**  
✅ **Interrupt-driven data handling**  
✅ **Round-off filtering for accurate signal transmission**  

## 📂 Project Structure  
- `codec/codec_v1_0.c` – Codec configuration for audio signal processing  
- `fft/DSPF_sp_cfftr2_dit.c` – Optimized FFT implementation (not used directly, but referenced for potential signal analysis)  
- `main.c` – Main source file handling interrupt processing and FIR filtering  
- `math.h` – Mathematical functions for signal processing  

## 📡 How It Works  
1. **Audio data** is captured from **McASP1** via an interrupt-driven process.  
2. The incoming samples are stored in a buffer and processed using a **FIR filter** with predefined coefficients.  
3. The filter applies weighted sums of the recent samples, controlled by the **filter coefficients** defined in the array `B[]`.  
4. The filtered sample is rounded to the nearest integer for transmission.  
5. The processed audio is sent back through **McASP1**.  

