#!/bin/bash
HOST=10.0.0.125
SYSROOT=/opt/homebrew/opt/x86_64-unknown-linux-gnu/toolchain/x86_64-unknown-linux-gnu/sysroot
#rsync -av $HOST:/usr/include/GL $SYSROOT/usr/include/
rsync -av deck:/usr/lib/'libGL.so*' $SYSROOT/usr/lib/
rsync -av deck:/usr/lib/'libGLdispatch.so*' $SYSROOT/usr/lib/
rsync -av deck:/usr/lib/'libGLX.so*' $SYSROOT/usr/lib/
rsync -av deck:/usr/lib/'libxcb.so*' $SYSROOT/usr/lib/
rsync -av deck:/usr/lib/'libXau.so*' $SYSROOT/usr/lib/
rsync -av deck:/usr/lib/'libXdmcp.so*' $SYSROOT/usr/lib/
#rsync -av $HOST:/usr/include/X11 $SYSROOT/usr/include/
rsync -av deck:/usr/lib/'libX11.so*' $SYSROOT/usr/lib/
