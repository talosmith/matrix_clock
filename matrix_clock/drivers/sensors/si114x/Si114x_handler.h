#include "Si114x_functions.h"
//#include "Si114x_debug.h"

static u8 xdata initial_baseline_counter = 128;
u16 xdata baseline[3];    // Array to store calcBaseline return values

char isIRStable( SI114X_IRQ_SAMPLE *samples);

void IRCompensation( u8 proxChannel, SI114X_IRQ_SAMPLE *samples, u8 ircorrection[]);

void calcBaseline( u8 proxChannel, SI114X_IRQ_SAMPLE *samples, u16 noise_margin);

void SliderAlgorithm( HANDLE si114x_handle, SI114X_IRQ_SAMPLE *samples, u16 scale);

void si114x_process_samples(HANDLE si114x_handle, SI114X_IRQ_SAMPLE *samples);