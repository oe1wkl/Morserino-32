# Morserino-32 Pocket — Häufig gestellte Fragen (FAQ)

Diese FAQ ist aus dem offiziellen Benutzerhandbuch und aus Diskussionen der Community auf groups.io zusammengestellt. Sie behandelt den **M32 Pocket**; wo eine Antwort auch für den klassischen Morserino-32 (1. und 2. Edition) gilt, ist das vermerkt. Sie ist eine Ergänzung zum Benutzerhandbuch, kein Ersatz — dort sind alle Betriebsarten und Einstellungen vollständig beschrieben.

Für jeden Morserino gibt es ein eigenes Handbuch. Diese Links verweisen immer auf jenes zur aktuellsten veröffentlichten Firmware:

| Dein Morserino | Deutsch | English |
|---|---|---|
| **Morserino-32 Pocket** | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_DE.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_DE.epub) | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_EN.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_EN.epub) |
| **Morserino-32, 1. / 2. Edition** | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_DE.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_DE.epub) | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_EN.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_EN.epub) |
| **Pocket, Accessibility Edition** | [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_DE.epub) · [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_DE.pdf) | [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_EN.epub) · [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_EN.pdf) |

Die Antworten beziehen sich auf **Firmware-Version 9.0**.

Die Namen der Einstellungen sind am Gerät englisch und werden hier deshalb ebenfalls englisch angegeben.

## 1. Umstieg von einem Morserino der 1. oder 2. Edition

* **Ich habe einen älteren Morserino (1. oder 2. Edition). Soll ich mir einen M32 Pocket zulegen?**
    * Ein funktionierendes Gerät muss man deswegen nicht ersetzen. Beide Modelle laufen mit **derselben Firmware** aus demselben Release und bekommen dieselben Updates. Alle Übungs-Betriebsarten gibt es auf beiden: Koch-Trainer, CW-Generator, Echo Trainer, die Transceiver-Betriebsarten, den QSO Bot, die Bluetooth-Tastatur und BLE Serial, den WiFi-/ESP-NOW-Transceiver sowie das serielle Protokoll mit dem Configuration Tool.
    * **Was der M32 Pocket kann und die älteren Modelle nicht:**
        * Er ist kleiner und hat ein geschlossenes Gehäuse — damit passt er wirklich in die Tasche.
        * Ein Farbdisplay mit vier oder fünf Textzeilen statt drei auf einem monochromen OLED, dazu wählbare Farbschemata und die Einstellung **Font Size**.
        * Deutlich besseres Audio: ein eigener Audio-Codec statt eines per PWM erzeugten Tons — der Mithörton ist ein sauberer Sinus, seine Flankenformung ist einstellbar (**Tone Softness**), und du kannst eigene Klänge für den Echo Trainer hochladen.
        * Die sieben **Spiele** und die **Übungsstatistik** für den Koch-Trainer.
        * Die **Accessibility Edition**, die die Bedienoberfläche laut vorliest — es gibt sie nur für den Pocket.
        * USB-C statt Micro-USB.
    * **Worauf du verzichten würdest:**
        * **LoRa.** Die 1. und 2. Edition haben einen LoRa-Transceiver mit SMA-Antennenanschluss (um 433 MHz), der M32 Pocket nicht. Da sowohl die älteren Morserinos als auch der Pocket mit ESP-NOW vergleichbare Funktionalität liefern, wird dies nur selten deine Entscheidung beeinflussen.
        * Die **Trimmpotentiometer** — den Trimmer für den Audio-Eingangspegel und, bei der 2. Edition, den für den Kopfhörerpegel. Der Pocket hat beides nicht.
    * **Ehrliches Fazit**: Wenn dein Morserino funktioniert und du weder die Spiele noch die Statistik, die gesprochenen Menüs oder das bessere Audio besonders brauchst, gibt es keinen zwingenden Grund für ein zweites Gerät. Beim eigentlichen CW-Training — und dafür sind beide da — entgeht dir nichts. Kauf den Pocket wegen der Größe, des Displays und des Klangs, oder weil du die Accessibility Edition brauchst. Behalte den klassischen, wenn du auf LoRa angewiesen bist — und nicht wenige Besitzer haben schlicht beide.

