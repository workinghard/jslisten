## Update 12/22/2018

If you have many different /dev/inputs, you can pass it as an arguement at the startup:

```jslisten --device /dev/input/js0```

** jslisten will also default to listen to the first device found if the defined device is not found.

## Update 02/12/2019

Changed the default service user to "pi" in jslisten.service

Use user root only if it's really required.

## Update 02/16/2019

Added a different mode to easier trigger scripts.

When started with `--mode plain` (the default) the program behaves as used. This is fine if you need the button combination once in a while.

However if you want to activate a script a few times in a short time, lets say to adjust your audio volume or if you have a led stripe installed in your arcade and want to adjust the brightness, then
the `--mode hold` comes in handy.

When started with `--mode hold` and you have at least two buttons defined to enter an action, then you hold down the first one, while tipping the second one. If you have three buttons defined you can
hold the second two and then just tip the third. In a four button set you will have to hold the first three.

As soon as you release any but the last button (according to the order in the `jslisten.cfg`) you will have to press the all buttons again.

So the difference to the plain mode is that once you have entered the engaged/elevated state with a three button set for example, you hold down the first two and can quickly trigger the action by
pressing the third. In plain mode you would have to press all three buttons over again to do audio or brightness adjustment.

Also added `--help` for the command line for a brief summary of options.