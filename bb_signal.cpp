#include "bb_signal.h"

volatile sig_atomic_t g_interrupted = 0;

void onSigInt(int) {
    g_interrupted = 1;
}
