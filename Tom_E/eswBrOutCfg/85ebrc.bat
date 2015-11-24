rem 85ebrc - e-switch brownout version with configuration (eswBrOutCfg)
rem
avrdude -p attiny85 -c usbasp -u -Uflash:w:\Tiny254585Projects\eswBrOutCfg\eswBrOutCfg\Release\eswBrOutCfg.hex:a -Ueeprom:w:\Tiny254585Projects\eswBrOutCfg\eswBrOutCfg\Release\eswBrOutCfg.eep:a
