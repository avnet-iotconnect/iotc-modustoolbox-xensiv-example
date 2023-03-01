/*******************************************************************************
 * Copyright 2020-2021, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 *******************************************************************************/
//
// Copyright: Avnet 2021
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 11/11/21.
//
#include "cyhal.h"
#include "cybsp.h"

/* FreeRTOS header files */
#include "FreeRTOS.h"
#include "task.h"

/* Middleware libraries */
#include "cy_retarget_io.h"
#include "cy_lwip.h"
#include "cy_mqtt_api.h"

#include "cy_mqtt_api.h"
#include "clock.h"

/* LwIP header files */
#include "lwip/netif.h"
#include "iotconnect_certs.h"
#include "iotc_mqtt_client.h"

/* Maximum number of retries for MQTT subscribe operation */
#define MAX_SUBSCRIBE_RETRIES                   (3u)

/* Time interval in milliseconds between MQTT subscribe retries. */
#define MQTT_SUBSCRIBE_RETRY_INTERVAL_MS        (1000)

/* Queue length of a message queue that is used to communicate with the
 * subscriber task.
 */
#define SUBSCRIBER_TASK_QUEUE_LENGTH            (1u)

#define MQTT_NETWORK_BUFFER_SIZE          ( 4 * CY_MQTT_MIN_NETWORK_BUFFER_SIZE )

/* Maximum MQTT connection re-connection limit. */
#ifndef IOTC_MAX_MQTT_CONN_RETRIES
#define IOTC_MAX_MQTT_CONN_RETRIES            (150u)
#endif

/* MQTT re-connection time interval in milliseconds. */
#ifndef IOTC_MQTT_CONN_RETRY_INTERVAL_MS
#define IOTC_MQTT_CONN_RETRY_INTERVAL_MS      (5000)
#endif

#define STARFIELD_G2 \
"-----BEGIN CERTIFICATE-----\n" \
"MIIEdTCCA12gAwIBAgIJAKcOSkw0grd/MA0GCSqGSIb3DQEBCwUAMGgxCzAJBgNV\n" \
"BAYTAlVTMSUwIwYDVQQKExxTdGFyZmllbGQgVGVjaG5vbG9naWVzLCBJbmMuMTIw\n" \
"MAYDVQQLEylTdGFyZmllbGQgQ2xhc3MgMiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0\n" \
"eTAeFw0wOTA5MDIwMDAwMDBaFw0zNDA2MjgxNzM5MTZaMIGYMQswCQYDVQQGEwJV\n" \
"UzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2NvdHRzZGFsZTElMCMGA1UE\n" \
"ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjE7MDkGA1UEAxMyU3RhcmZp\n" \
"ZWxkIFNlcnZpY2VzIFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi\n" \
"MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDVDDrEKvlO4vW+GZdfjohTsR8/\n" \
"y8+fIBNtKTrID30892t2OGPZNmCom15cAICyL1l/9of5JUOG52kbUpqQ4XHj2C0N\n" \
"Tm/2yEnZtvMaVq4rtnQU68/7JuMauh2WLmo7WJSJR1b/JaCTcFOD2oR0FMNnngRo\n" \
"Ot+OQFodSk7PQ5E751bWAHDLUu57fa4657wx+UX2wmDPE1kCK4DMNEffud6QZW0C\n" \
"zyyRpqbn3oUYSXxmTqM6bam17jQuug0DuDPfR+uxa40l2ZvOgdFFRjKWcIfeAg5J\n" \
"Q4W2bHO7ZOphQazJ1FTfhy/HIrImzJ9ZVGif/L4qL8RVHHVAYBeFAlU5i38FAgMB\n" \
"AAGjgfAwge0wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0O\n" \
"BBYEFJxfAN+qAdcwKziIorhtSpzyEZGDMB8GA1UdIwQYMBaAFL9ft9HO3R+G9FtV\n" \
"rNzXEMIOqYjnME8GCCsGAQUFBwEBBEMwQTAcBggrBgEFBQcwAYYQaHR0cDovL28u\n" \
"c3MyLnVzLzAhBggrBgEFBQcwAoYVaHR0cDovL3guc3MyLnVzL3guY2VyMCYGA1Ud\n" \
"HwQfMB0wG6AZoBeGFWh0dHA6Ly9zLnNzMi51cy9yLmNybDARBgNVHSAECjAIMAYG\n" \
"BFUdIAAwDQYJKoZIhvcNAQELBQADggEBACMd44pXyn3pF3lM8R5V/cxTbj5HD9/G\n" \
"VfKyBDbtgB9TxF00KGu+x1X8Z+rLP3+QsjPNG1gQggL4+C/1E2DUBc7xgQjB3ad1\n" \
"l08YuW3e95ORCLp+QCztweq7dp4zBncdDQh/U90bZKuCJ/Fp1U1ervShw3WnWEQt\n" \
"8jxwmKy6abaVd38PMV4s/KCHOkdp8Hlf9BRUpJVeEXgSYCfOn8J3/yNTd126/+pZ\n" \
"59vPr5KW7ySaNRB6nJHGDn2Z9j8Z3/VyVOEVqQdZe4O/Ui5GjLIAZHYcSNPYeehu\n" \
"VsyuLAOQ1xk4meTKCRlb/weWsKh/NEnfVqn3sF/tM+2MR7cwA130A4w=\n"         \
"-----END CERTIFICATE-----"

