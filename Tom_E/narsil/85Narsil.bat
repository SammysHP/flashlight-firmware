rem 85Narsil - downloads Narsil (Tiny85 e-switch UI configurable)
rem
avrdude -p attiny85 -c usbasp -u -Uflash:w:\Tiny254585Projects\Narsil\Release\Narsil.hex:a
