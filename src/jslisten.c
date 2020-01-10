/*
 *  jslisten
 *
 *  Created by Nikolai Rinas on 27.03.15.
 *  Copyright (c) 2015 Nikolai Rinas. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libudev.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/joystick.h>

#include <assert.h>

#include "minIni.h"
#include "axbtnmap.h"

//---------------------------------
// Some static stuff
//---------------------------------
#define sizearray(a)  (sizeof(a) / sizeof((a)[0]))
#define BUTTON_UNSET -1
#define true 0
#define false 1

#define NAME_LENGTH 128
#define MYPROGNAME "jslisten"
#define myConfFile "/.jslisten"
#define myGlConfFile "/etc/jslisten.cfg"
//#define MY_LOG_LEVEL LOG_NOTICE //LOG_DEBUG //LOG_NOTICE

#define INI_BUFFERSIZE      512
#define MAX_HOTKEYS 99


//---------------------------------
// Global definitions
//---------------------------------
char iniFile[INI_BUFFERSIZE];
char myDevPath[NAME_LENGTH];
int joyFD;
struct KeySet {
  // Set Default unassigned
  char swFilename[100];
  long delay;
  long button1;
  unsigned long button1Active;
  long button2;
  unsigned long button2Active;
  long button3;
  unsigned long button3Active;
  long button4;
  unsigned long button4Active;
  int hotKeyCount;
  int isTriggered;
};

struct KeySet myKeys[MAX_HOTKEYS];

int delayedSection = -1;
int hotKeyCombos = 0;
int logLevel = LOG_INFO;

// recognized values for --loglevel
#define LVL_DEBUG_STR "debug"
#define LVL_NOTICE_STR  "notice"

// recognized values for --mode
#define MODE_PLAIN_STR "plain"
#define MODE_HOLD_STR  "hold"

static struct option long_options[] = {
  {"device", required_argument, 0, 'd'},
  {"help", no_argument, NULL, 'h'},
  {"loglevel", required_argument, 0, 'l'},
  {"mode", required_argument, 0, 'm'},
  {0, 0, 0, 0}
};

typedef enum {PLAIN, HOLD} mode_type;
mode_type mode = PLAIN;

unsigned long getMicrotime(){
  struct timeval currentTime;
  gettimeofday(&currentTime, NULL);
  return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

//---------------------------------------------
// Reset the keys
//---------------------------------------------
void resetHotkeys() {
  syslog(LOG_DEBUG, "Reseting Keys");
  for (int i=0; i<hotKeyCombos; i++) {
    myKeys[i].button1Active = 0;
    myKeys[i].button2Active = 0;
    myKeys[i].button3Active = 0;
    myKeys[i].button4Active = 0;
    myKeys[i].isTriggered = false;
  }
  delayedSection = -1;
}

//---------------------------------
// Check if the button was assigned
//---------------------------------
int buttonDefined(int val) {
  if ( val > BUTTON_UNSET ) {
    return true;
  } else {
    return false;
  }
}

//---------------------------------
// get the configuration file
//---------------------------------
int getConfigFile() {
  int rc=1; // Default nothing found
  // Determine home dir
  if ( getenv("HOME") != NULL ) {
    syslog(LOG_INFO, "taking user config %s\n", iniFile);
    strncat(strncpy(iniFile, getenv("HOME"), INI_BUFFERSIZE-1), myConfFile, INI_BUFFERSIZE-1);
    // Look for personal file
    if( access( iniFile, R_OK ) != -1 ) {
      // file exists
      rc = 0;
      syslog(LOG_INFO, "reading config %s\n", iniFile);
    }
  }
  if ( rc > 0 ) {
    // file doesn't exist, check global
    if( access( myGlConfFile, R_OK ) != -1 ) {
      strncpy(iniFile, myGlConfFile, INI_BUFFERSIZE-1);
      rc = 0;
      syslog(LOG_INFO, "reading config %s\n", iniFile);
    } else {
      // Write a default file to the home dir
      FILE *f = fopen(iniFile, "w");
      if (f == NULL) {
        syslog(LOG_ERR, "err: failed write config file %s\n", myConfFile);
      } else {
        const char *defaultConfig = "[Generic]\nprogram=\nbutton1=\nbutton2=\nbutton3=\nbutton4=\n";
        fprintf(f, "%s\n", defaultConfig);
        syslog(LOG_ERR, "err: no config found. Please maintain all required values in %s\n", iniFile);
      }
    }
  }
  return rc;
}

//---------------------------------------
// Get configuration items from the file
//---------------------------------------
void readConfig(void) {
  char str[100];
  int s, k;
  char section[50];
  long l;
  int n;

  /* section/key enumeration */
  for (s = 0; ini_getsection(s, section, sizearray(section), iniFile) > 0; s++) {
    if ( hotKeyCombos < MAX_HOTKEYS ) {
      for (k = 0; ini_getkey(section, k, str, sizearray(str), iniFile) > 0; k++) {

        if ( strncmp("program", str, 7) == 0 ) { 
          n = ini_gets(section, str, "dummy", myKeys[hotKeyCombos].swFilename, sizearray(myKeys[hotKeyCombos].swFilename), iniFile);
          if ( n > 5 && strncmp("dummy", myKeys[hotKeyCombos].swFilename, 5) != 0 ) { // Value is not empty
            syslog(LOG_INFO, "Filename: %s\n", myKeys[hotKeyCombos].swFilename);
          }
        }

        if ( strncmp("delay", str, 5) == 0 ) { 
          l = ini_getl(section, str, 0, iniFile);
          syslog(LOG_INFO, "Delay: %ld\n", l);
          myKeys[hotKeyCombos].delay = l * 1000000;
        }


        if ( strncmp("button1", str, 7) == 0 ) { 
          l = ini_getl(section, str, BUTTON_UNSET, iniFile);
          if ( buttonDefined(l) == true ) { // Value is not empty
            syslog(LOG_INFO, "button1: %ld\n", l);
            myKeys[hotKeyCombos].button1 = l;
            myKeys[hotKeyCombos].hotKeyCount++;
          }
        }

        if ( strncmp("button2", str, 7) == 0 ) { 
          l = ini_getl(section, str, BUTTON_UNSET, iniFile);
          if ( buttonDefined(l) == true ) { // Value is not empty
            syslog(LOG_INFO, "button2: %ld\n", l);
            myKeys[hotKeyCombos].button2 = l;
            myKeys[hotKeyCombos].hotKeyCount++;
          }
        }

        if ( strncmp("button3", str, 7) == 0 ) { 
          l = ini_getl(section, str, BUTTON_UNSET, iniFile);
          if ( buttonDefined(l) == true ) { // Value is not empty
            syslog(LOG_INFO, "button3: %ld\n", l);
            myKeys[hotKeyCombos].button3 = l;
            myKeys[hotKeyCombos].hotKeyCount++;
          }
        }

        if ( strncmp("button4", str, 7) == 0 ) { 
          l = ini_getl(section, str, BUTTON_UNSET, iniFile);
          if ( buttonDefined(l) == true ) { // Value is not empty
            syslog(LOG_INFO, "button4: %ld\n", l);
            myKeys[hotKeyCombos].button4 = l;
            myKeys[hotKeyCombos].hotKeyCount++;
          }
        }
      } /* for */
    }
    hotKeyCombos++; // Remember how many sections we have
  } /* for */
}

