/*
------------------------------------------------------------------------------
          Licensing information can be found at the end of the file.
------------------------------------------------------------------------------

speech.hpp - v0.1 - Very basic text-to-speech synthesizer library for C++.

Do this:
    #define SPEECH_IMPLEMENTATION
before you include this file in *one* C++ file to create the implementation.

------------------------------------------------------------------------------

This file contains modified code based on the TTS code from the SoLoud audio
engine (http://sol.gfxile.net/soloud/), which in turn is based on rsynth by 
Nick Ing-Simmons (et al). I have not included any code which were under the 
SoLoud license (ZLib/LibPNG). I've basically just merged the code into a 
single header library, and put my own interface on top of it instead of the 
original SoLoud interface.

The original license text can be found at the end of this file, and I place my
changes under the same license.

                        / Mattias Gustavsson ( mattias@mattiasgustavsson.com )
*/

#ifndef speech_hpp
#define speech_hpp

short* speech_gen( int* samples_pairs_generated, char const* str, void* memctx );
void speech_free( float* sample_pairs, void* memctx );

#endif /* speech_hpp */

/*
----------------------
    IMPLEMENTATION
----------------------
*/

#ifdef SPEECH_IMPLEMENTATION
#undef SPEECH_IMPLEMENTATION

#pragma warning( push )
#pragma warning( disable: 4619 ) // pragma warning : there is no warning number 'number'
#pragma warning( disable: 4127 ) // conditional expression is constant
#pragma warning( disable: 4365 ) // 'action' conversion from 'type_1' to 'type_2', signed/unsigned mismatch
#pragma warning( disable: 4456 ) // declaration hides previous local declaration
#pragma warning( disable: 4548 ) // expression before comma has no effect; expected expression with side-effect
#pragma warning( disable: 4623 ) // default constructor was implicitly defined as deleted
#pragma warning( disable: 4626 ) // assignment operator was implicitly defined as deleted
#pragma warning( disable: 4706 ) // assignment within conditional expression
#pragma warning( disable: 4510 ) // default constructor could not be generated
#pragma warning( disable: 4512 ) // assignment operator could not be generated
#pragma warning( disable: 4616 ) // warning number '5027' out of range, must be between '4001' and '4999'
#pragma warning( disable: 4610 ) // class can never be instantiated - user defined constructor required
#pragma warning( disable: 5027 ) // move assignment operator was implicitly defined as deleted

#ifndef SPEECH_MALLOC
    #define _CRT_NONSTDC_NO_DEPRECATE 
    #define _CRT_SECURE_NO_WARNINGS
    #include <stdlib.h>
    #if defined(__cplusplus)
        #define SPEECH_MALLOC( ctx, size ) ( ::malloc( size ) )
        #define SPEECH_FREE( ctx, ptr ) ( ::free( ptr ) )
    #else
        #define SPEECH_MALLOC( ctx, size ) ( malloc( size ) )
        #define SPEECH_FREE( ctx, ptr ) ( free( ptr ) )
    #endif
#endif


#define _CRT_NONSTDC_NO_DEPRECATE 
#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>


// TODO: convert implementation to C

namespace internal { 


class darray
{
protected:
    void* memctx;
    char *mData;
    int mUsed;
    int mAllocated;
    int mAllocChunk;
    char mem[ 1024 ];
public:
    darray( void* memctx );
    ~darray();
    void clear();
    char *getDataInPos(int aPosition);
    void put(int aData);
    int getSize() const { return mUsed; }
    char *getData() { return mData; } 
};


darray::darray( void* memctx_ ):
    memctx( memctx_ )
{
    mAllocChunk = 1024;
    mUsed = 0;
    mAllocated = sizeof( mem );
    mData = mem;
}

void darray::clear()
{
    if( mData != mem ) SPEECH_FREE( memctx, mData );
    mAllocChunk = 1024;
    mUsed = 0;
    mAllocated = sizeof( mem );
    mData = mem;
}

darray::~darray()
{
    clear();
}

char * darray::getDataInPos(int aPosition)
{
    if (aPosition < mAllocated && aPosition < mUsed)
        return mData + aPosition;

    if (aPosition >= mAllocated)
    {
        int newsize = mAllocated;

        while (newsize <= aPosition)
        {
            newsize += mAllocChunk;
            mAllocChunk *= 2;
        }

        char *newdata = (char*)SPEECH_MALLOC( memctx, newsize);
        if (!newdata)
        {
            if( mData != mem ) SPEECH_FREE( memctx, mData);
            mData = NULL;
            mAllocated = mUsed = 0;
            return NULL;
        }
        memcpy( newdata, mData, mAllocated );
        if( mData != mem ) SPEECH_FREE( memctx, mData );
        mData = newdata;
        mAllocated = newsize;           
    }

    if (aPosition >= mUsed)
    {
        mUsed = aPosition + 1;
    }

    return mData + aPosition;
}

void darray::put(int aData)
{
    char *s = getDataInPos(mUsed);

    *s = (char) aData;
}




class resonator
{
    float mA, mB, mC, mP1, mP2;
public:

    /* Convert formant freqencies and bandwidth into resonator difference equation coefficents
     */
    void initResonator(
        int aFrequency,                       /* Frequency of resonator in Hz  */
        int aBandwidth,                      /* Bandwidth of resonator in Hz  */
        int aSamplerate);

    /* Convert formant freqencies and bandwidth into anti-resonator difference equation constants
     */
    void initAntiresonator(
        int aFrequency,                       /* Frequency of resonator in Hz  */
        int aBandwidth,                      /* Bandwidth of resonator in Hz  */
        int aSamplerate);

    /* Set gain */
    void setGain(float aG);

    /* Generic resonator function */
    float resonate(float input);

    /* Generic anti-resonator function
       Same as resonator except that a,b,c need to be set with initAntiresonator()
       and we save inputs in p1/p2 rather than outputs.
       There is currently only one of these - "mNasalZero"
    
       Output = (mNasalZero.a * input) + (mNasalZero.b * oldin1) + (mNasalZero.c * oldin2) 
     */

    float antiresonate(float input);

    resonator();

    ~resonator();
};



#ifndef PI
#define PI 3.1415926535897932384626433832795f
#endif

/* Convert formant freqencies and bandwidth into resonator difference equation coefficents
    */
void resonator::initResonator(
    int aFrequency,                       /* Frequency of resonator in Hz  */
    int aBandwidth,                      /* Bandwidth of resonator in Hz  */
    int aSamplerate)
{
    float arg = (-PI / aSamplerate) * aBandwidth;
    float r = (float)exp(arg);  
    mC = -(r * r);             
    arg = (-2.0f * PI / aSamplerate) * aFrequency;
    mB = r * (float)cos(arg) * 2.0f;   
    mA = 1.0f - mB - mC;    
}

/* Convert formant freqencies and bandwidth into anti-resonator difference equation constants
    */
void resonator::initAntiresonator(
    int aFrequency,                       /* Frequency of resonator in Hz  */
    int aBandwidth,                      /* Bandwidth of resonator in Hz  */
    int aSamplerate)
{
    initResonator(aFrequency, aBandwidth, aSamplerate); /* First compute ordinary resonator coefficients */
    /* Now convert to antiresonator coefficients */
    mA = 1.0f / mA;             /* a'=  1/a */
    mB *= -mA;                  /* b'= -b/a */
    mC *= -mA;                  /* c'= -c/a */
}

/* Generic resonator function */
float resonator::resonate(float input)
{
    float x = mA * input + mB * mP1 + mC * mP2;
    mP2 = mP1;
    mP1 = x;
    return x;
}

/* Generic anti-resonator function
    Same as resonator except that a,b,c need to be set with initAntiresonator()
    and we save inputs in p1/p2 rather than outputs.
    There is currently only one of these - "mNasalZero"
*/
/*  Output = (mNasalZero.a * input) + (mNasalZero.b * oldin1) + (mNasalZero.c * oldin2) */

float resonator::antiresonate(float input)
{
    float x = mA * input + mB * mP1 + mC * mP2;
    mP2 = mP1;
    mP1 = input;
    return x;
}

resonator::resonator()
{
    mA = mB = mC = mP1 = mP2 = 0;
}

resonator::~resonator()
{
}

void resonator::setGain(float aG)
{
    mA *= aG;
}



#define CASCADE_PARALLEL      1
#define ALL_PARALLEL          2
#define NPAR                 40

class klatt_frame
{
public:
    int mF0FundamentalFreq;          // Voicing fund freq in Hz                       
    int mVoicingAmpdb;               // Amp of voicing in dB,            0 to   70    
    int mFormant1Freq;               // First formant freq in Hz,        200 to 1300  
    int mFormant1Bandwidth;          // First formant bw in Hz,          40 to 1000   
    int mFormant2Freq;               // Second formant freq in Hz,       550 to 3000  
    int mFormant2Bandwidth;          // Second formant bw in Hz,         40 to 1000   
    int mFormant3Freq;               // Third formant freq in Hz,        1200 to 4999 
    int mFormant3Bandwidth;          // Third formant bw in Hz,          40 to 1000   
    int mFormant4Freq;               // Fourth formant freq in Hz,       1200 to 4999 
    int mFormant4Bandwidth;          // Fourth formant bw in Hz,         40 to 1000   
    int mFormant5Freq;               // Fifth formant freq in Hz,        1200 to 4999 
    int mFormant5Bandwidth;          // Fifth formant bw in Hz,          40 to 1000   
    int mFormant6Freq;               // Sixth formant freq in Hz,        1200 to 4999 
    int mFormant6Bandwidth;          // Sixth formant bw in Hz,          40 to 2000   
    int mNasalZeroFreq;              // Nasal zero freq in Hz,           248 to  528  
    int mNasalZeroBandwidth;         // Nasal zero bw in Hz,             40 to 1000   
    int mNasalPoleFreq;              // Nasal pole freq in Hz,           248 to  528  
    int mNasalPoleBandwidth;         // Nasal pole bw in Hz,             40 to 1000   
    int mAspirationAmpdb;            // Amp of aspiration in dB,         0 to   70    
    int mNoSamplesInOpenPeriod;      // # of samples in open period,     10 to   65   
    int mVoicingBreathiness;         // Breathiness in voicing,          0 to   80    
    int mVoicingSpectralTiltdb;      // Voicing spectral tilt in dB,     0 to   24    
    int mFricationAmpdb;             // Amp of frication in dB,          0 to   80    
    int mSkewnessOfAlternatePeriods; // Skewness of alternate periods,   0 to   40 in sample#/2
    int mFormant1Ampdb;              // Amp of par 1st formant in dB,    0 to   80  
    int mFormant1ParallelBandwidth;  // Par. 1st formant bw in Hz,       40 to 1000 
    int mFormant2Ampdb;              // Amp of F2 frication in dB,       0 to   80  
    int mFormant2ParallelBandwidth;  // Par. 2nd formant bw in Hz,       40 to 1000 
    int mFormant3Ampdb;              // Amp of F3 frication in dB,       0 to   80  
    int mFormant3ParallelBandwidth;  // Par. 3rd formant bw in Hz,       40 to 1000 
    int mFormant4Ampdb;              // Amp of F4 frication in dB,       0 to   80  
    int mFormant4ParallelBandwidth;  // Par. 4th formant bw in Hz,       40 to 1000 
    int mFormant5Ampdb;              // Amp of F5 frication in dB,       0 to   80  
    int mFormant5ParallelBandwidth;  // Par. 5th formant bw in Hz,       40 to 1000 
    int mFormant6Ampdb;              // Amp of F6 (same as r6pa),        0 to   80  
    int mFormant6ParallelBandwidth;  // Par. 6th formant bw in Hz,       40 to 2000 
    int mParallelNasalPoleAmpdb;     // Amp of par nasal pole in dB,     0 to   80  
    int mBypassFricationAmpdb;       // Amp of bypass fric. in dB,       0 to   80  
    int mPalallelVoicingAmpdb;       // Amp of voicing,  par in dB,      0 to   70  
    int mOverallGaindb;              // Overall gain, 60 dB is unity,    0 to   60  
    klatt_frame();
};

class darray;
class Element;

class Slope
{
public:
    float mValue;                   /* boundary value */
    int mTime;                      /* transition time */
    Slope() 
    {
        mValue = 0;
        mTime = 0;
    }
};


class klatt
{
    // resonators
    resonator mParallelFormant1, mParallelFormant2, mParallelFormant3, 
              mParallelFormant4, mParallelFormant5, mParallelFormant6,
              mParallelResoNasalPole, mNasalPole, mNasalZero, 
              mCritDampedGlotLowPassFilter, mDownSampLowPassFilter, mOutputLowPassFilter;
public:
    int mF0Flutter;
    int mSampleRate;
    int mNspFr;
    int mF0FundamentalFreq;        // Voicing fund freq in Hz  
    int mVoicingAmpdb;          // Amp of voicing in dB,    0 to   70  
    int mSkewnessOfAlternatePeriods;         // Skewness of alternate periods,0 to   40  
    int mTimeCount;     // used for f0 flutter
    int mNPer;          // Current loc in voicing period   40000 samp/s
    int mT0;            // Fundamental period in output samples times 4 
    int mNOpen;         // Number of samples in open phase of period  
    int mNMod;          // Position in period to begin noise amp. modul 

    // Various amplitude variables used in main loop

    float mAmpVoice;     // mVoicingAmpdb converted to linear gain  
    float mAmpBypas;     // mBypassFricationAmpdb converted to linear gain  
    float mAmpAspir;     // AP converted to linear gain  
    float mAmpFrica;     // mFricationAmpdb converted to linear gain  
    float mAmpBreth;     // ATURB converted to linear gain  

    // State variables of sound sources

    int mSkew;                  // Alternating jitter, in half-period units  
    float mNatglotA;           // Makes waveshape of glottal pulse when open  
    float mNatglotB;           // Makes waveshape of glottal pulse when open  
    float mVWave;               // Ditto, but before multiplication by mVoicingAmpdb  
    float mVLast;               // Previous output of voice  
    float mNLast;               // Previous output of random number generator  
    float mGlotLast;            // Previous value of glotout  
    float mDecay;               // mVoicingSpectralTiltdb converted to exponential time const  
    float mOneMd;               // in voicing one-pole ELM_FEATURE_LOW-pass filter  


    float natural_source(int aNper);

    void frame_init(klatt_frame *frame);
    void flutter(klatt_frame *pars);
    void pitch_synch_par_reset(klatt_frame *frame, int ns);
    void parwave(klatt_frame *frame, short int *jwave);
    void init();
    static int phone_to_elm(char *aPhoneme, int aCount, darray *aElement);

    int mElementCount;
    unsigned char *mElement;
    int mElementIndex;
    klatt_frame mKlattFramePars;
    Element * mLastElement;
    int mTStress;
    int mNTStress;
    Slope mStressS;
    Slope mStressE;
    float mTop;
    void initsynth(int aElementCount,unsigned char *aElement);
    int synth(int aSampleCount, short *aSamplePointer);
    klatt();

