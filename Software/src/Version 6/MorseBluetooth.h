#ifndef MORSEBLUETOOTH_H
#define MORSEBLUETOOTH_H

#ifdef CONFIG_BLUETOOTH_KEYBOARD

namespace MorseBluetooth
{
	// Forward declarations
	void initializeBluetooth(void);
	void bluetoothTypeLCTRL(bool ctrl);
	void bluetoothTypeCharacter(const char chr);
	void bluetoothTypeString(const String str);
};

#endif //ifdef CONFIG_BLUETOOTH_KEYBOARD
#endif //ifndef MORSEBLUETOOTH_H