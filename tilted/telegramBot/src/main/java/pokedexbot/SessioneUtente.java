package pokedexbot;

/**
 * FASE 2: Macchina a Stati (State Machine) per il flusso guidato della chat.
 * Memorizza la "fase" corrente in cui si trova l'utente (legata al chatId)
 * e conserva i dati temporanei del Pokémon che si sta creando o modificando.
 */
public class SessioneUtente {

    public enum Fase {
        IDLE,                   // Nessuna operazione in corso
        ATTESA_NOME,            // Fase 1a: Inserimento nome a testo libero
        ATTESA_DESCRIZIONE,     // Fase 1b: Inserimento descrizione Pokédex a testo libero
        SCELTA_TIPO_1,          // Fase 2a: Scelta Tipo primario tramite Tastiera Inline
        SCELTA_TIPO_2,          // Fase 2b: Scelta Tipo secondario tramite Tastiera Inline
        SCELTA_CURVA_CRESCITA,  // Fase 2c: Scelta Curva di crescita tramite Tastiera Inline
        SCELTA_GRUPPO_UOVA,     // Fase 2d: Scelta Gruppo uova tramite Tastiera Inline
        ATTESA_STATISTICHE,     // Fase 3: Inserimento statistiche base (PS, ATT, DIF, ASP, DSP, VEL)
        ATTESA_SPRITE,          // Fase 4: Invio e validazione tecnica dello Sprite PNG per 3DS
        MODIFICA_TESTO,         // Stato speciale per modificare un singolo campo testuale/statistica
        MODIFICA_SPRITE         // Stato speciale per sostituire lo sprite di un Pokémon esistente
    }

    private Fase faseCorrente;
    private Pokemon pokemonInCostruzione;
    private int idPokemonInModifica;
    private String campoInModifica;

    public SessioneUtente() {
        reset();
    }

    /**
     * Riporta la macchina a stati in IDLE e pulisce la memoria temporanea.
     */
    public void reset() {
        this.faseCorrente = Fase.IDLE;
        this.pokemonInCostruzione = null;
        this.idPokemonInModifica = -1;
        this.campoInModifica = null;
    }

    /**
     * Avvia il flusso di creazione di un nuovo Pokémon.
     */
    public void iniziaNuovoPokemon(int prossimoId) {
        this.pokemonInCostruzione = new Pokemon();
        this.pokemonInCostruzione.setId(prossimoId);
        this.faseCorrente = Fase.ATTESA_NOME;
        this.idPokemonInModifica = -1;
        this.campoInModifica = null;
    }

    public Fase getFaseCorrente() {
        return faseCorrente;
    }

    public void setFaseCorrente(Fase faseCorrente) {
        this.faseCorrente = faseCorrente;
    }

    public Pokemon getPokemonInCostruzione() {
        return pokemonInCostruzione;
    }

    public void setPokemonInCostruzione(Pokemon pokemonInCostruzione) {
        this.pokemonInCostruzione = pokemonInCostruzione;
    }

    public int getIdPokemonInModifica() {
        return idPokemonInModifica;
    }

    public void setIdPokemonInModifica(int idPokemonInModifica) {
        this.idPokemonInModifica = idPokemonInModifica;
    }

    public String getCampoInModifica() {
        return campoInModifica;
    }

    public void setCampoInModifica(String campoInModifica) {
        this.campoInModifica = campoInModifica;
    }
}
