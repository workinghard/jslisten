# jslisten

## Index
 1. [Overview](#overview)
 2. [Requirements](#requirements)
 3. [Installation](##installation)

## Overview

This program listen in the background for gamepad inputs. If a special button combination is getting pressed
the provided command line will be invoked.

It's basically a copy of jstest and udev example implementations. 

## 

Main purpose for this development is a switcher between Kodi and Retropie with the PS3 controller. 
## Requirements

If you have many different /dev/inputs, you might adjust this program to search for special one.

## Installation
Following example for Raspbian. Should work for many other distributions almost the same way.
 * Use the precompiled binary in /bin or run "# make" to get the binary
 * Place the binary to "/opt/bin" (if you change the folder, please update your init script) 
 * Copy the init script "utils/jslisten-init" to "/etc/init.d/jslisten"
 * Execute "# update-rc.d jslisten defaults" to make the daemon run on startup
