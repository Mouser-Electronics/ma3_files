/***********************************************************************************************************************
* File Name    : console_thread_entry.c
* Description  : This file contains console thread function which handles CLI events in this application.
***********************************************************************************************************************/

/***********************************************************************************************************************

* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws.
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
* this software. By using this software, you agree to the additional terms and conditions found by accessing the
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2017 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
#include "console_thread.h"
#include "MQTT_Config.h"
#include "common_util.h"
#include "MQTT_Thread.h"
#include "console_config.h"
#include "internal_flash.h"
#include "base64.h"
#include "sf_cellular_qctlcatm1_private_api.h"
#include "sf_cellular_common_private.h"
#include "sf_cellular_serial.h"
#include "sensors.h"

uint8_t certPEM[4096];
unsigned int certDER[4096];
void print_to_console(const char* msg);
void print_ipv4_addr(ULONG address, char *str, size_t len);
static uint8_t sq_number = 0;

/* BEGIN ADDED */

static int console_connected = 1;   // 1=connected, 0=not connected

void g_sf_console_err_callback(void *p_instance, void *p_data);
void g_sf_console_err_callback(void *p_instance, void *p_data)
{
    /** Suppress compiler warning for not using parameters. */
    SSP_PARAMETER_NOT_USED (p_instance);
    SSP_PARAMETER_NOT_USED (p_data);

    /* Console is not connected */

    console_connected = 0;
}

/* END ADDED */

static void print_static_addr(net_input_cfg_t net_cfg)
{
    CHAR str[64];

    print_to_console("\r\n  IP Address  : ");
    print_ipv4_addr(net_cfg.netif_static.address, str, sizeof(str));
    print_to_console(str);
    print_to_console("\r\n");

    print_to_console("  Netmask     : ");
    print_ipv4_addr(net_cfg.netif_static.mask, str, sizeof(str));
    print_to_console(str);
    print_to_console("\r\n");

    print_to_console("  Gateway     : ");
    print_ipv4_addr(net_cfg.netif_static.gw, str, sizeof(str));
    print_to_console(str);
    print_to_console("\r\n");

    print_to_console("  DNS Server  : ");
    print_ipv4_addr(net_cfg.netif_static.dns, str, sizeof(str));
    print_to_console(str);
    print_to_console("\r\n");

}

/*********************************************************************************************************************
 * @brief  print_to_console function
 *
 * This function Print the message in the serial port
 ********************************************************************************************************************/
void print_to_console(const char* msg)
{
    UINT status;
    char str[128];
    unsigned int i = 0, j = 0;

    /* BEGIN ADDED */

    if (!console_connected ) {
        return;
    }

    /* END ADDED */

    status = tx_mutex_get(&g_console_mutex, TX_WAIT_FOREVER);
    if (status != TX_SUCCESS)
        return;

    /* The console framework's maximum write string length is set to
     * 128. If we see an input string >128 bytes, we chunk it up
     * and print it.
     */
    j = 0;
    do {
        /* Take the smaller of the remaining part of the message and
         * the size of the temporary buffer.
         */
        i = MIN(strlen(msg) - j, sizeof(str) - 1);

        /* Copy the next chunk into the temporary buffer */
        memcpy(str, &msg[j], i);
        str[i] = '\0';

        /* Write it to the serial port */
        g_sf_console.p_api->write(g_sf_console.p_ctrl,(const uint8_t*)str, TX_NO_WAIT);

        /* Move to the next chunk */
        j += i;
    } while (j < strlen(msg));

    /* Release the mutex */
    tx_mutex_put(&g_console_mutex);
}

/*********************************************************************************************************************
 * @brief  main_menu function
 *
 * This function display the Main selection menu and collects the user input as well.
 ********************************************************************************************************************/
static uint8_t main_menu(void)
{
    char data[128];
    print_to_console("\r\n##################    Main Menu  #######################\r\n");
    print_to_console("\r\n 1. Network Interface Selection \r\n 2. GCloud IoT Core Configuration \r\n 3. Dump previous configuration from flash\r\n 4. Exit \r\n");
    print_to_console("\r\n Please Enter Your Choice:");
    print_to_console(">");
    GetUserInput(&data[0]);
    return((uint8_t)atoi(data));
}

static uint8_t cellular_config_main_men(void)
{
    char data[128];

    do{
        print_to_console("\r\n################ Cellular Modem Config Menu #################\r\n");
        print_to_console("\r\n 1. Start Provisioning \r\n 2. Start SIM configuration \r\n");
        print_to_console("\r\n Enter Your Choice: ");
        GetUserInput(&data[0]);

        if(data[0] == '1')
        {
            return((uint8_t)atoi(data));
        }
        else if(data[0] == '2')
        {
            return((uint8_t)atoi(data));
        }
        else
        {
            print_to_console("Invalid Choice !!!\r\n");
        }
    }while(1);

    return 0;
}

/*********************************************************************************************************************
 * @brief  network_select_menu function
 *
 * This function display the network interface selection menu and collects the user input as well.
 *********************************************************************************************************************/
static uint8_t network_select_menu(void)
{
    char data[128];
    print_to_console("\r\nNetwork Interface Selection:\r\n 1. Ethernet\r\n 2. Wi-Fi\r\n 3. Cellular\r\n 4. Exit\r\n");
    print_to_console("\r\n Please Enter Your Choice:");
    print_to_console(">");
    GetUserInput(&data[0]);
    print_to_console("\r\nEntered Network Interface: ");
    if(data[0] == '1')
    {
        print_to_console("Ethernet\r\n");
    }
    else if(data[0] == '2')
    {
        print_to_console("Wi-Fi\r\n");
    }
    else if(data[0] == '3')
    {
        print_to_console("Cellular\r\n");
    }
    else if(data[0] == '4')
    {
        print_to_console("\r\n");
    }
    else
    {
        print_to_console("Invalid Argument\r\n");
    }
    return((uint8_t)atoi(data));
}

static void ip_mode_static_menu(net_input_cfg_t *net_cfg)
{
    char data[128];
    ULONG a0, a1, a2, a3;

    memset(&data[0],'\0',128);
    print_to_console("\r\nEnter the IP Address:\r\n");
    print_to_console(">");
    GetUserInput(&data[0]);
    if(sscanf(data,"%lu.%lu.%lu.%lu",&a3, &a2, &a1, &a0) != 4){
        print_to_console("Invalid IP address\r\n");
        return;
    }

    net_cfg->netif_static.address = ((a3 & 0xff) << 24) | ((a2 & 0xff) << 16) | ((a1 & 0xff) << 8) | (a0 & 0xff);

    memset(&data[0],'\0',128);
    print_to_console("\r\nEnter Network Mask: \r\n");
    print_to_console(">");
    GetUserInput(&data[0]);
    if (sscanf(data, "%lu.%lu.%lu.%lu", &a3, &a2, &a1, &a0) != 4) {
        print_to_console("Invalid network mask\r\n");
        return;
    }

    net_cfg->netif_static.mask = ((a3 & 0xff) << 24) | ((a2 & 0xff) << 16) | ((a1 & 0xff) << 8) | (a0 & 0xff);
    memset(&data[0],'\0',128);

    print_to_console("\r\nEnter Gateway:\r\n");
    print_to_console(">");
    GetUserInput(&data[0]);
    if (sscanf(data, "%lu.%lu.%lu.%lu", &a3, &a2, &a1, &a0) != 4) {
        print_to_console("Invalid Gateway address\r\n");
        return;
    }

    net_cfg->netif_static.gw = ((a3 & 0xff) << 24) | ((a2 & 0xff) << 16) | ((a1 & 0xff) << 8) | (a0 & 0xff);

    memset(&data[0],'\0',128);
    print_to_console("\r\nEnter DNS:\r\n");
    print_to_console(">");
    GetUserInput(&data[0]);
    if (sscanf(data, "%lu.%lu.%lu.%lu", &a3, &a2, &a1, &a0) != 4) {
        print_to_console("Invalid DNS address\r\n");
        return;
    }

    net_cfg->netif_static.dns = ((a3 & 0xff) << 24) | ((a2 & 0xff) << 16) | ((a1 & 0xff) << 8) | (a0 & 0xff);
}

