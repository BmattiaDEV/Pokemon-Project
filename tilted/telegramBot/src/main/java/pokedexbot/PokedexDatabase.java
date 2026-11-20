package pokedexbot;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * FASE 5: Gestione del database intermedio in formato JSON (pokedex_db.json).
 * Funge da "fonte di verità" leggibile e modificabile usando la libreria org.json
 * (Parte 2 del PDF: JSONObject e JSONArray) e sincronizza l'intera cartella Sprite
 * e il relativo file .ZIP per ogni Pokémon.
 */
public class PokedexDatabase {

    private final Path dbPath;
    private final Path spritesDir;
    private final List<Pokemon> listaPokemon;

    public PokedexDatabase(Path dbPath) {
        this(dbPath, dbPath.getParent() != null ? dbPath.getParent().resolve("sprites") : Path.of("sprites"));
    }

    public PokedexDatabase(Path dbPath, Path spritesDir) {
        this.dbPath = dbPath;
        this.spritesDir = spritesDir;
        this.listaPokemon = new ArrayList<>();
        carica();
    }

    /**
     * Carica il database dal file JSON sul disco, sincronizza l'intera cartella sprite
     * (creando il relativo file .zip con tutti i file della cartella) o lo inizializza.
     */
    public synchronized void carica() {
        listaPokemon.clear();

        if (!Files.exists(dbPath)) {
            inizializzaDatabaseDefault();
            salva();
            return;
        }

        try {
            String testoGrezzo = Files.readString(dbPath, StandardCharsets.UTF_8);
            // Costruzione dell'oggetto mappato (Slide 6 del PDF: new JSONObject(testoGrezzo))
            JSONObject root = new JSONObject(testoGrezzo);
            JSONArray array = root.optJSONArray("pokemon");

            boolean aggiornatoConCartellaIntera = false;
            if (array != null) {
                for (int i = 0; i < array.length(); i++) {
                    JSONObject item = array.getJSONObject(i);
                    Pokemon p = Pokemon.fromJSON(item);
                    // Se un Pokémon aveva solo "uccello_front.png" senza l'intera cartella/ZIP,
                    // aggiorniamo automaticamente i suoi metadati con tutta la cartella "uccello" e "uccello.zip"!
                    if (sincronizzaCartellaSpritePokemon(p)) {
                        aggiornatoConCartellaIntera = true;
                    }
                    listaPokemon.add(p);
                }
            }
            ordinaPerId();
            if (aggiornatoConCartellaIntera) {
                salva();
            }
        } catch (Exception e) {
            System.err.println("[DB] Errore lettura " + dbPath + ": " + e.getMessage());
        }
    }

    /**
     * Assicura che il Pokémon punti all'intera cartella degli sprite (es. sprites/uccello/)
     * e al pacchetto completo .zip (es. uccello.zip con uccello_front.png, uccello_back.png, uccello_sprite.png).
     */
    public boolean sincronizzaCartellaSpritePokemon(Pokemon p) {
        try {
            String folder = p.getSpriteFolderName();
            if (folder == null || folder.isEmpty()) {
                // Ricava il nome della cartella (es. da "uccello_front.png" -> "uccello")
                String front = p.getSpriteFrontFile();
                if (front != null && front.contains("_")) {
                    folder = front.substring(0, front.indexOf('_'));
                } else {
                    folder = "uccello";
                }
            }

            SpriteValidator.EsitoCartellaZip esito = SpriteValidator.impacchettaCartellaSprite(folder, spritesDir);
            if (esito.isValido()) {
                boolean changed = !folder.equals(p.getSpriteFolderName())
                        || !esito.getZipFileName().equals(p.getSpriteZipFileName())
                        || p.getSpriteFiles().size() != esito.getDescrizioniFile().size();

                p.setSpriteFolderName(esito.getFolderName());
                p.setSpriteZipFileName(esito.getZipFileName());
                p.setSpriteFiles(esito.getDescrizioniFile());
                p.setSpriteFrontFile(esito.getFrontFile());
                p.setSpriteBackFile(esito.getBackFile());
                p.setSpriteIconFile(esito.getIconFile());
                p.setSpriteWidth(esito.getMainWidth());
                p.setSpriteHeight(esito.getMainHeight());
                return changed;
            }
        } catch (IOException e) {
            System.err.println("[DB] Avviso sincronizzazione cartella sprite: " + e.getMessage());
        }
        return false;
    }

