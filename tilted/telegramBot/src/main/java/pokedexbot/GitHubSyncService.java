package pokedexbot;

import org.json.JSONObject;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Base64;
import java.util.List;

/**
 * FASE 7: Sincronizzazione automatica con GitHub tramite API REST.
 * Utilizza l'architettura a 3 Pilastri di java.net.http (HttpClient, HttpRequest, HttpResponse - Parte 3 del PDF)
 * e org.json.JSONObject (Parte 2 del PDF) per:
 * 1. Interrogare la repository Pokemon-Project e leggere lo SHA attuale dei file (.h, .c e intera cartella sprite)
 * 2. Caricare TUTTI gli sprite della cartella di ogni Pokémon (front, back, sprite) e lo ZIP nella repo
 * 3. Sovrascrivere i file pokedex_data.h, pokedex_data.c e pokedex_db.json
 * 4. Restituire il link diretto al commit effettuato su GitHub.
 */
public class GitHubSyncService {

    private final HttpClient client;
    private String githubToken;
    private final String repoOwner;
    private final String repoName;
    private final String branch;

    public GitHubSyncService(HttpClient client, String githubToken, String repoOwner, String repoName, String branch) {
        this.client = client;
        this.githubToken = githubToken != null ? githubToken.trim() : "";
        this.repoOwner = repoOwner;
        this.repoName = repoName;
        this.branch = branch;
    }

    public boolean isTokenConfigurato() {
        return githubToken != null && !githubToken.isEmpty() && !githubToken.startsWith("INSERISCI_");
    }

    public void setGithubToken(String githubToken) {
        this.githubToken = githubToken != null ? githubToken.trim() : "";
    }

    public String getRepoFullName() {
        return repoOwner + "/" + repoName;
    }

    /**
     * Sincronizza le intere cartelle sprite (e i file .zip), i file C generati (.h e .c)
     * e il database JSON sulla repository GitHub.
     */
    public String pubblicaSuGitHub(
            CSourceGenerator.FileGenerati fileC,
            Path dbJsonPath,
            Path spritesDir,
            List<Pokemon> listaPokemon,
            String messaggioCommit) {

        if (!isTokenConfigurato()) {
            return "⚠️ *Token GitHub non configurato!*\n\n" +
                   "Per pubblicare automaticamente su `" + getRepoFullName() + "`:\n" +
                   "1. Genera un Personal Access Token (PAT) su GitHub con permesso `Contents: Read & Write` sulla repo `" + repoName + "`.\n" +
                   "2. Invia qui in chat il comando:\n" +
                   "`/github_token ghp_tuoTokenQui`\n" +
                   "(oppure inseriscilo nel file `config.properties`).";
        }

        String ultimoCommitUrl = "";
        int fileAggiornati = 0;
        StringBuilder logErrori = new StringBuilder();

        try {
            // 1. Carica l'intera cartella sprite (tutti i PNG: front, back, sprite) per ciascun Pokémon
            for (Pokemon p : listaPokemon) {
                String folderName = p.getSpriteFolderName();
                if (folderName != null && !folderName.isEmpty()) {
                    Path localFolder = spritesDir.resolve(folderName);
                    if (Files.isDirectory(localFolder)) {
                        try (DirectoryStream<Path> ds = Files.newDirectoryStream(localFolder, "*.png")) {
                            for (Path pngFile : ds) {
                                byte[] spriteBytes = Files.readAllBytes(pngFile);
                                String fileName = pngFile.getFileName().toString();
                                String remoteSpritePath = "tilted/progetto_3ds/data/" + fileName;
                                String url = caricaOAggiornaFile(remoteSpritePath, spriteBytes,
                                        "Aggiornato sprite 3DS " + folderName + "/" + fileName + " per " + p.getNome() + " via Telegram Bot");
                                if (url != null && !url.startsWith("ERRORE:")) {
                                    ultimoCommitUrl = url;
                                    fileAggiornati++;
                                } else if (url != null) {
                                    logErrori.append("• Sprite `").append(fileName).append("`: ").append(url).append("\n");
                                }
                            }
                        }
                    }
                }
            }

            // 2. Aggiorna tilted/progetto_3ds/source/core/pokedex_data.h
            byte[] headerBytes = fileC.getHeaderContent().getBytes(StandardCharsets.UTF_8);
            String urlH = caricaOAggiornaFile(
                    "tilted/progetto_3ds/source/core/pokedex_data.h",
                    headerBytes,
                    messaggioCommit + " (pokedex_data.h)");
            if (urlH != null && !urlH.startsWith("ERRORE:")) {
                ultimoCommitUrl = urlH;
                fileAggiornati++;
            } else if (urlH != null) {
                logErrori.append("• `pokedex_data.h`: ").append(urlH).append("\n");
            }

            // 3. Aggiorna tilted/progetto_3ds/source/core/pokedex_data.c
            byte[] sourceBytes = fileC.getSourceContent().getBytes(StandardCharsets.UTF_8);
            String urlC = caricaOAggiornaFile(
                    "tilted/progetto_3ds/source/core/pokedex_data.c",
                    sourceBytes,
                    messaggioCommit + " (pokedex_data.c)");
            if (urlC != null && !urlC.startsWith("ERRORE:")) {
                ultimoCommitUrl = urlC;
                fileAggiornati++;
            } else if (urlC != null) {
                logErrori.append("• `pokedex_data.c`: ").append(urlC).append("\n");
            }

            // 4. Aggiorna il database JSON centrale tilted/telegramBot/pokedex_db.json
            if (Files.exists(dbJsonPath)) {
                byte[] dbBytes = Files.readAllBytes(dbJsonPath);
                String urlDb = caricaOAggiornaFile(
                        "tilted/telegramBot/pokedex_db.json",
                        dbBytes,
                        messaggioCommit + " (pokedex_db.json)");
                if (urlDb != null && !urlDb.startsWith("ERRORE:")) {
                    ultimoCommitUrl = urlDb;
                    fileAggiornati++;
                }
            }

        } catch (Exception e) {
            return "❌ *Errore imprevisto durante la sincronizzazione GitHub:*\n`" + e.getMessage() + "`";
        }

        if (fileAggiornati == 0) {
            return "❌ *Sincronizzazione GitHub fallita.*\n" + logErrori;
        }

        StringBuilder report = new StringBuilder();
        report.append("✅ *Sincronizzazione con GitHub completata!*\n");
        report.append("━━━━━━━━━━━━━━━━━━━━\n");
        report.append("📦 *Repository:* `").append(getRepoFullName()).append("` (branch `").append(branch).append("`)\n");
        report.append("📄 *File aggiornati:* `").append(fileAggiornati).append("`\n");
        if (!ultimoCommitUrl.isEmpty()) {
            report.append("🔗 *Link diretto al Commit:*\n").append(ultimoCommitUrl).append("\n");
        }
        if (logErrori.length() > 0) {
            report.append("\n⚠️ *Avvisi:*\n").append(logErrori);
        }
        return report.toString();
    }