/*********************************************************************************************************************
 * @brief  ip_mode_select_menu function
 *
 * This function display the IP Address Configuration selection menu and collects the user input as well.
 *********************************************************************************************************************/
static uint8_t ip_mode_select_menu(void)
{
    char data[128];

    print_to_console("\r\n Enter IP Address Configuration Mode\r\n 1. Static\r\n 2. DHCP\r\n");
    print_to_console("Please Enter Your Choice\r\n");
    print_to_console(">");

    GetUserInput(&data[0]);

    if(strlen(data) == 1)
    {
        print_to_console("\r\nEntered IP Configuration Mode: ");
        if (data[0] == '1')
        {
            print_to_console("Static\r\n");
            return IOTKIT_ADDR_MODE_STATIC;
        }
        else if (data[0] == '2')
        {
            print_to_console("DHCP\r\n");
            return IOTKIT_ADDR_MODE_DHCP;
        }
    }

    return IOTKIT_ADDR_MODE_NOTCONFIG;
}

static uint8_t cellular_config_menu(void)
{
    char data[128];

    memset(&data[0], 0, sizeof(data));

    do {
        print_to_console("\r\n");
        print_to_console("\r\n################ Cellular Configuration Menu #################\r\n");
        print_to_console(" 1. Manual Config using AT cmd shell\r\n 2. Auto Config from Pre-stored AT cmd list\r\n");
        print_to_console("\r\n Enter your choice: ");
        GetUserInput(&data[0]);

        if(data[0] == '1')
        {
            return((uint8_t)atoi(data));
        }
        else if(data[0] == '2')
        {
            return((uint8_t)atoi(data));
        }
        else
        {
            print_to_console("Invalid choice !!!\r\n");
        }

    }while(1);

    return 0;
}

static uint8_t cellular_autocfg_menu(uint8_t mode)
{
    char data[32];

    memset(&data[0], 0, sizeof(data));

    if(mode != CELL_PREQUALIFIED_LIST)
    {
        do
        {
            print_to_console("\r\n");
            print_to_console("\r\n################ Cellular AutoCfg Menu #################\r\n");
            print_to_console(" 1. Autocfg using stored user's AT cmd list\r\n");
            print_to_console("\r\n Enter your choice: ");
            GetUserInput(&data[0]);

            if(data[0] == '1')
            {
                return((uint8_t)atoi(data));
            }
            else
            {
                print_to_console("Invalid choice !!!\r\n");
            }

        }while(1);
    }

    return 0;
}

/*********************************************************************************************************************
 * @brief  cellular_prov_menu function
 *
 * This function display the Cellular Configuration selection menu and collects the user input as well.
 *********************************************************************************************************************/
static UINT cellular_prov_menu(net_input_cfg_t *net_cfg)
{
    char data[128];

    memset(&data[0], 0, sizeof(data));
    print_to_console("\r\n Cellular Provisioning");
    print_to_console("\r\n Enter the APN associated with the Cellular Provider\r\n");
    print_to_console(">");
    GetUserInput(&data[0]);

    /*update APN */
    if(strlen(data) != 0)
        memcpy(&net_cfg->cell_prov.apn, &data, strlen(data));
    else
        return 0;

    memset(&data[0], '\0', 128);
    print_to_console("\r\nEnter Context ID: Valid range is 1 to 5. \r\n");
    print_to_console(">");
    GetUserInput(&data[0]);

    if(strlen(data) == 1)
    {
        net_cfg->cell_prov.context_id = (uint8_t)atoi(data);
        if((net_cfg->cell_prov.context_id < 1) || (net_cfg->cell_prov.context_id > 5))
            return 0;
    }
    else
        return 0;

    memset(&data[0], '\0', 128);
    print_to_console("\r\n Enter PDP Type\r\n 1. IP\r\n 2. IPV4V6\r\n");
    print_to_console("Please enter your choice\r\n");
    print_to_console(">");
    GetUserInput(&data[0]);

    if(strlen(data) == 1)
    {
        print_to_console ("\r\nEntered PDP Type: ");
        if (data[0] == '1')
        {
            print_to_console ("IP\r\n");
            net_cfg->cell_prov.pdp_type = SF_CELLULAR_PDP_TYPE_IP;
        }
        else if (data[0] == '2')
        {
            print_to_console ("IPV4V6\r\n");
            net_cfg->cell_prov.pdp_type = SF_CELLULAR_PDP_TYPE_IPV4V6;
        }
        else
            return 0;
    }
    else
        return 0;

    /*Default values*/
    net_cfg->cell_prov.airplane_mode = SF_CELLULAR_AIRPLANE_MODE_OFF;
    net_cfg->cell_prov.auth_type = SF_CELLULAR_AUTH_TYPE_NONE;
    net_cfg->cell_prov.username[0] = '\0';
    net_cfg->cell_prov.password[0] = '\0';

    return 1;
}

/*********************************************************************************************************************
 * @brief  wifi_config_menu function
 *
 * This function display the Wi-Fi Configuration selection menu and collects the user input as well.
 *********************************************************************************************************************/
