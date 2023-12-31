/*
   BLE peripheral

*/

/****************************************************************************
   Included Files
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <bluetooth/ble_gatt.h>
#include "system/readline.h"
/* include the GNSS library */
#include <GNSS.h>
#define STRING_BUFFER_SIZE  128       /**< %Buffer size */
#define RESTART_CYCLE       (60 * 5)  /**< positioning test term */
SpGnss Gnss;                   /**< SpGnss object */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <bluetooth/ble_gatt.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BLE_UUID_SDS_SERVICE_IN  0x3802
#define BLE_UUID_SDS_CHAR_IN     0x4a02

#define BONDINFO_FILENAME "/mnt/spif/BONDINFO"



/****************************************************************************
 * Private Data
 ****************************************************************************/
 char set_pos(SpNavData *pNavData);


BT_ADDR local_addr               = {{0x19, 0x84, 0x06, 0x14, 0xAB, 0xCD}};

char local_ble_name[BT_NAME_LEN] = "SPR-PERIPHERAL";

struct ble_gatt_service_s *g_ble_gatt_service;

BLE_ATTR_PERM attr_param =
  {
    .readPerm  = BLE_SEC_MODE1LV2_NO_MITM_ENC,
    .writePerm = BLE_SEC_MODE1LV2_NO_MITM_ENC
  };

uint8_t char_data[BLE_MAX_CHAR_SIZE];

BLE_CHAR_VALUE char_value =
  {
    .length = BLE_MAX_CHAR_SIZE
  };

BLE_CHAR_PROP char_property =
  {
    .read   = 1,
    .write  = 1,  
    .notify = 1
  };
/****************************************************************************
 * Function Prototypes BLE
 ****************************************************************************/
/* BLE common callbacks */
/* Connection status change */
void onLeConnectStatusChanged(struct ble_state_s *ble_state,
                                     bool connected, uint8_t reason);
/* Device name change */
void onConnectedDeviceNameResp(const char *name);
/* Save bonding information */
void onSaveBondInfo(int num, struct ble_bondinfo_s *bond);
/* Load bonding information */
int onLoadBondInfo(int num, struct ble_bondinfo_s *bond);
/* Negotiated MTU size */
void onMtuSize(uint16_t handle, uint16_t sz);
/* Encryption result */
void onEncryptionResult(uint16_t, bool result);
/* BLE GATT callbacks */
/* Write request */
void onWrite(struct ble_gatt_char_s *ble_gatt_char);
/* Read request */
void onRead(struct ble_gatt_char_s *ble_gatt_char);
/* Notify request */
void onNotify(struct ble_gatt_char_s *ble_gatt_char, bool enable);
/* BLE GATT callbacks */
/* Write request */
void onWrite(struct ble_gatt_char_s *ble_gatt_char);
/* Read request */
void onRead(struct ble_gatt_char_s *ble_gatt_char);
/* Notify request */
void onNotify(struct ble_gatt_char_s *ble_gatt_char, bool enable);

int g_ble_bonded_device_num;
struct ble_cccd_s **g_cccd = NULL;
bool ble_is_notify_enabled = false;
uint16_t ble_conn_handle;
struct ble_common_ops_s ble_common_ops =
  {
    .connect_status_changed     = onLeConnectStatusChanged,
    .connected_device_name_resp = onConnectedDeviceNameResp,
    .mtusize                    = onMtuSize,
    .save_bondinfo              = onSaveBondInfo,
    .load_bondinfo              = onLoadBondInfo,
    .encryption_result          = onEncryptionResult,
  };

struct ble_gatt_peripheral_ops_s ble_gatt_peripheral_ops =
  {
    .write  = onWrite,
    .read   = onRead,
    .notify = onNotify
  };


struct ble_gatt_char_s g_ble_gatt_char =
  {
    .handle = 0,
    .ble_gatt_peripheral_ops = &ble_gatt_peripheral_ops
  };