    /**
     * Salva l'intero elenco dei Pokémon nel file pokedex_db.json formattato (indentazione a 4 spazi).
     */
    public synchronized void salva() {
        ordinaPerId();
        JSONObject root = new JSONObject();
        root.put("progetto", "Pokemon Dusk & Dawn 3DS");
        root.put("versione_schema", 2);
        root.put("totale_pokemon", listaPokemon.size());

        JSONArray array = new JSONArray();
        for (Pokemon p : listaPokemon) {
            array.put(p.toJSON());
        }
        root.put("pokemon", array);

        try {
            if (dbPath.getParent() != null) {
                Files.createDirectories(dbPath.getParent());
            }
            Files.writeString(dbPath, root.toString(4), StandardCharsets.UTF_8);
        } catch (IOException e) {
            System.err.println("[DB] Errore salvataggio " + dbPath + ": " + e.getMessage());
        }
    }

    /**
     * Popola il database iniziale con il primo Pokémon includendo l'INTERA cartella sprites/uccello/
     * (uccello_back.png, uccello_front.png, uccello_sprite.png) e l'archivio uccello.zip.
     */
    private void inizializzaDatabaseDefault() {
        Pokemon starterBird = new Pokemon();
        starterBird.setId(1);
        starterBird.setNome("Sparveloce");
        starterBird.setDescrizione("Un rapido Pokémon uccello che pattuglia le foreste dall'alto con vista acutissima.");
        starterBird.setTipo1("Normale");
        starterBird.setTipo2("Volante");
        starterBird.setCurvaCrescita("Media-Lenta");
        starterBird.setGruppoUova("Volante");
        starterBird.setPs(45);
        starterBird.setAttacco(60);
        starterBird.setDifesa(40);
        starterBird.setAttaccoSpeciale(35);
        starterBird.setDifesaSpeciale(40);
        starterBird.setVelocita(75);
        starterBird.setSpriteFolderName("uccello");

        sincronizzaCartellaSpritePokemon(starterBird);
        listaPokemon.add(starterBird);
    }

    private void ordinaPerId() {
        listaPokemon.sort(Comparator.comparingInt(Pokemon::getId));
    }

    public synchronized List<Pokemon> getTutti() {
        return Collections.unmodifiableList(new ArrayList<>(listaPokemon));
    }

    public synchronized int size() {
        return listaPokemon.size();
    }

    public synchronized int getProssimoId() {
        int max = 0;
        for (Pokemon p : listaPokemon) {
            if (p.getId() > max) {
                max = p.getId();
            }
        }
        return max + 1;
    }

    public synchronized Pokemon getById(int id) {
        for (Pokemon p : listaPokemon) {
            if (p.getId() == id) {
                return p;
            }
        }
        return null;
    }

    public synchronized Pokemon getByIndex(int index) {
        if (index < 0 || index >= listaPokemon.size()) {
            return null;
        }
        return listaPokemon.get(index);
    }

    public synchronized int getIndexById(int id) {
        for (int i = 0; i < listaPokemon.size(); i++) {
            if (listaPokemon.get(i).getId() == id) {
                return i;
            }
        }
        return -1;
    }

    public synchronized void aggiungiOSostituisci(Pokemon nuovo) {
        listaPokemon.removeIf(p -> p.getId() == nuovo.getId());
        listaPokemon.add(nuovo);
        salva();
    }

    public synchronized boolean eliminaById(int id) {
        boolean rimosso = listaPokemon.removeIf(p -> p.getId() == id);
        if (rimosso) {
            salva();
        }
        return rimosso;
    }

    public Path getDbPath() {
        return dbPath;
    }
}