#define USEASTAWS \
"-----BEGIN CERTIFICATE-----\n" \
"MIIGEDCCBPigAwIBAgIQAiLsl9xf34N+xFzemiMtpTANBgkqhkiG9w0BAQsFADBG\n" \
"MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRUwEwYDVQQLEwxTZXJ2ZXIg\n" \
"Q0EgMUIxDzANBgNVBAMTBkFtYXpvbjAeFw0yMjA5MDgwMDAwMDBaFw0yMzA4MTQy\n" \
"MzU5NTlaMCgxJjAkBgNVBAMMHSouaW90LnVzLWVhc3QtMS5hbWF6b25hd3MuY29t\n" \
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA5D/mYN0h7CNiAJKHZARv\n" \
"HMSyI1RvoI9Owem+tx5eHmCR9tKSarw9UuiH4TuNwJpM6tT0TWW1Gb/s/92+G3bC\n" \
"6tglihJ5Et/aSi4J7LKgEoJZHk72B3IYT465oB8ALIlbePIgYGxihXLDO3JHfPlK\n" \
"yUTzVIJEQlShjLAaZD3t6QpS6M4YRtVWNYHVlW4yl9bW9IfZfQo+6BP8RNZQidRJ\n" \
"A6N6Tt9vpJy33fgHCBy+lgKkM48oSWJLyKBB7O9OVUr1vBcHwhcrEHiVcIm6Ascb\n" \
"GtpMuOfi8hxhwKZ69HjrDw14apbhG3WICCvRJegM/fbTtI1xsRiGxmHH8pTsijmp\n" \
"1QIDAQABo4IDFjCCAxIwHwYDVR0jBBgwFoAUWaRmBlKge5WSPKOUByeWdFv5PdAw\n" \
"HQYDVR0OBBYEFHizXhJxf4DF5yYUiQOv/dOIzgLlMEUGA1UdEQQ+MDyCG2lvdC51\n" \
"cy1lYXN0LTEuYW1hem9uYXdzLmNvbYIdKi5pb3QudXMtZWFzdC0xLmFtYXpvbmF3\n" \
"cy5jb20wDgYDVR0PAQH/BAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEF\n" \
"BQcDAjA9BgNVHR8ENjA0MDKgMKAuhixodHRwOi8vY3JsLnNjYTFiLmFtYXpvbnRy\n" \
"dXN0LmNvbS9zY2ExYi0xLmNybDATBgNVHSAEDDAKMAgGBmeBDAECATB1BggrBgEF\n" \
"BQcBAQRpMGcwLQYIKwYBBQUHMAGGIWh0dHA6Ly9vY3NwLnNjYTFiLmFtYXpvbnRy\n" \
"dXN0LmNvbTA2BggrBgEFBQcwAoYqaHR0cDovL2NydC5zY2ExYi5hbWF6b250cnVz\n" \
"dC5jb20vc2NhMWIuY3J0MAwGA1UdEwEB/wQCMAAwggF/BgorBgEEAdZ5AgQCBIIB\n" \
"bwSCAWsBaQB3AOg+0No+9QY1MudXKLyJa8kD08vREWvs62nhd31tBr1uAAABgx4V\n" \
"iq0AAAQDAEgwRgIhAM4snf4zDJNt9t6a/SrojloHcwmd9I0WX54thJTs6GRJAiEA\n" \
"siWlskmePQHHWw6TfGRZxZ8AKgRx/AaA06BGFUiHMmMAdgA1zxkbv7FsV78PrUxt\n" \
"Qsu7ticgJlHqP+Eq76gDwzvWTAAAAYMeFYr7AAAEAwBHMEUCIQCxzYO1a417aXSU\n" \
"yd2Li9FW1OZGY2xBghm5ckmBeBZixQIgBklQIBiMWfMfPcc3jLY7IGfObmajl6f8\n" \
"7B30IfROzFcAdgC3Pvsk35xNunXyOcW6WPRsXfxCz3qfNcSeHQmBJe20mQAAAYMe\n" \
"FYsXAAAEAwBHMEUCIQC6YTAdlyaR/6J4QV1L7lggfAQ1WkK2Ee3OgfXGBC8WVQIg\n" \
"Eiks/c5rB2k6/gN+sYyx3OCBQybm7az44FQQNh+R9fMwDQYJKoZIhvcNAQELBQAD\n" \
"ggEBAA1Kd2NQujtre96VS9rF71PNK7sNczOCn6FwinvcKNzbU0JVUbqUEJA9EfQS\n" \
"YLuzCfv5ZlA2jRQsrS2hWu1d3cToi184+wR856iYU9Bza5sVCecqDolWtcqAsALd\n" \
"hNvkpuIZF5cbQW2sBUWSAC29NFLwSF9t+jF3idjysf0EZ3M6guGAegvnWgIc8cC3\n" \
"6iGVQCZUp/zzNT3KWVL23sXuZzi0kr9TBT5DqgfmGdTqkFnIY0l5KcKFp2nnG80d\n" \
"DxztQNEgFFb1IVVqqEBY92IklyPGXjGaLY8+0z95miX1E4PdOFJV4es3dBEAasf2\n" \
"PTX6rN24wuzDbIzmmSpfT6SQxhc=\n" \
"-----END CERTIFICATE-----"

