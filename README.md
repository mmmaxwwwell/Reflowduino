#A fork of the source code for the "Reflowduino"

##The goal is to use [triacs](http://en.wikipedia.org/wiki/TRIAC) to control the heating elements in a toaster oven

##You give it XY coordinates (time in seconds, temperature in celcius) and it gives you a reflow profile.

###Ladyada's proportional-derivative controller combined with the triacs should yield very acurate temp control if we can get it working right!
###Probably should add [this](https://github.com/br3ttb/Arduino-PID-AutoTune-Library) once we're up and running.

![test](http://github.com/mmmaxwwwell/reflowduino/raw/master/sermon.png)

![test](http://github.com/mmmaxwwwell/reflowduino/raw/master/graph.png)

#Thanks goes to:
##Ladyada for the [Reflowduino](https://github.com/adafruit/Reflowduino)
##Martin Nawrath for his [nonlinear mapping algorithm](http://interface.khm.de/index.php/lab/experiments/nonlinear-mapping/)
##Dave Berkeley for his [50hz triac signal generator & the worlds coolest coffee pot](http://www.rotwang.co.uk/projects/triac.html)