    int count; // MG: REINIT AT LONG PAUSE

};



#ifndef PI
#define PI 3.1415926535897932384626433832795f
#endif

#ifndef NULL
#define NULL 0
#endif

class Interp
{
public:
    float mSteady;
    float mFixed;
    char  mProportion;
    char  mExtDelay;
    char  mIntDelay;
};


enum Eparm_e
{
  ELM_FN, ELM_F1, ELM_F2, ELM_F3, 
  ELM_B1, ELM_B2, ELM_B3, ELM_AN, 
  ELM_A1, ELM_A2, ELM_A3, ELM_A4, 
  ELM_A5, ELM_A6, ELM_AB, ELM_AV, 
  ELM_AVC, ELM_ASP, ELM_AF, 
  ELM_COUNT
};
 
class Element
{
public:
      const char *mName; // unused
      const char mRK;
      const char mDU;
      const char mUD;
      unsigned char mFont; // unused
      const char  *mDict; // unused
      const char  *mIpa; // unused
      int   mFeat; // only ELM_FEATURE_VWL
      Interp mInterpolator[ELM_COUNT];
 };

enum ELEMENT_FEATURES
{
    ELM_FEATURE_ALV = 0x00000001,
    ELM_FEATURE_APR = 0x00000002,
    ELM_FEATURE_BCK = 0x00000004,
    ELM_FEATURE_BLB = 0x00000008,
    ELM_FEATURE_CNT = 0x00000010,
    ELM_FEATURE_DNT = 0x00000020,
    ELM_FEATURE_FNT = 0x00000040,
    ELM_FEATURE_FRC = 0x00000080,
    ELM_FEATURE_GLT = 0x00000100,
    ELM_FEATURE_HGH = 0x00000200,
    ELM_FEATURE_LAT = 0x00000400,
    ELM_FEATURE_LBD = 0x00000800,
    ELM_FEATURE_LBV = 0x00001000,
    ELM_FEATURE_LMD = 0x00002000,
    ELM_FEATURE_LOW = 0x00004000,
    ELM_FEATURE_MDL = 0x00008000,
    ELM_FEATURE_NAS = 0x00010000,
    ELM_FEATURE_PAL = 0x00020000,
    ELM_FEATURE_PLA = 0x00040000,
    ELM_FEATURE_RND = 0x00080000,
    ELM_FEATURE_RZD = 0x00100000,
    ELM_FEATURE_SMH = 0x00200000,
    ELM_FEATURE_STP = 0x00400000,
    ELM_FEATURE_UMD = 0x00800000,
    ELM_FEATURE_UNR = 0x01000000,
    ELM_FEATURE_VCD = 0x02000000,
    ELM_FEATURE_VEL = 0x04000000,
    ELM_FEATURE_VLS = 0x08000000,
    ELM_FEATURE_VWL = 0x10000000
};

enum ELEMENTS 
{
    ELM_END = 0,    
    ELM_Q,  ELM_P,  ELM_PY, ELM_PZ, ELM_T,  ELM_TY, 
    ELM_TZ, ELM_K,  ELM_KY, ELM_KZ, ELM_B,  ELM_BY, ELM_BZ, 
    ELM_D,  ELM_DY, ELM_DZ, ELM_G,  ELM_GY, ELM_GZ, ELM_M,  
    ELM_N,  ELM_NG, ELM_F,  ELM_TH, ELM_S,  ELM_SH, ELM_X,
    ELM_H,  ELM_V,  ELM_QQ, ELM_DH, ELM_DI, ELM_Z,  ELM_ZZ,
    ELM_ZH, ELM_CH, ELM_CI, ELM_J,  ELM_JY, ELM_L,  ELM_LL,
    ELM_RX, ELM_R,  ELM_W,  ELM_Y,  ELM_I,  ELM_E,  ELM_AA,
    ELM_U,  ELM_O,  ELM_OO, ELM_A,  ELM_EE, ELM_ER, ELM_AR,
    ELM_AW, ELM_UU, ELM_AI, ELM_IE, ELM_OI, ELM_OU, ELM_OV,
    ELM_OA, ELM_IA, ELM_IB, ELM_AIR,ELM_OOR,ELM_OR
};

#define PHONEME_COUNT 52
#define AMP_ADJ 14
#define StressDur(e,s) (s,((e->mDU + e->mUD)/2))




class PhonemeToElements 
{
public:
    int mKey;
    char mData[8];
};

/* Order is important - 2 byte phonemes first, otherwise
   the search function will fail*/
static PhonemeToElements phoneme_to_elements[PHONEME_COUNT] = 
{
    /* mKey, count, 0-7 elements */
/* tS */ 0x5374, 2, ELM_CH, ELM_CI, 0, 0, 0, 0, 0,
/* dZ */ 0x5a64, 4, ELM_J, ELM_JY, ELM_QQ, ELM_JY, 0, 0, 0,
/* rr */ 0x7272, 3, ELM_R, ELM_QQ, ELM_R, 0, 0, 0, 0,
/* eI */ 0x4965, 2, ELM_AI, ELM_I, 0, 0, 0, 0, 0,
/* aI */ 0x4961, 2, ELM_IE, ELM_I, 0, 0, 0, 0, 0,
/* oI */ 0x496f, 2, ELM_OI, ELM_I, 0, 0, 0, 0, 0,
/* aU */ 0x5561, 2, ELM_OU, ELM_OV, 0, 0, 0, 0, 0,
/* @U */ 0x5540, 2, ELM_OA, ELM_OV, 0, 0, 0, 0, 0,
/* I@ */ 0x4049, 2, ELM_IA, ELM_IB, 0, 0, 0, 0, 0,
/* e@ */ 0x4065, 2, ELM_AIR, ELM_IB, 0, 0, 0, 0, 0,
/* U@ */ 0x4055, 2, ELM_OOR, ELM_IB, 0, 0, 0, 0, 0,
/* O@ */ 0x404f, 2, ELM_OR, ELM_IB, 0, 0, 0, 0, 0,
/* oU */ 0x556f, 2, ELM_OI, ELM_OV, 0, 0, 0, 0, 0,
/*    */ 0x0020, 1, ELM_Q, 0, 0, 0, 0, 0, 0,
/* p  */ 0x0070, 3, ELM_P, ELM_PY, ELM_PZ, 0, 0, 0, 0,
/* t  */ 0x0074, 3, ELM_T, ELM_TY, ELM_TZ, 0, 0, 0, 0,
/* k  */ 0x006b, 3, ELM_K, ELM_KY, ELM_KZ, 0, 0, 0, 0,
/* b  */ 0x0062, 3, ELM_B, ELM_BY, ELM_BZ, 0, 0, 0, 0,
/* d  */ 0x0064, 3, ELM_D, ELM_DY, ELM_DZ, 0, 0, 0, 0,
/* g  */ 0x0067, 3, ELM_G, ELM_GY, ELM_GZ, 0, 0, 0, 0,
/* m  */ 0x006d, 1, ELM_M, 0, 0, 0, 0, 0, 0,
/* n  */ 0x006e, 1, ELM_N, 0, 0, 0, 0, 0, 0,
/* N  */ 0x004e, 1, ELM_NG, 0, 0, 0, 0, 0, 0,
/* f  */ 0x0066, 1, ELM_F, 0, 0, 0, 0, 0, 0,
/* T  */ 0x0054, 1, ELM_TH, 0, 0, 0, 0, 0, 0,
/* s  */ 0x0073, 1, ELM_S, 0, 0, 0, 0, 0, 0,
/* S  */ 0x0053, 1, ELM_SH, 0, 0, 0, 0, 0, 0,
/* h  */ 0x0068, 1, ELM_H, 0, 0, 0, 0, 0, 0,
/* v  */ 0x0076, 3, ELM_V, ELM_QQ, ELM_V, 0, 0, 0, 0,
/* D  */ 0x0044, 3, ELM_DH, ELM_QQ, ELM_DI, 0, 0, 0, 0,
/* z  */ 0x007a, 3, ELM_Z, ELM_QQ, ELM_ZZ, 0, 0, 0, 0,
/* Z  */ 0x005a, 3, ELM_ZH, ELM_QQ, ELM_ZH, 0, 0, 0, 0,
/* l  */ 0x006c, 1, ELM_L, 0, 0, 0, 0, 0, 0,
/* r  */ 0x0072, 1, ELM_R, 0, 0, 0, 0, 0, 0,
/* R  */ 0x0052, 1, ELM_RX, 0, 0, 0, 0, 0, 0,
/* w  */ 0x0077, 1, ELM_W, 0, 0, 0, 0, 0, 0,
/* x  */ 0x0078, 1, ELM_X, 0, 0, 0, 0, 0, 0,
/* %  */ 0x0025, 1, ELM_QQ, 0, 0, 0, 0, 0, 0,
/* j  */ 0x006a, 1, ELM_Y, 0, 0, 0, 0, 0, 0,
/* I  */ 0x0049, 1, ELM_I, 0, 0, 0, 0, 0, 0,
/* e  */ 0x0065, 1, ELM_E, 0, 0, 0, 0, 0, 0,
/* &  */ 0x0026, 1, ELM_AA, 0, 0, 0, 0, 0, 0,
/* V  */ 0x0056, 1, ELM_U, 0, 0, 0, 0, 0, 0,
/* 0  */ 0x0030, 1, ELM_O, 0, 0, 0, 0, 0, 0,
/* U  */ 0x0055, 1, ELM_OO, 0, 0, 0, 0, 0, 0,
/* @  */ 0x0040, 1, ELM_A, 0, 0, 0, 0, 0, 0,
/* i  */ 0x0069, 1, ELM_EE, 0, 0, 0, 0, 0, 0,
/* 3  */ 0x0033, 1, ELM_ER, 0, 0, 0, 0, 0, 0,
/* A  */ 0x0041, 1, ELM_AR, 0, 0, 0, 0, 0, 0,
/* O  */ 0x004f, 1, ELM_AW, 0, 0, 0, 0, 0, 0,
/* u  */ 0x0075, 1, ELM_UU, 0, 0, 0, 0, 0, 0,
/* o  */ 0x006f, 1, ELM_OI, 0, 0, 0, 0, 0, 0,
};

static Element gElement[] =
{
/*mName, mRK, mDU, mUD, mFont, mDict, mIpa, mFeat, interpolators*/
/* (mSteady, mFixed, mProportion, mExtDelay, mIntDelay) */
{"END", 31, 5, 5,0x00,NULL,NULL,0,
 {
  {   270,    135,  50,  3,  3}, /* ELM_FN       0 */
  {   490,      0, 100,  0,  0}, /* ELM_F1       0 */
  {  1480,      0, 100,  0,  0}, /* ELM_F2       0 */
  {  2500,      0, 100,  0,  0}, /* ELM_F3       0 */
  {    60,      0, 100,  0,  0}, /* ELM_B1       0 */
  {    90,      0, 100,  0,  0}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  0}, /* ELM_B3       0 */
  {   -30,  -10.5, 100,  3,  0}, /* ELM_AN   -10.5 */
  {   -30,  -10.5, 100,  3,  0}, /* ELM_A1   -10.5 */
  {   -30,  -10.5, 100,  3,  0}, /* ELM_A2   -10.5 */
  {   -30,  -10.5, 100,  3,  0}, /* ELM_A3   -10.5 */
  {   -30,  -10.5, 100,  3,  0}, /* ELM_A4   -10.5 */
  {   -30,      0, 100,  3,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"Q",   29, 6, 6,0x00,NULL,NULL,0,
 {
  {   270,    135,  50,  3,  3}, /* ELM_FN       0 */
  {   490,      0, 100,  3,  3}, /* ELM_F1       0 */
  {  1480,      0, 100,  3,  3}, /* ELM_F2       0 */
  {  2500,      0, 100,  3,  3}, /* ELM_F3       0 */
  {    60,      0, 100,  3,  3}, /* ELM_B1       0 */
  {    90,      0, 100,  3,  3}, /* ELM_B2       0 */
  {   150,      0, 100,  3,  3}, /* ELM_B3       0 */
  {   -30,  -10.5, 100,  3,  0}, /* ELM_AN   -10.5 */
  {   -30,  -10.5, 100,  3,  0}, /* ELM_A1   -10.5 */
  {   -30,  -10.5, 100,  3,  0}, /* ELM_A2   -10.5 */
  {   -30,  -10.5, 100,  3,  0}, /* ELM_A3   -10.5 */
  {   -30,  -10.5, 100,  3,  0}, /* ELM_A4   -10.5 */
  {   -30,      0, 100,  3,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"P",   23, 8, 8,0x70,"p","p",ELM_FEATURE_BLB|ELM_FEATURE_STP|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  2,  2}, /* ELM_F1      15 */
  {   760,    350,  50,  2,  2}, /* ELM_F2     -30 */
  {  2500,      0, 100,  0,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  2,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  2,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"PY",  29, 1, 1,0x70,"p","p",ELM_FEATURE_BLB|ELM_FEATURE_STP|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,      0, 100,  0,  0}, /* ELM_F1       0 */
  {   760,      0, 100,  0,  0}, /* ELM_F2       0 */
  {  2500,      0, 100,  0,  0}, /* ELM_F3       0 */
  {    60,      0, 100,  0,  0}, /* ELM_B1       0 */
  {    90,      0, 100,  0,  0}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {  24.5,      0, 100,  0,  0}, /* ELM_A1       0 */
  {    49,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 43.75,      0, 100,  0,  0}, /* ELM_A3       0 */
  {  38.5,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"PZ",  23, 2, 2,0x70,"p","p",ELM_FEATURE_BLB|ELM_FEATURE_STP|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  2,  2}, /* ELM_F1      15 */
  {   760,    350,  50,  2,  2}, /* ELM_F2     -30 */
  {  2500,      0, 100,  2,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  2,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  2,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  2,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {  24.5,      0, 100,  0,  0}, /* ELM_A1       0 */
  {  38.5,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 33.25,      0, 100,  0,  0}, /* ELM_A3       0 */
  {    28,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"T",   23, 6, 6,0x74,"t","t",ELM_FEATURE_ALV|ELM_FEATURE_STP|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  2,  2}, /* ELM_F1      15 */
  {  1780,    950,  50,  2,  2}, /* ELM_F2      60 */
  {  2680,   2680,   0,  0,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  2,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  2,  2}, /* ELM_B2       0 */
  {   150,    150,   0,  0,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"TY",  29, 1, 1,0x74,"t","t",ELM_FEATURE_ALV|ELM_FEATURE_STP|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,      0, 100,  0,  0}, /* ELM_F1       0 */
  {  1780,      0, 100,  0,  0}, /* ELM_F2       0 */
  {  2680,      0, 100,  0,  0}, /* ELM_F3       0 */
  {    60,      0, 100,  0,  0}, /* ELM_B1       0 */
  {    90,      0, 100,  0,  0}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {  38.5,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 50.75,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"TZ",  23, 2, 2,0x74,"t","t",ELM_FEATURE_ALV|ELM_FEATURE_STP|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  2,  1}, /* ELM_F1      15 */
  {  1780,    950,  50,  2,  1}, /* ELM_F2      60 */
  {  2680,   2680,   0,  2,  0}, /* ELM_F3       0 */
  {    60,     30,  50,  2,  1}, /* ELM_B1       0 */
  {    90,     45,  50,  2,  1}, /* ELM_B2       0 */
  {   150,    150,   0,  2,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {    28,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 40.25,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"K",   23, 8, 8,0x6B,"k","k",ELM_FEATURE_STP|ELM_FEATURE_VEL|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  3,  3}, /* ELM_F1      15 */
  {  1480,   1550,  50,  3,  3}, /* ELM_F2     810 */
  {  2620,   1580,  50,  3,  3}, /* ELM_F3     270 */
  {    60,     30,  50,  3,  3}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  3}, /* ELM_B2       0 */
  {   150,     75,  50,  3,  3}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"KY",  29, 1, 1,0x6B,"k","k",ELM_FEATURE_STP|ELM_FEATURE_VEL|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,      0, 100,  0,  0}, /* ELM_F1       0 */
  {  1480,      0, 100,  0,  0}, /* ELM_F2       0 */
  {  2620,      0, 100,  0,  0}, /* ELM_F3       0 */
  {    60,      0, 100,  0,  0}, /* ELM_B1       0 */
  {    90,      0, 100,  0,  0}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  { 50.75,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 50.75,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 29.75,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"KZ",  23, 4, 4,0x6B,"k","k",ELM_FEATURE_STP|ELM_FEATURE_VEL|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  3,  3}, /* ELM_F1      15 */
  {  1480,   1550,  50,  3,  3}, /* ELM_F2     810 */
  {  2620,   1580,  50,  3,  3}, /* ELM_F3     270 */
  {    60,     30,  50,  3,  3}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  3}, /* ELM_B2       0 */
  {   150,     75,  50,  3,  3}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  { 40.25,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 40.25,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 19.25,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"B",   26,12,12,0x62,"b","b",ELM_FEATURE_BLB|ELM_FEATURE_STP|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  2,  2}, /* ELM_F1      15 */
  {   760,    350,  50,  2,  2}, /* ELM_F2     -30 */
  {  2500,      0, 100,  0,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  2,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  2,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {  24.5,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"BY",  29, 1, 1,0x62,"b","b",ELM_FEATURE_BLB|ELM_FEATURE_STP|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,      0, 100,  0,  0}, /* ELM_F1       0 */
  {   760,      0, 100,  0,  0}, /* ELM_F2       0 */
  {  2500,      0, 100,  0,  0}, /* ELM_F3       0 */
  {    60,      0, 100,  0,  0}, /* ELM_B1       0 */
  {    90,      0, 100,  0,  0}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {  24.5,      0, 100,  0,  0}, /* ELM_A1       0 */
  {    49,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 43.25,      0, 100,  0,  0}, /* ELM_A3       0 */
  {  38.5,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"BZ",  26, 0, 0,0x62,"b","b",ELM_FEATURE_BLB|ELM_FEATURE_STP|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  2,  0}, /* ELM_F1      15 */
  {   760,    350,  50,  2,  0}, /* ELM_F2     -30 */
  {  2500,      0, 100,  0,  0}, /* ELM_F3       0 */
  {    60,     30,  50,  2,  0}, /* ELM_B1       0 */
  {    90,     45,  50,  2,  0}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"D",   26, 8, 8,0x64,"d","d",ELM_FEATURE_ALV|ELM_FEATURE_STP|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  2,  2}, /* ELM_F1      15 */
  {  1780,    950,  50,  2,  2}, /* ELM_F2      60 */
  {  2680,   2680,   0,  2,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  2,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  2,  2}, /* ELM_B2       0 */
  {   150,    150,   0,  2,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {  31.5,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"DY",  29, 1, 1,0x64,"d","d",ELM_FEATURE_ALV|ELM_FEATURE_STP|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,      0, 100,  0,  0}, /* ELM_F1       0 */
  {  1780,      0, 100,  0,  0}, /* ELM_F2       0 */
  {  2680,      0, 100,  0,  0}, /* ELM_F3       0 */
  {    60,      0, 100,  0,  0}, /* ELM_B1       0 */
  {    90,      0, 100,  0,  0}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {  38.5,      0, 100,  0,  0}, /* ELM_A1       0 */
  {  38.5,      0, 100,  0,  0}, /* ELM_A2       0 */
  {    35,      0, 100,  0,  0}, /* ELM_A3       0 */
  {  45.5,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"DZ",  26, 1, 1,0x64,"d","d",ELM_FEATURE_ALV|ELM_FEATURE_STP|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  2,  0}, /* ELM_F1      15 */
  {  1780,    950,  50,  2,  0}, /* ELM_F2      60 */
  {  2680,   2680,   0,  2,  0}, /* ELM_F3       0 */
  {    60,     30,  50,  2,  0}, /* ELM_B1       0 */
  {    90,     45,  50,  2,  0}, /* ELM_B2       0 */
  {   150,    150,   0,  2,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {  38.5,      0, 100,  0,  0}, /* ELM_A1       0 */
  {    28,      0, 100,  0,  0}, /* ELM_A2       0 */
  {  24.5,      0, 100,  0,  0}, /* ELM_A3       0 */
  {    35,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},


{"G",   26,12,12,0x67,"g","g",ELM_FEATURE_STP|ELM_FEATURE_VCD|ELM_FEATURE_VEL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  3,  3}, /* ELM_F1      15 */
  {  1480,   1550,  50,  3,  3}, /* ELM_F2     810 */
  {  2620,   1580,  50,  3,  3}, /* ELM_F3     270 */
  {    60,     30,  50,  3,  3}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  3}, /* ELM_B2       0 */
  {   150,     75,  50,  3,  3}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {    35,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"GY",  29, 1, 1,0x67,"g","g",ELM_FEATURE_STP|ELM_FEATURE_VCD|ELM_FEATURE_VEL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,      0, 100,  0,  0}, /* ELM_F1       0 */
  {  1480,      0, 100,  0,  0}, /* ELM_F2       0 */
  {  2620,      0, 100,  0,  0}, /* ELM_F3       0 */
  {    60,      0, 100,  0,  0}, /* ELM_B1       0 */
  {    90,      0, 100,  0,  0}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {    35,      0, 100,  0,  0}, /* ELM_A1       0 */
  {  45.5,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 40.25,      0, 100,  0,  0}, /* ELM_A3       0 */
  {  24.5,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"GZ",  26, 2, 2,0x67,"g","g",ELM_FEATURE_STP|ELM_FEATURE_VCD|ELM_FEATURE_VEL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  3,  2}, /* ELM_F1      15 */
  {  1480,   1550,  50,  3,  2}, /* ELM_F2     810 */
  {  2620,   1580,  50,  3,  2}, /* ELM_F3     270 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,     75,  50,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {    35,      0, 100,  0,  0}, /* ELM_A1       0 */
  {    35,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 29.75,      0, 100,  0,  0}, /* ELM_A3       0 */
  {    14,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"M",   15, 8, 8,0x6D,"m","m",ELM_FEATURE_BLB|ELM_FEATURE_NAS,
 {
  {   360,    360,   0,  3,  0}, /* ELM_FN       0 */
  {   480,    480,   0,  3,  0}, /* ELM_F1       0 */
  {  1000,    350,  50,  3,  0}, /* ELM_F2    -150 */
  {  2200,      0, 100,  5,  0}, /* ELM_F3       0 */
  {    40,     20,  50,  3,  0}, /* ELM_B1       0 */
  {   175,     87,  50,  3,  0}, /* ELM_B2    -0.5 */
  {   120,      0, 100,  5,  0}, /* ELM_B3       0 */
  {    42,     21,  50,  3,  0}, /* ELM_AN       0 */
  {    26,    -10, 100,  3,  0}, /* ELM_A1     -10 */
  {    30,    -10, 100,  3,  0}, /* ELM_A2     -10 */
  {    33,    -10, 100,  3,  0}, /* ELM_A3     -10 */
  {   -30,    -10, 100,  3,  0}, /* ELM_A4     -10 */
  {   -30,      0, 100,  3,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  2,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  2,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  2,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  2,  0}  /* ELM_AF       0 */
 }
},

{"N",   15, 8, 8,0x6E,"n","n",ELM_FEATURE_ALV|ELM_FEATURE_NAS,
 {
  {   450,    450,   0,  3,  0}, /* ELM_FN       0 */
  {   480,    480,   0,  3,  0}, /* ELM_F1       0 */
  {  1780,    950,  50,  3,  3}, /* ELM_F2      60 */
  {  2620,   2680,   0,  3,  0}, /* ELM_F3      60 */
  {    40,     20,  50,  3,  0}, /* ELM_B1       0 */
  {   300,    150,  50,  3,  3}, /* ELM_B2       0 */
  {   260,    130,  50,  3,  0}, /* ELM_B3       0 */
  {    42,     21,  50,  3,  0}, /* ELM_AN       0 */
  {    35,    -10, 100,  3,  0}, /* ELM_A1     -10 */
  {    35,    -10, 100,  3,  0}, /* ELM_A2     -10 */
  {    35,    -10, 100,  3,  0}, /* ELM_A3     -10 */
  {    20,    -10, 100,  3,  0}, /* ELM_A4     -10 */
  {   -30,      0, 100,  3,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  2,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  2,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  2,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  2,  0}  /* ELM_AF       0 */
 }
},

{"NG",  15, 8, 8,0x4E,"N","N",ELM_FEATURE_NAS|ELM_FEATURE_VEL,
 {
  {   360,    360,   0,  3,  0}, /* ELM_FN       0 */
  {   480,    480,   0,  3,  0}, /* ELM_F1       0 */
  {   820,   1550,  50,  5,  3}, /* ELM_F2    1140 */
  {  2800,   1580,  50,  3,  3}, /* ELM_F3     180 */
  {   160,     80,   0,  5,  0}, /* ELM_B1     -80 */
  {   150,     75,  50,  5,  3}, /* ELM_B2       0 */
  {   100,     50,  50,  3,  0}, /* ELM_B3       0 */
  {    42,     21,  50,  3,  3}, /* ELM_AN       0 */
  {    20,      0, 100,  3,  0}, /* ELM_A1       0 */
  {    30,      0, 100,  3,  0}, /* ELM_A2       0 */
  {    35,      0, 100,  3,  0}, /* ELM_A3       0 */
  {     0,      0, 100,  3,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  3,  0}, /* ELM_AB       0 */
  {    52,     26,  50,  2,  0}, /* ELM_AV       0 */
  {    56,     28,  50,  2,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  2,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  2,  0}  /* ELM_AF       0 */
 }
},

{"F",   18,12,12,0x66,"f","f",ELM_FEATURE_FRC|ELM_FEATURE_LBD|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   400,    170,  50,  3,  2}, /* ELM_F1     -30 */
  {  1420,    350,  50,  3,  2}, /* ELM_F2    -360 */
  {  2560,    980,  50,  3,  2}, /* ELM_F3    -300 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,     75,  50,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {     0,      0, 100,  0,  0}, /* ELM_A2       0 */
  {     0,      0, 100,  0,  0}, /* ELM_A3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {    54,     27,  50,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    32,     16,  50,  0,  0}, /* ELM_ASP      0 */
  {    54,     30,  50,  0,  0}  /* ELM_AF       3 */
 }
},

{"TH",  18,15,15,0x54,"T","T",ELM_FEATURE_DNT|ELM_FEATURE_FRC|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   400,    170,  50,  3,  2}, /* ELM_F1     -30 */
  {  1780,   1190,  50,  3,  2}, /* ELM_F2     300 */
  {  2680,   2680,   0,  3,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,    150,   0,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A2       0 */
  {    28,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 22.75,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"S",   18,12,12,0x73,"s","s",ELM_FEATURE_ALV|ELM_FEATURE_FRC|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   400,    170,  50,  3,  2}, /* ELM_F1     -30 */
  {  1720,    950,  50,  3,  2}, /* ELM_F2      90 */
  {  2620,      0, 100,  3,  2}, /* ELM_F3       0 */
  {   200,    100,  50,  3,  2}, /* ELM_B1       0 */
  {    96,     48,  50,  3,  2}, /* ELM_B2       0 */
  {   220,      0, 100,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {    28,      0, 100,  0,  0}, /* ELM_A2       0 */
  {    28,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 40.25,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    32,     16,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"SH",  18,12,12,0x53,"S","S",ELM_FEATURE_FRC|ELM_FEATURE_PLA|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   400,    170,  50,  3,  2}, /* ELM_F1     -30 */
  {  2200,   1190,  50,  3,  2}, /* ELM_F2      90 */
  {  2560,      0, 100,  3,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {  31.5,      0, 100,  0,  0}, /* ELM_A2       0 */
  {    42,      0, 100,  0,  0}, /* ELM_A3       0 */
  {  31.5,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"X",   18,12,12,0x78,"x","x",ELM_FEATURE_FRC|ELM_FEATURE_VEL|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  3,  3}, /* ELM_F1      15 */
  {  1480,   1550,  50,  3,  3}, /* ELM_F2     810 */
  {  2620,   1580,  50,  3,  3}, /* ELM_F3     270 */
  {    60,     30,  50,  3,  3}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  3}, /* ELM_B2       0 */
  {   150,     75,  50,  3,  3}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  { 40.25,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 40.25,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 19.25,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"H",    9,10,10,0x68,"h","h",ELM_FEATURE_APR|ELM_FEATURE_GLT,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   490,      0, 100,  0,  7}, /* ELM_F1       0 */
  {  1480,      0, 100,  0,  7}, /* ELM_F2       0 */
  {  2500,      0, 100,  0,  7}, /* ELM_F3       0 */
  {    60,      0, 100,  0,  7}, /* ELM_B1       0 */
  {    90,      0, 100,  0,  7}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  7}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  7}, /* ELM_AN       0 */
  {    35,    -14, 100,  0,  7}, /* ELM_A1     -14 */
  { 36.75,    -14, 100,  0,  7}, /* ELM_A2     -14 */
  { 26.25,     -7, 100,  0,  7}, /* ELM_A3      -7 */
  { 22.75,   -3.5, 100,  0,  7}, /* ELM_A4    -3.5 */
  {   -30,      0, 100,  0,  7}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  7}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  7}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"V",   20, 4, 4,0x76,"v","v",ELM_FEATURE_FRC|ELM_FEATURE_LBD|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   280,    170,  50,  3,  2}, /* ELM_F1      30 */
  {  1420,    350,  50,  3,  2}, /* ELM_F2    -360 */
  {  2560,    980,  50,  3,  2}, /* ELM_F3    -300 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,     75,  50,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  { 29.75,      0, 100,  0,  0}, /* ELM_A1       0 */
  { 40.25,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 36.75,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 33.25,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"QQ",  30, 0, 0,0x5A,"Z","Z",ELM_FEATURE_FRC|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   280,      0, 100,  0,  0}, /* ELM_F1       0 */
  {  1420,      0, 100,  0,  0}, /* ELM_F2       0 */
  {  2560,      0, 100,  0,  0}, /* ELM_F3       0 */
  {    60,      0, 100,  0,  0}, /* ELM_B1       0 */
  {    90,      0, 100,  0,  0}, /* ELM_B2       0 */
  {   150,      0, 100,  0,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  { 29.75,      0, 100,  0,  0}, /* ELM_A1       0 */
  { 40.25,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 36.75,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 33.25,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"DH",  20, 4, 4,0x54,"D","D",ELM_FEATURE_DNT|ELM_FEATURE_FRC|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   280,    170,  50,  3,  2}, /* ELM_F1      30 */
  {  1600,   1190,  50,  3,  2}, /* ELM_F2     390 */
  {  2560,      0, 100,  3,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  { 29.75,      0, 100,  0,  0}, /* ELM_A1       0 */
  {  31.5,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A3       0 */
  {    28,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {    54,     27,  50,  0,  0}, /* ELM_AB       0 */
  {    36,     18,  50,  0,  0}, /* ELM_AV       0 */
  {    54,     27,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"DI",  20, 4, 4,0x54,"D","D",ELM_FEATURE_DNT|ELM_FEATURE_FRC|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   280,    170,  50,  3,  2}, /* ELM_F1      30 */
  {  1600,   1190,  50,  3,  2}, /* ELM_F2     390 */
  {  2560,      0, 100,  3,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  { 29.75,      0, 100,  0,  0}, /* ELM_A1       0 */
  {  31.5,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A3       0 */
  {    28,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"Z",   20, 4, 4,0x7A,"z","z",ELM_FEATURE_ALV|ELM_FEATURE_FRC|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   280,    170,  50,  3,  2}, /* ELM_F1      30 */
  {  1720,    950,  50,  3,  2}, /* ELM_F2      90 */
  {  2560,      0, 100,  3,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  { 29.75,      0, 100,  0,  0}, /* ELM_A1       0 */
  {  24.5,      0, 100,  0,  0}, /* ELM_A2       0 */
  {  24.5,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 36.75,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    40,     20,  50,  0,  0}, /* ELM_AV       0 */
  {    54,     27,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"ZZ",  20, 4, 4,0x7A,"z","z",ELM_FEATURE_ALV|ELM_FEATURE_FRC|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   280,    170,  50,  3,  2}, /* ELM_F1      30 */
  {  1720,    950,  50,  3,  2}, /* ELM_F2      90 */
  {  2560,      0, 100,  3,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  { 29.75,      0, 100,  0,  0}, /* ELM_A1       0 */
  {  24.5,      0, 100,  0,  0}, /* ELM_A2       0 */
  {  24.5,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 36.75,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"ZH",  20, 4, 4,0x5A,"Z","Z",ELM_FEATURE_FRC|ELM_FEATURE_PLA|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   280,    170,  50,  3,  2}, /* ELM_F1      30 */
  {  2020,   1190,  50,  3,  2}, /* ELM_F2     180 */
  {  2560,      0, 100,  3,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  { 29.75,      0, 100,  0,  0}, /* ELM_A1       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 36.75,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"CH",  23, 4, 4,0x74,"t","t",ELM_FEATURE_ALV|ELM_FEATURE_STP|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  2,  2}, /* ELM_F1      15 */
  {  1780,    950,  50,  2,  2}, /* ELM_F2      60 */
  {  2680,   2680,   0,  2,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  2,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  2,  2}, /* ELM_B2       0 */
  {   150,    150,   0,  2,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"CI",  18, 8, 8,0x53,"S","S",ELM_FEATURE_FRC|ELM_FEATURE_PLA|ELM_FEATURE_VLS,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   400,    170,  50,  3,  2}, /* ELM_F1     -30 */
  {  2020,   1190,  50,  3,  2}, /* ELM_F2     180 */
  {  2560,      0, 100,  3,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A1       0 */
  {  31.5,      0, 100,  0,  0}, /* ELM_A2       0 */
  {    42,      0, 100,  0,  0}, /* ELM_A3       0 */
  {  31.5,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AV       0 */
  {     0,      0,  50,  0,  0}, /* ELM_AVC      0 */
  {    60,     30,  50,  0,  0}, /* ELM_ASP      0 */
  {    60,     30,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"J",   26, 4, 4,0x64,"d","d",ELM_FEATURE_ALV|ELM_FEATURE_STP|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,    110,  50,  2,  2}, /* ELM_F1      15 */
  {  1780,    950,  50,  2,  2}, /* ELM_F2      60 */
  {  2680,   2680,   0,  2,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  2,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  2,  2}, /* ELM_B2       0 */
  {   150,    150,   0,  2,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  {  31.5,      0, 100,  0,  0}, /* ELM_A1       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A2       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"JY",  20, 3, 3,0x5A,"Z","Z",ELM_FEATURE_FRC|ELM_FEATURE_PLA|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   280,    170,  50,  3,  2}, /* ELM_F1      30 */
  {  2020,   1190,  50,  3,  2}, /* ELM_F2     180 */
  {  2560,      0, 100,  3,  2}, /* ELM_F3       0 */
  {    60,     30,  50,  3,  2}, /* ELM_B1       0 */
  {    90,     45,  50,  3,  2}, /* ELM_B2       0 */
  {   150,      0, 100,  3,  2}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  { 29.75,      0, 100,  0,  0}, /* ELM_A1       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 36.75,      0, 100,  0,  0}, /* ELM_A3       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"L",   11, 8, 8,0x6C,"l","l",ELM_FEATURE_ALV|ELM_FEATURE_LAT|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   460,    230,  50,  6,  0}, /* ELM_F1       0 */
  {  1480,    710,  50,  6,  0}, /* ELM_F2     -30 */
  {  2500,   1220,  50,  6,  0}, /* ELM_F3     -30 */
  {    60,     30,  50,  6,  0}, /* ELM_B1       0 */
  {    90,     45,  50,  6,  0}, /* ELM_B2       0 */
  {   150,     75,  50,  6,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  { 36.75,      0, 100,  0,  0}, /* ELM_A1       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A3       0 */
  {    21,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"LL",  11, 8, 8,0x6C,"l","l",ELM_FEATURE_ALV|ELM_FEATURE_LAT|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   460,    230,  50,  6,  0}, /* ELM_F1       0 */
  {   940,    470,  50,  6,  0}, /* ELM_F2       0 */
  {  2500,   1220,  50,  6,  0}, /* ELM_F3     -30 */
  {    60,     30,  50,  6,  0}, /* ELM_B1       0 */
  {    90,     45,  50,  6,  0}, /* ELM_B2       0 */
  {   150,     75,  50,  6,  0}, /* ELM_B3       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AN       0 */
  { 36.75,      0, 100,  0,  0}, /* ELM_A1       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A2       0 */
  { 26.25,      0, 100,  0,  0}, /* ELM_A3       0 */
  {    21,      0, 100,  0,  0}, /* ELM_A4       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A5       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_A6       0 */
  {   -30,      0, 100,  0,  0}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"RX",  10,10,10,0xD5,"R","<ELM_FEATURE_RZD>",ELM_FEATURE_RZD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   490,      0, 100,  0,  5}, /* ELM_F1       0 */
  {  1180,      0, 100,  0,  5}, /* ELM_F2       0 */
  {  1600,   1600,   0,  5,  5}, /* ELM_F3       0 */
  {    60,     30,  50,  0,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {    70,     35,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  {    42,     21,  50,  5,  5}, /* ELM_A1       0 */
  {    35,   17.5,  50,  5,  5}, /* ELM_A2       0 */
  {    35,   17.5,  50,  5,  5}, /* ELM_A3       0 */
  {   -30,      0,  50,  5,  5}, /* ELM_A4      15 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    50,     25,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"R",   10,11,11,0xA8,"r","r",ELM_FEATURE_ALV|ELM_FEATURE_APR,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   490,      0, 100,  0,  5}, /* ELM_F1       0 */
  {  1180,    590,  50,  5,  5}, /* ELM_F2       0 */
  {  1600,    740,  50,  5,  5}, /* ELM_F3     -60 */
  {    60,      0, 100,  0,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {   150,     75,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  {    42,     21,  50,  5,  5}, /* ELM_A1       0 */
  {    35,   17.5,  50,  5,  5}, /* ELM_A2       0 */
  {    35,   17.5,  50,  5,  5}, /* ELM_A3       0 */
  {   -30,      0,  50,  5,  5}, /* ELM_A4      15 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"W",   10, 8, 8,0x77,"w","w",ELM_FEATURE_APR|ELM_FEATURE_LBV|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   190,     50,  50,  4,  4}, /* ELM_F1     -45 */
  {   760,    350,  50,  4,  4}, /* ELM_F2     -30 */
  {  2020,    980,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 43.75,     21,  50,  4,  4}, /* ELM_A1  -0.875 */
  {    28,     14,  50,  4,  4}, /* ELM_A2       0 */
  {    21,   10.5,  50,  4,  4}, /* ELM_A3       0 */
  {   -30,      0,  50,  4,  4}, /* ELM_A4      15 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"Y",   10, 7, 7,0x6A,"j","j",ELM_FEATURE_APR|ELM_FEATURE_PAL|ELM_FEATURE_VCD,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   250,    110,  50,  4,  4}, /* ELM_F1     -15 */
  {  2500,   1190,  50,  4,  4}, /* ELM_F2     -60 */
  {  2980,   1460,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  { 33.25,   17.5,  50,  4,  4}, /* ELM_A2   0.875 */
  {  38.5,   17.5,  50,  4,  4}, /* ELM_A3   -1.75 */
  {  31.5,     14,  50,  4,  4}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"I",    2, 8, 6,0x49,"I","I",ELM_FEATURE_FNT|ELM_FEATURE_SMH|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   400,    170,  50,  4,  4}, /* ELM_F1     -30 */
  {  2080,   1070,  50,  4,  4}, /* ELM_F2      30 */
  {  2560,   1340,  50,  4,  4}, /* ELM_F3      60 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  { 36.75,   17.5,  50,  4,  4}, /* ELM_A2  -0.875 */
  {    35,   17.5,  50,  4,  4}, /* ELM_A3       0 */
  { 29.75,     14,  50,  4,  4}, /* ELM_A4  -0.875 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"E",    2, 8, 4,0x45,"e","E",ELM_FEATURE_FNT|ELM_FEATURE_LMD|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   640,    350,  50,  4,  4}, /* ELM_F1      30 */
  {  2020,   1070,  50,  4,  4}, /* ELM_F2      60 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  {    42,     21,  50,  4,  4}, /* ELM_A2       0 */
  {  38.5,   17.5,  50,  4,  4}, /* ELM_A3   -1.75 */
  {  31.5,     14,  50,  4,  4}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"AA",   2,10, 5,0x51,"&","&",ELM_FEATURE_FNT|ELM_FEATURE_LOW|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   790,    410,  50,  4,  4}, /* ELM_F1      15 */
  {  1780,    950,  50,  4,  4}, /* ELM_F2      60 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {   130,     65,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  { 47.25,   24.5,  50,  4,  4}, /* ELM_A2   0.875 */
  {  38.5,   17.5,  50,  4,  4}, /* ELM_A3   -1.75 */
  {  31.5,     14,  50,  4,  4}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"U",    2, 9, 6,0xC3,"V","V",ELM_FEATURE_BCK|ELM_FEATURE_LMD|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   700,    350,  50,  4,  4}, /* ELM_F1       0 */
  {  1360,    710,  50,  4,  4}, /* ELM_F2      30 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  { 43.75,     21,  50,  4,  4}, /* ELM_A2  -0.875 */
  {  31.5,     14,  50,  4,  4}, /* ELM_A3   -1.75 */
  {  24.5,   10.5,  50,  4,  4}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"O",    2, 9, 6,0x81,"0","A.",ELM_FEATURE_BCK|ELM_FEATURE_LOW|ELM_FEATURE_RND|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   610,    290,  50,  4,  4}, /* ELM_F1     -15 */
  {   880,    470,  50,  4,  4}, /* ELM_F2      30 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  { 47.25,   24.5,  50,  4,  4}, /* ELM_A2   0.875 */
  { 22.75,   10.5,  50,  4,  4}, /* ELM_A3  -0.875 */
  { 15.75,      7,  50,  4,  4}, /* ELM_A4  -0.875 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"OO",   2, 6, 4,0x55,"U","U",ELM_FEATURE_BCK|ELM_FEATURE_RND|ELM_FEATURE_SMH|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   370,    170,  50,  4,  4}, /* ELM_F1     -15 */
  {  1000,    470,  50,  4,  4}, /* ELM_F2     -30 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  {    42,     21,  50,  4,  4}, /* ELM_A2       0 */
  {    28,     14,  50,  4,  4}, /* ELM_A3       0 */
  { 22.75,   10.5,  50,  4,  4}, /* ELM_A4  -0.875 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"A",    2, 4, 4,0xAB,"@","@",ELM_FEATURE_CNT|ELM_FEATURE_MDL|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   490,    230,  50,  4,  4}, /* ELM_F1     -15 */
  {  1480,    710,  50,  4,  4}, /* ELM_F2     -30 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A2  -0.875 */
  { 33.25,   17.5,  50,  4,  4}, /* ELM_A3   0.875 */
  { 26.25,     14,  50,  4,  4}, /* ELM_A4   0.875 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"EE",   2,11, 7,0x69,"i","i",ELM_FEATURE_FNT|ELM_FEATURE_HGH|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   250,    110,  50,  4,  4}, /* ELM_F1     -15 */
  {  2320,   1190,  50,  4,  4}, /* ELM_F2      30 */
  {  3200,   1580,  50,  4,  4}, /* ELM_F3     -20 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  { 33.25,   17.5,  50,  4,  4}, /* ELM_A2   0.875 */
  { 36.75,   17.5,  50,  4,  4}, /* ELM_A3  -0.875 */
  {  31.5,     14,  50,  4,  4}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"ER",   2,16,16,0xCE,"3","V\"",ELM_FEATURE_CNT|ELM_FEATURE_LMD|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   580,    290,  50,  4,  4}, /* ELM_F1       0 */
  {  1420,    710,  50,  4,  4}, /* ELM_F2       0 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  {  45.5,     21,  50,  4,  4}, /* ELM_A2   -1.75 */
  { 33.25,   17.5,  50,  4,  4}, /* ELM_A3   0.875 */
  { 26.25,     14,  50,  4,  4}, /* ELM_A4   0.875 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"AR",   2,15,15,0x41,"A","A",ELM_FEATURE_BCK|ELM_FEATURE_LOW|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   790,    410,  50,  4,  4}, /* ELM_F1      15 */
  {   880,    470,  50,  4,  4}, /* ELM_F2      30 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  {    49,   24.5,  50,  4,  4}, /* ELM_A2       0 */
  { 29.75,     14,  50,  4,  4}, /* ELM_A3  -0.875 */
  { 22.75,   10.5,  50,  4,  4}, /* ELM_A4  -0.875 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"AW",   2,16,10,0x8D,"O","O",ELM_FEATURE_BCK|ELM_FEATURE_LMD|ELM_FEATURE_RND|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   490,    230,  50,  4,  4}, /* ELM_F1     -15 */
  {   820,    470,  50,  4,  4}, /* ELM_F2      60 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  {  45.5,     21,  50,  4,  4}, /* ELM_A2   -1.75 */
  { 22.75,   10.5,  50,  4,  4}, /* ELM_A3  -0.875 */
  {  17.5,      7,  50,  4,  4}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"UU",   2,14, 9,0x75,"u","u",ELM_FEATURE_BCK|ELM_FEATURE_HGH|ELM_FEATURE_RND|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   250,    110,  50,  4,  4}, /* ELM_F1     -15 */
  {   880,    470,  50,  4,  4}, /* ELM_F2      30 */
  {  2200,   1100,  50,  4,  4}, /* ELM_F3       0 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  {  38.5,   17.5,  50,  4,  4}, /* ELM_A2   -1.75 */
  {  17.5,      7,  50,  4,  4}, /* ELM_A3   -1.75 */
  {  10.5,    3.5,  50,  4,  4}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"AI",   2, 9, 6,0x45,"e","E",ELM_FEATURE_FNT|ELM_FEATURE_LMD|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   640,    290,  50,  5,  5}, /* ELM_F1     -30 */
  {  1600,    830,  50,  5,  5}, /* ELM_F2      30 */
  {  2500,   1220,  50,  5,  5}, /* ELM_F3     -30 */
  {    60,     30,  50,  5,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {   150,     75,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  5,  5}, /* ELM_A1  -0.875 */
  {  45.5,     21,  50,  5,  5}, /* ELM_A2   -1.75 */
  {    35,   17.5,  50,  5,  5}, /* ELM_A3       0 */
  { 29.75,     14,  50,  5,  5}, /* ELM_A4  -0.875 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"IE",   2, 9, 6,0x61,"a","a",ELM_FEATURE_CNT|ELM_FEATURE_LOW|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   790,    410,  50,  5,  5}, /* ELM_F1      15 */
  {   880,    470,  50,  5,  5}, /* ELM_F2      30 */
  {  2500,   1220,  50,  5,  5}, /* ELM_F3     -30 */
  {    60,     30,  50,  5,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {   150,     75,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  5,  5}, /* ELM_A1  -0.875 */
  {    49,   24.5,  50,  5,  5}, /* ELM_A2       0 */
  { 29.75,     14,  50,  5,  5}, /* ELM_A3  -0.875 */
  { 22.75,   10.5,  50,  5,  5}, /* ELM_A4  -0.875 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"OI",   2, 9, 6,0x6F,"o","o",ELM_FEATURE_BCK|ELM_FEATURE_RND|ELM_FEATURE_UMD|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   490,    230,  50,  5,  5}, /* ELM_F1     -15 */
  {   820,    350,  50,  5,  5}, /* ELM_F2     -60 */
  {  2500,   1220,  50,  5,  5}, /* ELM_F3     -30 */
  {    60,     30,  50,  5,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {   150,     75,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  5,  5}, /* ELM_A1  -0.875 */
  {  45.5,     21,  50,  5,  5}, /* ELM_A2   -1.75 */
  { 22.75,   10.5,  50,  5,  5}, /* ELM_A3  -0.875 */
  {  17.5,      7,  50,  5,  5}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"OU",   2, 9, 6,0x61,"a","a",ELM_FEATURE_CNT|ELM_FEATURE_LOW|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   790,    410,  50,  5,  5}, /* ELM_F1      15 */
  {  1300,    590,  50,  5,  5}, /* ELM_F2     -60 */
  {  2500,   1220,  50,  5,  5}, /* ELM_F3     -30 */
  {    60,     30,  50,  5,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {   150,     75,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  5,  5}, /* ELM_A1  -0.875 */
  { 47.25,   24.5,  50,  5,  5}, /* ELM_A2   0.875 */
  {    35,   17.5,  50,  5,  5}, /* ELM_A3       0 */
  {    28,     14,  50,  5,  5}, /* ELM_A4       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"OV",   2, 8, 6,0x55,"U","U",ELM_FEATURE_BCK|ELM_FEATURE_RND|ELM_FEATURE_SMH|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   370,    170,  50,  4,  4}, /* ELM_F1     -15 */
  {  1000,    470,  50,  4,  4}, /* ELM_F2     -30 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  {    42,     21,  50,  4,  4}, /* ELM_A2       0 */
  {    28,     14,  50,  4,  4}, /* ELM_A3       0 */
  { 22.75,   10.5,  50,  4,  4}, /* ELM_A4  -0.875 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"OA",   2, 9, 6,0xAB,"@","@",ELM_FEATURE_CNT|ELM_FEATURE_MDL|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   490,    230,  50,  5,  5}, /* ELM_F1     -15 */
  {  1480,    710,  50,  5,  5}, /* ELM_F2     -30 */
  {  2500,   1220,  50,  5,  5}, /* ELM_F3     -30 */
  {    60,     30,  50,  5,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {   150,     75,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  5,  5}, /* ELM_A1  -0.875 */
  { 50.75,   24.5,  50,  5,  5}, /* ELM_A2  -0.875 */
  { 33.25,   17.5,  50,  5,  5}, /* ELM_A3   0.875 */
  { 26.25,     14,  50,  5,  5}, /* ELM_A4   0.875 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"IA",   2, 9, 6,0x49,"I","I",ELM_FEATURE_FNT|ELM_FEATURE_SMH|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   310,    170,  50,  5,  5}, /* ELM_F1      15 */
  {  2200,   1070,  50,  5,  5}, /* ELM_F2     -30 */
  {  2920,   1460,  50,  5,  5}, /* ELM_F3       0 */
  {    60,     30,  50,  5,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {   150,     75,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  5,  5}, /* ELM_A1  -0.875 */
  {    35,   17.5,  50,  5,  5}, /* ELM_A2       0 */
  { 36.75,   17.5,  50,  5,  5}, /* ELM_A3  -0.875 */
  {  31.5,     14,  50,  5,  5}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"IB",   2, 8, 6,0x51,"@","@",ELM_FEATURE_FNT|ELM_FEATURE_LOW|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   490,    230,  50,  4,  4}, /* ELM_F1     -15 */
  {  1480,    710,  50,  4,  4}, /* ELM_F2     -30 */
  {  2500,   1220,  50,  4,  4}, /* ELM_F3     -30 */
  {    60,     30,  50,  4,  4}, /* ELM_B1       0 */
  {    90,     45,  50,  4,  4}, /* ELM_B2       0 */
  {   150,     75,  50,  4,  4}, /* ELM_B3       0 */
  {   -30,      0, 100,  4,  4}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A1  -0.875 */
  { 50.75,   24.5,  50,  4,  4}, /* ELM_A2  -0.875 */
  { 33.25,   17.5,  50,  4,  4}, /* ELM_A3   0.875 */
  { 26.25,     14,  50,  4,  4}, /* ELM_A4   0.875 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A5       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_A6       0 */
  {   -30,    -15,  50,  4,  4}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"AIR",  2, 9, 6,0x45,"e","E",ELM_FEATURE_FNT|ELM_FEATURE_LMD|ELM_FEATURE_UNR|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   640,    350,  50,  5,  5}, /* ELM_F1      30 */
  {  2020,   1070,  50,  5,  5}, /* ELM_F2      60 */
  {  2500,   1220,  50,  5,  5}, /* ELM_F3     -30 */
  {    60,     30,  50,  5,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {   150,     75,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  5,  5}, /* ELM_A1  -0.875 */
  {    42,     21,  50,  5,  5}, /* ELM_A2       0 */
  {  38.5,   17.5,  50,  5,  5}, /* ELM_A3   -1.75 */
  {  31.5,     14,  50,  5,  5}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"OOR",  2, 9, 6,0x55,"U","U",ELM_FEATURE_BCK|ELM_FEATURE_RND|ELM_FEATURE_SMH|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   370,    170,  50,  5,  5}, /* ELM_F1     -15 */
  {  1000,    470,  50,  5,  5}, /* ELM_F2     -30 */
  {  2500,   1220,  50,  5,  5}, /* ELM_F3     -30 */
  {    60,     30,  50,  5,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {   150,     75,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  5,  5}, /* ELM_A1  -0.875 */
  {    42,     21,  50,  5,  5}, /* ELM_A2       0 */
  {    28,     14,  50,  5,  5}, /* ELM_A3       0 */
  { 22.75,      7,  50,  5,  5}, /* ELM_A4  -4.375 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
},

{"OR",   2, 9, 6,0x8D,"O","O",ELM_FEATURE_BCK|ELM_FEATURE_LMD|ELM_FEATURE_RND|ELM_FEATURE_VWL,
 {
  {   270,    135,  50,  0,  0}, /* ELM_FN       0 */
  {   490,    230,  50,  5,  5}, /* ELM_F1     -15 */
  {   820,    470,  50,  5,  5}, /* ELM_F2      60 */
  {  2500,   1220,  50,  5,  5}, /* ELM_F3     -30 */
  {    60,     30,  50,  5,  5}, /* ELM_B1       0 */
  {    90,     45,  50,  5,  5}, /* ELM_B2       0 */
  {   150,     75,  50,  5,  5}, /* ELM_B3       0 */
  {   -30,      0, 100,  5,  5}, /* ELM_AN       0 */
  { 50.75,   24.5,  50,  5,  5}, /* ELM_A1  -0.875 */
  {  45.5,     21,  50,  5,  5}, /* ELM_A2   -1.75 */
  { 22.75,   10.5,  50,  5,  5}, /* ELM_A3  -0.875 */
  {  17.5,      7,  50,  5,  5}, /* ELM_A4   -1.75 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A5       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_A6       0 */
  {   -30,    -15,  50,  5,  5}, /* ELM_AB       0 */
  {    62,     31,  50,  0,  0}, /* ELM_AV       0 */
  {    16,      8,  50,  0,  0}, /* ELM_AVC      0 */
  {     0,      0,  50,  0,  0}, /* ELM_ASP      0 */
  {     0,      0,  50,  0,  0}  /* ELM_AF       0 */
 }
} 

};

static short clip(float input)
{
    int temp = (int)input;
    /* clip on boundaries of 16-bit word */

    if (temp < -32767)
    {
        //assert?
        temp = -32767;
    }
    else
    if (temp > 32767)
    {
        //assert?
        temp = 32767;
    }

    return (short)(temp);
}

/* Convert from decibels to a linear scale factor */
static float DBtoLIN(int dB)
{
    /*
    * Convertion table, db to linear, 87 dB --> 32767
    *                                 86 dB --> 29491 (1 dB down = 0.5**1/6)
    *                                 ...
    *                                 81 dB --> 16384 (6 dB down = 0.5)
    *                                 ...
    *                                  0 dB -->     0
    *
    * The just noticeable difference for a change in intensity of a vowel
    *   is approximately 1 dB.  Thus all amplitudes are quantized to 1 dB
    *   steps.
    */

    static const float amptable[88] =
    {
        0.0, 0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 6.0, 7.0,
        8.0, 9.0, 10.0, 11.0, 13.0,
        14.0, 16.0, 18.0, 20.0, 22.0,
        25.0, 28.0, 32.0, 35.0, 40.0,
        45.0, 51.0, 57.0, 64.0, 71.0,
        80.0, 90.0, 101.0, 114.0, 128.0,
        142.0, 159.0, 179.0, 202.0, 227.0,
        256.0, 284.0, 318.0, 359.0, 405.0,
        455.0, 512.0, 568.0, 638.0, 719.0,
        811.0, 911.0, 1024.0, 1137.0, 1276.0,
        1438.0, 1622.0, 1823.0, 2048.0, 2273.0,
        2552.0, 2875.0, 3244.0, 3645.0, 4096.0,
        4547.0, 5104.0, 5751.0, 6488.0, 7291.0,
        8192.0, 9093.0, 10207.0, 11502.0, 12976.0,
        14582.0, 16384.0, 18350.0, 20644.0, 23429.0,
        26214.0, 29491.0, 32767.0
    };

    // Check limits or argument (can be removed in final product)
    if (dB < 0)
    {
        dB = 0;
    }
    else
    if (dB >= 88)
    {
        dB = 87;
    }

    return amptable[dB] * 0.001f;
}



klatt_frame::klatt_frame() :
    mF0FundamentalFreq(1330),   mVoicingAmpdb(60),              mFormant1Freq(500),  
    mFormant1Bandwidth(60),     mFormant2Freq(1500),            mFormant2Bandwidth(90),
    mFormant3Freq(2800),        mFormant3Bandwidth(150),        mFormant4Freq(3250), 
    mFormant4Bandwidth(200),    mFormant5Freq(3700),            mFormant5Bandwidth(200),  
    mFormant6Freq(4990),        mFormant6Bandwidth(500),        mNasalZeroFreq(270),  
    mNasalZeroBandwidth(100),   mNasalPoleFreq(270),            mNasalPoleBandwidth(100), 
    mAspirationAmpdb(0),        mNoSamplesInOpenPeriod(30),     mVoicingBreathiness(0),      
    mVoicingSpectralTiltdb(10), mFricationAmpdb(0),             mSkewnessOfAlternatePeriods(0),   
    mFormant1Ampdb(0),          mFormant1ParallelBandwidth(80), mFormant2Ampdb(0),      
    mFormant2ParallelBandwidth(200), mFormant3Ampdb(0),         mFormant3ParallelBandwidth(350),
    mFormant4Ampdb(0),          mFormant4ParallelBandwidth(500), mFormant5Ampdb(0),      
    mFormant5ParallelBandwidth(600), mFormant6Ampdb(0),         mFormant6ParallelBandwidth(800),    
    mParallelNasalPoleAmpdb(0), mBypassFricationAmpdb(0),       mPalallelVoicingAmpdb(0),   
    mOverallGaindb(62)  
{
};


klatt::klatt() : 
    mF0Flutter(0), 
    mSampleRate(0), 
    mNspFr(0),
    mF0FundamentalFreq(0),
    mVoicingAmpdb(0),
    mSkewnessOfAlternatePeriods(0),
    mTimeCount(0), 
    mNPer(0),
    mT0(0),
    mNOpen(0),
    mNMod(0),
    mAmpVoice(0),
    mAmpBypas(0),
    mAmpAspir(0),
    mAmpFrica(0),
    mAmpBreth(0),
    mSkew(0),
    mNatglotA(0),
    mNatglotB(0),
    mVWave(0),
    mVLast(0),
    mNLast(0),
    mGlotLast(0),
    mDecay(0),
    mOneMd(0),
    mElementCount(0),
    mElement(0),
    mElementIndex(0),
    mLastElement(0),
    mTStress(0),
    mNTStress(0),
    mTop(0)
{
}

/*
function FLUTTER

This function adds F0 flutter, as specified in:

"Analysis, synthesis and perception of voice quality variations among
female and male talkers" D.H. Klatt and L.C. Klatt JASA 87(2) February 1990.
Flutter is added by applying a quasi-random element constructed from three
slowly varying sine waves.
*/
void klatt::flutter(klatt_frame *pars)
{
    int original_f0 = pars->mF0FundamentalFreq / 10;
    float fla = (float) mF0Flutter / 50;
    float flb = (float) original_f0 / 100;
    float flc = (float)sin(2 * PI * 12.7 * mTimeCount);
    float fld = (float)sin(2 * PI * 7.1 * mTimeCount);
    float fle = (float)sin(2 * PI * 4.7 * mTimeCount);
    float delta_f0 = fla * flb * (flc + fld + fle) * 10;
    mF0FundamentalFreq += (int) delta_f0;
}

/* Vwave is the differentiated glottal flow waveform, there is a weak
spectral zero around 800 Hz, magic constants a,b reset pitch-synch
*/

float klatt::natural_source(int aNper)
{
    // See if glottis open 

    if (aNper < mNOpen)
    {
        float lgtemp;
        mNatglotA -= mNatglotB;
        mVWave += mNatglotA;
        lgtemp = mVWave * 0.028f;        /* function of samp_rate ? */
        return (lgtemp);
    }
    else
    {
        // Glottis closed 
        mVWave = 0.0;
        return (0.0);
    }
}

/* Reset selected parameters pitch-synchronously */

void klatt::pitch_synch_par_reset(klatt_frame *frame, int ns)
{
    /*
    * Constant natglot[] controls shape of glottal pulse as a function
    * of desired duration of open phase N0
    * (Note that N0 is specified in terms of 40,000 samples/sec of speech)
    *
    *    Assume voicing waveform V(t) has form: k1 t**2 - k2 t**3
    *
    *    If the radiation characterivative, a temporal derivative
    *      is folded in, and we go from continuous time to discrete
    *      integers n:  dV/dt = mVWave[n]
    *                         = sum over i=1,2,...,n of { a - (i * b) }
    *                         = a n  -  b/2 n**2
    *
    *      where the  constants a and b control the detailed shape
    *      and amplitude of the voicing waveform over the open
    *      potion of the voicing cycle "mNOpen".
    *
    *    Let integral of dV/dt have no net dc flow --> a = (b * mNOpen) / 3
    *
    *    Let maximum of dUg(n)/dn be constant --> b = gain / (mNOpen * mNOpen)
    *      meaning as mNOpen gets bigger, V has bigger peak proportional to n
    *
    *    Thus, to generate the table below for 40 <= mNOpen <= 263:
    *
    *      natglot[mNOpen - 40] = 1920000 / (mNOpen * mNOpen)
    */
    static const short natglot[224] =
    {
        1200, 1142, 1088, 1038, 991, 948, 907, 869, 833, 799,
        768, 738, 710, 683, 658, 634, 612, 590, 570, 551,
        533, 515, 499, 483, 468, 454, 440, 427, 415, 403,
        391, 380, 370, 360, 350, 341, 332, 323, 315, 307,
        300, 292, 285, 278, 272, 265, 259, 253, 247, 242,
        237, 231, 226, 221, 217, 212, 208, 204, 199, 195,
        192, 188, 184, 180, 177, 174, 170, 167, 164, 161,
        158, 155, 153, 150, 147, 145, 142, 140, 137, 135,
        133, 131, 128, 126, 124, 122, 120, 119, 117, 115,
        113, 111, 110, 108, 106, 105, 103, 102, 100, 99,
        97, 96, 95, 93, 92, 91, 90, 88, 87, 86,
        85, 84, 83, 82, 80, 79, 78, 77, 76, 75,
        75, 74, 73, 72, 71, 70, 69, 68, 68, 67,
        66, 65, 64, 64, 63, 62, 61, 61, 60, 59,
        59, 58, 57, 57, 56, 56, 55, 55, 54, 54,
        53, 53, 52, 52, 51, 51, 50, 50, 49, 49,
        48, 48, 47, 47, 46, 46, 45, 45, 44, 44,
        43, 43, 42, 42, 41, 41, 41, 41, 40, 40,
        39, 39, 38, 38, 38, 38, 37, 37, 36, 36,
        36, 36, 35, 35, 35, 35, 34, 34, 33, 33,
        33, 33, 32, 32, 32, 32, 31, 31, 31, 31,
        30, 30, 30, 30, 29, 29, 29, 29, 28, 28,
        28, 28, 27, 27
    };

    if (mF0FundamentalFreq > 0)
    {
        mT0 = (40 * mSampleRate) / mF0FundamentalFreq;

        /* Period in samp*4 */
        mAmpVoice = DBtoLIN(mVoicingAmpdb);

        /* Duration of period before amplitude modulation */
        mNMod = mT0;

        if (mVoicingAmpdb > 0)
        {
            mNMod >>= 1;
        }

        /* Breathiness of voicing waveform */

        mAmpBreth = DBtoLIN(frame->mVoicingBreathiness) * 0.1f;

        /* Set open phase of glottal period */
        /* where  40 <= open phase <= 263 */

        mNOpen = 4 * frame->mNoSamplesInOpenPeriod;

        if (mNOpen >= (mT0 - 1))
        {
            mNOpen = mT0 - 2;
        }

        if (mNOpen < 40)
        {
            mNOpen = 40;                  /* F0 max = 1000 Hz */
        }

        /* Reset a & b, which determine shape of "natural" glottal waveform */

        mNatglotB = natglot[mNOpen - 40];
        mNatglotA = (mNatglotB * mNOpen) * .333f;

        /* Reset width of "impulsive" glottal pulse */

        int temp;
        float temp1;

        temp = mSampleRate / mNOpen;
        mCritDampedGlotLowPassFilter.initResonator(0L, temp, mSampleRate);

        /* Make gain at F1 about constant */

        temp1 = mNOpen * .00833f;
        mCritDampedGlotLowPassFilter.setGain(temp1 * temp1);

        /* Truncate skewness so as not to exceed duration of closed phase
        of glottal period */

        temp = mT0 - mNOpen;

        if (mSkewnessOfAlternatePeriods > temp)
        {
            mSkewnessOfAlternatePeriods = temp;
        }

        if (mSkew >= 0)
        {
            mSkew = mSkewnessOfAlternatePeriods;                /* Reset mSkew to requested mSkewnessOfAlternatePeriods */
        }
        else
        {
            mSkew = -mSkewnessOfAlternatePeriods;
        }

        /* Add skewness to closed portion of voicing period */

        mT0 = mT0 + mSkew;
        mSkew = -mSkew;
    }
    else
    {
        mT0 = 4;                        /* Default for f0 undefined */
        mAmpVoice = 0.0;
        mNMod = mT0;
        mAmpBreth = 0.0;
        mNatglotA = 0.0;
        mNatglotB = 0.0;
    }

    /* Reset these pars pitch synchronously or at update rate if f0=0 */

    if ((mT0 != 4) || (ns == 0))
    {
        /* Set one-pole ELM_FEATURE_LOW-pass filter that tilts glottal source */
        mDecay = (0.033f * frame->mVoicingSpectralTiltdb);  /* Function of samp_rate ? */

        if (mDecay > 0.0f)
        {
            mOneMd = 1.0f - mDecay;
        }
        else
        {
            mOneMd = 1.0f;
        }
    }
}


/* Get variable parameters from host computer,
initially also get definition of fixed pars
*/

void klatt::frame_init(klatt_frame *frame)
{
    int mOverallGaindb;                       /* Overall gain, 60 dB is unity  0 to   60  */
    float amp_parF1;                 /* mFormant1Ampdb converted to linear gain  */
    float amp_parFN;                 /* mParallelNasalPoleAmpdb converted to linear gain  */
    float amp_parF2;                 /* mFormant2Ampdb converted to linear gain  */
    float amp_parF3;                 /* mFormant3Ampdb converted to linear gain  */
    float amp_parF4;                 /* mFormant4Ampdb converted to linear gain  */
    float amp_parF5;                 /* mFormant5Ampdb converted to linear gain  */
    float amp_parF6;                 /* mFormant6Ampdb converted to linear gain  */

    /* Read  speech frame definition into temp store
       and move some parameters into active use immediately
       (voice-excited ones are updated pitch synchronously
       to avoid waveform glitches).
     */

    mF0FundamentalFreq = frame->mF0FundamentalFreq;
    mVoicingAmpdb = frame->mVoicingAmpdb - 7;

    if (mVoicingAmpdb < 0) mVoicingAmpdb = 0;

    mAmpAspir = DBtoLIN(frame->mAspirationAmpdb) * .05f;
    mAmpFrica = DBtoLIN(frame->mFricationAmpdb) * 0.25f;
    mSkewnessOfAlternatePeriods = frame->mSkewnessOfAlternatePeriods;

    /* Fudge factors (which comprehend affects of formants on each other?)
       with these in place ALL_PARALLEL should sound as close as
       possible to CASCADE_PARALLEL.
       Possible problem feeding in Holmes's amplitudes given this.
    */
    amp_parF1 = DBtoLIN(frame->mFormant1Ampdb) * 0.4f;  /* -7.96 dB */
    amp_parF2 = DBtoLIN(frame->mFormant2Ampdb) * 0.15f; /* -16.5 dB */
    amp_parF3 = DBtoLIN(frame->mFormant3Ampdb) * 0.06f; /* -24.4 dB */
    amp_parF4 = DBtoLIN(frame->mFormant4Ampdb) * 0.04f; /* -28.0 dB */
    amp_parF5 = DBtoLIN(frame->mFormant5Ampdb) * 0.022f;    /* -33.2 dB */
    amp_parF6 = DBtoLIN(frame->mFormant6Ampdb) * 0.03f; /* -30.5 dB */
    amp_parFN = DBtoLIN(frame->mParallelNasalPoleAmpdb) * 0.6f; /* -4.44 dB */
    mAmpBypas = DBtoLIN(frame->mBypassFricationAmpdb) * 0.05f;  /* -26.0 db */

    // Set coeficients of nasal resonator and zero antiresonator 
    mNasalPole.initResonator(frame->mNasalPoleFreq, frame->mNasalPoleBandwidth, mSampleRate);

    mNasalZero.initAntiresonator(frame->mNasalZeroFreq, frame->mNasalZeroBandwidth, mSampleRate);

    // Set coefficients of parallel resonators, and amplitude of outputs 
    mParallelFormant1.initResonator(frame->mFormant1Freq, frame->mFormant1ParallelBandwidth, mSampleRate);
    mParallelFormant1.setGain(amp_parF1);

    mParallelResoNasalPole.initResonator(frame->mNasalPoleFreq, frame->mNasalPoleBandwidth, mSampleRate);
    mParallelResoNasalPole.setGain(amp_parFN);

    mParallelFormant2.initResonator(frame->mFormant2Freq, frame->mFormant2ParallelBandwidth, mSampleRate);
    mParallelFormant2.setGain(amp_parF2);

    mParallelFormant3.initResonator(frame->mFormant3Freq, frame->mFormant3ParallelBandwidth, mSampleRate);
    mParallelFormant3.setGain(amp_parF3);

    mParallelFormant4.initResonator(frame->mFormant4Freq, frame->mFormant4ParallelBandwidth, mSampleRate);
    mParallelFormant4.setGain(amp_parF4);

    mParallelFormant5.initResonator(frame->mFormant5Freq, frame->mFormant5ParallelBandwidth, mSampleRate);
    mParallelFormant5.setGain(amp_parF5);

    mParallelFormant6.initResonator(frame->mFormant6Freq, frame->mFormant6ParallelBandwidth, mSampleRate);
    mParallelFormant6.setGain(amp_parF6);


    /* fold overall gain into output resonator */
    mOverallGaindb = frame->mOverallGaindb - 3;

    if (mOverallGaindb <= 0)
        mOverallGaindb = 57;

    /* output ELM_FEATURE_LOW-pass filter - resonator with freq 0 and BW = globals->mSampleRate
    Thus 3db point is globals->mSampleRate/2 i.e. Nyquist limit.
    Only 3db down seems rather mild...
    */
    mOutputLowPassFilter.initResonator(0L, (int) mSampleRate, mSampleRate);
    mOutputLowPassFilter.setGain(DBtoLIN(mOverallGaindb));
}

/*
function PARWAV

CONVERT FRAME OF PARAMETER DATA TO A WAVEFORM CHUNK
Synthesize globals->mNspFr samples of waveform and store in jwave[].
*/

void klatt::parwave(klatt_frame *frame, short int *jwave)
{
    /* Output of cascade branch, also final output  */

    /* Initialize synthesizer and get specification for current speech
    frame from host microcomputer */

    frame_init(frame);

    if (mF0Flutter != 0)
    {
        mTimeCount++;                  /* used for f0 flutter */
        flutter(frame);       /* add f0 flutter */
    }

    /* MAIN LOOP, for each output sample of current frame: */

    int ns;
    for (ns = 0; ns < mNspFr; ns++)
    {
        static unsigned int seed = 5; /* Fixed staring value */
        float noise;
        int n4;
        float sourc;                   /* Sound source if all-parallel config used  */
        float glotout;                 /* Output of glottal sound source  */
        float par_glotout;             /* Output of parallelglottal sound sourc  */
        float voice = 0;               /* Current sample of voicing waveform  */
        float frics;                   /* Frication sound source  */
        float aspiration;              /* Aspiration sound source  */
        int nrand;                    /* Varible used by random number generator  */

        /* Our own code like rand(), but portable
        whole upper 31 bits of seed random
        assumes 32-bit unsigned arithmetic
        with untested code to handle larger.
        */
        seed = seed * 1664525 + 1;

        if (8 * sizeof(unsigned int) > 32)
            seed &= 0xFFFFFFFF;

        /* Shift top bits of seed up to top of int then back down to LS 14 bits */
        /* Assumes 8 bits per sizeof unit i.e. a "byte" */
        nrand = (((int) seed) << (8 * sizeof(int) - 32)) >> (8 * sizeof(int) - 14);

        /* Tilt down noise spectrum by soft ELM_FEATURE_LOW-pass filter having
        *    a pole near the origin in the z-plane, i.e.
        *    output = input + (0.75 * lastoutput) */

        noise = nrand + (0.75f * mNLast);   /* Function of samp_rate ? */

        mNLast = noise;

        /* Amplitude modulate noise (reduce noise amplitude during
        second half of glottal period) if voicing simultaneously present
        */

        if (mNPer > mNMod)
        {
            noise *= 0.5f;
        }

        /* Compute frication noise */
        sourc = frics = mAmpFrica * noise;

        /* Compute voicing waveform : (run glottal source simulation at
        4 times normal sample rate to minimize quantization noise in
        period of female voice)
        */

        for (n4 = 0; n4 < 4; n4++)
        {
            /* use a more-natural-shaped source waveform with excitation
            occurring both upon opening and upon closure, stronest at closure */
            voice = natural_source(mNPer);

            /* Reset period when counter 'mNPer' reaches mT0 */

            if (mNPer >= mT0)
            {
                mNPer = 0;
                pitch_synch_par_reset(frame, ns);
            }

            /* Low-pass filter voicing waveform before downsampling from 4*globals->mSampleRate */
            /* to globals->mSampleRate samples/sec.  Resonator f=.09*globals->mSampleRate, bw=.06*globals->mSampleRate  */

            voice = mDownSampLowPassFilter.resonate(voice); /* in=voice, out=voice */

            /* Increment counter that keeps track of 4*globals->mSampleRate samples/sec */
            mNPer++;
        }

        /* Tilt spectrum of voicing source down by soft ELM_FEATURE_LOW-pass filtering, amount
        of tilt determined by mVoicingSpectralTiltdb
        */
        voice = (voice * mOneMd) + (mVLast * mDecay);

        mVLast = voice;

        /* Add breathiness during glottal open phase */
        if (mNPer < mNOpen)
        {
            /* Amount of breathiness determined by parameter mVoicingBreathiness */
            /* Use nrand rather than noise because noise is ELM_FEATURE_LOW-passed */
            voice += mAmpBreth * nrand;
        }

        /* Set amplitude of voicing */
        glotout = mAmpVoice * voice;

        /* Compute aspiration amplitude and add to voicing source */
        aspiration = mAmpAspir * noise;

        glotout += aspiration;

        par_glotout = glotout;

        /* NIS - rsynth "hack"
        As Holmes' scheme is weak at nasals and (physically) nasal cavity
        is "back near glottis" feed glottal source through nasal resonators
        Don't think this is quite right, but improves things a bit
        */
        par_glotout = mNasalZero.antiresonate(par_glotout);
        par_glotout = mNasalPole.resonate(par_glotout);
        /* And just use mParallelFormant1 NOT mParallelResoNasalPole */     
        float out = mParallelFormant1.resonate(par_glotout);
        /* Sound sourc for other parallel resonators is frication
        plus first difference of voicing waveform.
        */
        sourc += (par_glotout - mGlotLast);
        mGlotLast = par_glotout;

        /* Standard parallel vocal tract
        Formants F6,F5,F4,F3,F2, outputs added with alternating sign
        */
        out = mParallelFormant6.resonate(sourc) - out;
        out = mParallelFormant5.resonate(sourc) - out;
        out = mParallelFormant4.resonate(sourc) - out;
        out = mParallelFormant3.resonate(sourc) - out;
        out = mParallelFormant2.resonate(sourc) - out;
        out = mAmpBypas * sourc - out;
        out = mOutputLowPassFilter.resonate(out);

        *jwave++ = clip(out); /* Convert back to integer */
    }
}



static char * phoneme_to_element_lookup(char *s, void ** data)
{
    int key8 = *s;
    int key16 = key8 + (s[1] << 8);
    if (s[1] == 0) key16 = -1; // avoid key8==key16
    int i;
    for (i = 0; i < PHONEME_COUNT; i++)
    {
        if (phoneme_to_elements[i].mKey == key16)
        {
            *data = &phoneme_to_elements[i].mData;
            return s+2;
        }
        if (phoneme_to_elements[i].mKey == key8)
        {
            *data = &phoneme_to_elements[i].mData;
            return s+1;
        }
    }
    // should never happen
    *data = NULL;
    return s+1;
}



int klatt::phone_to_elm(char *aPhoneme, int aCount, darray *aElement)
{
    int stress = 0;
    char *s = aPhoneme;
    int t = 0;
    char *limit = s + aCount;

    while (s < limit && *s)
    {
        char *e = NULL;
        s = phoneme_to_element_lookup(s, (void**)&e);

        if (e)
        {
            int n = *e++;

            while (n-- > 0)
            {
                int x = *e++;
                Element * p = &gElement[x];
                /* This works because only vowels have mUD != mDU,
                and we set stress just before a vowel
                */
                aElement->put(x);

                if (!(p->mFeat & ELM_FEATURE_VWL))
                    stress = 0;

                int stressdur = StressDur(p,stress);

                t += stressdur;

                aElement->put(stressdur);
                aElement->put(stress);
            }
        }

        else
        {
            char ch = *s++;

            switch (ch)
            {

            case '\'':                /* Primary stress */
                stress = 3;
                break;

            case ',':                 /* Secondary stress */
                stress = 2;
                break;

            case '+':                 /* Tertiary stress */
                stress = 1;
                break;

            case '-':                 /* hyphen in input */
                break;

            default:
//              fprintf(stderr, "Ignoring %c in '%.*s'\n", ch, aCount, aPhoneme);
                break;
            }
        }
    }

    return t;
}



/* 'a' is dominant element, 'b' is dominated
    ext is flag to say to use external times from 'a' rather
    than internal i.e. ext != 0 if 'a' is NOT current element.
 */

static void set_trans(Slope *t, Element * a, Element * b,int ext, int /* e */)
{
    int i;

    for (i = 0; i < ELM_COUNT; i++)
    {
        t[i].mTime = ((ext) ? a->mInterpolator[i].mExtDelay : a->mInterpolator[i].mIntDelay);

        if (t[i].mTime)
        {
            t[i].mValue = a->mInterpolator[i].mFixed + (a->mInterpolator[i].mProportion * b->mInterpolator[i].mSteady) * 0.01f; // mProportion is in scale 0..100, so *0.01.
        }
        else
        {
            t[i].mValue = b->mInterpolator[i].mSteady;
        }
    }
}

static float lerp(float a, float b, int t, int d)
{
    if (t <= 0)
    {
        return a;
    }

    if (t >= d)
    {
        return b;
    }

    float f = (float)t / (float)d;
    return a + (b - a) * f;
}

static float interpolate(Slope *aStartSlope, Slope *aEndSlope, float aMidValue, int aTime, int aDuration)
{
    int steadyTime = aDuration - (aStartSlope->mTime + aEndSlope->mTime);

    if (steadyTime >= 0)
    {
        // Interpolate to a midpoint, stay there for a while, then interpolate to end

        if (aTime < aStartSlope->mTime)
        {
            // interpolate to the first value
            return lerp(aStartSlope->mValue, aMidValue, aTime, aStartSlope->mTime);
        }
        // reached midpoint

        aTime -= aStartSlope->mTime;

        if (aTime <= steadyTime)
        {
            // still at steady state
            return aMidValue;  
        }

        // interpolate to the end
        return lerp(aMidValue, aEndSlope->mValue, aTime - steadyTime, aEndSlope->mTime);
    }
    else
    {
        // No steady state
        float f = 1.0f - ((float) aTime / (float) aDuration);
        float sp = lerp(aStartSlope->mValue, aMidValue, aTime, aStartSlope->mTime);
        float ep = lerp(aEndSlope->mValue, aMidValue, aDuration - aTime, aEndSlope->mTime);
        return f * sp + ((float) 1.0 - f) * ep;
    }
}



void klatt::initsynth(int aElementCount,unsigned char *aElement)
{
    mElement = aElement;
    mElementCount = aElementCount;
    mElementIndex = 0;
    mLastElement = &gElement[0];
    mTStress = 0;
    mNTStress = 0;
    mKlattFramePars.mF0FundamentalFreq = 1330;
    mTop = 1.1f * mKlattFramePars.mF0FundamentalFreq;
    mKlattFramePars.mNasalPoleFreq = (int)mLastElement->mInterpolator[ELM_FN].mSteady;
    mKlattFramePars.mFormant1ParallelBandwidth = mKlattFramePars.mFormant1Bandwidth = 60;
    mKlattFramePars.mFormant2ParallelBandwidth = mKlattFramePars.mFormant2Bandwidth = 90;
    mKlattFramePars.mFormant3ParallelBandwidth = mKlattFramePars.mFormant3Bandwidth = 150;
//  mKlattFramePars.mFormant4ParallelBandwidth = (default)

    // Set stress attack/decay slope 
    mStressS.mTime = 40;
    mStressE.mTime = 40;
    mStressE.mValue = 0.0;
}

int klatt::synth(int /* aSampleCount */, short *aSamplePointer)
{
    short *samp = aSamplePointer;

    if (mElementIndex >= mElementCount)
        return -1;

    Element * currentElement = &gElement[mElement[mElementIndex++]];
    int dur = mElement[mElementIndex++];
    mElementIndex++; // skip stress 

    // Skip zero length elements which are only there to affect
    // boundary values of adjacent elements     

    if (dur > 0)
    {
        Element * ne = (mElementIndex < mElementCount) ? &gElement[mElement[mElementIndex]] : &gElement[0];
        Slope start[ELM_COUNT];
        Slope end[ELM_COUNT];
        int t;

        if (currentElement->mRK > mLastElement->mRK)
        {
            set_trans(start, currentElement, mLastElement, 0, 's');
            // we dominate last 
        }
        else
        {
            set_trans(start, mLastElement, currentElement, 1, 's');
            // last dominates us 
        }

        if (ne->mRK > currentElement->mRK)
        {
            set_trans(end, ne, currentElement, 1, 'e');
            // next dominates us 
        }
        else
        {
            set_trans(end, currentElement, ne, 0, 'e');
            // we dominate next 
        }

        for (t = 0; t < dur; t++, mTStress++)
        {
            float base = mTop * 0.8f; // 3 * top / 5 
            float tp[ELM_COUNT];
            int j;

            if (mTStress == mNTStress)
            {
                int j = mElementIndex;
                mStressS = mStressE;
                mTStress = 0;
                mNTStress = dur;

                while (j <= mElementCount)
                {
                    Element * e   = (j < mElementCount) ? &gElement[mElement[j++]] : &gElement[0];
                    int du = (j < mElementCount) ? mElement[j++] : 0;
                    int s  = (j < mElementCount) ? mElement[j++] : 3;

                    if (s || e->mFeat & ELM_FEATURE_VWL)
                    {
                        int d = 0;

                        if (s)
                            mStressE.mValue = (float) s / 3;
                        else
                            mStressE.mValue = (float) 0.1;

                        do
                        {
                            d += du;
                            e = (j < mElementCount) ? &gElement[mElement[j++]] : &gElement[0];
                            du = mElement[j++];
                        }

                        while ((e->mFeat & ELM_FEATURE_VWL) && mElement[j++] == s);

                        mNTStress += d / 2;

                        break;
                    }

                    mNTStress += du;
                }
            }

            for (j = 0; j < ELM_COUNT; j++)
            {
                tp[j] = interpolate(&start[j], &end[j], (float) currentElement->mInterpolator[j].mSteady, t, dur);
            }

            // Now call the synth for each frame 

            mKlattFramePars.mF0FundamentalFreq = (int)(base + (mTop - base) * interpolate(&mStressS, &mStressE, (float) 0, mTStress, mNTStress));
            mKlattFramePars.mVoicingAmpdb = mKlattFramePars.mPalallelVoicingAmpdb = (int)tp[ELM_AV];
            mKlattFramePars.mFricationAmpdb = (int)tp[ELM_AF];
            mKlattFramePars.mNasalZeroFreq = (int)tp[ELM_FN];
            mKlattFramePars.mAspirationAmpdb = (int)tp[ELM_ASP];
            mKlattFramePars.mVoicingBreathiness = (int)tp[ELM_AVC];
            mKlattFramePars.mFormant1ParallelBandwidth = mKlattFramePars.mFormant1Bandwidth = (int)tp[ELM_B1];
            mKlattFramePars.mFormant2ParallelBandwidth = mKlattFramePars.mFormant2Bandwidth = (int)tp[ELM_B2];
            mKlattFramePars.mFormant3ParallelBandwidth = mKlattFramePars.mFormant3Bandwidth = (int)tp[ELM_B3];
            mKlattFramePars.mFormant1Freq = (int)tp[ELM_F1];
            mKlattFramePars.mFormant2Freq = (int)tp[ELM_F2];
            mKlattFramePars.mFormant3Freq = (int)tp[ELM_F3];

            // AMP_ADJ + is a kludge to get amplitudes up to klatt-compatible levels
                
                
            //pars.mParallelNasalPoleAmpdb  = AMP_ADJ + tp[ELM_AN];
                
            mKlattFramePars.mBypassFricationAmpdb = AMP_ADJ + (int)tp[ELM_AB];
            mKlattFramePars.mFormant5Ampdb = AMP_ADJ + (int)tp[ELM_A5];
            mKlattFramePars.mFormant6Ampdb = AMP_ADJ + (int)tp[ELM_A6];
            mKlattFramePars.mFormant1Ampdb = AMP_ADJ + (int)tp[ELM_A1];
            mKlattFramePars.mFormant2Ampdb = AMP_ADJ + (int)tp[ELM_A2];
            mKlattFramePars.mFormant3Ampdb = AMP_ADJ + (int)tp[ELM_A3];
            mKlattFramePars.mFormant4Ampdb = AMP_ADJ + (int)tp[ELM_A4];

            parwave(&mKlattFramePars, samp);

            samp += mNspFr;

            // Declination of f0 envelope 0.25Hz / cS 
            mTop -= 0.5;
        }
    }

    mLastElement = currentElement;

    /* MG: BEGIN REINIT AT LONG PAUSE */
    if( currentElement->mName[0] == 'Q' && currentElement->mName[1] == '\0' ) 
        ++count; 
    else 
        count = 0;
    if( count == 3 )
        {
        mTStress = 0;
        mNTStress = 0;
        mKlattFramePars.mF0FundamentalFreq = 1330;
        mTop = 1.1f * mKlattFramePars.mF0FundamentalFreq;
        mKlattFramePars.mNasalPoleFreq = (int)mLastElement->mInterpolator[ELM_FN].mSteady;
        mKlattFramePars.mFormant1ParallelBandwidth = mKlattFramePars.mFormant1Bandwidth = 60;
        mKlattFramePars.mFormant2ParallelBandwidth = mKlattFramePars.mFormant2Bandwidth = 90;
        mKlattFramePars.mFormant3ParallelBandwidth = mKlattFramePars.mFormant3Bandwidth = 150;
    //  mKlattFramePars.mFormant4ParallelBandwidth = (default)

        // Set stress attack/decay slope 
        mStressS.mTime = 40;
        mStressE.mTime = 40;
        mStressE.mValue = 0.0;
        }

    /* MG: END REINIT AT LONG PAUSE */

    return (int)(samp - aSamplePointer);
}


void klatt::init()
{
    mSampleRate = 11025;
    mF0Flutter = 0;

    int FLPhz = (950 * mSampleRate) / 10000;
    int BLPhz = (630 * mSampleRate) / 10000;
    mNspFr = (int)(mSampleRate * 10) / 1000;

    mDownSampLowPassFilter.initResonator(FLPhz, BLPhz, mSampleRate);

    mNPer = 0;                        /* LG */
    mT0 = 0;                          /* LG */

    mVLast = 0;                       /* Previous output of voice  */
    mNLast = 0;                       /* Previous output of random number generator  */
    mGlotLast = 0;                    /* Previous value of glotout  */
    count = 0;
}



extern int xlate_string (const char *string,darray *phone, void* memctx);


static const char *ASCII[] =
{
    "null", "", "", "",
    "", "", "", "",
    "", "", "", "",
    "", "", "", "",
    "", "", "", "",
    "", "", "", "",
    "", "", "", "",
    "", "", "", "",
    "space", "exclamation mark", "double quote", "hash",
    "dollar", "percent", "ampersand", "quote",
    "open parenthesis", "close parenthesis", "asterisk", "plus",
    "comma", "minus", "full stop", "slash",
    "zero", "one", "two", "three",
    "four", "five", "six", "seven",
    "eight", "nine", "colon", "semi colon",
    "less than", "equals", "greater than", "question mark",
#ifndef ALPHA_IN_DICT
    "at", "ay", "bee", "see",
    "dee", "e", "eff", "gee",
    "aych", "i", "jay", "kay",
    "ell", "em", "en", "ohe",
    "pee", "kju", "are", "es",
    "tee", "you", "vee", "double you",
    "eks", "why", "zed", "open bracket",
#else                             /* ALPHA_IN_DICT */
    "at", "A", "B", "C",
    "D", "E", "F", "G",
    "H", "I", "J", "K",
    "L", "M", "N", "O",
    "P", "Q", "R", "S",
    "T", "U", "V", "W",
    "X", "Y", "Z", "open bracket",
#endif                            /* ALPHA_IN_DICT */
    "back slash", "close bracket", "circumflex", "underscore",
#ifndef ALPHA_IN_DICT
    "back quote", "ay", "bee", "see",
    "dee", "e", "eff", "gee",
    "aych", "i", "jay", "kay",
    "ell", "em", "en", "ohe",
    "pee", "kju", "are", "es",
    "tee", "you", "vee", "double you",
    "eks", "why", "zed", "open brace",
#else                             /* ALPHA_IN_DICT */
    "back quote", "A", "B", "C",
    "D", "E", "F", "G",
    "H", "I", "J", "K",
    "L", "M", "N", "O",
    "P", "Q", "R", "S",
    "T", "U", "V", "W",
    "X", "Y", "Z", "open brace",
#endif                            /* ALPHA_IN_DICT */
    "vertical bar", "close brace", "tilde", "delete",
    NULL
};

/* Context definitions */
static const char Anything[] = "";
/* No context requirement */

static const char Nothing[] = " ";
/* Context is beginning or end of word */

static const char Silent[] = "";
/* No phonemes */


#define LEFT_PART       0
#define MATCH_PART      1
#define RIGHT_PART      2
#define OUT_PART        3

typedef const char *Rule[4];
/* Rule is an array of 4 character pointers */


/*0 = Punctuation */
/*
**      LEFT_PART       MATCH_PART      RIGHT_PART      OUT_PART
*/


static Rule punct_rules[] =
{
    {Anything, " ", Anything, " "},
    {Anything, "-", Anything, ""},
    {".", "'S", Anything, "z"},
    {"#:.E", "'S", Anything, "z"},
    {"#", "'S", Anything, "z"},
    {Anything, "'", Anything, ""},
    {Anything, ",", Anything, " "},
    {Anything, ".", Anything, " "},
    {Anything, "?", Anything, " "},
    {Anything, "!", Anything, " "},
    {Anything, 0, Anything, Silent},
};

static Rule A_rules[] =
{
    {Anything, "A", Nothing, "@"},
    {Nothing, "ARE", Nothing, "0r"},
    {Nothing, "AR", "O", "@r"},
    {Anything, "AR", "#", "er"},
    {"^", "AS", "#", "eIs"},
    {Anything, "A", "WA", "@"},
    {Anything, "AW", Anything, "O"},
    {" :", "ANY", Anything, "eni"},
    {Anything, "A", "^+#", "eI"},
    {"#:", "ALLY", Anything, "@li"},
    {Nothing, "AL", "#", "@l"},
    {Anything, "AGAIN", Anything, "@gen"},
    {"#:", "AG", "E", "IdZ"},
    {Anything, "A", "^+:#", "&"},
    {" :", "A", "^+ ", "eI"},
    {Anything, "A", "^%", "eI"},
    {Nothing, "ARR", Anything, "@r"},
    {Anything, "ARR", Anything, "&r"},
    {" :", "AR", Nothing, "0r"},
    {Anything, "AR", Nothing, "3"},
    {Anything, "AR", Anything, "0r"},
    {Anything, "AIR", Anything, "er"},
    {Anything, "AI", Anything, "eI"},
    {Anything, "AY", Anything, "eI"},
    {Anything, "AU", Anything, "O"},
    {"#:", "AL", Nothing, "@l"},
    {"#:", "ALS", Nothing, "@lz"},
    {Anything, "ALK", Anything, "Ok"},
    {Anything, "AL", "^", "Ol"},
    {" :", "ABLE", Anything, "eIb@l"},
    {Anything, "ABLE", Anything, "@b@l"},
    {Anything, "ANG", "+", "eIndZ"},
    {"^", "A", "^#", "eI"},
    {Anything, "A", Anything, "&"},
    {Anything, 0, Anything, Silent},
};

static Rule B_rules[] =
{
    {Nothing, "BE", "^#", "bI"},
    {Anything, "BEING", Anything, "biIN"},
    {Nothing, "BOTH", Nothing, "b@UT"},
    {Nothing, "BUS", "#", "bIz"},
    {Anything, "BUIL", Anything, "bIl"},
    {Anything, "B", Anything, "b"},
    {Anything, 0, Anything, Silent},
};

static Rule C_rules[] =
{
    {Nothing, "CH", "^", "k"},
    {"^E", "CH", Anything, "k"},
    {Anything, "CH", Anything, "tS"},
    {" S", "CI", "#", "saI"},
    {Anything, "CI", "A", "S"},
    {Anything, "CI", "O", "S"},
    {Anything, "CI", "EN", "S"},
    {Anything, "C", "+", "s"},
    {Anything, "CK", Anything, "k"},
    {Anything, "COM", "%", "kVm"},
    {Anything, "C", Anything, "k"},
    {Anything, 0, Anything, Silent},
};

static Rule D_rules[] =
{
    {"#:", "DED", Nothing, "dId"},
    {".E", "D", Nothing, "d"},
    {"#:^E", "D", Nothing, "t"},
    {Nothing, "DE", "^#", "dI"},
    {Nothing, "DO", Nothing, "mDU"},
    {Nothing, "DOES", Anything, "dVz"},
    {Nothing, "DOING", Anything, "duIN"},
    {Nothing, "DOW", Anything, "daU"},
    {Anything, "DU", "A", "dZu"},
    {Anything, "D", Anything, "d"},
    {Anything, 0, Anything, Silent},
};

static Rule E_rules[] =
{
    {"#:", "E", Nothing, ""},
    {"':^", "E", Nothing, ""},
    {" :", "E", Nothing, "i"},
    {"#", "ED", Nothing, "d"},
    {"#:", "E", "D ", ""},
    {Anything, "EV", "ER", "ev"},
    {Anything, "E", "^%", "i"},
    {Anything, "ERI", "#", "iri"},
    {Anything, "ERI", Anything, "erI"},
    {"#:", "ER", "#", "3"},
    {Anything, "ER", "#", "er"},
    {Anything, "ER", Anything, "3"},
    {Nothing, "EVEN", Anything, "iven"},
    {"#:", "E", "W", ""},
    {"T", "EW", Anything, "u"},
    {"S", "EW", Anything, "u"},
    {"R", "EW", Anything, "u"},
    {"D", "EW", Anything, "u"},
    {"L", "EW", Anything, "u"},
    {"Z", "EW", Anything, "u"},
    {"N", "EW", Anything, "u"},
    {"J", "EW", Anything, "u"},
    {"TH", "EW", Anything, "u"},
    {"CH", "EW", Anything, "u"},
    {"SH", "EW", Anything, "u"},
    {Anything, "EW", Anything, "ju"},
    {Anything, "E", "O", "i"},
    {"#:S", "ES", Nothing, "Iz"},
    {"#:C", "ES", Nothing, "Iz"},
    {"#:G", "ES", Nothing, "Iz"},
    {"#:Z", "ES", Nothing, "Iz"},
    {"#:X", "ES", Nothing, "Iz"},
    {"#:J", "ES", Nothing, "Iz"},
    {"#:CH", "ES", Nothing, "Iz"},
    {"#:SH", "ES", Nothing, "Iz"},
    {"#:", "E", "S ", ""},
    {"#:", "ELY", Nothing, "li"},
    {"#:", "EMENT", Anything, "ment"},
    {Anything, "EFUL", Anything, "fUl"},
    {Anything, "EE", Anything, "i"},
    {Anything, "EARN", Anything, "3n"},
    {Nothing, "EAR", "^", "3"},
    {Anything, "EAD", Anything, "ed"},
    {"#:", "EA", Nothing, "i@"},
    {Anything, "EA", "SU", "e"},
    {Anything, "EA", Anything, "i"},
    {Anything, "EIGH", Anything, "eI"},
    {Anything, "EI", Anything, "i"},
    {Nothing, "EYE", Anything, "aI"},
    {Anything, "EY", Anything, "i"},
    {Anything, "EU", Anything, "ju"},
    {Anything, "E", Anything, "e"},
    {Anything, 0, Anything, Silent},
};

static Rule F_rules[] =
{
    {Anything, "FUL", Anything, "fUl"},
    {Anything, "F", Anything, "f"},
    {Anything, 0, Anything, Silent},
};

static Rule G_rules[] =
{
    {Anything, "GIV", Anything, "gIv"},
    {Nothing, "G", "I^", "g"},
    {Anything, "GE", "T", "ge"},
    {"SU", "GGES", Anything, "gdZes"},
    {Anything, "GG", Anything, "g"},
    {" B#", "G", Anything, "g"},
    {Anything, "G", "+", "dZ"},
    {Anything, "GREAT", Anything, "greIt"},
    {"#", "GH", Anything, ""},
    {Anything, "G", Anything, "g"},
    {Anything, 0, Anything, Silent},
};

static Rule H_rules[] =
{
    {Nothing, "HAV", Anything, "h&v"},
    {Nothing, "HERE", Anything, "hir"},
    {Nothing, "HOUR", Anything, "aU3"},
    {Anything, "HOW", Anything, "haU"},
    {Anything, "H", "#", "h"},
    {Anything, "H", Anything, ""},
    {Anything, 0, Anything, Silent},
};

static Rule I_rules[] =
{
    {Nothing, "IAIN", Nothing, "I@n"},
    {Nothing, "ING", Nothing, "IN"},
    {Nothing, "IN", Anything, "In"},
    {Nothing, "I", Nothing, "aI"},
    {Anything, "IN", "D", "aIn"},
    {Anything, "IER", Anything, "i3"},
    {"#:R", "IED", Anything, "id"},
    {Anything, "IED", Nothing, "aId"},
    {Anything, "IEN", Anything, "ien"},
    {Anything, "IE", "T", "aIe"},
    {" :", "I", "%", "aI"},
    {Anything, "I", "%", "i"},
    {Anything, "IE", Anything, "i"},
    {Anything, "I", "^+:#", "I"},
    {Anything, "IR", "#", "aIr"},
    {Anything, "IZ", "%", "aIz"},
    {Anything, "IS", "%", "aIz"},
    {Anything, "I", "D%", "aI"},
    {"+^", "I", "^+", "I"},
    {Anything, "I", "T%", "aI"},
    {"#:^", "I", "^+", "I"},
    {Anything, "I", "^+", "aI"},
    {Anything, "IR", Anything, "3"},
    {Anything, "IGH", Anything, "aI"},
    {Anything, "ILD", Anything, "aIld"},
    {Anything, "IGN", Nothing, "aIn"},
    {Anything, "IGN", "^", "aIn"},
    {Anything, "IGN", "%", "aIn"},
    {Anything, "IQUE", Anything, "ik"},
    {"^", "I", "^#", "aI"},
    {Anything, "I", Anything, "I"},
    {Anything, 0, Anything, Silent},
};

static Rule J_rules[] =
{
    {Anything, "J", Anything, "dZ"},
    {Anything, 0, Anything, Silent},
};

static Rule K_rules[] =
{
    {Nothing, "K", "N", ""},
    {Anything, "K", Anything, "k"},
    {Anything, 0, Anything, Silent},
};

static Rule L_rules[] =
{
    {Anything, "LO", "C#", "l@U"},
    {"L", "L", Anything, ""},
    {"#:^", "L", "%", "@l"},
    {Anything, "LEAD", Anything, "lid"},
    {Anything, "L", Anything, "l"},
    {Anything, 0, Anything, Silent},
};

static Rule M_rules[] =
{
    {Anything, "MOV", Anything, "muv"},
    {"#", "MM", "#", "m"},
    {Anything, "M", Anything, "m"},
    {Anything, 0, Anything, Silent},
};

static Rule N_rules[] =
{
    {"E", "NG", "+", "ndZ"},
    {Anything, "NG", "R", "Ng"},
    {Anything, "NG", "#", "Ng"},
    {Anything, "NGL", "%", "Ng@l"},
    {Anything, "NG", Anything, "N"},
    {Anything, "NK", Anything, "Nk"},
    {Nothing, "NOW", Nothing, "naU"},
    {"#", "NG", Nothing, "Ng"},
    {Anything, "N", Anything, "n"},
    {Anything, 0, Anything, Silent},
};

static Rule O_rules[] =
{
    {Anything, "OF", Nothing, "@v"},
    {Anything, "OROUGH", Anything, "3@U"},
    {"#:", "OR", Nothing, "3"},
    {"#:", "ORS", Nothing, "3z"},
    {Anything, "OR", Anything, "Or"},
    {Nothing, "ONE", Anything, "wVn"},
    {Anything, "OW", Anything, "@U"},
    {Nothing, "OVER", Anything, "@Uv3"},
    {Anything, "OV", Anything, "Vv"},
    {Anything, "O", "^%", "@U"},
    {Anything, "O", "^EN", "@U"},
    {Anything, "O", "^I#", "@U"},
    {Anything, "OL", "D", "@Ul"},
    {Anything, "OUGHT", Anything, "Ot"},
    {Anything, "OUGH", Anything, "Vf"},
    {Nothing, "OU", Anything, "aU"},
    {"H", "OU", "S#", "aU"},
    {Anything, "OUS", Anything, "@s"},
    {Anything, "OUR", Anything, "Or"},
    {Anything, "OULD", Anything, "Ud"},
    {"^", "OU", "^L", "V"},
    {Anything, "OUP", Anything, "up"},
    {Anything, "OU", Anything, "aU"},
    {Anything, "OY", Anything, "oI"},
    {Anything, "OING", Anything, "@UIN"},
    {Anything, "OI", Anything, "oI"},
    {Anything, "OOR", Anything, "Or"},
    {Anything, "OOK", Anything, "Uk"},
    {Anything, "OOD", Anything, "Ud"},
    {Anything, "OO", Anything, "u"},
    {Anything, "O", "E", "@U"},
    {Anything, "O", Nothing, "@U"},
    {Anything, "OA", Anything, "@U"},
    {Nothing, "ONLY", Anything, "@Unli"},
    {Nothing, "ONCE", Anything, "wVns"},
    {Anything, "ON'T", Anything, "@Unt"},
    {"C", "O", "N", "0"},
    {Anything, "O", "NG", "O"},
    {" :^", "O", "N", "V"},
    {"I", "ON", Anything, "@n"},
    {"#:", "ON", Nothing, "@n"},
    {"#^", "ON", Anything, "@n"},
    {Anything, "O", "ST ", "@U"},
    {Anything, "OF", "^", "Of"},
    {Anything, "OTHER", Anything, "VD3"},
    {Anything, "OSS", Nothing, "Os"},
    {"#:^", "OM", Anything, "Vm"},
    {Anything, "O", Anything, "0"},
    {Anything, 0, Anything, Silent},
};

static Rule P_rules[] =
{
    {Anything, "PH", Anything, "f"},
    {Anything, "PEOP", Anything, "pip"},
    {Anything, "POW", Anything, "paU"},
    {Anything, "PUT", Nothing, "pUt"},
    {Anything, "P", Anything, "p"},
    {Anything, 0, Anything, Silent},
};

static Rule Q_rules[] =
{
    {Anything, "QUAR", Anything, "kwOr"},
    {Anything, "QU", Anything, "kw"},
    {Anything, "Q", Anything, "k"},
    {Anything, 0, Anything, Silent},
};

static Rule R_rules[] =
{
    {Nothing, "RE", "^#", "ri"},
    {Anything, "R", Anything, "r"},
    {Anything, 0, Anything, Silent},
};

static Rule S_rules[] =
{
    {Anything, "SH", Anything, "S"},
    {"#", "SION", Anything, "Z@n"},
    {Anything, "SOME", Anything, "sVm"},
    {"#", "SUR", "#", "Z3"},
    {Anything, "SUR", "#", "S3"},
    {"#", "SU", "#", "Zu"},
    {"#", "SSU", "#", "Su"},
    {"#", "SED", Nothing, "zd"},
    {"#", "S", "#", "z"},
    {Anything, "SAID", Anything, "sed"},
    {"^", "SION", Anything, "S@n"},
    {Anything, "S", "S", ""},
    {".", "S", Nothing, "z"},
    {"#:.E", "S", Nothing, "z"},
    {"#:^##", "S", Nothing, "z"},
    {"#:^#", "S", Nothing, "s"},
    {"U", "S", Nothing, "s"},
    {" :#", "S", Nothing, "z"},
    {Nothing, "SCH", Anything, "sk"},
    {Anything, "S", "C+", ""},
    {"#", "SM", Anything, "zm"},
    {"#", "SN", "'", "z@n"},
    {Anything, "S", Anything, "s"},
    {Anything, 0, Anything, Silent},
};

static Rule T_rules[] =
{
    {Nothing, "THE", Nothing, "D@"},
    {Anything, "TO", Nothing, "tu"},
    {Anything, "THAT", Nothing, "D&t"},
    {Nothing, "THIS", Nothing, "DIs"},
    {Nothing, "THEY", Anything, "DeI"},
    {Nothing, "THERE", Anything, "Der"},
    {Anything, "THER", Anything, "D3"},
    {Anything, "THEIR", Anything, "Der"},
    {Nothing, "THAN", Nothing, "D&n"},
    {Nothing, "THEM", Nothing, "Dem"},
    {Anything, "THESE", Nothing, "Diz"},
    {Nothing, "THEN", Anything, "Den"},
    {Anything, "THROUGH", Anything, "Tru"},
    {Anything, "THOSE", Anything, "D@Uz"},
    {Anything, "THOUGH", Nothing, "D@U"},
    {Nothing, "THUS", Anything, "DVs"},
    {Anything, "TH", Anything, "T"},
    {"#:", "TED", Nothing, "tId"},
    {"S", "TI", "#N", "tS"},
    {Anything, "TI", "O", "S"},
    {Anything, "TI", "A", "S"},
    {Anything, "TIEN", Anything, "S@n"},
    {Anything, "TUR", "#", "tS3"},
    {Anything, "TU", "A", "tSu"},
    {Nothing, "TWO", Anything, "tu"},
    {Anything, "T", Anything, "t"},
    {Anything, 0, Anything, Silent},
};

static Rule U_rules[] =
{
    {Nothing, "UN", "I", "jun"},
    {Nothing, "UN", Anything, "Vn"},
    {Nothing, "UPON", Anything, "@pOn"},
    {"T", "UR", "#", "Ur"},
    {"S", "UR", "#", "Ur"},
    {"R", "UR", "#", "Ur"},
    {"D", "UR", "#", "Ur"},
    {"L", "UR", "#", "Ur"},
    {"Z", "UR", "#", "Ur"},
    {"N", "UR", "#", "Ur"},
    {"J", "UR", "#", "Ur"},
    {"TH", "UR", "#", "Ur"},
    {"CH", "UR", "#", "Ur"},
    {"SH", "UR", "#", "Ur"},
    {Anything, "UR", "#", "jUr"},
    {Anything, "UR", Anything, "3"},
    {Anything, "U", "^ ", "V"},
    {Anything, "U", "^^", "V"},
    {Anything, "UY", Anything, "aI"},
    {" G", "U", "#", ""},
    {"G", "U", "%", ""},
    {"G", "U", "#", "w"},
    {"#N", "U", Anything, "ju"},
    {"T", "U", Anything, "u"},
    {"S", "U", Anything, "u"},
    {"R", "U", Anything, "u"},
    {"D", "U", Anything, "u"},
    {"L", "U", Anything, "u"},
    {"Z", "U", Anything, "u"},
    {"N", "U", Anything, "u"},
    {"J", "U", Anything, "u"},
    {"TH", "U", Anything, "u"},
    {"CH", "U", Anything, "u"},
    {"SH", "U", Anything, "u"},
    {Anything, "U", Anything, "ju"},
    {Anything, 0, Anything, Silent},
};

static Rule V_rules[] =
{
    {Anything, "VIEW", Anything, "vju"},
    {Anything, "V", Anything, "v"},
    {Anything, 0, Anything, Silent},
};

static Rule W_rules[] =
{
    {Nothing, "WERE", Anything, "w3"},
    {Anything, "WA", "S", "w0"},
    {Anything, "WA", "T", "w0"},
    {Anything, "WHERE", Anything, "hwer"},
    {Anything, "WHAT", Anything, "hw0t"},
    {Anything, "WHOL", Anything, "h@Ul"},
    {Anything, "WHO", Anything, "hu"},
    {Anything, "WH", Anything, "hw"},
    {Anything, "WAR", Anything, "wOr"},
    {Anything, "WOR", "^", "w3"},
    {Anything, "WR", Anything, "r"},
    {Anything, "W", Anything, "w"},
    {Anything, 0, Anything, Silent},
};

static Rule X_rules[] =
{
    {Anything, "X", Anything, "ks"},
    {Anything, 0, Anything, Silent},
};

static Rule Y_rules[] =
{
    {Anything, "YOUNG", Anything, "jVN"},
    {Nothing, "YOU", Anything, "ju"},
    {Nothing, "YES", Anything, "jes"},
    {Nothing, "Y", Anything, "j"},
    {"#:^", "Y", Nothing, "i"},
    {"#:^", "Y", "I", "i"},
    {" :", "Y", Nothing, "aI"},
    {" :", "Y", "#", "aI"},
    {" :", "Y", "^+:#", "I"},
    {" :", "Y", "^#", "aI"},
    {Anything, "Y", Anything, "I"},
    {Anything, 0, Anything, Silent},
};

static Rule Z_rules[] =
{
    {Anything, "Z", Anything, "z"},
    {Anything, 0, Anything, Silent},
};

static Rule *Rules[] =
{
    punct_rules,
    A_rules, B_rules, C_rules, D_rules, E_rules, F_rules, G_rules,
    H_rules, I_rules, J_rules, K_rules, L_rules, M_rules, N_rules,
    O_rules, P_rules, Q_rules, R_rules, S_rules, T_rules, U_rules,
    V_rules, W_rules, X_rules, Y_rules, Z_rules
};


static const char *Cardinals[] =
{
    "zero", "one", "two", "three", "four", 
    "five", "six", "seven", "eight", "nine", 
    "ten", "eleven", "twelve", "thirteen", "fourteen", 
    "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"
};


static const char *Twenties[] =
{
    "twenty", "thirty", "forty", "fifty",
    "sixty", "seventy", "eighty", "ninety"
};


static const char *Ordinals[] =
{
    "zeroth", "first", "second", "third", "fourth", 
    "fifth", "sixth", "seventh","eighth", "ninth",
    "tenth", "eleventh", "twelfth", "thirteenth", "fourteenth", 
    "fifteenth", "sixteenth", "seventeenth", "eighteenth", "nineteenth"
};


static const char *Ord_twenties[] =
{
    "twentieth", "thirtieth", "fortieth", "fiftieth",
    "sixtieth", "seventieth", "eightieth", "ninetieth"
};


/*
** Translate a number to phonemes.  This version is for CARDINAL numbers.
**       Note: this is recursive.
*/
static int xlate_cardinal(int value, darray *phone, void* memctx)
{
    int nph = 0;

    if (value < 0)
    {
        nph += xlate_string("minus", phone, memctx);
        value = (-value);

        if (value < 0)                 /* Overflow!  -32768 */
        {
            nph += xlate_string("a lot", phone, memctx);
            return nph;
        }
    }

    if (value >= 1000000000L)
        /* Billions */
    {
        nph += xlate_cardinal(value / 1000000000L, phone, memctx);
        nph += xlate_string("billion", phone, memctx);
        value = value % 1000000000;

        if (value == 0)
            return nph;                   /* Even billion */

        if (value < 100)
            nph += xlate_string("and", phone, memctx);

        /* as in THREE BILLION AND FIVE */
    }

    if (value >= 1000000L)
        /* Millions */
    {
        nph += xlate_cardinal(value / 1000000L, phone, memctx);
        nph += xlate_string("million", phone, memctx);
        value = value % 1000000L;

        if (value == 0)
            return nph;                   /* Even million */

        if (value < 100)
            nph += xlate_string("and", phone, memctx);

        /* as in THREE MILLION AND FIVE */
    }

    /* Thousands 1000..1099 2000..99999 */
    /* 1100 to 1999 is eleven-hunderd to ninteen-hunderd */

    if ((value >= 1000L && value <= 1099L) || value >= 2000L)
    {
        nph += xlate_cardinal(value / 1000L, phone, memctx);
        nph += xlate_string("thousand", phone, memctx);
        value = value % 1000L;

        if (value == 0)
            return nph;                   /* Even thousand */

        if (value < 100)
            nph += xlate_string("and", phone, memctx);

        /* as in THREE THOUSAND AND FIVE */
    }

    if (value >= 100L)
    {
        nph += xlate_string(Cardinals[value / 100], phone, memctx);
        nph += xlate_string("hundred", phone, memctx);
        value = value % 100;

        if (value == 0)
            return nph;                   /* Even hundred */
    }

    if (value >= 20)
    {
        nph += xlate_string(Twenties[(value - 20) / 10], phone, memctx);
        value = value % 10;

        if (value == 0)
            return nph;                   /* Even ten */
    }

    nph += xlate_string(Cardinals[value], phone, memctx);

    return nph;
}

/*
** Translate a number to phonemes.  This version is for ORDINAL numbers.
**       Note: this is recursive.
*/
#if 0
static int xlate_ordinal(int value, darray *phone)
{
    int nph = 0;

    if (value < 0)
    {
        nph += xlate_string("minus", phone);
        value = (-value);

        if (value < 0)                 /* Overflow!  -32768 */
        {
            nph += xlate_string("a lot", phone);
            return nph;
        }
    }

    if (value >= 1000000000L)
        /* Billions */
    {
        nph += xlate_cardinal(value / 1000000000L, phone);
        value = value % 1000000000;

        if (value == 0)
        {
            nph += xlate_string("billionth", phone);
            return nph;                  /* Even billion */
        }

        nph += xlate_string("billion", phone);

        if (value < 100)
            nph += xlate_string("and", phone);

        /* as in THREE BILLION AND FIVE */
    }

    if (value >= 1000000L)
        /* Millions */
    {
        nph += xlate_cardinal(value / 1000000L, phone);
        value = value % 1000000L;

        if (value == 0)
        {
            nph += xlate_string("millionth", phone);
            return nph;                  /* Even million */
        }

        nph += xlate_string("million", phone);

        if (value < 100)
            nph += xlate_string("and", phone);

        /* as in THREE MILLION AND FIVE */
    }

    /* Thousands 1000..1099 2000..99999 */
    /* 1100 to 1999 is eleven-hunderd to ninteen-hunderd */

    if ((value >= 1000L && value <= 1099L) || value >= 2000L)
    {
        nph += xlate_cardinal(value / 1000L, phone);
        value = value % 1000L;

        if (value == 0)
        {
            nph += xlate_string("thousandth", phone);
            return nph;                  /* Even thousand */
        }

        nph += xlate_string("thousand", phone);

        if (value < 100)
            nph += xlate_string("and", phone);

        /* as in THREE THOUSAND AND FIVE */
    }

    if (value >= 100L)
    {
        nph += xlate_string(Cardinals[value / 100], phone);
        value = value % 100;

        if (value == 0)
        {
            nph += xlate_string("hundredth", phone);
            return nph;                  /* Even hundred */
        }

        nph += xlate_string("hundred", phone);
    }

    if (value >= 20)
    {
        if ((value % 10) == 0)
        {
            nph += xlate_string(Ord_twenties[(value - 20) / 10], phone);
            return nph;                  /* Even ten */
        }

        nph += xlate_string(Twenties[(value - 20) / 10], phone);

        value = value % 10;
    }

    nph += xlate_string(Ordinals[value], phone);

    return nph;
}
#endif

static int isvowel(int chr)
{
    return (chr == 'A' || chr == 'E' || chr == 'I' ||
        chr == 'O' || chr == 'U');
}

static int isconsonant(int chr)
{
    return (isupper(chr) && !isvowel(chr));
}

static int leftmatch(
    const char *pattern,                    /* first char of pattern to match in text */
    const char *context)                     /* last char of text to be matched */

{
    const char *pat;
    const char *text;
    int count;

    if (*pattern == '\0')
        /* null string matches any context */
    {
        return 1;
    }

    /* point to last character in pattern string */
    count = (int)strlen(pattern);

    pat = pattern + (count - 1);

    text = context;

    for (; count > 0; pat--, count--)
    {
        /* First check for simple text or space */
        if (isalpha(*pat) || *pat == '\'' || *pat == ' ')
            if (*pat != *text)
                return 0;
            else
            {
                text--;
                continue;
            }

            switch (*pat)
            {

            case '#':                   /* One or more vowels */

                if (!isvowel(*text))
                    return 0;

                text--;

                while (isvowel(*text))
                    text--;

                break;

            case ':':                   /* Zero or more consonants */
                while (isconsonant(*text))
                    text--;

                break;

            case '^':                   /* One consonant */
                if (!isconsonant(*text))
                    return 0;

                text--;

                break;

            case '.':                   /* B, D, V, G, J, L, M, N, R, W, Z */
                if (*text != 'B' && *text != 'D' && *text != 'V'
                    && *text != 'G' && *text != 'J' && *text != 'L'
                    && *text != 'M' && *text != 'N' && *text != 'R'
                    && *text != 'W' && *text != 'Z')
                    return 0;

                text--;

                break;

            case '+':                   /* E, I or Y (front vowel) */
                if (*text != 'E' && *text != 'I' && *text != 'Y')
                    return 0;

                text--;

                break;

            case '%':

            default:
                fprintf(stderr, "Bad char in left rule: '%c'\n", *pat);

                return 0;
            }
    }

    return 1;
}

static int rightmatch(
    const char *pattern,                    /* first char of pattern to match in text */
    const char *context)                     /* last char of text to be matched */
{
    const char *pat;
    const char *text;

    if (*pattern == '\0')
        /* null string matches any context */
        return 1;

    pat = pattern;

    text = context;

    for (pat = pattern; *pat != '\0'; pat++)
    {
        /* First check for simple text or space */
        if (isalpha(*pat) || *pat == '\'' || *pat == ' ')
            if (*pat != *text)
                return 0;
            else
            {
                text++;
                continue;
            }

            switch (*pat)
            {

            case '#':                   /* One or more vowels */

                if (!isvowel(*text))
                    return 0;

                text++;

                while (isvowel(*text))
                    text++;

                break;

            case ':':                   /* Zero or more consonants */
                while (isconsonant(*text))
                    text++;

                break;

            case '^':                   /* One consonant */
                if (!isconsonant(*text))
                    return 0;

                text++;

                break;

            case '.':                   /* B, D, V, G, J, L, M, N, R, W, Z */
                if (*text != 'B' && *text != 'D' && *text != 'V'
                    && *text != 'G' && *text != 'J' && *text != 'L'
                    && *text != 'M' && *text != 'N' && *text != 'R'
                    && *text != 'W' && *text != 'Z')
                    return 0;

                text++;

                break;

            case '+':                   /* E, I or Y (front vowel) */
                if (*text != 'E' && *text != 'I' && *text != 'Y')
                    return 0;

                text++;

                break;

            case '%':                   /* ER, E, ES, ED, ING, ELY (a suffix) */
                if (*text == 'E')
                {
                    text++;

                    if (*text == 'L')
                    {
                        text++;

                        if (*text == 'Y')
                        {
                            text++;
                            break;
                        }

                        else
                        {
                            text--;               /* Don't gobble L */
                            break;
                        }
                    }

                    else
                        if (*text == 'R' || *text == 'S' || *text == 'D')
                            text++;

                    break;
                }

                else
                    if (*text == 'I')
                    {
                        text++;

                        if (*text == 'N')
                        {
                            text++;

                            if (*text == 'G')
                            {
                                text++;
                                break;
                            }
                        }

                        return 0;
                    }

                    else
                        return 0;

            default:
                fprintf(stderr, "Bad char in right rule:'%c'\n", *pat);

                return 0;
            }
    }

    return 1;
}

static void phone_cat(darray *arg, const char *s)
{
    char ch;

    while ((ch = *s++))
        arg->put(ch);
}


static int find_rule(darray *arg, char *word, int index, Rule *rules)
{
    for (;;)                         /* Search for the rule */
    {
        Rule *rule;
        const char *left,
            *match,
            *right,
            *output;
        int remainder;
        rule = rules++;
        match = (*rule)[1];

        if (match == 0)
            /* bad symbol! */
        {
            fprintf(stderr, "Error: Can't find rule for: '%c' in \"%s\"\n",
                word[index], word);
            return index + 1;            /* Skip it! */
        }

        for (remainder = index; *match != '\0'; match++, remainder++)
        {
            if (*match != word[remainder])
                break;
        }

        if (*match != '\0')
            continue;                     /* found missmatch */

        left = (*rule)[0];

        right = (*rule)[2];

        if (!leftmatch(left, &word[index - 1]))
            continue;

        if (!rightmatch(right, &word[remainder]))
            continue;

        output = (*rule)[3];

        phone_cat(arg, output);

        return remainder;
    }
}

static void guess_word(darray *arg, char *word)
{
    int index;                       /* Current position in word */
    int type;                        /* First letter of match part */
    index = 1;                       /* Skip the initial blank */

    do
    {
        if (isupper(word[index]))
            type = word[index] - 'A' + 1;
        else
            type = 0;

        index = find_rule(arg, word, index, Rules[type]);
    }

    while (word[index] != '\0');
}

static int NRL(const char *s, int n, darray *phone, void* memctx )
{
    (void) memctx;
    int old = phone->getSize();
    char mem[ 64 ];    
    char *word = mem;
    if( n + 3 > sizeof( mem ) ) word = (char *) SPEECH_MALLOC( memctx, n + 3);
    char *d = word;
    *d++ = ' ';

    while (n-- > 0)
    {
        char ch = *s++;

        if (islower(ch))
            ch = (char) toupper(ch);

        *d++ = ch;
    }

    *d++ = ' '; // kinda unnecessary

    *d = '\0';
    guess_word(phone, word);
    if( word != mem ) SPEECH_FREE( memctx, word );
    return phone->getSize() - old;
}


static int spell_out(const char *word, int n, darray *phone, void* memctx)
{
    int nph = 0;

    while (n-- > 0)
    {
        nph += xlate_string(ASCII[*word++ & 0x7F], phone, memctx);
    }

    return nph;
}

static int suspect_word(const char *s, int n)
{
    int i = 0;
    int seen_lower = 0;
    int seen_upper = 0;
    int seen_vowel = 0;
    int last = 0;

    for (i = 0; i < n; i++)
    {
        char ch = *s++;

        if (i && last != '-' && isupper(ch))
            seen_upper = 1;

        if (islower(ch))
        {
            seen_lower = 1;
            ch = (char) toupper(ch);
        }

        if (ch == 'A' || ch == 'E' || ch == 'I' || ch == 'O' || ch == 'U' || ch == 'Y')
            seen_vowel = 1;

        last = ch;
    }

    return !seen_vowel || (seen_upper && seen_lower) || !seen_lower;
}

static int xlate_word(const char *word, int n, darray *phone, void* memctx)
{
    int nph = 0;

    if (*word != '[')
    {
        if (suspect_word(word, n))
            return spell_out(word, n, phone, memctx);
        else
        {
            nph += NRL(word, n, phone, memctx);
        }
    }

    else
    {
        if ((++word)[(--n) - 1] == ']')
            n--;

        while (n-- > 0)
        {
            phone->put(*word++);
            nph++;
        }
    }

    phone->put(' ');

    return nph + 1;
}


int xlate_string(const char *string, darray *phone, void* memctx)
{
    int nph = 0;
    const char *s = string;
    char ch;

    while (isspace(ch = *s))
        s++;

    while ((ch = *s))
    {
        const char *word = s;

        if (isalpha(ch))
        {
            while (isalpha(ch = *s) || ((ch == '\'' || ch == '-' || ch == '.') && isalpha(s[1])))
                s++;

            if (!ch || isspace(ch) || ispunct(ch) || (isdigit(ch) && !suspect_word(word, (int)(s - word))))
                nph += xlate_word(word, (int)(s - word), phone, memctx);
            else
            {
                while ((ch = *s) && !isspace(ch) && !ispunct(ch))
                    s++;

                nph += spell_out(word, (int)(s - word), phone, memctx);
            }
        }

        else
            if (isdigit(ch) || (ch == '-' && isdigit(s[1])))
            {
                int sign = (ch == '-') ? -1 : 1;
                int value = 0;

                if (sign < 0)
                    ch = *++s;

                while (isdigit(ch = *s))
                {
                    value = value * 10 + ch - '0';
                    s++;
                }

                if (ch == '.' && isdigit(s[1]))
                {
                    word = ++s;
                    nph += xlate_cardinal(value * sign, phone, memctx);
                    nph += xlate_string("point", phone, memctx);

                    while (isdigit(ch = *s))
                        s++;

                    nph += spell_out(word, (int)(s - word), phone, memctx);
                }

                else
                {
                    /* check for ordinals, date, time etc. can go in here */
                    nph += xlate_cardinal(value * sign, phone, memctx);
                }
            }

            else
                if (ch == '[' && strchr(s, ']'))
                {
                    const char *word = s;

                    while (*s && *s++ != ']')
                        /* nothing */
                        ;

                    nph += xlate_word(word, (int)(s - word), phone, memctx);
                }

                else
                    if (ispunct(ch))
                    {
                        switch (ch)
                        {

                        case '!':

                        case '?':

                        case '.':
                            s++;
                            phone->put(' ');
                            break;

                        case '"':                 /* change pitch ? */

                        case ':':

                        case '-':

                        case ';':

                        case ',':

                        case '(':

                        case ')':
                            s++;
                            phone->put(' ');
                            break;

                        case '[':
                            {
                                const char *e = strchr(s, ']');

                                if (e)
                                {
                                    s++;

                                    while (s < e)
                                        phone->put(*s++);

                                    s = e + 1;

                                    break;
                                }
                            }

                        default:
                            nph += spell_out(word, 1, phone, memctx);
                            s++;
                            break;
                        }
                    }

                    else
                    {
                        while ((ch = *s) && !isspace(ch))
                            s++;

                        nph += spell_out(word, (int)(s - word), phone, memctx);
                    }

                    while (isspace(ch = *s))
                        s++;
    }

    return nph;
}

} /* namespace internal */


short* speech_gen( int* samples_pairs_generated, char const* str, void* memctx )
    {
    internal::darray element( memctx );
    internal::darray phone( memctx );
    internal::xlate_string( str, &phone, memctx );
    int frames = internal::klatt::phone_to_elm( phone.getData(), phone.getSize(), &element );

    internal::klatt synth;
    synth.init();
    
    int sample_count = synth.mNspFr * frames;

    if( samples_pairs_generated ) *samples_pairs_generated = sample_count * 4;
    short* sample_pairs = (short*) SPEECH_MALLOC( memctx, sample_count * 8 * sizeof( short ) );   

    short* sample = (short*) sample_pairs;

    synth.initsynth( element.getSize(), (unsigned char*) element.getData()) ;
    int x = 0;
    int pos = 0;
    while( x >= 0 ) 
        {
        x = synth.synth( 0, &sample[ pos ] );
        if( x < 0 ) break;
        pos += x;
        }

    short* in = sample + sample_count;
    short* out = sample_pairs + sample_count * 4 * 2;
    while( out > sample_pairs )
        {
        out -= 4 *2;
        --in;
        short prev_sample = in - 1 > sample ? * ( in - 1 ) : 0;

        short sample = *in;
        int o[ 8 ];
        o[0] = ( ((sample + (3 * prev_sample)) >> 2) ) * 2;
        o[1] = ( ((sample + prev_sample) >> 1) ) * 2;
        o[2] = ( (((3 * sample) + prev_sample) >> 2) ) * 2;
        o[3] = ( sample ) * 2;
        o[4] = ( ((sample + (3 * prev_sample)) >> 2) ) * 2;
        o[5] = ( ((sample + prev_sample) >> 1) ) * 2;
        o[6] = ( (((3 * sample) + prev_sample) >> 2) ) * 2;
        o[7] = ( sample ) * 2;
        for( int i = 0; i < 8; ++i ) out[ i ] = (short)( o[ i ] > 32767 ? 32767 : o[ i ] < -32767 ? -32767 : o[ i ] );
        }
    
    return sample_pairs;
    }


void speech_free( float* sample_pairs, void* memctx )
    {
    (void) memctx;
    SPEECH_FREE( memctx, sample_pairs );
    }


#pragma warning( pop )

#endif /* SPEECH_IMPLEMENTATION */


/*
----------------------------------------------------------------------------
    LICENSE
----------------------------------------------------------------------------

The speech synth is based on rsynth by the late Nick Ing-Simmons (et al).

He described the legal status as:

    This is a text to speech system produced by integrating various pieces 
    of code and tables of data, which are all (I believe) in the public 
    domain.
    
Since then, the rsynth source code has passed legal checks by several open 
source organizations, so it "should" be pretty safe.

The primary copyright claims seem to have to do with text-to-speech 
dictionary use, which I've removed completely.

I've done some serious refactoring, clean-up and feature removal on the 
source, as all I need is "a" free, simple speech synth, not a "good" speech 
synth. Since I've removed a bunch of stuff, this is probably safer public 
domain release than the original.

(I'm rather surprised there's no good public domain speech synths out 
there; after all, it's 2013..)

I'm placing my changes in public domain as well, or if that's not 
acceptable for you, then CC0:
http://creativecommons.org/publicdomain/zero/1.0/

The SoLoud interface files (soloud_speech.*) are under ZLib/LibPNG license.

-- Jari Komppa 2013
   
----------------------------------------------------------------------------
*/
