# autocam — Analisi di pubblicabilità del carving, riferimenti e taglio del paper

**Destinatari:** Nicola + il collega che scriverà il paper.
**Oggetto:** l'algoritmo di material removal ("carving") descritto in `DOCS/carving-simulation.md`
e `DOCS/description.md`.
**Scopo:** dire onestamente *cosa è pubblicabile e a che livello*, *cosa è davvero nuovo e cosa no*,
*quali sono i riferimenti obbligatori* e *che taglio dare al paper* perché regga la peer review.

> Nota metodologica: le citazioni qui sotto sono state verificate via ricerca bibliografica (luglio
> 2026); i link sono nella sezione §8. Prima della submission **ricontrollate volume/pagine sul sito
> dell'editore** — ho segnalato con ⚠️ i dettagli da confermare.

---

## 1. Verdetto in una pagina (bottom line up front)

**È pubblicabile? Sì, ma non "così com'è" e non come contributo algoritmico fondamentale.** Allo
stato attuale è un solido lavoro *ingegneristico/di sistema*: rappresentazione ben scelta,
implementazione GPU efficiente, metodologia di validazione rigorosa (bit-exact) e — punto raro e
apprezzato — **un risultato negativo documentato** (RMQ). Questo lo rende adatto a una **rivista
applicata di fascia media** (CAD/CAM, manufacturing) o a un **workshop/conferenza di settore**, *a
condizione* di riformulare il claim di novità e di aggiungere alcuni esperimenti (§6).

**Il rischio numero uno** è di presentarlo come "nuova rappresentazione / nuovo metodo di
simulazione": non lo è. La rappresentazione a **colonne di transizioni Z** è, formalmente, un
**dexel model monodirezionale** (single-axis dexel / campo di occupazione run-length lungo Z), una
famiglia che esiste dal 1986 (van Hook) e che ha discendenti diretti molto vicini al vostro lavoro
(tri-dexel su GPU, LDNI). Un revisore esperto lo riconoscerà entro il primo paragrafo. La novità va
quindi spostata **dove effettivamente c'è**: il *come* si esegue la sottrazione (fusione
sweep+subtract per segmento in un singolo pass GPU, senza materializzare il volume spazzato) e
**l'analisi di sistema** (diventa memory-bound, con il negative result a supporto).

**In una riga:** *paper applicato, di fascia media, sull'ingegneria di un simulatore di asportazione
GPU-efficiente per 3 assi — non un paper di novità algoritmica pura.* Con il taglio giusto (§5) e 2–3
esperimenti in più (§6) diventa un contributo pulito e difendibile.

---

## 2. Che cos'è davvero il contributo (dal punto di vista di chi pubblica)

Separiamo nettamente ciò che è **noto** da ciò che è **vostro**.

