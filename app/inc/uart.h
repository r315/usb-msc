// =============================================================================
/*!
 * @file       msc_diskio.h
 *
 * This file contains definitions for mass storage class usb device
 *
 * @version    x.x.x
 *
 * @copyright  Copyright &copy; &nbsp; 2024 Bithium S.A.
 */
// =============================================================================
#ifndef _UART_H
#define _UART_H
#include <stdint.h>
#include <stdio.h>
// =============================================================================
// Definitions
// =============================================================================
#define MAX_RX_DATA           0x800
#define YES                   0x01
#define NO                    0x00
#define UART_SYNC_BYTE        0x5A
// UART message receive timeout in us
#define UART_MSG_TIMEOUT      800000
#define UART_SYNC_LEN         1
#define UART_SIZE_LEN         2
#define UART_PRIM_LEN         1
#define UART_CHCKSM_LEN       1
// Minimum message SIZE is Primitive + CHCKSM = 2
// Sync byte and Size byte not included
#define UART_MIN_MSG_SIZE     (UART_PRIM_LEN + UART_CHCKSM_LEN)
void UART_Init(void);
// =============================================================================
// UART_WriteByte
// =============================================================================
/*!
 *
 * Send byte over the UART.
 *
 * \param Data - Byte to send
 *
 */
// =============================================================================
void UART_WriteByte (uint8_t Data);
// =============================================================================
// UART_PutMessage
// =============================================================================
/*!
 *
 * Encode messages to send over the UART.
 *
 * \param Message   - Message to send
 *
 */
// =============================================================================
void UART_PutMessage (uint8_t *Message);
// =============================================================================
// UART_WriteDbgLog
// =============================================================================
/*!
 *
 * Transmits debug log data through uart
 *
 * \param Buf - Log data
 * \param Len - Log length
 *
 * \returns Number of bytes transmitted
 *
 */
// =============================================================================
uint32_t UART_WriteDbgLog(const uint8_t *Buf, uint32_t Len);
#endif