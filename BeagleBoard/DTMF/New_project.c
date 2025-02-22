#include <stdio.h>
#include <ti/sysbios/BIOS.h> //for BIOS_start()
#include <xdc/cfg/global.h>  // for Hwi_enable_Interrupt(13)
#include "codec/codec_v1_0.c"
#include "fft/utility.h"
#include "fft/utility.c"
#include "fft/DSPF_sp_cfftr2_dit.h"
#include "fft/DSPF_sp_cfftr2_dit.c"
#include "math.h"

#define N_FFT 512
#define Nmaska 1023
#define FS 48000.0f //czestotliwosc probkowania

//kluczowe f DTMF
const float dtmf_frequencies[8] = {697, 770, 852, 941, 1209, 1336, 1477, 1633};
int dtmf_indices[8]; //indeksy dla czestotliwosci DTMF w widmie
int dtmf_indices_harmonic[8]; // Indeksy dla drugich harmonicznych


//dedykowany znak
char dtmf_result = ' ';

int probka,Z,q;
short flag = 0;
short  n=0; //przyrownalem do 0
float xBH[2*N_FFT];//dla Balckamana-Harrisa
float wBH[N_FFT]; //Tablica wsp Blackamna-Harrisa
float x_ABS_BH[4*N_FFT];//Czy ta 4 powinna tu byc?
float w[N_FFT];

//Okno Blackmana-Harrisa
void generate_Blackmana_Harris_window(float *w, int N){
    int n;
    for (n=0; n<N; n++){
        w[n] = 0.35875 - 0.48829 * cos((2 * M_PI * n)/N) + 0.14128 * cos((4 * M_PI * n)/N) - 0.01168 * cos((6 * M_PI * n)/N);
    }
}

//Obliczanie indeksow FFT dla czestotliwosci DTMF
void calculate_dtmf_indices() {
    int o;
    for (o = 0; o < 8; o++) {
        dtmf_indices[o] = (int)(dtmf_frequencies[o] * N_FFT / FS);        // Indeks podstawowy
        dtmf_indices_harmonic[o] = (int)(2 * dtmf_frequencies[o] * N_FFT / FS); // Indeks harmoniczny

        //Upewniamy sie, ze indeks harmoniczny miesci sie w tablicy FFT
        if (dtmf_indices_harmonic[o] >= N_FFT) {
            dtmf_indices_harmonic[o] = -1; // Oznaczamy jako niewazny
        }
        printf("Frequency: %.0f Hz, Index: %d, Harmonic Index: %d\n", dtmf_frequencies[o], dtmf_indices[o],dtmf_indices_harmonic[o]);
    }

}

//Dekodowanie sygnalu DTMF na podstawie widma amplitudowego
char decode_dtmf(const float* amplitude_spectrum) {
    int p,b;
    int low_index = -1, high_index = -1;
    float max_low = 0, max_high = 0;
    char result = ' ';

    const char dtmf_keys[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    // Znajdz dominujaca czestotliwosc w grupie niskiej
    for (p = 0; p < 4; p++) {
        if (amplitude_spectrum[dtmf_indices[p]] > max_low) {
            max_low = amplitude_spectrum[dtmf_indices[p]];
            low_index = p;
        }
    }

    // Znajdz dominujaca czestotliwosc w grupie wysokiej
    for (b = 4; b < 8; b++) {
        if (amplitude_spectrum[dtmf_indices[b]] > max_high) {
            max_high = amplitude_spectrum[dtmf_indices[b]];
            high_index = b - 4;
        }
    }

    // Sprawdzenie drugich harmonicznych
    if (low_index != -1 && dtmf_indices_harmonic[low_index] != -1) {
        int harmonic_index = dtmf_indices_harmonic[low_index];
        if (amplitude_spectrum[harmonic_index] > 0.7 * max_low) {
            return ' '; // Blad zbyt wysoka druga harmoniczna
        }
    }

    if (high_index != -1 && dtmf_indices_harmonic[high_index + 4] != -1) {
        int harmonic_index = dtmf_indices_harmonic[high_index + 4];
        if (amplitude_spectrum[harmonic_index] > 0.2 * max_high) {
            return ' '; // Blad zbyt wysoka druga harmoniczna
        }
    }

    // Mapowanie na klawisze
    if (low_index != -1 && high_index != -1) {
        result = dtmf_keys[low_index][high_index];
    }

    return result;
}


int main(void)
{
    Pins_config();
    //printf("Start programu New_projecet\n");
    BIOS_start();
    return 0;
}

void przerwanie_rcv()
{
    probka = Read_mcasp1_rcv();

    xBH[n] = (short)probka;
    xBH[n+1] = 0;

    n+=2;

    n &= Nmaska; //Zerowanie

    if (n == 0)
    {

        //Blackman-Harris
        for (q = 0; q < N_FFT ;q++){
            xBH[2*q] *= wBH[q];
        }

       DSPF_sp_cfftr2_dit(xBH,w,N_FFT);
       bit_rev(xBH,N_FFT);

        for(Z = 0; Z < N_FFT; Z++){
            x_ABS_BH[Z] = sqrt(xBH[2*Z]*xBH[2*Z] + xBH[2*Z+1] * xBH[2*Z+1]);
        }

        dtmf_result = decode_dtmf(x_ABS_BH);

        flag = 1;
    }

    Write_mcasp1_xmt(probka);
    Restart_mcasp1_if_error(); //It must be the last line of INT

    if((n==4) && (flag==1)){
        n=0;
        flag = 0;
    }
}

void pierwszy_task(UArg arg0, UArg arg1)
{
    //printf("Start pierwszego zadania\n");
    Config_i2c_and_codec();
    n = 0;
    calculate_dtmf_indices();
    
    tw_genr2fft(w,N_FFT); //generacja wspolczynnikow, funkcja nazywa sie nieco inaczej niz w instrukcji (wsp kolowe)
    bit_rev(w,N_FFT>>1);

    generate_Blackmana_Harris_window(wBH, N_FFT); //generacja wspolczynniko do Blackamana Harrisa
    
    Config_and_start_mcasp1();  //these 3 lines keep together
    Hwi_enableInterrupt(13);

    while(1)
    {

    }
}

