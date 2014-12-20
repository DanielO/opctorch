Introduction
========
This is a C port of Lukas Zeller's [Spark.IO Message Torch](https://community.spark.io/t/messagetorch-torch-fire-animation-with-ws2812-leds-message-display/2551).
It generates an [Open Pixel Controller](http://openpixelcontrol.org/) packet stream.

Build
====
Has a BSD make file but can be trivially compiled with

    cc *.c -o opctorch

Example
======
* Clone and  build [Open Pixel Control](https://github.com/DanielO/openpixelcontrol) (my fork has a few minor bug fixes)
* Create cylinder layout

    python ./layouts/make_cylinder.py --n_around 11 --n_tall 22 --radius 0.2 --height 2 >layouts/cylinder_r0.2_h2_11x22.json

* Run the GL server with

    ./bin/gl_server layouts/cylinder_r0.2_h2_11x22.json

* Run opctorch with

    ./opctorch localhost:7890

Hardware
=======
My setup uses a Beaglebone Black running [LEDscape](https://github.com/Yona-Appletree/LEDscape) to a 4m string of LEDs (60 LEDs/m)

Copyright
======
Original implementation is copyright Lukas Zeller and this is a derivative work.
INI parsing library is from [CCAN](http://git.ozlabs.org/?p=ccan;a=tree;f=ccan/ciniparser) and is released under the MIT license.


