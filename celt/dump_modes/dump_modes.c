/* Copyright (c) 2008 CSIRO
   Copyright (c) 2008-2009 Xiph.Org Foundation
   Written by Jean-Marc Valin */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include "modes.h"
#include "celt.h"
#include "rate.h"
#include "dump_modes_arch.h"

#define INT16 "%d"
#define INT32 "%d"
#define FLOAT "%#0.8gf"

#ifdef FIXED_POINT
#define WORD16 INT16
#define WORD32 INT32
#else
#define WORD16 FLOAT
#define WORD32 FLOAT
#endif

#define COEF16(x, a) ((opus_int16)SATURATE(((opus_int64)(x)+(1<<(a)>>1))>>(a), 32767))
int opus_select_arch(void) {
   return 0;
}

void dump_modes(FILE *file, CELTMode **modes, int nb_modes)
{
   int i, j, k;
   int mdct_twiddles_size;
   fprintf(file, "/* The contents of this file was automatically generated by dump_modes.c\n");
   fprintf(file, "   with arguments:");
   for (i=0;i<nb_modes;i++)
   {
      CELTMode *mode = modes[i];
      fprintf(file, " %d %d",mode->Fs,mode->shortMdctSize*mode->nbShortMdcts);
   }
   fprintf(file, "\n   It contains static definitions for some pre-defined modes. */\n");
   fprintf(file, "#include \"modes.h\"\n");
   fprintf(file, "#include \"rate.h\"\n");
   fprintf(file, "\n#ifdef HAVE_ARM_NE10\n");
   fprintf(file, "#define OVERRIDE_FFT 1\n");
   fprintf(file, "#include \"%s\"\n", ARM_NE10_ARCH_FILE_NAME);
   fprintf(file, "#endif\n");

   fprintf(file, "\n");

   for (i=0;i<nb_modes;i++)
   {
      CELTMode *mode = modes[i];
      int mdctSize;
      int standard, framerate;

      mdctSize = mode->shortMdctSize*mode->nbShortMdcts;
      standard = (mode->Fs == 400*(opus_int32)mode->shortMdctSize);
      framerate = mode->Fs/mode->shortMdctSize;

      if (!standard)
      {
         fprintf(file, "#ifndef DEF_EBANDS%d_%d\n", mode->Fs, mdctSize);
         fprintf(file, "#define DEF_EBANDS%d_%d\n", mode->Fs, mdctSize);
         fprintf (file, "static const opus_int16 eBands%d_%d[%d] = {\n", mode->Fs, mdctSize, mode->nbEBands+2);
         for (j=0;j<mode->nbEBands+2;j++)
            fprintf (file, "%d, ", mode->eBands[j]);
         fprintf (file, "};\n");
         fprintf(file, "#endif\n");
         fprintf(file, "\n");
      }

      fprintf(file, "#ifndef DEF_WINDOW%d\n", mode->overlap);
      fprintf(file, "#define DEF_WINDOW%d\n", mode->overlap);
      fprintf (file, "static const celt_coef window%d[%d] = {\n", mode->overlap, mode->overlap);
#if defined(FIXED_POINT) && defined(ENABLE_QEXT)
      fprintf(file, "#ifdef ENABLE_QEXT\n");
      for (j=0;j<mode->overlap;j++)
         fprintf (file, WORD32 ",%c", mode->window[j],(j+6)%5==0?'\n':' ');
      fprintf(file, "#else\n");
      for (j=0;j<mode->overlap;j++)
         fprintf (file, WORD16 ",%c", COEF16(mode->window[j], 16),(j+6)%5==0?'\n':' ');
      fprintf(file, "#endif\n");
#else
      for (j=0;j<mode->overlap;j++)
         fprintf (file, WORD16 ",%c", mode->window[j],(j+6)%5==0?'\n':' ');
#endif
      fprintf (file, "};\n");
      fprintf(file, "#endif\n");
      fprintf(file, "\n");

      if (!standard)
      {
         fprintf(file, "#ifndef DEF_ALLOC_VECTORS%d_%d\n", mode->Fs, mdctSize);
         fprintf(file, "#define DEF_ALLOC_VECTORS%d_%d\n", mode->Fs, mdctSize);
         fprintf (file, "static const unsigned char allocVectors%d_%d[%d] = {\n", mode->Fs, mdctSize, mode->nbEBands*mode->nbAllocVectors);
         for (j=0;j<mode->nbAllocVectors;j++)
         {
            for (k=0;k<mode->nbEBands;k++)
               fprintf (file, "%2d, ", mode->allocVectors[j*mode->nbEBands+k]);
            fprintf (file, "\n");
         }
         fprintf (file, "};\n");
         fprintf(file, "#endif\n");
         fprintf(file, "\n");
      }

      fprintf(file, "#ifndef DEF_LOGN%d\n", framerate);
      fprintf(file, "#define DEF_LOGN%d\n", framerate);
      fprintf (file, "static const opus_int16 logN%d[%d] = {\n", framerate, mode->nbEBands);
      for (j=0;j<mode->nbEBands;j++)
         fprintf (file, "%d, ", mode->logN[j]);
      fprintf (file, "};\n");
      fprintf(file, "#endif\n");
      fprintf(file, "\n");

      /* Pulse cache */
      fprintf(file, "#ifndef DEF_PULSE_CACHE%d\n", mode->Fs/mdctSize);
      fprintf(file, "#define DEF_PULSE_CACHE%d\n", mode->Fs/mdctSize);
      fprintf (file, "static const opus_int16 cache_index%d[%d] = {\n", mode->Fs/mdctSize, (mode->maxLM+2)*mode->nbEBands);
      for (j=0;j<mode->nbEBands*(mode->maxLM+2);j++)
         fprintf (file, "%d,%c", mode->cache.index[j],(j+16)%15==0?'\n':' ');
      fprintf (file, "};\n");
      fprintf (file, "static const unsigned char cache_bits%d[%d] = {\n", mode->Fs/mdctSize, mode->cache.size);
      for (j=0;j<mode->cache.size;j++)
         fprintf (file, "%d,%c", mode->cache.bits[j],(j+16)%15==0?'\n':' ');
      fprintf (file, "};\n");
      fprintf (file, "static const unsigned char cache_caps%d[%d] = {\n", mode->Fs/mdctSize, (mode->maxLM+1)*2*mode->nbEBands);
      for (j=0;j<(mode->maxLM+1)*2*mode->nbEBands;j++)
         fprintf (file, "%d,%c", mode->cache.caps[j],(j+16)%15==0?'\n':' ');
      fprintf (file, "};\n");

      fprintf(file, "#endif\n");
      fprintf(file, "\n");

      /* FFT twiddles */
      fprintf(file, "#ifndef FFT_TWIDDLES%d_%d\n", mode->Fs, mdctSize);
      fprintf(file, "#define FFT_TWIDDLES%d_%d\n", mode->Fs, mdctSize);

      fprintf (file, "static const kiss_twiddle_cpx fft_twiddles%d_%d[%d] = {\n",
            mode->Fs, mdctSize, mode->mdct.kfft[0]->nfft);
#if defined(FIXED_POINT) && defined(ENABLE_QEXT)
      fprintf(file, "#ifdef ENABLE_QEXT\n");
      for (j=0;j<mode->mdct.kfft[0]->nfft;j++)
         fprintf (file, "{" WORD32 ", " WORD32 "},%c", mode->mdct.kfft[0]->twiddles[j].r, mode->mdct.kfft[0]->twiddles[j].i,(j+3)%2==0?'\n':' ');
      fprintf(file, "#else\n");
      for (j=0;j<mode->mdct.kfft[0]->nfft;j++)
         fprintf (file, "{" WORD16 ", " WORD16 "},%c", COEF16(mode->mdct.kfft[0]->twiddles[j].r,16), COEF16(mode->mdct.kfft[0]->twiddles[j].i,16),(j+3)%2==0?'\n':' ');
      fprintf(file, "#endif\n");
#else
      for (j=0;j<mode->mdct.kfft[0]->nfft;j++)
         fprintf (file, "{" WORD16 ", " WORD16 "},%c", mode->mdct.kfft[0]->twiddles[j].r, mode->mdct.kfft[0]->twiddles[j].i,(j+3)%2==0?'\n':' ');
#endif
      fprintf (file, "};\n");

#ifdef OVERRIDE_FFT
      dump_mode_arch(mode);
#endif
      /* FFT Bitrev tables */
      for (k=0;k<=mode->mdct.maxshift;k++)
      {
         fprintf(file, "#ifndef FFT_BITREV%d\n", mode->mdct.kfft[k]->nfft);
         fprintf(file, "#define FFT_BITREV%d\n", mode->mdct.kfft[k]->nfft);
         fprintf (file, "static const opus_int16 fft_bitrev%d[%d] = {\n",
               mode->mdct.kfft[k]->nfft, mode->mdct.kfft[k]->nfft);
         for (j=0;j<mode->mdct.kfft[k]->nfft;j++)
            fprintf (file, "%d,%c", mode->mdct.kfft[k]->bitrev[j],(j+16)%15==0?'\n':' ');
         fprintf (file, "};\n");

         fprintf(file, "#endif\n");
         fprintf(file, "\n");
      }

      /* FFT States */
      for (k=0;k<=mode->mdct.maxshift;k++)
      {
         fprintf(file, "#ifndef FFT_STATE%d_%d_%d\n", mode->Fs, mdctSize, k);
         fprintf(file, "#define FFT_STATE%d_%d_%d\n", mode->Fs, mdctSize, k);
         fprintf (file, "static const kiss_fft_state fft_state%d_%d_%d = {\n",
               mode->Fs, mdctSize, k);
         fprintf (file, "%d,    /* nfft */\n", mode->mdct.kfft[k]->nfft);

#if defined(FIXED_POINT) && defined(ENABLE_QEXT)
         fprintf(file, "#ifdef ENABLE_QEXT\n");
         fprintf (file, WORD32 ",    /* scale */\n", mode->mdct.kfft[k]->scale);
         fprintf(file, "#else\n");
         fprintf (file, WORD16 ",    /* scale */\n", COEF16(mode->mdct.kfft[k]->scale, 15));
         fprintf(file, "#endif\n");
#else
         fprintf (file, WORD16 ",    /* scale */\n", mode->mdct.kfft[k]->scale);
#endif
#ifdef FIXED_POINT
         fprintf (file, "%d,    /* scale_shift */\n", mode->mdct.kfft[k]->scale_shift);
#endif
         fprintf (file, "%d,    /* shift */\n", mode->mdct.kfft[k]->shift);
         fprintf (file, "{");
         for (j=0;j<2*MAXFACTORS;j++)
            fprintf (file, "%d, ", mode->mdct.kfft[k]->factors[j]);
         fprintf (file, "},    /* factors */\n");
         fprintf (file, "fft_bitrev%d,    /* bitrev */\n", mode->mdct.kfft[k]->nfft);
         fprintf (file, "fft_twiddles%d_%d,    /* bitrev */\n", mode->Fs, mdctSize);

         fprintf (file, "#ifdef OVERRIDE_FFT\n");
         fprintf (file, "(arch_fft_state *)&cfg_arch_%d,\n", mode->mdct.kfft[k]->nfft);
         fprintf (file, "#else\n");
         fprintf (file, "NULL,\n");
         fprintf(file, "#endif\n");

         fprintf (file, "};\n");

         fprintf(file, "#endif\n");
         fprintf(file, "\n");
      }

      fprintf(file, "#endif\n");
      fprintf(file, "\n");

      /* MDCT twiddles */
      mdct_twiddles_size = mode->mdct.n-(mode->mdct.n/2>>mode->mdct.maxshift);
      fprintf(file, "#ifndef MDCT_TWIDDLES%d\n", mdctSize);
      fprintf(file, "#define MDCT_TWIDDLES%d\n", mdctSize);
      fprintf (file, "static const celt_coef mdct_twiddles%d[%d] = {\n",
            mdctSize, mdct_twiddles_size);

#if defined(FIXED_POINT) && defined(ENABLE_QEXT)
      fprintf(file, "#ifdef ENABLE_QEXT\n");
      for (j=0;j<mdct_twiddles_size;j++)
         fprintf (file, WORD32 ",%c", mode->mdct.trig[j],(j+6)%5==0?'\n':' ');
      fprintf(file, "#else\n");
      for (j=0;j<mdct_twiddles_size;j++)
         fprintf (file, WORD16 ",%c", COEF16(mode->mdct.trig[j], 16),(j+6)%5==0?'\n':' ');
      fprintf(file, "#endif\n");
#else
      for (j=0;j<mdct_twiddles_size;j++)
         fprintf (file, WORD16 ",%c", mode->mdct.trig[j],(j+6)%5==0?'\n':' ');
#endif

      fprintf (file, "};\n");

      fprintf(file, "#endif\n");
      fprintf(file, "\n");


      /* Print the actual mode data */
      fprintf(file, "static const CELTMode mode%d_%d_%d = {\n", mode->Fs, mdctSize, mode->overlap);
      fprintf(file, INT32 ",    /* Fs */\n", mode->Fs);
      fprintf(file, "%d,    /* overlap */\n", mode->overlap);
      fprintf(file, "%d,    /* nbEBands */\n", mode->nbEBands);
      fprintf(file, "%d,    /* effEBands */\n", mode->effEBands);
      fprintf(file, "{");
      for (j=0;j<4;j++)
         fprintf(file, WORD16 ", ", mode->preemph[j]);
      fprintf(file, "},    /* preemph */\n");
      if (standard)
         fprintf(file, "eband5ms,    /* eBands */\n");
      else
         fprintf(file, "eBands%d_%d,    /* eBands */\n", mode->Fs, mdctSize);

      fprintf(file, "%d,    /* maxLM */\n", mode->maxLM);
      fprintf(file, "%d,    /* nbShortMdcts */\n", mode->nbShortMdcts);
      fprintf(file, "%d,    /* shortMdctSize */\n", mode->shortMdctSize);

      fprintf(file, "%d,    /* nbAllocVectors */\n", mode->nbAllocVectors);
      if (standard)
         fprintf(file, "band_allocation,    /* allocVectors */\n");
      else
         fprintf(file, "allocVectors%d_%d,    /* allocVectors */\n", mode->Fs, mdctSize);

      fprintf(file, "logN%d,    /* logN */\n", framerate);
      fprintf(file, "window%d,    /* window */\n", mode->overlap);
      fprintf(file, "{%d, %d, {", mode->mdct.n, mode->mdct.maxshift);
      for (k=0;k<=mode->mdct.maxshift;k++)
         fprintf(file, "&fft_state%d_%d_%d, ", mode->Fs, mdctSize, k);
      fprintf (file, "}, mdct_twiddles%d},    /* mdct */\n", mdctSize);

      fprintf(file, "{%d, cache_index%d, cache_bits%d, cache_caps%d},    /* cache */\n",
            mode->cache.size, mode->Fs/mdctSize, mode->Fs/mdctSize, mode->Fs/mdctSize);
      fprintf(file, "};\n");
   }
   fprintf(file, "\n");
   fprintf(file, "/* List of all the available modes */\n");
   fprintf(file, "#define TOTAL_MODES %d\n", nb_modes);
   fprintf(file, "static const CELTMode * const static_mode_list[TOTAL_MODES] = {\n");
   for (i=0;i<nb_modes;i++)
   {
      CELTMode *mode = modes[i];
      int mdctSize;
      mdctSize = mode->shortMdctSize*mode->nbShortMdcts;
      fprintf(file, "&mode%d_%d_%d,\n", mode->Fs, mdctSize, mode->overlap);
   }
   fprintf(file, "};\n");
}

