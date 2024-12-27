#!/bin/sh
mkdir -p obj/assets
rm -r obj/assets/* || true
echo "[ Compiling 8x8 font ]"
python3 tools/font2raw.py res/font_default.png 8 8 a obj/assets/font_default.bin
wf-bin2s -a 1 --address-space __wf_rom --section ".farrodata.a.font_default" obj/assets/ obj/assets/font_default.bin
echo "[ Generating strings ]"
python3 tools/gen_strings.py lang obj/assets/lang.c obj/assets/lang.h
echo "[ Generating binary blobs ]"
echo "- wsmonitor"
wf-zx0-salvador thirdparty/wsmonitor.bin obj/assets/wsmonitor.zx0
wf-bin2s -a 1 --address-space __wf_rom --section ".farrodata.a.wsmonitor" obj/assets/ obj/assets/wsmonitor.zx0
echo "- AthenaBIOS"
cd thirdparty/athenaos
rm -r dist || true
make CONFIG=config/config.flashmasta.mk bios
cd ../..
wf-zx0-salvador thirdparty/athenaos/dist/AthenaBIOS-*-flashmasta.raw obj/assets/athenabios.zx0
wf-bin2s -a 1 --address-space __wf_rom --section ".farrodata.a.athenabios" --hide-size-in-header obj/assets/ obj/assets/athenabios.zx0
