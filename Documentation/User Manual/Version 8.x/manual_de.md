---
title: Morserino-32 Benutzerhandbuch
---

# Vorwort

::: quote
*„Morserino-32 – Ein multifunktionales Morsegerät, ideal zum Lernen und
Trainieren – und zum Spaßhaben."*
:::

Dieses Handbuch beschreibt die Funktionen der Firmware-Version 8.x des
Morserino-32. Diese Firmware-Version ist sowohl für den neuen Morserino
Pocket **(M32Pocket)** als auch für die früheren 1st und 2nd Editions
des Morserino-32 erhältlich. Die Funktionalität ist im Wesentlichen für
alle drei Morserino-Varianten gleich, mit der Ausnahme, dass der
M32Pocket in seiner Standardkonfiguration keine
LoRa-Transceiver-Fähigkeiten besitzt, dafür aber neue Funktionen wie ein
Farbdisplay, Spiele und ein Batteriestatussymbol bietet.

Ich möchte mich bei allen bedanken, die durch Code, Kommentare,
Vorschläge, Kritik, Rezensionen, Blogeinträge, YouTube-Videos und andere
Beiträge dazu beigetragen haben, den Morserino-32 zu einem erfolgreichen
und herausragenden Produkt zu machen. Unter den vielen Mitwirkenden
verdient einer besondere Erwähnung: Hari, OE6HKE – ohne ihn gäbe es den
M32Pocket nicht!

Was ist neu in Version 8?

-   Im Modus Echo Trainer kann jetzt eine Geschwindigkeitsbegrenzung für
    die eigene Eingabe eingestellt werden.
-   Die Generierung zufälliger Rufzeichen wurde verbessert. Es ist nun
    möglich, einen Filter zu setzen, um ausschließlich Rufzeichen eines
    bestimmten Kontinents zu erhalten, oder sehr seltene Präfixe
    auszublenden.
-   Der File Player unterstützt jetzt mehrteilige Dateien – eine
    praktische Möglichkeit, mit einer einzigen hochgeladenen Datei
    mehrere Textteile zu verwalten. Wird eine hochgeladene Textdatei als
    mehrteilig erkannt, fragt der File Player beim Start, welchen Teil
    du verwenden möchtest.
-   Nur für den M32Pocket: Akkustand und Ladestatus werden nun durch
    ein Symbol in der obersten Displayzeile angezeigt, solange du dich
    in einem Menü befindest.
-   Ebenfalls nur für den M32Pocket: Wir haben begonnen, Spiele zu
    implementieren, um das Lernen und Üben des Morsecodes noch
    unterhaltsamer zu gestalten! Das erste Spiel heißt „Morse Invaders":
    Der Spieler muss verhindern, dass die angreifenden Buchstaben vom
    Planeten Morse (oder war es Mars?) den Boden erreichen.

# Anschlüsse und Bedienelemente

## M32Pocket

![](images/m32pocket.jpg)

| # | Anschluss / Bedienelement | Verwendung |
|:---:|---|---|
| 1 | USB-C | Verwende ein normales 5-V-USB-Ladegerät, um das Gerät mit Strom zu versorgen und den LiIon-Akku zu laden. Die Firmware des Mikrocontrollers kann auch über USB neu programmiert werden; eine weitere Möglichkeit, die Firmware zu aktualisieren, ist eine WLAN-Verbindung. Du kannst außerdem getastete oder dekodierte Zeichen auf dem USB-Seriell-Gerät ausgeben, um diese Informationen in einem Computerprogramm zu verwenden – siehe die Einstellung **Serial Output** für weitere Informationen. |
| 2 | 3,5-mm-Klinkenbuchse (3-polig): Externes Paddle | Hier kannst du entweder ein externes (mechanisches) Paddle anschließen (Spitze = linkes Paddle, Ring = rechtes Paddle, Hülse = Masse) oder eine Handtaste (Spitze = Taste). |
| 3 | 3,5-mm-Klinkenbuchse (3-polig): an TX | Verbinde diesen Anschluss mit deinem Sender oder Transceiver, wenn du ihn mit diesem Gerät tasten möchtest. Es werden nur Spitze und Hülse verwendet. |
| 4 | 3,5-mm-Klinkenbuchse (4-polig): Kopfhörer / Audio In / Line Out | Schließe hier deinen Kopfhörer an (mit 3- oder 4-poligem Stecker). Audioeingang für den CW-Decoder; schließe den Audioausgang eines Empfängers zum Dekodieren von CW-Signalen an. Audioausgang (für externe Verstärker oder PC usw.). Die Belegung der Buchse: Spitze und 1. Ring – Audio- oder Kopfhörerausgang; 2. Ring: Masse; Hülse: Audioeingang. Siehe auch Abschnitt **6.2.1 Allgemeine Einstellungen** für verfügbare Einstellungen zu diesem Anschluss! **Schließe diesen Anschluss NICHT mit einem einfachen 3- oder 4-poligen Audiokabel an einen Transceiver an – hohe Spannungen am Audioausgang des Transceivers können den M32Pocket zerstören! Wie du Audio korrekt in den M32Pocket einspeisen kannst, erfährst du in den FAQs auf morserino.info!** |
| 5 | Netzschalter | Schließt den LiIon-Akku an das Gerät an bzw. trennt ihn davon. Bei häufiger Nutzung des Morserino-32 kann der Akku angeschlossen bleiben. Die EIN-Stellung befindet sich in Richtung der Touchpaddles und ist mit einer kleinen Kerbe am Gehäuse markiert. Wenn du das Gerät mehrere Tage nicht benutzt, trenne den Akku (über den Netzschalter), da er sich sonst langsam entlädt. <br> Zum Aufladen muss der Akku angeschlossen sein, d.h. der Schalter muss auf ON stehen! |
| 6 | ENCODER – Drehgeber und sein Druckknopfschalter | Kann gedreht und als Drucktaste gedrückt werden. Dient zur Auswahl in den Menüs, zur Einstellung von Geschwindigkeit und Lautstärke, zum Blättern auf dem Display sowie zur Wahl verschiedener Einstellungen und Optionen. |
| 7 | Touchpaddles | Dies sind die kapazitiven Touchpaddles. Für linkshändige Bedienung kann das Display um 180° gedreht werden! |
| 8 | FN-Tastenschalter (in das Gehäuse integriert) | Wenn das Gerät im Tiefschlaf ist, weckt ein Druck auf diese Taste den Morserino auf und startet ihn neu. Wenn das Gerät in Betrieb ist (in einem der Betriebsmodi), schaltet ein kurzer Druck auf die FN-Taste den Drehgeber zwischen Keyer-Geschwindigkeit und Lautstärkeregelung um. Ein langer Druck auf die FN-Taste ermöglicht das Blättern auf dem Display mit dem Drehgeber; ein erneuter Tastendruck wechselt die Funktion zurück zur Geschwindigkeitssteuerung. Weitere Details findest du im Abschnitt **4.2 Verwendung von ENCODER und FN-Taste**. <br> Ein Doppelklick auf diese Taste verringert die Displayhelligkeit in mehreren Stufen. |



## Morserino-32 2. Edition

![](images/m32_2nd_edition.jpg)

| # | Anschluss / Bedienelement | Verwendung |
|:---:|---|---|
| 1 | 3,5-mm-Klinkenstecker (3-polig): an TX | Verbinde diesen Anschluss mit deinem Sender oder Transceiver, wenn du ihn mit diesem Gerät tasten möchtest. Es werden nur Spitze und Hülse verwendet. |
| 2 | 3,5-mm-Klinkenbuchse (4-polig): Audio In / Line Out | Audioeingang für den CW-Decoder; schließe den Audioausgang eines Empfängers zum Dekodieren von CW-Signalen an. Audioausgang (nahezu eine reine Sinuswelle), der durch die Lautsprecherlautstärke nicht beeinflusst wird. Die Belegung der Buchse: Spitze und 1. Ring – Audioeingang; 2. Ring: Masse; Hülse: Audioausgang. |
| 3 | Audioeingangs-Pegeltrimmer | Stelle den Audioeingangspegel mithilfe dieses Potentiometers ein; es gibt eine spezielle Funktion zur Pegelanpassung, siehe **Anhang 3 Einstellen des Audiopegels**. |
| 4 | 3,5-mm-Klinkenbuchse (3-polig): Kopfhörer | Schließe hier deinen Kopfhörer an (jeder Stereo-Kopfhörer mit Standard-Klinkenstecker von Mobiltelefonen sollte funktionieren), um über Kopfhörer zu hören und den Lautsprecher auszuschalten. Du kannst einen Lautsprecher nicht direkt an diese Buchse anschließen, ohne eine entsprechende Schnittstelle bereitzustellen (der Kopfhörerausgang benötigt eine Gleichstromverbindung zur Masse über 50–300 Ohm). |
| 5 | Kopfhörer-Pegeltrimmer | Damit kannst du den Kopfhörerpegel für maximalen Komfort einstellen. Die 1. Edition des M32 verfügt nicht über diesen Trimmer. |
| 6 | Netzschalter | Schließt den LiPo-Akku an das Gerät an bzw. trennt ihn davon. Bei häufiger Nutzung des Morserino-32 kann der Akku angeschlossen bleiben. Die EIN-Stellung befindet sich auf der Seite des Antennenanschlusses. Wenn du das Gerät mehrere Tage nicht benutzt, trenne den Akku (über den Netzschalter), da er sich sonst langsam entlädt. <br> Zum Aufladen muss der Akku angeschlossen sein, d.h. der Schalter muss auf ON stehen! |
| 7 | SMA-Buchse (Antennenanschluss) | Schließe eine für die Betriebsfrequenz geeignete Antenne an (Standard ca. 433 MHz) für den LoRa-Betrieb. Sende LoRa niemals ohne Antenne oder Kunstantenne! |
| 8 | FN-Taste (ROTE Taste) | Wenn das Gerät im Tiefschlaf ist, weckt ein Druck auf diese Taste den Morserino auf und startet ihn neu. Wenn das Gerät in Betrieb ist (in einem der Betriebsmodi), schaltet ein kurzer Druck auf die FN-Taste den Drehgeber zwischen Keyer-Geschwindigkeit und Lautstärkeregelung um. Ein langer Druck auf die FN-Taste ermöglicht das Blättern auf dem Display mit dem Drehgeber; ein erneuter Druck wechselt die Funktion zurück zur Geschwindigkeitssteuerung. Im Menü startet ein langer Druck den Modus zur Anpassung des Audioeingangspegels. Weitere Details findest du im Abschnitt **4.2 Verwendung von ENCODER und FN-Taste**. <br> Ein Doppelklick auf diese Taste verringert die Displayhelligkeit in mehreren Stufen. |
| 9 | ENCODER – Drehgeber und sein Druckknopfschalter | Kann gedreht und als Drucktaste gedrückt werden. Dient zur Auswahl in den Menüs, zur Einstellung von Geschwindigkeit und Lautstärke, zum Blättern auf dem Display sowie zur Wahl verschiedener Einstellungen und Optionen. |
| 10 | Anschlüsse für Touchpaddles | Diese Platinenanschlüsse nehmen die kapazitiven Touchpaddles auf. Wenn du nur ein externes Paddle verwendest (oder für den Transport), können die Touchpaddles entfernt werden. |
| 11 | Serielle Schnittstelle | Du kannst ein Kabel (direkt oder über eine 4-polige Stiftleiste) an ein externes serielles Gerät anschließen, z.B. ein GPS-Empfängermodul (**dies wird von der aktuellen Firmware nicht unterstützt**). Die 4 Pole sind T (Senden), R (Empfangen), + und − (3,3 V vom Heltec-Modul). |
| 12 | 3,5-mm-Klinkenbuchse (3-polig): Externes Paddle | Hier kannst du entweder ein externes (mechanisches) Paddle anschließen (Spitze = linkes Paddle, Ring = rechtes Paddle, Hülse = Masse) oder eine Handtaste (Spitze = Taste). |
| 13 | Reset-Taste | Durch ein kleines Loch erreichst du die Reset-Taste des Heltec-Moduls (für den normalen Betrieb nicht erforderlich). |
| 14 | USB – Micro-USB-Anschluss | Verwende ein normales 5-V-USB-Ladegerät, um das Gerät mit Strom zu versorgen und den LiPo-Akku zu laden. Die Firmware des Mikrocontrollers kann auch über USB neu programmiert werden. Du kannst außerdem getastete oder dekodierte Zeichen auf dem USB-Seriell-Gerät ausgeben – siehe die Einstellung **Serial Output** für weitere Informationen. |
| 15 | PRG-Taste | Durch ein kleines Loch erreichst du die Programmiertaste des Heltec-Moduls (für den normalen Betrieb nicht erforderlich). |



## Morserino-32 1. Edition

![](images/m32_1st_edition.jpg)

| # | Anschluss / Bedienelement | Verwendung |
|:---:|---|---|
| 1 | 3,5-mm-Klinkenstecker (3-polig): an TX | Verbinde diesen Anschluss mit deinem Sender oder Transceiver, wenn du ihn mit diesem Gerät tasten möchtest. Es werden nur Spitze und Hülse verwendet. |
| 2 | 3,5-mm-Klinkenbuchse (4-polig): Audio In / Line Out | Audioeingang für den CW-Decoder; schließe den Audioausgang eines Empfängers zum Dekodieren von CW-Signalen an. Audioausgang (nahezu eine reine Sinuswelle), der durch die Lautsprecherlautstärke nicht beeinflusst wird. Die Belegung der Buchse: Spitze und 1. Ring – Audioeingang; 2. Ring: Masse; Hülse: Audioausgang. |
| 3 | Audioeingangs-Pegeltrimmer | Stelle den Audioeingangspegel mithilfe dieses Potentiometers ein; es gibt eine spezielle Funktion zur Pegelanpassung, siehe **Anhang 3 Einstellen des Audiopegels**. |
| 4 | 3,5-mm-Klinkenbuchse (3-polig): Kopfhörer | Schließe hier deinen Kopfhörer an (jeder Stereo-Kopfhörer mit Standard-Klinkenstecker von Mobiltelefonen sollte funktionieren), um über Kopfhörer zu hören und den Lautsprecher auszuschalten. Du kannst einen Lautsprecher nicht direkt an diese Buchse anschließen, ohne eine entsprechende Schnittstelle bereitzustellen (der Kopfhörerausgang benötigt eine Gleichstromverbindung zur Masse über 50–300 Ohm). |
| 5 | Netzschalter | Schließt den LiPo-Akku an das Gerät an bzw. trennt ihn davon. Bei häufiger Nutzung des Morserino-32 kann der Akku angeschlossen bleiben. Die EIN-Stellung befindet sich auf der Seite des Antennenanschlusses. Wenn du das Gerät mehrere Tage nicht benutzt, trenne den Akku (über den Netzschalter), da er sich sonst langsam entlädt. <br> Zum Aufladen muss der Akku angeschlossen sein, d.h. der Schalter muss auf ON stehen! |
| 6 | SMA-Buchse (Antennenanschluss) | Schließe eine für die Betriebsfrequenz geeignete Antenne an (Standard ca. 433 MHz) für den LoRa-Betrieb. Sende LoRa niemals ohne Antenne oder Kunstantenne! |
| 7 | FN-Taste (ROTE Taste) | Wenn das Gerät im Tiefschlaf ist, weckt ein Druck auf diese Taste den Morserino auf und startet ihn neu. Wenn das Gerät in Betrieb ist (in einem der Betriebsmodi), schaltet ein kurzer Druck auf die FN-Taste den Drehgeber zwischen Keyer-Geschwindigkeit und Lautstärkeregelung um. Ein langer Druck auf die FN-Taste ermöglicht das Blättern auf dem Display mit dem Drehgeber; ein erneuter Druck wechselt die Funktion zurück zur Geschwindigkeitssteuerung. Im Menü startet ein langer Druck den Modus zur Anpassung des Audioeingangspegels. Weitere Details findest du im Abschnitt **4.2 Verwendung von ENCODER und FN-Taste**. <br> Ein Doppelklick auf diese Taste verringert die Displayhelligkeit in mehreren Stufen. |
| 8 | ENCODER – Drehgeber und sein Druckknopfschalter | Kann gedreht und als Drucktaste gedrückt werden. Dient zur Auswahl in den Menüs, zur Einstellung von Geschwindigkeit und Lautstärke, zum Blättern auf dem Display sowie zur Wahl verschiedener Einstellungen und Optionen. |
| 9 | Anschlüsse für Touchpaddles | Diese Platinenanschlüsse nehmen die kapazitiven Touchpaddles auf. Wenn du nur ein externes Paddle verwendest (oder für den Transport), können die Touchpaddles entfernt werden. |
| 10 | 3,5-mm-Klinkenbuchse (3-polig): Externes Paddle | Hier kannst du entweder ein externes (mechanisches) Paddle anschließen (Spitze = linkes Paddle, Ring = rechtes Paddle, Hülse = Masse) oder eine Handtaste (Spitze = Taste). |
| 11 | Serielle Schnittstelle | Du kannst ein Kabel (direkt oder über eine 4-polige Stiftleiste) an ein externes serielles Gerät anschließen, z.B. ein GPS-Empfängermodul (**dies wird von der aktuellen Firmware nicht unterstützt**). Die 4 Pole sind T (Senden), R (Empfangen), + und − (3,3 V vom Heltec-Modul). |
| 12 | Reset-Taste | Durch ein kleines Loch erreichst du die Reset-Taste des Heltec-Moduls (für den normalen Betrieb nicht erforderlich). |
| 13 | USB – Micro-USB-Anschluss | Verwende ein normales 5-V-USB-Ladegerät, um das Gerät mit Strom zu versorgen und den LiPo-Akku zu laden. Die Firmware des Mikrocontrollers kann auch über USB neu programmiert werden. Du kannst außerdem getastete oder dekodierte Zeichen auf dem USB-Seriell-Gerät ausgeben – siehe die Einstellung **Serial Output** für weitere Informationen. |
| 14 | PRG-Taste | Durch ein kleines Loch erreichst du die Programmiertaste des Heltec-Moduls (für den normalen Betrieb nicht erforderlich). |

# Kurzanleitung zur Benutzung des M32

::: important
**Diese Anleitung ist für Ungeduldige gedacht, ersetzt aber nicht das
Lesen des gesamten Handbuchs!**
:::

::: important
Achte darauf, dass du einen geeigneten Akku für deinen Morserino
verwendest: M32 1st und 2nd Edition benötigen einen 3,7-V-LiPo-Akku,
der M32Pocket verwendet eine 14500-Li-Ion-Zelle mit 3,7 V.
:::

**Zu verwendende Bedienelemente:**

• EIN/AUS (Akku): Der Schiebeschalter befindet sich auf der Rückseite
in der Nähe des Lautsprechers. Verbindet bzw. trennt den Akku.

• ENCODER: Der schwarze Drehknopf, den du drehen und drücken kannst.

• FN: Der andere Druckknopfschalter (rot bei der 1st und 2nd Edition
des Morserinos; beim M32Pocket in das Gehäuse integriert).

**So schaltest du den M32 ein:**

Schließe entweder ein USB-Netzteil an, oder, wenn du einen Akku
eingebaut hast, stelle den Akkuschalter auf ON (I).

