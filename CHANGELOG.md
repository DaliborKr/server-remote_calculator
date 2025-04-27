# CHANGELOG
## **IPK - Project 2 - IOTA: Server for Remote Calculator**
#### Autor: `Dalibor Kříčka`
#### Login: `xkrick01`
#### Brno 2023
---

## **Stručný popis implementované funkcionality**

Program po ověření a zpracování argumentů programu (_IPv4 adresa_, _port_, _režim_) vytvoří hlavní soket serveru podle typu zvoleného režimu pomocí funkce `socket()`. V případě režimu _TCP_ se nastaví funkcí `setsockopt()` znovupoužitelnost portu na soketu. Dále nastaví ze zadaných argumentů programu informace o serveru a sváže adresu serveru s hlavním soketem funkcí `bind()`, aby s ním bylo možné komunikovat. 

Při _UDP_ komunikaci server přijme zprávu od klienta, zkontroluje její hlavičku, zpracuje zadaný matematický výraz a odešle zprávu zpět klientovi. Tento proces probíhá v nekonečné smyčce. Při _TCP_ komunikaci se nejprve inicializuje pole deskriptorů soketů na hodnotu _0_. Velikost tohoto pole určuje maximální počet v jeden moment připojených uživatelů, který je dán hodnotou makra `MAX_CONNECTIONS`. Dále se funkcí `listen()` umožní serveru přijímat nová spojení. Dále již v nekonečné smyčce. Vytvoří a nastaví množinu deskriptorů (typ `fd_set`). Následně funkcí `select()` zmonitoruje zdali některý z deskriptorů nezaznamenal změnu, na kterou následně program reaguje. V případě že změnu zaregistroval hlavní soket serveru, vytvoří se nový soket pro komunikaci s klientem (`accept()`) a přidá se do pole deskriptorů. Pokud změnu zaznamenal některý ze soketů pro komunikaci s klienty, přečte se od daného klienta zpráva a po jejím zpracování se odešle zpět patřičná odpověď.

Program sice běží v nekonečných smyčkách, nicméně lze kdykoliv ukončit pomocí signálu přerušení (C-c), kdy server patřičně uzavře všechny své připojení a skončí. UML diagram aktivit je k nalezení v [README.md](README.md).

## **Limitace**

Program byl testován na operačních systémech `NixOS` a `Fedora`. Na jiných než výše uvedených operačních systémech tedy není zaručena korektní funkcionalita (Windows, MacOS, ...). Konkrétně systém Windows ve své standardní knihovně neobsahuje hlavičkové soubory `sys/socket.h`, `arpa/inet.h` a `sys/select.h` a bylo by tak třeba použít nějakou alternativu (např. `winsock2.h`).

**Maximální** počet Bytů, který může mít zpráva přijatá od klienta (včetně operačního kódu, stavového kódu i délky posílané zprávy), je **1024B**.

**Maximální** počet uživatelů, který může v jeden moment komunikovat se serverem přes TCP protokol je **10**. Maximální počet uživatelů lze změnit upravením hodnoty makra `MAX_CONNECTIONS`, nicméně s vyššími hodnotami bude narůstat i režie při iteraci skrze více deskriptorů.

**Výsledky** posílané ve zprávách klientům jsou **absolutní hodnotou** skutečných výsledků, jelikož validní hodnoty _IPK Calculator protokolu_ nemohou dle zadané ABNF notace obsahovat znaménka (tedy záporné hodnoty).