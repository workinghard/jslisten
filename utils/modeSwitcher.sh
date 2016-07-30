#!/bin/bash 
#
# Switch between Kodi and Retropie
#

KODISERVICE="/usr/bin/kodi"
KODIINIT="/etc/init.d/kodi"
X11SERVICE="/usr/sbin/lightdm"
X11INIT="/etc/init.d/lightdm"
RETROSERVICE="emulationstation"
RETROINIT="/etc/init.d/retropie"

KODIRUN="FALSE"
X11RUN="FALSE"
RETRORUN="FALSE"

check_kodi() {
  if ps ax | grep -v grep | grep $KODISERVICE > /dev/null ; then
    KODIRUN="TRUE"
  else
    KODIRUN="FALSE"
  fi
}

check_lightdm() {
  if ps ax | grep -v grep | grep $X11SERVICE > /dev/null ; then
    X11RUN="TRUE"
  else
    X11RUN="FALSE"
  fi
}

check_retropie() {
  if ps ax | grep -v grep | grep $RETROSERVICE > /dev/null ; then
    RETRORUN="TRUE"
  else
    RETRORUN="FALSE"
  fi
}

check_kodi
check_lightdm
check_retropie

# Switch to Kodi
if [ $RETRORUN == "TRUE" ];then
  # Stop Retropie service
  $RETROINIT stop 
  # Only if the service was shut down properly
  if [ $? == 0 ]; then
    # Start lightdm
    #$X11INIT start
    # Start kodi
    chmod g-rw /dev/input/*
    $KODIINIT start
  fi
  exit 0
else
  # Shut down lightdm if running
  if [ $X11RUN == "TRUE" ]; then
    $X11INIT stop
    check_lightdm
  fi
  # Shut down kodi if running
  if [ $KODIRUN == "TRUE" ]; then
    $KODIINIT stop
    check_kodi
  fi
  # If nothing else is running, start Retropie
  if [ $X11RUN == "FALSE" ] && [ $KODIRUN == "FALSE" ]; then
    chmod g+rw /dev/input/*
    $RETROINIT start
  fi
  exit 0
fi


