#!/bin/bash

# Verifică dacă a fost furnizat un singur argument
if [ "$#" -ne 1 ]; then
    echo "Utilizare: $0 <c>"
    exit 1
fi

# Extrage primul argument
char="$1"

# Initializare contor
contor=0

# Citeste linii pana la end-of-file de la intrarea standard
while IFS= read -r line; do
    # Verifică dacă linia respectă condițiile
    if [[ $line =~ ^[[:upper:]].*[a-zA-Z0-9\ ,.!?]$ && $line != *", si"* && $line == *"$char"* && $line != *", $char"* ]]; then
        ((contor++))
    fi
done

# Afisează rezultatul
echo "$contor"