* **Ich habe (oder hatte) einen älteren Morserino und jetzt einen M32 Pocket. Welche Stolperfallen sind die wichtigsten?**
    * **Die Audiobuchse ist anders belegt, und ein Fehler dabei kann das Gerät zerstören.** Bei der 1. und 2. Edition ist der **Schaft der Audioausgang**. Beim M32 Pocket ist die Buchse 4-polig (TRRS) und der **Schaft der Audioeingang**. Verwende also kein Kabel und keinen Adapter weiter, der am alten Morserino funktioniert hat — er würde die Audiospannung deines Empfängers direkt auf den Ausgang des Pocket legen. Abschnitt 5 beschreibt das Y-Adapterkabel, das du wirklich brauchst.
    * **Ein anderer Akku.** Die 1. und 2. Edition verwenden eine flache 3,7-V-LiPo-Zelle. Der M32 Pocket braucht eine **14500-Li-Ion-Zelle** — Bauform AA, 3,6–3,7 V, höchstens 52 mm lang und 14,5 mm dick, geschützte Zelle empfohlen. Die beiden sind in keiner Richtung austauschbar.
    * **Ein anderer USB-Anschluss.** Micro-USB bei den älteren Modellen, **USB-C** beim Pocket. Der Pocket zieht beim Laden auch mehr: bis zu etwa 500–600 mA gegenüber rund 200 mA.
    * **Kein LoRa.** Die LoRa-Transceiver-Betriebsarten gibt es schlicht nicht; nimm stattdessen den WiFi-Transceiver.
    * **Keine Trimmer.** Es gibt kein Potentiometer für den Audio-Eingangspegel — den Pegel stellst du stattdessen an der Quelle ein, siehe Abschnitt 5.
    * **Die FN-Taste ist eine Aussparung im Gehäuse**, rechts unterhalb des Drehgebers, und kein hervorstehender roter Knopf. Mehr als ein Besitzer hat sie erst einmal gesucht.
    * **Eine Geste bedeutet etwas anderes.** Ein langer Druck auf FN im Menü startet bei den älteren Modellen die Einstellung des Audio-Eingangspegels; beim Pocket tastet er nur den Sender und erzeugt einen Mithörton.
    * **Deine Einstellungen wandern nicht mit.** Es gibt keine Sicherung und Wiederherstellung zwischen Geräten — ein neuer Pocket startet mit den Werkseinstellungen. Rechne damit, Rufzeichen, Koch-Lektion, Geschwindigkeit und Einstellungen neu zu setzen. Auch Schnappschüsse werden nicht übertragen.

## 2. Stromversorgung, Akku und Laden

* **Welchen Akku soll ich verwenden?**
    * Eine **14500-Li-Ion-Zelle (3,7 V)**. 14500-Zellen haben dieselbe Bauform wie gewöhnliche AA-Batterien, wir brauchen aber 3,6–3,7-V-Zellen, also Li-Ion. Die Höchstmaße sind 52 mm Länge und 14,5 mm Durchmesser.
    * **Empfehlung**: Verwende eine **geschützte Zelle** (mit eingebautem Schutz gegen Tiefentladung), damit der Akku nicht vorzeitig stirbt. Aus der Community werden häufig hochwertige Marken wie Keeppower (800 mAh) oder XTar empfohlen.
* **Wo bekomme ich einen 14500-Akku?**
    * Dazu sagt das Handbuch nichts. In der Community werden übliche Versandhändler (Amazon, Marken wie JESSPOW) oder spezialisierte Akku-Shops genannt.
* **Brauche ich ein bestimmtes USB-C-Netzteil?**
    * Nein. Jedes normale **5-V-USB-Netzteil** (etwa ein übliches Handy-Ladegerät) genügt.
    * Der M32 Pocket zieht beim Laden des Akkus höchstens **500–600 mA**.
* **Wie lade ich den Akku?**
    * USB-C-Kabel anstecken und darauf achten, dass der **Ein/Aus-Schalter auf ON (I) steht**. Bei ausgeschaltetem Hardware-Schalter wird der Akku **nicht** geladen.
    * **Anzeige**: Der M32 Pocket zeigt in jedem Menü ein Akku- bzw. Ladesymbol in der obersten Zeile des Displays.
