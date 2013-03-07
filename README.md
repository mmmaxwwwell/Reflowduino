#A fork of the source code for the "Reflowduino"

##You give it XY coordinates (time in seconds, temperature in celcius) and it does the rest.

I bought a black and decker toaster oven at my local walmart for ~$40. It has 2 upper and 2 lower heating elements and a fan on the inside. A simple bi-metal switch in series with the heating elements controlls the temperature.
The label says 1500 watts but my measurements (clip on amprobe) put it at 1200. "60V 300W" is visible on the ceramic part of the bottom two heating elements. The top two do not have any markings visible.

I've got a cold resistance of ~23 ohms across the 2 series bottom heating elements and ~16 ohms aross the top two. This puts the bottom heating elements in the 600 watt range, consistent with their markings. The top elements are a bit more powerful and should draw about 900 watts.

I took it apart, drew up a schematic, and replaced the controls with a [4 channel relay](http://www.emartee.com/product/41894/). We are using an [Adafruit Max31855](http://www.adafruit.com/products/269) and [K type thermocouple](http://www.adafruit.com/products/270) for temperature readings.

A bit of tweaking and some guessing gets us a (mostly) working oven that preheats, waits for input from the user, then executes its cycle.

![test](http://github.com/mmmaxwwwell/reflowduino/raw/master/sermon.png)

![test](http://github.com/mmmaxwwwell/reflowduino/raw/master/graph.png)


## Test 1: mount a QFN20 attiny85 , SOT25 voltage regulator and 0201 resistor
I ordered some boards from batchpcb.com and the matching components from digikey.

I started by placing a liberal amount of solder paste over all the pads including the center of the 20QFN. I then carefully placed and aligned each piece with tweezers and a magnifier.
With paste and chips on board it was time to cook. I put the boards in the oven and set it to run. At 150C I started to notice small bubbles coming from under the 20QFN chips. On a few of the boards the bubbling was violent enough to knock the chip out of place. The rest of the 20QFNs floated ~2mm out of place before landing in the wrong spot. All "floaters" seemed to lift off around 210C. The other components stayed in place and seemed to make good connections. The whole process took about 4 to 4 and a half mins, and left me with 0 usable boards.

So what went wrong? After a bit of googleing and help from my friend pete I have 3 main ideas
1. The reflow profile I copied was probably designed for professional machines. My $40 toaster oven can't achieve the same deg/sec rise the profesional ovens can, so its always catching up, and will over/undershoot its targets.
Solution: Increase the profile duration making the deg/sec goals achievable.

2. Bubbling under the 20QFN chips. I think this is supposed to happen at a slower rate and during the soak phase so its more gentle and doesn't knock the chips around.
Soultion: Increase preheat temperature from 100C to 150C and increase duration of soak. This may also be fixed by the solution to #3.

3. 20QFN chips floating away. I think this was caused by too much paste on the center pad. 
Solution: Don't put paste on the center pad

This is a work in progress, expect to see more!

#Thanks goes to:
##Ladyada for the [Reflowduino](https://github.com/adafruit/Reflowduino)
##Martin Nawrath for his [nonlinear mapping algorithm](http://interface.khm.de/index.php/lab/experiments/nonlinear-mapping/)
##Dave Berkeley for his [50hz triac signal generator](http://www.rotwang.co.uk/projects/triac.html)
