/*
 * sensors.c
 *
 *  Created on: Nov 8, 2017
 *      Author: Rajkumar.Thiagarajan
 */

#include "MQTT_Config.h"
#include "MQTT_Thread.h"
#include <math.h>
#include "sensors.h"

struct bmi160_dev bmi160;
struct bme680_dev gas_sensor;
struct bmm150_dev bmm150;
struct isl29035_dev isl_dev;


char gps_raw_string[400];
void bmm150_read_data(struct bmi160_dev *bmi160_info, struct bmm150_dev *bmm150_info, uint8_t *mag_data);

/*wrapper function to match the signature of bmm150.read */
static int8_t bmi160_aux_rd(uint8_t id, uint8_t reg_addr, uint8_t *aux_data, uint16_t len)
{
    int8_t rslt;
    (void) ((id));

    /* Discarding the parameter id as it is redundant*/
        rslt = bmi160_aux_read(reg_addr, aux_data, len, &bmi160);

    return rslt;
}

/*wrapper function to match the signature of bmm150.write */
static int8_t bmi160_aux_wr(uint8_t id, uint8_t reg_addr, uint8_t *aux_data, uint16_t len)
{
    int8_t rslt;
    (void) ((id));

    /* Discarding the parameter id as it is redundant */
    rslt = bmi160_aux_write(reg_addr, aux_data, len, &bmi160);

    return rslt;
}

void bmm150_read_data(struct bmi160_dev *bmi160_info, struct bmm150_dev *bmm150_info, uint8_t *mag_data)
{
    /* In BMM150 Mag data starts from register address 0x42 */
    uint8_t aux_addr = 0x42;

    /* Configure the Auxiliary sensor either in auto/manual modes and set the
        polling frequency for the Auxiliary interface */
    bmi160_info->aux_cfg.aux_odr = 8; /* Represents polling rate in 100 Hz*/
    bmi160_config_aux_mode(bmi160_info);

    /* Set the auxiliary sensor to auto mode */
    bmi160_set_aux_auto_mode(&aux_addr, bmi160_info);

    /* Reading data from BMI160 data registers */
    bmi160_read_aux_data_auto_mode(mag_data, bmi160_info);

    /* Compensating the raw mag data available from the BMM150 API */
    bmm150_aux_mag_data(mag_data, bmm150_info);
}

static int8_t isl29035_Initialize(void)
{
    int8_t status = 0;

    /* ISL29035 Sensor Initialization */
    isl_dev.id = ISL29035_I2C_ADDR;
    isl_dev.interface = ISL29035_I2C_INTF;
    isl_dev.read = synergy_i2c_read;
    isl_dev.write = synergy_i2c_write;
    isl_dev.delay_ms = synergy_delay_ms;

    status = isl29035_init(&isl_dev);
    if( status != ISL29035_OK)
    {
        print_to_console("ISL 29035 sensor init failed!!!\r\n");
        return status;
    }
    /* Configure ISL29035 ALS Sensor */
    status = isl29035_configure(&isl_dev);
    if( status != ISL29035_OK)
    {
        print_to_console("ISL 29035 sensor sensor config failed!!!\r\n");
        return status;
    }

    return status;
}