/****************************************************************************
 * Private Functions
 ****************************************************************************/
  
void onLeConnectStatusChanged(struct ble_state_s *ble_state,
                                      bool connected, uint8_t reason)
{
  BT_ADDR addr = ble_state->bt_target_addr;

  /* If receive connected status data, this function will call. */

  printf("[BLE_GATT] Connect status ADDR:%02X:%02X:%02X:%02X:%02X:%02X, status:%s, reason:%d\n",
          addr.address[5], addr.address[4], addr.address[3],
          addr.address[2], addr.address[1], addr.address[0],
          connected ? "Connected" : "Disconnected", reason);

  ble_conn_handle = ble_state->ble_connect_handle;
}

void onConnectedDeviceNameResp(const char *name)
{
  /* If receive connected device name data, this function will call. */

  printf("%s [BLE] Receive connected device name = %s\n", __func__, name);
}

void onSaveBondInfo(int num, struct ble_bondinfo_s *bond)
{
  int i;
  FILE *fp;
  int sz;

  /* In this example, save the parameter `num` and each members of
   * the parameter `bond` in order to the file.
   */

  fp = fopen(BONDINFO_FILENAME, "wb");
  if (fp == NULL)
    {
      printf("Error: could not create file %s\n", BONDINFO_FILENAME);
      return;
    }

  fwrite(&num, 1, sizeof(int), fp);

  for (i = 0; i < num; i++)
    {
      fwrite(&bond[i], 1, sizeof(struct ble_bondinfo_s), fp);

      /* Because only cccd is pointer member, save it individually. */

      sz = bond[i].cccd_num * sizeof(struct ble_cccd_s);
      fwrite(bond[i].cccd, 1, sz, fp);
    }

  fclose(fp);
}

int onLoadBondInfo(int num, struct ble_bondinfo_s *bond)
{
  int i;
  FILE *fp;
  int stored_num;
  int sz;
  size_t ret;

  fp = fopen(BONDINFO_FILENAME, "rb");
  if (fp == NULL)
    {
      return 0;
    }

  ret = fread(&stored_num, 1, sizeof(int), fp);
  if (ret != sizeof(int))
    {
      printf("Error: could not load due to %s read error.\n",
             BONDINFO_FILENAME);
      fclose(fp);
      return 0;
    }

  g_ble_bonded_device_num = (stored_num < num) ? stored_num : num;
  sz = g_ble_bonded_device_num * sizeof(struct ble_cccd_s *);
  g_cccd = (struct ble_cccd_s **)malloc(sz);
  if (g_cccd == NULL)
    {
      printf("Error: could not load due to malloc error.\n");
      g_ble_bonded_device_num = 0;
    }

  for (i = 0; i < g_ble_bonded_device_num; i++)
    {
      ret = fread(&bond[i], 1, sizeof(struct ble_bondinfo_s), fp);
      if (ret != sizeof(struct ble_bondinfo_s))
        {
          printf("Error: could not load all data due to %s read error.\n",
                 BONDINFO_FILENAME);
          printf("The number of loaded device is %d\n", i);
          g_ble_bonded_device_num = i;
          break;
        }

      if (bond[i].cccd_num > 1)
        {
          printf("Error: could not load all data due to invalid data.\n");
          printf("cccd_num does not exceed the number of characteristics\n");
          printf("that is set by this application.\n");
          printf("The number of loaded device is %d\n", i);

          g_ble_bonded_device_num = i;
          break;
        }

      /* Because only cccd is pointer member, load it individually. */

      sz = bond[i].cccd_num * sizeof(struct ble_cccd_s);
      g_cccd[i] = (struct ble_cccd_s *)malloc(sz);

      if (g_cccd[i] == NULL)
        {
          printf("Error: could not load all data due to malloc error.");
          printf("The number of loaded device is %d\n", i);

          g_ble_bonded_device_num = i;
          break;
        }

      bond[i].cccd = g_cccd[i];
      ret = fread(bond[i].cccd, 1, sz, fp);
      if (ret != sz)
        {
          printf("Error: could not load all data due to %s read error.\n",
                 BONDINFO_FILENAME);
          printf("The number of loaded device is %d\n", i);
          g_ble_bonded_device_num = i;
          break;
        }
    }

  fclose(fp);

  return g_ble_bonded_device_num;
}

