package pokedexbot;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Duration;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;

/**
 * ============================================================================
 * POKÉDEX 3DS PORTABLE CMS — BOT TELEGRAM IN JAVA
 * ============================================================================
 * Basato sull'architettura del PDF del Professore:
 * - PARTE 1: Architettura Long Polling (while(true), timeout=10, offset = update_id + 1)
 * - PARTE 2: Manipolazione JSON con org.json (JSONObject, JSONArray, metodi get() e opt())
 * - PARTE 3: Networking nativo Java 11+ (java.net.http: HttpClient, HttpRequest, HttpResponse)
 *
 * Esteso con le 7 Fasi del CMS Pokédex per Nintendo 3DS:
 * 1. Registrazione comandi BotFather e configurazione GitHub PAT
 * 2. Macchina a stati per inserimento/modifica guidata per singolo utente (chatId)
 * 3. Tastiere Inline interattive (callback_query + editMessageText per non intasare la chat)
 * 4. Download e validazione dell'intera cartella Sprite (file .ZIP o .PNG) per 3DS
 * 5. Salvataggio nel database intermedio JSON (pokedex_db.json) con /lista, /sprites, /modifica, /elimina
 * 6. Generazione automatica di pokedex_data.h e pokedex_data.c per devkitARM e invio in chat
 * 7. Sincronizzazione automatica con la repository GitHub Pokemon-Project via REST API
 */
public class PokedexBot {

    // ========================================================================
    // PARTE 3 DEL PDF (Slide 10): Creazione una-tantum di HttpClient statico
    // ========================================================================
    private static final HttpClient client = HttpClient.newBuilder()
            .connectTimeout(Duration.ofSeconds(15))
            .build();

    // Configurazione Token e Percorsi
    private static String BOT_TOKEN = "9009047721:AAFx1EXL3ow7rFgJ3gCHuU2WImBMlVu8NWU";
    private static String BASE_API_URL = "https://api.telegram.org/bot" + BOT_TOKEN;
    private static String FILE_API_URL = "https://api.telegram.org/file/bot" + BOT_TOKEN;

    private static final Path BASE_DIR = Paths.get("").toAbsolutePath();
    private static final Path CONFIG_PATH = BASE_DIR.resolve("config.properties");
    private static final Path DB_PATH = BASE_DIR.resolve("pokedex_db.json");
    private static final Path SPRITES_DIR = BASE_DIR.resolve("sprites");
    private static final Path GENERATED_C_DIR = BASE_DIR.resolve("generated_c");

    // Opzioni chiuse per le Tastiere Inline (Fase 3)
    private static final String[] TIPI_POKEMON = {
            "Fuoco", "Acqua", "Erba", "Elettro", "Volante", "Normale",
            "Drago", "Psico", "Buio", "Acciaio", "Roccia", "Terra",
            "Ghiaccio", "Lotta", "Veleno", "Coleottero", "Spettro", "Folletto"
    };

    private static final String[] CURVE_CRESCITA = {
            "Rapida", "Media-Rapida", "Media-Lenta", "Lenta", "Erratica", "Fluttuante"
    };

    private static final String[] GRUPPI_UOVA = {
            "Mostro", "Acqua 1", "Coleottero", "Volante", "Campo", "Magico",
            "Erba", "Umanoide", "Minerale", "Amorfo", "Drago", "Sconosciuto"
    };

    // Componenti del CMS (Fasi 2, 5, 7)
    private static final Map<Long, SessioneUtente> sessioniUtenti = new HashMap<>();
    private static PokedexDatabase database;
    private static GitHubSyncService gitHubService;

    public static void main(String[] args) {
        System.out.println("====================================================");
        System.out.println("   POKÉDEX 3DS CMS - AVVIO BOT TELEGRAM IN JAVA     ");
        System.out.println("====================================================");

        try {
            Files.createDirectories(SPRITES_DIR);
            Files.createDirectories(GENERATED_C_DIR);
        } catch (IOException e) {
            System.err.println("Errore creazione cartelle: " + e.getMessage());
        }

        caricaConfigurazione();
        database = new PokedexDatabase(DB_PATH, SPRITES_DIR);

        // Fase 1: Imposta automaticamente il menu dei comandi su Telegram
        impostaComandiMenuTelegram();

        System.out.println("[OK] Database JSON caricato: " + database.size() + " Pokémon presenti.");
        System.out.println("[OK] Bot in ascolto (Long Polling timeout=10s) su t.me/Pokemon_DuskDawnBot ...");

        // ====================================================================
        // PARTE 1 DEL PDF (Slide 2, 3, 4): Loop infinito di Long Polling e Offset
        // ====================================================================
        long offset = 0;

        while (true) {
            try {
                // Costruzione URL con offset e timeout=10 (Slide 2 e Slide 4 del PDF)
                String url = BASE_API_URL + "/getUpdates?offset=" + offset + "&timeout=10";

                // Slide 11 del PDF: Configurazione chiamata GET con Pattern Builder
                HttpRequest request = HttpRequest.newBuilder()
                        .uri(URI.create(url))
                        .GET()
                        .build();

                // Slide 10 e 13 del PDF: Esecuzione bloccante client.send() e BodyHandlers.ofString()
                HttpResponse<String> risposta = client.send(request, HttpResponse.BodyHandlers.ofString());
                String jsonGrezzo = risposta.body();

                // Slide 6 del PDF: Costruzione del JSONObject dalla stringa grezza HTTP
                JSONObject jsonTelegram = new JSONObject(jsonGrezzo);

                // Slide 8 del PDF: Metodo safe optBoolean("ok", false)
                boolean statusOk = jsonTelegram.optBoolean("ok", false);
                if (!statusOk) {
                    System.err.println("[TELEGRAM] Risposta non OK: " + jsonGrezzo);
                    Thread.sleep(2000);
                    continue;
                }

                // Estrazione dell'array "result" contenente gli update in coda
                JSONArray results = jsonTelegram.getJSONArray("result");

                for (int i = 0; i < results.length(); i++) {
                    JSONObject update = results.getJSONObject(i);

                    // Slide 4 del PDF: Il ruolo critico dell'Update ID
                    long idCorrente = update.getLong("update_id");
                    offset = idCorrente + 1;

                    // Gestione dell'update (messaggio testuale/documento oppure clic su Tastiera Inline)
                    gestisciUpdate(update);
                }

            } catch (IOException | InterruptedException e) {
                // Slide 10 del PDF: Il metodo send() genera IOException / InterruptedException
                System.err.println("[RETE] Errore durante il Long Polling: " + e.getMessage());
                try {
                    Thread.sleep(3000);
                } catch (InterruptedException ignored) {
                    Thread.currentThread().interrupt();
                    break;
                }
            } catch (Exception e) {
                System.err.println("[ERRORE] Eccezione nella gestione update: " + e.getMessage());
                e.printStackTrace();
            }
        }
    }

    // ========================================================================
    // SMISTAMENTO DEGLI UPDATE (Messaggi, Documenti e Callback Tastiere Inline)
    // ========================================================================
    private static void gestisciUpdate(JSONObject update) {
        // 1. Controlliamo con optJSONObject (Slide 8) se l'utente ha cliccato un pulsante Inline (Fase 3)
        JSONObject callbackQuery = update.optJSONObject("callback_query");
        if (callbackQuery != null) {
            gestisciCallbackInline(callbackQuery);
            return;
        }

        // 2. Controlliamo se è un normale messaggio in chat (Slide 3 del PDF)
        JSONObject message = update.optJSONObject("message");
        if (message == null) {
            return;
        }

        // Slide 3 e 7 del PDF: Estrazione obbligatoria del chat_id
        long chatId = message.getJSONObject("chat").getLong("id");
        SessioneUtente sessione = sessioniUtenti.computeIfAbsent(chatId, k -> new SessioneUtente());

        // Controllo se l'utente ha inviato un Documento (Fase 4: Archivio .ZIP della cartella sprite o .PNG)
        JSONObject document = message.optJSONObject("document");
        if (document != null) {
            gestisciDocumentoRicevuto(chatId, sessione, document);
            return;
        }

        // Controllo se l'utente ha inviato una Foto compressa mentre il bot aspettava lo Sprite
        JSONArray photo = message.optJSONArray("photo");
        if (photo != null) {
            if (sessione.getFaseCorrente() == SessioneUtente.Fase.ATTESA_SPRITE
                    || sessione.getFaseCorrente() == SessioneUtente.Fase.MODIFICA_SPRITE) {
                inviaMessaggio(chatId,
                        "⚠️ *Hai inviato l'immagine come Foto compressa!*\n\n" +
                        "Telegram comprime le foto normali in JPEG rimuovendo la **trasparenza (canale Alpha)** necessaria per il 3DS.\n" +
                        "📎 Premi l'icona della graffetta -> **File / Documento** e invia il file **.ZIP** con l'intera cartella sprite (oppure i file `.png`)!",
                        null);
            }
            return;
        }

        // Slide 8 del PDF: Uso di optString per estrarre il testo in modo sicuro
        String testo = message.optString("text", "").trim();
        if (testo.isEmpty()) {
            return;
        }

        gestisciMessaggioTesto(chatId, sessione, testo);
    }