static UINT wifi_config_menu(net_input_cfg_t *net_cfg)
{
    char data[128];

    memset(&net_cfg->wifi_prov.ssid,0,sizeof(net_cfg->wifi_prov.ssid));
    memset(&net_cfg->wifi_prov.key,0,sizeof(net_cfg->wifi_prov.key));

    print_to_console("\r\n Wi-Fi Configuration");
    print_to_console("\r\nEnter the SSID associated with the Network\r\n");
    memset(&data[0], '\0', 128);
    print_to_console(">");
    GetUserInput(&data[0]);

    /* update SSID */
    memcpy(&net_cfg->wifi_prov.ssid, &data, strlen(data));

    print_to_console("\r\nEnter the passphrase \r\n");
    print_to_console(">");
    GetUserInput(&data[0]);

    /* update key */
    memcpy(&net_cfg->wifi_prov.key, &data, strlen(data));

    print_to_console("\r\n Enter Security Type\r\n 1. WEP\r\n 2. WPA\r\n 3. WPA2\r\n 4. None\r\n");
    print_to_console("Please Enter Your Choice\r\n");
    print_to_console(">");
    GetUserInput(&data[0]);

    /* Default WiFi setup */
    net_cfg->wifi_prov.channel = 6;
    net_cfg->wifi_prov.encryption = SF_WIFI_ENCRYPTION_TYPE_AUTO;
    net_cfg->wifi_prov.mode = SF_WIFI_INTERFACE_MODE_CLIENT;

    print_to_console("\r\nEntered Security Type: ");
    if(strlen(data) == 1)
    {
        if(data[0] == '1')
        {
            print_to_console("WEP\r\n");
            net_cfg->wifi_prov.security = SF_WIFI_SECURITY_TYPE_WEP;
        }
        else if(data[0] == '2')
        {
            print_to_console("WPA\r\n");
            net_cfg->wifi_prov.security = SF_WIFI_SECURITY_TYPE_WPA;
        }
        else if(data[0] == '3')
        {
            print_to_console("WPA2\r\n");
            net_cfg->wifi_prov.security = SF_WIFI_SECURITY_TYPE_WPA2;
        }
        else if(data[0] == '4')
        {
            print_to_console("None\r\n");
            net_cfg->wifi_prov.security = SF_WIFI_SECURITY_TYPE_OPEN;
        }
        else
        {
            print_to_console("Invalid Input\r\n");
            return 0;
        }
    }

    return 1;
}

/*********************************************************************************************************************
 * @brief  iot_service_submenu function
 *
 * This function display the IoT Cloud sub menu and collects the user input as well.
 *********************************************************************************************************************/
static uint8_t iot_service_submenu(void)
{
    char data[128];

    do {
        print_to_console("\r\n 1. Google IoT Core Setting Menu\r\n 2. Device Certificate/Keys Setting Menu\r\n 3. Exit\r\n");
        print_to_console("\r\n Please Enter Your Choice:");
        print_to_console(">");
        GetUserInput(&data[0]);

        if((data[0] == '1') || (data[0] == '2') || (data[0] == '3'))
            break;
        else
            print_to_console("Invalid Input !!!\r\n");

    }while(1);

    return((uint8_t)atoi(data));
}

/*********************************************************************************************************************
 * @brief  gcloud_submenu function
 *
 * This function gets the GCloud Thing information from the user.
 *********************************************************************************************************************/
static void gcloud_submenu(iot_input_cfg_t *gCloudcfg)
{
    char data[128];

    do {
        print_to_console("\r\n");
        print_to_console("\r\n ############ Google Cloud Settings Menu ###############\r\n");
        print_to_console("\r\n 1. Enter Project Id:\r\n 2. Enter Endpoint information:\r\n 3. Enter Device Id:\r\n");
        print_to_console(" 4. Enter Cloud Region:\r\n 5. Enter Registry Id:\r\n 6. Exit\r\n");
        print_to_console("\r\n Please Enter Your Choice:");
        print_to_console(">");
        GetUserInput(&data[0]);

        if(data[0] == '1')
        {
            print_to_console("\r\nEnter Project ID: ");
            GetUserInput(&data[0]);
            strncpy((char*)gCloudcfg->gCloud_info.project_id, data, sizeof(data));
        }
        else if(data[0] == '2')
        {
            print_to_console("\r\nEnter Endpoint information: ");
            GetUserInput(&data[0]);
            strncpy((char*)gCloudcfg->gCloud_info.endp_address, data, sizeof(data));
        }
        else if(data[0] == '3')
        {
            print_to_console("\r\nEnter Device ID: ");
            GetUserInput(&data[0]);
            strncpy((char*)gCloudcfg->gCloud_info.device_id, data, sizeof(data));
        }
        else if(data[0] == '4')
        {
            print_to_console("\r\nEnter Cloud Region: ");
            GetUserInput(&data[0]);
            strncpy((char*)gCloudcfg->gCloud_info.cloud_region, data, sizeof(data));
        }
        else if(data[0] == '5')
        {
            print_to_console("\r\nEnter Registry ID: ");
            GetUserInput(&data[0]);
            strncpy((char*)gCloudcfg->gCloud_info.registry_id, data, sizeof(data));
        }
        else if(data[0] == '6')
            break;
        else
        {
            print_to_console("Invalid Input !!! \r\n");
        }

    }while(1);
}

/*********************************************************************************************************************
 * @brief  GetrootCA function
 *
 * This function gets the rootCA certificate from the user and store in the internal flash.
 *********************************************************************************************************************/
static unsigned int GetrootCA(void)
{
    unsigned int i = 0, j =0;
    unsigned int temp;
    uint8_t str[66];
    int skipBegin = 0;
    char *begin_CA = "-----BEGIN CERTIFICATE-----";
    char *end_CA = "-----END CERTIFICATE-----";
    memset(certPEM,0,sizeof(certPEM));
    memset(certDER,0,sizeof(certDER));

    do {
        g_sf_console.p_api->read(g_sf_console.p_ctrl, &str[0], 66, TX_WAIT_FOREVER);

        if(!skipBegin)
        {
            skipBegin = 1;
            if(str[0] == '-')
            {
                i += strlen(begin_CA);
                strncpy((char*)&certPEM[0], (char*)&str[i],sizeof(str));
                i = (strlen((char*)str) - strlen(begin_CA));
            }

            continue;
        }

        else if(strcmp((char*)&str[0], end_CA) == 0)
            break;
        else
        {
            strncpy((char*)&certPEM[i], (char*)&str[0],sizeof(str));
        }

        i +=strlen((char*)str);

    }while(1);

    base64_decode(&certPEM[0], i ,&certDER[0]);

    temp = (unsigned int)base64d_size(i);

    for(j = 0; j <= temp; j++)
        certPEM[j] = (unsigned char)certDER[j];

    //store the rootCA certificate in internal flash
    if(int_storage_write((uint8_t*)&certPEM[0], temp, ROOTCA_CERT_CFG,0) != SSP_SUCCESS)
    {
        print_to_console("Flash Write Failed\r\n");
        APP_ERR_TRAP(1);
    }
    else
        print_to_console("rootCA Certificate stored in flash\r\n");

    return((unsigned int)base64d_size(i));
}

/*********************************************************************************************************************
 * @brief  GetDevCert function
 *
 * This function gets the Device certificate from the user and store in the internal flash.
 *********************************************************************************************************************/
