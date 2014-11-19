// Chinese USB/HID relay command line tool:
//
// pa02 19-Nov-2014 supports 1,2,4 - relay devices
// Currently finds the 1st matching device by ven,dev, product name string.
// TODO:
// - Support multiple devices, select one by ID!
// Build for Windows: using VC++ 2008 and WDK7.1
//~~~~~~~~~~~~~~~~~~~~~~~~

/* Prototype: V-USB example: vusb-20121206/examples/hid-data/commandline/hidtool.c 
 * Author: Christian Starkjohann
 * Creation Date: 2008-04-11
 * Copyright: (c) 2008 by OBJECTIVE DEVELOPMENT Software GmbH
 */

#define A_VER_STR "r1.1x (1 device only)"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hidusb-tool.h"

#define USB_RELAY_VENDOR_NAME     "www.dcttech.com"
#define USB_RELAY1_NAME           "USBRelay1"  // 1 relay
#define USB_RELAY2_NAME           "USBRelay2"  // 2 relays
#define USB_RELAY4_NAME           "USBRelay4"  // 4 relays


static int g_max_relay_num = 0;

static int rel_read_status_raw(USBDEVHANDLE dev, void *raw_data);

/* ------------------------------------------------------------------------- */

static const char *usbErrorMessage(int errCode)
{
    static char buffer[80];

    switch (errCode) {
        case USBOPEN_ERR_ACCESS:      return "Access to device denied";
        case USBOPEN_ERR_NOTFOUND:    return "The specified device was not found";
        case USBOPEN_ERR_IO:          return "Communication error with device";
    }

    snprintf(buffer, sizeof(buffer), "Unknown USB error %d", errCode);
    return buffer;
}

static USBDEVHANDLE openDevice(void)
{
    USBDEVHANDLE    dev = 0;
    char            vendorName[]  = USB_RELAY_VENDOR_NAME, 
                    productName[] = USB_RELAY1_NAME; 
    int             vid = USB_CFG_VENDOR_ID;
    int             pid = USB_CFG_DEVICE_ID;
    int             err;
    int num = 0;

    // TODO: enumerate all instances, then filter  by unique ID
    //$$$ find any one device
    strcpy(productName, USB_RELAY2_NAME);
    if((err = usbhidOpenDevice(&dev, vid, vendorName, pid, productName, 0)) == 0) {
      num = 2;
      goto check1;
    }

    strcpy(productName, USB_RELAY1_NAME);
    if((err = usbhidOpenDevice(&dev, vid, vendorName, pid, productName, 0)) == 0) {
      num = 1;
      goto check1;
    }

    strcpy(productName, USB_RELAY4_NAME);
    if((err = usbhidOpenDevice(&dev, vid, vendorName, pid, productName, 0)) == 0) {
      num = 4;
      goto check1;
    }

    fprintf(stderr, "error finding USB relay: %s\n", usbErrorMessage(err));
    return NULL;

    check1:
    { /* Check the unique ID: 5 bytes at offs 0 */
        char buffer[16];
        err = rel_read_status_raw(dev, buffer);
        if( err < 0 ){
            fprintf(stderr, "error reading report 0: %s\n", usbErrorMessage(err));
            usbhidCloseDevice(dev);
            return NULL;
        }else{
            int i;
            //hexdump(buffer + 1, sizeof(buffer) - 1);
            for (i=1; i <=5; i++) {
                unsigned char x = (unsigned char)buffer[i];
                if (x <= 0x20 || x >= 0x7F) {
                    fprintf(stderr, "Bad device ID!\n");
                    usbhidCloseDevice(dev);
                    return NULL;
                }
            }
            if( buffer[6] != 0 ) {
                    fprintf(stderr, "Bad device ID!\n");
                    usbhidCloseDevice(dev);
                    return NULL;
            }
            
            DEBUG_PRINT(("Device %s found: ID=[%5s]\n", USB_CFG_DEVICE_NAME, &buffer[1]));
            g_max_relay_num = num;
        }
    }

    return dev;
}

/* ------------------------------------------------------------------------- */
#if 0
static void hexdump(char *buffer, int len)
{
int     i;
FILE    *fp = stdout;

    for(i = 0; i < len; i++){
        if(i != 0){
            if(i % 16 == 0){
                fprintf(fp, "\n");
            }else{
                fprintf(fp, " ");
            }
        }
        fprintf(fp, "0x%02x", buffer[i] & 0xff);
    }
    if(i != 0)
        fprintf(fp, "\n");
}
#endif

/* ------------------------------------------------------------------------- */

static void usage(char *myName)
{
    char *p = strrchr(myName, '\\'); /* windows */
    if (p) myName = p + 1;
    else p = strrchr(myName, '/'); /* whatever */
    if (p) myName = p + 1;

    fprintf(stderr, "USBHID relay utility, " A_VER_STR "\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s on <num>  - turn relay <num> ON\n", myName);
    fprintf(stderr, "  %s off <num> - turn relay <num> OFF\n", myName);
    fprintf(stderr, "  %s state     - print state of the relays\n", myName);
}