    // ========================================================================
    // GESTIONE COMANDI E MACCHINA A STATI (FASE 2)
    // ========================================================================
    private static void gestisciMessaggioTesto(long chatId, SessioneUtente sessione, String testo) {
        String comando = testo.split("\\s+")[0].toLowerCase();
        if (comando.contains("@")) {
            comando = comando.substring(0, comando.indexOf('@'));
        }

        // Comandi globali sempre attivi (anche a metà inserimento)
        switch (comando) {
            case "/start":
            case "/help":
            case "/go": // Comando base presente nella Slide 3 del PDF
                inviaMessaggioBenvenuto(chatId);
                return;

            case "/annulla":
                sessione.reset();
                inviaMessaggio(chatId, "🛑 *Operazione annullata.*\nLa memoria temporanea è stata pulita. Usa /nuovo o /lista per ricominciare.", null);
                return;

            case "/nuovo":
                int nuovoId = database.getProssimoId();
                sessione.iniziaNuovoPokemon(nuovoId);
                inviaMessaggio(chatId,
                        String.format("🆕 *Creazione Nuovo Pokémon (#%03d)*\n" +
                                      "━━━━━━━━━━━━━━━━━━━━\n" +
                                      "*Fase 1/8 — Nome:*\n" +
                                      "Scrivi il **nome** della nuova creatura (oppure /annulla per uscire):", nuovoId),
                        creaTastieraAnnulla());
                return;

            case "/lista":
                inviaOSfogliaLista(chatId, null, 0);
                return;

            case "/sprites":
                inviaPanoramicaTutteCartelleSprite(chatId);
                return;

            case "/modifica":
                gestisciComandoModifica(chatId, testo);
                return;

            case "/elimina":
                gestisciComandoElimina(chatId, testo);
                return;

            case "/esporta":
                eseguiEsportazioneC(chatId, true);
                return;

            case "/github":
                eseguiSincronizzazioneGitHub(chatId);
                return;

            case "/github_token":
                gestisciImpostazioneTokenGitHub(chatId, testo);
                return;

            default:
                break;
        }

        // Se non è un comando speciale, facciamo avanzare la Macchina a Stati (Fase 2)
        switch (sessione.getFaseCorrente()) {
            case IDLE:
                inviaMessaggio(chatId,
                        "ℹ️ Nessuna operazione in corso.\n" +
                        "Usa /nuovo per creare un Pokémon, /lista per sfogliare il Pokédex e scaricare lo ZIP degli sprite, /sprites per vedere tutte le cartelle sprite o /esporta per generare il codice C 3DS.",
                        null);
                break;

            case ATTESA_NOME:
                if (testo.length() < 2 || testo.length() > 24) {
                    inviaMessaggio(chatId, "⚠️ Il nome deve avere tra 2 e 24 caratteri. Riprova:", null);
                    return;
                }
                sessione.getPokemonInCostruzione().setNome(testo);
                sessione.setFaseCorrente(SessioneUtente.Fase.ATTESA_DESCRIZIONE);
                inviaMessaggio(chatId,
                        "✅ Nome impostato: *" + testo + "*\n\n" +
                        "*Fase 2/8 — Descrizione Pokédex:*\n" +
                        "Scrivi una breve descrizione della creatura per il Pokédex:",
                        creaTastieraAnnulla());
                break;

            case ATTESA_DESCRIZIONE:
                if (testo.length() < 5 || testo.length() > 200) {
                    inviaMessaggio(chatId, "⚠️ La descrizione deve avere tra 5 e 200 caratteri. Riprova:", null);
                    return;
                }
                sessione.getPokemonInCostruzione().setDescrizione(testo);
                sessione.setFaseCorrente(SessioneUtente.Fase.SCELTA_TIPO_1);
                inviaMessaggio(chatId,
                        "✅ Descrizione salvata!\n\n" +
                        "*Fase 3/8 — Scelta Tipo Primario (Tipo 1):*\n" +
                        "Seleziona il **Tipo 1** dalla tastiera interattiva qui sotto:",
                        creaTastieraTipi("tipo1", false));
                break;

            case SCELTA_TIPO_1:
            case SCELTA_TIPO_2:
            case SCELTA_CURVA_CRESCITA:
            case SCELTA_GRUPPO_UOVA:
                inviaMessaggio(chatId, "👆 Usa i **pulsanti interattivi** nel messaggio sopra (oppure scrivi /annulla).", null);
                break;

            case ATTESA_STATISTICHE:
                processaInputStatistiche(chatId, sessione, testo);
                break;

            case ATTESA_SPRITE:
            case MODIFICA_SPRITE:
                inviaMessaggio(chatId,
                        "📦 Sto aspettando l'archivio **.ZIP dell'intera cartella Sprite** (con `front.png`, `back.png`, `sprite.png`) oppure un file `.png`!\n" +
                        "Allegalo come **File / Documento**, oppure scegli una delle cartelle Sprite già presenti cliccando i pulsanti sopra.",
                        null);
                break;

            case MODIFICA_TESTO:
                processaModificaCampoEsistente(chatId, sessione, testo);
                break;
        }
    }