static unsigned int GetDevCert(void)
{
    unsigned int i = 0, j =0;
    unsigned int temp;
    int skipBegin = 0;
    uint8_t str[66];
    char *begin_CA = "-----BEGIN CERTIFICATE-----";
    char *end_CA = "-----END CERTIFICATE-----";
    memset(certPEM,0,sizeof(certPEM));
    memset(certDER,0,sizeof(certDER));

    do {
        g_sf_console.p_api->read(g_sf_console.p_ctrl, &str[0], 66, TX_WAIT_FOREVER);

        if(!skipBegin)
        {
            skipBegin = 1;
            if(str[0] == '-')
            {
                i += strlen(begin_CA);
                strncpy((char*)&certPEM[0], (char*)&str[i],sizeof(str));
                i = (strlen((char*)str) - strlen(begin_CA));
            }

            continue;
        }
        else if(strcmp((char*)&str[0], end_CA) == 0)
            break;
        else
        {
            strncpy((char*)&certPEM[i], (char*)&str[0],sizeof(str));
        }

        i += strlen((char*)str);

    }while(1);

    base64_decode(&certPEM[0], i ,&certDER[0]);

    temp = (unsigned int)base64d_size(i);

    for(j = 0; j <= temp; j++)
        certPEM[j] = (unsigned char)certDER[j];

    //store the device certificate in internal flash
    if(int_storage_write((uint8_t*)&certPEM[0], temp, DEVCERT_CFG,0) != SSP_SUCCESS)
    {
        print_to_console("Flash Write Failed\r\n");
        APP_ERR_TRAP(1);
    }
    else
        print_to_console("Device Certificate stored in flash\r\n");

    return((unsigned int)base64d_size(i));
}

/*********************************************************************************************************************
 * @brief  GetPrivKey function
 *
 * This function gets the private key from the user and store in the internal flash.
 *********************************************************************************************************************/
static unsigned int GetPrivKey(void)
{
    unsigned int i = 0, j =0;
    unsigned int temp;
    int skipBegin = 0;
    uint8_t str[66];
    char *begin_CA = "-----BEGIN RSA PRIVATE KEY-----";
    char *end_CA = "-----END RSA PRIVATE KEY-----";
    memset(certPEM,0,sizeof(certPEM));
    memset(certDER,0,sizeof(certDER));

    do {
        g_sf_console.p_api->read(g_sf_console.p_ctrl, &str[0], 66, TX_WAIT_FOREVER);

        if(!skipBegin)
        {
            skipBegin = 1;
            if(str[0] == '-')
            {
                i += strlen(begin_CA);
                strncpy((char*)&certPEM[0], (char*)&str[i],sizeof(str));
                i = (strlen((char*)str) - strlen(begin_CA));
            }

            continue;
        }
        else if(strcmp((char*)&str[0], end_CA) == 0)
            break;
        else
        {
            strncpy((char*)&certPEM[i], (char*)&str[0],sizeof(str));
        }

        i +=strlen((char*)str);

    }while(1);

    base64_decode(&certPEM[0], i ,&certDER[0]);

    temp = (unsigned int)base64d_size(i);

    for(j = 0; j <= temp; j++)
        certPEM[j] = (unsigned char)certDER[j];

    //store the private key in internal flash
    if(int_storage_write((uint8_t*)&certPEM[0], temp, PRI_KEY_CFG,0) != SSP_SUCCESS)
    {
        print_to_console("Flash Write Failed\r\n");
        APP_ERR_TRAP(1);
    }
    else
        print_to_console("Private key stored in flash\r\n");

    return((unsigned int)base64d_size(i));
}

/*********************************************************************************************************************
 * @brief  devCert_menu function
 *
 * This function display the certificates/keys menu option and wait for the user input.
 *********************************************************************************************************************/
static void devCert_menu(iot_input_cfg_t *gCloud_cfg)
{
    char data[128];

    do {
        print_to_console("\r\n");
        print_to_console("\r\n Certificate/Keys Settings Menu\r\n");
        print_to_console("\r\n 1. Enter rootCA Certificate\r\n 2. Enter Thing Certificate\r\n 3. Enter Thing Private Key\r\n 4. Exit\r\n");
        print_to_console("\r\n Please Enter Your Choice:");
        print_to_console(">");
        GetUserInput(&data[0]);

        if(data[0] == '1')
        {
            print_to_console("\r\n Enter rootCA Certificate: <paste rootCA and Press Enter key> \r\n");
            gCloud_cfg->cert_info.rootCA_len = GetrootCA();
        }
        else if(data[0] == '2')
        {
            print_to_console("\r\n Enter Thing Certificate: <paste device Cert and Press Enter key> \r\n");
            gCloud_cfg->cert_info.devCert_len = GetDevCert();
        }
        else if(data[0] == '3')
        {
            print_to_console("\r\n Enter Thing Private Key: <paste Thing private key and Press Enter key> \r\n");
            gCloud_cfg->cert_info.priKey_len = GetPrivKey();
        }
        else if(data[0] == '4')
            break;
        else
            print_to_console("Invalid Input !!! \r\n");

    }while(1);
}

static void parse_atcmd_resp(char *buf, int buf_len)
{
    int index = 0;
    char temp_buf[2048];

    memset(&temp_buf[0], 0 , sizeof(temp_buf));

    if(*buf != '\0')
    {
        if(strncmp(buf,"\r\nERROR\r\n",9) == 0)
        {
            strncpy(&temp_buf[0],"\r\nERROR\r\n",sizeof("\r\nERROR\r\n"));
            print_to_console(temp_buf);
            return;
        }
        /*
         * strip the wierd 'D' character received from cellular framework
         */

        for(index = 0; index < buf_len; index++)
        {
            if((buf[index] == 'O') && (buf[index + 1] == 'K'))
                break;
            else
                temp_buf[index] = buf[index];
        }

        temp_buf[index++] = 'O';
        temp_buf[index] = 'K';
        print_to_console(temp_buf);
        print_to_console("\r\n");
    }
    else
    {
         strncpy(&temp_buf[0],"\r\nERROR\r\n",sizeof("\r\nERROR\r\n"));
        print_to_console(temp_buf);
        return;
    }
}

