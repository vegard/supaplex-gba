#! /bin/bash

set -e
set -u


# Get the supaplex data files. These are NOT covered by the license of
# this program.

if [ ! -e supaplex.zip ]
then
	wget http://www.elmerproductions.com/sp/software/supaplex.zip
fi

if ! sha1sum --check supaplex.zip.sha1
then
	echo Checksum verification failed. Please remove supaplex.zip and rerun.
	exit 1
fi

#unzip -o supaplex.zip FIXED.DAT LEVELS.DAT MOVING.DAT PALETTES.DAT -d data
mkdir -p data
for f in FIXED LEVELS MOVING PALETTES
do
	if [ ! -e data/$f.DAT ]
	then
		unzip -o supaplex.zip $f.DAT -d data
	fi
done


# Compile host programs

hostcxx=g++
hostcxxflags="-Wall -g"

${hostcxx} ${hostcxxflags} -o convert convert.cc

./convert > src/graphics.cc


# Compile target programs

cxx=arm-eabi-g++
cxxflags="-std=c++0x -g -Wall -O3 -mcpu=arm7tdmi -mtune=arm7tdmi -fomit-frame-pointer -ffast-math -marm -specs=gba.specs"

${cxx} ${cxxflags} -o supaplex.elf src/supaplex.cc

arm-eabi-objcopy -O binary -S supaplex.elf supaplex.bin
ln -sf supaplex.bin supaplex.gba
