!contains(QMAKE_HOST.arch, x86_64) {
  CONFIG += mmx sse sse2
}

linux-g++|win32-g++ {
  !contains(QMAKE_HOST.arch, x86_64) {
    # All floating point operations are to be performed on the SSE2 unit
    QMAKE_CXXFLAGS += -march=i686 -mmmx -msse -msse2 -mfpmath=sse
    QMAKE_CFLAGS += -march=i686 -mmmx -msse -msse2 -mfpmath=sse
  }

  QMAKE_CFLAGS += -w -std=gnu99

  # Optimize C++ code for size.
  QMAKE_CXXFLAGS_RELEASE -= -O2
  QMAKE_CXXFLAGS_RELEASE += -Os

  # Improve floating point performance
  QMAKE_CXXFLAGS += -fno-math-errno -fno-signed-zeros
  QMAKE_CFLAGS += -fno-math-errno -fno-signed-zeros -fno-common -ffast-math

  # Reduce export symbol table size and binary size.
  QMAKE_CXXFLAGS += -fvisibility-inlines-hidden
  QMAKE_CFLAGS += -fvisibility=hidden

  # All "hot" code is placed in plain old C files; these need to be fully
  # optimized, also in debug mode.
  QMAKE_CFLAGS_RELEASE -= -O2
  QMAKE_CFLAGS += -O3
}

linux-g++ {
  # Used for performance analysis.
  #QMAKE_CFLAGS += -Wa,-ahl=/tmp/assembly-debug.s
}

win32-g++ {
  # Required for 32-bit Windows/MingW to prevent crashing SSE code on unaligned
  # stack data.
  QMAKE_CFLAGS += -mstackrealign
}
