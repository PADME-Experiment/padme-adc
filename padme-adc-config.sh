#!/bin/bash

# Define PADMEADC from location of this script assuming
# it is located in the main padme-adc directory
export PADMEADC=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# This directory contains the lib and include directories needed
# to access to CAEN digitizer libraries
export CAENDIR="/home/pi/CAEN"

# Add CAEN libraries to LD_LIBRARY_PATH
if [ -z "$LD_LIBRARY_PATH" ]; then
    export LD_LIBRARY_PATH="${CAENDIR}/lib"
else
    export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${CAENDIR}/lib"
fi