void free_cccd(void)
{
  int i;

  if (g_cccd)
    {
      for (i = 0; i < g_ble_bonded_device_num; i++)
        {
          if (g_cccd[i])
            {
              free(g_cccd[i]);
            }
        }

      free(g_cccd);
      g_cccd = NULL;
    }
}

void onMtuSize(uint16_t handle, uint16_t sz)
{
  printf("negotiated MTU size(connection handle = %d) : %d\n", handle, sz);
}

void onEncryptionResult(uint16_t handle, bool result)
{
  printf("Encryption result(connection handle = %d) : %s\n",
         handle, (result) ? "Success" : "Fail");
}

void show_uuid(BLE_UUID *uuid)
{
  int i;

  printf("uuid : ");

  switch (uuid->type)
    {
      case BLE_UUID_TYPE_UUID128:

        /* UUID format YYYYYYYY-YYYY-YYYY-YYYY-YYYYYYYYYYYY */

        for (i = 0; i < BT_UUID128_LEN; i++)
          {
            printf("%02x", uuid->value.uuid128.uuid128[BT_UUID128_LEN - i - 1]);
            if ((i == 3) || (i == 5) || (i == 7) || (i == 9))
              {
                printf("-");
              }
          }

        printf("\n");

        break;

      case BLE_UUID_TYPE_BASEALIAS_BTSIG:
      case BLE_UUID_TYPE_BASEALIAS_VENDOR:

        /* UUID format XXXX */

        printf("%04x\n", uuid->value.alias.uuidAlias);
        break;

      default:
        printf("Irregular UUID type.\n");
        break;
    }
}

void onWrite(struct ble_gatt_char_s *ble_gatt_char)
{
  int i;

  /* If receive connected device name data, this function will call. */

  printf("%s [BLE] start\n", __func__);
  printf("handle : %d\n", ble_gatt_char->handle);
  show_uuid(&ble_gatt_char->uuid);
  printf("value_len : %d\n", ble_gatt_char->value.length);
  printf("value : ");
  for (i = 0; i < ble_gatt_char->value.length; i++)
    {
      printf("%02x ", ble_gatt_char->value.data[i]);
    }

  printf("\n");

  printf("%s [BLE] end\n", __func__);
}

void onRead(struct ble_gatt_char_s *ble_gatt_char)
{
  /* If receive connected device name data, this function will call. */

  printf("%s [BLE] \n", __func__);
}

void onNotify(struct ble_gatt_char_s *ble_gatt_char, bool enable)
{
  /* If receive connected device name data, this function will call. */

  printf("%s [BLE] start \n", __func__);
  printf("handle : %d\n", ble_gatt_char->handle);
  show_uuid(&ble_gatt_char->uuid);

  if (enable)
    {
      printf("notification enabled\n");
      ble_is_notify_enabled = true;
    }
  else
    {
      printf("notification disabled\n");
      ble_is_notify_enabled = false;
    }

  printf("%s [BLE] end \n", __func__);
}

/* Do not used now */
void ble_peripheral_exit(void)
{
  int ret;

  /* Turn OFF BT */

  ret = bt_disable();
  if (ret != BT_SUCCESS)
    {
      printf("%s [BT] BT disable failed. ret = %d\n", __func__, ret);
    }

  /* Finalize BT */

  ret = bt_finalize();
  if (ret != BT_SUCCESS)
    {
      printf("%s [BT] BT finalize failed. ret = %d\n", __func__, ret);
    }
}


/****************************************************************************
   Pre-processor Definitions GNSS
 ****************************************************************************/

