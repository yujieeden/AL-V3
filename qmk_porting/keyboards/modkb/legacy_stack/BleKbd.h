/********************************** (C) COPYRIGHT *******************************
 * File Name          : BleKbd.h
 * Description        : BLE HID keyboard transport (CH583 BLE stack on top of
 *                      the WCH GATT/profile library).
 *
 *                      This module only deals with "getting an 8-byte HID
 *                      keyboard report to a BLE host".  Matrix scanning, LED
 *                      strip and report building live in Kbd.c, USB lives in
 *                      UsbHid.c.  Main.c picks one of the two transports at
 *                      boot according to the USB presence detect.
 *
 *                      App-level symbols provided here (the rest of the BLE
 *                      stack comes from BLE_HID/, a copy of the WCH CH583EVT
 *                      example tree).
 *******************************************************************************/

#ifndef __BLEKBD_H
#define __BLEKBD_H

/* Task events owned by this file.  The WCH profile layer already uses
   0x0001 (START_DEVICE_EVT), 0x0002 (START_REPORT_EVT), 0x0004
   (START_PARAM_UPDATE_EVT) and 0x0008 (START_PHY_UPDATE_EVT), and
   TMOS_SystemProcess owns 0x8000 (SYS_EVENT_MSG), so keep away from those. */
#define BLE_KB_TASK_EVT             0x0100   /* scan matrix / send report */
#define BLE_KB_ADV_WATCHDOG_EVT     0x0200   /* 1s reconnect advertising watchdog */

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * @fn      BleKbd_ProcessEvent
 *
 * @brief   TMOS event handler of the BLE keyboard app task.  Called by the
 *          TMOS scheduler from BleKbd_Process().
 *
 * @param   task_id - TMOS task id (unused)
 * @param   events  - pending event bit map
 *
 * @return  events that were not processed
 */
extern uint16_t BleKbd_ProcessEvent(uint8_t task_id, uint16_t events);

/*********************************************************************
 * @fn      BleKbd_Init
 *
 * @brief   Bring up the whole BLE stack and start advertising as a HID
 *          keyboard:
 *            CH58X_BLEInit()  - BLE lib (heap, MAC, RF calibration hooks)
 *            HAL_Init()       - RTC / TMOS 625us time base
 *            GAPRole_PeripheralInit()
 *            HidDev_Init()    - GAP role + GAP bond manager + HID dev
 *            HidEmu_Init()    - HID service, advertising data, app task
 *
 *          Must be called *instead of* USB_DeviceInit(); PHY (PB10/PB11) is
 *          owned by either the USB controller or the BLE radio, not both.
 *          Requires SetSysClock() to have been called and the global
 *          MEM_BUF[BLE_MEMHEAP_SIZE/4] heap to exist.
 *
 * @return  none
 */
extern void BleKbd_Init(void);

/*********************************************************************
 * @fn      BleKbd_Process
 *
 * @brief   BLE main loop body: runs the TMOS scheduler plus the keyboard
 *          report task.  Call it in a tight loop from main().
 *
 * @return  none
 */
extern void BleKbd_Process(void);

/*********************************************************************
 * @fn      BleKbd_Notify
 *
 * @brief   Send a BLE HID NKRO report built from the shared matrix state.
 *          The 8-byte input is retained for the startup self-test path.
 *
 * @param   report  - 8-byte standard keyboard report
 *
 * @return  1 - report handed to the stack, 0 - not connected / notifications
 *              not enabled yet (caller should retry)
 */
extern uint8_t BleKbd_Notify(uint8_t *report);

/*********************************************************************
 * @fn      BleKbd_Connected
 *
 * @brief   Is a BLE host connected and has it enabled HID notifications?

 * @return  1 - yes, 0 - no (advertising, idle or not subscribed yet)
 */
extern uint8_t BleKbd_Connected(void);

#ifdef __cplusplus
}
#endif

#endif /* __BLEKBD_H */