**Noto / non rivendicabile come novità:**
- La rappresentazione a transizioni Z per colonna = **dexel monodirezionale** (van Hook 1986; la
  variante tri-dexel/LDNI è lo stato dell'arte). Compatta ∝ superficie, non volume: proprietà
  classica dei dexel.
- La sottrazione booleana come **merge di due liste ordinate per colonna** con semantica di
  differenza: è la sweep 1D standard su insiemi di intervalli.
- La voxelizzazione per **rasterizzazione a strati / depth-slicing** in una texture array: è la
  dexelizzazione per depth-peeling già usata in letteratura (Ren et al.; Inui). Ben fatta, ma non
  algoritmicamente nuova.
- Il marching cubes su GPU: classico (Lorensen–Cline; su GPU Dyken et al.).

**Vostro / difendibile come contributo:**
1. **Sottrazione del volume spazzato per segmento, fusa in un unico kernel, senza materializzare lo
   swept volume.** Invece di (a) timbrare il tool a ogni micro-passo *o* (b) costruire esplicitamente
   il volume spazzato e poi sottrarlo, calcolate **al volo, per colonna, l'inviluppo Z del tool lungo
   il segmento** e lo sottraete in-place nello stesso pass. Questa fusione "sweep-and-subtract in one
   shot" con **bounding del range di sub-step per colonna** è il cuore ingegneristico e la parte più
   originale.
2. **Correttezza dimostrabile e bit-exact rispetto al timbratore discreto** (ordine-indipendenza
   della differenza insiemistica + campionamento a passo di Chebyshev che colpisce ogni posizione
   intera). Non "sembra uguale": è *lo stesso volume solido residuo al byte*, con la sola rimozione —
   voluta e argomentata — dello scallop tra i campioni.
3. **Modello di costo e collo di bottiglia:** dimostrate, misure alla mano, che una volta rimossi gli
   overhead per-passo il simulatore diventa **memory-bandwidth-bound** sul buffer di lavoro, non
   compute-bound.
4. **Negative result (RMQ / sparse table):** una micro-ottimizzazione del *calcolo* dell'inviluppo
   rende solo ~25–30%, perché il collo di bottiglia è la banda di memoria. Documentato e revertato.
   In un paper serio questo vale molto: mostra rigore e delimita onestamente il contributo.
5. **Fusione vs buffer esterno — misurata (velocità *e* memoria).** La sottrazione *fusa in-place*,
   *senza materializzare il volume spazzato in un buffer esterno* (inviluppo Z O(1) per colonna), è ora
   **quantificata con un A/B diretto** contro un'implementazione a buffer esterno (`--legacy-external-buffer`,
   `shaders/subtract_swept_ext.comp`): fondere è **≈1.25–2.4× più veloce (geomedia ≈1.9×) e dimezza la
   memoria residente** (il buffer esterno richiede un "gemello" da 128 MB del working buffer, che la
   versione fusa elimina). La matrice cross-workload ha ora **quattro livelli** — stamping (S0) → swept a
   buffer esterno (S1) → swept fuso in-place (S2, *nostro*) → +pruning (S3, *nostro*) — così lo swept
   noto (S0→S1) è separato dalla nostra fusione (S1→S2). Più la **compattazione GPU del read-back**
   (128 MB → ~8 MB) e, in fase 2, il backend sparse tiled (~8 MB vs 122 MB, ~15×). Da presentare sia in
   velocità **sia in memoria**.

Il messaggio da vendere non è "abbiamo una nuova struttura dati", ma **"su una struttura dati nota e
ben scelta per il 3 assi, mostriamo come far collassare il costo per-mossa di ~2 ordini di grandezza
con una sottrazione swept fusa, e caratterizziamo il vero limite (banda), con validazione bit-exact e
un negative result"**.

---

## 3. Posizionamento nella letteratura (il paragrafo "Related Work" che i revisori pretendono)

Ordine cronologico/concettuale del filone in cui cadete. Dovete citarli **tutti** i capisaldi, e
soprattutto **differenziarvi esplicitamente** da LDNI e dal tri-dexel su GPU, che sono i vicini più
prossimi.

- **van Hook 1986** — introduce il *dexel* ("depth element") e la simulazione di fresatura per
  sottrazione booleana in image-space. **È l'origine della vostra rappresentazione**: citazione
  obbligata, va in apertura del Related Work.
- **Saito & Takahashi 1991 (G-buffer)** e **Jerard & Drysdale 1989 (verifica di superfici scolpite,
  z-buffer/point-set)** — i classici della verifica NC discreta. Contesto storico.
- **Blackmore, Leu & Wang 1997 (SEDE, swept-envelope differential equation)** — il riferimento
  "esatto/analitico" per il volume spazzato. Vi serve per dire: *noi non risolviamo la ODC
  dell'inviluppo; approssimiamo lo swept per colonna in Z, il che è sufficiente e molto più veloce
  per il 3 assi*. Ottimo contrasto.