static void atcmd_save(void)
{
    char data[256] = {0};
    at_cmd_save_stage_t at_cmd_stage = AT_CMD_STRING;
    at_cmd_t at_cmd;

    memset(&at_cmd,0,sizeof(at_cmd));
    memset(data,0,sizeof(data));

    do {
        /* Check if the user wants to save the AT commands for a carrier */
        print_to_console("\r\n Do you wish to store the AT commands for your carrier? [Y/N]: ");
        GetUserInput(&data[0]);

        if(data[0] == 'N' || data[0] == 'n')
            break;
        else if(data[0] == 'Y' || data[0] == 'y')
        {
            sq_number = 0;

            do
            {
                switch(at_cmd_stage)
                {
                    case AT_CMD_STRING:
                        print_to_console("\r\n***** Start Inserting AT Commands. Type exit to terminate!!!  *****\r\n");
                        print_to_console("\r\n AT Command: ");
                        GetUserInput(&data[0]);

                        /* Check whether user enter exit command to break the loop */
                        if((0 == strcmp(data, "EXIT")) || (0 == strcmp(data, "exit")))
                        {
                            /* Save the sequence number in internal flash */
                            if(SSP_SUCCESS != int_storage_write((uint8_t*)&sq_number, sizeof(sq_number), AT_CMD_CFG_TYPE,0))
                            {
                                print_to_console("\r\nFailed to store AT command sq_number!!!\r\n");
                            }
                            return;
                        }
                        else
                        {
                            if(strlen(data) < sizeof(at_cmd.cmd))
                            {
                                memcpy(at_cmd.cmd, data, strlen(data));
                                at_cmd_stage = AT_CMD_RESP_STRING;
                            }
                            else
                                print_to_console("\r\n Command is too large to save. Max allowed length is 125 bytes \r\n");
                        }

                        break;

                    case AT_CMD_RESP_STRING:
                        print_to_console ("\r\n");
                        print_to_console("Response <case sensitive>: ");
                        GetUserInput(&data[0]);

                        if(strlen(data) <  sizeof(at_cmd.cmd))
                        {
                            memcpy(at_cmd.resp, data, strlen(data));
                            at_cmd_stage = AT_CMD_RESP_WAIT_TIME;
                        }
                        else
                            print_to_console("\r\n Response is too large to save. Max allowed length is 125 bytes \r\n");
                        break;

                    case AT_CMD_RESP_WAIT_TIME:
                        print_to_console ("\r\n");
                        print_to_console("Response Wait time in MilliSeconds: ");
                        GetUserInput(data);
                        at_cmd.resp_waittime = (uint32_t)atoi(data);
                        at_cmd_stage = AT_CMD_RETRY_COUNT;
                        break;

                    case AT_CMD_RETRY_COUNT:
                        print_to_console ("\r\n");
                        print_to_console("Retry Count: ");
                        GetUserInput (data);
                        at_cmd.retry_cnt = (uint8_t)atoi(data);
                        at_cmd_stage = AT_CMD_RETRY_DELAY;
                        break;

                    case AT_CMD_RETRY_DELAY:
                        print_to_console ("\r\n");
                        print_to_console("Retry Delay in milli-seconds : ");
                        GetUserInput(data);

                        at_cmd.retry_delay = (uint16_t)atoi(data);

                        /* check AT command retry delay is within allowable maximum Limit */
                        if(at_cmd.retry_delay <= SF_CELLULAR_DELAY_2000MS)
                            at_cmd_stage = AT_CMD_SAVE;
                        else
                            print_to_console("\r\n Retry Delay is invalid. Max Allowed is 2000ms \r\n");
                        break;

                    case AT_CMD_SAVE:
                        print_to_console ("\r\n");
                        print_to_console("\r\n###################################################### \r\n");
                        print_to_console ("AT Command: ");
                        print_to_console ((char *) at_cmd.cmd);
                        print_to_console ("\r\n");
                        print_to_console ("Response string: ");
                        print_to_console ((char*)at_cmd.resp);
                        print_to_console ("\r\n");
                        sprintf(data, "Response Wait time: %d", (int)at_cmd.resp_waittime);
                        print_to_console (data);
                        print_to_console ("\r\n");
                        sprintf(data,"Retry Count: %d",at_cmd.retry_cnt);
                        print_to_console (data);
                        print_to_console ("\r\n");
                        sprintf(data,"Retry Delay: %d",at_cmd.retry_delay);
                        print_to_console (data);
                        print_to_console ("\r\n");
                        print_to_console("\r\n ###################################################### \r\n");

                        print_to_console ("Do you Want to save this AT Command ? [y/n]: ");

                        GetUserInput(data);

                        if((0 == strcmp(data,"y")) || (0 == strcmp(data, "Y")))
                        {
                            /* Save this command to internal flash */
                            if(SSP_SUCCESS != int_storage_write((uint8_t*)&at_cmd, sizeof(at_cmd_t), AT_CMD_INFO_TYPE, sq_number))
                            {
                                print_to_console("\r\nFailed to store AT command \r\n");
                                break;
                            }

                            ++sq_number;
                        }

                        memset(&at_cmd,0,sizeof(at_cmd_t));
                        at_cmd_stage = AT_CMD_STRING;
                        break;
                }

            }while(1);

        }
        else
            print_to_console("\r\nInvalid Input!!!\r\n");

    }while(1);

}

static void cell_cfg_user_atcmd(void)
{
    ssp_err_t result = SSP_SUCCESS;
    at_cmd_t usr_atcmd;
    uint8_t index = 0;
    char rx_data[100] = {0};
    sf_cellular_cmd_resp_t send_cmd, reci_cmd;
    uint8_t retry_cnt = 0;

    print_to_console("\r\n");
    print_to_console("\r\n #################################################\r\n");
    do
    {
        result = int_storage_read((uint8_t *)&usr_atcmd, sizeof(at_cmd_t), AT_CMD_INFO_TYPE, index);
        if(result == SSP_SUCCESS)
        {
            /* Append String termination for modem */
            strcat(usr_atcmd.cmd,"\r\n");

            send_cmd.p_buff = (uint8_t*)usr_atcmd.cmd;
            send_cmd.buff_len = (uint16_t)strlen((const char*)usr_atcmd.cmd);
            reci_cmd.p_buff = (uint8_t *)rx_data;
            reci_cmd.buff_len = sizeof(rx_data);

            do
            {
                print_to_console("\r\n Command: ");
                print_to_console((char*)send_cmd.p_buff);
                print_to_console("\r\n");

                result = g_sf_cellular0.p_api->commandSend(g_sf_cellular0.p_ctrl,
                                                           &send_cmd,
                                                           &reci_cmd,
                                                           usr_atcmd.resp_waittime);

                if ((result == SSP_SUCCESS) || (result == SSP_ERR_TIMEOUT))
                {
                    /* Parse response of modem */
                    if(sf_cellular_is_str_present((const char *) reci_cmd.p_buff,
                                                  (const char *) (uint8_t *) usr_atcmd.resp) == SF_CELLULAR_TRUE)
                    {
                        break;
                    }
                    else
                        print_to_console("\r\nIncorrect response to AT command!!!!\r\n");
                }
                else
                    print_to_console("\r\n Failed to read Cellular modem response!!!!\r\n");

                /* Delay before next try */
                sf_cellular_msec_delay(usr_atcmd.retry_delay);
                ++retry_cnt;

            }while(retry_cnt < usr_atcmd.retry_cnt);

            if(usr_atcmd.retry_cnt)
            {
                if(retry_cnt >= usr_atcmd.retry_cnt)
                {
                    print_to_console("User AT command failed!!!!\r\n");
                    break;
                }
            }

            parse_atcmd_resp(&rx_data[0], sizeof(rx_data));

            if(result == SSP_SUCCESS)
            {
                /* Goto next command*/
               index++;
               retry_cnt = 0;
               --sq_number;
            }
            else
                break;
        }
        else
        {
            print_to_console("Failed to read User AT commands from Flash!!!\r\n");
            break;
        }

    }while(sq_number);
}