// Read state of all relays
// @return bit mask of all relays (R1->bit 0, R2->bit 1 ...) or -1 on error
static int rel_read_status_raw(USBDEVHANDLE dev, void *raw_data)
{
    char buffer[10];
    int err;
    int reportnum = 0;
    int len = 8 + 1; /* report id 1 byte + 8 bytes data */
    memset(buffer, 0, sizeof(buffer));

    err = usbhidGetReport(dev, reportnum, buffer, &len);
    if ( err ) {
        fprintf(stderr, "error reading status: %s\n", usbErrorMessage(err));
        return -1;
    }

    if ( len != 9 || buffer[0] != reportnum ) {
        fprintf(stderr, "ERROR: wrong HID report returned!\n", len);
        return -2;
    }

    if (raw_data) {
        /* copy raw report data */
        memcpy( raw_data, buffer, sizeof(buffer) );
    }

    return (int)(unsigned char)buffer[8]; /* byte of relay states */
}


static int rel_onoff( USBDEVHANDLE dev, int is_on, char const *numstr )
{
    unsigned char buffer[10];
    int err = -1;
    int relaynum = numstr ? atoi(numstr) : 0;
    
    if ( numstr && (0 == strcmp(numstr,"*")) ) {
        char x[2] = {'1', 0};
        int i;
        for (i = 1; i <= g_max_relay_num; i++) {
            x[0] = (char)('0' + i);
            err = rel_onoff(dev, is_on, x);
            if (err) break;
        }
        return err;
    }

    if ( relaynum <= 0 || relaynum > g_max_relay_num ) {
        fprintf(stderr, "Invalid relay number. Must be 1-%d or * (all)\n", g_max_relay_num);
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 0; /* report # */
    buffer[1] = is_on ? 0xFF : 0xFD;
    buffer[2] = (unsigned char)relaynum;
    if((err = usbhidSetReport(dev, buffer, 9)) != 0) {  
        fprintf(stderr, "Error writing data: %s\n", usbErrorMessage(err));
        return 1;
    }
    
    // Read back & verify
    err = rel_read_status_raw(dev, NULL);
    if ( err >= 0 ) {
        err = (err >> (unsigned)(relaynum -1)) & 1;
        err ^= !!is_on;
    }

    if ( err ) {
        fprintf(stderr, "Error: failed set %s relay %u\n", is_on ? "ON":"OFF", relaynum);
        return 1;
    }

    return 0;
}


static int show_status(USBDEVHANDLE dev)
{
    int err;
    char buffer[10];
	static const char* on_off[] = {"OFF","ON"};

#define onoff(n) on_off[!!(err & (1U << n))]

    err = rel_read_status_raw(dev, buffer);
    if ( err < 0 ){
        fprintf(stderr, "error reading data: %s\n", usbErrorMessage(err));
        err = 1;
    } else {
        //hexdump(buffer + 1, len - 1);
        switch (g_max_relay_num) {
            case 1:
                printf("Board ID=[%5.5s] State: R1=%s\n", &buffer[1], onoff(0) );
				break;
            case 2:
                printf("Board ID=[%5.5s] State: R1=%s R2=%s\n",
					&buffer[1],	onoff(0), onoff(1) );
				break;
            case 4:
                printf("Board ID=[%5.5s] State: R1=%s R3=%s R1=%s R4=%s\n",
					&buffer[1],	onoff(0), onoff(1), onoff(2), onoff(3) );
				break;
            default:
				printf("Board ID=[%5.5s] State: %2.2X (hex)\n", &buffer[1], (unsigned char)err );
				break;
        }
        err = 0;
    }
    return err;
#undef onoff
}

int main(int argc, char **argv)
{
    USBDEVHANDLE dev = 0;
    int         err;
    char const *arg1 = (argc >= 2) ? argv[1] : NULL;
    char const *arg2 = (argc >= 3) ? argv[2] : NULL;

    if ( !arg1 ) {
        usage(argv[0]);
        exit(1);
    }

    dev = openDevice();
    if ( !dev )
        exit(1);

    if ( strncasecmp(arg1, "stat", 4) == 0 ) { // stat|state|status
        err = show_status(dev);
		// TODO enumerate all devices
    }else if( strcasecmp(arg1, "on" ) == 0) {
        err = rel_onoff(dev, 1, arg2);
    }else if( strcasecmp(arg1, "off" ) == 0) {
        err = rel_onoff(dev, 0, arg2);
    }else {
        usage(argv[0]);
        err = 2;
    }

    if ( dev ) {
        usbhidCloseDevice(dev);
    }

    return err;
}

/* ------------------------------------------------------------------------- */
