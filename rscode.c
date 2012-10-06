/*
 * qrencode - QR Code encoder
 *
 * Reed solomon encoder. This code is taken from Phil Karn's libfec then
 * editted and packed into a pair of .c and .h files.
 *
 * Copyright (C) 2002, 2003, 2004, 2006 Phil Karn, KA9Q
 * (libfec is released under the GNU Lesser General Public License.)
 *
 * Copyright (C) 2006-2011 Kentaro Fukuchi <kentaro@fukuchi.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdlib.h>
#include <string.h>

#include "rscode.h"

/* Stuff specific to the 8-bit symbol version of the general purpose RS codecs
 *
 */
typedef unsigned char data_t;


/**
 * Reed-Solomon codec control block
 */

#define MAX_SYMSIZE 8
#define MAX_NROOTS 30
#define MAX_ALPHA_TO (1<<MAX_SYMSIZE)
#define MAX_INDEX_OF (1<<MAX_SYMSIZE)
#define MAX_GENPOLY  (MAX_NROOTS + 1)

struct _RS {
	int mm;         /* Bits per symbol */
	int nn;         /* Symbols per block (= (1<<mm)-1) */
	data_t alpha_to[MAX_ALPHA_TO];     /* log lookup table */
	data_t index_of[MAX_INDEX_OF];     /* Antilog lookup table */
	data_t genpoly[MAX_GENPOLY];       /* Generator polynomial */
	int nroots;     /* Number of generator roots = number of parity symbols */
	int fcr;        /* First consecutive root, index form */
	int prim;       /* Primitive element, index form */
	int iprim;      /* prim-th root of 1, index form */
	int pad;        /* Padding bytes in shortened block */
	int gfpoly;
};

static struct _RS srs; /* 'srs' stands for static-RS */

static inline int modnn(int x){
	while (x >= srs.nn) {
		x -= srs.nn;
		x = (x >> srs.mm) + (x & srs.nn);
	}
	return x;
}


#define MODNN(x) modnn(x)

#define MM (srs.mm)
#define NN (srs.nn)
#define ALPHA_TO (srs.alpha_to) 
#define INDEX_OF (srs.index_of)
#define GENPOLY (srs.genpoly)
#define NROOTS (srs.nroots)
#define FCR (srs.fcr)
#define PRIM (srs.prim)
#define IPRIM (srs.iprim)
#define PAD (srs.pad)
#define A0 (NN)


/* Initialize a Reed-Solomon codec
 * symsize = symbol size, bits
 * gfpoly = Field generator polynomial coefficients
 * fcr = first root of RS code generator polynomial, index form
 * prim = primitive element to generate polynomial roots
 * nroots = RS code generator polynomial degree (number of roots)
 * pad = padding bytes at front of shortened block
 */