static ssp_err_t activate_atcmd_shell(void)
{
    ssp_err_t result = SSP_SUCCESS;
    char at_cmd_send[256] = {0};
    char at_cmd_resp[256] = {0};

    sf_cellular_cmd_resp_t send_cmd;
    sf_cellular_cmd_resp_t reci_cmd;

    print_to_console("\r\n Entering AT command shell. Type 'exit' to terminate the shell \r\n");

    /*Handle AT commands entered by User in at_shell>> prompt*/
    do
    {
        memset(at_cmd_send, '\0', sizeof(at_cmd_send));
        memset(at_cmd_resp, '\0', sizeof(at_cmd_resp));

        print_to_console("\r\nat_shell>>");
        GetUserInput((char*)at_cmd_send);
        strcat(at_cmd_send,"\r\n");

        reci_cmd.p_buff = (uint8_t *)at_cmd_resp;
        reci_cmd.buff_len = sizeof(at_cmd_resp);

        send_cmd.p_buff = (uint8_t*)at_cmd_send;
        send_cmd.buff_len = strlen(at_cmd_send);

        /* Handle a case when user inputs enter key */
        if(strlen(at_cmd_send) == strlen("\r\n"))
            continue;

        /* Check for exit command from user to terminate shell interface */
        if((0 == strcmp(at_cmd_send, "EXIT\r\n")) || (0 == strcmp(at_cmd_send,"exit\r\n")))
        {
            break;
        }

        /* Write command to the driver */
        result = g_sf_cellular0.p_api->commandSend(g_sf_cellular0.p_ctrl, &send_cmd,
                                                   &reci_cmd, SF_CELLULAR_SERIAL_READ_TIMEOUT_TICKS);
        if(result != SSP_SUCCESS)
            print_to_console("Failed to execute AT command\r\n");
        else
        {
            parse_atcmd_resp(&at_cmd_resp[0], sizeof(at_cmd_resp));
        }

    }while(1);

    return result;
}

/*********************************************************************************************************************
 * @brief  config_menu_callback function
 *
 * This function handles the cwiz user command option from CLI
 *********************************************************************************************************************/
