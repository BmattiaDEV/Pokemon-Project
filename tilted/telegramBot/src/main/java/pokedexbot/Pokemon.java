package pokedexbot;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Modello dati di un Pokémon per il CMS del Pokédex 3DS.
 * Utilizza org.json.JSONObject e JSONArray (Parte 2 del PDF: get() strict e opt() safe)
 * per gestire l'intera cartella/archivio ZIP degli sprite (front, back, sprite/icon).
 */
public class Pokemon {

    private int id;
    private String nome;
    private String descrizione;
    private String tipo1;
    private String tipo2;
    private String curvaCrescita;
    private String gruppoUova;

    // Statistiche base (1 - 255)
    private int ps;
    private int attacco;
    private int difesa;
    private int attaccoSpeciale;
    private int difesaSpeciale;
    private int velocita;

    // Dati dell'intera cartella Sprite e del relativo archivio ZIP per il 3DS
    private String spriteFolderName;     // es. "uccello" (cartella sprites/uccello/)
    private String spriteZipFileName;    // es. "uccello.zip"
    private List<String> spriteFiles;    // es. ["uccello_back.png (1024x2048 px)", "uccello_front.png (1024x2048 px)", "uccello_sprite.png (256x256 px)"]
    private String spriteFrontFile;      // es. "uccello_front.png"
    private String spriteBackFile;       // es. "uccello_back.png"
    private String spriteIconFile;       // es. "uccello_sprite.png"
    private int spriteWidth;
    private int spriteHeight;

    public Pokemon() {
        this.tipo2 = "Nessuno";
        this.curvaCrescita = "Media-Rapida";
        this.gruppoUova = "Campo";
        this.spriteFolderName = "";
        this.spriteZipFileName = "";
        this.spriteFiles = new ArrayList<>();
        this.spriteFrontFile = "";
        this.spriteBackFile = "";
        this.spriteIconFile = "";
        this.spriteWidth = 512;
        this.spriteHeight = 1024;
    }

    /**
     * Converte l'oggetto Pokemon in un JSONObject (Parte 2 del PDF: Costruire un JSONObject).
     */
    public JSONObject toJSON() {
        JSONObject obj = new JSONObject();
        obj.put("id", this.id);
        obj.put("nome", this.nome);
        obj.put("descrizione", this.descrizione);
        obj.put("tipo1", this.tipo1);
        obj.put("tipo2", this.tipo2);
        obj.put("curva_crescita", this.curvaCrescita);
        obj.put("gruppo_uova", this.gruppoUova);

        // Sotto-oggetto nidificato per le statistiche (Slide 6: Nidificazione JSONObject)
        JSONObject stats = new JSONObject();
        stats.put("ps", this.ps);
        stats.put("attacco", this.attacco);
        stats.put("difesa", this.difesa);
        stats.put("attacco_speciale", this.attaccoSpeciale);
        stats.put("difesa_speciale", this.difesaSpeciale);
        stats.put("velocita", this.velocita);
        obj.put("statistiche", stats);

        // Sotto-oggetto nidificato per l'intera cartella/ZIP degli sprite 3DS
        JSONObject sprite = new JSONObject();
        sprite.put("folder_name", this.spriteFolderName != null ? this.spriteFolderName : "");
        sprite.put("zip_file", this.spriteZipFileName != null ? this.spriteZipFileName : "");
        sprite.put("front_file", this.spriteFrontFile != null ? this.spriteFrontFile : "");
        sprite.put("back_file", this.spriteBackFile != null ? this.spriteBackFile : "");
        sprite.put("icon_file", this.spriteIconFile != null ? this.spriteIconFile : "");
        sprite.put("width", this.spriteWidth);
        sprite.put("height", this.spriteHeight);

        JSONArray filesArr = new JSONArray();
        if (this.spriteFiles != null) {
            for (String f : this.spriteFiles) {
                filesArr.put(f);
            }
        }
        sprite.put("files", filesArr);
        obj.put("sprite", sprite);

        return obj;
    }

