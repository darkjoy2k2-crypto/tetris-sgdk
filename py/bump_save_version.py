import re
import os

# Pfad zur Header-Datei
FILE_PATH = 'inc/states/save_manager.h'

def bump_version():
    if not os.path.exists(FILE_PATH):
        print(f"Fehler: {FILE_PATH} nicht gefunden.")
        return

    with open(FILE_PATH, 'r') as f:
        content = f.read()

    # Sucht nach #define SAVE_VERSION gefolgt von einem Hex-Wert
    pattern = r'(#define\s+SAVE_VERSION\s+)(0x[0-9a-fA-F]+)'
    match = re.search(pattern, content)

    if match:
        prefix = match.group(1)
        current_hex = match.group(2)
        
        # Wert um 1 erhöhen
        new_val = int(current_hex, 16) + 1
        new_hex = f"0x{new_val:08X}"
        
        # Inhalt ersetzen
        new_content = content.replace(current_hex, new_hex)
        
        with open(FILE_PATH, 'w') as f:
            f.write(new_content)
        print(f"SAVE_VERSION erhöht auf: {new_hex}")
    else:
        print("Muster #define SAVE_VERSION nicht gefunden.")

if __name__ == "__main__":
    bump_version()