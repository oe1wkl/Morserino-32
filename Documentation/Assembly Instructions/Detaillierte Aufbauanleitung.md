# Morserino-32: Detaillierte Aufbauanleitung

### Bevor du beginnst:

* **Obwohl der Bausatz recht einfach zu montieren ist, solltest du einige Erfahrungen mit dem Zusammenbau von elektronischen Bausätzen haben.** Wenn nicht, empfehle ich dringend, jemanden um Hilfe zu bitten, der diese Erfahrung hat. Selbst einfach zu bauende Kits können leicht ruiniert werden (natürlich kann ich dir Ersatzteile zur Verfügung stellen, falls etwas Schlimmes passiert ist - aber du wirst verstehen, dass ich das nicht kostenlos tun kann - ich muss die Teile ja selbst auch kaufen.)

* Stelle sicher, dass du weißt, welchen Typ von LiPo-Akku du verwendest und wo du ihn anbringen möchtest:
	* Wenn er klein genug ist, sollte er unter das ESP32-Modul passen.
	* Andernfalls solltest du ihn an der Bodenplatte des Gehäuses befestigen und sicherstellen, dass zwischen Batterie und Leiterplatte genügend Abstand besteht (**Achtung! Stelle sicher, dass keine scharfen Drahtenden oder dergleichen den Akku beschädigen** - Dies kann zur Explosion des Akkus führen!). Zwei verschiedene Größen von Abstandsbolzen sind im Lieferumfang enthalten - verwende die 12-mm-Bolzen, wenn der Akku auf der Bodenplatte sitzt. Wenn er SEHR dick ist, kannst du  sogar die 6 mm und 12 mm Abstandhalter kombinieren, um einen Abstand von 18 mm zu erhalten! Wenn du keinen zusätzlichen Platz unter der Leiterplatte benötigst, verwende die 6-mm-Abstandhalter.