- **Multi-/tri-dexel + GPU — i vostri competitor diretti:**
  - **Tukora & Szalay 2011/2012** — material removal multi-dexel su GPGPU (+ forze di taglio). ⚠️
    verificare anno/volume.
  - **Inui, Kobayashi & Umezu (2019–2021)** — triple-dexel su GPU, cutter-workpiece engagement,
    dexelizzazione con RT-core. **Sono lo stato dell'arte GPU-dexel**: dovete confrontarvi (almeno
    qualitativamente, meglio quantitativamente) con questi.
- **LDNI — Wang, Leung & Chen 2010** ("Solid modeling by Layered Depth-Normal Images on the GPU",
  CAD 42(6):535–544) e **Wang & Chen 2010** (sparse implicit representation). **Questo è il vicino
  più pericoloso:** rappresentazione implicita *sparsa*, per-asse, con **Boolean su GPU** — cioè
  concettualmente identica alla vostra (voi tenete un solo asse, Z, senza normali). Un revisore che
  conosce LDNI chiederà: *"in cosa differite?"*. La risposta onesta e forte è: **voi vi specializzate
  al 3 assi tenendo un singolo campo di dexel lungo l'asse utensile, e ottimizzate il caso d'uso
  "sottrazione lungo un toolpath" con la sweep fusa** — non fate modellazione solida generale.
  Mettetelo nero su bianco.
- **Marching cubes:** Lorensen–Cline 1987 (base), Dyken et al. 2008 (GPU/HistoPyramid). Vi serve solo
  per la parte di ricostruzione superficie / export.
- **Volumi sparsi (contesto rappresentazione):** Laine & Karras 2010 (Sparse Voxel Octrees), Museth
  2013 (OpenVDB) — per dire "esistono anche gli octree sparsi, ma per il 3 assi il campo dexel per
  colonna è più semplice e mappa 1:1 sui thread GPU". Facoltativi ma rafforzano.
- **Motivazione ML (solo per l'intro):** RL/DL per toolpath (brevetto US20210397142A1; DRL per
  toolpath termico arXiv:2404.07209; concept RL per CAM). Servono a giustificare "perché serve un
  forward model veloce per generare dati", **non** sono il contributo.

**Come usarli, in pratica:** van Hook + LDNI + tri-dexel/Inui vanno *contrastati esplicitamente*
(cosa fate di diverso); SEDE va contrastato come "alternativa esatta ma costosa"; gli altri sono
contesto. Se saltate LDNI o Inui, il paper rischia il reject "insufficient related work / unclear
novelty".

---

## 4. Punti di forza e di debolezza (visti da un revisore)