//---------------------------------------------
// Validity check of the provided config items
//---------------------------------------------
int checkConfig(void) {
  int rc=0;
  for (int i=0; i<hotKeyCombos; i++) {
    if ( sizearray(myKeys[i].swFilename) < 3 ) { // no program make no sense
      syslog(LOG_ERR, "err: no valid filename provided in section %d. Please check ini file\n", i);
      rc = 1;
    }
    if ( buttonDefined(myKeys[i].button1) == false ) { // we need at least one button for tracking
      syslog(LOG_ERR, "err: button assignment missing in section %d. Please set at least button1 in the ini file!\n", i);
      rc = 1;
    }
    syslog(LOG_INFO, "Active assigned buttons in section %d: ", i);
    syslog(LOG_INFO, "%d\n", myKeys[i].hotKeyCount);

  }
  return rc;
}

//---------------------------------------------
// Check button pressed
//---------------------------------------------
int checkButtonPressed(struct js_event js) {
  int section = -1;
  int i;

  // Update the button press in all key combinations
  syslog(LOG_DEBUG, "Checking button state");
  for (i=0; i<hotKeyCombos; i++) {
    if ( js.number == myKeys[i].button1 ) {
      if (js.value == 1) {
        if ( myKeys[i].button1Active == 0 ) { myKeys[i].button1Active = getMicrotime(); }
      } else { 
        myKeys[i].button1Active = 0; 
        if (i == delayedSection) { delayedSection = -1; }
      }
    } else if ( js.number == myKeys[i].button2 ) {
      if (js.value == 1) {
        if ( myKeys[i].button2Active == 0 ) { myKeys[i].button2Active = getMicrotime(); }
      } else { myKeys[i].button2Active = 0; }
    } else if ( js.number == myKeys[i].button3 ) {
      if (js.value == 1) {
        if ( myKeys[i].button3Active == 0 ) { myKeys[i].button3Active = getMicrotime(); }
      } else { myKeys[i].button3Active = 0; }
    } else if ( js.number == myKeys[i].button4 ) {
      if (js.value == 1) {
        if ( myKeys[i].button4Active == 0 ) { myKeys[i].button4Active = getMicrotime(); }
      } else { myKeys[i].button4Active = 0; }
    }
  }

  // Analyse combinations
  for (i=0; i<hotKeyCombos; i++) {
    switch (myKeys[i].hotKeyCount) {
    case 1:
      if ( myKeys[i].button1Active > 0  ) { 
        unsigned long hold_time = getMicrotime() - myKeys[i].button1Active;
        syslog(LOG_DEBUG, "Delayed: %ld\n", hold_time);
        if (myKeys[i].delay == 0 || hold_time >= myKeys[i].delay) {
          myKeys[i].isTriggered = 1; 
          section = i; 
          delayedSection = -1;
        } else {
          delayedSection = myKeys[i].delay > 0 ? 1 : delayedSection;
        }
      } 
      break;
    case 2:
      if ( myKeys[i].button1Active > 0 && myKeys[i].button2Active > 0 ) {
        myKeys[i].isTriggered = 1; 
        section = i;
      }
      break;
    case 3:
      if ( myKeys[i].button1Active > 0 && myKeys[i].button2Active > 0 && myKeys[i].button3Active > 0 ) {
        myKeys[i].isTriggered = 1; 
        section = i;
      }
      break;
    case 4:
      if ( myKeys[i].button1Active > 0 && myKeys[i].button2Active > 0 
          && myKeys[i].button3Active > 0 && myKeys[i].button4Active > 0 ) {
        myKeys[i].isTriggered = 1; 
        section = i;
      }
      break;
    }
  }
  return section;
}