* **Stolperfalle leerer Akku**: Ist die Akkuspannung gefährlich niedrig, erscheint ein leeres Akkusymbol und das Gerät startet nicht. Auch Symptome wie aussetzender Ton beim Geben deuten darauf hin, dass die Akkuspannung zu niedrig ist und geladen werden muss.

## 3. Gehäuse und 3D-Druck

* **Wo finde ich Dateien für den 3D-Druck?**
    * Das Handbuch nennt keine Adressen, aus der Community wird aber auf **Printables** verwiesen (z. B. Modell 1550518 von Michael K Johnson/KZ4LY) — dort gibt es FreeCAD-, STEP- und STL-Dateien für den M32 Pocket.
* **Wie übertrage ich die Sensorflächen in ein neues Gehäuse?**
    * Die Touchpaddles sind **kapazitiv** und bestehen aus einseitigem Platinenmaterial.
    * **Vorgehen**: Das Platinenmaterial ist ins Gehäuse geklebt — mit etwas Aceton lässt sich der Kleber aufweichen, sodass du die Plättchen vorsichtig aus dem alten Gehäuse lösen und ins neue einsetzen kannst. Achte darauf, die Drähte genauso wieder anzuschließen wie zuvor.
* **Der „fehlende" rote Knopf**: Beim M32 Pocket ist der „rote Knopf" (die FN-Taste) **ins Gehäuse integriert** und kein eigenes hervorstehendes Bauteil. Es ist eine Aussparung im Gehäuse, rechts unterhalb des Drehgebers.

## 4. Externe Tasten und Anschlüsse

* **An welche Buchse kommt eine externe Taste?**
    * An die mit „External Paddle" beschriftete **3,5-mm-Klinkenbuchse, 3-polig (TRS)**. Es ist die Buchse, die dem USB-Anschluss am nächsten liegt.
* **Belegung**:
    * **Doppelhebel-Paddle**: Spitze = links, Ring = rechts, Schaft = Masse.
    * **Handtaste**: Spitze = Taste, Schaft = Masse.
* **Einstellung**: Um im Echo Trainer oder in den Transceiver-Betriebsarten eine Handtaste oder einen Bug zu verwenden, musst du die Einstellung **Keyer Mode** auf **Straight Key** ändern.
* **Kann ich eine mechanische Taste an den internen Paddle-Anschluss hängen?**
    * Ja, das ist allerdings noch **experimentell**. Der Anschluss auf der Platine, der normalerweise die kapazitiven Touchpaddles des Pocket bedient (CN3), lässt sich über das Menü **Hardware Config** auf eine mechanische Taste bzw. ein mechanisches Paddle umschalten. Das ist praktisch, wenn du eine mechanische Taste gemeinsam mit dem M32 Pocket in ein Gehäuse einbauen willst.

## 5. Audio-Ein- und -Ausgang

* **Wie ist die Audio-I/O-Buchse belegt?**
    * Der M32 Pocket verwendet eine **3,5-mm-Klinkenbuchse, 4-polig (TRRS)**.
    * **Belegung M32 Pocket**: Spitze und 1. Ring = Audio-/Kopfhörerausgang; 2. Ring = Masse; **Schaft = Audioeingang**.
    * **Achtung**: Das ist anders als beim Morserino-32 der 1. und 2. Edition, wo der Schaft der Audioausgang war.

* **Kann ich einfach ein normales Audiokabel vom Empfänger/Transceiver zum M32 Pocket nehmen?**
    * **Nein — verwende kein einfaches zwei- oder dreipoliges Audiokabel, um einen Transceiver mit dem M32 Pocket zu verbinden.** Die Audiospannung deines Empfängers läge dann am *Ausgang* des M32 Pocket und würde ihn wahrscheinlich zerstören.

