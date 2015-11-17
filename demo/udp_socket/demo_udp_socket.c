/**
 * \file    demo_udp_socket.c
 * \author  Phuong Nguyen
 * \version v0.0.2
 *
 * \brief   UDP demo application using emb6 stack
 */

/*
********************************************************************************
*                                   INCLUDES
********************************************************************************
*/
#include "emb6.h"

#include "bsp.h"
#include "etimer.h"
#include "evproc.h"

#include "tcpip.h"
#include "uip.h"
#include "uiplib.h"
#include "uip-debug.h"
#include "uip-udp-packet.h"
#include "uip-ds6.h"
#include "rpl.h"

#include "demo_udp_socket.h"


/*
********************************************************************************
*                               LOCAL MACROS
********************************************************************************
*/
#if !defined(IAR_COMPILER)
#undef SMARTMAC_TX
#undef SMARTMAC_RX

#define SMARTMAC_TX
#define SMARTMAC_RX
#endif


#define     LOGGER_ENABLE       LOGGER_DEMO_UDP_SOCKET
#if         LOGGER_ENABLE   ==  TRUE
#define     LOGGER_SUBSYSTEM    "UDP Socket"
#endif
#include    "logger.h"

#define DEF_UDP_MAX_PAYLOAD_LEN         40U

#ifdef  SMARTMAC_RX
#define DEF_UDP_DEVPORT                 4211UL
#define DEF_UDP_REMPORT                 4233UL
#endif

#ifdef  SMARTMAC_TX
#define DEF_UDP_DEVPORT                 4233UL
#define DEF_UDP_REMPORT                 4211UL
#endif


#define DEF_UDP_SEND_INTERVAL           (clock_time_t)( 1U )


/*
********************************************************************************
*                               LOCAL VARIABLES
********************************************************************************
*/
static struct uip_udp_conn *UDPSocket_Conn = (struct uip_udp_conn *) 0;

#ifdef  SMARTMAC_TX
static uint32_t UDPSocket_CurrSeqTx;
static uint32_t UDPSocket_LastSeqRx;
static uint32_t UDPSocket_LostPktQty;
static struct etimer UDPSocket_Etimer;
#endif


/*=============================================================================
 *                              LOCAL FUNCTION PROTOTYPE
 *============================================================================*/
static void     UDPSocket_EventHandler(c_event_t c_event, p_data_t p_data);
static void     UDPSocket_Tx(uint32_t seq);
static uint32_t UDPSocket_GetSeq(uint8_t *p_data, uint16_t len);


/*
********************************************************************************
*                          LOCAL FUNCTION DECLARATIONS
********************************************************************************
*/

/**
 * @brief   Achieve sequence number of an UDP message
 */
static uint32_t UDPSocket_GetSeq(uint8_t *p_data, uint16_t len)
{
    uint32_t ret = 0u;
    uint8_t is_valid = 0u;


    is_valid = (p_data != NULL) &&
               (len > 0);
    if (is_valid == 1u) {
        memcpy(&ret, p_data, sizeof(ret));
    }

    return ret;
}


/**
 * @brief   UDP socket transmission
 *
 * @param   seq     Sequence number of the UDP message to transmit
 */
static void UDPSocket_Tx(uint32_t seq)
{
    uint8_t     seq_size = sizeof(seq);
    uint8_t     payload[] = { 0, 0, 0, 0, 0x50, 0x51, 0x52, 0x53, 0x54 };
    uint16_t    payload_len = sizeof(payload);

    uip_ds6_addr_t  *ps_src_addr;
    rpl_dag_t       *ps_dag_desc;


    /*
     * create packet to send
     */
    memcpy(payload, &seq, seq_size);

#ifdef SMARTMAC_TX
    /*
     * Get Server IP address
     */
    ps_src_addr = uip_ds6_get_global(ADDR_PREFERRED);
    if (ps_src_addr) {
        ps_dag_desc = rpl_get_any_dag();
        if (ps_dag_desc) {
            /*
             * Logging
             */
            printf("UDP Sending...  : ");
            uint16_t len = payload_len;
            uint8_t *p_data = payload;
            while (len--) {
                printf("%02x ", *p_data++);
            }
            printf("\r\n");


            /*
             * Issue transmission request
             */
            uip_ipaddr_copy(&UDPSocket_Conn->ripaddr, &ps_src_addr->ipaddr);
            memcpy(&UDPSocket_Conn->ripaddr.u8[8], &ps_dag_desc->dag_id.u8[8], 8);

            uip_udp_packet_send(UDPSocket_Conn, payload, payload_len);
            uip_create_unspecified(&UDPSocket_Conn->ripaddr);
        }
    }
#endif


#if 0   /* echoed-message transmission from UDP server */
#ifdef SMARTMAC_RX
    uip_udp_packet_sendto(UDPSocket_Conn, payload, payload_len, &remipaddr,
            UIP_HTONS(DEF_UDP_REMPORT));

    printf("Sending...  : ");
    uint8_t *ptr = payload;
    while (payload_len--) {
        printf("%02x ", *ptr++);
    }
    printf("\r\n");

    /* Shows number of lost packets */
    uint8_t is_lost = 0u;

    is_lost = (UDPSocket_CurrSeqTx != 0u)
            && (UDPSocket_CurrSeqTx != (UDPSocket_LastSeqRx + 1));

    if (is_lost != 0u) {
        UDPSocket_LostPktQty++;
    }
    printf("Lost packets: %lu / %lu\r\n\r\n", UDPSocket_LostPktQty, UDPSocket_CurrSeqTx);
#endif /* MAC_MODE_DEVICE */
#endif
}


