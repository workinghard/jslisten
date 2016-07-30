# jslisten

## Index
 1. [Overview](#overview)
 2. [Usage](#usage)
 3. [Installation](##installation)
 4. [Configuration](##configuration)
 5. [Known limitations](##known_limitations)

## Overview

This program listen in the background for gamepad inputs. If a special button combination is getting pressed
the provided command line will be invoked. It runs a command once defined buttons were pressed.

## Usage

I have a Raspberry Pi. The installed operation system is raspbian. The Kodi package runs pretty well as a media center. The remote control is an App on a smartphone. So no other controller is needed. 
After couple of month I found this great [RetroPie](https://retropie.org.uk) project. There is also another great one which brings it on a software level instead making a boot image. It's called (EmulationStation)[http://www.emulationstation.org]. The setup with my preferred PS3 controller and the configuration went well and it worked fantastic. The game console works perfect and brings me back to the good old jump'n'run 90s but now with big screen and cool wireless controller!

But one thing was an issue. I wanted to use the same hardware and don't miss my system (raspbian). Dual boot is not cool and takes too long. So basically what i needed is some kind of trigger to end any X11 processes and start Retropie

Main purpose for this development is a switcher between Kodi and Retropie with the PS3 controller.


## Installation

Following example for Raspbian. Should work for many other distributions almost the same way.
 * Use the precompiled binary in bin/ or run "# make" to create the binary
 * Place the binary to "/opt/bin" (if you change the folder, please update your init script) 
 * Copy the init script "utils/jslisten-init" to "/etc/init.d/jslisten"
 * Execute "# update-rc.d jslisten defaults" to make the daemon run on startup

## Configuration


## Known limitations

 * If you have many different /dev/inputs, you might adjust this program to search for special one.

