#![](Images/M32c.png) 
#Morserino-32 Benutzer-Handbuch  


###Ein multif-funktionales Morsegerät, perfekt zum Lernen und Trainieren


![](Images/Morserino.jpg)
<div style="page-break-after: always;"></div>

## Anschlüsse und Bedienelemente

![](Images/M32_layout.jpg)

| # |Connector / Control  | Usage |
|:---:|:-----    |:----   |
| 1| 3.5mm Klinkenbuchse (2-polig): zum Sender | Verbinde diese mit deinem Sender oder Transceiver, wenn du diesen mit dem Morserino-32 tasten willst. |
| 2 | 3,5 mm Klinkenbuchse (4 Pole): Audio In / Line Out | **Audioeingang** für den CW-Decoder; Schließen Sie den Audioausgang eines Empfängers an, um CW-Signale zu dekodieren. **Audioausgabe** (fast reiner Sinus), die nicht durch die Einstellung der Lautsprecherlautstärke beeinflusst wird. Die Zuordnungen zur Buchse lauten wie folgt: Tip und 1. Ring - Audioeingang; 2. Ring: Masse; Sleeve: Audioausgang. |
| 3 | Audio-Eingangspegel | Passe den Audio-Eingangspegel mit Hilfe dieses Trimmers an. Für die Pegelanpassung gibt es eine spezielle Funktion, siehe Abschnitt "Startmenü" (gegen Ende des Abschnitts). |
| 4 | 3,5-mm-Klinkenbuchse (3 Pole): Kopfhörer | Schließe hier deine Kopfhörer an (alle Stereokopfhörer von Mobiltelefonen mit Klinkenbuchsen sollten geeignet sein), um über Kopfhörer zu hören und den Lautsprecher auszuschalten. Man kann keinen Lautsprecher direkt an diese Buchse anschließen, ohne zusätzliches Interface (dieser Ausgang benötigt eine Gleichstromverbindung mit Masse über 50 - 300 Ohm.) |
| 5 | Ein-/Aus-Schalter | Verbindet den LiPo-Akku mit dem Gerät, bzw. trennt ihn davon. Bei häufigem Gebrauch des Morserino-32 kann man den Akku angeschlossen lassen. Wenn man aber das Gerät mehrere Tage nicht benutzt, sollte es vom Akku getrennt werden, da er sonst langsam entladen wird. |
| 6 | SMA-Antennenbuchse | Anschluss für eine 430-MHz-Antenne für den LoRa-Betrieb. Beim Sednen mit loRa muss unbedingt eine Antenne angeschlossen sein! |
| 7 | ROTER (Power / Vol / Scroll) -Drucktaster | Wenn das Gerät ausgeschaltet ist (aber bei angeschlossenem Akku, d.h. im Tiefschlaf), wird das Gerät aktiviert und neu gestartet. Wenn das Gerät in Betrieb ist, schaltet der Drehgeber durch kurzes Drücken dieser Taste zwischen der Einstellung der Keyer-Geschwindigkeit und der Lautstärkeregelung um. Durch langes Drücken der Taste kann man die Anzeige mit dem Drehgeber scrollen. Durch erneutes Drücken der Taste wird die Funktion auf Geschwindigkeitsregelung zurückgesetzt. Wenn du dich im Menü befindest, wird durch langes Drücken der Modus für die Einstellung des Audio-Eingangspegels aktiviert, und durch kurzen Dreifachklick werden die WLAN-Funktionen aufgerufen. Weitere Einzelheiten siehe unten. |
| 8 | SCHWARZER Drehgeber-Knopf | Ermöglicht die Auswahl innerhalb von Menüs, dient zum Einstellen der Geschwindigkeit, der Lautstärke oder zum Scrollen der Anzeige sowie zum Einstellen verschiedener Parameter und Optionen. Kann gedreht und gedrückt werden (Drucktaster). |
| 9 | Anschlüsse für Touch Paddles | Diese Leiterplattensteckverbinder halten die kapazitiven Touch Paddles. Wenn du immer nur ein externes Paddel verwendest (oder für den Transport), kannst du die Touch-Paddles entfernen. |
| 10 | 3,5 mm Klinkenbuchse (3 Pole): Externes Paddel | Hier wird entweder ein externes (mechanisches) Paddel angeschlossen (Tip ist linkes Paddel, Ring ist rechtes Paddel, Sleeve ist Masse) oder eine einfache Handtaste (Straight Key). Mit einer solchen Taste kannst du den CW-Decoder verwenden, um die Qualität deiner Gebeweise zu überwachen und zu verbssern!
| 11 | Serielle Schnittstelle | Hier kann man ein Kabel (direkt oder über einen 4-poligen Stiftstecker) zu einem externen seriellen Gerät anschließen, z.B. einem GPS-Empfängermodul (dies wird derzeit nicht von der Software unterstützt, ist jedoch nicht sehr schwierig zu implementieren). Die 4 Anschlüsse sind T (Senden), R (Empfangen), + und - (3,3 V Spannung vom Heltec-Modul). |
| 12 | Reset-Taste | Durch ein kleines Loch erreicht man die Reset-Taste des Heltec-Moduls (selten benötigt).
| 13 | USB | Verwende ein normales 5-V-USB-Ladegerät, um das Gerät mit Strom zu versorgen und den LiPo-Akku aufzuladen. Die Mikrocontroller-Firmware kann auch über USB neu programmiert werden (du kannst die Morserino-32-Firmware aber auch über eine WLAN-Verbindung aktualisieren). |
| 14 | PRG-Taste | Durch ein kleines Loch erreicht man die Programmiertaste des Heltec-Moduls (normalerweise nicht erforderlich). |

<div style = "Seitenumbruch: immer;"> </ div>

## Ein- und Ausschalten / Laden des Akkus
Wenn das Gerät ausgeschaltet, aber die Batterie angeschlossen ist (der Schiebeschalter ist eingeschaltet), befindet es sich tatsächlich im Tiefschlaf: Fast alle Funktionen des Mikrocontrollers sind deaktiviert, und der Energieverbrauch ist minimal (weniger als 5% des normalen Betriebsstroms). 

Um das Gerät aus dem Tiefschlaf einzuschalten, drücke kurz die ROTE Taste (Power / Vol / Scroll). Es erscheint ein Startbildschirm für einige Sekunden. Der einzig interessante Punkt des Startbildschirms ist ganz unten: Man sieht einen Hinweis darauf, wie viel Akkukapazität noch vorhanden ist. Wenn diese dem Ende entgegen geht, sollte man das Gerät an eine USB-Stromquelle anschließen. (Der Akku wird auch leer, wenn man das Gerät im Tiefschlaf belässt:  nach einigen Tagen wird der Akku leer.) Wenn du also nicht vorhast, den Morserino in den nächsten Tagen wieder zu verwenden, trenne besser mit dem Schiebeschalter auf der Rückseite die Batterie vom Gerät...)

Wenn die Batteriespannung gefährlich niedrig ist, wird beim Einschalten ein leeres Batteriesymbol auf dem Bildschirm angezeigt, und das Gerät lässt sich nicht starten. Wenn dieses Symbol gezeigt wird, sollte man so bald wie möglich mit dem Laden des Akkus beginnen.

Um das Gerät auszuschalten (d.h. in diesem Fall in den Tiefschlaf zu versetzen), gibt es zwei Möglichkeiten:

* Wähle im Hauptmenü die Option "Go to Sleep".
* Tue nichts. Nach 5 Minuten Bildschirminaktivität (7,5 Minuten im Transceiver-Modus) schaltet sich das Gerät aus und geht in den Tiefschlaf.

Um den Akku aufzuladen, verbinde ihn mit einem USB-Kabel mit einer zuverlässigen USB-5-V-Stromquelle wie z.B. deinem Computer oder einem USB-Ladegerät (z.B. eines Mobiltelefons). Vergewissere dich, dass der Schiebeschalter des Geräts **eingeschaltet ist**. Wenn Ser Akku vom gerät getrennt ist, kann er nicht geladen werden. Während des Ladevorgangs leuchtet die orangefarbene LED am ESP32-Modul hell. Wenn der Akku nicht angeschlossen ist, leuchtet diese LED nicht hell, sondern blinkt nervös oder leuchtet gedimmt.

Wenn der Akku vollständig aufgeladen ist, leuchtet die orangefarbene LED nicht mehr.

Man das Gerät natürlich immer verwenden, wenn es über USB mit Strom versorgt wird, ob der Akku nun gerade aufgeladen wird oder nicht.


