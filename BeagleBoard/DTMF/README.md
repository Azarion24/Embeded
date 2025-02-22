# 🎵 DTMF Signal Decoder with FFT and Blackman-Harris Window

## 📌 Project Overview  
This project implements a **real-time DTMF signal decoder** using the **Fast Fourier Transform (FFT)** and a **Blackman-Harris window** to accurately decode Dual-Tone Multi-Frequency (DTMF) signals. The system processes audio signals and decodes keypresses based on the frequency components, which are commonly used in telephone systems.

## 🛠 Features  
✅ **Real-time DTMF signal decoding**  
✅ **Fast Fourier Transform (FFT)** implementation  
✅ **Blackman-Harris window** for improved frequency resolution  
✅ **Harmonic filtering** to eliminate spurious tones  
✅ **Interrupt-driven audio data acquisition**  
✅ **DTMF keypad mapping** for signal decoding (1-9, *, 0, #, A-D)

## 📂 Project Structure  
- `codec/codec_v1_0.c` – Codec configuration for audio signal processing  
- `fft/utility.c` – Helper functions for FFT, bit-reversal, and windowing  
- `fft/DSPF_sp_cfftr2_dit.c` – Optimized FFT implementation  
- `main.c` – Main source file handling signal processing and interrupts  
- `math.h` – Mathematical functions for signal processing  

## 📡 How It Works  
1. **Audio data** is captured via **McASP1** and processed using **interrupts**.  
2. The incoming signal is preprocessed with the **Blackman-Harris window** to minimize spectral leakage.  
3. **FFT** is applied using the **DSPF_sp_cfftr2_dit()** function.  
4. The **frequency spectrum** is analyzed to identify dominant frequencies corresponding to DTMF tones.  
5. The detected tones are mapped to their corresponding **DTMF keypad characters**.  
6. Processed audio is transmitted back through **McASP1**.  


