package pokedexbot;

import javax.imageio.ImageIO;
import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/**
 * FASE 4: Ricezione, estrazione e validazione tecnica dell'intera cartella Sprite (file .ZIP o .PNG)
 * per l'engine Nintendo 3DS.
 *
 * Supporta:
 * 1. Invio di un intero archivio .ZIP contenente la cartella degli sprite del Pokémon
 *    (es. uccello_front.png, uccello_back.png, uccello_sprite.png)
 * 2. Creazione automatica del file .ZIP a partire da una cartella di sprite (per mostrarlo/inviarlo tutto su Telegram)
 * 3. Verifica tecnica di ogni PNG (formato PNG, dimensioni potenze di 2 da 32 a 2048 px, canale Alpha RGBA).
 */
public class SpriteValidator {

    // Magic bytes dell'header PNG: 89 50 4E 47 0D 0A 1A 0A
    private static final byte[] PNG_SIGNATURE = new byte[]{
            (byte) 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
    };

    // Magic bytes dell'header ZIP: 50 4B 03 04 ("PK\003\004")
    private static final byte[] ZIP_SIGNATURE = new byte[]{
            0x50, 0x4B, 0x03, 0x04
    };

    public static class EsitoValidazione {
        private final boolean valido;
        private final String messaggio;
        private final int larghezza;
        private final int altezza;
        private final boolean haTrasparenza;

        public EsitoValidazione(boolean valido, String messaggio, int larghezza, int altezza, boolean haTrasparenza) {
            this.valido = valido;
            this.messaggio = messaggio;
            this.larghezza = larghezza;
            this.altezza = altezza;
            this.haTrasparenza = haTrasparenza;
        }

        public boolean isValido() { return valido; }
        public String getMessaggio() { return messaggio; }
        public int getLarghezza() { return larghezza; }
        public int getAltezza() { return altezza; }
        public boolean isHaTrasparenza() { return haTrasparenza; }
    }

    public static class EsitoCartellaZip {
        private final boolean valido;
        private final String messaggio;
        private final String folderName;
        private final String zipFileName;
        private final Path zipPath;
        private final List<String> descrizioniFile;
        private final List<Path> fileEstratti;
        private final String frontFile;
        private final String backFile;
        private final String iconFile;
        private final int mainWidth;
        private final int mainHeight;

        public EsitoCartellaZip(
                boolean valido,
                String messaggio,
                String folderName,
                String zipFileName,
                Path zipPath,
                List<String> descrizioniFile,
                List<Path> fileEstratti,
                String frontFile,
                String backFile,
                String iconFile,
                int mainWidth,
                int mainHeight) {
            this.valido = valido;
            this.messaggio = messaggio;
            this.folderName = folderName;
            this.zipFileName = zipFileName;
            this.zipPath = zipPath;
            this.descrizioniFile = descrizioniFile;
            this.fileEstratti = fileEstratti;
            this.frontFile = frontFile;
            this.backFile = backFile;
            this.iconFile = iconFile;
            this.mainWidth = mainWidth;
            this.mainHeight = mainHeight;
        }

        public boolean isValido() { return valido; }
        public String getMessaggio() { return messaggio; }
        public String getFolderName() { return folderName; }
        public String getZipFileName() { return zipFileName; }
        public Path getZipPath() { return zipPath; }
        public List<String> getDescrizioniFile() { return descrizioniFile; }
        public List<Path> getFileEstratti() { return fileEstratti; }
        public String getFrontFile() { return frontFile; }
        public String getBackFile() { return backFile; }
        public String getIconFile() { return iconFile; }
        public int getMainWidth() { return mainWidth; }
        public int getMainHeight() { return mainHeight; }
    }