    // ========================================================================
    // FASE 3: GESTIONE CLICK TASTIERE INLINE (callback_query + editMessageText)
    // ========================================================================
    private static void gestisciCallbackInline(JSONObject callbackQuery) {
        String callbackId = callbackQuery.getString("id");
        String data = callbackQuery.optString("data", "");
        JSONObject message = callbackQuery.optJSONObject("message");
        if (message == null) {
            rispondiCallbackQuery(callbackId, "");
            return;
        }

        long chatId = message.getJSONObject("chat").getLong("id");
        long messageId = message.getLong("message_id");
        SessioneUtente sessione = sessioniUtenti.computeIfAbsent(chatId, k -> new SessioneUtente());

        // 1. Pulsante Annulla
        if ("annulla".equals(data)) {
            sessione.reset();
            rispondiCallbackQuery(callbackId, "Operazione annullata");
            modificaMessaggio(chatId, messageId, "🛑 *Operazione annullata dall'utente.*", null);
            return;
        }

        // 2. Scelta Tipo 1 (tipo1:Fuoco)
        if (data.startsWith("tipo1:")) {
            String tipoScelto = data.substring("tipo1:".length());
            if (sessione.getPokemonInCostruzione() == null) {
                rispondiCallbackQuery(callbackId, "Sessione scaduta, usa /nuovo");
                return;
            }
            sessione.getPokemonInCostruzione().setTipo1(tipoScelto);
            sessione.setFaseCorrente(SessioneUtente.Fase.SCELTA_TIPO_2);
            rispondiCallbackQuery(callbackId, "Tipo 1: " + tipoScelto);

            modificaMessaggio(chatId, messageId,
                    "✅ *Tipo 1 selezionato:* `" + tipoScelto + "`\n\n" +
                    "*Fase 4/8 — Scelta Tipo Secondario (Tipo 2):*\n" +
                    "Seleziona il **Tipo 2** oppure scegli *Nessuno (Monotipo)*:",
                    creaTastieraTipi("tipo2", true));
            return;
        }

        // 3. Scelta Tipo 2 (tipo2:Volante)
        if (data.startsWith("tipo2:")) {
            String tipo2Scelto = data.substring("tipo2:".length());
            Pokemon p = sessione.getPokemonInCostruzione();
            if (p == null) {
                rispondiCallbackQuery(callbackId, "Sessione scaduta, usa /nuovo");
                return;
            }
            if (tipo2Scelto.equalsIgnoreCase(p.getTipo1())) {
                tipo2Scelto = "Nessuno";
            }
            p.setTipo2(tipo2Scelto);
            sessione.setFaseCorrente(SessioneUtente.Fase.SCELTA_CURVA_CRESCITA);
            rispondiCallbackQuery(callbackId, "Tipo 2: " + tipo2Scelto);

            modificaMessaggio(chatId, messageId,
                    "✅ *Tipi selezionati:* `" + p.getTipo1() + (tipo2Scelto.equals("Nessuno") ? "" : " / " + tipo2Scelto) + "`\n\n" +
                    "*Fase 5/8 — Curva di Crescita:*\n" +
                    "Seleziona la velocità di livellamento del Pokémon:",
                    creaTastieraOpzioni("curva", CURVE_CRESCITA, 2));
            return;
        }

        // 4. Scelta Curva di Crescita (curva:Media-Rapida)
        if (data.startsWith("curva:")) {
            String curva = data.substring("curva:".length());
            Pokemon p = sessione.getPokemonInCostruzione();
            if (p == null) {
                rispondiCallbackQuery(callbackId, "Sessione scaduta, usa /nuovo");
                return;
            }
            p.setCurvaCrescita(curva);
            sessione.setFaseCorrente(SessioneUtente.Fase.SCELTA_GRUPPO_UOVA);
            rispondiCallbackQuery(callbackId, "Curva: " + curva);

            modificaMessaggio(chatId, messageId,
                    "✅ *Curva di crescita:* `" + curva + "`\n\n" +
                    "*Fase 6/8 — Gruppo Uova:*\n" +
                    "Seleziona il gruppo uova del Pokémon:",
                    creaTastieraOpzioni("uova", GRUPPI_UOVA, 3));
            return;
        }

        // 5. Scelta Gruppo Uova (uova:Campo)
        if (data.startsWith("uova:")) {
            String gruppo = data.substring("uova:".length());
            Pokemon p = sessione.getPokemonInCostruzione();
            if (p == null) {
                rispondiCallbackQuery(callbackId, "Sessione scaduta, usa /nuovo");
                return;
            }
            p.setGruppoUova(gruppo);
            sessione.setFaseCorrente(SessioneUtente.Fase.ATTESA_STATISTICHE);
            rispondiCallbackQuery(callbackId, "Gruppo Uova: " + gruppo);

            modificaMessaggio(chatId, messageId,
                    "✅ *Gruppo Uova:* `" + gruppo + "`\n\n" +
                    "*Fase 7/8 — Statistiche Base:*\n" +
                    "Invia i 6 valori numerici (da 1 a 255) separati da spazio o virgola in questo ordine:\n" +
                    "`PS Attacco Difesa AttSp DifSp Velocità`\n\n" +
                    "💡 _Esempio:_ `50 65 55 40 45 80`",
                    creaTastieraAnnulla());
            return;
        }

        // 6. Scelta di un'intera cartella Sprite / ZIP esistente (sprite_folder:<nomeCartella>)
        if (data.startsWith("sprite_folder:") || "sprite_default".equals(data)) {
            String folderScelta = data.startsWith("sprite_folder:")
                    ? data.substring("sprite_folder:".length())
                    : "uccello";

            Pokemon p = (sessione.getFaseCorrente() == SessioneUtente.Fase.MODIFICA_SPRITE)
                    ? database.getById(sessione.getIdPokemonInModifica())
                    : sessione.getPokemonInCostruzione();

            if (p == null) {
                rispondiCallbackQuery(callbackId, "Sessione scaduta");
                return;
            }

            p.setSpriteFolderName(folderScelta);
            database.sincronizzaCartellaSpritePokemon(p);
            database.aggiungiOSostituisci(p);
            sessione.reset();

            rispondiCallbackQuery(callbackId, "Cartella Sprite " + folderScelta + " (.zip) associata!");
            modificaMessaggio(chatId, messageId,
                    "🎉 *Pokémon salvato con l'intera cartella Sprite (`" + folderScelta + ".zip`)!*\n\n" + p.toTelegramCard(),
                    creaTastieraPostSalvataggio(p.getId()));
            return;
        }

        // 7. Paginazione /lista (lista:pageIndex)
        if (data.startsWith("lista:")) {
            int page = Integer.parseInt(data.substring("lista:".length()));
            rispondiCallbackQuery(callbackId, "");
            inviaOSfogliaLista(chatId, messageId, page);
            return;
        }

        // 8. Scarica/Mostra l'intero ZIP e tutti i file della cartella Sprite (zip_sprite:id)
        if (data.startsWith("zip_sprite:")) {
            int id = Integer.parseInt(data.substring("zip_sprite:".length()));
            Pokemon p = database.getById(id);
            if (p == null) {
                rispondiCallbackQuery(callbackId, "Pokémon non trovato");
                return;
            }
            rispondiCallbackQuery(callbackId, "Invio ZIP e file della cartella Sprite...");
            inviaZipCartellaSpritePokemon(chatId, p, true);
            return;
        }

        // 9. Apertura menu modifica da /lista (mod_menu:id)
        if (data.startsWith("mod_menu:")) {
            int id = Integer.parseInt(data.substring("mod_menu:".length()));
            Pokemon p = database.getById(id);
            if (p == null) {
                rispondiCallbackQuery(callbackId, "Pokémon non trovato");
                return;
            }
            rispondiCallbackQuery(callbackId, "Scegli cosa modificare");
            modificaMessaggio(chatId, messageId,
                    "✏️ *Modifica Pokémon #" + String.format("%03d", id) + " (" + p.getNome() + ")*\n" +
                    "Seleziona quale campo vuoi aggiornare senza rifare tutto da capo:",
                    creaTastieraSceltaCampoModifica(id));
            return;
        }

        // 10. Scelta del singolo campo da modificare (mod_campo:id:campo)
        if (data.startsWith("mod_campo:")) {
            String[] parts = data.split(":");
            int id = Integer.parseInt(parts[1]);
            String campo = parts[2];
            Pokemon p = database.getById(id);
            if (p == null) {
                rispondiCallbackQuery(callbackId, "Pokémon non trovato");
                return;
            }

            sessione.setIdPokemonInModifica(id);
            sessione.setCampoInModifica(campo);
            rispondiCallbackQuery(callbackId, "Modifica " + campo);

            if ("sprite".equals(campo)) {
                sessione.setFaseCorrente(SessioneUtente.Fase.MODIFICA_SPRITE);
                modificaMessaggio(chatId, messageId,
                        "📦 *Sostituzione Cartella Sprite (.ZIP) per " + p.getNome() + "*\n\n" +
                        "Invia ora un archivio **.ZIP** (come **File/Documento**) contenente l'intera cartella degli sprite (es. `front.png`, `back.png`, `sprite.png`), oppure seleziona una cartella sprite già pronta qui sotto:",
                        creaTastieraAttesaSprite());
            } else {
                sessione.setFaseCorrente(SessioneUtente.Fase.MODIFICA_TESTO);
                String istruzioni = "Scrivi il nuovo valore per *" + campo.toUpperCase() + "* di *" + p.getNome() + "*:";
                if ("stats".equals(campo)) {
                    istruzioni = "Invia le 6 nuove statistiche (`PS ATT DIF ASP DSP VEL`) separate da spazio per *" + p.getNome() + "*\n" +
                                 "(Valori attuali: `" + p.getPs() + " " + p.getAttacco() + " " + p.getDifesa() + " "
                                 + p.getAttaccoSpeciale() + " " + p.getDifesaSpeciale() + " " + p.getVelocita() + "`):";
                }
                modificaMessaggio(chatId, messageId, "✏️ " + istruzioni, creaTastieraAnnulla());
            }
            return;
        }

        // 11. Richiesta conferma eliminazione (del_ask:id)
        if (data.startsWith("del_ask:")) {
            int id = Integer.parseInt(data.substring("del_ask:".length()));
            Pokemon p = database.getById(id);
            if (p == null) {
                rispondiCallbackQuery(callbackId, "Pokémon già eliminato");
                return;
            }
            rispondiCallbackQuery(callbackId, "Conferma eliminazione");
            modificaMessaggio(chatId, messageId,
                    "⚠️ *Sei sicuro di voler eliminare definitivamente #" + String.format("%03d", id) + " " + p.getNome() + "?*",
                    creaTastieraConfermaEliminazione(id));
            return;
        }

        // 12. Conferma eliminazione (del_confirm:id)
        if (data.startsWith("del_confirm:")) {
            int id = Integer.parseInt(data.substring("del_confirm:".length()));
            boolean ok = database.eliminaById(id);
            rispondiCallbackQuery(callbackId, ok ? "Eliminato!" : "Non trovato");
            if (database.size() > 0) {
                inviaOSfogliaLista(chatId, messageId, 0);
            } else {
                modificaMessaggio(chatId, messageId, "🗑️ Pokémon eliminato. Il Pokédex è ora vuoto. Usa /nuovo per aggiungerne uno!", null);
            }
            return;
        }

        // 13. Azione rapida "Genera Sorgenti C" (action_esporta)
        if ("action_esporta".equals(data)) {
            rispondiCallbackQuery(callbackId, "Generazione file C in corso...");
            eseguiEsportazioneC(chatId, true);
            return;
        }

        // 14. Azione rapida "Pubblica su GitHub" (action_github)
        if ("action_github".equals(data)) {
            rispondiCallbackQuery(callbackId, "Sincronizzazione GitHub avviata...");
            eseguiSincronizzazioneGitHub(chatId);
            return;
        }

        rispondiCallbackQuery(callbackId, "");
    }

