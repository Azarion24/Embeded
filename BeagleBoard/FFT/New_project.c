/*
 * New_project.c
 *
 *  Created on: 2 paü 2024
 *      Author: Stud13
 */

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

int probka,i,q,k,j,Z,M;
short flag = 0;
short  n;
float x_disp[2*N_FFT];//po to zeby wyswietlal sie ladni sin
float x[2*N_FFT];
float x1[2*N_FFT];
float xBH[2*N_FFT];//dla Balckamana-Harrisa
float xBH_zeros[2*N_FFT];//dla Balckamana-Harrisa
float xFFT[2*N_FFT]; //Dla programu wbudwoanego
float w[N_FFT];
float wh[N_FFT]; //Tablica wspolczynnikow Hann
float wBH[N_FFT]; //Tablica wsp Blackamna-Harrisa
float x_ABS_Prostokat[N_FFT];
float x_ABS_Han[N_FFT];
float x_ABS_BH[4*N_FFT];
float x_ABS_BH_ZERO[4*N_FFT];
float x_zero_padded[4 * N_FFT];



//Okno Hanninga
void generate_hanning_window(float *w, int N){
    int n;
    for (n=0; n<N; n++){
        w[n] = 0.5*(1 - cos((2 * M_PI * n)/N));
    }
}

//Okno Blackmana-Harrisa
void generate_Blackmana_Harris_window(float *w, int N){
    int n;
    for (n=0; n<N; n++){
        w[n] = 0.35875 - 0.48829 * cos((2 * M_PI * n)/N) + 0.14128 * cos((4 * M_PI * n)/N) - 0.01168 * cos((6 * M_PI * n)/N);
    }
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

    x_disp[n] = (short)probka; // Dostajemy mlodsza czesc
    x_disp[n+1] = 0;// czesc urojona jest naprzemiennie z rzeczywista

    x[n] = (short)probka;
    x[n+1] = 0;

    x1[n] = (short)probka;
    x1[n+1] = 0;

    xBH[n] = (short)probka;
    xBH[n+1] = 0;

    xBH_zeros[n] = (short)probka;
    xBH_zeros[n+1] = 0;

    xFFT[n] = (short)probka;
    xFFT[n+1] = 0;

    n+=2;

    n &= Nmaska; //Zerowanie

    if (n == 0)
    {

        //Prostokatne
        DSPF_sp_cfftr2_dit(x, w, N_FFT);//robimy to tutaj bo od tego sie zaczyna tak na prawde (ze mamy wszystkie probki)
        bit_rev(x,N_FFT);

        //Hann
        int i;
        for (i=0; i < N_FFT; i++){
              x1[2*i] *= wh[i]; //mnozene probek przez wsp
              //x1[i+512] *= wh[i];
        }

        DSPF_sp_cfftr2_dit(x1,w,N_FFT);
        bit_rev(x1,N_FFT);

        //Blackman-Harris
        for (q = 0; q < N_FFT ;q++){
            xBH[2*q] *= wBH[q];
        }

        DSPF_sp_cfftr2_dit(xBH,w,N_FFT);
        bit_rev(xBH,N_FFT);

        //Zera i Blackamn-Harris
        for(j = 0; j < 4 * N_FFT; j++){ //zerowanie tablicy
                    x_zero_padded[j] = 0;
                }

        for(k = 0; k < 2*N_FFT; k++){
                    x_zero_padded[k] = xBH_zeros[k]; //powinno nastapic wpisanie do tablicy
                }

        for (q = 0; q < N_FFT ;q++){
            x_zero_padded[2*q] *= wBH[q]; //pomnozenie przez wsp BH
                }

        DSPF_sp_cfftr2_dit(x_zero_padded,w,N_FFT); //Czy to jest dobrze?
        bit_rev(x_zero_padded,N_FFT);

        for(Z = 0; Z < N_FFT; Z++){
            x_ABS_Prostokat[Z] = sqrt(x[2*Z]*x[2*Z] + x[2*Z+1] * x[2*Z+1]);
            x_ABS_Han[Z] = sqrt(x1[2*Z]*x1[2*Z] + x1[2*Z+1] * x1[2*Z+1]);
            x_ABS_BH[Z] = sqrt(xBH[2*Z]*xBH[2*Z] + xBH[2*Z+1] * xBH[2*Z+1]);
        }

        for(M = 0; M < 2*N_FFT;M++){
            x_ABS_BH_ZERO[M] = sqrt(x_zero_padded[2*M]*x_zero_padded[2*M] + x_zero_padded[2*M+1] * x_zero_padded[2*M+1]);
        }

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

    tw_genr2fft(w,N_FFT); //generacja wspÛlczynnikÛw, funkcja nazywa siÍ nieco inaczej niz w instrukcji (wsp kolowe)
    bit_rev(w,N_FFT>>1);

    generate_hanning_window(wh, N_FFT);//generacja wspolczynnikow do Hanninga

    generate_Blackmana_Harris_window(wBH, N_FFT); //generacja wspolczynniko do Blackamana Harrisa

    Config_and_start_mcasp1();  //these 3 lines keep together
    Hwi_enableInterrupt(13);

    while(1)
    {

    }
}