    public static boolean isZipFile(byte[] fileBytes, String fileName) {
        if (fileName != null && fileName.toLowerCase().endsWith(".zip")) {
            return true;
        }
        if (fileBytes != null && fileBytes.length >= 4) {
            for (int i = 0; i < 4; i++) {
                if (fileBytes[i] != ZIP_SIGNATURE[i]) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    /**
     * Riceve un file .ZIP (o un .PNG) inviato su Telegram, estrae tutti gli sprite della cartella
     * in sprites/<folderName>/, valida ciascun PNG per il 3DS e genera l'archivio completo
     * sprites/<folderName>.zip pronto da mostrare e scaricare.
     */
    public static EsitoCartellaZip elaboraZipOSprite3DS(
            byte[] fileBytes,
            String originalFileName,
            String targetFolderName,
            Path baseSpritesDir) throws IOException {

        Files.createDirectories(baseSpritesDir);
        Path targetFolder = baseSpritesDir.resolve(targetFolderName);
        Files.createDirectories(targetFolder);

        List<String> descrizioni = new ArrayList<>();
        List<Path> estratti = new ArrayList<>();
        String frontFile = "";
        String backFile = "";
        String iconFile = "";
        int mainW = 512;
        int mainH = 1024;

        if (isZipFile(fileBytes, originalFileName)) {
            // Estrazione e validazione di tutti i PNG contenuti nella cartella dello ZIP
            try (ZipInputStream zis = new ZipInputStream(new ByteArrayInputStream(fileBytes))) {
                ZipEntry entry;
                while ((entry = zis.getNextEntry()) != null) {
                    if (entry.isDirectory()) {
                        continue;
                    }
                    String entryName = Path.of(entry.getName()).getFileName().toString();
                    if (entryName.startsWith(".") || entryName.startsWith("__MACOSX") || !entryName.toLowerCase().endsWith(".png")) {
                        continue;
                    }

                    byte[] pngBytes = readAllBytesFromStream(zis);
                    EsitoValidazione val = validaSprite3DS(pngBytes, entryName);
                    if (!val.isValido()) {
                        return new EsitoCartellaZip(false,
                                "❌ *Errore nel file `" + entryName + "` dentro lo ZIP:*\n" + val.getMessaggio(),
                                targetFolderName, "", null, Collections.emptyList(), Collections.emptyList(),
                                "", "", "", 0, 0);
                    }

                    Path savedPng = targetFolder.resolve(entryName);
                    Files.write(savedPng, pngBytes);
                    estratti.add(savedPng);
                }
            }

            if (estratti.isEmpty()) {
                return new EsitoCartellaZip(false,
                        "❌ *Archivio ZIP vuoto o privo di file PNG:* L'archivio `" + originalFileName + "` non contiene nessuno sprite `.png` valido.",
                        targetFolderName, "", null, Collections.emptyList(), Collections.emptyList(),
                        "", "", "", 0, 0);
            }
        } else {
            // L'utente ha inviato un singolo file PNG: lo validiamo e lo aggiungiamo alla cartella del Pokémon
            EsitoValidazione val = validaSprite3DS(fileBytes, originalFileName);
            if (!val.isValido()) {
                return new EsitoCartellaZip(false, val.getMessaggio(),
                        targetFolderName, "", null, Collections.emptyList(), Collections.emptyList(),
                        "", "", "", 0, 0);
            }
            String cleanName = (originalFileName != null && originalFileName.toLowerCase().endsWith(".png"))
                    ? Path.of(originalFileName).getFileName().toString()
                    : targetFolderName + "_front.png";
            Path savedPng = targetFolder.resolve(cleanName);
            Files.write(savedPng, fileBytes);
        }

        // Scansiona TUTTA la cartella sprites/<targetFolderName>/ e rigenera il file .zip completo
        return impacchettaCartellaSprite(targetFolderName, baseSpritesDir);
    }

    /**
     * Scansiona l'intera cartella sprites/<folderName>/, raccoglie i metadati di tutti i PNG
     * (front, back, sprite/icona) e crea il file ZIP completo sprites/<folderName>.zip.
     */
    public static EsitoCartellaZip impacchettaCartellaSprite(String folderName, Path baseSpritesDir) throws IOException {
        Files.createDirectories(baseSpritesDir);
        Path folderPath = baseSpritesDir.resolve(folderName);

        // Se la cartella locale non esiste ancora, controlliamo se esiste in "../data useful pngs for story/pokemon/sprites/<folderName>"
        if (!Files.isDirectory(folderPath) || isDirEmpty(folderPath)) {
            Path storySpritesFolder = baseSpritesDir.toAbsolutePath().normalize()
                    .resolve("../../data useful pngs for story/pokemon/sprites/" + folderName).normalize();
            if (Files.isDirectory(storySpritesFolder)) {
                Files.createDirectories(folderPath);
                try (DirectoryStream<Path> ds = Files.newDirectoryStream(storySpritesFolder, "*.png")) {
                    for (Path src : ds) {
                        Files.copy(src, folderPath.resolve(src.getFileName()), StandardCopyOption.REPLACE_EXISTING);
                    }
                }
            }
        }

        if (!Files.isDirectory(folderPath)) {
            Files.createDirectories(folderPath);
        }

        List<Path> pngFiles = new ArrayList<>();
        try (DirectoryStream<Path> ds = Files.newDirectoryStream(folderPath, "*.png")) {
            for (Path p : ds) {
                pngFiles.add(p);
            }
        }
        pngFiles.sort((a, b) -> a.getFileName().toString().compareToIgnoreCase(b.getFileName().toString()));

        List<String> descrizioni = new ArrayList<>();
        String frontFile = "";
        String backFile = "";
        String iconFile = "";
        int mainW = 512;
        int mainH = 1024;

        String zipFileName = folderName + ".zip";
        Path zipPath = baseSpritesDir.resolve(zipFileName);

        try (ZipOutputStream zos = new ZipOutputStream(Files.newOutputStream(zipPath))) {
            for (Path pngPath : pngFiles) {
                String fname = pngPath.getFileName().toString();
                byte[] bytes = Files.readAllBytes(pngPath);
                BufferedImage img = ImageIO.read(new ByteArrayInputStream(bytes));
                int w = (img != null) ? img.getWidth() : 0;
                int h = (img != null) ? img.getHeight() : 0;

                String ruolo = "Sprite";
                String lower = fname.toLowerCase();
                if (lower.contains("front")) {
                    frontFile = fname;
                    mainW = w;
                    mainH = h;
                    ruolo = "Front Battle Sheet";
                } else if (lower.contains("back")) {
                    backFile = fname;
                    ruolo = "Back Battle Sheet";
                } else if (lower.contains("sprite") || lower.contains("icon")) {
                    iconFile = fname;
                    ruolo = "Base/Icon Sprite";
                }

                descrizioni.add(String.format("%s (%dx%d px - %s)", fname, w, h, ruolo));

                // Aggiunge il file dentro lo ZIP mantenendo la cartella <folderName>/<fname>
                ZipEntry entry = new ZipEntry(folderName + "/" + fname);
                zos.putNextEntry(entry);
                zos.write(bytes);
                zos.closeEntry();
            }
        }

        if (frontFile.isEmpty() && !pngFiles.isEmpty()) {
            frontFile = pngFiles.get(0).getFileName().toString();
        }
        if (backFile.isEmpty() && pngFiles.size() > 1) {
            backFile = pngFiles.get(1).getFileName().toString();
        }
        if (iconFile.isEmpty() && pngFiles.size() > 2) {
            iconFile = pngFiles.get(2).getFileName().toString();
        }

        StringBuilder msg = new StringBuilder();
        msg.append("✅ *Intera cartella Sprite e archivio ZIP validati!*\n");
        msg.append("• 📦 Archivio ZIP: `").append(zipFileName).append("`\n");
        msg.append("• 📁 Cartella: `sprites/").append(folderName).append("/` (").append(pngFiles.size()).append(" file)\n");
        for (String d : descrizioni) {
            msg.append("  • `").append(d).append("`\n");
        }

        return new EsitoCartellaZip(
                !pngFiles.isEmpty(),
                msg.toString().stripTrailing(),
                folderName,
                zipFileName,
                zipPath,
                descrizioni,
                pngFiles,
                frontFile,
                backFile,
                iconFile,
                mainW,
                mainH
        );
    }

    private static boolean isDirEmpty(Path dir) throws IOException {
        try (DirectoryStream<Path> ds = Files.newDirectoryStream(dir)) {
            return !ds.iterator().hasNext();
        }
    }

    private static byte[] readAllBytesFromStream(InputStream in) throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        int r;
        while ((r = in.read(buf)) != -1) {
            bos.write(buf, 0, r);
        }
        return bos.toByteArray();
    }

    /**
     * Valida i byte di una singola immagine PNG secondo i vincoli del 3DS.
     */
    public static EsitoValidazione validaSprite3DS(byte[] fileBytes, String fileName) {
        if (fileBytes == null || fileBytes.length < 8) {
            return new EsitoValidazione(false,
                    "❌ Il file ricevuto è vuoto o corrotto.", 0, 0, false);
        }

        // 1. Controllo firma binaria PNG
        for (int i = 0; i < PNG_SIGNATURE.length; i++) {
            if (fileBytes[i] != PNG_SIGNATURE[i]) {
                return new EsitoValidazione(false,
                        "❌ *Formato non valido:* Il file `" + fileName + "` non è un PNG o un archivio ZIP valido.\n" +
                        "Invia un archivio **.ZIP** con l'intera cartella sprite oppure un file **.PNG**.",
                        0, 0, false);
            }
        }

        // 2. Decodifica dell'immagine per ispezionare risoluzione e ColorModel
        BufferedImage img;
        try {
            img = ImageIO.read(new ByteArrayInputStream(fileBytes));
        } catch (IOException e) {
            return new EsitoValidazione(false,
                    "❌ *Errore lettura PNG:* Impossibile decodificare l'immagine (" + e.getMessage() + ").",
                    0, 0, false);
        }

        if (img == null) {
            return new EsitoValidazione(false,
                    "❌ *Immagine non leggibile:* Il file non contiene dati grafici validi.",
                    0, 0, false);
        }

        int w = img.getWidth();
        int h = img.getHeight();
        boolean hasAlpha = img.getColorModel().hasAlpha();

        // 3. Controllo dimensioni potenze di 2 (32..2048) per sprite singoli (32x32, 64x64, 128x128, 256x256)
        // e spritesheet battaglia 3DS (512x1024 o 1024x2048 griglia 4x8)
        boolean dimensioniValide = isPotenzaDiDueValida3DS(w) && isPotenzaDiDueValida3DS(h)
                && (w == h || (w == 512 && h == 1024) || (w == 1024 && h == 2048) || (w == 32 && h == 512) || (w == 128 && h == 512));

        if (!dimensioniValide) {
            return new EsitoValidazione(false,
                    String.format(
                            "❌ *Dimensioni non compatibili con 3DS in `%s`:* Rilevato `%dx%d` pixel.\n\n" +
                            "Dimensioni consentite (potenze di 2):\n" +
                            "• Sprite / Icona: `32x32`, `64x64`, `128x128` o `256x256` px\n" +
                            "• Spritesheet battaglia (32 frame 4x8): `512x1024` o `1024x2048` px",
                            fileName, w, h),
                    w, h, hasAlpha);
        }

        // 4. Controllo presenza del canale Alpha (trasparenza RGBA)
        if (!hasAlpha) {
            return new EsitoValidazione(false,
                    String.format(
                            "❌ *Trasparenza mancante (Canale Alpha assente) in `%s`:* Lo sprite `%dx%d` è RGB opaco.\n" +
                            "Esporta il PNG in modalità **RGBA (32-bit con trasparenza)**!",
                            fileName, w, h),
                    w, h, false);
        }

        String tipoTexture = (w == 512 && h == 1024) || (w == 1024 && h == 2048)
                ? "Spritesheet Animato 3DS (32 frame 4x8)"
                : "Sprite Singolo 3DS";

        return new EsitoValidazione(true,
                String.format("✅ *Sprite `%s` validato!* (`%dx%d` px - %s, RGBA)", fileName, w, h, tipoTexture),
                w, h, true);
    }

    /**
     * Verifica che la dimensione sia una potenza di 2 supportata (da 32 a 2048).
     */
    private static boolean isPotenzaDiDueValida3DS(int n) {
        return n >= 32 && n <= 2048 && (n & (n - 1)) == 0;
    }
}