    // ========================================================================
    // FASE 3 & 4: INPUT STATISTICHE E RICEZIONE / VALIDAZIONE CARTELLA ZIP SPRITE
    // ========================================================================
    private static void processaInputStatistiche(long chatId, SessioneUtente sessione, String testo) {
        int[] valori = parseSeiStatistiche(testo);
        if (valori == null) {
            inviaMessaggio(chatId,
                    "❌ *Formato statistiche non valido!*\n" +
                    "Devi inserire esattamente **6 numeri interi compresi tra 1 e 255** separati da spazi o virgole:\n" +
                    "`PS Attacco Difesa AttSp DifSp Velocità`\n\n" +
                    "Esempio: `45 60 40 35 40 75`",
                    creaTastieraAnnulla());
            return;
        }

        Pokemon p = sessione.getPokemonInCostruzione();
        p.setPs(valori[0]);
        p.setAttacco(valori[1]);
        p.setDifesa(valori[2]);
        p.setAttaccoSpeciale(valori[3]);
        p.setDifesaSpeciale(valori[4]);
        p.setVelocita(valori[5]);

        sessione.setFaseCorrente(SessioneUtente.Fase.ATTESA_SPRITE);
        inviaMessaggio(chatId,
                String.format("✅ *Statistiche salvate!* (Totale BST: `%d`)\n\n" +
                              "*Fase 8/8 — Invio Cartella Sprite (.ZIP) per 3DS:*\n" +
                              "📦 Invia ora un archivio **.ZIP** (come **File / Documento**) contenente l'intera cartella degli sprite del Pokémon:\n" +
                              "• `<nome>_front.png` (Spritesheet frontale)\n" +
                              "• `<nome>_back.png` (Spritesheet posteriore)\n" +
                              "• `<nome>_sprite.png` (Sprite base / icona)\n\n" +
                              "🔍 *Vincoli verificati su tutti i PNG dello ZIP:*\n" +
                              "• Formato `PNG` con **trasparenza (Canale Alpha RGBA)**\n" +
                              "• Dimensioni potenze di 2 (`32x32`..`256x256`, `512x1024`, `1024x2048`)\n\n" +
                              "👇 _Oppure seleziona una cartella Sprite già presente nel progetto:_",
                        p.getTotaleStatistiche()),
                creaTastieraAttesaSprite());
    }

    private static void gestisciDocumentoRicevuto(long chatId, SessioneUtente sessione, JSONObject document) {
        SessioneUtente.Fase fase = sessione.getFaseCorrente();
        if (fase != SessioneUtente.Fase.ATTESA_SPRITE && fase != SessioneUtente.Fase.MODIFICA_SPRITE) {
            inviaMessaggio(chatId, "ℹ️ Per caricare un archivio `.zip` o `.png` di sprite, avvia prima /nuovo oppure scegli *Modifica -> Sprite* da /lista.", null);
            return;
        }

        String fileId = document.getString("file_id");
        String fileName = document.optString("file_name", "sprites.zip");

        Pokemon targetPokemon = (fase == SessioneUtente.Fase.ATTESA_SPRITE)
                ? sessione.getPokemonInCostruzione()
                : database.getById(sessione.getIdPokemonInModifica());

        if (targetPokemon == null) {
            sessione.reset();
            inviaMessaggio(chatId, "⚠️ Sessione scaduta. Usa /nuovo o /lista.", null);
            return;
        }

        inviaMessaggio(chatId, "⏳ Scaricamento e validazione dell'archivio/cartella sprite `" + fileName + "` in corso...", null);

        try {
            // 1. Scarica i byte del file (.zip o .png) da Telegram
            byte[] fileBytes = scaricaFileDaTelegram(fileId);

            // 2. Determina il nome della cartella sprite
            String targetFolderName = targetPokemon.getSlugLower();
            if (fileName.toLowerCase().endsWith(".zip")) {
                String baseZipName = fileName.substring(0, fileName.length() - 4)
                        .toLowerCase().replaceAll("[^a-z0-9]+", "_").replaceAll("^_+|_+$", "");
                if (!baseZipName.isEmpty()) {
                    targetFolderName = baseZipName;
                }
            }

            // 3. Estrae l'intera cartella, valida tutti i PNG e genera l'archivio .zip completo
            SpriteValidator.EsitoCartellaZip esito = SpriteValidator.elaboraZipOSprite3DS(
                    fileBytes, fileName, targetFolderName, SPRITES_DIR);

            if (!esito.isValido()) {
                inviaMessaggio(chatId,
                        esito.getMessaggio() + "\n\n🔄 _Invia un archivio `.zip` (o file `.png`) valido per completare l'operazione (oppure /annulla):_",
                        fase == SessioneUtente.Fase.ATTESA_SPRITE ? creaTastieraAttesaSprite() : creaTastieraAnnulla());
                return;
            }

            // 4. Aggiorna i metadati dell'intera cartella sprite sul Pokémon
            targetPokemon.setSpriteFolderName(esito.getFolderName());
            targetPokemon.setSpriteZipFileName(esito.getZipFileName());
            targetPokemon.setSpriteFiles(esito.getDescrizioniFile());
            targetPokemon.setSpriteFrontFile(esito.getFrontFile());
            targetPokemon.setSpriteBackFile(esito.getBackFile());
            targetPokemon.setSpriteIconFile(esito.getIconFile());
            targetPokemon.setSpriteWidth(esito.getMainWidth());
            targetPokemon.setSpriteHeight(esito.getMainHeight());

            // 5. Salva nel database JSON centrale
            database.aggiungiOSostituisci(targetPokemon);
            sessione.reset();

            inviaMessaggio(chatId,
                    esito.getMessaggio() + "\n\n" +
                    "🎉 *Pokémon salvato nel database JSON (`pokedex_db.json`)!*\n\n" +
                    targetPokemon.toTelegramCard(),
                    creaTastieraPostSalvataggio(targetPokemon.getId()));

        } catch (Exception e) {
            inviaMessaggio(chatId, "❌ Errore durante l'elaborazione della cartella sprite: `" + e.getMessage() + "`", null);
        }
    }

    /**
     * Invia in chat l'intero file .ZIP contenente tutta la cartella degli sprite del Pokémon
     * (es. uccello.zip con uccello_front.png, uccello_back.png, uccello_sprite.png)
     * e, se richiesto, anche tutti i singoli file PNG della cartella.
     */
    private static void inviaZipCartellaSpritePokemon(long chatId, Pokemon p, boolean inviaAncheSingoliFile) {
        try {
            database.sincronizzaCartellaSpritePokemon(p);
            String folderName = p.getSpriteFolderName();
            if (folderName == null || folderName.isEmpty()) {
                folderName = "uccello";
            }

            SpriteValidator.EsitoCartellaZip esito = SpriteValidator.impacchettaCartellaSprite(folderName, SPRITES_DIR);
            if (esito.getZipPath() != null && Files.exists(esito.getZipPath())) {
                StringBuilder caption = new StringBuilder();
                caption.append("📦 Archivio ZIP Cartella Sprite: ").append(esito.getZipFileName()).append("\n");
                caption.append("📁 Cartella: sprites/").append(folderName).append("/ (").append(esito.getFileEstratti().size()).append(" file):\n");
                for (String desc : esito.getDescrizioniFile()) {
                    caption.append("• ").append(desc).append("\n");
                }
                inviaDocumentoInChat(chatId, esito.getZipPath(), caption.toString().stripTrailing());

                if (inviaAncheSingoliFile) {
                    for (Path pngPath : esito.getFileEstratti()) {
                        inviaDocumentoInChat(chatId, pngPath,
                                "🖼 File della cartella sprites/" + folderName + "/: " + pngPath.getFileName());
                    }
                }
            } else {
                inviaMessaggio(chatId, "⚠️ Nessuna cartella sprite trovata per *" + p.getNome() + "*.", null);
            }
        } catch (IOException e) {
            inviaMessaggio(chatId, "❌ Errore creazione/invio ZIP della cartella sprite: `" + e.getMessage() + "`", null);
        }
    }