    /**
     * Ricostruisce un Pokemon da un JSONObject usando:
     * - Metodi strict get*() per i campi obbligatori (Slide 7 del PDF)
     * - Metodi safe opt*() con valore di fallback per i campi opzionali (Slide 8 del PDF)
     */
    public static Pokemon fromJSON(JSONObject obj) {
        Pokemon p = new Pokemon();

        // Campi obbligatori -> getInt(), getString() (Slide 7)
        p.id = obj.getInt("id");
        p.nome = obj.getString("nome");

        // Campi con fallback sicuro -> optString() (Slide 8)
        p.descrizione = obj.optString("descrizione", "Nessuna descrizione disponibile.");
        p.tipo1 = obj.optString("tipo1", "Normale");
        p.tipo2 = obj.optString("tipo2", "Nessuno");
        p.curvaCrescita = obj.optString("curva_crescita", "Media-Rapida");
        p.gruppoUova = obj.optString("gruppo_uova", "Campo");

        JSONObject stats = obj.optJSONObject("statistiche");
        if (stats != null) {
            p.ps = stats.optInt("ps", 50);
            p.attacco = stats.optInt("attacco", 50);
            p.difesa = stats.optInt("difesa", 50);
            p.attaccoSpeciale = stats.optInt("attacco_speciale", 50);
            p.difesaSpeciale = stats.optInt("difesa_speciale", 50);
            p.velocita = stats.optInt("velocita", 50);
        }

        JSONObject sprite = obj.optJSONObject("sprite");
        if (sprite != null) {
            String legacyFile = sprite.optString("file_name", "");
            p.spriteFolderName = sprite.optString("folder_name", "");
            p.spriteZipFileName = sprite.optString("zip_file", "");
            p.spriteFrontFile = sprite.optString("front_file", legacyFile);
            p.spriteBackFile = sprite.optString("back_file", "");
            p.spriteIconFile = sprite.optString("icon_file", "");
            p.spriteWidth = sprite.optInt("width", 512);
            p.spriteHeight = sprite.optInt("height", 1024);

            JSONArray filesArr = sprite.optJSONArray("files");
            if (filesArr != null) {
                for (int i = 0; i < filesArr.length(); i++) {
                    p.spriteFiles.add(filesArr.optString(i));
                }
            } else if (!legacyFile.isEmpty()) {
                p.spriteFiles.add(legacyFile);
            }
        }

        return p;
    }

    public int getTotaleStatistiche() {
        return ps + attacco + difesa + attaccoSpeciale + difesaSpeciale + velocita;
    }

    /**
     * Formatta la scheda completa per Telegram mostrando l'INTERA cartella Sprite e il file ZIP.
     */
    public String toTelegramCard() {
        String tipi = (tipo2 == null || tipo2.equalsIgnoreCase("Nessuno") || tipo2.isEmpty())
                ? tipo1
                : tipo1 + " / " + tipo2;

        String folderDisplay = (spriteFolderName == null || spriteFolderName.isEmpty())
                ? "Nessuna cartella"
                : "sprites/" + spriteFolderName + "/";
        String zipDisplay = (spriteZipFileName == null || spriteZipFileName.isEmpty())
                ? "Nessun ZIP"
                : spriteZipFileName;

        StringBuilder filesBuilder = new StringBuilder();
        if (spriteFiles == null || spriteFiles.isEmpty()) {
            filesBuilder.append("  • _Nessun file sprite caricato_\n");
        } else {
            for (String fileInfo : spriteFiles) {
                filesBuilder.append("  • `").append(fileInfo).append("`\n");
            }
        }

        return String.format(
                "📖 *POKÉDEX #%03d — %s*\n" +
                "━━━━━━━━━━━━━━━━━━━━\n" +
                "🏷 *Tipo:* %s\n" +
                "📈 *Curva Crescita:* %s\n" +
                "🥚 *Gruppo Uova:* %s\n" +
                "📝 *Descrizione:* _%s_\n\n" +
                "📊 *Statistiche Base (BST: %d):*\n" +
                "• PS: `%d` | ATT: `%d` | DIF: `%d`\n" +
                "• ASP: `%d` | DSP: `%d` | VEL: `%d`\n\n" +
                "📦 *Archivio ZIP Sprite:* `%s`\n" +
                "📁 *Cartella Sprite (%d file):* `%s`\n" +
                "%s",
                id, nome,
                tipi,
                curvaCrescita,
                gruppoUova,
                descrizione,
                getTotaleStatistiche(),
                ps, attacco, difesa,
                attaccoSpeciale, difesaSpeciale, velocita,
                zipDisplay,
                spriteFiles != null ? spriteFiles.size() : 0,
                folderDisplay,
                filesBuilder.toString().stripTrailing()
        );
    }

