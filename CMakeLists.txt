
set(MAJOR_VERSION 3)
set(MINOR_VERSION 9)
set(MICRO_VERSION 0)
configure_file( config.h.cmake config.h )
add_definitions(-DHAVE_CONFIG_H)

add_library(qrencode STATIC qrencode.c qrencode_inner.h  qrinput.c qrinput.h  bitstream.c bitstream.h  qrspec.c qrspec.h  rsecc.c rsecc.h  split.c split.h  mask.c mask.h  mqrspec.c mqrspec.h  mmask.c mmask.h )