* **Wie bekomme ich das Audiosignal eines Transceivers zum Dekodieren in den M32 Pocket?**
    * Du brauchst ein **Y-Adapterkabel: 1× 3,5-mm-Stecker CTIA 4-polig (TRRS) auf 2× 3,5-mm-Buchsen**.
    * Steck den 4-poligen Stecker in die Headset-Buchse des M32 Pocket und deinen Kopfhörer in die grüne Buchse (Kopfhörersymbol) des Adapters.
    * Verbinde den Kopfhörer- oder Lautsprecherausgang des Transceivers über ein 3,5-mm-Kabel mit der rosa Buchse (Mikrofonsymbol) des Adapters. Der Transceiver wird dabei erwartungsgemäß stumm.
    * Stelle am Empfänger eine saubere, gut lesbare CW-Station ein und **drehe seine Lautstärke ganz weit zurück** — es ist entscheidend, den Eingang des M32 Pocket nicht zu übersteuern.
    * Schalte den M32 Pocket in die Dekoder-Betriebsart und stelle seine Lautstärke auf etwa ein Drittel. Zunächst hörst du im Kopfhörer nichts.
    * Drehe nun die Lautstärke am Transceiver langsam auf. Sobald der M32 Pocket ein dekodierbares Signal erkennt, beginnt er zu arbeiten: Zu jedem erkannten Dit und Dah hörst du einen Piepton, und links oben im Display blinkt ein kleines schwarzes Quadrat.
    * Übersteuere den Eingang nicht — sonst bricht die Dekodierung ab und es kommt auch kein Ton mehr zum Headset.
    * Solange nicht dekodiert wird, bleibt der Kopfhörerausgang stumm; du kannst also beim Absuchen des Bandes nicht mithören. Zum Abstimmen das Kabel abziehen und danach wieder anstecken.

* **Wie stelle ich die Audiopegel ein?**
    * Mit der **FN-Taste** wechselst du während einer laufenden Betriebsart zwischen Geschwindigkeits- und Lautstärkeregelung.
    * Beim M32 Pocket tastet ein **langer Druck auf die FN-Taste im Menü** den Sender (falls einer angeschlossen ist) und erzeugt einen Mithörton — praktisch, um Pegel an einem angeschlossenen Computer oder Transceiver einzustellen. Ein kurzer Druck auf FN schaltet das wieder ab. (Beim klassischen Morserino-32 startet dieselbe Geste stattdessen die Einstellung des Audio-Eingangspegels.)

* **Meine eigenen Erfolgs- und Fehlertöne sind mit Version 9 leiser geworden. Warum?**
    * Weil sie nicht mehr verzerrt werden. Bis Version 9 war die interne Verstärkung des Audiochips so hoch eingestellt, dass alles schon im Chip übersteuert wurde, bevor es überhaupt zur Lautstärkeregelung kam. Version 9 behebt das — eigene Klänge werden nun sauber wiedergegeben, und sauber heißt eben leiser als die verzerrte Fassung.
    * Wenn du sie lauter haben willst, bereite die Dateien selbst etwas lauter auf, halte den Spitzenpegel aber **bei oder knapp unter −1 dBFS**. Eine auf volle 0 dBFS normalisierte Datei verzerrt weiterhin, weil dem MP3-Dekoder der Aussteuerungsspielraum früher ausgeht als dem Audiochip.
    * Die beiden Bestätigungsklänge des Echo Trainers lassen sich ersetzen, indem du mit dem Configuration Tool `/sounds/success.mp3` und `/sounds/error.mp3` hochlädst. Fehlen diese Dateien, werden die eingebauten Pieptöne verwendet.

* **Der CW-Mithörton klingt seit Version 9 anders.**
    * Dieselbe Ursache. Der Mithörton ist jetzt ein sauberer Sinus statt des vorher obertonreichen Klangs. Über den kleinen eingebauten Lautsprecher kann er dadurch etwas weniger durchdringend wirken, obwohl er nicht leiser ist.

## 6. Bluetooth und VBand

* **Wie schalte ich Bluetooth ein?**
    * Über die Einstellung **Bluetooth Use**. Neben **No Bluetooth** gibt es vier Tastatur-Betriebsarten (VBand Kbd, Decoded output, VBand+Decoded, Generic Kbd) und **BLE Serial** (das M32-Serial-Protokoll über Bluetooth, siehe unten).
    * Da **Bluetooth Use** eine einzige Auswahl ist, bedient Bluetooth entweder die Tastaturausgabe oder das serielle Protokoll — nie beides gleichzeitig. Jede WiFi-Funktion (Mehrspieler-Spiele, Upload, Update, WiFi-Transceiver) unterbricht die Bluetooth-Verbindung, bis du ins Hauptmenü zurückkehrst.
