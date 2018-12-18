# Morserino-32: Detailed Assembly Instructions

### Before you begin:

* **While the kit is rather easy to assemble, you should have some kit-building experience.** If not, I strongly suggest that you get some help from someone who has that experience. Even kits that are easy to build can be ruined (of course I can supply you with spare parts should something bad have happened - but you will understand that I cannot do so for free - I have to buy the parts myself.)

* Make sure you know which type of LiPo battery you are going to use, and where you are going to mount it:
	* If it is small enough it should fit under the Heltec module.
	* Otherwise you should mount it to the bottom plate of the case, making sure you have enough clearance between the battery and the PCB (**Danger!** **Make sure no sharp ends of wires etc. are doing any harm to the LiPo battery** - this could result in exploding batteries!). Two different sizes of stand-off bolts have been supplied - use the 12mm ones if your battery sits on the bottom plate; if it is VERY thick you could even combine the 6 mm and the 12 mm stand-offs to give you 18 mm of clearance! If you do not need extra space below the PCB, use the 6 mm stand-offs.
	* I am assuming that you are going to use a battery with a Molex connector - should you prefer a battery with a different connector, you need to supply our own cable with a fitting connctor for the battery.
* Get all the tools necessary: a fine-tipped soldering iron, thin solder wire (do not use lead-free solder unless you want to make your life hard), good lighting, possibly a magnifying glass, a small wire cutter and (for assembling the case) a small hex key (2 mm; if not available a T6X20 Torx will work reasonably well).
* **Soldering advice:** Do NOT use lead-free solder (I mentioned it before, didn't I). Use a high quality soldering iron, set to a reasonable temperature (I prefer them to be a bit on the high side - allows you to make the joints quickly, and some of the parts are biggish; if connected to ground, you need a lot of heat to make the joint, and staying there with the iron for many, many seconds waiting for the warm-up is a good way to ruin your PCB).
* Check that all components are ready - identify them with the help of the packing list, and identify where they will go, with the help of the placement drawing. Please notify [mailto:info@morserino.info]() if any components are missing or defective, so that some replacement can be sent to you!![placement drawing][Ass01]
[Ass01]: Images/3_placement.jpg "Location of Components"

###Step-by-step instructions (at least read them, even if you decide to do it differently)


1. First solder the following components to the PCB: The 4 phone jacks (3 different types, but it is easy to see which one fits where); then the trimmer resistor, and the sliding switch: then the rotary encoder and the push-button switch. You will find all these components in the pink plastic bag. Do not yet solder the 2 female pin headers, the cables and the loudspeaker.  Make sure all components are flush on the PCB - especially the rotary encoder, the trimmer and the push button switch (otherwise the case might not fit very well). Make sure the sliding switch is in the "OFF" position (towards the phone jack).
2. Put the female pin headers (18 positions) onto the pins of the Heltec module, again making sure they are sitting flush on the module.
3. Connect the cable with the small 1.25 mm JST connector to the connector on the underside of the Heltec module (be careful not to push the socket on the module off the board with brute force - hold it with your finger)
4. Solder the other ends of this cable to the pads marked ESP32+ (red) and ESP32- (black) on the top of the PCB, after threading the wires through the holes besides the pads (intendes as strain relief), and threading them into the pad holes from below.  **Do not make a mistake with the polarity of the cable - doing so will almost certainly destroy the Heltec module!**
5. Put the Heltec module with its attached headers carefully onto the PCB, making sure you are not bending any pins, and solder the header pins to the PCB, again making sure that everything sits very flush on the PCB.
6. Shorten the battery cable with the Molex connector  - the length depends on where you will put the battery. If it will sit under the Heltec module, it should be around 3-4 cm (1.2 - 1.5 inches) long. Remove the insulation for about 3 mm (1/8 inch), and tin the ends carefully.
7. If the battery will sit under the module, thread the battery cables from the top of the PCB through the holes besides the pads LIPO+ and LIPO-, and then into the holes of the pads, and solder them on the top side of the PCB. If your battery will sit underneath the PCB, shorten the cable to a convenient length, thread them from underneath through the strain relief holes and into the pad holes, and solder them on the bottom side. Top and bottom sides of the PCB are marked so that you will find the correct pad. **Do not make a mistake with the polarity of the cable - doing so will almost certainly destroy the Heltec module!**
8. Solder the loudspeaker to the PCB (this is last, because it will make soldering the cables on the top side difficult once the speaker sits on the board).
9. Put the black knob onto the shaft of the encoder (inside the know there are two small guiding ledges that need to be aligned with the flattened side of the shaft), and the red cap onto the shaft of the push button switch.
9. If the battery sits under the the module, place it there and fix it with a small strip of double sided adhesive (like cellotape, but sticky on both sides). Do not use thick double sided adhesives, as the battery might then not fit under the module with enough clearance! 
10. I would now check the battery connector with an Ohm meter after setting the sliding switch to the ON position, to see that we did not create a short circuit anywhere. It should show a relatively high resistance. If it shows close to 0 Ohms, check the cabling! Slide the switch into the OFF position again.
11. Plug the battery cable into the Molex connector.

    **The following steps only apply if you got the kit with the case (and antenna).**

12. First mount the 15mm standoff bolts (female/female) with the supplied metal screws to the top plate of the case; the stand-offs should be under the plate with the engraved inscriptions readable from above.
13. Mount the antenna connector to the top plate, using the serrated washer on the underside, and the split washer and nut on the top side (it should sit firmly, but do not exercise too much pressure on to the acrylic glass, as otherwise it might crack).
14. Press the tiny IPX coax connector onto the coax socket on the HeltecHeltec module.
15. Remove the protecting foil from the two case plates (the top plate one has the foil only on the non-engraved side).Now mount the top plate with its stand-off spacers to the PCB, using either the 6 mm male/female stand-offs (if the battery will sit under the Heltec module) or the 12 mm male/female stand-offs (if the battery will sit under the PCB and needs a bit more space) on the underside of the PCB. Carefully make sure the display fits into the opening, as well as the little coil (WLAN antenna). Make sure the battery cables and the antenna cable run smoothly and are not obstructing the encoder or the push-button switch.
16. If the battery will sit on the bottom plate, fix the battery now (with double-sided adhesive tape) and plug it in.
17. Now fix the bottom plate to the stand-offs with the supplied metal screws.
18. Stick the adhesive feet onto the bottom plate, besides the screws.
19. Put the paddles into the PCB connectors (left and right paddle are the same, so you cannot make a mistake here).
20. Screw the antenna to the antenna connector. (**Of course, you only need the antenna when you are transmitting with LoRa; but be aware, that transmitting without an antenna will eventually destroy the LoRa transceiver on the Heltec module!** Better safe than sorry!)
21. Slide the battery switch to "ON" and start using the Morserino-32! If you do not have a battery yet, you can use the USB cable to connect to a 5V source, either from your computer or a phone charger or similar. As the module has been programmed already, you can use the Morserino-32 immediately. **See the User Manual** on how to use it and learn about its many functions. You can also update the software, should there be a newer version available - this is also described in detail in the User Manual.
22. The cardboard box the kit came in is also suitable for storing the assembled Morserino-32 while not in use (Eventually it is advisable to glue a piece of plastic foam to the bottom and to the top of the box).  
   
    ![Storag][Ass02]  Should you be looking for something a bit more durable, I would recommend you a plastic lunch box (see the photo; I would recommend some foam inserts here as well). 
[Ass02]: Images/lunchbox.jpg "Storage in a lunchbox"
 

###Choice of LiPo battery for the Morserino-32

For my prototypes I used a small 600 mAh single-cell LiPo battery that is commonly used in RC devices like quadcopters etc. The brand I use is „Tattu“ and has the following specs (see [https://www.gensace.de/tattu-600mah-3-7v-30c-1s1p-lipo-battery-pack-with-molex-plug-1-pcs-pack.html]() or [https://www.genstattu.com/tattu-25c-1s-3-7-v-600mah-lipo-battery-pack-with-molex-plug-6pcs.html]()): 

**Tattu** 600mAh 3.7V 30C 1S1P Lipo Battery Pack with Molex Plug

* Capacity: 600mAh
* Voltage: 3.7V
* Max Continuous Discharge:30C (18A)
* Weight: 15.7g
* Dimensions: 60 x 19 x 7mm

See the picture below; Tattu is the battery at the top).

Of course you can use similar batteries from other manufacturers; in order to fit under the micro controller the **maximum dimensions** are: **65 x 20 x 8 mm**. 

It must be a single cell LiPo (nominally 3.7 V), and should have a Molex plug (because a cable with such a connector is part of the kit - if you use a battery with a different type of connector, you need to supply your own cable for connecting it to the Morserino-32!).

You can use larger batteries if you mount them under the PCB, on the bottom plate of the case. To give you more flexibility, the kit will contain 6mm and 12mm distance bolts for mounting to the bottom plate. This means you can use pretty large LiPos, as long as they are < 10 mm thick. If you use the 6mm and 12 mm stand-offs in combination, you could even use batteries that are up to 16 mm thick! You have to make sure that you mount the battery in such a way that it is impossible for any sharp metal spikes to scratch the surface of the battery - this could lead to fire or explosion!

One option would be this type (in the picture the battery at the bottom): ![Akuss][Ass03]
[Ass03]: Images/batteries.jpg "Position der Komponenten"
[https://www.amazon.de/gp/product/B01JJ6DA7A/]() or even that one, for even more capacity: [https://www.gensace.de/gens-ace-3500mah-3-7v-tx-2s1p-lipo-battery-pack-with-jr-plug.html]() (but be aware that this one does not use a Molex plug - you would need to find a cable with a suitable plug to connect this to your Morserino-32). 
[Ass03]: Images/batteries.jpg "Recommended batteries"