//---------------------------------------------
// Get the input device
//---------------------------------------------
void listenJoy (void) {
  /* Listen to the input devices and try to recognize connected joystick */
  /* This function is basically a copy of a udev example */
  struct udev *udev;
  struct udev_device *dev;
  struct udev_device *mydev;
  struct udev_monitor *mon;
  int fd;
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;

  // Clear previous joystick
  joyFD = -1;  

  /* Create the udev object */
  udev = udev_new();
  if (!udev) {
    syslog (LOG_ERR, "Can't create udev\n");
    exit(1);
  }

  // Check if a joypad is connected already ...

  /* Create a list of the devices in the 'hidraw' subsystem. */
  enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "input");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);

  udev_list_entry_foreach(dev_list_entry, devices) {
    const char* name;
    const char* sysPath;
    const char* devPath;

    name = udev_list_entry_get_name(dev_list_entry);
    mydev = udev_device_new_from_syspath(udev, name);
    sysPath = udev_device_get_syspath(mydev);
    devPath = udev_device_get_devnode(mydev);

    if (sysPath != NULL && devPath != NULL && strstr(sysPath, "/js") != 0) {
      syslog (LOG_NOTICE, "Found Device: %s\n", devPath);
      if (joyFD < 0 || strcmp(devPath, myDevPath) == 0) {
        // Open the file descriptor
        if ((joyFD = open(devPath, O_RDONLY)) < 0) { 
          syslog (LOG_INFO, "error: failed to open fd\n");
        } else {
          syslog (LOG_NOTICE, "Watching: %s\n", devPath);
        }
      }
    }

    udev_device_unref(mydev);
  }
  /* cleanup */
  udev_enumerate_unref(enumerate);

  // Still no joystick found
  if ( joyFD < 0 ) { 
    syslog (LOG_NOTICE, "No devices found\n");

    /* Set up a monitor to monitor input devices */
    mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "input", NULL);
    udev_monitor_enable_receiving(mon);
    /* Get the file descriptor (fd) for the monitor.
       This fd will get passed to select() */
    fd = udev_monitor_get_fd(mon);


    /* This section will run continuously, calling usleep() at
       the end of each pass. This is to demonstrate how to use
       a udev_monitor in a non-blocking way. */
    while (joyFD<0) {
      /* Set up the call to select(). In this case, select() will
         only operate on a single file descriptor, the one
         associated with our udev_monitor. Note that the timeval
         object is set to 0, which will cause select() to not
         block. */
      fd_set fds;
      struct timeval tv;
      int ret;

      FD_ZERO(&fds);
      FD_SET(fd, &fds);
      tv.tv_sec = 0;
      tv.tv_usec = 0;

      ret = select(fd+1, &fds, NULL, NULL, &tv);

      /* Check if our file descriptor has received data. */
      if (ret > 0 && FD_ISSET(fd, &fds)) {
        syslog(LOG_DEBUG,"\nselect() says there should be data\n");

        /* Make the call to receive the device.
           select() ensured that this will not block. */
        dev = udev_monitor_receive_device(mon);
        if (dev) {
          syslog(LOG_DEBUG, "Got Device\n");
          syslog(LOG_DEBUG, "   Node: %s\n", udev_device_get_devnode(dev));
          syslog(LOG_DEBUG, "   Subsystem: %s\n", udev_device_get_subsystem(dev));
          syslog(LOG_DEBUG, "   Devtype: %s\n", udev_device_get_devtype(dev));
          syslog(LOG_DEBUG, "   Action: %s\n",udev_device_get_action(dev));

          if ( strncmp(udev_device_get_action(dev), "add", 3) == 0 ) { // Device added
            /* enumerate joypad devices */
            /* Create a list of the devices in the 'input' subsystem. */
            enumerate = udev_enumerate_new(udev);
            udev_enumerate_add_match_subsystem(enumerate, "input");
            udev_enumerate_scan_devices(enumerate);
            devices = udev_enumerate_get_list_entry(enumerate);

            udev_list_entry_foreach(dev_list_entry, devices) {
              const char* name;
              const char* sysPath;
              const char* devPath;

              name = udev_list_entry_get_name(dev_list_entry);
              mydev = udev_device_new_from_syspath(udev, name);
              sysPath = udev_device_get_syspath(mydev);
              devPath = udev_device_get_devnode(mydev);

              if (sysPath != NULL && devPath != NULL && strstr(sysPath, myDevPath) != 0) {
                syslog(LOG_NOTICE, "Found Device: %s\n", devPath);
                if ((joyFD = open(devPath, O_RDONLY)) < 0) { // Open the file descriptor
                  syslog(LOG_INFO, "error: failed to open fd\n");
                }
              }

              udev_device_unref(mydev);
            }
            /* cleanup */
            udev_enumerate_unref(enumerate);
          } else {
            if ( strncmp(udev_device_get_action(dev), "remove", 6) == 0 ) { // Device remove
              if ( joyFD >= 0 ) {
                close(joyFD);
              }
              joyFD = -1; // Reset
            }
          }
        } else {
          syslog(LOG_WARNING, "No Device from receive_device(). An error occured.\n");
        }
        udev_device_unref(dev);
      }
      usleep(250*1000);
      syslog(LOG_DEBUG, ".");
      fflush(stdout);
    }
  }

  udev_unref(udev);

}

