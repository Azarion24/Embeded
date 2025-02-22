# 🎵 Real-Time Signal Processing on BB AI  

## 📌 Project Overview  
This project implements **real-time signal processing** using the **BB AI** module. It utilizes **Fast Fourier Transform (FFT)** with different windowing techniques to analyze and process audio signals.  

## 🛠 Features  
✅ **Real-time audio processing** using McASP  
✅ **Fourier Transform (FFT)** implementation  
✅ **Windowing techniques:**  
   - Rectangular  
   - Hanning  
   - Blackman-Harris  
✅ **Zero-padding for improved resolution**  
✅ **Interrupt-based data acquisition**  

## 📂 Project Structure  
- `codec/codec_v1_0.c` – Codec configuration for audio signal processing  
- `fft/utility.c` – Helper functions for FFT and bit-reversal  
- `fft/DSPF_sp_cfftr2_dit.c` – Optimized FFT implementation  
- `New_project.c` – Main source file handling signal processing  

## 📡 How It Works  
1. **Audio data** is acquired via **McASP1** and processed in an **interrupt-driven** manner.  
2. The incoming signal is stored in buffers and preprocessed with **windowing functions**.  
3. **FFT** is applied using the **DSPF_sp_cfftr2_dit()** function.  
4. The magnitude spectrum is computed and stored for analysis.  
5. Processed data is **transmitted back** via McASP1.  

## ⚙️ Dependencies  
- **TI SYS/BIOS** (Real-Time Operating System)  
- **TI DSP Library** (Optimized FFT functions)  
- **McASP Driver** (for audio streaming)  

## 📈 Future Improvements  
🔹 Implement **frequency-domain filtering**  
🔹 Add **machine learning** for real-time classification  
🔹 Improve **performance** using hardware acceleration  