#define AMAZONROOTCA1 \
"-----BEGIN CERTIFICATE-----\n" \
"MIIESTCCAzGgAwIBAgITBn+UV4WH6Kx33rJTMlu8mYtWDTANBgkqhkiG9w0BAQsF\n" \
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
"b24gUm9vdCBDQSAxMB4XDTE1MTAyMjAwMDAwMFoXDTI1MTAxOTAwMDAwMFowRjEL\n" \
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEVMBMGA1UECxMMU2VydmVyIENB\n" \
"IDFCMQ8wDQYDVQQDEwZBbWF6b24wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n" \
"AoIBAQDCThZn3c68asg3Wuw6MLAd5tES6BIoSMzoKcG5blPVo+sDORrMd4f2AbnZ\n" \
"cMzPa43j4wNxhplty6aUKk4T1qe9BOwKFjwK6zmxxLVYo7bHViXsPlJ6qOMpFge5\n" \
"blDP+18x+B26A0piiQOuPkfyDyeR4xQghfj66Yo19V+emU3nazfvpFA+ROz6WoVm\n" \
"B5x+F2pV8xeKNR7u6azDdU5YVX1TawprmxRC1+WsAYmz6qP+z8ArDITC2FMVy2fw\n" \
"0IjKOtEXc/VfmtTFch5+AfGYMGMqqvJ6LcXiAhqG5TI+Dr0RtM88k+8XUBCeQ8IG\n" \
"KuANaL7TiItKZYxK1MMuTJtV9IblAgMBAAGjggE7MIIBNzASBgNVHRMBAf8ECDAG\n" \
"AQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUWaRmBlKge5WSPKOUByeW\n" \
"dFv5PdAwHwYDVR0jBBgwFoAUhBjMhTTsvAyUlC4IWZzHshBOCggwewYIKwYBBQUH\n" \
"AQEEbzBtMC8GCCsGAQUFBzABhiNodHRwOi8vb2NzcC5yb290Y2ExLmFtYXpvbnRy\n" \
"dXN0LmNvbTA6BggrBgEFBQcwAoYuaHR0cDovL2NydC5yb290Y2ExLmFtYXpvbnRy\n" \
"dXN0LmNvbS9yb290Y2ExLmNlcjA/BgNVHR8EODA2MDSgMqAwhi5odHRwOi8vY3Js\n" \
"LnJvb3RjYTEuYW1hem9udHJ1c3QuY29tL3Jvb3RjYTEuY3JsMBMGA1UdIAQMMAow\n" \
"CAYGZ4EMAQIBMA0GCSqGSIb3DQEBCwUAA4IBAQCFkr41u3nPo4FCHOTjY3NTOVI1\n" \
"59Gt/a6ZiqyJEi+752+a1U5y6iAwYfmXss2lJwJFqMp2PphKg5625kXg8kP2CN5t\n" \
"6G7bMQcT8C8xDZNtYTd7WPD8UZiRKAJPBXa30/AbwuZe0GaFEQ8ugcYQgSn+IGBI\n" \
"8/LwhBNTZTUVEWuCUUBVV18YtbAiPq3yXqMB48Oz+ctBWuZSkbvkNodPLamkB2g1\n" \
"upRyzQ7qDn1X8nn8N8V7YJ6y68AtkHcNSRAnpTitxBKjtKPISLMVCx7i4hncxHZS\n" \
"yLyKQXhw2W2Xs0qLeC1etA+jTGDK4UfLeC0SF7FSi8o5LL21L8IzApar2pR/\n" \
"-----END CERTIFICATE-----"

