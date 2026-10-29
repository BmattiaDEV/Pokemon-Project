import os
from PIL import Image

def convert_png_to_3ds_bin(png_path, bin_path, target_w=None, target_h=None):
    if not os.path.exists(png_path):
        print(f"Errore: File '{png_path}' non trovato.")
        return False
    
    img = Image.open(png_path).convert("RGBA")
    if target_w and target_h:
        img = img.resize((target_w, target_h), Image.NEAREST)
        
    w, h = img.size
    pixels = img.tobytes()
    
    # Crea le cartelle di destinazione (data/bin) in automatico
    os.makedirs(os.path.dirname(bin_path), exist_ok=True)
    
    with open(bin_path, "wb") as f:
        f.write(pixels)
        
    print(f"Convertito: {png_path} -> {bin_path}")
    return True

if __name__ == "__main__":
    print("--- CONVERSIONE TEXTURE ---")
    # Legge dalla tua cartella pngs e salva dove si aspetta il Makefile
    convert_png_to_3ds_bin("pngs/outdoor.png", "data/bin/outdoor.bin", 64, 128)