#define BLE_UUID_SDS_SERVICE_IN  0x3802
#define BLE_UUID_SDS_CHAR_IN     0x4a02
#define BONDINFO_FILENAME "/mnt/spif/BONDINFO"

void ble_settings();
void Led_isActive();
void Led_isPosfix(bool state);
void print_pos(SpNavData *pNavData);
void print_condition(SpNavData *pNavData);
void setup_start_led_driver();
void setup_gnss(int begin_message);
void setup_gnss_cold_start(int begin_message);
void setup_done_led_driver();
char set_pos(SpNavData *pNavData);


  
void setup() {
   /* put your setup code here, to run once: */
  int error_flag = 0;
  /* Set serial baudrate. */
  Serial.begin(115200);
  /* Wait HW initialization done. */
  sleep(3);
  /* Turn on all LED:Setup start. */
 setup_start_led_driver();
  /* Set Debug mode to Info */
 Gnss.setDebugMode(PrintInfo);
  /* Activate GNSS device */
  int result;
  result = Gnss.begin();
  setup_gnss(result);
  /* Start positioning */
  result = Gnss.start(COLD_START);
  setup_gnss_cold_start(result);
  setup_done_led_driver();
  int ret = 0;
  BLE_UUID *s_uuid;
  BLE_UUID *c_uuid;

  /* Initialize BT HAL */

  ret = bt_init();
  if (ret != BT_SUCCESS)
    {
      printf("%s [BT] Initialization failed. ret = %d\n", __func__, ret);
      goto error;
    }

  /* Register BLE common callbacks */

  ret = ble_register_common_cb(&ble_common_ops);
  if (ret != BT_SUCCESS)
    {
      printf("%s [BLE] Register common call back failed. ret = %d\n", __func__, ret);
      goto error;
    }

  /* Turn ON BT */

  ret = bt_enable();
  if (ret != BT_SUCCESS)
    {
      printf("%s [BT] Enabling failed. ret = %d\n", __func__, ret);
      goto error;
    }

  /* Free memory that is allocated in onLoadBond() callback function. */

  free_cccd();

  /* BLE set name */

  ret = ble_set_name(local_ble_name);
  if (ret != BT_SUCCESS)
    {
      printf("%s [BLE] Set name failed. ret = %d\n", __func__, ret);
      goto error;
    }

  /* BLE set address */

  ret = ble_set_address(&local_addr);
  if (ret != BT_SUCCESS)
    {
      printf("%s [BLE] Set address failed. ret = %d\n", __func__, ret);
      goto error;
    }

  /* BLE enable */

  ret = ble_enable();
  if (ret != BT_SUCCESS)
    {      printf("%s [BLE] Enable failed. ret = %d\n", __func__, ret);
      goto error;
    }

  /* BLE create GATT service instance */
  ret = ble_create_service(&g_ble_gatt_service);
  if (ret != BT_SUCCESS)
    {
      printf("%s [BLE] Create GATT service failed. ret = %d\n", __func__, ret);
      goto error;
    }

  /* Setup Service */

  /* Get Service UUID pointer */
  s_uuid = &g_ble_gatt_service->uuid;

  /* Setup Service UUID */
  s_uuid->type                  = BLE_UUID_TYPE_BASEALIAS_BTSIG;
  s_uuid->value.alias.uuidAlias = BLE_UUID_SDS_SERVICE_IN;

  /* Setup Characteristic */

  /* Get Characteristic UUID pointer */
  c_uuid = &g_ble_gatt_char.uuid;

  /* Setup Characteristic UUID */
  c_uuid->type =BLE_UUID_TYPE_BASEALIAS_BTSIG;
  c_uuid->value.alias.uuidAlias = BLE_UUID_SDS_CHAR_IN;

  /* Set data point */
  char_value.data = char_data;

  /* Setup Characteristic BLE_ATTR_PERM */
  memcpy(&char_value.attrPerm, &attr_param, sizeof(BLE_ATTR_PERM));

  /* Setup Characteristic BLE_CHAR_VALUE */
  memcpy(&g_ble_gatt_char.value, &char_value, sizeof(BLE_CHAR_VALUE));

  /* Setup Characteristic BLE_CHAR_PROP */
  memcpy(&g_ble_gatt_char.property, &char_property, sizeof(BLE_CHAR_PROP));

  /* BLE add GATT characteristic into service */
  ret = ble_add_characteristic(g_ble_gatt_service, &g_ble_gatt_char);
  if (ret != BT_SUCCESS)
    {
      printf("%s [BLE] Add GATT characteristic failed. ret = %d\n", __func__, ret);
      goto error;
    }

  /* BLE register GATT service */

  ret = ble_register_servce(g_ble_gatt_service);
  if (ret != BT_SUCCESS)
    {
      printf("%s [BLE] Register GATT service failed. ret = %d\n", __func__, ret);
      goto error;
    }

  /* BLE start advertise */

  ret = ble_start_advertise();
  if (ret != BT_SUCCESS)
    {
      printf("%s [BLE] Start advertise failed. ret = %d\n", __func__, ret);
      goto error;
    }

  return ret;

error:
  return ret;

}