static cy_mqtt_t mqtt_connection;
static uint8_t mqtt_network_buffer[MQTT_NETWORK_BUFFER_SIZE];
static char *publish_topic = NULL; // pointer to sync response's publish topic. Should be available as long as we are connected.
static bool is_connected = false;
static bool is_disconnect_requested = false;
static bool is_mqtt_initialized = false;
static IotConnectC2dCallback c2d_msg_cb = NULL; // callback for inbound messages
static IotConnectStatusCallback status_cb = NULL; // callback for connection status

static void mqtt_event_callback(cy_mqtt_t mqtt_handle, cy_mqtt_event_t event, void *user_data) {
    (void) mqtt_handle;
    (void) user_data;

    switch (event.type) {
    case CY_MQTT_EVENT_TYPE_DISCONNECT: {
        /* MQTT connection with the MQTT broker is broken as the client
         * is unable to communicate with the broker. Set the appropriate
         * command to be sent to the MQTT task.
         */
        printf("Unexpectedly disconnected from MQTT broker!\n");

        /* Send the message to the MQTT client task to handle the
         * disconnection.
         */
        if (status_cb) {
            status_cb(IOTC_CS_MQTT_DISCONNECTED);
        }
        break;
    }

    case CY_MQTT_EVENT_TYPE_SUBSCRIPTION_MESSAGE_RECEIVE: {
        cy_mqtt_publish_info_t *received_msg;


        /* Incoming MQTT message has been received. Send this message to
         * the subscriber callback function to handle it.
         */
        received_msg = &(event.data.pub_msg.received_message);
        if (c2d_msg_cb && !is_disconnect_requested) {
            c2d_msg_cb(received_msg->payload, received_msg->payload_len);
        }
        is_disconnect_requested = false;
        break;
    }
    default: {
        /* Unknown MQTT event */
        printf("Unknown Event received from MQTT callback!\n");
        break;
    }
    }
}

