# ReiNand
*The original open source N3DS CFW ..Now with O3DS support!*


**Compiling:**

You'll need armips added to your Path. [HERE](https://www.dropbox.com/s/ceuv2qeqp38lpah/armips.exe?dl=0) is a pre-compiled version.

    make - Compiles All. (launcher and a9lh)
    make launcher - Compiles CakeHax/CakeBrah payload
    make a9lh - Compiles arm9loaderhax payload

Copy everything in 'out' folder to SD root and run!


**Features:**

* Ninjhax and MSET support!

* Sig checks disabled

* RAM dump (edit RAM.txt with a base 10 number for offset) [Start Button + X]

* Emunand (with 'Rei' version string)

* Compatibility with arm9loaderhax
 

**Credits:**
 
 Cakes team for teaching me a few things and just being helpful in general! And for ROP/mset related code, and crypto libs.
    
 3DBREW for saving me plenty of reverse engineering time.
    
 Patois/Cakes for CakesBrah.
 
 Normmatt for sdmmc.c and generally being helpful!
 
 AuroraWright for being helpful!
    
 Me (Rei) for coding everything else.
 
 The community for your support and help!
 