//---------------------------------------------
// check if any of the n-1 buttons is released.
//
// only to be called when js.value is 0. if the release event relates to
// one of the n-1 buttons the "engaged" level will be left.
//
// returns 1 if all but the last buttons are no longer pressed or
//           if only one button is defined to trigger an action
// returns 0 if any of the n-1 is still pressed
//---------------------------------------------
int button_held(int js_btn_number, int button_set_idx) {
  int activeKeys = myKeys[button_set_idx].hotKeyCount - 1;
  switch (activeKeys) {
  case 1:
    return js_btn_number == myKeys[button_set_idx].button1;
  case 2:
    return js_btn_number == myKeys[button_set_idx].button1
           || js_btn_number == myKeys[button_set_idx].button2;
  case 3:
    return js_btn_number == myKeys[button_set_idx].button1
           || js_btn_number == myKeys[button_set_idx].button2
           || js_btn_number == myKeys[button_set_idx].button3;
  default:
    return 1;
  }
}

//---------------------------------------------
// Listen on the input and call the program
//---------------------------------------------
int bindJoy(void) {

  char *axis_names[ABS_MAX + 1] = { "X", "Y", "Z", "Rx", "Ry", "Rz", "Throttle", "Rudder",
                                    "Wheel", "Gas", "Brake", "?", "?", "?", "?", "?",
                                    "Hat0X", "Hat0Y", "Hat1X", "Hat1Y", "Hat2X", "Hat2Y", "Hat3X", "Hat3Y",
                                    "?", "?", "?", "?", "?", "?", "?",};

  char *button_names[KEY_MAX - BTN_MISC + 1] = {
    "Btn0", "Btn1", "Btn2", "Btn3", "Btn4", "Btn5", "Btn6", "Btn7", "Btn8", "Btn9", "?", "?", "?", "?", "?", "?",
    "LeftBtn", "RightBtn", "MiddleBtn", "SideBtn", "ExtraBtn", "ForwardBtn", "BackBtn", "TaskBtn", "?", "?", "?", "?", "?", "?", "?", "?",
    "Trigger", "ThumbBtn", "ThumbBtn2", "TopBtn", "TopBtn2", "PinkieBtn", "BaseBtn", "BaseBtn2", "BaseBtn3", "BaseBtn4", "BaseBtn5", "BaseBtn6", "BtnDead",
    "BtnA", "BtnB", "BtnC", "BtnX", "BtnY", "BtnZ", "BtnTL", "BtnTR", "BtnTL2", "BtnTR2", "BtnSelect", "BtnStart", "BtnMode", "BtnThumbL", "BtnThumbR", "?",
    "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?",
    "WheelBtn", "Gear up",
  };

  unsigned char axes = 2;
  unsigned char buttons = 2;
  int version = 0x000800;
  char name[NAME_LENGTH] = "Unknown";
  uint16_t btnmap[BTNMAP_SIZE];
  uint8_t axmap[AXMAP_SIZE];
  int btnmapok = 1;
  int i;

  ioctl(joyFD, JSIOCGVERSION, &version);
  ioctl(joyFD, JSIOCGAXES, &axes);
  ioctl(joyFD, JSIOCGBUTTONS, &buttons);
  ioctl(joyFD, JSIOCGNAME(NAME_LENGTH), name);

  getaxmap(joyFD, axmap);
  getbtnmap(joyFD, btnmap);

  syslog(LOG_INFO, "Driver version is %d.%d.%d.\n",
         version >> 16, (version >> 8) & 0xff, version & 0xff);

  /* Determine whether the button map is usable. */
  for (i = 0; btnmapok && i < buttons; i++) {
    if (btnmap[i] < BTN_MISC || btnmap[i] > KEY_MAX) {
      btnmapok = 0;
      break;
    }
  }
  if (!btnmapok) {
    /* btnmap out of range for names. Don't print any. */
    syslog(LOG_WARNING, "jslisten is not fully compatible with your kernel. Unable to retrieve button map!\n");
    syslog(LOG_INFO, "Joystick (%s) has %d axes ", name, axes);
    syslog(LOG_INFO, "and %d buttons.\n", buttons);
  } else {
    syslog(LOG_INFO, "Joystick (%s) has %d axes (", name, axes);
    for (i = 0; i < axes; i++) {
      syslog(LOG_INFO, "%s%s", i > 0 ? ", " : "", axis_names[axmap[i]]);
    }
    syslog(LOG_INFO, ")\n");

    syslog(LOG_INFO, "and %d buttons (", buttons);
    for (i = 0; i < buttons; i++) {
      syslog(LOG_INFO, "%s%s", i > 0 ? ", " : "", button_names[btnmap[i] - BTN_MISC]);
    }
    syslog(LOG_INFO, ").\n");
  }

  // Non-blocking reading
  struct js_event js;
  int triggerSection;
  int lastTriggeredSet = -1;
  fcntl(joyFD, F_SETFL, O_NONBLOCK);

  while (1) {
    while (read(joyFD, &js, sizeof(struct js_event)) == sizeof(struct js_event) || delayedSection >= 0) {
      syslog(LOG_DEBUG, "Button Event");
      if (js.type == JS_EVENT_BUTTON) {
        syslog(LOG_DEBUG, "Event: type %d, time %d, number %d, value %d\n",
               js.type, js.time, js.number, js.value);
        if (mode == HOLD && js.value == 0 && lastTriggeredSet > -1 && button_held(js.number, lastTriggeredSet)) {
          resetHotkeys();
          lastTriggeredSet = -1;
        }
        triggerSection = checkButtonPressed(js);
        if ( triggerSection > -1 ) {   
          // We have found one key section
          if (mode == HOLD) {
            // set to "engaged" mode (lastTriggeredSet > -1) and "fire" command once
            lastTriggeredSet = triggerSection;
          }
          syslog(LOG_INFO, "Swtching mode. ...\n");
          // call external program
          int rc = system(myKeys[triggerSection].swFilename);
          if ( rc == 0 ) {
            syslog(LOG_INFO, "Call succesfull\n");
          } else {
            syslog(LOG_INFO, "Call failed\n");
          }
          if (mode == PLAIN) {
            // reset state, so we call only once
            resetHotkeys();
          }
        }
      }
    }

    if (errno != EAGAIN) {
      syslog(LOG_DEBUG, "\njslisten: error reading"); // Regular exit if the joystick disconnect
      return 1;
    }

    usleep(10000);
  }
  return 0;
}