static int8_t bmi160_Initialize(void)
{
    int8_t status;

    /* BMI160 Sensor Initialization */
    bmi160.id = BMI160_I2C_ADDR;
    bmi160.interface = BMI160_I2C_INTF;
    bmi160.read = synergy_i2c_read;
    bmi160.write = synergy_i2c_write;
    bmi160.delay_ms = synergy_delay_ms;

    status = bmi160_init(&bmi160);
    if( status != BMI160_OK)
        APP_ERR_TRAP(status);

    /* Configure BMI160 Accel and Gyro sensor */

    /* Select the Output data rate, range of accelerometer sensor */
    bmi160.accel_cfg.odr = BMI160_ACCEL_ODR_1600HZ;
    bmi160.accel_cfg.range = BMI160_ACCEL_RANGE_4G;
    bmi160.accel_cfg.bw = BMI160_ACCEL_BW_NORMAL_AVG4;

    /* Select the power mode of accelerometer sensor */
    bmi160.accel_cfg.power = BMI160_ACCEL_NORMAL_MODE;

    /* Select the Output data rate, range of Gyroscope sensor */
    bmi160.gyro_cfg.odr = BMI160_GYRO_ODR_3200HZ;
    bmi160.gyro_cfg.range = BMI160_GYRO_RANGE_2000_DPS;
    bmi160.gyro_cfg.bw = BMI160_GYRO_BW_NORMAL_MODE;

    /* Select the power mode of Gyroscope sensor */
    bmi160.gyro_cfg.power = BMI160_GYRO_NORMAL_MODE;

    /* Set the sensor configuration */
    status = bmi160_set_sens_conf(&bmi160);
    if( status != BMI160_OK)
        APP_ERR_TRAP(status);

    return status;
}

static int8_t bmm150_Initialize(void)
{
    int8_t status;

    /* Configure device structure for auxiliary sensor parameter */
    bmi160.aux_cfg.aux_sensor_enable = 1; // auxiliary sensor enable
    bmi160.aux_cfg.aux_i2c_addr = BMI160_AUX_BMM150_I2C_ADDR; // auxiliary sensor address
    bmi160.aux_cfg.manual_enable = 1; // setup mode enable
    bmi160.aux_cfg.aux_rd_burst_len = 2;// burst read of 2 byte

    /* Configure the BMM150 device structure by
    mapping user_aux_read and user_aux_write */
    bmm150.read = bmi160_aux_rd;
    bmm150.write = bmi160_aux_wr;
    bmm150.dev_id = BMM150_DEFAULT_I2C_ADDRESS;
    /* Ensure that sensor.aux_cfg.aux_i2c_addr = bmm150.id
       for proper sensor operation */
    bmm150.delay_ms = synergy_delay_ms;
    bmm150.intf = BMM150_I2C_INTF;

    /* Initialize the auxiliary sensor interface */
    status = bmi160_aux_init(&bmi160);
    if(status != BMI160_OK)
        APP_ERR_TRAP(status);

    status = bmm150_init(&bmm150);
    if( status != BMM150_OK)
        APP_ERR_TRAP(status);

    /* Configure the sensor in normal power mode */
    bmm150.settings.pwr_mode = BMM150_NORMAL_MODE;
    status = bmm150_set_op_mode(&bmm150);
    if( status != BMM150_OK)
        APP_ERR_TRAP(status);

    /* Configure the sensor in Low power preset mode */
    bmm150.settings.preset_mode = BMM150_PRESETMODE_LOWPOWER;
    status = bmm150_set_presetmode(&bmm150);
    if( status != BMM150_OK)
        APP_ERR_TRAP(status);

    return status;
}

/*
 * Convert the units for temperature from celcius to Fahrenheit
 */
static double convert_celsius_2_Fahrenheit(double temp_celsius)
{
    return (((temp_celsius * 9) / 5) + 32);
}

static ssp_err_t gps_Initialize(void)
{
    ssp_err_t result = SSP_SUCCESS;

    print_to_console("Initializing GPS: ");

    result = gps_init();

    return result;
}