    /**
     * Interroga GitHub (GET) per verificare se il file esiste già (recuperando il suo "sha")
     * e poi esegue l'upload/sovrascrittura tramite richiesta PUT alle API REST di GitHub.
     */
    private String caricaOAggiornaFile(String repoPath, byte[] fileBytes, String commitMessage) {
        String apiUrl = String.format("https://api.github.com/repos/%s/%s/contents/%s",
                repoOwner, repoName, repoPath);

        try {
            // PASSO A: Richiesta GET per recuperare lo SHA attuale del file se esiste (Slide 11 del PDF)
            String shaAttuale = recuperaShaFileEsistente(apiUrl);

            // PASSO B: Costruzione payload JSON con org.json.JSONObject (Slide 6 del PDF)
            JSONObject payload = new JSONObject();
            payload.put("message", commitMessage);
            payload.put("content", Base64.getEncoder().encodeToString(fileBytes));
            payload.put("branch", branch);
            if (shaAttuale != null && !shaAttuale.isEmpty()) {
                payload.put("sha", shaAttuale);
            }

            // PASSO C: Richiesta HTTP PUT con BodyPublishers.ofString (Slide 12 del PDF)
            HttpRequest putRequest = HttpRequest.newBuilder()
                    .uri(URI.create(apiUrl))
                    .header("Authorization", "Bearer " + githubToken)
                    .header("Accept", "application/vnd.github+json")
                    .header("Content-Type", "application/json")
                    .header("User-Agent", "Pokedex-3DS-Telegram-CMS")
                    .PUT(HttpRequest.BodyPublishers.ofString(payload.toString(), StandardCharsets.UTF_8))
                    .build();

            HttpResponse<String> risposta = client.send(putRequest, HttpResponse.BodyHandlers.ofString());
            int stato = risposta.statusCode(); // Slide 13 del PDF

            if (stato == 200 || stato == 201) {
                JSONObject jsonRisposta = new JSONObject(risposta.body());
                JSONObject commitObj = jsonRisposta.optJSONObject("commit");
                if (commitObj != null) {
                    return commitObj.optString("html_url", "https://github.com/" + getRepoFullName() + "/commits/" + branch);
                }
                return "https://github.com/" + getRepoFullName();
            } else {
                JSONObject errJson = new JSONObject(risposta.body());
                String msg = errJson.optString("message", "HTTP " + stato);
                return "ERRORE: " + msg + " (HTTP " + stato + ")";
            }
        } catch (IOException | InterruptedException e) {
            return "ERRORE: " + e.getMessage();
        }
    }

    /**
     * Esegue una chiamata GET alle API GitHub per ottenere lo "sha" del file se già presente nella repo.
     */
    private String recuperaShaFileEsistente(String apiUrl) throws IOException, InterruptedException {
        HttpRequest getRequest = HttpRequest.newBuilder()
                .uri(URI.create(apiUrl + "?ref=" + branch))
                .header("Authorization", "Bearer " + githubToken)
                .header("Accept", "application/vnd.github+json")
                .header("User-Agent", "Pokedex-3DS-Telegram-CMS")
                .GET()
                .build();

        HttpResponse<String> risposta = client.send(getRequest, HttpResponse.BodyHandlers.ofString());
        if (risposta.statusCode() == 200) {
            JSONObject json = new JSONObject(risposta.body());
            // Metodo safe optString() (Slide 8 del PDF)
            return json.optString("sha", null);
        }
        return null;
    }
}