//---------------------------------------------
// Exit function
//---------------------------------------------
void signal_callback_handler(int signum) {
  syslog(LOG_NOTICE, "Exit. Caught signal %d\n",signum);
  // Cleanup and close up stuff here

  closelog();

  // Terminate program
  exit(0);
}


void usage() {
  printf("jslisten [<options>], where <options> are:\n");
  printf("  --device <path>\t use explicit game controller path e.g. /dev/input/by-id/... instead of first one found.\n");
  printf("  --loglevel <level>\t defines the minimum syslog log level. <level> is one of '%s' or '%s'. Defaults to 'info'.\n", LVL_DEBUG_STR, LVL_NOTICE_STR);
  printf("  --mode <mode>\t\t defines the button trigger behaviour. <mode> is either '%s' or '%s'. Defaults to '%s'\n", MODE_PLAIN_STR, MODE_HOLD_STR, MODE_PLAIN_STR);
  printf("  --help\t\t print this help and exit\n\n");
  printf("For more information please visit: https://github.com/workinghard/jslisten\n");
}

//---------------------------------------------
// parse the command line parameters
//---------------------------------------------
void parse_command_line(int argc, char* argv[]) {
  int c;
  while (1) {
    int option_index = 0;

    c = getopt_long (argc, argv, "hl:d:m:", long_options, &option_index);

    if (c == -1) {
      break;
    }

    switch (c) {
    case 'h':
      usage();
      exit(0);
    case 'l':
      if (strncmp(optarg, LVL_DEBUG_STR, strlen(LVL_DEBUG_STR)) == 0) {
        logLevel = LOG_DEBUG;
      } else if ((strncmp(optarg, LVL_NOTICE_STR, strlen(LVL_NOTICE_STR)) == 0)) {
        logLevel = LOG_NOTICE;
      }
      break;
    case 'd':
      if (strlen(optarg) < NAME_LENGTH) {
        strncpy(myDevPath, optarg, NAME_LENGTH-1);
      } else {
        syslog(LOG_WARNING, "--device <path> parameter too long. Using default.\n");
      }
      break;
    case 'm':
      if (strncmp(optarg, MODE_HOLD_STR, strlen(MODE_HOLD_STR)) == 0) {
        mode = HOLD;
      }
      if (strncmp(optarg, MODE_PLAIN_STR, strlen(MODE_PLAIN_STR)) == 0) {
        mode = PLAIN;
      } else {
        syslog(LOG_WARNING, "--mode %s parameter unknown. Using default.\n", optarg);

      }
      break;
    case '?':
      syslog(LOG_ERR, "Misconfigured command line parameters. Exiting. ...\n");
      exit(1);
    default:
      abort ();
    }
  }
}