static int8_t bme680_Initialize(void)
{
    uint8_t set_required_settings;
    int8_t status;

    /* BME680 Sensor Initialization */
    gas_sensor.dev_id = BME680_I2C_ADDR_PRIMARY;
    gas_sensor.intf = BME680_I2C_INTF;
    gas_sensor.read = synergy_i2c_read;
    gas_sensor.write = synergy_i2c_write;
    gas_sensor.delay_ms = synergy_delay_ms;

    status = bme680_init(&gas_sensor);
    if( status != BME680_OK)
        return status;

    /* Configure BME680 Sensor */
    /* Set the temperature, pressure and humidity settings */
    gas_sensor.tph_sett.os_hum = BME680_OS_2X;
    gas_sensor.tph_sett.os_pres = BME680_OS_4X;
    gas_sensor.tph_sett.os_temp = BME680_OS_8X;
    gas_sensor.tph_sett.filter = BME680_FILTER_SIZE_3;

    /* Set the remaining gas sensor settings and link the heating profile */
    gas_sensor.gas_sett.run_gas = BME680_ENABLE_GAS_MEAS;
    /* Create a ramp heat waveform in 3 steps */
    gas_sensor.gas_sett.heatr_temp = 320; /* degree Celsius */
    gas_sensor.gas_sett.heatr_dur = 150; /* milliseconds */

    /* Select the power mode */
    /* Must be set before writing the sensor configuration */
    gas_sensor.power_mode = BME680_FORCED_MODE;

    /* Set the required sensor settings needed */
    set_required_settings = BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_FILTER_SEL
                            | BME680_GAS_SENSOR_SEL;

    /* Set the desired sensor configuration */
    status = bme680_set_sensor_settings(set_required_settings,&gas_sensor);
    if( status != BME680_OK)
        return status;

    /* Set the power mode */
    status = bme680_set_sensor_mode(&gas_sensor);
    if( status != BME680_OK)
        return status;

    /* Get the total measurement duration so as to sleep or wait till the
     * measurement is complete */
    bme680_get_profile_dur(&gas_sensor.meas_period, &gas_sensor);

    return status;
}

ssp_err_t init_sensors(void)
{
    ssp_err_t ssp_err = SSP_SUCCESS;
    int8_t status = 0;


    /* Open I2C driver instance */
    ssp_err = g_i2c0.p_api->open(g_i2c0.p_ctrl, g_i2c0.p_cfg);
    if(ssp_err != SSP_SUCCESS)
    {
        print_to_console("Unable to Open I2C driver\r\n");
        return ssp_err;
    }

    /* Initialize & Configure the BME680 Sensor */
    status = bme680_Initialize();
    if(BME680_OK != status)
    {
        print_to_console("BME680 Sensor Init Failed !!!\r\n");
        return status;
    }

    /* Initialize & Configure the BMI160 Sensor */
    status = bmi160_Initialize();
    if(BMI160_OK != status)
    {
        print_to_console("BMI160 Sensor Init Failed !!!\r\n");
        return status;
    }

    /* Initialize & Configure the BMM150 Sensor */
    status = bmm150_Initialize();
    if(BMM150_OK != status)
    {
        print_to_console("BMM150 Sensor Init Failed !!!\r\n");
        return status;
    }

    /* Initialize & Configure the ISL29035 Sensor */
    status = isl29035_Initialize();
    if(ISL29035_OK != status)
    {
        print_to_console("ISL29035 Sensor Init Failed !!!\r\n");
        return status;
    }

    /* Initialize & Configure the GPS module */
    ssp_err = gps_Initialize();
    if(ssp_err != SSP_SUCCESS)
    {
        print_to_console("GPS Init Failed !!!\r\n");
        return ssp_err;
    }

    /* Initialize & Configure Microphone */
    ssp_err = init_mic();
    if(ssp_err != SSP_SUCCESS)
    {
        print_to_console("Mic Init Failed !!!\r\n");
        return ssp_err;
    }

    return ssp_err;
}


