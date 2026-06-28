shift + CTRL + B: debug compile
CTRL + ALT + B: release compile
F5: run debug

L'eseguibile si chiama 'autocam' (in 'release/' e 'debug/').
Esecuzione dalla radice del repo: ./release/autocam <comando> [opzioni]
Per richiamarlo come 'autocam' da qualsiasi cartella è installato un wrapper in ~/.local/bin/autocam.
Il wrapper esegue la release per default; con 'autocam --debug <comando>' (o -d) esegue la build di debug.
Riferimento completo dei comandi: MANUAL.md