//---------------------------------------------
// init button keysets to defaults
//---------------------------------------------
void init_button_keysets() {
  for (int i=0; i<MAX_HOTKEYS; i++) {
    myKeys[i].button1 = BUTTON_UNSET;
    myKeys[i].button1Active = 0;
    myKeys[i].button2 = BUTTON_UNSET;
    myKeys[i].button2Active = 0;
    myKeys[i].button3 = BUTTON_UNSET;
    myKeys[i].button3Active = 0;
    myKeys[i].button4 = BUTTON_UNSET;
    myKeys[i].button4Active = 0;
    myKeys[i].hotKeyCount = 0;
    myKeys[i].isTriggered = false;
  }
}

//---------------------------------------------
// main function
//---------------------------------------------
int main(int argc, char* argv[]) {
  int rc;
  // Register signal and signal handler
  signal(SIGINT, signal_callback_handler);
  signal(SIGKILL, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);
  signal(SIGHUP, signal_callback_handler);

  // Default device path to check
  strncpy(myDevPath, "/js", NAME_LENGTH-1);

  // Open Syslog
  openlog (MYPROGNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

  parse_command_line(argc, argv);

  setlogmask (LOG_UPTO (logLevel));

  syslog(LOG_NOTICE, "Using device path: %s\n", myDevPath);
  syslog(LOG_NOTICE, "Using button mode: %s\n", mode == PLAIN ? MODE_PLAIN_STR : MODE_HOLD_STR);

  syslog(LOG_NOTICE, "Listen to joystick inputs ...\n");

  // Get the configuration file
  rc = getConfigFile();
  if ( rc > 0 ) {
    return rc;
  }

  // Read the configuration
  readConfig();

  // Check if we have everything
  rc = checkConfig();
  if ( rc > 0 ) {
    return rc;
  }

  // If everything is set up, run ...
  if ( rc == 0 ) {
    // Main endless loop
    while (1) {
      listenJoy(); // Find our joystick
      if ( joyFD > 0 ) {
        bindJoy(); // If found, use it and listen to the keys
      }
    }
  }
  return 0;
}