void read_gps_coordinates(sensors_data_t *sens)
{
    uint8_t i = 0;
    char *star_ptr = NULL;
    char *post_ptr = NULL;
    char *last_ptr = NULL;
    char *token[18];
    char str[100];
    uint32_t index = 0;
    UINT retval = 0;
    float value = 0;

    /* Keep reading till there is a data in the queue */
    while(g_gps_queue.tx_queue_enqueued)
    {
        tx_thread_sleep(1);

        /* Read a character from a queue */
        retval = tx_queue_receive(&g_gps_queue, (gps_raw_string + index), TX_NO_WAIT);

        if(TX_SUCCESS == retval)
        {
            /* Check for end token (*) of GPS location string */
            if('*' == gps_raw_string[index++])
            {
                /* Flush queue to receive new updates */
                tx_queue_flush(&g_gps_queue);

                /* Null terminate GPS location information string */
                gps_raw_string[0] = '\0';
                break;
            }

            if(index > (sizeof(gps_raw_string) - 1))
                index = 0;
        }
    }

    if(GGA_FORMAT_STR_LEN > strlen(gps_raw_string))
    {
        /* Parse GPS location information string for "*" post text  */
        star_ptr = strstr (gps_raw_string, SEARCH_POST_TEXT);
        if (NULL == star_ptr)
        {
            /* Return if not found */
            //print_to_console ("No * found in String !!!\r\n");
           sens->latitude[0] = '\0';
            sens->longitude[0] = '\0';
            return;
        }

        /* Parse GPS location information string for pre text */
        post_ptr = gps_raw_string;
        while ((post_ptr = strstr (post_ptr, SEARCH_PRE_TEXT)) != NULL)
        {
            /* Copy location ptr of pre text */
            last_ptr = post_ptr;
            post_ptr++;
        }

        /* Returns first token */
        token[0] = strtok (last_ptr, ",");

        /* Keep printing tokens while one of the
         * delimiters present in str[].
         */
        while (token[i] != NULL)
        {
            i++;
            token[i] = strtok (NULL, ",");
        }

        if(*token[1] == '0')
        {
            sens->latitude[0] = '\0';
            sens->longitude[0] = '\0';
            return;
        }

        /* Convert decimal to degrees */
        value = (float) atof (token[2] + 2);
        value /= 60;
        memset (token[2] + 2, 0, 1);
        /* Add result to get final result in degrees */
        value += (float) atof (token[2]);
        ftoa(value, str);

        if (sens != NULL)
        {
            /* Check if we receive South in GPS location */
            if ('S' == *token[3])
            {
                /* Add - sign to latitude */
                sens->latitude[0] = '-';
                strcat (sens->latitude, str);
            }
            else
            {
                strncpy (sens->latitude, str, sizeof(str));
            }

            /* Convert decimal to degrees */
            value = (float) atof (token[4] + 3);
            value /= 60;
            memset (token[4] + 3, 0, 1);
            /* Add result to get final result in degrees */
            value += (float) atof (token[4]);
            ftoa (value, str);

            /* Check if we receive West in GPS location */
            if ('W' == *token[5])
            {
                /* Add - sign to longitude */
                sens->longitude[0] = '-';
                strcat (sens->longitude, str);
            }
            else
            {
                strncpy (sens->longitude, str, sizeof(str));
            }
        }
    }
}

void read_sensor(sensors_data_t *sens)
{
    bmi160_data accel_data;
    bmi160_data gyro_data;
    uint8_t mag_data[8];
    UINT status;
    double temp_value;
    struct bme680_field_data  bme_data;

    /* To read both Accel and Gyro data */
    status = (UINT)bmi160_get_sensor_data(BMI160_BOTH_ACCEL_AND_GYRO, &accel_data, &gyro_data, &bmi160);
    if(status == BMI160_OK)
    {
        sens->accel.x_axis = accel_data.x_axis;
        sens->accel.y_axis = accel_data.y_axis;
        sens->accel.z_axis = accel_data.z_axis;

        sens->gyro.x_axis = gyro_data.x_axis;
        sens->gyro.y_axis = gyro_data.y_axis;
        sens->gyro.z_axis = gyro_data.z_axis;
    }

    //Read temperature, pressure and humidity data
    status = (UINT)bme680_read_sensor(&bme_data, &gas_sensor);
    if(status == BME680_OK)
    {
        temp_value = bme_data.temperature/100.0f;
        sens->temperature = convert_celsius_2_Fahrenheit(temp_value);
        sens->humidity = ((double)bme_data.humidity/1000.0f);
        sens->pressure = ((double)bme_data.pressure/100.0f);
    }

    //Read magnetometer sensor data
    bmm150_read_data(&bmi160,&bmm150,&mag_data[0]);
    sens->mag.x = bmm150.data.x;
    sens->mag.y = bmm150.data.y;
    sens->mag.z = bmm150.data.z;

    //Read GPS data
    read_gps_coordinates(sens);
}