void dump_header(FILE *file, CELTMode **modes, int nb_modes)
{
   int i;
   int channels = 0;
   int frame_size = 0;
   int overlap = 0;
   fprintf (file, "/* This header file is generated automatically*/\n");
   for (i=0;i<nb_modes;i++)
   {
      CELTMode *mode = modes[i];
      if (frame_size==0)
         frame_size = mode->shortMdctSize*mode->nbShortMdcts;
      else if (frame_size != mode->shortMdctSize*mode->nbShortMdcts)
         frame_size = -1;
      if (overlap==0)
         overlap = mode->overlap;
      else if (overlap != mode->overlap)
         overlap = -1;
   }
   if (channels>0)
   {
      fprintf (file, "#define CHANNELS(mode) %d\n", channels);
      if (channels==1)
         fprintf (file, "#define DISABLE_STEREO\n");
   }
   if (frame_size>0)
   {
      fprintf (file, "#define FRAMESIZE(mode) %d\n", frame_size);
   }
   if (overlap>0)
   {
      fprintf (file, "#define OVERLAP(mode) %d\n", overlap);
   }
}

#ifdef FIXED_POINT
#define BASENAME "static_modes_fixed"
#else
#define BASENAME "static_modes_float"
#endif

int main(int argc, char **argv)
{
   int i, nb;
   FILE *file;
   CELTMode **m;
   if (argc%2 != 1 || argc<3)
   {
      fprintf (stderr, "Usage: %s rate frame_size [rate frame_size] [rate frame_size]...\n",argv[0]);
      return 1;
   }
   nb = (argc-1)/2;
   m = malloc(nb*sizeof(CELTMode*));
   for (i=0;i<nb;i++)
   {
      int Fs, frame;
      Fs      = atoi(argv[2*i+1]);
      frame   = atoi(argv[2*i+2]);
      m[i] = opus_custom_mode_create(Fs, frame, NULL);
      if (m[i]==NULL)
      {
         fprintf(stderr,"Error creating mode with Fs=%s, frame_size=%s\n",
               argv[2*i+1],argv[2*i+2]);
         return EXIT_FAILURE;
      }
   }
   file = fopen(BASENAME ".h", "w");
#ifdef OVERRIDE_FFT
   dump_modes_arch_init(m, nb);
#endif
   dump_modes(file, m, nb);
   fclose(file);
#ifdef OVERRIDE_FFT
   dump_modes_arch_finalize();
#endif
   for (i=0;i<nb;i++)
      opus_custom_mode_destroy(m[i]);
   free(m);
   return 0;
}