void config_menu_callback(sf_console_cb_args_t *p_args)
{
    uint8_t result = NETIF_MAX;
    e_config_state net_wizard_state = STATE_NETIF_SELECTION;
    uint8_t menu_cfg = 0, addr_mode = 0, cell_cfg_mode = 0;
    net_input_cfg_t net_cfg;
    iot_input_cfg_t iot_cfg;
    char str[80];
    uint8_t iot_serv_select = 0;

    SSP_PARAMETER_NOT_USED(p_args);

    //clear the network input cfg structure
    memset(&net_cfg,0,sizeof(net_cfg));
    memset(&iot_cfg,0,sizeof(iot_cfg));

    while(menu_cfg != STATE_CONFIG_EXIT)
    {
        menu_cfg = main_menu();

        switch(menu_cfg)
        {
            case NET_CONFIG:

                net_wizard_state = STATE_NETIF_SELECTION;

                while(net_wizard_state != STATE_CONFIG_EXIT)
                {
                    switch(net_wizard_state)
                    {
                        case STATE_NETIF_SELECTION:
                            //Show Network selection menu
                            result = network_select_menu();

                            switch(result)
                            {
                                case ETHERNET:
                                    net_cfg.netif_valid = 1;
                                    net_cfg.interface_index = 0;
                                    strncpy((char*)net_cfg.netif_select, "Ethernet",sizeof("Ethernet"));
                                    net_wizard_state = STATE_ADDR_MODE_CONFIG;
                                    break;

                                case WIFI:
                                    net_cfg.netif_valid = 1;
                                    net_cfg.interface_index = 1;
                                    strncpy((char*)net_cfg.netif_select, "WiFi",sizeof("WiFi"));
                                    net_wizard_state = STATE_WIFI_CONFIG;
                                    break;

                                case CELLULAR:
                                    net_cfg.netif_valid = 1;
                                    net_cfg.interface_index = 2;
                                    strncpy((char*)net_cfg.netif_select, "Cellular",sizeof("Cellular"));
                                    net_wizard_state = STATE_CELLULAR_CONFIG;
                                    break;
                                case EXIT_NET_MENU:
                                    net_wizard_state = STATE_CONFIG_EXIT;
                                    break;
                                default:
                                    print_to_console("\r\nInvalid Input !!!\r\n");
                            }

                            break;

                        case STATE_WIFI_CONFIG:
                            /*Display Wi-Fi Configuration menu */
                            if(wifi_config_menu(&net_cfg))
                            {
                                net_wizard_state = STATE_ADDR_MODE_CONFIG;
                            }

                            break;

                        case STATE_CELLULAR_CONFIG:
                            cell_cfg_mode = cellular_config_main_men();
                            if(cell_cfg_mode == CELLULAR_CONFIG_MENU)
                            {
                                print_to_console("\r\nOpening Cellular module instance....");

                                /* Open Cellular Framework instance */
                                result = g_sf_cellular0.p_api->open(g_sf_cellular0.p_ctrl, g_sf_cellular0.p_cfg);
                                if(result != SSP_SUCCESS)
                                    print_to_console("Failed!!!\r\n");
                                else
                                {
                                    print_to_console("done\r\n");

                                    cell_cfg_mode = cellular_config_menu();

                                    if(cell_cfg_mode == CELLULAR_MANUAL_CONFIG)
                                    {
                                        /* Activate the Shell for user to explore SIM configuration using AT commands */
                                        if(activate_atcmd_shell() != SSP_SUCCESS)
                                            print_to_console("\r\n Failed to open AT command shell!!!! \r\n");
                                        else
                                        {
                                            /* Check if user wants to save AT commands for new carrier */
                                            atcmd_save();
                                        }
                                    }
                                    else if(cell_cfg_mode == CELLULAR_AUTO_CFG)
                                    {
                                        /* Check if the user stored AT commands using manual mode */
                                        cell_cfg_mode = cellular_autocfg_menu(0);
                                        if(cell_cfg_mode == 1)
                                        {
                                            if(SSP_SUCCESS != int_storage_read((uint8_t *)&sq_number, sizeof(sq_number), AT_CMD_CFG_TYPE, 0))
                                            {
                                                print_to_console("\r\nFailed to read the sq_number from internal flash\r\n");
                                            }
                                            else
                                            {
                                                if(sq_number > 0)
                                                    cell_cfg_user_atcmd();
                                                else
                                                    print_to_console("\r\n No AT Commands stored by User in data flash!!!\r\n");
                                            }
                                        }
                                    }

                                    result = g_sf_cellular0.p_api->close(g_sf_cellular0.p_ctrl);
                                    if(result != SSP_SUCCESS)
                                        print_to_console("Failed to close Cellular Module instance!!!!\r\n");
                                }

                                net_wizard_state = STATE_CONFIG_EXIT;
                            }
                            else if(cell_cfg_mode == CELLULAR_PROV_MENU)
                            {
                                /* Display Cellular Configuration menu */
                                if(cellular_prov_menu(&net_cfg))
                                {
                                    /*Store the cellular config info in the internal flash */
                                    if(int_storage_write((uint8_t*)(&net_cfg), sizeof(net_cfg), NET_INPUT_CFG, 0) != SSP_SUCCESS)
                                    {
                                        print_to_console("\r\nFlash Write Failed!!!\r\n");
                                        APP_ERR_TRAP(1);
                                    }
                                    else
                                    {
                                        net_wizard_state = STATE_CONFIG_EXIT;
                                    }
                                }
                            }

                            break;
                        case STATE_ADDR_MODE_CONFIG:
                            /* IP Address Configuration mode selection */
                            addr_mode = ip_mode_select_menu();
                            if(addr_mode != IOTKIT_ADDR_MODE_NOTCONFIG)
                            {
                                net_wizard_state = STATE_CONFIG_EXIT;

                                if(addr_mode == IOTKIT_ADDR_MODE_DHCP)
                                    net_cfg.netif_addr_mode = IOTKIT_ADDR_MODE_DHCP;
                                else
                                {
                                    net_cfg.netif_addr_mode = IOTKIT_ADDR_MODE_STATIC;

                                    ip_mode_static_menu(&net_cfg);
                                }

                                //write the netif information in the flash
                                if(int_storage_write((uint8_t*)&net_cfg, sizeof(net_cfg), NET_INPUT_CFG, 0) != SSP_SUCCESS)
                                {
                                    print_to_console("NETCFG Flash Write Failed\r\n");
                                    APP_ERR_TRAP(1);
                                }
                                else
                                    print_to_console("Network Configuration stored in flash\r\n");
                            }
                            else
                            {
                                net_wizard_state = STATE_ADDR_MODE_CONFIG;
                            }

                            break;

                        default:
                            print_to_console("\r\nInvalid Input !!!\r\n");
                            break;
                    }
                }

                break;

            case CLOUD_CONFIG:

                do {
                    iot_serv_select = iot_service_submenu();

                    if(iot_serv_select == 1)
                    {
                        gcloud_submenu(&iot_cfg);
                        iot_cfg.iotserv_valid = 1;
                    }
                    else if(iot_serv_select == 2)
                    {
                        devCert_menu(&iot_cfg);
                        iot_cfg.iotserv_valid = 1;
                    }

                }while(iot_serv_select != 3);

                if(iot_cfg.iotserv_valid == 1)
                {
                    //write the iot service information in the flash
                    if(int_storage_write((uint8_t*)&iot_cfg, sizeof(iot_cfg), IOT_INPUT_CFG, 0) != SSP_SUCCESS)
                    {
                        print_to_console("\r\nFlash Write Failed!!!\r\n");
                        APP_ERR_TRAP(1);
                    }
                    else
                    {
                        print_to_console("\r\nDevice Certificate information stored in flash\r\n");
                    }
                }
                break;

            case DUMP_CONFIG:

                if(SSP_SUCCESS != int_storage_read((uint8_t *)&net_cfg, sizeof(net_cfg), NET_INPUT_CFG, 0))
                {
                    print_to_console("\r\nFlash read failed!!!\r\n");
                    APP_ERR_TRAP(1);
                }

                if(SSP_SUCCESS != int_storage_read((uint8_t*)&iot_cfg, sizeof(iot_cfg),IOT_INPUT_CFG, 0))
                {
                    print_to_console("\r\nFlash read failed!!!\r\n");
                    APP_ERR_TRAP(1);
                }

                print_to_console("\r\n");
                print_to_console("\r\n ################### Flash Dump Start#########################\r\n");

                if(net_cfg.netif_valid)
                {
                    sprintf(str,"\r\nNetwork Interface selected: %s", net_cfg.netif_select);
                    print_to_console(str);
                    print_to_console("\r\n");

                    if(net_cfg.netif_addr_mode == IOTKIT_ADDR_MODE_DHCP)
                    {
                        sprintf(str,"IP Mode: %s", "DHCP");
                        print_to_console(str);
                        print_to_console("\r\n");
                    }
                    else
                    {
                        sprintf(str,"IP Mode: %s", "Static");
                        print_to_console(str);
                        print_to_console("\r\n");
                        print_static_addr(net_cfg);
                    }

                    if(strcmp((char*)net_cfg.netif_select, "WiFi") == 0)
                    {
                        print_to_console("\r\n");
                        print_to_console("WiFi Configuration\r\n");
                        snprintf(str, sizeof(str), "  SSID        : %s\r\n", net_cfg.wifi_prov.ssid);
                        print_to_console(str);
                        snprintf(str, sizeof(str), "  Key         : %s\r\n", net_cfg.wifi_prov.key);
                        print_to_console(str);

                        snprintf(str, sizeof(str), "  Security    : ");
                        if (net_cfg.wifi_prov.security == SF_WIFI_SECURITY_TYPE_WPA2)
                            strcat(str, "WPA2");
                        else if (net_cfg.wifi_prov.security == SF_WIFI_SECURITY_TYPE_OPEN)
                            strcat(str, "Open");
                        else if (net_cfg.wifi_prov.security == SF_WIFI_SECURITY_TYPE_WEP)
                            strcat(str, "WEP");
                        else if (net_cfg.wifi_prov.security == SF_WIFI_SECURITY_TYPE_WPA)
                            strcat(str, "WPA");
                        else
                            strcat(str, "Unknown");

                        print_to_console(str);
                        print_to_console("\r\n");
                        print_to_console("\r\n");
                     }

                     else if(strcmp((char*)net_cfg.netif_select, "Cellular") == 0)
                     {
                         print_to_console("\r\n");
                         print_to_console("Cellular Configuration\r\n");
                         print_to_console("\r\n APN: ");
                         print_to_console((char*)net_cfg.cell_prov.apn);
                         print_to_console("\r\n");

                         print_to_console("\r\n Context ID: ");
                         snprintf(str, sizeof(str), "%d", net_cfg.cell_prov.context_id);
                         print_to_console(str);
                         print_to_console("\r\n");

                         print_to_console("\r\n PDP Type: ");
                         switch(net_cfg.cell_prov.pdp_type)
                         {
                             case SF_CELLULAR_PDP_TYPE_IP:
                                 print_to_console("IP\r\n");
                                 break;

                             case SF_CELLULAR_PDP_TYPE_IPV4V6:
                                 print_to_console("IPV4V6\r\n");
                                 break;
                             default:
                                 print_to_console("Unknown PDP type\r\n");
                                 break;
                         }

                         print_to_console("\r\n");
                     }
                }
                else
                    print_to_console("No Network Interface selected. Run cwiz command\r\n");

                if(iot_cfg.iotserv_valid)
                {
                    snprintf(str,sizeof(str), "GCloud Endpoint:  %s", iot_cfg.gCloud_info.endp_address);
                    print_to_console(str);
                    print_to_console("\r\n");
                    snprintf(str,sizeof(str), "GCloud Project ID: %s", iot_cfg.gCloud_info.project_id);
                    print_to_console(str);
                    print_to_console("\r\n");
                    snprintf(str,sizeof(str), "GCloud Device ID: %s", iot_cfg.gCloud_info.device_id);
                    print_to_console(str);
                    print_to_console("\r\n");
                    snprintf(str,sizeof(str), "GCloud Cloud Region: %s", iot_cfg.gCloud_info.cloud_region);
                    print_to_console(str);
                    print_to_console("\r\n");
                    snprintf(str,sizeof(str), "GCloud Registry Id: %s", iot_cfg.gCloud_info.registry_id);
                    print_to_console(str);
                    print_to_console("\r\n");
                }

                print_to_console("\r\n ################## Flash Dump End #########################\r\n");
                print_to_console("\r\n");
                break;

            case MAIN_CONFIG_EXIT:
                menu_cfg = STATE_CONFIG_EXIT;
                print_to_console("\r\n");
                break;

            default:
                print_to_console("\r\nInvalid Input !!!\r\n");
                break;
        }
    }
}