    /**
     * Comando /sprites: mostra tutte le cartelle sprite disponibili e invia i relativi archivi .ZIP completi.
     */
    private static void inviaPanoramicaTutteCartelleSprite(long chatId) {
        List<String> cartelle = elencaCartelleSpriteDisponibili();
        if (cartelle.isEmpty()) {
            inviaMessaggio(chatId, "📭 Nessuna cartella sprite trovata.", null);
            return;
        }

        for (String folder : cartelle) {
            try {
                SpriteValidator.EsitoCartellaZip esito = SpriteValidator.impacchettaCartellaSprite(folder, SPRITES_DIR);
                if (esito.getZipPath() != null && Files.exists(esito.getZipPath())) {
                    StringBuilder info = new StringBuilder();
                    info.append("📁 Cartella Sprite: sprites/").append(folder).append("/\n");
                    info.append("📦 File contenuti in ").append(esito.getZipFileName()).append(" (").append(esito.getFileEstratti().size()).append(" file):\n");
                    for (String d : esito.getDescrizioniFile()) {
                        info.append("• ").append(d).append("\n");
                    }
                    inviaDocumentoInChat(chatId, esito.getZipPath(), info.toString().stripTrailing());
                }
            } catch (IOException e) {
                inviaMessaggio(chatId, "❌ Errore lettura cartella `" + folder + "`: " + e.getMessage(), null);
            }
        }
    }

    private static List<String> elencaCartelleSpriteDisponibili() {
        List<String> folders = new ArrayList<>();
        // Controlla anche la cartella story sprites per sincronizzare eventuali nuove sottocartelle
        Path storySpritesRoot = SPRITES_DIR.toAbsolutePath().normalize()
                .resolve("../../data useful pngs for story/pokemon/sprites").normalize();
        if (Files.isDirectory(storySpritesRoot)) {
            try (DirectoryStream<Path> ds = Files.newDirectoryStream(storySpritesRoot)) {
                for (Path p : ds) {
                    if (Files.isDirectory(p)) {
                        String name = p.getFileName().toString();
                        SpriteValidator.impacchettaCartellaSprite(name, SPRITES_DIR);
                    }
                }
            } catch (IOException ignored) {
            }
        }

        if (Files.isDirectory(SPRITES_DIR)) {
            try (DirectoryStream<Path> ds = Files.newDirectoryStream(SPRITES_DIR)) {
                for (Path p : ds) {
                    if (Files.isDirectory(p)) {
                        folders.add(p.getFileName().toString());
                    }
                }
            } catch (IOException ignored) {
            }
        }
        folders.sort(String::compareToIgnoreCase);
        return folders;
    }

    // ========================================================================
    // FASE 5: /lista, /modifica, /elimina SUL DATABASE JSON
    // ========================================================================
    private static void inviaOSfogliaLista(long chatId, Long messageIdToEdit, int pageIndex) {
        int totale = database.size();
        if (totale == 0) {
            String msg = "📭 *Il Pokédex è vuoto!*\nUsa il comando /nuovo per creare il tuo primo Pokémon.";
            if (messageIdToEdit != null) {
                modificaMessaggio(chatId, messageIdToEdit, msg, null);
            } else {
                inviaMessaggio(chatId, msg, null);
            }
            return;
        }

        if (pageIndex < 0) pageIndex = totale - 1;
        if (pageIndex >= totale) pageIndex = 0;

        Pokemon p = database.getByIndex(pageIndex);
        database.sincronizzaCartellaSpritePokemon(p);

        String testo = String.format("📚 *ARCHIVIO POKÉDEX (%d di %d)*\n\n%s",
                pageIndex + 1, totale, p.toTelegramCard());

        JSONObject tastiera = creaTastieraPaginazioneLista(pageIndex, totale, p.getId());
        if (messageIdToEdit != null) {
            modificaMessaggio(chatId, messageIdToEdit, testo, tastiera);
        } else {
            inviaMessaggio(chatId, testo, tastiera);
        }
    }

    private static void gestisciComandoModifica(long chatId, String testoCompleto) {
        String[] parts = testoCompleto.trim().split("\\s+");
        if (parts.length < 2) {
            inviaMessaggio(chatId,
                    "✏️ *Uso del comando /modifica:*\n" +
                    "Scrivi `/modifica <ID>` (es. `/modifica 1`) oppure apri `/lista` e premi il pulsante **✏️ Modifica** sotto la scheda del Pokémon!",
                    null);
            return;
        }
        try {
            int id = Integer.parseInt(parts[1].replace("#", ""));
            Pokemon p = database.getById(id);
            if (p == null) {
                inviaMessaggio(chatId, "❌ Nessun Pokémon trovato con ID `#" + id + "`. Usa /lista per vedere gli ID.", null);
                return;
            }
            inviaMessaggio(chatId,
                    "✏️ *Modifica Pokémon #" + String.format("%03d", id) + " (" + p.getNome() + ")*\n" +
                    "Seleziona quale campo vuoi modificare:",
                    creaTastieraSceltaCampoModifica(id));
        } catch (NumberFormatException e) {
            inviaMessaggio(chatId, "⚠️ L'ID deve essere un numero intero (es. `/modifica 1`).", null);
        }
    }

    private static void gestisciComandoElimina(long chatId, String testoCompleto) {
        String[] parts = testoCompleto.trim().split("\\s+");
        if (parts.length < 2) {
            inviaMessaggio(chatId,
                    "🗑️ *Uso del comando /elimina:*\n" +
                    "Scrivi `/elimina <ID>` (es. `/elimina 1`) oppure usa il pulsante **🗑️ Elimina** dentro `/lista`.",
                    null);
            return;
        }
        try {
            int id = Integer.parseInt(parts[1].replace("#", ""));
            Pokemon p = database.getById(id);
            if (p == null) {
                inviaMessaggio(chatId, "❌ Nessun Pokémon trovato con ID `#" + id + "`.", null);
                return;
            }
            inviaMessaggio(chatId,
                    "⚠️ *Confermi l'eliminazione di #" + String.format("%03d", id) + " " + p.getNome() + "?*",
                    creaTastieraConfermaEliminazione(id));
        } catch (NumberFormatException e) {
            inviaMessaggio(chatId, "⚠️ L'ID deve essere un numero intero (es. `/elimina 1`).", null);
        }
    }

    private static void processaModificaCampoEsistente(long chatId, SessioneUtente sessione, String nuovoValore) {
        Pokemon p = database.getById(sessione.getIdPokemonInModifica());
        if (p == null) {
            sessione.reset();
            inviaMessaggio(chatId, "❌ Pokémon non trovato.", null);
            return;
        }

        String campo = sessione.getCampoInModifica();
        switch (campo) {
            case "nome":
                p.setNome(nuovoValore);
                break;
            case "descrizione":
                p.setDescrizione(nuovoValore);
                break;
            case "tipo1":
                p.setTipo1(nuovoValore);
                break;
            case "tipo2":
                p.setTipo2(nuovoValore);
                break;
            case "stats":
                int[] stats = parseSeiStatistiche(nuovoValore);
                if (stats == null) {
                    inviaMessaggio(chatId, "❌ Inserisci 6 numeri tra 1 e 255 separati da spazio (es. `50 60 55 45 50 75`):", creaTastieraAnnulla());
                    return;
                }
                p.setPs(stats[0]);
                p.setAttacco(stats[1]);
                p.setDifesa(stats[2]);
                p.setAttaccoSpeciale(stats[3]);
                p.setDifesaSpeciale(stats[4]);
                p.setVelocita(stats[5]);
                break;
            default:
                break;
        }

        database.aggiungiOSostituisci(p);
        sessione.reset();

        inviaMessaggio(chatId,
                "✅ *Campo `" + campo + "` aggiornato con successo!*\n\n" + p.toTelegramCard(),
                creaTastieraPostSalvataggio(p.getId()));
    }

