#!/bin/bash
set -e

OUT_NSO=${OUT}/${BINARY_NAME}
OUT_NPDM=${OUT}/main.npdm

# Create out directory.
mkdir -p "${OUT}"

# Copy build into out
cp "${NAME}.nso" "${OUT_NSO}"
cp "${NAME}.npdm" "${OUT_NPDM}"

# Copy ELF to user path if defined.
if [ ! -z $ELF_EXTRACT ]; then
    cp "$NAME.elf" "$ELF_EXTRACT"
fi