* **Wie verbinde ich mich mit VBand?**
    * Der Morserino kann sich über Bluetooth mit **VBand** (hamradio.solutions/vband) verbinden.
    * Aus der Community: Achte darauf, dass dein Browser die nötigen Protokolle unterstützt, und stelle die VBand-Eingabe auf „Straight Key", falls deine Paddles nicht richtig erkannt werden.
* **Bluetooth-Tastatur**: Der M32 Pocket kann als **Bluetooth-Tastatur** arbeiten. Dekodierte Zeichen werden so gesendet, als wären sie auf einer US-QWERTY-Tastatur getippt worden.

* **Können Apps über Bluetooth mit dem Morserino sprechen (z. B. vom iPhone aus)?**
    * Ja. Stelle die Einstellung **Bluetooth Use** auf **BLE Serial** (wirksam, sobald du das nächste Mal ins Hauptmenü zurückkehrst). Das Gerät bietet dann das vollständige **M32-Serial-Protokoll** über Bluetooth Low Energy an, sichtbar als „Morserino-32". Apps können es damit fernsteuern und Text zum Geben schicken — dasselbe Protokoll, das auch über USB zur Verfügung steht. Siehe Anhang 8 des Benutzerhandbuchs.
* **Ich habe BLE Serial eingestellt, aber die App verbindet sich nicht. Was fehlt?**
    * Seit Version 9 **fragt der Morserino am Gerät um Erlaubnis**, bevor er die Kontrolle abgibt. Fordert eine App eine Sitzung an, zeigt das Display **„Allow connect?"** und wartet darauf, dass du **FN** drückst. Der Grund: Ein USB-Kabel muss jemand anstecken, der neben dem Gerät steht — über Bluetooth kann sich dagegen alles verbinden, was in Funkreichweite ist.
    * Gefragt wirst du nur **im Hauptmenü**. Trifft eine Anfrage ein, während du in einer Übungs-Betriebsart bist, wird sie abgelehnt, ohne dich zu unterbrechen — lass das Gerät also im Hauptmenü, wenn du eine App-Verbindung erwartest.
    * Eine App, die sich **innerhalb einer Minute** wieder verbindet, wird ohne erneutes Nachfragen durchgelassen.
    * Während eine Sitzung offen ist, zeigt ein kleines Bluetooth-Symbol in der obersten Zeile an, dass etwas verbunden ist.
    * Ein Koppeln („Pairing") im Sinne der Bluetooth-Einstellungen ist nicht nötig; die Rückfrage am Gerät tritt an dessen Stelle.

* **Kann ich Audio über Bluetooth übertragen?**
    * Nein, und es wäre auch nicht sinnvoll: Bluetooth-Audio bringt eine spürbare Verzögerung mit sich, die sauberes Geben sehr schwer macht.

## 7. Die Accessibility Edition (nur M32 Pocket)

* **Was ist das?**
    * Eine eigene Ausgabe der Firmware, die **Menüs und Einstellungen laut vorliest** — für blinde und sehbehinderte Operatoren.
    * Drei Dinge fehlen darin, sonst nichts: die **Spiele** sowie die beiden WLAN-Einträge **Upload File** und **Update Firmw**. Die Spiele sind ohne Augenlicht nicht spielbar. Die beiden anderen erledigen die Arbeit ohnehin nicht selbst — sie versetzen den Morserino nur in einen Wartezustand und erwarten, dass du die Sache in einem Browser auf einem anderen Rechner zu Ende bringst; dorthin geschickt zu werden ist unangenehm, wenn das Gerät nicht sagen kann, was gerade passiert. Beides geht ohnehin besser gleich vom Rechner aus: für die Firmware die Installationsseite, zum Hochladen einer Datei das Konfigurationsprogramm. Alles Übrige — jede Betriebsart, jede Einstellung — ist vorhanden.
* **Wie kommt sie auf mein Gerät?**
    * Ein neuer Pocket wird mit der Standard-Edition ausgeliefert, die stumm ist. Die Accessibility Edition installierst du über dieselbe Installationsseite wie jede andere Firmware (`morserino.info/install.html`) — sie wird für dasselbe Gerät zur Auswahl angeboten.
    * Du kannst jederzeit zwischen Standard- und Accessibility Edition wechseln. Deine Einstellungen, Schnappschüsse und die gespeicherte Geschwindigkeit überstehen den Wechsel in beide Richtungen. **Nur der Text des File Players nicht** — genau diesen Speicher übernehmen die Sprachclips.
* **Sie hat gesprochen und tut es plötzlich nicht mehr — ist sie kaputt?**
    * Mit ziemlicher Sicherheit nicht. Die Sprachclips liegen getrennt vom Programm, deshalb können die beiden auseinanderlaufen: Installierst du nur das Programm neu, oder wird eine Installation mittendrin abgebrochen, läuft der Morserino einwandfrei weiter — Keyer, Generator, Dekoder und alles andere — hat aber nichts mehr, womit er sprechen könnte, oder nur noch die Clips einer älteren Version.
    * Das Gerät sagt dir, wenn das passiert ist. Beim Einschalten hörst du **vier Paare abwechselnd hoher und tiefer Töne** (ein Zweiklang-Alarm von etwa zwei Sekunden), und der Startbildschirm nennt den Fall:
        * **„No voice clips!"** — es sind überhaupt keine Clips vorhanden; außer dem Alarm bleibt das Gerät stumm.
        * **„Wrong voice pack!"** — die Clips gehören zu einer anderen Version. Die meisten passen noch, das Gerät spricht also weiter; nur das, was seit diesen Clips dazugekommen oder umformuliert worden ist, bleibt stumm.
    * Abhilfe in beiden Fällen: das Installationsprogramm noch einmal laufen lassen und dabei **Accessibility Edition** sowie **Keep my settings** wählen. Damit werden Programm und passende Sprachclips gemeinsam installiert.
    * Der Alarm ist bewusst weder Sprache noch Morsezeichen: Er muss gerade dann funktionieren, wenn die Sprache fehlt, und er darf nicht voraussetzen, dass du schon Morsezeichen lesen kannst.

## 8. Firmware-Updates

* **Was ist der einfachste Weg zu einem Update?**
    * **Webserial**: `morserino.info/install.html` in einem unterstützten Browser aufrufen. Dafür brauchst du weder Kommandozeilen-Werkzeuge noch einen separaten Firmware-Download.
    * Bei der **Accessibility Edition** ist das der einzige Weg: In deren WLAN-Menü gibt es keinen Eintrag *Update Firmw*, weil dieser die Aufgabe ohnehin nur an einen Browser auf einem anderen Rechner weiterreicht. Führe das Installationsprogramm also gleich dort aus.
    * Es gibt jetzt **ein einziges Installationsprogramm für alle Morserinos**. Es fragt den Prozessor im Gerät, welcher Morserino das ist — du musst also nicht mehr wissen, ob du auf einer Seite für den klassischen M32 oder für den M32 Pocket beginnen musst. Es zeigt außerdem an, welche Firmware-Version gerade auf dem Gerät ist, bevor du irgendetwas installierst, und lässt dich wählen, ob deine Einstellungen erhalten bleiben oder gelöscht werden. Die beiden bisherigen Installationsseiten leiten dorthin weiter, alte Lesezeichen funktionieren also weiter.
* **Welche Browser funktionieren?**
    * **Google Chrome**, **Microsoft Edge**, **Opera** und **Firefox ab Version 151** (die im Mai 2026 Unterstützung für Web Serial gebracht hat). **Safari kann das nicht.**
    * Du brauchst einen Desktop- oder Laptop-Computer — auf Telefonen und Tablets gibt es Web Serial nicht.
    * Wenn du Firefox in einer verwalteten Firmeninstallation verwendest: **Firefox-Enterprise-Policies können Web Serial abschalten**. Dann findet das Installationsprogramm dein Gerät auch mit einer aktuellen Version nicht.
* **Wichtige Stolperfallen**:
    * **Wach bleiben**: Der M32 Pocket muss beim Anstecken zum Update **eingeschaltet sein** und **darf nicht im Schlafmodus sein**.
    * **Kabelqualität**: Nimm ein **datenfähiges USB-Kabel**, kein reines Ladekabel.
    * **Ein dunkler Bildschirm direkt nach dem Löschen ist normal**: Nach einer Installation mit der Lösch-Option — oder bei einem fabrikneuen Gerät — kann es bis zu etwa zehn Sekunden dauern, bis überhaupt etwas zu sehen ist, weil das Dateisystem vorbereitet wird. Seit Version 9 wird währenddessen ein erklärender Startbildschirm angezeigt, damit du das von einem Fehler unterscheiden kannst.

## 9. Übungsfunktionen und Einstellungen

* **Was sind „Practice Sets"?**
    * Sie treten an die Stelle der früheren Custom-Zeichensätze. Du wählst direkt am Gerät eine Teilmenge einzelner Zeichen aus und übst dann genau diese im CW-Generator oder im Echo Trainer.
* **Und „Custom Characters"?**
    * Eine eigene Zeichenreihenfolge verhält sich jetzt genau wie eine der eingebauten Koch-Sequenzen: Select Lesson, Learn New Character und die übrigen Koch-Methoden funktionieren damit auf dieselbe Weise.
* **Kann ich sehen, wie gut ich vorankomme?**
    * Ja. Beim Üben mit dem Koch-Trainer zeichnet der M32 Pocket eine **Übungsstatistik** auf. Ansehen kannst du sie über das Configuration Tool oder über den eingebauten Webserver des Morserino mit einem Browser.
* **Die Schrift im Scrollbereich ist mir zu klein / zu groß.**
    * Die Einstellung **Font Size** schaltet den Scrollbereich zwischen der normalen Textgröße und einer kleineren um. Die kleinere fasst mehr Zeichen pro Zeile und zeigt fünf statt vier Zeilen — praktisch bei höheren Geschwindigkeiten oder bei langen Wörtern, wenn du mehr vom gerade Gegebenen sehen willst.
* **Der CW-Ton klickt, oder er ist mir zu weich.**
    * **Tone Softness** bestimmt die Zeitkonstante der Flankenformung, die die Knackgeräusche beseitigt. Voreingestellt sind 5 ms; einstellbar ist alles zwischen 1 ms (ziemlich hart, mit Klicken) und 9 ms (für schnelles CW eindeutig zu lang).
* **Was macht der QSO Bot?**
    * Er simuliert einen QSO-Partner, für Contest-, SOTA/POTA- und normale QSOs. Eine **Difficulty**-Einstellung bestimmt, wie anspruchsvoll der Ablauf ist, und dein Gegenüber kann mit einer anderen Geschwindigkeit arbeiten als du.
* **Meine Schnappschüsse haben sich unter einer älteren Firmware seltsam verhalten.**
    * Es gab einen Fehler, der den Einstellungsspeicher des Geräts volllaufen lassen und das Speichern von Schnappschüssen verhindern konnte. Er ist behoben, und Schnappschüsse werden jetzt deutlich sparsamer abgelegt. Beim ersten Start nach dem Update werden vorhandene Schnappschüsse einmalig ins neue Format umgewandelt — das kann ein paar Sekunden dauern und geschieht nur dieses eine Mal.

## 10. Spiele (nur M32 Pocket)

* **Welche Spiele gibt es?**
    * Sieben: **Morse Invaders**, **Fight the Pileup**, **Radio Cave**, **Morsel**, **Trailblazer**, **Fox Hunt** und **Memory Chain**. Sie sind im Benutzerhandbuch beschrieben.
    * Die meisten berücksichtigen deine aktuellen Koch-Einstellungen, üben also die Zeichen, an denen du gerade arbeitest (Ausnahme sind Fight the Pileup, das mit Rufzeichen arbeitet, und Radio Cave).
    * Vier davon — Fight the Pileup, Morsel, Trailblazer und Fox Hunt — lassen sich auch gegen einen zweiten oder mehrere Morserinos spielen.
* **Warum hat mein Gerät keine Spiele?**
    * Entweder ist es ein klassischer Morserino-32 (die Spiele gibt es nur für den M32 Pocket), oder es läuft die **Accessibility Edition**, in der sie nicht enthalten sind.
* **Wie lösche ich die Highscores?**
    * Mit **Reset Scores** im Einstellungsmenü am Gerät. Das Configuration Tool kann dasselbe und zeigt außerdem die gespeicherten Punktetabellen an — einschließlich der Frage, ob für Radio Cave ein gespeichertes Spiel vorliegt — auf dem Reiter „User Identity".
