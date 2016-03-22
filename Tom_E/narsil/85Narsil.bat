rem 85Narsil - downloads Narsil (Tiny85 e-switch UI configurable)
rem
avrdude -p attiny85 -c usbasp -u -Uflash:w:\Tiny254585Projects\Narsil\Narsil\Release\Narsil.hex:a -Ueeprom:w:\Tiny254585Projects\Narsil\Narsil\Release\Narsil.eep:a