void loop() {
  /* put your main code here, to run repeatedly: */

  static int LoopCount = 0;
  static int LastPrintMin = 0;
  static int ret = 0;
  static uint8_t data[BLE_MAX_CHAR_SIZE];
  static uint8_t ble_notify_data = 0;
  static int len;
  /* Blink LED. */
  Led_isActive();

  /* Check update. */
  if (Gnss.waitUpdate(-1))
  {
    /* Get NaviData. */
    SpNavData NavData;
    Gnss.getNavData(&NavData);

    /* Set posfix LED. */
    bool LedSet = (NavData.posDataExist && (NavData.posFixMode != FixInvalid));
    Led_isPosfix(LedSet);
    // Get the location
      float lat = (float)NavData.latitude;
      float lon = (float)NavData.longitude;
    /* Print satellite information every minute.
      if (NavData.time.minute != LastPrintMin)
      {
      print_condition(&NavData);
      LastPrintMin = NavData.time.minute;
      }*/
    
    /* Print position information. */
    print_pos(&NavData);
    sprintf(data, "%.6f,%.6f", lat, lon);
    
    if (ble_is_notify_enabled)
    {
      ret = ble_characteristic_notify(ble_conn_handle,
                                      &g_ble_gatt_char,
                                      data,
                                      BLE_MAX_CHAR_SIZE);
      if (ret != BT_SUCCESS)
      {
        printf("%s [BLE] Send data failed. ret = %d\n", __func__, ret);
      }
    }
    sleep(1);
    return ret;
  }

  /* Check loop count. */
  LoopCount++;
  if (LoopCount >= RESTART_CYCLE)
  {
    int error_flag = 0;

    /* Turn off LED0 */
    ledOff(PIN_LED0);

    /* Set posfix LED. */
    Led_isPosfix(false);

    /* Restart GNSS. */
    if (Gnss.stop() != 0)
    {
      Serial.println("Gnss stop error!!");
      error_flag = 1;
    }
    else if (Gnss.end() != 0)
    {
      Serial.println("Gnss end error!!");
      error_flag = 1;
    }
    else
    {
      Serial.println("Gnss stop OK.");
    }

    if (Gnss.begin() != 0)
    {
      Serial.println("Gnss begin error!!");
      error_flag = 1;
    }
    else if (Gnss.start(HOT_START) != 0)
    {
      Serial.println("Gnss start error!!");
      error_flag = 1;
    }
    else
    {
      Serial.println("Gnss restart OK.");
    }

    LoopCount = 0;

    /* Set error LED. */
    if (error_flag == 1)
    {
      Led_isError(true);
      exit(0);
    }
  }
  return ret;
}