## Verwenden des Drehgebers und der ROTEN Power / Vol / Scroll-Taste
Die Auswahl der verschiedenen Modi und das Einstellen aller Arten von Parametern erfolgt mit dem Drehknopf bzw. dem **SCHWARZEN Knopf**. *Drehen* des Drehgebers führt durch die Optionen oder Werte, *das Klicken* des Knopfs wählt eine Option oder einen Wert aus oder führt zur nächsten Ebene des Menüs (es gibt bis zu drei Ebenen im Menü) und a *langes Drücken* beendet den aktuellen Status und führt eine Ebene nach oben bzw. zurück.

Ein ***Doppelklick*** des SCHWARZEN Knopfes ruft das Einstellungsmenü für die Parameter auf. Wird das vom Startmenü (bzw. einem Untermenü) aus gemacht, können alle Parameter geändert werden. Ist man gerade in einem aktiven Modus, werden nur die Parameter angezeigt, die für den aktuellen Modus relevant sind.

***Langes Drücken*** führt aus einem beliebigen Modus zurück zum Menü, bzw. innerhalb des Menüs eine Ebene nach oben.

Mit der zusätzlichen Taste **ROT (Power / Vol / Scroll)** kann man schnell zwischen **Geschwindigkeitsregelung** und **Lautstärkeregelung** mit einem einzigen kurzen ***Klick*** umschalten.