static cy_rslt_t mqtt_subscribe(IotConnectMqttConfig *mqtt_config, cy_mqtt_qos_t qos) {
    /* Status variable */

    cy_mqtt_subscribe_info_t subscribe_info = { .qos = qos, .topic = "iot/testdevice/cmd", .topic_len =
            strlen("iot/testdevice/cmd") };

    cy_rslt_t result = 1;

    /* Subscribe with the configured parameters. */
    for (uint32_t retry_count = 0; retry_count < MAX_SUBSCRIBE_RETRIES; retry_count++) {
        result = cy_mqtt_subscribe(mqtt_connection, &subscribe_info, 1);
        if (result == CY_RSLT_SUCCESS) {
            printf("MQTT client subscribed to the topic '%s' successfully.\n", subscribe_info.topic);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(MQTT_SUBSCRIBE_RETRY_INTERVAL_MS));
    }
    return result;
}

static cy_rslt_t mqtt_connect(IotConnectMqttConfig *mqtt_config) {
    /* Variable to indicate status of various operations. */
    cy_rslt_t result = CY_RSLT_SUCCESS;

    cy_mqtt_connect_info_t connection_info = { //
            .client_id = "testdevice", //
                    .client_id_len = strlen("testdevice"), //
                    .username = "a3etk4e19usyja-ats.iot.us-east-1.amazonaws.com/testdevice", //
                    .username_len = strlen("a3etk4e19usyja-ats.iot.us-east-1.amazonaws.com/testdevice"), //
                    .password = NULL, //
                    .password_len = 0, //
                    .clean_session = true, //
                    .keep_alive_sec = 60, //
                    .will_info = NULL //
            };

    /* NOTE: Symmetric key not supported yet */
    if (mqtt_config->auth->type == IOTC_AT_TOKEN || mqtt_config->auth->type == IOTC_AT_SYMMETRIC_KEY) {
        connection_info.password = "";
        connection_info.password_len = 0;
    }

    /* Generate a unique client identifier with 'MQTT_CLIENT_IDENTIFIER' string
     * as a prefix if the `GENERATE_UNIQUE_CLIENT_ID` macro is enabled.
     */

    for (uint32_t retry_count = 0; retry_count < IOTC_MAX_MQTT_CONN_RETRIES; retry_count++) {

        /* Establish the MQTT connection. */
        result = cy_mqtt_connect(mqtt_connection, &connection_info);

        if (result == CY_RSLT_SUCCESS) {
            printf("MQTT connection successful.\n");
            return result;
        }

        printf("MQTT connection failed with error code 0x%08x. Retrying in %d ms. Retries left: %d\n", (int) result,
        IOTC_MQTT_CONN_RETRY_INTERVAL_MS, (int) (IOTC_MAX_MQTT_CONN_RETRIES - retry_count - 1));
        vTaskDelay(pdMS_TO_TICKS(IOTC_MQTT_CONN_RETRY_INTERVAL_MS));
    }

    printf("Exceeded maximum MQTT connection attempts\n");
    printf("MQTT connection failed after retrying for %d mins\n",
            (int) ((IOTC_MQTT_CONN_RETRY_INTERVAL_MS * IOTC_MAX_MQTT_CONN_RETRIES) / 60000u));
    return result;
}

static cy_rslt_t iotc_cleanup_mqtt() {
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_rslt_t ret = CY_RSLT_SUCCESS;
    is_connected = false;

    result = cy_mqtt_disconnect(mqtt_connection);
    if (result) {
        printf("Failed to disconnect the MQTT client. Error was:0x%08lx\n", result);
        is_disconnect_requested = false;
        ret = ret == CY_RSLT_SUCCESS ? result : CY_RSLT_SUCCESS;
    }
    is_connected = false;

    if (mqtt_connection) {
        result = cy_mqtt_delete(mqtt_connection);
        if (result) {
            printf("Failed to delete the MQTT client. Error was:0x%08lx\n", result);
        }
        mqtt_connection = NULL;
        ret = ret == CY_RSLT_SUCCESS ? result : CY_RSLT_SUCCESS;

    }
    if (is_mqtt_initialized) {
        result = cy_mqtt_deinit();
        if (result) {
            printf("Failed to deinit the MQTT client. Error was:0x%08lx\n", result);
        }
        is_mqtt_initialized = false;
        ret = ret == CY_RSLT_SUCCESS ? result : CY_RSLT_SUCCESS;
    }
    publish_topic = NULL;
    c2d_msg_cb = NULL;
    status_cb = NULL;
    return ret;
}