/**
 * @brief   UDP event handler function
 *
 * @param   c_event     Event to be processed
 * @param   p_data      Callback argument that was registered as the event was
 *                      raised
 */
static void UDPSocket_EventHandler(c_event_t c_event, p_data_t p_data)
{
    uint32_t    seq;
    uint16_t    len;
    uint8_t     has_data;
    uint8_t    *p_dataptr;


    /*
     * process input TCPIP packet
     */
    if (c_event == EVENT_TYPE_TCPIP) {
        has_data = uip_newdata();
        if (has_data != 0u) {
            p_dataptr = (uint8_t *)uip_appdata;
            len = (uint16_t)uip_datalen();

            /*
             * Logging
             */
            printf("UDP Receiving...  : ");
            while (len--) {
                printf("%02x ", *p_dataptr++);
            }
            printf("\r\n");

            /*
             * Obtain sequence number of the received message
             */
            seq = UDPSocket_GetSeq(p_dataptr, len);
#ifdef SMARTMAC_RX
            UDPSocket_Tx(seq);
#else
            UDPSocket_LastSeqRx = seq;
#endif
        }
    }
#ifdef SMARTMAC_TX
    else {
        int err = 0;

        err = etimer_expired(&UDPSocket_Etimer);
        if (err != 0) {
            /*
             * Restart event timer
             */
            etimer_restart(&UDPSocket_Etimer);

            /*
             * Sends a UDP message
             */
            UDPSocket_Tx(UDPSocket_CurrSeqTx);

            /*
             * Increase sequence number of UDP message
             */
            if (++UDPSocket_CurrSeqTx >= 1000) {
                UDPSocket_CurrSeqTx = 0;
            }
        }
    }
#endif
}


/*
********************************************************************************
*                           API FUNCTION DEFINITIONS
********************************************************************************
*/

/**
 * @brief   Initialize UDP Socket demo application
 * @return
 */
int8_t demo_udpSocketInit(void)
{
    /*
     * Create a new UDP connection
     */
    UDPSocket_Conn = udp_new(NULL,
                      UIP_HTONS(DEF_UDP_REMPORT),
                      NULL);

    /*
     * Bind the UDP connection to a local port
     */
    udp_bind(UDPSocket_Conn,
             UIP_HTONS(DEF_UDP_DEVPORT));

    /* set callback for event process */
    evproc_regCallback(EVENT_TYPE_TCPIP,
                       UDPSocket_EventHandler);

#ifdef SMARTMAC_TX
    clock_time_t interval = 0;

    /* set UDP event timer interval */
    interval  = DEF_UDP_SEND_INTERVAL;
    interval *= bsp_get(E_BSP_GET_TRES);

    /* Set event timer for periodic data process */
    etimer_set(&UDPSocket_Etimer,
                interval,
                &UDPSocket_EventHandler);

    /* initialize statistic */
    UDPSocket_CurrSeqTx = 0u;
    UDPSocket_LastSeqRx = 0u;
    UDPSocket_LostPktQty = 0u;
#endif

    /* Always success */
    return 1;
}


/**
 * @brief   Configure UDP socket demo application
 *
 * @param   p_netstk    Pointer to net stack structure
 * @return
 */
int8_t demo_udpSocketCfg(s_ns_t *p_netstk)
{
    int8_t i_ret = -1;


    if (p_netstk != NULL) {
        if (!p_netstk->c_configured) {
            p_netstk->hc    = &sicslowpan_driver;
            p_netstk->frame = &framer_802154;
            p_netstk->llsec = &nullsec_driver;
            i_ret = 1;
        } else {
            if ((p_netstk->hc    == &sicslowpan_driver) &&
                (p_netstk->frame == &framer_802154)     &&
                (p_netstk->llsec == &nullsec_driver)) {
                i_ret = 1;
            }
            else {
                p_netstk = NULL;
                i_ret = 0;
            }
        }
    }
    return i_ret;
}