Ein ***langes Drücken*** wechselt die Anzeige und den Encoder in den **Scroll-Modus** (die Anzeige verfügt über einen Puffer von 15 Zeilen und normalerweise sind nur die unteren drei Zeilen zu sehen; im Scroll-Modus kann man zurück blättern und so die vorherigen Zeilen ansehen: Befindet man sich im Scroll-Modus  wird ganz rechts im Display eine **Scroll-Leiste** angezeigt, die ungefähr angibt, wo man sich innerhalb der 15 Zeilen des Textpuffers befindet. Durch erneutes* Klicken* im Scroll-Modusverlässt man diesen wieder, und der Drehgeber dient wieder zur Geschwindigkeitssteuerung.

Während man sich im Menü befindet (z.B. unmittelbar nach dem Einschalten), verfügt die **ROTE Taste** über einige zusätzliche Funktionen (weitere Informationen dazu im Abschnitt **Startmenü**):

**Langes Drücken** der ROTEN Taste startet eine Funktion zur Einstellung des Audio-Eingangspegels (und möglicherweise des Ausgangspegels eines Geräts, das Sie an den Line-Out-Anschluss des Morserino-32 angeschlossen haben).

**Ein dreimaliges Klicken** der ROTEN Taste öffnet das Menü für die **WLAN** Funktionen. WLAN ist nicht immer aktiviert, da es nur für zwei spezifische Zwecke verwendet wird:

* Zum **Hochladen** einer Textdatei für den Morsecode-Player

* Zum **Update** der Firmware des Morserino-32 auf die neueste und beste Version. Dies funktioniert ähnlich wie das Hochladen der Textdatei, aber man wählt stattdessen eine kompilierte Binärdatei, die die Firmware enthält (die Firmware erh#ält man auf dem Morserino-32 Repository auf GitHub).

* Mit einer dritten Funktion kannst du dem Morserino die SSID und das Passwort deines WLAN-Netzwerks mitteilen (dies muss passieren, damit du den Morserino-32 mit dem WLAN-Netzwerk verbinden kannst).


Alle WLAN-Funktionen enden mit einem Neustart des Morserino-32, nachdem diese Funktionen ausgeführt wurden.

##Das Display

Die Anzeige ist in zwei Hauptabschnitte unterteilt: Oben ist die Statuszeile, die wichtige Informationen zum aktuellen Status des Geräts enthält, und darunter ein **Bereich mit drei Zeilen**, in dem die generierten Morsecode-Zeichen im Klartext angezeigt werden. Alle Morsezeichen werden zur besseren Lesbarkeit in Kleinbuchstaben dargestellt. Betriebsakürzungen ("pro signs") werden als Buchstaben in spitzen Klammern angezeigt, z. B. `<ka>` oder `<sk>`. Im Echo Trainer-Modus (siehe unten) wird das Ergebnis deines Versuchs, den richtigen Morse-Code einzugeben, als "ERR" oder "OK" (zusammen mit akustischen Signalen) angezeigt.

Obwohl nur drei Zeilen Lauftext angezeigt werden, gibt es intern einen Puffer von 15 Zeilen - nach langem Drücken der ROTEN (Vol / Scroll) Taste kann man den Drehgeber verwenden, um zurückzublättern und die vorherigen Zeilen wieder sichtbar zu machen. Dies funktioniert auch, während du dich in einem der Modi befindest und Zeichen auf dem Bildschirm ausgegeben werden. Nichts geht verloren und die Anzeige kehrt zu ihrem normalen Verhalten zurück, wenn du den Scroll-Modus verlässt.

#### Die Statuszeile ####

Wenn ein Menü präsentiert wird (entweder das Startmenü oder ein Menü zur Auswahl der Voreinstellungen), wird in der Statuszeile entweder **Select Modus** (im Startmenü) oder **Set Preferences** (für die Einstellung von Parametern) angezeigt.

Im Keyer-Modus, im CW-Generator-Modus oder im Echo-Trainer-Modus zeigt die Statuszeile von links nach rechts Folgendes an:

* Ein **T** oder **X** bedeutet, dass das Gerät die internen **T**ouch Paddles oder E**X**terne Paddles verwendet (auswählbar durch Aufrufen des Voreinstellungsmenüs).
* **A**, **B** oder **U**, was den automatischen Keyer-Modus angibt: Iambic **A**, Iambic **B** oder **U**ltimatic (für Einzelheiten zu diesen Modi siehe unten).
* Die aktuell eingestellte Geschwindigkeit in Worten pro Minute. Im CW-Keyer-Modus als **nn** WpM, im CW-Generator- oder Echo-Trainer-Modus als (nn)**nn** WpM. Der Wert in Klammern zeigt die effektive Geschwindigkeit, die sich unterscheidet, wenn der Wortabstand oder der Zeichenabstand auf andere als die durch die Norm definierten Werte eingestellt wird (Länge von 3 Punkten für den Abstand zwischen Zeichen und 7 Punkte für den Abstand zwischen den Zeichen) -Wortabstand). Beachten Sie die nachstehenden Hinweise zu den Parametern, die du im CW-Generator-Modus einstellen kannst.

  Befindest du dich im Transceiver-Modus, gibt es auch zwei Werte für die Geschwindigkeit - der in Klammern ist die Geschwindigkeit des empfangenen Signals, der andere die Geschwindigkeit deines Keyers.

  Wenn die Ziffern, die die Geschwindigkeit angeben, mit **fetten** Ziffern angezeigt werden, wird durch Drehen des Drehgebers die Geschwindigkeit geändert. Werden sie mit normalen Zeichen dargestellt, ändert das Drehen des Drehgebers die Lautstärke.
  
* Ein horizontaler "Balken", der sich von links nach rechts erstreckt, zeigt die Lautstärke des vom Gerät erzeugten Tons an (die volle Länge des Balkens steht für die maximale Lautstärke). Normalerweise gibt es einen weißen Rahmen um den schwarzen Balken (sozusagen die normale Fortsetzung der Statuszeile); wenn dies umgekehrt ist (weißer Balken in schwarzer Umgebung - und die WpM-Ziffern sind nicht fett dargestellt), wird durch Drehen des Drehgebers die Lautstärke geändert.
* Ganz rechts in der Statuszeile befindet sich eine Anzeige (konzentrische Halbkreise), die die Funkübertragung symbolisiert, wenn der LoRA-Modus aktiv ist (wenn sich der Morserino-32 im LoRa-Transceiver-Modus befindet oder du den Parameter aktiviert hast, LoRa auch in einem der CW-Generatormodi zu übertragen).

## Startmenü

Das Startmenü bietet eine Auswahl der Modi, die man mit dem Morserino-32 verwenden kann. Es ist ein mehrstufiges Menü (bis zu drei Ebenen tief). Die oberste Ebene wird immer in der oberen Zeile angezeigt, die zweite Ebene in der zweiten Zeile und die dritte Ebene in der dritten Zeile unterhalb der Statuszeile. Die aktuelle Auswahl in der aktuellen Menüebene wird fett dargestellt. Wenn man sich in der obersten oder zweiten Ebene befinden und ein Untermenü eine Ebene tiefer verfügbar ist, sieht man zwei Punkte (..) in der Zeile unter dem aktuellen Eintrag.

Hier sind die verschiedenen Optionen, die man aus dem Startmenü auswählen kann (genauere Informationen gibt es weiter unten):

1. **CW Keyer**. Dies ist ein automatischer Keyer, der Iambic A, Iambic B und Ultimatic unterstützt.
2. **CW Generator**. Dies generiert entweder zufällige Zeichen und Wörter für CW-Trainingszwecke oder spielt den Inhalt einer Textdatei im Morse-Code ab. Man kann eine Reihe von Optionen durch Auswahl der entsprechenden Parameter festlegen (siehe Abschnitt Parameter weiter unten). Eine interessante Option zum Üben ist die Wortverdoppelung, bei der jedes Wort (oder eine Gruppe von Zeichen, alle Zeichen usw.) zweimal abgespielt wird. Auf Menüebene 2 kann man zwischen den folgenden Optionen wählen:

	* **Random**: Erzeugt Gruppen von zufälligen Zeichen. Die Länge der Gruppen sowie die Auswahl der Zeichen können in den Parametern durch Doppelklick auf den schwarzen Drehknopf ausgewählt werden (siehe Beschreibung der Parameter für Details).
	* **CW Abbrevs**: Aus den im CW betrieb üblichen Abkürzungen wird zufällig ausgewählt (durch eine Parametereinstellung kann man die maximale Länge der Abkürzungen wählen, die man trainieren möchte).
	* **English Words**: Zufällige Wörter aus einer Liste der 200 häufigsten Wörter in der englischen Sprache (wiederum kann man über einen Parameter eine maximale Länge festlegen).
	* **Call Signs**: Erzeugt zufällige Zeichenfolgen, die die Struktur und das Erscheinungsbild von Amateurfunkrufzeichen haben (dies sind keine echten Rufzeichen, und es werden einige erzeugt, die es in der realen Welt nicht gibt, da entweder das Präfix nicht in Gebrauch ist oder die Verwaltung eines Landes bestimmte Suffixe nicht verteilt). Die maximale Länge kann über einen Parameter ausgewählt werden.
	* **Mixed**: Wählt zufällig aus den vorherigen Möglichkeiten (zufällige Zeichengruppen, Abkürzungen, englische Wörter und Rufzeichen).
	* **File Player**: Spielt den Inhalt einer Datei im Morsecode ab, die auf den Morserino-32 hochgeladen wurde. Derzeit kann man nur eine einzige Datei hochladen. Wird eine neue Datei hochgeladen, wird die alte Datei überschrieben. Upload funktioniert über WLAN von Ihrem PC (oder Mac, Tablet oder Smartphone oder was auch immer). Dieser Modus merkt sich, wo man mit dem Abspielen aufgehört hat, und wird dort fortgesetzt, wenn man den File Player das nächste Mal startet. Sobald das Ende der Datei erreicht ist, beginnt das Abspielen wieder am Dateianfang.

	Du kannst den CW-Generator starten und stoppen, indem du schnell ein Paddel berührst oder den SCHWARZEN Knopf drückst.

3. **Echo Trainer**. Hier erzeugt der Morserino-32 ein Wort (das ist eine Reihe von Zeichen, mit den Möglichkeiten wie beim CW Generator) und wartet dann darauf, dass du diese Zeichen mit dem Paddel wiederholst. Wenn du zu lange wartest oder die Antwort nicht mit den vorgespielten Zeichen übereinstimmt, wird ein Fehler angezeigt (auf dem Display und akustisch), und das Wort wird wiederholt. Wenn du die richtigen Zeichen eingegeben hast, wird dies auch akustisch und auf dem Bildschirm angezeigt, und du wirst aufgefordert, das nächste Wort einzugeben.
    
    Die Untermenüs sind die gleichen wie beim CW-Generator: **Random, CW-Abbrevs, English Words, Call Signs, Mixed** und **File Player**.

	Wie beim CW-Generator können auch hier eine Vielzahl von Parametern zur Feinabstimmung der Generierung eingstellt werden.
     
  Man den Echo Trainer durch Berühren eines Paddels oder Drücken des SCHWARZEN Knopfes starten und durch Drücken des SCHWARZEN Knopfes wieder stoppen.
  
4. **Koch Trainer**. Dieses Menü enthält eine Reihe von Funktionalitäten, um Morsen nach der Koch-Methode zu erlernen, wobei man ein Zeichen nach dem anderen lernt und übt. Dies ist ein dreistufiges Menü, das wie folgt aufgebaut ist:

	* **Select Lesson**: Wähle eine Koch-Lektion zwischen 1 und 50 aus (Man lernt insgesamt 50 Zeichen nach der Koch-Methode). Die Nummer der Lektion und das dieser Lektion zugeordnete Zeichen werden im Menü angezeigt.
	* **Learn New Chr**: Wird diese Option gewählt, wird das neue Zeichen eingeführt. Man hört den Ton und sieht kurz die Reihenfolge der Punkte und Striche auf dem Bildschirm, sowie das Zeichen im Klartext. Dies wird wiederholt, bis man durch Drücken des SCHWARZEN Knopfes aufhört. Nach jedem Ereignis hat man die Möglichkeit, mit den Paddles zu wiederholen, was man gehört hat, und das Gerät zeigt dann an, ob dies richtig war oder nicht.
	* **CW Generator**: Hiermit werden zufällige Zeichen und Wörter für CW-Trainingszwecke generiert. Dabei werden nur die bisher gelernten Zeichen verwendet (durch die ausgewählte Koch-Lektion definiert). Auf der Menüebene 3 kann man zischen den folgenden Optionen wählen:
		* **Random**: Erzeugt Gruppen von zufälligen Zeichen. Die Länge der Gruppen kann in den Parametern durch Doppelklick auf den schwarzen Drehknopf ausgewählt werden (Details siehe Parameterbeschreibung).
		* **CW Abbrevs**: Zufällig gewählte Abkürzungen, wie sie im CW Betrieb häufig vorkommen (jedoch nur mit den bisher gelernten Buchstaben). Man kann damit ab Lektion 2 oder 3 beginnen, aber die Anzahl der ausgegebenen Abkürzungen wird natürlich sehr begrenzt sein; über eine Parametereinstellung kann man die maximale Länge der Abkürzungen auswählen, die man trainieren möchte.
		* **English Words**: Zufällige Wörter aus einer Liste der 200 gebräuchlichsten Wörter in der englischen Sprache (wiederum kann man über einen Parameter eine maximale Länge festlegen, und es werden natürlich nur Wörter angezeigt, die ausschließlich bereits gelernte Buchstaben enthalten).
		* **Mixed**: Wählt zufällig aus den vorherigen Möglichkeiten (zufällige Zeichengruppen, Abkürzungen und englische Wörter).
	* **Echo Trainer**: Hier generiert das Gerät ein Wort und wartet darauf, dass du diese Zeichen mit dem Paddel wiederholst. Wenn du zu lange wartest oder die Antwort nicht mit der Vorgabe übereinstimmt, wird ein Fehler angezeigt (auf dem Display und akustisch), und das Wort wird wiederholt. Wenn du die richtigen Zeichen eingegeben hast, wird dies auch akustisch und auf dem Bildschirm angezeigt, und es geht weiter mit dem nächsten Wort.

		Die Untermenüs sind dieselben wie für den CW-Generator im Koch-Modus: **Random, CW Abbrevs, English Words** und **Mixed**.
 
5. **Transceiver**. Hier gibt es zwei Einträge im entsprechenden Untermenü:

	* **LoRa Trx**: Dies ist ein Morsecode-Transceiver, der LoRa im 70-cm-ISM-Band verwendet. Zusätzlich zu der Funktionalität des CW-Keyers werden durch den LoRa-Transceiver alle Morsezeichen gesendet (wobei ein spezielles Datenformat verwendet wird, das die eingegebenen Punkte und Striche codiert, unabhängig davon, ob es sich um zulässige Morsezeichen handelt oder nicht), und es empfängt auf dem Band, wenn man gerade keine zeichen morst; damit kann man wirklich ein interaktives Gespräch in Morsezeichen zwischen zwei oder mehr Morserino-32-Geräten führen! Man möge beachten, dass die Zeichen nicht einzeln sodern Wort für Wort übertragen werden, daher kommt es auf der Empfangsseite zu einer kleinen Verzögerung - QSK ist daher nicht möglich. Am besten geht es, wenn man die Taste sauber an die Gegenstation übergibt, wenn man mti einem Durchgang fertig ist!
	* **iCW / Ext Trx**: In diesem (noch etwas experimentellen) Modus wird ein an den Morserino-32 angeschlossener Transceiver durch den Keyer getastet, oder man verwendet Line-Out-Audio entweder für einen FM-Transceiver oder für CW über das Internet (iCW - dies verwendet Mumble als Audio-Übertragungsprotokoll). Alle CW-Signale, die über den Audioeingang als Töne eingehen, werden dekodiert und auf dem Bildschirm angezeigt.

6. **CW Decoder**. In diesem Modus werden Morsezeichen decodiert und auf dem Bildschirm angezeigt. Der Morse-Code kann entweder über eine Handaste ("Straight Key") eingegeben werden, die an die Buchse angeschlossen wird, an der normalerweise ein externes Paddle abgeschlossen wird; man kann aber auch ein Paddle (intern oder extern) als Handtaste "missbrauchen". Auf diese Weise kann man seine Eingaben mit der Handtaste überprüfen und verbessern, indem man kontrolliert, ob auch das dekodiert wurde, was man eingeben wollte.

	Man kann auch einen Tonsignal (am Audioeingang) dekodieren lassen, das zum Beispiel von einem Kurzwellenempfänger stammt. Der Ton sollte bei 700 Hz liegen. Optional gibt es ein recht scharfes (in Software implementiertes) Filter, das nur Töne in einem sehr engen Bereich um 700 Hz erkennt und alle anderen ignoriert. Dies wird durch Auswahl des Parameters "Narrow" eingeschaltet (siehe den Abschnitt zu den Parametern von Morserino-32).


7. **Go to Sleep**. Dadurch wird das Gerät ausgeschaltet (oder genauer gesagt, es wird in einen "Deep Sleep" Modus versetzt, so dass es sehr wenig Strom verbraucht und der Akku einige Tage überleben sollte). Um das Gerät wieder "aufzuwecken" , drücket man einfach die Taste **ROT (Power / Vol / Scroll)**. Wenn man den Morserino-32 für mehrere Tage nicht verwenden möchte, sollte man den Akku mit dem Schiebeschalter auf der Rückseite vom gerät trennen, um eine Tiefentladung des LiPo-Akkus zu vermeiden.

Man kann auch noch einige **weitere Funktionen** erreichen, wenn man sich im Startmenü befindet - nicht durch eine Menüauswahl, sondern entweder durch langes Drücken der ROTEN Taste oder durch Doppelklick auf die ROTE Taste:

* **Langer Druck auf die ROTE Taste**: Hiermit wird eine Funktion zur Einstellung des Audio-Eingangspegels gestartet: Falls ein Tonsignal am Audio Eingang anliegt, zeigt ein Balkendiagramm die Spannung des Eingangssignals an. Stelle die Einstellung mit dem blauen Trimmer-Potentiometer so ein, dass sich das linke und rechte Ende des Balkens innerhalb der beiden äußeren Rechtecke befinden. Gleichzeitig wird am Line-Out ein Sinussignal ausgegeben, und der Transceiver-Ausgang wird verkürzt (Tasten eines Senders, falls er mit eienm solchen verbunden ist).
	
	Ein einfacher Test oder eine Demo für die Audioeingangsanpassung besteht darin, den Line-Out-Eingang mit dem Audioeingang zu verbinden und den Sinusausgang in den Audioeingang einzuspeisen. Wenn man das Potentiometer dreht, von einem Anschlag zum anderen, kann man sehen, wie sich das Balkendiagramm ändert, so dass auf der einen Seite nur ein winziger Balken in der Mitte verbleibt und die beiden Rechtecke an beiden Enden des Diagramms sichtbar werden (im Wesentlichen misst man so nur nur das Rauschen auf dem Operationsverstärker-Eingang), bzw. der Balken an beiden Enden über die Rechtecke hinaus ragt. Jetzt kann man das Potentiometer so einstellen, dass der durchgezogene Balken die äußeren Begrenzungen der Rechtecke fast berührt. Dies ist die optimale Einstellung für den Audio-In-Pegel. Offensichtlich muss man dies für die Audioquelle durchführen, die man verwenden möchte, z.B. für einen Funkempfänger.

* **Dreimaliges Klicken auf die ROTE Taste**: Das Menü für die WLAN-Funktionen wird angezeigt (nähere Beschreibung siehe unten). WLAN ist nicht immer aktiviert, da es nur für zwei spezifische Zwecke verwendet wird:

	* Zum **Hochladen** einer Textdatei für den Morse-Code-Player (derzeit kann nur eine Textdatei hochgeladen werden; beim Hochladen einer neuen Datei werden alle zuvor hochgeladenen Dateien überschrieben). In diesem Modus stellt der Morserino-32 eine Verbindung zu deinem lokalen WLAN-Netzwerk her und startet einen einfachen Webserver.
	* Zum **Update** der Firmware des Morserino-32 auf die neueste und beste Version. Dies geht ähnlich wie das Hochladen der Datei, aber man muss natürlich die kompilierte Binärdatei auswählen, die die Firmware enthält (diese gibt es über das Morserino-32-Repository auf GitHub).
	* **WLAN-Konfiguration**: Mit einer dritten Funktion kannst du dem Morserino die SSID und das Passwort deines WLAN-Netzwerks mitteilen (sonst kann keine Verbindung zu deinem Netzwerk hergestellt werden). Dazu startet der Morserino-32 als Access Point und stellt sein eigenes Netzwerk (mit SSID "morserino") zur Verfügung. Du musst deinen Computer mit diesem Netzwerk verbinden (es ist kein Passwort erforderlich) und mit einen Browser zu *m32.local* gehen. Es erscheint ein einfaches Formular, in dem du die SSID und das Passwort für dein WLAN eingeben kannst.

Alle WLAN-Funktionen enden mit einem Neustart des Morserino32, nachdem diese Funktionen ausgeführt wurden.

### Verwenden der Modi "CW Keyer", "CW Generator" und "Echo Trainer"
Wenn man einen dieser Modi verwendent enthält die *oberste Zeile* (Statuszeile) folgende Informationen (von links nach rechts):

- **X** oder **T**: Das derzeit verwendete Paddel, entweder ein e**X**ternes (mechanisches) Paddel oder das eingebaute **T**ouch Paddel.

- **A** oder **B** oder **U**: Der automatische Keyer-Modus, entweder Iambic A (Curtis A Modus), Iambic B (Curtis B Modus) oder Ultimatic-Modus.

- Die **Geschwindigkeit** in Wpm (Wörter pro Minute).

- Die **Lautstärke**, angezeigt durch eine grafischen Balken.

- Wenn du die Option **transmit with LoRa** gesetzt hast (siehe Abschnitt Parameter), wird am rechten Ende der Statuszeile ein kleines Symbol für Funkwellen angezeigt.


Mit einem einzigen Klick mit der (SCHWARZEN) Encoder-Taste wird der Modus des Encoders zwischen der Einstellung der Keyer-Geschwindigkeit und der Lautstärke umgeschaltet. Die Keyer-Geschwindigkeit wird in Wörtern pro Minute angegeben (das Referenzwort ist das Wort PARIS), was auch bedeutet, dass 1 wpm 5 Zeichen pro Minute entspricht.

Andere Parameter können durch **Doppelklick** der Encoder-Taste eingestellt werden, wodurch man in das Menü für die Parameter kommt (siehe Abschnitt Parameter).

Einige Besonderheiten für jeden dieser Modi:

- Im **CW Keyer** Modus verwendet man entweder die integrierten kapazitiven Touch-Paddles oder ein externes Paddle, um Morsezeichen zu generieren.
- Im **CW Generator** Modus generiert das Gerät zufällige Gruppen von Morsezeichen, die du mitlesen kannst. Wenn du diesen Modus ausgewählt hast, kannst du diedas Generieren von Morsezeichen **starten** und **stoppen**, indem du **das Paddel drückst** (Entwese nur eine Seite oder beide).

	Beim ersten Start wird man zuerst durch die Generierung von  "`vvv<ka>`" (`..._   ..._   ..._    _._._`) in Morse-Code vorgewarnt, bevor tatsächlich die zufälligen Gruppen erzeugt werden.
- Im Modus **Echo Trainer** generiert das Gerät eine Gruppe von Zeichen (also ein Wort) und wartet darauf, dass du dasselbe Wort wiederholst. Wenn du zu lange wartest oder die Antwort nicht korrekt ist, wird das Wort wiederholt, bis du es richtig wiederholt hast.

	In diesem Modus wird auf dem Display nicht angezeigt, wie das erzeugte Aufforderungswort aussieht. Es wird nur deine Antwort angezeigt.

	Wie im CW-Trainer-Modus startet man die Generierung durch Drücken des Paddels. Anschließend wird die Sequenz "`vvv<ka>`" als Warnung generiert, bevor das Echo-Training beginnt. Man kann diesen Modus nicht durch Drücken des Paddels stoppen oder unterbrechen - schließlich generierst du ja deine Antworten mit dem Paddel! Die einzige Möglichkeit, diesen Modus zu stoppen, ist durch Drücken der schwarzen Encoder-Taste.
	
	Für die Echo-Trainer-Modi gibt es einen speziellen Parameter, der dir helfen soll, mit maximaler Geschwindigkeit zu trainieren, genannt **Adaptv. Speed** (adaptive Geschwindigkeit; siehe Abschnitt Parameter). Wenn deine Antwort korrekt war, wird die Geschwindigkeit um 1 Wort pro Minute erhöht. Hast du einen Fehler gemacht, sinkt der Wert um 1. Dadurch trainierst du letztlich immer an deinem Limit, was sicherlich die beste Möglichkeit ist, deine Grenzen zu verschieben.
	
### Koch-Methode

Der deutsche Psychologe Koch entwickelte eine Methode zum Erlernen des Morsens (in den 1930er Jahren), bei der in jeder Lektion ein zusätzliches Zeichen hinzugefügt wird. Die Reihenfolge ist weder alphabetisch noch nach der Länge der Morsezeichen sortiert, sondern folgt einem bestimmten rhythmischen Muster, so dass die einzelnen Zeichen als Rhythmus gelernt werden und nicht als Folge von Dits und Dahs.

Um zu verhindern, dass Punkte und Hintergründe gezählt oder das Gehörte nachgedacht und rekonstruiert wird, sollte die Geschwindigkeit ausreichend hoch sein (min. 18 WpM). Pausen zwischen Zeichen und Wörtern sollten nicht enorm verlängert werden (und es ist immer besser, nur die Wortabständezu verlängern, die Abstände zwischen den Zeichen mehr oder weniger bei der Norm zu belassen). Mit unserem Gerät kannst du den Abstand zwischen den Wörtern unabhängig vom Abstand zwischen den Zeichen einstellen, um eine Einstellung zu finden, die perfekt zu deinen Bedürfnissen passt.

Die Reihenfolge der gelernten Zeichen wurde von Koch nicht genau festgelegt. Daher verwenden unterschiedliche Lernkurse etwas unterschiedliche Reihenfolgen. Hier verwenden wir dieselbe Reihenfolge der Zeichen wie im Softwarepaket "SuperMorse" (siehe http://www.qsl.net/kb5wck/super.html), das auch von der "CW Schule Graz" verwendet wird. Die Reihenfolge lautet wie folgt:

* Lektion 1: m
* Lektion 2: k
* Lektion 3: r
* Lektion 4: s
* Lektion 5: u
* Lektion 6: a
* Lektion 7: p
* Lektion 8: t
* Lektion 9: l
* Lektion 10: o
* Lektion 11: w
* Lektion 12: i
* Lektion 13: . (Punkt)
* Lektion 14: n
* Lektion 15: j
* Lektion 16: e
* Lektion 17: f
* Lektion 18: 0 (Null)
* Lektion 19: y
* Lektion 20: v
* Lektion 21: , (Komma)
* Lektion 22: g
* Lektion 23: 5
* Lektion 24: /
* Lektion 25: q
* Lektion 26: 9
* Lektion 27: z
* Lektion 28: h
* Lektion 29: 3
* Lektion 30: 8
* Lektion 31: b
* Lektion 32: ?
* Lektion 33: 4
* Lektion 34: 2
* Lektion 35: 7
* Lektion 36: c
* Lektion 37: 1
* Lektion 38: d
* Lektion 39: 6
* Lektion 40: x
* Lektion 41: - (Minus)
* Lektion 42: =
* Lektion 43: SK (Pro Sign)
* Lektion 44: KA (Pro Sign)
* Lektion 45: AR (Pro-Zeichen, entspricht +)
* Lektion 46: AS (Pro Sign)
* Lektion 47: KN (Pro Sign)
* Lektion 48: VE (Pro Sign)
* Lektion 49: @
* Lektion 50: : (Doppelpunkt)

Wenn du die Koch-Methode zum Erlernen des Morsens verwenden möchtest, findest du alles dazu Nötige im Menüpunkt **Koch Trainer**. Es gibt ein Untermenü, in dem man die Lektion einstellen kann, eines, um nur diesen einen neuen Buchstaben zu üben (mit dem Echo-Trainer-Modus, damit man das wiederholen kann, was man hört), und die Modi "CW Generator" und "Echo Trainer", jeder der letzten beiden enthält die Untermenüs für "Random" (Gruppen von zufälligen Zeichen aus den bisher gefundenen Zeichen),"CW Abbrevs" (die in CW-QSOs üblicherweise verwendeten Abkürzungen), "English Words" (die häufigsten Englische Wörter) und "Mixed" (zufällige Gruppen, Abkürzungen und Wörter zufällig ausgewählt). Natürlich werden nur die bereits gelernten Zeichen verwendet - was bedeutet, dass man, während man noch mit den ersten Zeichen kämpft, nur eine begrenzte Anzahl von Abkürzungen und Wörtern erhalten wird.
### Verwenden des Modus "LoRa Trx"
Grundsätzlich verwendet dieser die gleiche Statuszeile wie der CW-Keyer. Sobald man jedoch etwas via LoRa empfängt, zeigt die Statuszeile zusätzlich zur eigenen Geschwindigkeit auch die Geschwindigkeit der sendenden Station an - man sieht zum Beispiel **18r20sWpM**, was bedeutet, dass eine Station mit einer Geschwindigkeit von 18 Wpm empfangen wird, und man selber mit 20 WpM gibt.
Außerdem ändert der Balken rechts in der Statuszeile seine Funktion: Anstatt den aktuellen Lautstärkepegel anzuzeigen, zeigt er die Signalstärke des empfangenen Signals an - eine grobe Form eines S-Meters, sozusagen.
Der volle Balken zeigt einen RSSI-Pegel von etwa -20 dB an, und der Balken beginnt bei einem Pegel von etwa -150 dB.

Wenn man die ROTE Pwr / Vol / Scroll-Taste drückt, können man dennoch wie gewohnt  die Lautstärke einstellen.

Vom Transceiver empfangene Morsezeichen
werden im (scrollbaren) Textbereich der Anzeige fett dargestellt, während alles, was man selber sendet, in normalen Zeichen dargestellt wird.

Eine weitere Besonderheit ist hier erwähnenswert: Die Frequenz des Tons, den du hörst, wenn du einen anderen Sender empfängst, wird wie in den anderen Modi über den Parameter "Pitch" eingestellt. Die Tonhöhe beim Senden kann gleich oder ein Halbton höher oder niedriger sein als beim Empfang - dies wird wie im Echo Trainer-Modus mit dem Tone Shift-Parameter eingestellt.

Eine andere Sache, die dich vielleicht interessiert: Der LoRa CW-Transceiver funktioniert nicht wie ein CW-Transceiver auf Kurzwelle, bei dem ein unmodulierter Träger getastet wird, und die Verzögerung zwischen Sender und Empfänger nur durch die Verzögerung des elektromagnetischen Pfads bestimmt wird.  LoRa verwendet eine Spread-Spectrum-Technologie zum Senden von Datenpaketen - auf eine ähnliche Weise wie WLAN. Daher muss alles, was als Morsezeichen eingegeben wird - in erster Linie die Geschwindigkeit und alle Punkte, Striche und Pausen zwischen den Zeichen, zuerst in ein passendes Datenformat übertragen werden. Sobald die Pause lang genug ist, um als Pause zwischen Wörtern (sozusagen als Leerzeichen) erkannt zu werden, wird das gesamte bisher zusammengestellte Datenpaket übertragen und schließlich mit der angegebenen Geschwindigkeit von der vom empfangenden Morserino-32 abgespielt.

Bei der Umsetzung in ein Datenpaket werden tatsächlich Punkte, Striche und Pausen kodiert, und nicht etwa der Klartext als ASCII Zeichen übertragen; damit ist es möglich, auch "falsche" Morsezeichen zu senden, oder Morsezeichen, die nur in gewissen Sprachen übich sind. Sie weden korrekt übertragen (allerdings am Display als nicht-dekodierbar dargestellt).


Das Senden Wort für Wort bedeutet, dass zwischen Sender und Empfänger eine nicht unerhebliche Verzögerung auftritt und die Verzögerung in hohem Maße von der Länge der gesendeten Wörter und der verwendeten Geschwindigkeit abhängt. Da die meisten Wörter in einer typischen CW-Konversation eher kurz sind (7 Zeichen oder mehr stellen bereits ein sehr langes Wort dar), muss man sich keine Sorgen machen (es sei denn, beide sitzen im selben Raum ohne Kopfhörer - dann wird es wirklich verwirrend). Aber sende doch einmal ein sehr langes Wort, sagen wir mindestens 10 Zeichen lang, und zwar mit sehr niedriger Geschwindigkeit (5 WpM). Dann wird klar weren, wovon wir hier reden!

### Verwendung des Modus "Morse Decoder"
Die Statuszeile unterscheidet sich geringfügig von den anderen Modi. Zunächst befindet sich der Drehgeber immer im Volumeneinstellmodus - die Geschwindigkeit wird aus dem dekodierten Morsecode bestimmt und kann nicht manuell eingestellt werden. Durch kurzes oder langes Drücken des Encoder Buttons gelangt man zurück zum Startmenü.

Auf der linken Seite der Statusanzeige wird oben ein schwarzes Rechteck angezeigt, wenn die Taste gedrückt wird (oder ein 700 Hz-Ton erkannt wird). Dadurch werden die Anzeigen für externe Paddles und für den Keyer-Modus ersetzt.

Die vom Decoder erkannte aktuelle Geschwindigkeit wird in der Statuszeile als WpM angezeigt. Wird der Decoder Modus verlassen, wird diese Geschwindigkeitseinstellung beibehalten. Wenn man dann auf den CW-Keyer umschaltent wird der zuletzt eingestellte Geschwindigkeitswert des Decoders benutzt. Ich bin mir nicht sicher, ob dies ein Fehler oder ein Feature ist ;-)

Dieser Modus hat nur wenige Paramater (sie den nächsten Abschnitt); der vielleicht wichtigste davon ist die Möglichkeit, die Filterbandbreite für die Tonerkennung zwischen schmal (ca 150 Hz) und breit (ca 600 Herz) umzuschalten.

## Parameter

Man erreicht die Einstellung der Parameter immer durch **Doppelklick** des **SCHWARZEN-Drehgeberknopfes**. Dadurch gelangt man in ein Menü mit Parametern (Ein "**>**" - Zeichen steht vor dem aktuellen Parameter und die Zeile darunter zeigt den aktuellen Wert). Durch Drehen des Encoders bewegt man sich durch die verfügbaren Parameter. Wenn man das Parametereinstellungsmenü verlassen möchte, drückt man die Encoder-Taste etwas länger, und man ist wieder im selben Zustand wie zu dem zeitpunkt, zu dem man das Parametereinstellungsmenü aufgerufen hat .

Wenn man den Parameter erreicht hat, den man ändern möchte, klickt man einmal. Das Zeichen "**>**" befindet sich nun in der unteren Zeile vor dem Parameterwert. Dies zeigt an, dass nun das Drehen des Drehgebers diesen Wert ändert. Ist man mit dem Wert zufrieden, klickt man **einmal**, um zur Auswahl der Parameter zurückzukehren, oder man drückt den Knopf etwas länger, um das Parametermenü komplett zu verlassen.

Die einstellbaren Parameter richten sich nach dem Modus, in dem man sich beim Doppelklick befunden hat: Wenn man in einem bestimmten Modus doppelklickt, werden nur die Parameter angezeigt, die für diesen Modus relevant sind. Ein Doppelklick im Startmenü führt dazu, dass alle Parameter zur Auswahl stehen.


#### Liste aller Morserino-32-Parameter ####
Fettgedruckte Werte sind Standard- oder empfohlene Werte. Bei Aufruf über das Startmenü stehen alle Parameter zur Änderung zur Verfügung. Weiter unten gibt es eine Übersicht, welche Parameter in den verschiedenen Modi verfügbar sind.


| Parameter Name | Beschreibung                       | Werte |
| -------------- | --------------------------------- | ------ |
| Encoder Click | Legt fest, ob beim Drehen des Drehgebers bei jedem Schritt ein kurzer Click zu hören sein soll oder nichtT   | Off / On |
| Tone Pitch Hz   | Die Frequenz des Mithörtons, in Hz | Reihe von Tönen zwiscehnbetween 233 und 932 Hz, entsprechend der musikalischen Noten der B Dur Tonleiter von b bis b'' (2 Oktaven) |
| Paddle        | Auswahl zwischen dem Touch Paddle oder einem externen Paddel | Touch Paddle / Ext. Paddle |
| Paddle Polarity | Legt fest, auf welcher Seite die dits und auf welcher die dahs sind | ` _. dah-dit` / **`._ di-dah`**  |
| Keyer Mode     | Bestimmt den  Iambic Mode (A oder B), bzw. Ultimatic Modus; mehr dazu weiter unten  | Curtis A / Curtis B / Ultimatic |
| CurtisB DahT% | Timing für den Curtis B Modus für dahs; mehr dazu weiter unten     | 0 -- 100 %, in Schritten von 5 % [**35 - 55**] |
| CurtisB DitT% | Timing für den Curtis B Modus für dits; mehr dazu weiter unten    | 0 -- 100 %, in Schritten von 5 % [**55 - 100**] |
| AutoChar Spce   | Minimaler Abstand zwischen zwei zeichen  | Off / min. 2 / **3** / 4 dots |
| Tone Shift | Im Echo Trainer Modus, und im Transceiver Modus kann der Ton zwischen Geben und Hören gleich sein, oder leicht unterschiedlich (Halbton höher oder tiefer). |**No Tone Shift** / Up 1/2 Tone / Down 1/2 Tone |
| Interword Spc | Die Zeit (in Anzahl von dits) die zwischen Wörter eingefügt wird (mehr dazu weiter unten)     | 6 -- 45 [**7**] siehe unten|
| Interchar Spc | Die Zeit (in Anzahl von dits) die zwischen Zeichen eingefügt wird (mehr dazu weiter unten) | 3 -- 15 [**3**]|
| Random Groups | Auswahl von zeichengruppen, die beim generieren von **Random** Gruppen benutzt werden sollen | Alpha / Numerals / Interpunct. / Pro Signs / Alpha + Num / Num+Interp. / Interp+ProSn / Alpha+Num+Int / Num+Int+ProS / All Chars |
| Length Rnd Gr | Länge der zufälligen Zeichengruppen; traditionellerweise oft 5. | Fixe Längen 1 -- 6, bzw. 2 bis 3 -- 2 bis 6 (Die Länge wird dann zufällig innerhalb dieser Limits determiniert) [**5**] |
| Length Calls | Maximale Länge der generierten Rufzeichen | Unlimited / max. 3 -- max. 6 |
| Length Abbrev | Maximale Länge der zufällig erzeugten CW Abkürzungen und Q Gruppen | Unlimited / max. 2 -- max. 6 |
| Length Words | Maximale Länge der zufällig erzeugten häufigen englischen Wörter | Unlimited / max. 2 -- max. 6 |
| Trainer Disp | Wie der CW Generator die ausgegebene zeichen am Display darstellen soll | Display off / **Char by Char** / Word by word |
| Each Word 2x | Der CW generator kann hiermit auf "Wortverdoppelung" geschaltet werden, zur Unterstützung des Gehörlesen Lernens | **Off** / On |
|Echo Repeats    | Angabe, wie oft  im Echo Trainer Modu ein Wort maximal wiederholt werden soll, bevor nach fehlern oder versäumter Eingabe ein neues Wort gewählt wird. Ist der Wert 0, wird das nächste Wort immer ein neues sein, egal ob die Antwort richtig oder falsch war.              | 0 -- 6 / Forever |
| Confrm. Tone  | Legt fest, ob im Echo Trainer Modus eine akustische Rückmeldung erfolgen soll. Die visuelle Anzeige "OK" oder "ERR" wird in jedem Fall angezeigt. | **On** / Off |
|Key ext TX        | Legt fest, unter welchen Umständen ein extern angeschlossener Sender getastet werden soll. | Never / CW Keyer only / Keyer&Trainer |
| Send via LoRa | Ist hier "ON gewählt, wird alles, was der CW generator erzeugt, auch via LoRa gesendet - man kann so zB mit einem gerät etwas erzeugen, und mehrere andere geräte empfangen alle dasselbe (diese müssen dann im LoR Trx Modus betrieben werden). achtung! Bei loRa betrieb immer darauf achten, dass eine Antenne angeschlossen ist! Bei Sendebetrieb ohne antenne kann das loRa Modul Schaden erleiden!B| LoRa Tx ON / **LoRa Tx OFF** |
| Bandwidth |Bandbreite des Filters für den  CW Decoder (implementiert in Software mit einem sogenannten  Goertzel filter).  (Wide = ca. 600 Hz, Narrow = ca. 150 Hz; Mittenfrequenz = ca 700 Hz) | **Wide** / Narrow |
| Adaptv. Speed | Ist dieser Wert auf "ON", wird im Echo Trainer Modus die Geschwindigekit nach jeder richtigen Antwort erhöht, und nach jeder falschen vermindert, jeweils um 1 WpMI. | ON / **OFF** |



#### Parameter im ***CW Keyer*** Modus



 Parameters |  | | 
 -----      |---  |--- |---
| Encoder Click | Tone Pitch Hz   |  Paddle        |  Paddle Polarity | 
| Keyer Mode     |  CurtisB DahT% | CurtisB DitT% |  AutoChar Spce   |


Hinweis:

**Iambic-Modi**: Wenn Sie beide Paddles eines Iambic-Keyers drücken, werden abwechselnd Dahs und Dits erzeugt, während beide Paddles gedrückt werden, beginnend mit dem zuerst berührten. Der Unterschied zwischen den Modi A und B ist das Verhalten, wenn beide Paddles losgelassen werden, während das aktuelle Element erzeugt wird: In Modus A stoppt der Keyer nach dem aktuellen Element, in Modus B fügt der Keyer ein anderes Element hinzu, und zwar das jeweils entgegen gesetzte.

Mit anderen Worten, im Curtis B-Modus wird das Paddel der anderen Seite überprüft, während das aktuelle Element (dit oder dah) ausgegeben wird. Wenn ein Paddel während dieser Zeit gedrückt wird, wird ein weiteres gegenteiliges Element zum aktuellen Element hinzugefügt. In Modus A ist dies nicht der Fall. Da Modus B etwas kompliziert zu bedienen ist, wurde dies später geändert, sodass die Paddles erst nach einem bestimmten Prozentsatz der Dauer des Elements überprüft werden. Dies ist der Prozentsatz, den man hier mit den Parametern "**CurtisB DahT%**" und "**CurtisB DitT%**" einstellen kann.

Wenn man den Wert auf 0 setzt, ist der Modus identisch mit dem ursprünglichen Curtis B-Modus. Der später entwickelte "verbesserte" Curtis B-Modus verwendet einen Prozentsatz von etwa 35% bis 40%. Wenn man den Prozentsatz auf 100 (den höchsten Wert) einstellt, ist das Verhalten dasselbe wie im Modus Curtis A.

Mit diesem Parameter kann man jedes Verhalten zwischen den Modi Curtis A und dem ursprünglichen Curtis B auf einer kontinuierlichen Skala einstellen. Man kann die Prozentsätze für Dits und Dahs separat einstellen (dies ist sinnvoll, da das Timing für Dits nur ein Drittel des Werts für Dahs beträgt und da möchte man vielleicht einen höheren Prozentsatz).

**Ultimatic-Modu **: Wenn man im Ultimatic-Modus beide Paddel gedrückt hält, wird zuerst ein Dit oder Dah erzeugt, je nachdem, welches Paddel man zuerst gedrückt hat, und danach wird das gegenteilige Element kontinuierlich erzeugt. Dies ist von Vorteil für Zeichen wie j, b, 1, 2, 6, 7.

**NB**: Die Modi Iambic und Ultimatic können nur mit dem eingebauten Touch-Paddle oder einem externen Doppelhebel-Paddles verwendet werden. Die Auswahl dieser Modi ist bei Verwendung eines externen Einhebel-Paddles nicht relevant.



#### Parameter im ***CW Generator*** Modus


| Parameters | | | |
| ---| ---|---|---|
| Encoder Click | Tone Pitch Hz   | Paddle        | Interword Spc |
| Interchar Spc | Random Groups | Length Rnd Gr | Length Calls | 
| Length Abbrev |Length Words | Trainer Disp | Each Word 2x | 
|Key ext TX        | Send via LoRa | 

Anmerkungen:

* *Intercharacter Space*. Das bedeutet, wie viel Platz zwischen Zeichen eingefügt wird. Die "Norm" ist ein Abstand, der die Länge von drei Punkten hat. Um das Kopieren von Code, der mit hoher Geschwindigkeit gesendet wird, zu vereinfachen und um Morsen zu lernen, kann dieser Abstand vergrößert werden. Der Code sollte mit ziemlich hohen Geschwindigkeiten (> 18 Wpm) gesendet werden, um das "Zählen" von Dits und Dahs unmöglich zu machen, so dass man eher den "Rhythmus" jedes Charakters lernt. Im Allgemeinen ist es besser, den Abstand zwischen Wörtern und nicht so sehr den Abstand zwischen den Zeichen zu vergrößern. Daher wird empfohlen, diesen Wert zwischen 3 und max 6 einzustellen. Siehe unten.

* *Interword Space*. Der Abstand zwischen Wörtern, normalerweise wird dies als Länge von 7 Punkten definiert. Im CW-Keyer-Modus bestimmen wir ein neues Wort nach einer Pause von 6 Dits, um zu vermeiden, dass der Text auf dem Display als lange Wurst ohne Spatien verläuft. Im CW-Generator-Modus kann man den Interword-Abstand auf Werte zwischen 6 und 45 einstellen (mehr als das 6-fache des normalen Abstandes, um das Kopieren von Code im Kopf bei hohen Geschwindigkeiten zu erleichtern). In Analogie zum Abstand zwischen Farnsworth wird dies auch als Wordsworth-Abstand bezeichnet. Dies ist ein noch besserer Weg, um das Kopieren im Kopf von High-Speed-CWzu üben. Natürlich kann man Interword- und Intercharacter-Abstände auch kombinieren.

Da der Zeichenabstand unabhängig voneinander eingestellt werden kann, könnte man theoretisch den Zeichenabstand höher als den Abstand zwischen Wörtern festlegen, was ziemlich verwirrend wäre. Um diese Verwirrung zu vermeiden, ist der Abstand zwischen Wörtern immer um mindestens 4 Bit länger als der Zeichenabstand, auch wenn ein kleinerer Abstand zwischen Wörtern festgelegt wurde.

Die ARRL und einige Morse-Trainingsprogramme verwenden etwas, das sie *"Farnsworth Spacing"* (Farnsworth-Abstand) nennen: Dabei werden die Abstände zwischen Zeichen und zwischen Wörtern proportional um einen bestimmten Faktor verlängert. Man kann den Farnsworth-Abstand emulieren, indem man sowohl den Abstand zwischen den Zeichen als auch den zwischen den Wörtern erhöht, indem man z.B den Zeichenzwischenraum auf 6 und den Wortzwischenraum auf 14 setzt, werden alle abstände effeltiv verdoppelt. Wenn man dies bei einer Zeichengeschwindigkeit von 20 WpM macht, ergibt sich eine effektive Geschwindigkeit von 14 WpM. Dies wird in der Statuszeile als (14)**20** WpM angezeigt.


<div style="page-break-after: always;"></div>

#### Parameter im **CW Generator - File Player** Modus

| Parameter | | | |
| -------------- | ---|---|---|
| Encoder Click | Tone Pitch Hz   |Paddle        | Interword Spc | 
| Interchar Spc | Trainer Disp | Each Word 2x | Key ext TX        |
| Send via LoRa | 


#### Parameter im **Echo Trainer** Modus

| Parameter | | | |
| -------------- | ---|---|---|
| Encoder Click |  Tone Pitch Hz   | Paddle        | Paddle Polarity | 
| Keyer Mode     |  CurtisB DahT% |  CurtisB DitT% | AutoChar Spce   | 
| Tone Shift |  Interword Spc |  Interchar Spc |  Random Groups |
| Length Rnd Gr | Length Calls | Length Abbrev | Length Words |
|Echo Repeats    | Confrm. Tone  |


#### Parameter im **Echo Trainer - File Player** Modus

| Parameter | | | |
| -------------- | ---|---|---|
| Encoder Click | Tone Pitch Hz   |  Paddle        |  Paddle Polarity | 
| Keyer Mode     |  CurtisB DahT% | CurtisB DitT% | AutoChar Spce   | 
| Tone Shift |  Interword Spc | Interchar Spc | 

#### Parameter im **Koch Trainer - CW Generator** Modus

| Parameters| | | |
| -------------- | ---|---|---|
| Encoder Click | Tone Pitch Hz   | Paddle        | Interword Spc | 
| Interchar Spc | Length Rnd Gr | Length Abbrev | Length Words | 
| Trainer Disp | Each Word 2x | ey ext TX       Send via LoRa | 
<div style="page-break-after: always;"></div>

#### Parameter im **Koch Trainer - Echo Trainer** Modus

| Parameter | | | |
| -------------- | ---|---|---|
| Encoder Click | Tone Pitch Hz   | Paddle        |  Paddle Polarity | 
| Keyer Mode     |  CurtisB DahT% |  CurtisB DitT% |  AutoChar Spce   | 
| Tone Shift |  Interword Spc | Interchar Spc | Length Rnd Gr | 
| Length Abbrev |  Length Words | Echo Repeats    |  Confrm. Tone  | 

#### Parameter im **Transceiver - LoRa Trx** Modus

| Parameter | | | |
| -------------- | ---|---|---|
| Encoder Click | Tone Pitch Hz   | Paddle        |  Paddle Polarity |
| Keyer Mode     |  CurtisB DahT% |  CurtisB DitT% |  AutoChar Spce   | 
| Tone Shift | 

#### Parameter im **Transceiver - Ext / iCW Trx** Modus

| Parameter | | | |
| -------------- | ---|---|---|
| Encoder Click | Tone Pitch Hz   |  Paddle        |  Paddle Polarity |
| Keyer Mode     | CurtisB DahT% |  CurtisB DitT% |AutoChar Spce   | 
| Tone Shift |  Bandwidth | 

#### Parametersim **CW Decoder** mMdus
| Parameter | | | 
| -------------- | ---|---|
| Encoder Click | Tone Pitch Hz   |  Bandwidth | 


<div style="page-break-after: always;"></div>


## WLAN-Funktionen ##

Man kann die WLAN-Funktionalität des im Morserino-32 verwendeten Heltec ESP32 Wifi LoRa-Moduls für zwei Funktionen des Geräts verwenden:

* Laden einer Textdatei in den Morserino-32, die dann im CW-Generator-Modus oder im Echo-Trainer-Modus abgespielt werden kann.
* Hochladen der Binärdatei einer neuen Firmware-Version.

Für beide Funktionen muss sich die hochzuladende Datei (entweder eine Textdatei oder die kompilierte Binärdatei für das Softwareupdate) auf deinem Computer befinden (selbst ein Tablet oder Smartphone funktioniert, da auf diesem Gerät nur grundlegende Webbrowser-Funktionen erforderlich sind), und dein Morserino muss mit demselben WLAN-Netzwerk wie dein Computer verbunden sein.

Damit sich der Morserino-32 mit einem lokalen WLAN-Netzwerk verbinden kann, muss das Gerät die SSID (den "Namen") des Netzwerks kennen und ebenso das Passwort. Man muss also diese beiden Elemente in den Morserino-32 eingeben. Da es keine Tastatur für die bequeme Eingabe dieser Informationen gibt, verwenden wir eine andere Methode, und dafür wurde eine eigene WLAN-Funktion implementiert: die Netzwerkkonfiguration, die man zuerst anweden muss, bevor man die anderen Funktionen verwenden kann.

####Netzwerkkonfiguration####
Während der Morserino-32 das Startmenü anzeigt, klicke ***drei mal*** schnell auf die ROTE Taste, um in das WLAN-Menü zu gelangen. Der oberste Eintrag lautet "WiFi Config". Wähle ihn aus, um fortzufahren.

Das Gerät startet WiFi als Zugangspunkt (wie ein WLAN Router) und erstellt so ein eigenes WLAN-Netzwerk (mit der SSID "Morserino"). Wenn man die verfügbaren Netzwerke mit dem Computer oder Smartphone überprüft, wird man es leicht finden. Verbinde nun deinen Computer (oder dein Handy) mit diesem Netzwerk (man braucht kein Passwort, um eine Verbindung herzustellen).

Wenn du verbunden bist, gib "m32.local" in deinen Browser ein. Es erscheint nunein kleines Formular mit nur zwei Feldern: SSID und Passwort. Gib den Namen deines lokalen WLAN-Netzwerks und das entsprechende Passwort ein und klicke auf die Schaltfläche "Submit". Der Morserino-32 speichert diese Netzwerk-Anmeldeinformationen und führt einen Neustart durch (das Netzwerk "Morserino" wird verschwinden).

Hinweis: Man kann kein WLAN-Netzwerk mit einem "Captive-Portal" verwenden, wie sie häufig in öffentlichen Netzwerken verwendet werden. Diese Netzwerke setzen voraus, dass auf dem Gerät ein Browser verfügbar ist, der eine Verbindung zum Netzwerk herstellen möchte, aber der Morserino-32 hat keinen Browser ...

#### Hochladen einer Textdatei ####

Nachdem du den Morserino-32 für dein lokalens WLAN  konfiguriert hast, kannst du nun eine Textdatei hoch laden, die du für dein Morse-Training verwenden möchtest. Derzeit kann sich nur immer genau eine Datei auf dem Morserino-32 befinden. Wenn man eine neue Datei hochlädt, wird die alte Datei überschrieben.

Die **Datei**, die man hochlädt, sollte eine reine ASCII-Textdatei ohne Formatierung sein (keine Word-Dateien, PDF-Dokumente usw.). Deutsche Zeichen (ÄÖÜäöüß), die als UTF-8 codiert sind, sind zulässig und werden in ae, oe, ue und ss umgewandelt. Die Datei kann Groß- und Kleinbuchstaben sowie alle Zeichen enthalten, die Teil der Koch-Methode sind (insgesamt 50 Zeichen). Alle anderen Zeichen werden einfach ignoriert, wenn die Datei im Morse-Code abgespielt wird. Die Datei kann ziemlich groß sein - man hat mehr als 1 MB Speicherplatz zur Verfügung (genug, um eine Kopie von Mark Twains "Die Abenteuer von Huckleberry Finn" zu speichern).

Um die Datei hochzuladen, drücke dreimal schnell die ROTE Taste und wähle "File Upload" aus dem Menü. Nach einigen Sekunden (es muss zuerst eine Verbindung zu deinem WLAN-Netzwerk hergestellt werden), zeigt der Morserino-32 an, dass er auf den Upload wartet. Geh mit dem Browser deines Computers zu "m32.local" (oder zur auf dem Display angezeigten IP-Adresse).

Zuerst erscheint einen **Login** Bildschirm. Verwende als Benutzer-ID "**m32**" und als Kennwort "**upload**". Als nächstes wird im Browser ein Dateiauswahldialog gezeigt. Wählen die Datei aus, die du hochladen möchtest (der Name oder die Erweiterung spielt keine Rolle), und klicke auf die Schaltfläche "Begin". Sobald der Upload abgeschlossen ist (es dauert nicht lange), wird der Morserino-32 neu gestartet, und du kannst die hochgeladene Datei nun im Modus *CW Generator* oder *Echo Trainer* verwenden.

Wenn man den Vorgang aus irgendeinem Grund abbrechen mus, muss man das Gerät neu starten, indem man es vollständig von der Stromversorgung trennt (Akku ausschalten und USB trennen) oder die Reset-Taste mit einem kleinen Schraubendreher oder einem Kugelschreiber drückt (Die Resttaste ist durch das Loch unterhalb des USB-Anschlusses zu erreichen).

#### Aktualisieren der Morserino-32-Firmware ####

Das Aktualisieren der Firmware des Morserino-32 über WLAN ist der einfachste Weg, um die Firmware zu aktualisieren. Während man normalerweise eine Softwareentwicklungsumgebung auf einem Computer benötigt (in unserem Fall die Arduino IDE plus die erforderlichen Dateien für die Unterstützung des Heltec-Moduls), und man damit die Software kompiliert,muss man auch sicherstellen, dass alle erforderlichen Bibliotheken installiert sind. Erst dann kann man die Software  über USB in den Mikrocontroller laden. Mit der WLAN Methode benötigt man lediglich einen Computer mit einem Browser und ein WLAN-Netzwerk.

Das Aktualisieren der Firmware ist dem Hochladen einer Textdatei sehr ähnlich. Man muss zuerst die Binärdatei aus dem Morserino-32-Repository auf GitHub holen (https://github.com/oe1wkl/Morserino-32 - Unter "Software" findet man ein Verzeichnis mit der Bezeichnung "Binaries". Lade nun die neueste Version herunter und speichere sie auf deinem Computer. Der Dateiname sieht folgendermaßen aus:

`morse_3_vx.y.ino.wifi_lora_32.bin`, wobei x.y die Versionsnummer ist.

Rufe nun erneut das WLAN-Menü auf, indem du dreimal schnell auf die ROTE Taste klickst und die Option "**Update Firmw.**" auswählst. Ähnlich wie beim Hochladen von Dateien geh mit dem Browser zu  "m32.local", und schließlich wird wieder ein Anmeldebildschirm angezeigt. Dazu verwende den Benutzernamen "**m32**" und das Passwort "**Update**".

Als Nächstes wird wieder ein Dateiauswahldialogangezeigt, man wählt die gespeicherte Binärdatei aus und klicketauf die Schaltfläche "Begin". Diesmal dauert das laden  länger - es kann einige Minuten dauern, man sollte also etwas Geduld haben. Die Datei ist groß, muss hochgeladen und in den Morserino-32 geschrieben werden und dann überprüft werden, um sicherzustellen, dass es sich um eine ausführbare Datei handelt. Schließlich startet das Gerät neu und man sollten während des Startvorgangs die neue Versionsnummer auf dem Display sehen können.