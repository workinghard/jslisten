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
  
 *   You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <syslog.h>
#include <pwd.h>

#include <linux/input.h>
#include <linux/joystick.h>

#include <assert.h>
#include "minIni.h"

#include "axbtnmap.h"

//---------------------------------
// Some static stuff
//---------------------------------
#define sizearray(a)  (sizeof(a) / sizeof((a)[0]))
#define BUTTON_DEFINED_RANGE -2147483647 // sizeof(long)
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
int joyFD;
struct KeySet{
  // Set Default unassigned
  long button1;
  int button1Active;
  long button2;
  int button2Active;
  long button3;
  int button3Active;
  long button4;
  int button4Active;
  int activeButtons;
  int isTriggered;
  char swFilename[100];
};

struct KeySet myKeys[MAX_HOTKEYS];

int numHotkeys = 0;
//int buttonActive = 0;
int logLevel = LOG_NOTICE;

//---------------------------------
// Check if the button was assigned
//---------------------------------
int buttonDefined(int val) {
  if ( val > BUTTON_DEFINED_RANGE ) {
    return true;
  }else{
    return false;
  }
}

//---------------------------------
// get the configuration file
//---------------------------------
int getConfigFile() {
  int rc=1; // Default nothing found
  // Determine home dir
  strcat(strcpy(iniFile, getenv("HOME")), myConfFile);

  // Look for personal file
  if( access( iniFile, R_OK ) != -1 ) {
    // file exists
    rc = 0;
    syslog(LOG_INFO, "reading config %s\n", iniFile); 
  }else{
    // file doesn't exist, check global
    if( access( myGlConfFile, R_OK ) != -1 ) {
      strcpy(iniFile, myGlConfFile);
      rc = 0;
      syslog(LOG_INFO, "reading config %s\n", iniFile);
    }else{
      // Write a default file to the home dir
      FILE *f = fopen(iniFile, "w");
      if (f == NULL) {
        syslog(LOG_ERR, "err: failed write config file %s\n", myConfFile);
      }else{
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
    if ( numHotkeys < MAX_HOTKEYS ) {
      for (k = 0; ini_getkey(section, k, str, sizearray(str), iniFile) > 0; k++) {
        if ( strncmp("program", str, 7) == 0 ) { // Key found
          n = ini_gets(section, str, "dummy", myKeys[numHotkeys].swFilename, sizearray(myKeys[numHotkeys].swFilename), iniFile);
          if ( n > 5 && strncmp("dummy", myKeys[numHotkeys].swFilename, 5) != 0 ) { // Value is not empty
            syslog(LOG_INFO, "Filename: %s\n", myKeys[numHotkeys].swFilename);
          } 
        }  
        if ( strncmp("button1", str, 7) == 0 ) { // Key found
          l = ini_getl(section, str, BUTTON_DEFINED_RANGE, iniFile);
          if ( buttonDefined(l) == true ) { // Value is not empty
            syslog(LOG_INFO, "button1: %ld\n", l);
            myKeys[numHotkeys].button1 = l;
            myKeys[numHotkeys].activeButtons++;
          } 
        }  
        if ( strncmp("button2", str, 7) == 0 ) { // Key found
          l = ini_getl(section, str, BUTTON_DEFINED_RANGE, iniFile);
          if ( buttonDefined(l) == true ) { // Value is not empty
            syslog(LOG_INFO, "button2: %ld\n", l);
            myKeys[numHotkeys].button2 = l;
            myKeys[numHotkeys].activeButtons++;
          } 
        }  
        if ( strncmp("button3", str, 7) == 0 ) { // Key found
          l = ini_getl(section, str, BUTTON_DEFINED_RANGE, iniFile);
          if ( buttonDefined(l) == true ) { // Value is not empty
            syslog(LOG_INFO, "button3: %ld\n", l);
            myKeys[numHotkeys].button3 = l;
            myKeys[numHotkeys].activeButtons++;
          } 
        }  
        if ( strncmp("button4", str, 7) == 0 ) { // Key found
          l = ini_getl(section, str, BUTTON_DEFINED_RANGE, iniFile);
          if ( buttonDefined(l) == true ){ // Value is not empty
            syslog(LOG_INFO, "button4: %ld\n", l);
            myKeys[numHotkeys].button4 = l;
            myKeys[numHotkeys].activeButtons++;
          } 
        }  
      } /* for */
    }
    numHotkeys++; // Remember how many sections we have
  } /* for */
}

//---------------------------------------------
// Validity check of the provided config items
//---------------------------------------------
int checkConfig(void) {
  int rc=0;
  int i;
  for (i=0;i<numHotkeys;i++){
    if ( sizearray(myKeys[i].swFilename) < 3 ) { // no program make no sense
      syslog(LOG_ERR, "err: no valid filename provided in section %d. Please check ini file\n", i);
      rc = 1;
    }
    if ( buttonDefined(myKeys[i].button1) == false ) { // we need at least one button for tracking
      syslog(LOG_ERR, "err: button assignment missing in section %d. Please set at least button1 in the ini file!\n", i);
      rc = 1; 
    }
    syslog(LOG_INFO, "Active assigned buttons in section %d: ", i);
    syslog(LOG_INFO, "%d\n", myKeys[i].activeButtons);
  
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
  for (i=0;i<numHotkeys;i++){
    if ( js.number == myKeys[i].button1 ) {
      myKeys[i].button1Active = js.value;
    }
    if ( js.number == myKeys[i].button2 ) {
      myKeys[i].button2Active = js.value;
    }
    if ( js.number == myKeys[i].button3 ) {
      myKeys[i].button3Active = js.value;
    }
    if ( js.number == myKeys[i].button4 ) {
      myKeys[i].button4Active = js.value;
    }
  }
  // Analyse combinations
  for (i=0; i<numHotkeys;i++){
    switch (myKeys[i].activeButtons) {
      case 1:
        if ( myKeys[i].button1Active == 1 ) { myKeys[i].isTriggered = 1; section = i; }
        break;
      case 2:
        if ( myKeys[i].button1Active == 1 && myKeys[i].button2Active == 1 ) { 
          myKeys[i].isTriggered = 1; section = i; 
        }
        break;
      case 3:
        if ( myKeys[i].button1Active == 1 && myKeys[i].button2Active == 1 && myKeys[i].button3Active == 1 ) { 
          myKeys[i].isTriggered = 1; section = i;
        }
        break;
      case 4:
        if ( myKeys[i].button1Active == 1 && myKeys[i].button2Active == 1 && myKeys[i].button3Active == 1 && myKeys[i].button4Active == 1) { 
          myKeys[i].isTriggered = 1; section = i;
        }
        break;
    }
  }
  return section;
}

//---------------------------------------------
// Reset the keys
//---------------------------------------------
void resetHotkeys(){
  int i;
  for (i=0;i<numHotkeys;i++){
    myKeys[i].button1Active = 0;
    myKeys[i].button2Active = 0;
    myKeys[i].button3Active = 0;
    myKeys[i].button4Active = 0;
    myKeys[i].isTriggered = false;
  }
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

  joyFD = -1;  // Clear previous joystick
	
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
      if ((joyFD = open(devPath, O_RDONLY)) < 0) { // Open the file descriptor
        syslog (LOG_INFO, "error: failed to open fd\n");
      }
    }
   
    udev_device_unref(mydev);
  }
  /* cleanup */
  udev_enumerate_unref(enumerate);

  if ( joyFD < 0 ) { // Still no joystick found

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

              if (sysPath != NULL && devPath != NULL && strstr(sysPath, "/js") != 0) {
	        syslog(LOG_NOTICE, "Found Device: %s\n", devPath);
                if ((joyFD = open(devPath, O_RDONLY)) < 0) { // Open the file descriptor
                  syslog(LOG_INFO, "error: failed to open fd\n");
                }
              }

              udev_device_unref(mydev);
 	    }
            /* cleanup */
            udev_enumerate_unref(enumerate);
          }else{
            if ( strncmp(udev_device_get_action(dev), "remove", 6) == 0 ) { // Device remove
              if ( joyFD >= 0 ) {
                 close(joyFD);
              }
              joyFD = -1; // Reset 
            }
          }
        }else{
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
// Listen on the input and call the program 
//---------------------------------------------
int bindJoy(void) {

  char *axis_names[ABS_MAX + 1] = { "X", "Y", "Z", "Rx", "Ry", "Rz", "Throttle", "Rudder",
    "Wheel", "Gas", "Brake", "?", "?", "?", "?", "?",
    "Hat0X", "Hat0Y", "Hat1X", "Hat1Y", "Hat2X", "Hat2Y", "Hat3X", "Hat3Y",
    "?", "?", "?", "?", "?", "?", "?",
  };

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
  int needTrigger;
  fcntl(joyFD, F_SETFL, O_NONBLOCK);

  while (1) {
    while (read(joyFD, &js, sizeof(struct js_event)) == sizeof(struct js_event))  {
      syslog(LOG_DEBUG, "Event: type %d, time %d, number %d, value %d\n",
              js.type, js.time, js.number, js.value);
      needTrigger = checkButtonPressed(js);
      if ( needTrigger > -1 ) { // We have found one key section
        syslog(LOG_INFO, "Swtching mode. ...\n");
        // call external program
        int rc = system(myKeys[needTrigger].swFilename);
        if ( rc == 0 ) {
          syslog(LOG_INFO, "Call succesfull\n");
        }else{
          syslog(LOG_INFO, "Call failed\n");
        }
        // reset state, so we call only once
        resetHotkeys();
      }
    }

    if (errno != EAGAIN) {
      syslog(LOG_DEBUG, "\njslistent: error reading"); // Regular exit if the joystick disconnect
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


  // Init Defaults
  int i;
  for (i=0;i<MAX_HOTKEYS;i++) {
    myKeys[i].button1 = BUTTON_DEFINED_RANGE;
    myKeys[i].button1Active = 0;
    myKeys[i].button2 = BUTTON_DEFINED_RANGE;
    myKeys[i].button2Active = 0;
    myKeys[i].button3 = BUTTON_DEFINED_RANGE;
    myKeys[i].button3Active = 0;
    myKeys[i].button4 = BUTTON_DEFINED_RANGE;
    myKeys[i].button4Active = 0;
    myKeys[i].activeButtons = 0;
    myKeys[i].isTriggered = false;
  }

  // Parse parameters to set debug level
  if ( argc > 1 ) {
    if (strncmp(argv[1], "--debug", 7) == 0) {
      logLevel = LOG_DEBUG; 
    }
  } 

  // Open Syslog
  setlogmask (LOG_UPTO (logLevel));
  openlog (MYPROGNAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
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