static int init_rs_char(int symsize, int gfpoly, int fcr, int prim, int nroots, int pad)
{
/* Common code for intializing a Reed-Solomon control block (char or int symbols)
 * Copyright 2004 Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
//#undef NULL
//#define NULL ((void *)0)

  int i, j, sr,root,iprim;

  /* Check parameter ranges */
  if(symsize < 0 || symsize > (int)(8*sizeof(data_t))){
	  return -1;
  }

  if(fcr < 0 || fcr >= (1<<symsize))
	  return -1;
  if(prim <= 0 || prim >= (1<<symsize))
	  return -1;
  if(nroots < 0 || nroots >= (1<<symsize))
		return -1; /* Can't have more roots than symbol values! */
  if(pad < 0 || pad >= ((1<<symsize) -1 - nroots))
		return -1; /* Too much padding */

  srs.mm = symsize;
  srs.nn = (1<<symsize)-1;
  srs.pad = pad;

  /* Generate Galois field lookup tables */
  srs.index_of[0] = A0; /* log(zero) = -inf */
  srs.alpha_to[A0] = 0; /* alpha**-inf = 0 */
  sr = 1;
  for(i=0;i<srs.nn;i++){
    srs.index_of[sr] = i;
    srs.alpha_to[i] = sr;
    sr <<= 1;
    if(sr & (1<<symsize))
      sr ^= gfpoly;
    sr &= srs.nn;
  }
  if(sr != 1){
    /* field generator polynomial is not primitive! */
	  return -1;
  }

  /* Form RS code generator polynomial from its roots */
  srs.fcr = fcr;
  srs.prim = prim;
  srs.nroots = nroots;
  srs.gfpoly = gfpoly;

  /* Find prim-th root of 1, used in decoding */
  for(iprim=1;(iprim % prim) != 0;iprim += srs.nn)
    ;
  srs.iprim = iprim / prim;

  srs.genpoly[0] = 1;
  for (i = 0,root=fcr*prim; i < nroots; i++,root += prim) {
    srs.genpoly[i+1] = 1;

    /* Multiply rs->genpoly[] by  @**(root + x) */
    for (j = i; j > 0; j--){
      if (srs.genpoly[j] != 0)
	srs.genpoly[j] = srs.genpoly[j-1] ^ srs.alpha_to[modnn(srs.index_of[srs.genpoly[j]] + root)];
      else
	srs.genpoly[j] = srs.genpoly[j-1];
    }
    /* rs->genpoly[0] can never be zero */
    srs.genpoly[0] = srs.alpha_to[modnn(srs.index_of[srs.genpoly[0]] + root)];
  }
  /* convert rs->genpoly[] to index form for quicker encoding */
  for (i = 0; i <= nroots; i++)
    srs.genpoly[i] = srs.index_of[srs.genpoly[i]];
  return 0;
}

int init_rs(int symsize, int gfpoly, int fcr, int prim, int nroots, int pad)
{
	return init_rs_char(symsize, gfpoly, fcr, prim, nroots, pad);
}

/* The guts of the Reed-Solomon encoder, meant to be #included
 * into a function body with the following typedefs, macros and variables supplied
 * according to the code parameters:

 * data_t - a typedef for the data symbol
 * data_t data[] - array of NN-NROOTS-PAD and type data_t to be encoded
 * data_t parity[] - an array of NROOTS and type data_t to be written with parity symbols
 * NROOTS - the number of roots in the RS code generator polynomial,
 *          which is the same as the number of parity symbols in a block.
            Integer variable or literal.
	    * 
 * NN - the total number of symbols in a RS block. Integer variable or literal.
 * PAD - the number of pad symbols in a block. Integer variable or literal.
 * ALPHA_TO - The address of an array of NN elements to convert Galois field
 *            elements in index (log) form to polynomial form. Read only.
 * INDEX_OF - The address of an array of NN elements to convert Galois field
 *            elements in polynomial form to index (log) form. Read only.
 * MODNN - a function to reduce its argument modulo NN. May be inline or a macro.
 * GENPOLY - an array of NROOTS+1 elements containing the generator polynomial in index form

 * The memset() and memmove() functions are used. The appropriate header
 * file declaring these functions (usually <string.h>) must be included by the calling
 * program.

 * Copyright 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */

#undef A0
#define A0 (NN) /* Special reserved value encoding zero in index form */

void encode_rs_char(const data_t *data, data_t *parity)
{
  int i, j;
  data_t feedback;

  memset(parity,0,NROOTS*sizeof(data_t));

  for(i=0;i<NN-NROOTS-PAD;i++){
    feedback = INDEX_OF[data[i] ^ parity[0]];
    if(feedback != A0){      /* feedback term is non-zero */
#ifdef UNNORMALIZED
      /* This line is unnecessary when GENPOLY[NROOTS] is unity, as it must
       * always be for the polynomials constructed by init_rs()
       */
      feedback = MODNN(NN - GENPOLY[NROOTS] + feedback);
#endif
      for(j=1;j<NROOTS;j++)
	parity[j] ^= ALPHA_TO[MODNN(feedback + GENPOLY[NROOTS-j])];
    }
    /* Shift */
    memmove(&parity[0],&parity[1],sizeof(data_t)*(NROOTS-1));
    if(feedback != A0)
      parity[NROOTS-1] = ALPHA_TO[MODNN(feedback + GENPOLY[0])];
    else
      parity[NROOTS-1] = 0;
  }
}
