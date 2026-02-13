# VoidRunner
Void Runner — an Arduino mini‑game where you dodge enemies, trigger a hand‑wave shield, and survive as long as you can. Powered by a joystick, ultrasonic sensor, IR remote, LCD, RGB LED, and buzzer.

Features
• 	Joystick movement (up/down + button to start)
• 	Two independent enemies with random spacing
• 	Speed increases every 5 points
• 	Ultrasonic “hand‑wave” shield (2s active, 5s cooldown)
• 	RGB LED status feedback (ready, active, cooldown)
• 	IR remote for standby/menu control
• 	Buzzer sound effects (power, shield, cooldown, death)
• 	LCD display with custom characters
• 	Serial output for debugging and score logs

Wiring Summary
• 	LCD: RS→D2, EN→D3, D4→D4, D5→D5, D6→D6, D7→D7
• 	Joystick: VRy→A0, SW→D8
• 	Ultrasonic: TRIG→D9, ECHO→D10
• 	IR Receiver: OUT→D11
• 	Buzzer: D12
• 	RGB LED: R→A2, G→A3, B→A4
• 	Serial: USB to PC

How to Run
1. 	Open  in Arduino IDE
2. 	Upload to Arduino Uno
3. 	Open Serial Monitor
4. 	Type START to unlock the system
5. 	Press joystick button to begin playing

Gameplay
• 	Move between top/bottom rows with the joystick
• 	Avoid enemies as they scroll left
• 	Score increases when enemies pass
• 	Every 5 points, the game speeds up
• 	Wave your hand over the ultrasonic sensor to activate a shield
• 	Shield lasts 2 seconds, then enters cooldown
• 	Game ends on collision without shield