cy_rslt_t iotc_mqtt_client_disconnect() {
    is_disconnect_requested = true;
    return iotc_cleanup_mqtt();
}

bool iotc_mqtt_client_is_connected() {
    return is_connected;
}

cy_rslt_t iotc_mqtt_client_publish(const char *payload, int qos) {
    /* Status variable */
    cy_rslt_t result;

    if (!publish_topic) {
        printf("iotc_mqtt_client_publish: MQTT is not connected\n");
        return (cy_rslt_t) 1;
    }

    /* Structure to store publish message information. */
    cy_mqtt_publish_info_t publish_info = { .qos = (cy_mqtt_qos_t) qos, .topic = publish_topic,
    		.topic_len = strlen(publish_topic), .retain = false, .dup = false };

    /* Publish the data received over the message queue. */
    publish_info.payload = payload;
    publish_info.payload_len = strlen(payload);

    result = cy_mqtt_publish(mqtt_connection, &publish_info);

    if (result != CY_RSLT_SUCCESS) {
        printf("  Publisher: MQTT Publish failed with error 0x%0X.\n", (int) result);
        return result;
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t iotc_mqtt_client_init(IotConnectMqttConfig *c) {
    /* Variable to indicate status of various operations. */
    cy_rslt_t result;

    if (publish_topic) {
        printf("WARNING: MQTT client initialized without disconnecting?\n");
        free(publish_topic);
    }
    publish_topic = NULL;
    c2d_msg_cb = NULL;
    status_cb = NULL;
    is_connected = false;
    is_disconnect_requested = false;

    /* Initialize the MQTT library. */
    result = cy_mqtt_init();
    if (result) {
        iotc_cleanup_mqtt();
        printf("Failed to intialize the MQTT library. Error was:0x%08lx\n", result);
        return result;
    }

    cy_mqtt_broker_info_t broker_info = { //
            .hostname = "a3etk4e19usyja-ats.iot.us-east-1.amazonaws.com", //
            .hostname_len = strlen("a3etk4e19usyja-ats.iot.us-east-1.amazonaws.com"), .port = 8883 //
            };

    cy_awsport_ssl_credentials_t security_info = { 0 };
    security_info.root_ca = AMAZONROOTCA1;
    security_info.root_ca_size = sizeof(AMAZONROOTCA1);

    // mqtt_connect handles different auth types
    if (c->auth->type == IOTC_AT_X509) {
        security_info.client_cert = c->auth->data.cert_info.device_cert;
        security_info.client_cert_size = strlen(c->auth->data.cert_info.device_cert) + 1;
        security_info.private_key = c->auth->data.cert_info.device_key;
        security_info.private_key_size = strlen(c->auth->data.cert_info.device_key) + 1;
    }

    /* Create the MQTT client instance. */
    result = cy_mqtt_create(mqtt_network_buffer, MQTT_NETWORK_BUFFER_SIZE, &security_info, &broker_info,
            (cy_mqtt_callback_t) mqtt_event_callback, NULL, &mqtt_connection);

    if (result) {
        printf("Failed to create the MQTT client. Error was:0x%08lx\n", result);
        iotc_cleanup_mqtt();
        return result;
    }

    result = mqtt_connect(c);
    if (result) {
        iotc_cleanup_mqtt();
        return result;
    }
    result = mqtt_subscribe(c, (cy_mqtt_qos_t) 1);
    if (result) {
        printf("Failed to subscribe to the MQTT topic. Error was:0x%08lx\n", result);
        iotc_cleanup_mqtt();
        return result;
    }
    is_connected = true;
    publish_topic = "$aws/rules/msg_d2c_rpt/testdevice/XG4EXVM/2.1/0";
    status_cb = c->status_cb;
    c2d_msg_cb = c->c2d_msg_cb;
    return result;
}
