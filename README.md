# jslisten

## Index
 1. [Overview](#overview)
 2. [Usage](#usage)
 3. [Installation](#installation)
 4. [Configuration](#configuration)
 5. [Known limitations](#known-limitations)
 6. [Multiple key combinations](#multiple-key-combinations)
 7. [Version Updates](UPDATES.md)

## Overview

This program listens in the background for gamepad inputs. If a special button combination is getting pressed,
the provided command line will be invoked. It runs a command once defined buttons were pressed. It was developed for Raspbian and PS3 wireless controller but should work everywhere with `udev` and [joystick](https://sourceforge.net/projects/linuxconsole/) support.

### Example Use Case

I have a Raspberry Pi. The installed operation system is raspbian. The Kodi package runs pretty well as a media center. The remote control is an App on a smartphone. So no other controller is needed.

After couple of month I found this great [RetroPie](https://retropie.org.uk) project. There is also another great one which brings it on a software level instead making a boot image. It's called [EmulationStation](http://www.emulationstation.org). The setup with my preferred PS3 controller and the configuration went well and it worked fantastic. The game console works perfectly fine and brings me back to the good old jump'n'run 90s but now with big screen and cool wireless controller!

But one thing was an issue. I wanted to use the same hardware and don't miss my system (raspbian). Dual boot is not cool and takes too long. So basically what i needed is some kind of trigger to end any X11/Kodi processes and start EmulationStation ... and of course vice versa.

In my configuration Kodi runs as default. Media center is still the primary usage. But the idea was to take the PS3 controller press some button combination and the EmulationStation will apear. After I played enough, just press this buttons again and media center will apear and I can put the PS3 controller back.

After searching the internet, I found nothing really interesting. Kodi addon will not work because gamepad support is still missing. EmulationStation addon might work but than ... Kodi solution will be missing. So I had to go one level up to the OS. But even here I found nothing really easy to setup or working. So I decided to write this program. It runs as a daemon in the background and listening in nonblocking mode to /dev/input/js* devices. To make it hotplug capable, it's monitoring the udev signals if no device is present.


## Installation

### Quick Install

```
cd /tmp
git clone git@github.com:workinghard/jslisten.git
cd jslisten

echo "Building jslisten binary"
make

echo "Moving jslisten binary to executable path"
sudo mkdir -p /opt/bin
sudo cp bin/jslisten /opt/bin/jslisten

echo "Copying default config; you will need to modify this file for your use case."
cp /etc/jslisten.cfg ~/.jslisten

echo "Adding jslisten support at boot"
sudo cp utils/jslisten.service /etc/systemd/system
sudo systemctl daemon-reload
sudo systemctl start jslisten.service
sudo systemctl enable jslisten
```

You will need to modify your `~/.jslisten` config file based on your own requirements.

### Step by step

Following example for Raspbian. Should work for many other distributions almost the same way.

#### Setting up the service

    * run `make` to create the binary
    * Place the binary in `/opt/bin`
        - If you change the folder, please update the init script
    * Copy the configuration script `cp etc/jslisten.cfg /etc/jslisten.cfg`
        - This can either live in `/etc/jslisten.cfg` or as `~/.jslisten`
    * Modify the configuration script to your needs
        - See configuration walkthrough below

#### Testing
    * At this point you should have a runable version of `jslisten` that is configured for your use case.
    * you can test it by running `jslisten --device /dev/input/js[*]`
        - where `[*]` is your deviceID (ie `/dev/input/js0`)
    * Once you get it running manually, you can set it to start at boot

#### Start at boot
    * Copy the service script `sudo cp utils/jslisten.service /etc/systemd/system`
        - make any changes to the `ExecStart` to change joystick devices (defaults to `/dev/input/js0`)
    * Update the systemd daemon `sudo systemctl daemon-reload`
    * Make it start at boot `sudo systemctl enable jslisten`
    * You can start and stop the service via
        - Start: `sudo systemctl start jslisten.service`
        - Stop: `sudo systemctl stop jslisten.service`
        - Status: `sudo systemctl status jslisten.service`

## Configuration

    * I assume your joystick setup is already done and you can see activation with `jstest /dev/input/js0`
        - There are many tutorials out there and you should not proceed if this doesn't work for you!
    * Think which button combination will work for your gamepad. You can configure up to 4 button combination. In my case i picked `Left Shoulder` + `Right Shoulder` + `SELECT`. I'm pretty sure I will not need to press this buttons at the same time for any games.
    * Get the number ID's for this buttons with `jstest` (or any other similar program)
    * Edit the configuration file (either `/etc/jslisten.cfg` or `~/.jslisten`) and maintain the button ID's and the program which you want to run.
    * Make sure this script/program can handle multiple simultanios calls! Even if the daemon will wait till this program ends, you never know... And we like to press key combinations many times, if something doesn't work immediately...

### Example Config

Note that it calls a custom shell script `/opt/bin/modeSwitcher.sh`. You will need to define something like this for your use case.

```
[Generic]
program="/opt/bin/modeSwitcher.sh"
button1=10
button2=11
button3=0
button4=
```

## Known limitations

    * Kodi and X11 are blocking `/dev/input/*` events. For X11 you can add an exception in `/usr/share/X11/xorg.conf.d/10-quirks.conf` but Kodi is ... nasty ... As long as they don't implement a nice unified unput support, my workaround is to revoke the kodi group rights to the input devices. :(
    * If you experience any issues, feel free to use `--loglevel debug` option when starting the binary (either manually or adding to the `systemctl` [reference above] call) and check syslog.

## Multiple key combinations

You can have different key sets to run different programs:

```
[Generic]
program="/opt/bin/modeSwitcher.sh"
button1=10
button2=11
button3=0
button4=

[Fun]
program="/opt/bin/haveFun.sh"
button1=0
button2=3
button3=
button4=
```
