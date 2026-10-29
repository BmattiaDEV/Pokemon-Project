import os
from PIL import Image

def convert_png_to_3ds_bin(png_path, bin_path, target_w=None, target_h=None):
    # Controlliamo se l'immagine esiste
    if not os.path.exists(png_path):
        print(f"Errore: File '{png_path}' non trovato. Assicurati di averlo messo nella cartella principale.")
        return False
    
    # Apriamo l'immagine e forziamo il formato RGBA (4 canali: Rosso, Verde, Blu, Alpha)
    img = Image.open(png_path).convert("RGBA")
    
    # Il 3DS richiede texture che siano "potenze di 2". 
    # Se passiamo target_w e target_h, lo script ridimensiona l'immagine perfettamente senza sfocarla (NEAREST)
    if target_w and target_h:
        img = img.resize((target_w, target_h), Image.NEAREST)
        
    w, h = img.size
    pixels = img.tobytes()
    
    # Scriviamo i byte grezzi nel file .bin
    with open(bin_path, "wb") as f:
        f.write(pixels)
        
    print(f"Convertito con successo: {png_path} ({w}x{h}) -> {bin_path}")
    return True

if __name__ == "__main__":
    # Creiamo le cartelle di destinazione basate sulla tua struttura se non esistono
    os.makedirs("data/bin", exist_ok=True)
    
    print("--- INIZIO CONVERSIONE ---")
    
    # 1. IL NUOVO TILESET: Legge outdoor_2.png e lo salva come outdoor.bin
    # Lo forziamo a 64x128 in modo che la GPU lo legga correttamente
    convert_png_to_3ds_bin("pngs/outdoor.png", "data/bin/outdoor.bin", 64, 128)
    
    # 2. I POKEMON (Se li hai ancora nella root, altrimenti puoi commentare queste righe)
    #convert_png_to_3ds_bin("Charizard_back.png", "data/bin/Charizard_back.bin")
    #convert_png_to_3ds_bin("Charizard_front.png", "data/bin/Charizard_front.bin")
    
    print("--- CONVERSIONE COMPLETATA ---")