    // ========================================================================
    // FASE 6: TRADUZIONE AUTOMATICA IN CODICE C PER 3DS E INVIO FILE IN CHAT
    // ========================================================================
    private static CSourceGenerator.FileGenerati eseguiEsportazioneC(long chatId, boolean inviaAllegatiInChat) {
        List<Pokemon> lista = database.getTutti();
        if (lista.isEmpty()) {
            inviaMessaggio(chatId, "⚠️ Il database è vuoto! Aggiungi almeno un Pokémon con /nuovo prima di esportare.", null);
            return null;
        }

        try {
            CSourceGenerator.FileGenerati generati = CSourceGenerator.generaFileC(lista, GENERATED_C_DIR);

            // Se siamo nella repo locale con ../progetto_3ds/source/core, copiamo i file anche lì!
            Path progetto3dsCore = BASE_DIR.resolve("../progetto_3ds/source/core").normalize();
            if (Files.isDirectory(progetto3dsCore)) {
                Files.writeString(progetto3dsCore.resolve("pokedex_data.h"), generati.getHeaderContent(), StandardCharsets.UTF_8);
                Files.writeString(progetto3dsCore.resolve("pokedex_data.c"), generati.getSourceContent(), StandardCharsets.UTF_8);
            }

            if (inviaAllegatiInChat) {
                inviaMessaggio(chatId,
                        "⚙️ *Generazione Sorgenti C per devkitARM 3DS completata!*\n" +
                        "• Specie esportate: `" + lista.size() + "`\n" +
                        "• Invio allegati `pokedex_data.h` e `pokedex_data.c` in corso...",
                        null);

                inviaDocumentoInChat(chatId, generati.getHeaderPath(), "Header C per devkitARM 3DS (pokedex_data.h)");
                inviaDocumentoInChat(chatId, generati.getSourcePath(), "Sorgente C con array g_PokedexData (pokedex_data.c)");

                // Mostra pulsante per pubblicare subito su GitHub (Fase 7)
                JSONObject kb = new JSONObject();
                JSONArray rows = new JSONArray();
                rows.put(new JSONArray().put(creaBottoneInline("🚀 Pubblica ora su GitHub", "action_github")));
                kb.put("inline_keyboard", rows);
                inviaMessaggio(chatId, "Vuoi sincronizzare automaticamente questi file e tutte le cartelle sprite sulla repository GitHub `Pokemon-Project`?", kb);
            }

            return generati;
        } catch (IOException e) {
            inviaMessaggio(chatId, "❌ Errore durante la generazione dei file C: `" + e.getMessage() + "`", null);
            return null;
        }
    }

    // ========================================================================
    // FASE 7: SINCRONIZZAZIONE AUTOMATICA CON GITHUB
    // ========================================================================
    private static void eseguiSincronizzazioneGitHub(long chatId) {
        CSourceGenerator.FileGenerati fileC = eseguiEsportazioneC(chatId, false);
        if (fileC == null) {
            return;
        }

        inviaMessaggio(chatId, "🔄 *Connessione alle API REST di GitHub in corso...*\nCaricamento cartelle sprite e sorgenti C su `" + gitHubService.getRepoFullName() + "`...", null);

        String risultato = gitHubService.pubblicaSuGitHub(
                fileC,
                DB_PATH,
                SPRITES_DIR,
                database.getTutti(),
                "Aggiornato Pokédex 3DS (" + database.size() + " Pokémon) tramite Bot Telegram"
        );

        inviaMessaggio(chatId, risultato, null);
    }

    private static void gestisciImpostazioneTokenGitHub(long chatId, String testoCompleto) {
        String[] parts = testoCompleto.trim().split("\\s+", 2);
        if (parts.length < 2 || parts[1].isBlank()) {
            inviaMessaggio(chatId,
                    "🔑 *Configurazione GitHub Personal Access Token:*\n" +
                    "Invia:\n`/github_token ghp_tuoPersonalAccessToken`\n\n" +
                    "Il token verrà usato per fare commit su `" + gitHubService.getRepoFullName() + "`.",
                    null);
            return;
        }
        String nuovoToken = parts[1].trim();
        gitHubService.setGithubToken(nuovoToken);
        salvaTokenInConfig(nuovoToken);
        inviaMessaggio(chatId,
                "✅ *Token GitHub salvato con successo!*\n" +
                "Ora puoi usare `/github` o il pulsante **🚀 Pubblica su GitHub** per fare commit automatici sulla repository.",
                null);
    }

    // ========================================================================
    // COSTRUZIONE TASTIERE INLINE (FASE 3 - org.json.JSONObject / JSONArray)
    // ========================================================================
    private static JSONObject creaBottoneInline(String testo, String callbackData) {
        JSONObject btn = new JSONObject();
        btn.put("text", testo);
        btn.put("callback_data", callbackData);
        return btn;
    }

    private static JSONObject creaTastieraAnnulla() {
        JSONObject markup = new JSONObject();
        JSONArray rows = new JSONArray();
        rows.put(new JSONArray().put(creaBottoneInline("❌ Annulla operazione", "annulla")));
        markup.put("inline_keyboard", rows);
        return markup;
    }

    private static JSONObject creaTastieraTipi(String prefissoCallback, boolean includiNessuno) {
        JSONObject markup = new JSONObject();
        JSONArray rows = new JSONArray();

        if (includiNessuno) {
            rows.put(new JSONArray().put(creaBottoneInline("⭕ Nessuno (Monotipo)", prefissoCallback + ":Nessuno")));
        }

        JSONArray rigaCorrente = new JSONArray();
        for (int i = 0; i < TIPI_POKEMON.length; i++) {
            String tipo = TIPI_POKEMON[i];
            rigaCorrente.put(creaBottoneInline(tipo, prefissoCallback + ":" + tipo));
            if (rigaCorrente.length() == 3 || i == TIPI_POKEMON.length - 1) {
                rows.put(rigaCorrente);
                rigaCorrente = new JSONArray();
            }
        }

        rows.put(new JSONArray().put(creaBottoneInline("❌ Annulla", "annulla")));
        markup.put("inline_keyboard", rows);
        return markup;
    }

    private static JSONObject creaTastieraOpzioni(String prefissoCallback, String[] opzioni, int colonne) {
        JSONObject markup = new JSONObject();
        JSONArray rows = new JSONArray();

        JSONArray rigaCorrente = new JSONArray();
        for (int i = 0; i < opzioni.length; i++) {
            String opz = opzioni[i];
            rigaCorrente.put(creaBottoneInline(opz, prefissoCallback + ":" + opz));
            if (rigaCorrente.length() == colonne || i == opzioni.length - 1) {
                rows.put(rigaCorrente);
                rigaCorrente = new JSONArray();
            }
        }

        rows.put(new JSONArray().put(creaBottoneInline("❌ Annulla", "annulla")));
        markup.put("inline_keyboard", rows);
        return markup;
    }

    /**
     * Crea la tastiera inline che mostra TUTTE le cartelle Sprite (.ZIP) disponibili nel progetto.
     */
    private static JSONObject creaTastieraAttesaSprite() {
        JSONObject markup = new JSONObject();
        JSONArray rows = new JSONArray();

        List<String> cartelle = elencaCartelleSpriteDisponibili();
        if (cartelle.isEmpty()) {
            rows.put(new JSONArray().put(creaBottoneInline("📦 Usa Cartella Sprite: uccello.zip (3 file)", "sprite_folder:uccello")));
        } else {
            for (String folder : cartelle) {
                int count = 0;
                try (DirectoryStream<Path> ds = Files.newDirectoryStream(SPRITES_DIR.resolve(folder), "*.png")) {
                    for (Path ignored : ds) count++;
                } catch (IOException ignored) {
                }
                rows.put(new JSONArray().put(creaBottoneInline(
                        "📦 Usa Cartella Sprite: " + folder + ".zip (" + count + " file)",
                        "sprite_folder:" + folder)));
            }
        }

        rows.put(new JSONArray().put(creaBottoneInline("❌ Annulla", "annulla")));
        markup.put("inline_keyboard", rows);
        return markup;
    }

    private static JSONObject creaTastieraPostSalvataggio(int pokemonId) {
        JSONObject markup = new JSONObject();
        JSONArray rows = new JSONArray();
        rows.put(new JSONArray()
                .put(creaBottoneInline("⚙️ Genera Sorgenti C (.h/.c)", "action_esporta"))
                .put(creaBottoneInline("🚀 Pubblica su GitHub", "action_github")));
        rows.put(new JSONArray()
                .put(creaBottoneInline("✏️ Modifica #" + pokemonId, "mod_menu:" + pokemonId))
                .put(creaBottoneInline("📚 Vai a /lista", "lista:0")));
        markup.put("inline_keyboard", rows);
        return markup;
    }