Es erscheint kurz ein Startbildschirm, der die Firmware-Version und
den Akkustatus anzeigt. Danach befindest du dich im Hauptmenü (Display:
„**Select Mode:**"), es sei denn, du hast die Einstellung **Quick Start**
aktiviert; in diesem Fall startet automatisch der zuletzt gewählte
Betriebsmodus.

Wenn sich nach dem Einschalten des M32 lange Zeit nichts auf dem
Display ändert, wechselt er in den Schlafmodus. Du kannst ihn aufwecken,
indem du die FN-Taste drückst.

**So wählst du einen Modus (Menü):**

Drehe den ENCODER, um den gewünschten Modus zu finden. Klicke auf den
ENCODER, um ihn auszuwählen oder die nächsttiefere Menüebene aufzurufen.
Drücke und halte den ENCODER, um das Menü zu verlassen oder eine Ebene
höher zu gehen.

**So änderst du Geschwindigkeit oder Lautstärke und blätterst im Display:**

Dies geschieht mit FN und dem ENCODER, wenn du dich in einem der
Betriebsmodi befindest. Im Menü funktioniert das nicht.

-   Geschwindigkeit ändern: ENCODER drehen.
-   Lautstärke ändern: FN klicken, dann ENCODER drehen, um die
    Lautstärke anzupassen, FN erneut klicken, um zur
    Geschwindigkeitseinstellung zurückzukehren.
-   Display blättern: FN drücken und halten. Mit dem ENCODER vor- und
    zurückblättern. FN klicken, um den Scroll-Modus zu beenden.

Die Geschwindigkeitseinstellung wird in den permanenten Speicher
geschrieben, nachdem 12 Zeichen mit der gleichen Geschwindigkeit
eingegeben wurden. Die Lautstärkeeinstellung wird geschrieben, sobald du
mit der FN-Taste vom Lautstärke- in den Geschwindigkeitsmodus zurückschaltest.

**So änderst du die Displayhelligkeit:**

Es gibt fünf Helligkeitsstufen. Mit jedem Doppelklick auf die FN-Taste
wird die Helligkeit etwas verringert. Wenn die niedrigste Stufe erreicht
ist, setzt ein Doppelklick das Display auf volle Helligkeit zurück.

**So änderst du Einstellungen:**

Doppelklicke auf den ENCODER und drehe ihn dann, um die gewünschte
Einstellung auszuwählen. Drücke und halte den ENCODER, um das
Einstellungsmenü zu verlassen.

Wenn ein Modus aktiv ist, werden nur die für diesen Modus relevanten
Einstellungen angezeigt. Wenn du aus einem Menü heraus doppelklickst,
werden alle Einstellungen angezeigt.

Es gibt zahlreiche Einstellungen; was sie bewirken, erfährst du im
Abschnitt **6 Einstellungen**.

Du kannst deine Einstellungen auch in sogenannten „Schnappschüssen"
(englisch: Snapshots) speichern und abrufen, siehe Abschnitt
**6.1 Schnappschüsse**.

**Externe Paddles und Tasten verwenden:**

Über den 3,5-mm-Anschluss für externe Tasten kannst du externe Paddles
(Doppel- oder Einhebel) oder eine Handtaste (normal oder Sideswiper) an
deinen M32 anschließen.

Um eine Handtaste zu verwenden, kannst du den Modus CW Decoder benutzen,
ohne irgendwelche Einstellungen zu ändern. Dieser Modus dekodiert Morse
entweder über den Audio-I/O-Anschluss oder über deine Taste. Um die Modi
Echo Trainer oder Transceiver mit einer Handtaste zu verwenden, ändere
die Einstellung **Keyer Mode** auf **Straight Key**.

Beachte, dass der Modus CW Keyer mit einer Handtaste anders funktioniert:
Er verhält sich genauso wie im Decoder-Modus – mit einer Handtaste bist
DU der Keyer, nicht der Morserino!

Du kannst die eingebauten kapazitiven Touchpaddles wie einen Sideswiper /
Cootie Key verwenden, wenn **Keyer Mode** auf **Straight Key** eingestellt
ist (aber kein externes Paddle, außer es ist als Sideswiper verdrahtet)!

**So lädst du den Akku auf:**

Schließe USB an und stelle den Akkuschalter auf ON (I).

Bei der M32 1st oder 2nd Edition leuchtet eine orangefarbene LED sehr
hell auf. Wenn die LED dunkel ist, ist der Akku vollständig aufgeladen.
Wenn sie leuchtet oder schwach flackert, ist der Akku nicht angeschlossen
oder eingeschaltet.

Der M32Pocket zeigt den Akkustatus als Symbol an, solange du dich in
einem Menü befindest.

# Benutzung, Schritt für Schritt

## Ein- und Ausschalten / Aufladen des Akkus

Wenn du das Gerät mit USB-Strom betreiben möchtest, schließe einfach ein
USB-Kabel von praktisch jedem USB-Ladegerät an (das Gerät verbraucht beim
Laden des Akkus maximal 200 mA, beim M32Pocket maximal 500 mA – jedes
5-V-Ladegerät reicht also aus).

Wenn du das Gerät mit Akkustrom betreibst, schiebe den Schiebeschalter
auf die Position ON.

::: important
Achte darauf, dass du den Akku mit der richtigen Polarität einlegst,
bevor du den Schalter auf ON schiebst. Eine falsche Polung kann deinen
Morserino zerstören! Für den M32Pocket empfiehlt sich eine 14500
Li-Ion-Zelle (3,7 V) mit Tiefentladeschutz.
:::

Wenn das Gerät ausgeschaltet ist, aber der Akku angeschlossen ist (der
Schiebeschalter steht auf ON), befindet es sich im Tiefschlaf – in
Wirklichkeit sind fast alle Funktionen des Mikrocontrollers ausgeschaltet
und der Stromverbrauch ist minimal (weniger als 5 % des normalen Betriebs
bei M32 1st oder 2nd Edition, etwa 1 % beim M32Pocket).

**Um das Gerät aus dem Tiefschlaf einzuschalten**, drücke kurz die
FN-Taste.

Wenn der Morserino-32 hochfährt, erscheint für einige Sekunden ein
Startbildschirm. In der obersten Zeile wird die LoRa-Frequenz angezeigt,
für die der M32 konfiguriert ist (als fünfstellige Zahl; nicht auf dem
M32Pocket). Unten auf dem Display siehst du eine Anzeige, wie viel
Akkuleistung noch vorhanden ist (nicht auf dem M32Pocket). Wenn der Akku
schwach ist, schließe dein Gerät an eine USB-Stromquelle an. Der Akku
entlädt sich auch dann, wenn du das Gerät nie einschaltest. Obwohl die
Entladung im Tiefschlafmodus minimal ist, ist der Akku nach einigen Tagen
leer. Wenn du also vorhast, den Morserino über einen längeren Zeitraum
nicht zu benutzen, trenne den Akku mit dem Netzschalter auf der Rückseite
des Geräts.

Wenn die Akkuspannung beim Einschalten gefährlich niedrig ist, erscheint
auf dem Bildschirm das Symbol für einen leeren Akku und das Gerät lässt
sich nicht hochfahren. Wenn du dieses Symbol siehst, lade den Akku
so bald wie möglich auf.

Nur bei der 1st Edition des M32: Nach der Nutzung einer der
WLAN-Funktionen funktioniert die Akkumessung erst nach einem erneuten
Aus- und Einschalten des Morserino-32 (oder einem Reset mit der
Reset-Taste) wieder korrekt. Dies ist auf ein Hardwareproblem auf dem
Heltec-Board V2.0 zurückzuführen. In solchen Fällen zeigt der
Morserino-32 statt der Akkuspannung „Unknown" an, und das Akkusymbol
erscheint mit einem Fragezeichen. Nach einem Neustart sollte alles wieder
normal funktionieren.

Wenn das Display das Symbol für einen leeren Akku anzeigt, obwohl noch
ausreichend Ladung vorhanden sein sollte, empfiehlt sich eine Kalibrierung
der Akkumessung. Beim M32Pocket ist dies nicht notwendig und daher auch
nicht verfügbar. Siehe **Anhang 7.1.2: Kalibrierung der Akkumessung**.

Um das Gerät vom Akku zu trennen (und es damit auszuschalten, sofern es
nicht per USB versorgt wird), schiebe den Schiebeschalter auf OFF.

Um das Gerät in den Tiefschlaf zu versetzen, gibt es zwei Möglichkeiten:

-   Wähle im Hauptmenü die Option **Go To Sleep**
-   Wenn in den Einstellungen ein Wert für **Time Out** gesetzt wurde,
    tue nichts. Wenn das Display nicht aktualisiert wird, schaltet sich
    das Gerät nach Ablauf der eingestellten Zeit aus und geht in den
    Tiefschlaf.

**Um den Akku aufzuladen**, schließe das Gerät mit einem USB-Kabel an
eine zuverlässige 5-V-USB-Stromquelle an, z.B. deinen Computer oder ein
USB-Ladegerät wie dein Handy-Ladegerät.

::: important
Achte darauf, dass der Netzschalter des Geräts während des Ladevorgangs
auf ON steht – wenn du den Akku über den Schalter trennst, kann er nicht
geladen werden.
:::

Wenn du einen M32 der 1st oder 2nd Edition auflädst, leuchtet die
orangefarbene LED auf dem ESP32-Modul hell. Wenn der Akku abgetrennt ist,
leuchtet diese LED nicht hell, sondern blinkt nervös oder leuchtet nur
halb. Sobald der Akku vollständig aufgeladen ist, erlischt die orange LED.

Du kannst das Gerät natürlich jederzeit verwenden, wenn es per USB
versorgt wird, egal ob der Akku gerade geladen wird oder nicht.

::: important
Um eine Tiefentladung des Akkus zu verhindern, schalte den Morserino-32
immer über den Hauptschiebeschalter aus. Lass ihn nicht für längere Zeit
im Schlafmodus. Ein paar Tage sind in Ordnung, wenn der Akku voll
geladen ist.
:::

An Bord befindet sich eine Schaltung zum Laden des Akkus, die eine
Überladung zuverlässig verhindert. Sie bietet jedoch keinen Schutz vor
Tiefentladung (dies gilt für Morserinos der 1st und 2nd Edition; der
M32Pocket bietet ebenfalls Schutz vor Tiefentladung)!

::: warning
**Eine Tiefentladung führt zu verminderter Akkukapazität und
schließlich zum vorzeitigen Tod des Akkus!**
:::

## Verwendung von ENCODER und FN-Taste

Die Auswahl der verschiedenen Modi und das Ändern der Einstellungen
erfolgt über den **Dreh-ENCODER** und seine **Drucktastenfunktion**.

**Drehen** des ENCODERs führt dich durch die Optionen oder Werte; ein
**einmaliges Klicken** des ENCODER-Knopfs wählt eine Option oder einen
Wert aus, oder bringt dich auf die nächste Ebene des Menüs (es gibt bis
zu drei Ebenen im Menü).

Ein **Doppelklick** auf den ENCODER bringt dich in das Einstellungsmenü.
Wenn du dies aus dem Menü heraus tust, kannst du alle Einstellungen
ändern. Wenn du es aus einem aktiven Betriebsmodus heraus tust, werden nur
die für den aktuellen Modus relevanten Einstellungen angezeigt und können
geändert werden.

Ein **langer Druck** bringt dich aus einem beliebigen Modus zurück ins
Menü und befördert dich innerhalb des Menüs eine Ebene höher.

Ein **Doppelklick auf die FN-Taste** verringert die Displayhelligkeit;
es gibt fünf Helligkeitsstufen. Sobald die niedrigste Stufe erreicht ist,
setzt ein Doppelklick das Display auf volle Helligkeit zurück.

Nur bei M32 1st und 2nd Edition: Während du ein Menü aufrufst (z.B.
direkt nach dem Einschalten), startet ein **langer Druck auf die FN-Taste**
eine Funktion zum Einstellen des Audioeingangspegels (und ggf. des
Ausgangspegels an einem Gerät, das du an den Line-Out-Anschluss des
Morserino-32 angeschlossen hast).<br>
Beim **M32Pocket** tastet ein langer Druck auf die FN-Taste lediglich den
Sender (sofern einer angeschlossen ist) und erzeugt einen Mithörton (was
z.B. zum Einstellen des Audiopegels an einem angeschlossenen Computer
nützlich sein kann).

Ein Klick auf die FN-Taste schaltet diese Funktion wieder aus. Siehe
**Anhang 3 Einstellen des Audiopegels**.

Während einer der Betriebsmodi (Keyer, Generator, Echo Trainer usw.)
läuft, ermöglicht die FN-Taste das **schnelle Umschalten zwischen
Geschwindigkeits- und Lautstärkeregelung** mit einem einzigen Klick.

Ein **langer Klick auf die FN-Taste während ein Modus aktiv ist**
schaltet Display und Drehgeber in den Scroll-Modus (das Display hat
einen Puffer von 15 Zeilen, von denen normalerweise nur die unteren drei
sichtbar sind; im Scroll-Modus kannst du zu den vorherigen Zeilen
zurückblättern; dabei wird am rechten Rand des Displays eine Bildlaufleiste
angezeigt, die ungefähr anzeigt, wo du dich innerhalb der 15 Textzeilen
befindest). Ein erneuter Klick auf FN im Scroll-Modus kehrt zum normalen
Betrieb zurück und bringt den Drehgeber wieder zur Geschwindigkeitssteuerung.

Wenn du dich **im Einstellungsmenü** befindest, ruft ein **kurzer Klick
auf die FN-Taste** einen gespeicherten Schnappschuss ab, und ein **langer
Druck auf die FN-Taste** speichert einen Schnappschuss. Weitere Details
findest du im Abschnitt **6.1 Schnappschüsse**.

## Das Display

Das Display ist in zwei Hauptbereiche unterteilt: Oben befindet sich die
**Statuszeile**, die wichtige Informationen zum aktuellen Zustand des
Geräts anzeigt, und darunter ein Bereich mit **drei Laufzeilen**, in
dem die generierten Morsezeichen im Klartext angezeigt werden. Alle
Morsezeichen werden zur besseren Lesbarkeit in *Kleinbuchstaben*
dargestellt (es gibt aber auch eine Einstellung, um sie in
*GROSSBUCHSTABEN* umzuwandeln). Betriebszeichen werden als Buchstaben in
spitzen Klammern dargestellt, z.B. *\<ka>* oder *\<sk>*. Außerdem
wird im Modus Echo Trainer (siehe unten) das Ergebnis deines Versuchs,
den richtigen Morsecode einzugeben, als *ERR* oder *OK* angezeigt
(zusammen mit akustischen Signalen).

Obwohl nur drei Laufzeilen angezeigt werden, steht intern ein Puffer
von 15 Zeilen zur Verfügung. Durch Drücken und Halten der FN-Taste kannst
du mit dem Drehgeber zurückblättern und frühere Zeilen sichtbar machen.
Dies funktioniert in jedem Modus mit Bildschirmausgabe. Es geht nichts
verloren, und die Anzeige kehrt zum normalen Verhalten zurück, sobald du
den Scroll-Modus verlässt.

**Die Statuszeile**

Wenn dir ein Menü angezeigt wird (entweder das Startmenü oder ein
Einstellungsmenü), zeigt die Statuszeile an, was zu tun ist
(**Select Mode:** oder **Set Preferences:**).

![](images/status_line.png)

::: note
Dieses Bild zeigt die Statuszeile der älteren Morserino-Modelle. Der neue
M32Pocket ist ähnlich, kann unter bestimmten Umständen aber zusätzliche
Informationen anzeigen.
:::

Im Modus CW Keyer, CW Generator oder Echo Trainer zeigt die Statuszeile
von links nach rechts Folgendes an:

-   **A**, **B**, **U**, **N** oder **S**, was den automatischen
    Keyer-Modus angibt: Iambic **A**, Iambic **B**, **U**ltimatic,
    **N**on-Squeeze oder **S**traight Key (= Handtaste; Details zu
    diesen Modi findest du im Abschnitt **5.1 CW Keyer**).

::: note
Im Modus CW Decoder erscheint der Buchstabe **d** an dieser Stelle, und
immer wenn ein Signal erkannt wird, wird links davon ein kleines Quadrat
angezeigt.
:::

-   Die aktuell eingestellte **Geschwindigkeit** in Wörtern pro Minute
    (das Referenzwort ist das Wort PARIS, was bedeutet, dass 1 WpM
    5 Zeichen pro Minute entspricht). Im Modus CW Keyer als **nnWpM**,
    im Modus CW Generator oder Echo Trainer als **(nn) nnWpM**. Der
    *Wert in Klammern* gibt die effektive Geschwindigkeit an, die
    abweicht, wenn der Wort- oder Zeichenabstand auf andere Werte als
    die Norm gesetzt ist (3 Dits für den Zeichenabstand, 7 Dits für den
    Wortabstand). Siehe die Hinweise im Abschnitt **5.1 CW Keyer** zu
    den Einstellungen, die du im Keyer-Modus vornehmen kannst.

::: note
Im Transceiver-Modus siehst du zwei Geschwindigkeitswerte – der Wert in
Klammern gibt die Geschwindigkeit des empfangenen Signals an, der andere
die Geschwindigkeit deines Keyers.

Bei Verwendung einer Handtaste zeigt die Geschwindigkeit an, wie schnell
du tatsächlich gibst.

Wenn die Ziffern, die die Geschwindigkeit angeben, **fett** dargestellt
sind, kannst du durch Drehen des Drehgebers die Geschwindigkeit ändern.
Wenn sie in normaler Schrift angezeigt werden, ändert das Drehen des
Drehgebers die Lautstärke.
:::

-   Ein horizontaler „Fortschrittsbalken", der sich von links nach
    rechts erstreckt, zeigt die **Lautstärke** des vom Gerät erzeugten
    Mithörtons an (volle Länge des Balkens = maximale Lautstärke).
    Normalerweise ist ein weißer Rahmen um den schwarzen Balken zu sehen
    (eine Erweiterung der restlichen Statuszeile); ist dies umgekehrt
    (weißer Balken in schwarzer Umgebung – und die WpM-Ziffern sind
    nicht fett), ändert das Drehen des Drehgebers die Lautstärke und
    nicht die Geschwindigkeit.
-   Ein Symbol für Funkübertragung erscheint ganz rechts in der
    Statuszeile, wenn ein Funkmodus aktiv ist (wenn sich der
    Morserino-32 im LoRa- oder WLAN-Transceiver-Modus befindet oder
    wenn du in einem der CW-Generator-Modi eingestellt hast, dass über
    LoRa oder WLAN gesendet werden soll). Das WLAN-Symbol sieht ähnlich
    aus wie das übliche WiFi-Logo (ein 90-Grad-Sektor aus konzentrischen
    Kreisen); das LoRa-Symbol besteht aus konzentrischen Halbkreisen.

## Hardware-Einstellungen

Möglicherweise musst du bestimmte Hardware-Einstellungen ändern, z.B.
die Displayausrichtung oder die Kalibrierung der Akkumessung. Oder du
möchtest (fast) alle Einstellungen auf die Werkseinstellungen
zurücksetzen. All diese Einstellungen werden im
**Anhang 1 Hardware-Konfigurationsmenü** behandelt.

# Hauptmenü und Morserino-Modi

Du wählst den Betriebsmodus deines Morserino-32, indem du den
ENCODER-Knopf drehst und kurz darauf klickst, um den gewünschten Modus
(oder in einigen Fällen ein Untermenü für eine detailliertere Auswahl)
auszuwählen, wenn das Menü auf dem Display erscheint.

## CW Keyer

Dies ist ein automatischer Keyer, der die Modi Iambic A, Iambic B (auch
Curtis A und Curtis B genannt) und Ultimatic sowie den Non-Squeeze-Modus
(Emulation einer Einhebeltaste mit einem Doppelhebel-Paddle) unterstützt.
Du kannst entweder das eingebaute kapazitive Paddle verwenden oder ein
externes Paddle (Doppel- oder Einhebel-Paddle) anschließen. Interne und
externe Paddles arbeiten parallel, daher ist keine zusätzliche
Konfiguration notwendig.

::: note
Es gibt eine Reihe von **Einstellungen**, die bestimmen, wie der
automatische Keyer funktioniert. Einzelheiten findest du im Abschnitt
**6.2.2 Einstellungen zu Key, Paddles und Keyer**. Besonders wichtig
sind folgende:

**External Pol.**: Wenn dein externes Paddle „falsch herum" verkabelt
ist, kannst du das hier korrigieren.

**Paddle Polarity**: Auf welcher Seite sollen die Dits, auf welcher
die Dahs sein?

**Keyer Mode**: Wähle Iambic A oder B, Ultimatic, Non-Squeeze oder
Straight Key (Handtaste).
:::

Was sind diese **iambischen Modi**? Wenn beide Paddles eines iambischen
Keyers gedrückt werden, werden abwechselnd Dahs und Dits erzeugt,
beginnend mit dem zuerst gedrückten Paddle. Der Name „iambisch" kommt
daher, dass in einem Iambus abwechselnd kurze und lange Silben
vorkommen. Der Name „Curtis" – manchmal ebenfalls für „Iambic"
verwendet – geht auf den Entwickler des bahnbrechenden Curtis-Morse-
Keyer-Chips zurück: John G. „Jack" Curtis, K6KU (ex W3NSJ).

Der Unterschied zwischen Modus A und B liegt im Verhalten, wenn beide
Paddles losgelassen werden, während das aktuelle Element noch erzeugt
wird. Im Modus A hält der Keyer nach dem aktuellen Element an. Im
Modus B fügt er ein weiteres Element entgegengesetzt zum aktuellen
hinzu.

Konkret: Im Modus Curtis B (Iambic B) wird das gegenüberliegende Paddle
geprüft, während das aktuelle Element (Dit oder Dah) ausgegeben wird.
Wird in dieser Zeit ein Paddle gedrückt, wird ein zusätzliches,
entgegengesetztes Element hinzugefügt – im Modus A nicht so. Da Modus B
schwierig zu bedienen ist, wurde er später so geändert, dass die Paddles
erst nach einem bestimmten Prozentsatz der Elementdauer geprüft werden.
Diesen Prozentsatz kannst du mit den Einstellungen **CurtisB DahT%**
und **CurtisB DitT%** festlegen.

Bei 0 (niedrigster Wert) entspricht der Modus dem ursprünglichen Curtis
B; der später entwickelte „verbesserte" Curtis B verwendet etwa
35–40 %. Bei 100 (höchster Wert) verhält sich der Modus wie Curtis A.

Diese Einstellung erlaubt es dir, jedes beliebige Verhalten zwischen
Curtis A und dem ursprünglichen Curtis B auf einer stufenlosen Skala
einzustellen; du kannst den Prozentsatz für Dits und Dahs getrennt
festlegen (sinnvoll, da die Zeitdauer für Dits nur ein Drittel der von
Dahs beträgt).

**Ultimatic-Modus**: Durch Drücken beider Paddles wird ein Dit oder Dah
erzeugt. Was zuerst kommt, hängt davon ab, welches Paddle zuerst
gedrückt wurde. Danach wird kontinuierlich der entgegengesetzte Ton
erzeugt. Vorteilhaft für Zeichen wie J, B, 1, 2, 6 und 7. Dieser
Modus reagiert auch auf das gegenüberliegende Paddle mit denselben
Zeiteinstellungen wie Iambic B.

**Non-Squeeze-Modus**: Simuliert das Verhalten einer Einhebeltaste bei
Verwendung eines Doppelhebel-Paddles. Bediener, die an die Einhebeltaste
gewöhnt sind, drücken das Doppelpaddle manchmal versehentlich zusammen,
besonders bei höheren Geschwindigkeiten. Non-Squeeze ignoriert das
Zusammendrücken und erleichtert so die Umstellung.

::: note
Die Modi Iambic und Ultimatic können nur mit dem eingebauten Touchpaddle
oder einem externen Doppelhebel-Paddle verwendet werden. Bei einem
externen Einhebel-Paddle ist die Modusauswahl irrelevant.
:::

Die Einstellung **Latency** legt fest, wie lange die Paddles „taub"
sind, nachdem das aktuelle Element (Punkt oder Strich) erzeugt wurde.
In früheren Firmware-Versionen war dieser Wert 0, was dazu führte, dass
mehr Dits als beabsichtigt erzeugt wurden. Du kannst den Wert nun
zwischen 0 und 7 einstellen (0/8 bis 7/8 einer Punktlänge). Der
Standardwert ist 4 (halbe Punktlänge). Wenn du noch unerwünschte Dits
erzeugst, erhöhe diesen Wert.

Zur Einstellung **AutoChar Spce** (Mindestlänge für den
Zeichenzwischenraum) siehe Abschnitt **6.2.2 Einstellungen zu Key,
Paddles und Keyer**.

**Straight Key-Modus**: Dies ist eigentlich kein automatischer
Keyer-Modus, ermöglicht aber die Verwendung einer einfachen Handtaste.

Wenn du eine Handtaste angeschlossen und **Keyer Mode** auf
**Straight Key** eingestellt hast, kannst du alle Morserino-Modi (z.B.
Echo Trainer) nutzen. Der Modus CW Keyer verhält sich dabei jedoch
anders (wie der Decoder-Modus): Mit einer Handtaste bist DU der Keyer,
nicht der Morserino!

### CW Memory Keyer

Ab Version 5.1 verfügt der Morserino über einen Memory Keyer. Es stehen
acht Speicherplätze zur Verfügung, die jeweils bis zu 47 Zeichen
enthalten können. Neben Standard-Morsezeichen (Buchstaben, Zahlen,
Satzzeichen) können auch Betriebszeichen und ein Pausenzeichen
gespeichert werden. Die Textdarstellung von Betriebszeichen und
Pausenmarkierung findest du unter **Kodierung von Textdateien** im
Abschnitt **5.2.1 Was kann generiert werden?**

Gespeicherte Inhalte können in den Modi **CW Keyer** und **iCW/Ext
Trx** abgerufen werden (aus technischen Gründen jedoch nicht in den
Modi **WiFi Trx** oder **LoRa Trx**). **Um einen Speicher abzurufen,
klicke einmal kurz auf den ENCODER-Knopf.** Wenn Speicher definiert
wurden, kannst du in der obersten Zeile mit dem Drehgeber durch sie
blättern. Es gibt auch eine EXIT-Option, falls du es dir anders
überlegst. Wenn keine Speicherplätze definiert wurden, erscheint eine
Fehlermeldung.

Die Speicher 1 und 2 werden endlos wiederholt, bis du sie durch manuelle
Eingabe stoppst; alle anderen Speicher werden einmal abgespielt.

#### Definition von Speicherplätzen {-}

Die Speicher können nur über das serielle Protokoll definiert werden –
entweder über eine Computersoftware, die dieses Protokoll implementiert,
oder manuell über ein Terminalprogramm. (Das serielle Protokoll ist in
einem separaten Dokument beschrieben.)

Der Befehl zur Definition eines Speichers lautet:

`PUT cw/store/<n>/<Inhalt>`

Damit wird \<Inhalt> in Speicherplatz Nummer \<n> (n = 1 ... 8)
gespeichert; wenn \<Inhalt> ein leerer String ist, wird dieser Speicher
gelöscht. \<Inhalt> kann normale Morsezeichen, Betriebszeichen (z.B.
„\<bk>") und auch „\[p\]" oder „\\p" für eine Pause enthalten.

Bei der manuellen Methode über ein Terminal muss das serielle Protokoll
zunächst mit dem Befehl `PUT device/protocol/on` eingeleitet und
abschließend mit `PUT device/protocol/off` beendet werden.

::: note
Viel einfacher ist es, den Morserino über USB mit einem Computer zu
verbinden und den Anweisungen in **Anhang 7 Einrichten von
M32-Einstellungen über einen Browser** zu folgen.
:::

## CW Generator

Der CW Generator erzeugt entweder zufällige Zeichengruppen und Wörter
für das CW-Training oder spielt den Inhalt einer Textdatei im Morsecode
ab. Über entsprechende Einstellungen kannst du zahlreiche Optionen
festlegen (siehe Abschnitt **6.2.4 Einstellungen zur CW-Generierung**).

Du kannst den CW Generator **starten** und **stoppen**, indem du
**schnell ein Paddle drückst** (auf einer oder beiden Seiten) oder indem
du **den ENCODER-Knopf klickst** (bei Verwendung einer Handtaste kannst
du auch diese zum Starten und Stoppen verwenden).

Beim Start wird zunächst *vvv\<ka>*
(**\...\_  \...\_  \...\_  \_.\_.\_**) im Morsecode als Warnsignal
ausgegeben, bevor die eigentliche Generierung von Gruppen oder Wörtern
beginnt.

Durch Aktivieren der Einstellung **Stop/Next/Rep** spielt der Morserino
jeweils nur ein Wort oder eine Zeichengruppe ab und wartet dann auf eine
Paddle-Eingabe. Ein Druck auf das linke Paddle wiederholt das aktuelle
Wort, ein Druck auf das rechte Paddle gibt das nächste Wort aus. Dies
ist nützlich zum Training des Hörens ohne Mitschreiben: Lass ein Wort
abspielen, ohne auf das Display zu schauen, und versuche, es im Kopf zu
dekodieren. Wenn du dir nicht sicher bist, drücke das linke Paddle zur
Wiederholung; wenn du glaubst, es richtig zu haben, überprüfe es am
Display. (Die Funktion der Paddles entspricht typischen
Musikplayer-Tasten – links = zurück, rechts = vorwärts.)

::: note
Die Optionen **Each Word 2x** und **Stop/Next/Rep** (siehe Abschnitt
**6.2.4 Einstellungen zur CW-Generierung**) sind nicht miteinander
kompatibel. Wenn du eine Option auf ON setzt, wird die andere
automatisch auf OFF gesetzt.
:::

Sobald du ein Paddle berührst, zeigt das Display, was gerade gespielt
wurde, damit du prüfen kannst, ob du es richtig dekodiert hast. Ein
erneutes Berühren des Paddles spielt das nächste Wort ab.

Normalerweise läuft der Morserino-32 weiter, bis du ihn manuell
anhältst, aber es gibt eine Einstellung, die das Gerät nach einer
bestimmten Anzahl von Wörtern (oder Buchstabengruppen) pausieren lässt.
Siehe **Max \# of Words** im Abschnitt **6.2.4 Einstellungen zur
CW-Generierung**.

### Was kann generiert werden?

Auf der zweiten Menüebene kannst du zwischen folgenden Möglichkeiten
wählen:

-   **Random**: Erzeugt Gruppen von zufälligen Zeichen. Länge der
    Gruppen und Zeichenauswahl können in den Einstellungen festgelegt
    werden (Details in der Beschreibung der Einstellungen).
-   **CW Abbrevs**: Zufällige Abkürzungen und Q-Gruppen, die im
    CW-Verkehr sehr häufig vorkommen (über eine Einstellung kannst du
    die maximale Länge der zu trainierenden Abkürzungen festlegen). Siehe
    **Anhang 10 Gängige CW-Abkürzungen, verwendet vom Morserino-32** für
    die generierbaren Abkürzungen und ihre Bedeutung.
-   **English Words**: Zufällige Wörter aus einer Liste der 5.000
    gebräuchlichsten englischen Wörter (auch hier kann die maximale
    Wortlänge über eine Einstellung festgelegt werden).
-   **Call Signs**: Generiert Amateurfunk-Rufzeichen (dies sind nicht
    immer echte Rufzeichen; es werden auch solche generiert, die in der
    Realität nicht existieren würden, da bestimmte Suffixe möglicherweise
    noch niemandem zugeteilt wurden). Die maximale Länge kann über eine
    Einstellung gewählt werden, ebenso – wenn gewünscht – der Kontinent
    der generierten Rufzeichen sowie ob sehr seltene Präfixe ausgeblendet
    werden sollen.
-   **Mixed**: Wählt zufällig aus den oben genannten Möglichkeiten
    (zufällige Zeichengruppen, Abkürzungen, englische Wörter und
    Rufzeichen).
-   **File Player**: Spielt den Inhalt einer auf den Morserino-32
    hochgeladenen Datei im Morsecode ab. Derzeit kann nur eine Datei
    gespeichert werden; sobald du eine neue hochlädst, wird die alte
    überschrieben. Deine Datei kann jedoch **bis zu 16 Teile** enthalten
    (siehe unten zur Kodierung und Erstellung mehrteiliger Dateien), und
    du kannst auswählen, welchen Teil du verwenden möchtest!<br>
    **Das Hochladen** erfolgt über WLAN von deinem PC (oder Mac, Tablet,
    Smartphone usw. – Anweisungen dazu findest du im Abschnitt **5.8.4
    Hochladen einer Textdatei**), oder mit der unter **Anhang 7
    Einrichten von M32-Einstellungen über einen Browser** beschriebenen
    Methode.

::: note
Der File Player merkt sich, wo du aufgehört hast. (Beende diesen Modus
durch Drücken und Halten des ENCODERs. **Schalte das Gerät nicht
einfach aus** und warte auch nicht auf den Time-Out, da der Morserino
sonst die aktuelle Position nicht speichern kann.) Beim nächsten Start
des File Players wird die Wiedergabe an dieser Stelle fortgesetzt. Wenn
das Ende der Datei (oder des Dateiabschnitts) erreicht ist, beginnt der
Player wieder von vorne.
:::

#### Kodierung von Textdateien für den File Player {-}

Die Datei sollte nur ASCII-Zeichen enthalten (Groß-/Kleinschreibung
spielt keine Rolle) – Zeichen, die im Morsecode nicht dargestellt werden
können, werden einfach ignoriert. Betriebszeichen können in der Datei
enthalten sein; sie müssen als 2-Zeichen-Darstellung mit \[\] oder \<>
um sie herum geschrieben werden, z.B. \<sk> oder \[ka\], oder mit einem
Backslash vorangestellt werden, z.B. \\kn.

**Betriebszeichen**

Folgende Betriebszeichen werden erkannt (Bedeutung der Betriebszeichen
siehe Abschnitt **5.4.1 Koch: Select Lesson**):

-   \<ar>: wird auf dem Display als + (Pluszeichen) angezeigt
-   \<bt>: wird auf dem Display als = (Gleichheitszeichen) angezeigt
-   \<as>
-   \<ka>
-   \<kn>
-   \<sk>
-   \<ve>
-   \<bk>

Beim Abspielen einer Datei werden drei weitere „Sonderzeichen" erkannt,
die auf die gleiche Weise wie Betriebszeichen gebildet werden:

**Pausen**

Es ist möglich, Pausen einzufügen (nützlich z.B. beim Abspielen eines
QSO-Texts – du kannst längere Pausen zwischen Sätzen oder beim
Stationswechsel einbauen). Verwende dazu \<p> oder \\p (mit einem
Leerzeichen davor und danach): Jedes \<p> (oder \[p\] oder \\p)
erzeugt eine Pause von drei regulären Wortabständen. Für längere Pausen
verwende mehrere Pausenmarkierungen (z.B. \\p \\p \\p).

Achte darauf, dass die Pausenmarkierungen durch Leerzeichen voneinander
und vom übrigen Text getrennt sind – andernfalls wird das gesamte Wort
(z.B. cq\<p>) durch eine Pause ersetzt!

**Tonwechsel**

Mit dem zweiten Sonderzeichen kannst du Tonwechsel in die Datei
einfügen (nützlich beim Abspielen von QSO-Texten, um z.B. Station A
von Station B zu unterscheiden). Füge dazu \<t> oder \\t oder \[t\]
(als separates Wort, d.h. mit mindestens einem Leerzeichen davor und
danach!) als Tonmarkierung ein. An dieser Stelle ändert sich der Ton
(sofern die Einstellung **Tone Shift** nicht auf **No Tone Shift**
steht); beim nächsten Auftreten der Tonmarkierung wird wieder der
ursprüngliche Ton verwendet.

Achte darauf, dass die Tonmarkierung durch Leerzeichen vom übrigen Text
getrennt ist – andernfalls wird das gesamte Wort (z.B. cq\<t>) als
Tonmarkierung behandelt und der restliche Teil (in diesem Fall „cq")
geht verloren!

Im Modus Echo Trainer wird die Tonmarkierung ignoriert.

**Kommentare**

Das dritte Sonderzeichen dient zum Einfügen von Kommentaren. \<c> oder
\\c in einem Wort oder für sich allein macht dieses Wort und den Rest
der Zeile zu einem Kommentar, der vom File Player nicht abgespielt wird.

**Abschnittstrenner und mehrteilige Dateien**

Ein Abschnittstrenner ist eine spezielle Form eines Kommentars: Wenn
eine Zeile mit einem Kommentarindikator beginnt, gefolgt von einem
Dollarzeichen (\$) und Text, wird dies als Abschnittstrenner behandelt.
Der Text nach dem Dollarzeichen sollte kurz sein und dient als Name für
den beginnenden Abschnitt. Ein Beispiel für eine Abschnittstrennerzeile:

`<c>$ Kapitel Eins`

Wenn die Datei mit einem Abschnittstrenner als erster Zeile beginnt,
wird sie als mehrteilige Datei behandelt. Beim Starten des File Players
kannst du dann auswählen, welchen Teil du verwenden möchtest (der Name
nach dem Dollarzeichen wird zur Orientierung angezeigt). Dieser Teil
wird dann genau so behandelt, als wäre er eine eigenständige Datei: Der
Player merkt sich, wo er aufgehört hat, und springt am Ende des
Abschnitts wieder an dessen Anfang.

**Dateiinhalt zufällig abspielen**

Es gibt auch eine File Player-Einstellung namens **Randomize File**.
Wenn sie auf „On" gesetzt ist (Standard: „Off"), überspringt das Gerät
nach jedem gesendeten Wort eine zufällige Anzahl von Wörtern (zwischen
0 und 255). Da das Abspielen am Datei- (oder Abschnitts-)ende wieder
von vorne beginnt, siehst du irgendwann alle Wörter – allerdings kann
das eine Weile dauern. Bei einer alphabetischen Wortliste bleibt die
Reihenfolge in einem Durchlauf alphabetisch. Für wirklich unvorhersehbare
Ergebnisse empfiehlt sich eine bereits zufällig sortierte Liste.

Wofür kann das verwendet werden? Du könntest z.B. eine Rufzeichenliste
hochladen und den File Player damit zufällig üben lassen. Im
Morserino-32-GitHub-Repository findest du eine Datei mit aktiven
HF-Contest-Rufzeichen sowie weitere geeignete Übungsdateien.

#### Wichtige Einstellungen für den CW Generator: {-}

**Interchar Spc** (Zeichenabstand). Legt die Pause zwischen einzelnen
Zeichen fest. Die „Norm" ist ein Abstand von drei Dits. Zur Erleichterung
des Kopierens bei hohen Geschwindigkeiten kann dieser Abstand vergrößert
werden. Der Code sollte dabei mit hoher Geschwindigkeit gesendet werden
(> 18 WpM), damit Dits und Dahs nicht gezählt werden können und der
Rhythmus der Zeichen erlernt wird. Grundsätzlich ist es besser, den
Wortabstand zu vergrößern als den Zeichenabstand. Empfohlen wird ein
Wert zwischen 3 und 6.

**Interword Spc** (Wortabstand). Normalerweise sieben Dits lang. Im
CW-Trainer-Modus kannst du den Wortabstand auf bis zu 45 Dits einstellen
(mehr als das Sechsfache des Normalwerts), um das Kopieren bei hohen
Geschwindigkeiten zu erleichtern. In Anlehnung an die
Farnsworth-Methode wird dies „Wordsworth-Abstand" genannt.

Da der Zeichenabstand unabhängig eingestellt werden kann, könnte er
höher als der Wortabstand eingestellt werden, was zu Verwirrung führen
kann. Um das zu verhindern, ist der Wortabstand immer mindestens vier
Dit-Längen größer als der Zeichenabstand.

Die ARRL und einige Morse-Trainingsprogramme verwenden eine Technik
namens „Farnsworth-Abstand", bei der die Abstände zwischen Zeichen und
Wörtern proportional verlängert werden. Du kannst diesen Effekt
nachahmen, indem du sowohl Zeichen- als auch Wortabstand vergrößerst.
Beispiel: Zeichenabstand 6, Wortabstand 14 – damit verdoppeln sich alle
Abstände. Bei einer Zeichengeschwindigkeit von 20 WpM ergibt sich eine
effektive Geschwindigkeit von 14 WpM, die in der Statuszeile als
(14)20 WpM angezeigt wird.

**Random Groups**: Legt fest, welche Zeichen in den zufälligen
Zeichengruppen enthalten sein sollen. Wählbar: Alpha / Numerals /
Interpunct. / Pro Signs / Alpha+Num / Num+Interp. / Interp+ProSn /
Alpha+Num+Int / Num+Int+ProS / All Chars.

**Length Rnd Gr**: Wie viele Zeichen soll eine zufällige Gruppe
enthalten? Wählbar: feste Längen 1–6 oder zufällig gewählte Länge
zwischen 2 to 3 und 2 to 6.

**Length Calls**: Maximale Länge der generierten Rufzeichen (3–6 oder
Unlimited).

**Length Abbrev** und **Length Words**: Maximale Länge der zufällig
generierten CW-Abkürzungen bzw. englischen Wörter (2–6 oder Unlimited).

**Each Word 2x**: Jedes „Wort" (Zeichen zwischen Leerzeichen) wird
zweimal ausgegeben, um das Kopieren nach Gehör zu erleichtern (ON –
auch „Wortverdoppler" genannt). Wenn ein vergrößerter Zeichenabstand
gewählt wurde, kann die Wiederholung auch mit kleinerem Abstand
(**ON less ICS**) oder ohne Farnsworth-Abstand (**ON true WpM**)
erzeugt werden.

Zu den weniger häufig verwendeten Einstellungen **Key ext TX**,
**CW Gen Displ** und **Generator Tx** siehe Abschnitt **6.2.4
Einstellungen zur CW-Generierung**.

## Echo Trainer

In diesem Modus erzeugt der Morserino-32 ein Wort oder eine
Zeichengruppe als Vorgabe. Du hast die gleichen Auswahlmöglichkeiten
wie beim CW Generator. Dann wartet er darauf, dass du die Zeichen mit
dem Paddle (oder einer Handtaste) wiederholst. Wenn du zu lange wartest
oder deine Antwort nicht mit der Vorgabe übereinstimmt, wird ein Fehler
auf dem Display und durch einen Ton angezeigt, und die Vorgabe wird
wiederholt. Wenn du die richtigen Zeichen eingibst, wird dies akustisch
und auf dem Display bestätigt. Dann wird die nächste Vorgabe ausgegeben.

Die Vorgabe wird normalerweise nicht angezeigt; nur deine Antwort ist
sichtbar.

Die Untermenüs sind dieselben wie beim CW Generator: Random, CW Abbrevs,
English Words, Call Signs, Mixed und File Player.

Wie beim CW Generator **startest** du diesen Modus **durch Drücken eines
Paddles** (oder des ENCODERs oder – bei Verwendung einer – der
Handtaste); dann wird die Sequenz „vvv\<ka>" als Warnsignal ausgegeben,
bevor das Echo-Training beginnt. Du kannst diesen Modus nicht durch
Drücken des Paddles oder der Handtaste stoppen, da du diese ja zum
Eingeben deiner Antworten verwendest! **Die einzige Möglichkeit, diesen
Modus zu stoppen, ist ein Klick auf den ENCODER-Knopf.**

Wenn du während deiner Antwort einen Fehler feststellst, kannst du
deine Antwort „zurücksetzen", indem du das Zeichen für „ERROR" eingibst
– d.h. eine Reihe von 8 Dits (manchmal auch als Betriebszeichen \<HH>
bezeichnet; der Morserino akzeptiert jede Folge von 7 oder mehr
aufeinanderfolgenden Dits). \<err> wird auf dem Display angezeigt, und
du kannst die Eingabe von vorne beginnen.<br>
Der M32 akzeptiert auch eine Folge von viermal dem Buchstaben e
(„eeee") als Rücksetzsignal.

Wie beim CW Generator kannst du auch hier zahlreiche Einstellungen
vornehmen. Besonders relevant für den Echo Trainer sind:

#### Wichtige Einstellungen für den Echo Trainer: {-}

**Echo Repeats**: Wie oft wird dasselbe Wort wiederholt, wenn die
Antwort zu spät oder fehlerhaft war, bevor ein neues Wort erzeugt wird?

**Echo Prompt**: Legt fest, wie du im Echo Trainer zur Eingabe
aufgefordert wirst. Mögliche Einstellungen: **Sound only** (Standard;
am besten zum Erlernen des Kopierens im Kopf), **Display only** (das
einzugebende Wort wird auf dem Display angezeigt, kein Ton; gut zum
Training der Paddle-Eingabe) und **Sound & Display** (du hörst die
Vorgabe UND siehst sie auf dem Display).

**Confrm. Tone**: Normalerweise ertönt im Echo Trainer ein hörbarer
Bestätigungston. Wenn du ihn ausschaltest, wiederholt das Gerät nur die
Vorgabe bei falscher Antwort oder sendet eine neue Vorgabe. Die visuelle
Anzeige „OK" oder „ERR" ist auch bei ausgeschaltetem Ton sichtbar.

**Max \# of Words**: Wie beim CW Generator kannst du den M32 nach einer
bestimmten Anzahl von Wörtern pausieren lassen.

Wenn diese Einstellung auf einen Wert zwischen 5 und 250 (nicht auf
„Unlimited") gesetzt ist, zeigt der M32 bei der Pause für 5 Sekunden an,
wie viele Fehler du gemacht hast (und die Anzahl der Wörter). *Beachte:
Bei einem Wort können mehrere Fehler entstehen, und alle werden gezählt!*

**Adaptv. Speed**: Zur Unterstützung des Trainings auf maximale
Geschwindigkeit. Bei jeder richtigen Antwort wird die Geschwindigkeit
um 1 WpM erhöht, bei jedem Fehler um 1 WpM verringert. So trainierst
du dauerhaft an deiner Leistungsgrenze.

**Echo Speed Max**: Legt die maximale Keying-Geschwindigkeit im Echo
Trainer fest. Wenn du die Geschwindigkeit mit dem Encoder über diesen
Wert hinaus erhöhst oder sie durch Adaptive Speed schneller wird,
bleibt die Eingabegeschwindigkeit bei diesem Maximum (so kannst du
bei höheren Geschwindigkeiten kopieren als beim Geben).

**Interchar Spc** und **Interword Spc**: Der erste Wert legt den
Zeichenabstand in der Vorgabe fest (wie in den Generatormodi); beide
Einstellungen wirken sich auch auf die Wartezeit aus, die du nach der
Vorgabe für deine Antwort hast. Wird diese Zeit überschritten, geht der
M32 davon aus, dass du deine Eingabe beendet hast.

## Koch Trainer

In den 1930er Jahren entwickelte der deutsche Ingenieur Ludwig Koch eine
Methode zum Erlernen des Morsecodes, bei der in jeder Lektion ein
weiteres Zeichen eingeführt wird. Die Reihenfolge ist weder alphabetisch
noch nach der Länge der Morsezeichen sortiert, sondern folgt einem
rhythmischen Muster. So werden die einzelnen Zeichen als Rhythmus
gelernt, nicht als zählbare Abfolge von Dits und Dahs.

Wenn du den Morsecode nach der Koch-Methode erlernen möchtest (ein
Zeichen nach dem anderen), **findest du alles, was du brauchst, im
Menüpunkt „Koch Trainer"**.

Dieser hat ein Untermenü zur Auswahl der Lektion (d.h. zur Festlegung
des hinzuzufügenden Zeichens), eines zum Üben genau dieses neuen
Zeichens (mit dem Echo-Trainer-Mechanismus – du wirst ermutigt, das
Gehörte zu wiederholen, bist aber nicht dazu verpflichtet) sowie die
Modi **CW Generator** und **Echo Trainer**, jeweils mit den Untermenüs
**Random** (Gruppen zufälliger Zeichen aus den bisher gelernten
Zeichen), **CW Abbrevs** (im CW-QSO übliche Abkürzungen), **English
words** (die gebräuchlichsten englischen Wörter) und **Mixed** (zufällige
Gruppen, Abkürzungen und Wörter gemischt). Es werden natürlich **nur
die bereits gelernten Zeichen** verwendet – das bedeutet, dass zu
Beginn des Lernens nur wenige Abkürzungen und Wörter zur Verfügung
stehen.

Um das Zählen von Dits und Dahs zu verhindern, sollte die Geschwindigkeit
ausreichend hoch sein (mindestens 18 WpM); die Pausen zwischen Zeichen
und Wörtern sollten nicht zu stark vergrößert werden (besser nur den
Wortabstand vergrößern und den Zeichenabstand annähernd normal lassen).
Der M32 ermöglicht es, den Wortabstand unabhängig vom Zeichenabstand
einzustellen.

### Koch: Select Lesson

Wähle eine „Koch-Lektion" zwischen 1 und 51 (insgesamt 51 Zeichen
werden nach der Koch-Methode gelernt). Lektion und zugehöriges Zeichen
werden im Menü angezeigt.

Die Zeichenreihenfolge wurde von Koch nicht streng festgelegt, daher
verwenden verschiedene Lernkurse leicht unterschiedliche Reihenfolgen.
Hier wird die gleiche Reihenfolge wie im Programm „Just Learn Morse
Code" als Standard verwendet, die nahezu identisch mit der des
Softwarepakets „SuperMorse" ist (siehe *http://www.qsl.net/kb5wck/super.html*).

Diese Reihenfolge ist wie folgt:

| Lektion | Zeichen | Lektion  | Zeichen | Lektion  | Zeichen / Betriebszeichen |
|:-------:|---------|:--------:|---------|:--------:|--------------------------|
| 1  | m         | 18 | 0 (Null)  | 35 | 7 |
| 2  | k         | 19 | y         | 36 | c |
| 3  | r         | 20 | v         | 37 | 1 |
| 4  | s         | 21 | , (Komma) | 38 | d |
| 5  | u         | 22 | g         | 39 | 6 |
| 6  | a         | 23 | 5         | 40 | x |
| 7  | p         | 24 | /         | 41 | – (Minus) |
| 8  | t         | 25 | q         | 42 | = (auch Betriebszeichen BT: Leerzeichen / neuer Abschnitt) |
| 9  | l         | 26 | 9         | 43 | SK (Betriebszeichen: OUT / Ende des Kontakts) |
| 10 | o         | 27 | z         | 44 | AR (+, Betriebszeichen: OUT / Ende der Nachricht) |
| 11 | w         | 28 | h         | 45 | AS (Betriebszeichen: WAIT) |
| 12 | i         | 29 | 3         | 46 | KN (Betriebszeichen: OVER / bestimmte Station) |
| 13 | . (Punkt) | 30 | 8         | 47 | KA (Betriebszeichen: ACHTUNG / neue Nachricht) |
| 14 | n         | 31 | b         | 48 | VE (Betriebszeichen: VERIFIED / verstanden) |
| 15 | j         | 32 | ?         | 49 | BK (Betriebszeichen: BREAK-IN / lass mich unterbrechen) |
| 16 | e         | 33 | 4         | 50 | @ |
| 17 | f         | 34 | 2         | 51 | : (Doppelpunkt) |

::: note
Es gibt noch ein weiteres Betriebszeichen, das in den Koch-Lektionen
nicht behandelt wird:<br>
Das Zeichen \<HH> (acht aufeinanderfolgende Dits) zeigt einen Fehler an
(der Empfänger soll das/die vorherige(n) Zeichen ignorieren).
:::

#### Unterschiedliche Zeichenreihenfolgen {-}

Es besteht auch die Möglichkeit, die Zeichenreihenfolge auszuwählen.
Neben der nativen Reihenfolge kannst du die Reihenfolge des populären
Online-Trainingstools **Learn CW Online** (LCWO), die der **CW Ops CW
Academy**-Kurse oder die des „Carousel"-Lehrplans des **Long Island CW
Club (LICW)** wählen. Die Einstellung erfolgt in den Einstellungen des
Morserino-32 unter **Koch Sequence**.

Wenn du einen Kurs bei **LICW** belegst, solltest du auch die Einstellung
**LICW Carousel** entsprechend deinem Einstiegspunkt in den Lehrplan
setzen (z.B. wenn du einen Kurs innerhalb von BC1 – Basic Course 1 –
mit den Zeichen **p**, **g** und **s** beginnst, setze dies auf „BC1:
p g s". Alle weiteren Zeichen, die du in BC1 lernst, werden in der
gleichen Reihenfolge wie deine Koch-Lektionen im Morserino angezeigt.<br>
Wenn du BC1 abgeschlossen hast und in BC2 einsteigst, beginnst du z.B.
mit den Zeichen **7**, **3** und **?** – setze die Einstellung dann
entsprechend auf „BC2: 7 3 ?".)

Die Zeichenreihenfolge bei Auswahl von **LCWO** ist wie folgt:

| Lektion | Zeichen | Lektion | Zeichen | Lektion | Zeichen |
|:-------:|---------|:-------:|---------|:-------:|---------|
| 1  | k         | 18 | f    | 35 | 7  |
| 2  | m         | 19 | o    | 36 | c  |
| 3  | u         | 20 | y    | 37 | 1  |
| 4  | r         | 21 | ,    | 38 | d  |
| 5  | e         | 22 | v    | 39 | 6  |
| 6  | s         | 23 | g    | 40 | 0  |
| 7  | n         | 24 | 5    | 41 | x  |
| 8  | a         | 25 | /    | 42 | -  |
| 9  | p         | 26 | q    | 43 | SK |
| 10 | t         | 27 | 9    | 44 | AR |
| 11 | l         | 28 | 2    | 45 | KA |
| 12 | w         | 29 | h    | 46 | AS |
| 13 | i         | 30 | 3    | 47 | KN |
| 14 | .         | 31 | 8    | 48 | VE |
| 15 | j         | 32 | b    | 49 | BK |
| 16 | z         | 33 | ?    | 50 | @  |
| 17 | =         | 34 | 4    | 51 | :  |

Die Zeichenreihenfolge der **CW Academy** ist:

| Lektion | Zeichen | Lektion | Zeichen | Lektion | Zeichen |
|:-------:|---------|:-------:|---------|:-------:|---------|
| 1  | t  | 18 | m  | 35 | k  |
| 2  | e  | 19 | w  | 36 | j  |
| 3  | a  | 20 | 3  | 37 | 8  |
| 4  | n  | 21 | 6  | 38 | 0  |
| 5  | o  | 22 | ?  | 39 | =  |
| 6  | i  | 23 | f  | 40 | x  |
| 7  | s  | 24 | y  | 41 | z  |
| 8  | 1  | 25 | ,  | 42 | BK |
| 9  | 4  | 26 | p  | 43 | SK |
| 10 | r  | 27 | g  | 44 | .  |
| 11 | h  | 28 | q  | 45 | -  |
| 12 | d  | 29 | 7  | 46 | KA |
| 13 | l  | 30 | 9  | 47 | AS |
| 14 | 2  | 31 | /  | 48 | KN |
| 15 | 5  | 32 | b  | 49 | VE |
| 16 | u  | 33 | v  | 50 | @  |
| 17 | c  | 34 | AR | 51 | :  |

Die Zeichenreihenfolge der **LICW**-Kurse ist:

| Lektion | Zeichen | Lektion | Zeichen / Betriebszeichen | Lektion | Zeichen / Betriebszeichen |
|:-------:|---------|:-------:|--------------------------|:-------:|--------------------------|
| 1  | r  | 18 | b  | 35 | 6  |
| 2  | e  | 19 | k  | 36 | .  |
| 3  | a  | 20 | m  | 37 | z  |
| 4  | t  | 21 | y  | 38 | j  |
| 5  | i  | 22 | 5  | 39 | /  |
| 6  | n  | 23 | 9  | 40 | 2  |
| 7  | p  | 24 | ,  | 41 | 8  |
| 8  | s  | 25 | q  | 42 | BK |
| 9  | g  | 26 | x  | 43 | 4  |
| 10 | l  | 27 | v  | 44 | 0  |
| 11 | c  | 28 | 7  | 45 |    |
| 12 | d  | 29 | 3  | 46 |    |
| 13 | h  | 30 | ?  | 47 |    |
| 14 | o  | 31 | +  | 48 |    |
| 15 | f  | 32 | SK | 49 |    |
| 16 | u  | 33 | =  | 50 |    |
| 17 | w  | 34 | 1  | 51 |    |

::: note
Der LICW-Lehrplan umfasst nur 44 Zeichen. Wenn du die restlichen Zeichen
(*- \<KA> \<AS> \<KN> \<VE> @ :*) lernen möchtest, musst du die
Zeichenreihenfolge auf **CW Academy** umstellen und dort mit Lektion 45
fortfahren.
:::

#### Koch: Training mit einem benutzerdefinierten Zeichensatz {-}

Du kannst den Koch Trainer auch verwenden, um mit einem selbst
festgelegten Zeichensatz zu üben. Lade zunächst eine Textdatei in den
File Player hoch, die die zu trainierenden Zeichen enthält (als ein
„Wort" oder mehrere Wörter auf einer oder mehreren Zeilen). Setze dann
die Einstellung **Koch Sequence** auf die neue Option **Custom Chars**.
Der Koch Trainer liest die Zeichen dann aus der Datei.

Nun kannst du den Koch Trainer (CW Generator oder Echo Trainer)
verwenden, und er wird diese Zeichen für dein Training benutzen. Die
Einstellung der Koch-Lektion hat dabei **keinen Einfluss**.

Um den Zeichensatz zu ändern, lade eine neue Textdatei hoch und wähle
**Custom Chars** erneut aus. Selbst wenn es zuvor bereits ausgewählt
war, musst du **Custom Chars** erneut wählen, um den neuen Zeichensatz
zu übernehmen. Das bloße Hochladen einer neuen Datei ändert den
benutzerdefinierten Zeichensatz nicht.<br>
*Das ist ein Feature, kein Bug!*<br>
Es bedeutet, dass du zwischen dem Training mit deinem Zeichensatz und
der Verwendung einer anderen Textdatei für den File Player wechseln
kannst. Durch Umstellen von **Koch Sequence** auf M32, LCWO, LICW oder
CW Academy kehrst du zur „normalen" Koch-Trainer-Option zurück.

### Koch: Learn New Chr

Wenn du diesen Menüpunkt auswählst, wird das neue Zeichen entsprechend
der ausgewählten Koch-Lektion eingeführt: Du hörst den Ton und siehst
die Abfolge von Punkten und Strichen schnell auf dem Display sowie das
zugehörige Zeichen. Dies wird wiederholt, bis du durch Drücken des
ENCODER-Knopfs stoppst. Nach jeder Wiederholung hast du die Möglichkeit
(aber keine Pflicht), das Gehörte mit dem Paddle zu wiederholen; der
Morserino teilt dir dann mit, ob das richtig war.

Sobald du das neue Zeichen beherrschst, kannst du im Koch Trainer zum
**CW Generator** oder **Echo Trainer** wechseln, um das neu erlernte
Zeichen in Kombination mit allen bisher gelernten Zeichen zu üben.

### Koch: CW Generator und Echo Trainer

Die Funktionalität entspricht der oben für diese Modi beschriebenen, mit
folgenden kleinen Unterschieden:

-   Es werden nur die Zeichen bis zur ausgewählten Koch-Lektion generiert
    (oder die durch deinen benutzerdefinierten Zeichensatz definierten
    Zeichen).
-   Die Einstellung **Random Groups** wird ignoriert.
-   Es gibt kein Untermenü **File Player**.
-   Im Koch Echo Trainer gibt es außerdem ein Untermenü **Adapt. Rand.**,
    siehe unten.

### Koch Echo Trainer: Adapt. Rand.

Der Modus **Adaptive Random** passt die zufällige Zeichenauswahl anhand
der eingegebenen Antworten an. Ein falsch gegebenes Zeichen erhöht die
Wahrscheinlichkeit, dass es wieder ausgewählt wird; ein richtig gegebenes
Zeichen verringert diese Wahrscheinlichkeit.

Um den adaptiven Modus zu starten, wähle: **Koch Trainer** > **Echo
Trainer** > **Adapt. Rand.**

**Hinweise:**

-   Die Wahrscheinlichkeiten werden bei jedem Start des Modus auf die
    Standardwerte zurückgesetzt.
-   Die zuletzt gelernten Koch-Zeichen haben zu Beginn der Sitzung eine
    höhere Wahrscheinlichkeit.
-   Zu Beginn der Sitzung wird jedes Zeichen einmal ausgewählt (in
    zufälliger Reihenfolge).
-   Danach werden die nächsten Zeichen zufällig ausgewählt; falsch
    gegebene Zeichen haben eine höhere Auswahlwahrscheinlichkeit.
-   Ein falsch gegebenes Zeichen erhöht auch die Wahrscheinlichkeit
    des Zeichens davor und danach. Z.B.: Wenn „**z/?**" gefragt wird
    und du mit „**g/?**" antwortest, wird die Wahrscheinlichkeit für
    „z" erhöht und die Wahrscheinlichkeit für „/" ein wenig.
-   Nur das erste falsche Zeichen wird analysiert. Wenn z.B. „**z/?**"
    erwartet wird und du mit „**gz/?**" antwortest, werden die
    Wahrscheinlichkeiten genauso erhöht wie im vorherigen Beispiel.
-   Erwarte keinen Spaß in diesem Modus (benutze ihn trotzdem!). Der
    adaptive Modus wird dich mit den Zeichen ärgern, die du nicht
    jedes Mal zu 100 % richtig gibst. Wenn du einen totalen
    Frustrationslevel erreicht hast, wechsle zurück zum normalen Koch
    Zufallsmodus und entspanne dich ein wenig, bevor du **Adapt. Rand.**
    wieder verwendest ;-).

## Transceiver

Je nach Verfügbarkeit von LoRa auf deinem M32 gibt es drei oder vier
Transceiver-Modi.

* Wenn du LoRa hast, ist **der erste** ein eigenständiger Transceiver
  für die Kommunikation mit Morsecode, der die LoRa-Spreizspektrum-
  Funktechnologie nutzt (in der Standardversion im 433-MHz-Band).

* **Der nächste** nutzt das Internetprotokoll (speziell UDP auf Port
  7373) für die Kommunikation über ein IP-Netzwerk via WLAN mit einem
  Access Point.

  Es gibt jetzt auch einen alternativen WLAN-Modus (**EspNow**), der
  eine Peer-to-Peer-Kommunikation zwischen Morserinos in unmittelbarer
  Nähe ohne Access Point ermöglicht.

* **Der letzte Modus** ist ein Transceiver-Modus, der entweder mit
  einem externen Transceiver (z.B. einem Kurzwellen-Amateurfunk-
  Transceiver) oder mit einem Protokoll wie iCW (CW über Internet)
  oder VBand verwendet werden kann. In allen Fällen sind CW Keyer und
  Empfänger bzw. CW-Decoder gleichzeitig aktiv.

### LoRa Trx

::: note
Dieser Modus ist nur verfügbar, wenn dein Morserino einen
Hardware-LoRa-Transceiver enthält (wie bei Morserinos der 1st und 2nd
Edition der Fall, beim M32Pocket in der Standardkonfiguration jedoch
nicht)!
:::

Wie bereits erwähnt, ist dies ein Morsecode-Transceiver, der LoRa
verwendet, um Morsecode an andere Morserino-32-Geräte zu senden.
Zusätzlich zur CW-Keyer-Funktionalität sendet er alles, was du gibst,
über den LoRa-Transceiver. Dabei wird ein spezielles Datenformat
verwendet, das die eingegebenen Punkte und Striche kodiert – unabhängig
davon, ob es sich um zulässige Morsezeichen handelt. Außerdem empfängt
er auf seiner Frequenz, wenn du nicht gibst, sodass du interaktive
Unterhaltungen im Morsecode zwischen zwei oder mehr
Morserino-32-Geräten führen kannst! Die Zeichen werden Wort für Wort
übertragen, daher gibt es auf der Empfängerseite eine leichte
Verzögerung, und QSK ist nicht möglich. Dies möge dich zur Verwendung
korrekter Übergabeprozeduren anhalten.

#### Mehr über den LoRa-Transceiver-Modus {-}

Der LoRa-Transceiver-Modus verwendet grundsätzlich die gleiche
Benutzeroberfläche wie der CW Keyer. Sobald du jedoch etwas empfängst,
zeigt die Statuszeile zusätzlich zur eigenen Geschwindigkeit auch die
Geschwindigkeit der sendenden Station an – du siehst z.B.
*18r20sWpM*, was bedeutet, dass du eine Station mit 18 WpM empfängst
und mit 20 WpM sendest. Außerdem wechselt der Lautstärkebalken rechts
in der Statuszeile seine Funktion: Statt der aktuellen Lautstärke zeigt
er die Signalstärke an – eine einfache Form eines S-Meters. Der volle
Balken entspricht einem RSSI-Pegel von ca. -20 dB; der Balken beginnt
bei ca. -150 dB zu erscheinen.

Durch Drücken der FN-Taste kannst du weiterhin die Lautstärke einstellen.

Vom Transceiver empfangene Morsezeichen werden im (scrollbaren)
Textbereich des Displays **fett** dargestellt, alles Gesendete wird in
normalen Zeichen angezeigt.

Erwähnenswert ist auch: Die Tonhöhe beim Empfang der Gegenstation wird
wie in den anderen Modi über die Einstellung **Tone Pitch** angepasst.
Beim Senden kann die Tonhöhe gleich sein oder einen Halbton höher oder
tiefer als der Empfangston – dies wird über die Einstellung
**Tone Shift** festgelegt, genau wie im Echo Trainer Modus.

Wichtig zu wissen: Der LoRa CW-Transceiver funktioniert nicht wie ein
CW-Transceiver auf Kurzwelle, wo ein unmodulierter Träger getastet wird.
LoRa verwendet Spreizspektrum-Technologie, um **Datenpakete** zu senden
– ähnlich wie das WLAN auf deinem Handy oder PC. Alles, was du eingibst,
wird zunächst in Daten kodiert (Geschwindigkeit sowie alle Punkte,
Striche und Pausen zwischen den Zeichen). Sobald die Pause lang genug
ist, um als Wortpause erkannt zu werden, wird das gesamte bis dahin
zusammengestellte Datenpaket übertragen und vom empfangenden
Morserino-32 mit der ursprünglichen Geschwindigkeit wiedergegeben.

Weitere Informationen über LoRa findest du in **Anhang 2 Weitere
Informationen über LoRa**.

### WiFi Trx

Du kannst diesen Transceiver-Modus verwenden, um mit deinem CW-Buddy
über WLAN zu kommunizieren – entweder im lokalen Netzwerk, über das
Internet oder sogar Peer-to-Peer ohne Access Point. Diese Methode heißt
*EspNow*; der Morserino nutzt dabei seine Broadcast-Funktionalität.
Die Reichweite ist naturgemäß begrenzt (je nach Umgebung einige Meter
bis ca. 50 Meter bei freier Sicht). Wie du im Modus WiFi Trx zwischen
Access Point und EspNow umschaltest, erfährst du im Abschnitt **5.8.6
WiFi Select**.

#### Verwendung von zwei verschiedenen EspNow-Kanälen {-}

Der Morserino-32 kann im EspNow-Modus zwei Kanäle nutzen. Dies kann z.B.
in einem Klassenzimmer verwendet werden, um zwei unabhängige Gruppen zu
bilden, die sich nicht gegenseitig stören, oder in Umgebungen mit sehr
hohem WLAN-„Rauschen" – wenn der primäre Kanal nicht korrekt
funktioniert, versuche es mit dem sekundären Kanal.

Die Kanäle werden über die Einstellung **Trx Channel** ausgewählt, siehe
Abschnitt **6.2.6 Einstellungen zum Senden, Dekodieren und QSO Bot**.

#### Verwendung von WLAN mit einem Access Point (WLAN-Router) {-}

Um herkömmliches WLAN zu nutzen, musst du mit einem WLAN-Access-Point
verbunden sein. Das bedeutet, dass du zuvor die Funktion **WiFi Config**
ausgeführt haben musst. Im lokalen Netzwerk ist die Verwendung des
Transceiver-Modus sehr einfach: Wähle ihn im Menü aus, ohne eine
Peer-Adresse zu konfigurieren. Er sendet an die Broadcast-Adresse
255.255.255.255, die von allen Geräten im Netzwerk empfangen werden
kann. Der Morserino-32 verwendet UDP-Port 7373 für die asynchrone
Kommunikation.

Beim Start von WiFi Trx wird die IP-Adresse des Peers (oder „IP
Broadcast") kurz auf dem Display angezeigt. Bei Verwendung von EspNow
erscheint „EspNow".

Um mit einem bestimmten Morserino-32 über das Internet zu
kommunizieren, konfiguriere die IP-Adresse der Gegenstelle. Dies erfolgt
über den Menüpunkt **Config WiFi**, der neben SSID und Passwort ein
drittes Feld hat. Gib dort die IP-Adresse oder den DNS-Hostnamen der
Gegenstelle ein; der WLAN-Transceiver sendet dann Pakete an diese
Adresse.

Es gibt Dienste auf öffentlichen Internetservern, die über das
Morserino-Protokoll kommunizieren können, z.B. ein „Chatbot" auf
*cq.morserino.info* – diese sind normalerweise ohne weitere
Konfiguration des Internetrouters nutzbar, es sei denn, dein Router
sperrt bestimmte Portnummern. Komplizierter wird es, wenn sich die
Gegenstelle ebenfalls hinter einer Firewall oder einem NAT-Router
befindet:

Wenn die IP-Adresse nicht im lokalen Netzwerk liegt und du hinter einer
Firewall oder einem Router steckst, kann der Morserino Pakete ins
Internet senden (sofern keine Firewall-Regeln UDP-Ports sperren), aber
Pakete von der Gegenstelle hinter einer anderen Firewall werden vom
Router blockiert. In diesem Fall musst du „Portweiterleitung"
konfigurieren, damit der Router alle UDP-Pakete auf Port 7373 an deinen
Morserino sendet. Gleichzeitig teilst du deinem Buddy deine
externe IP-Adresse mit (die IP-Adresse deines Routers zum
Internetanbieter hin), und er tut das Gleiche.

Eine weitere, etwas aufwändigere Möglichkeit wäre die Einrichtung eines
VPN (Virtual Private Network), sodass sich beide Morserinos im selben
„virtuellen Netzwerk" befinden und ohne Firewall-Blockierung miteinander
kommunizieren können. Wie das geht, sprengt den Rahmen dieses Handbuchs –
frag einen Internet-Experten!

### iCW/Ext Trx

In diesem Modus wird ein am Morserino-32 angeschlossener Transceiver
getastet, oder du kannst den Line-Out-Audioausgang verwenden, um z.B.
einen FM-Transceiver zu tasten oder CW über das Internet zu verwenden
(iCW – dieses Protokoll verwendet Mumble als Audioaustauschprotokoll).
Über Bluetooth kannst du dich auch mit einem Computer verbinden und
über diesen mit VBand, einem weiteren internetbasierten CW-Dienst
(siehe Abschnitt **6.2.2 Einstellungen zu Key, Paddles und Keyer** für
die VBand-Bluetooth-Einstellung).

Alle CW-Signale, die als Audio über den Audioeingang eingehen, werden
dekodiert und auf dem Display angezeigt. Ein externer Transceiver, der
über den Anschluss „to Tx" angeschlossen ist, wird vom Keyer getastet;
alternativ kann der Audioausgang am Line-Out-Anschluss in einen Computer
oder FM-Transceiver eingespeist werden.

### QSO Bot

Der QSO Bot ist ein simulierter CW-QSO-Partner, mit dem du vollständige,
realistische Funkverbindungen ohne Sender üben kannst. Der Bot sendet in
Morsecode und liest mit, was du zurücktastest, und reagiert „in character" –
so kannst du das Hin und Her einer echten Verbindung in deinem eigenen Tempo
einüben, ohne auf Sendung zu gehen. Es handelt sich ausschließlich um einen
Übungspartner: Der Bot **sendet niemals über LoRa, WLAN oder den externen
Sender** – er ertönt nur über den lokalen Mithörton und kann daher jederzeit
gefahrlos verwendet werden.

Es gibt drei QSO-Typen, wählbar im Menü unter **Transceiver → QSO Bot**:

-   **SOTA/POTA** – eine Verbindung im Rahmen von „Summits On The Air" oder
    „Parks On The Air" (Rufzeichen, Rapport, Gipfel-/Parkreferenz). Der Bot
    wählt zufällig SOTA oder POTA und teilt dies in seinem CQ-Ruf mit.
-   **Standard** – ein klassisches „Rubber-Stamp"-QSO: Rapport, Name und
    QTH, dann Stationsangaben (Rig, Antenne, Wetter, Alter), dann die
    Verabschiedung.
-   **Contest** – eine fortlaufende Folge sehr kurzer Contest-QSOs. Das
    Austauschformat wird mit der Einstellung **Contest Type** festgelegt
    (siehe **6.2.6 Einstellungen zum Senden, Dekodieren und QSO Bot**):
    **CQ WW** (Rapport + CQ-Zone) oder **WPX/Sprint** (Rapport +
    Seriennummer).

#### Anzeige und Bedienung {-}

Der QSO Bot verwendet dieselbe Bildschirmdarstellung und Bedienung wie der
CW Keyer und die Transceiver-Modi. Was der Bot sendet, erscheint in
**Fettschrift**; was du tastest, wird dekodiert angezeigt. Die obere
Statuszeile zeigt den Keyer-Modus und deine Geschwindigkeit. Durch Drehen des
ENCODERs änderst du die Geschwindigkeit (oder die Lautstärke, nach einem
kurzen Druck auf die FN-Taste); ein langer Druck auf FN wechselt in den
Scroll-Modus, um den bisherigen Verlauf nachzulesen; ein Doppelklick auf den
ENCODER-Knopf öffnet die Einstellungen; ein langer Druck auf den
ENCODER-Knopf verlässt den QSO Bot.

#### Wer ruft CQ {-}

Wenn du einen QSO-Typ startest, hört der Bot einige Sekunden lang zu. **Rufst
du CQ**, antwortet dir der Bot – du bist der Aktivierer (SOTA/POTA) bzw. die
rufende Station (Contest). **Bleibst du still**, ruft der Bot CQ und du
antwortest ihm. Dafür gibt es keine Einstellung; wie auf dem Band übernimmt
derjenige die Führung, der CQ ruft. Der Bot verwendet für jede Verbindung ein
neues Rufzeichen, sodass du viele verschiedene Rufzeichen zum Mitschreiben
bekommst.

#### Tasten zum Bot {-}

Der Bot ist bewusst tolerant, damit sich das Üben natürlich anfühlt und nicht
wie ein Diktat:

-   Er erkennt deine Informationen innerhalb der üblichen
    Gesprächsfüllwörter – `de oe1wkl k`, `r ur 599 599 bk`, `qth vienna
    vienna` funktionieren alle; die Füllwörter werden ignoriert.
-   Rapporte können als volle Zahlen oder als **Cut Numbers** gesendet werden
    (`5nn` für 599, `t` für 0, `a` für 1, `n` für 9).
-   **Der Bot wartet, bis du fertig bist.** Er antwortet erst, wenn du ein
    Ende-der-Übergabe-Zeichen sendest (`k`, `<sk>`, `<ar>`, `bk`, `73`) oder
    einige Sekunden pausierst – er tastet dir nicht dazwischen, während du
    noch sendest.
-   **Um einen Fehler zu korrigieren**, sende `<err>` (eine Reihe von Dits)
    oder `eeee` und danach die korrigierte Information; der Bot verwirft das
    Zurückgenommene und übernimmt die neue Fassung. Bei einem einzelnen Wert
    wie einem Rapport oder einem Rufzeichen kannst du die korrigierte Fassung
    auch einfach später in derselben Übergabe noch einmal senden (zum Beispiel
    `599 = 579`) – es zählt die zuletzt gesendete.
-   **Um eine Wiederholung zu erbitten**, sende `agn`, `rpt` oder einfach `?`;
    der Bot wiederholt seine letzte Übergabe. Du kannst auch nach einem
    einzelnen Punkt fragen – `rpt rst`, `rpt call`, `rpt qth` – und der Bot
    wiederholt nur diesen.
-   **Um das Tempo des Bots zu ändern**, sende `qrs` (langsamer) oder `qrq`
    (schneller); der Bot passt sein Tempo an, und du hörst die neue
    Geschwindigkeit bei seiner nächsten Übergabe. Sende es erneut, um weiter
    nachzuregeln. Das ändert nur das Tempo des Bots, nicht dein eigenes
    Keyer-/Mithörton-Tempo (das du wie gewohnt mit dem Encoder einstellst).

Tastest du etwas, das der Bot nicht zuordnen kann, oder verstummst du
mittendrin, reagiert er wie ein echter Operator – er bittet mit wechselnden
Aufforderungen wie `agn agn`, `agn?`, `pse rpt` oder `qrz?` um eine
Wiederholung, oder wiederholt seine letzte Übergabe – statt einfach stehen zu
bleiben.

Wie nachsichtig und wie gesprächig der Bot ist, lässt sich mit der Einstellung
**QSO Difficulty** festlegen (**Beginner** / **Intermediate** / **Advanced**).
Bei **Beginner** gibt dir der Bot mehr Zeit, erlaubt einen zusätzlichen Versuch,
gibt Rapporte voll ausgeschrieben (599 statt 5nn), verwendet klare, ruhige
Aufforderungen und sendet immer mit deinem eigenen Tempo. Bei **Intermediate**
und **Advanced** ruft der Bot manchmal mit einem Tempo, das ein paar WpM von
deinem abweicht, damit du das Erbitten von `qrs`/`qrq` übst; **Advanced** hält
zudem ein strafferes Tempo und antwortet knapper. Das gilt für alle
QSO-Bot-Modi.

In einem **Standard**-QSO spiegelt der Bot außerdem deine Förmlichkeit: Sobald
das QSO läuft, lässt der Bot die Rahmung `<call> de <call>` zwischen den
Übergaben ebenfalls weg, wenn du sie weglässt. **Beginner** behält immer die
volle Rahmung bei; **Advanced** lässt sie früh weg, sobald das QSO etabliert
ist. Eröffnung und Schluss-Verabschiedung verwenden immer die volle
Rufzeichen-Rahmung.

#### SOTA / POTA {-}

Der Bot spielt eine Gipfel- oder Parkaktivierung. Ruft er CQ, ist er der
Aktivierer und sendet eine Gipfel-/Parkreferenz, die zum Land seines
Rufzeichens passt (`oe/st-002` für ein österreichisches Rufzeichen,
`w6/ss-001` für ein US-Rufzeichen usw.); du rufst als Jäger (Chaser) an und
tauschst Rapporte aus. Rufst **du** CQ, bist du der Aktivierer und der Bot
jagt dich. Gelegentlich entpuppt sich die Station, die auf deinen CQ-Ruf
antwortet, ebenfalls als Aktivierer und sendet ihre eigene Referenz – eine
Summit-to-Summit- bzw. Park-to-Park-Verbindung (sie kann sogar SOTA-zu-POTA
sein). Eine SOTA/POTA-Sitzung ist eine einzelne Verbindung; der Bot
verabschiedet sich, wenn sie beendet ist.

#### Standard {-}

Dies ist ein vollständiges „Rubber-Stamp"-QSO im Plauderstil, in drei Runden:
zuerst Rapport, Name und QTH; dann Stationsangaben (Rig, Antenne, Wetter,
Alter); dann die Verabschiedung. Der Bot sendet realistische eigene Angaben
und schreibt deine mit. Er erkennt deine Informationen an den
**Feld-Schlüsselwörtern** – `name`, `qth`, `rig`, `ant`, `wx`, `age` – sodass
es egal ist, ob du sie mit `=`, mit `es` oder gar nicht trennst, und Werte
dürfen aus mehreren Wörtern bestehen (`rig icom 7300`). Er merkt sich deinen
Namen und verwendet ihn, wenn er zu dir zurückkommt (`fb dr willi`). Eine
gültige Verbindung braucht einen Rapport; enthält deine erste Übergabe also
Name und QTH, aber keinen RST, fragt der Bot `pse ur rst?`, bevor er
fortfährt.

#### Contest {-}

Contest-Übung ist eine fortlaufende Folge sehr kurzer QSOs. Stelle zuerst
**Contest Type** ein: **CQ WW** tauscht Rapport plus CQ-Zone aus (der Bot
verwendet die echte Zone für das Land seines Rufzeichens), **WPX/Sprint**
tauscht Rapport plus Seriennummer aus. Sobald ein QSO beendet ist, ruft die
rufende Station erneut CQ – ruft der Bot, tut er dies mit einem neuen
Rufzeichen; rufst du, antwortet eine neue Station auf deinen nächsten CQ-Ruf.
Die Sitzung läuft weiter, solange du Stationen arbeitest, und endet
automatisch nach etwa fünfzehn Sekunden ohne Antwort (niemand antwortet auf
den CQ-Ruf des Bots, oder du rufst nicht erneut CQ). Drücke eine Taste, um
nach dem Ende der Sitzung zum Menü zurückzukehren.

## CW Decoder

In diesem Modus werden Morsezeichen dekodiert und auf dem Display
angezeigt. Der Morsecode kann entweder über eine Morsetaste eingegeben
werden (Handtaste – angeschlossen an die Buchse, an der normalerweise
ein externes Paddle angeschlossen wird; du kannst auch eines oder beide
der Touchpaddles zum manuellen Tasten des Decoders verwenden). Mit dem
Decoder auf diese Weise kannst du deinen Geberhythmus mit einer
Handtaste kontrollieren und verbessern, indem du prüfst, ob das
Gesendete korrekt dekodiert wird.

Du kannst auch Töne (am Audioeingang) dekodieren, die z.B. von einem
Empfänger stammen. Der Ton sollte bei ca. 700 Hz liegen. Optional gibt
es einen recht scharfen Filter (in Software implementiert), der nur
Töne in einem engen Bereich um 700 Hz erkennt und alle anderen
ignoriert. Dieser wird durch Auswahl der Einstellung **Narrow**
aktiviert (siehe Abschnitt **6.2.6 Einstellungen zum Senden, Dekodieren und QSO Bot**).

Die Statuszeile unterscheidet sich etwas von den anderen Modi. Der
Drehgeber befindet sich immer im Lautstärke-Einstellmodus – die
Geschwindigkeit wird durch den dekodierten Morsecode bestimmt und kann
nicht manuell eingestellt werden. Durch Drücken des ENCODER-Knopfs
wird der Decoder-Modus beendet und du gelangst zurück zum Startmenü.

Oben links in der Statuszeile siehst du ein kleines Quadrat oder
Rechteck, wenn die Taste gedrückt wird (oder ein 700-Hz-Ton erkannt
wird), und rechts davon den Buchstaben **d** – dieser ersetzt die
Keyer-Modus-Anzeige der anderen Modi.

Die aktuell erkannte Geschwindigkeit wird in der Statuszeile als WpM
angezeigt.

Dieser Modus hat wenige Einstellungen (siehe Abschnitt **6.2.6 Einstellungen zum Senden, Dekodieren und QSO Bot**); die wichtigste ist
vielleicht die Möglichkeit, die Filterbandbreite des Audio-Decoders
zwischen schmal (ca. 150 Hz) und breit (ca. 600 Hz) umzuschalten. Für
Signale aus einem Transceiver (wo andere Signale in der Nähe sein
könnten) ist es in der Regel am besten, die Bandbreite auf **Narrow** zu
setzen und das Signal auf 700 Hz (± 5 %) abzustimmen. Für Signale von
einem FM-Transceiver, iCW oder anderen Umgebungen mit wenig Störungen
empfiehlt sich die Einstellung **Wide** – in diesem Fall muss die
Tonfrequenz nicht so nahe an 700 Hz liegen.

## Spiele

Der Morserino Pocket bietet jetzt CW-basierte Spiele, die das Lernen
und Üben des Morsecodes unterhaltsamer gestalten. Die Spiele sind unter
dem Menüpunkt „**Games**" zu finden. Derzeit sind sechs Spiele verfügbar:
**Morse Invaders**, **Fight the Pileup**, **Radio Cave**, **Morsel**,
**Trailblazer** und **Fox Hunt**. Die beiden letzten sind ein Paar
Gitter-Labyrinth-Spiele mit demselben Spielfeld — Trailblazer trainiert
das Geben, Fox Hunt das Aufnehmen.

### Morse Invaders

Morse Invaders ist ein Spiel, bei dem Zeichen auf dem Farbdisplay nach
unten fallen und du den richtigen Morsecode eingeben musst, um sie zu
zerstören, bevor sie den unteren Rand erreichen. Es verbindet den Spaß
eines klassischen Arcade-Spiels mit effektivem CW-Training.

Der Zeichenpool wird durch deine aktuelle Koch-Lektion bestimmt – das
Spiel verwendet genau die Zeichen, die du bisher gelernt hast, und
passt sich damit automatisch deinem Niveau an. Jede Koch-Lektion bildet
eine „Stufe", innerhalb derer Sub-Level mit zunehmender Schwierigkeit
(höhere Fallgeschwindigkeit, häufigeres Erscheinen neuer Zeichen)
gebildet werden.

Morse Invaders ist nur auf dem M32Pocket verfügbar.

#### Spielstart {-}

Navigiere im Hauptmenü zu **Games > Morse Invaders**. Der
Spielmenü-Bildschirm zeigt deine aktuelle Koch-Lektion, die Zeichen
deines Pools, deine Geschwindigkeitseinstellung und eine
Bestenliste (sofern Ergebnisse vorhanden sind).

Im Spielmenü:

Drehe den ENCODER, um den Start-Sub-Level zu ändern. Drücke die
FN-Taste, um den ENCODER zwischen Sub-Level und Geschwindigkeit
umzuschalten (ein „\<"-Marker zeigt an, welchen Wert der ENCODER
gerade steuert). Berühre ein Paddle, um das Spiel zu starten. Ein
langer Druck auf den ENCODER kehrt zum Morserino-Menü zurück.

Nach dem Paddle-Berühren beginnt ein kurzer Countdown
(„3... 2... 1... GO!"), bevor das Spiel startet.

#### Spielablauf {-}

Zeichen erscheinen oben auf dem Bildschirm und fallen in einer von
sechs Bahnen nach unten. Jedes Zeichen wird als farbiges Rechteck
dargestellt: grün für Buchstaben, gelb für Zahlen, orange für
Satzzeichen und magenta für Betriebszeichen. Betriebszeichen werden
mit ihrer zweibuchstabigen Abkürzung angezeigt.

Um ein fallendes Zeichen zu zerstören, gib seinen Morsecode mit den
Paddles (oder einem externen Schlüssel) ein. Das Spiel zielt automatisch
auf das niedrigste – und damit dringlichste – Zeichen, das mit deiner
Eingabe übereinstimmt. Eine korrekte Übereinstimmung zerstört das
Zeichen mit einer kurzen Animation und vergibt Punkte. Stimmt kein
fallendes Zeichen mit dem eingegebenen Zeichen überein, zählt das als
Fehler und dein Streak wird zurückgesetzt, aber du verlierst kein Leben.

Wenn ein Zeichen den unteren Bildschirmrand erreicht, verlierst du eines
deiner drei Leben. Das Spiel endet, wenn alle Leben verloren sind.

#### Display-Layout {-}

Der Bildschirm ist in drei Bereiche unterteilt:

Die **obere Leiste (HUD)** zeigt links deine verbleibenden Leben als
farbige Punkte, in der Mitte das aktuelle Level (als Koch-Lektion –
Sub-Level, z.B. „12-3") und rechts deinen Punktestand.

Der **Hauptbereich** zeigt die sechs Bahnen mit fallenden Zeichen.

Die **untere Leiste (Tastenbereich)** zeigt links deine aktuelle
Geschwindigkeit in WpM, einen Streak-Zähler ab fünf aufeinanderfolgenden
Treffern und in der Mitte das zuletzt eingegebene Zeichen.

#### Steuerung im Spiel {-}

ENCODER: Keying-Geschwindigkeit (WpM) ändern. Die Geschwindigkeit wird
sofort übernommen und beim Beenden des Spiels gespeichert.

ENCODER-Klick (kurzer Druck): Spiel pausieren. Erneut klicken zum
Fortsetzen.

ENCODER langer Druck: Spiel beenden und zum Morserino-Menü zurückkehren.

FN kurzer Druck: Zwischen Geschwindigkeits- und Lautstärkeregelung
umschalten.

#### Punktewertung {-}

Basispunkte pro zerstörtem Zeichen: 10, multipliziert mit mehreren
Faktoren:

* Geschwindigkeitsmultiplikator: deine aktuelle WpM geteilt durch 10.
  Bei 20 WpM erhältst du doppelte Punkte; bei 30 WpM dreifache.

* Dringlichkeitsbonus: Das Zerstören eines Zeichens, das sich in den
  unteren 30 % des Bildschirms befindet, bringt doppelte Punkte.

* Streak-Bonus: Aufeinanderfolgende Treffer ohne Fehlschlag erhöhen den
  Multiplikator – ×1,5 ab 5 Treffern, ×2 ab 10, ×3 ab 20.

Ein zusätzliches Leben wird alle 1.000 Punkte vergeben, bis zu maximal
5 Leben.

#### Level {-}

Alle 10 zerstörten Zeichen wechselst du zum nächsten Sub-Level innerhalb
deiner Koch-Lektion. Jeder Sub-Level erhöht die Fallgeschwindigkeit,
die Erscheinungsrate und die maximale Anzahl gleichzeitiger Zeichen auf
dem Bildschirm.

Beim Aufstieg erscheint kurz ein „LEVEL"-Bildschirm, und alle
verbleibenden Zeichen werden für einen kleinen Bonus gelöscht.

#### Bestenliste {-}

Das Spiel speichert die fünf besten Ergebnisse. Jeder Eintrag enthält
Punktzahl, Koch-Lektion, erreichter Sub-Level und Geschwindigkeit.
Die Bestenliste wird sowohl im Spielmenü als auch im Game-Over-Bildschirm
angezeigt. Ein neuer Bestwert wird hervorgehoben.

#### Game Over {-}

Wenn alle Leben verloren sind, zeigt der Game-Over-Bildschirm deine
Endpunktzahl, das erreichte Level, deine Geschwindigkeit sowie
Statistiken (Treffer, Fehlschläge, verpasste Zeichen und Trefferquote).

Klicke den ENCODER, um erneut zu spielen (ab demselben Sub-Level wie
zu Beginn), oder drücke den ENCODER lang, um zum Morserino-Menü
zurückzukehren.

#### Tipps für Anfänger {-}

Beginne mit einer niedrigen Koch-Lektion (weniger Zeichen) und einer
angenehmen Geschwindigkeit. Das Spiel ist als Trainingstool am
wirksamsten, wenn du gefordert, aber nicht überfordert bist. Wenn
Zeichen zu schnell den Boden erreichen, verringere den Start-Sub-Level
oder die WpM. Mit fortschreitender Erkennungsleistung kannst du die
Koch-Lektion erhöhen, um mehr Zeichen in den Pool aufzunehmen.

### Fight the Pileup

**Fight the Pileup** ist ein CW-Trainingsspiel, bei dem du Rufzeichen
unter Zeitdruck kopieren und zurückgeben musst – ganz wie beim Arbeiten
eines echten Pileups auf dem Band. Rufzeichen kommen als CW-Audio; du
dekodierst sie nach Gehör und gibst sie zurück, bevor sie aufgeben. Es
kann allein gegen das Gerät oder als Mehrspieler-Match „letzte Station,
die übrig bleibt" gegen andere M32 Pockets im selben Raum gespielt werden.

Fight the Pileup ist nur auf dem M32 Pocket verfügbar.

#### Spielstart {-}

Navigiere im Hauptmenü zu **Games → Fight Pileup**. Wenn du das Spiel
zum ersten Mal spielst, wirst du aufgefordert, dein Rufzeichen und
deinen Namen über den ENCODER einzugeben:

- **ENCODER**: Zeichen auswählen
- **Klick**: Zeichen hinzufügen
- **FN (kurzer Klick)**: Letztes Zeichen löschen
- **Langer Druck**: Bestätigen (mindestens 2 Zeichen erforderlich)

Rufzeichen und Name werden gespeichert und für zukünftige Sitzungen
gemerkt.

#### Die Lobby {-}

In der Lobby richtest du ein Spiel ein, bevor du den Pileup betrittst:

- **ENCODER**: Schwierigkeitsgrad wählen (EASY, NORMAL, HARD, EXPERT)
- **Klick**: Zwischen Einzelspieler- und Mehrspielermodus umschalten
- **FN (kurzer Klick)**: Zwischen Rufzeichen- und Namensanzeige
  umschalten (wenn beides gesetzt ist)
- **Paddles**: Pileup betreten (Code-Aufgabe startet)
- **Langer Druck**: Zum Menü zurückkehren

Der Schwierigkeitsgrad beeinflusst, wie lange jeder Anrufer auf dich
wartet, wie viele Drops ein Leben kosten, wie schnell neue Anrufer
erscheinen und wann der Rufzeichen-Hinweis eingeblendet wird:

| Level  | Anrufer-Timeout | Drops pro Leben | Abspiele bis Hinweis |
|--------|-----------------|-----------------|----------------------|
| EASY   | 45 Sek          | 5               | 3                    |
| NORMAL | 30 Sek          | 4               | 3                    |
| HARD   | 20 Sek          | 3               | 2                    |
| EXPERT | 12 Sek          | 2               | 1                    |

Im **Mehrspielermodus** zeigt die Lobby zusätzlich den ESP-NOW-**Kanal**
(Standard oder Secondary) sowie eine aktuelle Liste der gefundenen
Mitspieler. Alle Geräte müssen auf denselben **Kanal** eingestellt sein,
um sich gegenseitig zu sehen – das ist die LoRa/ESP-NOW-Kanal-Einstellung.

#### Code-Aufgabe (Eingangstor) {-}

Bevor der Pileup beginnt, gibst du einen kurzen zufälligen Code (4 Zeichen)
auf deinen Paddles ein – eine Aufwärmübung, die zugleich bestätigt, dass
deine Paddles funktionieren. Jedes korrekte Zeichen leuchtet grün auf;
bei einem Fehler wird die Sequenz zurückgesetzt, versuche es einfach
erneut. Du kannst hier bereits deine Gebe-**Geschwindigkeit** einstellen:
der **ENCODER** ändert die WpM, **FN** schaltet auf Lautstärke um, und der
aktuelle Wert wird angezeigt. Langer Druck bricht ab und kehrt zur Lobby
zurück.

#### Spielablauf {-}

Rufzeichen („Anrufer") treffen ein und reihen sich in eine Warteschlange
ein. Du bearbeitest immer den **ältesten** zuerst – das ist der aktive
Anrufer, angezeigt unter **DEFEND!**

1. **Hören.** Der aktive Anrufer wird als CW-Audio mit einer etwas
   tieferen Tonhöhe als dein Mithörton abgespielt, damit du ihn von
   deiner eigenen Eingabe unterscheiden kannst. Er wiederholt sich
   automatisch.

2. **Dekodieren.** Identifiziere das Rufzeichen nach Gehör. Nach einigen
   Abspielungen (3 bei Easy und Normal, 2 bei Hard, 1 bei Expert) wird
   der Text als Hinweis eingeblendet – versuche aber, schneller zu sein.

3. **Zurückgeben.** Gib das Rufzeichen mit den Paddles ein; deine
   dekodierten Zeichen erscheinen währenddessen. Eine Pause in Wortlänge
   (etwa eine Sekunde) gibt automatisch ab, oder klicke den ENCODER, um
   sofort abzugeben.

4. **Ergebnis.**
   - **Richtig** – der Anrufer ist erledigt, du bekommst Punkte und
     verdienst sofort einen **Angriff** (siehe unten).
   - **Falsch** – das CW wird erneut abgespielt, sodass du denselben
     Anrufer nochmals versuchen kannst; innerhalb seines Zeitfensters
     gibt es keine Begrenzung der Versuche.

Jeder Anrufer hat sein eigenes Zeitfenster, angezeigt durch den
Fortschrittsbalken (grün → gelb → rot). Das Fenster startet, sobald der
Anrufer zum *aktiven* wird, sodass jeder Anrufer eine faire, volle Chance
erhält. Läuft das Fenster des aktiven Anrufers ab, gilt er als **Drop**.
Und wenn sich Anrufer schneller stapeln, als du sie abarbeitest, geben
die hinten Wartenden schließlich **auf** („MISSED!") – das zählt ebenfalls
als Drop. Den Stapel kurz zu halten ist die eigentliche Herausforderung.

#### Angriffe verdienen und geben {-}

Jeder korrekt kopierte Anrufer bringt dir einen **Angriff** ein. Der
Pileup pausiert, und eine magentafarbene Aufforderung – **YOUR ATTACK —
SEND:** gefolgt von einem Rufzeichen – erscheint. Gib dieses Rufzeichen
mit den Paddles, um es zu „senden". Im **Einzelspielermodus** bringt das
einfach einen Bonus; im **Mehrspielermodus** wird das Rufzeichen auf einen
anderen Spieler abgefeuert und landet in *dessen* Stapel als Anrufer, den
er abwehren muss (siehe *Mehrspielermodus* unten). Während du einen Angriff
gibst, ist der Pileup eingefroren – es kommen keine Anrufer und keiner
läuft ab – du kannst dir also Zeit lassen. Mit dem Senden des Angriffs
kehrst du zur Abwehr zurück.

#### Leben und Game Over (Einzelspieler) {-}

Du startest mit **3 Leben**. Alle *N* Drops (die „Drops pro Leben" deines
Schwierigkeitsgrads) kosten ein Leben; sowohl abgelaufene Anrufer als auch
„MISSED"-Aufgaben zählen. Wenn dein letztes Leben weg ist, endet das Spiel,
und der Game-Over-Bildschirm zeigt deinen Punktestand, abgewehrte vs.
verlorene Rufzeichen, die Genauigkeit, die beste Streak und die verwendeten
Einstellungen. Drücke **Klick**, um erneut zu spielen (zurück zur Lobby),
oder **Langer Druck**, um zum Hauptmenü zurückzukehren.

#### Punktewertung {-}

| Ereignis | Punkte |
|----------|--------|
| Richtiger Anrufer | +100 Basis + 10 × Streak |
| Angriff gesendet  | +50 |
| Falsche Antwort   | −50 |
| Abgelaufener Anrufer | −25 |

Deine **Streak** wächst mit jedem korrekten Kopieren und wird bei einer
falschen Antwort oder einem abgelaufenen Anrufer zurückgesetzt; eine höhere
Streak bringt mehr Bonus pro Kopie – und lässt neue Anrufer schneller
eintreffen.

#### Steuerung im Spiel {-}

| Steuerung | Aktion |
|-----------|--------|
| **Paddles** | Antwort (oder Angriffs-Rufzeichen) eingeben |
| **ENCODER** | WpM-Geschwindigkeit oder Lautstärke anpassen (siehe FN) |
| **Klick** (ENCODER) | Antwort sofort abgeben |
| **FN (kurzer Klick)** | ENCODER zwischen **Geschwindigkeit** und **Lautstärke** umschalten |
| **Langer Druck** (ENCODER) | Spiel beenden |

Die untere Statuszeile zeigt den Wert, den der ENCODER gerade steuert –
`15 wpm` oder `Vol 12` – und mit **FN** wechselst du zwischen beiden.

#### Mehrspielermodus {-}

Im Mehrspielermodus spielen zwei oder mehr M32 Pockets im selben Raum
gleichzeitig und greifen sich gegenseitig an; der **letzte Spieler, der
noch im Spiel ist, gewinnt**.

**Einrichtung.** Öffne auf jedem Gerät Fight the Pileup und schalte in der
Lobby mit **Klick** auf **Multiplayer** um. Achte darauf, dass alle Geräte
denselben **Kanal** anzeigen (Standard oder Secondary). Innerhalb weniger
Sekunden listet jedes Gerät die anderen unter MULTIPLAYER auf. Wenn alle
bereit sind, startet jeder Spieler durch Eingeben auf den Paddles – die
Spieler müssen nicht exakt gleichzeitig starten.

**Angriffe.** Immer wenn du einen Anrufer korrekt kopierst und deinen
verdienten Angriff gibst, wird dieses Rufzeichen zufällig an einen der
anderen Spieler gesendet. Auf dessen Gerät erscheint es als Anrufer mit der
Kennzeichnung **FROM** plus deinem Rufzeichen, in Magenta, und muss wie
jeder andere abgewehrt werden; ebenso landen die Rufzeichen, die die
anderen senden, in *deinem* Stapel. Bot-Anrufer kommen ebenfalls weiterhin,
sodass der Druck nie nachlässt.

**Gegner.** Während des Pileups zeigt ein kompakter Streifen am oberen
Bildschirmrand die anderen Spieler und ihre verbleibenden Leben, die sich
live aktualisieren, wenn sie Schaden nehmen; ein ausgeschiedener Spieler
wird abgedunkelt dargestellt.

**Gewinnen.** Wenn du dein letztes Leben verlierst, bist du draußen; dein
Bildschirm zeigt die Game-Over-Statistik und wartet auf das Ergebnis. Der
letzte noch lebende Spieler gewinnt: dessen Gerät zeigt **YOU WIN!**, und
jedes andere Gerät zeigt **WINNER:** gefolgt vom Rufzeichen des Gewinners.
Von dort kehrt **Klick** zur Lobby für ein weiteres Match zurück, und
**Langer Druck** beendet das Spiel.

#### Tipps für Anfänger {-}

- **Beginne auf EASY.** Das 45-Sekunden-Fenster lässt dich jeden Anrufer
  mehrmals hören, bevor du dich festlegst.

- **Nutze den Hinweis.** Wenn du unsicher bist, warte, bis das Rufzeichen
  eingeblendet wird – Warten kostet keine Punkte, nur das Fallenlassen von
  Anrufern.

- **Langsamer werden.** Senke die WpM mit dem ENCODER; Anrufer werden mit
  deiner eigenen Gebegeschwindigkeit abgespielt.

- **Lerne die Muster.** Rufzeichen folgen Mustern: ein 1–3-buchstabiges
  Präfix, eine Ziffer, dann ein 1–3-buchstabiges Suffix (z.B. DL1ABC, W3XY,
  VK2GR).

- **Halte den Stapel kurz.** Besonders im Mehrspielermodus: arbeite Anrufer
  zügig ab – wächst die Warteschlange, geben die ältesten Anrufer auf und
  kosten dich Leben.

### Radio Cave

**Radio Cave** ist ein textbasiertes Abenteuerspiel, inspiriert vom
Klassiker *Colossal Cave Adventure*. Du spielst einen Funkamateur, der
eine verlassene Kalter-Krieg-Radiostation entdeckt, die in einer
Berghöhle verborgen ist. Deine Mission: Die Station erkunden, die
Ausrüstung reparieren und ein QSO mit einer Gegenstation abschließen,
die seit über 50 Jahren ruft.

Im Gegensatz zu den anderen Spielen gibt es weder Punktewertung noch
Zeitdruck. Stattdessen ist es ein rätsellösungsbasiertes
Erkundungsspiel, bei dem du Räume erkundest, Gegenstände sammelst und
Aufgaben löst – alles durch Eingabe von Befehlen als Morsecode. CW-Hinweise
sind in die Spielwelt eingebettet: Einige Informationen werden nur als
Morse-Audio übermittelt, und die finale Aufgabe erfordert ein echtes
QSO-Austausch auf den Paddles.

Radio Cave ist nur auf dem M32Pocket verfügbar.

#### Spielstart {-}

Navigiere im Hauptmenü zu **Games → Radio Cave**. Nach einem kurzen
Lobby-Bildschirm kannst du einen Buchstaben auf den Paddles eingeben, um das Spiel zu
betreten. Wenn ein
gespeicherter Spielstand vorhanden ist, wird dieser automatisch geladen
und du setzt dort fort; andernfalls beginnt ein neues Spiel.

#### Spielweise {-}

Das Spiel zeigt Textbeschreibungen auf dem TFT-Bildschirm an. Du
erkundest die Umgebung durch Eingabe kurzer Befehle als Morsecode über
die Paddles. Das Spiel versteht ganze Wörter, Abkürzungen und auch
einzelne Buchstaben als Shortcuts. Es gibt keinen Zeitdruck – nimm die
Geschwindigkeit, die sich für dich angenehm anfühlt.

#### Display-Layout {-}

Der Bildschirm ist in vier Bereiche unterteilt:

Die **obere Leiste** zeigt den aktuellen Raumnamen (und ein kleines
Symbol mit der Raumnummer).

Der **Hauptbereich** zeigt die Raumbeschreibung, Aktionsergebnisse,
untersuchte Objekte und den Hilfsbildschirm. Nutze den Scroll-Modus
(FN langer Druck), um bei längeren Texten zu scrollen.

Die **Eingabezeile** zeigt, was du gerade eingibst oder – nach
Abgabe eines Befehls – die Ergebnismeldung.

Die **untere Leiste** zeigt die verfügbaren Ausgänge (z.B. `N E W`),
die Anzahl der Inventargegenstände und den Schrittzähler (z.B.
`1/2 • 12`) sowie den aktuellen ENCODER-Modus und dessen Wert (z.B.
`17 wpm`, `vol 7` oder `scroll`).

#### Befehle {-}

Alle Befehle werden als Morsecode eingegeben. Du kannst ganze Wörter
oder Abkürzungen verwenden – das kürzeste eindeutige Präfix reicht
immer aus (z.B. `T F` statt `TAKE FUEL`). Wenn das Präfix im aktuellen
Kontext eindeutig ist, wird der Befehl sofort ausgeführt; nur bei
echten Mehrdeutigkeiten erscheint „Which one?".

| Befehl | Aktion |
|--------|--------|
| `N`, `S`, `E`, `W` | Norden, Süden, Osten oder Westen gehen |
| `L` / `LOOK` | Vollständige Raumbeschreibung erneut anzeigen |
| `LOOK [Objekt]` | Gegenstand oder Merkmal im aktuellen Raum untersuchen |
| `I` / `INV` | Inventar anzeigen |
| `T [Gegenstand]` / `TAKE` | Gegenstand aufheben |
| `D [Gegenstand]` / `DROP` | Gegenstand ablegen |
| `U [Gegenstand]` / `USE` | Gegenstand benutzen oder mit etwas interagieren |
| `FIX [Gegenstand]` / `SOLDER` | Gegenstand reparieren |
| `R [Gegenstand]` / `READ` | Dokument oder Inschrift lesen |
| `KEY [Phrase]` | Eine CW-Phrase an einem Puzzleschloss eingeben |
| `QRS` | Den letzten CW-Hinweis mit halber Geschwindigkeit wiederholen |
| `H` / `HELP` | Befehlsübersicht anzeigen |
| `NEW` | Neues Spiel starten (Bestätigung mit `Y` erforderlich) |

Im Spiel erkannte CW-Phrasen können auch ohne den Präfix `KEY` direkt
eingegeben werden – das Spiel behandelt sie entsprechend.

#### Fehlerkonventionen {-}

Zwei echte CW-Konventionen werden zum Leeren des Eingabepuffers
akzeptiert:

- **`<hh>`** – acht als ein Zeichen eingegebene Dits (das Standard-
  „Fehler, bitte nochmals"-Betriebszeichen).
- **`eeee`** – vier aufeinanderfolgende E's als separate Buchstaben.

Beides leert den Puffer, sodass du deinen Befehl von vorne beginnen
kannst.

#### Inventar {-}

Du kannst bis zu **zwei Gegenstände** gleichzeitig tragen. Wenn deine
Hände voll sind, musst du erst etwas ablegen, bevor du einen neuen
Gegenstand aufheben kannst. Das begrenzte Inventar ist Absicht – du
musst planen, welche Gegenstände wohin gehören.

#### CW-Hinweise {-}

Einige Informationen im Spiel werden als CW-Audio statt als Bildschirmtext
übermittelt. Diese Hinweise werden mit verschiedenen Geschwindigkeiten
abgespielt, genau wie verschiedene Funker mit unterschiedlichen
Geschwindigkeiten senden. Das Spiel ignoriert deine Koch-Lektion –
alle Zeichen und Betriebszeichen, einschließlich `<sk>`, können
vorkommen.

Wenn ein CW-Hinweis zu schnell ist, gib `QRS` (oder `PSE QRS`) ein,
um eine langsamere Wiederholung anzufordern – genauso wie auf dem Band.
Du kannst QRS beliebig oft verwenden.

#### Steuerung {-}

| Steuerung | Aktion |
|-----------|--------|
| **Paddles** | Morsebefehle eingeben |
| **ENCODER** | Im Standardmodus: Keying-WpM anpassen. Nach FN kurzer Druck: Lautstärke anpassen. Nach FN langer Druck: Haupttextbereich scrollen. |
| **FN kurzer Druck** | ENCODER zwischen Geschwindigkeit (WpM) und Lautstärke umschalten |
| **FN langer Druck** | Scroll-Modus ein-/ausschalten |
| **ENCODER-Klick** | (Nur in der Lobby) Spiel ohne Paddle-Eingabe betreten |
| **ENCODER langer Druck** | Radio Cave beenden und zum Hauptmenü zurückgehen |

Die Keying-Geschwindigkeit (WpM) ist während des Spiels jederzeit
anpassbar und unabhängig von den CW-Hinweisgeschwindigkeiten.

*Der Scroll-Modus ist wichtig, da einige Beschreibungen nicht auf den
Bildschirm passen und du scrollen musst, um alles zu lesen!*

#### Speichern und Fortsetzen {-}

Das Spiel speichert deinen Fortschritt automatisch nach jedem Befehl.
Du kannst den Morserino jederzeit ausschalten und genau dort fortsetzen,
wo du aufgehört hast – auch wenn das Gerät durch den Time-Out mitten
im Spiel abschaltet.

Der Spielstand wird automatisch gelöscht, wenn du das Spiel gewinnst
oder stirbst. Beim nächsten Eintritt in Radio Cave beginnst du von vorne.

Um ein völlig neues Spiel zu starten, während ein Spielstand vorhanden
ist, gib `NEW` ein und bestätige mit `Y`. Jede andere Antwort bricht ab.

#### Der Schrittzähler {-}

Jeder eingegebene Befehl erhöht den Schrittzähler, der in der unteren
Leiste angezeigt wird. Der Zähler bleibt beim Speichern und Fortsetzen
erhalten. Bei Spielende wird die Gesamtschrittanzahl angezeigt – eine
persönliche Bestmarke, falls du einen effizienteren Durchlauf anstreben
möchtest.

#### Ein paar Hinweise {-}

- Erkunde gründlich. Schau dir Dinge an. Lies Dinge. Nicht alles ist
  auf den ersten Blick offensichtlich.

- Achte auf CW-Hinweise – sie enthalten Informationen, die du
  nirgendwo anders bekommst.

- Wenn etwas nicht funktioniert, fehlt vielleicht ein Schritt, den du
  noch nicht gemacht hast. Überlege, was ein echter Funkamateur tun müsste.

- Es gibt einen fatalen Fehler in diesem Spiel. Die Station warnt davor.
  Lies die Anleitung – die im Spiel, wohlgemerkt.

- Das Spiel ist lösbar, und jedes Rätsel hat eine logische Lösung,
  die in echter Funkpraxis und CW-Konventionen verwurzelt ist.

#### Game Over {-}

Es gibt genau einen fatalen Fehler in Radio Cave. Wenn er passiert,
erscheint ein Todesbildschirm; gib einen beliebigen Buchstaben ein oder
klicke den ENCODER, um von vorne zu starten. Der Spielstand wird
gelöscht und der Schrittzähler zurückgesetzt.

Es gibt auch eine Situation, in der das Spiel durch wiederholte Fehler
unlösbar werden kann (du wirst es bemerken). In diesem Fall gib `NEW`
ein und bestätige mit `Y`, um neu zu starten.

Wenn du das Spiel erfolgreich abschließt, zeigt ein Siegesbildschirm
deine Gesamtschrittanzahl. Herzlichen Glückwunsch – die Nachricht wurde
endlich empfangen.

### Morsel

**Morsel** ist ein Worträtsel-Spiel: Du musst ein verborgenes Wort
erraten, mit zwei Hinweisen — ein zufällig gewählter Buchstabe des
Wortes wird im Klartext am Display angezeigt, und das ganze Wort wird
in Morsecode wiedergegeben. Der CW-Hinweis beginnt schnell — 48 WpM —
und wird mit jedem nicht gelösten Versuch um 5 WpM langsamer, bis
hinunter auf 18 WpM. Wer mit weniger Versuchen löst, wird belohnt; das
Spiel umfasst 10 Wörter, gewertet wird die niedrigste angepasste
Gesamtzeit.

Morsel ist nur am M32Pocket verfügbar.

#### Starten des Spiels {-}

Navigiere im Hauptmenü zu **Games → Morsel**. Die Lobby zeigt die
aktuell eingestellte **Koch-Lektion** und **Wortlänge** sowie die
verfügbaren Bedienelemente. Berühre ein Paddle, um zu starten.

#### Einstellungen in der Lobby {-}

Zwei Einstellungen können direkt in der Lobby geändert werden:

| Bedienelement | Wirkung |
|---------------|---------|
| **Encoder (Drehknopf)** | Koch-Lektion ändern — nur für diese Spielsitzung; deine Trainingseinstellung wird beim Verlassen wiederhergestellt |
| **FN kurz drücken** | Wortlänge durchschalten: `3`, `4`, `max 4`, `5`, `max 5`, `6`, `max 6`. Über Stromzyklen hinweg gespeichert. `max N` bedeutet beliebige Länge von 3 bis N. |
| **FN lang drücken** | Persistente Highscore-Tabelle anzeigen |
| **Encoder-Klick oder Paddle-Berührung** | Spiel starten |
| **Encoder lang drücken** | Zurück zum Hauptmenü |

Wörter werden aus der kombinierten Morserino-Wortliste und der
Liste der Amateurfunk-Abkürzungen gezogen, gefiltert nach Länge und
aktueller Koch-Lektion (jeder Buchstabe jedes Kandidatenwortes muss
sich unter den bereits gelernten Zeichen befinden). Sind zu wenige
Kandidaten verfügbar, zeigt Morsel den Hinweis „Word pool too small"
mit einer empfohlenen Mindest-Koch-Lektion und kehrt zur Lobby zurück.

#### Bildschirmlayout {-}

Während des Spiels zeigt der Querformat-Bildschirm:

- **Oben links:** die aktuelle Rundennummer (1, 2, 3 …) für das
  aktuelle Wort.
- **Oben Mitte:** Spielfortschritt, z. B. `3/10`.
- **Oben rechts:** die aktuelle CW-Hinweisgeschwindigkeit in WpM
  (gelb hervorgehoben, sobald die 18-WpM-Untergrenze erreicht ist).
- **Mitte:** eine Reihe von Buchstabenfeldern, eines pro Position.
  Der enthüllte Buchstabe wird in Cyan mit cyanfarbenem Rahmen
  gezeigt, sodass die Position des Hinweises auf einen Blick
  erkennbar ist.
- **Statuszeile unten:** die aktuelle Nachricht („Listen and key
  the word", „Correct!", „Try again", „Skipped" …).
- **Hinweiszeile unten:** `click = skip   hold = quit`.
- **Untere HUD-Zeile:** Koch-Lektion · deine **key**-WpM ·
  Lautstärke. Das aktive Encoder-Ziel (key WpM oder Lautstärke) ist
  gelb hervorgehoben.

#### Spielablauf pro Wort {-}

Ein neues Wort wählt zufällig einen Buchstaben zur Enthüllung und
spielt das ganze Wort einmal mit der aktuellen Rundengeschwindigkeit
ab. Anschließend gibst du **das vollständige Wort mit den Paddles ein**,
einschließlich des enthüllten Buchstabens. Während du gibst, erscheint
jeder dekodierte Buchstabe in seinem Feld. Der CW-Hinweis wird
stummgeschaltet, sobald du mit dem Geben beginnst.

Eine lange Pause zwischen den Wörtern — sobald deine Eingabe die volle
Wortlänge erreicht hat — bestätigt die Eingabe. Jedes Feld wird dann
gefärbt:

| Farbe | Bedeutung |
|-------|-----------|
| **Grün** | Richtiger Buchstabe an richtiger Position |
| **Rot** | Falscher Buchstabe |
| **Grau** | Position des enthüllten Buchstabens falsch gegeben (zur Erinnerung wird der ursprünglich enthüllte Buchstabe gezeigt) |

Hast du weniger als die volle Wortlänge eingegeben, **löst die Pause
nicht aus** — das ist beabsichtigt, damit du mitten im Wort denken
oder korrigieren kannst, ohne dass die Eingabe ungewollt vorzeitig
abgeschickt wird.

Ist nicht jedes Feld grün, wird dasselbe Wort in der nächsten Runde
um 5 WpM langsamer abgespielt und du versuchst es erneut. Sind alle
Felder grün, ist das Wort gelöst; der Bildschirm zeigt kurz die
Lösungszeit und die Anzahl der Versuche, dann startet ein neues Wort
wieder bei 48 WpM.

#### Fehlerkorrektur {-}

Das Prosign `<err>` (8 Punkte hintereinander, auch als `<hh>` notiert)
löscht das zuletzt eingegebene Zeichen — die übliche Konvention auf
dem Band, eine Eingabe abzubrechen und neu zu senden. Durch das Löschen
fällt deine Eingabe wieder unter die volle Wortlänge und die
automatische Bestätigung wird deaktiviert, sodass du in Ruhe denken
und neu eingeben kannst.

#### Schwieriges Wort überspringen {-}

Wenn ein Wort selbst bei 18 WpM partout nicht erkennbar ist, drücke
den **Encoder-Knopf (Mode-Taste)** kurz, um es zu überspringen. Das
aktuelle Wort wird beendet, ein Hinweis `Skipped (-60s)` erscheint,
und das Spiel geht weiter. Der Skip fügt einen festen **60-Sekunden-
Aufschlag** zur Gesamtzeit hinzu, plus 5 Sekunden für jeden Versuch,
den du vor dem Skip bereits abgegeben hattest.

#### Verhalten bei Inaktivität {-}

Wenn ein CW-Hinweis abgespielt ist und du noch nicht zu geben begonnen
hast:

- Nach etwa **12 Sekunden** ohne Eingabe wiederholt Morsel den
  CW-Hinweis automatisch (mit derselben Geschwindigkeit — die Runde
  zählt **nicht** weiter).
- Nach etwa **60 Sekunden** Gesamtinaktivität kehrt Morsel
  schonend zur eigenen Lobby zurück (das Spiel wird abgebrochen, es
  wird keine Wertung gespeichert).

Jede Eingabe, Encoder-Drehung oder Tastenbetätigung setzt beide
Zeitgeber zurück, ebenso wie den globalen Abschalt-Timeout — eine
aktiv spielende Person wird also nie aus dem laufenden Spiel
geworfen.

#### Bedienelemente im Spiel {-}

| Bedienelement | Aktion |
|---------------|--------|
| **Paddles** | Eingabe der Vermutung |
| **Encoder** | Eigene **Gebegeschwindigkeit** (key-WpM) einstellen — unabhängig von der Hinweisgeschwindigkeit |
| **FN kurz drücken** | Encoder zwischen key-WpM und Lautstärke umschalten |
| **FN lang drücken** | Zurück zum Hauptmenü |
| **Mode-Taste kurz drücken** | Aktuelles Wort überspringen (60 s Strafzeit) |
| **Mode-Taste lang drücken** | Zurück zum Hauptmenü |

Der CW-Hinweis wird stets in der für die Runde vorgesehenen
Geschwindigkeit gespielt, unabhängig von deiner Gebegeschwindigkeit —
die beiden sind voneinander unabhängig.

#### Bewertung {-}

Für jedes Wort:

- Die **Zeit** wird vom ersten CW-Abspielen bis zur korrekten Eingabe
  gemessen.
- **+5 Sekunden Aufschlag pro abgegebener Eingabe** (ab dem ersten
  Versuch). Wer mit weniger Versuchen löst, wird belohnt.
- Ein **übersprungenes** Wort fließt mit der bis dahin verstrichenen
  Zeit zuzüglich der 60-Sekunden-Strafe und 5 Sekunden pro vorher
  bereits abgegebenen Versuch in die Wertung ein.

Die Gesamtbewertung ist die Summe der angepassten Zeiten aller 10
Wörter. Niedriger ist besser.

#### Spielende und Highscores {-}

Nach 10 Wörtern erscheint ein **GAME OVER**-Bildschirm mit:

- der angepassten Gesamtzeit im Format `M:SS`,
- gelösten vs. übersprungenen Wörtern,
- der Gesamtzahl der Versuche,
- und — falls deine Gesamtzeit eine Position in der persistenten
  Top-7-Liste belegt — einem Banner `NEW HIGH SCORE — rank N`.

Ein Klick auf den Encoder öffnet die **Highscore-Tabelle**: die
sieben besten Ergebnisse, sortiert nach angepasster Gesamtzeit,
gespeichert im nichtflüchtigen Speicher. Jede Zeile zeigt Rang, Zeit,
Wortlängeneinstellung (`max 4`, `5`, …), die Koch-Lektion (z. B.
`K12`) sowie das Verhältnis gelöst/gesamt. Dein letzter qualifizierter
Eintrag wird grün hervorgehoben.

Die Highscore-Tabelle kann jederzeit auch aus der Lobby aufgerufen
werden — mit einem langen Druck auf die **FN**-Taste.

#### Tipps {-}

- 48 WpM sind bewusst schnell — sie belohnen Decoder, die schnelles
  CW kopieren können. Schaffst du es in der ersten Runde nicht, warte
  auf die nächste Wiedergabe; sie ist 5 WpM langsamer.

- Der enthüllte Buchstabe ist ein starker Anker — sobald seine
  Position bekannt ist, ist das Wort schon stark eingegrenzt. Achte
  auf seine cyanfarbene Positionsnummer unter dem Feld.

- Lösen im **ersten Versuch** ist deutlich wertvoller als langsames
  Lösen über mehrere Runden — der Aufschlag von 5 s pro Versuch
  summiert sich schnell.

- Höhere Koch-Lektion einstellen, falls du wiederholt die Meldung
  „Pool zu klein" siehst: 3-stellige Wörter brauchen Lektion 8,
  4-stellige Lektion 10, 5-stellige Lektion 14, 6-stellige Lektion 16
  (bei der Standard-Morserino-Zeichenreihenfolge).

- Korrigiere ruhig (`<err>` = 8 Punkte) — die Zeit läuft zwar weiter,
  aber eine falsch abgegebene Eingabe kostet 5 s; meist ist es
  günstiger, den Fehler zu beheben als die Eingabe abzuschicken und
  es erneut zu versuchen.

#### Multiplayer {-}

Morsel kann auch als synchrones Mehrspieler-Match gegen andere
M32 Pockets im gleichen Raum gespielt werden: Ein Gerät übernimmt die
Rolle des **Servers**, beliebig viele weitere die der **Clients**.
Alle Spieler geben dieselben 10 Wörter zeitgleich ein; am Ende
verteilt der Server eine sortierte Rangliste, die auf allen Geräten
zugleich erscheint.

##### Mehrspieler-Spiel starten {-}

Wähle nach **Games → Morsel** im ersten Bildschirm **Multiplayer**.
Anschließend stehen zwei Rollen zur Wahl:

| Rolle | Aktion |
|-------|--------|
| **Start as Server** | Öffnet die Server-Lobby. Stelle die Koch-Lektion mit dem Encoder und die Wortlänge mit kurzem FN-Druck ein; in der Liste erscheinen die beigetretenen Spieler, sobald sie sich verbinden. Ein Klick auf den Encoder startet das Spiel. |
| **Join a game** | Öffnet die Client-Lobby. Sucht den Beacon eines Servers und zeigt dessen Einstellungen (Länge, Koch-Lektion, Wortanzahl), sobald einer gefunden ist. Warte, bis der Server-Bediener startet. |

Ein langer Druck auf den Encoder geht eine Ebene zurück: von einer Server- oder Client-Lobby zur Rollenauswahl, von dort zur Auswahl Einzel-/Mehrspieler und schließlich aus Morsel heraus.

Deine Kennung im Netz wird aus dem mit „Fight the Pileup" geteilten
**Rufzeichen / Spielernamen** übernommen. Ist nichts gesetzt, wird
automatisch eine kurze MAC-basierte Kennung verwendet — setze dein
Rufzeichen (`playerCall`) einmalig über das M32 Configuration Tool,
um in der Rangliste unter diesem Namen aufzuscheinen.

##### Ablauf {-}

Der Server wählt 10 Wörter aus dem Pool und überträgt sie an alle
Clients; anschließend spielt jedes Gerät jedes Wort lokal mit der
gewohnten Einzelspieler-Mechanik durch (CW-Hinweis, Paddle-Eingabe,
farbcodiertes Ergebnis). Geschwindigkeits-Schema, Bewertung,
Fehlerkorrektur, Inaktivitätsverhalten und Überspringen funktionieren
identisch wie im Einzelspieler.

Sobald ein Spieler die 10 Wörter abgeschlossen hat, sendet sein Gerät
das Endergebnis (angepasste Zeit, Versuchszahl, gelöste Wörter) an
den Server. Der Server spielt selbst als gleichberechtigter Teilnehmer
mit — kein Host-Vorteil — und nimmt sein eigenes Ergebnis in die
Rangliste auf.

##### Ranglisten-Bildschirm {-}

Sobald irgendein Spieler fertig ist, sendet der Server fortlaufend
einen sortierten **Ranking**-Bildschirm, den alle Geräte identisch
darstellen:

| Spalte | Bedeutung |
|--------|-----------|
| Rang | Platz in der Wertung (1 ist am besten) |
| Spieler | Rufzeichen oder Name |
| Zeit | Angepasste Gesamtzeit in Sekunden |
| Gelöst | Gelöste Wörter von 10 |
| Versuche | Gesamtzahl der Versuche im Match |

Deine eigene Zeile ist gelb hervorgehoben. Die Anzeige aktualisiert
sich laufend, sobald weitere Spieler fertig werden; bis zu sechs
Spitzeneinträge werden gleichzeitig angezeigt, ein `+N more`-Hinweis
erscheint, wenn mehr Spieler ihr Ergebnis gemeldet haben. Ein
Encoder-Klick führt zurück in die Multiplayer-Lobby; ein langer Druck
verlässt Morsel.

##### Hinweise zum Funkverkehr {-}

Das Protokoll ist **ESP-NOW-Broadcast** auf einem festen Kanal —
weder ein Router noch Internet oder ein Pairing-Vorgang sind nötig;
alle Geräte in Funkreichweite empfangen einander direkt. Die weiche
Obergrenze liegt bei **20 Spielern** pro Match. Ein Client, der die
Verbindung zum Server verliert, wird nach etwa 8 Sekunden aus der
angezeigten Rangliste entfernt.

### Trailblazer

**Trailblazer** und **Fox Hunt** (nächster Abschnitt) sind ein
zusammengehöriges Paar von Gitter-Labyrinth-Spielen. Beide verwenden
dasselbe Spielfeld: ein Gitter aus 12 × 4 zufälligen Zeichen aus deiner
aktuellen Koch-Lektion, mit einer **Startzelle** am linken Rand, einer
**Zielzelle** am rechten Rand und einem verborgenen, gewundenen **Pfad**
dazwischen. Du folgst diesem Pfad Zelle für Zelle vom Start zum Ziel. Die
beiden Spiele unterscheiden sich nur darin, was du für jeden Schritt tun
musst — Trailblazer trainiert das **Geben**, Fox Hunt das **Aufnehmen**.

Bei **Trailblazer** ist die nächste Zelle des Pfads am Display
**hervorgehoben**. Du gibst einfach dieses sichtbare Zeichen in Morse, um
in die Zelle vorzurücken. Richtiges Geben bringt dich weiter und hebt die
nächste Zelle hervor; ein falsches Zeichen erzeugt einen Fehlerton, und du
bleibst stehen. Weil das Zielzeichen stets sichtbar ist, ist Trailblazer im
Kern eine **Gebeübung** — das Labyrinth gibt ihr das Gefühl einer Reise mit
sichtbarem Fortschritt und eine natürliche Wertungsachse (wie schnell und
sauber du dich bis zum Ziel gibst).

Trailblazer ist nur am M32 Pocket verfügbar. Es ist ab **Koch-Lektion 6**
spielbar.

#### Spielstart {-}

Navigiere im Hauptmenü zu **Games → Trailblazer**. Zuerst erscheint eine
Auswahl **Single player / Multiplayer** (Encoder drehen zum Wechseln,
Klick zum Auswählen). **Single player** öffnet den Bereitschafts-Bildschirm
mit Spielname, aktueller Koch-Lektion und den Bedienelementen. **Berühre
ein Paddle, um zu starten.** (Der Mehrspielermodus ist am Ende dieses
Abschnitts beschrieben.)

Am Bereitschafts-Bildschirm:

| Bedienelement | Aktion |
|---------------|--------|
| **Paddle / Taste** | Neues Labyrinth starten |
| **Encoder (Knopf)** | Deine Gebe-WpM ändern (oder Lautstärke — siehe unten) |
| **FN kurz** | Encoder zwischen Tempo und Lautstärke umschalten |
| **Encoder-Klick** | Bestenliste anzeigen |
| **Encoder lang** | Zurück zum Hauptmenü |

#### Ablauf {-}

Das gesamte Gitter wird angezeigt, deine aktuelle Position mit einem Token
markiert und die **nächste** Pfadzelle gelb hervorgehoben. Gib das
hervorgehobene Zeichen auf den Paddles. Jedes richtige Zeichen bringt dich in
die Zelle, zeichnet die Spur in Cyan hinter dir und hebt die folgende Zelle
hervor. Mach weiter, bis du die Zielmarke am rechten Rand erreichst — dann
zeigt das Display **Solved!** und deine Wertung.

Ein falsches Zeichen erzeugt nur einen kurzen Fehlerton und lässt dich stehen;
jede Fehleingabe zählt gegen deine Wertung, sauberes Geben lohnt sich also. Es
wird immer nur die Spur **hinter** dir gezeichnet — der Pfad voraus wird nie
gezeigt, du folgst ihm Zelle für Zelle.

Sobald deine Koch-Lektion sie erreicht hat, können auch Prozeichen als
Gitterzellen auftreten. Sie werden als übliche zweibuchstabige Abkürzung
(`AS`, `KA`, `KN`, `SK`, `VE`, `BK`, `AR`) mit Balken darüber dargestellt,
genau wie in den anderen Betriebsarten; gib das Prozeichen selbst, um in eine
solche Zelle vorzurücken.

#### Bedienung während des Spiels {-}

| Bedienelement | Aktion |
|---------------|--------|
| **Paddles** | Das hervorgehobene Zeichen geben |
| **Encoder** | Deine Gebe-WpM ändern |
| **FN kurz** | Encoder zwischen Tempo und Lautstärke umschalten |
| **Encoder lang** | Zurück zum Hauptmenü |

#### Wertung und Bestenliste {-}

Deine Wertung ist eine **effektive Geschwindigkeit in Zeichen pro Minute
(CPM)** — höher ist besser. Sie ist die Anzahl deiner Schritte (die Pfadlänge)
geteilt durch deine **angepasste Zeit**: die reine Lösungszeit plus einer
**5-Sekunden-Strafe für jede Fehleingabe**. Das Teilen durch die Pfadlänge
macht die Wertungen über verschieden lange Labyrinthe hinweg vergleichbar und
verwandelt das Ergebnis in eine aussagekräftige CW-Kennzahl: deine effektive
Gebegeschwindigkeit.

Nach dem Lösen zeigt ein Ergebnis-Bildschirm die CPM, die Anzahl der Schritte,
die Fehleingaben, deine Zeit und die gespielte Koch-Lektion; schafft es das
Ergebnis in die Top-7-Liste, erscheint ein `NEW HIGH SCORE`-Banner. Ein
Encoder-Klick öffnet die **Bestenliste** (für jedes Spiel getrennt, im
nichtflüchtigen Speicher gesichert); ein weiterer Druck startet ein neues
Labyrinth, ein langer Druck beendet das Spiel. Die Bestenliste lässt sich auch
vom Bereitschafts-Bildschirm aus mit einem Encoder-Klick öffnen.

#### Mehrspielermodus {-}

Trailblazer und Fox Hunt lassen sich beide als **Rennen auf demselben
Labyrinth** gegen andere M32 Pockets im selben Raum spielen. Ein Gerät ist der
**Server**: Es wählt die Koch-Lektion, erzeugt das Labyrinth und überträgt es,
sodass **jedes Gerät dasselbe Gitter und denselben Pfad** zeitgleich spielt.
Wer als Erster mit der besten effektiven Geschwindigkeit das Ziel erreicht,
gewinnt. Der Mehrspieler-Ablauf ist für beide Spiele gleich.

Wähle bei der Auswahl **Single player / Multiplayer** den Punkt
**Multiplayer**. Danach wählst du eine Rolle:

| Rolle | Aktion |
|-------|--------|
| **Start as Server** | Öffnet die Server-Lobby. Die Sitzung startet mit dem vollen Alphabet (Koch 41), damit eine gemischte Gruppe einen gemeinsamen Zeichensatz hat; mit dem Encoder kannst du ihn herunterregeln. Die Liste füllt sich mit den beitretenden Spielern. Ein Encoder-Klick startet das Rennen. |
| **Join a game** | Öffnet die Client-Lobby. Sie sucht den Beacon eines Servers und zeigt dessen Koch-Lektion, sobald er gefunden ist. Warte, bis der Server-Bediener startet; das Labyrinth kommt dann an und dein Rennen beginnt automatisch. |

Ein langer Druck auf den Encoder geht eine Ebene zurück — von einer Server-
oder Client-Lobby zur Rollenauswahl, von dort zur Auswahl
Einzel-/Mehrspieler und schließlich aus dem Spiel heraus.

Deine Kennung im Netz wird aus dem mit „Fight the Pileup" und Morsel geteilten
**Rufzeichen / Spielernamen** übernommen. Ist nichts gesetzt, wird automatisch
eine kurze MAC-basierte Kennung verwendet.

::: note
Beim Beitritt übernimmt dein Gerät für das Rennen die Koch-Lektion des Servers,
sodass das Gitter — und die Richtungstasten von Fox Hunt — für alle gleich
sind. Deine eigene Trainingslektion wird beim Verlassen des Spiels
wiederhergestellt.
:::

Wenn du fertig bist, meldet dein Gerät seine Zeit und die Zahl der
Fehleingaben; der Server erstellt die Rangliste (er selbst spielt als
gleichberechtigter Teilnehmer mit) und überträgt einen **Ranking**-Bildschirm,
den alle Geräte identisch anzeigen. Jede Zeile nennt Rang, Spieler, die
effektive CPM, die Zeit und die Zahl der Fehleingaben; deine eigene Zeile ist
gelb hervorgehoben. Ein Encoder-Klick führt zurück in die Multiplayer-Lobby
für ein weiteres Rennen, ein langer Druck beendet das Spiel.
Mehrspieler-Ergebnisse werden **nicht** in die Bestenlisten übernommen.

Das Funkprotokoll ist **ESP-NOW-Broadcast** auf einem festen Kanal — weder
Router noch Internet oder Pairing nötig; alle Geräte in Reichweite empfangen
einander, mit einer weichen Obergrenze von 20 Spielern.

### Fox Hunt

**Fox Hunt** ist das Aufnahme-Gegenstück zu Trailblazer, gespielt auf demselben
12 × 4-Gitter mit verborgenem Pfad vom linken zum rechten Rand (das gemeinsame
Spielfeld ist im Trailblazer-Abschnitt beschrieben). Der Unterschied liegt
darin, wie du jeden Schritt machst: Statt dir die nächste Zelle zu zeigen,
**spielt das Gerät ihr Zeichen in Morse**. Du musst das Zeichen nach Gehör
erkennen, herausfinden, welche der Nachbarzellen es enthält, und die
**Richtung** dorthin geben. Richtige Richtung → du rückst vor, und das nächste
Zeichen wird gespielt; falsch → ein Fehlerton, und du bleibst stehen.

Das trainiert das **Aufnehmen nach Gehör** mit einer räumlichen Note — du musst
das Zeichen dekodieren *und* es unter den Nachbarn verorten. Du könntest eine
der vier Richtungen blind raten, aber jede Fehleingabe kostet Wertung, das
Dekodieren ist also der effizientere Weg.

Fox Hunt ist nur am M32 Pocket verfügbar und ab **Koch-Lektion 6** spielbar.

#### Richtungstasten {-}

Richtungen werden als einzelne Buchstaben nach der **Kompass**-Konvention
gegeben — dieselbe, die Radio Cave verwendet:

| Richtung | Kompassbuchstabe |
|----------|------------------|
| Oben   | **N** (Nord) |
| Rechts | **E** (Ost) |
| Unten  | **S** (Süd) |
| Links  | **W** (West) |

Eine **Legende** mit vier Pfeilen wird ständig am Display gezeigt, sodass du dir
die Zuordnung nie merken musst. Da die Kompassbuchstaben in der Koch-Reihenfolge
nicht alle früh gelernt werden, wird jeder noch nicht gelernte automatisch durch
ein anderes Zeichen deiner aktuellen Lektion **ersetzt**; die Legende zeigt für
jede Richtung stets den tatsächlich zu gebenden Buchstaben (eine Ersatztaste ist
gelb dargestellt). Du gibst die **Richtung**, niemals das gehörte Zeichen selbst
— ein gegebener Buchstabe, der nicht in der Legende steht, gilt als Fehlzug.

#### Start und Bedienung {-}

Navigiere im Hauptmenü zu **Games → Fox Hunt** und wähle dann **Single player**
oder **Multiplayer** (der Mehrspielermodus funktioniert genau wie bei
Trailblazer oben — beide Spiele teilen sich eine Lobby). Berühre am
Einzelspieler-Bereitschaftsbildschirm ein Paddle, um zu starten.

| Bedienelement | Aktion |
|---------------|--------|
| **Paddles** | Die Richtung (N/E/S/W oder Ersatz) zum gehörten Zeichen geben |
| **Encoder** | Deine Gebe-WpM ändern (oder Lautstärke) |
| **FN kurz** | Encoder zwischen Tempo und Lautstärke umschalten |
| **Encoder-Klick (Bereitschaft)** | Bestenliste anzeigen |
| **Encoder-Klick (im Spiel)** | Das aktuelle Zeichen wiederholen |
| **Encoder lang** | Zurück zum Hauptmenü |

#### Ablauf {-}

Bei Ankunft an jeder Zelle spielt das Gerät das nächste Zeichen einmal, in
deinem aktuellen Gebetempo und leicht in der Tonhöhe verschoben, damit du es von
deinem eigenen Mithörton unterscheiden kannst. Dekodiere es, finde die
Nachbarzelle, die es enthält, und gib die Richtung dorthin. Bleibst du einige
Sekunden untätig, wird das Zeichen automatisch **wiederholt**; du kannst es
außerdem jederzeit mit einem **Encoder-Klick** erneut abspielen. Wie bei
Trailblazer wird nur die Spur hinter dir gezeichnet — der Pfad voraus wird nie
enthüllt, du musst dich also auf deine Ohren verlassen.

#### Wertung, Bestenliste und Mehrspieler {-}

Wertung, die Ergebnis- und Bestenlisten-Bildschirme sowie der Mehrspielermodus
funktionieren genau wie bei Trailblazer (effektive CPM mit 5-Sekunden-Strafe je
Fehleingabe; eine eigene Bestenliste für Fox Hunt; dasselbe
Server/Client-Rennen auf demselben Labyrinth). Die Einzelheiten stehen im
Trailblazer-Abschnitt oben.

## WiFi Functions

Neben der WiFi-Transceiver-Funktionalität kannst du die WLAN-Funktion
des im Morserino-32 verwendeten ESP32-Prozessors für zwei weitere
Aufgaben nutzen, wenn du WLAN über einen Access Point verwendest:

-   Hochladen einer Textdatei auf den Morserino-32, die dann im Modus
    CW Generator oder Echo Trainer abgespielt werden kann.
-   Hochladen der Binärdatei einer neuen Firmware-Version für ein
    Firmware-Update.

Für beide Funktionen muss die hochzuladende Datei auf deinem Computer
(auch Tablet oder Smartphone) vorhanden sein, und dein Morserino muss
mit demselben WLAN-Netzwerk verbunden sein wie dein Computer.

::: note
Beide Funktionen lassen sich mittlerweile viel einfacher durch
Anschließen des Morserinos an einen Computer mit Chrome, Edge oder
Opera erledigen – siehe Anhänge 6 und 7 dieses Handbuchs!
:::

Um den Morserino-32 mit deinem lokalen WLAN-Netzwerk zu verbinden,
benötigst du in der Regel die SSID (den „Namen") des Netzwerks und das
Passwort, auch als „Zugangsdaten" bekannt. Diese Daten müssen im
Morserino-32 gespeichert werden. Da er keine Tastatur für die bequeme
Eingabe hat, erfolgt dies über eine eigene WLAN-Funktion: die
**Netzwerkkonfiguration** (**Config WiFi**), die du zuerst ausführen
musst, bevor du Upload- oder Update-Funktionen nutzen kannst.

Bei Heimnetzwerken, die eine Liste zulässiger MAC-Adressen verwenden
(aus Sicherheitsgründen), musst du zunächst die MAC-Adresse des M32 in
deinem Router eintragen, bevor der M32 eine Verbindung herstellen kann.
Dafür gibt es eine Funktion zur Anzeige der MAC-Adresse auf dem Display.

Alle netzwerkbezogenen Funktionen findest du unter dem Menüpunkt
**WiFi Functions**.

In Software-Versionen vor 2.0 waren die WiFi-Funktionen nicht ins
Hauptmenü integriert. Falls du von Version 1.x auf Version 2.x über
WLAN aktualisieren möchtest, lies bitte **Anhang 4: Aktualisieren der
Firmware über WLAN für Versionen \< 2.0**.

Wenn du über das WLAN-Menü (WiFi / Select) „EspNow" anstelle eines
Access Points ausgewählt hast, ist die einzige verfügbare WiFi-Funktion
„**WiFi Select**", da alle anderen Funktionen einen Access Point
erfordern!

### Anzeige der MAC-Adresse

Dies ist der erste Eintrag im Menü **WiFi Functions** und zeigt die
MAC-Adresse des Morserinos in der Statuszeile an. Jeder Morserino hat
eine eindeutige MAC-Adresse.

Mit dieser Information kannst du dem Morserino den Zugang zu deinem
WLAN-Netzwerk ermöglichen, wenn dein Router so konfiguriert ist, dass
er nur bestimmten MAC-Adressen Zugriff erlaubt.

Wenn du die FN-Taste drückst, kehrt der Morserino-32 normal zum Menü
zurück. Wenn du nichts tust, geht der Morserino je nach deinen
Time-Out-Einstellungen in den Tiefschlaf.

### Netzwerk-Konfiguration

Wähle das Untermenü **WiFi Config**, um mit der Netzwerkkonfiguration
fortzufahren.

Das Gerät startet WLAN **als Access Point** und erstellt so sein eigenes
WLAN-Netzwerk (mit der SSID „**morserino**"). Wenn du die verfügbaren
Netzwerke mit deinem Computer oder Smartphone überprüfst, findest du es
leicht; verbinde deinen Computer bitte mit diesem Netzwerk (kein
Passwort erforderlich).

Sobald du verbunden bist, gib *http://m32.local* in den Browser deines
Computers ein. Wenn dein Computer oder Smartphone mDNS nicht unterstützt
(Android unterstützt es z.B. nicht, Windows nur rudimentär), gib statt
„m32.local" die IP-Adresse **192.168.4.1** ein. Du siehst dann ein
kleines Formular mit 3 × 3 leeren Feldern: *SSID of WiFi network?*,
*WiFi Password?* und *WiFi TRX Peer IP?*

Du musst nur einen Satz Felder ausfüllen, kannst aber auch zwei oder
drei verwenden, um verschiedene Netzwerkkonfigurationen für
unterschiedliche Szenarien zu speichern (z.B. Verbindung zu verschiedenen
WLAN-Netzwerken). Im WLAN-Menü gibt es einen eigenen Eintrag zur Auswahl
der gewünschten Konfiguration.

Gib den Namen deines lokalen WLAN-Netzwerks und das zugehörige Passwort
ein (das dritte Feld kannst du vorerst leer lassen) und klicke auf
„Submit". Dein Morserino-32 speichert diese Zugangsdaten und startet
dann neu (das Netzwerk „morserino" verschwindet wieder).

Das dritte Feld (*WiFi TRX Peer IP/Host?*) wird verwendet, wenn du die
WiFi-Transceiver-Funktionalität über einen Access Point nutzen möchtest,
d.h. mit einem anderen Morserino-Nutzer oder Dienst über das Internet
kommunizieren möchtest. Gib dort die IP-Adresse oder den DNS-Hostnamen
der Gegenstelle ein. Siehe Abschnitt **5.5.2 WiFi Trx** weiter oben.
Wenn du mit anderen Morserinos **im lokalen Netzwerk** kommunizierst,
ist keine IP-Adresse erforderlich (es wird standardmäßig die
Broadcast-Adresse verwendet, sodass alle Morserinos empfangen können,
was einer von ihnen sendet).

Dein Morserino kann kein WLAN-Netzwerk mit einem „Captive Portal"
nutzen, wie es oft in öffentlichen Netzwerken verwendet wird. Diese
Netzwerke setzen einen Browser auf dem Gerät voraus – den hat der
Morserino-32 nicht.

Dein Morserino-32 unterstützt nur WLAN-Netzwerke im 2,4-GHz-Band,
nicht im 5-GHz-Band.

::: important
Wenn du zuvor über das Menü WiFi / Select „EspNow" ausgewählt hast,
musst du erst einen Access Point auswählen, bevor du die
WLAN-Konfiguration durchführen kannst!
:::

::: note
Wenn du dein WLAN bereits konfiguriert hast und diesen Schritt erneut
ausführst, wird der zuvor eingegebene SSID-Name im Formular
vorausgefüllt – du musst ihn nur bei Bedarf ändern. Das Passwortfeld
ist leer; wenn du kein neues eingibst, wird das alte weiterverwendet.
Das TRX-Peer-IP-Feld wird ebenfalls vorausgefüllt, wenn zuvor ein
Wert eingegeben wurde. Löschst du die Werte in diesem Feld, wird die
IP-Adresse entfernt.
:::

Du kannst drei verschiedene Netzwerkeinstellungen konfigurieren; ab
Version 4.5.1 werden Netzwerkkonfigurationen nicht mehr in Schnappschüssen
gespeichert – du kannst also keine Schnappschüsse verwenden, um
verschiedene Netzwerkeinstellungen abzurufen.

::: note
Einfacher geht es, wenn du deinen Morserino über USB an einen Computer
mit Chrome, Edge oder Opera anschließt und den Anweisungen in **Anhang 7
Einrichten von M32-Einstellungen über einen Browser** folgst.
:::

### Überprüfen der Netzwerkkonnektivität

Verwende den Untermenüeintrag **Check WiFi** unter **WiFi Functions**,
um die Netzwerkverbindung zu testen.

Es wird entweder eine Fehlermeldung angezeigt („No WiFi" und die
eingegebene SSID) oder eine Erfolgsmeldung („Connected!") mit SSID und
der vom WLAN-Router erhaltenen IP-Adresse.

Es kann sein, dass du deinen Morserino ziemlich nah an deinen
WLAN-Router bringen musst (im selben Raum ist normalerweise ausreichend)!
Die WLAN-Antenne des ESP32-Moduls ist sehr klein und empfängt sehr
schwache Signale möglicherweise nicht.

Wenn du trotz korrekter Zugangsdaten und direkter Nähe zum Router
eine Fehlermeldung erhältst, versuche es einfach noch einmal – manchmal
schlägt der erste Versuch aus unbekannten Gründen fehl.

Wenn du die FN-Taste drückst, kehrt diese Funktion zum Menü zurück.
Wenn du nichts tust, geht der Morserino je nach Einstellungen in den
Tiefschlaf.

### Hochladen einer Textdatei

Nachdem du deinen Morserino-32 mit deinen lokalen WLAN-Zugangsdaten
konfiguriert hast, kannst du eine Textdatei für das Morse-Training
hochladen. Derzeit kann nur eine Datei auf dem Morserino-32 gespeichert
werden; beim Hochladen einer neuen Datei wird die alte überschrieben.

Die **hochzuladende Datei** sollte eine einfache ASCII-Textdatei ohne
Formatierung sein (keine Word-Dateien, PDFs usw.). Deutsche Zeichen
(*ÄÖÜäöüß*) in UTF-8-Kodierung sind erlaubt und werden in *ae*, *oe*,
*ue* und *ss* umgewandelt. Die Datei kann Groß- und Kleinbuchstaben
sowie alle Zeichen der Koch-Methode (einschließlich Betriebszeichen,
insgesamt 51 verschiedene Zeichen) enthalten. Alle anderen Zeichen
werden beim Abspielen im Morsecode ignoriert. Die Datei kann recht groß
sein – ca. 1 MB Speicherplatz steht zur Verfügung (genug für Mark Twains
„The Adventures of Huckleberry Finn").

::: note
Informationen zur Kodierung von Textdateien findest du im Abschnitt
**5.2.1 Was kann generiert werden?** – du kannst z.B. eine Datei in
mehrere Teile unterteilen, die der File Player wie separate Dateien
behandelt.
:::

Um die Datei hochzuladen, wähle **File Upload** im Menü **WiFi
Functions**. Nach einigen Sekunden (der Morserino muss sich erst mit
dem WLAN-Netzwerk verbinden) zeigt er an, dass er auf den Upload wartet.
Ruf mit dem Browser deines Computers *http://m32.local* auf (oder, falls
das nicht funktioniert, ersetze „m32.local" durch die auf dem Display
angezeigte IP-Adresse).

::: note
Für die Upload-Funktion müssen dein Morserino-32 (und natürlich dein
PC oder Tablet usw.) wieder im lokalen WLAN-Netzwerk sein!
:::

Zuerst erscheint ein Login-Bildschirm in deinem Browser. Verwende **m32**
als Benutzer-ID und **upload** als Passwort. Auf dem nächsten Bildschirm
findest du einen Dateiauswahldialog – wähle die hochzuladende Datei aus
(Name und Erweiterung spielen keine Rolle) und klicke auf „Begin". Sobald
der Upload abgeschlossen ist (das dauert nicht lange), kehrt der
Morserino-32 zum Menü zurück, und du kannst die hochgeladene Datei im
Modus CW Generator oder Echo Trainer verwenden.

Wenn du den Vorgang abbrechen musst, starte das Gerät neu – entweder
durch vollständiges Trennen vom Strom (Akku aus und USB trennen) oder
(bei M32 1st oder 2nd Edition) durch Drücken der Reset-Taste mit einem
kleinen Schraubenzieher oder Kugelschreiber (die Reset-Taste ist durch
das Loch neben dem USB-Anschluss in Richtung des externen
Paddle-Anschlusses erreichbar).

::: note
Wenn es nicht klappt: Stelle sicher, dass du das Passwort **upload**
verwendest – NICHT **update**!
:::

### Aktualisieren der Morserino-32-Firmware über WLAN

Das Aktualisieren der Firmware über WLAN ist eine von mehreren
Möglichkeiten; du kannst es auch mit der Arduino IDE (oder PlatformIO)
tun, oder einfacher mit einem speziellen Update-Programm (siehe
**Anhang 5 Aktualisieren der Firmware über USB und ein Update-Programm**),
oder – und das ist **der einfachste Weg** – direkt mit einem Browser und
USB (siehe **Anhang 6 Aktualisieren der Firmware über USB und einen
Browser (Webserial)**).

Du kannst auf jede beliebige Version aktualisieren, Versionen überspringen
und auch zu einer älteren Version zurückkehren.

Das Firmware-Update über WLAN ist dem Hochladen einer Textdatei sehr
ähnlich. Hole dir zunächst die Binärdatei aus dem Morserino-32-Repository
auf GitHub (*https://github.com/oe1wkl/Morserino-32* – suche unter
„Software" nach einem Verzeichnis namens „Binaries"). Lade die neueste
Version auf deinen Computer herunter. Der Dateiname sieht wahrscheinlich
so aus:

*m32\_Vx.y\[\...\].bin* (x.y ist die Versionsnummer).

Rufe nun wieder das Menü **WiFi Functions** auf und wähle den Punkt
**Update Firmw**. Ähnlich wie beim Datei-Upload rufst du mit deinem
Browser *http://m32.local* auf (oder die auf dem Display angezeigte
IP-Adresse, *http://n1.n2.n3.n4*) und siehst einen Login-Bildschirm.
Diesmal verwendest du den Benutzernamen **m32** und das Passwort
**update** (nicht **upload**!).

Im nächsten Schritt siehst du einen Dateiauswahldialog, wählst deine
Binärdatei aus und klickst auf „Begin". Der Upload dauert diesmal länger
– einige Minuten, also Geduld. Die Datei ist groß, muss hochgeladen und
auf den Morserino-32 geschrieben und überprüft werden, um
sicherzustellen, dass es eine ausführbare Datei ist. Zum Schluss startet
das Gerät neu, und du siehst die neue Versionsnummer auf dem
Startbildschirm.

Zusammengefasst, die Schritte für das Firmware-Update über WLAN:

1.  Führe die Netzwerkkonfiguration durch (der Morserino richtet sein
    eigenes WLAN-Netzwerk ein, und du gibst in deinem Browser den Namen
    und das Passwort deines Heimnetzwerks ein). Das musst du nur einmal
    tun, da der Morserino die Zugangsdaten speichert. Nutze „Check WiFi",
    um sicherzustellen, dass der Morserino eine Verbindung herstellen
    kann. Denk daran, nah genug am WLAN-Router zu sein!
2.  Lade die neue Binärdatei von GitHub auf deinen Computer herunter.
3.  Starte „Update firmware" auf deinem Morserino. Nach einer Weile zeigt
    er eine IP-Adresse (im Heimnetzwerk!) und eine Wartende-Meldung an.
4.  Öffne mit deinem Browser entweder die auf dem Morserino angezeigte
    IP-Adresse (*http://ww.xx.yy.zz*) oder *http://m32.local* (funktioniert
    auf Macs und iPhones; meist nicht auf Windows-PCs oder
    Android-Geräten).
5.  Gib im Browser-Login *m32* als Benutzernamen und *update* als Passwort
    ein.
6.  Wähle im Dateiauswahldialog die Binärdatei aus deinem Download-Ordner
    aus und klicke auf „Begin". Ein Fortschrittsbalken wird angezeigt;
    nach einiger Zeit (auch wenn 100 % angezeigt werden, kann es noch
    dauern) startet der Morserino neu und zeigt die neue Versionsnummer
    auf dem Startbildschirm. Dann war das Update erfolgreich.

::: note
Wenn es nicht klappt: Stelle sicher, dass du das Passwort **update**
verwendest – NICHT **upload**!
:::

### WiFi Select

Hier kannst du auswählen, welche Netzwerkkonfiguration verwendet werden
soll. SSID und Peer-Host werden angezeigt; du gehst mit dem ENCODER durch
die verfügbaren Konfigurationen.

Der beim Start angezeigte Eintrag ist die aktuell gewählte
Netzwerkkonfiguration. Wenn das die richtige ist, kehre einfach mit
einem langen Druck auf den ENCODER zum Menü zurück.

Der erste Eintrag (Nummer 0) ermöglicht die Auswahl von EspNow
(WLAN-Peer-to-Peer-Kommunikation) anstelle eines Access Points.

## Go To Sleep

Wenn du diesen Menüpunkt auswählst, versetzt er den Morserino-32 in den
Tiefschlaf, in dem er deutlich weniger Strom verbraucht als im
Normalbetrieb. Der Akku entlädt sich aber dennoch innerhalb einiger Tage,
daher ist dieser Modus nur für kürzere Pausen zwischen Trainingseinheiten
gedacht. Siehe Abschnitt **4.1 Ein- und Ausschalten / Aufladen des
Akkus** weiter oben in diesem Handbuch.

# Einstellungen

Du erreichst das Einstellungsmenü immer durch einen **Doppelklick** auf
den ENCODER-Drehknopf. Damit gelangst du in ein Menü (vor der aktuell
markierten Einstellung steht ein „>"-Zeichen; die Zeile darunter zeigt
den aktuellen Wert). Drehe den ENCODER, um durch die verfügbaren
Einstellungen zu navigieren. Um das Einstellungsmenü zu verlassen,
drücke den ENCODER-Knopf einfach etwas länger – du gelangst dann zurück
in den Betriebsmodus, von dem aus du das Menü aufgerufen hast (oder ins
Hauptmenü, wenn du den Doppelklick aus dem Menü heraus eingegeben hast).

Wenn du die gewünschte Einstellung erreicht hast, klicke einmal. Nun
steht das „>"-Zeichen in der unteren Zeile vor dem Einstellungswert und
zeigt an, dass das Drehen des ENCODERs diesen Wert ändert. Wenn du mit
dem Wert zufrieden bist, klicke einmal, um zur Einstellungsauswahl
zurückzukehren, oder drücke den Knopf etwas länger, um das
Einstellungsmenü zu verlassen.

Die jeweils verfügbaren Einstellungen variieren je nach Modus:
**Wenn du in einem bestimmten Modus doppelklickst, siehst du nur die
für diesen Modus relevanten Einstellungen.** Wenn du vom Startmenü aus
doppelklickst, werden alle Einstellungen angezeigt.

::: note
Fast alle Einstellungen können über das Hardware-Konfigurationsmenü auf
die Werkseinstellungen zurückgesetzt werden. Weitere Informationen
findest du in **Anhang 1**.
:::

## Schnappschüsse

Für verschiedene Trainingsarten benötigst du in der Regel
unterschiedliche Einstellungen – du möchtest vielleicht den
Zeichen- oder Wortabstand ändern, oder die Länge von Zeichengruppen
oder Wörtern usw. Das Wechseln von einer Trainingsart zur nächsten
würde bedeuten, jedes Mal verschiedene Einstellungen zu ändern.

Um dies zu vereinfachen, kannst du „**Schnappschüsse**" (Snapshots) der
Einstellungen verwenden: Sobald du alles für deinen ersten Trainingsmodus
eingestellt hast, speicherst du alle aktuellen Einstellungen in einem
von acht Schnappschüssen; dasselbe machst du für deine anderen
Trainingsmodi. Du kannst die Einstellungen dann durch Abrufen eines
bestimmten Schnappschusses schnell wiederherstellen.

Die gewählte „Koch-Lektion" wird im nichtflüchtigen Speicher abgelegt
und ist nach einem Neustart verfügbar, wird aber nicht in einem
Schnappschuss gespeichert oder überschrieben. Dasselbe gilt für
WLAN-Einstellungen, die Einstellung **Serial Output** sowie für deine
Geschwindigkeits- und Lautstärkeeinstellungen.

### Speichern eines Schnappschusses

Doppelklicke zunächst auf den ENCODER, um in das Einstellungsmenü zu
gelangen. Ein **langer Druck auf die FN-Taste** gibt dir die Möglichkeit,
mit dem ENCODER auszuwählen, an welchem Ort du die aktuellen
Einstellungen speichern möchtest – von **Snapshot 1** bis **Snapshot 8**.
Eine weitere Option lautet **Cancel Store** und ermöglicht den Ausstieg
ohne Speicherung. Bereits belegte Schnappschuss-Plätze sind **fett**
dargestellt, können aber überschrieben werden. Ein Klick auf den
ENCODER-Knopf speichert den Schnappschuss an der gewünschten Stelle und
zeigt kurz eine Erfolgsmeldung.

### Abrufen eines Schnappschusses

Doppelklicke erneut auf den ENCODER-Knopf, um in das Einstellungsmenü
zu gelangen. Ein **kurzer Klick auf die FN-Taste** ermöglicht dir, mit
dem ENCODER auszuwählen, welchen der gespeicherten Schnappschüsse du
abrufen möchtest; ein Klick auf den ENCODER-Knopf ruft ihn ab. Es gibt
auch eine Option **Cancel Recall**, die den Ausstieg ohne Abruf
ermöglicht. Wenn keine Schnappschüsse gespeichert sind, erscheint die
Meldung „NO SNAPSHOTS" und du kannst durch Klicken einer der Tasten
das Menü verlassen.

### Löschen eines Schnappschusses

Du kannst auch einen nicht mehr benötigten oder versehentlich erstellten
Schnappschuss löschen. Gehe so vor, als wolltest du einen Schnappschuss
abrufen, wähle den zu löschenden Schnappschuss aus und **klicke dann auf
die FN-Taste**, um ihn zu löschen. Ein Bestätigungsdialog fragt, ob du
den Schnappschuss wirklich löschen möchtest. Wenn du den ENCODER auf
„Yes" drehst und klickst, wird der Schnappschuss gelöscht; wie beim
Speichern und Abrufen bestätigt eine kurze Meldung den Erfolg.

Am einfachsten konfigurierst du Einstellungen und Schnappschüsse, indem
du deinen Morserino über USB mit einem Computer mit Chrome, Edge oder
Opera verbindest und den Anweisungen in **Anhang 7 Einrichten von
M32-Einstellungen über einen Browser** folgst.

## Rufzeichen, Name und Spielstände

Zwei Einstellungen sagen dem Morserino, wer du bist. **Call Sign** (Rufzeichen)
und **Op Name** (Name) werden von den Spielen (Fight the Pileup, Morsel) und
vom QSO-Bot verwendet. Wählst du einen dieser Einträge im Einstellungsmenü,
öffnet sich ein Texteditor am Bildschirm (er funktioniert am OLED und am LCD
gleich):

- **ENCODER drehen** wählt ein Zeichen,
- **ENCODER klicken** fügt es zum Text hinzu,
- **FN-Taste klicken** löscht das letzte Zeichen,
- **langer Druck** auf eine der Tasten beendet die Eingabe.

Call Sign akzeptiert Buchstaben, Ziffern und den Schrägstrich; Op Name
akzeptiert Buchstaben und ein Leerzeichen. Der aktuelle Wert wird neben der
Einstellung angezeigt.

**Reset Scores** löscht die gespeicherten Bestenlisten und den Spielfortschritt
aller Spiele (Morse Invaders, Morsel, Radio Cave sowie die Gitter-Labyrinth-Spiele
Trailblazer und Fox Hunt). Beim Auswählen erscheint
eine Bestätigung: **FN** drücken zum Bestätigen, oder den ENCODER klicken zum
Abbrechen.

## Liste aller Morserino-32-Einstellungen

**Fett gedruckte Werte** sind Standard- oder empfohlene Werte. Wenn du
das Einstellungsmenü vom Startmenü aus aufrufst, sind alle Einstellungen
verfügbar; wenn du es aus einem laufenden Modus heraus aufrufst, sind
nur die für diesen Modus relevanten Einstellungen verfügbar.

### Allgemeine Einstellungen

Eine Reihe von Einstellungen sind sehr allgemeiner Natur und gelten
daher für alle Modi des Morserino-32.

| Einstellung | Beschreibung | Werte |
|---|---|---|
| **Encoder Click** | Das Drehen des ENCODERs kann einen kurzen Ton erzeugen oder stumm sein. | Off / **On** |
| **Tone Pitch Hz** | Die Frequenz des Mithörtons in Hz. | Eine Reihe von Tönen zwischen 233 und 932 Hz, entsprechend den Noten der F-Dur-Tonleiter von Bb3 bis Bb5 (2 Oktaven). |
| **Time Out** | Wenn die hier eingestellte Zeit verstreicht, ohne dass das Display aktualisiert wird, wechselt das Gerät in den Tiefschlaf. Durch Drücken der FN-Taste kann es wieder gestartet werden. | No timeout / **5 min** / 10 min / 15 min |
| **Quick Start** | Ermöglicht es, die anfängliche Menüauswahl zu umgehen: Bei aktivierter Option (**ON**) startet das Gerät beim Einschalten sofort mit dem zuletzt aktiven Modus. | ON / **OFF** |
| **Output Case** | Ändert die Groß-/Kleinschreibung der dekodierten Zeichen auf dem Display (und auch bei der seriellen Ausgabe über USB sowie bei der Bluetooth-Tastaturausgabe!) von Kleinbuchstaben auf GROSSBUCHSTABEN. | **lower** / UPPER |
| **Headphone Output** | (Nur für M32Pocket) Legt fest, was passiert, wenn Kopfhörer oder ein anderes Gerät an den Kopfhörerausgang angeschlossen werden. Mit der Standardeinstellung erfolgt die Ausgabe über den Kopfhörer und der Lautsprecher wird stummgeschaltet. Mit „*line-out*" erfolgt die Ausgabe mit voller Lautstärke über den Kopfhörerausgang und normal über den Lautsprecher. Mit „*l-o: Var. Vol.*" ähnlich, aber die Ausgabe über den Stecker erfolgt mit der eingestellten Lautstärke. Mit „*l-o: Lsp Muted*" erfolgt die Ausgabe über den Stecker mit voller Lautstärke und der Lautsprecher wird stummgeschaltet. | **Phones** / line-out / l-o: Var. Vol. / l-o: Lsp Muted **Achtung: Bei Verwendung der line-out-Optionen niemals Kopfhörer anstecken! Da die Wiedergabe mit voller Lautstärke erfolgen kann, könnte dies das Gehör oder die Kopfhörer beschädigen!** |
| **Theme** | (Nur für Geräte mit Farbbildschirm, z.B. M32Pocket) Du kannst ein Farbthema für das Display einstellen, sodass du nicht auf „Weiß auf Schwarz" beschränkt bist. Jedes Thema (auch Plain) zeigt zudem Morsetext in einer eigenen Akzentfarbe — abgehoben von Menü- und Statustext — und stellt die OK/ERR-Rückmeldung im Echo Trainer in Grün bzw. Rot dar. | **Plain** (= Weiß auf Schwarz) / Blues / ePaper / Mandarin / Darkroom / Veggie / Garnet / Lemonade / Complements |
| **Invader Orient.** | (Nur für M32Pocket) Du kannst die bevorzugte Displayausrichtung für Spiele wie Morse Invaders wählen: Hochformat (Standard) oder Querformat. Bei Auswahl von Querformat wird die Linkshänder-Ausrichtung verwendet, wenn diese in der Hardware-Konfiguration eingestellt ist. | **Portrait** / Landscape |
| **Serial Output** | Legt fest, was an die serielle Schnittstelle (USB-Anschluss) gesendet wird; unterschieden wird zwischen getasteten Zeichen (**Keyer** – Ausgabe des iambischen Keyers), dekodierten Zeichen (**Decoded** – vom CW-Decoder oder einer Handtaste) und „generierten" Zeichen (**Generated** – vom CW-Generator usw., auch von der Empfangsseite der LoRa- oder WiFi-Transceiver-Modi). **Nothing** sendet keines dieser Zeichen (bestimmte System- oder Fehlermeldungen können aber trotzdem erscheinen), **All** sendet alles. Über das M32-Serielle-Protokoll können zudem weitere Informationen gesendet und empfangen werden, wenn die angeschlossene Computersoftware dies unterstützt. Siehe auch **Anhang 8 Nutzung des seriellen Ausgangs des M32**. Diese Einstellung gilt auch für den Zeichenstrom an eine über Bluetooth verbundene App (siehe die Einstellung **Bluetooth Use** in Abschnitt **6.2.2**). | Nothing / Keyer / Decoded / Keyed+Decoded / Generated / **All** (Standard seit V. 4.3) |

### Einstellungen zu Key, Paddles und Keyer

Diese Einstellungen steuern das Verhalten der Paddles (eingebaut oder
extern) sowie vor allem die Timing-Einstellungen, die für Iambic Keying
oder die Verwendung einer externen Handtaste relevant sind (stelle
**Keyer Mode** auf **Straight Key**, um eine Handtaste zu verwenden).

| Einstellung | Beschreibung | Werte |
|---|---|---|
| **Paddle Polarity** | Legt fest, welche Paddle-Seite für Dits und welche für Dahs ist. | -. dah-dit / **.- di-dah** |
| **External Pol.** | Ermöglicht die Umkehrung der Polarität eines externen Paddles. Verwende dies, wenn dein externes Paddle „falsch herum" verdrahtet ist, damit Dits und Dahs bei internem und externem Paddle auf der gleichen Seite sind. | **Normal** / Reversed |
| **Keyer Mode** | Stellt den Iambic-Modus (A oder B), Ultimatic, Non-Squeeze oder Handtaste (Straight Key) ein; siehe Abschnitt **5.1 CW Keyer**. | Curtis A / **Curtis B** / Ultimatic / Non-Squeeze / Straight Key |
| **CurtisB DahT%** | Timing im Curtis-B-Modus für Dahs; siehe Abschnitt **5.1 CW Keyer**. Beeinflusst auch das Verhalten im Ultimatic-Modus! | 0–100, in 5er-Schritten [**35–55**] |
| **CurtisB DitT%** | Timing im Curtis-B-Modus für Dits; siehe Abschnitt **5.1 CW Keyer**. Beeinflusst auch das Verhalten im Ultimatic-Modus! | 0–100, in 5er-Schritten [**55–95**] |
| **AutoChar Spce** | Mindestabstand zwischen den Zeichen. | Off / min. 2 / **3** / 4 dots |
| **Latency** | Legt fest, wie lange nach der Erzeugung des aktuellen Elements (Punkt oder Strich) die Paddles „taub" sind. Bei 0 % muss das Paddle losgelassen werden, während das letzte Element noch „an" ist. Bei 87,5 % reagieren die Paddles erst nach 7/8 einer Punktlänge auf einen Druck. | Ein Wert zwischen 0 % und 87,5 %, d.h. 0/8 bis 7/8 einer Punktlänge (Standard: **50 %**, d.h. eine halbe Punktlänge). |
| **Bluetooth Use** | Legt fest, wofür das Bluetooth-Funkmodul verwendet wird. Die ersten vier Optionen (neben **No Bluetooth**) sind Tastatur-Modi: Die Option **VBand Kbd** ermöglicht die Verwendung des Morserinos als VBand-Dongle (zu VBand siehe *https://hamradio.solutions/vband/*). **Decoded output** sendet alle dekodierten Zeichen nicht nur ans Display, sondern auch über Bluetooth. Die Option **Generic Kbd** macht im Wesentlichen dasselbe wie **Decoded output**, sendet aber zusätzlich den Code für die „**Enter**"-Taste (neue Zeile), wenn du \<KA> (neue Nachricht) eingibst, und für die „**Backspace**"-Taste, wenn du \<HH> eingibst (d.h. 8 Dits). Der M32 erscheint immer als US-Tastatur (QWERTY-Layout) – dies ist bei der Konfiguration am angeschlossenen Computer zu berücksichtigen. *Die Bluetooth-Tastaturausgabe ist nur im Modus CW Keyer aktiv (siehe auch **Anhang 9**).* Die letzte Option, **BLT Serial Prot.**, stellt stattdessen das M32-Serielle-Protokoll (siehe **Anhang 8**) über Bluetooth Low Energy zur Verfügung — damit können Apps auf Smartphones und Tablets den Morserino fernsteuern und Text senden, der als CW getastet wird, ganz ohne USB-Kabel; die Auswahl wird bei der nächsten Rückkehr ins Hauptmenü wirksam. Beachte: Wie eine USB-Protokollsitzung verhindert auch eine aktive BLE-Protokollsitzung die automatische Abschaltung (Timeout) — denke daran im Akkubetrieb. | **No Bluetooth** / VBand Kbd / Decoded output / VBand+Decoded / Generic Kbd / BLT Serial Prot. |
| **BLT \<AR>** | Nur im Modus **Generic Kbd** relevant (siehe **Bluetooth Use** oben). Legt fest, wie das \<AR>-Betriebszeichen über Bluetooth gesendet wird: als wörtliches Zeichen „**+**" oder als weicher Zeilenumbruch (Shift+Enter). | **+** / Linefeed |

### Einstellungen bezüglich der Koch-Zeichenfolge

Wenn du Kursen verschiedener Institutionen folgst, verwenden diese eine
bestimmte Reihenfolge, in der Morsezeichen eingeführt werden. Hier
kannst du auswählen, welcher Reihenfolge du folgen möchtest.

| Einstellung | Beschreibung | Werte |
|---|---|---|
| **Koch Sequence** | Legt die Reihenfolge der Zeichen bei Verwendung der Koch-Methode zum Lernen und Trainieren fest. Du kannst auch deinen benutzerdefinierten Zeichensatz verwenden, indem du **Custom Chars** wählst – siehe Abschnitt **5.4.1 Koch: Select Lesson**, letzter Absatz. | **M32** (native Reihenfolge, auch von JLMC – Just Learn Morse Code verwendet) / LCWO / CW Academy / LICW Carousel / Custom Chars |
| **LICW Carousel** | Legt den „Einstiegspunkt" in den LICW-Carousel-Lehrplan fest (nur relevant, wenn **Koch Sequence** auf **LICW Carousel** gesetzt ist). Wenn du einen Kurs in BC1 beginnst, solltest du dies entsprechend einstellen und auch wieder anpassen, wenn du in die Carousel-Kurse für BC2 einsteigst. | **BC1: r e a** / BC1: t i n / BC1: p g s / BC1: l c d / BC1: h o f / BC1: u w b / BC2: k m y / BC2: 5 9 , / BC2: q x v / BC2: 7 3 ? / BC2: \<ar> \<sk> = / BC2: 1 6 . / BC2: z j / / BC2: 2 8 \<bk> / BC2: 4 0 |

### Einstellungen zur CW-Generierung

Die folgenden Einstellungen steuern, wie Zeichen generiert und zufällig
abgespielt werden oder wie Textdateien als Morsezeichen abgespielt
werden. Besonders hinweisen möchte ich auf **Interchar Spc** und
**Interword Spc**, da du mit diesen Einstellungen die sogenannte
„Farnsworth-Geschwindigkeit" bzw. „Wordsworth-Geschwindigkeit"
einstellen kannst. Diese Einstellungen sind natürlich auch für den Echo
Trainer relevant!

| Einstellung | Beschreibung | Werte |
|---|---|---|
| **Interchar Spc** | Die Zeit (in Dit-Längen), die zwischen Zeichen eingefügt wird (siehe Abschnitt **5.2 CW Generator**). | 3–45 [**3**] |
| **Interword Spc** | Die Zeit (in Dit-Längen), die zwischen Wörtern eingefügt wird (siehe Abschnitt **5.2 CW Generator**). Hat auch eine gewisse Auswirkung auf den Decoder, da sie ihm hilft zu entscheiden, wann ein Leerzeichen eingefügt werden soll. | 6–105 [**7**] |
| **Random Groups** | Für die Ausgabe von Gruppen zufälliger Zeichen: Legt fest, welche Zeichenuntergruppen enthalten sein sollen. | Alpha / Numerals / Interpunct. / Pro Signs / Alpha+Num / Num+Interp. / Interp+ProSn / Alpha+Num+Int / Num+Int+ProS / **All Chars** |
| **Length Rnd Gr** | Legt fest, wie viele Zeichen in jeder Zufallszeichengruppe enthalten sein sollen; traditionell sind es 5, aber für das Training kann es sinnvoll sein, mit einer kleineren Zahl zu beginnen. | Feste Längen 1–6, und 2 to 3 – 2 to 6 (Länge innerhalb dieser Grenzen zufällig gewählt) [**5**] |
| **Length Calls** | Wähle die maximale Länge der generierten Rufzeichen. | **Unlimited** / max. 3 – max. 6 |
| **Calls Region** | Wähle, ob Rufzeichen aus aller Welt oder nur von einem bestimmten Kontinent generiert werden sollen. **Dieser Parameter wird ignoriert, wenn die Rufzeichenlänge auf maximal 3 Zeichen eingestellt ist.** | **All** / Europe / N America / S America / Africa / Asia / Oceania |
| **Call Prefixes** | Über diese Einstellung kannst du sehr seltene Präfixe herausfiltern und nur Rufzeichen mit geläufigeren Präfixen erhalten. | **Common only** / All |
| **Length Abbrev** | Wähle die maximale Länge der zufällig generierten gängigen CW-Abkürzungen und Q-Gruppen. | **Unlimited** / max. 2 – max. 6 |
| **Length Words** | Wähle die maximale Länge der zufällig generierten gängigen englischen Wörter. | **Unlimited** / max. 2 – max. 6 |
| **Max \# of Words** | Wenn die angegebene Anzahl von Wörtern oder Buchstabengruppen generiert wurde, erzeugt der Morserino-32 ein abschließendes AR-Betriebszeichen ("+"), um das Ende dieser Sequenz anzuzeigen, und hält dann an und wartet. Mit einem Paddle-Berühren (oder einem Klick auf den ENCODER-Knopf) fährt er fort und erzeugt die nächste Wortfolge. *(Wenn „Auto Stop" aktiv ist, wird diese Einstellung im Modus CW Generator ignoriert.)* | **Unlimited** / 5 bis 250 in 5er-Schritten |
| **Stop/Next/Rep** | Stoppt die Generierung von Morsezeichen nach jedem Wort in den Modi CW Generator und Koch Generator, um das Erlernen des Kopierens ohne Mitschreiben zu unterstützen. Fortfahren durch Berühren des rechten Paddles (nächstes Wort) oder des linken Paddles (Wort wiederholen). *Diese Option und die Option „Each Word 2x" sind nicht miteinander kompatibel: Wenn eine auf ON gesetzt wird, wird die andere automatisch auf OFF gesetzt.* | ON / **OFF** |
| **CW Gen Displ** | Wähle, wie der CW-Generator oder die CW-Transceiver das Generierte oder Empfangene anzeigen sollen. | Display off / **Char by Char** / Word by word |
| **Randomize File** | Wenn auf „On" gesetzt, überspringt der File Player nach jedem gesendeten Wort n Wörter (n = Zufallszahl zwischen 0 und 255). | **Off** / On |
| **Each Word 2x** | Im Modus CW Generator wird jedes „Wort" (Zeichen zwischen Leerzeichen) zweimal ausgegeben, um das Kopieren nach Gehör zu erleichtern. *Diese Option und die Option „Stop/Next/Rep" sind nicht miteinander kompatibel: Wenn eine auf ON gesetzt wird, wird die andere automatisch auf OFF gesetzt.* Es gibt drei ON-Einstellungen: **ON** (ein vergrößerter Zeichenabstand wird auch bei der Wiederholung berücksichtigt); *ON less ICS* (der zusätzliche Zeichenabstand wird bei der Wiederholung reduziert); *ON true WpM* (der vergrößerte Zeichenabstand wird bei der Wiederholung ignoriert). | **OFF** / ON / ON (less ICS) / ON (true WpM) |

### Einstellungen zum Echo Trainer

Die folgenden Einstellungen steuern die wesentlichen Eigenschaften des
Echo Trainers. (**Tone Shift** ist jedoch auch für die Transceiver-Modi
interessant.)

Wenn du den Wert für **Interword Spc** erhöhst, verlängert sich auch
die Wartezeit nach der Vorgabe, bis du mit der Eingabe deiner Antwort
beginnen musst!

| Einstellung | Beschreibung | Werte |
|---|---|---|
| **Echo Repeats** | Legt fest, wie oft ein Wort wiederholt wird, wenn die Antwort zu spät oder falsch war, bevor der Echo Trainer ein neues Wort erzeugt. Bei Wert 0 ist das nächste Wort immer ein neues, unabhängig davon, ob die Antwort richtig oder falsch war. Bei **Forever** wird dasselbe Wort wiederholt, bis eine korrekte Antwort erfolgt. | 0–6 / Forever (Standard: **3**) |
| **Echo Prompt** | Legt fest, wie du im Modus Echo Trainer zur Eingabe aufgefordert wirst. **Sound only** (Standard; am besten zum Erlernen des Kopierens im Kopf), **Display only** (das einzugebende Wort wird auf dem Display angezeigt, kein Ton; gut zum Training der Paddle-Eingabe) und **Sound & Displ** (du hörst die Vorgabe UND siehst sie auf dem Display). | **Sound only** / Display only / Sound&Displ |
| **Confrm. Tone** | Legt fest, ob im Modus Echo Trainer ein hörbarer Bestätigungston ertönen soll. Bei ausgeschaltetem Ton wiederholt das Gerät nur die Vorgabe bei falscher Antwort oder sendet eine neue Vorgabe. Die visuelle Anzeige „OK" oder „ERR" ist auch bei ausgeschaltetem Ton sichtbar. | **ON** / Off |
| **Tone Shift** | Die Tonhöhe beim Verwenden des Echo Trainers oder beim Senden im Transceiver-Modus kann dieselbe sein wie die des Empfängers (bzw. der Vorgabe im Echo Trainer – **No Tone Shift**), oder einen Halbton tiefer (**Down**) oder höher (**Up**). | **No Tone Shift** / Up 1/2 Tone / Down 1/2 Tone |
| **Adaptv. Speed** | Wenn auf ON gesetzt, wird die Geschwindigkeit im Modus Echo Trainer bei jeder richtigen Antwort um 1 WpM erhöht und bei jedem Fehler um 1 WpM verringert. | ON / **OFF** |
| **Echo Speed Max** | Legt die maximale Keying-Geschwindigkeit im Modus Echo Trainer fest. Wenn du die Geschwindigkeit mit dem ENCODER über diesen Wert hinaus erhöhst oder sie durch Adaptive Speed schneller wird, bleibt die Eingabegeschwindigkeit bei diesem Maximum (so kannst du bei höheren Geschwindigkeiten kopieren als du comfortabel geben kannst). | **No limit** / 5–50 WpM (in 5-WpM-Schritten) |

### Einstellungen zum Senden, Dekodieren und QSO Bot

Diese Einstellungen steuern einige Funktionen, die für das Senden
(entweder direkt über LoRa oder WLAN oder durch Tastung eines externen
Senders), für das Dekodieren von Morsezeichen oder für den QSO Bot
(siehe Abschnitt **5.5.4 QSO Bot**) verfügbar sind.

| Einstellung | Beschreibung | Werte |
|---|---|---|
| **Key ext TX** | Legt fest, ob ein angeschlossener Sender getastet werden soll. **Gen** = Generatormodi, **RX** = LoRa- oder WiFi-Empfangsmodi. Die Option **Keyer & Gen.** lässt den Morserino einen externen Sender auch im Generatormodus tasten (z.B. für Trainingsübertragungen). Die Option **Keyer&Gen.&RX** ist nützlich, wenn du auf deinem Sender das senden möchtest, was der Morserino über LoRa oder WLAN empfangen hat (für Fernbedienung). | Never / **CW Keyer only** (gilt auch für Transceiver-Modi) / Keyer & Gen. / Keyer&Gen.&RX |
| **Generator Tx** | Ermöglicht dem CW-Generator, das Generierte entweder über LoRa oder WLAN zu senden – so kann ein Gerät etwas erzeugen und mehrere andere die gleiche Sequenz empfangen. Verwendbar in allen CW-Generator- und Koch/CW-Generator-Modi einschließlich File Player. Nützlich für Lerngruppen. Auf öffentlichen M32-Chat-Servern sollte dies nur mit Vorsicht und nicht über längere Zeit verwendet werden; sehr praktisch hingegen für eine Gruppe im gleichen Netzwerksegment (Broadcast als TrX-Peer oder privater Chat-Server) oder über LoRa (oder WiFi Trx mit EspNow), wenn alle Teilnehmer nah genug beieinander sind. *Beim Senden über LoRa muss eine Antenne angeschlossen sein, sonst wird der LoRa-Transceiver irgendwann zerstört! Auf Geräten ohne LoRa kann nur über WLAN gesendet werden (entweder über einen Access Point oder über EspNow).* | **Tx OFF** (= generierte Morsezeichen nicht aussenden) / LoRa Tx ON (mit LoRa senden; nur wenn der M32 LoRa hat) / WiFi Tx ON (mit WLAN senden) |
| **Trx Channel** | Wählt den (virtuellen) Kanal für LoRa oder EspNow (ein WLAN-Peer-to-Peer-Modus ohne Access Point). Bei LoRa ist dies ein virtueller Kanal; bei EspNow wird die Frequenz tatsächlich zwischen WLAN-Kanal 6 (**Standard Ch**) und 1 (**Secondary Ch**) gewechselt. Weitere Informationen zu EspNow findest du in Abschnitt **5.5.2 WiFi Trx**. | **Standard Ch** / Secondary Ch |
| **Bandwidth** | Legt die Bandbreite fest, die der CW-Decoder verwendet (implementiert in Software als sogenannter Goertzel-Filter). **Wide** = ca. 600 Hz, **Narrow** = ca. 150 Hz; Mittenfrequenz = ca. 700 Hz. | **Wide** / Narrow |
| **Decoded on I/O** | Normalerweise wird dekodiertes CW von einer externen Quelle (bei Verwendung eines der Transceiver-Modi oder des Decoders für Audiodekodierung) über den Lautsprecher (oder Kopfhörer) abgespielt, aber nicht an den externen Audio-E/A-Anschluss gesendet. Bei Einstellung auf „ON" wird der Ton auch an den externen Audio-E/A-Anschluss gesendet. **Beim M32Pocket wird diese Einstellung ignoriert!** | On / **Off** |
| **Contest Type** | Nur relevant im **Contest**-Modus des QSO Bots (Abschnitt **5.5.4 QSO Bot**): welcher Contest-Austausch verwendet wird. **CQ WW** sendet 5NN + die CQ-Zone des Bot-Rufzeichens; **WPX/Sprint** sendet 5NN + eine Seriennummer. | **CQ WW** / WPX/Sprint |
| **QSO Difficulty** | Wie nachsichtig und wie gesprächig der QSO-Bot-Partner ist (alle QSO-Bot-Modi, Abschnitt **5.5.4 QSO Bot**). **Beginner** ist geduldig (mehr Zeit zum Antworten, ein zusätzlicher Versuch), gibt Rapporte voll ausgeschrieben (599 statt 5nn) und verwendet klare, ruhige Aufforderungen. **Advanced** hält ein strafferes Tempo und verwendet knappe Aufforderungen wie von einem erfahrenen Operator. **Intermediate** liegt dazwischen. | Beginner / **Intermediate** / Advanced |

### Einstellungen zu Rufzeichen, Name und Spielständen

Diese Punkte stehen ganz am Ende der Einstellungsliste. Die ersten beiden legen deine persönliche Identität fest, die vom Spiel **Fight the Pileup** und vom **QSO Bot** (Abschnitt **5.5.4 QSO Bot**) verwendet wird; der dritte löscht die gespeicherten Spielstände. Call Sign und Op Name können auch über USB über das M32-USB Serial-Protokoll gesetzt werden (z.B. mit einem Browser-Konfigurationstool).

| Einstellung | Beschreibung | Werte |
|---|---|---|
| **Call Sign** | Dein eigenes Amateurfunk-Rufzeichen. Gib es mit dem Encoder und den Tasten ein. Es wird in Großbuchstaben gespeichert und als dein Stationsrufzeichen in **Fight the Pileup** und im **QSO Bot** verwendet. | bis zu 8 Zeichen (in GROSSBUCHSTABEN gespeichert) |
| **Op Name** | Dein Operatorname (z.B. dein Vorname). Gib ihn mit dem Encoder und den Tasten ein. Er wird in Großbuchstaben gespeichert und zusammen mit deinem Rufzeichen in **Fight the Pileup** verwendet. | bis zu 8 Zeichen (in GROSSBUCHSTABEN gespeichert) |
| **Reset Scores** | Dies ist eine Aktion, keine Einstellung: Sie löscht die gespeicherten Bestenlisten und Spielstände der Spiele — die Bestenliste von **Morse Invaders**, die Bestwerte von **Morsel**, den gespeicherten Fortschritt von **Radio Cave** sowie die Bestenlisten von **Trailblazer** und **Fox Hunt**. Du wirst gebeten, mit der **FN**-Taste zu bestätigen. (Fight the Pileup speichert keine dauerhafte Bestenliste und ist nicht betroffen.) | mit FN bestätigen |

# Anhänge

## Anhang 1: Hardware-Konfigurationsmenü

Es gibt ein Hardware-Konfigurationsmenü, das du erreichst, indem du
**ein Paddle (oder ein externes Paddle oder eine Handtaste) gedrückt
hältst, während du den M32 einschaltest**. Du kannst dann die gewünschte
Konfiguration auswählen, indem du den Drehknopf drehst und ihn drückst,
sobald die richtige Option angezeigt wird.

Die wählbaren Optionen sind **Calibr. Batt.** (Kalibrierung der
Akkumessung), **Flip Screen** (Bildschirm umdrehen), **LoRa Config.**
(LoRa-Konfiguration), **Reset Prefs.** (Einstellungen auf Werksstandard
zurücksetzen), **CN3: Touch / CN3: Mechan.** (nur M32 Pocket — Paddle-Typ
am CN3-Anschluss) und **Cancel** (Abbruch; verlässt das Menü und fährt
mit dem normalen Einschalten des M32 fort).

### CN3-Anschluss: Touch- oder mechanisches Paddle (nur M32 Pocket)

Der M32 Pocket hat zwei Paddle-Eingänge: die 3,5-mm-Klinkenbuchse (für
ein mechanisches Paddle oder eine Handtaste) und den kleinen
**CN3**-Anschluss, der normalerweise für die kapazitiven
**Touch**-Paddles verwendet wird. Wenn du möchtest, kannst du stattdessen
ein **mechanisches** Paddle oder eine Handtaste an CN3 anschließen — dazu
ist keine Hardware-Änderung nötig, nur diese Einstellung.

Der Menüpunkt zeigt die aktuelle Einstellung an: **CN3: Touch**
(Standard) oder **CN3: Mechan.** So änderst du sie:

1.  Starte deinen M32, während du ein Paddle (Touchpaddle, das Paddle an
    der 3,5-mm-Klinke oder eine Handtaste) gedrückt hältst, um in das
    Hardware-Konfigurationsmenü zu gelangen.
2.  Drehe den ENCODER, bis der CN3-Punkt erscheint, und drücke den
    ENCODER-Knopf.
3.  Wähle mit dem ENCODER **Touch** oder **Mechanical** und bestätige mit
    einem Druck auf den ENCODER-Knopf.

Wenn du die Einstellung geändert hast, startet der M32 neu, damit die
CN3-Pins im gewählten Modus hochfahren. Im **Mechanical**-Modus
funktionieren die Touch-Paddles an CN3 nicht mehr (die beiden Pins werden
stattdessen wie ein gewöhnliches Paddle gelesen); die 3,5-mm-Klinke
funktioniert in beiden Modi. Hinweis: Ein an CN3 angeschlossenes
mechanisches Paddle lässt dich auch dann in dieses Menü, wenn die
Einstellung noch auf Touch steht — du kommst also immer wieder in den
Umschaltdialog.

### Umdrehen des Bildschirms für linkshändige Bedienung

Dies ist wahrscheinlich nur für den M32Pocket relevant! Wenn du beim
M32Pocket mit der linken Hand geben möchtest, würde das Display auf dem
Kopf stehen. Mit dieser Konfigurationsoption wird das Display um 180°
gedreht, um linkshändige Bedienung zu ermöglichen.

1.  Starte deinen M32, während du die Touchpaddles (oder externe Paddles
    oder eine Handtaste) gedrückt hältst.
2.  Wähle mit dem Drehgeber die Option **Flip Screen** aus und drücke
    den ENCODER-Knopf.

Der M32 startet neu, wobei das Display nun um 180° gedreht ist.

### Kalibrierung der Akkumessung

::: note
Beim M32Pocket ist eine Kalibrierung der Akkumessung nicht notwendig
und daher nur bei Morserinos der 1st und 2nd Edition verfügbar.
:::

Die eingebaute Fähigkeit der Heltec-Module, die Akkuspannung zu messen,
ist leider nicht sehr zuverlässig. Verschiedene Faktoren tragen zu dem
Problem bei: ein Messfehler im ESP32-Prozessor aufgrund einer leichten
Abweichung der Referenzspannung für jeden Chip (was zu einem relativ
kleinen Fehler führt) und Probleme mit der Spannungsteilerschaltung
auf dem Heltec-Modul (was zu ziemlich großen Abweichungen zwischen den
Modulen führt). Obwohl das Messen des Akkus für den Betrieb des
Morserino-32 nicht sehr wichtig ist, ist es dennoch lästig und kann auch
dazu führen, dass sich der M32 nicht einschalten lässt, weil die Firmware
die Spannung für zu niedrig hält, obwohl sie in Wirklichkeit noch
ausreichen würde.

Um die Spannungsmessung zu kalibrieren, musst du die tatsächliche
Akkuspannung deines Morserino-32 mit einem Multimeter messen. Sobald du
diesen Wert kennst, führe folgende Schritte durch:

1.  Starte deinen M32, während du die Touchpaddles (oder externe Paddles
    oder eine Handtaste) gedrückt hältst.
2.  Wähle die Option **Calibr. Batt.** mit dem ENCODER aus und drücke
    den ENCODER-Knopf.
3.  Auf dem Display wird ein Spannungswert (in Millivolt) angezeigt.
    Drehe den ENCODER, bis der angezeigte Wert so nah wie möglich an
    der gemessenen Akkuspannung liegt.
4.  Drücke den ENCODER-Knopf, um den Kalibrierungswert zu speichern und
    mit einem Neustart des M32 fortzufahren.

::: note
Wenn dein Akku relativ neu ist und du sicher weißt, dass er vollständig
geladen ist (mehr als 10 Stunden Ladezeit bei Morserinos der 1st & 2nd
Edition, mehr als 3 Stunden beim M32Pocket), kannst du von einer
Akkuspannung von 4,2 V ausgehen und diesen Wert anstelle einer Messung
verwenden.
:::

### Konfigurieren von LoRa-Band, Frequenz und Ausgangsleistung

Dieser Abschnitt gilt natürlich nur, wenn du einen Morserino mit
LoRa-Fähigkeit hast (z.B. einen Morserino der 1st oder 2nd Edition).

Wenn du ein Standard-433-MHz-Heltec-Modul in deinem Morserino-32
(1st oder 2nd Edition) hast – was praktisch immer der Fall ist –, wurde
es bereits für das richtige Band und eine Standardfrequenz innerhalb
dieses Bands vorkonfiguriert.

Wenn du entweder die LoRa-Frequenz innerhalb des Standardbandes ändern
musst oder ein Heltec-Modul für die 868- und 920-MHz-Bänder verwendest,
musst du deinen Morserino-32 konfigurieren, bevor du die LoRa-Funktion
nutzt.

Folgende Bänder und Frequenzbereiche können im Morserino-32 für Heltec-
Module konfiguriert werden, die die oberen UHF-LoRa-Module unterstützen:

-   868-MHz-Band: 866,25 bis 869,45 MHz in Schritten von 100 kHz
    (Standard: 869,15 MHz)
-   920-MHz-Band: 920,25 bis 923,15 MHz in Schritten von 100 kHz
    (Standard: 920,55 MHz)

Das Standard-Heltec-Modul unterstützt nur das 433-MHz-Band; der
Morserino-32 kann auf 433,65 bis 434,55 MHz in Schritten von 100 kHz
konfiguriert werden (Standard: 434,15 MHz).

Um den Morserino-32 für nicht standardmäßige Frequenzen und Bänder zu
konfigurieren oder die Ausgangsleistung einzustellen, gehe wie folgt vor:

1.  Starte deinen M32, während du die Touchpaddles (oder externe Paddles
    oder eine Handtaste) gedrückt hältst.
2.  Wähle die Option **LoRa Config.** mit dem ENCODER aus.
3.  Zunächst wirst du aufgefordert, das gewünschte Band auszuwählen
    (wähle 433 für das Standard-LoRa-Modul, 868 oder 920 für das obere
    UHF-LoRa-Modul). Drehe den ENCODER auf das gewünschte Band und
    klicke einmal. **Die Bandauswahl muss zum verwendeten Heltec-Modul
    passen!**
4.  Nun wirst du aufgefordert, eine Frequenz innerhalb des gewählten
    Bands auszuwählen. Die erste angezeigte Frequenz ist die
    Standardfrequenz für dieses Band – wenn das passt, klicke einfach
    einmal auf den ENCODER-Knopf.
5.  In einem weiteren Schritt kannst du die Ausgangsleistung des
    LoRa-Transceivers einstellen. Der Standardwert ist 14 dBm (= 25 mW);
    du kannst ihn in mehreren Schritten zwischen 10 dBm (= 10 mW) und
    20 dBm (= 100 mW) einstellen.

::: warning
*Beachte die in deinem Land geltenden Vorschriften – es könnte eine
gesetzliche Begrenzung der Ausgangsleistung geben! Außerdem gilt: Je
höher die Ausgangsleistung, desto höher das Risiko, den LoRa-Transceiver
zu zerstören, wenn er ohne geeigneten Abschluss (Antenne oder
Kunstantenne) verwendet wird.*
:::

Unmittelbar danach startet der Morserino-32 normal mit den neu gewählten
LoRa-Einstellungen. In der obersten Zeile des Startbildschirms siehst du
die konfigurierte QRG für LoRa als fünfstellige Zahl (z.B. 43415 für
den Standard im 433-MHz-Band).

### Zurücksetzen der Einstellungen auf Werksstandard

Dieser Menüpunkt setzt alle Einstellungen auf die Werkseinstellungen
zurück, ändert aber keine Hardware-Konfigurationseinstellungen und löscht
oder verändert keine in Schnappschüssen gespeicherten Einstellungen.

## Anhang 2: Weitere Informationen über LoRa

Wenn der Morsecode in ein LoRa-Datenpaket verpackt wird, werden Punkte,
Striche und Pausen kodiert – es wird nicht der Klartext als
ASCII-Zeichen gesendet. Daher ist es möglich, „illegale" Morsezeichen
oder Zeichen zu senden, die nur in bestimmten Sprachen verwendet werden.
Sie werden korrekt übertragen (erscheinen aber auf dem Display als nicht
dekodierbar).

Das Senden des Codes Wort für Wort bedeutet, dass es eine erhebliche
Verzögerung zwischen Sender und Empfänger gibt, die stark von der Länge
der gesendeten Wörter und der verwendeten Geschwindigkeit abhängt. Da
die meisten Wörter in einem typischen CW-Gespräch eher kurz sind (7
Zeichen oder mehr sind bereits ein sehr langes Wort), ist das kein Grund
zur Sorge – es sei denn, ihr sitzt beide im selben Raum und verwendet
keine Kopfhörer, dann wird es wirklich verwirrend. Aber versuche
einmal, wirklich lange Wörter (10 oder mehr Zeichen) bei wirklich
niedriger Geschwindigkeit (5 WpM) zu senden, dann siehst du, was ich
meine!

#### Verwendung von zwei verschiedenen LoRa-„Kanälen" {-}

LoRa-Datenpakete werden mit einem sogenannten „Sync Word" adressiert –
Empfänger verwerfen Pakete, die nicht das erwartete Sync Word enthalten.

Der Morserino-32 kann zwei verschiedene Sync Words verwenden und damit
effektiv zwei verschiedene „Kanäle" schaffen. Dies kann z.B. in einem
Klassenzimmer genutzt werden, um zwei unabhängige Gruppen zu bilden, die
sich nicht gegenseitig stören.

Normalerweise arbeitet der M32 LoRa mit dem Sync Word 0x27 (wir nennen
es den „Standard"-Kanal), kann aber über die Einstellung **Trx Channel**
im Einstellungsmenü auf 0x66 (genannt „Secondary"-Kanal) umgeschaltet
werden. Siehe Abschnitt **6.2.6 Einstellungen zum Senden, Dekodieren und QSO Bot**.

#### Verwendung anderer LoRa-Frequenzbänder und/oder Frequenzen {-}

Standardmäßig werden die Morserino-32-Bausätze mit einem LoRa-Modul
ausgeliefert, das im 70-cm-Band arbeitet, mit der Standardfrequenz
434,150 MHz (innerhalb des 70-cm-Amateurbandes und eines ISM-Bandes der
Region 1).

Wenn du diese Frequenz aus irgendeinem Grund nicht nutzen kannst (z.B.
aufgrund von Bandplänen oder regulatorischen Gründen), kannst du die
Frequenz auf dem Standard-LoRa-Modul in 100-kHz-Schritten zwischen
433,65 und 434,55 MHz ändern.

Wenn du eine LoRa-Frequenz um 868 MHz oder 920 MHz benötigst, musst du
dir ein Heltec-Modul besorgen, das diesen höheren Frequenzbereich
unterstützt (was inzwischen schwierig sein kann, da Heltec die Versionen
dieser Module, die in Morserinos der 1st und 2nd Edition verwendet
wurden, eingestellt hat). Für diese Bänder **musst** du deinen Morserino
entsprechend konfigurieren.

Siehe **Anhang 1 Hardware-Konfigurationsmenü** für die Konfiguration von
LoRa für Module, die die 868- und 920-MHz-Bänder unterstützen, und für
die Änderung der LoRa-Frequenzeinstellungen.

#### Technische Details von LoRa Trx {-}

-   Frequenz: Standard 434,150 MHz – siehe oben für andere Frequenzen
-   LoRa Spreading Factor: 7
-   LoRa Bandbreite: 250 kHz
-   LoRa CRC: kein CRC
-   LoRa Sync Word: 0x27 (= dezimal 39) für den Standardkanal, 0x66
    (= dezimal 102) für den Sekundärkanal
-   HF-Ausgangsleistung: standardmäßig 14 dBm (= 25 mW), kann auf
    20 dBm (100 mW) erhöht werden


## Anhang 3: Einstellen des Audiopegels

Du kannst **eine weitere Funktion** erreichen, während du dich im
Startmenü befindest – nicht durch eine Menüauswahl, sondern durch einen
**langen Druck auf die FN-Taste**:

::: important
Diese Funktion verhält sich beim M32Pocket anders als bei der 1st und
2nd Edition!
:::

**Nur bei M32 1st und 2nd Edition:** Damit wird eine Funktion zum
Einstellen des **Audioeingangspegels** gestartet. Stelle sicher, dass
ein Tonsignal am Eingang anliegt (z.B. von deinem Kurzwellenempfänger),
und ein Balkendiagramm zeigt die Spannung des Eingangssignals an. Stelle
es mit dem blauen Trimmpotentiometer so ein, dass sich die linken und
rechten Enden des durchgezogenen Balkens innerhalb der zwei äußeren
Rechtecke befinden.

**Alle Morserinos:** Wenn diese Funktion aktiv ist, wird ein Sinussignal
auf dem Line-Out ausgegeben und der Transceiver-Ausgang kurz geschlossen
(Tastung eines Senders, falls einer angeschlossen ist – trenne deinen
Transceiver vorher ab, wenn das nicht gewünscht ist!). Du kannst nun
z.B. den Ausgangspegel an einem angeschlossenen Computer einstellen oder
prüfen, ob ein Sender getastet wird.

**Nur für 1st und 2nd Edition:** Ein einfacher Test oder eine Demo für
die Audio-Eingangseinstellung ist, den Line-Out mit dem Audio-In zu
verbinden (Spitze mit Hülse verbinden) und die Sinuswelle des Ausgangs
in den Audioeingang einzuspeisen. Wenn du das Potentiometer drehst,
siehst du an einem Ende des Potentiometerbereichs, wie der durchgezogene
Balken kleiner wird und nur ein winziger Balken in der Mitte übrig
bleibt, während die beiden Rechtecke an beiden Enden der Grafik sichtbar
werden (du misst im Wesentlichen nur das Rauschen am Eingang des
Operationsverstärkers). Am anderen Ende des Potentiometerbereichs
erstreckt sich der Balken über die Rechtecke beider Enden hinaus. Stelle
das Potentiometer so ein, dass der durchgezogene Balken die äußeren
Grenzen der Rechtecke fast berührt. Das ist die optimale Einstellung für
den Audioeingangspegel. Natürlich musst du dies für die Audioquelle
durchführen, die du verwenden möchtest, z.B. deinen Funkempfänger.

::: note
Nur wenn du dich **im Menü** befindest, aktiviert ein langer Druck auf
die FN-Taste die Pegeleinstellungsfunktion.<br>
Während du einen der Morserino-Modi ausführst (Keyer, Generator, Echo
Trainer, Transceiver usw.), aktiviert ein langer Druck auf die FN-Taste
den **Scroll-Modus des Displays**, damit du bereits weggeschrollten Text
lesen kannst.
:::


## Anhang 4: Aktualisieren der Firmware über WLAN für Versionen < 2.0

Bei den Firmware-Versionen **1.x** waren die WLAN-Funktionen nicht
direkt über das Hauptmenü zugänglich, sondern **durch dreimaliges
schnelles Drücken der FN-Taste**. Sobald du im WLAN-Menü bist, ist der
Ablauf für die verschiedenen WLAN-Funktionen derselbe wie in Abschnitt
**5.8 WiFi Functions** beschrieben.

Natürlich kannst du auch **über USB aktualisieren**, wenn du noch eine
ältere Softwareversion verwendest (siehe nächste zwei Anhänge).

::: important
Wenn du noch Version 1.x verwendest, solltest du wirklich auf eine
neuere Firmware aktualisieren – sonst entgehen dir viele neue Funktionen
(und Fehlerbehebungen).
:::


## Anhang 5: Aktualisieren der Firmware über USB und ein Update-Programm

Dieses einfache Aktualisierungsverfahren für Morserinos wurde durch die
Arbeit von Matthias Jordan und Joe Wittmer möglich.

Für einen M32 der 1st oder 2nd Edition stelle sicher, dass du einen
**Treiber** für den Silicon Labs CP210x USB-zu-Seriell-Chip hast, den
das Heltec-Modul für seine USB-Schnittstelle verwendet. Aktuelle
Versionen von Windows 10 installieren diesen automatisch; falls nicht,
findest du den Treiber hier:<br>
*https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers*

Um zu prüfen, ob du den richtigen Treiber installiert hast und an
welchem Port er eingebunden ist, öffne den Geräte-Manager auf deinem
Computer (unter Windows: im Suchfeld unten links „Einstellungen: Gerät"
eingeben). OSX- oder Linux-Benutzer können mit Kommandozeilen-Tools die
verfügbaren seriellen Ports ermitteln.

Verbinde deinen Morserino mit einem USB-Kabel mit deinem Computer. Unter
Windows sollte der Geräte-Manager seinen Bildschirm aktualisieren und
einen Eintrag „Ports" anzeigen – öffne ihn, er sollte etwas wie „Silicon
Labs CP210x ... (COM3)" anzeigen. Bei dir könnte es ein anderer
COM-Port sein, also merke dir den richtigen Portnamen.

::: important
Stelle sicher, dass du ein „richtiges" USB-Kabel hast und nicht nur ein
Ladekabel!
:::

Lade nun das Update-Programm aus Joes GitHub-Repository herunter und
achte darauf, die richtige Zip-Datei für dein Betriebssystem zu wählen:<br>
*https://github.com/joewittmer/Morserino-32-Firmware-Updater/releases*

Entpacke die Datei. Du findest ein Programm (unter Windows)
*update_m32.exe* (ohne Dateinamenerweiterung unter anderen Betriebssystemen) –
kopiere es in einen Ordner deiner Wahl (z.B. Downloads). Hole dir dann
die binäre Morserino-Firmware-Datei für die gewünschte Version vom
Morserino-GitHub, idealerweise in denselben Ordner.

::: note
Beachte, dass es ab V.7.0 zwei verschiedene Binaries für jede
Firmware-Version gibt: eine für Morserinos der 1st und 2nd Edition und
eine für den M32Pocket!
:::

Öffne nun ein Befehlsfenster auf deinem Computer (unter Windows: im
Suchfeld unten links „cmd" eingeben). Wechsle zunächst mit „cd" in das
Verzeichnis, in dem sich das Dienstprogramm und die Binärdatei befinden;
z.B. bei Verwendung des Download-Ordners:

`cd Downloads`

Gib dann folgende Befehlszeile ein:

`update_m32 -p <COMx> -f <binaryfilename>`

Ersetze \<COMx> durch deinen COM-Port-Namen und \<binaryfilename> durch
den korrekten Namen der Morserino-Binärdatei.

::: note
Für den M32Pocket musst du den Parameter `-d M32Pocket` zur
Befehlszeile hinzufügen.
:::

In meinem Fall war das (auf einem Windows-Rechner):

`update_m32 -p COM3 -f m32_V8.0.bin`

oder unter OSX oder Linux für einen M32Pocket:

`./update_m32 -p /dev/tty[...] -d M32Pocket -f m32_V8.0.bin`

Nach kurzer Zeit sollte dein Morserino neu starten und die aktualisierte
Versionsnummer anzeigen.

Es gibt auch die Möglichkeit, den permanenten Speicher des M32 vor der
erneuten Firmware-Installation vollständig zu löschen; das kann nützlich
sein, wenn der permanente Speicher beschädigt wurde. Dazu fügst du den
optionalen Löschparameter `-e` hinzu. Beispiel (hier für OSX oder
Linux):

`./update_m32 -p /dev/tty[...] -f m32_V8.0.bin -e`


## Anhang 6: Aktualisieren der Firmware über USB und einen Browser (Webserial)

Einige Browser unterstützen die Webserial-Erweiterung, mit der man
direkt vom Browser aus auf eine serielle Schnittstelle zugreifen kann.
Derzeit sind das **Google Chrome** und **Microsoft Edge**, und zumindest
auf einigen Plattformen auch **Opera**. Mit einem solchen Browser wird
das Aktualisieren der Firmware auf die neueste Version sehr einfach –
kein Firmware-Download notwendig, keine Befehlszeilen.

Stelle sicher, dass du ein USB-Kabel hast, das Datenübertragung
ermöglicht.

::: note
Wenn du einen M32Pocket verwendest, muss das Gerät eingeschaltet sein
und darf sich nicht im Schlafmodus befinden, wenn du das USB-Kabel
anschließt!

Beim M32 der 1st und 2nd Edition stelle sicher, dass du einen Treiber
für den SiLab CP210x-Chip hast.

Diese Methode funktioniert **NICHT** mit Firefox oder Safari!
:::

Um die Firmware zu aktualisieren, schließe deinen Morserino-32 über USB
an deinen Computer an, gehe zu *https://www.morserino.info*, suche die
Seite für Firmware-Updates und **folge den Anweisungen dort**.


## Anhang 7: Einrichten von M32-Einstellungen über einen Browser und Hochladen von Textdateien

Es gibt derzeit drei Websites, auf denen du M32-Einstellungen über einen
Browser auf deinem Computer vornehmen kannst, sowie eine weitere, die
einen M32-Dateimanager implementiert.

* Es gibt das „offizielle" morserino.info-Tool unter
  [`https://www.morserino.info/m32_config_tool.html`](https://www.morserino.info/m32_config_tool.html).
  Es ermöglicht das Anzeigen und Setzen aller Einstellungen sowie das
  Erstellen, Löschen, Anzeigen und Vergleichen von Schnappschüssen.
  Außerdem enthält es Datei-Hilfsprogramme (siehe unten).

* Christof, **OE6CHD**, hat ein Javascript-Programm geschrieben, mit
  dem es sehr einfach ist, Einstellungen, WLAN-Zugangsdaten und
  Keyer-Speicher festzulegen sowie Textdateien für den File Player
  hochzuladen.

* Ähnliche Funktionen hat Oliver, **OM0RX**, in seine Anwendung
  *Morse Trainer Pro* integriert.

Am schnellsten geht das alles, indem du deinen Morserino über USB mit
einem Computer mit Chrome, Edge oder Opera (oder einem anderen Browser,
der das Webserial-Protokoll unterstützt) verbindest und eine der
folgenden Seiten aufrufst:

* [`https://www.morserino.info/m32_config_tool.html`](https://www.morserino.info/m32_config_tool.html)
  (das „offizielle" Konfigurationstool)

* [`https://tegmento.org/`](https://tegmento.org/)
  (die Anwendung von OE6CHD)

* [`https://morsetrainerpro.com/morserino-config.html`](https://morsetrainerpro.com/morserino-config.html)
  (die Anwendung von OM0RX)

::: note

* Das „offizielle" Tool ist ein einfaches HTML/Javascript-Programm, das
  auch lokal ausgeführt werden kann. Der Quellcode ist auch auf der
  Morserino-GitHub-Seite verfügbar, wo du auch eine Hilfedatei im
  JSON-Format findest, die Tooltipps innerhalb der Anwendung ermöglicht
  (und den Zweck der verschiedenen Einstellungen erklärt).

* Dieses Tool enthält auch einen **Dateimanager** zum Hochladen von
  Text- und Tondateien, zum Anzeigen der Standard-Textdatei sowie einen
  **Datei-Builder**, mit dem du mehrere Dateien zu einer mehrteiligen
  Datei kombinieren kannst, die mit dem M32-File-Player verwendet werden
  kann. **Der Datei-Builder akzeptiert nicht nur Textdateien, sondern
  auch PDF-, EPUB- und Textverarbeitungsformate und konvertiert sie in
  einfachen Text, der für den File Player geeignet ist.**

* Du kannst Christofs Javascript-Programm auch lokal ausführen; der
  Quellcode ist unter *https://github.com/cdaller/morserino32-trainer*
  verfügbar.
:::

::: important
Wenn du einen M32Pocket verwendest, muss das Gerät eingeschaltet sein
und darf sich nicht im Schlafmodus befinden, wenn du das USB-Kabel
anschließt!
:::

Auf diesen Websites musst du auf eine „Connect"-Schaltfläche klicken
(in einem Pop-up-Fenster teilst du dem Browser dann mit, welchen Port er
verwenden soll); ansonsten erklären diese Websites alles, was du zur
Nutzung ihrer Funktionen wissen musst.

Zusätzlich zu den Einrichtungsfunktionen hat Christof auch einige
nützliche Trainingsfunktionen, einen QSO-Bot und eine Sprachausgabe für
alle Menüpunkte und Einstellungen implementiert, um sehbehinderten
Operatoren zu helfen. Das alles findest du auf
[https://tegmento.org](https://tegmento.org/).

Auch Oliver hat auf seiner Website viele zusätzliche Trainingsfunktionen
implementiert, zu finden unter
[https://morsetrainerpro.com/index.html](https://morsetrainerpro.com/index.html).

Das Datei-Tool gibt auch Informationen über den freien Speicherplatz für
Dateien aus, und du kannst damit auch Dateien auf dem M32 löschen.

Diese Funktionen wurden durch die Implementierung eines seriellen
Protokolls im Morserino ermöglicht; weitere Informationen findest du im
nächsten Anhang.


## Anhang 8: Nutzung des seriellen Ausgangs des M32

Der Morserino-32 ist in der Lage, Daten über die serielle
USB-Schnittstelle auszugeben. Damit kannst du z.B. die Zeichen, die auf
dem Display angezeigt werden, in einem Computerterminalfenster
darstellen. Auf diese Weise kannst du die Morserino-Ausgabe auf einer
großen Leinwand oder einem Projektor anzeigen – nützlich für
Präsentationen oder im Unterricht.

In Version 5 wurde ein vollständiges Zwei-Wege-Protokoll namens „M32
Serial Protocol" eingeführt. Es ermöglicht (über Software auf einem per
USB angeschlossenen Computer) Bildschirm- oder Sprachausgabe von Menüs
und Einstellungen (z.B. um den M32 für blinde oder sehbehinderte
Menschen nutzbar zu machen) und erlaubt außerdem die Fernsteuerung aller
Morserino-Funktionen vom Computer aus (Einstellungen ändern, Geschwindigkeit
und Lautstärke anpassen, Menüs aufrufen und verlassen, automatische
CW-Erzeugung). Das Protokoll ist in einem separaten Dokument beschrieben,
das auf GitHub verfügbar ist.

::: note
Wenn du einen M32Pocket verwendest, muss das Gerät eingeschaltet sein
und darf sich nicht im Schlafmodus befinden, wenn du das USB-Kabel
anschließt!
:::

Für die serielle Schnittstelle des angeschlossenen Computers muss eine
Baudrate von **115200** gewählt werden.

Du kannst die serielle Kommunikation in Verbindung mit speziell für den
Morserino-32 geschriebener Software nutzen, um deine Trainingsleistungen
zu verbessern. Derzeit gibt es vier Softwareprodukte für diesen Zweck:

-   **Morserino-32 CW Training** von Christof, OE6CHD (siehe
    *https://tegmento.org/*), das die seriellen Protokollfunktionen
    nutzt – siehe auch den vorherigen Anhang.
-   **Morse Trainer Pro** von Oliver, OM0RX (sehr umfangreiche
    Funktionalität; einige Funktionen erfordern eine kostenpflichtige
    Mitgliedschaft, die Morserino-Einrichtungsfunktionen sind kostenlos;
    siehe [*https://morsetrainerpro.com/index.html*](https://morsetrainerpro.com/index.html)
    und den vorherigen Anhang).
-   **CW Trainer for Morserino** von Enzo, IW7DMH (siehe
    *https://iw7dmh.jimdofree.com/other-projects/cw-trainer-for-morserino-32/*).
-   **Morserino Phrases Trainer** von Tommy, OZ1THC (siehe
    *https://github.com/Tommy-de-oz1thc/Morserino-32-Phrases-trainer*).

Siehe auch die Beschreibung der Einstellung **Serial Output** im
Abschnitt **6.2.1 Allgemeine Einstellungen**.

### Das M32-Serielle-Protokoll über Bluetooth nutzen (BLE Serial)

Das M32-Serielle-Protokoll ist nicht an das USB-Kabel gebunden: Wenn die
Einstellung **Bluetooth Use** (siehe Abschnitt **6.2.2 Einstellungen zu
Key, Paddles und Keyer**) auf **BLT Serial Prot.** steht, ist dasselbe
Protokoll auch über Bluetooth Low Energy verfügbar. Das ist besonders für Smartphones und
Tablets nützlich — unter iOS etwa können Apps überhaupt keine
klassische serielle Verbindung über Bluetooth öffnen, BLE hingegen
funktioniert problemlos. Eine App kann den Morserino genauso fernsteuern
wie ein über USB verbundenes Programm, einschließlich des Sendens von
Text, den der Morserino als CW tastet (`PUT cw/play/...`).

Nach dem Auswählen dieser Option (sie wird bei der nächsten Rückkehr
ins Hauptmenü wirksam) meldet sich der Morserino als
„**Morserino-32**", solange er sich im Hauptmenü oder in einem der
Trainingsmodi befindet und keine App verbunden ist. Ein Pairing ist
nicht erforderlich — das Verbinden funktioniert wie das Anstecken eines
USB-Kabels, und die App startet die Sitzung mit dem üblichen Kommando
`PUT device/protocol/on`.

Ein paar Dinge sind zu beachten:

-   USB- und BLE-Sitzungen sind unabhängig voneinander und können sogar
    gleichzeitig aktiv sein; jede wird mit ihrem eigenen
    `PUT device/protocol/on` / `off` begonnen und beendet.
-   Jede Funktion, die das WLAN benötigt (WiFi-Transceiver-Modi,
    Mehrspieler-Spiele, Datei-Upload, Firmware-Update,
    WLAN-Konfiguration), unterbricht die Bluetooth-Verbindung für die
    Dauer dieser Aktivität; nach der Rückkehr ins Hauptmenü steht sie
    wieder zur Verfügung.
-   Die Einstellung **Bluetooth Use** weist das Bluetooth-Funkmodul
    entweder der Tastaturausgabe (Anhang 9) oder dem seriellen
    Protokoll zu — BLE Serial und die Bluetooth-Tastatur können also
    nie gleichzeitig aktiv sein.
-   Eine aktive Protokollsitzung verhindert die automatische
    Abschaltung (Timeout), genau wie eine USB-Sitzung — denke daran im
    Akkubetrieb.

Für Entwickler: Die technischen Details (Service-UUIDs, Framing,
Transportverhalten) sind in der *M32 Protocol*-Beschreibung im Ordner
`Documentation/Protocol Description` auf GitHub dokumentiert; ein
fertiges Test- und Demoskript (`ble_m32_test.py`, mit Python und
*bleak*) findet sich im Ordner `devdocs/ble-serial`.

## Anhang 9: Benutzung der Bluetooth-Tastatur-Funktion

Im Modus CW Keyer kann der M32 die getasteten Morsezeichen als
Tastaturcodes über Bluetooth an einen Computer (einschließlich
Mobiltelefone und Tablets) senden.

Dazu muss die Einstellung **Bluetooth Use** auf eine der
Tastatur-Optionen gesetzt werden (nähere Informationen zu den
verfügbaren Optionen findest du im Abschnitt
**6.2.2 Einstellungen zu Key, Paddles und Keyer**).

::: note
Diese Funktion ist nur im Modus CW Keyer aktiv! Erst wenn CW Keyer gestartet wird, wird die Tastatur für PC und Tablett etc. sichtbar!
:::

::: note
Beachte, dass der Morserino wie eine Tastatur mit US-Tastenlayout
arbeitet – dies ist ggf. auf dem verwendeten Computer entsprechend
einzustellen.
:::


## Anhang 10: Gängige CW-Abkürzungen, verwendet vom Morserino-32

Die Liste enthält Definitionen auf Englisch und Deutsch, getrennt durch
einen Schrägstrich. Nicht alle Abkürzungen sind in allen Sprachen sehr
gebräuchlich; im Englischen ungewöhnliche Abkürzungen stehen in eckigen
Klammern \[\].

**A**

| Abk. | Bedeutung |
|------|-----------|
| aa | all after / alles nach |
| ab | all before / alles vor |
| abt | about / ungefähr, um |
| ac | alternating current / Wechselstrom |
| adr | address / Adresse |
| af | audio frequency / Niederfrequenz |
| agc | automatic gain control / automatische Verstärkungsregelung |
| agn | again / wieder |
| alc | automatic level control / automatische Pegelanpassung |
| am | amplitude modulation / Amplitudenmodulation |
| am | ante meridiem / vormittags |
| ans | answer / Antwort |
| ant | antenna (aerial) / Antenne |
| atv | amateur TV |
| avc | automatic volume control / automatische Lautstärkeregelung |
| award | Award / Amateurfunkdiplom |
| awdh | [good bye] / auf Wiederhören |
| awds | [good bye] / auf Wiedersehen |

**B**

| Abk. | Bedeutung |
|------|-----------|
| b4 | before / bevor |
| bc | broadcast / Rundfunk |
| bci | broadcast interference / Rundfunkstörung(en) |
| bcnu | be seeing you / hoffe dich wieder zu treffen |
| bd | bad / schlecht |
| bfo | beat frequency oscillator / Überlagerungsoszillator |
| bk | break / Aufforderung zur Unterbrechung |
| bpm | [characters per minute] / Buchstaben pro Minute |
| btr | better / besser |
| btw | by the way / nebenbei bemerkt |
| bug | Bug / mechanisch-automatische Taste |
| buro | (QSL) bureau / QSL-Büro |

**C**

| Abk. | Bedeutung |
|------|-----------|
| call | call sign / Rufzeichen |
| cfm | confirm / bestätige |
| cl | closing / schließe meine Station |
| conds | conditions / (Ausbreitungs-)Bedingungen |
| condx | conditions for dx / Bedingungen für DX |
| congrats | congratulations / gratuliere |
| cq | cq (calling anybody) / allgemeiner Anruf |
| cu | see you / hoffe auf ein weiteres Treffen |
| cuagn | see you again / hoffe auf ein weiteres Treffen |
| cul | call you later / rufe dich später |
| cw | continuous wave (= Morse code) / Morsetelegrafie |

**D**

| Abk. | Bedeutung |
|------|-----------|
| db | deziBel |
| dc | direct current / Gleichstrom |
| de | from (call sign) / von (Rufzeichen) |
| diff | difference / Unterschied |
| dr | dear / liebe(r) |
| dwn | down / abwärts, hinab |
| dx | (great) distance / große Entfernung |

**E**

| Abk. | Bedeutung |
|------|-----------|
| ee | end / Ende |
| el | (antenna) element(s) / (Antennen-)Element(e) |
| elbug | Elbug (= electronic bug) / Elbug (elektronische autom. Taste) |
| es | and / und |
| excus | excuse me / Entschuldigung |

**F**

| Abk. | Bedeutung |
|------|-----------|
| fb | fine business / ausgezeichnet |
| fer | for / für |
| fm | frequency modulation / Frequenzmodulation |
| fone | telephony / Telefonie |
| fr | for / für |
| frd | friend / Freund |
| freq | frequency / Frequenz |
| fwd | forward / vorwärts |

**G**

| Abk. | Bedeutung |
|------|-----------|
| ga | [good evening] / Guten Abend |
| gb | good bye / Auf Wiedersehen |
| gd | good / gut |
| gd | good day / guten Tag |
| ge | good evening / guten Abend |
| gl | good luck / viel Glück |
| gm | good morning / guten Morgen |
| gn | good night / gute Nacht |
| gnd | ground / Erde (Erdpotenzial) |
| gp | ground plane / Groundplane-Antenne |
| gs | greenstamp / 1-Dollar-Note |
| gt | [good day] / guten Tag |
| gud | good / gut |

**H**

| Abk. | Bedeutung |
|------|-----------|
| ham | ham (radio amateur) / Funkamateur |
| hf | high frequency / Hochfrequenz |
| hi | hi(larious) – laughing / ich lache |
| hpe | hope / ich hoffe |
| hr | here / hier |
| hrd | heard / gehört |
| hrs | hours / Stunden |
| hv | have / habe |
| hvy | heavy / schwer |
| hw | how (copy) / wie werde ich gehört? |

**I**

| Abk. | Bedeutung |
|------|-----------|
| i | I / ich |
| iaru | International Amateur Radio Union |
| if | intermediate frequency / Zwischenfrequenz |
| ii | I repeat / ich wiederhole |
| info | information / Information |
| inpt | input / Eingangsleistung |
| irc | international return coupon / internationaler Antwortschein |
| itu | International Telecommunications Union / internationale Fernmeldeunion |

**K**

| Abk. | Bedeutung |
|------|-----------|
| k | come (please answer) / bitte kommen (bitte antworten) |
| khz | kiloHertz |
| km | kiloMeter |
| kw | kiloWatt |
| ky | key / Morsetaste |

**L**

| Abk. | Bedeutung |
|------|-----------|
| lbr | [dear] / lieber |
| lf | low frequency / Niederfrequenz |
| lid | bad operator / Funker mit schlechter Betriebstechnik |
| lis | licensed / lizenziert |
| lng | long / lang |
| loc | locator / Standort(kenner) |
| log | log book / Stationstagebuch |
| lp | long path / langer Ausbreitungsweg |
| lsb | lower sideband / unteres Seitenband |
| luf | lowest usable frequency / niedrigste brauchbare Frequenz |
| lw | long wire (antenna) / Langdrahtantenne |

**M**

| Abk. | Bedeutung |
|------|-----------|
| ma | milliAmpere |
| mesz | [middle European summer time] / mitteleuropäische Sommerzeit |
| mez | [middle European time] / mitteleuropäische Zeit(zone) |
| mgr | (QSL) manager / (QSL-)Manager |
| mhz | megaHertz |
| min | minute / Minute |
| mins | minutes / Minuten |
| mm | maritime mobile / Station auf einem Schiff zur See |
| mni | many / viele |
| mod | modulation / Modulation |
| msg | message / Nachricht |
| mtr | meter / Messinstrument |
| muf | maximum usable frequency / höchste brauchbare Frequenz |
| my | my / mein |

**N**

| Abk. | Bedeutung |
|------|-----------|
| n | no / nein, kein |
| net | network / (Funk-)Netzwerk |
| nf | [low frequency] / Niederfrequenz |
| nil | nothing / nichts |
| no | no / nein, kein |
| nr | near / nahe |
| nr | number / Nummer |
| nw | now / jetzt |

**O**

| Abk. | Bedeutung |
|------|-----------|
| ok | ok / in Ordnung |
| om | old man, ham / Anrede f. Funkamateur |
| op | operator / Funker |
| osc | oscillator / Oszillator |
| oscar | OSCAR (satellite) / OSCAR-Amateurfunksatellit |
| output | output / Ausgangsleistung |
| ow | old woman / Funkamateurin |

**P**

| Abk. | Bedeutung |
|------|-----------|
| pa | power amplifier / Endstufe, Leistungsverstärker |
| pep | peak envelop power / Hüllkurvenspitzenleistung |
| pm | post meridiem, afternoon / Nachmittag |
| pse | please / bitte |
| psed | pleased / erfreut |
| pwr | power / Leistung |
| px | prefix / Präfix, Landeskenner |

**Q**

| Abk. | Bedeutung |
|------|-----------|
| qaz | closing because of thunderstorm / ich beende wegen Gewitter |
| qra | name of my station is… / der Name meiner Funkstelle ist |
| qrb | distance between stations is … / Entfernung zwischen den Stationen ist… |
| qrg | exact frequency is … / genaue Frequenz ist … |
| qrl | I am busy, don't interfere / bin beschäftigt, bitte nicht stören |
| qrm | interference / Störung |
| qrn | atmospheric noise (static) / atmosphärische Störungen |
| qro | increase power / erhöhe die Senderleistung |
| qrp | decrease power / vermindere die Senderleistung |
| qrq | send faster / gib schneller |
| qrs | send slower / gib langsamer |
| qrt | suspending operation / Einstellen des Sendebetriebs |
| qru | I have nothing (more) for you / ich habe nichts (weiteres) für dich |
| qrv | I am ready / ich bin betriebsbereit |
| qrx | will call you again (on frequ. …) / werde dich wieder anrufen (auf Frequ. …) |
| qrz | you are called by … / du wirst von … gerufen |
| qsb | your signals are fading / die Stärke deiner Zeichen schwankt |
| qsk | I can hear between my signals / ich kann zwischen meinen Zeichen hören |
| qsl | I acknowledge receipt / ich gebe Empfangsbestätigung |
| qso | I can communicate (with …) directly / ich kann direkt (mit …) verkehren |
| qsp | I will relay (to …) / ich werde (an …) vermitteln |
| qst | broadcasting to all / Nachricht an alle |
| qsy | change (transmit) frequency to … / ändere (Sende-)Frequenz auf… |
| qsz | send each word twice / jedes Wort zweimal senden |
| qtc | I have messages for you / ich habe Nachrichten für dich |
| qth | my position is … / mein Standort ist … |
| qtr | correct time UTC is … / genaue Zeit UTC ist … |

**R**

| Abk. | Bedeutung |
|------|-----------|
| r | right, received, "roger" / richtig, (korrekt) empfangen |
| rcvd | received / empfangen |
| re | regarding / bezüglich |
| ref | reference / Bezug, Referenz |
| rf | radio frequency / Hochfrequenz |
| rfi | radio frequency interference / Hochfrequenzstörung |
| rig | rig, equipment / Stationseinrichtung, -ausstattung |
| rprt | report / Rapport (Empfangsbericht) |
| rpt | repeat / wiederhole |
| rst | RST (readability, signal strength, tone) / Lesbarkeit, Lautstärke, Ton |
| rtty | radio teletype / (Funk-)Fernschreiben |
| rx | receiver / Empfänger |

**S**

| Abk. | Bedeutung |
|------|-----------|
| sase | self addressed stamped envelope / frankiertes Kuvert mit eigener Adresse |
| shf | super high frequency / Zentimeterwellenbereich |
| sigs | signs / Zeichen |
| sked | schedule / Verabredung |
| sn | soon / bald |
| sp | short path / kurzer Ausbreitungsweg |
| sri | sorry / tut mir leid |
| ssb | single sideband / Einseitenbandmodulation |
| sstv | slow scan tv / Schmalbandfernsehen |
| stn | station / Station |
| sure | sure / sicher |
| swl | short wave listener / Kurzwellenhörer |
| swr | standing wave ratio / Stehwellenverhältnis |

**T**

| Abk. | Bedeutung |
|------|-----------|
| t | turns, tera-, abbr. f. 0 / Windungen, tera-, Abk. f. 0 |
| temp | temperature / Temperatur |
| test | test, contest / Versuch, Kontest |
| tia | thanks in advance / danke vorab |
| tks | thanks / danke |
| tnx | thanks / danke |
| trx | transceiver / Sendeempfänger |
| tu | thank you / danke dir |
| tvi | TV interference / Fernsehstörungen |
| tx | transmitter / Sender |

**U**

| Abk. | Bedeutung |
|------|-----------|
| u | you / du (Sie) |
| ufb | ultra fine business / ganz ausgezeichnet |
| uhf | ultra high frequency / Ultrakurzwelle, Dezimeterwellenbereich |
| ukw | (very high frequency, vhf) / Ultrakurzwelle |
| unlis | unlicensed / unlizenziert (Pirat) |
| up | up / nach oben |
| ur | your / dein |
| usb | upper sideband / oberes Seitenband |
| utc | universal time coordinated / Koordinierte Weltzeit |

**V**

| Abk. | Bedeutung |
|------|-----------|
| v | variable (frequency), voice / variable (Frequenz), Telefonie |
| vert | vertical (antenna) / Vertikalantenne |
| vfo | variable frequency oscillator / Oszillator mit einstellbarer Frequenz |
| vhf | very high frequency / UKW-Bereich, Meterwellenbereich |
| vl | [many] / viel |
| vln | [many] / vielen |
| vy | very / sehr |

**W**

| Abk. | Bedeutung |
|------|-----------|
| w | Watt |
| watts | watts / Watt (Plural) |
| wid | with / mit |
| wkd | worked / gearbeitet |
| wkg | working / arbeite gerade |
| wl | will / werde |
| wpm | words per minute / Wörter pro Minute |
| wtts | watts / Watt (Plural) |
| wx | weather / Wetter |

**X**

| Abk. | Bedeutung |
|------|-----------|
| xcus | excuse me / Entschuldigung |
| xcvr | transceiver / Sendeempfänger |
| xmas | christmas / Weihnachten |
| xtal | crystal / Quarz |

**Zahlen**

| Abk. | Bedeutung |
|------|-----------|
| 33 | female ham greeting / Gruß unter Funkerinnen |
| 44 | WFF greetings |
| 55 | [I wish you success] / Viel Erfolg! |
| 72 | QRP greeting / Gruß unter QRP-Stationen |
| 73 | best wishes / viele Grüße |
| 88 | love and kisses / Alles Liebe |
| 99 | get lost / verschwinde! |

---

## Anhang 11: Glossar

| Begriff | Definition |
|---------|------------|
| Access Point | Ein WLAN-Router oder eine Basisstation, mit der sich WLAN-Geräte verbinden, um über ein Netzwerk zu kommunizieren. |
| BLE | Bluetooth Low Energy – eine energiesparende Variante von Bluetooth, die für die drahtlose Tastaturausgabe und für das M32-Serielle-Protokoll über Bluetooth (siehe Einstellung Bluetooth Use) verwendet wird. |
| CW | Continuous Wave – der traditionelle Begriff für Morsecode-Kommunikation, bei der ein Radioträger ein- und ausgetastet wird. |
| CW Keyer | Ein Gerät (oder Modus), das automatisch korrekt getimte Dits und Dahs aus der Paddle-Eingabe erzeugt. |
| Tiefschlaf | Ein Zustand mit sehr niedrigem Stromverbrauch, in dem der Mikrocontroller größtenteils abgeschaltet ist. Das Gerät kann durch Drücken der FN-Taste geweckt werden. |
| Dit / Dah | Die zwei Grundelemente des Morsecodes. Ein Dit ist ein kurzer Ton (eine Einheit), ein Dah ist ein langer Ton (drei Einheiten). Auch Punkt und Strich genannt. |
| Doppelhebel-Paddle | Eine Morsetaste mit zwei separaten Hebeln – einer für Dits, einer für Dahs. Wird mit iambischen Keyern verwendet. |
| ENCODER | Der Drehknopf am Morserino, der gedreht und gedrückt werden kann. Wird zur Navigation, Geschwindigkeits-/Lautstärkeanpassung und Zeichenauswahl verwendet. |
| ESP32 | Der Mikrocontroller (von Espressif Systems) im Herzen des Morserino-32. Er bietet WLAN, Bluetooth und Rechenleistung. |
| ESPNow | Ein Peer-to-Peer-WLAN-Protokoll von Espressif, das eine direkte Kommunikation zwischen ESP32-Geräten ohne Access Point ermöglicht. Die Reichweite beträgt typischerweise bis zu 50 Meter. |
| Farnsworth-Abstand | Eine Trainingstechnik, bei der Morsezeichen mit voller Geschwindigkeit gesendet werden, die Pausen zwischen den Zeichen aber verlängert sind. Dadurch werden Zeichenrhythmen erlernt, anstatt Dits und Dahs zu zählen. |
| FN-Taste | Der sekundäre Druckknopfschalter am Morserino (rot bei 1st/2nd Edition, ins Gehäuse integriert beim M32Pocket). Wird zum Umschalten zwischen Geschwindigkeit/Lautstärke, Scrollen, Helligkeit und anderen Funktionen verwendet. |
| Goertzel-Filter | Ein digitaler Signalverarbeitungsalgorithmus, der im CW-Decoder verwendet wird, um Töne bei einer bestimmten Frequenz (ca. 700 Hz) zu erkennen. |
| Iambic Keying | Ein Tastmodus, bei dem das gleichzeitige Drücken beider Paddles abwechselnde Dits und Dahs erzeugt. Benannt nach dem Versfuß Iambus mit abwechselnd kurzen und langen Silben. |
| iCW | Internet CW – eine Methode zur Übertragung von Morsecode über das Internet mithilfe von Audioprotokollen wie Mumble. |
| Koch-Methode | Eine von Ludwig Koch in den 1930er Jahren entwickelte Lernmethode. Zeichen werden einzeln in einer bestimmten Reihenfolge eingeführt, die auf rhythmischer Ähnlichkeit basiert – nicht alphabetisch. |
| LoRa | Long Range – eine Spreizspektrum-Funktechnologie für energiesparende Kommunikation über große Entfernungen. Wird in Morserinos der 1st und 2nd Edition für den drahtlosen CW-Transceiver-Modus verwendet. |
| M32Pocket | Die dritte Generation des Morserinos mit farbigem TFT-Display, USB-C und einem kompakten 3D-gedruckten Gehäuse. |
| MAC-Adresse | Media Access Control Address – eine eindeutige Hardware-Kennung, die jeder Netzwerkschnittstelle zugewiesen ist. Dient zur Identifizierung deines Morserinos in einem WLAN-Netzwerk. |
| mDNS | Multicast DNS – ein Protokoll, das es ermöglicht, Geräte über ihren Namen (z.B. *m32.local*) in einem lokalen Netzwerk zu finden, ohne einen dedizierten DNS-Server. Unterstützt auf macOS und den meisten Linux-Systemen; eingeschränkte Unterstützung unter Windows und Android. |
| MOPP | Morserino Packet Protocol – das Datenformat zur Kodierung von Morsecode-Elementen (Dits, Dahs, Pausen) in kompakte Binärpakete für die Übertragung via LoRa oder WiFi/ESPNow. |
| NVS | Non-Volatile Storage – persistenter Speicher auf dem ESP32, der Daten über Stromzyklen hinweg erhält. Wird zum Speichern von Einstellungen, Schnappschüssen, Koch-Lektionsfortschritt und Spieler-Identität verwendet. |
| Betriebszeichen | Prozedurales Zeichen (Pro Sign) – ein spezielles Morsezeichen, das durch das Zusammenziehen zweier Buchstaben ohne die normale Zeichenpause gebildet wird. Beispiele: \<SK> (Ende des Kontakts), \<AR> (Ende der Nachricht), \<KN> (bitte antworten, nur benannte Station). |
| QSK | Volles Break-In – die Fähigkeit, zwischen gesendeten Elementen zu hören. In LoRa/WiFi-Transceiver-Modi wegen der paketbasierten Übertragung nicht möglich. |
| RSSI | Received Signal Strength Indicator – eine Messung der Leistung eines empfangenen Funksignals. Wird im LoRa-Transceiver-Modus als S-Meter-Balken angezeigt. |
| Einhebel-Paddle | Eine Morsetaste mit einem Hebel, der zwischen zwei Kontakten schwingt – einer für Dits, einer für Dahs. Es kann immer nur ein Kontakt hergestellt werden (kein Zusammendrücken). |
| Sideswiper | Eine Morsetaste (auch Cootie Key genannt), bei der der Bediener einen Hebel seitwärts bewegt. Beide Seiten erzeugen dieselbe Ausgabe – der Bediener timt Dits und Dahs manuell. Die Touchpaddles des Morserinos können im Modus Straight Key einen Sideswiper emulieren. |
| Schnappschuss | Ein gespeicherter Satz aller Morserino-Einstellungen. Es können bis zu acht Schnappschüsse gespeichert und abgerufen werden, was einen schnellen Wechsel zwischen verschiedenen Trainingskonfigurationen ermöglicht. |
| Spreizspektrum | Eine Funktechnik, bei der das Signal über eine größere Bandbreite als für die Daten notwendig verteilt wird. LoRa nutzt dies, um bei geringer Leistung große Reichweiten zu erzielen. |
| Handtaste | Der einfachste Morsetastentyp – ein einzelner Hebel, der beim Herunterdrücken Kontakt gibt. Der Bediener timt alle Dits, Dahs und Pausen manuell. |
| Tone Shift | Eine Einstellung, die die Tonhöhe des Mithörtons beim Senden oder Antworten um einen Halbton höher oder tiefer als den Empfangston verschiebt. Nützlich zur Unterscheidung von gesendeten und empfangenen Signalen. |
| UDP | User Datagram Protocol – ein leichtgewichtiges Netzwerkprotokoll, das der Morserino für die WLAN-Transceiver-Kommunikation auf Port 7373 verwendet. |
| VBand | Eine internetbasierte CW-Übungsplattform (hamradio.solutions/vband). Der Morserino kann sich über Bluetooth mit VBand verbinden. |
| Webserial | Eine Browser-API (unterstützt in Chrome, Edge und Opera), die es Webseiten ermöglicht, direkt über USB mit seriellen Geräten zu kommunizieren. Wird für Firmware-Updates und die Einstellungskonfiguration verwendet. |
| Wordsworth-Abstand | Eine Trainingstechnik (ähnlich wie Farnsworth), bei der nur die Pausen zwischen Wörtern verlängert werden, während der Zeichenabstand normal bleibt. Fördert das Kopieren Wort für Wort im Kopf. |
| WpM | Wörter pro Minute – das Standardmaß für die Morsecode-Geschwindigkeit. Das Referenzwort ist PARIS (jedes Vorkommen entspricht 50 Dit-Längen), also gilt: 1 WpM = 5 Zeichen pro Minute. |