### Punti di forza
- **Speedup ampio e onesto sul proprio baseline** (~33×), con **geometria bit-identica**: la
  validazione è più rigorosa della media di questo settore (dove spesso ci si accontenta di "sembra
  giusto").
- **Argomento di correttezza formale** (ordine-indipendenza + campionamento di Chebyshev): raro e
  apprezzato in un paper applicato.
- **Analisi del collo di bottiglia + negative result:** dà credibilità scientifica, non solo
  "abbiamo fatto una cosa veloce".
- **Sistema completo e riproducibile** (CLI zero-dipendenze, comandi documentati, invariante di
  volume verificabile con uno script Python puro).
- **Buon match tra dominio e struttura dati:** per il 3 assi, il singolo dexel lungo Z è la scelta
  "giusta"; è un punto di forza da rivendicare, non da nascondere.
- **Confronto ancorato alla letteratura, apples-to-apples (AGGIUNTO):** la matrice cross-workload a
  **4 livelli** (S0 stamping [van Hook] → S1 swept a buffer esterno [Müller&Surmann/tri-dexel] → S2
  swept fuso in-place [nostro] → S3 +pruning [nostro]) implementa le *classi* note nel nostro stesso
  harness/rappresentazione/GPU su 6 tipologie di lavorazione — separando il swept già noto (S0→S1, ~3–8×)
  dai nostri advancement misurati: la **fusione/no-buffer-esterno** (S1→S2, ~1.9× e memoria dimezzata) e
  il **pruning** (S2→S3, 2.7× diagonale, 1.8× aria). Risponde a #2 e #3. Più lo **studio sparse-tiling**
  come test del working-set (footprint condizionato, non velocità), la **mappa del collo di bottiglia**
  (solo il traffico sul bbox rompe il tetto) e l'**argomento roofline** (§7) che rende la tesi
  memory-bound indipendente dall'hardware.

### Punti di debolezza — ciò che i revisori attaccheranno (e come pararla)
1. **"La rappresentazione non è nuova (è un dexel monodirezionale)."** → Non rivendicatela come
   novità; rivendicate la *sweep fusa* e l'analisi. (§2, §5)
2. **"Il baseline è la vostra versione naive, non lo stato dell'arte."** → **IN GRAN PARTE RISOLTO:**
   la matrice a 4 livelli implementa **S0 stamping [van Hook]** e **S1 swept a buffer esterno
   [Müller&Surmann/tri-dexel]** come nostre implementazioni *eque* delle classi note, misurate nello
   stesso harness → il confronto è ancorato alla letteratura, non a uno strawman, e separa il metodo
   noto (S0→S1) dai nostri due advancement (fusione S1→S2, pruning S2→S3). (Resterebbe utile, se fattibile, riprodurre un tri-dexel GPU altrui
   per un punto assoluto esterno — ma i numeri cross-paper non sono comparabili, e va detto.)
3. **"Un solo benchmark (square_600) e una sola iGPU."** → **RISOLTO sul fronte workload:** 6 tipologie
   (contour assi-allineato, pocket raster assi, raster 45°, rapidi, localizzato, finishing fine) via
   `tools/bench_matrix.sh`, scelte per stressare assi diversi. Resta la singola iGPU: una **GPU
   discreta** rafforzerebbe (da fare se accessibile).
4. **"Accuratezza = auto-consistenza."** Il bit-exact è *contro il vostro stesso timbratore*, non
   contro una ground truth. → Aggiungete **un confronto contro una geometria di riferimento**
   (analitica per casi semplici, o voxelizzazione ad altissima risoluzione, o un CAM commerciale) con
   una **metrica di errore geometrico** (Hausdorff/scostamento superficie). Questo è probabilmente
   l'intervento che alza di più il valore. (§6)
5. **"Limite a utensili convessi in Z + cap MAX_TRANSITIONS=32 non gestito."** → Dichiarateli come
   *limitazioni note* (lo fate già); meglio ancora, quantificate l'over-removal su un utensile non
   convesso.
6. **"3 assi soltanto."** → Va bene se il paper si intitola e si limita esplicitamente al 3 assi;
   diventa un problema solo se promettete generalità.

---

## 5. Taglio consigliato per il paper

Tre inquadrature possibili; la **B è la raccomandata**.

**Opzione A — "Forward simulator per generare dati ML per CAM appreso" (motivation-led / systems).**
Intro sul problema dei dati scarsi, il carving come forward model dentro il loop GA/LLM.
*Pro:* narrazione attraente, timely. *Contro:* i revisori vorranno vedere *anche* il pezzo ML (il GA
che genera toolpath, risultati di apprendimento). Se il ML non è pronto, questo taglio vi espone al
"promette ML, consegna un simulatore". **Rimandatelo al secondo paper.**

**Opzione B — "Simulazione di asportazione GPU-efficiente per fresatura a 3 assi via sottrazione
swept fusa su campo dexel" (algoritmico/di sistema, focalizzato). ✅ CONSIGLIATA.**
Il paper parla *solo* di carving: rappresentazione (dexel Z, dichiarato come tale), operatore di
sottrazione, **la sweep fusa per segmento (contributo)**, sub-step bounding, compattazione,
correttezza bit-exact, modello di costo memory-bound, **negative result**. La motivazione ML resta un
paragrafo in intro ("questo abilita generazione di dati / loop di ottimizzazione"). *Pro:* claim
stretto e difendibile, tutto ciò che affermate è misurato, il negative result gioca a favore.
*Contro:* meno "sexy" — ma molto più solido e accettabile.

**Opzione C — "Nuova pipeline di modellazione a dexel su GPU" (rischiosa).** Enfatizza rappresentazione
+ voxelizzazione + boolean + meshing come pipeline. *Contro:* invita direttamente il confronto con
LDNI e vi mette in salita sulla novità. **Sconsigliata.**

**Titolo di lavoro suggerito (Opzione B):**
*"Fused Swept-Segment Boolean Subtraction for Fast GPU Material-Removal Simulation in 3-Axis Milling"*
oppure
*"Bit-Exact, Bandwidth-Bound: A GPU Dexel-Field Carving Simulator for 3-Axis CNC"*.

**Framing della novità in una frase (da mettere in abstract e contributions):**
> Su un campo dexel monodirezionale GPU-resident, sostituiamo la timbratura per-passo del tool con
> **una singola sottrazione booleana per segmento del volume spazzato, calcolata al volo per colonna
> come inviluppo Z**, ottenendo un risultato *bit-identico* alla timbratura discreta (a meno dello
> scallop, rimosso per costruzione) e riducendo il costo geometrico per-mossa di ~2 ordini di
> grandezza; caratterizziamo poi il regime risultante come *memory-bandwidth-bound*, corroborato da
> un risultato negativo (accelerazione RMQ dell'inviluppo).

---

## 6. Checklist operativa per rendere il paper accettabile (in ordine di impatto)

1. **[Alto] Metrica di accuratezza vs ground truth.** Non solo auto-consistenza: scostamento
   geometrico (es. distanza di Hausdorff o errore superficie) vs (a) soluzione analitica su casi
   canonici (piano fresato da ball-end, tasca), (b) voxelizzazione ad altissima risoluzione, o (c)
   output di un CAM/simulatore commerciale. Chiude la debolezza #4.
2. **[FATTO, parziale] Confronto ancorato alla letteratura.** La matrice cross-workload implementa
   S0 stamping [van Hook] e S1 swept per-move [Müller&Surmann/tri-dexel] come nostre implementazioni
   eque nello stesso harness (`bench_matrix.sh`, `carving-simulation.md` §7.1). Chiude #2 senza numeri
   prestati. *Opzionale:* riprodurre un tri-dexel GPU altrui per un punto assoluto esterno.
3. **[FATTO, workload] Più benchmark.** 6 tipologie eterogenee (contour, pocket assi, raster 45°,
   rapidi, localizzato, finishing) con tabella tempi per stadio. Chiude #3 lato workload; l'argomento
   **roofline** (§7 "Portability", punto 6) mostra *strutturalmente* che il regime memory-bound scala a
   qualsiasi GPU, riducendo la necessità di una seconda GPU a una **conferma opzionale** (una run su
   discreta rafforzerebbe il grafico, ma non è più load-bearing).
4. **[FATTO] Ablation study ordinato** — la matrice §7.1 + la tabella storica (stamping → swept →
   compaction → substep) formalizzano il Δ per stadio, e §8 (RMQ / tiled-dispatch / sparse-tiling)
   aggiunge i test negativi e la mappa del collo di bottiglia.
5. **[Medio] Quantificare i limiti:** over-removal misurato su un utensile non convesso in Z;
   comportamento al variare della risoluzione (voxel size) su tempo e accuratezza (curva
   accuratezza/velocità).
6. **[FATTO] Roofline / conferma memory-bound.** §7 "Portability and the roofline argument" rende il
   claim *strutturale*: il merge per colonna ha intensità aritmetica ≪ 1 op/byte, 1–3 ordini di
   grandezza sotto il ridge point di qualunque GPU ⇒ memory-bound su ogni hardware (e *più* su NVIDIA
   discreta, rapporto compute:banda più alto). Separa inoltre ciò che è invariante (algoritmo, tesi,
   geometria del pruning) da ciò che è API-specifico (il negative result del tiled-dispatch dipende dal
   modello di barrier OpenGL). *Opzionale:* aggiungere una misura di banda raggiunta vs picco.
7. **[Basso] Riproducibilità:** rilascio del codice/commit e degli STL/gcode di test — molto ben
   visto, e voi siete già pronti (repo, script di validazione).

Se riuscite a fare **1+2+3**, il paper passa da "borderline workshop" a "solido journal applicato".

---

## 7. Venue suggerite (dalla più ambiziosa alla più sicura)

**Journal (target primario, Opzione B):**
- **Computer-Aided Design (Elsevier)** — la casa naturale di van Hook-derivati, LDNI, SEDE; se il
  contributo algoritmico è ben confezionato è il target più prestigioso realistico. Esigente sulla
  novità: serve la §6.
- **Computers & Graphics (Elsevier)** — buon fit per il lato GPU/rappresentazione.
- **The Visual Computer (Springer)** — accetta lavori di simulazione/visualizzazione ben fatti.
- **International Journal of Advanced Manufacturing Technology (IJAMT)** — molto ricettivo a
  simulazione di fresatura/dexel; forse il **più probabile accept** per un taglio manufacturing.
- **Journal of Computational Design and Engineering (JCDE)** / **Journal of Manufacturing Systems** —
  alternative solide.

**Conferenze / workshop (via più rapida, o per un primo lavoro):**
- **Solid and Physical Modeling (SPM)** / **Shape Modeling International (SMI)** — pubblico giusto per
  la parte booleana/rappresentazione.
- **Procedia CIRP / CIRP conferences**, **Procedia Manufacturing** — accettano volentieri lavori
  applicati di simulazione NC; buon "primo paper".

**Raccomandazione:** puntare **IJAMT o Computers & Graphics** con l'Opzione B; tenere CAD come
stretch se completate 1+2+3. Un'anteprima come **poster/short a SPM/SMI** è un ottimo modo per
raccogliere feedback prima del journal.

---

## 8. Bibliografia essenziale (verificata; ricontrollare ⚠️ i dettagli prima della submission)

**Origine dexel / verifica NC (obbligatorie):**
- T. Van Hook, "Real-time shaded NC milling display," *Computer Graphics (SIGGRAPH '86)*, 20(4):15–20,
  1986. — origine del dexel. https://www.semanticscholar.org/paper/018b2581bbe632f83338d0cd10fffd29c37ea4b3
- T. Saito, T. Takahashi, "NC machining with G-buffer method," *Computer Graphics (SIGGRAPH '91)*,
  25(4):207–216, 1991. https://dl.acm.org/doi/10.1145/127719.122741
- R. B. Jerard, R. L. Drysdale, et al., "Methods for detecting errors in numerically controlled
  machining of sculptured surfaces," *IEEE CG&A*, 9(1):26–39, 1989.
  https://ieeexplore.ieee.org/document/20331/
- ⚠️ "Simulation of NC machining based on the dexel model: a critical analysis," *Int. J. Adv. Manuf.
  Technol.* (verificare autori/anno). https://link.springer.com/article/10.1007/BF01179343

**Volume spazzato (contrasto "esatto vs approssimato"):**
- D. Blackmore, M. C. Leu, L. P. Wang, "The sweep-envelope differential equation algorithm and its
  application to NC machining verification," *Computer-Aided Design*, 29(9):629–637, 1997.
  https://www.sciencedirect.com/science/article/abs/pii/S0010448596001017
- K. Abdel-Malek et al., "Swept volumes: foundations, perspectives, and applications" (review). ⚠️
  verificare venue/anno. https://user.engineering.uiowa.edu/~amalek/papers/swept-volume-review.pdf

**Multi/tri-dexel su GPU (competitor diretti — confrontarsi):**
- B. Tukora, T. Szalay, "Multi-dexel based material removal simulation and cutting force prediction
  with the use of general-purpose graphics processing units," *Advances in Engineering Software*,
  2011/2012 ⚠️. https://www.sciencedirect.com/science/article/abs/pii/S0965997811002262
- M. Inui, S. Kobayashi, N. Umezu, "Cutter engagement feature extraction using triple-dexel
  representation workpiece model and GPU parallel processing," ~2019 ⚠️.
  https://www.semanticscholar.org/paper/8d1730256c9af960a657747c3f5f7dd8c047bc68
- M. Inui, N. Umezu, et al., "Fast dexelization of polyhedral models using ray-tracing cores of GPU,"
  *Computer-Aided Design & Applications*, ~2021 ⚠️.
  https://www.researchgate.net/publication/346804866
- "GPU accelerated voxel-based machining simulation," *Int. J. Adv. Manuf. Technol.*, 2021.
  https://link.springer.com/article/10.1007/s00170-021-07001-w

**Rappresentazione implicita sparsa per-asse (il vicino più prossimo — differenziarsi):**
- C. C. L. Wang, Y.-S. Leung, Y. Chen, "Solid modeling of polyhedral objects by Layered Depth-Normal
  Images on the GPU," *Computer-Aided Design*, 42(6):535–544, 2010.
  https://www.sciencedirect.com/science/article/abs/pii/S0010448510000278
- C. C. L. Wang, Y. Chen, "Layered Depth-Normal Images: a sparse implicit representation of solid
  models," arXiv:1009.0794, 2010. https://arxiv.org/abs/1009.0794

**Ricostruzione superficie (parte meshing / export):**
- W. E. Lorensen, H. E. Cline, "Marching cubes: a high resolution 3D surface construction algorithm,"
  *Computer Graphics (SIGGRAPH '87)*, 21(4):163–169, 1987.
- C. Dyken, G. Ziegler, C. Theobalt, H.-P. Seidel, "High-speed marching cubes using HistoPyramids,"
  *Computer Graphics Forum*, 27(8):2028–2039, 2008.
  https://onlinelibrary.wiley.com/doi/10.1111/j.1467-8659.2008.01182.x

**Contesto volumi sparsi (facoltativi):**
- S. Laine, T. Karras, "Efficient sparse voxel octrees," *I3D/IEEE TVCG*, 2010.
- K. Museth, "VDB: high-resolution sparse volumes with dynamic topology," *ACM TOG*, 32(3), 2013.

**Motivazione ML (solo intro):**
- "Toolpath generation by reinforcement learning for computer aided manufacturing," US Patent
  Application US20210397142A1. https://patents.google.com/patent/US20210397142A1/en
- "Deep reinforcement learning based toolpath generation for thermal uniformity...," arXiv:2404.07209.
  https://arxiv.org/pdf/2404.07209

---

## 9. Cosa NON mettere nel primo paper

- Il loop GA/LLM e la generazione di dati come *risultato* (non è ancora dimostrato): tenetelo come
  motivazione e future work → **secondo paper**.
- Rivendicazioni di novità sulla rappresentazione o sulla voxelizzazione.
- Il fitness evaluator come contributo scientifico: è ottimo strumento, ma è "engineering di
  supporto" per il GA, fuori scope per il paper sul carving. Al massimo una riga in future work.

---

### Sintesi finale
Avete un buon **paper applicato di fascia media** se lo focalizzate sul **carving swept fuso** (non
sulla rappresentazione), vi **confrontate con il tri-dexel/LDNI**, e aggiungete **accuratezza vs
ground truth + un competitor reale + più benchmark**. Il negative result e la validazione bit-exact
sono già asset che molti paper del settore non hanno: valorizzateli. Target realistico: **IJAMT /
Computers & Graphics**, con **CAD** come obiettivo ambizioso se completate la checklist §6.
</content>