    private static JSONObject creaTastieraPaginazioneLista(int pageIndex, int totale, int pokemonId) {
        JSONObject markup = new JSONObject();
        JSONArray rows = new JSONArray();

        if (totale > 1) {
            JSONArray navRow = new JSONArray();
            navRow.put(creaBottoneInline("⬅️ Indietro", "lista:" + (pageIndex - 1)));
            navRow.put(creaBottoneInline((pageIndex + 1) + " / " + totale, "lista:" + pageIndex));
            navRow.put(creaBottoneInline("Avanti ➡️", "lista:" + (pageIndex + 1)));
            rows.put(navRow);
        }

        JSONArray editRow = new JSONArray();
        editRow.put(creaBottoneInline("✏️ Modifica", "mod_menu:" + pokemonId));
        editRow.put(creaBottoneInline("🗑️ Elimina", "del_ask:" + pokemonId));
        rows.put(editRow);

        JSONArray exportRow = new JSONArray();
        exportRow.put(creaBottoneInline("⚙️ Genera C (.h/.c)", "action_esporta"));
        exportRow.put(creaBottoneInline("🚀 Pubblica su GitHub", "action_github"));
        rows.put(exportRow);

        markup.put("inline_keyboard", rows);
        return markup;
    }

    private static JSONObject creaTastieraSceltaCampoModifica(int id) {
        JSONObject markup = new JSONObject();
        JSONArray rows = new JSONArray();

        rows.put(new JSONArray()
                .put(creaBottoneInline("🏷 Nome", "mod_campo:" + id + ":nome"))
                .put(creaBottoneInline("📝 Descrizione", "mod_campo:" + id + ":descrizione")));
        rows.put(new JSONArray()
                .put(creaBottoneInline("🔥 Tipo 1", "mod_campo:" + id + ":tipo1"))
                .put(creaBottoneInline("💧 Tipo 2", "mod_campo:" + id + ":tipo2")));
        rows.put(new JSONArray()
                .put(creaBottoneInline("📊 Statistiche (BST)", "mod_campo:" + id + ":stats"))
                .put(creaBottoneInline("📦 Cartella Sprite (.ZIP)", "mod_campo:" + id + ":sprite")));
        rows.put(new JSONArray()
                .put(creaBottoneInline("⬅️ Torna alla Scheda", "lista:" + Math.max(0, database.getIndexById(id)))));

        markup.put("inline_keyboard", rows);
        return markup;
    }

    private static JSONObject creaTastieraConfermaEliminazione(int id) {
        JSONObject markup = new JSONObject();
        JSONArray rows = new JSONArray();
        rows.put(new JSONArray()
                .put(creaBottoneInline("🗑️ SÌ, ELIMINA", "del_confirm:" + id))
                .put(creaBottoneInline("↩️ No, Annulla", "lista:" + Math.max(0, database.getIndexById(id)))));
        markup.put("inline_keyboard", rows);
        return markup;
    }

    // ========================================================================
    // PARTE 3 DEL PDF: CHIAMATE HTTP POST/GET VERSO LE API DI TELEGRAM
    // ========================================================================