/*********************************************************************************************************************
 * @brief  demo_service_callback function
 *
 * This function handles the demo command option from CLI
 *********************************************************************************************************************/
void demo_service_callback(sf_console_cb_args_t *p_args)
{
    net_input_cfg_t user_cfg;
    iot_input_cfg_t iot_cfg;
    ssp_err_t ssp_err;

    if((strcmp((void*)p_args->p_remaining_string, "start") == 0))
    {
        ssp_err = int_storage_read((uint8_t *)&user_cfg, sizeof(user_cfg), NET_INPUT_CFG, 0);
        if(ssp_err != SSP_SUCCESS)
        {
            print_to_console("Flash read failed\r\n");
            return;
        }

        if(SSP_SUCCESS != int_storage_read((uint8_t*)&iot_cfg, sizeof(iot_cfg),IOT_INPUT_CFG, 0))
        {
            print_to_console("Flash read failed\r\n");
            APP_ERR_TRAP(1);
        }

        if(user_cfg.netif_valid == 0 )
        {
            print_to_console("Network Interface is not selected. Run cwiz command\r\n");
            return;
        }

        if(iot_cfg.iotserv_valid == 0)
        {
            print_to_console("IoT service is not selected. Run cwiz command\r\n");
            return;
        }

        tx_event_flags_set(&g_user_event_flags, DEMO_START_FLAG, TX_OR);
    }
    else if(strcmp((void*)p_args->p_remaining_string, "stop") == 0)
        tx_event_flags_set(&g_user_event_flags, DEMO_STOP_FLAG, TX_OR);
    else
        print_to_console("Invalid Argument\r\n");
}

static const sf_console_command_t g_sf_console_commands[] =
{
     {
       .command = (uint8_t *)"cwiz",
       .help = (uint8_t *)"Network/Cloud Configuration Menu \r\n"
               "           Usage: cwiz \r\n",
               .callback = config_menu_callback,
               .context = NULL
      },
      {
       .command = (uint8_t*)"demo",
       .help = (uint8_t*)"Start/Stop Synergy Cloud Connectivity Demo \r\n"
             "          Usage: demo <start>/<stop> \r\n",
             .callback = demo_service_callback,
             .context = NULL
      },
};

const sf_console_menu_t g_sf_console_root_menu =
{
 .menu_prev = NULL,
 .menu_name = (uint8_t *)"",
 .num_commands = (sizeof(g_sf_console_commands)) / (sizeof(g_sf_console_commands[0])),
 .command_list = &g_sf_console_commands[0]
};

/* Center the string that needs to be displayed */
static void center_and_print_string(char *str)
{
    unsigned int len = strlen(str), pad, i;

    print_to_console("*");
    /* Length of line for most terminal programs is 80 columns.
     * We leave a space at the end since some programs wrap the line
     * after 79 characters. There is an asterisk at the start and end
     * of each line. Therefore effective line width is 77 columns.
     */
    pad = (77 - len) / 2;
    for (i = 0; i < pad; i++)
        print_to_console(" ");
    print_to_console(str);

    /* Padding may be unequal, adjust the trailing spaces */
    if((2 * pad) + len != 77)
        pad += 77 - ((2 * pad) + len);
    for (i = 0; i < pad; i++)
        print_to_console(" ");

    print_to_console("\r\n");
}

/*********************************************************************************************************************
 * @brief  cellular_module_reset function
 *
 * This function resets cellular module.
 ********************************************************************************************************************/
static void cellular_module_reset(ioport_port_pin_t pin_reset)
{
    g_ioport.p_api->pinWrite(pin_reset, IOPORT_LEVEL_HIGH);
    sf_cellular_msec_delay(SF_CELLULAR_MODULE_RESET_DELAY_MS);

    g_ioport.p_api->pinWrite(pin_reset, IOPORT_LEVEL_LOW);
    sf_cellular_msec_delay(SF_CELLULAR_MODULE_RESET_DELAY_MS);
}

static void cellular_module_powerup(ioport_port_pin_t pwr_pin)
{
    g_ioport.p_api->pinWrite(pwr_pin, IOPORT_LEVEL_HIGH);
    sf_cellular_msec_delay(200);
    g_ioport.p_api->pinWrite(pwr_pin, IOPORT_LEVEL_LOW);
}

static void BG96_init(void)
{
    /* Power Cellular Module */
    cellular_module_powerup(CELLULAR_MODULE_POWER_PIN);

    /* Hard reset cellular module */
    cellular_module_reset(CELLULAR_MODULE_RESET_PIN);
}

/* Console Thread entry function */
void console_thread_entry(void)
{
    /* BEGIN MODIFIED */

    // uint8_t ch[1];

    /* END MODIFIED */

    char str[96];
    ssp_pack_version_t ssp_version;

    /* BEGIN MODIFIED */

    // g_sf_console.p_api->read (g_sf_console.p_ctrl, ch, 1, TX_WAIT_FOREVER);

    if (console_connected) {
        tx_thread_sleep (10);   // delay to allow enumeration to complete
    }

    /* END MODIFIED */

    R_SSP_VersionGet(&ssp_version);

    print_to_console ("\r\n********************************************************************************\r\n");
    snprintf(str, sizeof(str), "Renesas Synergy GCloud IoT Cloud Connectivity Application\r\n" );
    center_and_print_string(str);
    snprintf(str, sizeof(str), "FW version  %d.%d.%d  -  %s, %s\r\n", MQTT_AP_MAJOR_VER, MQTT_AP_MINOR_VER,MQTT_AP_PATCH_VER, __DATE__, __TIME__);
    center_and_print_string(str);
    snprintf(str, sizeof(str),"Synergy Software Package Version: %d.%d.%d", ssp_version.major, ssp_version.minor, ssp_version.patch);
    center_and_print_string(str);
    print_to_console ("\r\n********************************************************************************\r\n");

    int_storage_init();

    print_to_console("\r\nPowering up BG96 Shield....");

    //Initialize BG96 shield. This needs to be done before GPS initialization
    BG96_init();

    print_to_console("done\r\n");

    init_sensors();

    /* BEGIN ADDED */

    // If the console is not connected, automatically initiate the equivalent
    // of the "demo start" command. Otherwise when the console is connected, use
    // "demo start" on the console to start sensor processing.

    if (!console_connected) {

        sf_console_callback_args_t args = {
          .p_ctrl = NULL,
          .context = NULL,
          .p_remaining_string = (uint8_t const *)"start",
          .bytes = 6
        };

        demo_service_callback(&args);
    }

    /* END ADDED */

    while (true)
    {
        /* BEGIN ADDED */

        if (console_connected) {

        /* END ADDED */

        g_sf_console.p_api->prompt(g_sf_console.p_ctrl, NULL, TX_WAIT_FOREVER);

        /* BEGIN ADDED */

        }

        /* END ADDED */

        tx_thread_sleep (1);
    }
}