    /**
     * Restituisce un identificatore C sicuro (es. "Sparveloce" -> "SPARVELOCE").
     */
    public String getCIdentifierUpper() {
        String clean = nome.toUpperCase().replaceAll("[^A-Z0-9]+", "_").replaceAll("^_+|_+$", "");
        return clean.isEmpty() ? "MON_" + id : clean;
    }

    /**
     * Restituisce uno slug in snake_case per i file e le cartelle (es. "Sparveloce" -> "sparveloce").
     */
    public String getSlugLower() {
        String clean = nome.toLowerCase().replaceAll("[^a-z0-9]+", "_").replaceAll("^_+|_+$", "");
        return clean.isEmpty() ? "pokemon_" + id : clean;
    }

    // Getters & Setters
    public int getId() { return id; }
    public void setId(int id) { this.id = id; }

    public String getNome() { return nome; }
    public void setNome(String nome) { this.nome = nome; }

    public String getDescrizione() { return descrizione; }
    public void setDescrizione(String descrizione) { this.descrizione = descrizione; }

    public String getTipo1() { return tipo1; }
    public void setTipo1(String tipo1) { this.tipo1 = tipo1; }

    public String getTipo2() { return tipo2; }
    public void setTipo2(String tipo2) { this.tipo2 = tipo2; }

    public String getCurvaCrescita() { return curvaCrescita; }
    public void setCurvaCrescita(String curvaCrescita) { this.curvaCrescita = curvaCrescita; }

    public String getGruppoUova() { return gruppoUova; }
    public void setGruppoUova(String gruppoUova) { this.gruppoUova = gruppoUova; }

    public int getPs() { return ps; }
    public void setPs(int ps) { this.ps = ps; }

    public int getAttacco() { return attacco; }
    public void setAttacco(int attacco) { this.attacco = attacco; }

    public int getDifesa() { return difesa; }
    public void setDifesa(int difesa) { this.difesa = difesa; }

    public int getAttaccoSpeciale() { return attaccoSpeciale; }
    public void setAttaccoSpeciale(int attaccoSpeciale) { this.attaccoSpeciale = attaccoSpeciale; }

    public int getDifesaSpeciale() { return difesaSpeciale; }
    public void setDifesaSpeciale(int difesaSpeciale) { this.difesaSpeciale = difesaSpeciale; }

    public int getVelocita() { return velocita; }
    public void setVelocita(int velocita) { this.velocita = velocita; }

    public String getSpriteFolderName() { return spriteFolderName; }
    public void setSpriteFolderName(String spriteFolderName) { this.spriteFolderName = spriteFolderName; }

    public String getSpriteZipFileName() { return spriteZipFileName; }
    public void setSpriteZipFileName(String spriteZipFileName) { this.spriteZipFileName = spriteZipFileName; }

    public List<String> getSpriteFiles() {
        return spriteFiles != null ? Collections.unmodifiableList(spriteFiles) : Collections.emptyList();
    }

    public void setSpriteFiles(List<String> spriteFiles) {
        this.spriteFiles = new ArrayList<>(spriteFiles);
    }

    public String getSpriteFrontFile() { return spriteFrontFile; }
    public void setSpriteFrontFile(String spriteFrontFile) { this.spriteFrontFile = spriteFrontFile; }

    public String getSpriteBackFile() { return spriteBackFile; }
    public void setSpriteBackFile(String spriteBackFile) { this.spriteBackFile = spriteBackFile; }

    public String getSpriteIconFile() { return spriteIconFile; }
    public void setSpriteIconFile(String spriteIconFile) { this.spriteIconFile = spriteIconFile; }

    public int getSpriteWidth() { return spriteWidth; }
    public void setSpriteWidth(int spriteWidth) { this.spriteWidth = spriteWidth; }

    public int getSpriteHeight() { return spriteHeight; }
    public void setSpriteHeight(int spriteHeight) { this.spriteHeight = spriteHeight; }
}