    /**
     * Invia un messaggio di testo con eventuale Tastiera Inline.
     * Usa esattamente il pattern della Slide 12 del PDF (application/x-www-form-urlencoded + BodyPublishers.ofString).
     */
    private static void inviaMessaggio(long chatId, String testo, JSONObject replyMarkup) {
        try {
            StringBuilder dati = new StringBuilder();
            dati.append("chat_id=").append(chatId);
            dati.append("&text=").append(URLEncoder.encode(testo, StandardCharsets.UTF_8));
            dati.append("&parse_mode=Markdown");

            if (replyMarkup != null) {
                dati.append("&reply_markup=").append(URLEncoder.encode(replyMarkup.toString(), StandardCharsets.UTF_8));
            }

            // Slide 12 del PDF: HttpRequest: Invio Dati (POST) con header application/x-www-form-urlencoded
            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(BASE_API_URL + "/sendMessage"))
                    .header("Content-Type", "application/x-www-form-urlencoded")
                    .POST(HttpRequest.BodyPublishers.ofString(dati.toString()))
                    .build();

            HttpResponse<String> risposta = client.send(request, HttpResponse.BodyHandlers.ofString());

            // Se il Markdown fallisce per caratteri speciali dell'utente, ritenta in testo semplice
            if (risposta.statusCode() != 200) {
                String fallbackDati = "chat_id=" + chatId + "&text=" + URLEncoder.encode(testo, StandardCharsets.UTF_8);
                if (replyMarkup != null) {
                    fallbackDati += "&reply_markup=" + URLEncoder.encode(replyMarkup.toString(), StandardCharsets.UTF_8);
                }
                HttpRequest retryReq = HttpRequest.newBuilder()
                        .uri(URI.create(BASE_API_URL + "/sendMessage"))
                        .header("Content-Type", "application/x-www-form-urlencoded")
                        .POST(HttpRequest.BodyPublishers.ofString(fallbackDati))
                        .build();
                client.send(retryReq, HttpResponse.BodyHandlers.ofString());
            }
        } catch (IOException | InterruptedException e) {
            System.err.println("[TELEGRAM] Errore invio messaggio: " + e.getMessage());
        }
    }

    /**
     * Aggiorna un messaggio esistente quando l'utente clicca un pulsante Inline (Fase 3: senza intasare la chat).
     */
    private static void modificaMessaggio(long chatId, long messageId, String nuovoTesto, JSONObject replyMarkup) {
        try {
            StringBuilder dati = new StringBuilder();
            dati.append("chat_id=").append(chatId);
            dati.append("&message_id=").append(messageId);
            dati.append("&text=").append(URLEncoder.encode(nuovoTesto, StandardCharsets.UTF_8));
            dati.append("&parse_mode=Markdown");

            if (replyMarkup != null) {
                dati.append("&reply_markup=").append(URLEncoder.encode(replyMarkup.toString(), StandardCharsets.UTF_8));
            }

            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(BASE_API_URL + "/editMessageText"))
                    .header("Content-Type", "application/x-www-form-urlencoded")
                    .POST(HttpRequest.BodyPublishers.ofString(dati.toString()))
                    .build();

            client.send(request, HttpResponse.BodyHandlers.ofString());
        } catch (IOException | InterruptedException e) {
            System.err.println("[TELEGRAM] Errore editMessageText: " + e.getMessage());
        }
    }

    /**
     * Conferma a Telegram la ricezione del clic su un pulsante inline (spegne l'icona di caricamento sul bottone).
     */
    private static void rispondiCallbackQuery(String callbackQueryId, String notificaToast) {
        try {
            String dati = "callback_query_id=" + URLEncoder.encode(callbackQueryId, StandardCharsets.UTF_8);
            if (notificaToast != null && !notificaToast.isEmpty()) {
                dati += "&text=" + URLEncoder.encode(notificaToast, StandardCharsets.UTF_8);
            }

            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(BASE_API_URL + "/answerCallbackQuery"))
                    .header("Content-Type", "application/x-www-form-urlencoded")
                    .POST(HttpRequest.BodyPublishers.ofString(dati))
                    .build();

            client.send(request, HttpResponse.BodyHandlers.ofString());
        } catch (IOException | InterruptedException e) {
            System.err.println("[TELEGRAM] Errore answerCallbackQuery: " + e.getMessage());
        }
    }

    /**
     * Fase 4: Scarica i byte di un file inviato su Telegram tramite getFile + download HTTP.
     */
    private static byte[] scaricaFileDaTelegram(String fileId) throws IOException, InterruptedException {
        String urlInfo = BASE_API_URL + "/getFile?file_id=" + URLEncoder.encode(fileId, StandardCharsets.UTF_8);

        HttpRequest reqInfo = HttpRequest.newBuilder()
                .uri(URI.create(urlInfo))
                .GET()
                .build();

        HttpResponse<String> respInfo = client.send(reqInfo, HttpResponse.BodyHandlers.ofString());
        JSONObject jsonInfo = new JSONObject(respInfo.body());

        if (!jsonInfo.optBoolean("ok", false)) {
            throw new IOException("Impossibile recuperare informazioni sul file da Telegram.");
        }

        String filePath = jsonInfo.getJSONObject("result").getString("file_path");
        String downloadUrl = FILE_API_URL + "/" + filePath;

        HttpRequest reqDownload = HttpRequest.newBuilder()
                .uri(URI.create(downloadUrl))
                .GET()
                .build();

        HttpResponse<byte[]> respBytes = client.send(reqDownload, HttpResponse.BodyHandlers.ofByteArray());
        if (respBytes.statusCode() != 200) {
            throw new IOException("Download fallito con codice HTTP " + respBytes.statusCode());
        }
        return respBytes.body();
    }

    /**
     * Invia un file locale (.zip, .png, .h o .c) direttamente nella chat di Telegram tramite sendDocument (multipart/form-data).
     */
    private static void inviaDocumentoInChat(long chatId, Path filePath, String didascalia) {
        try {
            String boundary = "----PokedexBotBoundary" + System.currentTimeMillis();
            byte[] fileBytes = Files.readAllBytes(filePath);
            String fileName = filePath.getFileName().toString();
            String lower = fileName.toLowerCase();
            String mimeType = lower.endsWith(".zip")
                    ? "application/zip"
                    : (lower.endsWith(".png") ? "image/png" : "text/plain; charset=utf-8");

            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            String nl = "\r\n";

            // Campo chat_id
            bos.write(("--" + boundary + nl).getBytes(StandardCharsets.UTF_8));
            bos.write(("Content-Disposition: form-data; name=\"chat_id\"" + nl + nl).getBytes(StandardCharsets.UTF_8));
            bos.write((chatId + nl).getBytes(StandardCharsets.UTF_8));

            // Campo caption
            if (didascalia != null && !didascalia.isEmpty()) {
                bos.write(("--" + boundary + nl).getBytes(StandardCharsets.UTF_8));
                bos.write(("Content-Disposition: form-data; name=\"caption\"" + nl + nl).getBytes(StandardCharsets.UTF_8));
                bos.write((didascalia + nl).getBytes(StandardCharsets.UTF_8));
            }

            // Campo document (file binario)
            bos.write(("--" + boundary + nl).getBytes(StandardCharsets.UTF_8));
            bos.write(("Content-Disposition: form-data; name=\"document\"; filename=\"" + fileName + "\"" + nl).getBytes(StandardCharsets.UTF_8));
            bos.write(("Content-Type: " + mimeType + nl + nl).getBytes(StandardCharsets.UTF_8));
            bos.write(fileBytes);
            bos.write(nl.getBytes(StandardCharsets.UTF_8));

            // Chiusura boundary
            bos.write(("--" + boundary + "--" + nl).getBytes(StandardCharsets.UTF_8));

            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(BASE_API_URL + "/sendDocument"))
                    .header("Content-Type", "multipart/form-data; boundary=" + boundary)
                    .POST(HttpRequest.BodyPublishers.ofByteArray(bos.toByteArray()))
                    .build();

            client.send(request, HttpResponse.BodyHandlers.ofString());
        } catch (IOException | InterruptedException e) {
            System.err.println("[TELEGRAM] Errore invio documento " + filePath + ": " + e.getMessage());
        }
    }

    /**
     * Fase 1: Registra i comandi ufficiali nel menu di Telegram (visibili premendo "/" in chat).
     */
    private static void impostaComandiMenuTelegram() {
        try {
            JSONArray commands = new JSONArray();
            commands.put(new JSONObject().put("command", "nuovo").put("description", "Crea un nuovo Pokémon (procedura guidata)"));
            commands.put(new JSONObject().put("command", "lista").put("description", "Sfoglia il Pokédex e ricevi lo ZIP della cartella Sprite"));
            commands.put(new JSONObject().put("command", "sprites").put("description", "Mostra e scarica i file ZIP di tutte le cartelle Sprite"));
            commands.put(new JSONObject().put("command", "esporta").put("description", "Genera e scarica i file C per 3DS (.h e .c)"));
            commands.put(new JSONObject().put("command", "github").put("description", "Pubblica sprite e sorgenti C su GitHub"));
            commands.put(new JSONObject().put("command", "modifica").put("description", "Modifica un Pokémon per ID (es. /modifica 1)"));
            commands.put(new JSONObject().put("command", "elimina").put("description", "Elimina un Pokémon per ID (es. /elimina 1)"));
            commands.put(new JSONObject().put("command", "annulla").put("description", "Annulla l'inserimento in corso"));

            JSONObject payload = new JSONObject();
            payload.put("commands", commands);

            HttpRequest request = HttpRequest.newBuilder()
                    .uri(URI.create(BASE_API_URL + "/setMyCommands"))
                    .header("Content-Type", "application/json")
                    .POST(HttpRequest.BodyPublishers.ofString(payload.toString(), StandardCharsets.UTF_8))
                    .build();

            client.send(request, HttpResponse.BodyHandlers.ofString());
        } catch (Exception e) {
            System.err.println("[TELEGRAM] Avviso setMyCommands: " + e.getMessage());
        }
    }

    private static void inviaMessaggioBenvenuto(long chatId) {
        String msg = "🎮 *POKÉDEX 3DS PORTABLE CMS*\n" +
                     "━━━━━━━━━━━━━━━━━━━━\n" +
                     "Benvenuto nel pannello di gestione del Pokédex per **Nintendo 3DS (devkitARM)**!\n\n" +
                     "📌 *Comandi disponibili:*\n" +
                     "• /nuovo — Avvia la creazione guidata di un Pokémon (con upload `.ZIP` della cartella Sprite)\n" +
                     "• /lista — Sfoglia il Pokédex e mostra/scarica lo **ZIP dell'intera cartella Sprite**\n" +
                     "• /sprites — Mostra e invia i file `.ZIP` di tutte le cartelle Sprite presenti\n" +
                     "• /modifica `<id>` — Modifica un dato o la cartella Sprite (`.ZIP`) di un Pokémon\n" +
                     "• /elimina `<id>` — Rimuove un Pokémon dall'archivio\n" +
                     "• /esporta — Genera `pokedex_data.h` e `pokedex_data.c` e li invia in chat\n" +
                     "• /github — Esegue il commit automatico di cartelle sprite e file C su GitHub\n" +
                     "• /github\\_token `<PAT>` — Imposta il Personal Access Token di GitHub\n" +
                     "• /annulla — Interrompe l'operazione corrente";
        inviaMessaggio(chatId, msg, null);
    }

    // ========================================================================
    // UTILITÀ DI PARSING E CONFIGURAZIONE
    // ========================================================================
    private static int[] parseSeiStatistiche(String testo) {
        String[] tokens = testo.trim().split("[\\s,;]+");
        if (tokens.length != 6) {
            return null;
        }
        int[] stats = new int[6];
        for (int i = 0; i < 6; i++) {
            try {
                int v = Integer.parseInt(tokens[i]);
                if (v < 1 || v > 255) {
                    return null;
                }
                stats[i] = v;
            } catch (NumberFormatException e) {
                return null;
            }
        }
        return stats;
    }

    private static void caricaConfigurazione() {
        Properties props = new Properties();
        if (Files.exists(CONFIG_PATH)) {
            try (InputStream in = Files.newInputStream(CONFIG_PATH)) {
                props.load(in);
            } catch (IOException e) {
                System.err.println("[CONFIG] Errore lettura config.properties: " + e.getMessage());
            }
        }

        String envBotToken = System.getenv("TELEGRAM_BOT_TOKEN");
        String propBotToken = props.getProperty("telegram.bot.token", BOT_TOKEN).trim();
        if (envBotToken != null && !envBotToken.isBlank()) {
            BOT_TOKEN = envBotToken.trim();
        } else if (!propBotToken.isEmpty()) {
            BOT_TOKEN = propBotToken;
        }
        BASE_API_URL = "https://api.telegram.org/bot" + BOT_TOKEN;
        FILE_API_URL = "https://api.telegram.org/file/bot" + BOT_TOKEN;

        String githubToken = System.getenv("GITHUB_TOKEN");
        if (githubToken == null || githubToken.isBlank()) {
            githubToken = props.getProperty("github.token", "").trim();
        }
        String repoOwner = props.getProperty("github.owner", "BmattiaDEV").trim();
        String repoName = props.getProperty("github.repo", "Pokemon-Project").trim();
        String branch = props.getProperty("github.branch", "main").trim();

        gitHubService = new GitHubSyncService(client, githubToken, repoOwner, repoName, branch);
    }

    private static void salvaTokenInConfig(String githubToken) {
        try {
            String content = "# Configurazione Pokédex 3DS Telegram CMS\n" +
                             "telegram.bot.token=" + BOT_TOKEN + "\n" +
                             "github.owner=BmattiaDEV\n" +
                             "github.repo=Pokemon-Project\n" +
                             "github.branch=main\n" +
                             "github.token=" + githubToken + "\n";
            Files.writeString(CONFIG_PATH, content, StandardCharsets.UTF_8);
        } catch (IOException e) {
            System.err.println("[CONFIG] Impossibile salvare config.properties: " + e.getMessage());
        }
    }
}
