#!/bin/sh
mkdir -p obj/assets
echo "[ Compiling 8x8 font ]"
python3 tools/font2raw.py res/font_default.png 8 8 a obj/assets/font_default.bin
wf-bin2c -a 1 --address-space __wf_rom obj/assets/ obj/assets/font_default.bin
echo "[ Generating strings ]"
python3 tools/gen_strings.py lang obj/assets/lang.c obj/assets/lang.h
echo "[ Generating binary blobs ]"
wf-zx0-salvador thirdparty/wsmonitor.bin obj/assets/wsmonitor.zx0
wf-bin2c -a 1 --address-space __wf_rom obj/assets/ obj/assets/wsmonitor.zx0