* Ich gehe davon aus, dass du einen Akku mit einem Molex-Stecker verwendest. Wenn du einen Akku mit einem anderen Stecker bevorzugest, musst du dir ein eigenes Kabel mit einem passenden Stecker für den Akku besorgen.
* Lege alle nötigen Werkzeuge bereit: einen Lötkolben mit feiner Spitze, einen dünnen Lötdraht (verwende kein bleifreies Lot, wenn du dir nicht das Leben schwer machen willst), gute Beleuchtung, möglicherweise eine Lupe, einen kleinen Drahtschneider und (zur Montage des Gehäuses) einen kleinen Inbusschlüssel (2 mm; wenn nicht verfügbar, funktioniert ein T6X20 Torx zur Not auch).
* **Hinweise zum Löten:** KEIN bleifreies Lot verwenden (ich habe es schon erwähnt, nicht wahr?). Verwende einen hochwertigen Lötkolben, der auf eine vernünftige Temperatur eingestellt ist (ich bevorzuge es, ein wenig auf der hohen Seite zu sein - das ermöglicht es, die Verbindungen schnell herzustellen, und einige der Teile sind etwas groß, und wenn sie mit Masse verbunden sind, benötigen sie viel Hitze, um eine einwandfreie Lötstelle herzustellen. Dort viele Sekunden mit dem Lötkolben zu warten, bis das Lot endlich schmilzt, ist eine gute Möglichkeit, die Leiterplatte zu ruinieren.
* Überprüfe, ob alle Komponenten bereit sind - identifiziere sie anhand der Packliste und ermittele anhand der Zeichnung, wohin sie gehören. Bitte informiere [mailto:info@morserino.info]() falls Bauteile fehlen oder defelt sein sollten, damit dir Ersatz geschickt werden kann!
![Platzierungszeichnung][Ass01]
[Ass01]: Images/3_placement.jpg "Position der Komponenten"

### Schritt-für-Schritt-Anleitung (lies sie zumindest durch, auch wenn du dich entscheiden solltest, es anders zu machen)


1. Löte zunächst die folgenden Komponenten auf die Leiterplatte: Die 4 Klinkenbuchsen (3 verschiedene Typen, aber es ist leicht zu erkennen, welche an welcher Stelle passt); dann den blauen Trimmerwiderstand (seine Orientierung ist egal) und den Schiebeschalter (mit dem Hebel nach außen, natürlich); dann den Drehgeber und den Druckknopfschalter. 

	Dieser hat eine besondere Tendenz, nicht exakt vertikal montiert zu werden - was schlecht ist, weil er dann vielleicht nur schlecht durch das Loch der oberen Gehäuseplatte passt. Also achte bitte darauf, das er wirklich sauber auf der Platine sitzt. Diser Taster hat zwei vorspringende "Nasen" auf seinem Gehäuse - eine sollte zur langen Kante der Platine zeigen, und die andere logischerweise in Richtung Lautsprecher.
2. Du findest alle diese Bauteile in der rosafarbenen Plastiktüte (siehe auch die Abbildung Am Ende dieser Anleitung). Löte noch nicht die 2 Buchsenleisten, die Kabel und den Lautsprecher ein. Stelle sicher, dass alle Komponenten auf der Leiterplatte bündig sind - insbesondere der Drehgeber, der Trimmer und der Drucktastenschalter - ich hab das doch soeben erwähnt, nicht? (sonst passt das Gehäuse dann nicht sehr gut). Stelle sicher, dass sich der Schiebeschalter in der Position "OFF" (in Richtung der Klinkenbuchse) befindet.
3. Verbinde das Kabel mit dem kleinen 1,25-mm-JST-Stecker mit der Buchse an der Unterseite des Heltec-Moduls (achte darauf, dass du die Buchse nicht mit brutaler Gewalt von der Platine abreißt - halte sie mit dem Finger fest).
Der rote Draht muss auf der Seite sein, wo am Heltec-Modul ein + aufgedruckt ist.

	![Power][Ass04]
[Ass04]: Images/power_cable.jpg "Orientation of Power cable"
	![Power][Ass05]
[Ass05]: Images/attach_cable.jpg "Orientation of Power cable"

2. Stecke die Buchsenleisten (18 Positionen) auf die Stifte des Heltec-Moduls, und stelle auch hier sicher, dass sie bündig auf dem Modul sitzen. ![Power][Ass06]
[Ass06]: Images/pin_headers.jpg "Attached pin headers"
5. Setze das Heltec-Modul mit seinen angebrachten Steckerleisten sorgfältig auf die Leiterplatte, stelle dabei sicher, dass keine Stifte verbogen sind, und löte die Steckerleisten auf die Leiterplatte, und stelle auch hier sicher, dass alles bündig auf der Leiterplatte sitzt.
4. Löte die Enden des Kabels das vom Heltec Module kommt an die mit ESP32 + (rot) und ESP32- (schwarz) gekennzeichneten Kontaktstellen auf der Oberseite der Leiterplatte, nachdem du die Drähte durch die Löcher neben den Kontaktstellen (als Zugentlastung) geführt hast und diese von unten durch die Löcher des Kontakts geführt hast. **Mach keinen Fehler mit der Polarität des Kabels - das Heltec-Modul würde dadurch höchstwahrscheinlich zerstört!**

 ![Battery cable][Ass08]
[Ass08]: Images/mounted_battery_cable.jpg "Orientation of Power cable"
6. Kürze das Kabel für den Akkuanschluss mit dem Molex-Stecker - die Länge hängt davon ab, wo du den Akku platzierst. Wenn er unter dem Heltec-Modul sitzt, sollte es etwa 3 bis 4 cm lang sein. Entferne die Isolierung auf eine Länge von etwa 3 mm (1/8 Zoll) und verzinne die Enden sorgfältig.
7. Wenn sich der Akku unter dem Heltec-Modul befinden soll, führe die Kabel von der Oberseite der Platine durch die Löcher neben den Pads LIPO + und LIPO- und dann in die Löcher der Pads. Verlöte sie auf der Oberseite der Platine. Wenn sich der AKku jedoch unter der Leiterplatte befinden soll, kürze das Kabel auf eine passende Länge, führe es von unten durch die Zugentlastungslöcher und dann von oben durch die Löcher der Pads und verlöte sie an der Unterseite. Ober- und Unterseite der Platine sind so markiert, dass du das richtige Pad leicht finden kannst. **Mach keinen Fehler mit der Polarität des Kabels - das Heltec-Modul würde dadurch höchstwahrscheinlich zerstört!**
8. Löte nun den Lautsprecher auf die Platine (dies erst jetzt, da es schwierig wird, die Kabel auf der Oberseite zu löten, sobald der Lautsprecher auf der Platine sitzt).
9. Setze den schwarzen Knopf auf den Schaft des Drehgebers (im Inneren des Knopfs befinden sich 2 kleine Führungsleisten, die auf die abgeflachte Seite des Schafts auszurichten sind), und die rote Kappe auf den Schaft des Drucktasters.
9. Wenn sich der Akku unter dem Heltec-Modul befinden soll, befestige in dort mit einem kleinen Streifen doppelseitigem Klebeband (wie Tixo, aber auf beiden Seiten klebend). Verwende keine dicken doppelseitigen Klebebänder, da die Batterie dann unter Umständen nicht genügend Platz unter dem Modul hat!
	![double sided tape][Ass09]
[Ass09]: Images/tesa_double_sided.jpg "Orientation of Power cable"
10. Ich würde jetzt den Batterieanschluss mit einem Ohmmeter überprüfen, nachdem ich den Schiebeschalter auf ON gestellt habe, um zu sehen, dass wir nirgendwo einen Kurzschluss verursacht haben. Es sollte einen relativ hohen Widerstand zeigen (> 1 MOhm). Wenn es nahe 0 Ohm anzeigt, überprüfe die Verkabelung! Schiebe den Schalter dann wieder in die OFF-Position.
11. Stecke den Stecker des Akkus in den Molex-Anschluss.
19. Setze die Paddles in die Platinenstecker ein (linkes und rechtes Paddel sind gleich, daher kann man hier keinen Fehler machen). **Die gelben Klebestreifen von den Platinensteckern vorher vorsichtig entfernen!** (Diese sind vom Hersteller angebracht und weren für die aotomatische Bestückung der Platine benötigt.)

    **Die folgenden Schritte gelten nur, wenn du den Bausatz mit Gehäuse (und Antenne) erworben hast.**

12. Entferne die Schutzfolien von den Plexiglasplatten (die gravierte Seite hat keine Schutzfolie).
12. Montiere zuerst die 15 mm-Abstandsbolzen (weiblich / weiblich) mit den mitgelieferten Metallschrauben an der oberen Platte des Gehäuses. Die Bolzen sollten sich unter der Platte befinden, und die eingravierten Inschriften von oben lesbar sein.
13. Befestige den Antennenstecker an der oberen Platte. Verwende dazu die gezahnte Unterlegscheibe an der Unterseite und den Federring sowie die Mutter an der Oberseite (sie sollte fest sitzen, aber auf das Acrylglas keinen zu hohen Druck ausüben, da diese sonst springen könnte).

	![Antenna connector][Ass10]
[Ass10]: Images/sma_connector.jpg "SMA connector"
14. Drücke den kleinen IPX-Koaxialstecker auf die Koaxbuchse des Heltec-Moduls.
15. Entferne die Schutzfolie von den beiden Acrylglasplatten (die obere Platte hat nur eine Folie, auf der Seite, die nicht graviert ist). Montiere nun die obere Platte mit ihren Abstandshaltern auf die Leiterplatte. Verwende dazu entweder die 6-mm-Abstandshalter (wenn die Batterie unter dem Heltec-Modul sitzt) oder die 12-mm-Abstandshalter (wenn der Akku unter der Platine sitzt und etwas mehr Platz benötigt) auf der Unterseite der Platine. Stelle sicher, dass das Display in die Öffnung passt, und ebenso die kleine Spule (WLAN-Antenne). Stelle auch sicher, dass die Batteriekabel und das Antennenkabel sinnvoll geführt sind und den Drehgeber oder den Drucktastenschalter nicht behindern.
**Beschädige nicht die Kupferspirale (= WLAN Antenne), und gib acht dass du die flexible Verbindung zum Display nicht verbiegest oder beschädigst!**

	![Almost done][Ass11]
[Ass11]: Images/view_from_below.jpg "View from below"
16. Wenn sich der Akku auf der Bodenplatte befindet, befestige die Batterie jetzt (mit doppelseitigem Klebeband) und schließe sie an.
17. Befestige nun die Bodenplatte mit den mitgelieferten Metallschrauben an den Abstandshaltern.
18. Befestige die selbstklebenden Kunststofffüße neben den Schrauben auf der Bodenplatte.

20. Schraube die Antenne an den Antennenanschluss. (**Natürlich benötigt man die Antenne nur, wenn man mit LoRa sendet. Beachte jedoch, dass das Senden ohne Antenne den LoRa-Transceiver auf dem Heltec-Modul letztendlich zerstört!** Sicher ist sicher!)
21. Schiebe nun den Batterieschalter auf "ON" und starte den Morserino-32! Wenn du noch keinen Akku hast, kannst du das USB-Kabel verwenden, um ihn so an eine 5-V-Quelle anzuschließen, entweder an deinem Computer oder einem Telefonladegerät. Da das Modul bereits programmiert ist, kann der Morserino-32 sofort verwendet werden. **Lese im Benutzerhandbuch** nach, wie man das Gerät verwendet, und seine zahlreichen Funktionen benutzt. Die Software kann auch aktualisiert werden, wenn eine neuere Version verfügbar ist - dies wird auch ausführlich im Benutzerhandbuch beschrieben.
22. Die Schachtel, in der der Bausatz geliefert wurde, eigent sich auch dafür, das fertig aufgebaute Gerät aufzubewahren (eventuell empfiehlt es sich, auf den Boden und auf den Deckel ein Stück Schaumstoff aufzukleben).
    
     ![Aufbewahrung][Ass02] 
     
     Wer nach einer etwas dauerhafteren Aufbewahrungsmöglichkeit sucht, ist mit einer Lunchbox aus Kunststoff sehr gut aufgehoben  (siehe Foto; auch hier wären Einlagen aus Schaumstoff empfehlenswert).
[Ass02]: Images/lunchbox.jpg "Aufbewahrung in der Lunchbox"

###Auswahl eines geeigneten LiPo Akkus für den Morserino-32

Für meine Prototypen benutze ich einen kleinen 600 mAh einzelligen LiPo Akku wie er häufig in kleinen fengesteuerten Modellen wie zB. Quadcoptern verwendet wird. Die Marke, die ich nutzte ist "Tattu" und har folgenden Spezifikationen
(siehe [https://www.gensace.de/tattu-600mah-3-7v-30c-1s1p-lipo-battery-pack-with-molex-plug-1-pcs-pack.html]() oder [https://www.genstattu.com/tattu-25c-1s-3-7-v-600mah-lipo-battery-pack-with-molex-plug-6pcs.html]()): 

**Tattu** 600mAh 3.7V 30C 1S1P Lipo Akku mit Molex Stecker

* Kapazität: 600mAh
* Spannung: 3.7V
* Max Dauerentladestrom: 30C (18A)
* Gewicht: 15.7g
* Abmessungen: 60 x 19 x 7mm

Siehe das Bild unten, Tattu ist der obere Akku.

Natürlich kann man ähnliche Akkus auch anderer Hersteller benutzen; damit der Akku unter dem Mikrocontroller Platz hat, darf er **nicht größer** sein als **65 x 20 x 8 mm**. 

Der Akku darf nur eine Zelle haben (Nominalspannung 3,7 V), und sollte mit einem Molex-Stecker versehen sein (ein Kabel mit passendem Gegenstück ist im Bausatz enthalten. Willst du einen Akku mit anderem Anschluss benutzen, musst du dir selber ein passendes Kabel mit Stecker besorgen. 

Du kannst auch größere Akkus verwenden, die müssen dann unter der Platine auf der Bodenplatte des Gehäuses montiert werden. Damit man da etwas Flexibilität hat, liegen dem Bausatz Distanzstücke von 6mm und 12mm Länge bei, mit denen man die Bodenplatte befestigen kann. Mit den 12mm Distanzstücken dürfen die größeren Akkus bis zu 10 mm dick sein. Man köntne auch die 6mm und 12mm Abstandsbolzen in Kombination verwenden, da darf der Akku sogar bis zu 16 mm dick sein. Man muss in jedem Fall dafür sorgen, dass der Akku nicht durch vorstehende Spitzen auf der Paltinenunterseite beschädigt werden kann - dies könnte zu Feuer und Explosion des Akkus führen!

Eine mögliche Option für einen größeren Akku wäre dieser (der untere Akku am Bild): ![Akkus][Ass03] [https://www.amazon.de/gp/product/B01JJ6DA7A/]() oder sogar so etwas Ähnliches wie dieser: [https://www.gensace.de/gens-ace-3500mah-3-7v-tx-2s1p-lipo-battery-pack-with-jr-plug.html]() (dieser hat allerdings keinen Molexstecker, man müsste also ein dazu passendes Anschlusskabel besorgen). 
[Ass03]: Images/batteries.jpg "Empfohlene Akkus"

## Appendix: Bauteile
![Bauteile][Ass12]
[Ass12]: Images/M32_Components.png "Bilder aller Bauteile"

